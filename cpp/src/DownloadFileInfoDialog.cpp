#include "../include/Models.hpp"
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <sstream>
#include <iomanip>

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

        // URL row
        HWND hUrlLbl = CreateWindowW(L"STATIC", L"URL", WS_CHILD | WS_VISIBLE, 18, 18, 70, 18, hWnd, NULL, NULL, NULL);
        pState->hUrlEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pState->pItem->url.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 90, 16, 390, 22, hWnd, (HMENU)101, NULL, NULL);

        // Category row
        HWND hCatLbl = CreateWindowW(L"STATIC", L"Category", WS_CHILD | WS_VISIBLE, 18, 48, 70, 18, hWnd, NULL, NULL, NULL);
        pState->hCatCombo = CreateWindowW(WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 90, 46, 170, 140, hWnd, (HMENU)102, NULL, NULL);
        HWND hCatPlus = CreateWindowW(L"BUTTON", L"+", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 268, 45, 24, 24, hWnd, NULL, NULL, NULL);

        // Save As row
        HWND hSaveLbl = CreateWindowW(L"STATIC", L"Save As", WS_CHILD | WS_VISIBLE, 18, 78, 70, 18, hWnd, NULL, NULL, NULL);
        pState->hSaveEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pState->pItem->savePath.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 90, 76, 350, 22, hWnd, (HMENU)103, NULL, NULL);
        HWND hBrowseBtn = CreateWindowW(L"BUTTON", L"...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 448, 75, 32, 24, hWnd, (HMENU)201, NULL, NULL);

        // Checkbox: Remember path
        HWND hChkRem = CreateWindowW(L"BUTTON", L"Remember this path for \"Selected\" category", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 90, 106, 350, 18, hWnd, (HMENU)105, NULL, NULL);
        SendMessageW(hChkRem, BM_SETCHECK, BST_CHECKED, 0);

        // Folder preview box
        std::wstring catDir = pState->baseDownloads + (pState->pItem->category.empty() ? L"General" : pState->pItem->category) + L"\\";
        pState->hPathBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", catDir.c_str(), WS_CHILD | WS_VISIBLE | ES_READONLY, 90, 128, 390, 22, hWnd, (HMENU)104, NULL, NULL);

        // Description row
        HWND hDescLbl = CreateWindowW(L"STATIC", L"Description", WS_CHILD | WS_VISIBLE, 18, 160, 70, 18, hWnd, NULL, NULL, NULL);
        pState->hDescEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pState->pItem->description.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 90, 158, 390, 22, hWnd, (HMENU)106, NULL, NULL);

        // Action buttons at bottom
        HWND hLaterBtn = CreateWindowW(L"BUTTON", L"Download Later", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 90, 235, 110, 26, hWnd, (HMENU)301, NULL, NULL);
        HWND hStartBtn = CreateWindowW(L"BUTTON", L"Start Download", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 210, 235, 120, 26, hWnd, (HMENU)IDOK, NULL, NULL);
        HWND hCancelBtn = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 340, 235, 80, 26, hWnd, (HMENU)IDCANCEL, NULL, NULL);

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
        else if (pState->pItem->category == L"Music") sel = 3;
        else if (pState->pItem->category == L"Programs") sel = 4;
        else if (pState->pItem->category == L"Video") sel = 5;
        SendMessageW(pState->hCatCombo, CB_SETCURSEL, sel, 0);

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
                std::wstring newDir = pState->baseDownloads + pState->pItem->category + L"\\";
                EnsureDirectoryExists(newDir + pState->pItem->filename);
                pState->pItem->savePath = newDir + pState->pItem->filename;
                SetWindowTextW(pState->hSaveEdit, pState->pItem->savePath.c_str());
                SetWindowTextW(pState->hPathBox, newDir.c_str());
            }
            return 0;
        }

        if (id == 201) { // "..." Browse button
            OPENFILENAMEW ofn = { 0 };
            wchar_t szFile[MAX_PATH] = { 0 };
            wcsncpy_s(szFile, pState->pItem->savePath.c_str(), MAX_PATH - 1);

            wchar_t szDir[MAX_PATH] = { 0 };
            wcsncpy_s(szDir, pState->pItem->savePath.c_str(), MAX_PATH - 1);
            PathRemoveFileSpecW(szDir);

            ofn.lStructSize = sizeof(OPENFILENAMEW);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrInitialDir = (wcslen(szDir) > 0) ? szDir : NULL;
            ofn.lpstrTitle = L"Select Destination File";
            ofn.lpstrFilter = L"All Files (*.*)\0*.*\0Video Files (*.mp4;*.mkv;*.webm)\0*.mp4;*.mkv;*.webm\0Audio Files (*.mp3;*.flac;*.wav)\0*.mp3;*.flac;*.wav\0Compressed Archives (*.zip;*.rar;*.7z)\0*.zip;*.rar;*.7z\0";
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
            EnsureDirectoryExists(pState->pItem->savePath);
            pState->startImmediately = true;
            pState->confirmed = true;
            DestroyWindow(hWnd);
            return 0;
        }

        if (id == 301) { // "Download Later"
            wchar_t buf[2048] = { 0 };
            GetWindowTextW(pState->hUrlEdit, buf, 2048); pState->pItem->url = buf;
            GetWindowTextW(pState->hSaveEdit, buf, 2048); pState->pItem->savePath = buf;
            GetWindowTextW(pState->hDescEdit, buf, 2048); pState->pItem->description = buf;
            EnsureDirectoryExists(pState->pItem->savePath);
            pState->startImmediately = false;
            pState->confirmed = true;
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

bool ShowDownloadFileInfoDialog(HWND hParent, DownloadItem& item, bool& outStartImmediately) {
    static bool s_classRegistered = false;
    if (!s_classRegistered) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
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

    // Ensure category folder exists
    std::wstring catDir = state.baseDownloads + (item.category.empty() ? L"General" : item.category) + L"\\";
    EnsureDirectoryExists(catDir + item.filename);

    if (item.savePath.empty()) {
        item.savePath = catDir + item.filename;
    }

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"IDM_DownloadFileInfoDialogClass",
        L"Download File Info",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 510, 320,
        hParent, NULL, GetModuleHandle(NULL), &state
    );

    if (!hDlg) return false;

    if (hParent) EnableWindow(hParent, FALSE);

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
