import { useState, useEffect, useCallback } from "react";
import { Plus, Pencil, Trash2 } from "lucide-react";
import { api } from "../api.js";
import Modal from "../components/Modal.jsx";

const emptyForm = { full_name: "", email: "", phone: "", notes: "", active: true };

export default function UsersPage() {
  const [users, setUsers] = useState([]);
  const [loading, setLoading] = useState(true);
  const [editing, setEditing] = useState(null); // null = closed, {} = new, {...} = editing existing
  const [form, setForm] = useState(emptyForm);
  const [error, setError] = useState("");

  const load = useCallback(() => {
    setLoading(true);
    api.users
      .list()
      .then(setUsers)
      .catch((e) => setError(e.message))
      .finally(() => setLoading(false));
  }, []);

  useEffect(load, [load]);

  function openCreate() {
    setForm(emptyForm);
    setError("");
    setEditing({});
  }

  function openEdit(user) {
    setForm({
      full_name: user.full_name,
      email: user.email || "",
      phone: user.phone || "",
      notes: user.notes || "",
      active: user.active,
    });
    setError("");
    setEditing(user);
  }

  async function save(e) {
    e.preventDefault();
    setError("");
    try {
      if (editing.id) {
        await api.users.update(editing.id, form);
      } else {
        await api.users.create(form);
      }
      setEditing(null);
      load();
    } catch (err) {
      setError(err.message);
    }
  }

  async function remove(user) {
    if (!confirm(`¿Eliminar a ${user.full_name}? Esto borrará también sus credenciales y permisos.`)) return;
    await api.users.delete(user.id);
    load();
  }

  return (
    <div className="page">
      <div className="pageHeader">
        <h1>Usuarios</h1>
        <button type="button" className="btn btnPrimary" onClick={openCreate}>
          <Plus size={16} /> Nuevo usuario
        </button>
      </div>
      {loading ? (
        <p className="muted">Cargando...</p>
      ) : (
        <table className="table">
          <thead>
            <tr>
              <th>Nombre</th>
              <th>Email</th>
              <th>Teléfono</th>
              <th>Estado</th>
              <th></th>
            </tr>
          </thead>
          <tbody>
            {users.map((u) => (
              <tr key={u.id}>
                <td>{u.full_name}</td>
                <td>{u.email || "—"}</td>
                <td>{u.phone || "—"}</td>
                <td>
                  <span className={`badge ${u.active ? "badgeSuccess" : "badgeMuted"}`}>
                    {u.active ? "Activo" : "Inactivo"}
                  </span>
                </td>
                <td className="rowActions">
                  <button type="button" className="iconBtn" onClick={() => openEdit(u)}>
                    <Pencil size={16} />
                  </button>
                  <button type="button" className="iconBtn iconBtnDanger" onClick={() => remove(u)}>
                    <Trash2 size={16} />
                  </button>
                </td>
              </tr>
            ))}
            {users.length === 0 && (
              <tr>
                <td colSpan={5} className="muted">
                  No hay usuarios todavía.
                </td>
              </tr>
            )}
          </tbody>
        </table>
      )}

      {editing && (
        <Modal title={editing.id ? "Editar usuario" : "Nuevo usuario"} onClose={() => setEditing(null)}>
          <form className="form" onSubmit={save}>
            {error && <p className="formError">{error}</p>}
            <label>
              Nombre completo
              <input required value={form.full_name} onChange={(e) => setForm({ ...form, full_name: e.target.value })} />
            </label>
            <label>
              Email
              <input type="email" value={form.email} onChange={(e) => setForm({ ...form, email: e.target.value })} />
            </label>
            <label>
              Teléfono
              <input value={form.phone} onChange={(e) => setForm({ ...form, phone: e.target.value })} />
            </label>
            <label>
              Notas
              <textarea value={form.notes} onChange={(e) => setForm({ ...form, notes: e.target.value })} />
            </label>
            <label className="checkboxRow">
              <input type="checkbox" checked={form.active} onChange={(e) => setForm({ ...form, active: e.target.checked })} />
              Activo
            </label>
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
