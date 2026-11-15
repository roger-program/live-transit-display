# X por llegada programada — validación local, 15 noviembre 2026

**Implementado y probado localmente, no flasheado.** El usuario sustituyó el criterio
anterior de estado partial antes de cualquier flasheo. No se abrió el puerto serie,
leyó/escribió flash ni reinició el dispositivo en esta sesión. La enumeración de
puertos de la preparación anterior no alteró el hardware. Sin despliegue Worker,
operaciones D1, consultas HTTP ni cambios de selecciones/credenciales/Wi-Fi.

## Contrato y criterio definitivo

Feed v1 incluye `arrivals[].provenance` para **cada llegada**: realtime, scheduled
u unknown. El parser conserva el valor; una captura antigua sin provenance queda
Unknown y un enum inválido es rechazado. El estado de la parada (`partial`, `ok`,
`scheduled`, etc.) y realtimeStatus son metadatos globales y no permiten atribuir
la procedencia de cada tiempo. No se cambia Feed v1 ni su parser/modelo.

- **Realtime válido:** minutos sin X, también cuando stop.status es partial.
- **Scheduled:** X ámbar **delante de ese tiempo**, con un píxel de separación.
- **Fila mixta:** cada horario programado lleva su propia X; los realtime vecinos
  quedan sin ella. Una marca en la cabecera sería ambigua y se retira.
- **Unknown o procedencia ausente:** conserva el prefijo `?` existente; no se
  convierte en realtime ni en scheduled y no recibe la X.
- **Caducados/no disponibles:** se mantiene viewAt/caducidad y exclusión de grupos;
  nunca se cambia provenance para hacer visible una llegada caducada.

Mismo criterio para Metro, Bus, FGC y TRAM: se elimina del renderer la condición
FGC que ocultaba la procedencia. El glifo S tipográfico ya no se usa para dibujar
horarios; su distinción visual es la X de píxeles para todos los operadores.
Las funciones de estado/adquisición/formateo diagnóstico ajenas al renderer no
cambian.

## Cambio acotado

Solo renderer.cpp/.h y pruebas del renderer. El layout calcula los tiempos visibles
junto con el espacio de su X antes de seleccionar hasta tres; una marca y su
valor caben juntos o se omiten juntos. Mantiene la alineación derecha, separación
entre tiempos realtime, overflow y clipping durante las transiciones. Se reutiliza
la misma selección en la proyección de texto usada por las pruebas para evitar
una segunda lógica de ajuste.

X directa de 3×3: filas `#.#`, `.#.`, `#.#`, cinco píxeles **#ffaa00** y cuatro
posiciones vacías. Se centra verticalmente en la fila de texto de cinco píxeles
(y+1; fila principal y13–15); sin fuente C2. El hueco reservado al antiguo aviso
de cobertura en la cabecera desaparece. No cambia la tipografía de destinos,
cuenta atrás, símbolos, adquisición, calendario ni datos de backend.

## Pruebas y build

**78 pruebas nativas pasan**, incluidas:

- Patrón/color/posición exactos, cinco píxeles por marca y cuatro posiciones negras,
  tanto en 64×32 como 128×64 y para los cuatro modos/operadores.
- Realtime idéntico para estados ok/partial/scheduled; ninguna X por inferencia del
  estado global. Filas programadas y mixtas con marcación individual.
- Unknown/ausente conserva su identidad; parser separa realtime/scheduled dentro
  del mismo grupo, incluso con estado global scheduled.
- Ajuste de anchura incluye la marca, evita recorte, excluye una llegada expirada
  y conserva el indicador numérico de overflow.
- Stale/unavailable mantienen supresión de grupos; regresiones existentes pasan.

Dos referencias de raster cambian por los efectos solicitados: retirada del aviso
de cobertura y nuevo dibujo de horarios. El resto de referencias se conserva.
clang-format y git diff --check correctos; **hub75-live compila**.

Candidato `.bin`: **955.184 bytes**, SHA-256
`72f630673d49bb8e18c15a5a65bc9c212380bdac77e5701a35c189ed23c64179`.
Copia de revisión en
`artifacts/firmware/hub75-live-scheduled-x-2026-11-15/firmware.bin`;
[hashes y alcance](evidence/scheduled-arrival-x-2026-11-15.json).
No es el firmware instalado. Última instalación verificada previamente: TRAM
`b0cdb117…` ([flasheo anterior](tram-flash-2026-11-15.md)); no se ha vuelto a leer.

## Vista previa y siguiente paso

[Raster del renderer real](evidence/scheduled-arrival-x-preview-2026-11-15.png),
ampliado con vecino más próximo. Arriba realtime con estado partial; centro todos
programados con identidad FGC; abajo fila mixta, solo **7 minutos** es programado.
Son datos sintéticos y una revisión de representación, no observación de la matriz.

**Esperar confirmación del usuario sobre el criterio y la vista previa antes de
cualquier nuevo flasheo.** No se considera aceptada la apariencia física. Si se
autoriza después, reidentificar A, leer tabla y aplicación instalada, hacer copia
verificable y escribir solo app en 0x10000, con lectura posterior y comparación
de regiones protegidas. Los respaldos del flasheo anterior permanecen privados;
no hay motivo de rollback físico ahora porque no se ha escrito nada.
