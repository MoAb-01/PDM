#pragma once
#include <windows.h>
#include <unknwn.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <string>

#pragma comment(lib, "gdiplus.lib")

class IconFactory {
public:
    static void Init() {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);
    }

    static void Shutdown() {
        Gdiplus::GdiplusShutdown(m_gdiplusToken);
    }

    static HICON CreateColoredIcon(int size, COLORREF primaryColor, int iconType) {
        Gdiplus::Bitmap bitmap(size, size, PixelFormat32bppARGB);
        {
            Gdiplus::Graphics g(&bitmap);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            g.Clear(Gdiplus::Color(0, 0, 0, 0)); // fully transparent ARGB

            Gdiplus::Color gdiColor(255,
                GetRValue(primaryColor),
                GetGValue(primaryColor),
                GetBValue(primaryColor));
            Gdiplus::SolidBrush brush(gdiColor);

            int pad       = size / 5;
            int innerSize = size - pad * 2;

            if (iconType == 0) { // Plus (Add URL)
                g.FillEllipse(&brush, pad - 1, pad - 1, innerSize + 2, innerSize + 2);
                Gdiplus::Pen whitePen(Gdiplus::Color(255, 255, 255, 255), 2.5f);
                int mid = size / 2;
                g.DrawLine(&whitePen, mid, pad + 3, mid, size - pad - 3);
                g.DrawLine(&whitePen, pad + 3, mid, size - pad - 3, mid);
            }
            else if (iconType == 1) { // Play Triangle (Resume)
                Gdiplus::Point pts[3] = {
                    Gdiplus::Point(pad + 2, pad),
                    Gdiplus::Point(size - pad, size / 2),
                    Gdiplus::Point(pad + 2, size - pad)
                };
                g.FillPolygon(&brush, pts, 3);
            }
            else if (iconType == 2) { // Square (Stop)
                g.FillRectangle(&brush, pad + 1, pad + 1, innerSize - 2, innerSize - 2);
            }
            else if (iconType == 3) { // Octagon (Stop All)
                g.FillEllipse(&brush, pad, pad, innerSize, innerSize);
                Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 255, 255, 255));
                g.FillRectangle(&whiteBrush, size / 2 - 4, size / 2 - 4, 8, 8);
            }
            else if (iconType == 4) { // Trash (Delete)
                g.FillRectangle(&brush, pad + 2, pad + 4, innerSize - 4, innerSize - 4);
                Gdiplus::Pen whitePen(Gdiplus::Color(255, 255, 255, 255), 1.5f);
                g.DrawLine(&whitePen, pad, pad + 4, size - pad, pad + 4);
            }
            else if (iconType == 5) { // Checkmarks (Delete Completed)
                g.FillEllipse(&brush, pad, pad, innerSize, innerSize);
                Gdiplus::Pen whitePen(Gdiplus::Color(255, 255, 255, 255), 2.0f);
                g.DrawLine(&whitePen, pad + 3, size / 2, size / 2 - 1, size - pad - 4);
                g.DrawLine(&whitePen, size / 2 - 1, size - pad - 4, size - pad - 2, pad + 4);
            }
            else if (iconType == 6) { // Gear (Options)
                g.FillEllipse(&brush, pad, pad, innerSize, innerSize);
                Gdiplus::SolidBrush darkBrush(Gdiplus::Color(255, 40, 40, 40));
                g.FillEllipse(&darkBrush, size / 2 - 3, size / 2 - 3, 6, 6);
            }
            else if (iconType == 7) { // Clock (Scheduler)
                g.FillEllipse(&brush, pad, pad, innerSize, innerSize);
                Gdiplus::Pen whitePen(Gdiplus::Color(255, 255, 255, 255), 1.5f);
                g.DrawLine(&whitePen, size / 2, size / 2, size / 2, pad + 4);
                g.DrawLine(&whitePen, size / 2, size / 2, size - pad - 4, size / 2);
            }
            else if (iconType == 8) { // Start Queue
                g.FillEllipse(&brush, pad, pad, innerSize, innerSize);
                Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 255, 255, 255));
                Gdiplus::Point pts[3] = {
                    Gdiplus::Point(size / 2 - 2, size / 2 - 4),
                    Gdiplus::Point(size / 2 + 5, size / 2),
                    Gdiplus::Point(size / 2 - 2, size / 2 + 4)
                };
                g.FillPolygon(&whiteBrush, pts, 3);
            }
            else if (iconType == 9) { // Stop Queue
                g.FillEllipse(&brush, pad, pad, innerSize, innerSize);
                Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 255, 255, 255));
                g.FillRectangle(&whiteBrush, size / 2 - 3, size / 2 - 3, 6, 6);
            }
            else if (iconType == 10) { // Radio / Sniffer Hub
                g.FillEllipse(&brush, pad, pad, innerSize, innerSize);
                Gdiplus::Pen whitePen(Gdiplus::Color(255, 255, 255, 255), 1.5f);
                g.DrawArc(&whitePen, pad + 2, pad + 2, innerSize - 4, innerSize - 4, 180, 180);
                Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 255, 255, 255));
                g.FillEllipse(&whiteBrush, size / 2 - 2, size / 2 - 2, 4, 4);
            }
            else if (iconType == 20) { // Folder
                g.FillRectangle(&brush, pad, pad + 3, innerSize, innerSize - 3);
                g.FillRectangle(&brush, pad, pad, innerSize / 2, 5);
            }
            else { // Document / File badge
                g.FillRectangle(&brush, pad + 2, pad, innerSize - 4, innerSize);
            }
        }

        HICON hIcon = nullptr;
        if (bitmap.GetHICON(&hIcon) == Gdiplus::Ok) {
            return hIcon;
        }
        return nullptr;
    }

    static HICON CreateCategoryIcon(int size, const std::wstring& category) {
        Gdiplus::Bitmap bitmap(size, size, PixelFormat32bppARGB);
        {
            Gdiplus::Graphics g(&bitmap);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            g.Clear(Gdiplus::Color(0, 0, 0, 0));

            if (category == L"Compressed") {
                // Stack of 3 WinRAR-style books (Red, Green, Blue) with yellow belt
                int bW = size - 8;
                int bH = (size - 12) / 3;
                int startX = 4;
                int startY = 4;

                // Red book (top)
                Gdiplus::SolidBrush redBrush(Gdiplus::Color(255, 215, 38, 56));
                g.FillRectangle(&redBrush, startX, startY, bW, bH);

                // Green book (middle)
                Gdiplus::SolidBrush greenBrush(Gdiplus::Color(255, 46, 160, 67));
                g.FillRectangle(&greenBrush, startX, startY + bH + 2, bW, bH);

                // Blue book (bottom)
                Gdiplus::SolidBrush blueBrush(Gdiplus::Color(255, 9, 105, 218));
                g.FillRectangle(&blueBrush, startX, startY + (bH + 2) * 2, bW, bH);

                // Yellow buckle strap down the center
                int strapW = size / 6;
                int strapX = size / 2 - strapW / 2;
                Gdiplus::SolidBrush beltBrush(Gdiplus::Color(255, 245, 158, 11));
                g.FillRectangle(&beltBrush, strapX, startY - 1, strapW, (bH + 2) * 3);

                // Silver buckle in center
                Gdiplus::Pen bucklePen(Gdiplus::Color(255, 255, 255, 255), 1.5f);
                g.DrawRectangle(&bucklePen, strapX - 2, startY + bH + 1, strapW + 4, bH);
            }
            else if (category == L"Video") {
                // Film strip / slate in slate-dark with cyan play triangle
                int pad = 4;
                Gdiplus::SolidBrush slateBrush(Gdiplus::Color(255, 30, 41, 59));
                g.FillRectangle(&slateBrush, pad, pad, size - pad * 2, size - pad * 2);

                // Sprocket holes along top and bottom
                Gdiplus::SolidBrush holeBrush(Gdiplus::Color(255, 241, 245, 249));
                for (int x = pad + 3; x < size - pad - 4; x += 7) {
                    g.FillRectangle(&holeBrush, x, pad + 2, 4, 3);
                    g.FillRectangle(&holeBrush, x, size - pad - 5, 4, 3);
                }

                // Cyan play triangle in center
                Gdiplus::SolidBrush playBrush(Gdiplus::Color(255, 6, 182, 212));
                Gdiplus::Point pts[3] = {
                    Gdiplus::Point(size / 2 - 4, size / 2 - 6),
                    Gdiplus::Point(size / 2 + 6, size / 2),
                    Gdiplus::Point(size / 2 - 4, size / 2 + 6)
                };
                g.FillPolygon(&playBrush, pts, 3);
            }
            else if (category == L"Programs") {
                // Application setup box with blue header and gears
                int pad = 4;
                Gdiplus::SolidBrush boxBrush(Gdiplus::Color(255, 248, 250, 252));
                g.FillRectangle(&boxBrush, pad, pad, size - pad * 2, size - pad * 2);
                Gdiplus::Pen borderPen(Gdiplus::Color(255, 100, 116, 139), 1.5f);
                g.DrawRectangle(&borderPen, pad, pad, size - pad * 2, size - pad * 2);

                // Top caption bar
                Gdiplus::SolidBrush barBrush(Gdiplus::Color(255, 37, 99, 235));
                g.FillRectangle(&barBrush, pad, pad, size - pad * 2, size / 4);

                // Yellow gear / CD circle in center
                Gdiplus::SolidBrush cdBrush(Gdiplus::Color(255, 234, 179, 8));
                g.FillEllipse(&cdBrush, size / 2 - 6, size / 2 - 1, 12, 12);
                Gdiplus::SolidBrush cdHole(Gdiplus::Color(255, 255, 255, 255));
                g.FillEllipse(&cdHole, size / 2 - 2, size / 2 + 3, 4, 4);
            }
            else if (category == L"Music") {
                // Purple disc with musical note
                int pad = 4;
                Gdiplus::SolidBrush purpleBrush(Gdiplus::Color(255, 147, 51, 234));
                g.FillEllipse(&purpleBrush, pad, pad, size - pad * 2, size - pad * 2);

                // White note
                Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 255, 255, 255));
                g.FillEllipse(&whiteBrush, size / 2 - 6, size / 2 + 2, 6, 5);
                g.FillEllipse(&whiteBrush, size / 2 + 1, size / 2 - 1, 6, 5);
                Gdiplus::Pen notePen(Gdiplus::Color(255, 255, 255, 255), 2.0f);
                g.DrawLine(&notePen, size / 2 - 1, size / 2 + 4, size / 2 - 1, size / 2 - 6);
                g.DrawLine(&notePen, size / 2 + 6, size / 2 + 1, size / 2 + 6, size / 2 - 9);
                g.DrawLine(&notePen, size / 2 - 1, size / 2 - 6, size / 2 + 6, size / 2 - 9);
            }
            else if (category == L"PDF") {
                // Adobe Red Document with white "PDF" text or curved ribbon
                int pad = 5;
                Gdiplus::Point docPts[5] = {
                    Gdiplus::Point(pad, pad),
                    Gdiplus::Point(size - pad - 7, pad),
                    Gdiplus::Point(size - pad, pad + 7),
                    Gdiplus::Point(size - pad, size - pad),
                    Gdiplus::Point(pad, size - pad)
                };
                Gdiplus::SolidBrush pdfBrush(Gdiplus::Color(255, 220, 38, 38)); // Vibrant Crimson Red
                g.FillPolygon(&pdfBrush, docPts, 5);
                
                // Folded corner flap
                Gdiplus::Point flapPts[3] = {
                    Gdiplus::Point(size - pad - 7, pad),
                    Gdiplus::Point(size - pad, pad + 7),
                    Gdiplus::Point(size - pad - 7, pad + 7)
                };
                Gdiplus::SolidBrush flapBrush(Gdiplus::Color(255, 185, 28, 28));
                g.FillPolygon(&flapBrush, flapPts, 3);

                // Distinct white "PDF" badge rectangle on bottom half
                Gdiplus::SolidBrush badgeBrush(Gdiplus::Color(255, 255, 255, 255));
                int bH = (size >= 32) ? 12 : 6;
                int bY = size / 2 + 1;
                g.FillRectangle(&badgeBrush, pad + 3, bY, size - pad * 2 - 6, bH);

                // Inner red text / lines
                if (size >= 32) {
                    Gdiplus::FontFamily fontFamily(L"Arial");
                    Gdiplus::Font font(&fontFamily, (float)(size >= 40 ? 8.5f : 7.0f), Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
                    Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 220, 38, 38));
                    Gdiplus::RectF rectF((float)(pad + 3), (float)bY, (float)(size - pad * 2 - 6), (float)bH);
                    Gdiplus::StringFormat format;
                    format.SetAlignment(Gdiplus::StringAlignmentCenter);
                    format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                    g.DrawString(L"PDF", -1, &font, rectF, &format, &textBrush);
                } else {
                    Gdiplus::Pen redPen(Gdiplus::Color(255, 220, 38, 38), 1.0f);
                    g.DrawLine(&redPen, pad + 4, bY + bH / 2, size - pad - 4, bY + bH / 2);
                }
            }
            else if (category == L"DOCX" || category == L"Word" || category == L"DOC") {
                // Microsoft Word Blue Document with white "W" or "DOC" badge
                int pad = 5;
                Gdiplus::Point docPts[5] = {
                    Gdiplus::Point(pad, pad),
                    Gdiplus::Point(size - pad - 7, pad),
                    Gdiplus::Point(size - pad, pad + 7),
                    Gdiplus::Point(size - pad, size - pad),
                    Gdiplus::Point(pad, size - pad)
                };
                Gdiplus::SolidBrush docxBrush(Gdiplus::Color(255, 30, 64, 175)); // Microsoft Word Cobalt Blue (RGB: 30, 64, 175)
                g.FillPolygon(&docxBrush, docPts, 5);
                
                // Folded corner flap
                Gdiplus::Point flapPts[3] = {
                    Gdiplus::Point(size - pad - 7, pad),
                    Gdiplus::Point(size - pad, pad + 7),
                    Gdiplus::Point(size - pad - 7, pad + 7)
                };
                Gdiplus::SolidBrush flapBrush(Gdiplus::Color(255, 23, 37, 84));
                g.FillPolygon(&flapBrush, flapPts, 3);

                // Distinct white "W" / badge on bottom half
                Gdiplus::SolidBrush badgeBrush(Gdiplus::Color(255, 255, 255, 255));
                int bH = (size >= 32) ? 12 : 6;
                int bY = size / 2 + 1;
                g.FillRectangle(&badgeBrush, pad + 3, bY, size - pad * 2 - 6, bH);

                if (size >= 32) {
                    Gdiplus::FontFamily fontFamily(L"Arial");
                    Gdiplus::Font font(&fontFamily, (float)(size >= 40 ? 8.5f : 7.0f), Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
                    Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 30, 64, 175));
                    Gdiplus::RectF rectF((float)(pad + 3), (float)bY, (float)(size - pad * 2 - 6), (float)bH);
                    Gdiplus::StringFormat format;
                    format.SetAlignment(Gdiplus::StringAlignmentCenter);
                    format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                    g.DrawString(L"DOC", -1, &font, rectF, &format, &textBrush);
                } else {
                    Gdiplus::Pen bluePen(Gdiplus::Color(255, 30, 64, 175), 1.0f);
                    g.DrawLine(&bluePen, pad + 4, bY + bH / 2, size - pad - 4, bY + bH / 2);
                }
            }
            else if (category == L"Documents") {
                // Clean document page with folded corner
                int pad = 5;
                Gdiplus::Point docPts[5] = {
                    Gdiplus::Point(pad, pad),
                    Gdiplus::Point(size - pad - 6, pad),
                    Gdiplus::Point(size - pad, pad + 6),
                    Gdiplus::Point(size - pad, size - pad),
                    Gdiplus::Point(pad, size - pad)
                };
                Gdiplus::SolidBrush docBrush(Gdiplus::Color(255, 241, 245, 249));
                g.FillPolygon(&docBrush, docPts, 5);
                Gdiplus::Pen docPen(Gdiplus::Color(255, 71, 85, 105), 1.5f);
                g.DrawPolygon(&docPen, docPts, 5);

                // Text lines inside
                Gdiplus::Pen linePen(Gdiplus::Color(255, 148, 163, 184), 1.5f);
                g.DrawLine(&linePen, pad + 4, pad + 10, size - pad - 4, pad + 10);
                g.DrawLine(&linePen, pad + 4, pad + 16, size - pad - 4, pad + 16);
                g.DrawLine(&linePen, pad + 4, pad + 22, size - pad - 8, pad + 22);
            }
            else {
                // General Blue Globe
                int pad = 4;
                Gdiplus::SolidBrush globeBrush(Gdiplus::Color(255, 2, 132, 199));
                g.FillEllipse(&globeBrush, pad, pad, size - pad * 2, size - pad * 2);
                Gdiplus::Pen latPen(Gdiplus::Color(255, 255, 255, 255), 1.5f);
                g.DrawEllipse(&latPen, size / 2 - (size - pad * 2) / 4, pad, (size - pad * 2) / 2, size - pad * 2);
                g.DrawLine(&latPen, pad, size / 2, size - pad, size / 2);
            }
        }

        HICON hIcon = nullptr;
        if (bitmap.GetHICON(&hIcon) == Gdiplus::Ok) {
            return hIcon;
        }
        return nullptr;
    }

    static HICON CreateCompleteIcon(int size) {
        Gdiplus::Bitmap bitmap(size, size, PixelFormat32bppARGB);
        {
            Gdiplus::Graphics g(&bitmap);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            g.Clear(Gdiplus::Color(0, 0, 0, 0));

            // Golden open envelope
            int pad = 4;
            int envW = size - pad * 2;
            int envH = size - pad * 2 - 4;
            int envY = pad + 6;

            // Envelope body (Golden Yellow)
            Gdiplus::SolidBrush goldBrush(Gdiplus::Color(255, 245, 158, 11));
            g.FillRectangle(&goldBrush, pad, envY, envW, envH);

            // Sparkling blue diamond emerging from envelope
            int dW = size / 2;
            int dH = size / 2 - 2;
            int dX = size / 2 - dW / 2;
            int dY = pad + 1;

            Gdiplus::Point diamondPts[4] = {
                Gdiplus::Point(dX + dW / 2, dY),
                Gdiplus::Point(dX + dW, dY + dH / 2),
                Gdiplus::Point(dX + dW / 2, dY + dH),
                Gdiplus::Point(dX, dY + dH / 2)
            };
            Gdiplus::SolidBrush diamondBrush(Gdiplus::Color(255, 56, 189, 248));
            g.FillPolygon(&diamondBrush, diamondPts, 4);

            Gdiplus::Pen diamondBorder(Gdiplus::Color(255, 255, 255, 255), 1.5f);
            g.DrawPolygon(&diamondBorder, diamondPts, 4);

            // Open envelope flaps
            Gdiplus::Pen foldPen(Gdiplus::Color(255, 217, 119, 6), 1.5f);
            g.DrawLine(&foldPen, pad, envY, size / 2, envY + envH / 2);
            g.DrawLine(&foldPen, size - pad, envY, size / 2, envY + envH / 2);
            g.DrawLine(&foldPen, pad, envY + envH, size / 2, envY + envH / 2);
            g.DrawLine(&foldPen, size - pad, envY + envH, size / 2, envY + envH / 2);
        }

        HICON hIcon = nullptr;
        if (bitmap.GetHICON(&hIcon) == Gdiplus::Ok) {
            return hIcon;
        }
        return nullptr;
    }

private:
    static inline ULONG_PTR m_gdiplusToken = 0;
};
