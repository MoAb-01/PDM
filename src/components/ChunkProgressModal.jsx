import React, { useEffect, useRef } from "react";
import { formatBytes, formatSpeed } from "../services/storageService";
import { X, Play, Square, Activity, Cpu, Layers } from "lucide-react";

export default function ChunkProgressModal({
  item,
  onClose,
  onPause,
  onResume
}) {
  if (!item) return null;

  const isDownloading = item.status === "Downloading";
  const percent = Math.min(100, Math.round((item.downloadedBytes / item.sizeBytes) * 100)) || 0;
  const chunks = item.chunks || [];

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div
        className="idm-dialog dark-modal"
        style={{ width: "640px", borderRadius: "6px" }}
        onClick={(e) => e.stopPropagation()}
      >
        {/* Titlebar */}
        <div className="idm-dialog-titlebar">
          <div className="idm-dialog-title-text">
            <Layers size={15} color="#38bdf8" />
            <span style={{ fontWeight: 600, fontSize: "12px" }}>
              Download Status & Multi-Part Connection Visualizer: {item.filename}
            </span>
          </div>
          <button className="idm-dialog-close" onClick={onClose}>
            <X size={14} />
          </button>
        </div>

        {/* Modal Body */}
        <div className="idm-dialog-body" style={{ padding: "16px 20px" }}>
          {/* Main Info Stats */}
          <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 12, marginBottom: 14 }}>
            <div style={{ background: "#1b1b1e", padding: "8px 12px", borderRadius: 4, border: "1px solid #33333b" }}>
              <div style={{ color: "#8e8e99", fontSize: "10.5px" }}>Downloaded / Total Size</div>
              <div style={{ fontSize: "13px", fontWeight: "bold", color: "#f3f4f6" }}>
                {formatBytes(item.downloadedBytes)} / {formatBytes(item.sizeBytes)} ({percent}%)
              </div>
            </div>

            <div style={{ background: "#1b1b1e", padding: "8px 12px", borderRadius: 4, border: "1px solid #33333b" }}>
              <div style={{ color: "#8e8e99", fontSize: "10.5px" }}>Transfer Rate</div>
              <div style={{ fontSize: "13px", fontWeight: "bold", color: "#38bdf8", display: "flex", alignItems: "center", gap: 6 }}>
                <Activity size={14} className={isDownloading ? "animate-pulse" : ""} />
                {isDownloading ? formatSpeed(item.speedBytesPerSec) : "0 KB/s"}
              </div>
            </div>
          </div>

          {/* Main Progress Bar */}
          <div style={{ marginBottom: 16 }}>
            <div style={{ display: "flex", justifyContent: "space-between", fontSize: "11px", marginBottom: 4, color: "#ccc" }}>
              <span>Total Progress: {percent}%</span>
              <span>Server Resume Support: <strong style={{ color: "#4ade80" }}>Yes (HTTP 206 Partial Content)</strong></span>
            </div>
            <div className="table-prog-bar" style={{ height: 20 }}>
              <div
                className={`table-prog-fill ${item.status === "Paused" ? "paused" : ""}`}
                style={{ width: `${percent}%` }}
              />
              <span className="table-prog-text" style={{ fontSize: "11px" }}>
                {formatBytes(item.downloadedBytes)} of {formatBytes(item.sizeBytes)} ({percent}%)
              </span>
            </div>
          </div>

          {/* IDM's Iconic Segmented Chunk Bar */}
          <div style={{ marginBottom: 16 }}>
            <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: 6 }}>
              <div style={{ display: "flex", alignItems: "center", gap: 6, fontSize: "11.5px", fontWeight: 600, color: "#e2e8f0" }}>
                <Cpu size={14} color="#06b6d4" />
                <span>Concurrent Segment Streams ({chunks.length || 8} Connections)</span>
              </div>
              <span style={{ fontSize: "10.5px", color: "#888" }}>Dynamic Chunk Acceleration</span>
            </div>

            <div className="chunk-strip-container">
              {chunks.map((chunk, idx) => (
                <div key={chunk.id} className="chunk-segment" title={`Part ${chunk.id}: ${chunk.range} (${chunk.percent}%)`}>
                  <div
                    className="chunk-segment-fill"
                    style={{
                      width: `${chunk.percent}%`,
                      background: chunk.percent >= 100
                        ? "linear-gradient(180deg, #22c55e 0%, #16a34a 100%)"
                        : (chunk.active ? "linear-gradient(180deg, #38bdf8 0%, #0284c7 100%)" : "#64748b")
                    }}
                  />
                  <div className="chunk-segment-label">#{chunk.id}</div>
                </div>
              ))}
            </div>
          </div>

          {/* Chunk details table */}
          <div style={{ maxHeight: 150, overflowY: "auto", border: "1px solid #363640", borderRadius: 4 }}>
            <table style={{ width: "100%", borderCollapse: "collapse", fontSize: "10.5px", textAlign: "left" }}>
              <thead>
                <tr style={{ background: "#1f1f24", color: "#a0a0ab", borderBottom: "1px solid #363640" }}>
                  <th style={{ padding: "4px 8px" }}>Conn #</th>
                  <th style={{ padding: "4px 8px" }}>Byte Range</th>
                  <th style={{ padding: "4px 8px" }}>Progress</th>
                  <th style={{ padding: "4px 8px" }}>Status</th>
                </tr>
              </thead>
              <tbody>
                {chunks.map((c) => (
                  <tr key={c.id} style={{ borderBottom: "1px solid #282830" }}>
                    <td style={{ padding: "4px 8px", fontWeight: "bold" }}>Thread #{c.id}</td>
                    <td style={{ padding: "4px 8px", fontFamily: "var(--font-mono)", color: "#94a3b8" }}>{c.range}</td>
                    <td style={{ padding: "4px 8px" }}>
                      <div style={{ display: "flex", alignItems: "center", gap: 6 }}>
                        <div style={{ width: 60, height: 6, background: "#111", borderRadius: 2, overflow: "hidden" }}>
                          <div style={{ width: `${c.percent}%`, height: "100%", background: "#38bdf8" }} />
                        </div>
                        <span>{c.percent}%</span>
                      </div>
                    </td>
                    <td style={{ padding: "4px 8px" }}>
                      {c.percent >= 100 ? (
                        <span style={{ color: "#4ade80" }}>Merged</span>
                      ) : isDownloading ? (
                        <span style={{ color: "#38bdf8" }}>Receiving chunks</span>
                      ) : (
                        <span style={{ color: "#f59e0b" }}>Idle</span>
                      )}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>

        {/* Footer */}
        <div className="idm-dialog-footer">
          {isDownloading ? (
            <button className="idm-btn" onClick={() => onPause(item.id)}>
              <Square size={12} /> Pause
            </button>
          ) : (
            <button className="idm-btn primary" onClick={() => onResume(item.id)}>
              <Play size={12} /> Resume
            </button>
          )}
          <button className="idm-btn" onClick={onClose}>
            Close
          </button>
        </div>
      </div>
    </div>
  );
}
