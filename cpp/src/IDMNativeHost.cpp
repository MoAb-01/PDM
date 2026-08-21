// Native Messaging Host for Google Chrome, Microsoft Edge, and Mozilla Firefox
// When a stream or URL is passed from the browser extension, it immediately launches/pops up DownloadManagerAB.exe with the Download File Info dialog!

#include <iostream>
#include <string>
#include <vector>
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#include <shellapi.h>

void SetBinaryMode() {
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
}

std::string ReadMessage() {
    uint32_t length = 0;
    if (std::cin.read(reinterpret_cast<char*>(&length), 4)) {
        if (length == 0 || length > 1024 * 1024 * 10) return "";
        std::vector<char> buffer(length);
        if (std::cin.read(buffer.data(), length)) {
            return std::string(buffer.data(), length);
        }
    }
    return "";
}

void SendMessageToBrowser(const std::string& jsonMessage) {
    uint32_t length = static_cast<uint32_t>(jsonMessage.length());
    std::cout.write(reinterpret_cast<const char*>(&length), 4);
    std::cout.write(jsonMessage.data(), length);
    std::cout.flush();
}

std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size);
    return result;
}

// Simple JSON string extractor
std::string ExtractJsonField(const std::string& json, const std::string& field) {
    std::string key = "\"" + field + "\":\"";
    size_t pos = json.find(key);
    if (pos != std::string::npos) {
        pos += key.length();
        size_t endPos = json.find("\"", pos);
        if (endPos != std::string::npos) {
            return json.substr(pos, endPos - pos);
        }
    }
    return "";
}

int main(int argc, char* argv[]) {
    SetBinaryMode();

    while (true) {
        std::string incoming = ReadMessage();
        if (incoming.empty()) break;

        // Extract media URL and details
        std::string url = ExtractJsonField(incoming, "url");
        std::string filename = ExtractJsonField(incoming, "filename");
        std::string mime = ExtractJsonField(incoming, "mimeType");
        std::string referer = ExtractJsonField(incoming, "referer");

        if (url.empty()) {
            url = ExtractJsonField(incoming, "link");
        }

        if (!url.empty()) {
            std::wstring wUrl = Utf8ToWide(url);
            std::wstring wFilename = Utf8ToWide(filename);
            std::wstring wReferer = Utf8ToWide(referer);

            // Check if DownloadManagerAB.exe is already running
            HWND hWndMain = FindWindowW(L"IDM_Native_MainWindowClass", NULL);
            if (hWndMain && IsWindow(hWndMain)) {
                // Main app is running: Send WM_COPYDATA for instant zero-latency popup
                std::wstring dataStr = wUrl + L"\t" + wFilename + L"\t" + wReferer;
                COPYDATASTRUCT cds = { 0 };
                cds.dwData = 1001;
                cds.cbData = (DWORD)((dataStr.length() + 1) * sizeof(wchar_t));
                cds.lpData = (PVOID)dataStr.c_str();

                SendMessageW(hWndMain, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
                SetForegroundWindow(hWndMain);
            } else {
                // Main app is not running: Launch DownloadManagerAB.exe with CLI parameters
                std::wstring exePath = L"d:\\Download Manager AB\\cpp\\build\\Release\\DownloadManagerAB.exe";
                std::wstring args = L"--download-url \"" + wUrl + L"\" --referer \"" + wReferer + L"\"";

                STARTUPINFOW si = { sizeof(STARTUPINFOW) };
                PROCESS_INFORMATION pi = { 0 };

                std::wstring cmdLine = L"\"" + exePath + L"\" " + args;
                std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
                cmdBuf.push_back(0);

                if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                }
            }
        }

        // Return status OK to browser extension
        std::string response = "{\"status\":\"ok\",\"message\":\"Opened IDM Download File Info dialog in C++ application\"}";
        SendMessageToBrowser(response);
    }

    return 0;
}
