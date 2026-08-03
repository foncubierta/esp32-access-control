import { useState, useEffect, useCallback, useRef } from "react";
import { RefreshCw } from "lucide-react";
import { api } from "../api.js";

const ACTION_LABELS = {
  created: "Creado",
  updated: "Editado",
  deleted: "Eliminado",
  login_success: "Login correcto",
  login_failed: "Login fallido",
  password_changed: "Contraseña cambiada",
  manual_trigger: "Apertura manual",
  key_rotated: "Clave rotada",
};

const ENTITY_LABELS = {
  user: "Usuario",
  credential: "Credencial",
  credential_group: "Grupo",
  group_permission: "Acceso de grupo",
  door: "Puerta",
  permission: "Permiso",
  admin_account: "Cuenta admin",
};

const DANGER_ACTIONS = new Set(["deleted", "login_failed"]);
const SUCCESS_ACTIONS = new Set(["created", "login_success", "password_changed"]);

function actionBadgeClass(action) {
  if (DANGER_ACTIONS.has(action)) return "badgeDanger";
  if (SUCCESS_ACTIONS.has(action)) return "badgeSuccess";
  return "badgeMuted";
}

function formatDate(iso) {
  return new Date(iso.endsWith("Z") ? iso : iso + "Z").toLocaleString();
}

export default function AuditLogPage() {
  const [entries, setEntries] = useState([]);
  const [loading, setLoading] = useState(true);
  const [actor, setActor] = useState("");
  const [action, setAction] = useState("");
  const [entityType, setEntityType] = useState("");
  const [q, setQ] = useState("");
  const [since, setSince] = useState("");
  const [until, setUntil] = useState("");
  const debounceRef = useRef(null);

  const load = useCallback(() => {
    setLoading(true);
    api.auditLog
      .list({
        actor: actor || undefined,
        action: action || undefined,
        entity_type: entityType || undefined,
        q: q || undefined,
        since: since ? new Date(since).toISOString() : undefined,
        until: until ? new Date(until).toISOString() : undefined,
      })
      .then(setEntries)
      .finally(() => setLoading(false));
  }, [actor, action, entityType, q, since, until]);

  useEffect(() => {
    clearTimeout(debounceRef.current);
    debounceRef.current = setTimeout(load, 300);
    return () => clearTimeout(debounceRef.current);
  }, [load]);

  function clearFilters() {
    setActor("");
    setAction("");
    setEntityType("");
    setQ("");
    setSince("");
    setUntil("");
  }

  const hasFilters = actor || action || entityType || q || since || until;

  return (
    <div className="page">
      <div className="pageHeader">
        <h1>Auditoría</h1>
        <button type="button" className="btn" onClick={load}>
          <RefreshCw size={16} /> Actualizar
        </button>
      </div>
      <p className="muted">
        Quién cambió qué desde el panel — usuarios, credenciales, grupos, puertas, permisos, aperturas manuales, logins...
        Para eventos de acceso físico (tarjetas pasadas por un lector) ve a <strong>Logs</strong>.
      </p>
      <div className="filters">
        <input placeholder="Buscar en el resumen..." value={q} onChange={(e) => setQ(e.target.value)} />
        <input placeholder="Usuario admin" value={actor} onChange={(e) => setActor(e.target.value)} />
        <select value={action} onChange={(e) => setAction(e.target.value)}>
          <option value="">Todas las acciones</option>
          {Object.entries(ACTION_LABELS).map(([value, label]) => (
            <option key={value} value={value}>
              {label}
            </option>
          ))}
        </select>
        <select value={entityType} onChange={(e) => setEntityType(e.target.value)}>
          <option value="">Todas las entidades</option>
          {Object.entries(ENTITY_LABELS).map(([value, label]) => (
            <option key={value} value={value}>
              {label}
            </option>
          ))}
        </select>
        <input type="datetime-local" value={since} onChange={(e) => setSince(e.target.value)} title="Desde" />
        <input type="datetime-local" value={until} onChange={(e) => setUntil(e.target.value)} title="Hasta" />
        {hasFilters && (
          <button type="button" className="btn" onClick={clearFilters}>
            Limpiar filtros
          </button>
        )}
      </div>
      {loading ? (
        <p className="muted">Cargando...</p>
      ) : (
        <table className="table">
          <thead>
            <tr>
              <th>Fecha</th>
              <th>Quién</th>
              <th>Acción</th>
              <th>Entidad</th>
              <th>Resumen</th>
            </tr>
          </thead>
          <tbody>
            {entries.map((e) => (
              <tr key={e.id}>
                <td className="muted">{formatDate(e.created_at)}</td>
                <td>{e.actor}</td>
                <td>
                  <span className={`badge ${actionBadgeClass(e.action)}`}>{ACTION_LABELS[e.action] || e.action}</span>
                </td>
                <td className="muted">{ENTITY_LABELS[e.entity_type] || e.entity_type}</td>
                <td>
                  {e.summary}
                  {e.details && <div className="auditDetails">{e.details}</div>}
                </td>
              </tr>
            ))}
            {entries.length === 0 && (
              <tr>
                <td colSpan={5} className="muted">
                  No hay eventos con estos filtros.
                </td>
              </tr>
            )}
          </tbody>
        </table>
      )}
    </div>
  );
}
