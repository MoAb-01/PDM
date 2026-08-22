// Native Messaging Host for Google Chrome, Microsoft Edge, and Mozilla Firefox
// When a stream or URL is passed from the browser extension, it immediately launches/pops up DownloadManagerAB.exe with the Download File Info dialog!

#include <iostream>
#include <fstream>
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

// Robust JSON string extractor handling spaces, quotes, and escape sequences
std::string ExtractJsonStringField(const std::string& json, const std::string& key) {
    if (json.empty() || key.empty()) return "";

    std::string needle = "\"" + key + "\"";
    size_t pos = 0;
    while ((pos = json.find(needle, pos)) != std::string::npos) {
        size_t afterKey = pos + needle.length();
        // Skip whitespace
        while (afterKey < json.length() && (json[afterKey] == ' ' || json[afterKey] == '\t' || json[afterKey] == '\r' || json[afterKey] == '\n')) {
            afterKey++;
        }
        if (afterKey < json.length() && json[afterKey] == ':') {
            afterKey++; // skip ':'
            // Skip whitespace
            while (afterKey < json.length() && (json[afterKey] == ' ' || json[afterKey] == '\t' || json[afterKey] == '\r' || json[afterKey] == '\n')) {
                afterKey++;
            }
            if (afterKey < json.length() && json[afterKey] == '"') {
                afterKey++; // skip opening quote
                std::string result;
                bool escaped = false;
                while (afterKey < json.length()) {
                    char c = json[afterKey++];
                    if (escaped) {
                        if (c == '"') result += '"';
                        else if (c == '\\') result += '\\';
                        else if (c == '/') result += '/';
                        else if (c == 'b') result += '\b';
                        else if (c == 'f') result += '\f';
                        else if (c == 'n') result += '\n';
                        else if (c == 'r') result += '\r';
                        else if (c == 't') result += '\t';
                        else if (c == 'u' && afterKey + 3 < json.length()) {
                            std::string hexStr = json.substr(afterKey, 4);
                            afterKey += 4;
                            try {
                                unsigned int codepoint = std::stoul(hexStr, nullptr, 16);
                                if (codepoint < 0x80) {
                                    result += (char)codepoint;
                                } else if (codepoint < 0x800) {
                                    result += (char)(0xC0 | (codepoint >> 6));
                                    result += (char)(0x80 | (codepoint & 0x3F));
                                } else {
                                    result += (char)(0xE0 | (codepoint >> 12));
                                    result += (char)(0x80 | ((codepoint >> 6) & 0x3F));
                                    result += (char)(0x80 | (codepoint & 0x3F));
                                }
                            } catch (...) {
                                result += "\\u" + hexStr;
                            }
                        } else {
                            result += '\\';
                            result += c;
                        }
                        escaped = false;
                    } else if (c == '\\') {
                        escaped = true;
                    } else if (c == '"') {
                        return result;
                    } else {
                        result += c;
                    }
                }
                return result;
            } else {
                size_t endVal = afterKey;
                while (endVal < json.length() && json[endVal] != ',' && json[endVal] != '}' && json[endVal] != '\r' && json[endVal] != '\n') {
                    endVal++;
                }
                std::string rawVal = json.substr(afterKey, endVal - afterKey);
                while (!rawVal.empty() && (rawVal.front() == ' ' || rawVal.front() == '\t')) rawVal.erase(rawVal.begin());
                while (!rawVal.empty() && (rawVal.back() == ' ' || rawVal.back() == '\t')) rawVal.pop_back();
                return rawVal;
            }
        }
        pos += needle.length();
    }
    return "";
}

int main(int argc, char* argv[]) {
    SetBinaryMode();

    while (true) {
        std::string incoming = ReadMessage();
        if (incoming.empty()) break;

        // Extract media URL and details
        std::string url = ExtractJsonStringField(incoming, "url");
        if (url.empty()) url = ExtractJsonStringField(incoming, "link");
        std::string filename = ExtractJsonStringField(incoming, "filename");
        std::string mime = ExtractJsonStringField(incoming, "mimeType");
        std::string referer = ExtractJsonStringField(incoming, "referer");
        if (referer.empty()) referer = ExtractJsonStringField(incoming, "referrer");
        std::string cookies = ExtractJsonStringField(incoming, "cookies");
        std::string userAgent = ExtractJsonStringField(incoming, "userAgent");

        if (!url.empty()) {
            std::wstring wUrl = Utf8ToWide(url);
            std::wstring wFilename = Utf8ToWide(filename);
            std::wstring wReferer = Utf8ToWide(referer);
            std::wstring wCookies = Utf8ToWide(cookies);
            std::wstring wUserAgent = Utf8ToWide(userAgent);

            {
                CreateDirectoryW(L"C:\\temp", NULL);
                std::wofstream dbg(L"C:\\temp\\dm_debug.txt", std::ios::app);
                if (dbg.is_open()) {
                    dbg << L"=== [NATIVE HOST RECEIVED MESSAGE] ===" << std::endl;
                    dbg << L"Raw Message: " << Utf8ToWide(incoming) << std::endl;
                    dbg << L"Extracted URL: " << wUrl << std::endl;
                    dbg << L"Extracted Filename: " << wFilename << std::endl;
                    dbg << L"Extracted Referer: " << wReferer << std::endl;
                    dbg << L"Extracted Cookies: " << wCookies << std::endl;
                    dbg << L"Extracted UserAgent: " << wUserAgent << std::endl;
                    dbg << L"======================================" << std::endl << std::endl;
                    dbg.close();
                }
            }

            // Check if DownloadManagerAB.exe is already running
            HWND hWndMain = FindWindowW(L"IDM_Native_MainWindowClass", NULL);
            if (hWndMain && IsWindow(hWndMain)) {
                // Main app is running: Send WM_COPYDATA for instant zero-latency popup
                std::wstring dataStr = wUrl + L"\t" + wFilename + L"\t" + wReferer + L"\t" + wCookies + L"\t" + wUserAgent;
                COPYDATASTRUCT cds = { 0 };
                cds.dwData = 1001;
                cds.cbData = (DWORD)(dataStr.length() * sizeof(wchar_t));
                cds.lpData = (PVOID)dataStr.data();

                SendMessageW(hWndMain, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
                SetForegroundWindow(hWndMain);
            } else {
                // Main app is not running: Launch DownloadManagerAB.exe with CLI parameters
                wchar_t szModPath[MAX_PATH] = { 0 };
                GetModuleFileNameW(NULL, szModPath, MAX_PATH);
                std::wstring selfPath = szModPath;
                size_t lastSlash = selfPath.find_last_of(L"\\/");
                std::wstring exeDir = (lastSlash != std::wstring::npos) ? selfPath.substr(0, lastSlash + 1) : L"";
                std::wstring exePath = exeDir + L"DownloadManagerAB.exe";

                DWORD attr = GetFileAttributesW(exePath.c_str());
                if (attr == INVALID_FILE_ATTRIBUTES) {
                    exePath = L"d:\\Download Manager AB\\cpp\\build\\Release\\DownloadManagerAB.exe";
                }

                auto EscapeCliArg = [](const std::wstring& s) -> std::wstring {
                    std::wstring out;
                    for (wchar_t c : s) {
                        if (c == L'"') out += L"\\\"";
                        else out += c;
                    }
                    return out;
                };

                std::wstring args = L"--download-url \"" + EscapeCliArg(wUrl) + L"\"";
                if (!wFilename.empty()) args += L" --filename \"" + EscapeCliArg(wFilename) + L"\"";
                if (!wReferer.empty()) args += L" --referer \"" + EscapeCliArg(wReferer) + L"\"";
                if (!wCookies.empty()) args += L" --cookies \"" + EscapeCliArg(wCookies) + L"\"";
                if (!wUserAgent.empty()) args += L" --user-agent \"" + EscapeCliArg(wUserAgent) + L"\"";

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
