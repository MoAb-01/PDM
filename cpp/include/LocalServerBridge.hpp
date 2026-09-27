#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <atomic>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

class LocalServerBridge {
public:
  using DownloadTriggerCallback =
      std::function<void(const std::wstring &url, const std::wstring &filename,
                         const std::wstring &referer, const std::wstring &cookies,
                         const std::wstring &userAgent, const std::wstring &quality,
                         const std::wstring &originalPageUrl, uint64_t totalSize,
                         uint64_t videoSize, uint64_t audioSize)>;
  using ProbeCallback =
      std::function<std::string(const std::wstring &url)>;

  using StreamInitCallback = std::function<bool(
      const std::wstring &streamId, const std::wstring &url,
      const std::wstring &filename, uint64_t totalSize,
      const std::wstring &referer, const std::wstring &cookies,
      const std::wstring &userAgent)>;
  using StreamChunkCallback = std::function<void(
      const std::wstring &streamId, const char *data, size_t size)>;
  using StreamCompleteCallback = std::function<void(
      const std::wstring &streamId, bool success, const std::wstring &errorMsg)>;

  LocalServerBridge(int port = 9898) : m_port(port) {}

  ~LocalServerBridge() { Stop(); }

  void Start(DownloadTriggerCallback callback, ProbeCallback probeCallback = nullptr,
             StreamInitCallback streamInitCallback = nullptr,
             StreamChunkCallback streamChunkCallback = nullptr,
             StreamCompleteCallback streamCompleteCallback = nullptr) {
    m_callback = callback;
    m_probeCallback = probeCallback;
    m_streamInitCallback = streamInitCallback;
    m_streamChunkCallback = streamChunkCallback;
    m_streamCompleteCallback = streamCompleteCallback;
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

  static std::wstring Utf8ToWide(const std::string &str) {
    if (str.empty())
      return L"";
    int size =
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0],
                        size);
    return result;
  }

  static std::string ExtractJsonStringField(const std::string &json, const std::string &key) {
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
        size_t afterColon = afterKey + 1;
        while (afterColon < json.length() && (json[afterColon] == ' ' || json[afterColon] == '\t' || json[afterColon] == '\r' || json[afterColon] == '\n')) {
          afterColon++;
        }
        if (afterColon < json.length() && json[afterColon] == '"') {
          size_t valStart = afterColon + 1;
          std::string result;
          size_t i = valStart;
          while (i < json.length()) {
            if (json[i] == '\\' && i + 1 < json.length()) {
              char nextCh = json[i + 1];
              if (nextCh == '"') { result += '"'; i += 2; }
              else if (nextCh == '\\') { result += '\\'; i += 2; }
              else if (nextCh == '/') { result += '/'; i += 2; }
              else if (nextCh == 'n') { result += '\n'; i += 2; }
              else if (nextCh == 'r') { result += '\r'; i += 2; }
              else if (nextCh == 't') { result += '\t'; i += 2; }
              else if (nextCh == 'u' && i + 5 < json.length()) {
                std::string hexStr = json.substr(i + 2, 4);
                try {
                  unsigned int code = std::stoul(hexStr, nullptr, 16);
                  if (code < 128) {
                    result += (char)code;
                  } else {
                    wchar_t wch = (wchar_t)code;
                    int u8len = WideCharToMultiByte(CP_UTF8, 0, &wch, 1, NULL, 0, NULL, NULL);
                    if (u8len > 0) {
                      std::string u8(u8len, 0);
                      WideCharToMultiByte(CP_UTF8, 0, &wch, 1, &u8[0], u8len, NULL, NULL);
                      result += u8;
                    }
                  }
                } catch (...) {}
                i += 6;
              } else {
                result += json[i++];
              }
            } else if (json[i] == '"') {
              return result;
            } else {
              result += json[i++];
            }
          }
          return result;
        }
      }
      pos = afterKey;
    }
    return "";
  }

private:
  int m_port = 9898;
  std::atomic<bool> m_running{false};
  SOCKET m_listenSocket = INVALID_SOCKET;
  std::thread m_serverThread;
  DownloadTriggerCallback m_callback;
  ProbeCallback m_probeCallback;
  StreamInitCallback m_streamInitCallback;
  StreamChunkCallback m_streamChunkCallback;
  StreamCompleteCallback m_streamCompleteCallback;

  void ServerWorker() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET) {
      WSACleanup();
      return;
    }

    int opt = 1;
    setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons((u_short)m_port);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int bindRes = SOCKET_ERROR;
    for (int retry = 0; retry < 15 && bindRes == SOCKET_ERROR && m_running; ++retry) {
      bindRes = bind(m_listenSocket, (sockaddr *)&serverAddr, sizeof(serverAddr));
      if (bindRes == SOCKET_ERROR) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }

    if (bindRes == SOCKET_ERROR) {
      int err = WSAGetLastError();
      {
        CreateDirectoryW(L"C:\\temp", NULL);
        std::wofstream dbg(L"C:\\temp\\dm_debug.txt", std::ios::app);
        if (dbg.is_open()) {
          dbg << L"[BRIDGE ERROR] Failed to bind to 127.0.0.1:9898. Error: " << err << std::endl;
          dbg.close();
        }
      }
      closesocket(m_listenSocket);
      m_listenSocket = INVALID_SOCKET;
      WSACleanup();
      return;
    }

    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
      closesocket(m_listenSocket);
      m_listenSocket = INVALID_SOCKET;
      WSACleanup();
      return;
    }

    while (m_running) {
      sockaddr_in clientAddr;
      int clientLen = sizeof(clientAddr);
      SOCKET clientSocket = accept(m_listenSocket, (sockaddr *)&clientAddr, &clientLen);

      if (clientSocket == INVALID_SOCKET) {
        if (!m_running) break;
        continue;
      }

      DWORD timeoutMs = 3000;
      setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeoutMs, sizeof(timeoutMs));

      std::string fullRequest;
      char buffer[8192];
      size_t contentLength = 0;
      bool headersComplete = false;
      size_t bodyStart = std::string::npos;

      while (true) {
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) break;
        fullRequest.append(buffer, bytesRead);

        if (!headersComplete) {
          size_t hdrEnd = fullRequest.find("\r\n\r\n");
          if (hdrEnd != std::string::npos) {
            headersComplete = true;
            bodyStart = hdrEnd + 4;

            // Search for Content-Length case-insensitively
            std::string lowerHdr = fullRequest.substr(0, hdrEnd);
            for (char &c : lowerHdr) c = (char)tolower(c);
            size_t clPos = lowerHdr.find("content-length:");
            if (clPos != std::string::npos) {
              size_t valStart = clPos + 15;
              while (valStart < lowerHdr.length() && (lowerHdr[valStart] == ' ' || lowerHdr[valStart] == '\t')) valStart++;
              size_t valEnd = lowerHdr.find("\r\n", valStart);
              if (valEnd != std::string::npos) {
                try { contentLength = std::stoull(lowerHdr.substr(valStart, valEnd - valStart)); } catch (...) {}
              }
            }
          }
        }

        if (headersComplete) {
          size_t currentBodyLen = fullRequest.length() - bodyStart;
          if (fullRequest.rfind("GET", 0) == 0 || fullRequest.rfind("OPTIONS", 0) == 0) {
            break;
          }
          if (currentBodyLen >= contentLength) {
            break;
          }
        }
      }

      if (!fullRequest.empty()) {
        if (fullRequest.rfind("OPTIONS", 0) == 0) {
          std::string response =
              "HTTP/1.1 200 OK\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
              "Access-Control-Allow-Headers: *\r\n"
              "Access-Control-Allow-Private-Network: true\r\n"
              "Access-Control-Max-Age: 86400\r\n"
              "Content-Length: 0\r\n"
              "Connection: close\r\n\r\n";
          send(clientSocket, response.c_str(), (int)response.length(), 0);
        }
        else if (fullRequest.find("POST /probe") != std::string::npos) {
          std::string body = (bodyStart != std::string::npos) ? fullRequest.substr(bodyStart) : "";
          std::string url = ExtractJsonStringField(body, "url");
          std::string probeJson = "{\"status\":\"ok\",\"formats\":{}}";
          if (!url.empty() && m_probeCallback) {
            probeJson = m_probeCallback(Utf8ToWide(url));
          }

          std::string response = "HTTP/1.1 200 OK\r\n"
                                 "Access-Control-Allow-Origin: *\r\n"
                                 "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                                 "Access-Control-Allow-Headers: *\r\n"
                                 "Access-Control-Allow-Private-Network: true\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: " +
                                 std::to_string(probeJson.length()) +
                                 "\r\n"
                                 "Connection: close\r\n\r\n" + probeJson;
          send(clientSocket, response.c_str(), (int)response.length(), 0);
        }
        else if (fullRequest.find("POST /stream-init") != std::string::npos) {
          std::string body = (bodyStart != std::string::npos) ? fullRequest.substr(bodyStart) : "";
          std::string streamId = ExtractJsonStringField(body, "streamId");
          std::string url = ExtractJsonStringField(body, "url");
          std::string filename = ExtractJsonStringField(body, "filename");
          std::string referer = ExtractJsonStringField(body, "referer");
          std::string cookies = ExtractJsonStringField(body, "cookies");
          std::string userAgent = ExtractJsonStringField(body, "userAgent");

          uint64_t totalSize = 0;
          size_t tsPos = body.find("\"totalSize\":");
          if (tsPos != std::string::npos) {
            size_t numStart = tsPos + 12;
            while (numStart < body.length() && (body[numStart] == ' ' || body[numStart] == ':')) numStart++;
            try { totalSize = std::stoull(body.substr(numStart)); } catch (...) {}
          }

          bool ok = true;
          if (!streamId.empty() && m_streamInitCallback) {
            ok = m_streamInitCallback(Utf8ToWide(streamId), Utf8ToWide(url),
                                     Utf8ToWide(filename), totalSize,
                                     Utf8ToWide(referer), Utf8ToWide(cookies),
                                     Utf8ToWide(userAgent));
          }

          std::string responseBody = ok ? "{\"status\":\"ok\"}" : "{\"status\":\"error\",\"message\":\"Failed to init stream\"}";
          std::string response = "HTTP/1.1 200 OK\r\n"
                                 "Access-Control-Allow-Origin: *\r\n"
                                 "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                                 "Access-Control-Allow-Headers: *\r\n"
                                 "Access-Control-Allow-Private-Network: true\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: " + std::to_string(responseBody.length()) + "\r\n"
                                 "Connection: close\r\n\r\n" + responseBody;
          send(clientSocket, response.c_str(), (int)response.length(), 0);
        }
        else if (fullRequest.find("POST /stream-chunk") != std::string::npos) {
          std::string streamId;
          size_t sidParam = fullRequest.find("streamId=");
          if (sidParam != std::string::npos) {
            size_t sidEnd = fullRequest.find_first_of(" &\r\n", sidParam + 9);
            streamId = fullRequest.substr(sidParam + 9, sidEnd - (sidParam + 9));
          }
          if (streamId.empty()) {
            size_t sidHdr = fullRequest.find("X-Stream-Id:");
            if (sidHdr == std::string::npos) sidHdr = fullRequest.find("x-stream-id:");
            if (sidHdr != std::string::npos) {
              size_t valStart = sidHdr + 12;
              while (valStart < fullRequest.length() && fullRequest[valStart] == ' ') valStart++;
              size_t valEnd = fullRequest.find("\r\n", valStart);
              if (valEnd != std::string::npos) streamId = fullRequest.substr(valStart, valEnd - valStart);
            }
          }

          if (!streamId.empty() && m_streamChunkCallback && bodyStart != std::string::npos) {
            const char *chunkPtr = fullRequest.data() + bodyStart;
            size_t chunkSize = (contentLength > 0) ? contentLength : (fullRequest.length() - bodyStart);
            m_streamChunkCallback(Utf8ToWide(streamId), chunkPtr, chunkSize);
          }

          std::string responseBody = "{\"status\":\"ok\"}";
          std::string response = "HTTP/1.1 200 OK\r\n"
                                 "Access-Control-Allow-Origin: *\r\n"
                                 "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                                 "Access-Control-Allow-Headers: *\r\n"
                                 "Access-Control-Allow-Private-Network: true\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: " + std::to_string(responseBody.length()) + "\r\n"
                                 "Connection: close\r\n\r\n" + responseBody;
          send(clientSocket, response.c_str(), (int)response.length(), 0);
        }
        else if (fullRequest.find("POST /stream-complete") != std::string::npos) {
          std::string body = (bodyStart != std::string::npos) ? fullRequest.substr(bodyStart) : "";
          std::string streamId = ExtractJsonStringField(body, "streamId");
          std::string error = ExtractJsonStringField(body, "error");
          bool success = (body.find("\"success\":true") != std::string::npos || body.find("\"success\": true") != std::string::npos);

          if (!streamId.empty() && m_streamCompleteCallback) {
            m_streamCompleteCallback(Utf8ToWide(streamId), success, Utf8ToWide(error));
          }

          std::string responseBody = "{\"status\":\"ok\"}";
          std::string response = "HTTP/1.1 200 OK\r\n"
                                 "Access-Control-Allow-Origin: *\r\n"
                                 "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                                 "Access-Control-Allow-Headers: *\r\n"
                                 "Access-Control-Allow-Private-Network: true\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: " + std::to_string(responseBody.length()) + "\r\n"
                                 "Connection: close\r\n\r\n" + responseBody;
          send(clientSocket, response.c_str(), (int)response.length(), 0);
        }
        else {
          std::string body = (bodyStart != std::string::npos) ? fullRequest.substr(bodyStart) : "";

          std::string url = ExtractJsonStringField(body, "url");
          if (url.empty()) url = ExtractJsonStringField(body, "link");
          std::string filename = ExtractJsonStringField(body, "filename");
          std::string referer = ExtractJsonStringField(body, "referer");
          if (referer.empty()) referer = ExtractJsonStringField(body, "referrer");
          std::string cookies = ExtractJsonStringField(body, "cookies");
          std::string userAgent = ExtractJsonStringField(body, "userAgent");
          std::string quality = ExtractJsonStringField(body, "quality");
          std::string origPageUrl = ExtractJsonStringField(body, "originalPageUrl");

          uint64_t totalSize = 0;
          size_t tsPos = body.find("\"totalSize\":");
          if (tsPos == std::string::npos) tsPos = body.find("\"fileSize\":");
          if (tsPos != std::string::npos) {
            size_t numStart = tsPos + 11;
            while (numStart < body.length() && (body[numStart] == ' ' || body[numStart] == ':' || body[numStart] == '"')) numStart++;
            try { totalSize = std::stoull(body.substr(numStart)); } catch (...) {}
          }

          uint64_t videoSize = 0;
          size_t vsPos = body.find("\"videoSize\":");
          if (vsPos != std::string::npos) {
            size_t numStart = vsPos + 12;
            while (numStart < body.length() && (body[numStart] == ' ' || body[numStart] == ':' || body[numStart] == '"')) numStart++;
            try { videoSize = std::stoull(body.substr(numStart)); } catch (...) {}
          }

          uint64_t audioSize = 0;
          size_t asPos = body.find("\"audioSize\":");
          if (asPos != std::string::npos) {
            size_t numStart = asPos + 12;
            while (numStart < body.length() && (body[numStart] == ' ' || body[numStart] == ':' || body[numStart] == '"')) numStart++;
            try { audioSize = std::stoull(body.substr(numStart)); } catch (...) {}
          }

          {
            CreateDirectoryW(L"C:\\temp", NULL);
            std::wofstream dbg(L"C:\\temp\\dm_debug.txt", std::ios::app);
            if (dbg.is_open()) {
              dbg << L"=== [BRIDGE RECEIVED POST /download] ===" << std::endl;
              dbg << L"Raw Body: " << Utf8ToWide(body) << std::endl;
              dbg << L"Extracted URL: " << Utf8ToWide(url) << std::endl;
              dbg << L"Extracted Filename: " << Utf8ToWide(filename) << std::endl;
              dbg << L"Extracted TotalSize: " << totalSize << std::endl;
              dbg << L"Extracted VideoSize: " << videoSize << std::endl;
              dbg << L"Extracted AudioSize: " << audioSize << std::endl;
              dbg << L"Extracted Referer: " << Utf8ToWide(referer) << std::endl;
              dbg << L"Extracted Cookies: " << Utf8ToWide(cookies) << std::endl;
              dbg << L"Extracted UserAgent: " << Utf8ToWide(userAgent) << std::endl;
              dbg << L"Extracted Quality: " << Utf8ToWide(quality) << std::endl;
              dbg << L"========================================" << std::endl << std::endl;
              dbg.close();
            }
          }

          if (!url.empty() && m_callback) {
            m_callback(Utf8ToWide(url), Utf8ToWide(filename),
                       Utf8ToWide(referer), Utf8ToWide(cookies),
                       Utf8ToWide(userAgent), Utf8ToWide(quality),
                       Utf8ToWide(origPageUrl), totalSize, videoSize, audioSize);
          }

          std::string responseBody = "{\"status\":\"ok\",\"message\":\"Opened Download File Info in IDM C++\"}";
          std::string response = "HTTP/1.1 200 OK\r\n"
                                 "Access-Control-Allow-Origin: *\r\n"
                                 "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                                 "Access-Control-Allow-Headers: *\r\n"
                                 "Access-Control-Allow-Private-Network: true\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: " +
                                 std::to_string(responseBody.length()) +
                                 "\r\n"
                                 "Connection: close\r\n\r\n" + responseBody;
          send(clientSocket, response.c_str(), (int)response.length(), 0);
        }
      }

      closesocket(clientSocket);
    }

    WSACleanup();
  }
};
