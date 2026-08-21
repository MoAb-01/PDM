import React, { useState, useEffect } from "react";
import { parseM3U8, parseMPD } from "../services/hlsDashParser";
import {
  Radio,
  Tv,
  Film,
  Layers,
  ShieldAlert,
  Play,
  Download,
  FileCode,
  CheckCircle,
  ExternalLink,
  Code2,
  Lock,
  Sparkles,
  Zap,
  HelpCircle,
  X
} from "lucide-react";

export default function SnifferStudio({ onClose, onSendToDownloader }) {
  const [activeTab, setActiveTab] = useState("hls"); // "hls" | "dash" | "direct" | "drm" | "extension"
  const [selectedQuality, setSelectedQuality] = useState("1080p Full HD (6000 kbps)");
  const [isSniffing, setIsSniffing] = useState(true);
  const [capturedStreams, setCapturedStreams] = useState([
    {
      id: "sniff-1",
      url: "https://streams.redbull.com/manifest/stark-varg/master.m3u8",
      type: "HLS Manifest (.m3u8)",
      mime: "application/x-mpegURL",
      quality: "1080p 60fps / 720p / 480p",
      site: "redbull.com/motorsports",
      segments: 48,
      sizeEst: "142 MB"
    },
    {
      id: "sniff-2",
      url: "https://vimeo.com/api/v2/video/987654321/master.mpd",
      type: "MPEG-DASH (.mpd)",
      mime: "application/dash+xml",
      quality: "2160p 4K / 1080p / Audio Track",
      site: "vimeo.com/creative-showcase",
      segments: 72,
      sizeEst: "480 MB"
    },
    {
      id: "sniff-3",
      url: "https://cdn.highresaudio.com/lossless/master_soundtrack.flac",
      type: "Direct Audio Stream",
      mime: "audio/flac",
      quality: "24-bit 96kHz Lossless",
      site: "highresaudio.com",
      segments: 1,
      sizeEst: "65.4 MB"
    },
    {
      id: "sniff-4",
      url: "https://protected-streaming-service.com/drm/manifest.mpd",
      type: "DRM Widevine L1 (Encrypted)",
      mime: "application/dash+xml",
      quality: "Encrypted Keys Required",
      site: "netflix.com / disneyplus.com",
      segments: 0,
      sizeEst: "DRM Encrypted",
      isDrm: true
    }
  ]);

  const sampleM3U8Manifest = `#EXTM3U
#EXT-X-VERSION:4
#EXT-X-INDEPENDENT-SEGMENTS
#EXT-X-STREAM-INF:BANDWIDTH=6000000,RESOLUTION=1920x1080,CODECS="avc1.64002a,mp4a.40.2",NAME="1080p"
1080p/index.m3u8
#EXT-X-STREAM-INF:BANDWIDTH=3200000,RESOLUTION=1280x720,CODECS="avc1.4d401f,mp4a.40.2",NAME="720p"
720p/index.m3u8
#EXT-X-STREAM-INF:BANDWIDTH=1400000,RESOLUTION=854x480,CODECS="avc1.4d401e,mp4a.40.2",NAME="480p"
480p/index.m3u8
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio-aac",NAME="English",DEFAULT=YES,URI="audio/aac.m3u8"`;

  const parsedHls = parseM3U8(sampleM3U8Manifest);

  const handleDownloadStream = (stream) => {
    if (stream.isDrm) {
      alert("⚠️ DRM Widevine / FairPlay Boundary Notice:\nThis stream is hardware/browser encrypted by the streaming platform. IDM cannot extract raw unencrypted video without private cryptographic keys.");
      return;
    }

    let cleanFilename = stream.url.split("/").pop().replace(".m3u8", ".mp4").replace(".mpd", ".mp4") || "video_stream.mp4";
    if (!cleanFilename.endsWith(".mp4") && !cleanFilename.endsWith(".flac")) {
      cleanFilename += ".mp4";
    }

    onSendToDownloader({
      id: `dl-sniff-${Date.now()}`,
      filename: cleanFilename,
      category: stream.type.includes("Audio") ? "music" : "video",
      sizeBytes: stream.type.includes("Audio") ? 68000000 : 148000000,
      downloadedBytes: 0,
      status: "Downloading",
      speedBytesPerSec: 15400000,
      savePath: `C:\\Users\\UHD\\Downloads\\Video\\${cleanFilename}`,
      url: stream.url,
      referer: `https://${stream.site}`,
      description: `Auto-sniffed ${stream.type} (${stream.quality})`,
      login: "",
      password: "",
      lastTryDate: new Date().toLocaleDateString("en-US", { month: "short", day: "2-digit", year: "numeric" }),
      mimeType: stream.mime,
      connections: 16,
      chunks: []
    });

    onClose();
  };

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div
        className="idm-dialog dark-modal"
        style={{ width: "820px", maxHeight: "90vh", borderRadius: "6px" }}
        onClick={(e) => e.stopPropagation()}
      >
        {/* Titlebar */}
        <div className="idm-dialog-titlebar">
          <div className="idm-dialog-title-text">
            <Radio size={16} color="#06b6d4" className="animate-pulse" />
            <span style={{ fontWeight: 600, fontSize: "12.5px" }}>
              IDM Media Sniffing & Stream Capture Studio
            </span>
          </div>
          <button className="idm-dialog-close" onClick={onClose}>
            <X size={14} />
          </button>
        </div>

        {/* Tab Controls */}
        <div className="idm-tab-bar" style={{ background: "#1b1b1e", borderBottom: "1px solid #363640" }}>
          <button
            className={`idm-tab-btn ${activeTab === "hls" ? "active" : ""}`}
            onClick={() => setActiveTab("hls")}
          >
            HLS & DASH Streams (.m3u8 / .mpd)
          </button>
          <button
            className={`idm-tab-btn ${activeTab === "floating" ? "active" : ""}`}
            onClick={() => setActiveTab("floating")}
          >
            Live Web Player Floating Widget
          </button>
          <button
            className={`idm-tab-btn ${activeTab === "drm" ? "active" : ""}`}
            onClick={() => setActiveTab("drm")}
          >
            DRM & Platform Boundaries
          </button>
          <button
            className={`idm-tab-btn ${activeTab === "extension" ? "active" : ""}`}
            onClick={() => setActiveTab("extension")}
          >
            Companion Browser Extension (MV3)
          </button>
        </div>

        {/* Body */}
        <div className="idm-dialog-body" style={{ padding: "16px 20px" }}>
          {/* 1. HLS & DASH TAB */}
          {activeTab === "hls" && (
            <div>
              <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: 12 }}>
                <div>
                  <div style={{ fontSize: "13px", fontWeight: "bold", color: "#f3f4f6" }}>
                    Detected Network Media Requests (webRequest & DOM Interceptor)
                  </div>
                  <div style={{ fontSize: "11px", color: "#94a3b8" }}>
                    Captures .m3u8 playlists, MPEG-DASH manifest files, and concurrent chunk segments
                  </div>
                </div>
                <div className="badge-pulse">
                  <span className="pulse-dot" />
                  <span>SNIFFER ACTIVE</span>
                </div>
              </div>

              {/* Stream List */}
              <div style={{ display: "flex", flexDirection: "column", gap: 8, marginBottom: 16 }}>
                {capturedStreams.map((s) => (
                  <div
                    key={s.id}
                    style={{
                      background: s.isDrm ? "rgba(239, 68, 68, 0.08)" : "#1c1c22",
                      border: s.isDrm ? "1px solid rgba(239, 68, 68, 0.3)" : "1px solid #33333e",
                      borderRadius: 4,
                      padding: "10px 14px",
                      display: "flex",
                      alignItems: "center",
                      justifyContent: "space-between"
                    }}
                  >
                    <div style={{ display: "flex", alignItems: "center", gap: 12 }}>
                      <div
                        style={{
                          width: 34,
                          height: 34,
                          borderRadius: 4,
                          background: s.isDrm ? "#ef4444" : "#0284c7",
                          display: "flex",
                          alignItems: "center",
                          justifyContent: "center",
                          color: "#fff"
                        }}
                      >
                        {s.isDrm ? <Lock size={16} /> : <Film size={16} />}
                      </div>
                      <div>
                        <div style={{ fontSize: "12px", fontWeight: "bold", color: s.isDrm ? "#fca5a5" : "#f1f5f9" }}>
                          {s.type} — <span style={{ color: "#38bdf8" }}>{s.quality}</span>
                        </div>
                        <div style={{ fontSize: "10.5px", fontFamily: "var(--font-mono)", color: "#94a3b8" }}>
                          {s.url}
                        </div>
                        <div style={{ fontSize: "10px", color: "#64748b", marginTop: 2 }}>
                          Host: {s.site} • MIME: {s.mime} • Estimated: {s.sizeEst}
                        </div>
                      </div>
                    </div>

                    <button
                      className="idm-btn primary"
                      style={{
                        background: s.isDrm ? "#475569" : "#0284c7",
                        borderColor: s.isDrm ? "#64748b" : "#38bdf8",
                        minWidth: 110
                      }}
                      onClick={() => handleDownloadStream(s)}
                    >
                      {s.isDrm ? "DRM Locked" : <><Download size={13} /> Download</>}
                    </button>
                  </div>
                ))}
              </div>

              {/* Master Playlist Manifest Preview */}
              <div style={{ background: "#111", border: "1px solid #333", borderRadius: 4, padding: 10 }}>
                <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: 6 }}>
                  <span style={{ fontSize: "11px", fontWeight: 600, color: "#38bdf8" }}>
                    Live HLS Master Manifest Inspector (.m3u8)
                  </span>
                  <span style={{ fontSize: "10px", color: "#64748b" }}>Parsed Resolution Variants: {parsedHls.variants.length}</span>
                </div>
                <pre style={{ fontSize: "10.5px", fontFamily: "var(--font-mono)", color: "#a5f3fc", lineHeight: 1.4, margin: 0 }}>
                  {sampleM3U8Manifest}
                </pre>
              </div>
            </div>
          )}

          {/* 2. FLOATING VIDEO PLAYER SIMULATOR */}
          {activeTab === "floating" && (
            <div>
              <div style={{ fontSize: "12.5px", fontWeight: "bold", color: "#f3f4f6", marginBottom: 6 }}>
                Simulated Web Video Player with IDM Floating Download Widget
              </div>
              <div style={{ fontSize: "11px", color: "#94a3b8", marginBottom: 12 }}>
                When watching videos in Chrome, Edge or Firefox, IDM injects a floating download button in the corner of HTML5 & blob players.
              </div>

              {/* Simulated Video Player Container */}
              <div
                style={{
                  position: "relative",
                  width: "100%",
                  height: "260px",
                  background: "#000",
                  borderRadius: 6,
                  overflow: "hidden",
                  border: "1px solid #383844",
                  display: "flex",
                  alignItems: "center",
                  justifyContent: "center"
                }}
              >
                {/* IDM Floating Download Widget */}
                <div
                  style={{
                    position: "absolute",
                    top: 12,
                    right: 12,
                    background: "rgba(15, 23, 42, 0.92)",
                    border: "1px solid #38bdf8",
                    boxShadow: "0 4px 20px rgba(0, 0, 0, 0.7), 0 0 10px rgba(56, 189, 248, 0.3)",
                    borderRadius: 4,
                    padding: "6px 10px",
                    display: "flex",
                    alignItems: "center",
                    gap: 8,
                    zIndex: 10
                  }}
                >
                  <div className="idm-globe-icon" />
                  <span style={{ fontSize: "11px", fontWeight: "bold", color: "#f8fafc" }}>
                    Download this video
                  </span>
                  <select
                    style={{
                      background: "#1e293b",
                      color: "#38bdf8",
                      border: "1px solid #475569",
                      borderRadius: 2,
                      fontSize: "10.5px",
                      padding: "2px 4px"
                    }}
                    value={selectedQuality}
                    onChange={(e) => setSelectedQuality(e.target.value)}
                  >
                    <option>1080p Full HD (6000 kbps)</option>
                    <option>720p HD (3200 kbps)</option>
                    <option>480p SD (1400 kbps)</option>
                    <option>Audio Only (AAC 256kbps)</option>
                  </select>
                  <button
                    className="idm-btn primary"
                    style={{ height: 22, minWidth: 60, fontSize: "10.5px" }}
                    onClick={() => {
                      handleDownloadStream({
                        id: `sniff-sim-${Date.now()}`,
                        url: "https://streams.example.com/live/1080p/index.m3u8",
                        type: "HLS Video Stream",
                        mime: "video/mp4",
                        quality: selectedQuality,
                        site: "youtube.com/stream-demo",
                        isDrm: false
                      });
                    }}
                  >
                    Start
                  </button>
                </div>

                {/* Player Graphics */}
                <div style={{ textAlign: "center", color: "#64748b" }}>
                  <Play size={48} color="#38bdf8" style={{ margin: "0 auto 8px" }} />
                  <div style={{ fontSize: "12px", color: "#e2e8f0" }}>HTML5 Video Stream Player [blob:https://...]</div>
                  <div style={{ fontSize: "10px", color: "#94a3b8" }}>HLS / DASH manifest active • Audio track synchronized</div>
                </div>
              </div>
            </div>
          )}

          {/* 3. DRM & BOUNDARIES TAB */}
          {activeTab === "drm" && (
            <div style={{ display: "flex", flexDirection: "column", gap: 12 }}>
              <div style={{ background: "#2b1810", border: "1px solid #7c2d12", borderRadius: 4, padding: "12px 16px" }}>
                <div style={{ display: "flex", alignItems: "center", gap: 8, color: "#fdba74", fontWeight: "bold", fontSize: "13px" }}>
                  <ShieldAlert size={18} />
                  <span>The DRM Protection Boundary (Widevine, FairPlay, PlayReady)</span>
                </div>
                <p style={{ fontSize: "11.5px", color: "#fed7aa", marginTop: 6, lineHeight: 1.5 }}>
                  Platforms like <strong>Netflix, Spotify, Disney+, and Amazon Prime Video</strong> encrypt the actual video/audio segments using hardware-level or browser Encrypted Media Extensions (EME).
                </p>
                <p style={{ fontSize: "11px", color: "#fb923c", marginTop: 4 }}>
                  Even if IDM or any sniffer downloads the raw .mpd chunks, the resulting file contains scrambled encrypted payload bytes that cannot be played without hardware decryption keys.
                </p>
              </div>

              <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 12 }}>
                <div style={{ background: "#19241e", border: "1px solid #14532d", borderRadius: 4, padding: 12 }}>
                  <div style={{ color: "#4ade80", fontWeight: "bold", fontSize: "12px", marginBottom: 6 }}>
                    ✓ What Can Be Sniffed & Downloaded
                  </div>
                  <ul style={{ fontSize: "11px", color: "#bbf7d0", paddingLeft: 16, lineHeight: 1.6 }}>
                    <li>Direct MP4, WebM, MKV, ZIP, PDF, EXE files</li>
                    <li>Unencrypted HLS (.m3u8) playlists & TS chunks</li>
                    <li>MPEG-DASH (.mpd) stream segments</li>
                    <li>Social media video players & embedded web players</li>
                    <li>Gated files with Session Cookies & Referer headers</li>
                  </ul>
                </div>

                <div style={{ background: "#241818", border: "1px solid #7f1d1d", borderRadius: 4, padding: 12 }}>
                  <div style={{ color: "#f87171", fontWeight: "bold", fontSize: "12px", marginBottom: 6 }}>
                    ✗ What Cannot Be Downloaded
                  </div>
                  <ul style={{ fontSize: "11px", color: "#fecaca", paddingLeft: 16, lineHeight: 1.6 }}>
                    <li>DRM Encrypted content (Widevine L1/L3)</li>
                    <li>Pure WebAssembly / Canvas canvas-painted frames</li>
                    <li>Hardware secure enclave protected streams</li>
                  </ul>
                </div>
              </div>
            </div>
          )}

          {/* 4. EXTENSION TAB */}
          {activeTab === "extension" && (
            <div>
              <div style={{ fontSize: "12.5px", fontWeight: "bold", color: "#f3f4f6", marginBottom: 4 }}>
                Ready-to-Install Chrome / Edge Manifest V3 Companion Extension
              </div>
              <div style={{ fontSize: "11px", color: "#94a3b8", marginBottom: 10 }}>
                This extension package sits in <code style={{ color: "#38bdf8" }}>/src/extension</code> ready to load unpacked into Chrome/Edge at <code>chrome://extensions</code>.
              </div>

              <div style={{ background: "#111", border: "1px solid #333", borderRadius: 4, padding: 12, fontSize: "11px", color: "#e2e8f0" }}>
                <div style={{ fontWeight: "bold", color: "#38bdf8", marginBottom: 6 }}>Architecture Pipeline:</div>
                <div style={{ fontFamily: "var(--font-mono)", fontSize: "10.5px", lineHeight: 1.5, color: "#94a3b8" }}>
                  1. <strong>background.js</strong> listens to <code>chrome.webRequest.onResponseStarted</code> for MIME types matching video/*, application/x-mpegURL, application/dash+xml.<br />
                  2. <strong>content.js</strong> intercepts dynamic <code>window.fetch</code> / <code>XMLHttpRequest</code>.<br />
                  3. Forwards URL + User-Agent + Referer + Cookies to IDM engine over Native Messaging / WebSocket.<br />
                  4. IDM spawns 16 parallel Range request streams and stitches final file.
                </div>
              </div>
            </div>
          )}
        </div>

        {/* Footer */}
        <div className="idm-dialog-footer">
          <button className="idm-btn primary" onClick={onClose}>
            Close Studio
          </button>
        </div>
      </div>
    </div>
  );
}
