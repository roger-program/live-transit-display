# TRAM — flasheo controlado de A, 15 noviembre 2026

**Aplicación flasheada, verificada y arrancada; dos feeds autenticados observados.
Apariencia física pendiente del usuario.** Sin cambios de código, backend, D1,
selecciones, credenciales o Wi-Fi. [Evidencia segura](evidence/tram-flash-2026-11-15.json).

## Preflight y copia

Puerto **`/dev/cu.usbmodem3101`**, único Espressif USB Serial/JTAG enumerado;
ESP32-S3 con flash **32 MiB**. Chip contrastado privadamente con el A de la sesión
anterior; identidad NVS contrastada con la credencial existente de A. MAC, registros
NVS, SSID y tokens no se publican. Secure boot y cifrado flash deshabilitados.

Pruebas nativas **77/77** y build **hub75-live** correctos. Fuente `585364e`, árbol
limpio antes del trabajo; sin cambios de código. Entorno hub75-live hereda modo
normal APP_MODE=0 y activa panel; no es diagnóstico ni fixture. El binario incluye
la identidad TRAM y el camino de feed live. Tamaño real del `.bin` **954.496 bytes**
(el informe del enlazador indica 954.137 bytes usados), SHA-256:

`b0cdb117f9e9db25a717e57bf71b575ea3f1b44ee4875027811a0f13acf84567`

Tabla instalada y MD5 comprobados; coincide con la tabla del build:

| Región | Offset | Tamaño |
| --- | --- | --- |
| NVS / Wi-Fi | 0x9000 | 0x5000 |
| factory / aplicación | 0x10000 | 0x400000 |
| device-auth | 0x410000 | 0x6000 |

Antes de escribir se leyeron el prefijo protegido 0x0–0xffff y device-auth completos;
ambas NVS presentes, no vacías y legibles. Se hicieron **dos lecturas independientes
completas de la aplicación, 4.194.304 bytes**, idénticas. Hash de la copia completa:

`ee8578fb47a1916884cff07c9b590c3d95d2fc434a1b926eab18c58e27843890`

El prefijo de esa aplicación coincide exactamente con el binario conocido
**hub75-live FGC sin S de 12/11**, 954.384 bytes, SHA-256
`d5d906b98dadca489b34dfd57a5b764741f429ebc6135f932942c85d1713adbb`.
Se identificó por lectura real; no se asumió a partir del último build.

Copia privada persistente de rollback, no adjuntada ni versionada:
`/Users/rogerpau/mamp/live-transit-feed/data/device-firmware-private/tram-flash-2026-11-15/application-before-full.bin`.
El mismo directorio restringido contiene la segunda lectura, el binario nuevo,
lectura posterior y respaldos sensibles de regiones protegidas; no publicar esas
imágenes ni sus logs privados. Hash de la copia revalidado al cerrar la sesión.

## Escritura y verificación (hora Madrid)

- **21:10:55.537:** inicio; escritura exclusiva en **0x10000**, flash_mode/freq/size
  keep. El candidato cabe en factory. Sin erase-all, --force, helper, bootloader,
  tabla o NVS en los argumentos de escritura.
- **21:11:07.210:** escritura terminada, hash verificado por esptool 4.9.0.
- Bytes nuevos: **[0x10000, 0xf9080)**; sectores afectados:
  **[0x10000, 0xfa000)**, todos dentro de factory.
- **21:12:06.719:** lectura posterior de **toda factory (4 MiB)**. Su prefijo de
  954.496 bytes coincide por bytes y SHA-256 con el candidato. Padding del último
  sector verificado a FF; resto después de los sectores afectados idéntico al
  backup anterior. Hash de factory completa posterior:
  `543f2f4f7b7d0ada49213d7519bcc15e0065b1d17006d31fa488457de401072c`.
- **Antes de arrancar**, comparación byte a byte confirma que prefijo protegido
  (bootloader/tabla/Wi-Fi/helper) y device-auth siguen idénticos. Esto prueba
  conservación durante el flasheo; el arranque puede realizar operaciones NVS
  habituales, que no se han intervenido.
- **21:12:06.724:** reinicio mediante hard reset. No otro dispositivo flasheado.

## Arranque y feed

Observación serie acotada; solo lista permitida de resúmenes, sin guardar raw:

- **21:12:07.329:** firmware prototype-0.1.0, flash 32 MiB, panel enabled;
  GPIO0 HIGH. Ese texto genérico no identifica una release: el SHA verificado sí.
- OFFLINE inicial durante conexión, resuelto sin intervención.
- **21:12:14.441:** Feed OK, **4 paradas / 18 llegadas / 15 realtime**.
- **21:12:47.620:** segundo Feed OK, **4 / 18 / 15**.

No fallos de ejecución, inicialización o parsing observados; renderizado visual
no verificable por estos contadores. No se capturó una línea independiente
Wi-Fi connected; los dos feeds correctos corroboran conectividad operativa,
identidad y autenticación funcionales. Monitor cerrado, A queda funcionando.

GET autenticado antes/después: HTTP 200, Feed v1, private/no-store,
**X-Configuration-Revision: 7**, misma selección por hash. Último GET a
**21:13:32 Madrid**: FGC tres salidas programadas, Bus diez realtime,
TRAM tres para L'Illa y dos para Glòries. Los recuentos pueden cambiar entre polls.
El firmware actual no registra la cabecera de revisión; revisión 7 se corrobora
mediante servidor/identidad conservada y las cuatro paradas serie, **sin afirmar
un acuse directo de revisión en el ESP32**. No se modificó código para añadirlo.

La revisión 7 contiene FGC, Bus, L'Illa y Glòries; Metro no está seleccionado y
no puede declararse visualmente validado en este arranque. Sus regresiones
nativas pasan; no se alteró la configuración para añadirlo.

**Siguiente paso:** usuario comprueba en el LED FGC/Bus, T provisional TRAM,
destinos/tiempos de L'Illa y Glòries y paginación de las cuatro selecciones.
No se considera aceptada la apariencia a partir del build o del feed correcto.

## Rollback de aplicación

No ejecutado. Antes de usarlo, volver a identificar A, comprobar tabla y hash
`ee8578…` de la copia completa. Conservar NVS/identidad; no usar erase_flash ni
una subida PlatformIO que incluya bootloader/tabla. Con esptool 4.9.0, escribir
**solo application-before-full.bin en 0x10000**, con flash_mode/freq/size keep;
los 4 MiB terminan exactamente en 0x410000, antes de device-auth. Releer factory
completa y exigir el SHA anterior; comparar de nuevo regiones protegidas antes
de reiniciar. Verificar dos feeds autenticados tras el reinicio. No cambiar
configuración de backend: el parser genérico previo acepta modo/operator
adicionales, aunque TRAM volvería a su símbolo genérico hasta restaurar el nuevo
firmware. Cualquier otra escritura requiere revisión/autorización aparte.
