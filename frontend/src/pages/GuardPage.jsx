import { useState, useEffect, useCallback } from "react";
import { DoorOpen, DoorClosed, ScanEye, Zap } from "lucide-react";
import { api } from "../api.js";

const MODES = [
  { value: "auto", label: "Automático", icon: Zap, description: "Abre según los permisos de cada tarjeta" },
  { value: "open", label: "Abierto", icon: DoorOpen, description: "Puerta libre: cualquier tarjeta abre" },
  { value: "closed", label: "Cerrado", icon: DoorClosed, description: "Nunca abre, aunque la tarjeta tenga acceso" },
  { value: "identify", label: "Identificación", icon: ScanEye, description: "Identifica a la persona pero no abre" },
];

const POLL_MS = 1500;
const DOOR_REFRESH_MS = 5000;

function formatTime(iso) {
  return new Date(iso.endsWith("Z") ? iso : iso + "Z").toLocaleTimeString();
}

export default function GuardPage() {
  const [doors, setDoors] = useState([]);
  const [doorId, setDoorId] = useState(() => localStorage.getItem("guard_door_id") || "");
  const [users, setUsers] = useState([]);
  const [credentials, setCredentials] = useState([]);
  const [events, setEvents] = useState([]);
  const [changingMode, setChangingMode] = useState(false);

  useEffect(() => {
    api.doors.list().then((ds) => {
      setDoors(ds);
      setDoorId((current) => current || (ds[0] ? String(ds[0].id) : ""));
    });
    api.users.list().then(setUsers);
    api.credentials.list().then(setCredentials);
  }, []);

  useEffect(() => {
    if (doorId) localStorage.setItem("guard_door_id", doorId);
  }, [doorId]);

  useEffect(() => {
    const t = setInterval(() => api.doors.list().then(setDoors), DOOR_REFRESH_MS);
    return () => clearInterval(t);
  }, []);

  const door = doors.find((d) => String(d.id) === String(doorId));

  const loadEvents = useCallback(() => {
    if (!doorId) return;
    api.logs.list({ door_id: doorId, limit: 8 }).then(setEvents).catch(() => {});
  }, [doorId]);

  useEffect(() => {
    loadEvents();
    const t = setInterval(loadEvents, POLL_MS);
    return () => clearInterval(t);
  }, [loadEvents]);

  async function setMode(mode) {
    if (!door) return;
    setChangingMode(true);
    try {
      await api.doors.update(door.id, { mode });
      setDoors((prev) => prev.map((d) => (d.id === door.id ? { ...d, mode } : d)));
    } finally {
      setChangingMode(false);
    }
  }

  function credentialInfo(credentialId) {
    if (credentialId == null) return null;
    const c = credentials.find((c) => c.id === credentialId);
    if (!c) return null;
    const u = users.find((u) => u.id === c.user_id);
    return { userName: u?.full_name || `Usuario #${c.user_id}`, credLabel: c.label || c.type };
  }

  function modeLabel(value) {
    return MODES.find((m) => m.value === value)?.label || value || "—";
  }

  const latest = events[0];
  const latestInfo = latest ? credentialInfo(latest.credential_id) : null;
  const latestOpened = latest && (latest.door_mode === "open" || (latest.door_mode === "auto" && latest.result === "granted"));

  return (
    <div className="page">
      <div className="pageHeader">
        <h1>Vigilancia</h1>
        {doors.length > 0 && (
          <select value={doorId} onChange={(e) => setDoorId(e.target.value)}>
            {doors.map((d) => (
              <option key={d.id} value={d.id}>
                {d.name}
              </option>
            ))}
          </select>
        )}
      </div>

      {!door ? (
        <p className="muted">No hay puertas configuradas todavía.</p>
      ) : (
        <>
          <div className="modeGrid">
            {MODES.map((m) => {
              const Icon = m.icon;
              const active = (door.mode || "auto") === m.value;
              return (
                <button
                  type="button"
                  key={m.value}
                  className={`modeCard ${active ? "modeCardActive" : ""}`}
                  disabled={changingMode}
                  onClick={() => setMode(m.value)}
                >
                  <Icon size={22} />
                  <span className="modeCardLabel">{m.label}</span>
                  <span className="modeCardDesc">{m.description}</span>
                </button>
              );
            })}
          </div>

          <div className={`lastScanCard ${latest ? (latestOpened ? "lastScanOpen" : "lastScanDenied") : ""}`}>
            {!latest ? (
              <p className="muted">Todavía no se ha pasado ninguna tarjeta por esta puerta.</p>
            ) : (
              <>
                <div className="lastScanResult">{latestOpened ? "ABIERTO" : "NO ABIERTO"}</div>
                <div className="lastScanName">{latestInfo?.userName || "Tarjeta desconocida"}</div>
                <div className="lastScanMeta">
                  {latestInfo?.credLabel && <span>{latestInfo.credLabel} · </span>}
                  <span>{formatTime(latest.event_time)}</span>
                  {latest.reason && <span> · {latest.reason}</span>}
                </div>
              </>
            )}
          </div>

          <div>
            <h2 className="sectionTitle">Últimos pases</h2>
            <table className="table">
              <thead>
                <tr>
                  <th>Hora</th>
                  <th>Persona</th>
                  <th>Credencial</th>
                  <th>Acceso</th>
                  <th>Modo</th>
                </tr>
              </thead>
              <tbody>
                {events.map((ev) => {
                  const info = credentialInfo(ev.credential_id);
                  return (
                    <tr key={ev.id}>
                      <td className="muted">{formatTime(ev.event_time)}</td>
                      <td>{info?.userName || "Desconocida"}</td>
                      <td className="muted">{info?.credLabel || "—"}</td>
                      <td>
                        <span className={`badge ${ev.result === "granted" ? "badgeSuccess" : "badgeDanger"}`}>
                          {ev.result === "granted" ? "Con acceso" : "Sin acceso"}
                        </span>
                      </td>
                      <td className="muted">{modeLabel(ev.door_mode)}</td>
                    </tr>
                  );
                })}
                {events.length === 0 && (
                  <tr>
                    <td colSpan={5} className="muted">
                      Sin eventos.
                    </td>
                  </tr>
                )}
              </tbody>
            </table>
          </div>
        </>
      )}
    </div>
  );
}
