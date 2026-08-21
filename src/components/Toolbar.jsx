import React from "react";
import {
  Plus,
  Play,
  Square,
  Octagon,
  Trash2,
  CheckCheck,
  Settings,
  Clock,
  PlayCircle,
  StopCircle,
  Radio,
  Share2,
  Tv
} from "lucide-react";

export default function Toolbar({
  onAddUrl,
  onResume,
  onStop,
  onStopAll,
  onDelete,
  onDeleteCompleted,
  onOptions,
  onScheduler,
  onStartQueue,
  onStopQueue,
  onOpenSniffer,
  selectedItem,
  isSnifferActive
}) {
  return (
    <div className="idm-toolbar">
      <button className="idm-tool-btn" onClick={onAddUrl} title="Add New URL Download">
        <div className="idm-tool-icon-wrapper" style={{ background: "rgba(34, 197, 94, 0.2)", color: "#4ade80" }}>
          <Plus size={18} strokeWidth={2.5} />
        </div>
        <span>Add URL</span>
      </button>

      <button
        className="idm-tool-btn"
        onClick={onResume}
        disabled={!selectedItem || selectedItem.status === "Complete" || selectedItem.status === "Downloading"}
        title="Resume Selected Download"
      >
        <div className="idm-tool-icon-wrapper" style={{ background: "rgba(56, 189, 248, 0.2)", color: "#38bdf8" }}>
          <Play size={18} />
        </div>
        <span>Resume</span>
      </button>

      <button
        className="idm-tool-btn"
        onClick={onStop}
        disabled={!selectedItem || selectedItem.status !== "Downloading"}
        title="Stop / Pause Selected Download"
      >
        <div className="idm-tool-icon-wrapper" style={{ background: "rgba(245, 158, 11, 0.2)", color: "#fbbf24" }}>
          <Square size={16} />
        </div>
        <span>Stop</span>
      </button>

      <button className="idm-tool-btn" onClick={onStopAll} title="Stop All Active Downloads">
        <div className="idm-tool-icon-wrapper" style={{ background: "rgba(239, 68, 68, 0.2)", color: "#f87171" }}>
          <Octagon size={18} />
        </div>
        <span>Stop All</span>
      </button>

      <div className="idm-toolbar-divider" />

      <button
        className="idm-tool-btn"
        onClick={onDelete}
        disabled={!selectedItem}
        title="Delete Selected Download"
      >
        <div className="idm-tool-icon-wrapper" style={{ background: "rgba(239, 68, 68, 0.15)", color: "#f87171" }}>
          <Trash2 size={17} />
        </div>
        <span>Delete</span>
      </button>

      <button className="idm-tool-btn" onClick={onDeleteCompleted} title="Delete All Completed Downloads">
        <div className="idm-tool-icon-wrapper" style={{ background: "rgba(168, 85, 247, 0.2)", color: "#c084fc" }}>
          <CheckCheck size={17} />
        </div>
        <span>Delete Co...</span>
      </button>

      <div className="idm-toolbar-divider" />

      <button className="idm-tool-btn" onClick={onOptions} title="Internet Download Manager Configuration & Options">
        <div className="idm-tool-icon-wrapper" style={{ background: "rgba(148, 163, 184, 0.2)", color: "#cbd5e1" }}>
          <Settings size={18} />
        </div>
        <span>Options</span>
      </button>

      <button className="idm-tool-btn" onClick={onScheduler} title="Scheduler & Download Queues">
        <div className="idm-tool-icon-wrapper" style={{ background: "rgba(236, 72, 153, 0.2)", color: "#f472b6" }}>
          <Clock size={18} />
        </div>
        <span>Scheduler</span>
      </button>

      <button className="idm-tool-btn" onClick={onStartQueue} title="Start Main Queue">
        <div className="idm-tool-icon-wrapper" style={{ background: "rgba(34, 197, 94, 0.2)", color: "#4ade80" }}>
          <PlayCircle size={18} />
        </div>
        <span>Start Qu...</span>
      </button>

      <button className="idm-tool-btn" onClick={onStopQueue} title="Stop Main Queue">
        <div className="idm-tool-icon-wrapper" style={{ background: "rgba(239, 68, 68, 0.2)", color: "#f87171" }}>
          <StopCircle size={18} />
        </div>
        <span>Stop Qu...</span>
      </button>

      <div className="idm-toolbar-divider" />

      <button
        className="idm-tool-btn"
        onClick={onOpenSniffer}
        style={{
          background: isSnifferActive ? "rgba(6, 182, 212, 0.25)" : "transparent",
          borderColor: isSnifferActive ? "#06b6d4" : "transparent"
        }}
        title="Open Media Sniffer & Video Stream Capture Lab"
      >
        <div className="idm-tool-icon-wrapper" style={{ background: "rgba(6, 182, 212, 0.2)", color: "#22d3ee" }}>
          <Radio size={18} className={isSnifferActive ? "animate-pulse" : ""} />
        </div>
        <span style={{ color: "#22d3ee", fontWeight: 600 }}>Media Sniffer</span>
      </button>
    </div>
  );
}
