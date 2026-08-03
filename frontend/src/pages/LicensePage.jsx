import { useState, useEffect, useCallback } from "react";
import { ShieldCheck, ShieldAlert, ShieldQuestion } from "lucide-react";
import { api } from "../api.js";

function formatDate(iso) {
  if (!iso) return "—";
  return new Date(iso.endsWith("Z") ? iso : iso + "Z").toLocaleString();
}

export default function LicensePage() {
  const [status, setStatus] = useState(null);
  const [loading, setLoading] = useState(true);
  const [token, setToken] = useState("");
  const [error, setError] = useState("");
  const [saving, setSaving] = useState(false);

  const load = useCallback(() => {
    setLoading(true);
    api.license
      .get()
      .then(setStatus)
      .finally(() => setLoading(false));
  }, []);

  useEffect(load, [load]);

  async function install(e) {
    e.preventDefault();
    setError("");
    setSaving(true);
    try {
      const next = await api.license.install(token.trim());
      setStatus(next);
      setToken("");
    } catch (err) {
      setError(err.message);
    } finally {
      setSaving(false);
    }
  }

  const cardClass = !status ? "" : status.valid ? "lastScanOpen" : "lastScanDenied";
  const Icon = !status ? ShieldQuestion : status.valid ? ShieldCheck : ShieldAlert;

  return (
    <div className="page">
      <div className="pageHeader">
        <h1>Licencia</h1>
      </div>

      {loading ? (
        <p className="muted">Cargando...</p>
      ) : (
        <div className={`lastScanCard ${cardClass}`} style={{ textAlign: "left" }}>
          <div style={{ display: "flex", alignItems: "center", gap: 10 }}>
            <Icon size={22} />
            <span className="lastScanResult">
              {status.valid ? "LICENCIA VÁLIDA" : status.has_license ? "LICENCIA NO VÁLIDA" : "SIN LICENCIA"}
            </span>
          </div>
          <div className="lastScanName">
            {status.used_doors} / {status.valid ? status.max_doors : 0} puertas en uso
          </div>
          <div className="lastScanMeta" style={{ display: "flex", flexDirection: "column", gap: 4, marginTop: 6 }}>
            {status.error && <span>{status.error}</span>}
            {status.customer && <span>Cliente: {status.customer}</span>}
            {status.valid && <span>Emitida: {formatDate(status.issued_at)}</span>}
            {status.valid && <span>Caduca: {status.expires_at ? formatDate(status.expires_at) : "No caduca"}</span>}
          </div>
        </div>
      )}

      <div>
        <h2 className="sectionTitle">Instalar licencia</h2>
        <form className="form" onSubmit={install}>
          {error && <p className="formError">{error}</p>}
          <label>
            Token de licencia
            <textarea
              required
              rows={4}
              placeholder="Pega aquí el token que te ha dado el proveedor"
              value={token}
              onChange={(e) => setToken(e.target.value)}
            />
          </label>
          <p className="hint">
            Sin una licencia válida instalada el sistema no permite tener puertas activas. Si instalas una licencia
            con menos puertas de las que ya tienes creadas, las más recientes se desactivarán automáticamente para
            encajar en el nuevo límite.
          </p>
          <div className="formActions">
            <button type="submit" className="btn btnPrimary" disabled={saving}>
              {saving ? "Instalando..." : "Instalar"}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
}
