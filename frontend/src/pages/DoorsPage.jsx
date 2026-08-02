import { useState, useEffect, useCallback } from "react";
import { Plus, Pencil, Trash2, Eye, EyeOff, Copy, RefreshCw } from "lucide-react";
import { api } from "../api.js";
import Modal from "../components/Modal.jsx";

const emptyForm = { name: "", location: "", description: "", active: true };

function formatLastSeen(value) {
  if (!value) return "Nunca";
  const date = new Date(value.endsWith("Z") ? value : value + "Z");
  return date.toLocaleString();
}

export default function DoorsPage() {
  const [doors, setDoors] = useState([]);
  const [loading, setLoading] = useState(true);
  const [editing, setEditing] = useState(null);
  const [form, setForm] = useState(emptyForm);
  const [error, setError] = useState("");
  const [revealed, setRevealed] = useState({});

  const load = useCallback(() => {
    setLoading(true);
    api.doors
      .list()
      .then(setDoors)
      .catch((e) => setError(e.message))
      .finally(() => setLoading(false));
  }, []);

  useEffect(load, [load]);

  function openCreate() {
    setForm(emptyForm);
    setError("");
    setEditing({});
  }

  function openEdit(door) {
    setForm({ name: door.name, location: door.location || "", description: door.description || "", active: door.active });
    setError("");
    setEditing(door);
  }

  async function save(e) {
    e.preventDefault();
    setError("");
    try {
      if (editing.id) {
        await api.doors.update(editing.id, form);
      } else {
        await api.doors.create(form);
      }
      setEditing(null);
      load();
    } catch (err) {
      setError(err.message);
    }
  }

  async function remove(door) {
    if (!confirm(`¿Eliminar la puerta "${door.name}"? El nodo dejará de poder sincronizar.`)) return;
    await api.doors.delete(door.id);
    load();
  }

  async function rotateKey(door) {
    if (
      !confirm(
        `¿Rotar la API key de "${door.name}"? El nodo actual dejará de poder sincronizar hasta que lo reconfigures con la nueva clave.`
      )
    )
      return;
    await api.doors.rotateKey(door.id);
    load();
  }

  function copyKey(key) {
    navigator.clipboard?.writeText(key);
  }

  return (
    <div className="page">
      <div className="pageHeader">
        <h1>Puertas / Nodos</h1>
        <button type="button" className="btn btnPrimary" onClick={openCreate}>
          <Plus size={16} /> Nueva puerta
        </button>
      </div>
      {loading ? (
        <p className="muted">Cargando...</p>
      ) : (
        <table className="table">
          <thead>
            <tr>
              <th>Nombre</th>
              <th>Ubicación</th>
              <th>API key (nodo)</th>
              <th>Últ. sincronización</th>
              <th>Estado</th>
              <th></th>
            </tr>
          </thead>
          <tbody>
            {doors.map((d) => (
              <tr key={d.id}>
                <td>{d.name}</td>
                <td>{d.location || "—"}</td>
                <td className="apiKeyCell">
                  <code>{revealed[d.id] ? d.api_key : "•".repeat(16)}</code>
                  <button
                    type="button"
                    className="iconBtn"
                    title={revealed[d.id] ? "Ocultar" : "Mostrar"}
                    onClick={() => setRevealed({ ...revealed, [d.id]: !revealed[d.id] })}
                  >
                    {revealed[d.id] ? <EyeOff size={14} /> : <Eye size={14} />}
                  </button>
                  <button type="button" className="iconBtn" title="Copiar" onClick={() => copyKey(d.api_key)}>
                    <Copy size={14} />
                  </button>
                  <button type="button" className="iconBtn" title="Rotar clave" onClick={() => rotateKey(d)}>
                    <RefreshCw size={14} />
                  </button>
                </td>
                <td className="muted">{formatLastSeen(d.last_seen)}</td>
                <td>
                  <span className={`badge ${d.active ? "badgeSuccess" : "badgeMuted"}`}>{d.active ? "Activa" : "Bloqueada"}</span>
                </td>
                <td className="rowActions">
                  <button type="button" className="iconBtn" onClick={() => openEdit(d)}>
                    <Pencil size={16} />
                  </button>
                  <button type="button" className="iconBtn iconBtnDanger" onClick={() => remove(d)}>
                    <Trash2 size={16} />
                  </button>
                </td>
              </tr>
            ))}
            {doors.length === 0 && (
              <tr>
                <td colSpan={6} className="muted">
                  No hay puertas todavía.
                </td>
              </tr>
            )}
          </tbody>
        </table>
      )}

      {editing && (
        <Modal title={editing.id ? "Editar puerta" : "Nueva puerta"} onClose={() => setEditing(null)}>
          <form className="form" onSubmit={save}>
            {error && <p className="formError">{error}</p>}
            <label>
              Nombre
              <input required value={form.name} onChange={(e) => setForm({ ...form, name: e.target.value })} placeholder="p.ej. Puerta principal" />
            </label>
            <label>
              Ubicación
              <input value={form.location} onChange={(e) => setForm({ ...form, location: e.target.value })} placeholder="p.ej. Edificio A, planta baja" />
            </label>
            <label>
              Descripción
              <textarea value={form.description} onChange={(e) => setForm({ ...form, description: e.target.value })} />
            </label>
            <label className="checkboxRow">
              <input type="checkbox" checked={form.active} onChange={(e) => setForm({ ...form, active: e.target.checked })} />
              Activa (desactivarla bloquea todos los accesos en el siguiente sync del nodo)
            </label>
            {!editing.id && <p className="hint">Se generará una API key para el nodo al guardar — configúrala en el firmware del ESP32.</p>}
            <div className="formActions">
              <button type="button" className="btn" onClick={() => setEditing(null)}>
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
