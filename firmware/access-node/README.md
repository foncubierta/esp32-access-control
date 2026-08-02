# access-node — firmware ESP32

Firmware para el nodo que va en cada puerta: lee una tarjeta Wiegand, decide
si tiene paso (con la lista de credenciales cacheada localmente) y dispara
el relé conectado a la placa de control de la cancela. Sincroniza con el
backend cada pocos minutos y sube los logs de acceso en cuanto puede.

## Compatibilidad de lectoras

Soporta cualquier lector Wiegand por D0/D1, de cualquier longitud de trama
(26, 34, 37, 58 bits...). No hay un esquema de paridad estandarizado más
allá del 26-bit clásico, así que en vez de intentar adivinar el formato de
cada fabricante, cada trama se captura tal cual y se convierte en un valor
canónico `W<bits>:<HEX>` (p.ej. `W26:0A3F91`) — ese es el valor que se
hashea y se compara, igual para cualquier lector. Para tramas de 26 bits
además se decodifica Facility Code/Card Number, solo a efectos de mostrarlo
por Serial (el matching real siempre usa el valor crudo).

### Dar de alta una credencial nueva

**Vía normal — botón "Leer tarjeta"** en Credenciales > Nueva credencial del
panel web: elige esta puerta como lector, pasa la tarjeta, y el valor
aparece solo. Por debajo, el nodo detecta `enroll_armed=true` en su
siguiente poll de `/api/node/mode` (cada 2s) y, en el próximo escaneo,
reporta el valor crudo a `POST /api/node/enroll` — no afecta ni al relé ni
a la decisión de acceso normal, es un reporte aparte y de un solo uso.

**Vía monitor serie** (si no tienes el nodo alcanzable desde el panel en
ese momento, o prefieres hacerlo así):

1. Conecta el nodo por USB y abre el monitor serie (`pio device monitor`).
2. Pasa la tarjeta por el lector. Verás una línea `[card] ... raw=W26:0A3F91 ...`.
3. Copia ese `W26:0A3F91` (incluida la `W` y el número de bits) y pégalo tal
   cual en el campo "Valor" al crear la credencial en el panel web.

## Wiring

### Lector Wiegand

| Lector | ESP32 |
|---|---|
| D0 | GPIO 4 (`PIN_WIEGAND_D0`) |
| D1 | GPIO 16 (`PIN_WIEGAND_D1`) |
| GND | GND |
| 12V / 5V | fuente externa del lector (no lo alimentes desde el ESP32) |

D0/D1 son idle-high con pulsos a LOW — usa las resistencias pull-up que
traiga el propio lector; si no las trae, añade 1kΩ–10kΩ a 3.3V (con
level-shifting si el lector es de 5V/12V lógicos).

### Relé (hacia la placa Aprimatic/BFT/...)

| Relé | ESP32 |
|---|---|
| IN | GPIO 27 (`PIN_RELAY`) |
| VCC / GND | fuente 3.3V/5V según el módulo |

El contacto NO del relé se cablea en paralelo al pulsador físico de
START/PED de la placa de control. `RELAY_ACTIVE_HIGH` en `config.h` indica
si tu módulo dispara con HIGH o LOW.

### Ethernet (módulo W5500, opcional — solo si el nodo se configura en modo "eth")

| W5500 | ESP32 (VSPI) |
|---|---|
| SCK | GPIO 18 |
| MISO | GPIO 19 |
| MOSI | GPIO 23 |
| CS | GPIO 5 |
| RST | GPIO 33 |
| VCC / GND | 3.3V / GND |

### Otros

| Función | ESP32 |
|---|---|
| Botón de config (mantener 3s al arrancar) | GPIO 0 (botón BOOT de la mayoría de DevKits) |
| LED de estado | GPIO 2 |

Todos los pines son constantes en `include/config.h` — cámbialos si tu
placa concreta los usa para otra cosa.

## Compilar y flashear

Requiere [PlatformIO](https://platformio.org/) (CLI o la extensión de VS Code).

```bash
cd firmware/access-node
pio run                 # compila
pio run -t upload       # compila y flashea (ESP32 conectado por USB)
pio device monitor      # monitor serie a 115200 baudios
```

> **Nota:** este firmware se ha revisado a mano línea por línea pero **no
> se ha podido compilar en este entorno** — la política de red del sandbox
> bloquea el registro de paquetes de PlatformIO. Compílalo en tu máquina
> antes de flashear. El punto más probable de fallo si tu toolchain resuelve
> una versión antigua de mbedtls (anterior a la 3.x) es `CredentialHash.cpp`:
> si `pio run` se queja de `mbedtls_sha256_starts`/`_update`/`_finish`,
> añade el sufijo `_ret` a esas tres llamadas (`mbedtls_sha256_starts_ret`,
> etc. — pasan a devolver `int`, comprueba que sea `0`). El resto del
> fichero no cambia.

## Primer arranque — portal cautivo

Si el nodo no tiene configuración guardada (o mantienes el botón de config
3s al arrancar), levanta su propia red WiFi `AccessNode-Setup`:

1. Conéctate a esa red desde el móvil/portátil.
2. Se abrirá el portal (o entra a `http://192.168.4.1`).
3. Elige tu WiFi habitual y su contraseña (se usan solo si el modo de
   conexión es "wifi" — en modo "eth" ese paso se guarda pero no se usa).
4. Rellena los campos propios del nodo:
   - **Modo**: escribe `wifi` o `eth`.
   - **URL del backend**: p.ej. `http://192.168.1.10:8010`.
   - **API key de esta puerta**: la que te da el panel al crear la puerta
     (Puertas/Nodos → icono del ojo).
   - **Intervalo de sync**, **duración del pulso del relé**, **TZ** (por
     defecto Europe/Madrid) y una etiqueta opcional.
5. Guarda — el nodo reinicia y arranca ya en modo normal.

Para reconfigurar un nodo ya desplegado (cambiar de WiFi, mover a otra
puerta, etc.), mantén pulsado el botón de config 3 segundos al arrancar.

## Modos de puerta y disparo manual

El nodo consulta `GET /api/node/mode` cada 2s (más frecuente que el sync
completo de credenciales) para enterarse rápido de dos cosas que cambia el
vigilante desde la web:

- **Modo** (`auto`/`open`/`closed`/`identify`): se aplica como filtro final
  justo antes de `Relay.pulse()` — la evaluación de la credencial (¿tendría
  paso?) siempre se calcula igual y se sube al log tal cual, sea cual sea el
  modo.
- **Disparo manual** ("Abrir ahora" en la web): el backend lleva un
  contador (`trigger_seq`) que sube en cada clic; el nodo recuerda el último
  valor que ya atendió y, en cuanto ve que cambió, dispara un pulso una sola
  vez (respeta que la puerta esté activa, pero no el modo — es una
  anulación explícita del vigilante) y sube un log con
  `reason=manual_trigger`. Al arrancar, el nodo solo fija el valor de
  partida sin disparar, para no repetir un clic que hubiera quedado
  pendiente de una desconexión anterior.

## Horarios y validez

El nodo sincroniza hora por NTP usando la TZ configurada (string POSIX,
ver [nayarsystems/posix_tz_db](https://github.com/nayarsystems/posix_tz_db)
para encontrar la de tu zona) y evalúa `days_of_week`/`time_start`/
`time_end` en hora local — así los horarios que configuras en el panel
web se cumplen aunque el servidor esté en UTC.

## Limitaciones conocidas (v1)

- La cola de logs pendientes de subir vive solo en RAM: sobrevive a caídas
  de red (para eso es el sync periódico) pero no a un reinicio/corte de
  corriente antes del siguiente flush.
- Solo HTTP plano hacia el backend (sin TLS) — pensado para LAN, igual que
  el resto del proyecto.
- Un lector y un relé por nodo. Para una puerta con lector de entrada y
  salida, o doble relé (abrir/cerrar por separado), habría que extender
  `WiegandReader`/`RelayController` — pídelo si lo necesitas.
