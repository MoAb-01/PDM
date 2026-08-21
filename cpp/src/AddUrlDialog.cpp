#include "../include/Models.hpp"
#include <windows.h>
#include <commctrl.h>

struct AddUrlState {
    DownloadItem* pItem = nullptr;
    bool confirmed = false;
    HWND hDlg = NULL;
    HWND hAddrEdit = NULL;
    HWND hCatCombo = NULL;
    HWND hDescEdit = NULL;
};

static LRESULT CALLBACK AddUrlWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    AddUrlState* pState = (AddUrlState*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (message) {
    case WM_NCCREATE: {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pState = (AddUrlState*)pCreate->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pState);
        pState->hDlg = hWnd;
        return TRUE;
    }
    case WM_CREATE: {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        HWND hAddrLabel = CreateWindowW(L"STATIC", L"Address (URL):", WS_CHILD | WS_VISIBLE, 20, 16, 420, 18, hWnd, NULL, NULL, NULL);
        pState->hAddrEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 20, 36, 424, 22, hWnd, (HMENU)101, NULL, NULL);

        HWND hCatLabel = CreateWindowW(L"STATIC", L"Category:", WS_CHILD | WS_VISIBLE, 20, 70, 80, 18, hWnd, NULL, NULL, NULL);
        pState->hCatCombo = CreateWindowW(WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 100, 68, 140, 120, hWnd, (HMENU)102, NULL, NULL);

        HWND hDescLabel = CreateWindowW(L"STATIC", L"Description:", WS_CHILD | WS_VISIBLE, 20, 100, 80, 18, hWnd, NULL, NULL, NULL);
        pState->hDescEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Added manually", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 98, 344, 22, hWnd, (HMENU)103, NULL, NULL);

        HWND hStartNow = CreateWindowW(L"BUTTON", L"Download Now", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 180, 180, 120, 26, hWnd, (HMENU)IDOK, NULL, NULL);
        HWND hCancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 310, 180, 80, 26, hWnd, (HMENU)IDCANCEL, NULL, NULL);

        SendMessageW(hAddrLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(pState->hAddrEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hCatLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(pState->hCatCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hDescLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(pState->hDescEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hStartNow, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

        SendMessageW(pState->hCatCombo, CB_ADDSTRING, 0, (LPARAM)L"General");
        SendMessageW(pState->hCatCombo, CB_ADDSTRING, 0, (LPARAM)L"Compressed");
        SendMessageW(pState->hCatCombo, CB_ADDSTRING, 0, (LPARAM)L"Documents");
        SendMessageW(pState->hCatCombo, CB_ADDSTRING, 0, (LPARAM)L"Music");
        SendMessageW(pState->hCatCombo, CB_ADDSTRING, 0, (LPARAM)L"Programs");
        SendMessageW(pState->hCatCombo, CB_ADDSTRING, 0, (LPARAM)L"Video");
        SendMessageW(pState->hCatCombo, CB_SETCURSEL, 0, 0);

        return 0;
    }

    case WM_COMMAND: {
        if (!pState) return 0;
        int id = LOWORD(wParam);

        if (id == IDOK) {
            wchar_t urlBuf[2048] = { 0 };
            wchar_t descBuf[1024] = { 0 };
            GetWindowTextW(pState->hAddrEdit, urlBuf, 2048);
            GetWindowTextW(pState->hDescEdit, descBuf, 1024);

            std::wstring urlStr = urlBuf;
            if (!urlStr.empty()) {
                pState->pItem->id = L"dl-" + std::to_wstring(GetTickCount());
                pState->pItem->url = urlStr;

                size_t slash = urlStr.find_last_of(L'/');
                pState->pItem->filename = (slash != std::wstring::npos && slash + 1 < urlStr.length()) ? urlStr.substr(slash + 1) : L"download.bin";
                size_t qmark = pState->pItem->filename.find(L'?');
                if (qmark != std::wstring::npos) pState->pItem->filename = pState->pItem->filename.substr(0, qmark);

                pState->pItem->category = L"General";
                pState->pItem->savePath = L"C:\\Users\\UHD\\Downloads\\General\\" + pState->pItem->filename;
                pState->pItem->description = descBuf;
                pState->pItem->sizeBytes = 50000000;
                pState->pItem->downloadedBytes = 0;
                pState->pItem->status = DownloadStatus::Downloading;
                pState->pItem->lastTryDate = L"Aug 20, 2026";
                pState->pItem->connections = 16;
                pState->confirmed = true;
            }
            DestroyWindow(hWnd);
            return 0;
        }

        if (id == IDCANCEL) {
            pState->confirmed = false;
            DestroyWindow(hWnd);
            return 0;
        }
        break;
    }

    case WM_CLOSE: {
        if (pState) pState->confirmed = false;
        DestroyWindow(hWnd);
        return 0;
    }
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

bool ShowAddUrlDialog(HWND hParent, DownloadItem& outItem) {
    static bool s_registered = false;
    if (!s_registered) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = AddUrlWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"IDM_AddUrlDialogClass";
        RegisterClassExW(&wc);
        s_registered = true;
    }

    AddUrlState state;
    state.pItem = &outItem;

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"IDM_AddUrlDialogClass",
        L"Enter new address to download",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 480, 260,
        hParent, NULL, GetModuleHandle(NULL), &state
    );

    if (!hDlg) return false;

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

    return state.confirmed;
}
