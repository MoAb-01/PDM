// Local storage and initial state service for IDM Pro

export const INITIAL_SETTINGS = {
  // General tab
  launchOnStartup: true,
  clipboardAutoDetect: true,
  advancedBrowserIntegration: true,
  capturedBrowsers: [
    { name: "Google Chrome", enabled: true },
    { name: "Microsoft Edge", enabled: true },
    { name: "Mozilla Firefox", enabled: true },
    { name: "Apple Safari", enabled: true },
    { name: "Brave Browser", enabled: true },
    { name: "Opera / Opera GX", enabled: true }
  ],
  // File types tab
  autoCaptureExtensions: "3GP 7Z AAC ACE AIF APK ARJ ASF AVI BIN BZ2 EXE GZ GZIP IMG ISO LZH M4A M4V MKV MOV MP3 MP4 MPA MPE MPEG MPG MSI MSU OGG OGV PDF PLJ PPS PPT QT R0* R1* RA RAR RM RMVB SEA SIT SITX TAR TIF TIFF WAV WMA WMV Z ZIP TS M3U8 MPD",
  excludedSites: "*.update.microsoft.com download.windowsupdate.com *.download.windowsupdate.com siteseal.thawte.com ecom.cimetz.com *.voice2page.com",
  // Save to tab
  categories: [
    { id: "general", name: "General", folder: "C:\\Users\\UHD\\Downloads\\General\\", extensions: "*" },
    { id: "compressed", name: "Compressed", folder: "C:\\Users\\UHD\\Downloads\\Compressed\\", extensions: "zip rar 7z tar gz bz2 iso" },
    { id: "documents", name: "Documents", folder: "C:\\Users\\UHD\\Downloads\\Documents\\", extensions: "pdf doc docx txt ppt pptx xls xlsx" },
    { id: "music", name: "Music", folder: "C:\\Users\\UHD\\Downloads\\Music\\", extensions: "mp3 flac wav aac ogg m4a" },
    { id: "programs", name: "Programs", folder: "C:\\Users\\UHD\\Downloads\\Programs\\", extensions: "exe msi apk dmg app" },
    { id: "video", name: "Video", folder: "C:\\Users\\UHD\\Downloads\\Video\\", extensions: "mp4 mkv webm mov avi flv m3u8 mpd ts" }
  ],
  tempDirectory: "C:\\Users\\UHD\\AppData\\Roaming\\IDM\\",
  changeFolderOnLastSelected: true,
  setCreationDateFromServer: false,
  // Downloads tab
  showStartDialog: true,
  showCompleteDialog: false,
  startImmediately: true,
  showQueueSelectionOnLater: true,
  showQueueSelectionOnBatchClose: true,
  ignoreFileModTimeResuming: false,
  duplicateAction: "ask", // "ask" | "overwrite" | "rename"
  userAgent: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36",
  // Connection tab
  connectionType: "High Speed (Direct connection / 5G / Fiber)",
  maxConnections: 16, // 1 to 32 parallel chunk threads
  speedLimiterEnabled: false,
  speedLimitKBps: 2048,
  // Sites Logins tab
  savedSiteLogins: [
    { url: "drive.google.com", username: "uhd_workspace@gmail.com", password: "••••••••••••" },
    { url: "mega.nz", username: "uhd_developer", password: "••••••••••••" },
    { url: "internal.cdn.company.com", username: "auth_token_usr", password: "••••••••••••" }
  ],
  // Proxy
  proxyEnabled: false,
  proxyType: "HTTP",
  proxyHost: "127.0.0.1",
  proxyPort: "8080"
};

export const INITIAL_DOWNLOADS = [
  {
    id: "dl-1",
    filename: "win-e400-1_1-mcd.exe",
    category: "programs",
    sizeBytes: 49503792,
    downloadedBytes: 49503792,
    status: "Complete", // "Complete" | "Downloading" | "Paused" | "Error" | "Queued"
    speedBytesPerSec: 0,
    savePath: "C:\\Users\\UHD\\Downloads\\Programs\\win-e400-1_1-mcd.exe",
    url: "https://gdlp01.c-wss.com/gds/7/0100006027/02/win-e400-1_1-mcd.exe",
    referer: "https://www.canon-europe.com/",
    description: "Canon Inkjet Printer Driver Package",
    login: "",
    password: "",
    lastTryDate: "Aug 20, 2026 21:18",
    mimeType: "application/x-msdownload",
    connections: 8,
    chunks: [
      { id: 1, percent: 100, range: "0 - 6.18MB", active: false },
      { id: 2, percent: 100, range: "6.18MB - 12.37MB", active: false },
      { id: 3, percent: 100, range: "12.37MB - 18.56MB", active: false },
      { id: 4, percent: 100, range: "18.56MB - 24.75MB", active: false },
      { id: 5, percent: 100, range: "24.75MB - 30.93MB", active: false },
      { id: 6, percent: 100, range: "30.93MB - 37.12MB", active: false },
      { id: 7, percent: 100, range: "37.12MB - 43.31MB", active: false },
      { id: 8, percent: 100, range: "43.31MB - 47.20MB", active: false }
    ]
  },
  {
    id: "dl-2",
    filename: "video.mp4",
    category: "video",
    sizeBytes: 55626956,
    downloadedBytes: 55626956,
    status: "Complete",
    speedBytesPerSec: 0,
    savePath: "C:\\Users\\UHD\\Downloads\\Video\\video.mp4",
    url: "https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4",
    referer: "https://youtube.com/watch?v=mock778",
    description: "1080p Web Stream Capture",
    login: "",
    password: "",
    lastTryDate: "Aug 16, 2026 14:02",
    mimeType: "video/mp4",
    connections: 8,
    chunks: []
  },
  {
    id: "dl-3",
    filename: "basic-miktex-25.12-x64.exe",
    category: "programs",
    sizeBytes: 148871577,
    downloadedBytes: 148871577,
    status: "Complete",
    speedBytesPerSec: 0,
    savePath: "C:\\Users\\UHD\\Downloads\\Programs\\basic-miktex-25.12-x64.exe",
    url: "https://miktex.org/download/ctan/systems/win32/miktex/setup/windows-x64/basic-miktex-25.12-x64.exe",
    referer: "https://miktex.org/download",
    description: "LaTeX Distribution Installer",
    login: "",
    password: "",
    lastTryDate: "May 26, 2026 09:12",
    mimeType: "application/x-msdownload",
    connections: 16,
    chunks: []
  },
  {
    id: "dl-4",
    filename: "texstudio-4.9.4-win-qt6.exe",
    category: "programs",
    sizeBytes: 153563955,
    downloadedBytes: 153563955,
    status: "Complete",
    speedBytesPerSec: 0,
    savePath: "C:\\Users\\UHD\\Downloads\\Programs\\texstudio-4.9.4-win-qt6.exe",
    url: "https://github.com/texstudio-org/texstudio/releases/download/4.9.4/texstudio-4.9.4-win-qt6.exe",
    referer: "https://github.com/texstudio-org/texstudio/releases",
    description: "TeXstudio IDE",
    login: "",
    password: "",
    lastTryDate: "May 26, 2026 09:15",
    mimeType: "application/x-msdownload",
    connections: 8,
    chunks: []
  },
  {
    id: "dl-5",
    filename: "EMW_CH8_Summary.zip",
    category: "compressed",
    sizeBytes: 123430502,
    downloadedBytes: 123430502,
    status: "Complete",
    speedBytesPerSec: 0,
    savePath: "C:\\Users\\UHD\\Downloads\\Compressed\\EMW_CH8_Summary.zip",
    url: "https://cdn.university.edu/courses/ee204/EMW_CH8_Summary.zip",
    referer: "https://portal.university.edu/student/courses",
    description: "Electromagnetics Lecture Archives",
    login: "student_uhd",
    password: "••••••••",
    lastTryDate: "May 28, 2026 18:40",
    mimeType: "application/zip",
    connections: 8,
    chunks: []
  },
  {
    id: "dl-6",
    filename: "Chapter-8--The-Z-tran...pdf",
    category: "documents",
    sizeBytes: 769904,
    downloadedBytes: 769904,
    status: "Complete",
    speedBytesPerSec: 0,
    savePath: "C:\\Users\\UHD\\Downloads\\Documents\\Chapter-8--The-Z-transform.pdf",
    url: "https://ocw.mit.edu/resources/res-6-007-signals-and-systems-spring-2011/lecture-notes/MITRES_6_007S11_chap8.pdf",
    referer: "https://ocw.mit.edu/",
    description: "Discrete Signals and Systems Notes",
    login: "",
    password: "",
    lastTryDate: "May 28, 2026 19:02",
    mimeType: "application/pdf",
    connections: 4,
    chunks: []
  },
  {
    id: "dl-7",
    filename: "STARK VARG DRONE C...mp4",
    category: "video",
    sizeBytes: 70664847,
    downloadedBytes: 70664847,
    status: "Complete",
    speedBytesPerSec: 0,
    savePath: "C:\\Users\\UHD\\Downloads\\Video\\STARK_VARG_DRONE_4K.mp4",
    url: "https://streams.redbull.com/manifest/stark-varg/master.m3u8",
    referer: "https://redbull.com/motorsports/varg-drone-raw",
    description: "HLS 4K 60fps Sniffed Stream",
    login: "",
    password: "",
    lastTryDate: "Aug 01, 2026 11:30",
    mimeType: "video/mp4",
    connections: 16,
    chunks: []
  },
  {
    id: "dl-8",
    filename: "helium_0.15.3.1_x64-in...exe",
    category: "programs",
    sizeBytes: 129433600,
    downloadedBytes: 129433600,
    status: "Complete",
    speedBytesPerSec: 0,
    savePath: "C:\\Users\\UHD\\Downloads\\Programs\\helium_0.15.3.1_x64-installer.exe",
    url: "https://cdn.helium-audio.com/releases/helium_0.15.3.1_x64-installer.exe",
    referer: "https://helium-audio.com/download",
    description: "Music Tag & Organizer Studio",
    login: "",
    password: "",
    lastTryDate: "Aug 10, 2026 16:45",
    mimeType: "application/x-msdownload",
    connections: 8,
    chunks: []
  },
  {
    id: "dl-9",
    filename: "Cyberpunk_2077_OST_Flac.zip",
    category: "music",
    sizeBytes: 482344960,
    downloadedBytes: 289406976,
    status: "Downloading",
    speedBytesPerSec: 14200000, // 14.2 MB/s
    savePath: "C:\\Users\\UHD\\Downloads\\Music\\Cyberpunk_2077_OST_Flac.zip",
    url: "https://cdn.highresaudio.com/lossless/cyberpunk_soundtrack_flac24.zip",
    referer: "https://soundtracks.epicgames.com/",
    description: "24-bit 96kHz Lossless Master",
    login: "",
    password: "",
    lastTryDate: "Aug 20, 2026 21:20",
    mimeType: "application/zip",
    connections: 8,
    chunks: [
      { id: 1, percent: 85, range: "0 - 60.2MB", active: true },
      { id: 2, percent: 72, range: "60.2MB - 120.5MB", active: true },
      { id: 3, percent: 64, range: "120.5MB - 180.8MB", active: true },
      { id: 4, percent: 59, range: "180.8MB - 241.1MB", active: true },
      { id: 5, percent: 50, range: "241.1MB - 301.4MB", active: true },
      { id: 6, percent: 45, range: "301.4MB - 361.7MB", active: true },
      { id: 7, percent: 40, range: "361.7MB - 422.0MB", active: true },
      { id: 8, percent: 35, range: "422.0MB - 482.3MB", active: true }
    ]
  }
];

export function getStoredDownloads() {
  const saved = localStorage.getItem("idm_downloads");
  if (saved) {
    try {
      return JSON.parse(saved);
    } catch (e) {
      console.error(e);
    }
  }
  return INITIAL_DOWNLOADS;
}

export function saveStoredDownloads(downloads) {
  localStorage.setItem("idm_downloads", JSON.stringify(downloads));
}

export function getStoredSettings() {
  const saved = localStorage.getItem("idm_settings");
  if (saved) {
    try {
      return JSON.parse(saved);
    } catch (e) {
      console.error(e);
    }
  }
  return INITIAL_SETTINGS;
}

export function saveStoredSettings(settings) {
  localStorage.setItem("idm_settings", JSON.stringify(settings));
}

export function formatBytes(bytes, decimals = 2) {
  if (!bytes || bytes === 0) return "0 Bytes";
  const k = 1024;
  const dm = decimals < 0 ? 0 : decimals;
  const sizes = ["Bytes", "KB", "MB", "GB", "TB"];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + " " + sizes[i];
}

export function formatSpeed(bytesPerSec) {
  if (!bytesPerSec || bytesPerSec === 0) return "0 KB/s";
  if (bytesPerSec > 1024 * 1024) {
    return (bytesPerSec / (1024 * 1024)).toFixed(2) + " MB/s";
  }
  return (bytesPerSec / 1024).toFixed(1) + " KB/s";
}
