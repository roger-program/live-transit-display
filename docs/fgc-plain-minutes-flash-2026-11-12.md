# Flash autorizado de FGC sin S — 12/11/2026

**Aplicación flasheada, verificada por lectura y arrancada. Feed autenticado observado; apariencia física del LED pendiente del usuario.** Se usó exclusivamente el binario ya compilado solicitado. Sin recompilar ni cambiar código, configuración o backend.

## Preflight

Puerto verificado: **/dev/cu.usbmodem3101**, único USB Serial/JTAG Espressif enumerado. Las herramientas existentes son Python/pyserial del entorno del proyecto y esptool **4.9.0** instalado por PlatformIO. Se comprobó ESP32-S3, flash **32 MB**, secure boot y cifrado flash deshabilitados según el prototipo. Identidad del chip reconfirmada antes de escribir; MAC no publicada. No se usó `--force`.

Binario: `artifacts/firmware/hub75-live-fgc-plain-minutes-2026-11-12/firmware.bin`, **954.384 bytes**, SHA-256 **d5d906b98dadca489b34dfd57a5b764741f429ebc6135f932942c85d1713adbb**. Hash y tamaño reconfirmados inmediatamente antes de escribir; no se ejecutó PlatformIO build/upload.

Lecturas privadas previas del intervalo 0x0–0xffff y de identidad 0x410000–0x415fff. La tabla instalada coincide con la preparada: NVS/Wi-Fi en 0x9000/0x5000; factory app en 0x10000/0x400000; device-auth en 0x410000/0x6000. Sin repetir un backup completo del chip. Lecturas sensibles retenidas únicamente en almacenamiento privado ignorado; nunca impresas ni adjuntadas.

## Escritura y conservación

Esptool escribió **solo la aplicación en 0x10000**, con `--flash_mode keep --flash_freq keep --flash_size keep`. No se suministraron bootloader, tabla, helper ni imagen de identidad.

- Escritura terminada y hash verificado por esptool: **22:56:37.775 Madrid**.
- Rango de bytes de aplicación: **[0x10000, 0xf9010)**.
- Sectores afectados: **[0x10000, 0xfa000)**; dentro de factory, lejos de ambas regiones NVS.
- Lectura posterior completa de los 954.384 bytes: SHA-256 coincide exactamente con el binario solicitado.
- Comparación previa/posterior **antes de arrancar la aplicación**: bootloader, tabla, Wi-Fi NVS y helper (0x0–0xffff) idénticos; identidad (24 KiB en 0x410000) idéntica.
- Verificación completada: **22:57:06.561 Madrid**. Sin erase-all, reprovisión o regeneración de credenciales.

Esto prueba conservación byte a byte durante la escritura. El arranque normal puede realizar sus operaciones NVS habituales; no se bloqueó ni modificó ese comportamiento.

## Arranque observado

Hard reset mediante esptool/RTS: **22:57:06.828 Madrid**. Observación serie acotada, conservando únicamente mensajes permitidos sin SSID, tokens, identidades ni cuerpos de respuesta:

- **22:57:07.274:** `Firmware prototype-0.1.0`, flash 32 MB y panel habilitado.
- GPIO0 inicial HIGH; no entrada solicitada en configuración Wi-Fi.
- Estados iniciales OFFLINE y CLOCK, resueltos durante el arranque.
- **22:57:14.390:** `Feed OK; generatedAt=1794520633961; stops=2; arrivals=14; realtime=11`.

La aplicación respondió y obtuvo un feed autenticado en aproximadamente ocho segundos. No se capturó una línea Wi-Fi connected independiente; el feed correcto establece conectividad operativa sin revelar qué red se usa. No fue necesario enviar comandos serie, cambiar brillo o abrir configuración. Se cerró el monitor dejando el dispositivo funcionando.

No se infiere qué servicios concretos estaban incluidos a partir de los contadores serie, ni se afirma aceptación visual de los minutos sin S. No se consultó producción desde el agente ni se alteró backend/código; el dispositivo hizo su polling normal. Fuente del firmware conserva los hashes de la preparación; el binario no cambió.

[Evidencia segura](evidence/fgc-plain-minutes-flash-2026-11-12.json). Siguiente acción: usuario comprueba en el LED los minutos FGC sin S, la rotación de tres salidas/60 minutos y los servicios configurados.
