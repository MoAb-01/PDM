// Page-World Script for YouTube & Media Extraction
(function() {
  function sendPlayerData() {
    try {
      let resp = null;
      const player = document.getElementById("movie_player");
      if (player && typeof player.getPlayerResponse === "function") {
        resp = player.getPlayerResponse();
      }
      if (!resp && window.ytInitialPlayerResponse) {
        resp = window.ytInitialPlayerResponse;
      }
      window.dispatchEvent(new CustomEvent("IDM_PAGE_PLAYER_DATA", {
        detail: {
          title: resp?.videoDetails?.title || document.title || "",
          videoDetails: resp?.videoDetails || null,
          streamingData: resp?.streamingData || null
        }
      }));
    } catch(e) {
      window.dispatchEvent(new CustomEvent("IDM_PAGE_PLAYER_DATA", { detail: null }));
    }
  }

  window.addEventListener("IDM_QUERY_PAGE_PLAYER", sendPlayerData);
  window.addEventListener("yt-navigate-finish", () => {
    setTimeout(sendPlayerData, 500);
  });
  sendPlayerData();
})();
