import { useState } from "react";
import { Navigate, useNavigate, useLocation } from "react-router-dom";
import { useAuth } from "../context/AuthContext.jsx";

export default function LoginPage() {
  const { login, token } = useAuth();
  const [username, setUsername] = useState("");
  const [password, setPassword] = useState("");
  const [error, setError] = useState("");
  const [submitting, setSubmitting] = useState(false);
  const navigate = useNavigate();
  const location = useLocation();

  if (token) return <Navigate to="/" replace />;

  async function onSubmit(e) {
    e.preventDefault();
    setError("");
    setSubmitting(true);
    try {
      await login(username, password);
      navigate(location.state?.from || "/", { replace: true });
    } catch {
      setError("Usuario o contraseña incorrectos");
    } finally {
      setSubmitting(false);
    }
  }

  return (
    <div className="loginScreen">
      <form className="loginCard" onSubmit={onSubmit}>
        <h1>Access Control</h1>
        <p className="muted">Panel de administración</p>
        {error && <p className="formError">{error}</p>}
        <label>
          Usuario
          <input autoFocus value={username} onChange={(e) => setUsername(e.target.value)} />
        </label>
        <label>
          Contraseña
          <input type="password" value={password} onChange={(e) => setPassword(e.target.value)} />
        </label>
        <button className="btn btnPrimary" type="submit" disabled={submitting}>
          {submitting ? "Entrando..." : "Entrar"}
        </button>
      </form>
    </div>
  );
}
