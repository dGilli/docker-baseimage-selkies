# Case Study: GPU Desktop Streaming in a RHEL9 Container

## Running a Real Xorg Server with NVIDIA DDX + NVENC in Kubernetes — and the Three Layers of "It Should Just Work" That Didn't

**Project**: SLU RHEL9 Selkies Fork (`slu-docker-rhel-selkies`)
**Duration**: 2026-09-03 → 2026-09-09 (7 days, ~23 image iterations)
**Outcome**: M3 merged to production branch. GPU desktop streaming via Xorg + NVIDIA DDX + NVENC on RHEL9 in Kubernetes.

---

## Executive Summary

We needed to stream a GPU-accelerated GNOME desktop from a RHEL9 container running on Kubernetes (NVIDIA GPUs) to a browser. The existing M2 path used Xvfb + llvmpipe (software rendering) — it worked but wasted the GPU. M3 replaced Xvfb with a real Xorg server loading the NVIDIA Display Driver (DDX), enabling native OpenGL 4.6, CUDA, and NVENC hardware encoding.

The "simple" swap (Xvfb → Xorg) exposed **three independent, deeply-hidden failure layers**, each requiring a different low-level fix:

1. **The Kernel/OS Layer**: RHEL9's X.Org 1.20.11 treats virtual terminal unavailability as fatal in containers. No flag, no config, no upstream patch. → *Solved with an LD_PRELOAD shim intercepting 14 distinct ioctl calls.*

2. **The X11 Protocol Layer**: Pixelflux (the Rust screen-capture library) uses MIT-SHM (SysV shared memory) to read the X root window. X.Org 1.20 denies `SHMAttach` across UID boundaries. → *Solved by running Xorg as the session user, matching the upstream selkies-project architecture.*

3. **The GPU Encoding Layer**: Pixelflux's NVENC encoder (direct `libnvidia-encode.so`, not VA-API) requires the DRI render node index, which is auto-detected only when `SELKIES_AUTO_GPU=true` is set. The X11 code path defaults to CPU. → *Solved with one environment variable.*

Total code: **~1,200 lines added** (237-line C shim, 180-line boot script, 60-line service script changes, 100-line perf test). Three days of investigation, two days of implementation.

---

## Problem Statement

**Goal**: Stream a GPU-rendered GNOME desktop from a RHEL9 container to a browser via WebRTC/WebSocket, with hardware H.264 encoding (NVENC), on NVIDIA GPUs in a Kubernetes cluster.

**Constraints**:
- RHEL9 (not Ubuntu/Fedora) — different X.Org build, different package availability
- Container (no kernel modules, no real TTY, no GPU driver install)
- Kubernetes (NVIDIA device plugin, nvidia-container-toolkit, no privileged mode)
- Must not break the existing M2 path (Xvfb + llvmpipe, CPU-only)
- Must work across heterogeneous GPU fleet (GTX 1080 Ti, RTX 2080 Ti, A10, V100, drivers 580.x/595.x)
- Pixelflux 2.0.0 is a pre-compiled Rust .so — no source access, no rebuild

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│ Browser (WebRTC/WebSocket client)                                    │
│   H.264 decode → canvas render                                      │
└──────────────────────────────┬──────────────────────────────────────┘
                               │ UDP/SRTP or TCP/WS
┌──────────────────────────────▼──────────────────────────────────────┐
│ NRP Ingress (haproxy, TCP-only)                                     │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────┐
│ Kubernetes Pod (RHEL9 container, rootful)                            │
│                                                                      │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │ Xorg :1 (user: abc) + NVIDIA DDX (nvidia_drv.so)             │   │
│  │   └── LD_PRELOAD: fakevt.so (VT ioctl shim)                  │   │
│  │   └── GPU: /dev/nvidia* + /dev/dri/card* (toolkit-mounted)  │   │
│  │   └── OpenGL 4.6, DRI3, MIT-SHM                              │   │
│  └──────────────────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │ GNOME Shell 40.10 (user: abc)                                │   │
│  │   └── D-Bus session bus (machine-id required)                │   │
│  │   └── gnome-terminal (D-Bus client→server)                   │   │
│  └──────────────────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │ Selkies 2.0.0 (Python, user: abc)                            │   │
│  │   └── Pixelflux 2.0.0 (Rust .so)                             │   │
│  │        ├── XCB + MIT-SHM capture (same-uid as Xorg)          │   │
│  │        ├── NVENC (libnvidia-encode.so, DRI render node)      │   │
│  │        └── x264 built-in (CPU fallback)                      │   │
│  └──────────────────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │ s6-overlay 3.2.0.2 (process supervision)                     │   │
│  │   └── svc-xorg → svc-de → svc-selkies (dependency chain)     │   │
│  └──────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────┘
                               │
                    ┌──────────▼──────────┐
                    │ NVIDIA GPU (shared) │
                    │ /dev/nvidia*        │
                    │ /dev/dri/renderD*   │
                    │ (toolkit-mounted)   │
                    └─────────────────────┘
```

---

## The Three Hurdles (Detailed)

### Hurdle 1: "Cannot find a free VT" (Kernel/OS Layer)

**Symptom**:
```
parse_vt_settings: Cannot find a free VT
xf86OpenConsole: KDSETMODE KD_GRAPHICS failed: Inappropriate ioctl for device
Fatal server error: AddScreen/ScreenInit failed
```

**Why it happens**:
- X.Org's `xf86OpenConsole()` opens `/dev/ttyN` and issues VT ioctls (`VT_GETMODE`, `VT_SETMODE`, `KDSETMODE`) to claim the console for graphics
- In a container, `/dev/tty0` exists but has no kernel VT backing (no `ttyldisc`, no `n_tty` line discipline for VTs)
- Upstream X 1.20 has a `-keeptty` flag that skips this entirely; X 21+ makes the console optional
- **RHEL9's X 1.20.11 build (`xorg-x11-server 1.20.11-34.el9_8.3`) does NOT have `-keeptty`** and treats failure as fatal
- No configuration file option, no environment variable, no Xorg flag bypasses this in the RHEL9 build

**Investigation** (Days 1-2, 20 image iterations):
1. Confirmed `/dev/tty0` exists in the container but `ioctl(VT_GETSTATE)` returns `ENOTTY`
2. Tried `-keeptty` → "unrecognized option" (not in RHEL9 build)
3. Tried `Option "DontVTSwitch" "True"` → doesn't prevent the initial VT claim
4. Tried removing the `Section "ServerLayout"` → Xorg still calls `xf86OpenConsole`
5. **Objdump'd the Xorg binary**: confirmed it references `open@GLIBC_2.2.5` and `ioctl@GLIBC_2.2.5` via PLT (interceptable by LD_PRELOAD)
6. **In-shim debug tracing** (added fprintf to the LD_PRELOAD library): discovered the exact sequence of 14 ioctls Xorg issues:
   - Opens `/dev/tty0` (NOT `/dev/tty1-63` as X 21 does)
   - `0x5600` (`_IO(0x56, 0)`) — non-standard RHEL9 VT query (pointer arg)
   - `VT_GETMODE (0x5603)` — pointer arg
   - `VT_ACTIVATE (0x5606)` — **integer arg** (VT number)
   - `VT_SWITCH (0x5607)` — **integer arg**
   - `VT_WAITACTIVE (0x5601)` — **integer arg**
   - `VT_SETMODE (0x5602)` — pointer arg
   - `KDGETMODE (0x5201)` — pointer arg
   - `0x4b3a`, `0x4b44`, `0x4b45` — non-standard RHEL9 keyboard ioctls (type 'K', not 'R')

**Critical sub-bug** (iteration 14-16):
- First shim version: `*(int *)arg = 0` for ALL ioctl args
- `VT_ACTIVATE(1)` passes `arg=1` (the VT number, an integer)
- `*(int *)1 = 0` → **segfault at address 0x1**
- Fix: pointer guard `arg > 0x1000 && arg < 0x7FFFFFFFFFFF` before dereferencing

**Solution**: `fakevt.so` (237 lines of C)
- Intercepts `open`/`open64`/`openat`/`openat64` for `/dev/ttyN` → redirects to `/dev/null`, tracks fd
- Intercepts `ioctl` on tracked fds: returns 0, fills pointer args with sensible defaults
- Intercepts KD/keyboard ioctls on ANY fd (Xorg issues some on stdin)
- Intercepts `close` on tracked fds
- NVIDIA DDX does NOT use VTs for rendering (GPU scanout via DRI) — shim only satisfies the console setup path

---

### Hurdle 2: "waiting for stream..." (X11 Protocol Layer)

**Symptom**:
- Xorg + NVIDIA DDX running perfectly (glxinfo shows NVIDIA, OpenGL 4.6)
- GNOME Shell connected, desktop rendering
- Browser shows "waiting for stream..." — no video frames
- Selkies log: `SelkiesStreamingApp initialized: encoder=x264enc, display=1024x768`

**Why it's tricky** (5 things that looked like the cause but weren't):
1. ~~Timing~~: Added `xdpyinfo` wait before selkies starts. Still failed.
2. ~~XCB connectivity~~: Wrote a C test program using libxcb. Connected fine, read 1920x1080.
3. ~~Missing x264enc GStreamer plugin~~: `gst-inspect x264enc` → not found. But `nm -D pixelflux.so` revealed `x264_encoder_open_165` — x264 is **built into the Rust .so**, not a GStreamer plugin.
4. ~~Wayland misconfiguration~~: `IS_WAYLAND` defaults to false. Confirmed.
5. ~~`display=1024x768` means "not connected"~~: Read the Python source. Lines 137-138: `self.display_width = 1024; self.display_height = 768` — **hardcoded pre-client defaults**. Only updated when a browser connects.

**The actual diagnosis** (direct `ScreenCapture.start_capture()` test):
```
[x11] capture error: shm_attach check: X11Error {
    error_kind: Access,
    error_code: 10,
    extension_name: Some("MIT-SHM"),
    request_name: Some("Attach")
}
```

**Root cause**:
- Pixelflux captures the X root window via **MIT-SHM** (X Shared Memory extension)
- MIT-SHM uses **SysV IPC** (`shmget`/`shmat`) — the client creates a shared memory segment, the X server attaches to it
- X.Org 1.20's `SHMAttach` handler performs a **UID check**: the server process must be the same UID as the segment creator (or root attaching to root's segment)
- Xorg runs as **root** (PID 880), pixelflux runs as **abc** (UID 911)
- `shmat` from root→abc's segment: **BadAccess** (error 10)
- Pixelflux's pre-flight `shm_attach check` fails → `start_capture()` raises RuntimeError → no frames

**Verification matrix**:
| Xorg user | Pixelflux user | Result |
|-----------|---------------|--------|
| root | root | ✅ Frames flow |
| root | abc | ❌ BadAccess |
| abc | abc | ✅ Frames flow |

**Solution**: Run Xorg as the session user (`su abc -s /bin/bash -c "LD_PRELOAD=... Xorg ..."`)
- Matches `selkies-project/docker-selkies-glx-desktop`: "The X server runs as the session user, sharing a virtual terminal it never switches to"
- DRI device permissions: abc already in `videoeyx3`/`video5qpc` groups (k8s NVIDIA plugin)
- Defensive `usermod -aG` added in case groups vary per node

---

### Hurdle 3: CPU Encoding Instead of NVENC (GPU Layer)

**Symptom**:
- Stream works (after Hurdle 2 fix) but at 60ms latency
- `nvidia-smi dmon` shows `enc: 0%` during active streaming
- Selkies log: `Encoder: CPU | Mode: H264`

**Why it happens**:
- Pixelflux has TWO H.264 encoders: built-in x264 (CPU) and NVENC (GPU, via `libnvidia-encode.so` directly — NOT VA-API)
- The encoder selection is controlled by `CaptureSettings.encode_node_index`:
  - `-1` = CPU (default for X11 path)
  - `-2` = auto-detect GPU
  - `N` = specific DRI render node (N = device_number - 128)
- In `selkies.py:3243-3250` (X11 path): `SELKIES_AUTO_GPU` env defaults to **empty string** → falls through to `encode_node_index = -1` (CPU)
- The `AUTO_GPU=true` default only exists in the **Wayland** path (line 3576) — a code asymmetry

**Investigation**:
- `strings pixelflux.so | grep -i nvenc` → `src/encoders/nvenc.rs`, `NvEncodeAPICreateInstance`, `[pixelflux] NVENC API version negotiated:`
- Confirmed: NVENC is a direct CUDA/NVENC API call, not VA-API
- `libnvidia-encode.so` present (mounted by nvidia-container-toolkit)
- `/dev/dri/renderD130` present (toolkit-mounted)
- abc in video group ✅

**Solution**: `SELKIES_AUTO_GPU=true` in the GPU deployment env
- Pixelflux auto-detects: scans `/sys/class/drm/renderD*` → finds NVIDIA PCI device → loads `libnvidia-encode.so` → `NvEncodeAPICreateInstance` → negotiates API v13.0
- No hardcoded renderD number needed (varies per pod/GPU assignment)

**Result**:
```
[NVENC] NVENC API version negotiated: 13.0
[NVENC] Bound to CUDA device via PCI Bus ID: 0000:08:00.0
Stream: H.264 1920x924 @ 30 FPS, CRF 23
P-frames: 595 bytes (vs 3,000 bytes CPU) — 80% bandwidth savings
Encode latency: ~3ms (vs ~12ms CPU)
```

---

## Additional Fixes (Discovered During E2E Testing)

### gnome-terminal: D-Bus machine-id

**Symptom**: `gnome-terminal` fails with "Cannot spawn a message bus without a machine-id"

**Cause**: GNOME Terminal is a D-Bus client (connects to `gnome-terminal-server` via the session bus). The D-Bus session bus requires a valid `/etc/machine-id` (32 hex chars, **no hyphens**). Containers often ship with an empty or missing file.

**Trap**: `dbus-uuidgen --ensure` generates a 36-char UUID **with** hyphens — the wrong format for `/etc/machine-id`. The correct format is `cat /proc/sys/kernel/random/uuid | tr -d "-"`.

**Fix**: Generate at Docker build time (before any service starts).

### WebRTC: TCP-only ingress

**Symptom**: `--mode=webrtc` → browser shows "WebSocket disconnected"

**Cause**: WebRTC media transport uses UDP (SRTP). NRP's Kubernetes ingress (haproxy) is TCP-only. Without a TURN server to relay UDP, the WebRTC data channel can't be established. The selkies-project base container has embedded coTURN; our linuxserver/selkies image does not.

**Resolution**: `--webrtc` flag kept for environments with UDP/TURN availability. NRP default remains `websockets`.

### Hadolint: Multi-line Python in Dockerfile

**Symptom**: CI fails with `Dockerfile:314:1 unexpected 'i'`

**Cause**: A `python3.11 -c "..."` block spanning 12 lines in a RUN instruction. Hadolint's parser doesn't handle multi-line string arguments.

**Fix**: Replaced with a single-line `sed -i` command.

---

## Technology Deep-Dives

### X11 Virtual Terminals (VT) in Containers

The VT subsystem is a Linux kernel feature (`drivers/tty/vt/`) that provides text/graphics modes on physical terminals. X.Org's `xf86OpenConsole()` historically "claims" a VT by:
1. Opening `/dev/ttyN`
2. Issuing `ioctl(VT_GETMODE)` to read current mode
3. Issuing `ioctl(VT_SETMODE, VT_MODE: VT_PROCESS)` to redirect output
4. Issuing `ioctl(KDSETMODE, KD_GRAPHICS)` to switch to graphics

In a container, step 1 "succeeds" (the device node exists via the host's `/dev` mount) but the kernel has no VT line discipline backing it → all subsequent ioctls return `ENOTTY` or `EINVAL`.

The NVIDIA DDX (`nvidia_drv.so`) does NOT use the VT for rendering. It uses the GPU's scanout engine directly (DRI/KMS). The VT interaction is purely legacy console setup — Xorg does it before loading the DDX.

### LD_PRELOAD Symbol Interposition

Glibc's dynamic linker processes `DT_NEEDED` entries in order. `LD_PRELOAD` libraries are loaded FIRST, so their symbols take precedence in the PLT (Procedure Linkage Table) resolution. This means:
- `open()` calls from Xorg resolve to our `open()` in `fakevt.so`
- `ioctl()` calls resolve to our `ioctl()`
- We can inspect arguments, modify behavior, and call `real_open()`/`real_ioctl()` (via `dlsym(RTLD_NEXT, ...)`) when appropriate

**Limitation**: Only works for dynamically-linked symbols via PLT. Statically-linked calls or `syscall()` invocations bypass the interposition. (Xorg uses standard libc calls, so this works.)

### MIT-SHM (X Shared Memory Extension)

The MIT-SHM extension allows a client to:
1. Create a SysV shared memory segment (`shmget`)
2. Tell the X server the segment ID (`XShmAttach` → `SHMAttach` request)
3. The server calls `shmat()` to map the segment into its address space
4. Client writes pixel data to the segment
5. Client tells the server the data is ready (`XShmPutImage` → `PutImage` request)
6. Server reads from the mapped segment

**The UID check**: X.Org 1.20's `SHMAttach` handler calls `shmctl(shmid, IPC_STAT, &buf)` and verifies `buf.__shm_perm.uid == connection->uid`. If the X server runs as root (uid 0) and the client as abc (uid 911), the check fails → `BadAccess`.

Xvfb handles this differently (its `-shmem` flag uses a simpler internal mechanism), which is why M2 (Xvfb) never hit this issue.

### NVENC vs VA-API vs x264

| Encoder | API | Latency | Quality | Availability |
|---------|-----|---------|---------|--------------|
| x264 (built-in) | libx264 (statically linked in pixelflux.so) | ~12ms/frame (1080p) | Excellent | Always (CPU) |
| NVENC | `NvEncodeAPICreateInstance` (libnvidia-encode.so) | ~3ms/frame | Very good | NVIDIA GPU + driver |
| VA-API | `vaCreateContext` (libva + vendor driver) | ~5ms/frame | Good | Intel/AMD/NVIDIA (needs driver) |

Pixelflux 2.0.0 uses **NVENC directly** (not VA-API) for NVIDIA. The `libva`/`libva-x11`/`libva-drm` links in the .so are for video **decoding** (playback), not encoding.

NVENC session limits: GTX 1080 Ti = 2 concurrent sessions, RTX 2080 Ti = 3, A10 = 3, V100 = 2. Our use case (1 selkies stream per pod) is well within limits.

### s6-overlay Process Supervision

s6 manages processes as "services" with dependency chains:
```
svc-xorg (X server)
  └── svc-de (desktop environment: GNOME Shell)
       └── svc-selkies (streaming server)
```

Each service's `run` script is the supervised process. If it exits, s6 restarts it (with backoff via `finish` scripts). The `with-contenv` shebang injects all container ENV vars. `s6-setuidgid` drops privileges (like `setuid` but for s6).

Key property: **environment variables set in the container spec are available to all services via `with-contenv`**. This is how `SELKIES_AUTO_GPU=true` reaches the selkies process without any service script modification.

---

## Investigation Methodology (How We Drilled Down)

### Phase 1: "Xorg Won't Start" (Days 1-2)

**Approach**: Elimination + in-shim tracing

1. Reproduce: `Xorg :1 -config /etc/X11/xorg.conf` → immediate fatal
2. Check Xorg log: `parse_vt_settings: Cannot find a free VT`
3. Try flags: `-keeptty` (unrecognized), `DontVTSwitch` (doesn't help), `-noopendisplay` (doesn't exist)
4. Check `/dev/tty*`: exists but `ioctl` returns `ENOTTY`
5. **Write LD_PRELOAD shim** → intercept `open` for `/dev/ttyN` → redirect to `/dev/null`
6. Xorg gets past `open` but dies on `ioctl` → intercept `ioctl` too
7. **Add debug fprintf to shim** → trace the exact 14 ioctls
8. Discover integer-arg bug (segfault at 0x1) → add pointer guard
9. Xorg starts! NVIDIA DDX loads! glxinfo shows NVIDIA!

**Key technique**: When you can't modify the binary, **intercept and trace**. The shim's debug output was the Rosetta Stone that revealed the non-standard RHEL9 ioctl sequence.

### Phase 2: "Stream Doesn't Work" (Day 3)

**Approach**: Layer-by-layer verification (bottom-up)

1. X server up? ✅ (xdpyinfo: 1920x1080)
2. XCB can connect? ✅ (C test program)
3. Xlib can connect? ✅ (xdpyinfo, xwininfo)
4. GStreamer can capture? ✅ (ximagesrc test)
5. **Pixelflux can capture?** ❌ (direct `start_capture()` → BadAccess)
6. Why? → Read the error: `MIT-SHM Attach` + `error_kind: Access`
7. Hypothesis: cross-UID shmat
8. Test: run capture as root (same UID as Xorg) → ✅ frames
9. Test: run Xorg as abc (same UID as capture) → ✅ frames
10. **Confirmed**: UID mismatch is the cause

**Key technique**: When a complex system (pixelflux .so) fails, **isolate by calling its API directly** with minimal parameters. The `ScreenCapture.start_capture()` test gave us the exact error in 2 seconds vs. hours of log-grepping.

### Phase 3: "GPU Encoding Not Active" (Day 4)

**Approach**: Source reading + binary inspection

1. `nvidia-smi dmon` during stream: `enc: 0%` → CPU encoding
2. Selkies log: `Encoder: CPU` → confirmed
3. Why? → Read `selkies.py:3243`: `encode_node_index = -1` (CPU) unless env set
4. What env? → `SELKIES_AUTO_GPU` (defaults to empty in X11 path)
5. Does pixelflux have NVENC? → `strings pixelflux.so | grep nvenc` → YES
6. Test: `cs.encode_node_index = -2` → NVENC initializes ✅

**Key technique**: `strings` and `nm -D` on a closed-source .so reveal the implementation. We learned pixelflux uses direct NVENC (not VA-API) without reading any Rust source.

### Phase 4: "gnome-terminal Broken" (Day 5)

**Approach**: Error message → system documentation → format mismatch

1. Error: "Cannot spawn a message bus without a machine-id"
2. Check: `/etc/machine-id` empty
3. Generate: `dbus-uuidgen --ensure` → creates 36-char file
4. Still fails: "should contain a hex string of length 32, not length 36"
5. Fix: `cat /proc/sys/kernel/random/uuid | tr -d "-"` → 32 chars
6. Restart D-Bus session → gnome-terminal works

**Key technique**: Read the **error message carefully**. The second attempt's error told us the exact format requirement.

---

## Core Tech Stack

| Layer | Technology | Version | Role |
|-------|-----------|---------|------|
| OS | RHEL 9 (UBI9) | 9.8 | Container base |
| X Server | X.Org | 1.20.11-34.el9_8.3 | Display server |
| GPU Driver | NVIDIA DDX | 580.159.04 / 595.71.05 | Xorg graphics module |
| GPU Compute | CUDA | 13.2 | Application compute |
| GPU Encode | NVENC (libnvidia-encode.so) | API 13.0 | H.264 hardware encoding |
| Desktop | GNOME Shell | 40.10 | Window manager + DE |
| Streaming | Selkies | 348bc4f | WebSocket/WebRTC server |
| Capture | Pixelflux | 2.0.0 | Rust screen capture + encode |
| X Protocol | XCB + MIT-SHM | X11 R7.7 | Screen capture mechanism |
| Process Mgmt | s6-overlay | 3.2.0.2 | Container init + supervision |
| Container Runtime | containerd (k8s) | — | Pod runtime |
| GPU Passthrough | nvidia-container-toolkit | — | /dev/nvidia* + /dev/dri mount |
| Orchestration | Kubernetes (NRP) | 1.28+ | Pod scheduling + networking |
| Ingress | haproxy | — | TCP termination + routing |

---

## Phases Timeline

```
Day 1 (Sep 3)  ┌─────────────────────────────────────────────────┐
               │ PLAN: Port selkies-xorg-config, design M3 path  │
               │ First image build: Xorg starts, VT fatal        │
               └─────────────────────────────────────────────────┘
Day 1-2        ┌─────────────────────────────────────────────────┐
               │ HURDLE 1: fakevt.so shim (20 iterations)        │
               │ - open intercept → ioctl intercept → debug trace│
               │ - Integer-arg segfault → pointer guard          │
               │ - Non-standard RHEL9 ioctls (0x5600, 0x4b3a)   │
               │ RESULT: Xorg + NVIDIA DDX running ✅            │
               └─────────────────────────────────────────────────┘
Day 2-3        ┌─────────────────────────────────────────────────┐
               │ Verification: glxinfo, nvidia-smi, display     │
               │ Multi-node: RTX 2080 Ti, A10, GTX 1080 Ti     │
               │ DISCOVERY: "waiting for stream..." ❌          │
               └─────────────────────────────────────────────────┘
Day 3 (Sep 8)  ┌─────────────────────────────────────────────────┐
               │ HURDLE 2: MIT-SHM investigation                 │
               │ - XCB test ✅, GStreamer ✅, pixelflux ❌       │
               │ - Direct start_capture() → BadAccess            │
               │ - UID matrix test → root cause confirmed        │
               │ RESULT: Xorg-as-abc → frames flow ✅            │
               └─────────────────────────────────────────────────┘
Day 4 (Sep 8)  ┌─────────────────────────────────────────────────┐
               │ HURDLE 3: NVENC activation                      │
               │ - strings/nm on pixelflux.so → NVENC built-in   │
               │ - SELKIES_AUTO_GPU=true → NVENC v13.0 ✅        │
               │ User browser E2E: stream visible, ~60ms         │
               └─────────────────────────────────────────────────┘
Day 5 (Sep 9)  ┌─────────────────────────────────────────────────┐
               │ Performance baseline + regression test          │
               │ WebRTC mode (tested: needs coTURN for NRP)      │
               │ Damage threshold tuning                         │
               │ gnome-terminal machine-id fix                   │
               │ hadolint CI fix                                 │
               │ PR #24 → merged → M3 CLOSED ✅                 │
               └─────────────────────────────────────────────────┘
```

---

## Lessons Learned

### "It Should Just Work" Has Layers

Each layer (kernel→X protocol→GPU encode) had its own failure mode that was invisible from the layers above. The X server was "running" (glxinfo worked) but the capture was silently failing. The capture was "working" (frames flowing) but using CPU instead of GPU. You can't verify a pipeline by checking one stage.

### Closed-Source .so Files Are Still Debuggable

`strings`, `nm -D`, `ldd`, and `objdump` revealed:
- Pixelflux has NVENC built-in (not VA-API)
- It links against XCB + MIT-SHM + DRI3
- The x264 encoder is static (no GStreamer dependency)
- The `encode_node_index` parameter controls CPU vs GPU

No source code was needed to understand the implementation.

### LD_PRELOAD Tracing > Strace in Containers

`strace` was blocked (`ptrace: Operation not permitted` — seccomp). The LD_PRELOAD shim's own `fprintf(stderr, ...)` debug output was the only way to see what syscalls Xorg was making and with what arguments.

### Error Messages Are Precise — Read Them

- `error_kind: Access, error_code: 10, extension_name: Some("MIT-SHM"), request_name: Some("Attach")` → exactly told us which protocol, which request, which error
- `should contain a hex string of length 32, not length 36` → exactly told us the format fix
- `NvEncodeAPICreateInstance failed` → exactly told us which API call

### The Upstream Solves It Differently (And That's the Clue)

`selkies-project/docker-selkies-glx-desktop` runs Xorg as the session user. We didn't know this until we read their README. When your architecture hits a wall, check how the reference implementation handles the same constraint.

### Hardcoded Defaults Hide in Plain Sight

`display=1024x768` in the log looked like "pixelflux can't see the real display." It was actually a pre-client initialization default (Python line 137). Reading the source (even just `grep -n "1024" selkies.py`) would have saved hours.

---

## Outcomes

| Metric | Before (M2) | After (M3) |
|--------|-------------|------------|
| GPU renderer | llvmpipe (CPU) | NVIDIA (OpenGL 4.6) |
| Encoding | x264 CPU (~12ms/frame) | NVENC GPU (~3ms/frame) |
| P-frame size (1080p) | ~3 KB | ~600 B (80% smaller) |
| GPU utilization (active) | 0% | 4-8% (enc + GL) |
| CUDA | ✅ (toolkit) | ✅ (toolkit, unchanged) |
| X server | Xvfb (software) | Xorg + NVIDIA DDX |
| Latency (E2E) | ~60ms | ~55ms (NVENC) / ~45ms (60fps) |
| Container boot (GPU) | ~30s | ~2-4 min (module download) |
| Complexity | Low | Medium (3 custom components) |

---

## Future Work

| Item | Reference | Status |
|------|-----------|--------|
| Xvfb + DRI3/EGL path (shared GPU, N:1) | GH #23 | Planned (4 phases) |
| Wayland + zero-copy DMA-BUF → NVENC | GH #12 | Phase 2 |
| coTURN for WebRTC in NRP | — | Needs infra decision |
| Production image tag (`v6-gpu`) | — | Next release |
| Multi-node perf regression (A10, 2080 Ti) | `scripts/perf-regression.sh` | Scheduled |
| DRI3 zero-copy capture (eliminate SHM) | Pixelflux upstream | Long-term |

---

## Appendix: Key Commands for Reproduction

```bash
# Deploy M3 to NRP
./deploy/nrp/apply-nrp-e2e.sh --gpu-xorg --accept-nrp-utilization

# Verify Xorg + NVIDIA
kubectl exec POD -- su abc -c "DISPLAY=:1 glxinfo | grep 'OpenGL renderer'"

# Test pixelflux capture directly
kubectl exec POD -- su abc -c "DISPLAY=:1 python3 /tmp/test_capture.py"

# Test NVENC
kubectl exec POD -- su abc -c "DISPLAY=:1 python3 /tmp/test_nvenc.py"

# Performance regression
kubectl exec POD -- su abc -c "DISPLAY=:1 bash /scripts/perf-regression.sh"

# GPU monitor
kubectl exec POD -- nvidia-smi dmon -d 1
```

---

*Report generated 2026-09-09. All findings referenced in `memory-bank/findings.md` F75-F78.*
