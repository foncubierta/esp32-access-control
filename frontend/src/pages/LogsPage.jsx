import { useState, useEffect, useCallback } from "react";
import { RefreshCw } from "lucide-react";
import { api } from "../api.js";

export default function LogsPage() {
  const [logs, setLogs] = useState([]);
  const [doors, setDoors] = useState([]);
  const [loading, setLoading] = useState(true);
  const [doorId, setDoorId] = useState("");
  const [result, setResult] = useState("");

  const load = useCallback(() => {
    setLoading(true);
    Promise.all([api.logs.list({ door_id: doorId, result }), api.doors.list()])
      .then(([ls, ds]) => {
        setLogs(ls);
        setDoors(ds);
      })
      .finally(() => setLoading(false));
  }, [doorId, result]);

  useEffect(load, [load]);

  const doorName = (id) => doors.find((d) => d.id === id)?.name || `#${id}`;

  return (
    <div className="page">
      <div className="pageHeader">
        <h1>Logs de acceso</h1>
        <button type="button" className="btn" onClick={load}>
          <RefreshCw size={16} /> Actualizar
        </button>
      </div>
      <div className="filters">
        <select value={doorId} onChange={(e) => setDoorId(e.target.value)}>
          <option value="">Todas las puertas</option>
          {doors.map((d) => (
            <option key={d.id} value={d.id}>
              {d.name}
            </option>
          ))}
        </select>
        <select value={result} onChange={(e) => setResult(e.target.value)}>
          <option value="">Todos los resultados</option>
          <option value="granted">Concedidos</option>
          <option value="denied">Denegados</option>
        </select>
      </div>
      {loading ? (
        <p className="muted">Cargando...</p>
      ) : (
        <table className="table">
          <thead>
            <tr>
              <th>Fecha</th>
              <th>Puerta</th>
              <th>Resultado</th>
              <th>Motivo</th>
            </tr>
          </thead>
          <tbody>
            {logs.map((l) => (
              <tr key={l.id}>
                <td className="muted">{new Date(l.event_time.endsWith("Z") ? l.event_time : l.event_time + "Z").toLocaleString()}</td>
                <td>{doorName(l.door_id)}</td>
                <td>
                  <span className={`badge ${l.result === "granted" ? "badgeSuccess" : "badgeDanger"}`}>
                    {l.result === "granted" ? "Concedido" : "Denegado"}
                  </span>
                </td>
                <td className="muted">{l.reason || "—"}</td>
              </tr>
            ))}
            {logs.length === 0 && (
              <tr>
                <td colSpan={4} className="muted">
                  No hay eventos todavía.
                </td>
              </tr>
            )}
          </tbody>
        </table>
      )}
    </div>
  );
}
