#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "winhttp.lib")

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

enum class StreamState {
    Idle,
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
    int64_t etaSeconds = -1; // -1 means stalled/unknown
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

static ParsedHttpUrl CrackHttpUrl(const std::wstring& url) {
    ParsedHttpUrl res;
    URL_COMPONENTS urlComp = { 0 };
    urlComp.dwStructSize = sizeof(urlComp);
    
    wchar_t hostName[512] = { 0 };
    wchar_t urlPath[2048] = { 0 };
    
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 512;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 2048;
    
    if (WinHttpCrackUrl(url.c_str(), (DWORD)url.length(), 0, &urlComp)) {
        res.host = hostName;
        res.path = urlPath;
        res.port = urlComp.nPort;
        res.isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
        res.valid = true;
    }
    return res;
}

class SegmentedDownloader {
public:
    using StatsCallback = std::function<void(const DownloadStats& stats)>;

    SegmentedDownloader() = default;
    ~SegmentedDownloader() {
        cancel();
    }

    bool probeUrl(const std::wstring& url, uint64_t& outSize, bool& outSupportsRange, std::wstring& outFileName) {
        ParsedHttpUrl pUrl = CrackHttpUrl(url);
        if (!pUrl.valid) return false;

        HINTERNET hSession = WinHttpOpen(
            L"AB Download Manager 1.0 (WinHTTP Engine)",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );
        if (!hSession) return false;

        DWORD maxConns = 64;
        WinHttpSetOption(hSession, WINHTTP_OPTION_MAX_CONNS_PER_SERVER, &maxConns, sizeof(maxConns));
        WinHttpSetOption(hSession, WINHTTP_OPTION_MAX_CONNS_PER_1_0_SERVER, &maxConns, sizeof(maxConns));
        WinHttpSetTimeouts(hSession, 5000, 5000, 5000, 8000);

        HINTERNET hConnect = WinHttpConnect(hSession, pUrl.host.c_str(), pUrl.port, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return false;
        }

        DWORD flags = pUrl.isHttps ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", pUrl.path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        std::wstring headers = L"Range: bytes=0-0\r\n";
        WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)headers.length(), WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

        DWORD redirectOption = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectOption, sizeof(redirectOption));

        DWORD disableCache = 1;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_DISABLE_FEATURE, &disableCache, sizeof(disableCache));

        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(hRequest, NULL)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

        outSupportsRange = (statusCode == 206);

        wchar_t contentRange[256] = { 0 };
        DWORD crSize = sizeof(contentRange);
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_RANGE, WINHTTP_HEADER_NAME_BY_INDEX, contentRange, &crSize, WINHTTP_NO_HEADER_INDEX)) {
            std::wstring crStr = contentRange;
            size_t slash = crStr.find(L'/');
            if (slash != std::wstring::npos && slash + 1 < crStr.length()) {
                try {
                    outSize = std::stoull(crStr.substr(slash + 1));
                    outSupportsRange = true;
                } catch (...) {}
            }
        }

        if (outSize == 0) {
            DWORD cl = 0;
            DWORD clSize = sizeof(cl);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &cl, &clSize, WINHTTP_NO_HEADER_INDEX)) {
                outSize = cl;
            }
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return true;
    }

    bool start(const std::wstring& url, const std::wstring& outputPath, int numSegments = 8, StatsCallback callback = nullptr) {
        cancel();

        m_url = url;
        m_outputPath = outputPath;
        m_numSegments = (numSegments > 0) ? numSegments : 8;
        m_callback = callback;
        m_running = true;
        m_paused = false;
        m_completed = false;
        m_totalDownloadedBytes.store(0, std::memory_order_relaxed);

        // Reset speed & ETA tracking
        m_smoothedSpeed = 0.0;
        m_lastReportedEta = -1;
        m_speedDropStartTime = std::chrono::steady_clock::time_point::min();
        m_lastEtaUpdateTime = std::chrono::steady_clock::now();
        m_samples.clear();

        m_pUrl = CrackHttpUrl(url);

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

        // Initialize WinHTTP Session & Connection for Worker Multiplexing
        if (m_pUrl.valid) {
            m_hSession = WinHttpOpen(
                L"AB Download Manager 1.0 (WinHTTP Engine)",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0
            );
            if (m_hSession) {
                DWORD maxConns = 64;
                WinHttpSetOption(m_hSession, WINHTTP_OPTION_MAX_CONNS_PER_SERVER, &maxConns, sizeof(maxConns));
                WinHttpSetOption(m_hSession, WINHTTP_OPTION_MAX_CONNS_PER_1_0_SERVER, &maxConns, sizeof(maxConns));
                WinHttpSetTimeouts(m_hSession, 5000, 5000, 5000, 8000);

                m_hConnect = WinHttpConnect(m_hSession, m_pUrl.host.c_str(), m_pUrl.port, 0);
            }
        }

        // 2. Pre-allocate disk file with high performance FileAllocationInfo
        if (!preallocateFile(m_outputPath, m_totalSize)) {
            HANDLE hFile = CreateFileW(m_outputPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
        }

        // 3. Segment Splitting
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_segments.clear();

            if (m_supportsRange && m_totalSize > 0) {
                uint64_t chunkSize = (uint64_t)std::ceil((double)m_totalSize / (double)m_numSegments);
                for (int i = 0; i < m_numSegments; ++i) {
                    SegmentState seg;
                    seg.id = i;
                    seg.start = i * chunkSize;
                    seg.end = (i == m_numSegments - 1) ? (m_totalSize - 1) : ((i + 1) * chunkSize - 1);
                    seg.downloadedBytes.store(0, std::memory_order_relaxed);
                    seg.status = SegmentStatus::Pending;
                    m_segments.push_back(seg);
                }
            } else {
                SegmentState seg;
                seg.id = 0;
                seg.start = 0;
                seg.end = (m_totalSize > 0) ? (m_totalSize - 1) : 0;
                seg.downloadedBytes.store(0, std::memory_order_relaxed);
                seg.status = SegmentStatus::Pending;
                m_segments.push_back(seg);
            }
        }

        // 4. Parallel Concurrent Worker Launch:
        // Launch all segment workers concurrently on their own dedicated threads in parallel
        {
            std::lock_guard<std::mutex> lock(m_threadsMutex);
            m_workerThreads.clear();
            for (size_t i = 0; i < m_segments.size(); ++i) {
                m_workerThreads.emplace_back(&SegmentedDownloader::segmentWorker, this, (int)i);
            }
        }

        // 5. Spawn Monitor & Speed Calculator Thread
        m_monitorThread = std::thread(&SegmentedDownloader::monitorWorker, this);

        return true;
    }

    void pause() {
        m_paused = true;
        m_running = false;
        savePartialMetadata();
        joinThreads();
    }

    void resume() {
        if (!loadPartialMetadata()) {
            start(m_url, m_outputPath, m_numSegments, m_callback);
            return;
        }

        m_running = true;
        m_paused = false;

        // Resume remaining incomplete segments
        for (size_t i = 0; i < m_segments.size(); ++i) {
            if (m_segments[i].status != SegmentStatus::Completed) {
                m_segments[i].status = SegmentStatus::Pending;
                m_workerThreads.emplace_back(&SegmentedDownloader::segmentWorker, this, (int)i);
            }
        }
        m_monitorThread = std::thread(&SegmentedDownloader::monitorWorker, this);
    }

    void cancel() {
        m_running = false;
        joinThreads();
    }

    bool isRunning() const {
        return m_running.load();
    }

    bool isComplete() const {
        return m_completed.load();
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

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            stats.segments = m_segments;
            for (const auto& s : m_segments) {
                StreamTelemetry st;
                st.id = s.id + 1;
                st.state = s.state.load(std::memory_order_relaxed);
                st.lastPacketTimestampMs = s.lastPacketTimestampMs.load(std::memory_order_relaxed);
                st.start = s.start;
                st.end = s.end;
                st.downloadedBytes = s.downloadedBytes.load(std::memory_order_relaxed);
                st.latencyMs = s.latencyMs.load(std::memory_order_relaxed);
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
        }

        uint32_t avgLat = (latencySamples > 0) ? (uint32_t)(totalLatency / latencySamples) : 24;
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

        // Step-Damped & Grace-Period ETA Calculation
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
        stats.isComplete = m_completed;
        stats.isPaused = m_paused;
        stats.isFailed = m_failed;
        stats.errorMessage = m_errorMessage;

        return stats;
    }

private:
    std::wstring m_url;
    std::wstring m_outputPath;
    int m_numSegments = 8;
    StatsCallback m_callback = nullptr;

    uint64_t m_totalSize = 0;
    bool m_supportsRange = false;
    std::atomic<bool> m_running{ false };
    std::atomic<bool> m_paused{ false };
    std::atomic<bool> m_completed{ false };
    std::atomic<bool> m_failed{ false };
    std::string m_errorMessage;

    std::atomic<uint64_t> m_totalDownloadedBytes{ 0 };

    std::mutex m_mutex;
    std::vector<SegmentState> m_segments;
    std::vector<std::thread> m_workerThreads;
    std::thread m_monitorThread;

    struct SpeedSample {
        std::chrono::steady_clock::time_point time;
        uint64_t bytes = 0;
    };
    std::deque<SpeedSample> m_samples;
    uint64_t m_instantSpeed = 0;
    double m_smoothedSpeed = 0.0;

    // ETA Damping state
    int64_t m_lastReportedEta = -1;
    std::chrono::steady_clock::time_point m_speedDropStartTime = std::chrono::steady_clock::time_point::min();
    std::chrono::steady_clock::time_point m_lastEtaUpdateTime = std::chrono::steady_clock::now();

    ParsedHttpUrl m_pUrl;
    HINTERNET m_hSession = NULL;
    HINTERNET m_hConnect = NULL;
    std::mutex m_threadsMutex;
    std::thread m_rampUpThread;

    void joinThreads() {
        if (m_rampUpThread.joinable()) {
            m_rampUpThread.join();
        }
        std::vector<std::thread> threadsToJoin;
        {
            std::lock_guard<std::mutex> lock(m_threadsMutex);
            threadsToJoin = std::move(m_workerThreads);
            m_workerThreads.clear();
        }
        for (auto& t : threadsToJoin) {
            if (t.joinable()) t.join();
        }
        if (m_monitorThread.joinable()) m_monitorThread.join();
        if (m_hConnect) {
            WinHttpCloseHandle(m_hConnect);
            m_hConnect = NULL;
        }
        if (m_hSession) {
            WinHttpCloseHandle(m_hSession);
            m_hSession = NULL;
        }
    }

    // High-performance pre-allocation using FileAllocationInfo
    bool preallocateFile(const std::wstring& path, uint64_t size) {
        if (size == 0) return false;
        HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;

        bool allocated = false;
        FILE_ALLOCATION_INFO fai;
        fai.AllocationSize.QuadPart = (LONGLONG)size;
        if (SetFileInformationByHandle(hFile, FileAllocationInfo, &fai, sizeof(fai))) {
            allocated = true;
        }

        // Set file pointer to end and commit
        LARGE_INTEGER li;
        li.QuadPart = (LONGLONG)size;
        if (SetFilePointerEx(hFile, li, NULL, FILE_BEGIN)) {
            SetEndOfFile(hFile);
            allocated = true;
        }

        CloseHandle(hFile);
        return allocated;
    }

    void segmentWorker(int segIndex) {
        int retries = 0;
        const int maxRetries = 3;

        while (m_running && retries <= maxRetries) {
            uint64_t startByte = 0;
            uint64_t endByte = 0;

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (segIndex >= (int)m_segments.size()) return;
                auto& seg = m_segments[segIndex];
                if (seg.status == SegmentStatus::Completed) return;

                startByte = seg.start + seg.downloadedBytes.load(std::memory_order_relaxed);
                endByte = seg.end;
                if (startByte > endByte && endByte > 0) {
                    seg.status = SegmentStatus::Completed;
                    seg.state.store(StreamState::Completed, std::memory_order_relaxed);
                    return;
                }
                seg.status = SegmentStatus::Downloading;
                seg.state.store(StreamState::Connecting, std::memory_order_relaxed);
            }

            if (!m_hConnect) {
                retries++;
                std::this_thread::sleep_for(std::chrono::milliseconds(300 * (1 << retries)));
                continue;
            }

            DWORD flags = m_pUrl.isHttps ? WINHTTP_FLAG_SECURE : 0;
            HINTERNET hRequest = WinHttpOpenRequest(
                m_hConnect,
                L"GET",
                m_pUrl.path.c_str(),
                NULL,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                flags
            );

            if (!hRequest) {
                retries++;
                std::this_thread::sleep_for(std::chrono::milliseconds(300 * (1 << retries)));
                continue;
            }

            std::wstring headers;
            if (m_supportsRange && endByte > 0) {
                headers = L"Range: bytes=" + std::to_wstring(startByte) + L"-" + std::to_wstring(endByte) + L"\r\n";
            }
            if (!headers.empty()) {
                WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)headers.length(), WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
            }

            DWORD redirectOption = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectOption, sizeof(redirectOption));

            DWORD disableCache = 1;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_DISABLE_FEATURE, &disableCache, sizeof(disableCache));

            auto reqStartTime = std::chrono::steady_clock::now();

            if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
                !WinHttpReceiveResponse(hRequest, NULL)) {
                WinHttpCloseHandle(hRequest);
                retries++;
                std::this_thread::sleep_for(std::chrono::milliseconds(300 * (1 << retries)));
                continue;
            }

            auto reqEndTime = std::chrono::steady_clock::now();
            uint32_t latMs = (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(reqEndTime - reqStartTime).count();
            if (latMs == 0) latMs = 1;

            if (segIndex < (int)m_segments.size()) {
                m_segments[segIndex].latencyMs.store(latMs, std::memory_order_relaxed);
            }

            // Independent file handle per worker thread — non-interfering I/O
            HANDLE hFile = CreateFileW(m_outputPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == INVALID_HANDLE_VALUE) {
                WinHttpCloseHandle(hRequest);
                return;
            }

            constexpr DWORD BUFFER_SIZE = 131072; // 128 KB aligned read/write buffer
            std::vector<BYTE> buffer(BUFFER_SIZE);
            DWORD bytesRead = 0;
            bool failed = false;

            // Lock-Free Inner Download Loop using WinHttpReadData
            while (m_running && WinHttpReadData(hRequest, buffer.data(), BUFFER_SIZE, &bytesRead) && bytesRead > 0) {
                uint64_t nowMs = GetTickCount64();

                if (segIndex < (int)m_segments.size()) {
                    m_segments[segIndex].state.store(StreamState::Receiving, std::memory_order_relaxed);
                    m_segments[segIndex].lastPacketTimestampMs.store(nowMs, std::memory_order_relaxed);
                }

                OVERLAPPED ov = { 0 };
                ov.Offset = (DWORD)(startByte & 0xFFFFFFFF);
                ov.OffsetHigh = (DWORD)(startByte >> 32);

                DWORD bytesWritten = 0;
                if (!WriteFile(hFile, buffer.data(), bytesRead, &bytesWritten, &ov)) {
                    failed = true;
                    break;
                }

                startByte += bytesWritten;

                // Lock-Free relaxed atomic counters (No mutex in hot loop!)
                m_segments[segIndex].downloadedBytes.fetch_add(bytesWritten, std::memory_order_relaxed);
                m_totalDownloadedBytes.fetch_add(bytesWritten, std::memory_order_relaxed);
            }

            CloseHandle(hFile);
            WinHttpCloseHandle(hRequest);

            // Strict Segment Completion Check:
            bool isTrulyComplete = false;
            if (m_supportsRange && endByte > 0) {
                isTrulyComplete = (startByte > endByte);
            } else if (m_totalSize > 0) {
                isTrulyComplete = (m_totalDownloadedBytes.load(std::memory_order_relaxed) >= m_totalSize);
            } else {
                isTrulyComplete = (startByte > 0);
            }

            if (isTrulyComplete) {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    if (segIndex < (int)m_segments.size()) {
                        m_segments[segIndex].status = SegmentStatus::Completed;
                        m_segments[segIndex].state.store(StreamState::Completed, std::memory_order_relaxed);
                    }
                }
                checkForSegmentStealing();
                return;
            } else {
                retries++;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    if (segIndex < (int)m_segments.size()) {
                        m_segments[segIndex].status = SegmentStatus::Pending;
                        m_segments[segIndex].state.store(StreamState::Error, std::memory_order_relaxed);
                    }
                }
            }

            if (retries <= maxRetries && m_running) {
                std::this_thread::sleep_for(std::chrono::seconds(1 << (retries - 1))); // Exponential backoff (1s, 2s, 4s)
            }
        }

        if (retries > maxRetries) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (segIndex < (int)m_segments.size()) {
                    m_segments[segIndex].status = SegmentStatus::Failed;
                    m_segments[segIndex].state.store(StreamState::Error, std::memory_order_relaxed);
                }
            }
            m_failed = true;
            m_paused = true;
            m_errorMessage = "Network connection lost. Click Resume to continue.";
            savePartialMetadata();
        }
    }

    // Dynamic Segment Stealing: Non-blocking scoped micro-lock and external thread spawn
    void checkForSegmentStealing() {
        if (!m_supportsRange || !m_running) return;

        int targetIdx = -1;
        uint64_t maxRemaining = 0;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (int i = 0; i < (int)m_segments.size(); ++i) {
                const auto& seg = m_segments[i];
                if (seg.status == SegmentStatus::Downloading && seg.end > seg.start) {
                    uint64_t totalSegSize = (seg.end - seg.start) + 1;
                    uint64_t curDownloaded = seg.downloadedBytes.load(std::memory_order_relaxed);
                    uint64_t rem = (totalSegSize > curDownloaded) ? (totalSegSize - curDownloaded) : 0;
                    if (rem > 1024 * 1024 && rem > maxRemaining) { // Split if at least 1MB left
                        maxRemaining = rem;
                        targetIdx = i;
                    }
                }
            }
        }

        if (targetIdx < 0) return;

        int newIdx = -1;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (targetIdx < (int)m_segments.size()) {
                auto& oldSeg = m_segments[targetIdx];
                if (oldSeg.status == SegmentStatus::Downloading) {
                    uint64_t curPos = oldSeg.start + oldSeg.downloadedBytes.load(std::memory_order_relaxed);
                    uint64_t rem = (oldSeg.end > curPos) ? (oldSeg.end - curPos) : 0;
                    if (rem > 1024 * 1024) {
                        uint64_t splitPoint = curPos + (rem / 2);
                        if (splitPoint < oldSeg.end) {
                            SegmentState newSeg;
                            newSeg.id = (int)m_segments.size();
                            newSeg.start = splitPoint;
                            newSeg.end = oldSeg.end;
                            newSeg.downloadedBytes.store(0, std::memory_order_relaxed);
                            newSeg.status = SegmentStatus::Pending;
                            newSeg.state.store(StreamState::Idle, std::memory_order_relaxed);

                            oldSeg.end = splitPoint - 1;

                            m_segments.push_back(newSeg);
                            newIdx = (int)m_segments.size() - 1;
                        }
                    }
                }
            }
        }

        // Spawn worker thread OUTSIDE m_mutex (using dedicated m_threadsMutex)
        if (newIdx >= 0) {
            std::lock_guard<std::mutex> lock(m_threadsMutex);
            m_workerThreads.emplace_back(&SegmentedDownloader::segmentWorker, this, newIdx);
        }
    }

    void monitorWorker() {
        uint64_t prevDownloaded = m_totalDownloadedBytes.load(std::memory_order_relaxed);
        auto lastSampleTime = std::chrono::steady_clock::now();

        while (m_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Decoupled 100ms speed sampler

            uint64_t totalDownloaded = m_totalDownloadedBytes.load(std::memory_order_relaxed);
            auto now = std::chrono::steady_clock::now();
            auto dtMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSampleTime).count();
            if (dtMs <= 0) dtMs = 1;

            uint64_t deltaBytes = (totalDownloaded >= prevDownloaded) ? (totalDownloaded - prevDownloaded) : 0;
            prevDownloaded = totalDownloaded;
            lastSampleTime = now;

            if (deltaBytes == 0) {
                m_instantSpeed = 0;
                // Graceful exponential decay towards 0 instead of freezing UI
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

            // Real-Time Stall Detection: check if any receiving stream hasn't received a packet in >1000ms
            uint64_t nowMs = GetTickCount64();

            bool allComplete = true;
            bool anyFailed = false;

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_segments.empty()) allComplete = false;
                for (auto& s : m_segments) {
                    StreamState st = s.state.load(std::memory_order_relaxed);
                    if (st == StreamState::Receiving || st == StreamState::WritingDisk) {
                        uint64_t lastPkt = s.lastPacketTimestampMs.load(std::memory_order_relaxed);
                        if (lastPkt > 0 && nowMs > lastPkt && (nowMs - lastPkt > 1000)) {
                            s.state.store(StreamState::Stalled, std::memory_order_relaxed);
                        }
                    }

                    if (s.status == SegmentStatus::Failed) {
                        anyFailed = true;
                    }
                    if (s.status != SegmentStatus::Completed) {
                        allComplete = false;
                    }
                }
            }

            // If network failure occurred, immediately halt and preserve exact partial bytes
            if (anyFailed) {
                m_failed = true;
                m_paused = true;
                m_running = false;
                savePartialMetadata();
                if (m_callback) m_callback(getStats());
                break;
            }

            if (m_callback) {
                m_callback(getStats());
            }

            // Only mark complete if ALL segments finished AND byte count matches total size
            if (allComplete && (m_totalSize == 0 || totalDownloaded >= m_totalSize)) {
                m_completed = true;
                m_running = false;
                deletePartialMetadata();
                if (m_callback) m_callback(getStats());
                break;
            }
        }
    }

    // Step-Damped & 3-second Grace-Period ETA Calculation
    int64_t computeDampedEta(uint64_t bytesRemaining, double smoothedSpeedBps) {
        auto now = std::chrono::steady_clock::now();

        if (smoothedSpeedBps < 1024.0) { // < 1 KB/s
            if (m_speedDropStartTime == std::chrono::steady_clock::time_point::min()) {
                m_speedDropStartTime = now;
            }
            auto dropSecs = std::chrono::duration_cast<std::chrono::seconds>(now - m_speedDropStartTime).count();
            if (dropSecs < 3 && m_lastReportedEta > 0) {
                // 3-second zero-speed dampening buffer: continue natural countdown
                auto elapsedSecs = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastEtaUpdateTime).count();
                return (m_lastReportedEta > elapsedSecs) ? (m_lastReportedEta - elapsedSecs) : 0;
            }
            return -1; // Stalled after 3 seconds
        }

        m_speedDropStartTime = std::chrono::steady_clock::time_point::min();
        double rawEta = (double)bytesRemaining / smoothedSpeedBps;
        int64_t targetEta = (int64_t)std::ceil(rawEta);

        if (m_lastReportedEta < 0) {
            m_lastReportedEta = targetEta;
            m_lastEtaUpdateTime = now;
            return targetEta;
        }

        // Step Damping: Clamps ETA jump to max +-2 seconds per frame
        int64_t diff = targetEta - m_lastReportedEta;
        if (diff > 2) targetEta = m_lastReportedEta + 2;
        else if (diff < -2) targetEta = m_lastReportedEta - 2;

        m_lastReportedEta = targetEta;
        m_lastEtaUpdateTime = now;
        return targetEta;
    }

    // Display unit hysteresis with 5% dead-band boundary
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
        ofs << m_segments.size() << L"\n";

        for (const auto& s : m_segments) {
            ofs << s.id << L" " << s.start << L" " << s.end << L" " << s.downloadedBytes.load(std::memory_order_relaxed) << L" " << (int)s.status << L"\n";
        }
    }

    bool loadPartialMetadata() {
        std::wstring metaPath = m_outputPath + L".partial";
        std::wifstream ifs(metaPath);
        if (!ifs.is_open()) return false;

        std::wstring url;
        uint64_t totalSize = 0;
        size_t segCount = 0;

        if (std::getline(ifs, url) && (ifs >> totalSize) && (ifs >> segCount)) {
            m_url = url;
            m_totalSize = totalSize;
            m_segments.clear();
            m_totalDownloadedBytes.store(0, std::memory_order_relaxed);

            for (size_t i = 0; i < segCount; ++i) {
                SegmentState s;
                int st = 0;
                uint64_t dl = 0;
                ifs >> s.id >> s.start >> s.end >> dl >> st;
                s.downloadedBytes.store(dl, std::memory_order_relaxed);
                s.status = (SegmentStatus)st;
                m_totalDownloadedBytes.fetch_add(dl, std::memory_order_relaxed);
                m_segments.push_back(s);
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
        if (seconds > 99 * 3600 + 59 * 60 + 59) seconds = 99 * 3600 + 59 * 60 + 59; // Cap at 99:59:59

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
