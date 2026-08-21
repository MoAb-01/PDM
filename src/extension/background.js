// IDM Companion Background Service Worker (Manifest V3)
// Sniffs network requests matching video, audio, archive, and adaptive streaming protocols

const MEDIA_MIME_PATTERNS = [
  "video/",
  "audio/",
  "application/x-mpegurl",
  "application/vnd.apple.mpegurl",
  "application/dash+xml",
  "application/octet-stream"
];

const MEDIA_EXTENSIONS = [
  ".m3u8", ".mpd", ".mp4", ".mkv", ".webm", ".ts", ".m4s",
  ".mp3", ".flac", ".wav", ".aac",
  ".zip", ".rar", ".7z", ".tar.gz", ".iso", ".exe", ".msi", ".pdf"
];

// In-memory media store keyed by tabId
const capturedMediaByTab = {};

// Listen for network response headers
chrome.webRequest?.onResponseStarted?.addListener(
  async (details) => {
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
        if (header.name.toLowerCase() === "content-type") {
          mimeType = header.value ? header.value.toLowerCase() : "";
          if (MEDIA_MIME_PATTERNS.some(pat => mimeType.includes(pat))) {
            isMedia = true;
          }
        }
      }
    }

    if (isMedia) {
      const tabId = details.tabId;
      if (!capturedMediaByTab[tabId]) {
        capturedMediaByTab[tabId] = [];
      }

      // Avoid duplicates
      const exists = capturedMediaByTab[tabId].some(item => item.url === details.url);
      if (!exists) {
        // Retrieve cookies for authorization forwarding
        let cookiesString = "";
        try {
          const cookies = await chrome.cookies.getAll({ url: details.url });
          cookiesString = cookies.map(c => `${c.name}=${c.value}`).join("; ");
        } catch (e) {
          // Ignore
        }

        const mediaItem = {
          id: `media-${Date.now()}-${Math.random().toString(36).substr(2, 5)}`,
          url: details.url,
          mimeType: mimeType || "application/octet-stream",
          initiator: details.initiator || "Web Player",
          timestamp: Date.now(),
          cookies: cookiesString
        };

        capturedMediaByTab[tabId].push(mediaItem);

        // Update extension badge
        chrome.action?.setBadgeText?.({
          tabId: tabId,
          text: capturedMediaByTab[tabId].length.toString()
        });
        chrome.action?.setBadgeBackgroundColor?.({
          tabId: tabId,
          color: "#0284c7"
        });

        // Notify content script floating widget
        chrome.tabs?.sendMessage?.(tabId, {
          type: "IDM_MEDIA_DETECTED",
          media: mediaItem
        }).catch(() => {});
      }
    }
  },
  { urls: ["<all_urls>"] },
  ["responseHeaders"]
);

console.log("=========================================");
console.log("[IDM Extension] Active Extension ID:", chrome.runtime.id);
console.log("=========================================");

// Centralized forward function (Dual-Channel: HTTP Bridge + Native Messaging Host)
function forwardDownloadToIdmApp(stream) {
  console.log("[IDM Extension] Forwarding download to IDM Application:", stream);

  const payload = {
    url: stream.url || "",
    filename: stream.filename || "",
    referer: stream.referer || stream.referrer || stream.initiator || "",
    mimeType: stream.mimeType || stream.mime || ""
  };

  // 1. Direct Local HTTP Bridge (Instant zero-config popup when IDM app is running)
  fetch("http://127.0.0.1:8989/download", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload)
  })
  .then(res => res.json())
  .then(data => {
    console.log("[IDM Extension] Local HTTP Bridge Response:", data);
  })
  .catch(err => {
    console.log("[IDM Extension] Local HTTP Bridge unavailable (app might be closed), invoking Native Host...");
  });

  // 2. Chrome Native Messaging Host (Auto-launches DownloadManagerAB.exe if closed)
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
}

// Communication with popup and content script
chrome.runtime?.onMessage?.addListener((message, sender, sendResponse) => {
  if (message.type === "GET_CAPTURED_MEDIA") {
    const tabId = message.tabId;
    sendResponse({ media: capturedMediaByTab[tabId] || [] });
    return true;
  }

  if (message.type === "FORWARD_TO_IDM_APP") {
    forwardDownloadToIdmApp(message.payload || {});
    sendResponse({ success: true, message: "Opened Download File Info in IDM C++ Application" });
    return true;
  }
});

// Intercept browser downloads and forward to IDM
chrome.downloads?.onDeterminingFilename?.addListener((downloadItem, suggest) => {
  if (!downloadItem || !downloadItem.url) return;

  // Cancel standard Chrome download
  chrome.downloads.cancel(downloadItem.id, () => {
    chrome.downloads.erase({ id: downloadItem.id });
  });

  console.log("[IDM Extension] Intercepted browser download:", downloadItem);
  forwardDownloadToIdmApp({
    url: downloadItem.url,
    filename: downloadItem.filename,
    referer: downloadItem.referrer || downloadItem.finalUrl,
    mimeType: downloadItem.mime
  });
});
