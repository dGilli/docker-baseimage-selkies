# 090909_m3-gpu-xorg-nvenc

## Objective
Implement M3: GPU desktop rendering via real Xorg + NVIDIA DDX on RHEL9, with NVENC GPU encoding and latency optimization options.

## Outcome
- ✅ Xorg + NVIDIA DDX running in container (RHEL9 X 1.20.11)
- ✅ NVIDIA GPU rendering (glxinfo: OpenGL 4.6.0, multiple GPU models verified)
- ✅ Pixelflux MIT-SHM capture working (H.264 1080p frames flowing)
- ✅ NVENC GPU encoding (API v13.0, ~3ms per frame vs 12ms CPU)
- ✅ Performance baseline measured (server p50=31ms, E2E ~60ms)
- ✅ Latency options: WebRTC mode, configurable damage threshold
- ✅ gnome-terminal D-Bus fix (machine-id)
- ✅ PR #24 merged to `rhel9` (squash `eb44df9`)

## Files Modified
- `Dockerfile` — +xorg-x11-server-Xorg, +gcc (fakevt build), +fakevt compile, +machine-id, +damage threshold sed, +clearmodifiers sed
- `root/etc/s6-overlay/s6-rc.d/svc-xorg/run` — M3 Xorg path (session-user, fakevt, readiness, Xvfb fallback)
- `root/etc/s6-overlay/s6-rc.d/svc-selkies/run` — xdpyinfo wait, SELKIES_STREAM_MODE env
- `root/usr/local/bin/selu-xorg-config` — boot-time NVIDIA X module staging + xorg.conf writer
- `root/usr/local/src/fakevt.c` — LD_PRELOAD VT/KD ioctl shim (NEW)
- `deploy/nrp/apply-nrp-e2e.sh` — --gpu-xorg, --webrtc, SELKIES_AUTO_GPU, damage env, 600s timeout
- `deploy/nrp/selkies-rhel9.yaml.template` — GPU env docs
- `scripts/delta-allowlist.txt` — +svc-selkies/run, +svc-xorg/run entries
- `scripts/perf-baseline.md` — performance baseline data (NEW)
- `scripts/perf-regression.sh` — in-pod regression test (NEW)

## Patterns Applied
- `systemPatterns.md#S6-Service-Extension` — M3 path guarded by env+file-exists (no-op on f44)
- `decisions.md#2026-08-31-m3-driver-mismatch` — extract-only module staging (no full userspace install)
- Upstream `selkies-project/docker-selkies-glx-desktop` — session-user Xorg pattern

## Architectural Decisions
1. **fakevt.so over Xorg patch**: RHEL9 X 1.20.11 is a Red Hat package; can't patch. LD_PRELOAD shim intercepts the specific ioctls X 1.20 makes fatal. NVIDIA DDX doesn't use VTs for rendering.
2. **Xorg as session user**: MIT-SHM requires same-uid shmat. Matches upstream glx-desktop. DRI video groups handled by usermod at service start.
3. **NVENC via SELKIES_AUTO_GPU**: Pixelflux has built-in NVENC (direct libnvidia-encode.so, not VA-API). Auto-detect avoids hardcoded renderD numbers.
4. **Damage threshold 5/10 for GPU**: Lower than default 10/20 for faster input response. Configurable via env.

## Key Findings
- F75: RHEL9 X 1.20.11 VT fatal + fakevt.so shim details
- F76: MIT-SHM cross-user BadAccess + session-user fix
- F77: NVENC direct API (not VA-API) + activation mechanism
- F78: Performance baseline + optimization levers

## Testing
- GTX 1080 Ti (580.159.04): full stream + NVENC ✅
- RTX 2080 Ti (595.71.05): Xorg + NVIDIA ✅ (2 nodes)
- A10 (595.71.05/595.91.07): VT pass ✅ (2 nodes)
- V100 (580.159.04): full stream + NVENC + perf baseline ✅
- gnome-terminal: D-Bus machine-id fix ✅
- hadolint + delta gate: PASS ✅

## Artifacts
- PR: https://github.com/dGilli/docker-baseimage-selkies/pull/24
- Merge: `eb44df9` on `rhel9`
- Image: `m3-preview-24` (dev); production tag pending
- GH: #4 (closed), #22 (closed), #23 (open — EGL/DRI3 alternative)
