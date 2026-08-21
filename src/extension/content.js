// IDM Companion Content Script for YouTube, Shorts, and all HTML5 Video Players
// Automatically detects active video players and injects the iconic floating "Download this video" IDM panel

let currentFloatingWidget = null;
let activeVideoElement = null;

// CSS for the floating video download widget
const style = document.createElement("style");
style.textContent = `
  .idm-video-download-panel {
    position: absolute !important;
    top: 12px !important;
    right: 14px !important;
    z-index: 2147483647 !important;
    background: linear-gradient(180deg, #2b2b32 0%, #1c1c20 100%) !important;
    color: #f1f5f9 !important;
    border: 1px solid #0284c7 !important;
    box-shadow: 0 8px 24px rgba(0, 0, 0, 0.7), 0 0 10px rgba(2, 132, 199, 0.4) !important;
    border-radius: 4px !important;
    padding: 5px 10px !important;
    font-family: "Segoe UI", -apple-system, BlinkMacSystemFont, Roboto, sans-serif !important;
    font-size: 11.5px !important;
    display: flex !important;
    align-items: center !important;
    gap: 8px !important;
    cursor: pointer !important;
    transition: transform 0.15s ease, box-shadow 0.15s ease !important;
    user-select: none !important;
  }
  .idm-video-download-panel:hover {
    transform: scale(1.03) !important;
    box-shadow: 0 12px 28px rgba(0, 0, 0, 0.85), 0 0 14px rgba(56, 189, 248, 0.6) !important;
  }
  .idm-logo-dot {
    width: 14px;
    height: 14px;
    border-radius: 50%;
    background: radial-gradient(circle, #38bdf8, #0284c7, #1e3a8a);
    box-shadow: 0 0 6px #38bdf8;
    flex-shrink: 0;
  }
  .idm-quality-select {
    background: #111827 !important;
    color: #38bdf8 !important;
    border: 1px solid #374151 !important;
    border-radius: 3px !important;
    font-size: 11px !important;
    padding: 2px 4px !important;
    outline: none !important;
    cursor: pointer !important;
  }
  .idm-download-btn {
    background: #0284c7 !important;
    color: #fff !important;
    border: none !important;
    border-radius: 3px !important;
    padding: 3px 8px !important;
    font-size: 11px !important;
    font-weight: 600 !important;
    cursor: pointer !important;
  }
  .idm-download-btn:hover {
    background: #0369a1 !important;
  }
`;
document.documentElement.appendChild(style);

function attachWidgetToPlayer(playerContainer, videoEl) {
  if (!playerContainer || playerContainer.querySelector(".idm-video-download-panel")) return;

  // Make sure player has relative positioning
  const computedStyle = window.getComputedStyle(playerContainer);
  if (computedStyle.position === "static") {
    playerContainer.style.position = "relative";
  }

  const panel = document.createElement("div");
  panel.className = "idm-video-download-panel";
  panel.innerHTML = `
    <div class="idm-logo-dot"></div>
    <span style="font-weight:600; color:#f8fafc; text-shadow:0 1px 2px rgba(0,0,0,0.8);">Download this video</span>
    <select class="idm-quality-select">
      <option value="1080p Full HD">1080p Full HD (60fps)</option>
      <option value="720p HD" selected>720p HD</option>
      <option value="480p SD">480p SD</option>
      <option value="Audio MP3">Audio Track (MP3)</option>
    </select>
    <button class="idm-download-btn">Start</button>
  `;

  // Prevent video player click events from triggering on widget click
  panel.addEventListener("click", (e) => e.stopPropagation());
  panel.addEventListener("mousedown", (e) => e.stopPropagation());

  const downloadBtn = panel.querySelector(".idm-download-btn");
  const qualitySelect = panel.querySelector(".idm-quality-select");

  downloadBtn.addEventListener("click", (e) => {
    e.preventDefault();
    e.stopPropagation();

    const selectedQuality = qualitySelect.value;
    const pageUrl = window.location.href;
    const pageTitle = document.title.replace(" - YouTube", "").replace(" | TikTok", "") || "video_stream";

    // Clean file name
    let filename = pageTitle.replace(/[^a-zA-Z0-9_-]/g, "_") + (selectedQuality.includes("Audio") ? ".mp3" : ".mp4");

    const payload = {
      url: pageUrl,
      filename: filename,
      mimeType: selectedQuality.includes("Audio") ? "audio/mp3" : "video/mp4",
      quality: selectedQuality,
      referer: window.location.origin
    };

    console.log("[IDM Extension] Triggering Download for:", payload);

    // 1. Direct Local Bridge Trigger to C++ DownloadManagerAB.exe (Fastest & 100% Reliable)
    fetch("http://127.0.0.1:8989/download", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    }).then(res => res.json()).then(data => {
      console.log("[IDM Extension] Local bridge success:", data);
    }).catch(err => {
      console.log("[IDM Extension] Local bridge fallback to Native Messaging:", err);
      // 2. Native Messaging Fallback with Context Validation Protection
      try {
        if (typeof chrome !== "undefined" && chrome.runtime && chrome.runtime.id) {
          chrome.runtime.sendMessage({
            type: "FORWARD_TO_IDM_APP",
            payload: payload
          }, () => {
            if (chrome.runtime.lastError) {
              // Ignore benign disconnects
            }
          });
        }
      } catch (e) {
        console.warn("[IDM Extension] Extension context updated. Please refresh tab (F5).", e);
      }
    });
  });

  playerContainer.appendChild(panel);
}

// Scans the page for standard HTML5 videos, YouTube regular videos, and YouTube Shorts
function scanAndInject() {
  // 1. YouTube Shorts Container
  const shortsRenderer = document.querySelector("ytd-reel-video-renderer[is-active], ytd-shorts");
  if (shortsRenderer) {
    const shortsVideo = shortsRenderer.querySelector("video");
    if (shortsVideo) {
      const container = shortsRenderer.querySelector("#player-container, .player-container, .html5-video-player") || shortsRenderer;
      attachWidgetToPlayer(container, shortsVideo);
    }
  }

  // 2. YouTube Main Player
  const ytPlayer = document.querySelector("#movie_player, .html5-video-player");
  if (ytPlayer) {
    const ytVideo = ytPlayer.querySelector("video");
    if (ytVideo) {
      attachWidgetToPlayer(ytPlayer, ytVideo);
    }
  }

  // 3. Generic HTML5 Videos (Vimeo, Twitter, Streaming portals, TikTok, etc.)
  const allVideos = document.querySelectorAll("video");
  allVideos.forEach((video) => {
    if (video.offsetWidth > 150 && video.offsetHeight > 100) {
      const parent = video.parentElement;
      if (parent && !parent.querySelector(".idm-video-download-panel")) {
        attachWidgetToPlayer(parent, video);
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
