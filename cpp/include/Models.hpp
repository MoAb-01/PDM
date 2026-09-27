#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <array>
#include <atomic>
#include <memory>
#include <chrono>

enum class DownloadStatus {
    Queued,
    Connecting,
    Downloading,
    Merging,
    Paused,
    Complete,
    Error
};

enum class ChunkState {
    Idle,
    Connecting,
    Receiving,
    WritingDisk,
    Stalled,
    Completed,
    Error
};

struct alignas(64) LiveStreamSlot {
    std::atomic<uint64_t> downloadedBytes{ 0 };
    std::atomic<uint64_t> startByte{ 0 };
    std::atomic<uint64_t> endByte{ 0 };
    std::atomic<uint64_t> lastPacketTime{ 0 };
    std::atomic<uint32_t> latencyMs{ 0 };
    std::atomic<bool> active{ false };
    std::atomic<bool> completed{ false };
    std::atomic<ChunkState> state{ ChunkState::Idle };

    LiveStreamSlot() = default;
    LiveStreamSlot(const LiveStreamSlot& other) {
        downloadedBytes.store(other.downloadedBytes.load(std::memory_order_relaxed), std::memory_order_relaxed);
        startByte.store(other.startByte.load(std::memory_order_relaxed), std::memory_order_relaxed);
        endByte.store(other.endByte.load(std::memory_order_relaxed), std::memory_order_relaxed);
        lastPacketTime.store(other.lastPacketTime.load(std::memory_order_relaxed), std::memory_order_relaxed);
        latencyMs.store(other.latencyMs.load(std::memory_order_relaxed), std::memory_order_relaxed);
        active.store(other.active.load(std::memory_order_relaxed), std::memory_order_relaxed);
        completed.store(other.completed.load(std::memory_order_relaxed), std::memory_order_relaxed);
        state.store(other.state.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
    LiveStreamSlot& operator=(const LiveStreamSlot& other) {
        if (this != &other) {
            downloadedBytes.store(other.downloadedBytes.load(std::memory_order_relaxed), std::memory_order_relaxed);
            startByte.store(other.startByte.load(std::memory_order_relaxed), std::memory_order_relaxed);
            endByte.store(other.endByte.load(std::memory_order_relaxed), std::memory_order_relaxed);
            lastPacketTime.store(other.lastPacketTime.load(std::memory_order_relaxed), std::memory_order_relaxed);
            latencyMs.store(other.latencyMs.load(std::memory_order_relaxed), std::memory_order_relaxed);
            active.store(other.active.load(std::memory_order_relaxed), std::memory_order_relaxed);
            completed.store(other.completed.load(std::memory_order_relaxed), std::memory_order_relaxed);
            state.store(other.state.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        return *this;
    }
};

struct DownloadChunk {
    int id = 0;
    uint64_t startByte = 0;
    uint64_t endByte = 0;
    uint64_t downloadedBytes = 0;
    bool active = false;
    bool completed = false;
    ChunkState state = ChunkState::Idle;
    uint32_t latencyMs = 0;
};

struct DownloadItem {
    std::wstring id;
    std::wstring filename;
    std::wstring category; // General, Compressed, Documents, Music, Programs, Video
    std::wstring url;
    std::wstring originalPageUrl;
    std::wstring quality;
    std::wstring savePath;
    std::wstring videoTmpPath;
    std::wstring audioTmpPath;
    std::wstring referer;
    std::wstring description;
    std::wstring login;
    std::wstring password;
    std::wstring mimeType;
    std::wstring cookies;
    std::wstring userAgent;
    std::wstring lastTryDate;
    std::wstring queueName;

    uint64_t sizeBytes = 0;
    uint64_t downloadedBytes = 0;
    uint64_t speedBytesPerSec = 0;
    int connections = 8;
    DownloadStatus status = DownloadStatus::Downloading;
    bool resumeSupported = true;
    bool needsMux = false;

    std::vector<DownloadChunk> chunks;
    std::wstring diagnosticText = L"Active: 0 | Stalled: 0 | Connecting: 0 | Avg Latency: 0 ms";

    // Lock-Free 60 FPS UI Telemetry
    std::shared_ptr<std::array<LiveStreamSlot, 16>> liveSlots = std::make_shared<std::array<LiveStreamSlot, 16>>();
};

struct IDMSettings {
    bool launchOnStartup = true;
    bool clipboardAutoDetect = true;
    bool advancedBrowserIntegration = true;
    std::wstring autoCaptureExtensions = L"3GP 7Z AAC ACE AIF APK ARJ ASF AVI BIN BZ2 EXE GZ GZIP IMG ISO LZH M4A M4V MKV MOV MP3 MP4 MPA MPE MPEG MPG MSI MSU OGG OGV PDF PLJ PPS PPT QT R0* R1* RA RAR RM RMVB SEA SIT SITX TAR TIF TIFF WAV WMA WMV Z ZIP TS M3U8 MPD";
    std::wstring excludedSites = L"*.update.microsoft.com download.windowsupdate.com *.download.windowsupdate.com siteseal.thawte.com ecom.cimetz.com *.voice2page.com";
    std::wstring defaultDownloadDir = L"C:\\Users\\UHD\\Downloads\\";
    std::wstring tempDirectory = L"C:\\Users\\UHD\\AppData\\Roaming\\IDM\\";
    int maxConnections = 16;
    bool speedLimiterEnabled = false;
    int speedLimitKBps = 2048;
    std::wstring userAgent = L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36";
};

inline std::wstring DetectCategoryFromFilename(const std::wstring& filename) {
    if (filename == L"zip" || filename == L"rar" || filename == L"7z" || filename == L"tar" || filename == L"gz" || filename == L"bz2" || filename == L"iso") {
        return L"Compressed";
    }
    if (filename == L"mp4" || filename == L"mkv" || filename == L"avi" || filename == L"mov" || filename == L"webm" || filename == L"flv") {
        return L"Video";
    }
    if (filename == L"mp3" || filename == L"wav" || filename == L"flac" || filename == L"aac" || filename == L"m4a") {
        return L"Music";
    }
    if (filename == L"pdf" || filename == L"doc" || filename == L"docx" || filename == L"tex" || filename == L"txt") {
        return L"Documents";
    }
    if (filename == L"exe" || filename == L"msi" || filename == L"apk") {
        return L"Programs";
    }
    
    size_t dot = filename.find_last_of(L'.');
    if (dot != std::wstring::npos && dot + 1 < filename.length()) {
        std::wstring ext = filename.substr(dot + 1);
        for (auto& ch : ext) ch = towlower(ch);
        if (ext == L"zip" || ext == L"rar" || ext == L"7z" || ext == L"tar" || ext == L"gz" || ext == L"bz2" || ext == L"iso") return L"Compressed";
        if (ext == L"mp4" || ext == L"mkv" || ext == L"avi" || ext == L"mov" || ext == L"webm" || ext == L"flv") return L"Video";
        if (ext == L"mp3" || ext == L"wav" || ext == L"flac" || ext == L"aac" || ext == L"m4a") return L"Music";
        if (ext == L"pdf" || ext == L"doc" || ext == L"docx" || ext == L"tex" || ext == L"txt" || ext == L"pptx" || ext == L"xlsx") return L"Documents";
        if (ext == L"exe" || ext == L"msi" || ext == L"apk") return L"Programs";
    }
    return L"General";
}
