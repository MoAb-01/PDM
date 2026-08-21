import React, { useState } from "react";
import { X, Link2, Download, Clock, ShieldCheck, ChevronDown, ChevronUp } from "lucide-react";

export default function AddUrlModal({ categories, onClose, onAddDownload }) {
  const [url, setUrl] = useState("");
  const [category, setCategory] = useState("general");
  const [description, setDescription] = useState("");
  const [referer, setReferer] = useState("");
  const [login, setLogin] = useState("");
  const [password, setPassword] = useState("");
  const [showAdvanced, setShowAdvanced] = useState(false);

  const handleStart = (startNow = true) => {
    if (!url.trim()) {
      alert("Please enter a valid download URL.");
      return;
    }

    // Extract filename
    let filename = "downloaded_file";
    try {
      const parsed = new URL(url);
      const pathname = parsed.pathname;
      const lastPart = pathname.split("/").pop();
      if (lastPart && lastPart.includes(".")) {
        filename = decodeURIComponent(lastPart);
      } else {
        filename = `media_stream_${Date.now()}.mp4`;
      }
    } catch (e) {
      filename = `file_${Date.now()}.bin`;
    }

    // Determine category automatically if general
    let targetCategory = category;
    if (category === "general") {
      const ext = filename.split(".").pop().toLowerCase();
      if (["mp4", "mkv", "webm", "m3u8", "mpd", "ts"].includes(ext)) targetCategory = "video";
      else if (["zip", "rar", "7z", "tar", "gz", "iso"].includes(ext)) targetCategory = "compressed";
      else if (["exe", "msi", "apk"].includes(ext)) targetCategory = "programs";
      else if (["pdf", "doc", "docx", "ppt"].includes(ext)) targetCategory = "documents";
      else if (["mp3", "flac", "wav"].includes(ext)) targetCategory = "music";
    }

    const newDownload = {
      id: `dl-${Date.now()}`,
      filename,
      category: targetCategory,
      sizeBytes: 45000000 + Math.floor(Math.random() * 80000000), // realistic simulated size
      downloadedBytes: 0,
      status: startNow ? "Downloading" : "Queued",
      speedBytesPerSec: startNow ? 12000000 : 0,
      savePath: `C:\\Users\\UHD\\Downloads\\${targetCategory}\\${filename}`,
      url: url.trim(),
      referer: referer.trim(),
      description: description.trim() || `Captured via IDM Engine (${filename})`,
      login: login.trim(),
      password: password.trim(),
      lastTryDate: new Date().toLocaleDateString("en-US", { month: "short", day: "2-digit", year: "numeric" }) + " " + new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }),
      mimeType: "application/octet-stream",
      connections: 16,
      chunks: []
    };

    onAddDownload(newDownload);
    onClose();
  };

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div
        className="idm-dialog"
        style={{ width: "490px", borderRadius: "4px" }}
        onClick={(e) => e.stopPropagation()}
      >
        {/* Titlebar */}
        <div className="idm-dialog-titlebar">
          <div className="idm-dialog-title-text">
            <div className="idm-globe-icon" />
            <span style={{ fontWeight: 600, fontSize: "12px" }}>Enter new address to download</span>
          </div>
          <button className="idm-dialog-close" onClick={onClose}>
            <X size={14} />
          </button>
        </div>

        {/* Body */}
        <div className="idm-dialog-body" style={{ padding: "16px 20px" }}>
          <div style={{ display: "flex", alignItems: "center", gap: 6, marginBottom: 6, fontSize: "11.5px", fontWeight: 600 }}>
            <Link2 size={15} color="#0284c7" />
            <span>Address (URL):</span>
          </div>
          <input
            type="text"
            className="prop-input"
            style={{ width: "100%", height: 26, marginBottom: 12 }}
            placeholder="https://example.com/files/archive.zip or .m3u8 stream"
            value={url}
            autoFocus
            onChange={(e) => setUrl(e.target.value)}
          />

          <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 12, marginBottom: 12 }}>
            <div>
              <div style={{ fontSize: "11px", marginBottom: 4 }}>Category:</div>
              <select
                style={{ width: "100%", height: 26, padding: "0 6px" }}
                value={category}
                onChange={(e) => setCategory(e.target.value)}
              >
                {categories.map((c) => (
                  <option key={c.id} value={c.id}>{c.name}</option>
                ))}
              </select>
            </div>

            <div>
              <div style={{ fontSize: "11px", marginBottom: 4 }}>Description:</div>
              <input
                type="text"
                className="prop-input"
                style={{ width: "100%" }}
                placeholder="Optional notes"
                value={description}
                onChange={(e) => setDescription(e.target.value)}
              />
            </div>
          </div>

          {/* Collapsible Auth & Referer Section */}
          <div style={{ marginTop: 6, marginBottom: 10 }}>
            <button
              type="button"
              onClick={() => setShowAdvanced(!showAdvanced)}
              style={{
                background: "none",
                border: "none",
                color: "#0066cc",
                cursor: "pointer",
                fontSize: "11px",
                display: "flex",
                alignItems: "center",
                gap: 4,
                padding: 0
              }}
            >
              {showAdvanced ? <ChevronUp size={13} /> : <ChevronDown size={13} />}
              <span>{showAdvanced ? "Hide" : "Show"} Authorization, Cookies & Referer</span>
            </button>

            {showAdvanced && (
              <div style={{ background: "#f8f8f8", border: "1px solid #ddd", padding: 10, borderRadius: 3, marginTop: 8 }}>
                <div style={{ marginBottom: 8 }}>
                  <div style={{ fontSize: "10.5px", color: "#555", marginBottom: 2 }}>Referer Header (for hotlink-protected CDNs):</div>
                  <input
                    type="text"
                    className="prop-input"
                    style={{ width: "100%" }}
                    placeholder="https://original-site.com/player"
                    value={referer}
                    onChange={(e) => setReferer(e.target.value)}
                  />
                </div>

                <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 8 }}>
                  <div>
                    <div style={{ fontSize: "10.5px", color: "#555", marginBottom: 2 }}>Username / Login:</div>
                    <input
                      type="text"
                      className="prop-input"
                      style={{ width: "100%" }}
                      placeholder="user@host.com"
                      value={login}
                      onChange={(e) => setLogin(e.target.value)}
                    />
                  </div>
                  <div>
                    <div style={{ fontSize: "10.5px", color: "#555", marginBottom: 2 }}>Password:</div>
                    <input
                      type="password"
                      className="prop-input"
                      style={{ width: "100%" }}
                      placeholder="••••••••"
                      value={password}
                      onChange={(e) => setPassword(e.target.value)}
                    />
                  </div>
                </div>
              </div>
            )}
          </div>
        </div>

        {/* Footer */}
        <div className="idm-dialog-footer">
          <button
            className="idm-btn primary"
            onClick={() => handleStart(true)}
          >
            <Download size={13} /> Download Now
          </button>

          <button
            className="idm-btn"
            onClick={() => handleStart(false)}
          >
            <Clock size={13} /> Download Later
          </button>

          <button className="idm-btn" onClick={onClose}>
            Cancel
          </button>
        </div>
      </div>
    </div>
  );
}
