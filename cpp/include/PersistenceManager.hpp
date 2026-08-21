#pragma once
#include <windows.h>
#include <shlwapi.h>
#include <string>

#pragma comment(lib, "shlwapi.lib")

// Returns path to "downloads.dat" located directly in the application's own folder (portable)
inline std::wstring GetAppDataStoragePath() {
    wchar_t szPath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    PathRemoveFileSpecW(szPath);
    return std::wstring(szPath) + L"\\downloads.dat";
}
