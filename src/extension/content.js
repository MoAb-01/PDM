// IDM Companion Content Script for YouTube, Shorts, and all HTML5 Video Players
// Automatically detects active video players and injects an ultra-compact floating "Download" pill
// that opens a sleek glassmorphism dropdown popover with available qualities and formats.

let latestPagePlayerData = null;
window.addEventListener("IDM_PAGE_PLAYER_DATA", (e) => {
  if (e && e.detail) {
    latestPagePlayerData = e.detail;
  }
});

function requestPagePlayerInfo() {
  window.dispatchEvent(new CustomEvent("IDM_QUERY_PAGE_PLAYER"));
}

// Inject CSS styles for the floating pill and dropdown popover
const style = document.createElement("style");
style.textContent = `
  .idm-video-container-wrapper {
    position: absolute !important;
    top: 12px !important;
    right: 14px !important;
    z-index: 2147483647 !important;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif !important;
    user-select: none !important;
    direction: ltr !important;
    transition: opacity 0.2s ease, transform 0.2s ease !important;
  }

  .idm-video-container-wrapper.idm-dismissed {
    display: none !important;
  }

  /* Ultra-Compact Floating Pill */
  .idm-pill-btn {
    display: inline-flex !important;
    align-items: center !important;
    height: 28px !important;
    padding: 0 4px 0 8px !important;
    background: rgba(15, 23, 42, 0.88) !important;
    backdrop-filter: blur(16px) saturate(180%) !important;
    -webkit-backdrop-filter: blur(16px) saturate(180%) !important;
    color: #f1f5f9 !important;
    border: 1px solid rgba(56, 189, 248, 0.35) !important;
    border-radius: 9999px !important;
    box-shadow: 0 4px 16px rgba(0, 0, 0, 0.6), 0 0 8px rgba(2, 132, 199, 0.25) !important;
    font-size: 11.5px !important;
    font-weight: 600 !important;
    letter-spacing: 0.2px !important;
    cursor: pointer !important;
    transition: all 0.2s cubic-bezier(0.16, 1, 0.3, 1) !important;
    outline: none !important;
  }

  .idm-pill-btn:hover {
    background: rgba(15, 23, 42, 0.96) !important;
    border-color: #38bdf8 !important;
    box-shadow: 0 6px 20px rgba(0, 0, 0, 0.75), 0 0 14px rgba(56, 189, 248, 0.5) !important;
    transform: translateY(-1px) scale(1.02) !important;
  }

  .idm-pill-main {
    display: inline-flex !important;
    align-items: center !important;
    gap: 5px !important;
    padding: 2px 2px 2px 0 !important;
    cursor: pointer !important;
  }

  .idm-pill-btn.idm-active {
    border-color: #38bdf8 !important;
    background: rgba(2, 132, 199, 0.25) !important;
    box-shadow: 0 0 12px rgba(56, 189, 248, 0.6) !important;
  }

  .idm-pill-btn.idm-success {
    border-color: #10b981 !important;
    background: rgba(16, 185, 129, 0.22) !important;
    color: #34d399 !important;
    box-shadow: 0 0 12px rgba(16, 185, 129, 0.5) !important;
  }

  .idm-pill-icon {
    display: flex !important;
    align-items: center !important;
    justify-content: center !important;
    color: #38bdf8 !important;
    flex-shrink: 0 !important;
  }

  .idm-pill-chevron {
    transition: transform 0.2s cubic-bezier(0.16, 1, 0.3, 1) !important;
    color: #94a3b8 !important;
    margin-left: 2px !important;
  }

  .idm-pill-btn.idm-active .idm-pill-chevron {
    transform: rotate(180deg) !important;
    color: #38bdf8 !important;
  }

  /* Close / Dismiss buttons */
  .idm-pill-divider {
    width: 1px !important;
    height: 13px !important;
    background: rgba(255, 255, 255, 0.18) !important;
    margin: 0 2px 0 3px !important;
    flex-shrink: 0 !important;
  }

  .idm-pill-close {
    display: inline-flex !important;
    align-items: center !important;
    justify-content: center !important;
    width: 18px !important;
    height: 18px !important;
    border-radius: 50% !important;
    color: #94a3b8 !important;
    background: transparent !important;
    border: none !important;
    padding: 0 !important;
    cursor: pointer !important;
    transition: all 0.15s ease !important;
    flex-shrink: 0 !important;
  }

  .idm-pill-close:hover {
    background: rgba(239, 68, 68, 0.25) !important;
    color: #f87171 !important;
    transform: scale(1.1) !important;
  }

  .idm-popover-close {
    display: inline-flex !important;
    align-items: center !important;
    justify-content: center !important;
    width: 18px !important;
    height: 18px !important;
    border-radius: 4px !important;
    color: #94a3b8 !important;
    background: transparent !important;
    border: none !important;
    padding: 0 !important;
    cursor: pointer !important;
    transition: all 0.15s ease !important;
  }

  .idm-popover-close:hover {
    background: rgba(255, 255, 255, 0.12) !important;
    color: #f1f5f9 !important;
  }

  /* Glassmorphism Dropdown Popover */
  .idm-dropdown-popover {
    position: absolute !important;
    top: calc(100% + 7px) !important;
    right: 0 !important;
    width: 240px !important;
    background: rgba(13, 17, 23, 0.95) !important;
    backdrop-filter: blur(20px) saturate(190%) !important;
    -webkit-backdrop-filter: blur(20px) saturate(190%) !important;
    border: 1px solid rgba(255, 255, 255, 0.12) !important;
    border-radius: 12px !important;
    box-shadow: 0 14px 36px rgba(0, 0, 0, 0.85), 0 0 1px rgba(255, 255, 255, 0.2) !important;
    padding: 8px !important;
    box-sizing: border-box !important;
    opacity: 0 !important;
    transform: translateY(-6px) scale(0.97) !important;
    pointer-events: none !important;
    transition: opacity 0.18s cubic-bezier(0.16, 1, 0.3, 1), transform 0.18s cubic-bezier(0.16, 1, 0.3, 1) !important;
    z-index: 2147483647 !important;
  }

  .idm-dropdown-popover.idm-open {
    opacity: 1 !important;
    transform: translateY(0) scale(1) !important;
    pointer-events: auto !important;
  }

  /* Dropdown Header */
  .idm-popover-header {
    display: flex !important;
    align-items: center !important;
    justify-content: space-between !important;
    padding: 4px 6px 8px 6px !important;
    border-bottom: 1px solid rgba(255, 255, 255, 0.08) !important;
    margin-bottom: 6px !important;
  }

  .idm-popover-title-row {
    display: flex !important;
    align-items: center !important;
    gap: 6px !important;
  }

  .idm-popover-title {
    font-size: 11px !important;
    font-weight: 700 !important;
    color: #e2e8f0 !important;
    text-transform: uppercase !important;
    letter-spacing: 0.5px !important;
  }

  .idm-popover-badge {
    font-size: 9.5px !important;
    padding: 2px 6px !important;
    background: rgba(56, 189, 248, 0.15) !important;
    color: #38bdf8 !important;
    border-radius: 4px !important;
    font-weight: 600 !important;
  }

  /* Section Titles */
  .idm-section-label {
    font-size: 9.5px !important;
    font-weight: 700 !important;
    color: #64748b !important;
    text-transform: uppercase !important;
    letter-spacing: 0.5px !important;
    padding: 4px 8px 2px 8px !important;
  }

  /* Dropdown Options List */
  .idm-stream-list {
    display: flex !important;
    flex-direction: column !important;
    gap: 3px !important;
  }

  .idm-stream-item {
    display: flex !important;
    align-items: center !important;
    justify-content: space-between !important;
    padding: 6px 8px !important;
    border-radius: 6px !important;
    cursor: pointer !important;
    transition: all 0.12s ease !important;
    background: transparent !important;
    border: none !important;
    width: 100% !important;
    text-align: left !important;
    box-sizing: border-box !important;
  }

  .idm-stream-item:hover {
    background: rgba(56, 189, 248, 0.12) !important;
  }

  .idm-stream-left {
    display: flex !important;
    align-items: center !important;
    gap: 8px !important;
  }

  .idm-stream-icon {
    color: #94a3b8 !important;
    display: flex !important;
    align-items: center !important;
  }

  .idm-stream-item:hover .idm-stream-icon {
    color: #38bdf8 !important;
  }

  .idm-stream-details {
    display: flex !important;
    flex-direction: column !important;
  }

  .idm-stream-name {
    font-size: 11.5px !important;
    font-weight: 600 !important;
    color: #f1f5f9 !important;
    line-height: 1.2 !important;
  }

  .idm-stream-sub {
    font-size: 10px !important;
    color: #94a3b8 !important;
    margin-top: 1px !important;
  }

  .idm-stream-right {
    display: flex !important;
    align-items: center !important;
    gap: 6px !important;
  }

  .idm-stream-size {
    font-size: 10px !important;
    font-weight: 600 !important;
    color: #94a3b8 !important;
    letter-spacing: 0.2px !important;
    background: rgba(255, 255, 255, 0.06) !important;
    padding: 2px 5px !important;
    border-radius: 4px !important;
    border: 1px solid rgba(255, 255, 255, 0.06) !important;
  }

  .idm-stream-item:hover .idm-stream-size {
    color: #cbd5e1 !important;
    border-color: rgba(255, 255, 255, 0.12) !important;
  }

  .idm-quality-tag {
    font-size: 10px !important;
    font-weight: 700 !important;
    padding: 2px 6px !important;
    border-radius: 4px !important;
    letter-spacing: 0.3px !important;
  }

  .idm-tag-1080 {
    background: rgba(56, 189, 248, 0.2) !important;
    color: #38bdf8 !important;
    border: 1px solid rgba(56, 189, 248, 0.35) !important;
  }

  .idm-tag-720 {
    background: rgba(14, 165, 233, 0.15) !important;
    color: #7dd3fc !important;
  }

  .idm-tag-480 {
    background: rgba(100, 116, 139, 0.2) !important;
    color: #cbd5e1 !important;
  }

  .idm-tag-audio {
    background: rgba(168, 85, 247, 0.2) !important;
    color: #c084fc !important;
    border: 1px solid rgba(168, 85, 247, 0.35) !important;
  }

  .idm-tag-m4a {
    background: rgba(16, 185, 129, 0.2) !important;
    color: #34d399 !important;
`;
if (!document.getElementById("idm-injected-styles")) {
  style.id = "idm-injected-styles";
  (document.head || document.documentElement || document.body).appendChild(style);
}

// SVG Icons
const ICON_DOWNLOAD = `<svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2 2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>`;
const ICON_CHEVRON = `<svg class="idm-pill-chevron" width="10" height="10" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><polyline points="6 9 12 15 18 9"/></svg>`;
const ICON_CLOSE = `<svg width="10" height="10" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="18" x2="18" y2="6"/></svg>`;
const ICON_VIDEO = `<svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polygon points="23 7 16 12 23 17 23 7"/><rect x="1" y="5" width="15" height="14" rx="2" ry="2"/></svg>`;
const ICON_MUSIC = `<svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M9 18V5l12-2v13"/><circle cx="6" cy="18" r="3"/><circle cx="18" cy="16" r="3"/></svg>`;
const ICON_CHECK = `<svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"><polyline points="20 6 9 17 4 12"/></svg>`;

// Qualities and formats catalogue
const STREAM_OPTIONS = [
  { section: "video", name: "1080p Full HD", sub: "MP4 • H.264 Video", tag: "1080p", tagClass: "idm-tag-1080", isAudio: false },
  { section: "video", name: "720p HD", sub: "MP4 • H.264 Video", tag: "720p", tagClass: "idm-tag-720", isAudio: false },
  { section: "video", name: "480p SD", sub: "MP4 • Fast Stream", tag: "480p", tagClass: "idm-tag-480", isAudio: false },
  { section: "audio", name: "Audio Track (MP3)", sub: "MP3 • 320 kbps High Quality", tag: "MP3", tagClass: "idm-tag-audio", isAudio: true },
  { section: "audio", name: "Audio Track (M4A)", sub: "M4A • AAC Stream", tag: "M4A", tagClass: "idm-tag-m4a", isAudio: true }
];

function attachWidgetToPlayer(playerContainer, videoEl) {
  if (!playerContainer || !videoEl) return;

  // Prevent multiple widgets on same video element
  if (videoEl.__idmDismissed || videoEl.getAttribute("data-idm-dismissed") === "true") return;
  if (videoEl.__idmInjected || videoEl.getAttribute("data-idm-injected") === "true") return;

  // Check if container or its hierarchy already has a widget
  const rootPlayer = videoEl.closest("ytd-reel-video-renderer, ytd-shorts, #movie_player, .html5-video-player, .player-container") || playerContainer;
  if (rootPlayer.querySelector(".idm-video-container-wrapper") || playerContainer.querySelector(".idm-video-container-wrapper")) return;

  // Mark video as injected
  videoEl.__idmInjected = true;
  videoEl.setAttribute("data-idm-injected", "true");

  // Make sure player container has relative positioning
  const computedStyle = window.getComputedStyle(playerContainer);
  if (computedStyle.position === "static") {
    playerContainer.style.position = "relative";
  }

  const wrapper = document.createElement("div");
  wrapper.className = "idm-video-container-wrapper";

  const logoUrl = (typeof chrome !== "undefined" && chrome.runtime && chrome.runtime.getURL)
    ? chrome.runtime.getURL("icon16.png")
    : "";

  // 1. Ultra-Compact Pill with Close Button
  const pill = document.createElement("div");
  pill.className = "idm-pill-btn";
  pill.innerHTML = `
    <div class="idm-pill-main">
      ${logoUrl ? `<img src="${logoUrl}" width="14" height="14" style="border-radius: 3px; flex-shrink: 0;" alt="PDM">` : `<span class="idm-pill-icon">${ICON_DOWNLOAD}</span>`}
      <span class="idm-pill-label">Download</span>
      ${ICON_CHEVRON}
    </div>
    <span class="idm-pill-divider"></span>
    <button class="idm-pill-close" type="button" title="Dismiss">${ICON_CLOSE}</button>
  `;

  // 2. Dropdown Popover with Header Close Button
  const popover = document.createElement("div");
  popover.className = "idm-dropdown-popover";

  let popoverHtml = `
    <div class="idm-popover-header">
      <div class="idm-popover-title-row">
        ${logoUrl ? `<img src="${logoUrl}" width="14" height="14" style="border-radius: 3px;" alt="PDM">` : ""}
        <span class="idm-popover-title">Available Formats</span>
      </div>
      <button class="idm-popover-close" type="button" title="Close Menu">${ICON_CLOSE}</button>
    </div>
    <div class="idm-section-label">Video Streams</div>
    <div class="idm-stream-list">
  `;

  let lastSection = "video";
  STREAM_OPTIONS.forEach(opt => {
    if (opt.section !== lastSection) {
      popoverHtml += `</div><div class="idm-section-label" style="margin-top: 6px;">Audio Only</div><div class="idm-stream-list">`;
      lastSection = opt.section;
    }

    popoverHtml += `
      <div class="idm-stream-item" data-quality="${opt.name}" data-audio="${opt.isAudio}">
        <div class="idm-stream-left">
          <div class="idm-stream-icon">${opt.isAudio ? ICON_MUSIC : ICON_VIDEO}</div>
          <div class="idm-stream-details">
            <span class="idm-stream-name">${opt.name}</span>
            <span class="idm-stream-sub">${opt.sub}</span>
          </div>
        </div>
        <div class="idm-stream-right">
          <span class="idm-quality-tag ${opt.tagClass}">${opt.tag}</span>
        </div>
      </div>
    `;
  });

  popoverHtml += `</div>`;
  popover.innerHTML = popoverHtml;

  wrapper.appendChild(pill);
  wrapper.appendChild(popover);
  playerContainer.appendChild(wrapper);

  const pillMain = pill.querySelector(".idm-pill-main");
  const pillCloseBtn = pill.querySelector(".idm-pill-close");
  const popoverCloseBtn = popover.querySelector(".idm-popover-close");

  // Prevent video player events (play/pause, seek) when interacting with widget
  wrapper.addEventListener("click", (e) => e.stopPropagation());
  wrapper.addEventListener("mousedown", (e) => e.stopPropagation());
  wrapper.addEventListener("dblclick", (e) => e.stopPropagation());
  wrapper.addEventListener("mouseenter", () => requestPagePlayerInfo());

  // Toggle Dropdown when clicking main pill
  pillMain.addEventListener("click", (e) => {
    e.preventDefault();
    e.stopPropagation();
    requestPagePlayerInfo();

    const isOpen = popover.classList.contains("idm-open");
    if (isOpen) {
      popover.classList.remove("idm-open");
      pill.classList.remove("idm-active");
    } else {
      popover.classList.add("idm-open");
      pill.classList.add("idm-active");
    }
  });

  // Dismiss widget completely when clicking the close button on pill
  pillCloseBtn.addEventListener("click", (e) => {
    e.preventDefault();
    e.stopPropagation();

    wrapper.classList.add("idm-dismissed");
    wrapper.remove(); // Completely remove from DOM
    if (videoEl) {
      videoEl.__idmDismissed = true;
      videoEl.__idmInjected = false;
      videoEl.setAttribute("data-idm-dismissed", "true");
    }
    playerContainer.setAttribute("data-idm-dismissed", "true");

    // Clean up any remaining wrappers in the same player tree
    const root = videoEl ? (videoEl.closest("ytd-reel-video-renderer, ytd-shorts, #movie_player, .html5-video-player, .player-container") || playerContainer) : playerContainer;
    const existing = root.querySelectorAll(".idm-video-container-wrapper");
    existing.forEach(w => w.remove());
  });

  // Close only dropdown when clicking popover close button
  if (popoverCloseBtn) {
    popoverCloseBtn.addEventListener("click", (e) => {
      e.preventDefault();
      e.stopPropagation();

      popover.classList.remove("idm-open");
      pill.classList.remove("idm-active");
    });
  }

  // Handle format item click
  const streamItems = popover.querySelectorAll(".idm-stream-item");
  streamItems.forEach(item => {
    item.addEventListener("click", (e) => {
      e.preventDefault();
      e.stopPropagation();

      const selectedQuality = item.getAttribute("data-quality");
      const isAudio = item.getAttribute("data-audio") === "true";

      // Close dropdown
      popover.classList.remove("idm-open");
      pill.classList.remove("idm-active");

      // Visual feedback on the pill
      const labelEl = pillMain.querySelector(".idm-pill-label");
      const originalLabel = labelEl ? labelEl.textContent : "Download";
      pill.classList.add("idm-success");
      pillMain.innerHTML = `
        <span class="idm-pill-icon" style="color:#34d399;">${ICON_CHECK}</span>
        <span class="idm-pill-label">Sent to IDM</span>
      `;

      setTimeout(() => {
        pill.classList.remove("idm-success");
        pillMain.innerHTML = `
          <span class="idm-pill-icon">${ICON_DOWNLOAD}</span>
          <span class="idm-pill-label">${originalLabel}</span>
          ${ICON_CHEVRON}
        `;
      }, 1800);

      // Extract details and trigger download
      triggerDownload(selectedQuality, isAudio, videoEl);
    });
  });

  // Close dropdown on click outside
  document.addEventListener("click", (e) => {
    if (!wrapper.contains(e.target)) {
      popover.classList.remove("idm-open");
      pill.classList.remove("idm-active");
    }
  });

  playerContainer.appendChild(wrapper);
}

// Extracts the cleanest, most accurate video title across YouTube, Facebook, Instagram, TikTok, Twitter, etc.
function extractVideoTitle() {
  let candidate = "";

  // 1. YouTube specific title element (highest priority and 100% clean)
  if (window.location.hostname.includes("youtube.com") || window.location.hostname.includes("youtu.be")) {
    const ytH1 = document.querySelector('h1.ytd-watch-metadata yt-formatted-string, #title h1, h1.style-scope.ytd-watch-metadata, h2.ytd-shorts, h1.watch-title-container');
    if (ytH1 && ytH1.innerText && ytH1.innerText.trim().length > 0) {
      candidate = ytH1.innerText.trim();
    }
  }

  // 2. Meta tag extraction (OG, Twitter cards, standard title)
  if (!candidate) {
    const ogTitle = document.querySelector('meta[property="og:title"]')?.getAttribute('content');
    const twTitle = document.querySelector('meta[name="twitter:title"]')?.getAttribute('content');
    const metaTitle = document.querySelector('meta[name="title"]')?.getAttribute('content');
    candidate = (ogTitle || twTitle || metaTitle || "").trim();
  }

  // 3. Platform-specific DOM queries if meta tag is generic
  if (!candidate || candidate.toLowerCase() === "facebook" || candidate.toLowerCase() === "instagram" || candidate.toLowerCase() === "tiktok") {
    const fbReelText = document.querySelector('[data-ad-preview="message"], [role="main"] h1, [role="main"] [dir="auto"] span')?.innerText;
    if (fbReelText && fbReelText.trim().length > 3) {
      candidate = fbReelText.trim();
    }
  }

  // 4. Fallback to document.title
  if (!candidate || candidate.toLowerCase() === "facebook" || candidate.toLowerCase() === "watch") {
    candidate = document.title || "video";
  }

  // Clean common site suffixes, RTL markers, and notification counters
  candidate = candidate
    .replace(/^[\u200e\u200f\s]*\(\d+\)[\u200e\u200f\s]*/g, "") // Remove notification counts like "(1) "
    .replace(/[\s\u200e\u200f]*[-–—][\s\u200e\u200f]*YouTube$/i, "")
    .replace(/[\s\u200e\u200f]*[|][-–—][\s\u200e\u200f]*Facebook$/i, "")
    .replace(/[\s\u200e\u200f]*[|][\s\u200e\u200f]*TikTok$/i, "")
    .replace(/[\s\u200e\u200f]*•[\s\u200e\u200f]*Instagram.*$/i, "")
    .replace(/[\s\u200e\u200f]*\/[\s\u200e\u200f]*X$/i, "")
    .replace(/[\s\u200e\u200f]*on X:.*$/i, "")
    .replace(/[\s\u200e\u200f]*[|][\s\u200e\u200f]*Twitter$/i, "")
    .replace(/[\s\u200e\u200f]*[-–—][\s\u200e\u200f]*Vimeo$/i, "")
    .trim();

  if (!candidate || candidate.toLowerCase() === "facebook" || candidate.toLowerCase() === "reels") {
    candidate = "Facebook_Video";
  }

  // Sanitize for valid Windows filenames (allow alphanumeric, Arabic, unicode, hyphens, spaces)
  let cleanName = candidate
    .replace(/[\/\\:*?"<>|]/g, " ")
    .replace(/\s+/g, " ")
    .trim();

  if (cleanName.length > 120) {
    cleanName = cleanName.substring(0, 120).trim();
  }

  return cleanName || "video_stream";
}

// Helper to extract ytInitialPlayerResponse from page scripts or player
function extractYouTubeStreamingInfo() {
  try {
    const scripts = document.getElementsByTagName("script");
    for (let i = 0; i < scripts.length; i++) {
      const text = scripts[i].textContent;
      if (text && text.includes("ytInitialPlayerResponse =")) {
        const start = text.indexOf("ytInitialPlayerResponse =");
        if (start !== -1) {
          const jsonStart = text.indexOf("{", start);
          const end = text.indexOf("};", jsonStart);
          if (jsonStart !== -1 && end !== -1) {
            const raw = text.substring(jsonStart, end + 1);
            return JSON.parse(raw);
          }
        }
      }
    }
  } catch (e) {
    console.warn("[IDM Content] Error extracting ytInitialPlayerResponse:", e);
  }
  return null;
}

function getYouTubeStreamSizes(playerResponse, selectedQuality, isAudio) {
  let videoBytes = 0;
  let audioBytes = 0;
  let title = "";

  if (playerResponse && playerResponse.videoDetails) {
    title = playerResponse.videoDetails.title || "";
  }

  if (playerResponse && playerResponse.streamingData && playerResponse.streamingData.adaptiveFormats) {
    const formats = playerResponse.streamingData.adaptiveFormats;

    // Find best audio size
    const audioFormats = formats.filter(f => f.mimeType && f.mimeType.startsWith("audio/"));
    for (const af of audioFormats) {
      const len = parseInt(af.contentLength, 10);
      if (len > audioBytes) audioBytes = len;
    }

    // Find requested video quality size
    const targetHeight = selectedQuality.includes("1080") ? 1080 :
                         selectedQuality.includes("720") ? 720 :
                         selectedQuality.includes("480") ? 480 : 1080;

    const videoFormats = formats.filter(f => f.mimeType && f.mimeType.startsWith("video/"));
    for (const vf of videoFormats) {
      if (vf.height === targetHeight || (vf.qualityLabel && vf.qualityLabel.includes(targetHeight + "p"))) {
        const len = parseInt(vf.contentLength, 10);
        if (len > 0) {
          videoBytes = len;
          break;
        }
      }
    }
    // Fallback if target height not found
    if (videoBytes === 0 && videoFormats.length > 0) {
      for (const vf of videoFormats) {
        const len = parseInt(vf.contentLength, 10);
        if (len > videoBytes) videoBytes = len;
      }
    }
  }

  let totalSize = 0;
  if (isAudio) {
    totalSize = audioBytes;
  } else {
    totalSize = (videoBytes > 0 && audioBytes > 0) ? (videoBytes + audioBytes) : (videoBytes || audioBytes);
  }

  return { title, videoBytes, audioBytes, totalSize };
}

// Download execution handler
function triggerDownload(selectedQuality, isAudio, videoEl) {
  const pageUrl = window.location.href;
  const isYouTube = window.location.hostname.includes("youtube.com") || window.location.hostname.includes("youtu.be");

  let targetUrl = pageUrl;
  if (!isYouTube && videoEl) {
    const src = videoEl.currentSrc || videoEl.src;
    if (src && (src.startsWith("http://") || src.startsWith("https://"))) {
      targetUrl = src;
    }
  }

  let pageTitle = extractVideoTitle();
  let precomputedSize = 0;
  let precomputedVideoSize = 0;
  let precomputedAudioSize = 0;

  if (isYouTube) {
    let ytInfo = latestPagePlayerData;
    if (!ytInfo || !ytInfo.streamingData) {
      ytInfo = extractYouTubeStreamingInfo();
    }
    if (ytInfo) {
      const ytSizes = getYouTubeStreamSizes(ytInfo, selectedQuality, isAudio);
      if (ytSizes.title) pageTitle = ytSizes.title;
      precomputedSize = ytSizes.totalSize;
      precomputedVideoSize = ytSizes.videoBytes;
      precomputedAudioSize = ytSizes.audioBytes;
      console.log("[IDM Content] In-page YouTube Pre-Sniffing (0 ms):", ytSizes);
    }
  }

  const ext = isAudio ? ".mp3" : ".mp4";
  const filename = pageTitle + ext;

  const payload = {
    url: targetUrl,
    originalPageUrl: pageUrl,
    filename: filename,
    mimeType: isAudio ? "audio/mp3" : "video/mp4",
    quality: selectedQuality,
    referer: window.location.origin,
    totalSize: precomputedSize,
    videoSize: precomputedVideoSize,
    audioSize: precomputedAudioSize
  };

  console.log("[IDM Extension] Triggering Download for:", payload);

  try {
    if (typeof chrome !== "undefined" && chrome.runtime && chrome.runtime.id) {
      chrome.runtime.sendMessage(
        { type: "FORWARD_TO_IDM_APP", payload: payload },
        (response) => {
          if (chrome.runtime.lastError) {
            // Ignore stale context errors
          } else {
            console.log("[IDM Extension] Background bridge response:", response);
          }
        }
      );
    }
  } catch (e) {
    console.warn("[IDM Extension] Extension context error. Please refresh tab (F5).", e);
  }
}

// Scans the page for standard HTML5 videos, YouTube regular videos, and YouTube Shorts
function scanAndInject() {
  const allVideos = document.querySelectorAll("video");
  allVideos.forEach((video) => {
    // Ensure video is valid, visible, and has not been dismissed
    if (video.offsetWidth > 100 && video.offsetHeight > 80) {
      if (video.__idmDismissed || video.getAttribute("data-idm-dismissed") === "true") return;
      if (video.__idmInjected || video.getAttribute("data-idm-injected") === "true") return;

      // Find the single best player container for this video element
      const container = video.closest("ytd-reel-video-renderer, ytd-shorts, #movie_player, .html5-video-player, .player-container, [data-testid='videoComponent'], .video-js") || video.parentElement;
      if (container && !container.querySelector(".idm-video-container-wrapper") && container.getAttribute("data-idm-dismissed") !== "true") {
        attachWidgetToPlayer(container, video);
      }
    }
  });
}

// Run scanner immediately and on DOM changes / navigation
scanAndInject();

// Observer for dynamic SPAs (YouTube page transitions & infinite Shorts scrolling)
const observer = new MutationObserver(() => {
  scanAndInject();
});

observer.observe(document.body || document.documentElement, {
  childList: true,
  subtree: true
});

// Periodic fallback scan (handles fullscreen / tab focus transitions)
setInterval(scanAndInject, 1200);



