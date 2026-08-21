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

private:
    static inline ULONG_PTR m_gdiplusToken = 0;
};
