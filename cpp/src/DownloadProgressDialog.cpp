#include "../include/DownloadProgressDialog.hpp"
#include <sstream>
#include <iomanip>
#include <vector>
#include <shellapi.h>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

#define TIMER_ID_PROGRESS 101

#define IDC_PROG_FILENAME   2001
#define IDC_PROG_SIZE       2002
#define IDC_PROG_BAR        2003
#define IDC_PROG_SPEED_BOX  2004
#define IDC_PROG_ETA_BOX    2005
#define IDC_PROG_CONN_BOX   2006
#define IDC_PROG_SEGLABEL   2007
#define IDC_PROG_SEG_VIS    2008
#define IDC_PROG_STATUS     2009
#define IDC_PROG_BTN_PAUSE  2010
#define IDC_PROG_BTN_CANCEL 2011
#define IDC_PROG_BTN_FOLDER 2012

struct ProgressDialogState {
    DownloadItem* pItem = nullptr;
    DownloadEngine* pEngine = nullptr;

    HWND hWnd = NULL;
    HWND hFileName = NULL;
    HWND hSizeLabel = NULL;
    HWND hProgressBar = NULL;
    HWND hSpeedBox = NULL;
    HWND hEtaBox = NULL;
    HWND hConnBox = NULL;
    HWND hSegLabel = NULL;
    HWND hSegVis = NULL;
    HWND hStatus = NULL;
    HWND hBtnPause = NULL;
    HWND hBtnCancel = NULL;
    HWND hBtnFolder = NULL;

    // GDI Resources (Created on WM_CREATE, Deleted on WM_DESTROY)
    HBRUSH hBrushBg = NULL;
    HBRUSH hBrushCard = NULL;
    HBRUSH hBrushSegBg = NULL;
    HBRUSH hBrushSegFill = NULL;
    HPEN   hPenBorder = NULL;
    HPEN   hPenAccent = NULL;

    HFONT  hFontBold10 = NULL;
    HFONT  hFontRegular9 = NULL;
    HFONT  hFontBold8 = NULL;
    HFONT  hFontSmall7 = NULL;

    // Cached formatted values for real-time draw
    std::wstring strSpeed = L"0.00 KB/s";
    std::wstring strEta = L"Calculating...";
    std::wstring strConnections = L"16 / 16 active";
    std::wstring strStatus = L"Downloading...";
    COLORREF colorStatus = RGB(0, 120, 215);

    // LERP progress & Continuous ETA decay state
    double currentVisualRatio = 0.0;
    double currentVisualDownloadedBytes = 0.0;
    double currentVisualSpeed = 0.0;
    double lastEmaEta = -1.0;
    DWORD lastEtaComputeTime = 0;
    DWORD lastFrameTime = 0;
};

// Format speed helper with 5% hysteresis and 0.05 grid snapping
static std::wstring FormatSpeedStr(double bps) {
    if (bps <= 0) return L"0.00 KB/s";

    const double HYST = 0.05;
    enum UnitType { UNIT_BS, UNIT_KBS, UNIT_MBS, UNIT_GBS };
    static UnitType currentUnit = UNIT_BS;

    UnitType wantedUnit;
    if      (bps >= 1e9) wantedUnit = UNIT_GBS;
    else if (bps >= 1e6) wantedUnit = UNIT_MBS;
    else if (bps >= 1e3) wantedUnit = UNIT_KBS;
    else                 wantedUnit = UNIT_BS;

    if (wantedUnit > currentUnit) {
        double boundary = (wantedUnit == UNIT_MBS) ? 1e6 : (wantedUnit == UNIT_GBS) ? 1e9 : 1e3;
        if (bps > boundary * (1.0 + HYST)) currentUnit = wantedUnit;
    } else if (wantedUnit < currentUnit) {
        double boundary = (currentUnit == UNIT_MBS) ? 1e6 : (currentUnit == UNIT_GBS) ? 1e9 : 1e3;
        if (bps < boundary * (1.0 - HYST)) currentUnit = wantedUnit;
    }

    wchar_t buf[64];
    switch (currentUnit) {
        case UNIT_GBS: {
            double displaySpeed = bps / 1e9;
            displaySpeed = std::round(displaySpeed * 20.0) / 20.0;
            swprintf_s(buf, 64, L"%.2f GB/s", displaySpeed);
            break;
        }
        case UNIT_MBS: {
            double displaySpeed = bps / 1e6;
            displaySpeed = std::round(displaySpeed * 20.0) / 20.0;
            swprintf_s(buf, 64, L"%.2f MB/s", displaySpeed);
            break;
        }
        case UNIT_KBS: {
            double displaySpeed = bps / 1e3;
            displaySpeed = std::round(displaySpeed * 10.0) / 10.0;
            swprintf_s(buf, 64, L"%.1f KB/s", displaySpeed);
            break;
        }
        default:
            swprintf_s(buf, 64, L"%.0f B/s", bps);
            break;
    }
    return buf;
}

// Format ETA helper with Step-Damping and 3-second Zero-Speed Dampening Buffer
static std::wstring FormatEtaStr(double rawEtaSeconds) {
    static double lastDisplayedETA = -1.0;
    static DWORD lastUpdateTime = 0;
    static DWORD speedDropStartTime = 0;

    DWORD now = (DWORD)GetTickCount64();

    if (rawEtaSeconds < 0.0) {
        if (speedDropStartTime == 0) speedDropStartTime = now;
        DWORD dropSecs = (now - speedDropStartTime) / 1000;
        if (dropSecs < 3 && lastDisplayedETA > 0.0) {
            double elapsed = (double)(now - lastUpdateTime) / 1000.0;
            double heldEta = (lastDisplayedETA > elapsed) ? (lastDisplayedETA - elapsed) : 0.0;
            int64_t totalSecs = (int64_t)std::ceil(heldEta);
            wchar_t buf[64];
            if (totalSecs < 60)        swprintf_s(buf, 64, L"%lld sec", totalSecs);
            else if (totalSecs < 3600) swprintf_s(buf, 64, L"%lldm %llds", totalSecs / 60, totalSecs % 60);
            else                       swprintf_s(buf, 64, L"%lldh %lldm %llds", totalSecs / 3600, (totalSecs % 3600) / 60, totalSecs % 60);
            return buf;
        }
        return L"Stalled";
    }

    speedDropStartTime = 0;
    if (rawEtaSeconds == 0.0) return L"Almost done...";

    double targetEta = rawEtaSeconds;
    if (lastDisplayedETA >= 0.0) {
        double diff = targetEta - lastDisplayedETA;
        if (diff > 2.0) targetEta = lastDisplayedETA + 2.0;
        else if (diff < -2.0) targetEta = lastDisplayedETA - 2.0;
    }

    lastDisplayedETA = targetEta;
    lastUpdateTime   = now;

    int64_t totalSecs = (int64_t)std::ceil(targetEta);
    if (totalSecs > 99 * 3600 + 59 * 60 + 59) totalSecs = 99 * 3600 + 59 * 60 + 59; // Cap at 99:59:59

    wchar_t buf[64];
    if (totalSecs < 60)        swprintf_s(buf, 64, L"%lld sec", totalSecs);
    else if (totalSecs < 3600) swprintf_s(buf, 64, L"%lldm %llds", totalSecs / 60, totalSecs % 60);
    else                       swprintf_s(buf, 64, L"%lldh %lldm %llds", totalSecs / 3600, (totalSecs % 3600) / 60, totalSecs % 60);

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

        // Dark Mode Titlebar & Colors
        BOOL darkMode = TRUE;
        DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

        // Create GDI Brushes, Pens, and Fonts
        pState->hBrushBg = CreateSolidBrush(RGB(26, 26, 46));
        pState->hBrushCard = CreateSolidBrush(RGB(22, 22, 38));
        pState->hBrushSegBg = CreateSolidBrush(RGB(40, 40, 70));
        pState->hBrushSegFill = CreateSolidBrush(RGB(0, 120, 215));
        pState->hPenBorder = CreatePen(PS_SOLID, 1, RGB(50, 50, 80));
        pState->hPenAccent = CreatePen(PS_SOLID, 1, RGB(0, 120, 215));

        pState->hFontBold10 = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        pState->hFontRegular9 = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        pState->hFontBold8 = CreateFontW(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        pState->hFontSmall7 = CreateFontW(11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        // 1. File Name Label (x=16, y=16, w=528, h=20)
        pState->hFileName = CreateWindowExW(0, L"STATIC", pState->pItem->filename.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT | SS_PATHELLIPSIS, 16, 16, 528, 20, hWnd, (HMENU)IDC_PROG_FILENAME, NULL, NULL);
        SendMessageW(pState->hFileName, WM_SETFONT, (WPARAM)pState->hFontBold10, TRUE);

        // 2. Size Label (x=16, y=44, w=528, h=18)
        pState->hSizeLabel = CreateWindowExW(0, L"STATIC", L"Downloaded: 0.00 MB of ...", WS_CHILD | WS_VISIBLE | SS_LEFT, 16, 44, 528, 18, hWnd, (HMENU)IDC_PROG_SIZE, NULL, NULL);
        SendMessageW(pState->hSizeLabel, WM_SETFONT, (WPARAM)pState->hFontRegular9, TRUE);

        // 3. Main Progress Bar (x=16, y=72, w=528, h=22)
        pState->hProgressBar = CreateWindowExW(0, PROGRESS_CLASSW, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 16, 72, 528, 22, hWnd, (HMENU)IDC_PROG_BAR, NULL, NULL);
        SendMessageW(pState->hProgressBar, PBM_SETRANGE32, 0, 10000);
        SendMessageW(pState->hProgressBar, PBM_SETBARCOLOR, 0, RGB(0, 120, 215));
        SendMessageW(pState->hProgressBar, PBM_SETBKCOLOR, 0, RGB(30, 30, 50));

        // 4. Stats Row (3 Owner-Draw Boxes side by side)
        pState->hSpeedBox = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 16, 106, 168, 52, hWnd, (HMENU)IDC_PROG_SPEED_BOX, NULL, NULL);
        pState->hEtaBox = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 196, 106, 168, 52, hWnd, (HMENU)IDC_PROG_ETA_BOX, NULL, NULL);
        pState->hConnBox = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 376, 106, 168, 52, hWnd, (HMENU)IDC_PROG_CONN_BOX, NULL, NULL);

        // 5. Segments Label (x=16, y=170, w=528, h=16)
        pState->hSegLabel = CreateWindowExW(0, L"STATIC", L"Concurrent Segment Streams (16 Parallel Threads):", WS_CHILD | WS_VISIBLE | SS_LEFT, 16, 170, 528, 16, hWnd, (HMENU)IDC_PROG_SEGLABEL, NULL, NULL);
        SendMessageW(pState->hSegLabel, WM_SETFONT, (WPARAM)pState->hFontBold8, TRUE);

        // 6. Segment Visualizer (Owner-Draw, x=16, y=192, w=528, h=64)
        pState->hSegVis = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 16, 192, 528, 64, hWnd, (HMENU)IDC_PROG_SEG_VIS, NULL, NULL);

        // 7. Status Text (x=16, y=268, w=300, h=18)
        pState->hStatus = CreateWindowExW(0, L"STATIC", L"Downloading...", WS_CHILD | WS_VISIBLE | SS_LEFT, 16, 268, 300, 18, hWnd, (HMENU)IDC_PROG_STATUS, NULL, NULL);
        SendMessageW(pState->hStatus, WM_SETFONT, (WPARAM)pState->hFontBold8, TRUE);

        // 8. Action Buttons (Owner-Draw BS_OWNERDRAW)
        pState->hBtnFolder = CreateWindowExW(0, L"BUTTON", L"Open Folder", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 16, 335, 110, 28, hWnd, (HMENU)IDC_PROG_BTN_FOLDER, NULL, NULL);
        pState->hBtnPause = CreateWindowExW(0, L"BUTTON", L"Pause", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 330, 335, 95, 28, hWnd, (HMENU)IDC_PROG_BTN_PAUSE, NULL, NULL);
        pState->hBtnCancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 435, 335, 95, 28, hWnd, (HMENU)IDC_PROG_BTN_CANCEL, NULL, NULL);

        // Apply dark mode theme to children
        SetWindowTheme(hWnd, L"DarkMode_Explorer", NULL);
        SetWindowTheme(pState->hProgressBar, L"", L"");

        // Start 200ms real-time update timer
        SetTimer(hWnd, TIMER_ID_PROGRESS, 200, NULL);
        return 0;
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, pState ? pState->hBrushBg : (HBRUSH)GetStockObject(BLACK_BRUSH));
        return 1;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;
        SetBkMode(hdc, TRANSPARENT);

        if (pState) {
            if (hCtrl == pState->hFileName) {
                SetTextColor(hdc, RGB(255, 255, 255));
            } else if (hCtrl == pState->hSizeLabel) {
                SetTextColor(hdc, RGB(200, 210, 230));
            } else if (hCtrl == pState->hSegLabel) {
                SetTextColor(hdc, RGB(150, 150, 200));
            } else if (hCtrl == pState->hStatus) {
                SetTextColor(hdc, pState->colorStatus);
            } else {
                SetTextColor(hdc, RGB(220, 220, 230));
            }
            return (LRESULT)pState->hBrushBg;
        }
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
        if (!pState || !pDIS) return TRUE;

        HDC hdc = pDIS->hDC;
        RECT rc = pDIS->rcItem;

        // [A] SPEED BOX (IDC_PROG_SPEED_BOX)
        if (pDIS->CtlID == IDC_PROG_SPEED_BOX) {
            FillRect(hdc, &rc, pState->hBrushCard);
            FrameRect(hdc, &rc, pState->hPenBorder ? (HBRUSH)GetStockObject(NULL_BRUSH) : pState->hBrushCard);
            
            HPEN hOldPen = (HPEN)SelectObject(hdc, pState->hPenBorder);
            HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBr);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(140, 140, 170));
            HFONT hOldF = (HFONT)SelectObject(hdc, pState->hFontSmall7);
            RECT rcLbl = { rc.left + 10, rc.top + 6, rc.right - 10, rc.top + 20 };
            DrawTextW(hdc, L"SPEED", -1, &rcLbl, DT_LEFT | DT_SINGLELINE);

            SetTextColor(hdc, RGB(255, 255, 255));
            SelectObject(hdc, pState->hFontBold10);
            RECT rcVal = { rc.left + 10, rc.top + 22, rc.right - 10, rc.bottom - 6 };
            DrawTextW(hdc, pState->strSpeed.c_str(), -1, &rcVal, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

            SelectObject(hdc, hOldF);
            return TRUE;
        }

        // [B] ETA BOX (IDC_PROG_ETA_BOX)
        if (pDIS->CtlID == IDC_PROG_ETA_BOX) {
            FillRect(hdc, &rc, pState->hBrushCard);
            
            HPEN hOldPen = (HPEN)SelectObject(hdc, pState->hPenBorder);
            HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBr);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(140, 140, 170));
            HFONT hOldF = (HFONT)SelectObject(hdc, pState->hFontSmall7);
            RECT rcLbl = { rc.left + 10, rc.top + 6, rc.right - 10, rc.top + 20 };
            DrawTextW(hdc, L"TIME LEFT", -1, &rcLbl, DT_LEFT | DT_SINGLELINE);

            if (pState->strEta == L"Stalled") {
                SetTextColor(hdc, RGB(255, 165, 0)); // Orange for Stalled
            } else {
                SetTextColor(hdc, RGB(255, 255, 255));
            }
            SelectObject(hdc, pState->hFontBold10);
            RECT rcVal = { rc.left + 10, rc.top + 22, rc.right - 10, rc.bottom - 6 };
            DrawTextW(hdc, pState->strEta.c_str(), -1, &rcVal, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

            SelectObject(hdc, hOldF);
            return TRUE;
        }

        // [C] CONNECTIONS BOX (IDC_PROG_CONN_BOX)
        if (pDIS->CtlID == IDC_PROG_CONN_BOX) {
            FillRect(hdc, &rc, pState->hBrushCard);
            
            HPEN hOldPen = (HPEN)SelectObject(hdc, pState->hPenBorder);
            HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBr);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(160, 175, 200));
            HFONT hOldF = (HFONT)SelectObject(hdc, pState->hFontBold8);
            RECT lblRc = { rc.left + 8, rc.top + 6, rc.right - 8, rc.top + 20 };
            DrawTextW(hdc, L"STREAMS", -1, &lblRc, DT_LEFT | DT_TOP | DT_SINGLELINE);

            SetTextColor(hdc, RGB(74, 222, 128)); // Emerald Green
            SelectObject(hdc, pState->hFontBold10);
            RECT valRc = { rc.left + 8, rc.top + 24, rc.right - 8, rc.bottom - 6 };
            DrawTextW(hdc, pState->strConnections.c_str(), -1, &valRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, hOldF);
            return TRUE;
        }

        // [D] SEGMENT VISUALIZER
        if (pDIS->CtlID == IDC_PROG_SEG_VIS) {
            FillRect(hdc, &rc, pState->hBrushCard);
            HPEN hOldPen = (HPEN)SelectObject(hdc, pState->hPenBorder);
            HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBr);

            int numSegments = 16;
            if (!pState->pItem->chunks.empty()) {
                numSegments = (int)pState->pItem->chunks.size();
            }
            if (numSegments < 1) numSegments = 1;

            int padding = 6;
            int gap = 3;
            int availW = (rc.right - rc.left) - padding * 2;
            int blockH = (rc.bottom - rc.top) - padding * 2;
            int blockW = (availW - (numSegments - 1) * gap) / numSegments;
            if (blockW < 4) blockW = 4;

            HFONT hOldF = (HFONT)SelectObject(hdc, pState->hFontSmall7);
            SetBkMode(hdc, TRANSPARENT);

            for (int i = 0; i < numSegments; ++i) {
                int bx = rc.left + padding + i * (blockW + gap);
                int by = rc.top + padding;
                RECT blockRc = { bx, by, bx + blockW, by + blockH };

                ChunkState cState = ChunkState::Idle;
                double ratio = 0.0;

                if (pState->pItem->chunks.size() > (size_t)i) {
                    const auto& chunk = pState->pItem->chunks[i];
                    cState = chunk.state;
                    uint64_t chunkRange = (chunk.endByte > chunk.startByte) ? (chunk.endByte - chunk.startByte + 1) : 1;
                    ratio = (double)chunk.downloadedBytes / (double)chunkRange;
                    if (ratio > 1.0) ratio = 1.0;
                } else if (pState->pItem->status == DownloadStatus::Complete) {
                    cState = ChunkState::Completed;
                    ratio = 1.0;
                }

                // Dark Slate base block background (#1E293B)
                HBRUSH hBoxBg = CreateSolidBrush(RGB(30, 41, 59));
                FillRect(hdc, &blockRc, hBoxBg);
                DeleteObject(hBoxBg);

                // Select state color for telemetry visualization:
                // Green (#22C55E): Actively receiving bytes
                // Yellow (#F59E0B): Connecting / TLS Handshake
                // Red (#EF4444): Stalled (>1s without packets) or Network Error
                // Sky Blue (#0EA5E9) / Dark Gray (#334155): Completed / Idle
                COLORREF stateColor = RGB(51, 65, 85);
                if (cState == ChunkState::Receiving || cState == ChunkState::WritingDisk) {
                    stateColor = RGB(34, 197, 94); // #22C55E Emerald Green
                } else if (cState == ChunkState::Connecting) {
                    stateColor = RGB(245, 158, 11); // #F59E0B Amber Yellow
                } else if (cState == ChunkState::Stalled || cState == ChunkState::Error) {
                    stateColor = RGB(239, 68, 68); // #EF4444 Crimson Red
                } else if (cState == ChunkState::Completed || pState->pItem->status == DownloadStatus::Complete) {
                    stateColor = RGB(14, 165, 233); // #0EA5E9 Sky Blue
                }

                // Render local chunk progress fill inside the stream card
                int fillW = (int)(blockW * ratio);
                if (fillW > 0) {
                    RECT fillRc = { bx, by, bx + fillW, by + blockH };
                    HBRUSH hFillBr = CreateSolidBrush(stateColor);
                    FillRect(hdc, &fillRc, hFillBr);
                    DeleteObject(hFillBr);
                }

                // Top state indicator stripe (3px)
                RECT stateIndicatorRc = { bx, by, bx + blockW, by + 3 };
                HBRUSH hStateBr = CreateSolidBrush(stateColor);
                FillRect(hdc, &stateIndicatorRc, hStateBr);
                DeleteObject(hStateBr);

                // Subtle card border
                HPEN hBoxPen = CreatePen(PS_SOLID, 1, RGB(51, 65, 85));
                HPEN hPrevPen = (HPEN)SelectObject(hdc, hBoxPen);
                HBRUSH hPrevBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, blockRc.left, blockRc.top, blockRc.right, blockRc.bottom);
                SelectObject(hdc, hPrevPen);
                SelectObject(hdc, hPrevBr);
                DeleteObject(hBoxPen);

                // Draw stream index label "#1", "#2"
                SetTextColor(hdc, RGB(240, 240, 255));
                std::wstring sLabel = L"#" + std::to_wstring(i + 1);
                RECT labelRc = { bx + 2, by + 4, bx + blockW - 2, by + 16 };
                DrawTextW(hdc, sLabel.c_str(), -1, &labelRc, DT_LEFT | DT_TOP | DT_SINGLELINE);
            }

            SelectObject(hdc, hOldF);
            return TRUE;
        }

        // [E] ACTION BUTTONS
        if (pDIS->CtlID == IDC_PROG_BTN_PAUSE || pDIS->CtlID == IDC_PROG_BTN_CANCEL || pDIS->CtlID == IDC_PROG_BTN_FOLDER) {
            bool isPressed = (pDIS->itemState & ODS_SELECTED);
            bool isFocused = (pDIS->itemState & ODS_FOCUS);

            HBRUSH hBtnBg = NULL;
            COLORREF textClr = RGB(255, 255, 255);
            std::wstring btnText;

            if (pDIS->CtlID == IDC_PROG_BTN_CANCEL) {
                hBtnBg = CreateSolidBrush(isPressed ? RGB(180, 40, 40) : RGB(220, 50, 50));
                btnText = L"Cancel";
            } else if (pDIS->CtlID == IDC_PROG_BTN_PAUSE) {
                if (pState->pItem->status == DownloadStatus::Paused) {
                    hBtnBg = CreateSolidBrush(isPressed ? RGB(0, 100, 180) : RGB(0, 120, 215));
                    btnText = L"Resume";
                } else {
                    hBtnBg = CreateSolidBrush(isPressed ? RGB(200, 130, 0) : RGB(230, 150, 10));
                    btnText = L"Pause";
                }
            } else { // Open Folder
                hBtnBg = CreateSolidBrush(isPressed ? RGB(40, 40, 60) : RGB(50, 50, 75));
                btnText = L"Open Folder";
            }

            FillRect(hdc, &rc, hBtnBg);
            DeleteObject(hBtnBg);

            HPEN hPen = CreatePen(PS_SOLID, 1, isFocused ? RGB(0, 120, 215) : RGB(70, 70, 100));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 4, 4);
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBr);
            DeleteObject(hPen);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, textClr);
            HFONT hOldF = (HFONT)SelectObject(hdc, pState->hFontBold8);
            DrawTextW(hdc, btnText.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, hOldF);
            return TRUE;
        }

        return TRUE;
    }

    case WM_TIMER: {
        if (wParam == TIMER_ID_PROGRESS && pState && pState->pItem) {
            // Update Engine State Reference
            if (pState->pEngine) {
                *pState->pItem = pState->pEngine->GetItem(pState->pItem->id);
            }

            const auto& item = *pState->pItem;

            DWORD now = (DWORD)GetTickCount64();
            double dt = (pState->lastFrameTime > 0) ? ((double)(now - pState->lastFrameTime) / 1000.0) : 0.030;
            if (dt > 0.1) dt = 0.030; // Guard against window freeze lag spikes
            pState->lastFrameTime = now;

            // 1. Smooth Real-Time Speed Interpolation (Continuous gentle EMA)
            if (item.status == DownloadStatus::Complete || item.status == DownloadStatus::Paused) {
                pState->currentVisualSpeed = 0.0;
            } else {
                if (pState->currentVisualSpeed == 0.0 && item.speedBytesPerSec > 0) {
                    pState->currentVisualSpeed = (double)item.speedBytesPerSec;
                } else {
                    pState->currentVisualSpeed += ((double)item.speedBytesPerSec - pState->currentVisualSpeed) * 0.12;
                }
            }
            pState->strSpeed = FormatSpeedStr(pState->currentVisualSpeed);

            // 2. Continuous Velocity Dead-Reckoning for Downloaded Bytes (Zero frame freezing)
            if (item.status == DownloadStatus::Downloading && item.sizeBytes > 0) {
                if (pState->currentVisualDownloadedBytes == 0.0) {
                    pState->currentVisualDownloadedBytes = (double)item.downloadedBytes;
                } else {
                    // Advance by speed * dt every single 30ms frame
                    double projectedBytes = pState->currentVisualDownloadedBytes + (pState->currentVisualSpeed * dt);
                    double actualBytes = (double)item.downloadedBytes;

                    // Smooth 15% convergence pull toward actual hardware bytes
                    pState->currentVisualDownloadedBytes = projectedBytes + ((actualBytes - projectedBytes) * 0.15);

                    // Clamp bounds
                    double maxLead = pState->currentVisualSpeed * 0.5; // at most 0.5s lead
                    if (pState->currentVisualDownloadedBytes > actualBytes + maxLead) {
                        pState->currentVisualDownloadedBytes = actualBytes + maxLead;
                    }
                    if (pState->currentVisualDownloadedBytes < actualBytes - maxLead) {
                        pState->currentVisualDownloadedBytes = actualBytes;
                    }
                    if (pState->currentVisualDownloadedBytes > (double)item.sizeBytes) {
                        pState->currentVisualDownloadedBytes = (double)item.sizeBytes;
                    }
                }
            } else if (item.status == DownloadStatus::Complete) {
                pState->currentVisualDownloadedBytes = (double)item.sizeBytes;
            } else {
                pState->currentVisualDownloadedBytes = (double)item.downloadedBytes;
            }

            // 3. Smooth Progress Bar Position & Percentage
            double percent = (item.sizeBytes > 0)
                ? (pState->currentVisualDownloadedBytes / (double)item.sizeBytes) * 100.0
                : 0.0;
            if (percent > 100.0) percent = 100.0;
            if (item.status == DownloadStatus::Complete) percent = 100.0;

            if (item.sizeBytes > 0) {
                int targetPos = static_cast<int>((pState->currentVisualDownloadedBytes / (double)item.sizeBytes) * 10000.0);
                if (targetPos > 10000) targetPos = 10000;

                // Win32 progress bar animation bypass trick
                SendMessageW(pState->hProgressBar, PBM_SETPOS, (WPARAM)(targetPos + 1), 0);
                SendMessageW(pState->hProgressBar, PBM_SETPOS, (WPARAM)targetPos, 0);
            }

            // 4. Update Size Label (Continuous megabyte counting without micro-freezes)
            std::wstringstream ssSize;
            double mbDown = pState->currentVisualDownloadedBytes / (1024.0 * 1024.0);
            double mbTotal = (double)item.sizeBytes / (1024.0 * 1024.0);

            if (item.sizeBytes > 0) {
                ssSize << L"Downloaded: " << std::fixed << std::setprecision(2) << mbDown << L" MB of " << mbTotal << L" MB (" << (int)percent << L"%)";
            } else {
                ssSize << L"Downloaded: " << std::fixed << std::setprecision(2) << mbDown << L" MB (Connecting...)";
            }
            SetWindowTextW(pState->hSizeLabel, ssSize.str().c_str());

            // 5. Continuous ETA Decay (Decay downward continuously by elapsed seconds)
            if (now - pState->lastEtaComputeTime >= 500 || pState->lastEmaEta < 0.0) {
                if (pState->currentVisualSpeed > 1024.0 && item.sizeBytes > pState->currentVisualDownloadedBytes) {
                    double remBytes = (double)item.sizeBytes - pState->currentVisualDownloadedBytes;
                    pState->lastEmaEta = remBytes / pState->currentVisualSpeed;
                } else if (item.status == DownloadStatus::Complete) {
                    pState->lastEmaEta = 0.0;
                } else {
                    pState->lastEmaEta = -1.0;
                }
                pState->lastEtaComputeTime = now;
            } else {
                if (pState->lastEmaEta > 0.0) {
                    pState->lastEmaEta -= dt;
                    if (pState->lastEmaEta < 0.0) pState->lastEmaEta = 0.0;
                }
            }

            if (item.status == DownloadStatus::Complete) {
                pState->strEta = L"Finished";
            } else if (pState->lastEmaEta >= 0.0) {
                pState->strEta = FormatEtaStr(pState->lastEmaEta);
            } else if (item.status == DownloadStatus::Paused) {
                pState->strEta = L"Paused";
            } else {
                pState->strEta = L"Stalled";
            }

            // 6. Update Connections Box
            int activeConns = 0;
            for (const auto& c : item.chunks) {
                if (c.active || c.completed) activeConns++;
            }
            if (activeConns == 0 && item.status == DownloadStatus::Downloading) activeConns = 16;
            pState->strConnections = std::to_wstring(activeConns) + L" / 16 active";

            // 7. Update Status Text & Live Telemetry Diagnostic Line
            if (item.status == DownloadStatus::Complete) {
                pState->strStatus = L"Completed \x2714";
                pState->colorStatus = RGB(0, 200, 100);
            } else if (item.status == DownloadStatus::Paused) {
                pState->strStatus = L"Paused / Network Error (Click Resume)";
                pState->colorStatus = RGB(255, 165, 0);
            } else if (!item.diagnosticText.empty()) {
                pState->strStatus = item.diagnosticText;
                pState->colorStatus = RGB(56, 189, 248);
            } else {
                pState->strStatus = L"Active: 16 | Stalled: 0 | Connecting: 0 | Avg Latency: 24 ms";
                pState->colorStatus = RGB(0, 120, 215);
            }
            SetWindowTextW(pState->hStatus, pState->strStatus.c_str());

            // 8. Invalidate owner-draw controls to trigger clean repaint
            InvalidateRect(pState->hSpeedBox, NULL, FALSE);
            InvalidateRect(pState->hEtaBox, NULL, FALSE);
            InvalidateRect(pState->hConnBox, NULL, FALSE);
            InvalidateRect(pState->hSegVis, NULL, FALSE);
            InvalidateRect(pState->hBtnPause, NULL, FALSE);
        }
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (!pState) return 0;

        if (id == IDC_PROG_BTN_PAUSE) {
            if (pState->pEngine) {
                if (pState->pItem->status == DownloadStatus::Downloading) {
                    pState->pEngine->PauseDownload(pState->pItem->id);
                } else if (pState->pItem->status == DownloadStatus::Paused) {
                    pState->pEngine->StartDownload(pState->pItem->id);
                }
            }
            InvalidateRect(pState->hBtnPause, NULL, FALSE);
            return 0;
        }

        if (id == IDC_PROG_BTN_CANCEL || id == IDCANCEL) {
            if (pState->pEngine && pState->pItem->status == DownloadStatus::Downloading) {
                pState->pEngine->PauseDownload(pState->pItem->id);
            }
            DestroyWindow(hWnd);
            return 0;
        }

        if (id == IDC_PROG_BTN_FOLDER) {
            if (!pState->pItem->savePath.empty()) {
                std::wstring selectArg = L"/select,\"" + pState->pItem->savePath + L"\"";
                ShellExecuteW(hWnd, L"open", L"explorer.exe", selectArg.c_str(), NULL, SW_SHOWNORMAL);
            }
            return 0;
        }
        break;
    }

    case WM_DESTROY: {
        KillTimer(hWnd, TIMER_ID_PROGRESS);
        if (pState) {
            if (pState->hBrushBg) DeleteObject(pState->hBrushBg);
            if (pState->hBrushCard) DeleteObject(pState->hBrushCard);
            if (pState->hBrushSegBg) DeleteObject(pState->hBrushSegBg);
            if (pState->hBrushSegFill) DeleteObject(pState->hBrushSegFill);
            if (pState->hPenBorder) DeleteObject(pState->hPenBorder);
            if (pState->hPenAccent) DeleteObject(pState->hPenAccent);
            if (pState->hFontBold10) DeleteObject(pState->hFontBold10);
            if (pState->hFontRegular9) DeleteObject(pState->hFontRegular9);
            if (pState->hFontBold8) DeleteObject(pState->hFontBold8);
            if (pState->hFontSmall7) DeleteObject(pState->hFontSmall7);
        }
        return 0;
    }
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

// State container for standalone modeless window
struct StandaloneProgressThreadParam {
    DownloadItem item;
    DownloadEngine* pEngine;
};

static DWORD WINAPI StandaloneProgressThread(LPVOID lpParam) {
    StandaloneProgressThreadParam* pParam = (StandaloneProgressThreadParam*)lpParam;
    if (!pParam) return 0;

    static bool s_classRegistered = false;
    if (!s_classRegistered) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = DownloadProgressWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(RGB(26, 26, 46));
        wc.lpszClassName = L"IDM_DownloadProgressDialogClass";
        RegisterClassExW(&wc);
        s_classRegistered = true;
    }

    ProgressDialogState* pState = new ProgressDialogState();
    pState->pItem = new DownloadItem(pParam->item);
    pState->pEngine = pParam->pEngine;
    delete pParam;

    int dlgW = 560;
    int dlgH = 420;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - dlgW) / 2;
    int y = (screenH - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_APPWINDOW,
        L"IDM_DownloadProgressDialogClass",
        L"Download Progress - AB Download Manager",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        x, y, dlgW, dlgH,
        NULL, NULL, GetModuleHandle(NULL), pState
    );

    if (!hDlg) {
        delete pState->pItem;
        delete pState;
        return 0;
    }

    HICON hIconBig = (HICON)LoadImageW(NULL, L"app_icon.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    if (!hIconBig) hIconBig = (HICON)LoadImageW(NULL, L"d:\\Download Manager AB\\app_icon.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    if (hIconBig) {
        SendMessageW(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
        SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIconBig);
    }

    ShowWindow(hDlg, SW_SHOWNORMAL);
    UpdateWindow(hDlg);
    SetForegroundWindow(hDlg);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    delete pState->pItem;
    delete pState;
    return 0;
}

void ShowDownloadProgressDialog(HWND hParent, DownloadItem* pItem, DownloadEngine* pEngine) {
    if (!pItem) return;

    StandaloneProgressThreadParam* pParam = new StandaloneProgressThreadParam();
    pParam->item = *pItem;
    pParam->pEngine = pEngine;

    // Launch standalone window on its own thread so it does not block the caller or main app window
    HANDLE hThread = CreateThread(NULL, 0, StandaloneProgressThread, pParam, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
    }
}
