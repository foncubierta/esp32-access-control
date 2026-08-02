import { useState, useEffect, useCallback } from "react";
import { Plus, Trash2 } from "lucide-react";
import { api } from "../api.js";
import Modal from "../components/Modal.jsx";

const DAYS = [
  { value: 0, label: "L" },
  { value: 1, label: "M" },
  { value: 2, label: "X" },
  { value: 3, label: "J" },
  { value: 4, label: "V" },
  { value: 5, label: "S" },
  { value: 6, label: "D" },
];

const emptyForm = { credential_id: "", door_id: "", days: [], time_start: "", time_end: "", active: true };

export default function PermissionsPage() {
  const [permissions, setPermissions] = useState([]);
  const [credentials, setCredentials] = useState([]);
  const [users, setUsers] = useState([]);
  const [doors, setDoors] = useState([]);
  const [loading, setLoading] = useState(true);
  const [creating, setCreating] = useState(false);
  const [form, setForm] = useState(emptyForm);
  const [error, setError] = useState("");

  const load = useCallback(() => {
    setLoading(true);
    Promise.all([api.permissions.list(), api.credentials.list(), api.users.list(), api.doors.list()])
      .then(([perms, creds, us, ds]) => {
        setPermissions(perms);
        setCredentials(creds);
        setUsers(us);
        setDoors(ds);
      })
      .catch((e) => setError(e.message))
      .finally(() => setLoading(false));
  }, []);

  useEffect(load, [load]);

  const userName = (id) => users.find((u) => u.id === id)?.full_name || `#${id}`;
  const credentialLabel = (id) => {
    const c = credentials.find((c) => c.id === id);
    if (!c) return `#${id}`;
    return `${userName(c.user_id)} — ${c.label || c.type}`;
  };
  const doorName = (id) => doors.find((d) => d.id === id)?.name || `#${id}`;

  function openCreate() {
    setForm(emptyForm);
    setError("");
    setCreating(true);
  }

  function toggleDay(value) {
    setForm((f) => ({
      ...f,
      days: f.days.includes(value) ? f.days.filter((d) => d !== value) : [...f.days, value].sort((a, b) => a - b),
    }));
  }

  async function save(e) {
    e.preventDefault();
    setError("");
    try {
      await api.permissions.create({
        credential_id: Number(form.credential_id),
        door_id: Number(form.door_id),
        days_of_week: form.days.length ? form.days.join(",") : null,
        time_start: form.time_start || null,
        time_end: form.time_end || null,
        active: form.active,
      });
      setCreating(false);
      load();
    } catch (err) {
      setError(err.message);
    }
  }

  async function toggleActive(perm) {
    await api.permissions.update(perm.id, { active: !perm.active });
    load();
  }

  async function remove(perm) {
    if (!confirm("¿Eliminar este permiso?")) return;
    await api.permissions.delete(perm.id);
    load();
  }

  function scheduleLabel(perm) {
    const days = perm.days_of_week
      ? perm.days_of_week
          .split(",")
          .map((d) => DAYS[Number(d)].label)
          .join("")
      : "Todos los días";
    const hours = perm.time_start || perm.time_end ? `${perm.time_start || "00:00"}–${perm.time_end || "23:59"}` : "Todo el día";
    return `${days} · ${hours}`;
  }

  return (
    <div className="page">
      <div className="pageHeader">
        <h1>Permisos</h1>
        <button type="button" className="btn btnPrimary" onClick={openCreate} disabled={!credentials.length || !doors.length}>
          <Plus size={16} /> Nuevo permiso
        </button>
      </div>
      {(!credentials.length || !doors.length) && !loading && (
        <p className="muted">Necesitas al menos una credencial y una puerta para crear permisos.</p>
      )}
      {loading ? (
        <p className="muted">Cargando...</p>
      ) : (
        <table className="table">
          <thead>
            <tr>
              <th>Credencial</th>
              <th>Puerta</th>
              <th>Horario</th>
              <th>Estado</th>
              <th></th>
            </tr>
          </thead>
          <tbody>
            {permissions.map((p) => (
              <tr key={p.id}>
                <td>{credentialLabel(p.credential_id)}</td>
                <td>{doorName(p.door_id)}</td>
                <td className="muted">{scheduleLabel(p)}</td>
                <td>
                  <button type="button" className={`badge badgeButton ${p.active ? "badgeSuccess" : "badgeMuted"}`} onClick={() => toggleActive(p)}>
                    {p.active ? "Activo" : "Inactivo"}
                  </button>
                </td>
                <td className="rowActions">
                  <button type="button" className="iconBtn iconBtnDanger" onClick={() => remove(p)}>
                    <Trash2 size={16} />
                  </button>
                </td>
              </tr>
            ))}
            {permissions.length === 0 && (
              <tr>
                <td colSpan={5} className="muted">
                  No hay permisos todavía.
                </td>
              </tr>
            )}
          </tbody>
        </table>
      )}

      {creating && (
        <Modal title="Nuevo permiso" onClose={() => setCreating(false)}>
          <form className="form" onSubmit={save}>
            {error && <p className="formError">{error}</p>}
            <label>
              Credencial
              <select required value={form.credential_id} onChange={(e) => setForm({ ...form, credential_id: e.target.value })}>
                <option value="" disabled>
                  Selecciona una credencial
                </option>
                {credentials.map((c) => (
                  <option key={c.id} value={c.id}>
                    {credentialLabel(c.id)}
                  </option>
                ))}
              </select>
            </label>
            <label>
              Puerta
              <select required value={form.door_id} onChange={(e) => setForm({ ...form, door_id: e.target.value })}>
                <option value="" disabled>
                  Selecciona una puerta
                </option>
                {doors.map((d) => (
                  <option key={d.id} value={d.id}>
                    {d.name}
                  </option>
                ))}
              </select>
            </label>
            <label>Días permitidos (ninguno seleccionado = todos)</label>
            <div className="dayPicker">
              {DAYS.map((d) => (
                <button
                  type="button"
                  key={d.value}
                  className={`dayChip ${form.days.includes(d.value) ? "dayChipActive" : ""}`}
                  onClick={() => toggleDay(d.value)}
                >
                  {d.label}
                </button>
              ))}
            </div>
            <div className="formGrid">
              <label>
                Desde
                <input type="time" value={form.time_start} onChange={(e) => setForm({ ...form, time_start: e.target.value })} />
              </label>
              <label>
                Hasta
                <input type="time" value={form.time_end} onChange={(e) => setForm({ ...form, time_end: e.target.value })} />
              </label>
            </div>
            <div className="formActions">
              <button type="button" className="btn" onClick={() => setCreating(false)}>
                Cancelar
              </button>
              <button type="submit" className="btn btnPrimary">
                Guardar
              </button>
            </div>
          </form>
        </Modal>
      )}
    </div>
  );
}
