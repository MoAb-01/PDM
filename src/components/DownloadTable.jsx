import React, { useState } from "react";
import { formatBytes, formatSpeed } from "../services/storageService";
import {
  FileText,
  FileArchive,
  Film,
  Music,
  AppWindow,
  FileCode,
  FileQuestion,
  ExternalLink,
  Play,
  Square,
  Trash2,
  Sliders,
  FolderOpen,
  Copy
} from "lucide-react";

export default function DownloadTable({
  downloads,
  selectedId,
  onSelectId,
  onOpenProperties,
  onOpenChunks,
  onResumeDownload,
  onPauseDownload,
  onDeleteDownload
}) {
  const [contextMenu, setContextMenu] = useState(null);

  const getFileBadge = (filename = "") => {
    const ext = filename.split(".").pop().toLowerCase();
    switch (ext) {
      case "pdf":
        return <span className="file-icon-badge badge-pdf">PDF</span>;
      case "zip":
        return <span className="file-icon-badge badge-zip">ZIP</span>;
      case "rar":
        return <span className="file-icon-badge badge-rar">RAR</span>;
      case "7z":
        return <span className="file-icon-badge badge-7z">7Z</span>;
      case "exe":
        return <span className="file-icon-badge badge-exe">EXE</span>;
      case "msi":
        return <span className="file-icon-badge badge-exe">MSI</span>;
      case "mp4":
        return <span className="file-icon-badge badge-mp4">MP4</span>;
      case "mkv":
        return <span className="file-icon-badge badge-mp4">MKV</span>;
      case "m3u8":
        return <span className="file-icon-badge badge-mp4">HLS</span>;
      case "mp3":
      case "flac":
      case "wav":
        return <span className="file-icon-badge badge-mp3">{ext.toUpperCase()}</span>;
      default:
        return <span className="file-icon-badge badge-default">{ext.toUpperCase() || "FILE"}</span>;
    }
  };

  const handleContextMenu = (e, item) => {
    e.preventDefault();
    onSelectId(item.id);
    setContextMenu({
      x: e.clientX,
      y: e.clientY,
      item
    });
  };

  const closeContextMenu = () => {
    setContextMenu(null);
  };

  return (
    <div className="idm-table-container" onClick={closeContextMenu}>
      <table className="idm-table">
        <thead>
          <tr>
            <th style={{ width: "32%" }}>File Name</th>
            <th style={{ width: "4%", textAlign: "center" }}>Q</th>
            <th style={{ width: "10%" }}>Size</th>
            <th style={{ width: "16%" }}>Status</th>
            <th style={{ width: "8%" }}>Time left</th>
            <th style={{ width: "10%" }}>Transfer rate</th>
            <th style={{ width: "10%" }}>Last Try Date</th>
            <th style={{ width: "10%" }}>Description</th>
          </tr>
        </thead>
        <tbody>
          {downloads.length === 0 ? (
            <tr>
              <td colSpan={8} style={{ textAlign: "center", padding: "40px", color: "#666" }}>
                No downloads in this category. Click <strong>Add URL</strong> or open <strong>Media Sniffer</strong> to capture files.
              </td>
            </tr>
          ) : (
            downloads.map((item) => {
              const isSelected = item.id === selectedId;
              const percent = Math.min(100, Math.round((item.downloadedBytes / item.sizeBytes) * 100)) || 0;
              const isDownloading = item.status === "Downloading";
              const isComplete = item.status === "Complete";
              const isPaused = item.status === "Paused";

              // Estimated time left
              let timeLeftText = "";
              if (isDownloading && item.speedBytesPerSec > 0) {
                const remainingBytes = item.sizeBytes - item.downloadedBytes;
                const secs = Math.ceil(remainingBytes / item.speedBytesPerSec);
                if (secs < 60) timeLeftText = `${secs} sec`;
                else timeLeftText = `${Math.ceil(secs / 60)} min`;
              }

              return (
                <tr
                  key={item.id}
                  className={isSelected ? "selected" : ""}
                  onClick={(e) => {
                    e.stopPropagation();
                    onSelectId(item.id);
                  }}
                  onDoubleClick={(e) => {
                    e.stopPropagation();
                    if (isDownloading) {
                      onOpenChunks(item);
                    } else {
                      onOpenProperties(item);
                    }
                  }}
                  onContextMenu={(e) => handleContextMenu(e, item)}
                >
                  <td>
                    <div className="idm-file-cell">
                      {getFileBadge(item.filename)}
                      <span title={item.filename}>{item.filename}</span>
                    </div>
                  </td>
                  <td style={{ textAlign: "center", color: "#888" }}>
                    {item.status === "Queued" ? "✓" : ""}
                  </td>
                  <td>{formatBytes(item.sizeBytes)}</td>
                  <td>
                    {isComplete ? (
                      <span style={{ color: "#4ade80", fontWeight: 500 }}>Complete</span>
                    ) : (
                      <div style={{ display: "flex", flexDirection: "column", gap: 3 }}>
                        <div className="table-prog-bar">
                          <div
                            className={`table-prog-fill ${isPaused ? "paused" : ""}`}
                            style={{ width: `${percent}%` }}
                          />
                          <span className="table-prog-text">
                            {isPaused ? `Paused (${percent}%)` : `${percent}%`}
                          </span>
                        </div>
                      </div>
                    )}
                  </td>
                  <td>{timeLeftText}</td>
                  <td>{isDownloading ? formatSpeed(item.speedBytesPerSec) : ""}</td>
                  <td style={{ color: "#9e9eb0" }}>{item.lastTryDate}</td>
                  <td style={{ color: "#9e9eb0" }} title={item.description}>{item.description}</td>
                </tr>
              );
            })
          )}
        </tbody>
      </table>

      {/* Right Click Context Menu */}
      {contextMenu && (
        <div
          className="idm-dropdown-menu"
          style={{
            top: Math.min(contextMenu.y, window.innerHeight - 240),
            left: Math.min(contextMenu.x, window.innerWidth - 200)
          }}
          onClick={(e) => e.stopPropagation()}
        >
          <div
            className="idm-dropdown-entry"
            onClick={() => {
              onOpenProperties(contextMenu.item);
              closeContextMenu();
            }}
          >
            <span>Properties...</span>
            <Sliders size={13} />
          </div>

          <div
            className="idm-dropdown-entry"
            onClick={() => {
              onOpenChunks(contextMenu.item);
              closeContextMenu();
            }}
          >
            <span>View Chunk Threads (1-32)</span>
            <ExternalLink size={13} />
          </div>

          <div className="idm-dropdown-divider" />

          {contextMenu.item.status === "Downloading" ? (
            <div
              className="idm-dropdown-entry"
              onClick={() => {
                onPauseDownload(contextMenu.item.id);
                closeContextMenu();
              }}
            >
              <span>Pause / Stop</span>
              <Square size={13} />
            </div>
          ) : (
            <div
              className="idm-dropdown-entry"
              onClick={() => {
                onResumeDownload(contextMenu.item.id);
                closeContextMenu();
              }}
            >
              <span>Resume Download</span>
              <Play size={13} />
            </div>
          )}

          <div
            className="idm-dropdown-entry"
            onClick={() => {
              navigator.clipboard?.writeText(contextMenu.item.url);
              closeContextMenu();
            }}
          >
            <span>Copy Download Address</span>
            <Copy size={13} />
          </div>

          <div
            className="idm-dropdown-entry"
            onClick={() => {
              alert(`Opening folder: ${contextMenu.item.savePath}`);
              closeContextMenu();
            }}
          >
            <span>Open Destination Folder</span>
            <FolderOpen size={13} />
          </div>

          <div className="idm-dropdown-divider" />

          <div
            className="idm-dropdown-entry"
            style={{ color: "#f87171" }}
            onClick={() => {
              onDeleteDownload(contextMenu.item.id);
              closeContextMenu();
            }}
          >
            <span>Delete from List</span>
            <Trash2 size={13} />
          </div>
        </div>
      )}
    </div>
  );
}
