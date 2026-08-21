#include "../include/Models.hpp"
#include "../include/DownloadProgressDialog.hpp"
#include <windows.h>

void ShowChunkVisualizerDialog(HWND hParent, DownloadItem* pItem) {
    ShowDownloadProgressDialog(hParent, pItem, nullptr);
}
