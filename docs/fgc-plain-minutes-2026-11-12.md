# FGC: minutos sin indicador S — 12/11/2026

**Implementado y validado localmente; firmware hub75-live preparado. No flasheado ni validado físicamente.** El usuario solicita ocultar la distinción visual entre salidas FGC programadas y realtime, conservando su procedencia interna y el resto del comportamiento.

## Cambio mínimo

La S se añadía en `state.cpp::countdown` y en el desbordamiento `renderer.cpp::arrivalText`. La procedencia se parsea antes, en `feed_parser.cpp`, y no se modifica. Ahora ambos formateadores aceptan un parámetro de presentación que por defecto conserva el comportamiento anterior. El renderer lo desactiva únicamente para estaciones con `operatorId=FGC`.

FGC muestra `5'`, `12'`, etc. para Scheduled y Realtime, sin convertir datos programados en realtime. Se conservan los enums, identificadores, parser, estado y diagnósticos internos. Unknown mantiene su indicador anterior. Metro/Bus y otros operadores conservan sus valores por defecto. El ajuste también evita S en el indicador de desbordamiento. No hay cambios de paleta, tipografía, cuenta atrás, expiración, paginación, red, Wi-Fi, autenticación, NVS, particiones o protocolo.

El backend de tres salidas por estación/60 minutos y su filtro/deduplicación no cambia. No se consultó ni modificó producción durante este trabajo. Para ver el cambio hace falta actualizar la aplicación del dispositivo; cambiar el backend para ocultar la procedencia sería incorrecto.

## Validación

- `.venv/bin/pio test -e native`: **76/76** casos correctos (Wi-Fi, tipografía y núcleo).
- Nueva regresión: minutos FGC sin prefijo, mezcla scheduled/realtime, expiración y desbordamiento; la procedencia permanece Scheduled tras renderizar.
- Comparación de todos los píxeles: FGC Scheduled y Realtime producen exactamente el mismo framebuffer en **64×32 y 128×64**.
- Se mantienen las regresiones existentes de prefijos programados por defecto, renderer, frescura, estado y parser; Metro/Bus no cambia.
- Formato C++ y fixtures generados: comprobaciones correctas.
- `.venv/bin/pio run -e hub75-live`: compilación release correcta. RAM estática informada 47.012 B; programa 954.021 B. No son mediciones de memoria dinámica en hardware.

## Artefacto preparado

Aplicación: `artifacts/firmware/hub75-live-fgc-plain-minutes-2026-11-12/firmware.bin` (ignorado, permisos privados).

- Binario: **954.384 B**.
- SHA-256: **d5d906b98dadca489b34dfd57a5b764741f429ebc6135f932942c85d1713adbb**.
- Target normal `hub75-live`, aplicación en **0x10000**. Sin upload, erase, reset, monitor serie ni acceso al dispositivo.
- Tabla compilada idéntica a la anterior; `platformio.ini`, `partitions.csv` y `include/app_config.h` conservan sus hashes.
- Binario local anterior preservado aparte; su hash coincide con el artefacto documentado del flash del 11/11 (`0d261675...`). No se leyó ni verificó de nuevo el contenido actualmente instalado.
- No se crearon credenciales ni se editaron secretos. Los binarios permanecen fuera de Git.

[Hashes y evidencia segura](evidence/fgc-plain-minutes-preparation-2026-11-12.json). Fuente base `4cda409f3d11bb11605b894da4dee5d84476522b` más los cinco archivos de código/pruebas modificados, sin staging/commit/push. El tracking central está actualizado en el repositorio hermano.

Siguiente acción: autorización separada para cargar la aplicación y observación física de FGC sin S, verificando también Metro/Bus. Esta preparación no establece recepción del feed, exactitud de horarios ni funcionamiento físico del nuevo binario.
