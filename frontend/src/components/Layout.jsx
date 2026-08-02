import { NavLink, Outlet } from "react-router-dom";
import { Users, KeyRound, Layers, DoorOpen, ShieldCheck, ScrollText, LogOut, Eye } from "lucide-react";
import { useAuth } from "../context/AuthContext.jsx";

const NAV = [
  { to: "/users", label: "Usuarios", icon: Users },
  { to: "/credentials", label: "Credenciales", icon: KeyRound },
  { to: "/groups", label: "Grupos", icon: Layers },
  { to: "/doors", label: "Puertas / Nodos", icon: DoorOpen },
  { to: "/permissions", label: "Permisos", icon: ShieldCheck },
  { to: "/guard", label: "Vigilancia", icon: Eye },
  { to: "/logs", label: "Logs", icon: ScrollText },
];

export default function Layout() {
  const { admin, logout } = useAuth();

  return (
    <div className="layout">
      <aside className="sidebar">
        <div className="sidebarBrand">Access Control</div>
        <nav>
          {NAV.map(({ to, label, icon: Icon }) => (
            <NavLink key={to} to={to} className={({ isActive }) => `sidebarLink ${isActive ? "sidebarLinkActive" : ""}`}>
              <Icon size={18} />
              {label}
            </NavLink>
          ))}
        </nav>
        <div className="sidebarFooter">
          <span className="muted">{admin?.username}</span>
          <button type="button" className="iconBtn" title="Cerrar sesión" onClick={logout}>
            <LogOut size={16} />
          </button>
        </div>
      </aside>
      <main className="content">
        <Outlet />
      </main>
    </div>
  );
}
