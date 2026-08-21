#include "../include/DownloadEngine.hpp"
#include "../include/SegmentedDownloader.hpp"
#include "../include/PersistenceManager.hpp"
#include <iostream>
#include <fstream>
#include <shlwapi.h>
#include <wininet.h>
#include <sstream>
#include <regex>
#include <iomanip>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "wininet.lib")

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
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& d : m_downloads) {
        if (d.id == id) return d;
    }
    return DownloadItem{};
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

void DownloadEngine::DeleteItem(const std::wstring& id) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_downloads.begin(); it != m_downloads.end(); ++it) {
            if (it->id == id) {
                m_downloads.erase(it);
                break;
            }
        }
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
                if (d.chunks.empty()) {
                    int conn = (d.connections > 0) ? d.connections : 16;
                    uint64_t estSize = (d.sizeBytes > 0) ? d.sizeBytes : 10000000;
                    uint64_t chunkSize = estSize / conn;
                    for (int i = 0; i < conn; ++i) {
                        DownloadChunk c;
                        c.id = i + 1;
                        c.startByte = i * chunkSize;
                        c.endByte = (i == (conn - 1)) ? estSize : ((i + 1) * chunkSize - 1);
                        c.downloadedBytes = 0;
                        c.active = true;
                        c.completed = false;
                        d.chunks.push_back(c);
                    }
                }
                break;
            }
        }
    }

    if (m_onStatus) m_onStatus(id, DownloadStatus::Downloading);

    m_activeThreads.emplace_back(&DownloadEngine::DownloadWorker, this, id);
}

void DownloadEngine::PauseDownload(const std::wstring& id) {
    {
        std::lock_guard<std::mutex> lock(m_downloaderMutex);
        auto it = m_activeDownloaders.find(id);
        if (it != m_activeDownloaders.end() && it->second) {
            it->second->pause();
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
            if (pair.second) pair.second->pause();
        }
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& d : m_downloads) {
        if (d.status == DownloadStatus::Downloading) {
            d.status = DownloadStatus::Paused;
            d.speedBytesPerSec = 0;
            for (auto& c : d.chunks) c.active = false;
            if (m_onStatus) m_onStatus(d.id, DownloadStatus::Paused);
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

    // Case 1: Web Streaming Video (YouTube, Shorts, TikTok, etc.)
    if (IsStreamingPlatformUrl(item.url)) {
        std::wstring toolExe = L"d:\\Download Manager AB\\tools\\yt-dlp.exe";
        std::wstring ffmpegExe = L"C:\\ffmpeg\\ffmpeg.exe";

        // IDM 16-Thread Acceleration + Universal H.264 (AVC) + AAC encoding for 100% Windows Media Player compatibility
        std::wstring cmd = L"\"" + toolExe + L"\" --newline --progress-template \"download:[idm_p] %(progress._percent_str)s | %(progress._total_bytes_str)s | %(progress._speed_str)s | %(progress._eta_str)s\" --no-playlist -N 16 --concurrent-fragments 16 --buffer-size 16M -S \"res,vcodec:h264,acodec:m4a\" --merge-output-format mp4 --ffmpeg-location \"" + ffmpegExe + L"\" -o \"" + item.savePath + L"\" \"" + item.url + L"\"";

        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
        HANDLE hReadPipe, hWritePipe;
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) return;
        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi = { 0 };
        std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
        cmdBuf.push_back(0);

        BOOL success = CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        CloseHandle(hWritePipe);

        if (success) {
            char buffer[512];
            DWORD bytesRead;
            std::string lineAcc;

            // Pattern for custom unbuffered template: "download:[idm_p] 24.5% | 35.20MiB | 6.42MiB/s | 00:03"
            std::regex reTemplate(R"(\[idm_p\]\s+(\d+(?:\.\d+)?)%\s+\|\s+(\d+(?:\.\d+)?)\s*([A-Za-z]+)\s+\|\s+(\d+(?:\.\d+)?)\s*([A-Za-z]+)/s(?:\s+\|\s+(\d+:\d+(?::\d+)?))?)");
            // Standard fallback regex
            std::regex reStandard(R"(\[download\]\s+(\d+(?:\.\d+)?)%\s+of\s+~?\s*(\d+(?:\.\d+)?)\s*([A-Za-z]+)\s+at\s+(\d+(?:\.\d+)?)\s*([A-Za-z]+)/s)");

            double smoothedSpeed = 0.0;

            while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                lineAcc += buffer;

                size_t pos;
                while ((pos = lineAcc.find_first_of("\r\n")) != std::string::npos) {
                    std::string line = lineAcc.substr(0, pos);
                    lineAcc.erase(0, pos + 1);

                    double pct = 0;
                    double totalVal = 0, speedVal = 0;
                    std::string totalUnit = "MiB", speedUnit = "MiB";
                    bool matched = false;

                    std::smatch match;
                    if (std::regex_search(line, match, reTemplate)) {
                        pct = std::stod(match[1].str());
                        totalVal = std::stod(match[2].str());
                        totalUnit = match[3].str();
                        speedVal = std::stod(match[4].str());
                        speedUnit = match[5].str();
                        matched = true;
                    } else if (std::regex_search(line, match, reStandard)) {
                        pct = std::stod(match[1].str());
                        totalVal = std::stod(match[2].str());
                        totalUnit = match[3].str();
                        speedVal = std::stod(match[4].str());
                        speedUnit = match[5].str();
                        matched = true;
                    }

                    if (matched && totalVal > 0) {
                        uint64_t realTotal = ParseSizeToBytes(totalVal, totalUnit);
                        uint64_t instantSpeed = ParseSizeToBytes(speedVal, speedUnit);

                        // Exponential Smoothing: smoothed = 0.2 * instant + 0.8 * smoothed
                        if (smoothedSpeed == 0.0) smoothedSpeed = (double)instantSpeed;
                        else smoothedSpeed = 0.2 * (double)instantSpeed + 0.8 * smoothedSpeed;

                        uint64_t realDownloaded = (uint64_t)((pct / 100.0) * (double)realTotal);

                        {
                            std::lock_guard<std::mutex> lock(m_mutex);
                            for (auto& d : m_downloads) {
                                if (d.id == id) {
                                    d.downloadedBytes = realDownloaded;
                                    d.sizeBytes = realTotal;
                                    d.speedBytesPerSec = (uint64_t)smoothedSpeed;

                                    // Silky-smooth chunk thread visualizer
                                    for (size_t i = 0; i < d.chunks.size(); ++i) {
                                        double chunkRatio = (double)(i + 1) / (double)d.chunks.size();
                                        if ((pct / 100.0) >= chunkRatio) {
                                            d.chunks[i].completed = true;
                                            d.chunks[i].active = false;
                                        } else {
                                            d.chunks[i].active = true;
                                        }
                                    }
                                    break;
                                }
                            }
                        }

                        if (m_onProgress) {
                            m_onProgress(id, realDownloaded, realTotal, (uint64_t)smoothedSpeed);
                        }
                    }
                }
            }

            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hReadPipe);

            // Get exact final file size from disk
            WIN32_FILE_ATTRIBUTE_DATA fad;
            uint64_t finalSize = 0;
            if (GetFileAttributesExW(item.savePath.c_str(), GetFileExInfoStandard, &fad)) {
                finalSize = ((uint64_t)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
            }

            // Mark completed
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                for (auto& d : m_downloads) {
                    if (d.id == id) {
                        d.status = DownloadStatus::Complete;
                        d.downloadedBytes = (finalSize > 0) ? finalSize : d.sizeBytes;
                        d.sizeBytes = (finalSize > 0) ? finalSize : d.sizeBytes;
                        d.speedBytesPerSec = 0;
                        for (auto& c : d.chunks) { c.completed = true; c.active = false; }
                        break;
                    }
                }
            }
            SaveHistory();
            if (m_onStatus) m_onStatus(id, DownloadStatus::Complete);
            return;
        }
    }

    // Case 2: Segmented HTTP/HTTPS Range Downloader (Native SegmentedDownloader Engine)
    auto pDownloader = std::make_shared<SegmentedDownloader>();
    {
        std::lock_guard<std::mutex> lock(m_downloaderMutex);
        m_activeDownloaders[id] = pDownloader;
    }

    int conns = (item.connections > 0) ? item.connections : 16;
    pDownloader->start(item.url, item.savePath, conns, [this, id](const DownloadStats& stats) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& d : m_downloads) {
                if (d.id == id) {
                    d.downloadedBytes = stats.downloadedBytes;
                    d.sizeBytes = stats.totalSize;
                    d.speedBytesPerSec = stats.smoothedSpeedBps;

                    // Sync segment states to chunk list for visualizer
                    if (d.chunks.size() != stats.segments.size()) {
                        d.chunks.clear();
                        for (const auto& s : stats.segments) {
                            DownloadChunk c;
                            c.id = s.id + 1;
                            c.startByte = s.start;
                            c.endByte = s.end;
                            c.downloadedBytes = s.downloadedBytes.load(std::memory_order_relaxed);
                            c.active = (s.status == SegmentStatus::Downloading);
                            c.completed = (s.status == SegmentStatus::Completed);
                            d.chunks.push_back(c);
                        }
                    } else {
                        for (size_t i = 0; i < stats.segments.size(); ++i) {
                            d.chunks[i].downloadedBytes = stats.segments[i].downloadedBytes.load(std::memory_order_relaxed);
                            d.chunks[i].active = (stats.segments[i].status == SegmentStatus::Downloading);
                            d.chunks[i].completed = (stats.segments[i].status == SegmentStatus::Completed);
                        }
                    }

                    if (stats.isComplete) {
                        d.status = DownloadStatus::Complete;
                        d.speedBytesPerSec = 0;
                    } else if (stats.isFailed) {
                        d.status = DownloadStatus::Paused; // Paused on error for user resume
                        d.speedBytesPerSec = 0;
                    }
                    break;
                }
            }
        }

        if (m_onProgress) {
            m_onProgress(id, stats.downloadedBytes, stats.totalSize, stats.smoothedSpeedBps);
        }

        if (stats.isComplete) {
            SaveHistory();
            if (m_onStatus) m_onStatus(id, DownloadStatus::Complete);
        } else if (stats.isFailed) {
            SaveHistory();
            if (m_onStatus) m_onStatus(id, DownloadStatus::Paused);
        }
    });

    // Keep pDownloader alive and running
    while (pDownloader->isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
