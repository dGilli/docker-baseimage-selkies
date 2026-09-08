# Project Brief

**Project**: `slu-docker-rhel-selkies`
**Lineage**: SLU (Saint Louis University) fork of [`linuxserver/docker-baseimage-selkies`](https://github.com/linuxserver/docker-baseimage-selkies) (upstream `master`, release tag `debiantrixie`).

## Vision
Provide full-featured **web-native Linux desktop** container base images. Applications run inside a container and are streamed to a browser via [Selkies](https://github.com/selkies-project) (video: pixelflux, audio: pcmflux, served by NGINX with basic auth). These base images are the foundation for downstream app containers.

## What This Fork Does
- Starts from the upstream Debian-trixie-based image (current state, see baseline commit `eb4e145`).
- **Goal: add RHEL 9 as a supported base image** so downstream SLU workloads can run on an enterprise RHEL-based stream, not only Debian.

## Key Constraints (inherited from upstream)
- No `latest` tag by design — every image is versioned by distro/stream.
- `/config` is the only default-persisted mount; everything else is ephemeral.
- Ships passwordless sudo (user `abc`) for customization.
- `README.md` and `Jenkinsfile` are **generated** — never hand-edit (see `projectRules.md`).

## Success (for RHEL9 work)
A `:rhel9` image that **mirrors upstream `fedora44`** as its maintenance reference — same architecture, shared `root/` tree, and behavior — with the **smallest possible RHEL9-specific delta**. It boots the same desktop stack (X11 today; Wayland is a phase-2 goal), passes the same smoke test, and is built through the same CI/deploy flow.

**The delta is the primary success metric:** the `rhel9` branch = `upstream/fedora44` + a curated milestone series, held to a minimal-allowlisted-delta budget (a few modified upstream files, everything else additive-only), enforced by the delta gate (`scripts/upstream-delta.sh` + `scripts/delta-allowlist.txt`). A small diff keeps re-landing f44 updates cheap. RHEL9 substitutions apply only where forced (SLU-owned UBI9 base + entitled RHEL repos in place of the Fedora base, EL9 package names, `DISABLE_DRI3` divergence, etc.).

> **GH tracking:** every milestone + roadmap item is a GitHub issue on `dGilli/docker-baseimage-selkies`, kept current on the **project board** (https://github.com/users/dGilli/projects/1). Canonical issue↔area mapping + board commands: `toc.md#GH-Tracking`.
