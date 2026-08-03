# license-tool

Herramienta **solo para el vendedor** — no se despliega en el servidor del
cliente ni forma parte de la app. Sirve para generar el par de claves y,
con la clave privada, emitir licencias firmadas que limitan cuántas
puertas (nodos ESP32) puede tener activas un despliegue.

El cliente no tiene acceso SSH ni a la base de datos, así que la licencia
no protege contra manipulación directa del servidor — protege contra que
alguien active más puertas de las que pagó simplemente editando un campo:
solo quien tiene la clave privada puede emitir un token que el backend
acepte como válido.

## 1. Generar el par de claves (una sola vez)

```bash
python3 -m venv .venv && source .venv/bin/activate
pip install cryptography pyjwt
python3 generate_keypair.py
```

Esto crea `keys/private_key.pem` y `keys/public_key.pem` (la carpeta
`keys/` está en `.gitignore`, igual que cualquier `*.pem` suelto aquí).

- **`private_key.pem`**: guárdala en un sitio seguro (gestor de
  contraseñas, disco offline...). No la subas nunca a git, no se la
  mandes al cliente. Es lo único que hace falta para emitir licencias.
- **`public_key.pem`**: cópiala a `backend/license_public_key.pem` en el
  repo desplegable y haz commit. El backend solo la usa para *verificar*
  firmas, nunca puede emitir licencias con ella.

```bash
cp keys/public_key.pem ../backend/license_public_key.pem
```

## 2. Emitir una licencia para un cliente

```bash
python3 generate_license.py --max-doors 3 --customer "Acme S.L." --expires-days 365
```

Imprime un token (JWT firmado) por stdout. Pégalo en la página
**Licencia** del panel de administración del cliente (`PUT /api/license`).

- `--max-doors`: cuántas puertas puede tener activas el despliegue.
- `--customer`: texto libre, solo se muestra en el panel — no afecta a
  nada.
- `--expires-days`: opcional. Sin esto la licencia no caduca. Para una
  licencia temporal (p.ej. una prueba de 30 días) o una renovación anual,
  indícalo.

## Qué pasa si no hay licencia válida

Sin licencia instalada, o si caducó o la firma no es válida, el sistema
permite **0 puertas** — no hay nivel gratuito. Si se instala una licencia
con menos puertas de las que el cliente ya tiene creadas, el sistema
desactiva automáticamente las puertas más recientes hasta encajar en el
nuevo límite (nunca borra nada, solo las deja inactivas).
