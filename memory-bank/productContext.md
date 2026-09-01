# Product Context

## Users
- **Primary**: SLU IT / downstream container builders who need a RHEL-9-based web-desktop base image (enterprise compliance, standardization on RHEL).
- **End consumers**: Anyone running a web-native Linux desktop in a browser via these base images.

## Upstream Relationship
- Source of truth for behavior: [`linuxserver/docker-baseimage-selkies`](https://github.com/linuxserver/docker-baseimage-selkies).
- This repo tracks upstream `master` / `debiantrixie` release tag (see `jenkins-vars.yml:7`).
- Support from upstream is "Reasonable Endeavours" only (README). SLU owns the RHEL9 delta.

## Market / Why RHEL9
- SLU workloads standardize on RHEL. A Debian-only base image blocks reuse.
- RHEL9 brings `dnf`/`rpm` packaging, different package availability, and glibc/locale handling that must be reconciled with the desktop stack (Xorg, Wayland/labwc, pulseaudio, GPU/VA-API, NVIDIA).

## Out of Scope (unless requested)
- Changing Selkies/pixelflux upstream behavior.
- Introducing a `latest` tag.
- Replacing the s6-overlay service model.

## Open Product Questions (for RHEL9)
All resolved 2026-08-27 (see `decisions.md` 2026-08-27 ADRs + `activeContext.md#PLAN-v4`):
1. ~~Target tag name~~ → `rhel9`; x86_64 first, aarch64 later (user decision).
2. ~~RHEL9 base: ubi9 vs full RHEL repos~~ → SLU-owned `base` stage: `registry.access.redhat.com/ubi9/ubi` (digest-pinned) + entitled RHEL repos via host entitlement passthrough + EPEL9 delta (v4 ADR; baseimage-el:9 rejected — deprecated + Oracle-repo-based).
3. ~~Parity level~~ → reduced core set first (desktop + streaming); phase-2 deferrals: DinD, GPU/Zink, proot-apps, pelorus, Wayland.

## Future Roadmap (user-tracked 2026-09-01)
These are future-plan items requested by the user. They are tracked here so they survive compaction, but they are **not** yet scoped as active milestones.

1. **Implement GPU use for desktop rendering**
   - Current state: GPU nodes use NVENC for streaming, but desktop rendering remains Xvfb + llvmpipe.
   - Future goal: render the desktop itself on the GPU.
   - Related: M3 (deferred), F29, F58, F60, F64, F65.
   - Likely scope: real Xorg + NVIDIA DDX / EGL / GL path, driver-mismatch handling, GNOME/mutter validation, dynamic resolution, fallback to llvmpipe.

2. **Improve project CLI and UX**
   - Current state: `deploy/nrp/apply-nrp-e2e.sh` works, but the workflow is still script-heavy and operator-facing.
   - Future goal: make common project operations easier, clearer, and less error-prone.
   - Likely scope: workstation lifecycle commands (start/stop/status/logs/teardown), better GPU/CPU naming, clearer prompts, safer defaults, dry-run/plan output, and consistent error messages.

3. **Fix selkies menu app installer**
   - Current state: R1 steps 1–2 shipped proot-apps and FileZilla install/run, but the user explicitly tracked the selkies menu/app installer as needing repair.
   - Future goal: make the desktop menu / app installer flow reliable and user-friendly.
   - Related: R1, F53–F56, proot-apps dashboard chain.
   - Likely scope: reproduce the broken installer path, fix app discovery/install state, handle RHEL9/proot constraints, and verify from the UI rather than only shell commands.

4. **Implement proper project and fork maintenance workflow**
   - Current state: the repo is a SLU/user fork tracking upstream `linuxserver/docker-baseimage-selkies`, with RHEL9-specific divergence and local memory-bank governance.
   - Future goal: define a repeatable maintenance workflow for upstream tracking, branching, releases, and SLU-specific deltas.
   - Likely scope: upstream update strategy, branch model, changelog/release notes, generated-file rules, tagging policy, PR/review expectations, and memory-bank update requirements.

5. **Implement proper SLU image registry**
   - Current state: production image is private Docker Hub `docker.io/dgilli/selkies-rhel9:v4-llvmpipe`; NRP uses `dockerhub-dgilli` pull secrets.
   - Future goal: move to a proper SLU-owned registry and release workflow.
   - Related: F30, F66, `build-deployment.md`.
   - Likely scope: SLU registry selection, repository naming, production tags, image retention/signing/scanning, pull-secret automation, and migration away from personal Docker Hub.

6. **Docs, docs, docs**
   - Current state: memory bank tracks a lot of operational knowledge, but user-facing and maintainer-facing documentation still needs expansion.
   - Future goal: make the project understandable and operable without archaeology.
   - Likely scope: README, architecture overview, RHEL9 variant notes, NRP deployment guide, GPU troubleshooting guide, registry/maintenance docs, CLI reference, and runbooks for common operations.
