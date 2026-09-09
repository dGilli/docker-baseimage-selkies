# 2026-09 Task Summary

## Tasks Completed

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
