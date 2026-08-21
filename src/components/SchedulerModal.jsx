import React, { useState } from "react";
import { X, Clock, Calendar, Play, Square, ListOrdered, Sliders, Shield } from "lucide-react";

export default function SchedulerModal({ onClose, onSave }) {
  const [scheduleEnabled, setScheduleEnabled] = useState(false);
  const [startTime, setStartTime] = useState("02:00");
  const [stopTime, setStopTime] = useState("06:00");
  const [maxParallel, setMaxParallel] = useState(2);
  const [shutdownOnFinish, setShutdownOnFinish] = useState(false);
  const [speedLimit, setSpeedLimit] = useState(2048);

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div
        className="idm-dialog"
        style={{ width: "480px", borderRadius: "4px" }}
        onClick={(e) => e.stopPropagation()}
      >
        {/* Titlebar */}
        <div className="idm-dialog-titlebar">
          <div className="idm-dialog-title-text">
            <Clock size={15} color="#ec4899" />
            <span style={{ fontWeight: 600, fontSize: "12px" }}>Scheduler and Queue Processing</span>
          </div>
          <button className="idm-dialog-close" onClick={onClose}>
            <X size={14} />
          </button>
        </div>

        {/* Body */}
        <div className="idm-dialog-body" style={{ padding: "16px 20px" }}>
          <fieldset style={{ border: "1px solid #bcbcbc", padding: "10px 14px", borderRadius: 2, marginBottom: 12 }}>
            <legend style={{ padding: "0 4px", fontSize: "11px", color: "#333" }}>Schedule execution</legend>

            <label style={{ display: "flex", alignItems: "center", gap: 6, fontSize: "11.5px", cursor: "pointer", marginBottom: 10 }}>
              <input
                type="checkbox"
                checked={scheduleEnabled}
                onChange={(e) => setScheduleEnabled(e.target.checked)}
              />
              <span>Start download queue automatically at specified time</span>
            </label>

            <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 12, opacity: scheduleEnabled ? 1 : 0.5 }}>
              <div>
                <div style={{ fontSize: "11px", marginBottom: 3 }}>Start download at:</div>
                <input
                  type="time"
                  className="prop-input"
                  disabled={!scheduleEnabled}
                  value={startTime}
                  onChange={(e) => setStartTime(e.target.value)}
                />
              </div>

              <div>
                <div style={{ fontSize: "11px", marginBottom: 3 }}>Stop download at:</div>
                <input
                  type="time"
                  className="prop-input"
                  disabled={!scheduleEnabled}
                  value={stopTime}
                  onChange={(e) => setStopTime(e.target.value)}
                />
              </div>
            </div>
          </fieldset>

          <fieldset style={{ border: "1px solid #bcbcbc", padding: "10px 14px", borderRadius: 2, marginBottom: 12 }}>
            <legend style={{ padding: "0 4px", fontSize: "11px", color: "#333" }}>Queue limits & Parallel downloads</legend>

            <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 10 }}>
              <span style={{ fontSize: "11.5px" }}>Download files simultaneously:</span>
              <select
                style={{ width: 70, height: 24, padding: "0 4px" }}
                value={maxParallel}
                onChange={(e) => setMaxParallel(parseInt(e.target.value, 10))}
              >
                <option value="1">1</option>
                <option value="2">2</option>
                <option value="4">4</option>
                <option value="8">8</option>
              </select>
            </div>

            <label style={{ display: "flex", alignItems: "center", gap: 6, fontSize: "11.5px", cursor: "pointer" }}>
              <input
                type="checkbox"
                checked={shutdownOnFinish}
                onChange={(e) => setShutdownOnFinish(e.target.checked)}
              />
              <span>Turn off computer when downloads complete</span>
            </label>
          </fieldset>
        </div>

        {/* Footer */}
        <div className="idm-dialog-footer">
          <button
            className="idm-btn primary"
            onClick={() => {
              alert("Scheduler settings saved successfully.");
              onClose();
            }}
          >
            Apply
          </button>
          <button className="idm-btn" onClick={onClose}>
            Close
          </button>
        </div>
      </div>
    </div>
  );
}
