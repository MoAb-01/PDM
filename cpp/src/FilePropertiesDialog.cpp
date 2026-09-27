#include "../include/Models.hpp"
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

struct FilePropsState {
    DownloadItem* pItem = nullptr;
    HWND hDlg = NULL;
    HWND hSaveTo = NULL;
    HWND hAddr = NULL;
    HWND hDesc = NULL;
    HWND hRef = NULL;
    HWND hLogin = NULL;
    HWND hPass = NULL;
};

static LRESULT CALLBACK FilePropertiesWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    FilePropsState* pState = (FilePropsState*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (message) {
    case WM_NCCREATE: {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pState = (FilePropsState*)pCreate->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pState);
        pState->hDlg = hWnd;
        return TRUE;
    }
    case WM_CREATE: {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        // Header Filename
        HWND hName = CreateWindowW(L"STATIC", pState->pItem->filename.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 16, 400, 20, hWnd, (HMENU)101, NULL, NULL);
        SendMessageW(hName, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Type
        HWND hTypeLabel = CreateWindowW(L"STATIC", L"Type:", WS_CHILD | WS_VISIBLE, 20, 46, 70, 18, hWnd, NULL, NULL, NULL);
        HWND hType = CreateWindowW(L"STATIC", L"Application", WS_CHILD | WS_VISIBLE, 100, 46, 300, 18, hWnd, (HMENU)102, NULL, NULL);
        SendMessageW(hTypeLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hType, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Status
        HWND hStatusLabel = CreateWindowW(L"STATIC", L"Status:", WS_CHILD | WS_VISIBLE, 20, 68, 70, 18, hWnd, NULL, NULL, NULL);
        HWND hStatus = CreateWindowW(L"STATIC", (pState->pItem->status == DownloadStatus::Complete) ? L"Complete" : L"Downloading", WS_CHILD | WS_VISIBLE, 100, 68, 300, 18, hWnd, (HMENU)103, NULL, NULL);
        SendMessageW(hStatusLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hStatus, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Size
        std::wstringstream ss;
        double mb = (double)pState->pItem->sizeBytes / (1024.0 * 1024.0);
        ss << std::fixed << std::setprecision(2) << mb << L" MB (" << pState->pItem->sizeBytes << L" Bytes)";
        HWND hSizeLabel = CreateWindowW(L"STATIC", L"Size:", WS_CHILD | WS_VISIBLE, 20, 90, 70, 18, hWnd, NULL, NULL, NULL);
        HWND hSize = CreateWindowW(L"STATIC", ss.str().c_str(), WS_CHILD | WS_VISIBLE, 100, 90, 300, 18, hWnd, (HMENU)104, NULL, NULL);
        SendMessageW(hSizeLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hSize, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Save To
        HWND hSaveToLabel = CreateWindowW(L"STATIC", L"Save To:", WS_CHILD | WS_VISIBLE, 20, 116, 70, 18, hWnd, NULL, NULL, NULL);
        pState->hSaveTo = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pState->pItem->savePath.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 114, 250, 22, hWnd, (HMENU)201, NULL, NULL);
        HWND hMoveBtn = CreateWindowW(L"BUTTON", L"Move", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 360, 114, 60, 22, hWnd, (HMENU)302, NULL, NULL);
        SendMessageW(hSaveToLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(pState->hSaveTo, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hMoveBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Address
        HWND hAddrLabel = CreateWindowW(L"STATIC", L"Address:", WS_CHILD | WS_VISIBLE, 20, 144, 70, 18, hWnd, NULL, NULL, NULL);
        pState->hAddr = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pState->pItem->url.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 142, 320, 22, hWnd, (HMENU)202, NULL, NULL);
        SendMessageW(hAddrLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(pState->hAddr, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Description
        HWND hDescLabel = CreateWindowW(L"STATIC", L"Description:", WS_CHILD | WS_VISIBLE, 20, 172, 70, 18, hWnd, NULL, NULL, NULL);
        pState->hDesc = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pState->pItem->description.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 170, 320, 22, hWnd, (HMENU)203, NULL, NULL);
        SendMessageW(hDescLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(pState->hDesc, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Referer
        HWND hRefSubLabel = CreateWindowW(L"STATIC", L"The web page from which this file was obtained:", WS_CHILD | WS_VISIBLE, 20, 204, 400, 18, hWnd, NULL, NULL, NULL);
        HWND hRefLink = CreateWindowW(L"STATIC", pState->pItem->referer.c_str(), WS_CHILD | WS_VISIBLE, 20, 222, 400, 18, hWnd, NULL, NULL, NULL);
        SendMessageW(hRefSubLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hRefLink, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hRefLabel = CreateWindowW(L"STATIC", L"Referer:", WS_CHILD | WS_VISIBLE, 20, 248, 70, 18, hWnd, NULL, NULL, NULL);
        pState->hRef = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pState->pItem->referer.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 246, 320, 22, hWnd, (HMENU)204, NULL, NULL);
        SendMessageW(hRefLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(pState->hRef, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Login & Password
        HWND hLoginLabel = CreateWindowW(L"STATIC", L"Login", WS_CHILD | WS_VISIBLE, 20, 276, 70, 18, hWnd, NULL, NULL, NULL);
        pState->hLogin = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pState->pItem->login.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 274, 180, 22, hWnd, (HMENU)205, NULL, NULL);
        HWND hPassLabel = CreateWindowW(L"STATIC", L"Password", WS_CHILD | WS_VISIBLE, 20, 304, 70, 18, hWnd, NULL, NULL, NULL);
        pState->hPass = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pState->pItem->password.c_str(), WS_CHILD | WS_VISIBLE | ES_PASSWORD | ES_AUTOHSCROLL, 100, 302, 180, 22, hWnd, (HMENU)206, NULL, NULL);
        SendMessageW(hLoginLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(pState->hLogin, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hPassLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(pState->hPass, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Buttons
        HWND hOpenBtn = CreateWindowW(L"BUTTON", L"Open", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 250, 340, 75, 26, hWnd, (HMENU)301, NULL, NULL);
        HWND hOkBtn = CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 335, 340, 75, 26, hWnd, (HMENU)IDOK, NULL, NULL);
        SendMessageW(hOpenBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hOkBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

        return 0;
    }

    case WM_COMMAND: {
        if (!pState) return 0;
        int id = LOWORD(wParam);

        if (id == 301) { // Open
            if (pState->pItem && !pState->pItem->savePath.empty()) {
                ShellExecuteW(hWnd, L"open", pState->pItem->savePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
            return 0;
        }

        if (id == 302) { // Move
            OPENFILENAMEW ofn = { 0 };
            wchar_t szFile[MAX_PATH] = { 0 };
            wcsncpy_s(szFile, pState->pItem->savePath.c_str(), MAX_PATH - 1);

            ofn.lStructSize = sizeof(OPENFILENAMEW);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = L"Move File To...";
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

            if (GetSaveFileNameW(&ofn)) {
                MoveFileW(pState->pItem->savePath.c_str(), szFile);
                pState->pItem->savePath = szFile;
                SetWindowTextW(pState->hSaveTo, szFile);
            }
            return 0;
        }

        if (id == IDOK) {
            wchar_t buf[1024];
            GetWindowTextW(pState->hSaveTo, buf, 1024); pState->pItem->savePath = buf;
            std::wstring sp = pState->pItem->savePath;
            size_t slashPos = sp.find_last_of(L"\\/");
            if (slashPos != std::wstring::npos && slashPos + 1 < sp.length()) {
                pState->pItem->filename = sp.substr(slashPos + 1);
            }
            pState->pItem->category = DetectCategoryFromFilename(pState->pItem->filename);

            GetWindowTextW(pState->hAddr, buf, 1024); pState->pItem->url = buf;
            GetWindowTextW(pState->hDesc, buf, 1024); pState->pItem->description = buf;
            GetWindowTextW(pState->hRef, buf, 1024); pState->pItem->referer = buf;
            GetWindowTextW(pState->hLogin, buf, 1024); pState->pItem->login = buf;
            GetWindowTextW(pState->hPass, buf, 1024); pState->pItem->password = buf;
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

void ShowFilePropertiesDialog(HWND hParent, DownloadItem* pItem) {
    if (!pItem) return;

    static bool s_classRegistered = false;
    if (!s_classRegistered) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = FilePropertiesWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"IDM_FilePropertiesDialogClass";
        RegisterClassExW(&wc);
        s_classRegistered = true;
    }

    FilePropsState state;
    state.pItem = pItem;

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"IDM_FilePropertiesDialogClass",
        L"File Properties",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 460, 420,
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
