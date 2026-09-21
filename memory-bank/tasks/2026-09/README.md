# 2026-09 Task Summary

## Tasks Completed

### 2026-09-18: proot-apps on YAMA ptrace_scope=2 nodes (GH #6 installer regression)
- Root cause: node YAMA admin-only ptrace + container default caps → all proot calls EPERM (node-dependent: UCSC scope 2 vs fullerton/local scope 0/1)
- Fix: pod `SYS_PTRACE` capability + in-image proot wrapper (scope-2 → sudo route, guest-root with exit-time ownership normalization, scope-3 clear error)
- Pins: proot-apps 0.4.0 (was floating — silent 0.3.2→0.4.0 drift), python-xlib 0.33 (upstream fork repo deleted — build-breaking 404)
- E2E on the previously-failing node: blender install → launcher → Blender 5.2.1 LTS with **Cycles CUDA on GTX 1080 Ti** (nvidia_binds GPU passthrough unlocked); clean exit
- PR #25 merged (`e38aa7b`); image `:papps-fix`; F79
- Production pin bumped to `v6-llvmpipe` = `10c646b392e2` (2026-09-21, user-approved); deploy defaults now use the fixed image
- See: [180918_proot-apps-yama-scope2.md](./180918_proot-apps-yama-scope2.md)

### 2026-09-09: M3 GPU desktop rendering (Xorg + NVIDIA DDX + NVENC)
- 3 container hurdles resolved: VT fatal (fakevt.so), MIT-SHM cross-user (session-user Xorg), NVENC activation (SELKIES_AUTO_GPU)
- Performance baseline: server p50=31ms, E2E ~60ms, NVENC 3ms/frame
- Latency options: --webrtc flag, SELKIES_STREAM_MODE, damage threshold 5/10
- gnome-terminal D-Bus fix (machine-id 32-hex)
- 5 GPU nodes verified (GTX 1080 Ti, RTX 2080 Ti×2, A10×2, V100)
- PR #24 merged → `rhel9` (`eb44df9`); GH #4 + #22 closed
- Findings F75–F78
- See: [090909_m3-gpu-xorg-nvenc.md](./090909_m3-gpu-xorg-nvenc.md)

### 2026-09-01: Reconcile rhel9-dev MVP onto the upstream/fedora44 baseline
- `rhel9` branch = f44 (1c2870d) + curated 8-commit milestone series (phase-1, GNOME, R1, phase-1.5, GPU, journal, tooling, svc-dbus fix)
- 17-row merge resolved; delta budget = 5 modified upstream files (allowlisted) + 96 additive
- Boot matrix caught + fixed real regression: f44's svc-dbus deletion crashes GNOME 40 (F67)
- NRP CPU+GPU smoke PASS (NVENC 13.0, RTX 2080 Ti); PR #2 merged; branch protection active
- Fork maintenance workflow codified: delta allowlist + script + CI (Part B)
- Findings F67–F73; ADR (baseline + history + delta model)
- See: [010901_reconcile-f44-baseline.md](./010901_reconcile-f44-baseline.md)

## Artifacts
- [reconcile-delta-reference.patch](./reconcile-delta-reference.patch) — re-derivation checklist: our 7 master-era shared-file modifications (vs baseline 69f4fc9); the sync-time reference for the 5 allowlisted files
