#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include "../include/Models.hpp"
#include "../include/DownloadEngine.hpp"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "comctl32.lib")

// Shows the pure Win32 IDM Download Progress Dialog
void ShowDownloadProgressDialog(HWND hParent, DownloadItem* pItem, DownloadEngine* pEngine = nullptr);
