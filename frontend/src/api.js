const BASE = "/api";

function getToken() {
  return localStorage.getItem("token");
}

async function req(path, opts = {}) {
  const token = getToken();
  const isFormData = opts.body instanceof FormData;
  const res = await fetch(BASE + path, {
    headers: {
      ...(isFormData ? {} : { "Content-Type": "application/json" }),
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
    uploadPhoto: (id, file) => {
      const form = new FormData();
      form.append("file", file);
      return req(`/users/${id}/photo`, { method: "POST", body: form });
    },
    deletePhoto: (id) => req(`/users/${id}/photo`, { method: "DELETE" }),
    photoUrl: async (id) => {
      const token = getToken();
      const res = await fetch(`${BASE}/users/${id}/photo`, {
        headers: token ? { Authorization: `Bearer ${token}` } : {},
      });
      if (!res.ok) return null;
      return URL.createObjectURL(await res.blob());
    },
  },
  credentials: {
    list: (userId) => req(withQuery("/credentials", { user_id: userId })),
    create: (body) => req("/credentials", { method: "POST", body: JSON.stringify(body) }),
    update: (id, body) => req(`/credentials/${id}`, { method: "PATCH", body: JSON.stringify(body) }),
    delete: (id) => req(`/credentials/${id}`, { method: "DELETE" }),
  },
  groups: {
    list: () => req("/groups"),
    create: (body) => req("/groups", { method: "POST", body: JSON.stringify(body) }),
    update: (id, body) => req(`/groups/${id}`, { method: "PATCH", body: JSON.stringify(body) }),
    delete: (id) => req(`/groups/${id}`, { method: "DELETE" }),
    permissions: {
      list: (params) => req(withQuery("/groups/permissions", params)),
      create: (body) => req("/groups/permissions", { method: "POST", body: JSON.stringify(body) }),
      update: (id, body) => req(`/groups/permissions/${id}`, { method: "PATCH", body: JSON.stringify(body) }),
      delete: (id) => req(`/groups/permissions/${id}`, { method: "DELETE" }),
    },
  },
  doors: {
    list: () => req("/doors"),
    create: (body) => req("/doors", { method: "POST", body: JSON.stringify(body) }),
    update: (id, body) => req(`/doors/${id}`, { method: "PATCH", body: JSON.stringify(body) }),
    rotateKey: (id) => req(`/doors/${id}/rotate-key`, { method: "POST" }),
    trigger: (id) => req(`/doors/${id}/trigger`, { method: "POST" }),
    armEnroll: (id) => req(`/doors/${id}/enroll/arm`, { method: "POST" }),
    enrollStatus: (id) => req(`/doors/${id}/enroll/status`),
    disarmEnroll: (id) => req(`/doors/${id}/enroll`, { method: "DELETE" }),
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
