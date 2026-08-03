import { useState } from "react";
import { api } from "../api.js";
import Modal from "./Modal.jsx";

export default function ChangePasswordModal({ onClose }) {
  const [currentPassword, setCurrentPassword] = useState("");
  const [newPassword, setNewPassword] = useState("");
  const [confirmPassword, setConfirmPassword] = useState("");
  const [error, setError] = useState("");
  const [success, setSuccess] = useState(false);

  async function save(e) {
    e.preventDefault();
    setError("");
    setSuccess(false);
    if (newPassword !== confirmPassword) {
      setError("Las contraseñas nuevas no coinciden");
      return;
    }
    try {
      await api.auth.changePassword(currentPassword, newPassword);
      setSuccess(true);
      setCurrentPassword("");
      setNewPassword("");
      setConfirmPassword("");
    } catch (err) {
      setError(err.message);
    }
  }

  return (
    <Modal title="Cambiar contraseña" onClose={onClose}>
      <form className="form" onSubmit={save}>
        {error && <p className="formError">{error}</p>}
        {success && <p className="hint">Contraseña actualizada.</p>}
        <label>
          Contraseña actual
          <input
            type="password"
            required
            autoComplete="current-password"
            value={currentPassword}
            onChange={(e) => setCurrentPassword(e.target.value)}
          />
        </label>
        <label>
          Nueva contraseña
          <input
            type="password"
            required
            minLength={8}
            autoComplete="new-password"
            value={newPassword}
            onChange={(e) => setNewPassword(e.target.value)}
          />
        </label>
        <label>
          Repite la nueva contraseña
          <input
            type="password"
            required
            minLength={8}
            autoComplete="new-password"
            value={confirmPassword}
            onChange={(e) => setConfirmPassword(e.target.value)}
          />
        </label>
        <div className="formActions">
          <button type="button" className="btn" onClick={onClose}>
            Cerrar
          </button>
          <button type="submit" className="btn btnPrimary">
            Guardar
          </button>
        </div>
      </form>
    </Modal>
  );
}
