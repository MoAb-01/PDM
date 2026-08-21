// Download simulation and range request engine with multi-connection chunk slicing

export class DownloadEngine {
  constructor(listeners = {}) {
    this.downloads = [];
    this.listeners = listeners;
    this.intervalId = null;
    this.speedLimitEnabled = false;
    this.speedLimitKBps = 2048;
  }

  setDownloads(downloads) {
    this.downloads = downloads;
  }

  setSpeedLimit(enabled, limitKBps) {
    this.speedLimitEnabled = enabled;
    this.speedLimitKBps = limitKBps;
  }

  startTicker() {
    if (this.intervalId) return;
    this.intervalId = setInterval(() => {
      this.tick();
    }, 500);
  }

  stopTicker() {
    if (this.intervalId) {
      clearInterval(this.intervalId);
      this.intervalId = null;
    }
  }

  tick() {
    let hasActive = false;
    const updated = this.downloads.map(item => {
      if (item.status !== "Downloading") return item;
      hasActive = true;

      // Calculate speed
      let baseSpeed = 8 * 1024 * 1024; // 8 MB/s base
      // Random fluctuation
      const jitter = (Math.random() - 0.5) * 2 * 1024 * 1024;
      let currentSpeed = Math.max(500 * 1024, baseSpeed + jitter);

      if (this.speedLimitEnabled && this.speedLimitKBps > 0) {
        currentSpeed = Math.min(currentSpeed, this.speedLimitKBps * 1024);
      }

      const bytesAdded = Math.round(currentSpeed * 0.5);
      const newDownloaded = Math.min(item.sizeBytes, item.downloadedBytes + bytesAdded);
      const isFinished = newDownloaded >= item.sizeBytes;

      // Update chunks if any
      let updatedChunks = (item.chunks && item.chunks.length > 0) ? [...item.chunks] : this.generateInitialChunks(item.connections || 8, item.sizeBytes);

      const chunkProgressRatio = newDownloaded / item.sizeBytes;
      updatedChunks = updatedChunks.map((chunk, idx) => {
        const targetPercent = Math.min(100, Math.round(chunkProgressRatio * 100 + (idx % 3 === 0 ? 5 : -3)));
        return {
          ...chunk,
          percent: isFinished ? 100 : Math.max(chunk.percent || 0, Math.min(100, targetPercent)),
          active: !isFinished
        };
      });

      return {
        ...item,
        downloadedBytes: newDownloaded,
        speedBytesPerSec: isFinished ? 0 : currentSpeed,
        status: isFinished ? "Complete" : "Downloading",
        chunks: updatedChunks
      };
    });

    this.downloads = updated;
    if (this.listeners.onUpdate) {
      this.listeners.onUpdate(this.downloads);
    }
  }

  generateInitialChunks(numConnections, totalBytes) {
    const chunks = [];
    const chunkSize = Math.floor(totalBytes / numConnections);

    for (let i = 0; i < numConnections; i++) {
      const start = i * chunkSize;
      const end = i === numConnections - 1 ? totalBytes : (i + 1) * chunkSize;
      const startMb = (start / (1024 * 1024)).toFixed(2);
      const endMb = (end / (1024 * 1024)).toFixed(2);

      chunks.push({
        id: i + 1,
        percent: 0,
        range: `${startMb}MB - ${endMb}MB`,
        active: true
      });
    }
    return chunks;
  }

  startDownload(id) {
    this.downloads = this.downloads.map(d => {
      if (d.id === id) {
        return {
          ...d,
          status: "Downloading",
          chunks: (d.chunks && d.chunks.length > 0) ? d.chunks : this.generateInitialChunks(d.connections || 8, d.sizeBytes)
        };
      }
      return d;
    });
    this.startTicker();
    if (this.listeners.onUpdate) this.listeners.onUpdate(this.downloads);
  }

  pauseDownload(id) {
    this.downloads = this.downloads.map(d => {
      if (d.id === id) {
        return {
          ...d,
          status: "Paused",
          speedBytesPerSec: 0,
          chunks: d.chunks ? d.chunks.map(c => ({ ...c, active: false })) : []
        };
      }
      return d;
    });
    if (this.listeners.onUpdate) this.listeners.onUpdate(this.downloads);
  }

  stopAll() {
    this.downloads = this.downloads.map(d => {
      if (d.status === "Downloading") {
        return {
          ...d,
          status: "Paused",
          speedBytesPerSec: 0,
          chunks: d.chunks ? d.chunks.map(c => ({ ...c, active: false })) : []
        };
      }
      return d;
    });
    if (this.listeners.onUpdate) this.listeners.onUpdate(this.downloads);
  }

  resumeAll() {
    this.downloads = this.downloads.map(d => {
      if (d.status === "Paused" && d.downloadedBytes < d.sizeBytes) {
        return { ...d, status: "Downloading" };
      }
      return d;
    });
    this.startTicker();
    if (this.listeners.onUpdate) this.listeners.onUpdate(this.downloads);
  }
}
