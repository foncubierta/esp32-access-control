const BASE = "/api";

function getToken() {
  return localStorage.getItem("token");
}

async function req(path, opts = {}) {
  const token = getToken();
  const res = await fetch(BASE + path, {
    headers: {
      "Content-Type": "application/json",
      ...(token ? { Authorization: `Bearer ${token}` } : {}),
    },
    ...opts,
  });
  if (res.status === 401) {
    localStorage.removeItem("token");
    if (!location.pathname.startsWith("/login")) location.href = "/login";
    throw new Error("No autenticado");
  }
  if (!res.ok) {
    let detail = `Error ${res.status}`;
    try {
      const body = await res.json();
      detail = body.detail || detail;
    } catch {
      // response wasn't JSON, keep the generic message
    }
    throw new Error(detail);
  }
  if (res.status === 204) return null;
  return res.json();
}

function withQuery(path, params) {
  const qs = new URLSearchParams(
    Object.fromEntries(Object.entries(params || {}).filter(([, v]) => v !== undefined && v !== null && v !== ""))
  ).toString();
  return qs ? `${path}?${qs}` : path;
}

export const api = {
  auth: {
    login: (username, password) =>
      req("/auth/login", { method: "POST", body: JSON.stringify({ username, password }) }),
    me: () => req("/auth/me"),
  },
  users: {
    list: () => req("/users"),
    create: (body) => req("/users", { method: "POST", body: JSON.stringify(body) }),
    update: (id, body) => req(`/users/${id}`, { method: "PATCH", body: JSON.stringify(body) }),
    delete: (id) => req(`/users/${id}`, { method: "DELETE" }),
  },
  credentials: {
    list: (userId) => req(withQuery("/credentials", { user_id: userId })),
    create: (body) => req("/credentials", { method: "POST", body: JSON.stringify(body) }),
    update: (id, body) => req(`/credentials/${id}`, { method: "PATCH", body: JSON.stringify(body) }),
    delete: (id) => req(`/credentials/${id}`, { method: "DELETE" }),
  },
  doors: {
    list: () => req("/doors"),
    create: (body) => req("/doors", { method: "POST", body: JSON.stringify(body) }),
    update: (id, body) => req(`/doors/${id}`, { method: "PATCH", body: JSON.stringify(body) }),
    rotateKey: (id) => req(`/doors/${id}/rotate-key`, { method: "POST" }),
    delete: (id) => req(`/doors/${id}`, { method: "DELETE" }),
  },
  permissions: {
    list: (params) => req(withQuery("/permissions", params)),
    create: (body) => req("/permissions", { method: "POST", body: JSON.stringify(body) }),
    update: (id, body) => req(`/permissions/${id}`, { method: "PATCH", body: JSON.stringify(body) }),
    delete: (id) => req(`/permissions/${id}`, { method: "DELETE" }),
  },
  logs: {
    list: (params) => req(withQuery("/logs", params)),
  },
};
