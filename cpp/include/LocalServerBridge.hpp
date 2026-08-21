#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

class LocalServerBridge {
public:
    using DownloadTriggerCallback = std::function<void(const std::wstring& url, const std::wstring& filename, const std::wstring& referer)>;

    LocalServerBridge(int port = 8989) : m_port(port) {}

    ~LocalServerBridge() {
        Stop();
    }

    void Start(DownloadTriggerCallback callback) {
        m_callback = callback;
        m_running = true;
        m_serverThread = std::thread(&LocalServerBridge::ServerWorker, this);
    }

    void Stop() {
        m_running = false;
        if (m_listenSocket != INVALID_SOCKET) {
            closesocket(m_listenSocket);
            m_listenSocket = INVALID_SOCKET;
        }
        if (m_serverThread.joinable()) {
            m_serverThread.detach();
        }
    }

private:
    int m_port = 8989;
    SOCKET m_listenSocket = INVALID_SOCKET;
    std::atomic<bool> m_running{ false };
    std::thread m_serverThread;
    DownloadTriggerCallback m_callback;

    std::wstring Utf8ToWide(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
        std::wstring result(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size);
        return result;
    }

    std::string ExtractField(const std::string& json, const std::string& key) {
        std::string search = "\"" + key + "\":\"";
        size_t pos = json.find(search);
        if (pos != std::string::npos) {
            pos += search.length();
            size_t endPos = json.find("\"", pos);
            if (endPos != std::string::npos) {
                return json.substr(pos, endPos - pos);
            }
        }
        return "";
    }

    void ServerWorker() {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);

        m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_listenSocket == INVALID_SOCKET) {
            WSACleanup();
            return;
        }

        // Enable address reuse
        int opt = 1;
        setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        sockaddr_in serverAddr = { 0 };
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        serverAddr.sin_port = htons((u_short)m_port);

        if (bind(m_listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(m_listenSocket);
            WSACleanup();
            return;
        }

        if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(m_listenSocket);
            WSACleanup();
            return;
        }

        while (m_running) {
            sockaddr_in clientAddr;
            int clientLen = sizeof(clientAddr);
            SOCKET clientSocket = accept(m_listenSocket, (sockaddr*)&clientAddr, &clientLen);

            if (clientSocket == INVALID_SOCKET) {
                if (!m_running) break;
                continue;
            }

            char buffer[8192] = { 0 };
            int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

            if (bytesRead > 0) {
                std::string request(buffer, bytesRead);

                // Handle CORS preflight OPTIONS request
                if (request.find("OPTIONS") == 0) {
                    std::string response =
                        "HTTP/1.1 200 OK\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
                        "Access-Control-Allow-Headers: Content-Type\r\n"
                        "Content-Length: 0\r\n\r\n";
                    send(clientSocket, response.c_str(), (int)response.length(), 0);
                }
                // Handle POST /download
                else {
                    size_t bodyPos = request.find("\r\n\r\n");
                    std::string body = (bodyPos != std::string::npos) ? request.substr(bodyPos + 4) : "";

                    std::string url = ExtractField(body, "url");
                    std::string filename = ExtractField(body, "filename");
                    std::string referer = ExtractField(body, "referer");

                    if (url.empty()) {
                        url = "https://streams.example.com/video.mp4";
                    }

                    if (m_callback) {
                        m_callback(Utf8ToWide(url), Utf8ToWide(filename), Utf8ToWide(referer));
                    }

                    std::string responseBody = "{\"status\":\"ok\",\"message\":\"Opened Download File Info in IDM C++\"}";
                    std::string response =
                        "HTTP/1.1 200 OK\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "Content-Type: application/json\r\n"
                        "Content-Length: " + std::to_string(responseBody.length()) + "\r\n\r\n" + responseBody;
                    send(clientSocket, response.c_str(), (int)response.length(), 0);
                }
            }

            closesocket(clientSocket);
        }

        WSACleanup();
    }
};
