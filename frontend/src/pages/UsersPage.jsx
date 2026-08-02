import { useState, useEffect, useCallback, useRef } from "react";
import { Plus, Pencil, Trash2, User as UserIcon, Upload, X as XIcon } from "lucide-react";
import { api } from "../api.js";
import Modal from "../components/Modal.jsx";

const emptyForm = { full_name: "", email: "", phone: "", dni: "", address: "", notes: "", active: true };

function UserAvatar({ user, size = 32 }) {
  const [url, setUrl] = useState(null);

  useEffect(() => {
    let objectUrl = null;
    let cancelled = false;
    if (user.photo_path) {
      api.users.photoUrl(user.id).then((u) => {
        if (cancelled) return;
        objectUrl = u;
        setUrl(u);
      });
    } else {
      setUrl(null);
    }
    return () => {
      cancelled = true;
      if (objectUrl) URL.revokeObjectURL(objectUrl);
    };
  }, [user.id, user.photo_path]);

  if (url) {
    return <img className="avatarThumb" src={url} alt="" style={{ width: size, height: size }} />;
  }
  return (
    <div className="avatarPlaceholder" style={{ width: size, height: size }}>
      <UserIcon size={size * 0.55} />
    </div>
  );
}

export default function UsersPage() {
  const [users, setUsers] = useState([]);
  const [loading, setLoading] = useState(true);
  const [editing, setEditing] = useState(null); // null = closed, {} = new, {...} = editing existing
  const [form, setForm] = useState(emptyForm);
  const [error, setError] = useState("");
  const [photoUrl, setPhotoUrl] = useState(null);
  const [uploadingPhoto, setUploadingPhoto] = useState(false);
  const photoUrlRef = useRef(null);

  const load = useCallback(() => {
    setLoading(true);
    api.users
      .list()
      .then(setUsers)
      .catch((e) => setError(e.message))
      .finally(() => setLoading(false));
  }, []);

  useEffect(load, [load]);

  useEffect(() => () => {
    if (photoUrlRef.current) URL.revokeObjectURL(photoUrlRef.current);
  }, []);

  function setPhoto(url) {
    if (photoUrlRef.current) URL.revokeObjectURL(photoUrlRef.current);
    photoUrlRef.current = url;
    setPhotoUrl(url);
  }

  function openCreate() {
    setForm(emptyForm);
    setError("");
    setPhoto(null);
    setEditing({});
  }

  function openEdit(user) {
    setForm({
      full_name: user.full_name,
      email: user.email || "",
      phone: user.phone || "",
      dni: user.dni || "",
      address: user.address || "",
      notes: user.notes || "",
      active: user.active,
    });
    setError("");
    setEditing(user);
    setPhoto(null);
    if (user.photo_path) api.users.photoUrl(user.id).then(setPhoto);
  }

  function closeModal() {
    setEditing(null);
    setPhoto(null);
  }

  async function save(e) {
    e.preventDefault();
    setError("");
    try {
      if (editing.id) {
        const updated = await api.users.update(editing.id, form);
        setEditing(updated);
      } else {
        const created = await api.users.create(form);
        setEditing(created); // stays open, now with an id — photo upload becomes available
      }
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

  async function handlePhotoChange(e) {
    const file = e.target.files?.[0];
    e.target.value = "";
    if (!file || !editing?.id) return;
    setUploadingPhoto(true);
    setError("");
    try {
      await api.users.uploadPhoto(editing.id, file);
      setPhoto(await api.users.photoUrl(editing.id));
      load();
    } catch (err) {
      setError(err.message);
    } finally {
      setUploadingPhoto(false);
    }
  }

  async function removePhoto() {
    if (!editing?.id) return;
    await api.users.deletePhoto(editing.id);
    setPhoto(null);
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
              <th></th>
              <th>Nombre</th>
              <th>DNI</th>
              <th>Email</th>
              <th>Teléfono</th>
              <th>Estado</th>
              <th></th>
            </tr>
          </thead>
          <tbody>
            {users.map((u) => (
              <tr key={u.id}>
                <td>
                  <UserAvatar user={u} />
                </td>
                <td>{u.full_name}</td>
                <td className="muted">{u.dni || "—"}</td>
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
                <td colSpan={7} className="muted">
                  No hay usuarios todavía.
                </td>
              </tr>
            )}
          </tbody>
        </table>
      )}

      {editing && (
        <Modal title={editing.id ? "Editar usuario" : "Nuevo usuario"} onClose={closeModal}>
          <form className="form" onSubmit={save}>
            {error && <p className="formError">{error}</p>}

            <div className="photoRow">
              {photoUrl ? (
                <img className="photoPreview" src={photoUrl} alt="" />
              ) : (
                <div className="photoPreview photoPreviewEmpty">
                  <UserIcon size={28} />
                </div>
              )}
              <div className="photoRowActions">
                {editing.id ? (
                  <>
                    <label className="btn">
                      <Upload size={14} /> {uploadingPhoto ? "Subiendo..." : "Subir foto"}
                      <input type="file" accept="image/*" hidden disabled={uploadingPhoto} onChange={handlePhotoChange} />
                    </label>
                    {photoUrl && (
                      <button type="button" className="iconBtn iconBtnDanger" title="Quitar foto" onClick={removePhoto}>
                        <XIcon size={16} />
                      </button>
                    )}
                  </>
                ) : (
                  <p className="hint">Guarda el usuario primero para poder subir su foto.</p>
                )}
              </div>
            </div>

            <label>
              Nombre completo
              <input required value={form.full_name} onChange={(e) => setForm({ ...form, full_name: e.target.value })} />
            </label>
            <label>
              DNI
              <input value={form.dni} onChange={(e) => setForm({ ...form, dni: e.target.value })} placeholder="p.ej. 12345678A" />
            </label>
            <label>
              Dirección
              <input value={form.address} onChange={(e) => setForm({ ...form, address: e.target.value })} />
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
              <button type="button" className="btn" onClick={closeModal}>
                {editing.id ? "Cerrar" : "Cancelar"}
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
