# Desplegar en un LXC (Debian 12)

Despliegue nativo con systemd + nginx — sin Docker. Pensado para un LXC
(p.ej. de Proxmox) ya creado y con red hacia la LAN donde estarán los
nodos ESP32.

## Vía rápida: script

```bash
git clone https://github.com/foncubierta/esp32-access-control.git
cd esp32-access-control
sudo ./deploy/setup.sh
```

Hace todo lo de este documento: instala paquetes, crea el usuario del
sistema, monta el venv del backend, genera `.env` (con `SECRET_KEY`
aleatoria y una contraseña de admin que te imprime al final si no le pasas
una), levanta el servicio systemd, compila el frontend y configura nginx.
Es **idempotente** — puedes volver a ejecutarlo tras un cambio en el repo
(hace `git pull`, reinstala dependencias, recompila el frontend y reinicia
el servicio) sin que te machaque el `.env` ya existente.

Variables opcionales (por si tu instalación no encaja con los valores por
defecto):

```bash
sudo ADMIN_PASSWORD='miContraseña' INSTALL_DIR=/opt/access-control APP_USER=accesscontrol ./deploy/setup.sh
```

El resto de este documento son los mismos pasos a mano, por si prefieres
ir uno a uno o algo falla y quieres depurarlo.

## 1. Paquetes del sistema

```bash
apt update && apt upgrade -y
apt install -y python3 python3-venv python3-pip nodejs npm nginx git openssl
```

Debian 12 trae Node 18 por defecto, suficiente para compilar el frontend
(Vite 5 requiere Node ≥18).

## 2. Usuario dedicado y código

```bash
useradd --system --create-home --home-dir /opt/esp32-access-control --shell /usr/sbin/nologin access-control
git clone https://github.com/foncubierta/esp32-access-control.git /opt/esp32-access-control
chown -R access-control:access-control /opt/esp32-access-control
```

## 3. Backend

```bash
cd /opt/esp32-access-control/backend
sudo -u access-control python3 -m venv .venv
sudo -u access-control .venv/bin/pip install -r requirements.txt

sudo -u access-control cp .env.example .env
# Genera una SECRET_KEY real y anótala en .env:
openssl rand -hex 32
```

Edita `/opt/esp32-access-control/backend/.env` (como `access-control` o con
`sudoedit`):

```
DATABASE_URL=sqlite:///./data/access_control.db
SECRET_KEY=<pega aquí el valor de openssl rand -hex 32>
ACCESS_TOKEN_EXPIRE_MINUTES=480
ADMIN_USERNAME=admin
ADMIN_PASSWORD=<contraseña real, no dejes "change-me">
CORS_ORIGINS=*
```

`CORS_ORIGINS` no importa demasiado en este despliegue porque nginx sirve
el frontend y la API desde el mismo origen (ver más abajo) — el navegador
nunca hace una petición cross-origin.

### Servicio systemd

```bash
cp /opt/esp32-access-control/deploy/access-control-backend.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable --now access-control-backend
systemctl status access-control-backend   # debe estar "active (running)"
curl -s http://127.0.0.1:8010/api/health  # {"status":"ok"}
```

La base de datos SQLite se crea sola en `backend/data/access_control.db` la
primera vez que arranca (y con ella el usuario admin, con
`ADMIN_USERNAME`/`ADMIN_PASSWORD` del `.env`).

## 4. Frontend

```bash
cd /opt/esp32-access-control/frontend
sudo -u access-control npm install
sudo -u access-control npm run build   # genera dist/
```

### nginx

```bash
cp /opt/esp32-access-control/deploy/nginx.conf /etc/nginx/sites-available/access-control
ln -s /etc/nginx/sites-available/access-control /etc/nginx/sites-enabled/
rm -f /etc/nginx/sites-enabled/default   # evita que la config por defecto choque en el puerto 80
nginx -t
systemctl reload nginx
```

## 5. Comprobar

```bash
ip -4 addr show | grep inet   # apunta la IP del LXC en la LAN
curl -s http://localhost/api/health
```

Desde cualquier equipo de la LAN, abre `http://<ip-del-lxc>/` — deberías
ver el login del panel. Entra con el `ADMIN_USERNAME`/`ADMIN_PASSWORD` que
pusiste en el `.env`.

## 6. Apuntar los nodos ESP32 aquí

En el portal cautivo de cada nodo (ver `firmware/access-node/README.md`),
la **URL del backend** es simplemente:

```
http://<ip-del-lxc>
```

(puerto 80 por defecto — nginx hace de único punto de entrada tanto para
el navegador como para los nodos, igual que ya haces en `dude-modern`).

## Actualizar tras un `git pull`

```bash
cd /opt/esp32-access-control && sudo -u access-control git pull

# si cambió requirements.txt:
cd backend && sudo -u access-control .venv/bin/pip install -r requirements.txt
systemctl restart access-control-backend

# si cambió el frontend:
cd ../frontend && sudo -u access-control npm install && sudo -u access-control npm run build
systemctl reload nginx
```

## Notas

- La base de datos vive en `backend/data/` dentro del propio LXC — para
  una prueba está bien tal cual; si más adelante quieres poder recrear el
  LXC sin perder datos, monta ese directorio en un punto de montaje
  aparte (bind mount desde el host de Proxmox, por ejemplo).
- Este despliegue sirve todo por HTTP plano, coherente con que el
  firmware del ESP32 tampoco soporta HTTPS (ver limitaciones en
  `firmware/access-node/README.md`) — pensado para quedarse dentro de la LAN,
  no para exponerse a Internet tal cual.
