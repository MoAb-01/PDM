#include "../include/Models.hpp"
#include <windows.h>
#include <commctrl.h>

static LRESULT CALLBACK SchedulerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        HWND hChkSchedule = CreateWindowW(L"BUTTON", L"Start download queue automatically at specified time", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 20, 400, 20, hWnd, (HMENU)101, NULL, NULL);
        HWND hStartLabel = CreateWindowW(L"STATIC", L"Start time: 02:00 AM", WS_CHILD | WS_VISIBLE, 40, 50, 200, 18, hWnd, NULL, NULL, NULL);
        HWND hStopLabel = CreateWindowW(L"STATIC", L"Stop time: 06:00 AM", WS_CHILD | WS_VISIBLE, 40, 75, 200, 18, hWnd, NULL, NULL, NULL);

        HWND hChkParallel = CreateWindowW(L"BUTTON", L"Download 2 files simultaneously (Main Queue)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 115, 400, 20, hWnd, (HMENU)102, NULL, NULL);
        HWND hChkShutdown = CreateWindowW(L"BUTTON", L"Turn off computer when downloads complete", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 145, 400, 20, hWnd, (HMENU)103, NULL, NULL);

        HWND hApplyBtn = CreateWindowW(L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 255, 235, 75, 25, hWnd, (HMENU)IDOK, NULL, NULL);
        HWND hCloseBtn = CreateWindowW(L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 340, 235, 75, 25, hWnd, (HMENU)IDCANCEL, NULL, NULL);

        SendMessageW(hChkSchedule, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(hChkParallel, BM_SETCHECK, BST_CHECKED, 0);

        SendMessageW(hChkSchedule, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hStartLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hStopLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hChkParallel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hChkShutdown, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hApplyBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hCloseBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDOK || id == IDCANCEL) {
            DestroyWindow(hWnd);
            return 0;
        }
        break;
    }
    case WM_CLOSE: {
        DestroyWindow(hWnd);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, message, wParam, lParam);
}

void ShowSchedulerDialog(HWND hParent) {
    static bool s_registered = false;
    if (!s_registered) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = SchedulerWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"IDM_SchedulerDialogClass";
        RegisterClassExW(&wc);
        s_registered = true;
    }

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"IDM_SchedulerDialogClass",
        L"Scheduler and Queue Processing",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 460, 320,
        hParent, NULL, GetModuleHandle(NULL), NULL
    );

    if (!hDlg) return;

    if (hParent) EnableWindow(hParent, FALSE);

    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (hParent) {
        EnableWindow(hParent, TRUE);
        SetForegroundWindow(hParent);
    }
}

struct SnifferState {
    DownloadItem* pItem = nullptr;
    bool confirmed = false;
    HWND hList = NULL;
};

static LRESULT CALLBACK SnifferWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    SnifferState* pState = (SnifferState*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (message) {
    case WM_NCCREATE: {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pState = (SnifferState*)pCreate->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pState);
        return TRUE;
    }
    case WM_CREATE: {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        HWND hTitle = CreateWindowW(L"STATIC", L"Detected Network Video / Audio Streams (webRequest & DOM Injected Interceptor)", WS_CHILD | WS_VISIBLE, 20, 16, 520, 18, hWnd, NULL, NULL, NULL);
        SendMessageW(hTitle, WM_SETFONT, (WPARAM)hFont, TRUE);

        pState->hList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 20, 42, 524, 220, hWnd, (HMENU)201, NULL, NULL);
        ListView_SetExtendedListViewStyle(pState->hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        LVCOLUMNW lvc = { 0 };
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        wchar_t c1[] = L"Media Type"; lvc.pszText = c1; lvc.cx = 140; ListView_InsertColumn(pState->hList, 0, &lvc);
        wchar_t c2[] = L"Quality / Resolution"; lvc.pszText = c2; lvc.cx = 150; ListView_InsertColumn(pState->hList, 1, &lvc);
        wchar_t c3[] = L"Host / Source"; lvc.pszText = c3; lvc.cx = 210; ListView_InsertColumn(pState->hList, 2, &lvc);

        const wchar_t* streams[][3] = {
            { L"HLS (.m3u8 Playlist)", L"1080p 60fps Full HD", L"streams.redbull.com/motorsports" },
            { L"MPEG-DASH (.mpd)", L"2160p 4K HDR", L"vimeo.com/creative-showcase" },
            { L"Direct Stream (.mp4)", L"1080p Web Stream", L"commondatastorage.googleapis.com" },
            { L"Lossless Audio (.flac)", L"24-bit 96kHz Master", L"cdn.highresaudio.com" }
        };

        for (int i = 0; i < 4; ++i) {
            LVITEMW lvi = { 0 };
            lvi.mask = LVIF_TEXT;
            lvi.iItem = i;
            lvi.pszText = const_cast<LPWSTR>(streams[i][0]);
            ListView_InsertItem(pState->hList, &lvi);
            ListView_SetItemText(pState->hList, i, 1, const_cast<LPWSTR>(streams[i][1]));
            ListView_SetItemText(pState->hList, i, 2, const_cast<LPWSTR>(streams[i][2]));
        }
        ListView_SetItemState(pState->hList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

        HWND hDrmNote = CreateWindowW(L"STATIC", L"Note: Encrypted DRM streams (Widevine/FairPlay) cannot be decrypted without private keys.", WS_CHILD | WS_VISIBLE, 20, 275, 520, 36, hWnd, NULL, NULL, NULL);
        SendMessageW(hDrmNote, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hDlBtn = CreateWindowW(L"BUTTON", L"Download Selected Stream", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 230, 335, 190, 26, hWnd, (HMENU)IDOK, NULL, NULL);
        HWND hCloseBtn = CreateWindowW(L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 430, 335, 80, 26, hWnd, (HMENU)IDCANCEL, NULL, NULL);

        SendMessageW(hDlBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hCloseBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
        return 0;
    }
    case WM_COMMAND: {
        if (!pState) return 0;
        int id = LOWORD(wParam);
        if (id == IDOK) {
            pState->pItem->id = L"dl-sniff-" + std::to_wstring(GetTickCount());
            pState->pItem->filename = L"STARK_VARG_DRONE_4K_60FPS.mp4";
            pState->pItem->category = L"Video";
            pState->pItem->savePath = L"C:\\Users\\UHD\\Downloads\\Video\\STARK_VARG_DRONE_4K_60FPS.mp4";
            pState->pItem->url = L"https://streams.redbull.com/manifest/stark-varg/master.m3u8";
            pState->pItem->referer = L"https://redbull.com/motorsports/varg-drone-raw";
            pState->pItem->description = L"Sniffed HLS 1080p 60fps Stream";
            pState->pItem->sizeBytes = 168000000;
            pState->pItem->downloadedBytes = 0;
            pState->pItem->status = DownloadStatus::Downloading;
            pState->pItem->connections = 16;
            pState->pItem->lastTryDate = L"Aug 20, 2026";
            pState->confirmed = true;
            DestroyWindow(hWnd);
            return 0;
        }
        if (id == IDCANCEL) {
            DestroyWindow(hWnd);
            return 0;
        }
        break;
    }
    case WM_CLOSE: {
        DestroyWindow(hWnd);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, message, wParam, lParam);
}

void ShowMediaSnifferDialog(HWND hParent, DownloadItem& outStreamItem, bool& outAdd) {
    static bool s_registered = false;
    if (!s_registered) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = SnifferWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"IDM_SnifferDialogClass";
        RegisterClassExW(&wc);
        s_registered = true;
    }

    SnifferState state;
    state.pItem = &outStreamItem;

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"IDM_SnifferDialogClass",
        L"IDM Media Sniffing & Stream Capture Studio",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 580, 420,
        hParent, NULL, GetModuleHandle(NULL), &state
    );

    if (!hDlg) return;

    if (hParent) EnableWindow(hParent, FALSE);

    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (hParent) {
        EnableWindow(hParent, TRUE);
        SetForegroundWindow(hParent);
    }

    outAdd = state.confirmed;
}
