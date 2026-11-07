# Local FGC physical-display preparation — 7 November 2026

Local implementation/build preparation only. **Not flashed or physically validated.** Backend code, deployment, device configuration and browser simulator are unchanged. Central tracking remains in the sibling feed repository.

## Minimal architecture change

Feed v1's required fields/version are unchanged. Firmware accepts an optional string `operator` on a stop, stored as `operatorId`. It is an experimental additive presentation extension used by the local fixture; older clients ignore it, and the current backend does not emit it. Missing values preserve existing M/B/? behavior. Unknown operators preserve that same fallback; malformed supplied types/oversized strings reject the feed transactionally using existing text bounds.

The renderer looks up a bounded compiled symbol registry by operator ID. It does not interpret FGC routes, station IDs, API formats or line-colour rules. The registry's FGC entry copies the already-reviewed 7×5 simulator mask (rows 22,41,85,74,52), white on #A1D341, centered in the existing 7×7 Metro operator slot at (1,1). Line colours/foregrounds continue to come from ordinary `lineColor` / `lineTextColor`. No font/accent changes.

## Isolated offline test

`hub75-fgc-fixture` extends the existing ESP32-S3 board/panel configuration and selects only `display_driver.cpp` and `fgc_fixture_main.cpp` as application sources. It uses the regular parser, state/freshness logic, pagination, countdowns and renderer. It never includes `app_config.h`/`secrets.h`, starts Wi-Fi/SNTP, reads device identity, runs provisioning or calls persistent-storage APIs. The pinned Arduino framework startup is unchanged: it initializes default NVS and retains its existing recovery behavior for broken/incompatible NVS. This is not a byte-for-byte flash-persistence guarantee; no new erase/reset path is introduced. Brightness is fixed at 4/255. Panel pins/timing and the partition CSV are unchanged; this project currently has a factory application and separate identity NVS, **no OTA slots**.

`fixtures/fgc.json` is explicitly synthetic, not live FGC data. Six PC pages use the selected palette: L6 #867EC4/white, L7 #AE621C/white, L8 #E27AB4/white, L12 #B6B3E1/black, S1 #DC6F2D/white, S2 #93BC44/white. PC displays PL. CATALUNYA; two PADUA L7 groups exercise distinct destinations. Minute predictions carry unknown provenance (`?`), not realtime. The normal 120-second stale policy suppresses them. No colour comparison or new visual experiment is included.

Controls at 115200 baud: `n` next page, `r` restart the synthetic replay, `i` repeat the safe report. Otherwise pages rotate normally. Reset restarts replay. The known 59-pixel PL. CATALUNYA label still uses existing scrolling in the 53-pixel native header; there is no claim that it fits without scrolling.

## Build and checks (no hardware operations)

From this firmware repository, with the existing virtual environment activated:

```sh
python tools/generate_fixtures.py --check
python tools/format.py --check
pio test -e native
python tools/test_device_identity.py
pio run -e hub75-fgc-fixture -e hub75-live -e hub75-fixture
```

`make check` also includes the new environment alongside all existing environments. No command above uploads or connects to the ESP32. Build artifacts remain under ignored `.pio/build/`.

## Simplest controlled test on the ESP32-S3 (manual only)

The following procedure is prepared for the user to execute later; it was **not executed** during implementation. Use the established Waveshare controller/panel/power setup. Close any serial monitor first. Keep identity/Wi-Fi NVS and partition layout intact; no erase, identity provisioning, partition migration or full-flash image is required.

1. Identify the currently connected ESP32-S3 port with `.venv/bin/pio device list`, then set `FGC_TEST_PORT` to that actual port. Do not reuse an old port blindly. Retain a known-good live application for restoration, or build `hub75-live` with the existing unchanged local production settings. The existing private recovery baseline/procedure remains applicable; see the sibling [controlled application-only update](../../live-transit-feed/docs/preflash-verification-2026-10-31.md).
2. Build the diagnostic above. Verify the installed partition table against this unchanged build **before writing**. These reads may reset the board into its loader but do not write flash. Read only the table, never the identity partition:

```sh
mkdir -p artifacts/fgc-test
FGC_ESPTOOL="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"
.venv/bin/python "$FGC_ESPTOOL" --chip esp32s3 --port "$FGC_TEST_PORT" \
  --after no_reset read_flash 0x8000 0x1000 artifacts/fgc-test/installed-partitions.bin
.venv/bin/python - <<'PY'
from pathlib import Path
build = Path('.pio/build/hub75-fgc-fixture')
expected = (build / 'partitions.bin').read_bytes()
actual = Path('artifacts/fgc-test/installed-partitions.bin').read_bytes()
image = (build / 'firmware.bin').read_bytes()
assert len(actual) == 0x1000 and expected and actual[:len(expected)] == expected, 'Partition mismatch: STOP'
assert 0 < len(image) <= 0x400000, 'Application too large: STOP'
assert 0x10000 + ((len(image) + 4095) // 4096) * 4096 <= 0x410000, 'Identity boundary: STOP'
print('Table matches; application fits before device-auth. No write performed.')
PY
```

3. **Only when choosing to flash the diagnostic**, write the application alone at 0x10000, retaining flash settings. Do not substitute the ordinary multi-image upload command, rewrite partitions/bootloader/identity, or add erase options:

```sh
.venv/bin/python "$FGC_ESPTOOL" --chip esp32s3 --port "$FGC_TEST_PORT" --baud 460800 \
  --after hard_reset write_flash --flash_mode keep --flash_freq keep --flash_size keep \
  0x10000 .pio/build/hub75-fgc-fixture/firmware.bin
.venv/bin/pio device monitor --port "$FGC_TEST_PORT" --baud 115200
```

Confirm successful write verification. On boot, expect the synthetic/offline report, brightness 4/255, FGC symbol, existing paging and selected line data. Check six PC and two PADUA pages, the short-header reveal and separate L7 destinations. Wait at least 120 seconds and confirm STALE/no countdowns. `r` restarts the fixture. This validates local rendering/state behavior, not native API accuracy or production connectivity.

4. Close the monitor. Restore the known-good live application using the same application-only command with its `firmware.bin` path after confirming its size/layout. When using the rebuilt normal environment, that path is `.pio/build/hub75-live/firmware.bin`. Do not erase/reprovision either NVS partition. Restart and verify existing Wi-Fi/device-feed operation resumes. The diagnostic application does not modify stored credentials/configuration; actual persistence/recovery is a physical observation to record, not a build result.

If the table, chip, size, write verification or display initialization differs from expectations, stop. Record only safe observations and photos. No physical success, colour acceptance, live arrival accuracy or device recovery has yet been established by this work.

## Recorded preparation result

Final `make check`: formatting and generated-fixture checks passed; 74/74 native cases, 2/2 identity-image tests and 9/9 build environments passed. FGC/live compiled partition tables match, and the partition CSV is unchanged from HEAD. The application fits before device-auth. [Artifact size/hash and scope](evidence/fgc-display-preparation-2026-11-07.json). No device was accessed or flashed.
