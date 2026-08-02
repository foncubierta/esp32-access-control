import { Routes, Route, Navigate } from "react-router-dom";
import ProtectedRoute from "./components/ProtectedRoute.jsx";
import Layout from "./components/Layout.jsx";
import LoginPage from "./pages/LoginPage.jsx";
import UsersPage from "./pages/UsersPage.jsx";
import CredentialsPage from "./pages/CredentialsPage.jsx";
import DoorsPage from "./pages/DoorsPage.jsx";
import PermissionsPage from "./pages/PermissionsPage.jsx";
import LogsPage from "./pages/LogsPage.jsx";

export default function App() {
  return (
    <Routes>
      <Route path="/login" element={<LoginPage />} />
      <Route
        path="/"
        element={
          <ProtectedRoute>
            <Layout />
          </ProtectedRoute>
        }
      >
        <Route index element={<Navigate to="/users" replace />} />
        <Route path="users" element={<UsersPage />} />
        <Route path="credentials" element={<CredentialsPage />} />
        <Route path="doors" element={<DoorsPage />} />
        <Route path="permissions" element={<PermissionsPage />} />
        <Route path="logs" element={<LogsPage />} />
      </Route>
      <Route path="*" element={<Navigate to="/" replace />} />
    </Routes>
  );
}
