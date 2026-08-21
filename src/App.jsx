import React, { useState, useEffect, useRef } from "react";
import Toolbar from "./components/Toolbar";
import Sidebar from "./components/Sidebar";
import DownloadTable from "./components/DownloadTable";
import FilePropertiesModal from "./components/FilePropertiesModal";
import OptionsModal from "./components/OptionsModal";
import ChunkProgressModal from "./components/ChunkProgressModal";
import AddUrlModal from "./components/AddUrlModal";
import SnifferStudio from "./components/SnifferStudio";
import SchedulerModal from "./components/SchedulerModal";
import {
  getStoredDownloads,
  saveStoredDownloads,
  getStoredSettings,
  saveStoredSettings,
  formatBytes,
  formatSpeed
} from "./services/storageService";
import { DownloadEngine } from "./services/downloadEngine";
import {
  Minus,
  Square,
  X,
  Search,
  Zap,
  Activity,
  Radio,
  Sliders,
  Tv,
  CheckCircle2
} from "lucide-react";

export default function App() {
  const [downloads, setDownloads] = useState(getStoredDownloads());
  const [settings, setSettings] = useState(getStoredSettings());
  const [selectedCategory, setSelectedCategory] = useState("all");
  const [selectedId, setSelectedId] = useState(downloads[0]?.id || null);
  const [searchQuery, setSearchQuery] = useState("");

  // Modals state
  const [activeModal, setActiveModal] = useState(null); // "properties" | "options" | "chunks" | "addurl" | "sniffer" | "scheduler"
  const [modalItem, setModalItem] = useState(null);
  const [activeMenu, setActiveMenu] = useState(null);
  const [floatingSnifferVisible, setFloatingSnifferVisible] = useState(true);

  const engineRef = useRef(null);

  // Initialize engine
  useEffect(() => {
    const engine = new DownloadEngine({
      onUpdate: (updatedDownloads) => {
        setDownloads([...updatedDownloads]);
        saveStoredDownloads(updatedDownloads);
      }
    });
    engine.setDownloads(downloads);
    engine.setSpeedLimit(settings.speedLimiterEnabled, settings.speedLimitKBps);
    engine.startTicker();
    engineRef.current = engine;

    return () => {
      engine.stopTicker();
    };
  }, []);

  // Update engine speed limit if settings change
  useEffect(() => {
    if (engineRef.current) {
      engineRef.current.setSpeedLimit(settings.speedLimiterEnabled, settings.speedLimitKBps);
    }
  }, [settings.speedLimiterEnabled, settings.speedLimitKBps]);

  // Selected item reference
  const selectedItem = downloads.find((d) => d.id === selectedId) || null;

  // Filter downloads by category & search query
  const filteredDownloads = downloads.filter((item) => {
    // Search query match
    if (searchQuery.trim()) {
      const q = searchQuery.toLowerCase();
      const matchName = item.filename.toLowerCase().includes(q);
      const matchUrl = item.url.toLowerCase().includes(q);
      const matchDesc = (item.description || "").toLowerCase().includes(q);
      if (!matchName && !matchUrl && !matchDesc) return false;
    }

    // Category match
    if (selectedCategory === "all") return true;
    if (selectedCategory === "unfinished") return item.status !== "Complete";
    if (selectedCategory === "finished") return item.status === "Complete";
    if (selectedCategory === "queues") return item.status === "Queued";
    if (selectedCategory === "grabber") return false;
    return item.category === selectedCategory;
  });

  // Calculate totals
  const totalActiveSpeed = downloads
    .filter((d) => d.status === "Downloading")
    .reduce((acc, curr) => acc + (curr.speedBytesPerSec || 0), 0);

  const totalDownloaded = downloads.reduce((acc, curr) => acc + (curr.downloadedBytes || 0), 0);

  // Actions
  const handleResume = (id) => {
    const targetId = id || selectedId;
    if (!targetId || !engineRef.current) return;
    engineRef.current.startDownload(targetId);
  };

  const handlePause = (id) => {
    const targetId = id || selectedId;
    if (!targetId || !engineRef.current) return;
    engineRef.current.pauseDownload(targetId);
  };

  const handleStopAll = () => {
    if (engineRef.current) engineRef.current.stopAll();
  };

  const handleResumeAll = () => {
    if (engineRef.current) engineRef.current.resumeAll();
  };

  const handleDelete = (id) => {
    const targetId = id || selectedId;
    if (!targetId) return;
    const updated = downloads.filter((d) => d.id !== targetId);
    setDownloads(updated);
    saveStoredDownloads(updated);
    if (engineRef.current) engineRef.current.setDownloads(updated);
    if (selectedId === targetId) {
      setSelectedId(updated[0]?.id || null);
    }
  };

  const handleDeleteCompleted = () => {
    const updated = downloads.filter((d) => d.status !== "Complete");
    setDownloads(updated);
    saveStoredDownloads(updated);
    if (engineRef.current) engineRef.current.setDownloads(updated);
  };

  const handleAddDownload = (newDl) => {
    const updated = [newDl, ...downloads];
    setDownloads(updated);
    saveStoredDownloads(updated);
    setSelectedId(newDl.id);
    if (engineRef.current) {
      engineRef.current.setDownloads(updated);
      if (newDl.status === "Downloading") {
        engineRef.current.startDownload(newDl.id);
      }
    }
  };

  const handleSaveProperties = (updatedItem) => {
    const updated = downloads.map((d) => (d.id === updatedItem.id ? updatedItem : d));
    setDownloads(updated);
    saveStoredDownloads(updated);
    if (engineRef.current) engineRef.current.setDownloads(updated);
  };

  const handleSaveSettings = (newSettings) => {
    setSettings(newSettings);
    saveStoredSettings(newSettings);
  };

  return (
    <div className="idm-app-window" onClick={() => setActiveMenu(null)}>
      {/* 1. Title Bar */}
      <div className="idm-titlebar">
        <div className="idm-title-left">
          <div className="idm-globe-icon" />
          <span>Internet Download Manager 6.42</span>
          <span style={{ color: "#06b6d4", fontSize: "10.5px", fontWeight: "bold", marginLeft: 4 }}>
            [AB Edition • High-Speed Sniffer Engine]
          </span>
        </div>

        <div className="idm-title-controls">
          <button className="idm-win-btn" title="Minimize">
            <Minus size={13} />
          </button>
          <button className="idm-win-btn" title="Maximize">
            <Square size={11} />
          </button>
          <button className="idm-win-btn close" title="Close">
            <X size={13} />
          </button>
        </div>
      </div>

      {/* 2. Menu Bar */}
      <div className="idm-menubar">
        <div
          className={`idm-menu-item ${activeMenu === "tasks" ? "active" : ""}`}
          onClick={(e) => {
            e.stopPropagation();
            setActiveMenu(activeMenu === "tasks" ? null : "tasks");
          }}
        >
          Tasks
          {activeMenu === "tasks" && (
            <div className="idm-dropdown-menu">
              <div
                className="idm-dropdown-entry"
                onClick={() => {
                  setActiveModal("addurl");
                  setActiveMenu(null);
                }}
              >
                Add new download...
              </div>
              <div
                className="idm-dropdown-entry"
                onClick={() => {
                  setActiveModal("sniffer");
                  setActiveMenu(null);
                }}
              >
                Media Sniffer Studio...
              </div>
              <div className="idm-dropdown-divider" />
              <div className="idm-dropdown-entry" onClick={handleStopAll}>
                Stop all downloads
              </div>
              <div className="idm-dropdown-entry" onClick={handleResumeAll}>
                Resume all downloads
              </div>
              <div className="idm-dropdown-divider" />
              <div className="idm-dropdown-entry" onClick={() => alert("Exiting IDM Application")}>
                Exit
              </div>
            </div>
          )}
        </div>

        <div className="idm-menu-item" onClick={() => setActiveModal("addurl")}>
          File
        </div>

        <div
          className={`idm-menu-item ${activeMenu === "downloads" ? "active" : ""}`}
          onClick={(e) => {
            e.stopPropagation();
            setActiveMenu(activeMenu === "downloads" ? null : "downloads");
          }}
        >
          Downloads
          {activeMenu === "downloads" && (
            <div className="idm-dropdown-menu">
              <div
                className="idm-dropdown-entry"
                onClick={() => {
                  setActiveModal("options");
                  setActiveMenu(null);
                }}
              >
                Options...
              </div>
              <div
                className="idm-dropdown-entry"
                onClick={() => {
                  setActiveModal("scheduler");
                  setActiveMenu(null);
                }}
              >
                Scheduler...
              </div>
              <div className="idm-dropdown-entry" onClick={handleDeleteCompleted}>
                Delete completed
              </div>
            </div>
          )}
        </div>

        <div className="idm-menu-item" onClick={() => setActiveModal("options")}>
          View
        </div>

        <div
          className="idm-menu-item"
          onClick={() =>
            alert(
              "Internet Download Manager 6.42 Pro (AB Edition)\nEquipped with Multi-Thread Range Chunking, HLS/DASH Media Sniffer, and Native Browser Integration."
            )
          }
        >
          Help
        </div>

        <div
          className="idm-menu-item"
          style={{ color: "#4ade80", fontWeight: 600 }}
          onClick={() => alert("Registration Status: PRO LICENSE ACTIVATED (Unlimited Speed)")}
        >
          Registration
        </div>
      </div>

      {/* 3. Iconic Toolbar */}
      <Toolbar
        onAddUrl={() => setActiveModal("addurl")}
        onResume={() => handleResume()}
        onStop={() => handlePause()}
        onStopAll={handleStopAll}
        onDelete={() => handleDelete()}
        onDeleteCompleted={handleDeleteCompleted}
        onOptions={() => setActiveModal("options")}
        onScheduler={() => setActiveModal("scheduler")}
        onStartQueue={handleResumeAll}
        onStopQueue={handleStopAll}
        onOpenSniffer={() => setActiveModal("sniffer")}
        selectedItem={selectedItem}
        isSnifferActive={true}
      />

      {/* 4. Quick Search & Real-Time Stats Bar */}
      <div className="idm-filter-bar">
        <div className="idm-search-box">
          <Search size={13} color="#888" />
          <input
            type="text"
            className="idm-search-input"
            placeholder="Search downloads by name, URL or notes..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
          />
        </div>

        <div className="idm-quick-stats">
          <div className="stat-item">
            <span>Total Speed:</span>
            <span className="stat-val" style={{ color: totalActiveSpeed > 0 ? "#38bdf8" : "#888" }}>
              {formatSpeed(totalActiveSpeed)}
            </span>
          </div>
          <div className="stat-item">
            <span>Total Transferred:</span>
            <span className="stat-val">{formatBytes(totalDownloaded)}</span>
          </div>
          <div className="stat-item">
            <span>Acceleration:</span>
            <span className="stat-val" style={{ color: "#4ade80" }}>
              {settings.maxConnections}x Parallel Chunks
            </span>
          </div>
        </div>
      </div>

      {/* 5. Workspace Area: Sidebar + Data Table */}
      <div className="idm-workspace">
        <Sidebar
          selectedCategory={selectedCategory}
          onSelectCategory={setSelectedCategory}
          downloads={downloads}
        />

        <div className="idm-content-pane">
          <DownloadTable
            downloads={filteredDownloads}
            selectedId={selectedId}
            onSelectId={setSelectedId}
            onOpenProperties={(item) => {
              setModalItem(item);
              setActiveModal("properties");
            }}
            onOpenChunks={(item) => {
              setModalItem(item);
              setActiveModal("chunks");
            }}
            onResumeDownload={handleResume}
            onPauseDownload={handlePause}
            onDeleteDownload={handleDelete}
          />
        </div>
      </div>

      {/* 6. Status Bar */}
      <div className="idm-statusbar">
        <div className="idm-status-left">
          <span>
            {downloads.length} downloads in library ({downloads.filter((d) => d.status === "Complete").length} completed)
          </span>
          <span>•</span>
          <span style={{ color: "#38bdf8" }}>
            Browser Sniffer Integration: <strong>ENABLED (Chrome, Edge, Firefox)</strong>
          </span>
        </div>

        <div className="idm-status-right">
          <div
            className="speed-limiter-toggle"
            onClick={() =>
              handleSaveSettings({
                ...settings,
                speedLimiterEnabled: !settings.speedLimiterEnabled
              })
            }
          >
            <Zap
              size={13}
              color={settings.speedLimiterEnabled ? "#eab308" : "#666"}
              className={settings.speedLimiterEnabled ? "animate-pulse" : ""}
            />
            <span style={{ color: settings.speedLimiterEnabled ? "#fde047" : "#8e8e98" }}>
              Speed Limiter: {settings.speedLimiterEnabled ? `${settings.speedLimitKBps} KB/s` : "OFF (Max Speed)"}
            </span>
          </div>
        </div>
      </div>

      {/* Floating Web Sniffer Pill Simulation */}
      {floatingSnifferVisible && (
        <div
          className="sniffer-floating-pill"
          onClick={() => setActiveModal("sniffer")}
          title="Click to view captured video streams (.m3u8, .mpd, .mp4)"
        >
          <Radio size={16} color="#38bdf8" className="animate-pulse" />
          <div>
            <div style={{ fontSize: "11px", fontWeight: "bold", color: "#f8fafc" }}>
              Media Sniffer Active
            </div>
            <div style={{ fontSize: "9.5px", color: "#94a3b8" }}>
              4 Web Streams Detected
            </div>
          </div>
          <button
            style={{
              background: "transparent",
              border: "none",
              color: "#64748b",
              cursor: "pointer",
              marginLeft: 4
            }}
            onClick={(e) => {
              e.stopPropagation();
              setFloatingSnifferVisible(false);
            }}
          >
            <X size={12} />
          </button>
        </div>
      )}

      {/* Modals */}
      {activeModal === "properties" && (
        <FilePropertiesModal
          item={modalItem || selectedItem}
          onClose={() => setActiveModal(null)}
          onSave={handleSaveProperties}
          onOpenFile={(item) => alert(`Opening file: ${item.savePath}`)}
        />
      )}

      {activeModal === "options" && (
        <OptionsModal
          settings={settings}
          onClose={() => setActiveModal(null)}
          onSave={handleSaveSettings}
        />
      )}

      {activeModal === "chunks" && (
        <ChunkProgressModal
          item={modalItem || selectedItem}
          onClose={() => setActiveModal(null)}
          onPause={handlePause}
          onResume={handleResume}
        />
      )}

      {activeModal === "addurl" && (
        <AddUrlModal
          categories={settings.categories}
          onClose={() => setActiveModal(null)}
          onAddDownload={handleAddDownload}
        />
      )}

      {activeModal === "sniffer" && (
        <SnifferStudio
          onClose={() => setActiveModal(null)}
          onSendToDownloader={handleAddDownload}
        />
      )}

      {activeModal === "scheduler" && (
        <SchedulerModal
          onClose={() => setActiveModal(null)}
          onSave={() => setActiveModal(null)}
        />
      )}
    </div>
  );
}
