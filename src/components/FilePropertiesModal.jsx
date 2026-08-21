import React, { useState } from "react";
import { formatBytes } from "../services/storageService";
import { Globe, X, FolderOpen, Play } from "lucide-react";

export default function FilePropertiesModal({ item, onClose, onSave, onOpenFile }) {
  if (!item) return null;

  const [saveTo, setSaveTo] = useState(item.savePath || "");
  const [address, setAddress] = useState(item.url || "");
  const [description, setDescription] = useState(item.description || "");
  const [referer, setReferer] = useState(item.referer || "");
  const [login, setLogin] = useState(item.login || "");
  const [password, setPassword] = useState(item.password || "");

  const handleSave = () => {
    onSave({
      ...item,
      savePath: saveTo,
      url: address,
      description,
      referer,
      login,
      password
    });
    onClose();
  };

  const getTypeName = (filename = "") => {
    const ext = filename.split(".").pop().toLowerCase();
    if (["exe", "msi"].includes(ext)) return "Application";
    if (["zip", "rar", "7z", "tar", "gz"].includes(ext)) return "Compressed Archive";
    if (["mp4", "mkv", "webm", "m3u8"].includes(ext)) return "Video / Media Stream";
    if (["mp3", "wav", "flac"].includes(ext)) return "Audio File";
    if (["pdf", "doc", "docx", "txt"].includes(ext)) return "Document";
    return "Binary File";
  };

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div
        className="idm-dialog"
        style={{ width: "520px", borderRadius: "4px" }}
        onClick={(e) => e.stopPropagation()}
      >
        {/* Title Bar */}
        <div className="idm-dialog-titlebar">
          <div className="idm-dialog-title-text">
            <div className="idm-globe-icon" />
            <span style={{ fontWeight: 600, fontSize: "12px" }}>File Properties</span>
          </div>
          <button className="idm-dialog-close" onClick={onClose}>
            <X size={14} />
          </button>
        </div>

        {/* Content Body */}
        <div className="idm-dialog-body" style={{ padding: "16px 20px" }}>
          {/* File Header Row */}
          <div style={{ display: "flex", alignItems: "center", gap: 12, marginBottom: 14 }}>
            <div
              style={{
                width: 36,
                height: 36,
                border: "1px solid #999",
                background: "#f9f9f9",
                display: "flex",
                alignItems: "center",
                justifyContent: "center"
              }}
            >
              <div
                style={{
                  width: 24,
                  height: 24,
                  background: "#0284c7",
                  borderRadius: 2,
                  display: "flex",
                  alignItems: "center",
                  justifyContent: "center",
                  color: "#fff",
                  fontSize: "10px",
                  fontWeight: "bold"
                }}
              >
                IDM
              </div>
            </div>
            <div style={{ fontSize: "12.5px", fontWeight: "bold", color: "#111" }}>
              {item.filename}
            </div>
          </div>

          <hr style={{ borderColor: "#d8d8d8", marginBottom: 12, borderWidth: "0.5px" }} />

          {/* Properties Grid */}
          <div className="prop-row">
            <div className="prop-label">Type:</div>
            <div className="prop-value" style={{ fontWeight: 500 }}>
              {getTypeName(item.filename)}
            </div>
          </div>

          <div className="prop-row">
            <div className="prop-label">Status:</div>
            <div className="prop-value" style={{ color: item.status === "Complete" ? "#16a34a" : "#0284c7", fontWeight: 600 }}>
              {item.status}
            </div>
          </div>

          <div className="prop-row">
            <div className="prop-label">Size:</div>
            <div className="prop-value">
              {formatBytes(item.sizeBytes)} ({item.sizeBytes.toLocaleString()} Bytes)
            </div>
          </div>

          <div className="prop-row" style={{ marginTop: 8 }}>
            <div className="prop-label">Save To:</div>
            <input
              type="text"
              className="prop-input"
              value={saveTo}
              onChange={(e) => setSaveTo(e.target.value)}
            />
            <button
              className="idm-btn"
              style={{ marginLeft: 8, minWidth: 65 }}
              onClick={() => alert(`Browse target folder for ${item.filename}`)}
            >
              Move
            </button>
          </div>

          <div className="prop-row">
            <div className="prop-label">Address:</div>
            <input
              type="text"
              className="prop-input"
              value={address}
              onChange={(e) => setAddress(e.target.value)}
            />
          </div>

          <div className="prop-row">
            <div className="prop-label">Description:</div>
            <input
              type="text"
              className="prop-input"
              value={description}
              onChange={(e) => setDescription(e.target.value)}
            />
          </div>

          <hr style={{ borderColor: "#d8d8d8", margin: "14px 0 10px 0", borderWidth: "0.5px" }} />

          <div style={{ fontSize: "11px", color: "#444", marginBottom: 4 }}>
            The web page from which this file was obtained:
          </div>
          <a
            href={item.referer || "#"}
            target="_blank"
            rel="noreferrer"
            style={{
              color: "#0066cc",
              textDecoration: "underline",
              fontSize: "12px",
              display: "block",
              marginBottom: 12,
              wordBreak: "break-all"
            }}
          >
            {item.referer || "Direct URL / No Referer"}
          </a>

          <div className="prop-row">
            <div className="prop-label">Referer:</div>
            <input
              type="text"
              className="prop-input"
              value={referer}
              onChange={(e) => setReferer(e.target.value)}
            />
          </div>

          <div className="prop-row">
            <div className="prop-label">Login</div>
            <input
              type="text"
              className="prop-input"
              style={{ maxWidth: 220 }}
              value={login}
              onChange={(e) => setLogin(e.target.value)}
              placeholder="Optional"
            />
          </div>

          <div className="prop-row">
            <div className="prop-label">Password</div>
            <input
              type="password"
              className="prop-input"
              style={{ maxWidth: 220 }}
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              placeholder="••••••••"
            />
          </div>
        </div>

        {/* Footer Buttons */}
        <div className="idm-dialog-footer">
          <button
            className="idm-btn primary"
            style={{ minWidth: 75 }}
            onClick={() => onOpenFile(item)}
          >
            Open
          </button>
          <button className="idm-btn" style={{ minWidth: 75 }} onClick={handleSave}>
            OK
          </button>
          <button className="idm-btn" style={{ minWidth: 75 }} onClick={onClose}>
            Cancel
          </button>
        </div>
      </div>
    </div>
  );
}
