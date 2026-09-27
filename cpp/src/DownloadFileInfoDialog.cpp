#include "../include/Models.hpp"
#include "../include/DownloadEngine.hpp"
#include "../include/SegmentedDownloader.hpp"
#include "../include/IconFactory.hpp"
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <sstream>
#include <iomanip>
#include <thread>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")

// Ensures directory exists recursively
inline void EnsureDirectoryExists(const std::wstring& path) {
    wchar_t folder[MAX_PATH] = { 0 };
    wcsncpy_s(folder, path.c_str(), MAX_PATH - 1);
    PathRemoveFileSpecW(folder);
    if (wcslen(folder) > 0) {
        SHCreateDirectoryExW(NULL, folder, NULL);
    }
}

// Get user's default Downloads directory
inline std::wstring GetDefaultDownloadsFolder() {
    wchar_t* pPath = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &pPath))) {
        std::wstring res = pPath;
        CoTaskMemFree(pPath);
        if (!res.empty() && res.back() != L'\\') res += L'\\';
        return res;
    }
    return L"C:\\Downloads\\";
}

static std::wstring FormatFileSizeDisplay(uint64_t bytes) {
    if (bytes == 0) return L"Unknown size";
    wchar_t buf[64];
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        swprintf_s(buf, 64, L"%.2f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024ULL * 1024ULL) {
        swprintf_s(buf, 64, L"%.2f MB", (double)bytes / (1024.0 * 1024.0));
    } else {
        swprintf_s(buf, 64, L"%.2f KB", (double)bytes / 1024.0);
    }
    return buf;
}

struct MediaProbeCacheEntry {
    std::wstring title;
    uint64_t videoSize = 0;
    uint64_t audioSize = 0;
};
static std::unordered_map<std::wstring, MediaProbeCacheEntry> s_streamingCache;
static std::mutex s_streamingCacheMutex;

void CacheStreamingMediaSizes(const std::wstring& url, const std::wstring& title, uint64_t videoSize, uint64_t audioSize) {
    if (url.empty()) return;
    std::lock_guard<std::mutex> lock(s_streamingCacheMutex);
    auto& entry = s_streamingCache[url];
    if (!title.empty()) entry.title = title;
    if (videoSize > 0) entry.videoSize = videoSize;
    if (audioSize > 0) entry.audioSize = audioSize;
}

struct DownloadFileInfoState {
    DownloadItem* pItem = nullptr;
    bool startImmediately = true;
    bool confirmed = false;
    HWND hDlg = NULL;
    HWND hUrlEdit = NULL;
    HWND hCatCombo = NULL;
    HWND hSaveEdit = NULL;
    HWND hPathBox = NULL;
    HWND hDescEdit = NULL;
    HWND hCatIconPic = NULL;
    HWND hSizeLabel = NULL;
    HWND hPreviewBtn = NULL;
    HICON hCurrentCatIcon = NULL;
    std::wstring baseDownloads;
};

static LRESULT CALLBACK DownloadFileInfoWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    DownloadFileInfoState* pState = (DownloadFileInfoState*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (message) {
    case WM_NCCREATE: {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pState = (DownloadFileInfoState*)pCreate->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pState);
        pState->hDlg = hWnd;
        return TRUE;
    }
    case WM_CREATE: {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        // Top App Logo
        HICON hAppLogo = (HICON)LoadImageW(GetModuleHandle(NULL), MAKEINTRESOURCEW(101), IMAGE_ICON, 18, 18, 0);
        if (!hAppLogo) {
            hAppLogo = (HICON)LoadImageW(NULL, L"app_icon.ico", IMAGE_ICON, 18, 18, LR_LOADFROMFILE);
        }
        if (!hAppLogo) {
            hAppLogo = (HICON)LoadImageW(NULL, L"d:\\Download Manager AB\\app_icon.ico", IMAGE_ICON, 18, 18, LR_LOADFROMFILE);
        }
        if (hAppLogo) {
            SendMessageW(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hAppLogo);
            SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hAppLogo);
        }

        // Check if item is an audio item
        bool isInitialAudio = (pState->pItem->quality.find(L"Audio") != std::wstring::npos ||
                               pState->pItem->quality.find(L"MP3") != std::wstring::npos ||
                               pState->pItem->quality.find(L"M4A") != std::wstring::npos ||
                               pState->pItem->category == L"Music" ||
                               pState->pItem->filename.find(L".mp3") != std::wstring::npos);
        if (isInitialAudio) {
            pState->pItem->category = L"Music";
            std::wstring fn = pState->pItem->filename;
            size_t dotPos = fn.find_last_of(L'.');
            if (dotPos != std::wstring::npos) {
                pState->pItem->filename = fn.substr(0, dotPos) + L".mp3";
            } else {
                pState->pItem->filename += L".mp3";
            }
            pState->pItem->savePath = pState->baseDownloads + L"Music\\" + pState->pItem->filename;
        }

        // URL row
        HWND hUrlLbl = CreateWindowW(L"STATIC", L"URL", WS_CHILD | WS_VISIBLE, 18, 18, 70, 18, hWnd, NULL, NULL, NULL);
        pState->hUrlEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pState->pItem->url.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 90, 16, 375, 22, hWnd, (HMENU)101, NULL, NULL);

        // Category row
        HWND hCatLbl = CreateWindowW(L"STATIC", L"Category", WS_CHILD | WS_VISIBLE, 18, 48, 70, 18, hWnd, NULL, NULL, NULL);
        pState->hCatCombo = CreateWindowW(WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 90, 46, 170, 140, hWnd, (HMENU)102, NULL, NULL);
        HWND hCatPlus = CreateWindowW(L"BUTTON", L"+", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 268, 45, 24, 24, hWnd, NULL, NULL, NULL);

        // Save As row
        HWND hSaveLbl = CreateWindowW(L"STATIC", L"Save As", WS_CHILD | WS_VISIBLE, 18, 78, 70, 18, hWnd, NULL, NULL, NULL);
        pState->hSaveEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pState->pItem->savePath.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 90, 76, 340, 22, hWnd, (HMENU)103, NULL, NULL);
        HWND hBrowseBtn = CreateWindowW(L"BUTTON", L"...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 436, 75, 28, 24, hWnd, (HMENU)201, NULL, NULL);

        // Checkbox: Remember path
        HWND hChkRem = CreateWindowW(L"BUTTON", L"Remember this path for \"Selected\" category", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 90, 106, 350, 18, hWnd, (HMENU)105, NULL, NULL);
        SendMessageW(hChkRem, BM_SETCHECK, BST_CHECKED, 0);

        // Folder preview box
        std::wstring catDir = pState->baseDownloads + (pState->pItem->category.empty() ? L"General" : pState->pItem->category) + L"\\";
        pState->hPathBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", catDir.c_str(), WS_CHILD | WS_VISIBLE | ES_READONLY, 90, 128, 375, 22, hWnd, (HMENU)104, NULL, NULL);

        // Description row
        HWND hDescLbl = CreateWindowW(L"STATIC", L"Description", WS_CHILD | WS_VISIBLE, 18, 160, 70, 18, hWnd, NULL, NULL, NULL);
        pState->hDescEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pState->pItem->description.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 90, 158, 375, 22, hWnd, (HMENU)106, NULL, NULL);

        // Right side: Category Icon, File Size, Preview Button
        pState->hCatIconPic = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ICON, 495, 24, 48, 48, hWnd, (HMENU)107, NULL, NULL);
        
        std::wstring sizeText = FormatFileSizeDisplay(pState->pItem->sizeBytes);
        pState->hSizeLabel = CreateWindowW(L"STATIC", sizeText.c_str(), WS_CHILD | WS_VISIBLE | SS_CENTER, 470, 78, 100, 18, hWnd, (HMENU)108, NULL, NULL);
        pState->hPreviewBtn = CreateWindowW(L"BUTTON", L"Preview", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 485, 102, 70, 24, hWnd, (HMENU)202, NULL, NULL);

        // Action buttons at bottom (tightened closer to the content rows)
        HWND hLaterBtn = CreateWindowW(L"BUTTON", L"Download Later", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 100, 195, 110, 26, hWnd, (HMENU)301, NULL, NULL);
        HWND hStartBtn = CreateWindowW(L"BUTTON", L"Start Download", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 220, 195, 120, 26, hWnd, (HMENU)IDOK, NULL, NULL);
        HWND hCancelBtn = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 350, 195, 80, 26, hWnd, (HMENU)IDCANCEL, NULL, NULL);

        // Set fonts
        SendMessageW(hUrlLbl, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(pState->hUrlEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hCatLbl, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(pState->hCatCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hCatPlus, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hSaveLbl, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(pState->hSaveEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hBrowseBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hChkRem, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(pState->hPathBox, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hDescLbl, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(pState->hDescEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(pState->hSizeLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(pState->hPreviewBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hLaterBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hStartBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hCancelBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Populate Categories
        SendMessageW(pState->hCatCombo, CB_ADDSTRING, 0, (LPARAM)L"General");
        SendMessageW(pState->hCatCombo, CB_ADDSTRING, 0, (LPARAM)L"Compressed");
        SendMessageW(pState->hCatCombo, CB_ADDSTRING, 0, (LPARAM)L"Documents");
        SendMessageW(pState->hCatCombo, CB_ADDSTRING, 0, (LPARAM)L"Music");
        SendMessageW(pState->hCatCombo, CB_ADDSTRING, 0, (LPARAM)L"Programs");
        SendMessageW(pState->hCatCombo, CB_ADDSTRING, 0, (LPARAM)L"Video");

        int sel = 0;
        if (pState->pItem->category == L"Compressed") sel = 1;
        else if (pState->pItem->category == L"Documents") sel = 2;
        else if (pState->pItem->category == L"Music" || isInitialAudio) sel = 3;
        else if (pState->pItem->category == L"Programs") sel = 4;
        else if (pState->pItem->category == L"Video") sel = 5;
        SendMessageW(pState->hCatCombo, CB_SETCURSEL, sel, 0);

        // Set initial category icon (check extension for dedicated badges like PDF, DOCX, Music)
        std::wstring initialCat = (isInitialAudio || pState->pItem->category == L"Music") ? L"Music" : (pState->pItem->category.empty() ? L"General" : pState->pItem->category);
        std::wstring fn = pState->pItem->filename;
        size_t dotPos = fn.find_last_of(L'.');
        if (dotPos != std::wstring::npos) {
            std::wstring ext = fn.substr(dotPos + 1);
            for (auto& c : ext) c = towlower(c);
            if (ext == L"pdf") initialCat = L"PDF";
            else if (ext == L"docx" || ext == L"doc") initialCat = L"DOCX";
            else if (ext == L"mp3" || ext == L"m4a" || ext == L"wav" || ext == L"flac") initialCat = L"Music";
        }
        if (isInitialAudio) {
            initialCat = L"Music";
        }
        pState->hCurrentCatIcon = IconFactory::CreateCategoryIcon(44, initialCat);
        if (pState->hCurrentCatIcon) {
            SendMessageW(pState->hCatIconPic, STM_SETICON, (WPARAM)pState->hCurrentCatIcon, 0);
        }

        // If it's a streaming URL, probe and update real title and approximate filesize
        if (DownloadEngine::IsStreamingMediaURL(pState->pItem->url)) {
            std::wstring url = pState->pItem->url;
            std::wstring quality = pState->pItem->quality;
            HWND hSave = pState->hSaveEdit;
            HWND hSize = pState->hSizeLabel;
            HWND hCatCombo = pState->hCatCombo;
            HWND hPathBox = pState->hPathBox;
            std::wstring baseDownloads = pState->baseDownloads;
            DownloadItem* pTargetItem = pState->pItem;

            bool hitCache = false;
            {
                std::lock_guard<std::mutex> lock(s_streamingCacheMutex);
                auto it = s_streamingCache.find(url);
                if (it != s_streamingCache.end()) {
                    bool isAudio = (pTargetItem->category == L"Music" || isInitialAudio);
                    uint64_t sz = isAudio ? it->second.audioSize : it->second.videoSize;
                    if (sz > 0) {
                        pTargetItem->sizeBytes = sz;
                        SetWindowTextW(hSize, FormatFileSizeDisplay(sz).c_str());
                        hitCache = true;
                    }
                    if (!it->second.title.empty()) {
                        std::wstring ext = isAudio ? L".mp3" : L".mp4";
                        std::wstring targetCat = isAudio ? L"Music" : (pTargetItem->category.empty() ? L"Video" : pTargetItem->category);
                        std::wstring newFilename = it->second.title + ext;
                        std::wstring newSavePath = baseDownloads + targetCat + L"\\" + newFilename;
                        pTargetItem->filename = newFilename;
                        pTargetItem->savePath = newSavePath;
                        pTargetItem->category = targetCat;
                        SetWindowTextW(hSave, newSavePath.c_str());
                        SetWindowTextW(hPathBox, (baseDownloads + targetCat + L"\\").c_str());
                    }
                }
            }

            if (!hitCache) {
                std::thread([url, quality, hSave, hSize, hCatCombo, hPathBox, baseDownloads, pTargetItem, hWnd]() {
                    wchar_t szPath[MAX_PATH] = { 0 };
                    GetModuleFileNameW(NULL, szPath, MAX_PATH);
                    PathRemoveFileSpecW(szPath);
                    std::wstring ytDlp = std::wstring(szPath) + L"\\..\\..\\tools\\yt-dlp.exe";
                    if (!PathFileExistsW(ytDlp.c_str())) ytDlp = L"d:\\Download Manager AB\\tools\\yt-dlp.exe";

                    std::wstring fmt;
                    bool isAudioTarget = (pTargetItem->category == L"Music" ||
                                          quality.find(L"Audio") != std::wstring::npos ||
                                          quality.find(L"MP3") != std::wstring::npos ||
                                          quality.find(L"M4A") != std::wstring::npos);

                    if (isAudioTarget) {
                        fmt = L"bestaudio[acodec^=mp4a]/bestaudio[ext=m4a]/bestaudio[acodec^=aac]/bestaudio/best";
                    } else if (quality.find(L"1080") != std::wstring::npos) {
                        fmt = L"bestvideo[vcodec^=avc1][height<=1080]+bestaudio[acodec^=mp4a]/"
                              L"bestvideo[vcodec^=avc][height<=1080]+bestaudio[acodec^=mp4a]/"
                              L"bestvideo[height<=1080]+bestaudio/"
                              L"best[height<=1080]/best";
                    } else if (quality.find(L"720") != std::wstring::npos) {
                        fmt = L"bestvideo[vcodec^=avc1][height<=720]+bestaudio[acodec^=mp4a]/"
                              L"bestvideo[vcodec^=avc][height<=720]+bestaudio[acodec^=mp4a]/"
                              L"bestvideo[height<=720]+bestaudio/"
                              L"best[height<=720]/best";
                    } else if (quality.find(L"480") != std::wstring::npos) {
                        fmt = L"bestvideo[vcodec^=avc1][height<=480]+bestaudio[acodec^=mp4a]/"
                              L"bestvideo[vcodec^=avc][height<=480]+bestaudio[acodec^=mp4a]/"
                              L"bestvideo[height<=480]+bestaudio/"
                              L"best[height<=480]/best";
                    } else {
                        fmt = L"bestvideo[vcodec^=avc1]+bestaudio[acodec^=mp4a]/"
                              L"bestvideo+bestaudio/best";
                    }

                    std::wstring cmd = L"\"" + ytDlp + L"\" --no-playlist --no-warnings --print \"%(title)s\" --print \"%(filesize,filesize_approx)s\" -f \"" + fmt + L"\" \"" + url + L"\"";

                    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
                    HANDLE hReadPipe, hWritePipe;
                    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) return;
                    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

                    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
                    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
                    si.hStdOutput = hWritePipe;
                    si.hStdError = hWritePipe;
                    si.wShowWindow = SW_HIDE;

                    PROCESS_INFORMATION pi = { 0 };
                    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
                    cmdBuf.push_back(0);

                    BOOL success = CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi);
                    CloseHandle(hWritePipe);

                    if (!success) {
                        CloseHandle(hReadPipe);
                        return;
                    }

                    std::string out;
                    char buffer[1024];
                    DWORD bytesRead = 0;
                    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                        buffer[bytesRead] = 0;
                        out += buffer;
                    }
                    CloseHandle(hReadPipe);
                    WaitForSingleObject(pi.hProcess, 10000);
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);

                    std::istringstream iss(out);
                    std::string titleLine, sizeLine;
                    std::getline(iss, titleLine);
                    std::getline(iss, sizeLine);

                    while (!titleLine.empty() && (titleLine.back() == '\r' || titleLine.back() == '\n' || titleLine.back() == ' ')) titleLine.pop_back();
                    while (!titleLine.empty() && (titleLine.front() == ' ')) titleLine.erase(titleLine.begin());
                    while (!sizeLine.empty() && (sizeLine.back() == '\r' || sizeLine.back() == '\n' || sizeLine.back() == ' ')) sizeLine.pop_back();

                    std::wstring parsedTitle;
                    if (!titleLine.empty() && titleLine != "NA") {
                        int len = MultiByteToWideChar(CP_UTF8, 0, titleLine.c_str(), (int)titleLine.length(), NULL, 0);
                        std::wstring title(len, 0);
                        MultiByteToWideChar(CP_UTF8, 0, titleLine.c_str(), (int)titleLine.length(), &title[0], len);

                        for (auto& ch : title) {
                            if (ch == L'/' || ch == L'\\' || ch == L':' || ch == L'*' ||
                                ch == L'?' || ch == L'"' || ch == L'<' || ch == L'>' || ch == L'|') {
                                ch = L' ';
                            }
                        }
                        while (!title.empty() && (title.back() == L' ' || title.back() == L'\t')) title.pop_back();
                        while (!title.empty() && (title.front() == L' ' || title.front() == L'\t')) title.erase(title.begin());
                        if (title.length() > 120) title = title.substr(0, 120);

                        parsedTitle = title;
                        if (!title.empty()) {
                            bool isAudio = (pTargetItem->category == L"Music" ||
                                            pTargetItem->quality.find(L"Audio") != std::wstring::npos ||
                                            pTargetItem->quality.find(L"MP3") != std::wstring::npos ||
                                            pTargetItem->quality.find(L"M4A") != std::wstring::npos ||
                                            pTargetItem->filename.find(L".mp3") != std::wstring::npos ||
                                            pTargetItem->savePath.find(L"\\Music\\") != std::wstring::npos);
                            std::wstring ext = (pTargetItem->quality.find(L"M4A") != std::wstring::npos) ? L".m4a" : (isAudio ? L".mp3" : L".mp4");
                            std::wstring targetCat = isAudio ? L"Music" : (pTargetItem->category.empty() ? L"Video" : pTargetItem->category);
                            std::wstring targetDir = baseDownloads + targetCat + L"\\";
                            std::wstring newFilename = title + ext;
                            std::wstring newSavePath = targetDir + newFilename;
                            pTargetItem->filename = newFilename;
                            pTargetItem->savePath = newSavePath;
                            pTargetItem->category = targetCat;

                            if (IsWindow(hWnd)) {
                                if (IsWindow(hSave)) SetWindowTextW(hSave, newSavePath.c_str());
                                if (IsWindow(hPathBox)) SetWindowTextW(hPathBox, targetDir.c_str());
                                if (isAudio && IsWindow(hCatCombo)) {
                                    SendMessageW(hCatCombo, CB_SETCURSEL, 3, 0);
                                }
                            }
                        }
                    }

                    uint64_t parsedSize = 0;
                    if (!sizeLine.empty() && sizeLine != "NA") {
                        try {
                            parsedSize = std::stoull(sizeLine);
                            if (parsedSize > 0) {
                                pTargetItem->sizeBytes = parsedSize;
                                if (IsWindow(hWnd) && IsWindow(hSize)) {
                                    SetWindowTextW(hSize, FormatFileSizeDisplay(parsedSize).c_str());
                                }
                            }
                        } catch (...) {}
                    }

                    // Save to memory cache
                    {
                        std::lock_guard<std::mutex> lock(s_streamingCacheMutex);
                        auto& entry = s_streamingCache[url];
                        if (!parsedTitle.empty()) entry.title = parsedTitle;
                        if (parsedSize > 0) {
                            if (isAudioTarget) entry.audioSize = parsedSize;
                            else entry.videoSize = parsedSize;
                        }
                    }
                }).detach();
            }
        }

        // If file size is 0 and it's a direct URL, probe Content-Length in background with WinHTTP
        if (pState->pItem->sizeBytes == 0 && !DownloadEngine::IsStreamingMediaURL(pState->pItem->url)) {
            std::wstring url = pState->pItem->url;
            std::wstring cookies = pState->pItem->cookies;
            std::wstring referer = pState->pItem->referer;
            std::wstring userAgent = pState->pItem->userAgent;
            HWND hSize = pState->hSizeLabel;
            DownloadItem* pTargetItem = pState->pItem;

            std::thread([url, cookies, referer, userAgent, hSize, pTargetItem, hWnd]() {
                SegmentedDownloader probe;
                uint64_t totalSize = 0;
                bool supportsRange = false;
                std::wstring fn;
                probe.m_cookies = cookies;
                probe.m_referer = referer;
                probe.m_customUserAgent = userAgent;
                if (probe.probeUrl(url, totalSize, supportsRange, fn) && totalSize > 0) {
                    pTargetItem->sizeBytes = totalSize;
                    if (IsWindow(hWnd) && IsWindow(hSize)) {
                        std::wstring formatted = FormatFileSizeDisplay(totalSize);
                        SetWindowTextW(hSize, formatted.c_str());
                    }
                }
            }).detach();
        }

        return 0;
    }

    case WM_COMMAND: {
        if (!pState) return 0;
        int id = LOWORD(wParam);

        if (id == 102 && HIWORD(wParam) == CBN_SELCHANGE) {
            int curSel = (int)SendMessageW(pState->hCatCombo, CB_GETCURSEL, 0, 0);
            const wchar_t* cats[] = { L"General", L"Compressed", L"Documents", L"Music", L"Programs", L"Video" };
            if (curSel >= 0 && curSel < 6) {
                pState->pItem->category = cats[curSel];
                bool isMusic = (pState->pItem->category == L"Music");

                // Update filename extension if switching between Video and Music
                std::wstring fn = pState->pItem->filename;
                size_t dotPos = fn.find_last_of(L'.');
                if (isMusic) {
                    pState->pItem->quality = L"Audio Track (MP3)";
                    if (dotPos != std::wstring::npos) {
                        fn = fn.substr(0, dotPos) + L".mp3";
                    } else {
                        fn += L".mp3";
                    }
                } else if (pState->pItem->category == L"Video") {
                    pState->pItem->quality = L"1080p Full HD";
                    if (dotPos != std::wstring::npos) {
                        fn = fn.substr(0, dotPos) + L".mp4";
                    } else {
                        fn += L".mp4";
                    }
                }
                pState->pItem->filename = fn;

                std::wstring newDir = pState->baseDownloads + pState->pItem->category + L"\\";
                EnsureDirectoryExists(newDir + pState->pItem->filename);
                pState->pItem->savePath = newDir + pState->pItem->filename;
                SetWindowTextW(pState->hSaveEdit, pState->pItem->savePath.c_str());
                SetWindowTextW(pState->hPathBox, newDir.c_str());

                // Update category icon
                if (pState->hCurrentCatIcon) DestroyIcon(pState->hCurrentCatIcon);
                pState->hCurrentCatIcon = IconFactory::CreateCategoryIcon(44, pState->pItem->category);
                if (pState->hCurrentCatIcon) {
                    SendMessageW(pState->hCatIconPic, STM_SETICON, (WPARAM)pState->hCurrentCatIcon, 0);
                }

                // If streaming URL, check fast cache or re-probe size for newly selected category!
                if (DownloadEngine::IsStreamingMediaURL(pState->pItem->url)) {
                    std::wstring url = pState->pItem->url;
                    std::wstring quality = pState->pItem->quality;
                    HWND hSize = pState->hSizeLabel;
                    DownloadItem* pTargetItem = pState->pItem;
                    HWND hDlg = hWnd;
                    bool isAudio = (pTargetItem->category == L"Music");

                    bool foundCache = false;
                    {
                        std::lock_guard<std::mutex> lock(s_streamingCacheMutex);
                        auto it = s_streamingCache.find(url);
                        if (it != s_streamingCache.end()) {
                            uint64_t cachedSize = isAudio ? it->second.audioSize : it->second.videoSize;
                            if (cachedSize > 0) {
                                pTargetItem->sizeBytes = cachedSize;
                                SetWindowTextW(hSize, FormatFileSizeDisplay(cachedSize).c_str());
                                foundCache = true;
                            }
                        }
                    }

                    if (!foundCache) {
                        SetWindowTextW(hSize, L"Probing...");

                        std::thread([url, quality, hSize, pTargetItem, hDlg, isAudio]() {
                            wchar_t szPath[MAX_PATH] = { 0 };
                            GetModuleFileNameW(NULL, szPath, MAX_PATH);
                            PathRemoveFileSpecW(szPath);
                            std::wstring ytDlp = std::wstring(szPath) + L"\\..\\..\\tools\\yt-dlp.exe";
                            if (!PathFileExistsW(ytDlp.c_str())) ytDlp = L"d:\\Download Manager AB\\tools\\yt-dlp.exe";

                            std::wstring fmt;
                            if (isAudio || quality.find(L"Audio") != std::wstring::npos || quality.find(L"MP3") != std::wstring::npos) {
                                fmt = L"bestaudio[acodec^=mp4a]/bestaudio[ext=m4a]/bestaudio[acodec^=aac]/bestaudio/best";
                            } else {
                                fmt = L"bestvideo[vcodec^=avc1]+bestaudio[acodec^=mp4a]/bestvideo+bestaudio/best";
                            }

                            std::wstring cmd = L"\"" + ytDlp + L"\" --no-playlist --no-warnings --print \"%(filesize,filesize_approx)s\" -f \"" + fmt + L"\" \"" + url + L"\"";

                            SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
                            HANDLE hReadPipe, hWritePipe;
                            if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) return;
                            SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

                            STARTUPINFOW si = { sizeof(STARTUPINFOW) };
                            si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
                            si.hStdOutput = hWritePipe;
                            si.hStdError = hWritePipe;
                            si.wShowWindow = SW_HIDE;

                            PROCESS_INFORMATION pi = { 0 };
                            std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
                            cmdBuf.push_back(0);

                            BOOL success = CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi);
                            CloseHandle(hWritePipe);
                            if (!success) { CloseHandle(hReadPipe); return; }

                            std::string out;
                            char buffer[1024];
                            DWORD bytesRead = 0;
                            while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                                buffer[bytesRead] = 0;
                                out += buffer;
                            }
                            CloseHandle(hReadPipe);
                            WaitForSingleObject(pi.hProcess, 10000);
                            CloseHandle(pi.hProcess);
                            CloseHandle(pi.hThread);

                            while (!out.empty() && (out.back() == '\r' || out.back() == '\n' || out.back() == ' ')) out.pop_back();
                            while (!out.empty() && (out.front() == ' ')) out.erase(out.begin());

                            if (!out.empty() && out != "NA") {
                                try {
                                    uint64_t parsedSize = std::stoull(out);
                                    if (parsedSize > 0) {
                                        pTargetItem->sizeBytes = parsedSize;
                                        if (IsWindow(hDlg) && IsWindow(hSize)) {
                                            SetWindowTextW(hSize, FormatFileSizeDisplay(parsedSize).c_str());
                                        }

                                        // Update cache
                                        {
                                            std::lock_guard<std::mutex> lock(s_streamingCacheMutex);
                                            auto& entry = s_streamingCache[url];
                                            if (isAudio) entry.audioSize = parsedSize;
                                            else entry.videoSize = parsedSize;
                                        }
                                    }
                                } catch (...) {}
                            }
                        }).detach();
                    }
                }
            }
            return 0;
        }

        if (id == 202) { // "Preview" button
            if (pState && pState->pItem && !pState->pItem->url.empty()) {
                ShellExecuteW(hWnd, L"open", pState->pItem->url.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
            return 0;
        }

        if (id == 201) { // "..." Browse button
            OPENFILENAMEW ofn = { 0 };
            wchar_t szFile[MAX_PATH] = { 0 };
            wchar_t szDir[MAX_PATH] = { 0 };

            if (pState->hSaveEdit && IsWindow(pState->hSaveEdit)) {
                GetWindowTextW(pState->hSaveEdit, szFile, MAX_PATH);
            }
            if (wcslen(szFile) == 0 && pState->pItem) {
                wcsncpy_s(szFile, pState->pItem->savePath.c_str(), MAX_PATH - 1);
            }

            // Extract directory and file name
            wchar_t* pLastSlash = wcsrchr(szFile, L'\\');
            if (pLastSlash) {
                size_t dirLen = pLastSlash - szFile;
                if (dirLen < MAX_PATH) {
                    wcsncpy_s(szDir, szFile, dirLen);
                    szDir[dirLen] = 0;
                }
            }

            // Extract file part and sanitize illegal filename characters for Windows
            std::wstring filePart = pLastSlash ? (pLastSlash + 1) : szFile;
            for (auto& ch : filePart) {
                if (ch == L'/' || ch == L'\\' || ch == L':' || ch == L'*' ||
                    ch == L'?' || ch == L'"' || ch == L'<' || ch == L'>' || ch == L'|') {
                    ch = L' ';
                }
            }
            while (!filePart.empty() && (filePart.back() == L' ' || filePart.back() == L'\t')) filePart.pop_back();
            while (!filePart.empty() && (filePart.front() == L' ' || filePart.front() == L'\t')) filePart.erase(filePart.begin());
            if (filePart.empty()) filePart = L"download.bin";

            wcsncpy_s(szFile, filePart.c_str(), MAX_PATH - 1);

            ofn.lStructSize = sizeof(OPENFILENAMEW);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrInitialDir = (wcslen(szDir) > 0 && PathFileExistsW(szDir)) ? szDir : NULL;
            ofn.lpstrTitle = L"Select Destination File";
            ofn.lpstrFilter = L"All Files (*.*)\0*.*\0Video Files (*.mp4;*.mkv;*.webm)\0*.mp4;*.mkv;*.webm\0Audio Files (*.mp3;*.flac;*.wav)\0*.mp3;*.flac;*.wav\0Documents (*.pdf;*.docx;*.xlsx)\0*.pdf;*.docx;*.xlsx\0Compressed Archives (*.zip;*.rar;*.7z)\0*.zip;*.rar;*.7z\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR | OFN_EXPLORER;

            if (GetSaveFileNameW(&ofn)) {
                pState->pItem->savePath = szFile;
                SetWindowTextW(pState->hSaveEdit, szFile);

                wchar_t updatedDir[MAX_PATH] = { 0 };
                wcsncpy_s(updatedDir, szFile, MAX_PATH - 1);
                PathRemoveFileSpecW(updatedDir);
                SetWindowTextW(pState->hPathBox, updatedDir);
            }
            return 0;
        }

        if (id == IDOK) { // "Start Download"
            wchar_t buf[2048] = { 0 };
            GetWindowTextW(pState->hUrlEdit, buf, 2048); pState->pItem->url = buf;
            GetWindowTextW(pState->hSaveEdit, buf, 2048); pState->pItem->savePath = buf;
            GetWindowTextW(pState->hDescEdit, buf, 2048); pState->pItem->description = buf;

            // Extract updated filename from savePath
            std::wstring sp = pState->pItem->savePath;
            size_t slashPos = sp.find_last_of(L"\\/");
            if (slashPos != std::wstring::npos && slashPos + 1 < sp.length()) {
                pState->pItem->filename = sp.substr(slashPos + 1);
            }
            pState->pItem->category = DetectCategoryFromFilename(pState->pItem->filename);

            EnsureDirectoryExists(pState->pItem->savePath);
            pState->startImmediately = true;
            pState->confirmed = true;
            DestroyWindow(hWnd);
            PostMessageW(NULL, WM_NULL, 0, 0);
            return 0;
        }

        if (id == 301) { // "Download Later"
            wchar_t buf[2048] = { 0 };
            GetWindowTextW(pState->hUrlEdit, buf, 2048); pState->pItem->url = buf;
            GetWindowTextW(pState->hSaveEdit, buf, 2048); pState->pItem->savePath = buf;
            GetWindowTextW(pState->hDescEdit, buf, 2048); pState->pItem->description = buf;

            // Extract updated filename from savePath
            std::wstring sp = pState->pItem->savePath;
            size_t slashPos = sp.find_last_of(L"\\/");
            if (slashPos != std::wstring::npos && slashPos + 1 < sp.length()) {
                pState->pItem->filename = sp.substr(slashPos + 1);
            }
            pState->pItem->category = DetectCategoryFromFilename(pState->pItem->filename);

            EnsureDirectoryExists(pState->pItem->savePath);
            pState->startImmediately = false;
            pState->confirmed = true;
            DestroyWindow(hWnd);
            PostMessageW(NULL, WM_NULL, 0, 0);
            return 0;
        }

        if (id == IDCANCEL) {
            pState->confirmed = false;
            DestroyWindow(hWnd);
            PostMessageW(NULL, WM_NULL, 0, 0);
            return 0;
        }
        break;
    }

    case WM_CLOSE: {
        if (pState) pState->confirmed = false;
        DestroyWindow(hWnd);
        PostMessageW(NULL, WM_NULL, 0, 0);
        return 0;
    }
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

bool ShowDownloadFileInfoDialog(HWND hParent, DownloadItem& item, bool& outStartImmediately) {
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icex);

    static bool s_classRegistered = false;
    if (!s_classRegistered) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = DownloadFileInfoWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"IDM_DownloadFileInfoDialogClass";
        RegisterClassExW(&wc);
        s_classRegistered = true;
    }

    DownloadFileInfoState state;
    state.pItem = &item;
    state.baseDownloads = GetDefaultDownloadsFolder();

    bool isAudioItem = (item.quality.find(L"Audio") != std::wstring::npos ||
                        item.quality.find(L"MP3") != std::wstring::npos ||
                        item.quality.find(L"M4A") != std::wstring::npos ||
                        item.category == L"Music" ||
                        item.filename.find(L".mp3") != std::wstring::npos ||
                        item.savePath.find(L"\\Music\\") != std::wstring::npos);
    if (isAudioItem) {
        item.category = L"Music";
        item.quality = (item.quality.find(L"M4A") != std::wstring::npos) ? L"Audio Track (M4A)" : L"Audio Track (MP3)";
        std::wstring targetExt = (item.quality.find(L"M4A") != std::wstring::npos) ? L".m4a" : L".mp3";
        size_t dotPos = item.filename.find_last_of(L'.');
        if (dotPos != std::wstring::npos) {
            item.filename = item.filename.substr(0, dotPos) + targetExt;
        } else {
            item.filename += targetExt;
        }
        item.savePath = state.baseDownloads + L"Music\\" + item.filename;
    }

    // Ensure category folder exists
    std::wstring catDir = state.baseDownloads + (item.category.empty() ? L"General" : item.category) + L"\\";
    EnsureDirectoryExists(catDir + item.filename);

    if (item.savePath.empty()) {
        item.savePath = catDir + item.filename;
    }

    int dlgW = 580;
    int dlgH = 275;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - dlgW) / 2;
    int y = (screenH - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_APPWINDOW | WS_EX_TOPMOST,
        L"IDM_DownloadFileInfoDialogClass",
        L"Download File Info",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        x, y, dlgW, dlgH,
        hParent, NULL, GetModuleHandle(NULL), &state
    );

    if (!hDlg) {
        DWORD err = GetLastError();
        wchar_t errMsg[128];
        swprintf_s(errMsg, L"Failed to create Download File Info Dialog. Error: %lu", err);
        MessageBoxW(NULL, errMsg, L"UI Error", MB_ICONERROR | MB_OK);
        return false;
    }

    if (hParent && IsWindow(hParent)) EnableWindow(hParent, FALSE);

    ShowWindow(hDlg, SW_SHOWNORMAL);
    UpdateWindow(hDlg);
    SetForegroundWindow(hDlg);
    BringWindowToTop(hDlg);

    // Modal Message Loop
    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (hParent) {
        EnableWindow(hParent, TRUE);
        SetForegroundWindow(hParent);
    }

    outStartImmediately = state.startImmediately;
    return state.confirmed;
}
