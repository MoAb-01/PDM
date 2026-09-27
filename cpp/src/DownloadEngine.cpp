#include "../include/DownloadEngine.hpp"
#include "../include/SegmentedDownloader.hpp"
#include "../include/PersistenceManager.hpp"
#include <iostream>
#include <fstream>
#include <shlwapi.h>
#include <shlobj.h>
#include <sstream>
#include <regex>
#include <iomanip>

inline std::wstring GetDefaultDownloadsFolder() {
    wchar_t* pPath = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &pPath))) {
        std::wstring res = pPath;
        CoTaskMemFree(pPath);
        if (!res.empty() && res.back() != L'\\') res += L'\\';
        return res;
    }
    return L"C:\\Downloads\\";
}

// Checks if URL is a web streaming video platform (YouTube, Shorts, TikTok, Vimeo, etc.)
static bool IsStreamingPlatformUrl(const std::wstring& url) {
    if (url.find(L"youtube.com") != std::wstring::npos ||
        url.find(L"youtu.be") != std::wstring::npos ||
        url.find(L"vimeo.com") != std::wstring::npos ||
        url.find(L"tiktok.com") != std::wstring::npos ||
        url.find(L"instagram.com") != std::wstring::npos ||
        url.find(L"twitter.com") != std::wstring::npos ||
        url.find(L"x.com") != std::wstring::npos) {
        return true;
    }
    return false;
}

// Parse human unit string (e.g., 24.15MiB, 850KiB, 1.2GiB) into exact bytes
static uint64_t ParseSizeToBytes(double val, const std::string& unit) {
    std::string u = unit;
    for (auto& c : u) c = toupper(c);
    if (u.find("G") != std::string::npos) {
        return (uint64_t)(val * 1024.0 * 1024.0 * 1024.0);
    } else if (u.find("M") != std::string::npos) {
        return (uint64_t)(val * 1024.0 * 1024.0);
    } else if (u.find("K") != std::string::npos) {
        return (uint64_t)(val * 1024.0);
    }
    return (uint64_t)val;
}

DownloadEngine::DownloadEngine() {
    LoadHistory();
}

DownloadEngine::~DownloadEngine() {
    SaveHistory();
    m_running = false;
    StopAll();
    for (auto& t : m_activeThreads) {
        if (t.joinable()) {
            t.detach();
        }
    }
}

void DownloadEngine::SaveHistory() {
    std::wstring path = GetAppDataStoragePath();
    std::wofstream ofs(path, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& d : m_downloads) {
        // Only save completed or paused downloads
        if (d.status == DownloadStatus::Complete || d.status == DownloadStatus::Paused) {
            ofs << d.id << L"\t"
                << d.filename << L"\t"
                << d.category << L"\t"
                << d.sizeBytes << L"\t"
                << d.downloadedBytes << L"\t"
                << (int)d.status << L"\t"
                << d.savePath << L"\t"
                << d.url << L"\t"
                << d.referer << L"\t"
                << d.description << L"\t"
                << d.lastTryDate << L"\n";
        }
    }
}

void DownloadEngine::LoadHistory() {
    std::wstring path = GetAppDataStoragePath();
    std::wifstream ifs(path);
    if (!ifs.is_open()) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_downloads.clear();

    std::wstring line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        std::wstringstream ss(line);
        std::wstring id, filename, category, strSize, strDownloaded, strStatus, savePath, url, referer, description, lastTryDate;

        if (std::getline(ss, id, L'\t') &&
            std::getline(ss, filename, L'\t') &&
            std::getline(ss, category, L'\t') &&
            std::getline(ss, strSize, L'\t') &&
            std::getline(ss, strDownloaded, L'\t') &&
            std::getline(ss, strStatus, L'\t') &&
            std::getline(ss, savePath, L'\t') &&
            std::getline(ss, url, L'\t') &&
            std::getline(ss, referer, L'\t') &&
            std::getline(ss, description, L'\t') &&
            std::getline(ss, lastTryDate)) {

            DownloadItem item;
            item.id = id;
            item.filename = filename;
            item.category = category;
            try {
                item.sizeBytes = std::stoull(strSize);
                item.downloadedBytes = std::stoull(strDownloaded);
            } catch (...) {
                item.sizeBytes = 0;
                item.downloadedBytes = 0;
            }
            int st = 0;
            try { st = std::stoi(strStatus); } catch (...) {}
            item.status = (st == 2) ? DownloadStatus::Complete : DownloadStatus::Paused;
            item.savePath = savePath;
            item.url = url;
            item.referer = referer;
            item.description = description;
            item.lastTryDate = lastTryDate;
            item.connections = 16;
            m_downloads.push_back(item);
        }
    }
}

void DownloadEngine::SetCallbacks(ProgressCallback onProgress, StatusCallback onStatus) {
    m_onProgress = onProgress;
    m_onStatus = onStatus;
}

void DownloadEngine::AddItem(const DownloadItem& item) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_downloads.push_back(item);
    }
    SaveHistory();
}

std::vector<DownloadItem> DownloadEngine::GetDownloads() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_downloads;
}

DownloadItem DownloadEngine::GetItem(const std::wstring& id) {
    DownloadItem item;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& d : m_downloads) {
            if (d.id == id) {
                item = d;
                found = true;
                break;
            }
        }
    }
    if (!found) return DownloadItem{};

    std::shared_ptr<SegmentedDownloader> pDownloader = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_downloaderMutex);
        auto it = m_activeDownloaders.find(id);
        if (it != m_activeDownloaders.end() && !it->second.empty()) {
            pDownloader = it->second.front();
        }
    }

    if (pDownloader && pDownloader->isRunning()) {
        DownloadStats stats = pDownloader->getStats();
        item.downloadedBytes = stats.downloadedBytes;
        item.sizeBytes = stats.totalSize;
        item.speedBytesPerSec = stats.smoothedSpeedBps;
        item.diagnosticText = stats.diagnosticText;
        item.liveSlots = pDownloader->getLiveSlots();

        item.chunks.clear();
        for (const auto& st : stats.streams) {
            DownloadChunk c;
            c.id = st.id;
            c.startByte = st.start;
            c.endByte = st.end;
            c.downloadedBytes = st.downloadedBytes;
            c.active = (st.state == StreamState::Receiving || st.state == StreamState::Connecting || st.state == StreamState::WritingDisk);
            c.completed = (st.state == StreamState::Completed);
            c.state = (ChunkState)st.state;
            c.latencyMs = st.latencyMs;
            item.chunks.push_back(c);
        }
    }

    return item;
}

std::shared_ptr<std::array<LiveStreamSlot, 16>> DownloadEngine::GetLiveSlots(const std::wstring& id) {
    {
        std::lock_guard<std::mutex> lock(m_downloaderMutex);
        auto it = m_activeDownloaders.find(id);
        if (it != m_activeDownloaders.end() && !it->second.empty() && it->second.front()) {
            return it->second.front()->getLiveSlots();
        }
    }
    std::lock_guard<std::mutex> lockItem(m_mutex);
    for (const auto& d : m_downloads) {
        if (d.id == id) return d.liveSlots;
    }
    return nullptr;
}

void DownloadEngine::UpdateItem(const DownloadItem& item) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& d : m_downloads) {
            if (d.id == item.id) {
                d = item;
                break;
            }
        }
    }
    SaveHistory();
}

void DownloadEngine::CleanupDownloadDiskFiles(const DownloadItem& item, bool deleteIncompleteTarget) {
    if (!item.videoTmpPath.empty()) {
        DeleteFileW(item.videoTmpPath.c_str());
        std::wstring vMeta = item.videoTmpPath + L".partial";
        DeleteFileW(vMeta.c_str());
    }
    if (!item.audioTmpPath.empty()) {
        DeleteFileW(item.audioTmpPath.c_str());
        std::wstring aMeta = item.audioTmpPath + L".partial";
        DeleteFileW(aMeta.c_str());
    }
    if (!item.savePath.empty()) {
        std::wstring videoTmp = item.savePath + L".video_" + item.id + L".tmp";
        std::wstring audioTmp = item.savePath + L".audio_" + item.id + L".tmp";
        std::wstring rawAudioTmp = item.savePath + L".raw_audio_" + item.id + L".tmp";
        DeleteFileW(videoTmp.c_str());
        DeleteFileW(audioTmp.c_str());
        DeleteFileW(rawAudioTmp.c_str());

        std::wstring vMeta = videoTmp + L".partial";
        std::wstring aMeta = audioTmp + L".partial";
        std::wstring rMeta = rawAudioTmp + L".partial";
        DeleteFileW(vMeta.c_str());
        DeleteFileW(aMeta.c_str());
        DeleteFileW(rMeta.c_str());

        std::wstring metaPath = item.savePath + L".partial";
        DeleteFileW(metaPath.c_str());

        if (deleteIncompleteTarget && item.status != DownloadStatus::Complete) {
            if (PathFileExistsW(item.savePath.c_str()) && (item.sizeBytes == 0 || item.downloadedBytes < item.sizeBytes)) {
                DeleteFileW(item.savePath.c_str());
            }
        }
    }
}

void DownloadEngine::CancelDownload(const std::wstring& id, bool deleteFiles) {
    DownloadItem itemToClean;
    bool found = false;
    {
        std::lock_guard<std::mutex> lockDownloader(m_downloaderMutex);
        auto it = m_activeDownloaders.find(id);
        if (it != m_activeDownloaders.end()) {
            for (auto& dl : it->second) {
                if (dl) dl->cancel(deleteFiles);
            }
            m_activeDownloaders.erase(it);
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& d : m_downloads) {
            if (d.id == id) {
                d.status = DownloadStatus::Error;
                d.speedBytesPerSec = 0;
                d.diagnosticText = L"Download cancelled by user. Temporary files cleaned up.";
                for (auto& c : d.chunks) c.active = false;
                itemToClean = d;
                found = true;
                break;
            }
        }
    }
    if (found && deleteFiles) {
        CleanupDownloadDiskFiles(itemToClean, true);
    }
    if (m_onStatus) m_onStatus(id, DownloadStatus::Error);
    SaveHistory();
}

void DownloadEngine::DeleteItem(const std::wstring& id) {
    DownloadItem itemToClean;
    bool found = false;
    {
        std::lock_guard<std::mutex> lockDownloader(m_downloaderMutex);
        auto it = m_activeDownloaders.find(id);
        if (it != m_activeDownloaders.end()) {
            for (auto& dl : it->second) {
                if (dl) dl->cancel(true);
            }
            m_activeDownloaders.erase(it);
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_downloads.begin(); it != m_downloads.end(); ++it) {
            if (it->id == id) {
                itemToClean = *it;
                found = true;
                m_downloads.erase(it);
                break;
            }
        }
    }
    if (found) {
        CleanupDownloadDiskFiles(itemToClean, itemToClean.status != DownloadStatus::Complete);
    }
    SaveHistory();
}

void DownloadEngine::SetSpeedLimit(bool enabled, int limitKBps) {
    m_speedLimitEnabled = enabled;
    m_speedLimitKBps = limitKBps;
}

void DownloadEngine::StartDownload(const std::wstring& id) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& d : m_downloads) {
            if (d.id == id) {
                d.status = DownloadStatus::Downloading;
                break;
            }
        }
    }

    if (m_onStatus) m_onStatus(id, DownloadStatus::Downloading);

    std::thread([this, id]() {
        DownloadWorker(id);
    }).detach();
}

void DownloadEngine::PauseDownload(const std::wstring& id) {
    {
        std::lock_guard<std::mutex> lock(m_downloaderMutex);
        auto it = m_activeDownloaders.find(id);
        if (it != m_activeDownloaders.end()) {
            for (auto& dl : it->second) {
                if (dl) dl->pause();
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& d : m_downloads) {
            if (d.id == id) {
                d.status = DownloadStatus::Paused;
                d.speedBytesPerSec = 0;
                for (auto& c : d.chunks) c.active = false;
                break;
            }
        }
    }
    if (m_onStatus) m_onStatus(id, DownloadStatus::Paused);
    SaveHistory();
}

void DownloadEngine::StopAll() {
    {
        std::lock_guard<std::mutex> lock(m_downloaderMutex);
        for (auto& pair : m_activeDownloaders) {
            for (auto& dl : pair.second) {
                if (dl) dl->pause();
            }
        }
    }
    std::vector<std::wstring> pausedIds;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& d : m_downloads) {
            if (d.status == DownloadStatus::Downloading) {
                d.status = DownloadStatus::Paused;
                d.speedBytesPerSec = 0;
                for (auto& c : d.chunks) c.active = false;
                pausedIds.push_back(d.id);
            }
        }
    }
    if (m_onStatus) {
        for (const auto& id : pausedIds) {
            m_onStatus(id, DownloadStatus::Paused);
        }
    }
    SaveHistory();
}

void DownloadEngine::ResumeAll() {
    std::vector<std::wstring> toResume;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& d : m_downloads) {
            if (d.status == DownloadStatus::Paused && (d.sizeBytes == 0 || d.downloadedBytes < d.sizeBytes)) {
                toResume.push_back(d.id);
            }
        }
    }
    for (const auto& id : toResume) {
        StartDownload(id);
    }
}

bool DownloadEngine::CheckDiskSpace(const std::wstring& path, uint64_t requiredBytes, uint64_t& outFreeBytes) {
    outFreeBytes = 0;
    wchar_t volumePath[MAX_PATH] = { 0 };
    if (!GetVolumePathNameW(path.c_str(), volumePath, MAX_PATH)) {
        if (path.length() >= 3 && path[1] == L':') {
            wcsncpy_s(volumePath, path.substr(0, 3).c_str(), MAX_PATH - 1);
        } else {
            return true; // Unable to determine root, allow download
        }
    }
    ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
    if (GetDiskFreeSpaceExW(volumePath, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
        outFreeBytes = freeBytesAvailable.QuadPart;
        // Require file size + 100 MB safety buffer
        uint64_t safetyMargin = 100ULL * 1024ULL * 1024ULL;
        if (requiredBytes > 0 && outFreeBytes < (requiredBytes + safetyMargin)) {
            return false;
        }
    }
    return true;
}

bool DownloadEngine::StartQueue() {
    bool hasQueued = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& d : m_downloads) {
            if (d.status == DownloadStatus::Queued) {
                hasQueued = true;
                break;
            }
        }
    }

    if (!hasQueued) {
        return false;
    }

    m_queueActive = true;
    ProcessNextQueueItem();
    return true;
}

void DownloadEngine::StopQueue() {
    m_queueActive = false;
    std::wstring currentId;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        currentId = m_activeQueueItemId;
        m_activeQueueItemId.clear();
    }
    if (!currentId.empty()) {
        PauseDownload(currentId);
    }
}

bool DownloadEngine::IsQueueActive() const {
    return m_queueActive.load();
}

std::wstring DownloadEngine::GetActiveQueueItemId() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeQueueItemId;
}

void DownloadEngine::ProcessNextQueueItem() {
    if (!m_queueActive.load()) return;

    DownloadItem targetItem;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& d : m_downloads) {
            if (d.status == DownloadStatus::Queued) {
                targetItem = d;
                found = true;
                break;
            }
        }
    }

    if (!found) {
        m_queueActive = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_activeQueueItemId.clear();
        }
        return;
    }

    // Check destination drive space
    uint64_t freeBytes = 0;
    if (!CheckDiskSpace(targetItem.savePath, targetItem.sizeBytes, freeBytes)) {
        m_queueActive = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& d : m_downloads) {
                if (d.id == targetItem.id) {
                    d.status = DownloadStatus::Error;
                    d.diagnosticText = L"Paused: Insufficient disk space on destination drive.";
                    break;
                }
            }
            m_activeQueueItemId.clear();
        }
        if (m_onStatus) m_onStatus(targetItem.id, DownloadStatus::Error);
        SaveHistory();
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activeQueueItemId = targetItem.id;
    }

    StartDownload(targetItem.id);
}

std::shared_ptr<SegmentedDownloader> DownloadEngine::GetActiveDownloader(const std::wstring& id) {
    std::lock_guard<std::mutex> lock(m_downloaderMutex);
    auto it = m_activeDownloaders.find(id);
    if (it != m_activeDownloaders.end() && !it->second.empty()) {
        return it->second.front();
    }
    return nullptr;
}

bool DownloadEngine::IsStreamingMediaURL(const std::wstring& url) {
    if (url.empty()) return false;

    // Check all popular streaming / social video platforms
    if (url.find(L"youtube.com") != std::wstring::npos ||
        url.find(L"youtu.be") != std::wstring::npos ||
        url.find(L"facebook.com") != std::wstring::npos ||
        url.find(L"fb.watch") != std::wstring::npos ||
        url.find(L"fb.com") != std::wstring::npos ||
        url.find(L"instagram.com") != std::wstring::npos ||
        url.find(L"tiktok.com") != std::wstring::npos ||
        url.find(L"twitter.com") != std::wstring::npos ||
        url.find(L"x.com/") != std::wstring::npos ||
        url.find(L"reddit.com") != std::wstring::npos ||
        url.find(L"v.redd.it") != std::wstring::npos ||
        url.find(L"vimeo.com") != std::wstring::npos ||
        url.find(L"dailymotion.com") != std::wstring::npos ||
        url.find(L"twitch.tv") != std::wstring::npos ||
        url.find(L"bilibili.com") != std::wstring::npos ||
        url.find(L"threads.net") != std::wstring::npos ||
        url.find(L"pinterest.com") != std::wstring::npos ||
        url.find(L"soundcloud.com") != std::wstring::npos) {
        return true;
    }

    // Check if URL looks like a streaming video webpage without a binary file extension
    size_t q = url.find(L'?');
    std::wstring pathOnly = (q != std::wstring::npos) ? url.substr(0, q) : url;
    size_t dot = pathOnly.find_last_of(L'.');
    size_t slash = pathOnly.find_last_of(L'/');
    if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash)) {
        if (url.find(L"/reel") != std::wstring::npos ||
            url.find(L"/watch") != std::wstring::npos ||
            url.find(L"/video") != std::wstring::npos ||
            url.find(L"/shorts") != std::wstring::npos ||
            url.find(L"/status/") != std::wstring::npos ||
            url.find(L"/p/") != std::wstring::npos) {
            return true;
        }
    }
    return false;
}

bool DownloadEngine::IsYouTubeURL(const std::wstring& url) {
    return IsStreamingMediaURL(url);
}

std::wstring DownloadEngine::FindFFmpegPath() {
    wchar_t szPath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    PathRemoveFileSpecW(szPath);

    std::wstring localTools = std::wstring(szPath) + L"\\..\\..\\tools\\ffmpeg.exe";
    if (PathFileExistsW(localTools.c_str())) return localTools;

    std::wstring localSame = std::wstring(szPath) + L"\\ffmpeg.exe";
    if (PathFileExistsW(localSame.c_str())) return localSame;

    if (PathFileExistsW(L"d:\\Download Manager AB\\tools\\ffmpeg.exe")) return L"d:\\Download Manager AB\\tools\\ffmpeg.exe";
    if (PathFileExistsW(L"C:\\ffmpeg\\bin\\ffmpeg.exe")) return L"C:\\ffmpeg\\bin\\ffmpeg.exe";
    if (PathFileExistsW(L"C:\\ffmpeg\\ffmpeg.exe")) return L"C:\\ffmpeg\\ffmpeg.exe";

    return L"ffmpeg.exe"; // Fallback to system PATH
}

YouTubeStreams DownloadEngine::ExtractYouTubeStreams(const std::wstring& pageUrl, const std::wstring& quality) {
    YouTubeStreams res;

    wchar_t szPath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    PathRemoveFileSpecW(szPath);

    std::wstring ytDlp = std::wstring(szPath) + L"\\..\\..\\tools\\yt-dlp.exe";
    if (!PathFileExistsW(ytDlp.c_str())) {
        ytDlp = L"d:\\Download Manager AB\\tools\\yt-dlp.exe";
    }

    // Prioritize universal H.264 (AVC) video and AAC audio so videos play natively in Windows Media Player without requiring extra AV1 codecs
    std::wstring fmt;
    if (quality.find(L"Audio") != std::wstring::npos || quality.find(L"MP3") != std::wstring::npos) {
        fmt = L"bestaudio[acodec^=mp4a]/bestaudio[ext=m4a]/bestaudio[acodec^=aac]/bestaudio/best";
    } else if (quality.find(L"1080") != std::wstring::npos) {
        fmt = L"bestvideo[vcodec^=avc1][height<=1080]+bestaudio[acodec^=mp4a]/"
              L"bestvideo[vcodec^=avc][height<=1080]+bestaudio[acodec^=mp4a]/"
              L"bestvideo[vcodec^=avc][height<=1080]+bestaudio/"
              L"best[vcodec^=avc1][height<=1080]/"
              L"best[vcodec^=avc][height<=1080]/"
              L"best[vcodec^=h264][height<=1080]/"
              L"best[ext=mp4][height<=1080][vcodec!=av01][vcodec!=vp09]/"
              L"bestvideo[height<=1080][ext=mp4]+bestaudio[ext=m4a]/"
              L"best[height<=1080][ext=mp4]/"
              L"bestvideo[height<=1080]+bestaudio/"
              L"best[height<=1080]/best";
    } else if (quality.find(L"720") != std::wstring::npos) {
        fmt = L"bestvideo[vcodec^=avc1][height<=720]+bestaudio[acodec^=mp4a]/"
              L"bestvideo[vcodec^=avc][height<=720]+bestaudio[acodec^=mp4a]/"
              L"bestvideo[vcodec^=avc][height<=720]+bestaudio/"
              L"best[vcodec^=avc1][height<=720]/"
              L"best[vcodec^=avc][height<=720]/"
              L"best[vcodec^=h264][height<=720]/"
              L"best[ext=mp4][height<=720][vcodec!=av01][vcodec!=vp09]/"
              L"bestvideo[height<=720][ext=mp4]+bestaudio[ext=m4a]/"
              L"best[height<=720][ext=mp4]/"
              L"bestvideo[height<=720]+bestaudio/"
              L"best[height<=720]/best";
    } else if (quality.find(L"480") != std::wstring::npos) {
        fmt = L"bestvideo[vcodec^=avc1][height<=480]+bestaudio[acodec^=mp4a]/"
              L"bestvideo[vcodec^=avc][height<=480]+bestaudio[acodec^=mp4a]/"
              L"bestvideo[vcodec^=avc][height<=480]+bestaudio/"
              L"best[vcodec^=avc1][height<=480]/"
              L"best[vcodec^=avc][height<=480]/"
              L"best[vcodec^=h264][height<=480]/"
              L"best[ext=mp4][height<=480][vcodec!=av01][vcodec!=vp09]/"
              L"bestvideo[height<=480][ext=mp4]+bestaudio[ext=m4a]/"
              L"best[height<=480][ext=mp4]/"
              L"bestvideo[height<=480]+bestaudio/"
              L"best[height<=480]/best";
    } else {
        fmt = L"bestvideo[vcodec^=avc1]+bestaudio[acodec^=mp4a]/"
              L"bestvideo[vcodec^=avc]+bestaudio[acodec^=mp4a]/"
              L"bestvideo[vcodec^=avc]+bestaudio/"
              L"best[vcodec^=avc1]/"
              L"best[vcodec^=avc]/"
              L"best[vcodec^=h264]/"
              L"best[ext=mp4][vcodec!=av01][vcodec!=vp09]/"
              L"bestvideo[ext=mp4]+bestaudio[ext=m4a]/"
              L"best[ext=mp4]/"
              L"bestvideo+bestaudio/"
              L"best";
    }

    std::wstring cmd = L"\"" + ytDlp + L"\" --no-playlist --no-warnings --print \"%(title)s\" --get-url -f \"" + fmt + L"\" \"" + pageUrl + L"\"";

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) return res;
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    BOOL success = CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi);
    CloseHandle(hWritePipe);

    if (!success) {
        CloseHandle(hReadPipe);
        return res;
    }

    std::string out;
    char buffer[1024];
    DWORD bytesRead = 0;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = 0;
        out += buffer;
    }
    CloseHandle(hReadPipe);

    WaitForSingleObject(pi.hProcess, 15000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    std::vector<std::wstring> rawLines;
    std::stringstream ss(out);
    std::string l;
    while (std::getline(ss, l)) {
        while (!l.empty() && (l.back() == '\r' || l.back() == '\n' || l.back() == ' ')) l.pop_back();
        while (!l.empty() && (l.front() == ' ')) l.erase(l.begin());
        if (!l.empty()) {
            int len = MultiByteToWideChar(CP_UTF8, 0, l.c_str(), (int)l.length(), NULL, 0);
            std::wstring wl(len, 0);
            MultiByteToWideChar(CP_UTF8, 0, l.c_str(), (int)l.length(), &wl[0], len);
            rawLines.push_back(wl);
        }
    }

    std::vector<std::wstring> urlLines;
    for (const auto& line : rawLines) {
        if (line.rfind(L"http://", 0) == 0 || line.rfind(L"https://", 0) == 0) {
            urlLines.push_back(line);
        } else if (res.title.empty()) {
            res.title = line;
        }
    }

    if (urlLines.size() >= 2) {
        res.videoUrl = urlLines[0];
        res.audioUrl = urlLines[1];
        res.needsMux = true;
    } else if (urlLines.size() == 1) {
        res.videoUrl = urlLines[0];
        res.needsMux = false;
    }

    return res;
}

bool DownloadEngine::MuxVideoAudio(const std::wstring& videoPath, const std::wstring& audioPath, const std::wstring& outputPath) {
    std::wstring ffmpeg = FindFFmpegPath();

    std::wstring cmd = L"\"" + ffmpeg + L"\" -y -i \"" + videoPath + L"\" -i \"" + audioPath + L"\" -c copy -movflags +faststart \"" + outputPath + L"\"";

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    BOOL success = CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi);
    if (!success) return false;

    WaitForSingleObject(pi.hProcess, 30000);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (exitCode == 0);
}

bool DownloadEngine::ConvertToMp3(const std::wstring& inputAudioPath, const std::wstring& outputMp3Path) {
    std::wstring ffmpeg = FindFFmpegPath();

    std::wstring cmd = L"\"" + ffmpeg + L"\" -y -i \"" + inputAudioPath + L"\" -vn -c:a libmp3lame -b:a 320k \"" + outputMp3Path + L"\"";

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    BOOL success = CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi);
    if (!success) return false;

    WaitForSingleObject(pi.hProcess, 60000);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (exitCode == 0);
}

void DownloadEngine::DownloadWorker(std::wstring id) {
    DownloadItem item;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& d : m_downloads) {
            if (d.id == id) {
                item = d;
                break;
            }
        }
    }

    if (item.url.empty()) return;

    // Streaming Media Resolution & Dual-Stream 16-Thread Download (YouTube, Facebook, TikTok, Instagram, Twitter, etc.)
    if (IsStreamingMediaURL(item.url)) {
        item.originalPageUrl = item.url;
        YouTubeStreams streams = ExtractYouTubeStreams(item.url, item.quality);

        if (streams.videoUrl.empty()) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                for (auto& d : m_downloads) {
                    if (d.id == id) {
                        d.status = DownloadStatus::Error;
                        d.diagnosticText = L"Failed to extract video stream from URL. Please check the link.";
                        break;
                    }
                }
            }
            if (m_onStatus) m_onStatus(id, DownloadStatus::Error);
            return;
        }

        bool isAudio = (item.quality.find(L"Audio") != std::wstring::npos ||
                        item.quality.find(L"MP3") != std::wstring::npos ||
                        item.quality.find(L"M4A") != std::wstring::npos);
        std::wstring targetExt = (item.quality.find(L"M4A") != std::wstring::npos) ? L".m4a" : (isAudio ? L".mp3" : L".mp4");

        // Apply extracted video title automatically if available and user didn't specify a custom filename
        bool hasCustomFilename = (!item.filename.empty() &&
                                  item.filename != L"YouTube_Video.mp4" &&
                                  item.filename != L"YouTube_Audio.mp3" &&
                                  item.filename != L"Facebook_Video.mp4" &&
                                  item.filename != L"Facebook_Audio.mp3" &&
                                  item.filename != L"Stream_Video.mp4" &&
                                  item.filename != L"Stream_Audio.mp3" &&
                                  item.filename != L"video.mp4" &&
                                  item.filename != L"audio.mp3" &&
                                  item.filename != L"watch.mp4" &&
                                  item.filename != L"download.bin" &&
                                  item.filename != L"videoplayback.mp4");

        if (!hasCustomFilename && !streams.title.empty()) {
            std::wstring cleanTitle = streams.title;
            for (auto& ch : cleanTitle) {
                if (ch == L'/' || ch == L'\\' || ch == L':' || ch == L'*' ||
                    ch == L'?' || ch == L'"' || ch == L'<' || ch == L'>' || ch == L'|') {
                    ch = L' ';
                }
            }
            while (!cleanTitle.empty() && (cleanTitle.back() == L' ' || cleanTitle.back() == L'\t')) cleanTitle.pop_back();
            while (!cleanTitle.empty() && (cleanTitle.front() == L' ' || cleanTitle.front() == L'\t')) cleanTitle.erase(cleanTitle.begin());
            if (cleanTitle.length() > 120) cleanTitle = cleanTitle.substr(0, 120);

            if (!cleanTitle.empty()) {
                item.filename = cleanTitle + targetExt;

                wchar_t szDir[MAX_PATH] = { 0 };
                wcsncpy_s(szDir, item.savePath.c_str(), MAX_PATH - 1);
                PathRemoveFileSpecW(szDir);
                if (wcslen(szDir) > 0) {
                    item.savePath = std::wstring(szDir) + L"\\" + item.filename;
                } else {
                    item.savePath = GetDefaultDownloadsFolder() + (isAudio ? L"Music\\" : L"Video\\") + item.filename;
                }

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    for (auto& d : m_downloads) {
                        if (d.id == id) {
                            d.filename = item.filename;
                            d.savePath = item.savePath;
                            break;
                        }
                    }
                }
            }
        }

        // Ensure proper filename and savePath extension (.mp4 or .mp3)
        if (isAudio && item.savePath.length() > 4 && item.savePath.substr(item.savePath.length() - 4) == L".mp4") {
            item.savePath = item.savePath.substr(0, item.savePath.length() - 4) + targetExt;
        }
        if (isAudio && item.filename.length() > 4 && item.filename.substr(item.filename.length() - 4) == L".mp4") {
            item.filename = item.filename.substr(0, item.filename.length() - 4) + targetExt;
        }

        if (item.savePath.find(L".mp4") == std::wstring::npos && item.savePath.find(L".mp3") == std::wstring::npos && item.savePath.find(L".m4a") == std::wstring::npos) {
            item.savePath += targetExt;
            item.filename += targetExt;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                for (auto& d : m_downloads) {
                    if (d.id == id) {
                        d.savePath = item.savePath;
                        d.filename = item.filename;
                        break;
                    }
                }
            }
        }

        if (streams.needsMux && !streams.audioUrl.empty()) {
            std::wstring videoTmp = item.savePath + L".video_" + item.id + L".tmp";
            std::wstring audioTmp = item.savePath + L".audio_" + item.id + L".tmp";

            auto pVideoDownloader = std::make_shared<SegmentedDownloader>();
            auto pAudioDownloader = std::make_shared<SegmentedDownloader>();

            {
                std::lock_guard<std::mutex> lock(m_downloaderMutex);
                m_activeDownloaders[id] = { pVideoDownloader, pAudioDownloader };
            }
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                for (auto& d : m_downloads) {
                    if (d.id == id) {
                        d.liveSlots = pVideoDownloader->getLiveSlots();
                        d.videoTmpPath = videoTmp;
                        d.audioTmpPath = audioTmp;
                        d.needsMux = true;
                        break;
                    }
                }
            }

            auto pVideoComplete = std::make_shared<std::atomic<bool>>(false);
            auto pAudioComplete = std::make_shared<std::atomic<bool>>(false);
            auto pVideoFailed = std::make_shared<std::atomic<bool>>(false);
            auto pAudioFailed = std::make_shared<std::atomic<bool>>(false);

            pVideoDownloader->start(streams.videoUrl, videoTmp, 12, [this, id, pAudioDownloader, pVideoComplete, pVideoFailed](const DownloadStats& vStats) {
                if (vStats.isComplete) pVideoComplete->store(true, std::memory_order_relaxed);
                if (vStats.isFailed) pVideoFailed->store(true, std::memory_order_relaxed);

                uint64_t aDown = pAudioDownloader->getDownloadedBytes();
                uint64_t aTot = pAudioDownloader->getTotalSize();
                uint64_t totalDown = vStats.downloadedBytes + aDown;
                uint64_t totalSize = (vStats.totalSize > 0 && aTot > 0) ? (vStats.totalSize + aTot) : vStats.totalSize;

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    for (auto& d : m_downloads) {
                        if (d.id == id) {
                            d.downloadedBytes = totalDown;
                            d.sizeBytes = totalSize;
                            d.speedBytesPerSec = vStats.smoothedSpeedBps;
                            d.diagnosticText = vStats.diagnosticText;
                            break;
                        }
                    }
                }
                if (m_onProgress) m_onProgress(id, totalDown, totalSize, (uint64_t)vStats.smoothedSpeedBps);
            });

            pAudioDownloader->start(streams.audioUrl, audioTmp, 4, [pAudioComplete, pAudioFailed](const DownloadStats& aStats) {
                if (aStats.isComplete) pAudioComplete->store(true, std::memory_order_relaxed);
                if (aStats.isFailed) pAudioFailed->store(true, std::memory_order_relaxed);
            });

            while (m_running && (!pVideoComplete->load() || !pAudioComplete->load()) && !pVideoFailed->load() && !pAudioFailed->load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            if (pVideoComplete->load() && pAudioComplete->load()) {
                // Transition status to Merging
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    for (auto& d : m_downloads) {
                        if (d.id == id) {
                            d.status = DownloadStatus::Merging;
                            d.diagnosticText = L"Merging Video & Audio Streams (0.5s lossless)...";
                            break;
                        }
                    }
                }
                if (m_onStatus) m_onStatus(id, DownloadStatus::Merging);

                bool muxSuccess = MuxVideoAudio(videoTmp, audioTmp, item.savePath);
                DeleteFileW(videoTmp.c_str());
                DeleteFileW(audioTmp.c_str());

                if (muxSuccess) {
                    WIN32_FILE_ATTRIBUTE_DATA fad;
                    uint64_t finalSize = 0;
                    if (GetFileAttributesExW(item.savePath.c_str(), GetFileExInfoStandard, &fad)) {
                        finalSize = ((uint64_t)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
                    }

                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        for (auto& d : m_downloads) {
                            if (d.id == id) {
                                d.status = DownloadStatus::Complete;
                                d.downloadedBytes = (finalSize > 0) ? finalSize : d.sizeBytes;
                                d.sizeBytes = (finalSize > 0) ? finalSize : d.sizeBytes;
                                d.speedBytesPerSec = 0;
                                break;
                            }
                        }
                    }
                    SaveHistory();
                    if (m_onStatus) m_onStatus(id, DownloadStatus::Complete);
                } else {
                    CleanupDownloadDiskFiles(item, true);
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        for (auto& d : m_downloads) {
                            if (d.id == id) {
                                d.status = DownloadStatus::Error;
                                d.diagnosticText = L"FFmpeg muxing failed. Temporary files cleaned up.";
                                break;
                            }
                        }
                    }
                    if (m_onStatus) m_onStatus(id, DownloadStatus::Error);
                }
            } else if (pVideoFailed->load() || pAudioFailed->load()) {
                CleanupDownloadDiskFiles(item, true);
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    for (auto& d : m_downloads) {
                        if (d.id == id) {
                            d.status = DownloadStatus::Error;
                            d.diagnosticText = L"Stream connection failed. Temporary files cleaned up.";
                            break;
                        }
                    }
                }
                if (m_onStatus) m_onStatus(id, DownloadStatus::Error);
            } else {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    for (auto& d : m_downloads) {
                        if (d.id == id) {
                            d.status = DownloadStatus::Paused;
                            break;
                        }
                    }
                }
                if (m_onStatus) m_onStatus(id, DownloadStatus::Paused);
            }
            if (m_queueActive.load() && id == m_activeQueueItemId) {
                std::thread([this]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    ProcessNextQueueItem();
                }).detach();
            }
            return;
        } else {
            // Single Stream (e.g. 720p / 360p or Audio only)
            item.url = streams.videoUrl;
        }
    }

    // Direct Segmented HTTP/HTTPS Range Downloader (All direct files, TikTok, Instagram, Twitter, etc.)
    auto pDownloader = std::make_shared<SegmentedDownloader>();
    {
        std::lock_guard<std::mutex> lock(m_downloaderMutex);
        m_activeDownloaders[id] = { pDownloader };
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& d : m_downloads) {
            if (d.id == id) {
                d.liveSlots = pDownloader->getLiveSlots();
                break;
            }
        }
    }

    int conns = (item.connections > 0) ? item.connections : 16;
    {
      CreateDirectoryW(L"C:\\temp", NULL);
      std::wofstream dbg(L"C:\\temp\\dm_debug.txt", std::ios::app);
      if (dbg.is_open()) {
        dbg << L"=== [ENGINE WORKER BEFORE START] ===" << std::endl;
        dbg << L"ID: " << item.id << std::endl;
        dbg << L"URL: " << item.url << std::endl;
        dbg << L"Referer: " << item.referer << std::endl;
        dbg << L"Cookies: " << item.cookies << std::endl;
        dbg << L"UserAgent: " << item.userAgent << std::endl;
        dbg << L"===================================" << std::endl << std::endl;
        dbg.close();
      }
    }
    bool isAudioStream = (item.quality.find(L"Audio") != std::wstring::npos ||
                          item.quality.find(L"MP3") != std::wstring::npos ||
                          item.category == L"Music");
    std::wstring destFilePath = isAudioStream ? (item.savePath + L".raw_audio_" + item.id + L".tmp") : item.savePath;

    pDownloader->start(item.url, destFilePath, conns, [this, id, isAudioStream, destFilePath, item](const DownloadStats& stats) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& d : m_downloads) {
                if (d.id == id) {
                    d.downloadedBytes = stats.downloadedBytes;
                    d.sizeBytes = stats.totalSize;
                    d.speedBytesPerSec = (uint64_t)stats.smoothedSpeedBps;
                    d.diagnosticText = stats.diagnosticText;

                    if (stats.isComplete && stats.downloadedBytes > 0) {
                        d.status = isAudioStream ? DownloadStatus::Merging : DownloadStatus::Complete;
                        d.speedBytesPerSec = 0;
                    } else if (stats.isComplete && stats.downloadedBytes == 0) {
                        d.status = DownloadStatus::Error;
                        d.diagnosticText = L"Remote server returned 0 bytes or an invalid stream response.";
                        d.speedBytesPerSec = 0;
                    } else if (stats.isFailed) {
                        d.status = DownloadStatus::Paused;
                        d.speedBytesPerSec = 0;
                    }
                    break;
                }
            }
        }

        if (m_onProgress) {
            m_onProgress(id, stats.downloadedBytes, stats.totalSize, (uint64_t)stats.smoothedSpeedBps);
        }

        if (stats.isComplete && stats.downloadedBytes > 0) {
            if (isAudioStream) {
                // Transcode raw stream into authentic 320kbps MP3
                if (m_onStatus) m_onStatus(id, DownloadStatus::Merging);

                bool cvtSuccess = ConvertToMp3(destFilePath, item.savePath);
                DeleteFileW(destFilePath.c_str());

                if (cvtSuccess) {
                    WIN32_FILE_ATTRIBUTE_DATA fad;
                    uint64_t finalSize = 0;
                    if (GetFileAttributesExW(item.savePath.c_str(), GetFileExInfoStandard, &fad)) {
                        finalSize = ((uint64_t)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
                    }
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        for (auto& d : m_downloads) {
                            if (d.id == id) {
                                d.status = DownloadStatus::Complete;
                                d.downloadedBytes = (finalSize > 0) ? finalSize : d.sizeBytes;
                                d.sizeBytes = (finalSize > 0) ? finalSize : d.sizeBytes;
                                d.speedBytesPerSec = 0;
                                break;
                            }
                        }
                    }
                    SaveHistory();
                    if (m_onStatus) m_onStatus(id, DownloadStatus::Complete);
                } else {
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        for (auto& d : m_downloads) {
                            if (d.id == id) {
                                d.status = DownloadStatus::Error;
                                d.diagnosticText = L"MP3 transcoding failed.";
                                break;
                            }
                        }
                    }
                    if (m_onStatus) m_onStatus(id, DownloadStatus::Error);
                }
            } else {
                SaveHistory();
                if (m_onStatus) m_onStatus(id, DownloadStatus::Complete);
            }
        } else if (stats.isComplete && stats.downloadedBytes == 0) {
            SaveHistory();
            if (m_onStatus) m_onStatus(id, DownloadStatus::Error);
        } else if (stats.isFailed) {
            SaveHistory();
            if (m_onStatus) m_onStatus(id, DownloadStatus::Paused);
        }
    }, item.cookies, item.referer, item.userAgent);

    while (pDownloader->isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (m_queueActive.load() && id == m_activeQueueItemId) {
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            ProcessNextQueueItem();
        }).detach();
    }
}

std::string DownloadEngine::ProbeFormatSizesJson(const std::wstring& pageUrl) {
    if (!DownloadEngine::IsStreamingMediaURL(pageUrl)) {
        return "{\"status\":\"ok\",\"formats\":{}}";
    }

    wchar_t szPath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    PathRemoveFileSpecW(szPath);

    std::wstring ytDlp = std::wstring(szPath) + L"\\..\\..\\tools\\yt-dlp.exe";
    if (!PathFileExistsW(ytDlp.c_str())) {
        ytDlp = L"d:\\Download Manager AB\\tools\\yt-dlp.exe";
    }

    // Fast probe for standard heights and audio streams
    std::wstring fmt = L"bestvideo[height<=1080]+bestaudio/bestvideo+bestaudio,bestvideo[height<=720]+bestaudio/bestvideo+bestaudio,bestvideo[height<=480]+bestaudio/bestvideo+bestaudio,bestaudio[acodec^=mp4a]/bestaudio";
    std::wstring cmd = L"\"" + ytDlp + L"\" --no-playlist --no-warnings -f \"" + fmt + L"\" --print \"%(height)s|%(filesize,filesize_approx)s\" \"" + pageUrl + L"\"";

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) return "{\"status\":\"ok\",\"formats\":{}}";
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    BOOL success = CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi);
    CloseHandle(hWritePipe);

    if (!success) {
        CloseHandle(hReadPipe);
        return "{\"status\":\"ok\",\"formats\":{}}";
    }

    std::string out;
    char buffer[2048];
    DWORD bytesRead = 0;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = 0;
        out += buffer;
    }
    CloseHandle(hReadPipe);

    WaitForSingleObject(pi.hProcess, 8000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    uint64_t size1080 = 0, size720 = 0, size480 = 0, sizeAudio = 0;
    std::stringstream ss(out);
    std::string line;
    while (std::getline(ss, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) line.pop_back();
        size_t pipePos = line.find('|');
        if (pipePos != std::string::npos) {
            std::string key = line.substr(0, pipePos);
            std::string valStr = line.substr(pipePos + 1);
            try {
                uint64_t bytes = std::stoull(valStr);
                if (key == "1080" && size1080 == 0) size1080 = bytes;
                else if (key == "720" && size720 == 0) size720 = bytes;
                else if (key == "480" && size480 == 0) size480 = bytes;
                else if (key == "NA" && sizeAudio == 0) sizeAudio = bytes;
            } catch (...) {}
        }
    }

    std::ostringstream json;
    json << "{\"status\":\"ok\",\"formats\":{"
         << "\"1080p\":" << size1080 << ","
         << "\"720p\":" << size720 << ","
         << "\"480p\":" << size480 << ","
         << "\"audio\":" << sizeAudio
         << "}}";
    return json.str();
}

std::wstring DownloadEngine::GetMediaTitle(const std::wstring& pageUrl) {
    if (!DownloadEngine::IsStreamingMediaURL(pageUrl)) {
        return L"";
    }

    wchar_t szPath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    PathRemoveFileSpecW(szPath);

    std::wstring ytDlp = std::wstring(szPath) + L"\\..\\..\\tools\\yt-dlp.exe";
    if (!PathFileExistsW(ytDlp.c_str())) {
        ytDlp = L"d:\\Download Manager AB\\tools\\yt-dlp.exe";
    }

    std::wstring cmd = L"\"" + ytDlp + L"\" --encoding utf-8 --no-check-certificates --no-warnings --no-playlist --socket-timeout 5 --print \"%(title)s\" \"" + pageUrl + L"\"";

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) return L"";
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    BOOL success = CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi);
    CloseHandle(hWritePipe);

    if (!success) {
        CloseHandle(hReadPipe);
        return L"";
    }

    std::string out;
    char buffer[1024];
    DWORD bytesRead = 0;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = 0;
        out += buffer;
    }
    CloseHandle(hReadPipe);

    WaitForSingleObject(pi.hProcess, 12000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    while (!out.empty() && (out.back() == '\r' || out.back() == '\n' || out.back() == ' ')) out.pop_back();
    while (!out.empty() && (out.front() == ' ')) out.erase(out.begin());

    if (!out.empty()) {
        int len = MultiByteToWideChar(CP_UTF8, 0, out.c_str(), (int)out.length(), NULL, 0);
        std::wstring title;
        if (len > 0) {
            title.resize(len);
            MultiByteToWideChar(CP_UTF8, 0, out.c_str(), (int)out.length(), &title[0], len);
        } else {
            len = MultiByteToWideChar(CP_ACP, 0, out.c_str(), (int)out.length(), NULL, 0);
            if (len > 0) {
                title.resize(len);
                MultiByteToWideChar(CP_ACP, 0, out.c_str(), (int)out.length(), &title[0], len);
            }
        }

        for (auto& ch : title) {
            if (ch == L'/' || ch == L'\\' || ch == L':' || ch == L'*' ||
                ch == L'?' || ch == L'"' || ch == L'<' || ch == L'>' || ch == L'|') {
                ch = L' ';
            }
        }
        while (!title.empty() && (title.back() == L' ' || title.back() == L'\t')) title.pop_back();
        while (!title.empty() && (title.front() == L' ' || title.front() == L'\t')) title.erase(title.begin());
        if (title.length() > 120) title = title.substr(0, 120);

        return title;
    }

    return L"";
}
