import { useState, useEffect, useCallback } from "react";
import { Plus, Pencil, Trash2, DoorOpen } from "lucide-react";
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

const emptyForm = { name: "", description: "", active: true };
const emptyPermForm = { door_id: "", days: [], time_start: "", time_end: "" };

export default function GroupsPage() {
  const [groups, setGroups] = useState([]);
  const [doors, setDoors] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");

  const [editing, setEditing] = useState(null);
  const [form, setForm] = useState(emptyForm);

  const [managingGroup, setManagingGroup] = useState(null);
  const [groupPermissions, setGroupPermissions] = useState([]);
  const [permForm, setPermForm] = useState(emptyPermForm);
  const [permError, setPermError] = useState("");

  const load = useCallback(() => {
    setLoading(true);
    Promise.all([api.groups.list(), api.doors.list()])
      .then(([gs, ds]) => {
        setGroups(gs);
        setDoors(ds);
      })
      .catch((e) => setError(e.message))
      .finally(() => setLoading(false));
  }, []);

  useEffect(load, [load]);

  const doorName = (id) => doors.find((d) => d.id === id)?.name || `#${id}`;

  function openCreate() {
    setForm(emptyForm);
    setError("");
    setEditing({});
  }

  function openEdit(group) {
    setForm({ name: group.name, description: group.description || "", active: group.active });
    setError("");
    setEditing(group);
  }

  async function save(e) {
    e.preventDefault();
    setError("");
    try {
      if (editing.id) {
        await api.groups.update(editing.id, form);
      } else {
        await api.groups.create(form);
      }
      setEditing(null);
      load();
    } catch (err) {
      setError(err.message);
    }
  }

  async function remove(group) {
    if (!confirm(`¿Eliminar el grupo "${group.name}"? Las credenciales que lo tengan asignado se quedan sin grupo.`)) return;
    await api.groups.delete(group.id);
    load();
  }

  function loadGroupPermissions(groupId) {
    api.groups.permissions.list({ group_id: groupId }).then(setGroupPermissions);
  }

  function openManageDoors(group) {
    setManagingGroup(group);
    setPermForm(emptyPermForm);
    setPermError("");
    loadGroupPermissions(group.id);
  }

  function toggleDay(value) {
    setPermForm((f) => ({
      ...f,
      days: f.days.includes(value) ? f.days.filter((d) => d !== value) : [...f.days, value].sort((a, b) => a - b),
    }));
  }

  async function addDoorPermission(e) {
    e.preventDefault();
    setPermError("");
    try {
      await api.groups.permissions.create({
        group_id: managingGroup.id,
        door_id: Number(permForm.door_id),
        days_of_week: permForm.days.length ? permForm.days.join(",") : null,
        time_start: permForm.time_start || null,
        time_end: permForm.time_end || null,
      });
      setPermForm(emptyPermForm);
      loadGroupPermissions(managingGroup.id);
    } catch (err) {
      setPermError(err.message);
    }
  }

  async function toggleDoorPermissionActive(perm) {
    await api.groups.permissions.update(perm.id, { active: !perm.active });
    loadGroupPermissions(managingGroup.id);
  }

  async function removeDoorPermission(perm) {
    await api.groups.permissions.delete(perm.id);
    loadGroupPermissions(managingGroup.id);
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

  const availableDoors = doors.filter((d) => !groupPermissions.some((p) => p.door_id === d.id));

  return (
    <div className="page">
      <div className="pageHeader">
        <h1>Grupos de credenciales</h1>
        <button type="button" className="btn btnPrimary" onClick={openCreate}>
          <Plus size={16} /> Nuevo grupo
        </button>
      </div>
      <p className="muted">
        Perfiles de acceso (VIP, Mantenimiento, Zona A...) que dan a todas sus credenciales acceso a las puertas que le asignes aquí.
        Una credencial además puede seguir teniendo permisos sueltos desde "Permisos".
      </p>
      {loading ? (
        <p className="muted">Cargando...</p>
      ) : (
        <table className="table">
          <thead>
            <tr>
              <th>Nombre</th>
              <th>Descripción</th>
              <th>Estado</th>
              <th></th>
            </tr>
          </thead>
          <tbody>
            {groups.map((g) => (
              <tr key={g.id}>
                <td>{g.name}</td>
                <td className="muted">{g.description || "—"}</td>
                <td>
                  <span className={`badge ${g.active ? "badgeSuccess" : "badgeMuted"}`}>{g.active ? "Activo" : "Inactivo"}</span>
                </td>
                <td className="rowActions">
                  <button type="button" className="iconBtn" title="Puertas del grupo" onClick={() => openManageDoors(g)}>
                    <DoorOpen size={16} />
                  </button>
                  <button type="button" className="iconBtn" onClick={() => openEdit(g)}>
                    <Pencil size={16} />
                  </button>
                  <button type="button" className="iconBtn iconBtnDanger" onClick={() => remove(g)}>
                    <Trash2 size={16} />
                  </button>
                </td>
              </tr>
            ))}
            {groups.length === 0 && (
              <tr>
                <td colSpan={4} className="muted">
                  No hay grupos todavía.
                </td>
              </tr>
            )}
          </tbody>
        </table>
      )}

      {editing && (
        <Modal title={editing.id ? "Editar grupo" : "Nuevo grupo"} onClose={() => setEditing(null)}>
          <form className="form" onSubmit={save}>
            {error && <p className="formError">{error}</p>}
            <label>
              Nombre
              <input required value={form.name} onChange={(e) => setForm({ ...form, name: e.target.value })} placeholder="p.ej. VIP, Mantenimiento, Zona A" />
            </label>
            <label>
              Descripción
              <textarea value={form.description} onChange={(e) => setForm({ ...form, description: e.target.value })} />
            </label>
            <label className="checkboxRow">
              <input type="checkbox" checked={form.active} onChange={(e) => setForm({ ...form, active: e.target.checked })} />
              Activo (desactivarlo quita el acceso a todas sus credenciales)
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

      {managingGroup && (
        <Modal title={`Puertas de "${managingGroup.name}"`} onClose={() => setManagingGroup(null)}>
          <div className="form">
            {groupPermissions.length === 0 ? (
              <p className="muted">Este grupo no da acceso a ninguna puerta todavía.</p>
            ) : (
              <table className="table">
                <thead>
                  <tr>
                    <th>Puerta</th>
                    <th>Horario</th>
                    <th>Estado</th>
                    <th></th>
                  </tr>
                </thead>
                <tbody>
                  {groupPermissions.map((p) => (
                    <tr key={p.id}>
                      <td>{doorName(p.door_id)}</td>
                      <td className="muted">{scheduleLabel(p)}</td>
                      <td>
                        <button
                          type="button"
                          className={`badge badgeButton ${p.active ? "badgeSuccess" : "badgeMuted"}`}
                          onClick={() => toggleDoorPermissionActive(p)}
                        >
                          {p.active ? "Activo" : "Inactivo"}
                        </button>
                      </td>
                      <td className="rowActions">
                        <button type="button" className="iconBtn iconBtnDanger" onClick={() => removeDoorPermission(p)}>
                          <Trash2 size={16} />
                        </button>
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            )}

            {availableDoors.length > 0 ? (
              <form className="form" onSubmit={addDoorPermission}>
                {permError && <p className="formError">{permError}</p>}
                <label>
                  Añadir puerta
                  <select required value={permForm.door_id} onChange={(e) => setPermForm({ ...permForm, door_id: e.target.value })}>
                    <option value="" disabled>
                      Selecciona una puerta
                    </option>
                    {availableDoors.map((d) => (
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
                      className={`dayChip ${permForm.days.includes(d.value) ? "dayChipActive" : ""}`}
                      onClick={() => toggleDay(d.value)}
                    >
                      {d.label}
                    </button>
                  ))}
                </div>
                <div className="formGrid">
                  <label>
                    Desde
                    <input type="time" value={permForm.time_start} onChange={(e) => setPermForm({ ...permForm, time_start: e.target.value })} />
                  </label>
                  <label>
                    Hasta
                    <input type="time" value={permForm.time_end} onChange={(e) => setPermForm({ ...permForm, time_end: e.target.value })} />
                  </label>
                </div>
                <div className="formActions">
                  <button type="submit" className="btn btnPrimary">
                    Añadir puerta al grupo
                  </button>
                </div>
              </form>
            ) : (
              doors.length > 0 && <p className="muted">Ya tiene acceso a todas las puertas.</p>
            )}
          </div>
        </Modal>
      )}
    </div>
  );
}
