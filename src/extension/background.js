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
                  }).catch(() => {});
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

// Intercept browser downloads and forward to IDM
if (chrome.downloads && chrome.downloads.onDeterminingFilename) {
  chrome.downloads.onDeterminingFilename.addListener((downloadItem, suggest) => {
    if (!downloadItem || !downloadItem.url) return;

    console.log("[IDM Extension] Intercepted browser download:", downloadItem.url);

    // Must call suggest() to signal we handled it, then cancel the Chrome download
    suggest({ filename: downloadItem.filename || "download" });

    // Cancel and erase the Chrome-managed download after a short delay
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

    // Forward to C++ app via HTTP bridge
    forwardDownloadToIdmApp({
      url: downloadItem.url,
      filename: downloadItem.filename,
      referer: downloadItem.referrer || downloadItem.finalUrl || "",
      mimeType: downloadItem.mime || ""
    });
  });
}
