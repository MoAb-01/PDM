#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <chrono>

enum class DownloadStatus {
    Queued,
    Connecting,
    Downloading,
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
    std::wstring savePath;
    std::wstring referer;
    std::wstring description;
    std::wstring login;
    std::wstring password;
    std::wstring mimeType;
    std::wstring lastTryDate;

    uint64_t sizeBytes = 0;
    uint64_t downloadedBytes = 0;
    uint64_t speedBytesPerSec = 0;
    int connections = 8;
    DownloadStatus status = DownloadStatus::Complete;
    bool resumeSupported = true;

    std::vector<DownloadChunk> chunks;
    std::wstring diagnosticText = L"Active: 0 | Stalled: 0 | Connecting: 0 | Avg Latency: 0 ms";
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
