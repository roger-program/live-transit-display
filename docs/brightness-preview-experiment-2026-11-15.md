# Phase 1 brightness preview — local firmware work

2026-11-15, Europe/Madrid. User authorized local implementation/testing only. No hardware access, serial session, application flash, deployment or production configuration change. No schedule, timezone or persistent brightness settings.

## Source changes

- `lib/transit/src/brightness_preview.h/.cpp`: bounded JSON parser and pure RAM lease/generation state. Raw ceiling 16, lease <=15 s; repeated responses never renew, elapsed requests cannot extend expiry, canceled/expired/interrupted commands cannot replay. Fresh instance starts at normal baseline.
- `src/preview_client.h/.cpp`: independent bounded network task (12 KiB stack), random boot ID, 2 s idle / 1 s active polls after request completion, 2 s HTTP timeout, failure backoff/jitter/Retry-After. Network failure, setup/invalid clock or Wi-Fi loss disables preview. Mutex protects shared state; only render loop chooses/applies brightness. ACK after presentation includes exact generation/raw, monotonic/UTC timings and observed request/D1 counters. No ongoing idle D1 presence writes.
- `src/feed_client.h/.cpp`: reuse read-only identity/verified origin/CA/bearer behavior for fixed control paths. Control responses capped 1024 bytes, redirects prohibited, no SDK exceptions/credentials logged. Existing arrival fetch behavior remains unchanged.
- `src/main.cpp`: start independent preview task after existing setup; choose temporary brightness over current serial/compiled baseline and publish acknowledgement after `display.present`. No renderer/provenance/operator changes. Raw zero leaves normal CPU/network/frame loop running. Provisioning takes precedence and restores baseline.
- `test/test_core/test_main.cpp`: native parser/lease/zero/delay/supersession/disconnect/reboot regression cases.

Identity, Wi-Fi provisioning implementation, NVS records, partition layout, bootloader and transport parser/rendering are unchanged. Existing identity reader is reused, not rewritten; no new NVS write path. Serial baseline defaults to 4/255 and existing b4/b8/b16/b32 behavior survives.

## Local results

80 native tests passed; hub75-live built successfully (47,164 bytes static RAM, 964,301 program bytes). Formatting and whitespace checks pass. Static build data does not measure runtime task/TLS memory or panel output. Physical darkness, applied-light latency, reboot/expiry observations and transport continuity with parallel TLS remain pending.

## Integration, physical experiment and rollback

Central [experiment report](../../live-transit-feed/docs/brightness-preview-experiment-2026-11-15.md) contains API/security/metric semantics, local D1 measurements, ≥30 initial/100 subsequent physical sampling procedure, failure cases and separate deployment/flash authorization boundary. Browser/Worker/SNTP clock differences make cross-clock timing approximate; ACK proves driver application only, not photons. Normal baseline restoration is evaluated locally every frame even if network task blocks.

A later authorized rollout must retain current backup/identity/partition safeguards. Rollback uses previous application-only firmware and previous Worker version, after Stop/15-second expiry; no NVS erase, provisioning or partition change. Additive D1 experiment table can remain unused. Current source is **not flashed**; no hardware acceptance claimed.


## 2026-11-16 — Authorized application-only activation on A

User authorized focused production activation and application flashing, preserving all protected state and leaving slider available. Exact reviewed964,672-byte candidate **f3a28399b088ba90346a213b993c9faae6fb6433dc1b8439e66bbd4cb828aa9d** written only at0x10000 (00:06 Madrid). Fresh installed-image read matched both original4MiB backups; full post-write readback verifies candidate, FF padding and untouched tail. Protected prefix/identity byte-identical before00:07:11 restart. No NVS/provisioning/partition/bootloader change. Two four-stop Feed OK summaries and real baseline raw4 render acknowledgement observed; monitor closed, A running. No experimental brightness command was sent; visible output, recovery and latency/calibration acceptance remain user-pending.

Rollback: restore only full4MiB application backup at0x10000, SHA **98090fb3e7defabc78873c8109c91be01f26deb1d5be28560295826f8ad406cc**, verify protected regions before restart. Current backend7eb05a49… can revert to8fa8b1e6… while retaining additive0006. [Central execution, private backup location and rollback](../../live-transit-feed/docs/brightness-activation-2026-11-16.md). Earlier local-only sections retain their historical boundary; no source changes or rebuild in activation.
