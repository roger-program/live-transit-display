# Metro Display development instructions

This repository contains Metro Display firmware, hardware integration and device-specific technical documentation. The sibling `../live-transit-feed` repository contains the backend and the single central product tracking record.

## Project tracking

- Keep `PROJECT_STATUS.md`, `ROADMAP.md`, `DECISIONS.md` and `CHANGELOG.md` exclusively in `../live-transit-feed`; do not create parallel copies here.
- At the start of meaningful work, consult the central `PROJECT_STATUS.md` and relevant decisions when accessible. Follow this repository's README and relevant `docs/` procedures; inspect actual code and evidence when needed. Documentation does not override contradictory implementation, deployment or hardware evidence.
- After meaningful firmware or hardware work, update the affected central documents: what changed, what was tested and observed, unresolved issues and next action. Add only significant milestones to the changelog; change roadmap/decisions only when warranted. Keep detailed methods and device evidence in this repository and link them from the central record.
- Distinguish proposals, local implementation, automated tests/builds, flashing, backend deployment and physical validation. A build or successful flash is not physical functional validation. Attribute user-reported observations and record dates/scope; do not promote discussion into confirmed decisions.
- Check sibling access before planning tracking updates. If it is missing, unreadable or outside the writable workspace, state the limitation and report the exact pending central updates in the final handoff. If local documentation is writable, leave a concise pending-tracking note in the relevant technical work record, linking evidence; reconcile it when central access returns. Do not silently skip tracking or duplicate the four documents.
- Never expose or commit credentials, private identity images or credential-bearing URLs. Preserve existing device-specific safety, backup and authorization procedures.
