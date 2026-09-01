# Active Context

**Last Updated**: 2026-09-01 | **State Machine**: `GPU M2 CLOSED / GPU UTILIZATION + SELF-SERVICE START DOCUMENTED` (**GPU M0 passed, M1 codified with `--gpu` + `DISABLE_ZINK=true` + Recreate strategy, M2 clean-path rerun PASSED; minimal GPU GNOME fix = deployment env `DISABLE_ZINK=true` only, no `MOZ_*`, no explicit Mesa/EGL/Vulkan pins, no `NVIDIA_DRIVER_CAPABILITIES` override, no fixed `SELKIES_MANUAL_*`. 2026-09-01: live GPU-utilization monitoring documented (F65), `nvidia-smi dmon` + selkies NVENC verified on the running pod, and `apply-nrp-e2e.sh` now falls back to the existing namespace pull secret when local container auth is unavailable (F66). Phase 1.5 production was previously complete end-to-end**: `v4-llvmpipe` pin (private) + drop-in template `deploy/nrp/selkies-rhel9.yaml.template` + one-shot `deploy/nrp/apply-nrp-e2e.sh` → live NRP cluster (ns `slu-researchtechnologies-dgilli`), **user manual verification PASSED** (GNOME + SLU wallpaper + H.264 + FileZilla R1); user tore down the pod, secrets remain documented. R1 steps 1–2: proot-apps shipped c8 `cc2c2b7`. Phase 1.5 dev: `docker.io/dgilli/selkies-rhel9:latest` + dev manifest; F28/F30/F55/F57 closed. Task 2: `11a8afd` + `06bc207`. Phase 1: `bd46cdb` → `23964ff` → `24e1575`. **GPU plan (2026-08-31, user-approved)**: M0–M2 next — free scheduling `nvidia.com/gpu: 1` (NO node targeting, NO A100 — A100 is platform-gated `nvidia.com/a100`, unreachable via generic resource), zero image changes (pixelflux 2.0.0 auto NVENC on L4 landings; A100 has no NVENC anyway), M1 = `apply-nrp-e2e.sh --gpu` + optional pixelflux==2.0.0 pin, M2 = live E2E. **M3 DEFERRED**, pattern fixed: port official selkies boot-time userspace driver install (F58 + ADR). Future roadmap tracked 2026-09-01: GPU desktop rendering, CLI/UX, selkies menu app installer, fork maintenance workflow, SLU image registry, docs)

## Task: Reconcile rhel9-dev MVP with upstream baseline (started 2026-09-01)

**State**: `PLAN` (analysis complete, awaiting user approval)

### Context
- Milestone 1 (GPU on NRP) closed. Goal: reconcile our working MVP (`rhel9-dev`, 1298cb8) with the upstream fork baseline, smallest delta possible.
- User already copied `upstream/fedora44` (1c2870d) → local branch `rhel9` (current). `rhel9-dev` = pre-reconciliation MVP, kept intact as revert reference.
- User assumption under test: "upstream fedora44 is the closest to what we want."

### Analysis result (git-verified 2026-09-01)
- **Assumption is HALF right**: fedora44 is the correct *maintenance reference* (el9 = deprecated/dead: `project_deprecation_status`, CI deleted 2026-05; our `Dockerfile.rhel9` header explicitly models f44's Dockerfile; our 5 shared-tree edits were designed as no-ops on Fedora). BUT rhel9-dev's *current tree* is master (69f4fc9) + purely additive: 0 deletions vs master, 7 files we modified + ~95 added. Raw deltas: vs master 103 files/4025+/27- | vs f44 123 files/7321+/1576- | vs el9 137 files/9853+/1413-.
- Dry-run merge `rhel9-dev` + `upstream/fedora44` (git merge-tree) = **17 conflicting paths**, all small/mechanical:
  - **take f44 verbatim (8)**: Dockerfile, Dockerfile.aarch64, Jenkinsfile, README.md, jenkins-vars.yml, package_versions.txt, startwm_wayland.sh, init-video/run
  - **union (1)**: readme-vars.yml (f44 + our RHEL row)
  - **f44 base + re-apply our hunks (6)**: startwm.sh (our +53-line GNOME branch), init-nginx/run (keep OUR branch — subsumes f44 hard-swap), init-selkies-config/run (f44 PIXELFLUX_WAYLAND fix + our .cache chown + proot guard), svc-docker/run (f44 whitespace + our dockerd guard), svc-selkies/run (DROP our non-Debian DEV_MODE gate — f44's DEV_MODE is already dnf-based, CS9-verified per PLAN v4 B.2), selkies-proot (f44 + our 12-line /etc/redhat-release-guarded bwrap stub, no-op on f44)
  - **judgment calls (2)**: (a) svc-dbus — f44 DELETED the system-bus service (4 files); recommend adopt deletion (GNOME path runs own session dbus-daemon per F51; openbox path = f44's CI-proven bare `openbox-session`; RHEL9 openbox-session verified NOT to wrap dbus); (b) f44 behavior changes to re-verify: `GBM_BACKEND=nvidia-drm` + `GBM_BACKENDS_PATH=/usr/lib/x86_64-linux-gnu/gbm` (DEBIAN path — upstream bug, wrong on EL; added in abd61f wayland work; keep for parity, report upstream) and svc-de/run **wait-for-X loop removed** (xrandr race risk at boot).
- f44 frontend stage drift (follow-up, separate task): f44 = alpine 3.23 + 3 dashboards (incl. `selkies-dashboard-zinc`); ours = 3.22 + 2. Changing it re-bakes verified image → do at next selkies bump.
- Branch CI identity (follow-up roadmap "fork maintenance workflow"): taking f44's jenkins-vars/Jenkinsfile makes branch identity `release_tag: fedora44` (inert in our fork — we build via podman on this host); adapting to rhel9 identity = separate task.

### Git Workflow Master verification (2026-09-01, plan v2 input)
- **`origin/rhel9` does NOT exist on the server** — local `origin/rhel9` (1298cb8) is a stale phantom remote-tracking ref. Reconcile push = plain branch creation, **no force-push anywhere**. Purge with `git fetch --prune` after.
- **Merge hard-blocker**: `memory-bank/` is untracked in the current checkout (tracked on rhel9-dev @1298cb8) and 2 files drifted from rhel9-dev (`activeContext.md`, `ops-log.jsonl` — session journal writes). A merge would abort ("untracked working tree files would be overwritten"). Resolution: commit the docs-only journal drift to rhel9-dev BEFORE merging ("untouched" = no feature/code work, not no log entries).
- **Clean**: 0 symlinks, no .gitattributes, identical modes on all 7 shared files (run scripts 100755 both sides); 0-byte s6-rc markers content-identical (no conflict).
- Audit argument for single merge commit: `git show <merge>` combined diff prints ONLY manual resolutions (the 8 take-verbatim files don't appear) — audit surface = exactly the judgment calls.
- Guard predicate rule: **`dnf` is NOT a distro discriminator** (present on f44 AND UBI9) — that's why the DEV_MODE gate can be dropped; `/etc/redhat-release` (absent on Fedora) is the correct RHEL predicate. Test every guard against trixie/f44/UBI9 fixtures.
- Rollback = reset to pre-tag, NEVER `git revert -m 1` on the reconcile (re-merge trap: rhel9-dev becomes an ancestor → later re-land silently no-ops).
- Dual-lineage trap: post-merge, `git merge upstream/master` will look mechanically clean (base 69f4fc9) — do NOT be tempted; it drags the Debian tree in. Audit by tree-diff vs upstream/fedora44 only.
- readme-vars.yml vs generated README.md divergence is permanent + cosmetic — never hand-edit README.md.
- QA item: grep build context to confirm nothing consumes f44's `package_versions.txt` at build/runtime (it becomes the Fedora list in our tree).
- Forward-looking: f44 ships `Dockerfile.aarch64`, we have no rhel9 counterpart (future additive file, not drift); f44 EOL ~May 2027 → quarterly review must watch for a fedora45 re-baseline decision (el9 deprecation precedent).

### Plan (pending approval)
1. Merge `rhel9-dev` → `rhel9` (f44 tip); resolve 17 conflicts per table above.
2. QA: local `podman build -f Dockerfile.rhel9` + boot matrix (services stable, GNOME, `DESKTOP=openbox` edge, manual resolution, RESTART_APP, hardening trio) — catches svc-de wait-loop removal + openbox-no-bus + GBM lines on CPU.
3. QA: NRP smoke — CPU deploy + GPU deploy (`--gpu`, DISABLE_ZINK=true) + NVENC confirm — catches GBM line on GPU nodes.
4. DOCS: task doc, progress, findings F67+, ADR (baseline=fedora44; el9 rejected dead; master rejected wrong-variant).
5. `rhel9-dev` untouched (revert point).

**Budget**: 3 cycles (1 merge-resolution + 2 build/verify) | ~45 min work + build times
**Risks**: xrandr boot race (svc-de) → boot matrix | GBM_BACKEND on llvmpipe-forced image → NRP GPU smoke | openbox session-bus removal → DESKTOP=openbox edge

## GPU M2 clean-path + utilization state (2026-08-31 → 2026-09-01)
### User directives
- M0–M2 approved; do **not** pin `pixelflux` unless required.
- R1 step 3 removed from active todos.
- Openbox is **not** a valid GPU alternative; the goal is to get **GNOME working on GPU nodes**.
- User approved continuing the post-live-verification troubleshooting/codification step on 2026-08-31.
- On 2026-09-01 the user asked for the best manual way to verify GPU utilization and for self-service commands to start GPU and non-GPU workstations.

### Completed
- **M0 passed**: free-scheduling `nvidia.com/gpu: 1` probe landed on UCSC GTX 1080 Ti / driver 580.159.04; `nvidia-smi`, `/proc/driver/nvidia/version`, HTTP 200, 0 restarts, and pixelflux NVENC all verified. Probe torn down.
- **M1 codified**: `deploy/nrp/apply-nrp-e2e.sh --gpu` now renders GPU resources, `DISABLE_ZINK=true`, and a `Recreate` deployment strategy. `--accept-nrp-utilization`, interactive NRP >40% gate, dry-run behavior, and rendered-manifest cross-checks are present. No pixelflux pin.
- **M2 live user verification passed (2026-08-31)**: after `DISABLE_ZINK=true` and removal of fixed `SELKIES_MANUAL_WIDTH/HEIGHT`, the user confirmed the desktop works like the previously good CPU state: resolution follows the browser, Firefox and Settings launch/display properly and are usable.
- **Minimal-env reduction completed**: the full debug env was reduced stepwise. The smallest verified GPU GNOME + Firefox + dynamic-resolution + NVENC state is only deployment env `DISABLE_ZINK=true`; image defaults provide `NVIDIA_DRIVER_CAPABILITIES=all`, `LIBGL_ALWAYS_SOFTWARE=1`, `GALLIUM_DRIVER=llvmpipe`, `MESA_GL_VERSION_OVERRIDE=4.5`, and `DISABLE_DRI3=true`.
- **Clean M2 rerun from committed deploy path passed**: deleted the e2e deployment/service/ingress, ran `./deploy/nrp/apply-nrp-e2e.sh --gpu --accept-nrp-utilization -n slu-researchtechnologies-dgilli`, and re-verified GNOME, Settings, Firefox, dynamic xrandr resize, and NVENC on the fresh pod.
- **GPU utilization monitoring documented and live-tested (2026-09-01)**: on running pod `slu-rhel9-e2e-545d4555d5-gq5zf` (`fiona-prg1.cesnet.cz`, RTX 2080 Ti), idle `nvidia-smi` showed `0%` GPU / `4 MiB` / no compute apps, which is expected. A synthetic selkies client + screen-change test produced NVENC log confirmation and `nvidia-smi dmon` encoder/power activity. Blender viewport use is CPU/llvmpipe in this image; only a Cycles CUDA/OptiX render should appear as GPU compute.
- **Self-service start path hardened (2026-09-01)**: `apply-nrp-e2e.sh` now reuses an existing `dockerhub-dgilli` pull secret in the target namespace when local podman/docker auth is unavailable, instead of failing before render.

### Root cause (F60, codified for GPU deploys)
`root/defaults/startwm.sh:4-8` forces zink when `nvidia-smi` exists, `/dev/dri` is non-empty, and `DISABLE_ZINK=false`:
```bash
export MESA_LOADER_DRIVER_OVERRIDE=zink
export GALLIUM_DRIVER=zink
```
`Dockerfile.rhel9:191` sets `DISABLE_ZINK=false`, so GPU nodes push GNOME/mutter into Mesa zink unless the deployment overrides it. The committed GPU deploy path now sets `DISABLE_ZINK=true` (F64). A future image-level fix could change the default or make the `startwm.sh` hook respect the llvmpipe image intent, but that is not required for the current M2 deploy path.

### Current live deployment (2026-09-01)
Namespace: `slu-researchtechnologies-dgilli`
Deployment: `slu-rhel9-e2e`
Running GPU pod observed: `slu-rhel9-e2e-545d4555d5-gq5zf`
Node: `fiona-prg1.cesnet.cz`
GPU: RTX 2080 Ti, driver `595.71.05`

Earlier clean-path verification pod: `slu-rhel9-e2e-545d4555d5-zbfgb` on `k8s-chase-ci-10.calit2.optiputer.net`. Free scheduling can place the deployment on different GPU nodes.

Deployment env rendered by the committed path:
```text
TZ=UTC
USERNAME=abc
PASSWORD=<secretKeyRef selkies-password/password>
DISABLE_ZINK=true
```
Image-provided defaults relevant to GPU GNOME:
```text
NVIDIA_DRIVER_CAPABILITIES=all
DISABLE_DRI3=true
LIBGL_ALWAYS_SOFTWARE=1
GALLIUM_DRIVER=llvmpipe
MESA_GL_VERSION_OVERRIDE=4.5
```
`DESKTOP` is unset, so the image default GNOME branch is active. `SELKIES_MANUAL_WIDTH` and `SELKIES_MANUAL_HEIGHT` are intentionally unset for dynamic resolution.

Dynamic resolution state (clean-path verified 2026-08-31):
- Xvfb: `-screen 0 15360x8640x24`
- boot xrandr: `1024x768`
- xrandr maximum: `15360x8640`
- selkies settings: `is_manual_resolution_mode=false`, `manual_width=0`, `manual_height=0`
- in-pod selkies protocol resize from `1024x768` to `1920x1080` worked; `gnome-shell` stayed up; NVENC re-initialized at `1920x1080`.

### Repo state being finalized
- `deploy/nrp/apply-nrp-e2e.sh` — M1/M2 GPU path: `--gpu`, `--accept-nrp-utilization`, NRP >40% gate, dry-run support, `DISABLE_ZINK=true` render, `Recreate` strategy render, rendered-manifest cross-checks, and pull-secret fallback when local container auth is absent.
- `deploy/nrp/selkies-rhel9.yaml.template` — GPU env/strategy placeholders and docs.
- `memory-bank/...` — M2 clean-path state, F64, GPU utilization monitoring F65, start-script fallback F66, and self-service commands.
- No image rebuild is part of this M2 fix; the current private image remains `docker.io/dgilli/selkies-rhel9:v4-llvmpipe`.

### Self-service workstation commands
From the repo root:

```bash
cd /home/its_admin/projects/slu-docker-rhel-selkies

# GPU workstation, same app as the current e2e deployment
./deploy/nrp/apply-nrp-e2e.sh \
  --name slu-rhel9-e2e \
  --namespace slu-researchtechnologies-dgilli \
  --gpu \
  --accept-nrp-utilization

# Non-GPU workstation, separate app name
./deploy/nrp/apply-nrp-e2e.sh \
  --name slu-rhel9-cpu \
  --namespace slu-researchtechnologies-dgilli
```

Separate named GPU + CPU pair:

```bash
# optional: free the current GPU e2e first
kubectl -n slu-researchtechnologies-dgilli \
  delete deployment,service,ingress \
  -l app=slu-rhel9-e2e \
  --ignore-not-found

./deploy/nrp/apply-nrp-e2e.sh \
  --name slu-rhel9-gpu \
  --namespace slu-researchtechnologies-dgilli \
  --gpu \
  --accept-nrp-utilization

./deploy/nrp/apply-nrp-e2e.sh \
  --name slu-rhel9-cpu \
  --namespace slu-researchtechnologies-dgilli
```

URLs:

```text
https://slu-rhel9-e2e.nrp-nautilus.io
https://slu-rhel9-gpu.nrp-nautilus.io
https://slu-rhel9-cpu.nrp-nautilus.io
```

Login: `abc` + password from secret `selkies-password`, key `password`.

### GPU utilization checks
```bash
NS=slu-researchtechnologies-dgilli
APP=slu-rhel9-e2e
POD=$(kubectl -n "$NS" get pod -l app="$APP" -o jsonpath='{.items[0].metadata.name}')

# live monitor: sm = CUDA/graphics compute, enc = NVENC, pwr = power
kubectl -n "$NS" exec "$POD" -- nvidia-smi dmon -d 1

# CUDA/compute process list
kubectl -n "$NS" exec "$POD" -- \
  nvidia-smi --query-compute-apps=pid,process_name,used_memory --format=csv

# encoder session stats
kubectl -n "$NS" exec "$POD" -- \
  nvidia-smi --query-gpu=encoder.stats.sessionCount,encoder.stats.averageFps,encoder.stats.averageLatency --format=csv

# selkies NVENC confirmation
kubectl -n "$NS" logs -l app="$APP" --tail=200 | grep -E "NVENC|Stream settings"
```

Interpretation:
- idle desktop → `0%` is normal
- active selkies stream with changing pixels → `enc` / power / NVENC logs should show activity
- Blender viewport only → GPU compute stays `0%` because desktop GL is llvmpipe
- Blender Cycles CUDA/OptiX render → `sm` / memory / compute-app PID should rise

### Next
1. M2 is closed for the current GPU GNOME + NVENC + dynamic-resolution scope. M3 remains deferred.
2. If Blender GPU rendering is needed later, treat it as a workload-specific follow-up: verify Cycles device selection and confirm `nvidia-smi dmon sm` rises during a render.
3. Future roadmap items are tracked in `productContext.md#Future-Roadmap-user-tracked-2026-09-01`:
   - GPU use for desktop rendering
   - project CLI/UX improvements
   - selkies menu app installer fix
   - proper project/fork maintenance workflow
   - proper SLU image registry
   - documentation expansion

## Git Reality (changed 2026-08-27)
- `origin` = **user's fork** `git@github.com:dGilli/docker-baseimage-selkies.git` (commits/pushes allowed)
- `upstream` = `linuxserver/docker-baseimage-selkies` — **push DISABLED, never push there**
- Branch `rhel9` created from `master` (upstream tip `69f4fc9`, incl. PR #184 svc-de→legacy-cont-init fix)
- Old local baseline commits (`eb4e145`/`ed91a5a`) are gone (repo re-initialized as full upstream clone); memory-bank re-committed on `rhel9`
- User checked out `fedora42` first; `fedora43`/`fedora44` available on `upstream` refs — **fedora44 = reference** (same selkies pin 348bc4f as master)

## Task: Add RHEL 9 to the supported images
### User Decisions (recorded)
~~UBI9 base via `lscr.io/linuxserver/baseimage-el:9`~~ → **SLU-owned UBI9 base + entitled RHEL repos** (2026-08-27 vetting decision; baseimage-el deprecated + Oracle-repo-based, see vetting doc §2) | x86_64 first | `Dockerfile.rhel9` in-repo | local podman (5.8.2) | scope: **desktop + streaming first** (defer: DinD, GPU/Zink, proot-apps, pelorus, Wayland) | **NRP = production environment** (SLU k8s researcher desktops) | **phase-1 CPU must test locally** on this RHEL 9.8 host | fold NRP learnings into this repo's image

### PLAN v4 (final — vetted, SLU-owned base) — APPROVED 2026-08-27
Supersedes PLAN v3. Full vetting evidence: `tasks/2026-08/270827_rhel9-vetting-plan-v4.md`. Key v3→v4 deltas: **vendored SLU-owned base** (baseimage-el is deprecated + Oracle-repo-based), defect fixes D1–D6, expanded test matrix, provenance artifact.

**Model**: `Dockerfile.rhel9` = **fedora44's Dockerfile with EL9 substitutions** (NOT a Debian port), on an **SLU-owned UBI9 base stage with entitled RHEL repos**. Shared `root/` tree stays one codebase across debian/fedora/rhel9 variants. NRP production realities (llvmpipe, repos, k8s) folded in.

**A0. Base stage (NEW in v4 — replaces baseimage-el:9)**
Vendor the ~100-line deprecated `docker-baseimage-el` Dockerfile as stage `base`:
- `FROM registry.access.redhat.com/ubi9/ubi` (digest-pinned phase 1)
- **No Oracle repo file, no RPMFusion** (breeze-cursor-theme is EPEL; RPMFusion only needed for deferred phase-2 DEV_MODE ffmpeg)
- RHEL content via **entitlement passthrough** (podman on this registered 9.8 host auto-mounts entitlements). Preflight: `podman run --rm registry.access.redhat.com/ubi9/ubi dnf repolist` must show `rhel-9-for-x86_64-*`
- EPEL9: `dnf install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm` (NRP-proven on UBI9)
- s6-overlay 3.2.0.2 tarballs (noarch + x86_64 + symlinks), base tools (`catatonit jq busybox…` — busybox is EPEL), `abc` (uid 911, /config, /bin/false), `mkdir /app /config /defaults /lsiopy`, docker-mods scripts, ENV (`S6_*`, `VIRTUAL_ENV=/lsiopy`, `PATH=/lsiopy/bin:$PATH`)
- **Build constraint**: image builds only on entitled RHEL hosts; runtime needs no entitlement (NRP just pulls)

**A. Dockerfile.rhel9 — runtime stages & steps**
1. Stage `frontend`: copy master's frontend stage verbatim (alpine 3.22, npm, selkies pin `348bc4f`, dashboards `selkies-dashboard selkies-dashboard-wish`)
2. Runtime: `FROM base` (stage A0)
3. ENV: LSIO set (`DISPLAY=:1 HOME=/config START_DOCKER=true PULSE_RUNTIME_PATH=/defaults SELKIES_INTERPOSER=/usr/lib/selkies_joystick_interposer.so NVIDIA_DRIVER_CAPABILITIES=all DISABLE_ZINK=false` **`DISABLE_DRI3=true`** `SELKIES_ENCODER="x264enc,jpeg" TITLE=Selkies`) **+ NRP llvmpipe trio** (`LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe MESA_GL_VERSION_OVERRIDE=4.5`). **DISABLE_DRI3=true is a deliberate rhel9 divergence (defect D5)**: stock EL9 Xvfb has no LSIO `-vfbdevice` patch — the env gate at `svc-xorg/run:19` prevents a crash-loop on any host/node exposing `/dev/dri` (NRP GPU nodes!)
4. (EPEL already in base — no repo step needed here)
5. RUN dnf install (verified against entitled RHEL 9.8 repos; provenance recorded at build end):
   - build: `gcc gcc-c++ make glibc-devel kernel-headers ncurses` (ncurses = `tic` for st terminfo)
   - X: `xorg-x11-server-Xvfb xorg-x11-server-utils xorg-x11-utils xorg-x11-xauth xorg-x11-xkb-utils xkeyboard-config xorg-x11-fonts-{75dpi,100dpi,misc}` (**+xkb-utils = xkbcomp for Xvfb keymaps, D6b**)
   - GL/mesa: `mesa-libGL mesa-libEGL mesa-libgbm mesa-dri-drivers mesa-vulkan-drivers libva` (KEEP mesa-dri-drivers — llvmpipe GLX)
   - desktop: `openbox xsettingsd st xdotool xclip xsel exo breeze-cursor-theme` (all EPEL9, verified) + `xterm xdg-utils` (AppStream; **xdg-utils IS available — D6**)
   - audio/web: `pulseaudio pulseaudio-utils nginx nginx-mod-fancyindex` (pulseaudio = AppStream 15.0 verified; fancyindex = EPEL9 → shared `default.conf` UNMODIFIED)
   - python: `python3.11 python3.11-pip python3.11-libs`
   - fonts/locales: `dejavu-sans-fonts google-noto-sans-fonts` **`google-noto-sans-cjk-ttc-fonts`** `google-noto-cjk-fonts-common google-noto-emoji-fonts glibc-all-langpacks glibc-locale-source` (**EL9 CJK pkg name — D3**; same `localedef` loop as fedora44/el9/master)
   - dbus/misc: `dbus dbus-daemon` **`dbus-x11`** `file procps-ng psmisc iproute kbd which tar curl openssl sudo shadow-utils util-linux` (**dbus-x11 = dbus-launch for shared startwm.sh — D2**)
6. RUN openbox tweaks (fedora44/el9-proven sed set — no `/debian-menu/d` line on EL): NLIMC→NLMC, maximized+position application, C-S-d ToggleDecorations keybind, desktops number 4→1, `/usr/bin/openbox-session` `--replace`
7. RUN st terminfo fix: `tic -i /usr/share/doc/st/st.info` (EPEL gap; verified file location)
8. RUN selkies: `python3.11 -m venv --system-site-packages /lsiopy`; fetch selkies `348bc4f` tarball; seds (`/"av>/d`, `/cryptography/d` — master parity); `pip install . && pip install setuptools` (pixelflux/pcmflux cp311 manylinux_2_28 wheels verified on PyPI; NO rust needed). Fallback if pip stumbles: `pip install -U pip` first
9. RUN interposer (`gcc -shared -fPIC -ldl` → `/usr/lib/selkies_joystick_interposer.so`) + fake-udev (`make` → `/opt/lib/libudev.so.1.0.0-fake`)
10. RUN icons (selkies-logo/favicon from docker-templates, master parity)
11. RUN user: `chpasswd abc:abc`; `usermod -s /bin/bash abc`; **`groupadd sudo` (D4 — group doesn't exist on EL9)**; `usermod -aG sudo abc`; append `%sudo ALL=(ALL:ALL) NOPASSWD: ALL` to `/etc/sudoers` (main file, not sudoers.d — shared hardening sed `s/NOPASSWD/CORRUPT_FILE/` targets `/etc/sudoers`)
12. ~~legacy-cont-init stub~~ **DELETED (D1)**: s6-overlay 3.2.0.2 ships `legacy-cont-init` builtin (base bundle, verified in tarball) — a user-level stub would be a duplicate definition and **break s6-rc-compile at boot**. Same reason Debian master needs no stub for svc-de's dep
13. RUN theme (Clearlooks openbox theme from lang-stash, master parity — EPEL openbox ships the theme dir)
14. `COPY /root /` + `COPY --from=frontend /buildout /usr/share/selkies`
15. RUN provenance + cleanup: `dnf repoquery --installed --qf '%{name}|%{version}|%{reponame}'` → basis for `package_versions_rhel9.txt` (RHEL-vs-EPEL evidence for the "supported" claim); `dnf clean all`, `rm -rf /tmp/* /var/cache/dnf/*`
16. `EXPOSE 3000 3001` + `VOLUME /config` (master parity; ws port 8082 runtime-dynamic like other variants)

**B. Shared `root/` tree — 4 distro-aware edits (each a no-op on Debian/Fedora)**
1. `init-nginx/run`: branch `NGINX_CONFIG` — `sites-available` if present (debian), else `mkdir -p /etc/nginx/conf.d && NGINX_CONFIG=/etc/nginx/conf.d/default.conf` (EL) [fedora44 did the hard swap; we branch to keep one tree]
2. `svc-selkies/run` DEV_MODE: **phase-1 = gate off on non-Debian** (`. /etc/os-release; [[ $ID == debian ]] || { echo "DEV_MODE not supported on $ID (phase 1)"; }` skip). Documented upgrade path = fedora44's dnf DEV_MODE (rust/cargo/nodejs verified in CS9) if dev parity is wanted later. *(micro-decision for user at approval)*
3. `init-selkies-config/run`: proot-apps block guarded by `[ -d /proot-apps ]` (absent in phase 1)
4. `svc-docker/run`: `command -v dockerd >/dev/null 2>&1 || { echo "docker not installed on this variant"; sleep infinity; }` (DinD = phase 2)

**C. Artifacts**
- `package_versions_rhel9.txt` (same NAME/VERSION/TYPE format; rpm -qa + pip freeze **+ per-package repo provenance** `%{name}|%{version}|%{reponame}` — RHEL-vs-EPEL evidence)
- `readme-vars.yml`: add rhel9 image row (source of truth; README.md stays builder-generated)

**D. Local phase-1 CPU test procedure (this RHEL 9.8 host, podman 5.8.2)**
0. **Preflight (v4)**: `podman run --rm registry.access.redhat.com/ubi9/ubi dnf repolist` shows `rhel-9-for-x86_64-*` (entitlement passthrough works)
1. `podman build -f Dockerfile.rhel9 -t dgilli/baseimage-selkies:rhel9-p1 .`
2. `podman run -d --name selkies-rhel9-p1 -p 3000:3000 -p 3001:3001 -p 8082:8082 -e TZ=America/Chicago -e PASSWORD=baseimage123 dgilli/baseimage-selkies:rhel9-p1`
   - rootless caveats: cgroupv2 ✅ on RHEL9; `mknod` gamepad nodes will fail → code's `touch` fallback handles it; if s6 cgroup issues appear, re-test with `sudo podman run` / `--privileged` (documented)
3. Verify: s6 chain reaches `init-selkies-end` (no flapping via `s6-rcstatus`); procs = Xvfb, openbox, `st` (autostart), selkies, nginx, pulseaudio; `curl -sI :3000` (200/301), `curl -skI :3001`, basic-auth with abc/baseimage123; 8082 listening; `pactl list sinks` shows output+input null sinks; GL sanity via browser; **dbus-daemon up as abc** (svc-dbus watch item)
4. Manual: browser → dashboard → connect → desktop renders, window + keyboard + audio work
5. Regression: `bash -n` on the 4 edited shared scripts; optional `podman build -f Dockerfile .` (debian) to prove no-ops
6. **Negative/edge matrix (v4)**: `--device /dev/dri/renderD128` run → Xvfb stays up (proves D5 fix) | `--privileged` → svc-docker guard message, no flapping | `-e HARDEN_DESKTOP=true` → sudoers sed round-trip + xdg-open/exo-open chmod 0000 | `-e PIXELFLUX_WAYLAND=true` → documented wait-forever behavior (no labwc phase 1) | `-e LC_ALL=de_DE.UTF-8` boot | `-e DEV_MODE=pixelflux` → gate message, default boot unaffected

**E. NRP production path (phase 1.5 — AFTER local approval, separate work item)**
- Push `dgilli/baseimage-selkies:rhel9-*` to the SLU registry NRP pulls from
- Update `slu-nrp-k8s-vm/selkies-rhel9.yaml.template` env mapping: `PASSWD→PASSWORD`, drop `BASIC_AUTH_*` (nginx does basic auth), `DISPLAY_SIZEW/H → SELKIES_MANUAL_WIDTH/HEIGHT`, ports 3000/3001 (+8082 ws) instead of 8080
- **Open production question for user**: NRP's current image runs non-root (`USER rheluser`); our LSIO-parity image is **rootful** (s6 `/init`, services drop to `abc`). NRP Deployment must permit root (no `runAsNonRoot: true`) — same as every LSIO image in a k8s desktop
- Audio divergence documented: NRP image uses PipeWire; LSIO stack uses PulseAudio (pcmflux/selkies capture via pulse null sinks) — keep PulseAudio for parity

**Risks**
| Risk | Mitigation |
|------|-----------|
| rootless podman + s6 quirks | graceful mknod fallback in code; sudo-podman/`--privileged` test fallback |
| entitled-host build constraint (v4) | documented in build-deployment.md; SLU builds on registered RHEL hosts; runtime/NRP unaffected |
| ubi9/ubi tag float | digest-pin phase 1 |
| pip vs selkies build backend | optional in-venv pip upgrade fallback |
| EPEL openbox rc.xml sed targets | proven on el9+fedora44 (openbox 3.6.x); smoke test catches openbox boot |
| EPEL openbox auto-deps (redhat-menus, python3-pyxdg) | dnf resolves; build surfaces conflicts |
| `cvt` absent on EL9 | svc-de modeline step no-ops (Xvfb screen size still honored) — documented degradation; verified graceful (empty MODELINE_NAME → grep matches all → block skipped) |
| svc-dbus `--system` as abc on EL9 | `<user>dbus</user>` directive ignored non-root (Debian-identical behavior expected); explicit smoke test |
| `PIXELFLUX_WAYLAND=true` waits forever (no labwc phase 1) | documented limitation; optional guard echo |
| EL nginx default `:80` server block remains | harmless (port unexposed); optional cleanliness sed |
| NRP rootful requirement | flagged to user (phase 1.5 gate) |

**Budget**: 4 cycles | ~90 min (base stage adds one layer; preflight adds 5 min)

### Verified EL9 gaps (accepted degradations, phase-2 candidates)
dunst | xorg-x11-drv-{intel,amdgpu,nouveau,qxl} + mesa-va-drivers (NOT in real RHEL9 either — verified on 9.8 host; GPU phase 2) | *(REMOVED from gaps by vetting: xdg-utils — IS in AppStream 1.1.3 (D6); breeze-cursor-theme — IS in EPEL9, RPMFusion not needed; cvt/gtf — IN xorg-x11-server-Xorg, installed, F08 corrected + F41)*

### Upstream lessons (el9 branch forensics — git-verified 2026-08-31, full history in `upstream/el9`)
- **Timeline (11 months, zero post-mortem in git)**: branched 2025-06-19 (`89ba111`, from 2025-05-14 tree `4ec1948`) onto `baseimage-el:9` (UBI9 + **Oracle** repos — deprecated/frozen base; vetting §2) → **DRI3 wave 2025-07-06/07 hit every other branch** (`088cf3b` bookworm, `1fabcf6` fedora, `951dbaf` kali, `c3ad147` ubuntu, `8647a55` arch, `fdff0a9` alpine) — **el9 never merged forward; only branch that never had the `-vfbdevice` svc-xorg block** → 2025-11-20: mesa removal (`4b42cad`) **25 min after** selkies bump `991d47e` (same-day build break; bot resumed same evening) → Dec 2025: selkies `159656d` + **`pixelflux==1.4.7` manual pin** (`6e6c321`) + "fallback to jpeg on hosts that require it" era (`1d25850`) → 18 weeks bot-only upkeep → 2026-05-07 `5fa9f96` "deprecate" = **one line** (`project_deprecation_status: true`) → 2026-05-10 PR #159 merged + CI deleted the Jenkinsfile (`2683a56`). cstate #310 not public (404).
- **Death chain (root cause NOW KNOWN — was "never recorded")**: (1) RHEL9 stock Xvfb **lacks `-vfbdevice`** (verified in our c8 image: `Unrecognized option`) → DRI3 GPU capture path impossible on EL9 without custom Xvfb or real Xorg+DDX (F29/F58); (2) **reproduced dnf conflict on frozen baseimage-el:9**: Oracle `el9_appstream` (mesa-filesystem 25.0.7-3.el9_7) + UBI `ubi-9-appstream-rpms` (25.2.7-4.el9) + `el9_distro_builder` + RPMFusion double-provide → mesa-dri-drivers/mesa-va-drivers unresolvable → `4b42cad` removed both → **no DRI drivers, no VA-API at all** → software-only desktop + JPEG-fallback era; (3) base deprecated/frozen/Oracle-sourced → unfixable by the team
- **Corrections to prior MB notes**: F16's "upstream el9 deleted the whole block" = **wrong** (they never had it — see F16 updated); `4b42cad` = same-day build fix, not planned feature removal
- Drift at death: openbox (never GNOME), `python3` 3.9 venv w/o `--system-site-packages`, selkies `159656d` + pinned pixelflux — vs ours: 348bc4f + **pixelflux 2.0.0 FLOATING (pin it — F58)**, 3.11 + system-site-packages, SLU-owned base (single-source repos → their layer-2 mesa failure cannot recur here)

## RHEL9 GPU Facts (verified 2026-08-27 on subscribed RHEL 9.8 host — real cdn.redhat.com repos)
- **No DDX drivers in RHEL9**: AppStream has only xorg-x11-drv-{dummy,evdev,fbdev,v4l,vmware,wacom}; NO intel/amdgpu/nouveau/qxl. Xorg rendering = modesetting driver only (kernel KMS + mesa DRI).
- **No `mesa-va-drivers`, no `intel-media-driver`** in RHEL9. Intel VA-API encode = `libva-intel-hybrid-driver` (iHD, present ✅). `libva-nvidia-driver` (library only) present ✅; **no nvidia driver packages/module in enabled repos**.
- `mesa-dri-drivers` (all DRI incl. llvmpipe/swrast) present ✅; xorg-x11-server-{Xorg,Xvfb,Xwayland,Xephyr} present ✅.
- Upstream el9's death ("DRI3 is not supported on el9", 4b42cad 2025-11-20) = the **Xvfb `-vfbdevice /dev/dri/renderD` DRI3 render-node path** (our svc-xorg GPU flow) failed on EL9; they stripped mesa-dri+mesa-va → GPU nonfunctional → deprecated 2026-05 (PR #159 + cstate #310). No commit records the deeper root cause.
- **Phase-2 GPU conclusion**: only RHEL-supported path is real **Xorg + modesetting + DRI3** (not Xvfb-vfbdevice). Intel encode via libva-intel-hybrid-driver; NVIDIA needs out-of-repo driver.
- Core (CPU) scope unaffected: Xvfb without render node + llvmpipe GLX works. NRP project proved the force-llvmpipe env trick on EL9 (`LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe MESA_GL_VERSION_OVERRIDE=4.5`) — **adopt as ENV in Dockerfile.rhel9** (GL fallback guarantee).
- **2026-08-31 empirical additions (GPU plan)**: RHEL9 AppStream Xvfb rejects `-vfbdevice` outright (`Unrecognized option` — probed in c8 image; D5/F16 upgraded to observed). Our SLU base installs `mesa-dri-drivers` 25.2.7-4.el9 clean (`Dockerfile.rhel9:243`) — upstream's mesa death was `baseimage-el:9`-specific repo double-provide (reproduced verbatim). NRP GPU platform model + SLU nautilus GPU inventory (8 nodes, driver 595.71.05 uniform; L4×16 gpu01–06 = generic NVENC-capable pool; A100×4 gpu07–08 = gated `nvidia.com/a100`, no NVENC) + pixelflux 2.0.0 capability + **M3 driver-mismatch pattern (official boot-time userspace install)**: all in **F58**.

## NRP relationship (clarified 2026-08-27 by user)
1. **This repo, `rhel9` branch (PLAN v3)**: the image we build — LSIO baseimage fork (baseimage-el:9 + s6 + `abc` + openbox/Xvfb, parity with debian).
2. **NRP (`slu-nrp-k8s-vm/`)** = **the production environment** (SLU k8s researcher desktops; `nrp-workspace up --type desktop --variant rhel9`; yaml templates, coTURN, GPU node scheduling). Its current ad-hoc image (`Dockerfile.ubi9-selkies`, v2/v3/v4-llvmpipe) = UBI9 + supervisord + `rheluser` + GNOME-on-Xorg.
3. Direction: **this repo's image is the phase-1 CPU deliverable, tested locally, then deployed to NRP** (phase 1.5 = registry push + NRP template env mapping). NRP *learnings* folded into PLAN v3 (llvmpipe ENV, EPEL-on-UBI9 repo line, xorg container flags as phase-2 Xorg input); NRP's supervisord/GNOME architecture NOT adopted (LSIO parity kept).

## Guest image note
`registry.redhat.io/rhel9/rhel-guest-image:latest` = **qcow2 VM disk delivery vehicle** (rhel-guest-image-9.8-20260428.2, KubeVirt env), 12 files total — not executable, not a container base, empty content-sets. Version info: RHEL **9.8**. If a true RHEL9 *container* base is wanted later: `registry.redhat.io/rhel9/rhel-core:9` (subscription; this host can pull it) vs UBI9 (free, current plan).

## BUILD executed 2026-08-27 (user released the hold)

**Revert tag**: `pre-rhel9-build` (annotated, → `7b3af8b`) — everything built is after this tag.

**Image**: `dgilli/baseimage-selkies:rhel9-p1` = `10bbd70e1502` (cycle 5, +`xorg-x11-server-Xorg` for cvt/gtf — F41; x86_64; base pin ubi9@`03b3d228` amd64 manifest).

**New files**: `Dockerfile.rhel9` (3 stages: base/frontend/runtime, 15 steps), `root-base/` (vendored baseimage-el s6 tree, 68 files, Oracle repo+GPG excluded), `package_versions_rhel9.txt` (final: 213 ubi9-base / 211 rhel9 / 24 epel + 41 python), `readme-vars.yml` +RHEL row.

**Shared-tree edits (4, all no-ops on Debian)**: `init-nginx/run` (conf.d branch), `svc-selkies/run` (DEV_MODE non-Debian gate), `init-selkies-config/run` (`[ -d /proot-apps ]` guard), `svc-docker/run` (dockerd guard).

**Build cycles (5)**: c1 `xorg-x11-{xkb,font}-utils` don't exist on RHEL9 → `xkbcomp`+`mkfontscale` (F31) | c2 evdev missing Python.h → `python3.11-devel` (F32) | c3 xkbcommon cffi vs libxkbcommon 1.4 → `xkbcommon<1.5` pin + `libxkbcommon-devel` (F34) | c4 `tic -i` unsupported on ncurses 6.2 → plain `tic` (F35) | c5 **user manual test #1**: dashboard OK but "Waiting for stream..." → selkies runtime resize needs `cvt`/`gtf` (F41) → added `xorg-x11-server-Xorg` (F08 corrected). Full resolution dry-run (`--downloadonly`) added to catch name errors pre-build.

**Autonomous QA (ALL PASS)**:
- Boot: 10/10 user-bundle services up & stable (s6-svstat), init chain complete, no flapping
- Desktop: Xvfb `:1` (xdpyinfo OK), openbox `--replace` via dbus-launch (F19/D2 ✅), `st` from autostart (F22/F35 ✅), X socket abc-owned
- Web: 3000/3001 nginx + basic auth (401→200 w/ abc:baseimage123), dashboard HTML served, 8082 selkies ws listening, gamepad interposers up
- Audio: output+input null sinks (pactl)
- GL: llvmpipe probe — `llvmpipe (LLVM 21.1.7)` / GL 4.5 (F36)
- sudo: NOPASSWD as abc (F33/D4) | locale: de_DE.utf8 boots (LC_ALL matrix)
- Matrix: `--device /dev/dri/renderD128` → Xvfb no `-vfbdevice`, stable (D5/F16 ✅) | `SELKIES_MANUAL_WIDTH/HEIGHT=1280x720` → Xvfb `-screen 0 1280x720x24` ✅ | harden (`DISABLE_SUDO`+`DISABLE_OPEN_TOOLS`+`HARDEN_DESKTOP`) → CORRUPT_FILE sed + xdg-open/exo-open mode 0000 + nginx files block removed ✅ | `DEV_MODE=pixelflux` → gate msg "not supported on rhel", default boot ✅ (F39) | dockerd guard (simulated privileged via /dev/cpu_dma_latency + s6-svc -t) → "docker not installed… service idle", stable (F38)
- cvt fatal line in boot log = expected (F08 verified live; modeline skipped, size via Xvfb env)

**Needs MANUAL (user)**: browser → dashboard → connect → desktop renders + window drag + keyboard + audio. (Everything up to the browser handshake is verified autonomously.)

## Task 2: GNOME desktop as RHEL9 default X11 DE — **COMPLETE** (2026-08-28: approved + committed `11a8afd`; SLU wallpaper follow-up committed `06bc207`)
**User ask**: "get a GUI desktop working locally, not just a shell" → "the standard GNOME WM RHEL ships with" → "finalize your plan to get gnome-shell running as our window manager".

**Goal**: RHEL9 image's streamed X11 desktop boots **standard RHEL GNOME (gnome-shell 40.10)** instead of openbox+st-only. openbox stays as fallback (`DESKTOP=openbox` knob); LSIO autostart/`RESTART_APP` mechanism preserved.

**Key decision (NRP-proven pattern, F44)**: `startwm.sh` gnome branch runs `dbus-run-session -- /usr/bin/gnome-shell --x11 --sm-disable` — **direct gnome-shell launch, NOT gnome-session**. Bypasses: gnome-initial-setup welcome screen (not in gnome-shell's dep tree — F43), keyring prompts, logind dependency, blanking/lock. Matches NRP's production UBI9 recipe.

**Packages (FINAL — F46/F47)**: dnf list += `gnome-session gnome-session-xsession gnome-shell gnome-settings-daemon mutter nautilus gnome-terminal gedit gnome-calculator gnome-screenshot firefox glx-utils` (12). NOT needed: `xorg-x11-server-utils` (xsetroot already in image via F41's Xorg pkg) / `dbus-tools` (dbus-run-session ships in dbus-daemon on EL9, present).

**Changes (FINAL)**:
1. Revert tag `pre-gnome-desktop` → `24e1575` (create at BUILD start)
2. `Dockerfile.rhel9` runtime dnf list += the 12 pkgs — **dnf dry-run resolution first** (F31 lesson)
3. `root/defaults/startwm.sh` — **5th distro-aware no-op branch** (Debian path untouched; exact final code):
```bash
# Start DE
if [ -x /usr/bin/gnome-shell ] && [ "${DESKTOP}" != "openbox" ]; then
  # RHEL9 standard GNOME (gnome-shell 40.x): direct launch, NRP-proven pattern (F44/F47)
  export XDG_SESSION_TYPE=x11
  export XDG_SESSION_ID="${DISPLAY#:}"
  export XDG_CURRENT_DESKTOP=GNOME
  export DESKTOP_SESSION=gnome
  # fresh per-boot runtime dir: /config/.XDG is on the persistent volume and would
  # keep a stale dbus socket across container restarts (F48); NRP uses same pattern
  export XDG_RUNTIME_DIR=/tmp/runtime-abc
  mkdir -p "$XDG_RUNTIME_DIR" && chmod 0700 "$XDG_RUNTIME_DIR"
  xsetroot -solid "#2d2d2d" 2>/dev/null || true
  # wait for GLX readiness (mutter composites via software GL)
  for i in $(seq 1 30); do
    glxinfo 2>/dev/null | grep -q "OpenGL renderer string" && break
    sleep 1
  done
  dbus-run-session -- /usr/bin/gnome-shell --x11 --sm-disable &
  GNOME_PID=$!
  sleep 10
  nautilus -d 2>/dev/null || true
  # autostart app — exact command string svc-watchdog pgreps for RESTART_APP (F22)
  sh "$HOME/.config/openbox/autostart" &
  wait "$GNOME_PID"
else
  exec dbus-launch --exit-with-session /usr/bin/openbox-session > /dev/null 2>&1
fi
```
   Watchdog match proof: `HOME=/config` is container ENV → `sh "$HOME/.config/openbox/autostart"` cmdline = `sh /config/.config/openbox/autostart` = svc-watchdog `AUTOSTART_CMD` verbatim (`svc-watchdog/run:13`) → `RESTART_APP` works UNCHANGED. `wait $GNOME_PID` keeps svc-de alive; s6 kills the whole process group on service stop (clean gnome teardown).
4. `svc-xorg/run` — **no change** (F45)
5. `init-selkies-config/run` — **no change** (its `TERMINAL_NAMES` already lists `gnome-terminal` → `DISABLE_TERMINALS` hardening covers it, line 108)

**Tests**: autonomous = services 10/10 stable, `pgrep -u abc gnome-shell` (+nautilus), `glxinfo` renderer=llvmpipe, **`gnome-screenshot -f` → read PNG to visually confirm GNOME top bar**, launch gnome-calculator (window via xprop), autostart cmdline matches watchdog string. Edge = `DESKTOP=openbox` (old behavior intact) · `SELKIES_MANUAL_WIDTH/HEIGHT=1280x720` · `RESTART_APP=true` watchdog under GNOME (kill `sh /config/.config/openbox/autostart` → respawns) · existing hardening trio. **Manual (user)** = browser → GNOME desktop (Activities, app grid, firefox, nautilus, terminal, drag, clipboard).

**Risks**: gnome-shell-on-Xvfb (NRP proved on Xorg; extension set matches + GNOME CI precedent → low; **Plan B** = Xorg-in-gnome-mode in svc-xorg, NRP verbatim, one cycle) · +1.5–2 GB image (gnome+firefox) · gsd installed but logind-less → gsd power/idle inert, Settings app NOT installed (user configures via gsettings if ever needed).

**Budget**: 3 build cycles | **State**: awaiting approval ("approved"/"proceed" to start cycle 1).

### BUILD executed 2026-08-28 (approved; PLAN v2)
**Revert tag**: `pre-gnome-desktop` → `b4c199f`. **Final image (task 2)**: `dgilli/baseimage-selkies:rhel9-p1-gnome` = `99da8c1475f5` (c7, SLU wallpaper; superseded same day by R1 c8 `5c835fb6a147` — see Working Context).
**Cycles**: c1 `3c31e79a` — clean build; smoke found nautilus absent + dbus socket mismatch | c2 `5394646a` — F49 cache chown + dbus export attempt (still dbus-run-session) | c3 `a1ac52a0` — Dockerfile +`nautilus` (edit omission), explicit `dbus-daemon --address` (F51) | c4 `15f963f8` — `nautilus --no-desktop` (F50; **budget extension flagged to user, 1-line cached rebuild**) | c5 `f54738a5` — **final (user-directed at approval: remove st + nautilus auto-launch from the gnome branch — clean desktop)** | c6 `0325190e` — wallpaper attempt (F52: `span` invalid on RHEL9) | c7 `99da8c14` — **current final** (wallpaper `spanned`, screenshot-verified).
**Files**: `Dockerfile.rhel9` (+12 gnome pkgs), `root/defaults/startwm.sh` (gnome branch, 41 lines), `root/etc/s6-overlay/s6-rc.d/init-selkies-config/run` (+6 lines .cache chown, F49).
**Autonomous QA (ALL PASS, 2026-08-28)**: services 13/13 (s6rc-fdholder down=by design) · gnome-shell 474 + nautilus + st from autostart + deterministic session bus `/tmp/runtime-abc/bus` · GLX llvmpipe 4.5 · web 200 w/ abc:baseimage123 both ports · ws 8082 listening · **screenshot-confirmed full GNOME desktop** (top bar Activities/clock, st, nautilus Home window, dash w/ running indicators) · calc launches on real bus (F49 verified) · `/config/.cache` abc-owned · **edge 4/4**: `DESKTOP=openbox` → openbox+st, no gnome-shell (dbus-launch path) · `SELKIES_MANUAL_WIDTH/HEIGHT=1280x720` → Xvfb `-screen 0 1280x720x24` + gnome-shell up · `RESTART_APP=true` → killed st respawns (PIDs 829/830 → 1616/1620, watchdog exact-string match) · `HARDEN_DESKTOP=true` → sudo/xdg-open/**gnome-terminal** 0000 + CORRUPT_FILE sudoers + gnome-shell still up.
**Wallpaper follow-up (2026-08-28, user-provided)**: `SLU-RHEL.jpg` (1920×1080) moved to `root/usr/share/backgrounds/slu-rhel.jpg` (→ `/usr/share/backgrounds/slu-rhel.jpg` in image); gnome branch of `startwm.sh` sets `org.gnome.desktop.background` picture-uri/-dark + `picture-options=spanned` after the session bus is up (dconf user DB under /config, idempotent per boot). c6 `0325190e` exposed **F52** (RHEL9 enum is `spanned`, not Fedora's `span` — invalid value silently rejected, default `zoom` left); c7 `99da8c14` final, screenshot-verified SLU/RHEL wallpaper full-desktop under the top bar.

## Working Context
- Branch `rhel9`; **working tree clean**; commits: `bd46cdb` (phase-1 build) → `23964ff` (docs) → `24e1575` (close) → …MB… → `11a8afd` (task-2 GNOME build) → `a6cd73a` (task-2 docs) → `06bc207` (SLU wallpaper) → `710f7e9` (MB: proot-apps intel F53–F55 + roadmap R1, user session) → `b97a611`/`89f567a` (phase-1.5 push + NRP mapping) → `cc2c2b7` (R1 code) → `153290d` (artifact); revert tags: `pre-rhel9-build` → `7b3af8b`, `pre-gnome-desktop` → `b4c199f`, `pre-r1-proot-apps` → `89f567a`
- **Pushed image (PRIVATE)**: `docker.io/dgilli/selkies-rhel9` — production pin **`v4-llvmpipe`** = c8 `5c835fb6a147` (manifest `sha256:b70d42e30c7bef93ca7f0a4bd6a46540d4dfd2eabff1eafadd1d82fbdec5d059`); `:latest` = same c8 (dev). First c7 push = manifest `sha256:46246466…`. **NRP artifacts**: `deploy/nrp/selkies-rhel9.yaml.template` (PRODUCTION drop-in, F57) + `deploy/nrp/apply-nrp-e2e.sh` (one-shot idempotent cluster bootstrap — `7d7f2ea`, `--dry-run` read-only, pull secret from local podman auth) + `deploy/nrp-selkies-rhel9.yaml` (standalone dev manifest). **Live NRP facts**: context `nautilus`, ns `slu-researchtechnologies-dgilli` (RBAC ns-scoped — `default` 403s; context-ns auto-detect works), no PSA labels (rootful OK), haproxy ingress ~2-min reconcile (initial 503 = timing), node `nautilus-it-cpu15.fullerton.edu`; **secrets in ns**: `selkies-password` (temp, user-created) + `dockerhub-dgilli` (pull, script-managed) — e2e workload torn down by user post-verification
- Live containers: `selkies-rhel9-p1-gnome` (:3000/:3001/:8082, abc/baseimage123, c7 image — filezilla proot-app on its volume) + `selkies-r1-verify` (:3200/:3201/:3282, **c8 image**, FileZilla installed + running). Phase-1 container `selkies-rhel9-p1` exited (137); phase-1 image `10bbd70e1502` available for A/B
- Host: RHEL 9.8 (Plow), subscribed; /dev/dri/renderD128 present; podman 5.8.2 rootless
- Evidence: vetting `tasks/2026-08/270827_rhel9-vetting-plan-v4.md` · build `tasks/2026-08/270827_rhel9-build.md` · GNOME `tasks/2026-08/280828_rhel9-gnome-desktop.md` · phase-1.5 `tasks/2026-08/280828_phase1-5-nrp-dev-push.md` · R1 `tasks/2026-08/280828_r1-proot-apps.md` · findings `findings.md` F01–F56
- NRP reference repo (**earlier attempt — user-confirmed REFERENCE ONLY, not the current system**): `/home/its_admin/projects/slu-nrp-k8s-vm/{Dockerfile.ubi9-selkies,selkies-rhel9-entrypoint.sh,supervisord-rhel9.conf,nrp-workspace,workspace-config.conf,selkies-rhel9.yaml.template}` — source of the GNOME launch recipe (F44), the placeholder/sed template contract (F57), and config values (ingress domain, QoS, secrets, DNS). Production artifact for the current NRP system = OUR `deploy/nrp/selkies-rhel9.yaml.template`
- Scratch: /tmp/opencode/ref/{f44,el9}.Dockerfile, prov_attr.sh, ubi_base.txt, gl_probe.c, baseimage-el clone (disposable)

## Roadmap (deferred — user decision 2026-08-28: "do later down the line")

### R1: proot-apps — make the dashboard "Install/Update/Remove app" actually work
**Steps 1–2 SHIPPED in c8 `5c835fb6a147` (2026-08-28, commit `cc2c2b7`); step 3 (SLU catalog) pending separate decision.** Findings F53–F56. Fresh-volume verified: install → run → FileZilla 3.68.1 rendered (NotSandboxed fallback). Dev registry re-pushed (manifest `sha256:b70d42e3…`). Task doc: `tasks/2026-08/280828_r1-proot-apps.md`.
- **Root cause (F53)**: dashboard buttons are client-side simulations; real path = `cmd,<command>` over the selkies WebSocket → Python agent fire-and-forget shell (`input_handler.py:2310`, stdout/stderr→DEVNULL, no feedback channel) → `/selkies-proot <action> <app>` → `st $HOME/.local/bin/proot-apps`. Our variant ships no `/proot-apps` (upstream builds it at `Dockerfile:534-540` from the latest `linuxserver/proot-apps` release tarball: static `proot`+`jq`+`ncat`+`proot-apps` bash script — self-contained, no host packages). Shared `init-selkies-config/run:260-273` already has the variant-guarded `$HOME/.local/bin` bootstrap — **shipping /proot-apps is the only image-side gap** for the tool chain.
- **Deeper bug (F54)**: glycin 2.1.5 apps (e.g. filezilla 3.68.1, NixOS-built Alpine rootfs) hang at startup under proot — glycin's bwrap sandbox-availability test (`bwrap --unshare-all … --seccomp <fd> /usr/bin/true`) is awaited via `command.output()` **with no timeout** (glycin/src/sandbox.rs:726), and bwrap's namespace creation deadlocks inside proot's ptrace emulation. Reproduced under GNOME and openbox; blocking `unshare` via seccomp does NOT help. **Verified fix**: stub the app rootfs's `bwrap` (3-line script emitting the glycin-recognized string `bwrap: No permissions to create a new namespace`, exit 1) → test returns in ~9 ms → clean `NotSandboxed` fallback ("WARNING: Glycin running without sandbox") → FileZilla window rendered (evidence `/tmp/opencode/fz-stub.png`). Only bwrap-bundling apps affected.
- **Runtime (F55)**: podman default seccomp here allows `ptrace`+`unshare` (proot viable as-is); stock docker's default likely blocks `ptrace` → verify + map at phase 1.5 (NRP).
- **Catalog (user question, answered in F53)**: app list is fetched **by the user's browser** from hardcoded `REPO_BASE_URL` (`Sidebar.jsx:74-78`, selkies frontend commit 348bc4f): `https://raw.githubusercontent.com/linuxserver/proot-apps/master/metadata/metadata.yml` (+`img/<icon>`). YAML `include:` entries `{name, full_name, arch, icon, description, disabled}`; `disabled: True` hides. Installables = single-layer OCI images, bare name → `ghcr.io/linuxserver/proot-apps:<name>`, **full refs `org/img:tag` accepted** (proot-apps treats an arg containing `/` or `:` as a full image ref); offline = `PA_REPO_FOLDER`+`LOCALREPO` (`proot-apps localrepo get/update/remove`).
- **Planned fix steps** (ready to execute when scheduled):
  1. Port upstream proot-apps build section into `Dockerfile.rhel9` (release tarball → `/proot-apps/` + `pversion`); shared `init-selkies-config` bootstrap then wires `$HOME/.local/bin` unmodified.
  2. RHEL9 variant of `root/selkies-proot`: before exec, replace `bwrap` with the stub in each installed app rootfs (loop over `$HOME/proot-apps/*/usr/bin/bwrap`; idempotent).
  3. Optional (separate decision): patch `REPO_BASE_URL` in the frontend stage (one-line sed before `npm run build`) → SLU catalog (SLU fork of proot-apps, or `metadata.yml`+icons served from the container's own nginx = offline-capable); prune via `disabled:`; add SLU apps as full OCI refs.
- **Live-container state (outside the image)**: filezilla is installed + working in `selkies-rhel9-p1-gnome` (rootfs under `/config` volume, stub applied, app-grid entry `FileZilla PA`, `filezilla-pa` CLI wrapper) — persists on that volume only; a fresh volume loses it until the image fix lands.
