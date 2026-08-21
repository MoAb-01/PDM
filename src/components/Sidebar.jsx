import React from "react";
import {
  Folder,
  FolderArchive,
  FileText,
  Music,
  AppWindow,
  Film,
  CheckCircle2,
  Clock,
  Layers,
  ListOrdered,
  ChevronDown,
  ChevronRight
} from "lucide-react";

export default function Sidebar({
  selectedCategory,
  onSelectCategory,
  downloads = []
}) {
  // Compute counts
  const countAll = downloads.length;
  const countCompressed = downloads.filter(d => d.category === "compressed").length;
  const countDocs = downloads.filter(d => d.category === "documents").length;
  const countMusic = downloads.filter(d => d.category === "music").length;
  const countPrograms = downloads.filter(d => d.category === "programs").length;
  const countVideo = downloads.filter(d => d.category === "video").length;
  const countUnfinished = downloads.filter(d => d.status !== "Complete").length;
  const countFinished = downloads.filter(d => d.status === "Complete").length;

  return (
    <div className="idm-sidebar">
      <div className="idm-sidebar-header">
        <span>Categories</span>
        <Folder size={13} color="#8e8e99" />
      </div>

      <div className="idm-tree-section">
        <div
          className={`idm-tree-item ${selectedCategory === "all" ? "active" : ""}`}
          onClick={() => onSelectCategory("all")}
        >
          <Folder size={15} color="#eab308" />
          <span>All Downloads</span>
          <span className="idm-tree-count">{countAll}</span>
        </div>

        <div
          className={`idm-tree-item nested ${selectedCategory === "compressed" ? "active" : ""}`}
          onClick={() => onSelectCategory("compressed")}
        >
          <FolderArchive size={14} color="#f97316" />
          <span>Compressed</span>
          <span className="idm-tree-count">{countCompressed}</span>
        </div>

        <div
          className={`idm-tree-item nested ${selectedCategory === "documents" ? "active" : ""}`}
          onClick={() => onSelectCategory("documents")}
        >
          <FileText size={14} color="#ef4444" />
          <span>Documents</span>
          <span className="idm-tree-count">{countDocs}</span>
        </div>

        <div
          className={`idm-tree-item nested ${selectedCategory === "music" ? "active" : ""}`}
          onClick={() => onSelectCategory("music")}
        >
          <Music size={14} color="#10b981" />
          <span>Music</span>
          <span className="idm-tree-count">{countMusic}</span>
        </div>

        <div
          className={`idm-tree-item nested ${selectedCategory === "programs" ? "active" : ""}`}
          onClick={() => onSelectCategory("programs")}
        >
          <AppWindow size={14} color="#3b82f6" />
          <span>Programs</span>
          <span className="idm-tree-count">{countPrograms}</span>
        </div>

        <div
          className={`idm-tree-item nested ${selectedCategory === "video" ? "active" : ""}`}
          onClick={() => onSelectCategory("video")}
        >
          <Film size={14} color="#a855f7" />
          <span>Video</span>
          <span className="idm-tree-count">{countVideo}</span>
        </div>
      </div>

      <div className="idm-sidebar-header" style={{ marginTop: 8 }}>
        <span>Status Filters</span>
      </div>

      <div className="idm-tree-section">
        <div
          className={`idm-tree-item ${selectedCategory === "unfinished" ? "active" : ""}`}
          onClick={() => onSelectCategory("unfinished")}
        >
          <Clock size={14} color="#eab308" />
          <span>Unfinished</span>
          <span className="idm-tree-count">{countUnfinished}</span>
        </div>

        <div
          className={`idm-tree-item ${selectedCategory === "finished" ? "active" : ""}`}
          onClick={() => onSelectCategory("finished")}
        >
          <CheckCircle2 size={14} color="#22c55e" />
          <span>Finished</span>
          <span className="idm-tree-count">{countFinished}</span>
        </div>

        <div
          className={`idm-tree-item ${selectedCategory === "grabber" ? "active" : ""}`}
          onClick={() => onSelectCategory("grabber")}
        >
          <Layers size={14} color="#06b6d4" />
          <span>Grabber projects</span>
          <span className="idm-tree-count">0</span>
        </div>

        <div
          className={`idm-tree-item ${selectedCategory === "queues" ? "active" : ""}`}
          onClick={() => onSelectCategory("queues")}
        >
          <ListOrdered size={14} color="#ec4899" />
          <span>Queues</span>
          <span className="idm-tree-count">1</span>
        </div>
      </div>
    </div>
  );
}
