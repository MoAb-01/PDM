#include "../include/Models.hpp"
#include <windows.h>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

struct OptionsDlgState {
    IDMSettings* pSettings = nullptr;
    HWND hDlg = NULL;
    HWND hChkStartup = NULL;
    HWND hChkClip = NULL;
    HWND hExtEdit = NULL;
};

static LRESULT CALLBACK OptionsWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    OptionsDlgState* pState = (OptionsDlgState*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (message) {
    case WM_NCCREATE: {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pState = (OptionsDlgState*)pCreate->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pState);
        pState->hDlg = hWnd;
        return TRUE;
    }
    case WM_CREATE: {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        // Tab Control
        HWND hTab = CreateWindowW(WC_TABCONTROLW, L"", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE, 10, 10, 435, 385, hWnd, (HMENU)1001, NULL, NULL);
        SendMessageW(hTab, WM_SETFONT, (WPARAM)hFont, TRUE);

        TCITEMW tie;
        tie.mask = TCIF_TEXT;
        wchar_t tab1[] = L"General"; tie.pszText = tab1; TabCtrl_InsertItem(hTab, 0, &tie);
        wchar_t tab2[] = L"File types"; tie.pszText = tab2; TabCtrl_InsertItem(hTab, 1, &tie);
        wchar_t tab3[] = L"Save to"; tie.pszText = tab3; TabCtrl_InsertItem(hTab, 2, &tie);
        wchar_t tab4[] = L"Downloads"; tie.pszText = tab4; TabCtrl_InsertItem(hTab, 3, &tie);
        wchar_t tab5[] = L"Connection"; tie.pszText = tab5; TabCtrl_InsertItem(hTab, 4, &tie);

        // General Tab Controls
        HWND hGrpBrowser = CreateWindowW(L"STATIC", L"Capture downloads from the following browsers:", WS_CHILD | WS_VISIBLE | SS_LEFT, 24, 50, 400, 18, hWnd, NULL, NULL, NULL);
        pState->hChkStartup = CreateWindowW(L"BUTTON", L"Launch Internet Download Manager on startup", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 24, 75, 380, 20, hWnd, (HMENU)2001, NULL, NULL);
        pState->hChkClip = CreateWindowW(L"BUTTON", L"Automatically start downloading of URLs placed to clipboard", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 24, 100, 380, 20, hWnd, (HMENU)2002, NULL, NULL);

        HWND hChkChrome = CreateWindowW(L"BUTTON", L"Google Chrome", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 40, 130, 200, 20, hWnd, NULL, NULL, NULL);
        HWND hChkEdge = CreateWindowW(L"BUTTON", L"Microsoft Edge", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 40, 155, 200, 20, hWnd, NULL, NULL, NULL);
        HWND hChkFirefox = CreateWindowW(L"BUTTON", L"Mozilla Firefox", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 40, 180, 200, 20, hWnd, NULL, NULL, NULL);
        HWND hChkSafari = CreateWindowW(L"BUTTON", L"Apple Safari", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 40, 205, 200, 20, hWnd, NULL, NULL, NULL);

        SendMessageW(pState->hChkStartup, BM_SETCHECK, pState->pSettings->launchOnStartup ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(pState->hChkClip, BM_SETCHECK, pState->pSettings->clipboardAutoDetect ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(hChkChrome, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(hChkEdge, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(hChkFirefox, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(hChkSafari, BM_SETCHECK, BST_CHECKED, 0);

        SendMessageW(hGrpBrowser, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(pState->hChkStartup, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(pState->hChkClip, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hChkChrome, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hChkEdge, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hChkFirefox, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hChkSafari, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hExtLabel = CreateWindowW(L"STATIC", L"Automatically start downloading the following file types:", WS_CHILD | WS_VISIBLE, 24, 235, 400, 18, hWnd, NULL, NULL, NULL);
        pState->hExtEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pState->pSettings->autoCaptureExtensions.c_str(), WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL, 24, 255, 400, 60, hWnd, (HMENU)3001, NULL, NULL);
        SendMessageW(hExtLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(pState->hExtEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hOkBtn = CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 275, 405, 75, 25, hWnd, (HMENU)IDOK, NULL, NULL);
        HWND hCancelBtn = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 360, 405, 75, 25, hWnd, (HMENU)IDCANCEL, NULL, NULL);
        SendMessageW(hOkBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hCancelBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

        return 0;
    }

    case WM_COMMAND: {
        if (!pState) return 0;
        int id = LOWORD(wParam);

        if (id == IDOK) {
            pState->pSettings->launchOnStartup = (SendMessageW(pState->hChkStartup, BM_GETCHECK, 0, 0) == BST_CHECKED);
            pState->pSettings->clipboardAutoDetect = (SendMessageW(pState->hChkClip, BM_GETCHECK, 0, 0) == BST_CHECKED);
            wchar_t buf[2048] = { 0 };
            GetWindowTextW(pState->hExtEdit, buf, 2048);
            pState->pSettings->autoCaptureExtensions = buf;
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

void ShowOptionsDialog(HWND hParent, IDMSettings* pSettings) {
    if (!pSettings) return;

    static bool s_registered = false;
    if (!s_registered) {
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_TAB_CLASSES | ICC_STANDARD_CLASSES;
        InitCommonControlsEx(&icex);

        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = OptionsWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"IDM_OptionsDialogClass";
        RegisterClassExW(&wc);
        s_registered = true;
    }

    OptionsDlgState state;
    state.pSettings = pSettings;

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"IDM_OptionsDialogClass",
        L"Internet Download Manager Configuration",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 470, 480,
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
}
