#!/usr/bin/env bash
# Deploys/updates the ESP32 Access Control backend + frontend on a Debian 12
# LXC via systemd + nginx. Safe to re-run: pulls latest instead of
# re-cloning, reuses an existing .env instead of overwriting it, restarts
# rather than fails if the service is already up.
#
# Usage: sudo ./setup.sh
# Optional env vars:
#   REPO_URL       git remote to clone/pull (default: this project on GitHub)
#   INSTALL_DIR    where to put it (default: /opt/esp32-access-control)
#   APP_USER       system user to run it as (default: access-control)
#   ADMIN_PASSWORD admin password for a first-time .env (default: generated + printed)
#   CORS_ORIGINS   default: * (nginx serves same-origin, so this rarely matters)

set -euo pipefail

REPO_URL="${REPO_URL:-https://github.com/foncubierta/esp32-access-control.git}"
INSTALL_DIR="${INSTALL_DIR:-/opt/esp32-access-control}"
APP_USER="${APP_USER:-access-control}"
CORS_ORIGINS="${CORS_ORIGINS:-*}"

if [[ $EUID -ne 0 ]]; then
  echo "Ejecuta este script como root (sudo ./setup.sh)." >&2
  exit 1
fi

echo "==> Paquetes del sistema"
apt-get update -y
apt-get install -y python3 python3-venv python3-pip nodejs npm nginx git openssl curl

echo "==> Usuario del sistema"
if ! id "$APP_USER" &>/dev/null; then
  useradd --system --create-home --home-dir "$INSTALL_DIR" --shell /usr/sbin/nologin "$APP_USER"
  echo "Usuario '$APP_USER' creado."
else
  echo "Usuario '$APP_USER' ya existe, sigo."
fi

echo "==> Código"
if [[ -d "$INSTALL_DIR/.git" ]]; then
  runuser -u "$APP_USER" -- git -C "$INSTALL_DIR" pull
else
  rm -rf "$INSTALL_DIR"
  git clone "$REPO_URL" "$INSTALL_DIR"
  chown -R "$APP_USER:$APP_USER" "$INSTALL_DIR"
fi

BACKEND_DIR="$INSTALL_DIR/backend"
FRONTEND_DIR="$INSTALL_DIR/frontend"

echo "==> Backend: entorno virtual + dependencias"
runuser -u "$APP_USER" -- python3 -m venv "$BACKEND_DIR/.venv"
runuser -u "$APP_USER" -- "$BACKEND_DIR/.venv/bin/pip" install --quiet --upgrade pip
runuser -u "$APP_USER" -- "$BACKEND_DIR/.venv/bin/pip" install --quiet -r "$BACKEND_DIR/requirements.txt"

ENV_FILE="$BACKEND_DIR/.env"
if [[ -f "$ENV_FILE" ]]; then
  echo "==> $ENV_FILE ya existe, no lo toco (borra el fichero si quieres que lo regenere)"
else
  echo "==> Generando $ENV_FILE"
  SECRET_KEY="$(openssl rand -hex 32)"
  if [[ -z "${ADMIN_PASSWORD:-}" ]]; then
    ADMIN_PASSWORD="$(openssl rand -base64 18)"
    GENERATED_PASSWORD=1
  fi
  cat > "$ENV_FILE" <<EOF
DATABASE_URL=sqlite:///./data/access_control.db
SECRET_KEY=$SECRET_KEY
ACCESS_TOKEN_EXPIRE_MINUTES=480
ADMIN_USERNAME=admin
ADMIN_PASSWORD=$ADMIN_PASSWORD
CORS_ORIGINS=$CORS_ORIGINS
EOF
  chown "$APP_USER:$APP_USER" "$ENV_FILE"
  chmod 600 "$ENV_FILE"
  if [[ "${GENERATED_PASSWORD:-0}" == "1" ]]; then
    echo
    echo "    Admin: admin / $ADMIN_PASSWORD"
    echo "    (guardado en $ENV_FILE — cámbiala si quieres una tuya)"
    echo
  fi
fi

mkdir -p "$BACKEND_DIR/data"
chown -R "$APP_USER:$APP_USER" "$BACKEND_DIR/data"

echo "==> Servicio systemd"
cp "$INSTALL_DIR/deploy/access-control-backend.service" /etc/systemd/system/
# The unit ships with the default paths/user — patch them if this install
# doesn't match INSTALL_DIR/APP_USER defaults.
sed -i "s#/opt/esp32-access-control#$INSTALL_DIR#g" /etc/systemd/system/access-control-backend.service
sed -i "s/^User=.*/User=$APP_USER/; s/^Group=.*/Group=$APP_USER/" /etc/systemd/system/access-control-backend.service
systemctl daemon-reload
systemctl enable --now access-control-backend
systemctl restart access-control-backend

echo "==> Frontend: build"
runuser -u "$APP_USER" -- npm install --prefix "$FRONTEND_DIR" --silent
runuser -u "$APP_USER" -- npm run build --prefix "$FRONTEND_DIR" --silent

echo "==> nginx"
cp "$INSTALL_DIR/deploy/nginx.conf" /etc/nginx/sites-available/access-control
sed -i "s#/opt/esp32-access-control#$INSTALL_DIR#g" /etc/nginx/sites-available/access-control
ln -sf /etc/nginx/sites-available/access-control /etc/nginx/sites-enabled/access-control
rm -f /etc/nginx/sites-enabled/default
nginx -t
systemctl reload nginx

echo "==> Comprobando"
sleep 1
if curl -sf http://127.0.0.1:8010/api/health >/dev/null; then
  echo "Backend OK"
else
  echo "El backend no responde — revisa: journalctl -u access-control-backend -e" >&2
fi

IP="$(hostname -I | awk '{print $1}')"
echo
echo "Listo. Panel: http://$IP/"
echo "Los nodos ESP32 deben apuntar la 'URL del backend' del portal cautivo a: http://$IP"
