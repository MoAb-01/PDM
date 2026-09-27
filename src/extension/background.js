// IDM Companion Background Service Worker (Manifest V3)
// Sniffs network requests matching video, audio, archive, and adaptive streaming protocols

const MEDIA_MIME_PATTERNS = [
  "video/",
  "audio/",
  "application/x-mpegurl",
  "application/vnd.apple.mpegurl",
  "application/dash+xml"
];

const MEDIA_EXTENSIONS = [
  ".m3u8", ".mpd", ".mp4", ".mkv", ".webm", ".ts", ".m4s",
  ".mp3", ".flac", ".wav", ".aac",
  ".zip", ".rar", ".7z", ".tar.gz", ".iso", ".exe", ".msi", ".pdf"
];

// In-memory media store keyed by tabId
const capturedMediaByTab = {};

// Listen for network response headers
if (chrome.webRequest && chrome.webRequest.onResponseStarted) {
  chrome.webRequest.onResponseStarted.addListener(
    async (details) => {
      try {
        if (!details.url || details.url.startsWith("chrome-extension://")) return;

        let isMedia = false;
        let mimeType = "";
        const lowerUrl = details.url.toLowerCase();

        // Check URL extension
        for (const ext of MEDIA_EXTENSIONS) {
          if (lowerUrl.includes(ext)) {
            isMedia = true;
            break;
          }
        }

        // Check response Content-Type
        if (details.responseHeaders) {
          for (const header of details.responseHeaders) {
            if (header.name && header.name.toLowerCase() === "content-type") {
              mimeType = header.value ? header.value.toLowerCase() : "";
              if (MEDIA_MIME_PATTERNS.some(pat => mimeType.includes(pat))) {
                isMedia = true;
              }
            }
          }
        }

        if (isMedia) {
          const tabId = details.tabId;
          const key = (typeof tabId === "number" && tabId >= 0) ? tabId : "global";

          if (!capturedMediaByTab[key]) {
            capturedMediaByTab[key] = [];
          }

          // Avoid duplicates
          const exists = capturedMediaByTab[key].some(item => item.url === details.url);
          if (!exists) {
            // Retrieve cookies for authorization forwarding
            let cookiesString = "";
            try {
              if (chrome.cookies && chrome.cookies.getAll) {
                const cookies = await chrome.cookies.getAll({ url: details.url });
                if (Array.isArray(cookies)) {
                  cookiesString = cookies.map(c => `${c.name}=${c.value}`).join("; ");
                }
              }
            } catch (e) {
              // Ignore cookie read failures
            }

            const mediaItem = {
              id: `media-${Date.now()}-${Math.random().toString(36).substr(2, 5)}`,
              url: details.url,
              mimeType: mimeType || "application/octet-stream",
              initiator: details.initiator || "Web Player",
              timestamp: Date.now(),
              cookies: cookiesString
            };

            capturedMediaByTab[key].push(mediaItem);

            // Update extension badge only for valid tabId (>= 0)
            if (typeof tabId === "number" && tabId >= 0) {
              try {
                if (chrome.action && chrome.action.setBadgeText) {
                  chrome.action.setBadgeText({
                    tabId: tabId,
                    text: capturedMediaByTab[key].length.toString()
                  });
                }
                if (chrome.action && chrome.action.setBadgeBackgroundColor) {
                  chrome.action.setBadgeBackgroundColor({
                    tabId: tabId,
                    color: "#0284c7"
                  });
                }
              } catch (e) {
                // Ignore badge errors
              }

              // Notify content script floating widget
              try {
                if (chrome.tabs && chrome.tabs.sendMessage) {
                  chrome.tabs.sendMessage(tabId, {
                    type: "IDM_MEDIA_DETECTED",
                    media: mediaItem
                  }).catch(() => { });
                }
              } catch (e) {
                // Ignore tab message errors
              }
            }
          }
        }
      } catch (err) {
        console.warn("[IDM Extension] onResponseStarted error:", err);
      }
    },
    { urls: ["<all_urls>"] },
    ["responseHeaders"]
  );
}

console.log("=========================================");
console.log("[IDM Extension] Active Extension ID:", chrome.runtime?.id || "unknown");
console.log("=========================================");

// Debounce & deduplication cache to prevent duplicate popups
let lastDownloadUrl = "";
let lastDownloadTime = 0;

async function resolveTabTitleIfMissing(stream) {
  const isAudio = (stream.quality && (stream.quality.includes("Audio") || stream.quality.includes("MP3") || stream.quality.includes("M4A"))) ||
                  (stream.mimeType && stream.mimeType.includes("audio"));
  const audioExt = (stream.quality && stream.quality.includes("M4A")) ? ".m4a" : ".mp3";
  const defaultExt = isAudio ? audioExt : ".mp4";

  // Check URL query parameters first (e.g. ?path=/mnt/user-data/outputs/design.md, ?filename=, ?file=)
  if (stream.url) {
    try {
      const parsedUrl = new URL(stream.url);
      const queryParam = parsedUrl.searchParams.get("path") || parsedUrl.searchParams.get("filename") || parsedUrl.searchParams.get("file");
      if (queryParam) {
        const decoded = decodeURIComponent(queryParam);
        const base = decoded.split("/").pop().split("\\").pop();
        if (base && base.includes(".")) {
          return base;
        }
      }
    } catch (e) {}
  }

  if (stream.filename && stream.filename !== "watch" && stream.filename !== "download.bin" && stream.filename !== "video" && stream.filename !== "download-file") {
    let fn = stream.filename;
    if (isAudio && fn.toLowerCase().endsWith(".mp4")) {
      fn = fn.substring(0, fn.length - 4) + defaultExt;
    }
    return fn;
  }
  try {
    const tabs = await chrome.tabs.query({ active: true, currentWindow: true });
    if (tabs && tabs[0] && tabs[0].title) {
      let t = tabs[0].title
        .replace(/^[\u200e\u200f\s]*\(\d+\)[\u200e\u200f\s]*/g, "")
        .replace(/[\s\u200e\u200f]*[-–—][\s\u200e\u200f]*YouTube$/i, "")
        .replace(/[\s\u200e\u200f]*[|][-–—][\s\u200e\u200f]*Facebook$/i, "")
        .replace(/[\s\u200e\u200f]*[|][\s\u200e\u200f]*TikTok$/i, "")
        .replace(/[\s\u200e\u200f]*•[\s\u200e\u200f]*Instagram.*$/i, "")
        .replace(/[\s\u200e\u200f]*\/[\s\u200e\u200f]*X$/i, "")
        .trim();
      let clean = t.replace(/[\/\\:*?"<>|]/g, " ").replace(/\s+/g, " ").trim();
      if (clean.length > 120) clean = clean.substring(0, 120).trim();
      if (clean && clean.length > 2) {
        return clean + defaultExt;
      }
    }
  } catch (e) {}
  return stream.filename || "";
}

function isCloudflareOrProtected(url) {
  if (!url || typeof url !== "string") return false;
  const u = url.toLowerCase();
  return (
    u.includes("overleaf.com") ||
    u.includes("/download/project/") ||
    u.includes("claude.ai") ||
    u.includes("/wiggle/download-file") ||
    u.includes("cloudflare") ||
    u.includes("challenges.cloudflare.com") ||
    u.includes("cf-browser-verification")
  );
}

function getCategoryFolderForFilename(filename, mimeType) {
  if (!filename) filename = "";
  const ext = filename.split(".").pop().toLowerCase();

  if (["mp4", "mkv", "avi", "mov", "webm", "flv", "ts", "m4v"].includes(ext) || (mimeType && mimeType.includes("video"))) {
    return "Video";
  }
  if (["mp3", "wav", "flac", "aac", "ogg", "m4a", "wma"].includes(ext) || (mimeType && mimeType.includes("audio"))) {
    return "Music";
  }
  if (["zip", "rar", "7z", "tar", "gz", "bz2", "xz", "iso"].includes(ext) || (mimeType && (mimeType.includes("zip") || mimeType.includes("compressed") || mimeType.includes("archive")))) {
    return "Compressed";
  }
  if (["pdf", "doc", "docx", "xls", "xlsx", "ppt", "pptx", "txt", "epub", "csv", "md", "markdown", "json"].includes(ext) || (mimeType && (mimeType.includes("pdf") || mimeType.includes("document") || mimeType.includes("text")))) {
    return "Documents";
  }
  if (["exe", "msi", "bat", "cmd", "apk", "dmg", "pkg"].includes(ext)) {
    return "Programs";
  }
  return "General";
}

// Centralized forward function (Primary: HTTP Bridge | Fallback: Native Messaging Host)
async function forwardDownloadToIdmApp(stream) {
  if (!stream || !stream.url) return;

  // In-memory Blob/Data URLs (WhatsApp Web, Telegram Web, Mega, etc.) are handled directly by the browser
  if (stream.url.startsWith("blob:") || stream.url.startsWith("data:")) {
    console.log("[IDM Extension] Skipping in-memory blob/data URL, handled by browser:", stream.filename);
    return;
  }

  const now = Date.now();
  if (stream.url === lastDownloadUrl && (now - lastDownloadTime) < 1500) {
    console.log("[IDM Extension] Duplicate download request suppressed:", stream.url);
    return;
  }
  lastDownloadUrl = stream.url;
  lastDownloadTime = now;

  const resolvedFilename = await resolveTabTitleIfMissing(stream);

  let cookieHeader = "";
  try {
    const u = new URL(stream.url);
    const domain = u.hostname.replace(/^www\./, "");
    const [c1, c2, c3] = await Promise.all([
      chrome.cookies.getAll({ url: stream.url }).catch(() => []),
      chrome.cookies.getAll({ url: "https://" + u.hostname + "/" }).catch(() => []),
      chrome.cookies.getAll({ domain: domain }).catch(() => [])
    ]);
    const cookieMap = new Map();
    [...(c1 || []), ...(c2 || []), ...(c3 || [])].forEach(c => {
      if (c && c.name && !cookieMap.has(c.name)) {
        cookieMap.set(c.name, c.value);
      }
    });
    const parts = [];
    cookieMap.forEach((val, name) => parts.push(`${name}=${val}`));
    cookieHeader = parts.join("; ");
    console.log("[IDM] Cookie header built (" + parts.length + " cookies):", cookieHeader);
  } catch (e) {
    console.error("[IDM] Error harvesting cookies:", e);
  }

  let referer = stream.referer || stream.referrer || stream.initiator || "";
  if (!referer && chrome.tabs) {
    try {
      const tabs = await chrome.tabs.query({ active: true, currentWindow: true });
      if (tabs && tabs[0] && tabs[0].url) referer = tabs[0].url;
    } catch (e) {}
  }

  console.log("[IDM Extension] Forwarding download to IDM Application:", stream);

  const payload = {
    url: stream.url || "",
    originalPageUrl: stream.originalPageUrl || "",
    filename: resolvedFilename,
    referer: referer,
    mimeType: stream.mimeType || stream.mime || "",
    quality: stream.quality || "",
    cookies: cookieHeader,
    userAgent: navigator.userAgent,
    totalSize: (stream.totalSize && stream.totalSize > 0) ? stream.totalSize : ((stream.fileSize && stream.fileSize > 0) ? stream.fileSize : 0),
    videoSize: stream.videoSize || 0,
    audioSize: stream.audioSize || 0
  };

  // 1. Direct Local HTTP Bridge (Primary fast channel when IDM app is running)
  fetch("http://127.0.0.1:9898/download", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload)
  })
    .then(res => res.json())
    .then(data => {
      console.log("[IDM Extension] Local HTTP Bridge Response:", data);
    })
    .catch(err => {
      // 2. Native Messaging Host fallback (Only invoked if HTTP bridge fails / app is closed)
      console.log("[IDM Extension] Local HTTP Bridge unavailable, invoking Native Host fallback...");
      if (chrome.runtime && chrome.runtime.sendNativeMessage) {
        try {
          chrome.runtime.sendNativeMessage(
            "com.idm.nativehost",
            payload,
            (response) => {
              if (chrome.runtime.lastError) {
                console.warn("[IDM Native Host Message]", chrome.runtime.lastError.message);
              } else {
                console.log("[IDM Native Host Response]:", response);
              }
            }
          );
        } catch (e) {
          console.warn("[IDM Native Host Exception]", e);
        }
      }
    });
}

// Communication with popup and content script
if (chrome.runtime && chrome.runtime.onMessage) {
  chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
    if (message.type === "GET_CAPTURED_MEDIA") {
      const tabId = message.tabId;
      const key = (typeof tabId === "number" && tabId >= 0) ? tabId : "global";
      sendResponse({ media: capturedMediaByTab[key] || [] });
      return true;
    }

    if (message.type === "FORWARD_TO_IDM_APP") {
      forwardDownloadToIdmApp(message.payload || {});
      sendResponse({ success: true, message: "Opened Download File Info in IDM C++ Application" });
      return true;
    }
  });
}

// Intercept browser downloads and route appropriately
if (chrome.downloads && chrome.downloads.onDeterminingFilename) {
  chrome.downloads.onDeterminingFilename.addListener((downloadItem, suggest) => {
    if (!downloadItem || !downloadItem.url) return;

    // For in-memory Blob/Data URLs (WhatsApp Web, Telegram Web, Mega client-side decryption):
    // Allow the browser to save it directly from RAM in 0.01s with zero network friction.
    if (downloadItem.url.startsWith("blob:") || downloadItem.url.startsWith("data:")) {
      console.log("[IDM Extension] In-memory blob/data download detected (WhatsApp/Telegram), saving directly via browser:", downloadItem.filename);
      suggest({ filename: downloadItem.filename || "download" });
      return;
    }

    console.log("[IDM Extension] Intercepted browser download:", downloadItem.url, "Filename:", downloadItem.filename);

    // Call suggest first
    suggest({ filename: downloadItem.filename || "download" });

    // Cancel Chrome-managed download after a short delay so IDM takes over
    setTimeout(() => {
      try {
        chrome.downloads.cancel(downloadItem.id, () => {
          if (chrome.runtime.lastError) return;
          chrome.downloads.erase({ id: downloadItem.id }, () => {
            const eraseErr = chrome.runtime.lastError;
          });
        });
      } catch (e) {
        // Ignore cancellation errors
      }
    }, 200);

    const totalBytes = (downloadItem.fileSize && downloadItem.fileSize > 0)
      ? downloadItem.fileSize
      : ((downloadItem.totalBytes && downloadItem.totalBytes > 0) ? downloadItem.totalBytes : 0);

    // Forward to C++ app with cookies, referer, userAgent, and exact totalBytes
    forwardDownloadToIdmApp({
      url: downloadItem.url,
      filename: downloadItem.filename,
      referer: downloadItem.referrer || downloadItem.finalUrl || "",
      mimeType: downloadItem.mime || "",
      totalSize: totalBytes
    });
    return true;
  });
}
