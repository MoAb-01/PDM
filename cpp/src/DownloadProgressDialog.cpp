#include "../include/DownloadProgressDialog.hpp"
#include "../include/DownloadCompleteDialog.hpp"
#include "../include/Models.hpp"
#include "../include/SegmentedDownloader.hpp"
#include <sstream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <shellapi.h>
#include <shlwapi.h>
#include <commctrl.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comctl32.lib")

#define TIMER_ID_PROGRESS 101

#define IDC_PROG_TAB        2000
#define IDC_PROG_URL        2001
#define IDC_PROG_STATUS     2002
#define IDC_PROG_SIZE       2003
#define IDC_PROG_DOWN       2004
#define IDC_PROG_RATE       2005
#define IDC_PROG_TIMELEFT   2006
#define IDC_PROG_RESUME     2007
#define IDC_PROG_BAR        2008

#define IDC_PROG_BTN_DETAILS 2009
#define IDC_PROG_BTN_PAUSE   2010
#define IDC_PROG_BTN_CANCEL  2011

#define IDC_PROG_SEGLABEL   2012
#define IDC_PROG_SEG_BAR    2013
#define IDC_PROG_CONN_LIST  2014

struct ProgressDialogState {
    std::wstring itemId;
    DownloadItem currentItem;
    DownloadEngine* pEngine = nullptr;

    HWND hWnd = NULL;
    HWND hTab = NULL;
    HWND hUrlText = NULL;
    HWND hStatusVal = NULL;
    HWND hSizeVal = NULL;
    HWND hDownVal = NULL;
    HWND hRateVal = NULL;
    HWND hTimeLeftVal = NULL;
    HWND hResumeVal = NULL;
    HWND hProgressBar = NULL;

    HWND hBtnDetails = NULL;
    HWND hBtnPause = NULL;
    HWND hBtnCancel = NULL;

    HWND hSegLabel = NULL;
    HWND hSegBar = NULL;
    HWND hConnList = NULL;

    bool detailsVisible = true;
    int expandedHeight = 480;
    int collapsedHeight = 255;

    // GDI Resources
    HFONT hFontRegular = NULL;
    HFONT hFontBold = NULL;
    HFONT hFontUrl = NULL;
    HBRUSH hBrushBg = NULL;

    // Formatting cache
    std::wstring strSpeed = L"0 B/sec";
    std::wstring strEta = L"Calculating...";
    std::wstring strStatus = L"Receiving data...";
    double currentVisualRatio = 0.0;
    int completeTicks = 0;
};

// Format bytes in IDM style (e.g. "17,745 MB" or "4,360 MB")
static std::wstring FormatIdmSize(uint64_t bytes) {
    if (bytes == 0) return L"0 MB";
    double mb = (double)bytes / (1024.0 * 1024.0);
    if (mb >= 1024.0) {
        double gb = mb / 1024.0;
        wchar_t buf[64];
        swprintf_s(buf, 64, L"%.3f GB", gb);
        for (wchar_t* p = buf; *p; ++p) if (*p == L'.') *p = L',';
        return buf;
    }
    wchar_t buf[64];
    swprintf_s(buf, 64, L"%.3f MB", mb);
    for (wchar_t* p = buf; *p; ++p) if (*p == L'.') *p = L',';
    return buf;
}

static std::wstring FormatIdmSpeed(double bps) {
    if (bps <= 0) return L"0 B/sec";
    wchar_t buf[64];
    if (bps >= 1024.0 * 1024.0) {
        double mbs = bps / (1024.0 * 1024.0);
        swprintf_s(buf, 64, L"%.3f MB/sec", mbs);
        for (wchar_t* p = buf; *p; ++p) if (*p == L'.') *p = L',';
        return buf;
    } else if (bps >= 1024.0) {
        double kbs = bps / 1024.0;
        swprintf_s(buf, 64, L"%.2f KB/sec", kbs);
        for (wchar_t* p = buf; *p; ++p) if (*p == L'.') *p = L',';
        return buf;
    }
    swprintf_s(buf, 64, L"%.0f B/sec", bps);
    return buf;
}

static std::wstring FormatIdmEta(double rawEtaSeconds) {
    if (rawEtaSeconds < 0.0) return L"Stalled";
    if (rawEtaSeconds <= 0.5) return L"0 sec";
    int64_t totalSecs = (int64_t)std::round(rawEtaSeconds);
    if (totalSecs < 60) {
        wchar_t buf[32];
        swprintf_s(buf, 32, L"%lld sec", totalSecs);
        return buf;
    }
    int64_t m = totalSecs / 60;
    int64_t s = totalSecs % 60;
    if (m < 60) {
        wchar_t buf[32];
        swprintf_s(buf, 32, L"%lld min %lld sec", m, s);
        return buf;
    }
    int64_t h = m / 60;
    m = m % 60;
    wchar_t buf[32];
    swprintf_s(buf, 32, L"%lld hr %lld min", h, m);
    return buf;
}

static LRESULT CALLBACK DownloadProgressWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    ProgressDialogState* pState = (ProgressDialogState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW* pCS = (CREATESTRUCTW*)lParam;
        pState = (ProgressDialogState*)pCS->lpCreateParams;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)pState);
        pState->hWnd = hWnd;

        pState->hBrushBg = (HBRUSH)(COLOR_BTNFACE + 1);
        pState->hFontRegular = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        pState->hFontBold = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        pState->hFontUrl = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        // 1. Top Tab Control
        pState->hTab = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 8, 6, 508, 26, hWnd, (HMENU)IDC_PROG_TAB, NULL, NULL);
        SendMessageW(pState->hTab, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);
        TCITEMW tie = { 0 };
        tie.mask = TCIF_TEXT;
        tie.pszText = (LPWSTR)L"Download status";
        TabCtrl_InsertItem(pState->hTab, 0, &tie);
        tie.pszText = (LPWSTR)L"Speed Limiter";
        TabCtrl_InsertItem(pState->hTab, 1, &tie);
        tie.pszText = (LPWSTR)L"Options on completion";
        TabCtrl_InsertItem(pState->hTab, 2, &tie);
        TabCtrl_SetCurSel(pState->hTab, 0);

        // 2. URL row
        pState->hUrlText = CreateWindowExW(0, L"STATIC", pState->currentItem.url.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT | SS_PATHELLIPSIS, 18, 40, 488, 18, hWnd, (HMENU)IDC_PROG_URL, NULL, NULL);
        SendMessageW(pState->hUrlText, WM_SETFONT, (WPARAM)pState->hFontUrl, TRUE);

        // 3. Stats Labels & Value rows
        int y = 62;
        int rowH = 18;
        int lblX = 18, lblW = 110;
        int valX = 135, valW = 370;

        HWND hLbl;
        hLbl = CreateWindowW(L"STATIC", L"Status", WS_CHILD | WS_VISIBLE, lblX, y, lblW, rowH, hWnd, NULL, NULL, NULL);
        SendMessageW(hLbl, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);
        pState->hStatusVal = CreateWindowW(L"STATIC", L"Receiving data...", WS_CHILD | WS_VISIBLE, valX, y, valW, rowH, hWnd, (HMENU)IDC_PROG_STATUS, NULL, NULL);
        SendMessageW(pState->hStatusVal, WM_SETFONT, (WPARAM)pState->hFontBold, TRUE);

        y += rowH;
        hLbl = CreateWindowW(L"STATIC", L"File size", WS_CHILD | WS_VISIBLE, lblX, y, lblW, rowH, hWnd, NULL, NULL, NULL);
        SendMessageW(hLbl, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);
        pState->hSizeVal = CreateWindowW(L"STATIC", L"Calculating...", WS_CHILD | WS_VISIBLE, valX, y, valW, rowH, hWnd, (HMENU)IDC_PROG_SIZE, NULL, NULL);
        SendMessageW(pState->hSizeVal, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);

        y += rowH;
        hLbl = CreateWindowW(L"STATIC", L"Downloaded", WS_CHILD | WS_VISIBLE, lblX, y, lblW, rowH, hWnd, NULL, NULL, NULL);
        SendMessageW(hLbl, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);
        pState->hDownVal = CreateWindowW(L"STATIC", L"0 MB ( 0.00 % )", WS_CHILD | WS_VISIBLE, valX, y, valW, rowH, hWnd, (HMENU)IDC_PROG_DOWN, NULL, NULL);
        SendMessageW(pState->hDownVal, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);

        y += rowH;
        hLbl = CreateWindowW(L"STATIC", L"Transfer rate", WS_CHILD | WS_VISIBLE, lblX, y, lblW, rowH, hWnd, NULL, NULL, NULL);
        SendMessageW(hLbl, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);
        pState->hRateVal = CreateWindowW(L"STATIC", L"0 B/sec", WS_CHILD | WS_VISIBLE, valX, y, valW, rowH, hWnd, (HMENU)IDC_PROG_RATE, NULL, NULL);
        SendMessageW(pState->hRateVal, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);

        y += rowH;
        hLbl = CreateWindowW(L"STATIC", L"Time left", WS_CHILD | WS_VISIBLE, lblX, y, lblW, rowH, hWnd, NULL, NULL, NULL);
        SendMessageW(hLbl, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);
        pState->hTimeLeftVal = CreateWindowW(L"STATIC", L"Calculating...", WS_CHILD | WS_VISIBLE, valX, y, valW, rowH, hWnd, (HMENU)IDC_PROG_TIMELEFT, NULL, NULL);
        SendMessageW(pState->hTimeLeftVal, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);

        y += rowH;
        hLbl = CreateWindowW(L"STATIC", L"Resume capability", WS_CHILD | WS_VISIBLE, lblX, y, lblW, rowH, hWnd, NULL, NULL, NULL);
        SendMessageW(hLbl, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);
        pState->hResumeVal = CreateWindowW(L"STATIC", L"Yes", WS_CHILD | WS_VISIBLE, valX, y, valW, rowH, hWnd, (HMENU)IDC_PROG_RESUME, NULL, NULL);
        SendMessageW(pState->hResumeVal, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);

        // 4. Main Green Progress Bar (y=174, h=16)
        y = 174;
        pState->hProgressBar = CreateWindowExW(0, PROGRESS_CLASSW, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 18, y, 488, 16, hWnd, (HMENU)IDC_PROG_BAR, NULL, NULL);
        SendMessageW(pState->hProgressBar, PBM_SETRANGE32, 0, 10000);
        SendMessageW(pState->hProgressBar, PBM_SETBARCOLOR, 0, RGB(34, 197, 94)); // Classic Green

        // 5. Action Buttons (y=198, h=25)
        y = 198;
        pState->hBtnDetails = CreateWindowW(L"BUTTON", L"<< Hide details", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 18, y, 115, 25, hWnd, (HMENU)IDC_PROG_BTN_DETAILS, NULL, NULL);
        SendMessageW(pState->hBtnDetails, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);

        pState->hBtnPause = CreateWindowW(L"BUTTON", L"Pause", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 305, y, 95, 25, hWnd, (HMENU)IDC_PROG_BTN_PAUSE, NULL, NULL);
        SendMessageW(pState->hBtnPause, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);

        pState->hBtnCancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 410, y, 95, 25, hWnd, (HMENU)IDC_PROG_BTN_CANCEL, NULL, NULL);
        SendMessageW(pState->hBtnCancel, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);

        // 6. Section Label: Start positions and download progress by connections
        y = 230;
        pState->hSegLabel = CreateWindowW(L"STATIC", L"Start positions and download progress by connections", WS_CHILD | WS_VISIBLE | SS_CENTER, 18, y, 488, 16, hWnd, (HMENU)IDC_PROG_SEGLABEL, NULL, NULL);
        SendMessageW(pState->hSegLabel, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);

        // 7. Owner-Drawn Segment Connection Progress Bar (y=248, h=16)
        y = 248;
        pState->hSegBar = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 18, y, 488, 16, hWnd, (HMENU)IDC_PROG_SEG_BAR, NULL, NULL);

        // 8. Connection Details ListView (y=270, h=155)
        y = 270;
        pState->hConnList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL, 18, y, 488, 155, hWnd, (HMENU)IDC_PROG_CONN_LIST, NULL, NULL);
        ListView_SetExtendedListViewStyle(pState->hConnList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        SendMessageW(pState->hConnList, WM_SETFONT, (WPARAM)pState->hFontRegular, TRUE);

        LVCOLUMNW lvc = { 0 };
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        lvc.pszText = (LPWSTR)L"N.";
        lvc.cx = 40;
        ListView_InsertColumn(pState->hConnList, 0, &lvc);

        lvc.pszText = (LPWSTR)L"Downloaded";
        lvc.cx = 120;
        ListView_InsertColumn(pState->hConnList, 1, &lvc);

        lvc.pszText = (LPWSTR)L"Info";
        lvc.cx = 300;
        ListView_InsertColumn(pState->hConnList, 2, &lvc);

        SetTimer(hWnd, TIMER_ID_PROGRESS, 100, NULL);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;
        SetBkMode(hdc, TRANSPARENT);
        if (pState && hCtrl == pState->hStatusVal) {
            SetTextColor(hdc, RGB(0, 50, 200)); // Distinct blue for Status
        }
        return (LRESULT)GetStockObject(COLOR_BTNFACE + 1);
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
        if (!pState || !pDIS) return TRUE;

        // Custom render Segmented Connection Bar (IDC_PROG_SEG_BAR)
        if (pDIS->CtlID == IDC_PROG_SEG_BAR) {
            HDC hdc = pDIS->hDC;
            RECT rc = pDIS->rcItem;

            // Background & border
            HBRUSH bgBrush = CreateSolidBrush(RGB(245, 247, 250));
            FillRect(hdc, &rc, bgBrush);
            DeleteObject(bgBrush);

            HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(180, 195, 215));
            HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            DeleteObject(borderPen);

            int barW = rc.right - rc.left - 2;
            int barH = rc.bottom - rc.top - 2;
            if (barW <= 0 || barH <= 0) return TRUE;

            uint64_t totalSize = (pState->currentItem.sizeBytes > 0) ? pState->currentItem.sizeBytes : 1;

            // Draw active connection chunks & start tick marks
            HBRUSH chunkBrush = CreateSolidBrush(RGB(14, 165, 233)); // Cyan / Blue IDM gradient
            HPEN redTickPen = CreatePen(PS_SOLID, 1, RGB(220, 38, 38)); // Red start position ticks

            if (pState->currentItem.liveSlots) {
                const auto& slotArray = *(pState->currentItem.liveSlots);
                for (size_t i = 0; i < slotArray.size(); ++i) {
                    uint64_t start = slotArray[i].startByte.load(std::memory_order_relaxed);
                    uint64_t down = slotArray[i].downloadedBytes.load(std::memory_order_relaxed);
                    uint64_t curr = start + down;
                    if (start >= totalSize && totalSize > 1) continue;

                    int startX = rc.left + 1 + (int)((double)start / (double)totalSize * barW);
                    int endX = rc.left + 1 + (int)((double)curr / (double)totalSize * barW);
                    if (endX < startX) endX = startX;
                    if (endX > rc.right - 1) endX = rc.right - 1;

                    // Draw filled chunk
                    if (endX > startX) {
                        RECT chunkRc = { startX, rc.top + 1, endX, rc.bottom - 1 };
                        FillRect(hdc, &chunkRc, chunkBrush);
                    }

                    // Draw red starting position divider line
                    SelectObject(hdc, redTickPen);
                    MoveToEx(hdc, startX, rc.top + 1, NULL);
                    LineTo(hdc, startX, rc.bottom - 1);
                }
            }

            DeleteObject(chunkBrush);
            DeleteObject(redTickPen);
            return TRUE;
        }
        break;
    }

    case WM_COMMAND: {
        if (!pState) return 0;
        int id = LOWORD(wParam);

        if (id == IDC_PROG_BTN_DETAILS) { // Toggle << Hide details / Details >>
            pState->detailsVisible = !pState->detailsVisible;
            SetWindowTextW(pState->hBtnDetails, pState->detailsVisible ? L"<< Hide details" : L"Details >>");

            ShowWindow(pState->hSegLabel, pState->detailsVisible ? SW_SHOW : SW_HIDE);
            ShowWindow(pState->hSegBar, pState->detailsVisible ? SW_SHOW : SW_HIDE);
            ShowWindow(pState->hConnList, pState->detailsVisible ? SW_SHOW : SW_HIDE);

            RECT rc;
            GetWindowRect(hWnd, &rc);
            int newH = pState->detailsVisible ? pState->expandedHeight : pState->collapsedHeight;
            SetWindowPos(hWnd, NULL, rc.left, rc.top, rc.right - rc.left, newH, SWP_NOMOVE | SWP_NOZORDER);
            return 0;
        }

        if (id == IDC_PROG_BTN_PAUSE) {
            if (pState->pEngine) {
                if (pState->currentItem.status == DownloadStatus::Downloading) {
                    pState->pEngine->PauseDownload(pState->itemId);
                    pState->currentItem.status = DownloadStatus::Paused;
                    SetWindowTextW(pState->hBtnPause, L"Resume");
                    SetWindowTextW(pState->hStatusVal, L"Paused");
                } else {
                    pState->pEngine->StartDownload(pState->itemId);
                    pState->currentItem.status = DownloadStatus::Downloading;
                    SetWindowTextW(pState->hBtnPause, L"Pause");
                    SetWindowTextW(pState->hStatusVal, L"Receiving data...");
                }
            }
            return 0;
        }

        if (id == IDC_PROG_BTN_CANCEL) {
            if (pState->pEngine) {
                pState->pEngine->PauseDownload(pState->itemId);
            }
            DestroyWindow(hWnd);
            return 0;
        }
        break;
    }

    case WM_TIMER: {
        if (wParam == TIMER_ID_PROGRESS && pState && pState->pEngine) {
            auto items = pState->pEngine->GetDownloads();
            for (const auto& it : items) {
                if (it.id == pState->itemId) {
                    pState->currentItem = it;
                    break;
                }
            }

            uint64_t downloaded = pState->currentItem.downloadedBytes;
            uint64_t total = pState->currentItem.sizeBytes;
            uint64_t speed = pState->currentItem.speedBytesPerSec;

            double ratio = (total > 0) ? (double)downloaded / (double)total : 0.0;
            if (ratio > 1.0) ratio = 1.0;
            double pct = ratio * 100.0;

            // Window Title update (e.g. "24% Internet Download Manager video.mp4")
            wchar_t titleBuf[256];
            swprintf_s(titleBuf, 256, L"%.0f%% AB Download Manager - %s", pct, pState->currentItem.filename.c_str());
            SetWindowTextW(hWnd, titleBuf);

            // 1. Status Text
            if (pState->currentItem.status == DownloadStatus::Complete) {
                SetWindowTextW(pState->hStatusVal, L"Complete");
            } else if (pState->currentItem.status == DownloadStatus::Merging) {
                SetWindowTextW(pState->hStatusVal, L"Merging video & audio streams...");
            } else if (pState->currentItem.status == DownloadStatus::Paused) {
                SetWindowTextW(pState->hStatusVal, L"Paused");
            } else if (pState->currentItem.status == DownloadStatus::Error) {
                std::wstring errStr = L"Error";
                if (!pState->currentItem.diagnosticText.empty()) {
                    errStr += L" (" + pState->currentItem.diagnosticText + L")";
                }
                SetWindowTextW(pState->hStatusVal, errStr.c_str());
            } else {
                std::wstring stStr = L"Receiving data...";
                if (!pState->currentItem.diagnosticText.empty()) {
                    stStr = pState->currentItem.diagnosticText;
                }
                SetWindowTextW(pState->hStatusVal, stStr.c_str());
            }

            // 2. File size
            SetWindowTextW(pState->hSizeVal, FormatIdmSize(total).c_str());

            // 3. Downloaded
            wchar_t downBuf[128];
            swprintf_s(downBuf, 128, L"%s ( %.2f %% )", FormatIdmSize(downloaded).c_str(), pct);
            SetWindowTextW(pState->hDownVal, downBuf);

            // 4. Transfer rate
            SetWindowTextW(pState->hRateVal, FormatIdmSpeed((double)speed).c_str());

            // 5. Time left
            double rem = (total > downloaded) ? (double)(total - downloaded) : 0.0;
            double etaSec = (speed > 0) ? (rem / (double)speed) : -1.0;
            SetWindowTextW(pState->hTimeLeftVal, FormatIdmEta(etaSec).c_str());

            // 6. Main Progress Bar
            SendMessageW(pState->hProgressBar, PBM_SETPOS, (WPARAM)(ratio * 10000), 0);

            // 7. Redraw Segment Visualizer
            if (pState->detailsVisible && pState->hSegBar) {
                InvalidateRect(pState->hSegBar, NULL, TRUE);
            }

            // 8. Update Connections ListView
            if (pState->detailsVisible && pState->hConnList && pState->currentItem.liveSlots) {
                const auto& slotArray = *(pState->currentItem.liveSlots);
                int count = (int)slotArray.size();
                int currentListCount = ListView_GetItemCount(pState->hConnList);

                // Adjust items count
                if (currentListCount < count) {
                    for (int i = currentListCount; i < count; ++i) {
                        LVITEMW lvi = { 0 };
                        lvi.mask = LVIF_TEXT;
                        lvi.iItem = i;
                        wchar_t numStr[16];
                        swprintf_s(numStr, 16, L"%d", i + 1);
                        lvi.pszText = numStr;
                        ListView_InsertItem(pState->hConnList, &lvi);
                    }
                }

                for (int i = 0; i < count; ++i) {
                    uint64_t slotDown = slotArray[i].downloadedBytes.load(std::memory_order_relaxed);
                    wchar_t kbBuf[64];
                    swprintf_s(kbBuf, 64, L"%llu KB", slotDown / 1024ULL);
                    ListView_SetItemText(pState->hConnList, i, 1, kbBuf);

                    std::wstring info = L"Receiving data...";
                    if (!pState->currentItem.diagnosticText.empty()) {
                        info = pState->currentItem.diagnosticText;
                    }
                    bool comp = slotArray[i].completed.load(std::memory_order_relaxed);
                    ChunkState cState = slotArray[i].state.load(std::memory_order_relaxed);
                    if (comp || cState == ChunkState::Completed) {
                        info = L"Complete";
                    } else if (cState == ChunkState::Idle) {
                        info = L"Send GET...";
                    } else if (cState == ChunkState::Error) {
                        info = pState->currentItem.diagnosticText.empty() ? L"Error" : pState->currentItem.diagnosticText;
                    }
                    ListView_SetItemText(pState->hConnList, i, 2, (LPWSTR)info.c_str());
                }
            }

            // If Complete, update UI to 100% and show Download Complete Dialog after brief visual confirmation
            if (pState->currentItem.status == DownloadStatus::Complete) {
                SetWindowTextW(pState->hStatusVal, L"Complete");
                SetWindowTextW(pState->hDownVal, FormatIdmSize(pState->currentItem.sizeBytes > 0 ? pState->currentItem.sizeBytes : downloaded).c_str());
                SetWindowTextW(pState->hTimeLeftVal, L"0 sec");
                SendMessageW(pState->hProgressBar, PBM_SETPOS, 10000, 0);

                pState->completeTicks++;
                if (pState->completeTicks >= 8) { // ~800ms
                    KillTimer(hWnd, TIMER_ID_PROGRESS);
                    DownloadItem completedItem = pState->currentItem;
                    HWND hParent = GetParent(hWnd);
                    PostMessageW(hWnd, WM_CLOSE, 0, 0);
                    ShowDownloadCompleteDialog(hParent, completedItem);
                }
            }
        }
        break;
    }

    case WM_CLOSE: {
        KillTimer(hWnd, TIMER_ID_PROGRESS);
        DestroyWindow(hWnd);
        return 0;
    }

    case WM_DESTROY: {
        if (pState) {
            if (pState->hFontRegular) DeleteObject(pState->hFontRegular);
            if (pState->hFontBold) DeleteObject(pState->hFontBold);
            if (pState->hFontUrl) DeleteObject(pState->hFontUrl);
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

void ShowDownloadProgressDialog(HWND hParent, const DownloadItem& item, DownloadEngine* pEngine) {
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icex);

    static bool s_registered = false;
    if (!s_registered) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = DownloadProgressWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"IDM_DownloadProgressDialogClass";
        RegisterClassExW(&wc);
        s_registered = true;
    }

    auto* pState = new ProgressDialogState();
    pState->itemId = item.id;
    pState->currentItem = item;
    pState->pEngine = pEngine;

    int dlgW = 540;
    int dlgH = pState->expandedHeight;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - dlgW) / 2;
    int y = (screenH - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"IDM_DownloadProgressDialogClass",
        L"0% AB Download Manager",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        x, y, dlgW, dlgH,
        hParent, NULL, GetModuleHandle(NULL), pState
    );

    if (!hDlg) {
        delete pState;
        return;
    }

    ShowWindow(hDlg, SW_SHOWNORMAL);
    UpdateWindow(hDlg);
    SetForegroundWindow(hDlg);
    BringWindowToTop(hDlg);
}

void ShowDownloadProgressDialog(HWND hParent, DownloadItem* pItem, DownloadEngine* pEngine) {
    if (pItem) {
        ShowDownloadProgressDialog(hParent, *pItem, pEngine);
    }
}
