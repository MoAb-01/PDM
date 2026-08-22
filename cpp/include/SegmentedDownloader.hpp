#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <vector>
#include <deque>
#include <thread>
#include <atomic>
#include <memory>
#include <chrono>
#include <string>
#include <cmath>
#include <fstream>
#include <functional>
#include <sstream>
#include <iomanip>
#include "Models.hpp"

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

#ifndef WINHTTP_ENABLE_COOKIES
#define WINHTTP_ENABLE_COOKIES 0x00000001
#endif

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

enum class SegmentStatus {
    Pending,
    Downloading,
    Completed,
    Failed
};

enum class StreamState : uint8_t {
    Idle = 0,
    Connecting,
    Receiving,
    WritingDisk,
    Stalled,
    Completed,
    Error
};

struct StreamTelemetry {
    int id = 0;
    StreamState state = StreamState::Idle;
    uint64_t lastPacketTimestampMs = 0;
    uint64_t start = 0;
    uint64_t end = 0;
    uint64_t downloadedBytes = 0;
    uint32_t latencyMs = 0;
};

struct SegmentState {
    int id = 0;
    uint64_t start = 0;
    uint64_t end = 0;
    std::atomic<uint64_t> downloadedBytes{ 0 };
    SegmentStatus status = SegmentStatus::Pending;
    std::atomic<StreamState> state{ StreamState::Idle };
    std::atomic<uint64_t> lastPacketTimestampMs{ 0 };
    std::atomic<uint32_t> latencyMs{ 0 };

    SegmentState() = default;
    SegmentState(const SegmentState& other) {
        id = other.id;
        start = other.start;
        end = other.end;
        downloadedBytes.store(other.downloadedBytes.load(std::memory_order_relaxed), std::memory_order_relaxed);
        status = other.status;
        state.store(other.state.load(std::memory_order_relaxed), std::memory_order_relaxed);
        lastPacketTimestampMs.store(other.lastPacketTimestampMs.load(std::memory_order_relaxed), std::memory_order_relaxed);
        latencyMs.store(other.latencyMs.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
    SegmentState& operator=(const SegmentState& other) {
        if (this != &other) {
            id = other.id;
            start = other.start;
            end = other.end;
            downloadedBytes.store(other.downloadedBytes.load(std::memory_order_relaxed), std::memory_order_relaxed);
            status = other.status;
            state.store(other.state.load(std::memory_order_relaxed), std::memory_order_relaxed);
            lastPacketTimestampMs.store(other.lastPacketTimestampMs.load(std::memory_order_relaxed), std::memory_order_relaxed);
            latencyMs.store(other.latencyMs.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        return *this;
    }
};

struct DownloadStats {
    uint64_t totalSize = 0;
    uint64_t downloadedBytes = 0;
    double progressPercent = 0.0;
    uint64_t speedBps = 0;
    uint64_t smoothedSpeedBps = 0;
    int64_t etaSeconds = -1;
    std::string etaFormatted;
    std::string speedFormatted;
    std::vector<SegmentState> segments;
    std::vector<StreamTelemetry> streams;
    std::wstring diagnosticText;
    bool isComplete = false;
    bool isPaused = false;
    bool isFailed = false;
    std::string errorMessage;
};

struct ParsedHttpUrl {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTP_PORT;
    bool isHttps = false;
    bool valid = false;
};

inline void EnsureFolderExistsForFile(const std::wstring& filePath) {
    wchar_t folder[MAX_PATH] = { 0 };
    wcsncpy_s(folder, filePath.c_str(), MAX_PATH - 1);
    PathRemoveFileSpecW(folder);
    if (wcslen(folder) > 0) {
        SHCreateDirectoryExW(NULL, folder, NULL);
    }
}

static ParsedHttpUrl CrackHttpUrl(const std::wstring& url) {
    ParsedHttpUrl res;
    URL_COMPONENTS urlComp = { 0 };
    urlComp.dwStructSize = sizeof(urlComp);
    
    wchar_t hostName[512] = { 0 };
    wchar_t urlPath[4096] = { 0 };
    
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 512;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 4096;
    
    if (WinHttpCrackUrl(url.c_str(), (DWORD)url.length(), 0, &urlComp)) {
        res.host = std::wstring(urlComp.lpszHostName, urlComp.dwHostNameLength);
        res.path = std::wstring(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
        
        if (res.path.empty()) {
            res.path = L"/";
        }

        res.port = urlComp.nPort;
        res.isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
        res.valid = true;
    }
    return res;
}

class SegmentedDownloader {
public:
    using StatsCallback = std::function<void(const DownloadStats& stats)>;
    static constexpr int MAX_STREAMS = 16;
    static constexpr const wchar_t* USER_AGENT = L"Wget/1.21.4 (win32) DownloadManagerAB/2.0";

    SegmentedDownloader() = default;
    ~SegmentedDownloader() {
        stop();
    }

    bool probeUrl(const std::wstring& url, uint64_t& outSize, bool& outSupportsRange, std::wstring& outFileName) {
        ParsedHttpUrl pUrl = CrackHttpUrl(url);
        if (!pUrl.valid) return false;

        HINTERNET hSession = WinHttpOpen(
            USER_AGENT,
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );
        if (!hSession) return false;

        DWORD cookiePolicy = WINHTTP_ENABLE_COOKIES;
        WinHttpSetOption(hSession, WINHTTP_OPTION_ENABLE_FEATURE, &cookiePolicy, sizeof(cookiePolicy));
        DWORD maxConns = 64;
        WinHttpSetOption(hSession, WINHTTP_OPTION_MAX_CONNS_PER_SERVER, &maxConns, sizeof(maxConns));
        WinHttpSetOption(hSession, WINHTTP_OPTION_MAX_CONNS_PER_1_0_SERVER, &maxConns, sizeof(maxConns));
        WinHttpSetTimeouts(hSession, 10000, 10000, 10000, 15000);

        HINTERNET hConn = WinHttpConnect(hSession, pUrl.host.c_str(), pUrl.port, 0);
        if (!hConn) {
            WinHttpCloseHandle(hSession);
            return false;
        }

        DWORD flags = pUrl.isHttps ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hReq = WinHttpOpenRequest(hConn, L"GET", pUrl.path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hReq) {
            WinHttpCloseHandle(hConn);
            WinHttpCloseHandle(hSession);
            return false;
        }

        DWORD redir = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(hReq, WINHTTP_OPTION_REDIRECT_POLICY, &redir, sizeof(redir));

        DWORD noCache = 1;
        WinHttpSetOption(hReq, WINHTTP_OPTION_DISABLE_FEATURE, &noCache, sizeof(noCache));

        std::wstring rHdr = L"Range: bytes=0-0\r\n";
        WinHttpAddRequestHeaders(hReq, rHdr.c_str(), (DWORD)rHdr.length(), WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

        if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(hReq, NULL)) {

            // Resolve final redirected URL
            DWORD finalUrlSize = 0;
            WinHttpQueryOption(hReq, WINHTTP_OPTION_URL, NULL, &finalUrlSize);
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && finalUrlSize > 0) {
                std::vector<wchar_t> finalUrl(finalUrlSize / sizeof(wchar_t) + 1);
                if (WinHttpQueryOption(hReq, WINHTTP_OPTION_URL, finalUrl.data(), &finalUrlSize)) {
                    std::wstring resolved = finalUrl.data();
                    if (!resolved.empty()) {
                        m_url = resolved;
                        m_pUrl = CrackHttpUrl(resolved);
                    }
                }
            }

            DWORD sc = 0, scSz = sizeof(sc);
            WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &sc, &scSz, WINHTTP_NO_HEADER_INDEX);

            if (sc == 206) {
                outSupportsRange = true;
                wchar_t cr[256] = { 0 };
                DWORD crSz = sizeof(cr);
                if (WinHttpQueryHeaders(hReq, WINHTTP_QUERY_CONTENT_RANGE, WINHTTP_HEADER_NAME_BY_INDEX, cr, &crSz, WINHTTP_NO_HEADER_INDEX)) {
                    std::wstring crStr = cr;
                    size_t slash = crStr.find(L'/');
                    if (slash != std::wstring::npos && slash + 1 < crStr.length()) {
                        try { outSize = std::stoull(crStr.substr(slash + 1)); } catch (...) {}
                    }
                }
            } else if (sc == 200) {
                DWORD cl = 0, clSz = sizeof(cl);
                if (WinHttpQueryHeaders(hReq, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &cl, &clSz, WINHTTP_NO_HEADER_INDEX)) {
                    outSize = cl;
                }
                wchar_t ar[64] = { 0 };
                DWORD arSz = sizeof(ar);
                if (WinHttpQueryHeaders(hReq, WINHTTP_QUERY_CUSTOM, L"Accept-Ranges", ar, &arSz, WINHTTP_NO_HEADER_INDEX)) {
                    if (std::wstring(ar).find(L"bytes") != std::wstring::npos) {
                        outSupportsRange = true;
                    }
                }
            }

            // Drain small body
            DWORD avail = 0;
            WinHttpQueryDataAvailable(hReq, &avail);
            if (avail > 0) {
                std::vector<BYTE> trash(avail);
                DWORD got = 0;
                WinHttpReadData(hReq, trash.data(), avail, &got);
            }
        }

        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hSession);
        return true;
    }

    bool start(const std::wstring& url, const std::wstring& outputPath, int numSegments = 16, StatsCallback callback = nullptr) {
        stop();

        m_url = url;
        m_outputPath = outputPath;
        m_numSegments = (numSegments > 0) ? ((numSegments <= MAX_STREAMS) ? numSegments : MAX_STREAMS) : 16;
        m_callback = callback;
        m_running = true;
        m_paused = false;
        m_completed = false;
        m_failed = false;
        m_totalDownloadedBytes.store(0, std::memory_order_relaxed);

        m_smoothedSpeed = 0.0;
        m_lastReportedEta = -1;
        m_speedDropStartTime = std::chrono::steady_clock::time_point::min();
        m_lastEtaUpdateTime = std::chrono::steady_clock::now();
        m_samples.clear();

        m_pUrl = CrackHttpUrl(url);

        // Ensure directory exists
        EnsureFolderExistsForFile(m_outputPath);

        // 1. Probe Server
        uint64_t totalSize = 0;
        bool supportsRange = false;
        std::wstring fn;
        probeUrl(url, totalSize, supportsRange, fn);

        m_totalSize = totalSize;
        m_supportsRange = supportsRange;

        if (!m_supportsRange || m_totalSize == 0) {
            m_numSegments = 1;
        }

        // 2. Pre-allocate destination file if total size is known
        if (m_totalSize > 0) {
            HANDLE hPreFile = CreateFileW(
                m_outputPath.c_str(),
                GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );
            if (hPreFile != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER li;
                li.QuadPart = (LONGLONG)m_totalSize;
                SetFilePointerEx(hPreFile, li, NULL, FILE_BEGIN);
                SetEndOfFile(hPreFile);
                CloseHandle(hPreFile);
            }
        } else {
            // Truncate / create empty file
            HANDLE hEmpty = CreateFileW(m_outputPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hEmpty != INVALID_HANDLE_VALUE) CloseHandle(hEmpty);
        }

        // 3. Initialize Global Session
        if (m_pUrl.valid) {
            m_hSession = WinHttpOpen(
                USER_AGENT,
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0
            );
            if (m_hSession) {
                DWORD cookiePolicy = WINHTTP_ENABLE_COOKIES;
                WinHttpSetOption(m_hSession, WINHTTP_OPTION_ENABLE_FEATURE, &cookiePolicy, sizeof(cookiePolicy));

                DWORD maxConns = 64;
                WinHttpSetOption(m_hSession, WINHTTP_OPTION_MAX_CONNS_PER_SERVER, &maxConns, sizeof(maxConns));
                WinHttpSetOption(m_hSession, WINHTTP_OPTION_MAX_CONNS_PER_1_0_SERVER, &maxConns, sizeof(maxConns));
                WinHttpSetTimeouts(m_hSession, 10000, 10000, 10000, 15000);
            }
        }

        // 4. Slice Ranges and Initialize Lock-Free Atomic Slots
        uint64_t chunkSize = (m_supportsRange && m_totalSize > 0) 
            ? ((m_totalSize + m_numSegments - 1) / m_numSegments) 
            : m_totalSize;

        for (int i = 0; i < MAX_STREAMS; ++i) {
            if (i < m_numSegments) {
                uint64_t sByte = i * chunkSize;
                uint64_t eByte = (i == m_numSegments - 1) ? (m_totalSize > 0 ? m_totalSize - 1 : 0) : ((i + 1) * chunkSize - 1);
                if (m_totalSize > 0 && sByte >= m_totalSize) {
                    sByte = m_totalSize;
                    eByte = m_totalSize;
                }

                (*m_liveSlots)[i].startByte.store(sByte, std::memory_order_relaxed);
                (*m_liveSlots)[i].endByte.store(eByte, std::memory_order_relaxed);
                (*m_liveSlots)[i].downloadedBytes.store(0, std::memory_order_relaxed);
                (*m_liveSlots)[i].lastPacketTime.store(0, std::memory_order_relaxed);
                (*m_liveSlots)[i].latencyMs.store(0, std::memory_order_relaxed);
                (*m_liveSlots)[i].active.store(true, std::memory_order_relaxed);
                (*m_liveSlots)[i].completed.store(false, std::memory_order_relaxed);
                (*m_liveSlots)[i].state.store(ChunkState::Connecting, std::memory_order_relaxed);

                m_threads.emplace_back(&SegmentedDownloader::worker, this, i);
            } else {
                (*m_liveSlots)[i].startByte.store(0, std::memory_order_relaxed);
                (*m_liveSlots)[i].endByte.store(0, std::memory_order_relaxed);
                (*m_liveSlots)[i].downloadedBytes.store(0, std::memory_order_relaxed);
                (*m_liveSlots)[i].lastPacketTime.store(0, std::memory_order_relaxed);
                (*m_liveSlots)[i].latencyMs.store(0, std::memory_order_relaxed);
                (*m_liveSlots)[i].active.store(false, std::memory_order_relaxed);
                (*m_liveSlots)[i].completed.store(false, std::memory_order_relaxed);
                (*m_liveSlots)[i].state.store(ChunkState::Idle, std::memory_order_relaxed);
            }
        }

        // 5. Spawn Monitor & Speed Calculator Thread
        m_monitorThread = std::thread(&SegmentedDownloader::monitorWorker, this);
        return true;
    }

    void stop() {
        m_running = false;
        for (auto& t : m_threads) {
            if (t.joinable()) t.join();
        }
        m_threads.clear();

        if (m_monitorThread.joinable()) {
            m_monitorThread.join();
        }

        if (m_hSession) {
            WinHttpCloseHandle(m_hSession);
            m_hSession = NULL;
        }
    }

    void pause() {
        m_paused = true;
        m_running = false;
        savePartialMetadata();
        stop();
    }

    void resume() {
        if (!loadPartialMetadata()) {
            start(m_url, m_outputPath, m_numSegments, m_callback);
            return;
        }

        m_running = true;
        m_paused = false;

        if (m_pUrl.valid) {
            m_hSession = WinHttpOpen(
                USER_AGENT,
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0
            );
            if (m_hSession) {
                DWORD cookiePolicy = WINHTTP_ENABLE_COOKIES;
                WinHttpSetOption(m_hSession, WINHTTP_OPTION_ENABLE_FEATURE, &cookiePolicy, sizeof(cookiePolicy));
                DWORD maxConns = 64;
                WinHttpSetOption(m_hSession, WINHTTP_OPTION_MAX_CONNS_PER_SERVER, &maxConns, sizeof(maxConns));
                WinHttpSetOption(m_hSession, WINHTTP_OPTION_MAX_CONNS_PER_1_0_SERVER, &maxConns, sizeof(maxConns));
                WinHttpSetTimeouts(m_hSession, 10000, 10000, 10000, 15000);
            }
        }

        for (int i = 0; i < m_numSegments; ++i) {
            if (!(*m_liveSlots)[i].completed.load(std::memory_order_relaxed)) {
                (*m_liveSlots)[i].active.store(true, std::memory_order_relaxed);
                (*m_liveSlots)[i].state.store(ChunkState::Connecting, std::memory_order_relaxed);
                m_threads.emplace_back(&SegmentedDownloader::worker, this, i);
            }
        }
        m_monitorThread = std::thread(&SegmentedDownloader::monitorWorker, this);
    }

    void cancel() {
        stop();
    }

    bool isRunning() const {
        return m_running.load(std::memory_order_relaxed);
    }

    bool isComplete() const {
        return m_completed.load(std::memory_order_relaxed);
    }

    std::shared_ptr<std::array<LiveStreamSlot, 16>> getLiveSlots() {
        return m_liveSlots;
    }

    DownloadStats getStats() {
        DownloadStats stats;
        stats.totalSize = m_totalSize;
        stats.downloadedBytes = m_totalDownloadedBytes.load(std::memory_order_relaxed);

        int activeCount = 0;
        int stalledCount = 0;
        int connectingCount = 0;
        int completedCount = 0;
        uint64_t totalLatency = 0;
        int latencySamples = 0;

        for (int i = 0; i < MAX_STREAMS; ++i) {
            const auto& slot = (*m_liveSlots)[i];
            StreamTelemetry st;
            st.id = i + 1;
            st.state = (StreamState)slot.state.load(std::memory_order_relaxed);
            st.start = slot.startByte.load(std::memory_order_relaxed);
            st.end = slot.endByte.load(std::memory_order_relaxed);
            st.downloadedBytes = slot.downloadedBytes.load(std::memory_order_relaxed);
            st.latencyMs = slot.latencyMs.load(std::memory_order_relaxed);
            stats.streams.push_back(st);

            if (st.state == StreamState::Receiving || st.state == StreamState::WritingDisk) activeCount++;
            else if (st.state == StreamState::Stalled) stalledCount++;
            else if (st.state == StreamState::Connecting) connectingCount++;
            else if (st.state == StreamState::Completed) completedCount++;

            if (st.latencyMs > 0) {
                totalLatency += st.latencyMs;
                latencySamples++;
            }
        }

        uint32_t avgLat = (latencySamples > 0) ? (uint32_t)(totalLatency / latencySamples) : 18;
        std::wstringstream ssDiag;
        ssDiag << L"Active: " << activeCount << L" | Stalled: " << stalledCount
               << L" | Connecting: " << connectingCount << L" | Avg Latency: " << avgLat << L" ms";
        stats.diagnosticText = ssDiag.str();

        if (m_totalSize > 0) {
            stats.progressPercent = ((double)stats.downloadedBytes / (double)m_totalSize) * 100.0;
            if (stats.progressPercent > 100.0) stats.progressPercent = 100.0;
        } else {
            stats.progressPercent = 0.0;
        }

        stats.speedBps = m_instantSpeed;
        stats.smoothedSpeedBps = (uint64_t)m_smoothedSpeed;

        if (stats.smoothedSpeedBps > 0 && m_totalSize > stats.downloadedBytes) {
            uint64_t rem = m_totalSize - stats.downloadedBytes;
            stats.etaSeconds = computeDampedEta(rem, (double)stats.smoothedSpeedBps);
            stats.etaFormatted = formatEta(stats.etaSeconds);
        } else if (stats.isComplete) {
            stats.etaSeconds = 0;
            stats.etaFormatted = "Complete";
        } else {
            stats.etaSeconds = computeDampedEta(m_totalSize > stats.downloadedBytes ? (m_totalSize - stats.downloadedBytes) : 0, (double)stats.smoothedSpeedBps);
            stats.etaFormatted = formatEta(stats.etaSeconds);
        }

        stats.speedFormatted = formatSpeedWithHysteresis((double)stats.smoothedSpeedBps);
        stats.isComplete = m_completed.load(std::memory_order_relaxed);
        stats.isPaused = m_paused.load(std::memory_order_relaxed);
        stats.isFailed = m_failed.load(std::memory_order_relaxed);
        stats.errorMessage = m_errorMessage;

        return stats;
    }

private:
    std::wstring m_url;
    std::wstring m_outputPath;
    int m_numSegments = 16;
    StatsCallback m_callback = nullptr;

    uint64_t m_totalSize = 0;
    bool m_supportsRange = false;
    std::atomic<bool> m_running{ false };
    std::atomic<bool> m_paused{ false };
    std::atomic<bool> m_completed{ false };
    std::atomic<bool> m_failed{ false };
    std::string m_errorMessage;

    std::atomic<uint64_t> m_totalDownloadedBytes{ 0 };

    std::shared_ptr<std::array<LiveStreamSlot, 16>> m_liveSlots = std::make_shared<std::array<LiveStreamSlot, 16>>();
    HINTERNET m_hSession = NULL;
    std::vector<std::thread> m_threads;
    std::thread m_monitorThread;

    struct SpeedSample {
        std::chrono::steady_clock::time_point time;
        uint64_t bytes = 0;
    };
    std::deque<SpeedSample> m_samples;
    uint64_t m_instantSpeed = 0;
    double m_smoothedSpeed = 0.0;

    int64_t m_lastReportedEta = -1;
    std::chrono::steady_clock::time_point m_speedDropStartTime = std::chrono::steady_clock::time_point::min();
    std::chrono::steady_clock::time_point m_lastEtaUpdateTime = std::chrono::steady_clock::now();

    ParsedHttpUrl m_pUrl;

    void worker(int id) {
        if (!m_hSession || !m_pUrl.valid) {
            (*m_liveSlots)[id].state.store(ChunkState::Error, std::memory_order_relaxed);
            return;
        }

        HINTERNET hConnect = WinHttpConnect(m_hSession, m_pUrl.host.c_str(), m_pUrl.port, 0);
        if (!hConnect) {
            (*m_liveSlots)[id].state.store(ChunkState::Error, std::memory_order_relaxed);
            return;
        }

        DWORD flags = m_pUrl.isHttps ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hReq = WinHttpOpenRequest(hConnect, L"GET", m_pUrl.path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hReq) {
            WinHttpCloseHandle(hConnect);
            (*m_liveSlots)[id].state.store(ChunkState::Error, std::memory_order_relaxed);
            return;
        }

        uint64_t initialStart = (*m_liveSlots)[id].startByte.load(std::memory_order_relaxed);
        uint64_t alreadyDownloaded = (*m_liveSlots)[id].downloadedBytes.load(std::memory_order_relaxed);
        uint64_t start = initialStart + alreadyDownloaded;
        uint64_t end = (*m_liveSlots)[id].endByte.load(std::memory_order_relaxed);

        if (start > end && end > 0) {
            (*m_liveSlots)[id].state.store(ChunkState::Completed, std::memory_order_relaxed);
            (*m_liveSlots)[id].completed.store(true, std::memory_order_relaxed);
            (*m_liveSlots)[id].active.store(false, std::memory_order_relaxed);
            WinHttpCloseHandle(hReq);
            WinHttpCloseHandle(hConnect);
            return;
        }

        if (m_supportsRange && end > 0) {
            std::wstring range = L"Range: bytes=" + std::to_wstring(start) + L"-" + std::to_wstring(end) + L"\r\n";
            WinHttpAddRequestHeaders(hReq, range.c_str(), (DWORD)range.length(), WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
        }

        DWORD redirectOption = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(hReq, WINHTTP_OPTION_REDIRECT_POLICY, &redirectOption, sizeof(redirectOption));

        DWORD disableCache = 1;
        WinHttpSetOption(hReq, WINHTTP_OPTION_DISABLE_FEATURE, &disableCache, sizeof(disableCache));

        auto t0 = std::chrono::steady_clock::now();
        if (!WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(hReq, NULL)) {
            (*m_liveSlots)[id].state.store(ChunkState::Error, std::memory_order_relaxed);
            WinHttpCloseHandle(hReq);
            WinHttpCloseHandle(hConnect);
            return;
        }

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
        if (statusCode != 200 && statusCode != 206) {
            (*m_liveSlots)[id].state.store(ChunkState::Error, std::memory_order_relaxed);
            WinHttpCloseHandle(hReq);
            WinHttpCloseHandle(hConnect);
            return;
        }

        auto t1 = std::chrono::steady_clock::now();
        uint32_t lat = (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        if (lat == 0) lat = 1;
        (*m_liveSlots)[id].latencyMs.store(lat, std::memory_order_relaxed);

        // Open independent local file handle for multi-threaded overlapped writing
        HANDLE hLocalFile = CreateFileW(
            m_outputPath.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        constexpr DWORD BUF_SIZE = 65536; // 64KB high throughput buffer
        std::vector<BYTE> buf(BUF_SIZE);
        DWORD bytesRead = 0;
        uint64_t currentOffset = start;

        (*m_liveSlots)[id].state.store(ChunkState::Receiving, std::memory_order_relaxed);

        while (m_running && WinHttpReadData(hReq, buf.data(), BUF_SIZE, &bytesRead) && bytesRead > 0) {
            if (hLocalFile != INVALID_HANDLE_VALUE) {
                OVERLAPPED ov = { 0 };
                ov.Offset = (DWORD)(currentOffset & 0xFFFFFFFF);
                ov.OffsetHigh = (DWORD)(currentOffset >> 32);
                DWORD written = 0;
                WriteFile(hLocalFile, buf.data(), bytesRead, &written, &ov);
            }

            currentOffset += bytesRead;
            (*m_liveSlots)[id].downloadedBytes.fetch_add(bytesRead, std::memory_order_relaxed);
            m_totalDownloadedBytes.fetch_add(bytesRead, std::memory_order_relaxed);
            (*m_liveSlots)[id].lastPacketTime.store(GetTickCount64(), std::memory_order_relaxed);
        }

        if (hLocalFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hLocalFile);
        }

        bool complete = (end > 0) ? (currentOffset > end) : (currentOffset > 0);
        if (complete) {
            (*m_liveSlots)[id].state.store(ChunkState::Completed, std::memory_order_relaxed);
            (*m_liveSlots)[id].completed.store(true, std::memory_order_relaxed);
            (*m_liveSlots)[id].active.store(false, std::memory_order_relaxed);
        } else {
            (*m_liveSlots)[id].state.store(ChunkState::Error, std::memory_order_relaxed);
        }

        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConnect);
    }

    void monitorWorker() {
        uint64_t prevDownloaded = m_totalDownloadedBytes.load(std::memory_order_relaxed);
        auto lastSampleTime = std::chrono::steady_clock::now();

        while (m_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            uint64_t totalDownloaded = m_totalDownloadedBytes.load(std::memory_order_relaxed);
            auto now = std::chrono::steady_clock::now();
            auto dtMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSampleTime).count();
            if (dtMs <= 0) dtMs = 1;

            uint64_t deltaBytes = (totalDownloaded >= prevDownloaded) ? (totalDownloaded - prevDownloaded) : 0;
            prevDownloaded = totalDownloaded;
            lastSampleTime = now;

            if (deltaBytes == 0) {
                m_instantSpeed = 0;
                m_smoothedSpeed *= 0.70;
                if (m_smoothedSpeed < 10.0) m_smoothedSpeed = 0.0;
            } else {
                m_instantSpeed = (deltaBytes * 1000) / dtMs;
                if (m_smoothedSpeed == 0.0) {
                    m_smoothedSpeed = (double)m_instantSpeed;
                } else {
                    m_smoothedSpeed = 0.25 * (double)m_instantSpeed + 0.75 * m_smoothedSpeed;
                }
            }

            uint64_t nowMs = GetTickCount64();
            bool allComplete = true;
            bool anyFailed = false;

            for (int i = 0; i < m_numSegments; ++i) {
                auto st = (*m_liveSlots)[i].state.load(std::memory_order_relaxed);
                auto lp = (*m_liveSlots)[i].lastPacketTime.load(std::memory_order_relaxed);

                if (st == ChunkState::Receiving && lp > 0 && (nowMs - lp > 5000)) {
                    (*m_liveSlots)[i].state.store(ChunkState::Stalled, std::memory_order_relaxed);
                }

                if (st == ChunkState::Error) anyFailed = true;
                if (!(*m_liveSlots)[i].completed.load(std::memory_order_relaxed)) {
                    allComplete = false;
                }
            }

            if (m_callback) {
                m_callback(getStats());
            }

            if (allComplete && (m_totalSize == 0 || totalDownloaded >= m_totalSize)) {
                m_completed.store(true, std::memory_order_relaxed);
                m_running.store(false, std::memory_order_relaxed);
                deletePartialMetadata();
                if (m_callback) m_callback(getStats());
                break;
            }
        }
    }

    int64_t computeDampedEta(uint64_t bytesRemaining, double smoothedSpeedBps) {
        auto now = std::chrono::steady_clock::now();

        if (smoothedSpeedBps < 1024.0) {
            if (m_speedDropStartTime == std::chrono::steady_clock::time_point::min()) {
                m_speedDropStartTime = now;
            }
            auto dropSecs = std::chrono::duration_cast<std::chrono::seconds>(now - m_speedDropStartTime).count();
            if (dropSecs < 3 && m_lastReportedEta > 0) {
                auto elapsedSecs = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastEtaUpdateTime).count();
                return (m_lastReportedEta > elapsedSecs) ? (m_lastReportedEta - elapsedSecs) : 0;
            }
            return -1;
        }

        m_speedDropStartTime = std::chrono::steady_clock::time_point::min();
        double rawEta = (double)bytesRemaining / smoothedSpeedBps;
        int64_t targetEta = (int64_t)std::ceil(rawEta);

        if (m_lastReportedEta < 0) {
            m_lastReportedEta = targetEta;
            m_lastEtaUpdateTime = now;
            return targetEta;
        }

        int64_t diff = targetEta - m_lastReportedEta;
        if (diff > 2) targetEta = m_lastReportedEta + 2;
        else if (diff < -2) targetEta = m_lastReportedEta - 2;

        m_lastReportedEta = targetEta;
        m_lastEtaUpdateTime = now;
        return targetEta;
    }

    std::string formatSpeedWithHysteresis(double currentBps) {
        if (currentBps <= 0.0) return "0.00 KB/s";

        const double HYST = 0.05;
        enum UnitType { UNIT_BS, UNIT_KBS, UNIT_MBS, UNIT_GBS };
        static UnitType currentUnit = UNIT_KBS;

        UnitType wantedUnit;
        if      (currentBps >= 1e9) wantedUnit = UNIT_GBS;
        else if (currentBps >= 1e6) wantedUnit = UNIT_MBS;
        else if (currentBps >= 1e3) wantedUnit = UNIT_KBS;
        else                        wantedUnit = UNIT_BS;

        if (wantedUnit > currentUnit) {
            double boundary = (wantedUnit == UNIT_MBS) ? 1e6 : (wantedUnit == UNIT_GBS) ? 1e9 : 1e3;
            if (currentBps > boundary * (1.0 + HYST)) currentUnit = wantedUnit;
        } else if (wantedUnit < currentUnit) {
            double boundary = (currentUnit == UNIT_MBS) ? 1e6 : (currentUnit == UNIT_GBS) ? 1e9 : 1e3;
            if (currentBps < boundary * (1.0 - HYST)) currentUnit = wantedUnit;
        }

        std::stringstream ss;
        switch (currentUnit) {
            case UNIT_GBS:
                ss << std::fixed << std::setprecision(2) << (currentBps / 1e9) << " GB/s";
                break;
            case UNIT_MBS:
                ss << std::fixed << std::setprecision(2) << (currentBps / 1e6) << " MB/s";
                break;
            case UNIT_KBS:
                ss << std::fixed << std::setprecision(2) << (currentBps / 1e3) << " KB/s";
                break;
            default:
                ss << std::fixed << std::setprecision(0) << currentBps << " B/s";
                break;
        }
        return ss.str();
    }

    void savePartialMetadata() {
        std::wstring metaPath = m_outputPath + L".partial";
        std::wofstream ofs(metaPath, std::ios::out | std::ios::trunc);
        if (!ofs.is_open()) return;

        ofs << m_url << L"\n";
        ofs << m_totalSize << L"\n";
        ofs << m_numSegments << L"\n";

        for (int i = 0; i < m_numSegments; ++i) {
            const auto& slot = (*m_liveSlots)[i];
            ofs << i << L" " 
                << slot.startByte.load(std::memory_order_relaxed) << L" " 
                << slot.endByte.load(std::memory_order_relaxed) << L" " 
                << slot.downloadedBytes.load(std::memory_order_relaxed) << L" " 
                << (int)slot.state.load(std::memory_order_relaxed) << L"\n";
        }
    }

    bool loadPartialMetadata() {
        std::wstring metaPath = m_outputPath + L".partial";
        std::wifstream ifs(metaPath);
        if (!ifs.is_open()) return false;

        std::wstring url;
        uint64_t totalSize = 0;
        int segCount = 0;

        if (std::getline(ifs, url) && (ifs >> totalSize) && (ifs >> segCount)) {
            m_url = url;
            m_totalSize = totalSize;
            m_numSegments = (segCount > 0 && segCount <= MAX_STREAMS) ? segCount : 16;
            m_totalDownloadedBytes.store(0, std::memory_order_relaxed);

            for (int i = 0; i < m_numSegments; ++i) {
                int idVal = 0;
                int st = 0;
                uint64_t sByte = 0, eByte = 0, dl = 0;
                ifs >> idVal >> sByte >> eByte >> dl >> st;

                (*m_liveSlots)[i].startByte.store(sByte, std::memory_order_relaxed);
                (*m_liveSlots)[i].endByte.store(eByte, std::memory_order_relaxed);
                (*m_liveSlots)[i].downloadedBytes.store(dl, std::memory_order_relaxed);
                (*m_liveSlots)[i].state.store((ChunkState)st, std::memory_order_relaxed);
                (*m_liveSlots)[i].completed.store(st == (int)ChunkState::Completed, std::memory_order_relaxed);
                (*m_liveSlots)[i].active.store(st != (int)ChunkState::Completed, std::memory_order_relaxed);

                m_totalDownloadedBytes.fetch_add(dl, std::memory_order_relaxed);
            }
            return true;
        }
        return false;
    }

    void deletePartialMetadata() {
        std::wstring metaPath = m_outputPath + L".partial";
        DeleteFileW(metaPath.c_str());
    }

    std::string formatEta(int64_t seconds) {
        if (seconds < 0) return "Stalled";
        if (seconds == 0) return "Almost done...";
        if (seconds > 99 * 3600 + 59 * 60 + 59) seconds = 99 * 3600 + 59 * 60 + 59;

        int hrs = (int)(seconds / 3600);
        int mins = (int)((seconds % 3600) / 60);
        int secs = (int)(seconds % 60);

        std::stringstream ss;
        if (hrs > 0) {
            ss << hrs << " hr " << mins << " min " << secs << " sec";
        } else if (mins > 0) {
            ss << mins << " min " << secs << " sec";
        } else {
            ss << secs << " sec";
        }
        return ss.str();
    }
};
