# Memory Bank — Table of Contents

**Project**: `slu-docker-rhel-selkies` (SLU fork of linuxserver/docker-baseimage-selkies)
**MB Version**: 2026-08 | **Last Updated**: 2026-09-08

## GH Tracking (issues + project board)

All milestones and roadmap items are GitHub issues on `dGilli/docker-baseimage-selkies`, kept current on the **project board** `@dGilli's untitled project` (https://github.com/users/dGilli/projects/1, ID `PVT_kwHOAOLkSs4Bi2dv`). **Keep the board in sync as work moves** — columns Done / In Progress / Todo; kind labels `enhancement` / `documentation` / `process` / `bug` / `milestone`.

> `gh` needs scopes `repo` + `project`/`read:project`. Board ops: list `gh project item-list 1 --owner dGilli` · add `gh project item-add 1 --owner dGilli --url <issue-url>` · set Status `gh project item-edit --id <PVTI_…> --project-id PVT_kwHOAOLkSs4Bi2dv --field-id PVTSSF_lAHOAOLkSs4Bi2dvzhhs_Qo --single-select-option-id <opt>` (Todo `f75ad846`, In Progress `47fc9ee4`, Done `98236657`).

| # | state | kind | area / what | MB reference |
|---|-------|------|-------------|--------------|
| 4 | open · Done | enhancement | GPU desktop rendering (M3) | `activeContext.md` M3 task; F75 (fakevt shim); branch `feat/m3-gpu-xorg-ddx` |
| 5 | open · Todo | enhancement | CLI/UX workstation lifecycle | `productContext.md` roadmap §2 |
| 6 | open · Todo | bug | selkies menu/app installer fix | `productContext.md` roadmap §3; R1 (F53–F56) |
| 7 | open · Todo | process | fork maintenance workflow | `productContext.md` roadmap §4; `decisions.md` baseline ADR |
| 8 | open · Todo | enhancement | SLU image registry + release | `productContext.md` roadmap §5; F30/F66; `build-deployment.md` |
| 9 | open · Todo | documentation | docs expansion | `productContext.md` roadmap §6 |
| 10 | open · Todo | enhancement | R1 step 3 SLU app catalog | `progress.md` (R1 step 3 pending) |
| 11 | open · Todo | enhancement | aarch64 RHEL9 variant | `projectbrief.md` (x86_64 first, aarch64 later) |
| 12 | open · Todo | enhancement | Wayland (pixelflux Smithay + DMA-BUF→NVENC) | `progress.md` (Phase 2 candidates) |
| 13 | closed · Done | milestone | RHEL9 base image (Phase 1) | `tasks/2026-08/270827_rhel9-build.md` |
| 14 | closed · Done | milestone | GNOME default X11 DE (Task 2) | `tasks/2026-08/280828_rhel9-gnome-desktop.md` |
| 15 | closed · Done | milestone | NRP production deploy path (Phase 1.5) | `tasks/2026-08/280828_phase1-5-production-nrp.md` |
| 16 | closed · Done | milestone | R1 proot-apps (steps 1–2) | `tasks/2026-08/280828_r1-proot-apps.md` |
| 17 | closed · Done | milestone | GPU M0 (NVENC probe) | findings F58 |
| 18 | closed · Done | milestone | GPU M1 (`--gpu` deploy path) | findings F58–F64 |
| 19 | closed · Done | milestone | GPU M2 (live verify + monitoring) | findings F59–F66 |
| 20 | closed · Done | milestone | fedora44 reconcile + tooling | `tasks/2026-09/010901_reconcile-f44-baseline.md` |
| 21 | closed · Done | milestone | F67 svc-dbus regression fix | findings F67 |

## Core Files
| File | Purpose | Load When |
|------|---------|-----------|
| [projectbrief.md](./projectbrief.md) | Vision, goals, fork lineage | Complex tasks, orientation |
| [productContext.md](./productContext.md) | User goals, support scope, upstream, future roadmap | Complex tasks, planning |
| [systemPatterns.md](./systemPatterns.md) | Architecture: build stages, s6 services, dual DE mode | Before arch changes |
| [techContext.md](./techContext.md) | Stack: base images, services, pinned versions | Session start |
| [activeContext.md](./activeContext.md) | Current sprint: RHEL9 support | Every session |
| [progress.md](./progress.md) | Status, blockers, priorities | Session start |

## Reference Files
| File | Purpose | Load When |
|------|---------|-----------|
| [projectRules.md](./projectRules.md) | Coding standards, generated-file rules | When uncertain |
| [decisions.md](./decisions.md) | ADRs | Arch decisions |
| [findings.md](./findings.md) | Cross-cutting findings registry (F01–F66, evidence-linked) | RHEL9 work, debugging, GPU/NRP |
| [quick-start.md](./quick-start.md) | Common commands, build/run/test | Fast track |
| [build-deployment.md](./build-deployment.md) | Build/deploy/Jenkins flow | Build work |
| [testing-patterns.md](./testing-patterns.md) | QA strategy, CI env vars | Test work |

## Tasks
| Path | Purpose |
|------|---------|
| [tasks/2026-08/README.md](./tasks/2026-08/README.md) | Monthly summary |
| [tasks/2026-09/README.md](./tasks/2026-09/README.md) | Monthly summary |
| [tasks/2026-09/010901_reconcile-f44-baseline.md](./tasks/2026-09/010901_reconcile-f44-baseline.md) | Reconcile rhel9-dev MVP onto upstream/fedora44: curated series, 17-row resolution table, F67 fix, delta budget, sync workflow (F67–F73) |
| [tasks/2026-09/reconcile-delta-reference.patch](./tasks/2026-09/reconcile-delta-reference.patch) | Re-derivation checklist: our shared-file modifications vs the old master baseline (sync-time reference) |
| [tasks/2026-08/270827_rhel9-vetting-plan-v4.md](./tasks/2026-08/270827_rhel9-vetting-plan-v4.md) | PLAN v3 vetting evidence + defect log (D1–D6) + PLAN v4 delta |
| [tasks/2026-08/270827_rhel9-build.md](./tasks/2026-08/270827_rhel9-build.md) | Phase-1 build log: 5 cycles (F31–F35/F41), test evidence, artifacts |
| [tasks/2026-08/280828_rhel9-gnome-desktop.md](./tasks/2026-08/280828_rhel9-gnome-desktop.md) | Task-2 build log: GNOME desktop (gnome-shell 40.10), 7 cycles (F42–F52, incl. SLU wallpaper), edge 4/4, artifacts |
| [tasks/2026-08/280828_phase1-5-nrp-dev-push.md](./tasks/2026-08/280828_phase1-5-nrp-dev-push.md) | Phase-1.5 dev: Docker Hub push (dgilli/selkies-rhel9:latest), pull-by-digest verify, NRP k8s mapping (deploy/), F28/F30/F55 closures |
| [tasks/2026-08/280828_r1-proot-apps.md](./tasks/2026-08/280828_r1-proot-apps.md) | R1 (steps 1–2): proot-apps 0.3.2 in image + bwrap stub guard — dashboard install/run verified (FileZilla 3.68.1), F53/F56 |
| [tasks/2026-08/280828_phase1-5-production-nrp.md](./tasks/2026-08/280828_phase1-5-production-nrp.md) | Phase-1.5 production artifact: v4-llvmpipe pin (private), drop-in NRP template deploy/nrp/ (F57 mismatches fixed, render verified), NRP-side E2E checklist |

## Operational
| Path | Purpose |
|------|---------|
| [ops-log.jsonl](./ops-log.jsonl) | Append-only session/state JSONL log |
