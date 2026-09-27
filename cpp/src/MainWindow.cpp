#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "../include/DownloadEngine.hpp"
#include "../include/DownloadProgressDialog.hpp"
#include "../include/IconFactory.hpp"
#include "../include/LocalServerBridge.hpp"
#include "../include/Win32Dark.hpp"
#include <commctrl.h>
#include <fstream>
#include <iomanip>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <sstream>
#include <unordered_map>
#include <windows.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

// Declarations of subdialogs
void ShowFilePropertiesDialog(HWND hParent, DownloadItem *pItem);
void ShowOptionsDialog(HWND hParent, IDMSettings *pSettings);
void ShowChunkVisualizerDialog(HWND hParent, DownloadItem *pItem);
bool ShowAddUrlDialog(HWND hParent, DownloadItem &outItem);
void ShowSchedulerDialog(HWND hParent);
void ShowMediaSnifferDialog(HWND hParent, DownloadItem &outStreamItem,
                            bool &outAdd);
bool ShowDownloadFileInfoDialog(HWND hParent, DownloadItem &item,
                                bool &outStartImmediately);
bool ShowDownloadCompleteDialog(HWND hParent, const DownloadItem &item);
void CacheStreamingMediaSizes(const std::wstring& url, const std::wstring& title, uint64_t videoSize, uint64_t audioSize);

struct BridgeDownloadRequest {
  std::wstring url;
  std::wstring filename;
  std::wstring referer;
  std::wstring cookies;
  std::wstring userAgent;
  std::wstring quality;
  std::wstring originalPageUrl;
  uint64_t totalSize = 0;
  uint64_t videoSize = 0;
  uint64_t audioSize = 0;
};

class MainWindow {
public:
  static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                                  LPARAM lParam) {
    MainWindow *pThis = nullptr;
    if (message == WM_CREATE) {
      CREATESTRUCT *pCreate = (CREATESTRUCT *)lParam;
      pThis = (MainWindow *)pCreate->lpCreateParams;
      SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
      pThis->m_hWnd = hWnd;
      pThis->OnCreate();
      return 0;
    } else {
      pThis = (MainWindow *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    if (pThis) {
      return pThis->HandleMessage(message, wParam, lParam);
    }
    return DefWindowProcW(hWnd, message, wParam, lParam);
  }

  HWND Create() {
    IconFactory::Init();

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_BAR_CLASSES |
                 ICC_TAB_CLASSES;
    InitCommonControlsEx(&icex);

    HICON hIconBig = (HICON)LoadImageW(GetModuleHandle(NULL), MAKEINTRESOURCEW(101), IMAGE_ICON, 32, 32, 0);
    if (!hIconBig)
      hIconBig = (HICON)LoadImageW(NULL, L"app_icon.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    if (!hIconBig)
      hIconBig = (HICON)LoadImageW(NULL, L"d:\\Download Manager AB\\app_icon.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);

    HICON hIconSmall = (HICON)LoadImageW(GetModuleHandle(NULL), MAKEINTRESOURCEW(101), IMAGE_ICON, 16, 16, 0);
    if (!hIconSmall)
      hIconSmall = (HICON)LoadImageW(NULL, L"app_icon.ico", IMAGE_ICON, 16, 16, LR_LOADFROMFILE);
    if (!hIconSmall)
      hIconSmall = (HICON)LoadImageW(NULL, L"d:\\Download Manager AB\\app_icon.ico", IMAGE_ICON, 16, 16, LR_LOADFROMFILE);

    WNDCLASSEXW wcex = {sizeof(WNDCLASSEXW)};
    wcex.lpfnWndProc = MainWindow::WndProc;
    wcex.hInstance = GetModuleHandle(NULL);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"IDM_Native_MainWindowClass";
    wcex.hIcon = hIconBig;
    wcex.hIconSm = hIconSmall;
    RegisterClassExW(&wcex);

    HWND hWnd = CreateWindowExW(
        0, L"IDM_Native_MainWindowClass", L"PDM Download Manager",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
        1024, 620, NULL, NULL, GetModuleHandle(NULL), this);

    if (hWnd && hIconBig) {
      SendMessageW(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
      SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
    }

    return hWnd;
  }

inline std::wstring DetectCategoryFromFilename(const std::wstring& filename) {
    if (filename == L"zip" || filename == L"rar" || filename == L"7z" || filename == L"tar" || filename == L"gz" || filename == L"bz2" || filename == L"iso") {
        return L"Compressed";
    }
    if (filename == L"mp4" || filename == L"mkv" || filename == L"avi" || filename == L"mov" || filename == L"webm" || filename == L"flv") {
        return L"Video";
    }
    if (filename == L"mp3" || filename == L"wav" || filename == L"flac" || filename == L"aac" || filename == L"m4a") {
        return L"Music";
    }
    if (filename == L"pdf" || filename == L"doc" || filename == L"docx" || filename == L"tex" || filename == L"txt") {
        return L"Documents";
    }
    if (filename == L"exe" || filename == L"msi" || filename == L"apk") {
        return L"Programs";
    }

    size_t dot = filename.find_last_of(L'.');
    if (dot == std::wstring::npos) return L"General";
    std::wstring ext = filename.substr(dot + 1);
    for (auto& ch : ext) ch = towlower(ch);

    if (ext == L"zip" || ext == L"rar" || ext == L"7z" || ext == L"tar" || ext == L"gz" || ext == L"bz2" || ext == L"iso" || ext == L"tgz" || ext == L"xz") {
        return L"Compressed";
    }
    if (ext == L"mp4" || ext == L"mkv" || ext == L"avi" || ext == L"mov" || ext == L"wmv" || ext == L"webm" || ext == L"flv" || ext == L"m3u8" || ext == L"mpd" || ext == L"ts" || ext == L"ogv") {
        return L"Video";
    }
    if (ext == L"mp3" || ext == L"wav" || ext == L"flac" || ext == L"aac" || ext == L"ogg" || ext == L"m4a" || ext == L"wma") {
        return L"Music";
    }
    if (ext == L"exe" || ext == L"msi" || ext == L"apk" || ext == L"bat" || ext == L"cmd" || ext == L"jar" || ext == L"appx") {
        return L"Programs";
    }
    if (ext == L"pdf" || ext == L"doc" || ext == L"docx" || ext == L"xls" || ext == L"xlsx" || ext == L"ppt" || ext == L"pptx" || ext == L"txt" || ext == L"tex" || ext == L"rtf" || ext == L"epub" || ext == L"md" || ext == L"markdown" || ext == L"json" || ext == L"csv") {
        return L"Documents";
    }
    return L"General";
}

inline void EnsureDirectoryExists(const std::wstring& path) {
    wchar_t dir[MAX_PATH];
    wcscpy_s(dir, path.c_str());
    PathRemoveFileSpecW(dir);
    SHCreateDirectoryExW(NULL, dir, NULL);
}

inline std::wstring GetDefaultDownloadsFolder() {
    wchar_t userProfile[MAX_PATH] = { 0 };
    if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
        return std::wstring(userProfile) + L"\\Downloads\\";
    }
    return L"C:\\Users\\UHD\\Downloads\\";
}

  struct StreamDownloadSession {
    DownloadItem item;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    std::wstring savePath;
    uint64_t totalBytes = 0;
    uint64_t downloadedBytes = 0;
    std::chrono::steady_clock::time_point lastSpeedTime;
    uint64_t lastSpeedBytes = 0;
  };

  std::mutex m_streamSessionMutex;
  std::unordered_map<std::wstring, std::shared_ptr<StreamDownloadSession>> m_streamSessions;

  bool OnStreamInit(const std::wstring &streamId, const std::wstring &url,
                    const std::wstring &filename, uint64_t totalSize,
                    const std::wstring &referer, const std::wstring &cookies,
                    const std::wstring &userAgent) {
    DownloadItem item;
    item.id = L"stream-" + std::to_wstring(GetTickCount());
    item.url = url;
    item.filename = filename.empty() ? L"download.bin" : filename;
    item.category = DetectCategoryFromFilename(item.filename);
    std::wstring baseDownloads = GetDefaultDownloadsFolder();
    item.savePath = baseDownloads + item.category + L"\\" + item.filename;
    item.referer = referer;
    item.cookies = cookies;
    item.userAgent = userAgent;
    item.description = L"Captured via Browser Stream Relay (TLS Fingerprint Safe)";
    item.sizeBytes = totalSize;
    item.downloadedBytes = 0;
    item.status = DownloadStatus::Downloading;
    item.connections = 1;

    // Show the Download File Info dialog so user can choose destination folder / rename file
    bool startImmediately = true;
    if (!ShowDownloadFileInfoDialog(m_hWnd, item, startImmediately)) {
      return false; // User cancelled
    }

    EnsureDirectoryExists(item.savePath);

    HANDLE hFile = CreateFileW(
        item.savePath.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
      return false;
    }

    auto session = std::make_shared<StreamDownloadSession>();
    session->item = item;
    session->savePath = item.savePath;
    session->totalBytes = totalSize;
    session->downloadedBytes = 0;
    session->hFile = hFile;
    session->lastSpeedTime = std::chrono::steady_clock::now();
    session->lastSpeedBytes = 0;

    {
      std::lock_guard<std::mutex> lock(m_streamSessionMutex);
      m_streamSessions[streamId] = session;
    }

    m_engine.AddItem(item);

    // Notify UI on main thread to update List View and show Progress Dialog
    PostMessageW(m_hWnd, WM_USER + 202, (WPARAM) new DownloadItem(item), 0);
    return true;
  }

  void OnStreamChunk(const std::wstring &streamId, const char *data, size_t size) {
    std::shared_ptr<StreamDownloadSession> session;
    {
      std::lock_guard<std::mutex> lock(m_streamSessionMutex);
      auto it = m_streamSessions.find(streamId);
      if (it != m_streamSessions.end()) {
        session = it->second;
      }
    }

    if (session && session->hFile != INVALID_HANDLE_VALUE && data && size > 0) {
      DWORD written = 0;
      WriteFile(session->hFile, data, (DWORD)size, &written, NULL);
      session->downloadedBytes += size;
      session->item.downloadedBytes = session->downloadedBytes;

      auto now = std::chrono::steady_clock::now();
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - session->lastSpeedTime).count();
      if (ms >= 150) {
        uint64_t diff = session->downloadedBytes - session->lastSpeedBytes;
        session->item.speedBytesPerSec = (uint64_t)((double)diff / ((double)ms / 1000.0));
        session->lastSpeedBytes = session->downloadedBytes;
        session->lastSpeedTime = now;
        m_engine.UpdateItem(session->item);
      }
    }
  }

  void OnStreamComplete(const std::wstring &streamId, bool success, const std::wstring &errorMsg) {
    std::shared_ptr<StreamDownloadSession> session;
    {
      std::lock_guard<std::mutex> lock(m_streamSessionMutex);
      auto it = m_streamSessions.find(streamId);
      if (it != m_streamSessions.end()) {
        session = it->second;
        m_streamSessions.erase(it);
      }
    }

    if (session) {
      if (session->hFile != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(session->hFile);
        CloseHandle(session->hFile);
        session->hFile = INVALID_HANDLE_VALUE;
      }

      if (success && session->downloadedBytes > 0) {
        session->item.status = DownloadStatus::Complete;
        if (session->totalBytes > 0) session->item.downloadedBytes = session->totalBytes;
        else session->item.sizeBytes = session->downloadedBytes;
        session->item.speedBytesPerSec = 0;
        m_engine.UpdateItem(session->item);

        // Show Download Complete Dialog
        PostMessageW(m_hWnd, WM_USER + 203, (WPARAM) new DownloadItem(session->item), 0);
      } else {
        session->item.status = DownloadStatus::Error;
        session->item.diagnosticText = errorMsg.empty() ? L"Stream failed or returned 0 bytes" : errorMsg;
        m_engine.UpdateItem(session->item);
      }

      PostMessageW(m_hWnd, WM_USER + 101, 0, 0);
    }
  }

  void TriggerAddFromUrl(const std::wstring &url,
                         const std::wstring &filename = L"",
                         const std::wstring &referer = L"",
                         const std::wstring &cookies = L"",
                         const std::wstring &userAgent = L"",
                         const std::wstring &quality = L"",
                         const std::wstring &originalPageUrl = L"",
                         uint64_t totalSize = 0,
                         uint64_t videoSize = 0,
                         uint64_t audioSize = 0) {
    if (videoSize > 0 || audioSize > 0) {
      CacheStreamingMediaSizes(url, filename, videoSize, audioSize);
      if (!originalPageUrl.empty() && originalPageUrl != url) {
        CacheStreamingMediaSizes(originalPageUrl, filename, videoSize, audioSize);
      }
    }

    DownloadItem item;
    item.id = L"dl-" + std::to_wstring(GetTickCount());
    item.url = url;
    item.quality = quality;
    item.originalPageUrl = originalPageUrl;

    bool isAudio = (quality.find(L"Audio") != std::wstring::npos ||
                    quality.find(L"MP3") != std::wstring::npos ||
                    quality.find(L"M4A") != std::wstring::npos);
    std::wstring mediaExt = (quality.find(L"M4A") != std::wstring::npos) ? L".m4a" : L".mp3";

    // Parse filename
    if (!filename.empty() && filename != L"watch" && filename != L"download-file" && filename != L"YouTube_Video.mp4" && filename != L"Facebook_Video.mp4") {
      item.filename = filename;
      if (isAudio && item.filename.length() > 4 && item.filename.substr(item.filename.length() - 4) == L".mp4") {
        item.filename = item.filename.substr(0, item.filename.length() - 4) + mediaExt;
      }
    } else if (DownloadEngine::IsStreamingMediaURL(url)) {
      std::wstring exactTitle = DownloadEngine::GetMediaTitle(url);
      if (!exactTitle.empty()) {
        item.filename = exactTitle + (isAudio ? mediaExt : L".mp4");
      } else {
        if (url.find(L"youtube.com") != std::wstring::npos || url.find(L"youtu.be") != std::wstring::npos) {
          item.filename = isAudio ? L"YouTube_Audio.mp3" : L"YouTube_Video.mp4";
        } else if (url.find(L"facebook.com") != std::wstring::npos || url.find(L"fb.watch") != std::wstring::npos) {
          item.filename = isAudio ? L"Facebook_Audio.mp3" : L"Facebook_Video.mp4";
        } else {
          item.filename = isAudio ? L"Stream_Audio.mp3" : L"Stream_Video.mp4";
        }
      }
    } else {
      // Check query parameters first (e.g. ?path=%2Fmnt%2Fuser-data%2Foutputs%2Fdesign.md, ?filename=, ?file=)
      size_t pathPos = url.find(L"path=");
      if (pathPos == std::wstring::npos) pathPos = url.find(L"filename=");
      if (pathPos == std::wstring::npos) pathPos = url.find(L"file=");
      if (pathPos != std::wstring::npos) {
        size_t valStart = url.find(L'=', pathPos) + 1;
        size_t valEnd = url.find(L'&', valStart);
        std::wstring rawVal = (valEnd != std::wstring::npos) ? url.substr(valStart, valEnd - valStart) : url.substr(valStart);
        std::wstring decoded;
        for (size_t i = 0; i < rawVal.length(); ++i) {
          if (rawVal[i] == L'%' && i + 2 < rawVal.length()) {
            std::wstring hex = rawVal.substr(i + 1, 2);
            wchar_t ch = (wchar_t)wcstol(hex.c_str(), NULL, 16);
            decoded += ch;
            i += 2;
          } else {
            decoded += rawVal[i];
          }
        }
        size_t lastSlash = decoded.find_last_of(L"/\\");
        std::wstring base = (lastSlash != std::wstring::npos) ? decoded.substr(lastSlash + 1) : decoded;
        if (!base.empty() && base.find(L'.') != std::wstring::npos) {
          item.filename = base;
        }
      }

      if (item.filename.empty() || item.filename == L"download-file") {
        size_t slash = url.find_last_of(L'/');
        item.filename = (slash != std::wstring::npos && slash + 1 < url.length())
                            ? url.substr(slash + 1)
                            : L"download.bin";
        size_t qmark = item.filename.find(L'?');
        if (qmark != std::wstring::npos)
          item.filename = item.filename.substr(0, qmark);
      }
    }

    // Handle bare extension names (like "/download/zip")
    if (item.filename == L"zip") item.filename = L"project_archive.zip";
    else if (item.filename == L"rar") item.filename = L"archive.rar";
    else if (item.filename == L"7z") item.filename = L"archive.7z";
    else if (item.filename == L"tar") item.filename = L"archive.tar";
    else if (item.filename == L"gz") item.filename = L"archive.gz";
    else if (item.filename == L"mp4") item.filename = isAudio ? L"audio.mp3" : L"video.mp4";
    else if (item.filename == L"mp3") item.filename = L"audio.mp3";
    else if (item.filename == L"pdf") item.filename = L"document.pdf";
    else if (item.filename == L"exe") item.filename = L"setup.exe";

    item.category = isAudio ? L"Music" : DetectCategoryFromFilename(item.filename);
    std::wstring baseDownloads = GetDefaultDownloadsFolder();
    item.savePath = baseDownloads + item.category + L"\\" + item.filename;
    item.referer = referer;
    item.cookies = cookies;
    item.userAgent = userAgent;
    item.description = L"Captured via IDM Browser Sniffer";
    item.sizeBytes = totalSize;
    item.downloadedBytes = 0;
    item.status = DownloadStatus::Downloading;
    item.connections = 16;

    {
      CreateDirectoryW(L"C:\\temp", NULL);
      std::wofstream dbg(L"C:\\temp\\dm_debug.txt", std::ios::app);
      if (dbg.is_open()) {
        dbg << L"=== [MAINWINDOW TRIGGER] ===" << std::endl;
        dbg << L"URL: " << item.url << std::endl;
        dbg << L"Filename: " << item.filename << std::endl;
        dbg << L"Referer: " << item.referer << std::endl;
        dbg << L"Cookies: " << item.cookies << std::endl;
        dbg << L"UserAgent: " << item.userAgent << std::endl;
        dbg << L"============================" << std::endl << std::endl;
        dbg.close();
      }
      std::wofstream dbg2(L"d:\\Download Manager AB\\dm_debug.txt", std::ios::app);
      if (dbg2.is_open()) {
        dbg2 << L"=== [MAINWINDOW TRIGGER] ===" << std::endl;
        dbg2 << L"URL: " << item.url << std::endl;
        dbg2 << L"Filename: " << item.filename << std::endl;
        dbg2 << L"Referer: " << item.referer << std::endl;
        dbg2 << L"Cookies: " << item.cookies << std::endl;
        dbg2 << L"UserAgent: " << item.userAgent << std::endl;
        dbg2 << L"============================" << std::endl << std::endl;
        dbg2.close();
      }
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    const wchar_t *months[] = {L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun",
                               L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"};
    item.lastTryDate = std::wstring(months[st.wMonth - 1]) + L" " +
                       std::to_wstring(st.wDay) + L", " +
                       std::to_wstring(st.wYear);

    bool startImmediately = true;
    if (ShowDownloadFileInfoDialog(NULL, item, startImmediately)) {
      item.status = startImmediately ? DownloadStatus::Downloading
                                     : DownloadStatus::Queued;
      m_engine.AddItem(item);
      if (startImmediately) {
        m_engine.StartDownload(item.id);
      }
      PopulateListView();

      if (startImmediately) {
        ShowDownloadProgressDialog(NULL, &item, &m_engine);
      }
    }
  }

  ~MainWindow() {
    m_serverBridge.Stop();
    IconFactory::Shutdown();
  }

private:
  HWND m_hWnd = NULL;
  HWND m_hToolbar = NULL;
  HWND m_hTreeView = NULL;
  HWND m_hListView = NULL;
  HWND m_hStatusBar = NULL;
  HIMAGELIST m_hImageListTb = NULL;
  HIMAGELIST m_hImageListLv = NULL;

  DownloadEngine m_engine;
  IDMSettings m_settings;
  LocalServerBridge m_serverBridge;

  void OnCreate() {
    EnableWindowDarkMode(m_hWnd);

    CreateMenuBar();
    CreateToolbar();
    CreateTreeView();
    CreateListView();
    CreateStatusBar();

    // Start background bridge server on 127.0.0.1:9898
    m_serverBridge.Start(
        [this](const std::wstring &url, const std::wstring &filename,
               const std::wstring &referer, const std::wstring &cookies,
               const std::wstring &userAgent, const std::wstring &quality,
               const std::wstring &originalPageUrl, uint64_t totalSize,
               uint64_t videoSize, uint64_t audioSize) {
          // Post notification to main thread
          auto *req = new BridgeDownloadRequest{url, filename, referer, cookies, userAgent, quality, originalPageUrl, totalSize, videoSize, audioSize};
          PostMessageW(m_hWnd, WM_USER + 200, (WPARAM)req, 0);
        },
        [](const std::wstring &url) {
          return DownloadEngine::ProbeFormatSizesJson(url);
        },
        [this](const std::wstring &streamId, const std::wstring &url,
               const std::wstring &filename, uint64_t totalSize,
               const std::wstring &referer, const std::wstring &cookies,
               const std::wstring &userAgent) -> bool {
          return OnStreamInit(streamId, url, filename, totalSize, referer, cookies, userAgent);
        },
        [this](const std::wstring &streamId, const char *data, size_t size) {
          OnStreamChunk(streamId, data, size);
        },
        [this](const std::wstring &streamId, bool success, const std::wstring &errorMsg) {
          OnStreamComplete(streamId, success, errorMsg);
        });

    // Setup Engine Callbacks
    m_engine.SetCallbacks(
        [](const std::wstring &id, uint64_t downloaded, uint64_t total,
           uint64_t speed) { /* Handled smoothly by 250ms UI timer */ },
        [this](const std::wstring &id, DownloadStatus status) {
          PostMessageW(m_hWnd, WM_USER + 102, 0, 0);
        });

    SetTimer(m_hWnd, 1, 250, NULL);
  }

  void CreateMenuBar() {
    HMENU hMenuBar = CreateMenu();

    HMENU hMenuTasks = CreatePopupMenu();
    AppendMenuW(hMenuTasks, MF_STRING, 1001, L"&Add new download...\tCtrl+N");
    AppendMenuW(hMenuTasks, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenuTasks, MF_STRING, 1003, L"Stop all downloads");
    AppendMenuW(hMenuTasks, MF_STRING, 1004, L"Resume all downloads");
    AppendMenuW(hMenuTasks, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenuTasks, MF_STRING, 1005, L"E&xit");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuTasks, L"&Tasks");

    HMENU hMenuFile = CreatePopupMenu();
    AppendMenuW(hMenuFile, MF_STRING, 1001, L"Add URL...");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuFile, L"&File");

    HMENU hMenuDownloads = CreatePopupMenu();
    AppendMenuW(hMenuDownloads, MF_STRING, 1010, L"&Options...\tAlt+O");
    AppendMenuW(hMenuDownloads, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenuDownloads, MF_STRING, 1012, L"Delete &Completed");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuDownloads, L"&Downloads");

    HMENU hMenuView = CreatePopupMenu();
    AppendMenuW(hMenuView, MF_STRING, 1020, L"Toolbar");
    AppendMenuW(hMenuView, MF_STRING, 1021, L"Categories Sidebar");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuView, L"&View");

    HMENU hMenuHelp = CreatePopupMenu();
    AppendMenuW(hMenuHelp, MF_STRING, 1030, L"About PDM Download Manager...");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuHelp, L"&Help");

    HMENU hMenuReg = CreatePopupMenu();
    AppendMenuW(hMenuReg, MF_STRING, 1040, L"Registration (PRO ACTIVATED)");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuReg, L"&Registration");

    SetMenu(m_hWnd, hMenuBar);
  }

  static LRESULT CALLBACK ToolbarSubclassProc(HWND hWnd, UINT uMsg,
                                              WPARAM wParam, LPARAM lParam,
                                              UINT_PTR uIdSubclass,
                                              DWORD_PTR dwRefData) {
    if (uMsg == WM_ERASEBKGND) {
      HDC hdc = (HDC)wParam;
      RECT rc;
      GetClientRect(hWnd, &rc);
      FillRect(hdc, &rc, GetSysColorBrush(COLOR_BTNFACE));
      return 1;
    }
    if (uMsg == WM_PAINT) {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hWnd, &ps);

      // Fill background with light gray COLOR_BTNFACE
      RECT rcClient;
      GetClientRect(hWnd, &rcClient);
      FillRect(hdc, &rcClient, GetSysColorBrush(COLOR_BTNFACE));

      // Draw toolbar buttons on top of our clean background
      DefSubclassProc(hWnd, WM_PRINTCLIENT, (WPARAM)hdc,
                      PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND);

      EndPaint(hWnd, &ps);
      return 0;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
  }

  static LRESULT CALLBACK ListViewSubclassProc(HWND hWnd, UINT uMsg,
                                              WPARAM wParam, LPARAM lParam,
                                              UINT_PTR uIdSubclass,
                                              DWORD_PTR dwRefData) {
    MainWindow *pMain = (MainWindow *)dwRefData;
    static bool s_isDraggingSelection = false;
    static int s_dragStartIndex = -1;

    switch (uMsg) {
    case WM_LBUTTONDOWN: {
      POINT pt = { (SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam) };
      LVHITTESTINFO hti = { 0 };
      hti.pt = pt;
      int hit = ListView_HitTest(hWnd, &hti);
      s_isDraggingSelection = true;
      s_dragStartIndex = hit;
      SetCapture(hWnd);

      if (GetKeyState(VK_CONTROL) >= 0 && GetKeyState(VK_SHIFT) >= 0) {
        if (hit >= 0) {
          ListView_SetItemState(hWnd, -1, 0, LVIS_SELECTED);
          ListView_SetItemState(hWnd, hit, LVIS_SELECTED | LVIS_FOCUSED,
                                LVIS_SELECTED | LVIS_FOCUSED);
        } else {
          ListView_SetItemState(hWnd, -1, 0, LVIS_SELECTED);
        }
      }
      break;
    }

    case WM_MOUSEMOVE: {
      if (s_isDraggingSelection && (wParam & MK_LBUTTON)) {
        POINT pt = { (SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam) };
        LVHITTESTINFO hti = { 0 };
        hti.pt = pt;
        int currentHit = ListView_HitTest(hWnd, &hti);

        if (currentHit >= 0) {
          int start = (s_dragStartIndex >= 0) ? s_dragStartIndex : currentHit;
          int minIdx = min(start, currentHit);
          int maxIdx = max(start, currentHit);
          int count = ListView_GetItemCount(hWnd);

          for (int i = 0; i < count; ++i) {
            if (i >= minIdx && i <= maxIdx) {
              ListView_SetItemState(hWnd, i, LVIS_SELECTED, LVIS_SELECTED);
            } else if (GetKeyState(VK_CONTROL) >= 0) {
              ListView_SetItemState(hWnd, i, 0, LVIS_SELECTED);
            }
          }
          ListView_SetItemState(hWnd, currentHit, LVIS_FOCUSED, LVIS_FOCUSED);
        }
        return 0;
      }
      break;
    }

    case WM_LBUTTONUP: {
      if (s_isDraggingSelection) {
        s_isDraggingSelection = false;
        s_dragStartIndex = -1;
        ReleaseCapture();
      }
      break;
    }

    case WM_KEYDOWN: {
      if (wParam == VK_DELETE) {
        if (pMain) {
          SendMessageW(pMain->m_hWnd, WM_COMMAND, 2003, 0);
          return 0;
        }
      } else if (wParam == 'A' && (GetKeyState(VK_CONTROL) < 0)) {
        // Ctrl+A: Select all items
        ListView_SetItemState(hWnd, -1, LVIS_SELECTED, LVIS_SELECTED);
        return 0;
      }
      break;
    }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
  }

  void CreateToolbar() {
    m_hToolbar = CreateWindowExW(
        0, TOOLBARCLASSNAMEW, NULL,
        WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS | CCS_TOP | CCS_NODIVIDER, 0,
        0, 0, 0, m_hWnd, (HMENU)2000, GetModuleHandle(NULL), NULL);
    SendMessageW(m_hToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
    SendMessageW(m_hToolbar, TB_SETEXTENDEDSTYLE, 0,
                 TBSTYLE_EX_DOUBLEBUFFER | TBSTYLE_EX_MIXEDBUTTONS);

    // 1. Build 24x24 Toolbar ImageList with ILC_COLOR32 | ILC_MASK
    m_hImageListTb = ImageList_Create(24, 24, ILC_COLOR32 | ILC_MASK, 12, 12);

    auto AddTbIcon = [&](COLORREF c, int t) {
      HICON hIco = IconFactory::CreateColoredIcon(24, c, t);
      if (hIco) {
        ImageList_AddIcon(m_hImageListTb, hIco);
        DestroyIcon(hIco);
      }
    };

    AddTbIcon(RGB(34, 197, 94), 0);   // Add URL  (Green plus)
    AddTbIcon(RGB(56, 189, 248), 1);  // Resume   (Blue play)
    AddTbIcon(RGB(245, 158, 11), 2);  // Stop     (Orange square)
    AddTbIcon(RGB(239, 68, 68), 3);   // Stop All (Red octagon)
    AddTbIcon(RGB(239, 68, 68), 4);   // Delete   (Red trash)
    AddTbIcon(RGB(168, 85, 247), 5);  // Del. Completed (Purple check)
    AddTbIcon(RGB(148, 163, 184), 6); // Options  (Gray gear)
    AddTbIcon(RGB(236, 72, 153), 7);  // Scheduler (Pink clock)
    AddTbIcon(RGB(34, 197, 94), 8);   // Start Queue (Green circle play)
    AddTbIcon(RGB(239, 68, 68), 9);   // Stop Queue  (Red circle stop)
    AddTbIcon(RGB(6, 182, 212), 10);  // Media Sniffer (Cyan radio)

    SendMessageW(m_hToolbar, TB_SETIMAGELIST, 0, (LPARAM)m_hImageListTb);

    TBBUTTON tbButtons[] = {{0,
                             1001,
                             TBSTATE_ENABLED,
                             BTNS_BUTTON | BTNS_SHOWTEXT,
                             {0},
                             0,
                             (INT_PTR)L"Add URL"},
                            {1,
                             2001,
                             TBSTATE_ENABLED,
                             BTNS_BUTTON | BTNS_SHOWTEXT,
                             {0},
                             0,
                             (INT_PTR)L"Resume"},
                            {2,
                             2002,
                             TBSTATE_ENABLED,
                             BTNS_BUTTON | BTNS_SHOWTEXT,
                             {0},
                             0,
                             (INT_PTR)L"Stop"},
                            {3,
                             1003,
                             TBSTATE_ENABLED,
                             BTNS_BUTTON | BTNS_SHOWTEXT,
                             {0},
                             0,
                             (INT_PTR)L"Stop All"},
                            {0, 0, 0, BTNS_SEP, {0}, 0, 0},
                            {4,
                             2003,
                             TBSTATE_ENABLED,
                             BTNS_BUTTON | BTNS_SHOWTEXT,
                             {0},
                             0,
                             (INT_PTR)L"Delete"},
                            {5,
                             1012,
                             TBSTATE_ENABLED,
                             BTNS_BUTTON | BTNS_SHOWTEXT,
                             {0},
                             0,
                             (INT_PTR)L"Delete Co..."},
                            {0, 0, 0, BTNS_SEP, {0}, 0, 0},
                            {6,
                             1010,
                             TBSTATE_ENABLED,
                             BTNS_BUTTON | BTNS_SHOWTEXT,
                             {0},
                             0,
                             (INT_PTR)L"Options"},
                            {8,
                             1004,
                             TBSTATE_ENABLED,
                             BTNS_BUTTON | BTNS_SHOWTEXT,
                             {0},
                             0,
                             (INT_PTR)L"Start Qu..."},
                            {9,
                             1003,
                             TBSTATE_ENABLED,
                             BTNS_BUTTON | BTNS_SHOWTEXT,
                             {0},
                             0,
                             (INT_PTR)L"Stop Qu..."}};

    SendMessageW(m_hToolbar, TB_ADDBUTTONS,
                 sizeof(tbButtons) / sizeof(TBBUTTON), (LPARAM)&tbButtons);
    SendMessageW(m_hToolbar, TB_AUTOSIZE, 0, 0);

    // Subclass toolbar to paint background on WM_ERASEBKGND / WM_PAINT
    SetWindowSubclass(m_hToolbar, ToolbarSubclassProc, 1, 0);

    // SetWindowTheme removed to allow standard Windows toolbar background
    // erasing
  }

  void CreateTreeView() {
    m_hTreeView = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
        WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT |
            TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
        0, 52, 190, 500, m_hWnd, (HMENU)3000, GetModuleHandle(NULL), NULL);

    TVINSERTSTRUCTW tvis = {0};
    tvis.hParent = TVI_ROOT;
    tvis.item.mask = TVIF_TEXT;

    wchar_t rootText[] = L"All Downloads";
    tvis.item.pszText = rootText;
    HTREEITEM hAll = TreeView_InsertItem(m_hTreeView, &tvis);

    tvis.hParent = hAll;
    wchar_t c1[] = L"Compressed";
    tvis.item.pszText = c1;
    TreeView_InsertItem(m_hTreeView, &tvis);
    wchar_t c2[] = L"Documents";
    tvis.item.pszText = c2;
    TreeView_InsertItem(m_hTreeView, &tvis);
    wchar_t c3[] = L"Music";
    tvis.item.pszText = c3;
    TreeView_InsertItem(m_hTreeView, &tvis);
    wchar_t c4[] = L"Programs";
    tvis.item.pszText = c4;
    TreeView_InsertItem(m_hTreeView, &tvis);
    wchar_t c5[] = L"Video";
    tvis.item.pszText = c5;
    TreeView_InsertItem(m_hTreeView, &tvis);

    tvis.hParent = TVI_ROOT;
    wchar_t s1[] = L"Unfinished";
    tvis.item.pszText = s1;
    TreeView_InsertItem(m_hTreeView, &tvis);
    wchar_t s2[] = L"Finished";
    tvis.item.pszText = s2;
    TreeView_InsertItem(m_hTreeView, &tvis);
    wchar_t s3[] = L"Grabber projects";
    tvis.item.pszText = s3;
    TreeView_InsertItem(m_hTreeView, &tvis);
    wchar_t s4[] = L"Queues";
    tvis.item.pszText = s4;
    TreeView_InsertItem(m_hTreeView, &tvis);

    TreeView_Expand(m_hTreeView, hAll, TVE_EXPAND);
  }

  void CreateListView() {
    m_hListView = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
        192, 52, 800, 500, m_hWnd, (HMENU)4000, GetModuleHandle(NULL), NULL);

    ListView_SetExtendedListViewStyle(m_hListView, LVS_EX_FULLROWSELECT |
                                                       LVS_EX_GRIDLINES |
                                                       LVS_EX_DOUBLEBUFFER);

    // Subclass ListView to support mouse drag selection, Delete key, and Ctrl+A select all
    SetWindowSubclass(m_hListView, ListViewSubclassProc, 2, (DWORD_PTR)this);

    LVCOLUMNW lvc = {0};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    wchar_t col1[] = L"File Name";
    lvc.pszText = col1;
    lvc.cx = 240;
    lvc.iSubItem = 0;
    ListView_InsertColumn(m_hListView, 0, &lvc);
    wchar_t col2[] = L"Q";
    lvc.pszText = col2;
    lvc.cx = 35;
    lvc.iSubItem = 1;
    ListView_InsertColumn(m_hListView, 1, &lvc);
    wchar_t col3[] = L"Size";
    lvc.pszText = col3;
    lvc.cx = 85;
    lvc.iSubItem = 2;
    ListView_InsertColumn(m_hListView, 2, &lvc);
    wchar_t col4[] = L"Status";
    lvc.pszText = col4;
    lvc.cx = 120;
    lvc.iSubItem = 3;
    ListView_InsertColumn(m_hListView, 3, &lvc);
    wchar_t col5[] = L"Time left";
    lvc.pszText = col5;
    lvc.cx = 75;
    lvc.iSubItem = 4;
    ListView_InsertColumn(m_hListView, 4, &lvc);
    wchar_t col6[] = L"Transfer rate";
    lvc.pszText = col6;
    lvc.cx = 95;
    lvc.iSubItem = 5;
    ListView_InsertColumn(m_hListView, 5, &lvc);
    wchar_t col7[] = L"Last Try Date";
    lvc.pszText = col7;
    lvc.cx = 100;
    lvc.iSubItem = 6;
    ListView_InsertColumn(m_hListView, 6, &lvc);
    wchar_t col8[] = L"Description";
    lvc.pszText = col8;
    lvc.cx = 160;
    lvc.iSubItem = 7;
    ListView_InsertColumn(m_hListView, 7, &lvc);
  }

  void CreateStatusBar() {
    m_hStatusBar = CreateWindowExW(
        0, STATUSCLASSNAMEW, NULL, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0,
        0, 0, m_hWnd, (HMENU)5000, GetModuleHandle(NULL), NULL);
    int parts[] = {380, 750, -1};
    SendMessageW(m_hStatusBar, SB_SETPARTS, 3, (LPARAM)parts);
    SendMessageW(m_hStatusBar, SB_SETTEXTW, 0,
                 (LPARAM)L"PDM Engine: Ready (16-thread Acceleration)");
    SendMessageW(
        m_hStatusBar, SB_SETTEXTW, 1,
        (LPARAM)L"Browser Integration: Active (Chrome, Edge, Firefox)");
    SendMessageW(m_hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"Speed Limiter: OFF");
  }

  void LoadSampleDownloads() {
    // Start with clean downloads list
  }

  int GetSelectedListViewIndex() {
    return ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
  }

  std::vector<int> GetSelectedListViewIndices() {
    std::vector<int> indices;
    int idx = -1;
    while ((idx = ListView_GetNextItem(m_hListView, idx, LVNI_SELECTED)) != -1) {
      indices.push_back(idx);
    }
    return indices;
  }

  void PopulateListView() {
    auto items = m_engine.GetDownloads();
    int itemCount = ListView_GetItemCount(m_hListView);

    if (itemCount != (int)items.size()) {
      // Save selected item IDs to preserve multi-selection across list redraws
      std::vector<std::wstring> selectedIds;
      int selIdx = -1;
      while ((selIdx = ListView_GetNextItem(m_hListView, selIdx, LVNI_SELECTED)) != -1) {
        if (selIdx < (int)items.size()) {
          selectedIds.push_back(items[selIdx].id);
        }
      }

      ListView_DeleteAllItems(m_hListView);
      for (size_t i = 0; i < items.size(); ++i) {
        const auto &item = items[i];
        LVITEMW lvi = {0};
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = (int)i;
        lvi.iSubItem = 0;
        lvi.pszText = const_cast<LPWSTR>(item.filename.c_str());
        lvi.lParam = (LPARAM)i;
        ListView_InsertItem(m_hListView, &lvi);
      }

      // Restore multi-selection
      for (size_t i = 0; i < items.size(); ++i) {
        for (const auto &selId : selectedIds) {
          if (items[i].id == selId) {
            ListView_SetItemState(m_hListView, (int)i, LVIS_SELECTED, LVIS_SELECTED);
          }
        }
      }
    }

    auto SetItemTextIfChanged = [this](int itemIdx, int subItemIdx, const std::wstring& newText) {
      wchar_t curText[512] = {0};
      ListView_GetItemText(m_hListView, itemIdx, subItemIdx, curText, 512);
      if (newText != curText) {
        ListView_SetItemText(m_hListView, itemIdx, subItemIdx, const_cast<LPWSTR>(newText.c_str()));
      }
    };

    for (size_t i = 0; i < items.size(); ++i) {
      const auto &item = items[i];

      // Update File Name and Queue
      SetItemTextIfChanged((int)i, 0, item.filename);
      SetItemTextIfChanged((int)i, 1, item.queueName.empty() ? L"" : item.queueName);

      // Size format (GB / MB / KB / Bytes)
      std::wstring strSize;
      if (item.sizeBytes >= (uint64_t)1024 * 1024 * 1024) {
        std::wstringstream ssSize;
        ssSize << std::fixed << std::setprecision(2)
               << ((double)item.sizeBytes / (1024.0 * 1024.0 * 1024.0))
               << L" GB";
        strSize = ssSize.str();
      } else if (item.sizeBytes >= 1024 * 1024) {
        std::wstringstream ssSize;
        ssSize << std::fixed << std::setprecision(2)
               << ((double)item.sizeBytes / (1024.0 * 1024.0)) << L" MB";
        strSize = ssSize.str();
      } else if (item.sizeBytes >= 1024) {
        std::wstringstream ssSize;
        ssSize << std::fixed << std::setprecision(2)
               << ((double)item.sizeBytes / 1024.0) << L" KB";
        strSize = ssSize.str();
      } else if (item.sizeBytes > 0) {
        strSize = std::to_wstring(item.sizeBytes) + L" Bytes";
      } else {
        strSize = (item.status == DownloadStatus::Downloading)
                      ? L"Connecting..."
                      : L"--";
      }
      SetItemTextIfChanged((int)i, 2, strSize);

      // Status, Time Left, and Transfer Rate format
      if (item.status == DownloadStatus::Complete) {
        SetItemTextIfChanged((int)i, 3, L"Complete");
        SetItemTextIfChanged((int)i, 4, L"");
        SetItemTextIfChanged((int)i, 5, L"");
      } else if (item.status == DownloadStatus::Merging) {
        SetItemTextIfChanged((int)i, 3, L"Merging (100%)");
        SetItemTextIfChanged((int)i, 4, L"Merging...");
        SetItemTextIfChanged((int)i, 5, L"Lossless Mux");
      } else if (item.status == DownloadStatus::Downloading) {
        std::wstring st;
        if (item.sizeBytes > 0) {
          int pct = (int)((item.downloadedBytes * 100) / item.sizeBytes);
          if (pct > 100)
            pct = 100;
          st = L"Downloading (" + std::to_wstring(pct) + L"%)";
        } else {
          st = L"Connecting (0%)";
        }
        SetItemTextIfChanged((int)i, 3, st);

        // Calculate accurate Time Left
        std::wstring strTime = L"--";
        if (item.speedBytesPerSec > 0 &&
            item.sizeBytes > item.downloadedBytes) {
          uint64_t remBytes = item.sizeBytes - item.downloadedBytes;
          uint64_t secs =
              (uint64_t)((double)remBytes / (double)item.speedBytesPerSec);
          if (secs > 99 * 3600 + 59 * 60 + 59)
            secs = 99 * 3600 + 59 * 60 + 59; // Cap display at 99:59:59

          if (secs < 60) {
            strTime = std::to_wstring(secs) + L" sec";
          } else if (secs < 3600) {
            strTime = std::to_wstring(secs / 60) + L" min " +
                      std::to_wstring(secs % 60) + L" sec";
          } else {
            strTime = std::to_wstring(secs / 3600) + L" hr " +
                      std::to_wstring((secs % 3600) / 60) + L" min";
          }
        } else if (item.speedBytesPerSec == 0 && item.downloadedBytes > 0 &&
                   item.downloadedBytes < item.sizeBytes) {
          strTime = L"Stalled";
        }
        SetItemTextIfChanged((int)i, 4, strTime);

        // Calculate accurate Transfer Rate
        std::wstringstream ssSpeed;
        if (item.speedBytesPerSec >= 1024 * 1024) {
          ssSpeed << std::fixed << std::setprecision(2)
                  << ((double)item.speedBytesPerSec / (1024.0 * 1024.0))
                  << L" MB/s";
        } else if (item.speedBytesPerSec >= 1024) {
          ssSpeed << std::fixed << std::setprecision(2)
                  << ((double)item.speedBytesPerSec / 1024.0) << L" KB/s";
        } else if (item.speedBytesPerSec > 0) {
          ssSpeed << item.speedBytesPerSec << L" B/s";
        } else {
          ssSpeed << L"0.00 KB/s";
        }
        SetItemTextIfChanged((int)i, 5, ssSpeed.str());
      } else if (item.status == DownloadStatus::Queued) {
        SetItemTextIfChanged((int)i, 3, L"Queued (Download Later)");
        SetItemTextIfChanged((int)i, 4, L"In Queue");
        SetItemTextIfChanged((int)i, 5, L"");
      } else if (item.status == DownloadStatus::Paused) {
        SetItemTextIfChanged((int)i, 3, L"Paused");
        SetItemTextIfChanged((int)i, 4, L"");
        SetItemTextIfChanged((int)i, 5, L"");
      } else if (item.status == DownloadStatus::Error) {
        std::wstring st = item.diagnosticText.empty() ? L"Error" : (L"Error: " + item.diagnosticText);
        SetItemTextIfChanged((int)i, 3, st);
        SetItemTextIfChanged((int)i, 4, L"");
        SetItemTextIfChanged((int)i, 5, L"");
      }

      SetItemTextIfChanged((int)i, 6, item.lastTryDate);
      SetItemTextIfChanged((int)i, 7, item.description);
    }
  }

  std::wstring GetClipboardUrl() {
    if (!OpenClipboard(m_hWnd))
      return L"";
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (!hData) {
      CloseClipboard();
      return L"";
    }
    wchar_t *pszText = static_cast<wchar_t *>(GlobalLock(hData));
    std::wstring text = pszText ? pszText : L"";
    GlobalUnlock(hData);
    CloseClipboard();

    size_t first = text.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos)
      return L"";
    size_t last = text.find_last_not_of(L" \t\r\n");
    text = text.substr(first, (last - first + 1));

    if (text.rfind(L"http://", 0) == 0 || text.rfind(L"https://", 0) == 0) {
      return text;
    }
    return L"";
  }

  LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_SIZE: {
      int width = LOWORD(lParam);
      int height = HIWORD(lParam);
      SendMessageW(m_hToolbar, TB_AUTOSIZE, 0, 0);
      SendMessageW(m_hStatusBar, WM_SIZE, wParam, lParam);

      RECT tbRect, sbRect;
      GetWindowRect(m_hToolbar, &tbRect);
      GetWindowRect(m_hStatusBar, &sbRect);
      int tbHeight = tbRect.bottom - tbRect.top;
      int sbHeight = sbRect.bottom - sbRect.top;

      int clientH = height - tbHeight - sbHeight;
      SetWindowPos(m_hTreeView, NULL, 0, tbHeight, 200, clientH, SWP_NOZORDER);
      SetWindowPos(m_hListView, NULL, 202, tbHeight, width - 202, clientH,
                   SWP_NOZORDER);
      return 0;
    }

    case WM_TIMER:
    case WM_USER + 101: {
      PopulateListView();
      return 0;
    }

    case WM_USER + 102: {
      PopulateListView();
      if (m_engine.IsQueueActive()) {
        std::wstring activeId = m_engine.GetActiveQueueItemId();
        static std::wstring s_lastShownQueueId;
        if (!activeId.empty() && activeId != s_lastShownQueueId) {
          s_lastShownQueueId = activeId;
          DownloadItem activeItem = m_engine.GetItem(activeId);
          if (activeItem.status == DownloadStatus::Downloading) {
            ShowDownloadProgressDialog(m_hWnd, &activeItem, &m_engine);
          }
        }
      }
      return 0;
    }

    case WM_COPYDATA: {
      PCOPYDATASTRUCT pCDS = (PCOPYDATASTRUCT)lParam;
      if (pCDS && pCDS->lpData && pCDS->cbData > 0) {
        std::wstring dataStr((const wchar_t *)pCDS->lpData,
                             pCDS->cbData / sizeof(wchar_t));
        while (!dataStr.empty() && (dataStr.back() == L'\0' || dataStr.back() == L'\r' || dataStr.back() == L'\n')) {
          dataStr.pop_back();
        }

        std::wstring url, filename, referer, cookies, userAgent;

        if (dataStr.rfind(L"{", 0) == 0) {
          // JSON payload
          int u8Len = WideCharToMultiByte(CP_UTF8, 0, dataStr.c_str(), (int)dataStr.length(), NULL, 0, NULL, NULL);
          std::string u8Str(u8Len, 0);
          WideCharToMultiByte(CP_UTF8, 0, dataStr.c_str(), (int)dataStr.length(), &u8Str[0], u8Len, NULL, NULL);

          std::string u8Url = LocalServerBridge::ExtractJsonStringField(u8Str, "url");
          if (u8Url.empty()) u8Url = LocalServerBridge::ExtractJsonStringField(u8Str, "link");
          std::string u8Filename = LocalServerBridge::ExtractJsonStringField(u8Str, "filename");
          std::string u8Referer = LocalServerBridge::ExtractJsonStringField(u8Str, "referer");
          if (u8Referer.empty()) u8Referer = LocalServerBridge::ExtractJsonStringField(u8Str, "referrer");
          std::string u8Cookies = LocalServerBridge::ExtractJsonStringField(u8Str, "cookies");
          std::string u8UserAgent = LocalServerBridge::ExtractJsonStringField(u8Str, "userAgent");

          url = LocalServerBridge::Utf8ToWide(u8Url);
          filename = LocalServerBridge::Utf8ToWide(u8Filename);
          referer = LocalServerBridge::Utf8ToWide(u8Referer);
          cookies = LocalServerBridge::Utf8ToWide(u8Cookies);
          userAgent = LocalServerBridge::Utf8ToWide(u8UserAgent);
        } else {
          // Tab-delimited tokens
          std::vector<std::wstring> tokens;
          size_t start = 0;
          while (start < dataStr.length()) {
            size_t tab = dataStr.find(L'\t', start);
            if (tab == std::wstring::npos) {
              tokens.push_back(dataStr.substr(start));
              break;
            }
            tokens.push_back(dataStr.substr(start, tab - start));
            start = tab + 1;
          }

          url = tokens.size() > 0 ? tokens[0] : L"";
          filename = tokens.size() > 1 ? tokens[1] : L"";
          referer = tokens.size() > 2 ? tokens[2] : L"";
          cookies = tokens.size() > 3 ? tokens[3] : L"";
          userAgent = tokens.size() > 4 ? tokens[4] : L"";
        }

        {
          CreateDirectoryW(L"C:\\temp", NULL);
          std::wofstream dbg(L"C:\\temp\\dm_debug.txt", std::ios::app);
          if (dbg.is_open()) {
            dbg << L"=== [MAINWINDOW WM_COPYDATA RECEIVED] ===" << std::endl;
            dbg << L"URL: " << url << std::endl;
            dbg << L"Filename: " << filename << std::endl;
            dbg << L"Referer: " << referer << std::endl;
            dbg << L"Cookies: " << cookies << std::endl;
            dbg << L"UserAgent: " << userAgent << std::endl;
            dbg << L"==========================================" << std::endl << std::endl;
            dbg.close();
          }
        }

        SetForegroundWindow(m_hWnd);
        TriggerAddFromUrl(url, filename, referer, cookies, userAgent);
        return 1;
      }
      return 0;
    }

    case WM_USER + 200: { // Incoming request from browser extension HTTP bridge
      auto *req = (BridgeDownloadRequest *)wParam;
      if (req) {
        TriggerAddFromUrl(req->url, req->filename, req->referer, req->cookies, req->userAgent, req->quality, req->originalPageUrl, req->totalSize, req->videoSize, req->audioSize);
        delete req;
      }
      return 0;
    }

    case WM_USER +
        201: { // CLI-launched URL (deferred so message loop is running)
      auto *req = (BridgeDownloadRequest *)wParam;
      if (req) {
        TriggerAddFromUrl(req->url, req->filename, req->referer, req->cookies, req->userAgent, req->quality, req->originalPageUrl, req->totalSize, req->videoSize, req->audioSize);
        delete req;
      }
      return 0;
    }

    case WM_USER + 202: { // Browser-assisted stream relay started: show progress dialog
      auto *item = (DownloadItem *)wParam;
      if (item) {
        PopulateListView();
        ShowDownloadProgressDialog(NULL, item, &m_engine);
        delete item;
      }
      return 0;
    }

    case WM_USER + 203: { // Stream relay completed: show download complete dialog
      auto *item = (DownloadItem *)wParam;
      if (item) {
        PopulateListView();
        ShowDownloadCompleteDialog(NULL, *item);
        delete item;
      }
      return 0;
    }

    case WM_NOTIFY: {
      LPNMHDR pnm = (LPNMHDR)lParam;

      // --- Toolbar custom draw with transparent text background ---
      if (pnm->hwndFrom == m_hToolbar && pnm->code == NM_CUSTOMDRAW) {
        LPNMTBCUSTOMDRAW pCD = (LPNMTBCUSTOMDRAW)lParam;
        if (pCD->nmcd.dwDrawStage == CDDS_PREPAINT) {
          SetBkMode(pCD->nmcd.hdc, TRANSPARENT);
          HBRUSH hbr = GetSysColorBrush(COLOR_BTNFACE);
          FillRect(pCD->nmcd.hdc, &pCD->nmcd.rc, hbr);
          return CDRF_NOTIFYITEMDRAW;
        }
        if (pCD->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
          SetBkMode(pCD->nmcd.hdc, TRANSPARENT);
          return CDRF_DODEFAULT;
        }
        return CDRF_DODEFAULT;
      }

      if (pnm->idFrom == 4000) { // ListView
        if (pnm->code == NM_DBLCLK) {
          int index = GetSelectedListViewIndex();
          if (index >= 0) {
            auto items = m_engine.GetDownloads();
            if ((size_t)index < items.size()) {
              auto &it = items[index];
              if (it.status == DownloadStatus::Downloading ||
                  it.status == DownloadStatus::Paused) {
                ShowDownloadProgressDialog(m_hWnd, &it, &m_engine);
              } else if (it.status == DownloadStatus::Complete) {
                // Open the real downloaded file with default media player /
                // viewer!
                ShellExecuteW(m_hWnd, L"open", it.savePath.c_str(), NULL, NULL,
                              SW_SHOWNORMAL);
              } else {
                ShowFilePropertiesDialog(m_hWnd, &it);
                m_engine.UpdateItem(it);
                PopulateListView();
              }
            }
          }
        } else if (pnm->code == NM_RCLICK) {
          // Hit test to check right-clicked item
          POINT pt;
          GetCursorPos(&pt);
          POINT clientPt = pt;
          ScreenToClient(m_hListView, &clientPt);

          LVHITTESTINFO hti = {0};
          hti.pt = clientPt;
          int hitIndex = ListView_HitTest(m_hListView, &hti);

          auto selectedIndices = GetSelectedListViewIndices();
          bool hitInSelection = false;
          if (hitIndex >= 0) {
            for (int idx : selectedIndices) {
              if (idx == hitIndex) {
                hitInSelection = true;
                break;
              }
            }
            if (!hitInSelection) {
              // Select only this clicked item
              ListView_SetItemState(m_hListView, -1, 0, LVIS_SELECTED);
              ListView_SetItemState(m_hListView, hitIndex,
                                    LVIS_SELECTED | LVIS_FOCUSED,
                                    LVIS_SELECTED | LVIS_FOCUSED);
              selectedIndices = { hitIndex };
            }
          }

          if (!selectedIndices.empty()) {
            HMENU hMenu = CreatePopupMenu();
            if (selectedIndices.size() == 1) {
              AppendMenuW(hMenu, MF_STRING, 5003, L"Open File");
              AppendMenuW(hMenu, MF_STRING, 5004, L"Open Containing Folder");
              AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
              AppendMenuW(hMenu, MF_STRING, 5001, L"Properties...");
              AppendMenuW(hMenu, MF_STRING, 5002, L"Download Progress Dialog...");
              AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
              AppendMenuW(hMenu, MF_STRING, 2001, L"Resume Download");
              AppendMenuW(hMenu, MF_STRING, 2002, L"Stop Download");
              AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
              AppendMenuW(hMenu, MF_STRING, 2003, L"Delete from List");
            } else {
              std::wstring delLabel = L"Delete Selected (" + std::to_wstring(selectedIndices.size()) + L" files)";
              AppendMenuW(hMenu, MF_STRING, 2001, L"Resume Selected");
              AppendMenuW(hMenu, MF_STRING, 2002, L"Stop Selected");
              AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
              AppendMenuW(hMenu, MF_STRING, 2003, delLabel.c_str());
            }
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hWnd, NULL);
            DestroyMenu(hMenu);
          }
        }
      }
      break;
    }

    case WM_COMMAND: {
      int id = LOWORD(wParam);

      if (id == 1001) { // Add URL -> Launches the exact "Download File Info"
                        // dialog (screenshot replica)
        OutputDebugStringW(L"[IDM Debug] Add URL Clicked\n");
        DownloadItem item;
        item.id = L"dl-" + std::to_wstring(GetTickCount());

        std::wstring clipUrl = GetClipboardUrl();
        if (!clipUrl.empty()) {
          item.url = clipUrl;
          if (DownloadEngine::IsStreamingMediaURL(clipUrl)) {
            std::wstring exactTitle = DownloadEngine::GetMediaTitle(clipUrl);
            item.filename = (!exactTitle.empty()) ? (exactTitle + L".mp4") : L"Stream_Video.mp4";
            item.category = L"Video";
            item.savePath = L"C:\\Users\\UHD\\Downloads\\Video\\" + item.filename;
          } else {
            size_t slash = clipUrl.find_last_of(L'/');
            item.filename =
                (slash != std::wstring::npos && slash + 1 < clipUrl.length())
                    ? clipUrl.substr(slash + 1)
                    : L"download.bin";
            size_t qmark = item.filename.find(L'?');
            if (qmark != std::wstring::npos)
              item.filename = item.filename.substr(0, qmark);
            item.category = L"General";
            item.savePath =
                L"C:\\Users\\UHD\\Downloads\\General\\" + item.filename;
          }
        } else {
          item.url =
              L"https://www.internetdownloadmanager.com/languages/idm_al.zip";
          item.filename = L"idm_al.zip";
          item.category = L"Compressed";
          item.savePath = L"C:\\Users\\UHD\\Downloads\\Compressed\\idm_al.zip";
        }

        item.sizeBytes = 0;
        item.downloadedBytes = 0;
        item.status = DownloadStatus::Downloading;
        item.connections = 16;
        item.lastTryDate = L"Aug 22, 2026";
        item.description = L"Direct download";

        bool startImmediately = true;
        if (ShowDownloadFileInfoDialog(m_hWnd, item, startImmediately)) {
          item.status = startImmediately ? DownloadStatus::Downloading
                                         : DownloadStatus::Queued;
          m_engine.AddItem(item);
          if (startImmediately) {
            m_engine.StartDownload(item.id);
          }
          PopulateListView();
          if (startImmediately) {
            ShowDownloadProgressDialog(m_hWnd, &item, &m_engine);
          }
        }
      } else if (id == 2001) { // Resume / Start Selected
        OutputDebugStringW(L"[IDM Debug] Start Download Clicked\n");
        auto indices = GetSelectedListViewIndices();
        auto items = m_engine.GetDownloads();
        for (int idx : indices) {
          if (idx >= 0 && (size_t)idx < items.size()) {
            m_engine.StartDownload(items[idx].id);
          }
        }
        PopulateListView();
        if (indices.size() == 1 && indices[0] < (int)items.size()) {
          ShowDownloadProgressDialog(m_hWnd, &items[indices[0]], &m_engine);
        }
      } else if (id == 2002) { // Stop Selected
        auto indices = GetSelectedListViewIndices();
        auto items = m_engine.GetDownloads();
        for (int idx : indices) {
          if (idx >= 0 && (size_t)idx < items.size()) {
            m_engine.PauseDownload(items[idx].id);
          }
        }
        PopulateListView();
      } else if (id == 1003) { // Stop All / Stop Queue
        m_engine.StopQueue();
        m_engine.StopAll();
        PopulateListView();
      } else if (id == 1004) { // Start Queue (Sequential One-by-One Queue)
        if (!m_engine.StartQueue()) {
          MessageBoxW(m_hWnd,
              L"The download queue is empty.\n\nTo add files to the download queue, click 'Download Later' in the Download File Info dialog.",
              L"Download Queue", MB_ICONINFORMATION | MB_OK);
        } else {
          PopulateListView();
          std::wstring activeId = m_engine.GetActiveQueueItemId();
          if (!activeId.empty()) {
            DownloadItem activeItem = m_engine.GetItem(activeId);
            if (!activeItem.id.empty()) {
              ShowDownloadProgressDialog(m_hWnd, &activeItem, &m_engine);
            }
          }
        }
        PopulateListView();
      } else if (id == 2003) { // Delete Selected
        auto indices = GetSelectedListViewIndices();
        if (!indices.empty()) {
          auto items = m_engine.GetDownloads();
          std::vector<std::wstring> idsToDelete;
          for (int idx : indices) {
            if (idx >= 0 && (size_t)idx < items.size()) {
              idsToDelete.push_back(items[idx].id);
            }
          }
          for (const auto &delId : idsToDelete) {
            m_engine.DeleteItem(delId);
          }
          ListView_SetItemState(m_hListView, -1, 0, LVIS_SELECTED);
          PopulateListView();
        }
      } else if (id == 1012) { // Delete Completed
        auto items = m_engine.GetDownloads();
        for (const auto &it : items) {
          if (it.status == DownloadStatus::Complete) {
            m_engine.DeleteItem(it.id);
          }
        }
        PopulateListView();
      } else if (id == 1010) { // Options
        ShowOptionsDialog(m_hWnd, &m_settings);
      } else if (id == 1011) { // Scheduler
        ShowSchedulerDialog(m_hWnd);
      } else if (id == 1002) { // Media Sniffer
        DownloadItem streamItem;
        bool shouldAdd = false;
        ShowMediaSnifferDialog(m_hWnd, streamItem, shouldAdd);
        if (shouldAdd) {
          bool startImmediately = true;
          if (ShowDownloadFileInfoDialog(m_hWnd, streamItem,
                                         startImmediately)) {
            streamItem.status = startImmediately ? DownloadStatus::Downloading
                                                 : DownloadStatus::Queued;
            m_engine.AddItem(streamItem);
            if (startImmediately) {
              m_engine.StartDownload(streamItem.id);
            }
            PopulateListView();
            if (startImmediately) {
              ShowDownloadProgressDialog(m_hWnd, &streamItem, &m_engine);
            }
          }
        }
      } else if (id == 5003) { // Open File
        int index = GetSelectedListViewIndex();
        if (index >= 0) {
          auto items = m_engine.GetDownloads();
          if ((size_t)index < items.size()) {
            ShellExecuteW(m_hWnd, L"open", items[index].savePath.c_str(), NULL,
                          NULL, SW_SHOWNORMAL);
          }
        }
      } else if (id == 5004) { // Open Containing Folder
        int index = GetSelectedListViewIndex();
        if (index >= 0) {
          auto items = m_engine.GetDownloads();
          if ((size_t)index < items.size()) {
            std::wstring selectArg =
                L"/select,\"" + items[index].savePath + L"\"";
            ShellExecuteW(m_hWnd, L"open", L"explorer.exe", selectArg.c_str(),
                          NULL, SW_SHOWNORMAL);
          }
        }
      } else if (id == 5001) { // Properties
        int index = GetSelectedListViewIndex();
        if (index >= 0) {
          auto items = m_engine.GetDownloads();
          if ((size_t)index < items.size()) {
            auto &it = items[index];
            ShowFilePropertiesDialog(m_hWnd, &it);
            m_engine.UpdateItem(it);
            PopulateListView();
          }
        }
      } else if (id == 5002) { // Download Progress Dialog
        int index = GetSelectedListViewIndex();
        if (index >= 0) {
          auto items = m_engine.GetDownloads();
          if ((size_t)index < items.size()) {
            ShowDownloadProgressDialog(m_hWnd, &items[index], &m_engine);
          }
        }
      } else if (id == 1005) { // Exit
        PostQuitMessage(0);
      } else if (id == 1030) { // About
        MessageBoxW(m_hWnd,
                    L"PDM Download Manager\nEngine: 16-thread Range "
                    L"Acceleration & Media Sniffer\nAll rights reserved.",
                    L"About PDM Download Manager", MB_OK | MB_ICONINFORMATION);
      }
      return 0;
    }

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    }
    return DefWindowProcW(m_hWnd, message, wParam, lParam);
  }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
  // Check if another instance with an active window is already running
  HWND hWndExisting = FindWindowW(L"IDM_Native_MainWindowClass", NULL);
  if (hWndExisting && IsWindow(hWndExisting)) {
    std::wstring cmdLine = GetCommandLineW();
    auto ExtractCliValue = [](const std::wstring &cmd, const std::wstring &flag) -> std::wstring {
      size_t flagPos = cmd.find(flag + L" \"");
      if (flagPos != std::wstring::npos) {
        flagPos += flag.length() + 2;
        size_t endPos = flagPos;
        std::wstring result;
        while (endPos < cmd.length()) {
          if (cmd[endPos] == L'\\' && endPos + 1 < cmd.length() && cmd[endPos + 1] == L'"') {
            result += L'"';
            endPos += 2;
          } else if (cmd[endPos] == L'"') {
            break;
          } else {
            result += cmd[endPos++];
          }
        }
        return result;
      }
      return L"";
    };

    std::wstring sniffedUrl = ExtractCliValue(cmdLine, L"--download-url");
    if (sniffedUrl.empty()) sniffedUrl = ExtractCliValue(cmdLine, L"-u");

    if (!sniffedUrl.empty()) {
      std::wstring sniffedFilename = ExtractCliValue(cmdLine, L"--filename");
      std::wstring sniffedReferer = ExtractCliValue(cmdLine, L"--referer");
      std::wstring sniffedCookies = ExtractCliValue(cmdLine, L"--cookies");
      std::wstring sniffedUserAgent = ExtractCliValue(cmdLine, L"--user-agent");

      std::wstring dataStr = sniffedUrl + L"\t" + sniffedFilename + L"\t" + sniffedReferer + L"\t" + sniffedCookies + L"\t" + sniffedUserAgent;
      COPYDATASTRUCT cds = { 0 };
      cds.dwData = 1001;
      cds.cbData = (DWORD)(dataStr.length() * sizeof(wchar_t));
      cds.lpData = (PVOID)dataStr.data();
      SendMessageW(hWndExisting, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
    }

    SetForegroundWindow(hWndExisting);
    ShowWindow(hWndExisting, SW_RESTORE);
    return 0;
  }

  MainWindow win;
  HWND hWnd = win.Create();
  if (!hWnd) {
    return 1;
  }

  ShowWindow(hWnd, SW_SHOWNORMAL);
  UpdateWindow(hWnd);
  SetForegroundWindow(hWnd);

  // Check if launched with CLI parameters from Chrome/Edge sniffer or Native Host
  // IMPORTANT: Post a deferred message (WM_USER+201) instead of calling
  // TriggerAddFromUrl() directly here — the Win32 message loop hasn't
  // started yet, so any modal dialog opened now would be unresponsive/frozen.
  std::wstring cmdLine = GetCommandLineW();

  auto ExtractCliValue = [](const std::wstring &cmd, const std::wstring &flag) -> std::wstring {
    size_t flagPos = cmd.find(flag + L" \"");
    if (flagPos != std::wstring::npos) {
      flagPos += flag.length() + 2; // skip flag + "
      size_t endPos = flagPos;
      std::wstring result;
      while (endPos < cmd.length()) {
        if (cmd[endPos] == L'\\' && endPos + 1 < cmd.length() && cmd[endPos + 1] == L'"') {
          result += L'"';
          endPos += 2;
        } else if (cmd[endPos] == L'"') {
          break;
        } else {
          result += cmd[endPos++];
        }
      }
      return result;
    }
    return L"";
  };

  std::wstring sniffedUrl = ExtractCliValue(cmdLine, L"--download-url");
  if (sniffedUrl.empty()) sniffedUrl = ExtractCliValue(cmdLine, L"-u");

  if (!sniffedUrl.empty()) {
    std::wstring sniffedFilename = ExtractCliValue(cmdLine, L"--filename");
    std::wstring sniffedReferer = ExtractCliValue(cmdLine, L"--referer");
    std::wstring sniffedCookies = ExtractCliValue(cmdLine, L"--cookies");
    std::wstring sniffedUserAgent = ExtractCliValue(cmdLine, L"--user-agent");

    auto *req = new BridgeDownloadRequest{sniffedUrl, sniffedFilename, sniffedReferer, sniffedCookies, sniffedUserAgent};
    PostMessageW(hWnd, WM_USER + 201, (WPARAM)req, 0);
  }

  MSG msg;
  while (GetMessageW(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return (int)msg.wParam;
}
