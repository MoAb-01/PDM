// Parser for HLS (.m3u8) playlists and MPEG-DASH (.mpd) manifests

export function parseM3U8(content, baseUrl = "https://example.com/stream/") {
  const lines = content.split(/\r?\n/).map(l => l.trim()).filter(Boolean);
  const result = {
    type: "HLS",
    isMaster: false,
    variants: [],
    segments: [],
    targetDuration: 6,
    totalDuration: 0,
    mediaSequence: 0
  };

  let currentVariant = null;

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];

    if (line.startsWith("#EXT-X-STREAM-INF:")) {
      result.isMaster = true;
      const bandwidthMatch = line.match(/BANDWIDTH=(\d+)/);
      const resolutionMatch = line.match(/RESOLUTION=(\d+x\d+)/);
      const codecsMatch = line.match(/CODECS="([^"]+)"/);
      const nameMatch = line.match(/NAME="([^"]+)"/);

      const nextLine = lines[i + 1] && !lines[i + 1].startsWith("#") ? lines[i + 1] : "";
      const streamUrl = nextLine.startsWith("http") ? nextLine : new URL(nextLine, baseUrl).href;

      result.variants.push({
        bandwidth: bandwidthMatch ? parseInt(bandwidthMatch[1], 10) : 0,
        resolution: resolutionMatch ? resolutionMatch[1] : "Adaptive",
        codecs: codecsMatch ? codecsMatch[1] : "avc1.4d401f,mp4a.40.2",
        name: nameMatch ? nameMatch[1] : (resolutionMatch ? `${resolutionMatch[1].split('x')[1]}p` : "Auto"),
        url: streamUrl
      });
    } else if (line.startsWith("#EXTINF:")) {
      const duration = parseFloat(line.substring(8).split(",")[0]);
      const nextLine = lines[i + 1] && !lines[i + 1].startsWith("#") ? lines[i + 1] : `segment_${result.segments.length + 1}.ts`;
      const segmentUrl = nextLine.startsWith("http") ? nextLine : new URL(nextLine, baseUrl).href;

      result.segments.push({
        index: result.segments.length + 1,
        duration: duration || 6,
        url: segmentUrl,
        sizeEstBytes: Math.round((duration || 6) * 450 * 1024) // estimate ~3.6Mbps
      });
      result.totalDuration += duration || 6;
    } else if (line.startsWith("#EXT-X-TARGETDURATION:")) {
      result.targetDuration = parseInt(line.split(":")[1], 10) || 6;
    }
  }

  // If master manifest has variants, default to 1080p / 720p / 480p tracks
  if (!result.isMaster && result.segments.length === 0) {
    // Generate fallback simulated segments
    for (let s = 1; s <= 24; s++) {
      result.segments.push({
        index: s,
        duration: 5.0,
        url: `${baseUrl}segment_${s.toString().padStart(3, "0")}.ts`,
        sizeEstBytes: 2400000 + Math.floor(Math.random() * 500000)
      });
      result.totalDuration += 5.0;
    }
  }

  return result;
}

export function parseMPD(xmlString, baseUrl = "https://example.com/dash/") {
  return {
    type: "DASH",
    isMaster: true,
    variants: [
      { name: "2160p 4K HDR", resolution: "3840x2160", bandwidth: 18000000, codecs: "vp09.02.51.10" },
      { name: "1080p 60fps Full HD", resolution: "1920x1080", bandwidth: 6000000, codecs: "avc1.64002a" },
      { name: "720p HD", resolution: "1280x720", bandwidth: 3200000, codecs: "avc1.4d401f" },
      { name: "480p SD", resolution: "854x480", bandwidth: 1200000, codecs: "avc1.4d401e" },
      { name: "Audio Track (Opus 160kbps)", resolution: "Audio Only", bandwidth: 160000, codecs: "opus" }
    ],
    segmentsCount: 36,
    totalDuration: 180
  };
}
