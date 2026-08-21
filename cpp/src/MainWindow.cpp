#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "../include/LocalServerBridge.hpp"
#include "../include/DownloadEngine.hpp"
#include "../include/Win32Dark.hpp"
#include "../include/IconFactory.hpp"
#include "../include/DownloadProgressDialog.hpp"
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

// Declarations of subdialogs
void ShowFilePropertiesDialog(HWND hParent, DownloadItem* pItem);
void ShowOptionsDialog(HWND hParent, IDMSettings* pSettings);
void ShowChunkVisualizerDialog(HWND hParent, DownloadItem* pItem);
bool ShowAddUrlDialog(HWND hParent, DownloadItem& outItem);
void ShowSchedulerDialog(HWND hParent);
void ShowMediaSnifferDialog(HWND hParent, DownloadItem& outStreamItem, bool& outAdd);
bool ShowDownloadFileInfoDialog(HWND hParent, DownloadItem& item, bool& outStartImmediately);

class MainWindow {
public:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        MainWindow* pThis = nullptr;
        if (message == WM_CREATE) {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            pThis = (MainWindow*)pCreate->lpCreateParams;
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
            pThis->m_hWnd = hWnd;
            pThis->OnCreate();
            return 0;
        } else {
            pThis = (MainWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
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
        icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES;
        InitCommonControlsEx(&icex);

        HICON hIconBig = (HICON)LoadImageW(NULL, L"app_icon.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
        if (!hIconBig) hIconBig = (HICON)LoadImageW(NULL, L"d:\\Download Manager AB\\app_icon.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
        HICON hIconSmall = (HICON)LoadImageW(NULL, L"app_icon.ico", IMAGE_ICON, 16, 16, LR_LOADFROMFILE);
        if (!hIconSmall) hIconSmall = (HICON)LoadImageW(NULL, L"d:\\Download Manager AB\\app_icon.ico", IMAGE_ICON, 16, 16, LR_LOADFROMFILE);

        WNDCLASSEXW wcex = { sizeof(WNDCLASSEXW) };
        wcex.lpfnWndProc = MainWindow::WndProc;
        wcex.hInstance = GetModuleHandle(NULL);
        wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszClassName = L"IDM_Native_MainWindowClass";
        wcex.hIcon = hIconBig;
        wcex.hIconSm = hIconSmall;
        RegisterClassExW(&wcex);

        HWND hWnd = CreateWindowExW(
            0,
            L"IDM_Native_MainWindowClass",
            L"AB Download Manager",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT, CW_USEDEFAULT, 1024, 620,
            NULL, NULL, GetModuleHandle(NULL), this
        );

        if (hWnd && hIconBig) {
            SendMessageW(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
            SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
        }

        return hWnd;
    }

    void TriggerAddFromUrl(const std::wstring& url, const std::wstring& filename = L"", const std::wstring& referer = L"") {
        DownloadItem item;
        item.id = L"dl-" + std::to_wstring(GetTickCount());
        item.url = url;

        // Parse filename
        if (!filename.empty()) {
            item.filename = filename;
        } else {
            size_t slash = url.find_last_of(L'/');
            item.filename = (slash != std::wstring::npos && slash + 1 < url.length()) ? url.substr(slash + 1) : L"stream_media.mp4";
            size_t qmark = item.filename.find(L'?');
            if (qmark != std::wstring::npos) item.filename = item.filename.substr(0, qmark);
        }

        item.category = L"Video";
        item.savePath = L"C:\\Users\\UHD\\Downloads\\Video\\" + item.filename;
        item.referer = referer;
        item.description = L"Captured via IDM Browser Sniffer";
        item.sizeBytes = 0; // Starts at 0, dynamically resolved from manifest/headers
        item.downloadedBytes = 0;
        item.status = DownloadStatus::Downloading;
        item.connections = 16;

        SYSTEMTIME st;
        GetLocalTime(&st);
        const wchar_t* months[] = { L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec" };
        item.lastTryDate = std::wstring(months[st.wMonth - 1]) + L" " + std::to_wstring(st.wDay) + L", " + std::to_wstring(st.wYear);

        bool startImmediately = true;
        if (ShowDownloadFileInfoDialog(NULL, item, startImmediately)) {
            item.status = startImmediately ? DownloadStatus::Downloading : DownloadStatus::Queued;
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

        // Start background bridge server on 127.0.0.1:8989
        m_serverBridge.Start([this](const std::wstring& url, const std::wstring& filename, const std::wstring& referer) {
            // Post notification to main thread
            struct DownloadRequest {
                std::wstring url;
                std::wstring filename;
                std::wstring referer;
            };
            auto* req = new DownloadRequest{ url, filename, referer };
            PostMessageW(m_hWnd, WM_USER + 200, (WPARAM)req, 0);
        });

        // Setup Engine Callbacks
        m_engine.SetCallbacks(
            [this](const std::wstring& id, uint64_t downloaded, uint64_t total, uint64_t speed) {
                PostMessageW(m_hWnd, WM_USER + 101, 0, 0);
            },
            [this](const std::wstring& id, DownloadStatus status) {
                PostMessageW(m_hWnd, WM_USER + 102, 0, 0);
            }
        );

        SetTimer(m_hWnd, 1, 50, NULL);
    }

    void CreateMenuBar() {
        HMENU hMenuBar = CreateMenu();

        HMENU hMenuTasks = CreatePopupMenu();
        AppendMenuW(hMenuTasks, MF_STRING, 1001, L"&Add new download...\tCtrl+N");
        AppendMenuW(hMenuTasks, MF_STRING, 1002, L"&Media Sniffer Studio...");
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
        AppendMenuW(hMenuDownloads, MF_STRING, 1011, L"&Scheduler...");
        AppendMenuW(hMenuDownloads, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hMenuDownloads, MF_STRING, 1012, L"Delete &Completed");
        AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuDownloads, L"&Downloads");

        HMENU hMenuView = CreatePopupMenu();
        AppendMenuW(hMenuView, MF_STRING, 1020, L"Toolbar");
        AppendMenuW(hMenuView, MF_STRING, 1021, L"Categories Sidebar");
        AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuView, L"&View");

        HMENU hMenuHelp = CreatePopupMenu();
        AppendMenuW(hMenuHelp, MF_STRING, 1030, L"About AB Download Manager...");
        AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuHelp, L"&Help");

        HMENU hMenuReg = CreatePopupMenu();
        AppendMenuW(hMenuReg, MF_STRING, 1040, L"Registration (PRO ACTIVATED)");
        AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuReg, L"&Registration");

        SetMenu(m_hWnd, hMenuBar);
    }

    static LRESULT CALLBACK ToolbarSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
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
            DefSubclassProc(hWnd, WM_PRINTCLIENT, (WPARAM)hdc, PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
            
            EndPaint(hWnd, &ps);
            return 0;
        }
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }

    void CreateToolbar() {
        m_hToolbar = CreateWindowExW(
            0, TOOLBARCLASSNAMEW, NULL,
            WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS | CCS_TOP | CCS_NODIVIDER,
            0, 0, 0, 0,
            m_hWnd, (HMENU)2000, GetModuleHandle(NULL), NULL
        );
        SendMessageW(m_hToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
        SendMessageW(m_hToolbar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DOUBLEBUFFER | TBSTYLE_EX_MIXEDBUTTONS);

        // 1. Build 24x24 Toolbar ImageList with ILC_COLOR32 | ILC_MASK
        m_hImageListTb = ImageList_Create(24, 24, ILC_COLOR32 | ILC_MASK, 12, 12);

        auto AddTbIcon = [&](COLORREF c, int t) {
            HICON hIco = IconFactory::CreateColoredIcon(24, c, t);
            if (hIco) {
                ImageList_AddIcon(m_hImageListTb, hIco);
                DestroyIcon(hIco);
            }
        };

        AddTbIcon(RGB(34, 197, 94),   0);   // Add URL  (Green plus)
        AddTbIcon(RGB(56, 189, 248),  1);   // Resume   (Blue play)
        AddTbIcon(RGB(245, 158, 11),  2);   // Stop     (Orange square)
        AddTbIcon(RGB(239, 68, 68),   3);   // Stop All (Red octagon)
        AddTbIcon(RGB(239, 68, 68),   4);   // Delete   (Red trash)
        AddTbIcon(RGB(168, 85, 247),  5);   // Del. Completed (Purple check)
        AddTbIcon(RGB(148, 163, 184), 6);   // Options  (Gray gear)
        AddTbIcon(RGB(236, 72, 153),  7);   // Scheduler (Pink clock)
        AddTbIcon(RGB(34, 197, 94),   8);   // Start Queue (Green circle play)
        AddTbIcon(RGB(239, 68, 68),   9);   // Stop Queue  (Red circle stop)
        AddTbIcon(RGB(6, 182, 212),   10);  // Media Sniffer (Cyan radio)

        SendMessageW(m_hToolbar, TB_SETIMAGELIST, 0, (LPARAM)m_hImageListTb);

        TBBUTTON tbButtons[] = {
            { 0, 1001, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_SHOWTEXT, {0}, 0, (INT_PTR)L"Add URL" },
            { 1, 2001, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_SHOWTEXT, {0}, 0, (INT_PTR)L"Resume" },
            { 2, 2002, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_SHOWTEXT, {0}, 0, (INT_PTR)L"Stop" },
            { 3, 1003, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_SHOWTEXT, {0}, 0, (INT_PTR)L"Stop All" },
            { 0, 0, 0, BTNS_SEP, {0}, 0, 0 },
            { 4, 2003, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_SHOWTEXT, {0}, 0, (INT_PTR)L"Delete" },
            { 5, 1012, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_SHOWTEXT, {0}, 0, (INT_PTR)L"Delete Co..." },
            { 0, 0, 0, BTNS_SEP, {0}, 0, 0 },
            { 6, 1010, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_SHOWTEXT, {0}, 0, (INT_PTR)L"Options" },
            { 7, 1011, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_SHOWTEXT, {0}, 0, (INT_PTR)L"Scheduler" },
            { 8, 1004, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_SHOWTEXT, {0}, 0, (INT_PTR)L"Start Qu..." },
            { 9, 1003, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_SHOWTEXT, {0}, 0, (INT_PTR)L"Stop Qu..." },
            { 0, 0, 0, BTNS_SEP, {0}, 0, 0 },
            { 10, 1002, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_SHOWTEXT, {0}, 0, (INT_PTR)L"Media Sniffer" }
        };

        SendMessageW(m_hToolbar, TB_ADDBUTTONS, sizeof(tbButtons) / sizeof(TBBUTTON), (LPARAM)&tbButtons);
        SendMessageW(m_hToolbar, TB_AUTOSIZE, 0, 0);

        // Subclass toolbar to paint background on WM_ERASEBKGND / WM_PAINT
        SetWindowSubclass(m_hToolbar, ToolbarSubclassProc, 1, 0);

        // SetWindowTheme removed to allow standard Windows toolbar background erasing
    }

    void CreateTreeView() {
        m_hTreeView = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
            WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
            0, 52, 190, 500,
            m_hWnd, (HMENU)3000, GetModuleHandle(NULL), NULL
        );

        TVINSERTSTRUCTW tvis = { 0 };
        tvis.hParent = TVI_ROOT;
        tvis.item.mask = TVIF_TEXT;

        wchar_t rootText[] = L"All Downloads"; tvis.item.pszText = rootText;
        HTREEITEM hAll = TreeView_InsertItem(m_hTreeView, &tvis);

        tvis.hParent = hAll;
        wchar_t c1[] = L"Compressed"; tvis.item.pszText = c1; TreeView_InsertItem(m_hTreeView, &tvis);
        wchar_t c2[] = L"Documents"; tvis.item.pszText = c2; TreeView_InsertItem(m_hTreeView, &tvis);
        wchar_t c3[] = L"Music"; tvis.item.pszText = c3; TreeView_InsertItem(m_hTreeView, &tvis);
        wchar_t c4[] = L"Programs"; tvis.item.pszText = c4; TreeView_InsertItem(m_hTreeView, &tvis);
        wchar_t c5[] = L"Video"; tvis.item.pszText = c5; TreeView_InsertItem(m_hTreeView, &tvis);

        tvis.hParent = TVI_ROOT;
        wchar_t s1[] = L"Unfinished"; tvis.item.pszText = s1; TreeView_InsertItem(m_hTreeView, &tvis);
        wchar_t s2[] = L"Finished"; tvis.item.pszText = s2; TreeView_InsertItem(m_hTreeView, &tvis);
        wchar_t s3[] = L"Grabber projects"; tvis.item.pszText = s3; TreeView_InsertItem(m_hTreeView, &tvis);
        wchar_t s4[] = L"Queues"; tvis.item.pszText = s4; TreeView_InsertItem(m_hTreeView, &tvis);

        TreeView_Expand(m_hTreeView, hAll, TVE_EXPAND);
    }

    void CreateListView() {
        m_hListView = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            192, 52, 800, 500,
            m_hWnd, (HMENU)4000, GetModuleHandle(NULL), NULL
        );

        ListView_SetExtendedListViewStyle(m_hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

        // Build 16x16 ListView icons.
        m_hImageListLv = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 6, 6);

        auto AddLvIcon = [&](COLORREF c, int t) {
            HICON hIco = IconFactory::CreateColoredIcon(16, c, t);
            if (hIco) {
                ImageList_AddIcon(m_hImageListLv, hIco);
                DestroyIcon(hIco);
            }
        };

        AddLvIcon(RGB(37, 99, 235),   21); // EXE  (Blue)
        AddLvIcon(RGB(234, 88, 12),   21); // ZIP  (Orange)
        AddLvIcon(RGB(124, 58, 237),  21); // MP4  (Purple)
        AddLvIcon(RGB(220, 38, 38),   21); // PDF  (Red)
        AddLvIcon(RGB(5, 150, 105),   21); // MP3  (Green)
        ListView_SetImageList(m_hListView, m_hImageListLv, LVSIL_SMALL);

        LVCOLUMNW lvc = { 0 };
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

        wchar_t col1[] = L"File Name"; lvc.pszText = col1; lvc.cx = 240; lvc.iSubItem = 0; ListView_InsertColumn(m_hListView, 0, &lvc);
        wchar_t col2[] = L"Q"; lvc.pszText = col2; lvc.cx = 35; lvc.iSubItem = 1; ListView_InsertColumn(m_hListView, 1, &lvc);
        wchar_t col3[] = L"Size"; lvc.pszText = col3; lvc.cx = 85; lvc.iSubItem = 2; ListView_InsertColumn(m_hListView, 2, &lvc);
        wchar_t col4[] = L"Status"; lvc.pszText = col4; lvc.cx = 120; lvc.iSubItem = 3; ListView_InsertColumn(m_hListView, 3, &lvc);
        wchar_t col5[] = L"Time left"; lvc.pszText = col5; lvc.cx = 75; lvc.iSubItem = 4; ListView_InsertColumn(m_hListView, 4, &lvc);
        wchar_t col6[] = L"Transfer rate"; lvc.pszText = col6; lvc.cx = 95; lvc.iSubItem = 5; ListView_InsertColumn(m_hListView, 5, &lvc);
        wchar_t col7[] = L"Last Try Date"; lvc.pszText = col7; lvc.cx = 100; lvc.iSubItem = 6; ListView_InsertColumn(m_hListView, 6, &lvc);
        wchar_t col8[] = L"Description"; lvc.pszText = col8; lvc.cx = 160; lvc.iSubItem = 7; ListView_InsertColumn(m_hListView, 7, &lvc);
    }

    void CreateStatusBar() {
        m_hStatusBar = CreateWindowExW(
            0, STATUSCLASSNAMEW, NULL,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0,
            m_hWnd, (HMENU)5000, GetModuleHandle(NULL), NULL
        );
        int parts[] = { 380, 750, -1 };
        SendMessageW(m_hStatusBar, SB_SETPARTS, 3, (LPARAM)parts);
        SendMessageW(m_hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"AB Engine: Ready (16-thread Acceleration)");
        SendMessageW(m_hStatusBar, SB_SETTEXTW, 1, (LPARAM)L"Browser Integration: Active (Chrome, Edge, Firefox)");
        SendMessageW(m_hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"Speed Limiter: OFF");
    }

    void LoadSampleDownloads() {
        // Start with clean downloads list
    }

    int GetSelectedListViewIndex() {
        return ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
    }

    void PopulateListView() {
        auto items = m_engine.GetDownloads();
        int itemCount = ListView_GetItemCount(m_hListView);

        if (itemCount != (int)items.size()) {
            ListView_DeleteAllItems(m_hListView);
            for (size_t i = 0; i < items.size(); ++i) {
                const auto& item = items[i];
                LVITEMW lvi = { 0 };
                lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
                lvi.iItem = (int)i;
                lvi.iSubItem = 0;
                lvi.pszText = const_cast<LPWSTR>(item.filename.c_str());
                lvi.iImage = (item.category == L"Programs") ? 0 : (item.category == L"Compressed" ? 1 : 2);
                lvi.lParam = (LPARAM)i;
                ListView_InsertItem(m_hListView, &lvi);
            }
        }

        for (size_t i = 0; i < items.size(); ++i) {
            const auto& item = items[i];

            // Size format (GB / MB / KB / Bytes)
            std::wstring strSize;
            if (item.sizeBytes >= (uint64_t)1024 * 1024 * 1024) {
                std::wstringstream ssSize;
                ssSize << std::fixed << std::setprecision(2) << ((double)item.sizeBytes / (1024.0 * 1024.0 * 1024.0)) << L" GB";
                strSize = ssSize.str();
            } else if (item.sizeBytes >= 1024 * 1024) {
                std::wstringstream ssSize;
                ssSize << std::fixed << std::setprecision(2) << ((double)item.sizeBytes / (1024.0 * 1024.0)) << L" MB";
                strSize = ssSize.str();
            } else if (item.sizeBytes >= 1024) {
                std::wstringstream ssSize;
                ssSize << std::fixed << std::setprecision(2) << ((double)item.sizeBytes / 1024.0) << L" KB";
                strSize = ssSize.str();
            } else if (item.sizeBytes > 0) {
                strSize = std::to_wstring(item.sizeBytes) + L" Bytes";
            } else {
                strSize = (item.status == DownloadStatus::Downloading) ? L"Connecting..." : L"--";
            }
            ListView_SetItemText(m_hListView, (int)i, 2, const_cast<LPWSTR>(strSize.c_str()));

            // Status, Time Left, and Transfer Rate format
            if (item.status == DownloadStatus::Complete) {
                std::wstring st = L"Complete";
                ListView_SetItemText(m_hListView, (int)i, 3, const_cast<LPWSTR>(st.c_str()));
                ListView_SetItemText(m_hListView, (int)i, 4, const_cast<LPWSTR>(L""));
                ListView_SetItemText(m_hListView, (int)i, 5, const_cast<LPWSTR>(L""));
            } else if (item.status == DownloadStatus::Downloading) {
                std::wstring st;
                if (item.sizeBytes > 0) {
                    int pct = (int)((item.downloadedBytes * 100) / item.sizeBytes);
                    if (pct > 100) pct = 100;
                    st = L"Downloading (" + std::to_wstring(pct) + L"%)";
                } else {
                    st = L"Connecting (0%)";
                }
                ListView_SetItemText(m_hListView, (int)i, 3, const_cast<LPWSTR>(st.c_str()));

                // Calculate accurate Time Left
                std::wstring strTime = L"--";
                if (item.speedBytesPerSec > 0 && item.sizeBytes > item.downloadedBytes) {
                    uint64_t remBytes = item.sizeBytes - item.downloadedBytes;
                    uint64_t secs = (uint64_t)((double)remBytes / (double)item.speedBytesPerSec);
                    if (secs > 99 * 3600 + 59 * 60 + 59) secs = 99 * 3600 + 59 * 60 + 59; // Cap display at 99:59:59

                    if (secs < 60) {
                        strTime = std::to_wstring(secs) + L" sec";
                    } else if (secs < 3600) {
                        strTime = std::to_wstring(secs / 60) + L" min " + std::to_wstring(secs % 60) + L" sec";
                    } else {
                        strTime = std::to_wstring(secs / 3600) + L" hr " + std::to_wstring((secs % 3600) / 60) + L" min";
                    }
                } else if (item.speedBytesPerSec == 0 && item.downloadedBytes > 0 && item.downloadedBytes < item.sizeBytes) {
                    strTime = L"Stalled";
                }
                ListView_SetItemText(m_hListView, (int)i, 4, const_cast<LPWSTR>(strTime.c_str()));

                // Calculate accurate Transfer Rate
                std::wstringstream ssSpeed;
                if (item.speedBytesPerSec >= 1024 * 1024) {
                    ssSpeed << std::fixed << std::setprecision(2) << ((double)item.speedBytesPerSec / (1024.0 * 1024.0)) << L" MB/s";
                } else if (item.speedBytesPerSec >= 1024) {
                    ssSpeed << std::fixed << std::setprecision(2) << ((double)item.speedBytesPerSec / 1024.0) << L" KB/s";
                } else if (item.speedBytesPerSec > 0) {
                    ssSpeed << item.speedBytesPerSec << L" B/s";
                } else {
                    ssSpeed << L"0.00 KB/s";
                }
                std::wstring strSpeed = ssSpeed.str();
                ListView_SetItemText(m_hListView, (int)i, 5, const_cast<LPWSTR>(strSpeed.c_str()));
            } else if (item.status == DownloadStatus::Paused) {
                std::wstring st = L"Paused";
                ListView_SetItemText(m_hListView, (int)i, 3, const_cast<LPWSTR>(st.c_str()));
                ListView_SetItemText(m_hListView, (int)i, 4, const_cast<LPWSTR>(L""));
                ListView_SetItemText(m_hListView, (int)i, 5, const_cast<LPWSTR>(L""));
            }

            ListView_SetItemText(m_hListView, (int)i, 6, const_cast<LPWSTR>(item.lastTryDate.c_str()));
            ListView_SetItemText(m_hListView, (int)i, 7, const_cast<LPWSTR>(item.description.c_str()));
        }
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
            SetWindowPos(m_hListView, NULL, 202, tbHeight, width - 202, clientH, SWP_NOZORDER);
            return 0;
        }

        case WM_TIMER:
        case WM_USER + 101:
        case WM_USER + 102: {
            PopulateListView();
            return 0;
        }

        case WM_COPYDATA: {
            PCOPYDATASTRUCT pCDS = (PCOPYDATASTRUCT)lParam;
            if (pCDS && pCDS->lpData) {
                std::wstring dataStr((const wchar_t*)pCDS->lpData, pCDS->cbData / sizeof(wchar_t));
                size_t tab1 = dataStr.find(L'\t');
                std::wstring url = dataStr, filename = L"", referer = L"";
                if (tab1 != std::wstring::npos) {
                    url = dataStr.substr(0, tab1);
                    size_t tab2 = dataStr.find(L'\t', tab1 + 1);
                    if (tab2 != std::wstring::npos) {
                        filename = dataStr.substr(tab1 + 1, tab2 - tab1 - 1);
                        referer = dataStr.substr(tab2 + 1);
                    } else {
                        filename = dataStr.substr(tab1 + 1);
                    }
                }
                SetForegroundWindow(m_hWnd);
                TriggerAddFromUrl(url, filename, referer);
                return 1;
            }
            return 0;
        }

        case WM_USER + 200: { // Incoming request from browser extension
            struct DownloadRequest {
                std::wstring url;
                std::wstring filename;
                std::wstring referer;
            };
            auto* req = (DownloadRequest*)wParam;
            if (req) {
                TriggerAddFromUrl(req->url, req->filename, req->referer);
                delete req;
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
                            auto& it = items[index];
                            if (it.status == DownloadStatus::Downloading || it.status == DownloadStatus::Paused) {
                                ShowDownloadProgressDialog(m_hWnd, &it, &m_engine);
                            } else if (it.status == DownloadStatus::Complete) {
                                // Open the real downloaded file with default media player / viewer!
                                ShellExecuteW(m_hWnd, L"open", it.savePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
                            } else {
                                ShowFilePropertiesDialog(m_hWnd, &it);
                                m_engine.UpdateItem(it);
                                PopulateListView();
                            }
                        }
                    }
                } else if (pnm->code == NM_RCLICK) {
                    // Hit test to ensure right-clicked item is selected
                    POINT pt;
                    GetCursorPos(&pt);
                    POINT clientPt = pt;
                    ScreenToClient(m_hListView, &clientPt);

                    LVHITTESTINFO hti = { 0 };
                    hti.pt = clientPt;
                    int hitIndex = ListView_HitTest(m_hListView, &hti);
                    if (hitIndex >= 0) {
                        ListView_SetItemState(m_hListView, hitIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                    }

                    int index = GetSelectedListViewIndex();
                    if (index >= 0) {
                        HMENU hMenu = CreatePopupMenu();
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
                        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hWnd, NULL);
                        DestroyMenu(hMenu);
                    }
                }
            }
            break;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);

            if (id == 1001) { // Add URL -> Launches the exact "Download File Info" dialog (screenshot replica)
                DownloadItem item;
                item.id = L"dl-" + std::to_wstring(GetTickCount());
                item.url = L"https://www.internetdownloadmanager.com/languages/idm_al.zip";
                item.filename = L"idm_al.zip";
                item.category = L"Compressed";
                item.savePath = L"C:\\Users\\UHD\\Downloads\\Compressed\\idm_al.zip";
                item.sizeBytes = 0;
                item.downloadedBytes = 0;
                item.status = DownloadStatus::Downloading;
                item.connections = 8;
                item.lastTryDate = L"Aug 21, 2026";
                item.description = L"Language translation archive package";

                bool startImmediately = true;
                if (ShowDownloadFileInfoDialog(m_hWnd, item, startImmediately)) {
                    item.status = startImmediately ? DownloadStatus::Downloading : DownloadStatus::Queued;
                    m_engine.AddItem(item);
                    if (startImmediately) {
                        m_engine.StartDownload(item.id);
                    }
                    PopulateListView();
                    if (startImmediately) {
                        ShowDownloadProgressDialog(m_hWnd, &item, &m_engine);
                    }
                }
            } else if (id == 2001) { // Resume
                int index = GetSelectedListViewIndex();
                if (index >= 0) {
                    auto items = m_engine.GetDownloads();
                    if ((size_t)index < items.size()) {
                        m_engine.StartDownload(items[index].id);
                        PopulateListView();
                    }
                }
            } else if (id == 2002) { // Stop
                int index = GetSelectedListViewIndex();
                if (index >= 0) {
                    auto items = m_engine.GetDownloads();
                    if ((size_t)index < items.size()) {
                        m_engine.PauseDownload(items[index].id);
                        PopulateListView();
                    }
                }
            } else if (id == 1003) { // Stop All
                m_engine.StopAll();
                PopulateListView();
            } else if (id == 1004) { // Resume All / Start Queue
                m_engine.ResumeAll();
                PopulateListView();
            } else if (id == 2003) { // Delete
                int index = GetSelectedListViewIndex();
                if (index >= 0) {
                    auto items = m_engine.GetDownloads();
                    if ((size_t)index < items.size()) {
                        m_engine.DeleteItem(items[index].id);
                        PopulateListView();
                    }
                }
            } else if (id == 1012) { // Delete Completed
                auto items = m_engine.GetDownloads();
                for (const auto& it : items) {
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
                    if (ShowDownloadFileInfoDialog(m_hWnd, streamItem, startImmediately)) {
                        streamItem.status = startImmediately ? DownloadStatus::Downloading : DownloadStatus::Queued;
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
                        ShellExecuteW(m_hWnd, L"open", items[index].savePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
                    }
                }
            } else if (id == 5004) { // Open Containing Folder
                int index = GetSelectedListViewIndex();
                if (index >= 0) {
                    auto items = m_engine.GetDownloads();
                    if ((size_t)index < items.size()) {
                        std::wstring selectArg = L"/select,\"" + items[index].savePath + L"\"";
                        ShellExecuteW(m_hWnd, L"open", L"explorer.exe", selectArg.c_str(), NULL, SW_SHOWNORMAL);
                    }
                }
            } else if (id == 5001) { // Properties
                int index = GetSelectedListViewIndex();
                if (index >= 0) {
                    auto items = m_engine.GetDownloads();
                    if ((size_t)index < items.size()) {
                        auto& it = items[index];
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
                MessageBoxW(m_hWnd, L"AB Download Manager\nEngine: 16-thread Range Acceleration & Media Sniffer\nAll rights reserved.", L"About AB Download Manager", MB_OK | MB_ICONINFORMATION);
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    MainWindow win;
    HWND hWnd = win.Create();
    if (!hWnd) return 1;

    ShowWindow(hWnd, SW_SHOWNORMAL);
    UpdateWindow(hWnd);
    SetForegroundWindow(hWnd);

    // Check if launched with a URL parameter from Chrome/Edge sniffer
    std::wstring cmdLine = GetCommandLineW();
    size_t urlPos = cmdLine.find(L"--download-url \"");
    if (urlPos != std::wstring::npos) {
        urlPos += 16;
        size_t endUrl = cmdLine.find(L"\"", urlPos);
        if (endUrl != std::wstring::npos) {
            std::wstring sniffedUrl = cmdLine.substr(urlPos, endUrl - urlPos);
            win.TriggerAddFromUrl(sniffedUrl);
        }
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
