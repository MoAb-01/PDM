#include "../include/DownloadCompleteDialog.hpp"
#include "../include/IconFactory.hpp"
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

struct CompleteDialogState {
    DownloadItem item;
    HWND hDlg = NULL;
    HWND hIconPic = NULL;
    HWND hHeader = NULL;
    HWND hSubheader = NULL;
    HWND hAddrEdit = NULL;
    HWND hPathEdit = NULL;
    HWND hChkDontShow = NULL;

    HFONT hFontBold = NULL;
    HFONT hFontRegular = NULL;
    HICON hCompleteIcon = NULL;
};

static std::wstring FormatCompleteSizeStr(uint64_t bytes) {
    if (bytes == 0) return L"0.00 MB (0 Bytes)";
    double mb = (double)bytes / (1024.0 * 1024.0);
    wchar_t buf[128];
    swprintf_s(buf, 128, L"Downloaded %.2f MB (%llu Bytes)", mb, bytes);
    return buf;
}

static LRESULT CALLBACK DownloadCompleteWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    CompleteDialogState* pState = (CompleteDialogState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    switch (message) {
    case WM_NCCREATE: {
        CREATESTRUCTW* pCS = (CREATESTRUCTW*)lParam;
        pState = (CompleteDialogState*)pCS->lpCreateParams;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)pState);
        pState->hDlg = hWnd;
        return TRUE;
    }
    case WM_CREATE: {
        pState->hFontRegular = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        pState->hFontBold = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        // Top Left Diamond / Golden Envelope Icon (x=16, y=14, w=44, h=44)
        pState->hIconPic = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ICON, 18, 14, 44, 44, hWnd, NULL, NULL, NULL);
        pState->hCompleteIcon = IconFactory::CreateCompleteIcon(40);
        if (pState->hCompleteIcon) {
            SendMessageW(pState->hIconPic, STM_SETICON, (WPARAM)pState->hCompleteIcon, 0);
        }

        // Header: "Download complete" (x=72, y=14)
        pState->hHeader = CreateWindowW(L"STATIC", L"Download complete", WS_CHILD | WS_VISIBLE, 72, 14, 400, 20, hWnd, NULL, NULL, NULL);
        SendMessageW(pState->hHeader, WM_SETFONT, (WPARAM)pState->hFontBold, TRUE);

        // Subheader: "Downloaded X.XX MB (XXXXX Bytes)" (x=72, y=36)
        std::wstring subStr = FormatCompleteSizeStr(pState->item.sizeBytes > 0 ? pState->item.sizeBytes : pState->item.downloadedBytes);
        pState->hSubheader = CreateWindowW(L"STATIC", subStr.c_str(), WS_CHILD | WS_VISIBLE, 72, 36, 400, 18, hWnd, NULL, NULL, NULL);
        SendMessageW(pState->hSubheader, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);

        // Address row (y=66)
        HWND hAddrLbl = CreateWindowW(L"STATIC", L"Address", WS_CHILD | WS_VISIBLE, 18, 66, 460, 16, hWnd, NULL, NULL, NULL);
        SendMessageW(hAddrLbl, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);
        pState->hAddrEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pState->item.url.c_str(), WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL, 18, 84, 460, 22, hWnd, NULL, NULL, NULL);
        SendMessageW(pState->hAddrEdit, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);

        // The file saved as row (y=112)
        HWND hPathLbl = CreateWindowW(L"STATIC", L"The file saved as", WS_CHILD | WS_VISIBLE, 18, 112, 460, 16, hWnd, NULL, NULL, NULL);
        SendMessageW(hPathLbl, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);
        pState->hPathEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pState->item.savePath.c_str(), WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL, 18, 130, 460, 22, hWnd, NULL, NULL, NULL);
        SendMessageW(pState->hPathEdit, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);

        // Buttons row (y=164)
        HWND hBtnOpen = CreateWindowW(L"BUTTON", L"Open", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 18, 164, 95, 26, hWnd, (HMENU)1001, NULL, NULL);
        HWND hBtnOpenWith = CreateWindowW(L"BUTTON", L"Open with...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 120, 164, 100, 26, hWnd, (HMENU)1002, NULL, NULL);
        HWND hBtnFolder = CreateWindowW(L"BUTTON", L"Open folder", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 226, 164, 100, 26, hWnd, (HMENU)1003, NULL, NULL);
        HWND hBtnClose = CreateWindowW(L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 378, 164, 100, 26, hWnd, (HMENU)1004, NULL, NULL);

        SendMessageW(hBtnOpen, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);
        SendMessageW(hBtnOpenWith, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);
        SendMessageW(hBtnFolder, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);
        SendMessageW(hBtnClose, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);

        // Checkbox: Don't show this dialog again (y=200)
        pState->hChkDontShow = CreateWindowW(L"BUTTON", L"Don't show this dialog again", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 198, 300, 18, hWnd, (HMENU)1005, NULL, NULL);
        SendMessageW(pState->hChkDontShow, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);

        return 0;
    }

    case WM_COMMAND: {
        if (!pState) return 0;
        int id = LOWORD(wParam);

        if (id == 1001) { // "Open"
            if (PathFileExistsW(pState->item.savePath.c_str())) {
                ShellExecuteW(hWnd, L"open", pState->item.savePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
            DestroyWindow(hWnd);
            return 0;
        }

        if (id == 1002) { // "Open with..."
            if (PathFileExistsW(pState->item.savePath.c_str())) {
                ShellExecuteW(hWnd, L"openas", pState->item.savePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
            DestroyWindow(hWnd);
            return 0;
        }

        if (id == 1003) { // "Open folder"
            if (PathFileExistsW(pState->item.savePath.c_str())) {
                std::wstring param = L"/select,\"" + pState->item.savePath + L"\"";
                ShellExecuteW(NULL, L"open", L"explorer.exe", param.c_str(), NULL, SW_SHOWNORMAL);
            } else {
                wchar_t szDir[MAX_PATH] = { 0 };
                wcsncpy_s(szDir, pState->item.savePath.c_str(), MAX_PATH - 1);
                PathRemoveFileSpecW(szDir);
                ShellExecuteW(NULL, L"open", szDir, NULL, NULL, SW_SHOWNORMAL);
            }
            DestroyWindow(hWnd);
            return 0;
        }

        if (id == 1004 || id == IDCANCEL) { // "Close"
            DestroyWindow(hWnd);
            return 0;
        }
        break;
    }

    case WM_CLOSE: {
        DestroyWindow(hWnd);
        return 0;
    }

    case WM_DESTROY: {
        if (pState) {
            if (pState->hCompleteIcon) DestroyIcon(pState->hCompleteIcon);
            if (pState->hFontBold) DeleteObject(pState->hFontBold);
            if (pState->hFontRegular) DeleteObject(pState->hFontRegular);
        }
        return 0;
    }

    case WM_NCDESTROY: {
        if (pState) {
            delete pState;
        }
        return 0;
    }
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

bool ShowDownloadCompleteDialog(HWND hParent, const DownloadItem& item) {
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icex);

    static bool s_registered = false;
    if (!s_registered) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = DownloadCompleteWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"IDM_DownloadCompleteDialogClass";
        RegisterClassExW(&wc);
        s_registered = true;
    }

    auto* pState = new CompleteDialogState();
    pState->item = item;

    int dlgW = 510;
    int dlgH = 265;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - dlgW) / 2;
    int y = (screenH - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"IDM_DownloadCompleteDialogClass",
        L"Download complete",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, dlgW, dlgH,
        hParent, NULL, GetModuleHandle(NULL), pState
    );

    if (!hDlg) {
        delete pState;
        return false;
    }

    ShowWindow(hDlg, SW_SHOWNORMAL);
    UpdateWindow(hDlg);
    SetForegroundWindow(hDlg);

    return true;
}
