# X por horario — flasheo autorizado de A, 15 noviembre 2026

**Aplicación instalada, lectura verificada y arranque/feed observados. Apariencia
física pendiente del usuario.** Se instaló el candidato ya revisado de
[X por llegada programada](scheduled-arrival-x-2026-11-15.md), sin recompilar ni
modificar código. Fuente `7bd16ea`, hashes de los tres archivos contrastados con
la evidencia de las 78 pruebas y build previos; validación reutilizada.
[Evidencia segura de ejecución](evidence/scheduled-arrival-x-flash-2026-11-15.json).

## Verificaciones y respaldo

Puerto **/dev/cu.usbmodem3101**, único Espressif USB Serial/JTAG; ESP32-S3, flash
32 MiB, chip e identidad correspondientes a A verificados privadamente. Secure
boot/cifrado flash deshabilitados. Tabla con MD5 válida e idéntica al build:

| Partición | Offset | Tamaño |
| --- | --- | --- |
| NVS/Wi-Fi | 0x9000 | 0x5000 |
| factory | 0x10000 | 0x400000 |
| device-auth | 0x410000 | 0x6000 |

Dos lecturas independientes de toda factory (4.194.304 bytes) coinciden; copia
completa SHA-256 **`543f2f4f7b7d0ada49213d7519bcc15e0065b1d17006d31fa488457de401072c`**.
Aplicación instalada identificada por lectura real: TRAM anterior, 954.496 bytes,
prefijo SHA-256 `b0cdb117f9e9db25a717e57bf71b575ea3f1b44ee4875027811a0f13acf84567`.
NVS/identidad presentes y legibles; respaldos sensibles privados, no publicados.

Backup de aplicación para rollback:
`/Users/rogerpau/mamp/live-transit-feed/data/device-firmware-private/scheduled-x-flash-2026-11-15/application-before-full.bin`.
Hash revalidado al finalizar. Directorio privado ignorado, con segunda lectura,
comparaciones protegidas, binario candidato y logs de herramienta sin publicar.

## Escritura y verificación (Madrid)

Candidato **955.184 bytes**, SHA-256
**`72f630673d49bb8e18c15a5a65bc9c212380bdac77e5701a35c189ed23c64179`**.
Cabe en factory. Escritura exclusiva en **0x10000**, con modo/frecuencia/tamaño
keep; sin erase_flash, --force, escrituras de bootloader/tabla/helper/NVS/identidad.

- Inicio **22:20:29.503**; escritura/hash esptool verificados **22:20:41.256**.
- Lectura posterior completa de factory (4 MiB), verificada **22:21:40.808**:
  prefijo exactamente igual al candidato, por bytes y SHA-256. Padding del último
  sector a FF y resto de aplicación fuera de los sectores afectados idénticos.
- Hash de factory posterior:
  `98090fb3e7defabc78873c8109c91be01f26deb1d5be28560295826f8ad406cc`.
- Prefijo 0x0–0xffff (bootloader/tabla/Wi-Fi/helper) y device-auth comparados antes
  del reinicio: **idénticos byte a byte**. El arranque puede hacer sus operaciones
  NVS normales; no se ha intervenido ese comportamiento.
- Hard reset **22:21:40.813**. Un solo dispositivo flasheado; sin cambios de brillo,
  configuración, credenciales, identidad ni red.

## Funcionamiento observado

Panel enabled y GPIO0 HIGH durante arranque; **Wi-Fi connected** capturado a las
**22:21:43.484**. OFFLINE y CLOCK iniciales se resolvieron. Dos resúmenes Feed OK,
cada uno con **4 paradas / 17 llegadas / 14 realtime**, capturados a las 22:22:20.
Sus generatedAt distintos (1794777707721 y 1794777740562) corresponden a dos feeds
separados por 32,841 segundos; las marcas de recepción serie pueden agruparse por
buffering y no se presentan como tiempos exactos de ejecución de cada evento.
Sin fallos de ejecución, inicialización o parsing observados. Solo se guardaron
mensajes permitidos; no raw serie/SSID/tokens/cuerpos. Monitor cerrado, A funcionando.

GET autenticado antes/después: HTTP 200, Feed v1, private/no-store, revisión **7**,
mismo hash de selección. El firmware no registra la revisión: esta se corrobora
por el servidor y la identidad conservada, no por un acuse directo del ESP32.
Última lectura **22:23:29**: FGC 3 programadas, Bus 9 realtime, L'Illa 2 realtime y
Glòries 1 realtime. TRAM estaba stale en la comprobación previa y volvió a partial
con predicciones; no se cambió la adquisición ni se atribuye ese cambio al flash.

## Alcance y siguiente paso

X 3×3 ámbar junto a cada horario scheduled; realtime sin marca incluso en partial;
misma regla en todos los operadores y sin indicador global de cobertura.
Reglas de unknown/caducidad existentes conservadas. No se desplegó Worker, operó
D1 ni cambió selección alguna. Metro no está seleccionado en la revisión 7.

**Usuario debe comprobar visualmente las X, las filas mixtas y la legibilidad en
la matriz física.** El hash, las pruebas y los feeds correctos no constituyen
aceptación de apariencia ni precisión de llegadas. No se reclama esa aceptación.

## Rollback (no ejecutado)

Volver a identificar A y verificar tabla y hash `543f2f4f…` del backup completo.
Escribir solo esa aplicación de 4 MiB en 0x10000 con flash_mode/freq/size keep;
termina en 0x410000, antes de device-auth. Releer y exigir el SHA completo anterior,
comparar las regiones protegidas antes del reinicio y comprobar dos feeds tras
arrancar. No usar erase-all ni subir bootloader/tabla/NVS. Restauraría el TRAM
anterior (incluido aviso partial con fallback tipográfico y minutos FGC sin marca),
sin necesidad de cambiar el backend. Cualquier otro cambio requiere revisión aparte.
