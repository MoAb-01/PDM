#pragma once
#include "Models.hpp"
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#pragma comment(lib, "ws2_32.lib")

struct YouTubeStreams {
  std::wstring title;
  std::wstring videoUrl;
  std::wstring audioUrl;
  bool needsMux = false;
};

class DownloadEngine {
public:
  using ProgressCallback =
      std::function<void(const std::wstring &id, uint64_t downloaded,
                         uint64_t total, uint64_t speed)>;
  using StatusCallback =
      std::function<void(const std::wstring &id, DownloadStatus status)>;

  DownloadEngine();
  ~DownloadEngine();

  void SetCallbacks(ProgressCallback onProgress, StatusCallback onStatus);
  void AddItem(const DownloadItem &item);
  void StartDownload(const std::wstring &id);
  void PauseDownload(const std::wstring &id);
  void StopAll();
  void ResumeAll();

  std::vector<DownloadItem> GetDownloads();
  DownloadItem GetItem(const std::wstring &id);
  std::shared_ptr<std::array<LiveStreamSlot, 16>>
  GetLiveSlots(const std::wstring &id);
  std::shared_ptr<class SegmentedDownloader>
  GetActiveDownloader(const std::wstring &id);
  void UpdateItem(const DownloadItem &item);
  void DeleteItem(const std::wstring &id);
  void CancelDownload(const std::wstring &id, bool deleteFiles = true);
  static void CleanupDownloadDiskFiles(const DownloadItem &item,
                                       bool deleteIncompleteTarget = true);

  void SetSpeedLimit(bool enabled, int limitKBps);

  void SaveHistory();
  void LoadHistory();

  static bool IsStreamingMediaURL(const std::wstring &url);
  static bool IsYouTubeURL(const std::wstring &url);
  static std::wstring FindFFmpegPath();
  static YouTubeStreams ExtractYouTubeStreams(const std::wstring &pageUrl,
                                              const std::wstring &quality);
  static bool MuxVideoAudio(const std::wstring &videoPath,
                            const std::wstring &audioPath,
                            const std::wstring &outputPath);
  static std::string ProbeFormatSizesJson(const std::wstring &pageUrl);
  static std::wstring GetMediaTitle(const std::wstring &pageUrl);

private:
  void DownloadWorker(std::wstring id);
  void ChunkWorker(std::wstring id, int chunkIndex, uint64_t startByte,
                   uint64_t endByte, HANDLE hFile);

  std::mutex m_mutex;
  std::vector<DownloadItem> m_downloads;
  std::atomic<bool> m_running{true};
  std::atomic<bool> m_speedLimitEnabled{false};
  std::atomic<int> m_speedLimitKBps{2048};

  ProgressCallback m_onProgress;
  StatusCallback m_onStatus;

  std::vector<std::thread> m_activeThreads;
  std::mutex m_downloaderMutex;
  std::map<std::wstring, std::shared_ptr<class SegmentedDownloader>>
      m_activeDownloaders;
};
