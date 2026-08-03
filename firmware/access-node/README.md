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

Los pines del lector Wiegand, del relé y del sensor de puerta (más abajo)
**se configuran desde el propio portal** (AP, LAN vía WiFi, o el mini
servidor en modo Ethernet — ver "Reconfigurar sin AP ni botón" más abajo),
no hace falta recompilar para cambiarlos. Lo que hay en `config.h` son solo
los valores de fábrica que salen precargados la primera vez que abres el
portal.

### Lector Wiegand

| Lector | ESP32 (valor de fábrica) |
|---|---|
| D0 | GPIO 4 — campo "Pin Wiegand D0" en el portal |
| D1 | GPIO 16 — campo "Pin Wiegand D1" en el portal |
| GND | GND |
| 12V / 5V | fuente externa del lector (no lo alimentes desde el ESP32) |

D0/D1 son idle-high con pulsos a LOW — usa las resistencias pull-up que
traiga el propio lector; si no las trae, añade 1kΩ–10kΩ a 3.3V (con
level-shifting si el lector es de 5V/12V lógicos). A diferencia del sensor
de puerta, el lector no tiene opción de "deshabilitado" — el pin debe ser
un GPIO válido siempre.

### Relé (hacia la placa Aprimatic/BFT/...)

| Relé | ESP32 (valor de fábrica) |
|---|---|
| IN | GPIO 27 — campo "Pin del rele" en el portal |
| VCC / GND | fuente 3.3V/5V según el módulo |

El contacto NO del relé se cablea en paralelo al pulsador físico de
START/PED de la placa de control. El campo "Rele activo en HIGH?" del
portal indica si tu módulo dispara con HIGH (`1`) o LOW (`0`).

### Sensor de puerta (contacto magnético, opcional)

| Sensor | ESP32 |
|---|---|
| Un terminal | GPIO configurable — campo "Pin sensor de puerta" del portal (`-1` = deshabilitado, valor de fábrica) |
| Otro terminal | GND |

Sin resistencia externa — usa el pull-up interno. Deshabilitado por
defecto (`-1`); asígnale un GPIO libre desde el portal para activarlo. La
mayoría de sensores de puerta llevan el imán en la hoja móvil y cierran el
contacto cuando la puerta está cerrada, lo que deja el pin en LOW (cerrada)
/ HIGH (abierta) con el pull-up interno — es el valor de fábrica del campo
"Sensor cerrado=HIGH?" (`0`). Si el tuyo está al revés, pon ese campo a `1`.
Ver la lógica de detección de "puerta forzada" vs "abierta demasiado
tiempo" en el README principal del proyecto, sección "Sensor de puerta".

### Elegir los pines

Evita los *strapping pins* (0, 2, 5, 12, 15) y los de UART0 (1, 3) para
cualquiera de estos tres — GPIO 0 ya lo usa el botón de config. El
firmware no valida que los pines que metas en el portal no choquen entre
sí ni con los fijos (botón de config, LED de estado, SPI del W5500 en modo
`eth`) — si te equivocas solo se rompe la función física afectada (lector,
relé o sensor), nunca el acceso al portal en sí (usa pines distintos:
botón de config, radio WiFi/Ethernet), así que siempre puedes volver a
`http://<ip-del-nodo>/` y corregirlo.

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

Estos (Ethernet, botón de config, LED de estado) sí son constantes fijas en
`include/config.h` — cámbialas ahí y recompila si tu placa concreta los usa
para otra cosa. Wiegand, relé y sensor de puerta, en cambio, se configuran
sin recompilar desde el portal (ver más arriba).

## Compilar y flashear

Requiere [PlatformIO](https://platformio.org/) (CLI o la extensión de VS Code).

```bash
cd firmware/access-node
pio run                 # compila
pio run -t upload       # compila y flashea (ESP32 conectado por USB)
pio device monitor      # monitor serie a 115200 baudios
```

> **Nota:** este firmware se compila con PlatformIO desde el servidor de
> licencias (ver `esp32-access-control-licensing`, página "Firmware") o en
> tu propia máquina — ya verificado compilando y flasheando en hardware
> real. La política de red de *este* entorno de desarrollo (el asistente)
> sigue bloqueando el registro de paquetes de PlatformIO, así que cualquier
> cambio que se haga aquí sobre `NetworkManager.*` u otros ficheros del
> firmware se revisa a mano pero no se compila en el propio entorno —
> recompílalo (panel de licencias o `pio run` local) antes de flashear un
> cambio nuevo.

## Primer arranque — portal cautivo

El portal AP **solo existe para darle credenciales WiFi al nodo** — es lo
único que hace falta rellenar ahí. Si el nodo no tiene un WiFi guardado (o
mantienes el botón de config 3s al arrancar), levanta su propia red
`AccessNode-Setup`:

1. Conéctate a esa red desde el móvil/portátil.
2. Se abrirá el portal (o entra a `http://192.168.4.1`) — verás el menú
   principal de WiFiManager con dos botones que van a páginas totalmente
   separadas: **"Configure WiFi"** y **"Configuracion"**.
3. Pulsa **"Configure WiFi"**: elige tu red habitual y su contraseña (se
   usan solo si el modo de conexión es "wifi" — en modo "eth" ese paso se
   guarda pero no se usa; en ese modo el nodo ni siquiera pasa por aquí en
   el primer arranque, ver más abajo). **Esta página ya no lleva ningún
   otro campo** — nada de lo demás puede interferir con que el WiFi
   conecte.
4. Guarda — el nodo reinicia, se conecta al WiFi que le diste y arranca en
   modo normal (aunque el resto de la config esté vacío: sin API key
   simplemente no podrá hablar con el backend todavía, pero eso no le
   impide conectarse a tu red).
5. El botón **"Configuracion"** (campos "Red y puerta": modo, URL del
   backend, API key, intervalo de sync, pulso del relé, TZ, etiqueta; y
   "Cableado": pines de Wiegand/relé/sensor) puedes dejarlo sin tocar aquí
   y rellenarlo después desde el navegador, ya conectado a esa WiFi — ver
   "Reconfigurar sin AP ni botón" más abajo. Es el flujo recomendado:
   manejar el móvil/portátil metido en una red WiFi ajena (la del AP) solo
   para lo justo, y el resto ya desde tu red normal.

Un nodo en modo `eth` no necesita pasar por el AP en absoluto la primera
vez — Ethernet no lleva credenciales, así que arranca directo, y toda la
config (API key incluida) se hace desde su portal LAN una vez tiene IP por
DHCP.

Para reconfigurar un nodo ya desplegado (cambiar de WiFi, mover a otra
puerta, etc.), mantén pulsado el botón de config 3 segundos al arrancar —
esto siempre reabre el AP, tenga o no ya un WiFi guardado.

## Reconfigurar sin AP ni botón — portal en la LAN

En cuanto el nodo conecta (WiFi o Ethernet), el mismo formulario del portal
queda disponible en su propia IP dentro de la LAN, sin necesidad de
conectarte a ningún AP ni tocar el botón de config:

1. Mira la IP del nodo en el log serie (`[net] WiFi connected, IP: ...` /
   `[net] Ethernet connected, IP: ...`, seguido de
   `[net] LAN config portal available at http://...`).
2. Abre `http://<ip-del-nodo>/` desde cualquier equipo de esa misma red.
3. En modo `wifi` verás el mismo menú de WiFiManager que en el AP —
   "Configure WiFi" (para cambiar de red, página aparte) y "Configuracion"
   (todo lo demás, agrupado en dos secciones: **"Red y puerta"** — modo,
   backend, API key, sync, pulso del relé, TZ, etiqueta — y **"Cableado"**
   — pines de Wiegand/relé/sensor). En modo `eth` verás una única página
   más simple con todos esos campos juntos (no hay WiFi que separar) —
   está servida por un mini servidor HTTP propio, no por WiFiManager (ver
   nota técnica más abajo).
4. Al guardar, el nodo aplica y persiste los cambios y reinicia solo a los
   pocos segundos.

Puedes desactivar este portal LAN por completo (ambos modos) poniendo
`ENABLE_LAN_CONFIG_PORTAL false` en `include/config.h` si prefieres exigir
siempre el botón físico para cualquier cambio. Es HTTP plano, igual que el
resto de este firmware — pensado para quedarse dentro de la LAN.

> **Nota técnica:** en modo `eth` este portal *no* es WiFiManager — la
> librería `arduino-libraries/Ethernet` que habla con el módulo W5500
> implementa su propia pila TCP/IP por SPI, totalmente aparte de la pila
> WiFi/lwIP sobre la que corre el servidor web de WiFiManager, así que ese
> servidor no puede llegar a la interfaz Ethernet. El portal en modo `eth`
> es un servidor HTTP mínimo hecho a mano (una sola ruta, sin keep-alive)
> que sirve el mismo formulario.

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
- El reporte del sensor de puerta (`POST /api/node/sensor`) no pasa por la
  cola de logs — es de mejor esfuerzo con un reintento corto. Si el nodo
  está sin red durante todo el episodio (se abre y se cierra sin
  conectividad), ese episodio en concreto no llega a reportarse nunca.
