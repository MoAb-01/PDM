// Extension popup script
document.addEventListener("DOMContentLoaded", async () => {
  const tabs = await chrome.tabs?.query({ active: true, currentWindow: true });
  const activeTabId = tabs && tabs[0] ? tabs[0].id : null;

  if (!activeTabId) return;

  chrome.runtime?.sendMessage({ type: "GET_CAPTURED_MEDIA", tabId: activeTabId }, (response) => {
    const listEl = document.getElementById("media-list");
    const countBadge = document.getElementById("count-badge");

    if (response && response.media && response.media.length > 0) {
      countBadge.innerText = `${response.media.length} Found`;
      listEl.innerHTML = "";

      response.media.forEach((item) => {
        const div = document.createElement("div");
        div.className = "item";
        div.innerHTML = `
          <div style="font-weight:600; color:#38bdf8;">${item.mimeType}</div>
          <div class="item-url">${item.url}</div>
          <button class="btn" id="btn-${item.id}">Download with IDM</button>
        `;
        listEl.appendChild(div);

        document.getElementById(`btn-${item.id}`)?.addEventListener("click", () => {
          chrome.runtime.sendMessage({
            type: "FORWARD_TO_IDM_APP",
            payload: item
          });
          alert("Forwarded stream to IDM Downloader!");
        });
      });
    }
  });
});
