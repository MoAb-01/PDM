import React, { useState } from "react";
import { X, Globe, Monitor, FileCode, FolderArchive, Download, Wifi, KeyRound, Shield, Volume2, Plus, Trash2 } from "lucide-react";

export default function OptionsModal({ settings, onClose, onSave }) {
  const [activeTab, setActiveTab] = useState("general");
  const [formData, setFormData] = useState({ ...settings });
  const [selectedCategoryKey, setSelectedCategoryKey] = useState("general");

  const handleSave = () => {
    onSave(formData);
    onClose();
  };

  const currentCategory = formData.categories.find(c => c.id === selectedCategoryKey) || formData.categories[0];

  const updateCategoryFolder = (folder) => {
    const updated = formData.categories.map(c => c.id === selectedCategoryKey ? { ...c, folder } : c);
    setFormData({ ...formData, categories: updated });
  };

  const updateCategoryExt = (extensions) => {
    const updated = formData.categories.map(c => c.id === selectedCategoryKey ? { ...c, extensions } : c);
    setFormData({ ...formData, categories: updated });
  };

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div
        className="idm-dialog"
        style={{ width: "490px", maxHeight: "90vh", borderRadius: "4px" }}
        onClick={(e) => e.stopPropagation()}
      >
        {/* Title Bar */}
        <div className="idm-dialog-titlebar">
          <div className="idm-dialog-title-text">
            <div className="idm-globe-icon" />
            <span style={{ fontWeight: 600, fontSize: "12px" }}>Internet Download Manager Configuration</span>
          </div>
          <button className="idm-dialog-close" onClick={onClose}>
            <X size={14} />
          </button>
        </div>

        {/* Secondary Upper Tabs Row */}
        <div className="idm-tab-bar" style={{ background: "#dedede", borderBottom: "1px solid #bcbcbc", paddingTop: 2 }}>
          <button
            className={`idm-tab-btn ${activeTab === "proxy" ? "active" : ""}`}
            onClick={() => setActiveTab("proxy")}
          >
            Proxy / Socks
          </button>
          <button
            className={`idm-tab-btn ${activeTab === "sites" ? "active" : ""}`}
            onClick={() => setActiveTab("sites")}
          >
            Sites Logins
          </button>
          <button
            className={`idm-tab-btn ${activeTab === "dialup" ? "active" : ""}`}
            onClick={() => setActiveTab("dialup")}
          >
            Dial Up / VPN
          </button>
          <button
            className={`idm-tab-btn ${activeTab === "sounds" ? "active" : ""}`}
            onClick={() => setActiveTab("sounds")}
          >
            Sounds
          </button>
        </div>

        {/* Primary Tab Headers Row */}
        <div className="idm-tab-bar">
          <button
            className={`idm-tab-btn ${activeTab === "general" ? "active" : ""}`}
            onClick={() => setActiveTab("general")}
          >
            General
          </button>
          <button
            className={`idm-tab-btn ${activeTab === "filetypes" ? "active" : ""}`}
            onClick={() => setActiveTab("filetypes")}
          >
            File types
          </button>
          <button
            className={`idm-tab-btn ${activeTab === "saveto" ? "active" : ""}`}
            onClick={() => setActiveTab("saveto")}
          >
            Save to
          </button>
          <button
            className={`idm-tab-btn ${activeTab === "downloads" ? "active" : ""}`}
            onClick={() => setActiveTab("downloads")}
          >
            Downloads
          </button>
          <button
            className={`idm-tab-btn ${activeTab === "connection" ? "active" : ""}`}
            onClick={() => setActiveTab("connection")}
          >
            Connection
          </button>
        </div>

        {/* Tab Body */}
        <div className="idm-dialog-body" style={{ minHeight: 380 }}>
          {/* 1. GENERAL TAB */}
          {activeTab === "general" && (
            <div>
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 8 }}>
                <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
                  <Monitor size={24} color="#0284c7" />
                  <span style={{ fontSize: "11.5px", fontWeight: "bold" }}>Browser/System Integration</span>
                </div>
              </div>

              <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", margin: "10px 0 14px" }}>
                <span style={{ fontSize: "11px", color: "#222" }}>Advanced browser integration is enabled</span>
                <button className="idm-btn" onClick={() => alert("Browser integration restarted successfully.")}>
                  Restart
                </button>
              </div>

              <div style={{ display: "flex", flexDirection: "column", gap: 6, marginBottom: 14 }}>
                <label style={{ display: "flex", alignItems: "center", gap: 6, fontSize: "11.5px", cursor: "pointer" }}>
                  <input
                    type="checkbox"
                    checked={formData.launchOnStartup}
                    onChange={(e) => setFormData({ ...formData, launchOnStartup: e.target.checked })}
                  />
                  <span>Launch Internet Download Manager on startup</span>
                </label>

                <label style={{ display: "flex", alignItems: "center", gap: 6, fontSize: "11.5px", cursor: "pointer" }}>
                  <input
                    type="checkbox"
                    checked={formData.clipboardAutoDetect}
                    onChange={(e) => setFormData({ ...formData, clipboardAutoDetect: e.target.checked })}
                  />
                  <span>Automatically start downloading of URLs placed to clipboard</span>
                </label>
              </div>

              <fieldset style={{ border: "1px solid #bcbcbc", padding: "8px 12px", borderRadius: 2, marginBottom: 14 }}>
                <legend style={{ padding: "0 4px", fontSize: "11px", color: "#333" }}>Capture downloads from the following browsers:</legend>
                <div style={{ maxHeight: 110, overflowY: "auto", display: "flex", flexDirection: "column", gap: 4, padding: "4px 0" }}>
                  {formData.capturedBrowsers.map((b, idx) => (
                    <label key={b.name} style={{ display: "flex", alignItems: "center", gap: 6, fontSize: "11px", cursor: "pointer" }}>
                      <input
                        type="checkbox"
                        checked={b.enabled}
                        onChange={(e) => {
                          const updated = [...formData.capturedBrowsers];
                          updated[idx] = { ...b, enabled: e.target.checked };
                          setFormData({ ...formData, capturedBrowsers: updated });
                        }}
                      />
                      <span>{b.name}</span>
                    </label>
                  ))}
                </div>
                <div style={{ display: "flex", justifyContent: "flex-end", marginTop: 4 }}>
                  <button className="idm-btn" onClick={() => alert("Add Browser Executable (.exe)")}>
                    Add browser...
                  </button>
                </div>
              </fieldset>

              <div style={{ display: "flex", flexDirection: "column", gap: 8 }}>
                <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between" }}>
                  <span style={{ fontSize: "11px" }}>Customize keys to prevent or force downloading with IDM</span>
                  <button className="idm-btn" onClick={() => alert("Keys config: Hold ALT to force download, Hold CTRL to skip IDM.")}>
                    Keys...
                  </button>
                </div>
                <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between" }}>
                  <span style={{ fontSize: "11px" }}>Customize IDM menu items in context menu of browsers</span>
                  <button className="idm-btn" onClick={() => alert("Context menu options configured.")}>
                    Edit...
                  </button>
                </div>
                <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between" }}>
                  <span style={{ fontSize: "11px" }}>Customize IDM Download panels in browsers</span>
                  <button className="idm-btn" onClick={() => alert("Video download floating panel configured.")}>
                    Edit...
                  </button>
                </div>
              </div>
            </div>
          )}

          {/* 2. FILE TYPES TAB */}
          {activeTab === "filetypes" && (
            <div>
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 8 }}>
                <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
                  <FileCode size={24} color="#f59e0b" />
                  <span style={{ fontSize: "11.5px", fontWeight: "bold" }}>Downloaded file types</span>
                </div>
              </div>

              <div style={{ marginTop: 10 }}>
                <div style={{ fontSize: "11.5px", marginBottom: 4 }}>Automatically start downloading the following file types:</div>
                <textarea
                  style={{
                    width: "100%",
                    height: 75,
                    fontSize: "11px",
                    fontFamily: "var(--font-mono)",
                    padding: 6,
                    border: "1px solid #7a7a7a",
                    boxSizing: "border-box"
                  }}
                  value={formData.autoCaptureExtensions}
                  onChange={(e) => setFormData({ ...formData, autoCaptureExtensions: e.target.value })}
                />
                <div style={{ display: "flex", justifyContent: "flex-end", marginTop: 4 }}>
                  <button
                    className="idm-btn"
                    onClick={() => setFormData({
                      ...formData,
                      autoCaptureExtensions: "3GP 7Z AAC ACE AIF APK ARJ ASF AVI BIN BZ2 EXE GZ GZIP IMG ISO LZH M4A M4V MKV MOV MP3 MP4 MPA MPE MPEG MPG MSI MSU OGG OGV PDF PLJ PPS PPT QT R0* R1* RA RAR RM RMVB SEA SIT SITX TAR TIF TIFF WAV WMA WMV Z ZIP TS M3U8 MPD"
                    })}
                  >
                    Default
                  </button>
                </div>
              </div>

              <div style={{ marginTop: 12 }}>
                <div style={{ fontSize: "11.5px", marginBottom: 2 }}>Don't start downloading automatically from the following sites:</div>
                <textarea
                  style={{
                    width: "100%",
                    height: 65,
                    fontSize: "11px",
                    fontFamily: "var(--font-mono)",
                    padding: 6,
                    border: "1px solid #7a7a7a",
                    boxSizing: "border-box"
                  }}
                  value={formData.excludedSites}
                  onChange={(e) => setFormData({ ...formData, excludedSites: e.target.value })}
                />
                <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginTop: 4 }}>
                  <span style={{ fontSize: "10.5px", color: "#666" }}>(separate names by spaces)</span>
                  <button
                    className="idm-btn"
                    onClick={() => setFormData({
                      ...formData,
                      excludedSites: "*.update.microsoft.com download.windowsupdate.com *.download.windowsupdate.com siteseal.thawte.com ecom.cimetz.com *.voice2page.com"
                    })}
                  >
                    Default
                  </button>
                </div>
              </div>

              <div style={{ marginTop: 14 }}>
                <div style={{ fontSize: "11.5px", marginBottom: 4 }}>Don't start downloading automatically from the following addresses:</div>
                <button className="idm-btn" onClick={() => alert("Address exception rules list")}>
                  Edit list ...
                </button>
              </div>
            </div>
          )}

          {/* 3. SAVE TO TAB */}
          {activeTab === "saveto" && (
            <div>
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 8 }}>
                <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
                  <FolderArchive size={24} color="#eab308" />
                  <span style={{ fontSize: "11.5px", fontWeight: "bold" }}>Categories, file types, folders</span>
                </div>
              </div>

              <fieldset style={{ border: "1px solid #bcbcbc", padding: "10px 14px", borderRadius: 2, marginBottom: 14 }}>
                <legend style={{ padding: "0 4px", fontSize: "11px", color: "#333" }}>Save To...</legend>

                <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 10 }}>
                  <span style={{ width: 80, fontSize: "11.5px" }}>Category</span>
                  <select
                    style={{ flex: 1, height: 24, padding: "0 6px" }}
                    value={selectedCategoryKey}
                    onChange={(e) => setSelectedCategoryKey(e.target.value)}
                  >
                    {formData.categories.map(c => (
                      <option key={c.id} value={c.id}>{c.name}</option>
                    ))}
                  </select>
                  <button className="idm-btn primary" onClick={() => alert("Create new category dialog")}>
                    New
                  </button>
                  <button className="idm-btn" onClick={() => alert("Edit category rules")}>
                    Edit...
                  </button>
                </div>

                <div style={{ marginBottom: 10 }}>
                  <div style={{ fontSize: "11px", color: "#333", marginBottom: 3 }}>
                    Automatically put in "{currentCategory.name}" category the following file types:
                  </div>
                  <input
                    type="text"
                    className="prop-input"
                    style={{ width: "100%" }}
                    value={currentCategory.extensions}
                    onChange={(e) => updateCategoryExt(e.target.value)}
                  />
                </div>

                <div style={{ marginBottom: 10 }}>
                  <div style={{ fontSize: "11px", color: "#333", marginBottom: 3 }}>
                    Default download directory for "{currentCategory.name}" category
                  </div>
                  <div style={{ display: "flex", gap: 6 }}>
                    <input
                      type="text"
                      className="prop-input"
                      style={{ flex: 1 }}
                      value={currentCategory.folder}
                      onChange={(e) => updateCategoryFolder(e.target.value)}
                    />
                    <button className="idm-btn" onClick={() => alert("Browse directory")}>
                      Browse
                    </button>
                  </div>
                </div>

                <label style={{ display: "flex", alignItems: "center", gap: 6, fontSize: "11px", cursor: "pointer", marginTop: 6 }}>
                  <input
                    type="checkbox"
                    checked={formData.changeFolderOnLastSelected}
                    onChange={(e) => setFormData({ ...formData, changeFolderOnLastSelected: e.target.checked })}
                  />
                  <span>Change folder for "{currentCategory.name}" category on last selected</span>
                </label>
              </fieldset>

              <label style={{ display: "flex", alignItems: "center", gap: 6, fontSize: "11.5px", cursor: "pointer", marginBottom: 12 }}>
                <input
                  type="checkbox"
                  checked={formData.setCreationDateFromServer}
                  onChange={(e) => setFormData({ ...formData, setCreationDateFromServer: e.target.checked })}
                />
                <span>Set file creation date as provided by the server</span>
              </label>

              <fieldset style={{ border: "1px solid #bcbcbc", padding: "10px 14px", borderRadius: 2 }}>
                <legend style={{ padding: "0 4px", fontSize: "11px", color: "#333" }}>Temporary directory</legend>
                <div style={{ display: "flex", gap: 6, marginBottom: 6 }}>
                  <input
                    type="text"
                    className="prop-input"
                    style={{ flex: 1 }}
                    value={formData.tempDirectory}
                    onChange={(e) => setFormData({ ...formData, tempDirectory: e.target.value })}
                  />
                  <button className="idm-btn" onClick={() => alert("Browse temporary directory")}>
                    Browse
                  </button>
                </div>
                <div style={{ fontSize: "10.5px", color: "#555", lineHeight: 1.3 }}>
                  Temporary directory is required for storing file parts during download. If you have several physical drives on your computer, you should select different physical drives for temporary directory and "Save To" folders for faster assembling of downloaded files.
                </div>
              </fieldset>
            </div>
          )}

          {/* 4. DOWNLOADS TAB */}
          {activeTab === "downloads" && (
            <div>
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 8 }}>
                <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
                  <Download size={24} color="#16a34a" />
                  <span style={{ fontSize: "11.5px", fontWeight: "bold" }}>Default download settings</span>
                </div>
              </div>

              <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", margin: "6px 0 10px" }}>
                <span style={{ fontSize: "11px" }}>Customize "Download progress" dialog</span>
                <button className="idm-btn" onClick={() => alert("Progress dialog skin configured.")}>
                  Edit...
                </button>
              </div>

              <div style={{ display: "flex", flexDirection: "column", gap: 6, marginBottom: 12 }}>
                <label style={{ display: "flex", alignItems: "center", gap: 6, fontSize: "11.5px", cursor: "pointer" }}>
                  <input
                    type="checkbox"
                    checked={formData.showStartDialog}
                    onChange={(e) => setFormData({ ...formData, showStartDialog: e.target.checked })}
                  />
                  <span>Show start download dialog</span>
                </label>

                <label style={{ display: "flex", alignItems: "center", gap: 6, fontSize: "11.5px", cursor: "pointer" }}>
                  <input
                    type="checkbox"
                    checked={formData.showCompleteDialog}
                    onChange={(e) => setFormData({ ...formData, showCompleteDialog: e.target.checked })}
                  />
                  <span>Show download complete dialog</span>
                </label>

                <div style={{ fontSize: "10.5px", color: "#666", marginLeft: 20 }}>
                  Note: These settings don't relate to queue processing
                </div>
              </div>

              <hr style={{ borderColor: "#bcbcbc", margin: "8px 0 12px" }} />

              <div style={{ display: "flex", flexDirection: "column", gap: 6, marginBottom: 12 }}>
                <label style={{ display: "flex", alignItems: "center", gap: 6, fontSize: "11.5px", cursor: "pointer" }}>
                  <input
                    type="checkbox"
                    checked={formData.startImmediately}
                    onChange={(e) => setFormData({ ...formData, startImmediately: e.target.checked })}
                  />
                  <span>Start downloading immediately while displaying "Download File Info" dialog</span>
                </label>

                <label style={{ display: "flex", alignItems: "center", gap: 6, fontSize: "11.5px", cursor: "pointer" }}>
                  <input
                    type="checkbox"
                    checked={formData.showQueueSelectionOnLater}
                    onChange={(e) => setFormData({ ...formData, showQueueSelectionOnLater: e.target.checked })}
                  />
                  <span>Show queue selection panel on pressing "Download Later" button</span>
                </label>

                <label style={{ display: "flex", alignItems: "center", gap: 6, fontSize: "11.5px", cursor: "pointer" }}>
                  <input
                    type="checkbox"
                    checked={formData.showQueueSelectionOnBatchClose}
                    onChange={(e) => setFormData({ ...formData, showQueueSelectionOnBatchClose: e.target.checked })}
                  />
                  <span>Show queue selection panel on closing batch download dialogs</span>
                </label>

                <label style={{ display: "flex", alignItems: "center", gap: 6, fontSize: "11.5px", cursor: "pointer" }}>
                  <input
                    type="checkbox"
                    checked={formData.ignoreFileModTimeResuming}
                    onChange={(e) => setFormData({ ...formData, ignoreFileModTimeResuming: e.target.checked })}
                  />
                  <span>Ignore file modification time changes when resuming a download</span>
                </label>
              </div>

              <div style={{ marginTop: 10 }}>
                <div style={{ fontSize: "11px", marginBottom: 3 }}>If a duplicate download link is added:</div>
                <select
                  style={{ width: "100%", height: 24, padding: "0 6px" }}
                  value={formData.duplicateAction}
                  onChange={(e) => setFormData({ ...formData, duplicateAction: e.target.value })}
                >
                  <option value="ask">Show a dialog and ask what to do.</option>
                  <option value="overwrite">Overwrite existing file</option>
                  <option value="rename">Create new file with duplicate index</option>
                </select>
              </div>

              <div style={{ marginTop: 10 }}>
                <div style={{ fontSize: "11px", marginBottom: 3 }}>User-Agent for manually added downloads:</div>
                <input
                  type="text"
                  className="prop-input"
                  style={{ width: "100%" }}
                  value={formData.userAgent}
                  onChange={(e) => setFormData({ ...formData, userAgent: e.target.value })}
                />
              </div>

              <div style={{ display: "flex", justifyContent: "flex-end", alignItems: "center", gap: 8, marginTop: 10 }}>
                <span style={{ fontSize: "11px" }}>Virus checking settings</span>
                <button className="idm-btn" onClick={() => alert("Antivirus scanner path configuration")}>
                  Edit...
                </button>
              </div>
            </div>
          )}

          {/* 5. CONNECTION TAB */}
          {activeTab === "connection" && (
            <div>
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 8 }}>
                <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
                  <Wifi size={24} color="#3b82f6" />
                  <span style={{ fontSize: "11.5px", fontWeight: "bold" }}>Connection Type / Speed</span>
                </div>
              </div>

              <div style={{ marginTop: 12 }}>
                <div style={{ fontSize: "11px", marginBottom: 4 }}>Connection Type / Speed:</div>
                <select
                  style={{ width: "100%", height: 26, padding: "0 6px" }}
                  value={formData.connectionType}
                  onChange={(e) => setFormData({ ...formData, connectionType: e.target.value })}
                >
                  <option value="High Speed (Direct connection / 5G / Fiber)">High Speed (Direct connection / 5G / Fiber)</option>
                  <option value="Medium (DSL / 4G LTE)">Medium (DSL / 4G LTE)</option>
                  <option value="Low Speed (Dial-Up / 3G)">Low Speed (Dial-Up / 3G)</option>
                </select>
              </div>

              <div style={{ marginTop: 14 }}>
                <div style={{ fontSize: "11px", marginBottom: 4 }}>Default max. conn. number:</div>
                <select
                  style={{ width: 140, height: 26, padding: "0 6px" }}
                  value={formData.maxConnections}
                  onChange={(e) => setFormData({ ...formData, maxConnections: parseInt(e.target.value, 10) })}
                >
                  <option value="1">1 connection</option>
                  <option value="2">2 connections</option>
                  <option value="4">4 connections</option>
                  <option value="8">8 connections</option>
                  <option value="16">16 connections (Recommended)</option>
                  <option value="24">24 connections</option>
                  <option value="32">32 connections (Maximum Acceleration)</option>
                </select>
                <div style={{ fontSize: "10.5px", color: "#666", marginTop: 4 }}>
                  IDM opens multiple parallel connections to download file chunks concurrently.
                </div>
              </div>

              <fieldset style={{ border: "1px solid #bcbcbc", padding: "10px 14px", borderRadius: 2, marginTop: 16 }}>
                <legend style={{ padding: "0 4px", fontSize: "11px", color: "#333" }}>Speed Limiter</legend>
                <label style={{ display: "flex", alignItems: "center", gap: 6, fontSize: "11.5px", cursor: "pointer", marginBottom: 8 }}>
                  <input
                    type="checkbox"
                    checked={formData.speedLimiterEnabled}
                    onChange={(e) => setFormData({ ...formData, speedLimiterEnabled: e.target.checked })}
                  />
                  <span>Enable Speed Limiter</span>
                </label>
                <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
                  <span style={{ fontSize: "11px" }}>Maximum download speed:</span>
                  <input
                    type="number"
                    style={{ width: 90, height: 24, padding: "0 6px" }}
                    value={formData.speedLimitKBps}
                    onChange={(e) => setFormData({ ...formData, speedLimitKBps: parseInt(e.target.value, 10) || 0 })}
                  />
                  <span style={{ fontSize: "11px" }}>KBytes/sec</span>
                </div>
              </fieldset>
            </div>
          )}

          {/* 6. SITES LOGINS TAB */}
          {activeTab === "sites" && (
            <div>
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 8 }}>
                <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
                  <KeyRound size={24} color="#8b5cf6" />
                  <span style={{ fontSize: "11.5px", fontWeight: "bold" }}>Site Logins & Authentication</span>
                </div>
              </div>

              <div style={{ fontSize: "11px", color: "#444", marginBottom: 8 }}>
                IDM can supply login credentials automatically to password protected servers:
              </div>

              <div style={{ border: "1px solid #bcbcbc", maxHeight: 180, overflowY: "auto", background: "#fff" }}>
                <table style={{ width: "100%", borderCollapse: "collapse", fontSize: "11px" }}>
                  <thead>
                    <tr style={{ background: "#f0f0f0", borderBottom: "1px solid #ddd" }}>
                      <th style={{ padding: "4px 8px", textAlign: "left" }}>Server / Domain</th>
                      <th style={{ padding: "4px 8px", textAlign: "left" }}>User / Login</th>
                      <th style={{ padding: "4px 8px", textAlign: "left" }}>Password</th>
                    </tr>
                  </thead>
                  <tbody>
                    {formData.savedSiteLogins.map((site, i) => (
                      <tr key={i} style={{ borderBottom: "1px solid #eee" }}>
                        <td style={{ padding: "4px 8px" }}>{site.url}</td>
                        <td style={{ padding: "4px 8px" }}>{site.username}</td>
                        <td style={{ padding: "4px 8px" }}>{site.password}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>

              <div style={{ display: "flex", gap: 8, marginTop: 10 }}>
                <button
                  className="idm-btn primary"
                  onClick={() => {
                    const domain = prompt("Enter Server Hostname (e.g. drive.google.com):");
                    const user = prompt("Enter Username:");
                    if (domain && user) {
                      setFormData({
                        ...formData,
                        savedSiteLogins: [...formData.savedSiteLogins, { url: domain, username: user, password: "••••••••" }]
                      });
                    }
                  }}
                >
                  <Plus size={12} /> New
                </button>
                <button className="idm-btn" onClick={() => alert("Edit selected credentials")}>
                  Edit
                </button>
                <button
                  className="idm-btn"
                  onClick={() => {
                    if (formData.savedSiteLogins.length > 0) {
                      setFormData({
                        ...formData,
                        savedSiteLogins: formData.savedSiteLogins.slice(0, -1)
                      });
                    }
                  }}
                >
                  <Trash2 size={12} /> Delete
                </button>
              </div>
            </div>
          )}

          {/* 7. PROXY TAB */}
          {activeTab === "proxy" && (
            <div>
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 8 }}>
                <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
                  <Shield size={24} color="#06b6d4" />
                  <span style={{ fontSize: "11.5px", fontWeight: "bold" }}>Proxy / Socks Configuration</span>
                </div>
              </div>

              <label style={{ display: "flex", alignItems: "center", gap: 6, fontSize: "11.5px", cursor: "pointer", margin: "12px 0" }}>
                <input
                  type="checkbox"
                  checked={formData.proxyEnabled}
                  onChange={(e) => setFormData({ ...formData, proxyEnabled: e.target.checked })}
                />
                <span>Use HTTP / HTTPS / SOCKS proxy server</span>
              </label>

              <div style={{ display: "flex", flexDirection: "column", gap: 8, opacity: formData.proxyEnabled ? 1 : 0.5 }}>
                <div className="prop-row">
                  <div className="prop-label">Proxy Type:</div>
                  <select
                    style={{ flex: 1, height: 24 }}
                    value={formData.proxyType}
                    disabled={!formData.proxyEnabled}
                    onChange={(e) => setFormData({ ...formData, proxyType: e.target.value })}
                  >
                    <option value="HTTP">HTTP</option>
                    <option value="HTTPS">HTTPS</option>
                    <option value="SOCKS4">SOCKS4</option>
                    <option value="SOCKS5">SOCKS5</option>
                  </select>
                </div>

                <div className="prop-row">
                  <div className="prop-label">Host / IP:</div>
                  <input
                    type="text"
                    className="prop-input"
                    disabled={!formData.proxyEnabled}
                    value={formData.proxyHost}
                    onChange={(e) => setFormData({ ...formData, proxyHost: e.target.value })}
                  />
                </div>

                <div className="prop-row">
                  <div className="prop-label">Port:</div>
                  <input
                    type="text"
                    className="prop-input"
                    style={{ maxWidth: 90 }}
                    disabled={!formData.proxyEnabled}
                    value={formData.proxyPort}
                    onChange={(e) => setFormData({ ...formData, proxyPort: e.target.value })}
                  />
                </div>
              </div>
            </div>
          )}

          {/* 8. SOUNDS TAB */}
          {activeTab === "sounds" && (
            <div>
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 8 }}>
                <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
                  <Volume2 size={24} color="#ec4899" />
                  <span style={{ fontSize: "11.5px", fontWeight: "bold" }}>Sound Notifications</span>
                </div>
              </div>

              <div style={{ display: "flex", flexDirection: "column", gap: 10, marginTop: 14 }}>
                <label style={{ display: "flex", alignItems: "center", gap: 6, fontSize: "11.5px" }}>
                  <input type="checkbox" defaultChecked />
                  <span>Play sound on download complete (IDM Ding)</span>
                </label>
                <label style={{ display: "flex", alignItems: "center", gap: 6, fontSize: "11.5px" }}>
                  <input type="checkbox" defaultChecked />
                  <span>Play sound on download error / network timeout</span>
                </label>
                <label style={{ display: "flex", alignItems: "center", gap: 6, fontSize: "11.5px" }}>
                  <input type="checkbox" defaultChecked />
                  <span>Play sound on queue start and finish</span>
                </label>
              </div>
            </div>
          )}

          {/* 9. DIAL UP / VPN TAB */}
          {activeTab === "dialup" && (
            <div>
              <div style={{ fontSize: "11.5px", color: "#333", marginTop: 20 }}>
                Dial-up and VPN connections can be automated when starting scheduled queues.
              </div>
              <label style={{ display: "flex", alignItems: "center", gap: 6, fontSize: "11.5px", margin: "14px 0" }}>
                <input type="checkbox" />
                <span>Redial if connection was dropped</span>
              </label>
            </div>
          )}
        </div>

        {/* Footer Buttons */}
        <div className="idm-dialog-footer">
          <button className="idm-btn primary" style={{ minWidth: 75 }} onClick={handleSave}>
            OK
          </button>
          <button className="idm-btn" style={{ minWidth: 75 }} onClick={onClose}>
            Cancel
          </button>
          <button className="idm-btn" style={{ minWidth: 75 }} onClick={() => alert("IDM Configuration Help")}>
            Help
          </button>
        </div>
      </div>
    </div>
  );
}
