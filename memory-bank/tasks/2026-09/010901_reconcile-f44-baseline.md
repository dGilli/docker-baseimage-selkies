# 010901_reconcile-f44-baseline

## Objective
Reconcile the working NRP MVP (`rhel9-dev`, built on the upstream **master** baseline `69f4fc9`) with the fork's upstream so future maintenance is cheap — with the **smallest possible delta** to `upstream/fedora44`, per user direction 2026-09-01 ("RHEL should be very similar to fedora44… reconcile… with the smallest delta possible").

## User questions answered
- **"Is upstream/fedora44 the closest to what we want?"** → HALF right (git-verified): f44 is the correct **maintenance reference** (el9 = deprecated/dead branch, CI deleted 2026-05; `Dockerfile.rhel9` explicitly models f44's Dockerfile; our shared-tree edits were designed as no-ops on Fedora). But rhel9-dev's **current tree** was closest to **master** (it *is* master + additions: 0 deletions, 7 modified files, ~95 added). Raw deltas: vs master 103 files/4025+/27- | vs f44 123 files/7321+/1576- | vs el9 137 files/9853+/1413-.
- **History shape (two mid-build instructions):** do NOT pull rhel9-dev's messy 35-commit history into `rhel9`; but do NOT squash into one blob either → **curated milestone series** (6 work commits + 1 tooling commit), each re-using the original milestone commit messages.

## Outcome
- ✅ `rhel9` = `upstream/fedora44` @ `1c2870d` + curated series, tip `85c605c` (after the F67 fix)
- ✅ 17-row merge resolved; tip tree == fully-resolved merge tree (verified identical tree object `0c2244f`)
- ✅ Delta budget: **5 modified upstream files** (allowlisted) + 96 additive + 0 generated-file touches + 0 mode changes + 0 upstream drift
- ✅ Local boot matrix ALL PASS (post-fix): GNOME / openbox+1280x720 / RESTART_APP / hardening
- ✅ NRP smoke PASS: CPU (exp-19-11.sdsc) + GPU (k8s-haosu-15.sdsc, RTX 2080 Ti 595.91.07, **NVENC 13.0** init+bound+stream)
- ✅ PR #2 merged (ff), CI gate `delta budget + shell syntax + hadolint` green
- ✅ Branch protection: rhel9 (required check + PR gate 0 + no force-push + no delete), rhel9-dev (no force-push + no delete)
- ✅ Fork tooling committed: `scripts/delta-allowlist.txt`, `scripts/upstream-delta.sh`, `.github/workflows/fork-maintenance.yml`
- ⏳ Pending user: milestone tag ID (project rule: user-assigned) + production pin bump decision (`v4-llvmpipe` still = c8)

## The curated series (rhel9, top-down)
| Commit | Milestone | Source (rhel9-dev SHAs) |
|---|---|---|
| `85c605c` | **svc-dbus regression fix** (F67) | 6 files from `pre-rhel9-reconcile` verbatim |
| `0cc8477` | fork maintenance tooling | new (Part B) |
| `1959935` | memory-bank journal | `7015647` |
| `4de4f52` | GPU M1/M2 deploy path | `d75d14e..0f44871` |
| `f4974ff` | phase 1.5 NRP deploy | `b97a611..525f38d` |
| `2e5e95b` | R1 proot-apps + bwrap stub | `cc2c2b7` + `153290d` |
| `5d485ed` | GNOME task 2 + wallpaper | `11a8afd` + `06bc207` |
| `9ecf464` | phase-1 image (PLAN v4) | `bd46cdb` |

## 17-row resolution table (f44 baseline = "ours", rhel9-dev = "theirs")
| Path | Resolution | Rationale |
|---|---|---|
| Dockerfile, Dockerfile.aarch64, Jenkinsfile, README.md, jenkins-vars.yml, package_versions.txt, startwm_wayland.sh, init-video/run | f44 verbatim | baseline-identity files; ours is built by `Dockerfile.rhel9` |
| **svc-de/run** | f44 verbatim (wait-for-X loop removed) | verified safe — F72(a): 1280x720 applied, no flap |
| **svc-selkies/run** | f44 verbatim — our DEV_MODE gate **DROPPED** | f44's DEV_MODE is already dnf-based (PLAN v4 B.2 upgrade path realized; CS9-verified) |
| **readme-vars.yml** | f44 verbatim — our RHEL row **N/A** | f44's is an 8-line minimal file, no distro table — F71 |
| **pixman-patch/** | deleted (follow f44) | zero references in our tree |
| **svc-dbus/** (6 files) | f44 deletion adopted → **REVERTED** (F67) | boot matrix: fatal for GNOME 40 power indicator |
| startwm.sh | f44 + our GNOME branch; else = f44 bare `openbox-session` | F72(b): RHEL9 openbox-session doesn't wrap dbus |
| init-nginx/run | OURS (branch subsumes f44 hard-swap) | one-tree design (PLAN v4 B.1) |
| init-selkies-config/run | f44 + our 2 hunks (.cache chown F49, proot guard) | independent hunks |
| svc-docker/run | f44 + our dockerd guard | independent hunks |
| selkies-proot | f44 + our 12-line bwrap stub (`/etc/redhat-release`-guarded) | F54 fix, no-op on f44 |

## Key findings (F67–F73, full text in findings.md)
- **F67** f44's svc-dbus deletion breaks GNOME 40 (power indicator system-bus probe → fatal JS exception → crash loop) — 6 files restored verbatim (additive vs f44; [modified] budget unchanged)
- **F68** f44's `GBM_BACKEND=nvidia-drm` + debian-path `GBM_BACKENDS_PATH` — path is an upstream bug; export proven inert on our X11/CUDA path (NVENC OK)
- **F69** rulesets API 500s on personal repos → legacy branch protection (auto-disables force-push/deletions)
- **F70** gh mis-resolves fork repo to parent (needs `--repo`); no `gh pr merge --ff` → local ff + push
- **F71** f44 readme-vars.yml minimal → RHEL-row hunk obsolete (permanent cosmetic divergence)
- **F72** f44's svc-de wait-loop removal + openbox-without-session-bus both verified safe on RHEL9
- **F73** untracked memory-bank hard-blocks merges + phantom remote-tracking refs mislead (process codified)

## Files Modified (vs upstream/fedora44)
**Modified (5 — the whole delta budget):** `root/defaults/startwm.sh` (+50 GNOME branch), `root/etc/s6-overlay/s6-rc.d/init-nginx/run` (+5 branch), `root/etc/s6-overlay/s6-rc.d/init-selkies-config/run` (+10: chown + proot guard), `root/etc/s6-overlay/s6-rc.d/svc-docker/run` (+4 guard), `root/selkies-proot` (+16 stub)
**Additive (96):** `Dockerfile.rhel9`, `root-base/` (68), `deploy/` (4), `package_versions_rhel9.txt`, `memory-bank/` (24), `root/usr/share/backgrounds/slu-rhel.jpg`, `scripts/` (2), `.github/workflows/fork-maintenance.yml`
**Restored-as-additive (6):** `svc-dbus/*` + `user/contents.d/svc-dbus` + `svc-selkies/dependencies.d/svc-dbus` (F67)

## Patterns Applied
- `systemPatterns.md` — variant model (one baseline + guarded no-op hunks), PLAN v4 "f44 with EL9 substitutions"
- New: `projectRules.md#Fork-Maintenance` (delta budget, guard-predicate rule, sync procedure, history model)
- New: ADR `decisions.md#2026-09-01` (baseline selection + curated re-land + delta model)

## Integration Points
- `scripts/upstream-delta.sh` gates CI on `rhel9` (branch protection required check)
- `deploy/nrp/apply-nrp-e2e.sh --image <tag>` drives all NRP verification (recon deploys torn down post-smoke)
- Sync path: `git merge upstream/fedora44` — conflicts now limited to the 5 allowlisted files

## Artifacts
- PR: https://github.com/dGilli/docker-baseimage-selkies/pull/2 (merged, ff)
- Image: `docker.io/dgilli/selkies-rhel9:c9` = `a4e303101691` (built from `rhel9@85c605c`; `:latest` = c9; production pin `v4-llvmpipe` UNCHANGED = c8)
- Revert anchors: `pre-rhel9-reconcile` (MVP @ `7015647`), `pre-rhel9-reconcile-build` (`0cc8477`), reset-to-`upstream/fedora44` (any time)
- Reference: `memory-bank/tasks/2026-09/reconcile-delta-reference.patch` (re-derivation checklist for future syncs)
