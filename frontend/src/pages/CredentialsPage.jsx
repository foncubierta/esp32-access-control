import { useState, useEffect, useCallback, useRef } from "react";
import { Plus, Pencil, Trash2, Scan } from "lucide-react";
import { api } from "../api.js";
import Modal from "../components/Modal.jsx";

const emptyForm = { user_id: "", group_id: "", type: "rfid", label: "", value: "", active: true, valid_from: "", valid_until: "" };
const TYPE_LABELS = { rfid: "RFID", pin: "PIN", nfc: "NFC" };
const ENROLL_POLL_MS = 1000;
const ENROLL_TIMEOUT_MS = 30000;

export default function CredentialsPage() {
  const [credentials, setCredentials] = useState([]);
  const [users, setUsers] = useState([]);
  const [groups, setGroups] = useState([]);
  const [loading, setLoading] = useState(true);
  const [editing, setEditing] = useState(null);
  const [form, setForm] = useState(emptyForm);
  const [error, setError] = useState("");
  const [doors, setDoors] = useState([]);
  const [enrollDoorId, setEnrollDoorId] = useState("");
  const [enrolling, setEnrolling] = useState(false);
  const [enrollMessage, setEnrollMessage] = useState("");
  const enrollPollRef = useRef(null);
  const enrollTimeoutRef = useRef(null);

  const load = useCallback(() => {
    setLoading(true);
    Promise.all([api.credentials.list(), api.users.list(), api.groups.list()])
      .then(([creds, us, gs]) => {
        setCredentials(creds);
        setUsers(us);
        setGroups(gs);
      })
      .catch((e) => setError(e.message))
      .finally(() => setLoading(false));
  }, []);

  useEffect(load, [load]);

  useEffect(() => {
    api.doors.list().then((ds) => {
      setDoors(ds);
      setEnrollDoorId((current) => current || (ds[0] ? String(ds[0].id) : ""));
    });
  }, []);

  useEffect(() => () => stopEnrollPolling(), []);

  const userName = (id) => users.find((u) => u.id === id)?.full_name || `#${id}`;
  const groupName = (id) => (id == null ? null : groups.find((g) => g.id === id)?.name || `#${id}`);

  function stopEnrollPolling() {
    clearInterval(enrollPollRef.current);
    clearTimeout(enrollTimeoutRef.current);
    enrollPollRef.current = null;
    enrollTimeoutRef.current = null;
  }

  function cancelEnroll() {
    const wasArmed = enrolling;
    stopEnrollPolling();
    setEnrolling(false);
    setEnrollMessage("");
    if (wasArmed && enrollDoorId) {
      api.doors.disarmEnroll(enrollDoorId).catch(() => {});
    }
  }

  async function startEnroll() {
    if (!enrollDoorId || enrolling) return;
    setEnrollMessage("");
    try {
      await api.doors.armEnroll(enrollDoorId);
    } catch (err) {
      setEnrollMessage(err.message);
      return;
    }
    setEnrolling(true);
    setEnrollMessage("Pasa la tarjeta por el lector...");
    enrollPollRef.current = setInterval(async () => {
      try {
        const status = await api.doors.enrollStatus(enrollDoorId);
        if (status.captured) {
          stopEnrollPolling();
          setForm((f) => ({ ...f, value: status.captured.value }));
          setEnrolling(false);
          setEnrollMessage(`Tarjeta leída: ${status.captured.value}`);
          api.doors.disarmEnroll(enrollDoorId).catch(() => {});
        }
      } catch {
        // transient poll error — keep trying until the timeout below
      }
    }, ENROLL_POLL_MS);
    enrollTimeoutRef.current = setTimeout(() => {
      stopEnrollPolling();
      setEnrolling(false);
      setEnrollMessage("No se detectó ninguna tarjeta — inténtalo de nuevo.");
      api.doors.disarmEnroll(enrollDoorId).catch(() => {});
    }, ENROLL_TIMEOUT_MS);
  }

  function closeModal() {
    cancelEnroll();
    setEditing(null);
  }

  function openCreate() {
    cancelEnroll();
    setForm(emptyForm);
    setError("");
    setEditing({});
  }

  function openEdit(cred) {
    cancelEnroll();
    setForm({
      user_id: cred.user_id,
      group_id: cred.group_id != null ? String(cred.group_id) : "",
      type: cred.type,
      label: cred.label || "",
      value: "",
      active: cred.active,
      valid_from: cred.valid_from ? cred.valid_from.slice(0, 10) : "",
      valid_until: cred.valid_until ? cred.valid_until.slice(0, 10) : "",
    });
    setError("");
    setEditing(cred);
  }

  async function save(e) {
    e.preventDefault();
    setError("");
    try {
      const payload = {
        ...form,
        user_id: Number(form.user_id),
        group_id: form.group_id ? Number(form.group_id) : null,
        valid_from: form.valid_from || null,
        valid_until: form.valid_until || null,
      };
      if (editing.id) {
        if (!payload.value) delete payload.value;
        delete payload.user_id;
        delete payload.type;
        await api.credentials.update(editing.id, payload);
      } else {
        if (!payload.value) throw new Error("Introduce el valor de la credencial (UID/PIN)");
        await api.credentials.create(payload);
      }
      closeModal();
      load();
    } catch (err) {
      setError(err.message);
    }
  }

  async function remove(cred) {
    if (!confirm("¿Eliminar esta credencial? También se borrarán sus permisos.")) return;
    await api.credentials.delete(cred.id);
    load();
  }

  return (
    <div className="page">
      <div className="pageHeader">
        <h1>Credenciales</h1>
        <button type="button" className="btn btnPrimary" onClick={openCreate} disabled={users.length === 0}>
          <Plus size={16} /> Nueva credencial
        </button>
      </div>
      {users.length === 0 && !loading && <p className="muted">Crea primero un usuario para poder asignarle credenciales.</p>}
      {loading ? (
        <p className="muted">Cargando...</p>
      ) : (
        <table className="table">
          <thead>
            <tr>
              <th>Usuario</th>
              <th>Grupo</th>
              <th>Tipo</th>
              <th>Etiqueta</th>
              <th>Vista previa</th>
              <th>Validez</th>
              <th>Estado</th>
              <th></th>
            </tr>
          </thead>
          <tbody>
            {credentials.map((c) => (
              <tr key={c.id}>
                <td>{userName(c.user_id)}</td>
                <td className="muted">{groupName(c.group_id) || "—"}</td>
                <td>{TYPE_LABELS[c.type] || c.type}</td>
                <td>{c.label || "—"}</td>
                <td>
                  <code>••••{c.value_preview}</code>
                </td>
                <td className="muted">
                  {!c.valid_from && !c.valid_until && "Sin límite"}
                  {c.valid_from && `desde ${c.valid_from.slice(0, 10)} `}
                  {c.valid_until && `hasta ${c.valid_until.slice(0, 10)}`}
                </td>
                <td>
                  <span className={`badge ${c.active ? "badgeSuccess" : "badgeMuted"}`}>{c.active ? "Activa" : "Inactiva"}</span>
                </td>
                <td className="rowActions">
                  <button type="button" className="iconBtn" onClick={() => openEdit(c)}>
                    <Pencil size={16} />
                  </button>
                  <button type="button" className="iconBtn iconBtnDanger" onClick={() => remove(c)}>
                    <Trash2 size={16} />
                  </button>
                </td>
              </tr>
            ))}
            {credentials.length === 0 && (
              <tr>
                <td colSpan={8} className="muted">
                  No hay credenciales todavía.
                </td>
              </tr>
            )}
          </tbody>
        </table>
      )}

      {editing && (
        <Modal title={editing.id ? "Editar credencial" : "Nueva credencial"} onClose={closeModal}>
          <form className="form" onSubmit={save}>
            {error && <p className="formError">{error}</p>}
            {!editing.id && (
              <label>
                Usuario
                <select required value={form.user_id} onChange={(e) => setForm({ ...form, user_id: e.target.value })}>
                  <option value="" disabled>
                    Selecciona un usuario
                  </option>
                  {users.map((u) => (
                    <option key={u.id} value={u.id}>
                      {u.full_name}
                    </option>
                  ))}
                </select>
              </label>
            )}
            {!editing.id && (
              <label>
                Tipo
                <select value={form.type} onChange={(e) => setForm({ ...form, type: e.target.value })}>
                  <option value="rfid">RFID</option>
                  <option value="pin">PIN</option>
                  <option value="nfc">NFC</option>
                </select>
              </label>
            )}
            <label>
              Etiqueta
              <input placeholder="p.ej. Tarjeta principal" value={form.label} onChange={(e) => setForm({ ...form, label: e.target.value })} />
            </label>
            <label>
              Grupo (VIP, Mantenimiento, Zona A...)
              <select value={form.group_id} onChange={(e) => setForm({ ...form, group_id: e.target.value })}>
                <option value="">Sin grupo</option>
                {groups.map((g) => (
                  <option key={g.id} value={g.id}>
                    {g.name}
                  </option>
                ))}
              </select>
            </label>
            <label>
              {editing.id ? "Nuevo valor (dejar vacío para no cambiar)" : "Valor (UID de la tarjeta o PIN)"}
              <input value={form.value} onChange={(e) => setForm({ ...form, value: e.target.value })} placeholder={editing.id ? "sin cambios" : ""} />
            </label>
            {form.type !== "pin" && doors.length > 0 && (
              <div className="enrollRow">
                <select value={enrollDoorId} onChange={(e) => setEnrollDoorId(e.target.value)} disabled={enrolling}>
                  {doors.map((d) => (
                    <option key={d.id} value={d.id}>
                      {d.name}
                    </option>
                  ))}
                </select>
                {enrolling ? (
                  <button type="button" className="btn" onClick={cancelEnroll}>
                    Cancelar lectura
                  </button>
                ) : (
                  <button type="button" className="btn" onClick={startEnroll}>
                    <Scan size={16} /> Leer tarjeta
                  </button>
                )}
                {enrollMessage && <span className="enrollMessage">{enrollMessage}</span>}
              </div>
            )}
            <div className="formGrid">
              <label>
                Válida desde
                <input type="date" value={form.valid_from} onChange={(e) => setForm({ ...form, valid_from: e.target.value })} />
              </label>
              <label>
                Válida hasta
                <input type="date" value={form.valid_until} onChange={(e) => setForm({ ...form, valid_until: e.target.value })} />
              </label>
            </div>
            <label className="checkboxRow">
              <input type="checkbox" checked={form.active} onChange={(e) => setForm({ ...form, active: e.target.checked })} />
              Activa
            </label>
            <div className="formActions">
              <button type="button" className="btn" onClick={closeModal}>
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
