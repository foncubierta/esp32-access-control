# ESP32 Access Control

Control de accesos multi-puerta. Un nodo **ESP32** por puerta lee una credencial (RFID/NFC/PIN) y dispara un relé conectado a la placa de control de la cancela/puerta (tipo Aprimatic, BFT, etc.). El nodo solo actúa: toda la gestión de usuarios, credenciales y permisos vive en este backend + panel web centralizado.

## Arquitectura

```
[Panel web admin] ──HTTPS──> [Backend FastAPI] <──HTTPS── [Nodo ESP32 puerta A]
                                     │                     [Nodo ESP32 puerta B]
                                 SQLite (data/)             [Nodo ESP32 puerta N]
```

- **Backend** (`backend/`): FastAPI + SQLModel + SQLite. Expone la API de administración (usuarios, credenciales, puertas, permisos, logs) protegida por JWT, y una API separada para los nodos protegida por API key.
- **Frontend** (`frontend/`): React + Vite. Panel de administración para dar de alta usuarios, sus credenciales, las puertas/nodos y qué credencial puede abrir qué puerta (con horario opcional). Incluye una página de **Vigilancia** (`/guard`) pensada para un vigilante: qué puerta mirar, el modo actual de esa puerta con botones grandes para cambiarlo, y quién ha pasado la tarjeta en los últimos segundos (polling cada 1.5s).
- **Nodo ESP32** (firmware, pendiente de implementar): se autentica con una API key por puerta, sincroniza periódicamente la lista de credenciales permitidas para *su* puerta y cachea esa lista localmente — así sigue funcionando aunque se caiga el WiFi. Encola los eventos de apertura y los sube al backend en el siguiente sync.

### Por qué sync periódico y no validación en tiempo real

Un control de acceso físico no puede depender de que la red esté viva en el momento exacto de abrir la puerta. Cada nodo descarga cada pocos minutos (configurable en el propio firmware) las credenciales válidas para su puerta y las guarda en local. El "peor caso" de una revocación es la ventana entre syncs; a cambio, el sistema sigue abriendo/cerrando aunque el backend o el WiFi estén caídos un rato.

### Seguridad de las credenciales

El valor bruto de una credencial (UID de tarjeta, PIN...) **nunca se persiste**. Se guarda solo su hash SHA-256 (`Credential.value_hash`). El endpoint de sync (`/api/node/sync`) también entrega el hash, no el valor — el nodo hashea localmente lo que lee del lector y compara hashes.

## Modelo de datos

- **User**: persona que puede tener credenciales.
- **Credential**: una tarjeta/PIN/tag de un usuario (tipo `rfid` | `pin` | `nfc`), con validez opcional (`valid_from` / `valid_until`).
- **Door**: una puerta/cancela física = un nodo ESP32, con su propia `api_key`.
- **Permission**: une una `Credential` con una `Door`, con horario opcional (`days_of_week`, `time_start`, `time_end`). Sin horario = acceso permitido en cualquier momento.
- **AccessLog**: eventos de apertura/denegación subidos por los nodos, para auditoría. Incluye `door_mode`: en qué modo estaba la puerta cuando ocurrió, independientemente de si la credencial en sí tenía o no acceso (`result`).

Desactivar una `Door` hace que el siguiente `/api/node/sync` le devuelva la lista de credenciales vacía — así es como se bloquea una puerta remotamente.

### Modos de puerta (vista de vigilancia)

Cada `Door` tiene un `mode` que un vigilante puede cambiar en caliente desde la web (página **Vigilancia**), independiente del resultado real de la credencial:

| Modo | Comportamiento |
|---|---|
| `auto` (por defecto) | Dispara el relé solo si la credencial tiene permiso vigente |
| `open` | Puerta libre: cualquier tarjeta dispara el relé, tenga o no permiso |
| `closed` | Nunca dispara el relé, aunque la credencial tenga permiso |
| `identify` | Identifica a la persona (aparece en la web) pero nunca dispara el relé |

El nodo separa la **decisión de acceso** (¿tendría paso esta credencial?) de la **acción física** (¿se dispara el relé?) — así el log siempre registra la verdad sobre la credencial, y el modo se aplica como filtro final antes de tocar el relé. El nodo consulta el modo por un endpoint ligero (`/api/node/mode`) cada pocos segundos, separado del sync completo de credenciales, para que el cambio de modo se note casi al instante sin recargar toda la lista de credenciales en cada poll.

## Correr en local (desarrollo)

### Backend

```bash
cd backend
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env   # y edita SECRET_KEY / ADMIN_PASSWORD
uvicorn main:app --host 0.0.0.0 --port 8010 --reload
```

En el primer arranque se crea un usuario admin con `ADMIN_USERNAME` / `ADMIN_PASSWORD` (por defecto `admin` / `admin` — cámbialo).

### Frontend

```bash
cd frontend
npm install
npm run dev   # Vite en :5174, proxya /api/ a :8010
npm run build # build de producción → dist/
```

## Variables de entorno (backend)

| Variable | Descripción |
|---|---|
| `DATABASE_URL` | Por defecto `sqlite:///./data/access_control.db` |
| `SECRET_KEY` | Firma de los JWT del panel admin — cámbiala en producción |
| `ACCESS_TOKEN_EXPIRE_MINUTES` | Caducidad de la sesión admin (por defecto 480) |
| `ADMIN_USERNAME` / `ADMIN_PASSWORD` | Admin sembrado en el primer arranque si no existe ninguno |
| `CORS_ORIGINS` | Orígenes permitidos, separados por coma |

## API

### Panel admin (JWT, header `Authorization: Bearer <token>`)

```
POST   /api/auth/login              Login admin → { access_token }
GET    /api/auth/me                 Admin autenticado

GET    /api/users                   Listar usuarios
POST   /api/users                   Crear usuario
PATCH  /api/users/:id                Editar usuario
DELETE /api/users/:id                Borrar usuario (cascada: credenciales + permisos)

GET    /api/credentials?user_id=    Listar credenciales (opcionalmente por usuario)
POST   /api/credentials             Crear credencial ({ user_id, type, value, ... })
PATCH  /api/credentials/:id          Editar / rotar valor
DELETE /api/credentials/:id          Borrar credencial (cascada: permisos)

GET    /api/doors                   Listar puertas/nodos
POST   /api/doors                   Crear puerta (genera api_key)
PATCH  /api/doors/:id                Editar puerta (incluye mode: auto|open|closed|identify)
POST   /api/doors/:id/rotate-key     Regenerar api_key del nodo
DELETE /api/doors/:id                Borrar puerta (cascada: permisos)

GET    /api/permissions?door_id=&credential_id=&user_id=   Listar permisos
POST   /api/permissions             Crear permiso ({ credential_id, door_id, days_of_week?, time_start?, time_end? })
PATCH  /api/permissions/:id          Editar / activar / desactivar
DELETE /api/permissions/:id          Borrar permiso

GET    /api/logs?door_id=&credential_id=&result=&since=&limit=   Ver logs de acceso
```

### Nodo ESP32 (header `X-Api-Key: <api_key de la puerta>`)

```
GET    /api/node/sync        Credenciales (hasheadas) + modo, válidas para esta puerta ahora mismo
GET    /api/node/mode        Poll ligero y frecuente de door_active/door_mode (sin la lista de credenciales)
POST   /api/node/logs        Sube en lote los eventos de acceso registrados offline
POST   /api/node/heartbeat   Marca el nodo como visto (last_seen)
```

## Firmware ESP32

Ver [`firmware/access-node/README.md`](firmware/access-node/README.md). PlatformIO +
Arduino. Lee tarjetas Wiegand (D0/D1, cualquier longitud de trama), soporta
WiFi o Ethernet (módulo W5500) configurables por portal cautivo, cachea las
credenciales localmente para seguir funcionando sin red, y sube los logs de
acceso en cuanto puede.

## Pendiente

- Compilar/flashear y probar el firmware contra hardware real (no verificado en este entorno — ver nota de compilación en el README del firmware).
- Endpoint de revocación urgente (push) para no depender solo del intervalo de sync en casos como baja de un usuario.
- Lectores adicionales más allá de Wiegand (teclado/PIN nativo del nodo, NFC vía I2C, etc.) si hacen falta.
