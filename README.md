# VoiceChat para WoW 1.12.1

Chat de voz entre jugadores (grupo, banda o hermandad) con push-to-talk.

## Piezas
| Carpeta | Qué es |
|---|---|
| `Interface/AddOns/VoiceChat` | Addon Lua: interfaz, tecla PTT, cálculo de sala, indicador de quién habla |
| `dll/voicechat.cpp` | DLL de 32 bits: micrófono, Opus, UDP, altavoces y puente a Lua |
| `server/relay.py` | Relay UDP (uno por comunidad; necesita un puerto UDP abierto) |

## Cómo funciona
1. El addon calcula la sala: `reino:party:<líder>`, `reino:raid:<líder>` o `reino:guild:<hermandad>`. Todos los miembros obtienen el mismo nombre.
2. La DLL aplica un hash a la sala, captura el micro a 16 kHz y lo codifica con Opus (~20 kbps).
3. Mientras pulsas PTT, envía paquetes UDP al relay.
4. El relay los reenvía al resto de la sala. Cada cliente los decodifica y los mezcla.

## Instalación
1. **Relay:** `python3 server/relay.py 7777` en un VPS y abre el puerto UDP 7777.
2. **Compilar la DLL (32 bits):**
   - Descarga `miniaudio.h` y ponlo en `dll/`.
   - Instala libopus de 32 bits (MinGW i686).
   - `i686-w64-mingw32-g++ -O2 -std=c++17 -shared -static -o voicechat.dll voicechat.cpp -I. -lopus -lws2_32 -lole32`
3. **Cargar la DLL en el cliente:** copia `voicechat.dll` junto a `WoW.exe` y añádela a `dlls.txt`. Esto requiere un loader tipo VanillaFixes o SuperWoW.
4. **Addon:** copia `Interface/AddOns/VoiceChat` a la carpeta del juego.
5. **En el juego:**
   - `/vc server TU_IP:7777`
   - `/console reloadui`
   - Asigna la tecla en *Opciones > Teclas > VoiceChat*.

## Advertencias (importante)
- **No lo he compilado ni probado:** aquí no hay compilador MinGW ni cliente de WoW.
- **Direcciones de memoria:** las de `FrameScript_RegisterFunction` y de la API Lua (`lua_tostring`, etc.) son las que recuerdo del cliente 5875. Verifícalas en tu `WoW.exe` con un desensamblador (Ghidra o x64dbg) antes de usarlas. Si son incorrectas, el cliente se cierra o la DLL muestra un aviso.
- **Registro de funciones:** la DLL las registra tras una espera fija de 3 s, que es frágil. Lo más robusto es hacer un hook de un punto de inicialización, como hacen SuperWoW o nampower.
- **Cifrado y privacidad:** el audio va sin cifrar y el relay solo reenvía. Para una comunidad real, añade cifrado (por ejemplo libsodium) y control de acceso.
- **Compatibilidad:** algunos servidores prohíben DLLs inyectadas. Si el servidor tiene anticheat, puede bloquearla.
