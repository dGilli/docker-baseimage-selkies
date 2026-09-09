# Build & Deployment

## Upstream CI (reference — this fork currently reuses it)
- Jenkins job: `Docker-Pipeline-Builders/docker-baseimage-selkies/<branch>` (`jenkins-vars.yml`: `project_name: docker-baseimage-selkies`, `ls_branch: master`, `release_tag: debiantrixie`).
- Triggers: `.github/workflows/package_trigger_scheduler.yml` (package check builds) and `external_trigger_scheduler.yml` (upstream release triggers), both curl Jenkins.
- Multiarch: `MULTIARCH=true` (`jenkins-vars.yml:19`) — Jenkins builds `Dockerfile` (amd64) and `Dockerfile.aarch64` (arm64v8) and merges manifests. **Per-arch Dockerfile convention** (no buildx matrix in-repo).
- Outputs: `lsio/base/selkies` (dockerhub `lsiobase/selkies`), `ghcr.io/linuxserver/baseimage-selkies`, GitLab, Quay; dev `lsiodev/selkies-base`, PR `lspipepr/selkies-base` (`jenkins-vars.yml:15-17`, `Jenkinsfile:200-250`).

## SLU-local build (current path until CI is sorted)
```bash
# PREFLIGHT (required once per host): entitlement passthrough must work
podman run --rm registry.access.redhat.com/ubi9/ubi dnf repolist   # expect rhel-9-for-x86_64-*

podman build -t dgilli/baseimage-selkies:rhel9-p1-gnome .

# PUSH (phase 1.5 dev registry — Docker Hub, user decision 2026-08-28 / F30):
podman login docker.io            # rootless user (creds live in the rootless auth store)
podman tag dgilli/baseimage-selkies:rhel9-p1-gnome docker.io/dgilli/selkies-rhel9:latest
podman push docker.io/dgilli/selkies-rhel9:latest   # pushes an OCI manifest
# Registry manifest digest (for pinning — local store digest differs, see F30):
TOKEN=$(curl -s "https://auth.docker.io/token?service=registry.docker.io&scope=repository:dgilli/selkies-rhel9:pull" | jq -r .token)
curl -sH "Authorization: Bearer $TOKEN" -H 'Accept: application/vnd.oci.image.manifest.v1+json' \
  https://registry-1.docker.io/v2/dgilli/selkies-rhel9/manifests/latest | sha256sum
```
- **Entitled-host build constraint (PLAN v4)**: the rhel9 variant's vendored base stage (`FROM registry.access.redhat.com/ubi9/ubi`) resolves RHEL packages via podman's automatic entitlement passthrough — builds only succeed on **subscription-registered RHEL hosts**. Runtime needs no entitlement; NRP just pulls the image. (Rationale: `decisions.md` 2026-08-27 SLU-owned base ADR.)
- **Dev registry (F30, resolved 2026-08-28)**: Docker Hub `dgilli/selkies-rhel9:latest` — **now = c9 `a4e303101691`** (reconciled build, 2026-09-01; also pushed as `:c9` for provenance). History: c8 `5c835fb6a147` (manifest `sha256:b70d42e3…`) was latest until the reconcile; c7 = manifest `sha256:46246466…`. Production pin = **`v5-llvmpipe` = c9** (manifest `sha256:6b9ee56628b7ee30d857cd16cf87ac46bf59ffe4ffce08523c8f31b67ae40e26`), bumped 2026-09-01 per user decision — immutable tag sequence v2→v3→v4→v5-llvmpipe; old pins stay in the registry for one-step rollbacks. Milestone tag: dropped per user decision 2026-09-01.
- podman → Docker Hub pushes **OCI** manifests: the local store's docker-format digest ≠ registry manifest digest — always pin/verify by registry digest.
- NRP k8s mapping for this image: `deploy/nrp-selkies-rhel9.yaml` (single port 3000; ws same-origin via nginx `/websocket`; no securityContext — F28).

## Deployment notes
- Image assumes `/config` volume; SSL certs auto-generated into `/config/ssl` on first boot (`init-nginx/run`).
- GPU: pass `--gpus all` + `DRI_NODE`/`DRINODE` envs (upstream behavior; RHEL9 variant must keep the same env contract).
- DinD: run `--privileged` (or mount docker socket) with `START_DOCKER=true` default.
- NRP self-service (2026-09-01, F65–F66):
  - GPU workstation:
    ```bash
    ./deploy/nrp/apply-nrp-e2e.sh \
      --name slu-rhel9-gpu \
      --namespace slu-researchtechnologies-dgilli \
      --gpu \
      --accept-nrp-utilization
    ```
  - Non-GPU workstation:
    ```bash
    ./deploy/nrp/apply-nrp-e2e.sh \
      --name slu-rhel9-cpu \
      --namespace slu-researchtechnologies-dgilli
    ```
  - `apply-nrp-e2e.sh` recreates `dockerhub-dgilli` from local container auth when available; otherwise it reuses the existing namespace pull secret if present and fails only if neither exists.
  - GPU manifests render `nvidia.com/gpu: "1"`, `DISABLE_ZINK=true`, and `strategy.type: Recreate`; CPU manifests render none of those GPU-specific fields.

## Checklist for a new distro variant
1. New branch from the relevant upstream baseline; the branch's MAIN image = `Dockerfile` (upstream design — branch name implies the variant; the old `Dockerfile.<distro>` deviation (F22) is resolved for rhel9, 2026-09-01), mirroring stage architecture (`systemPatterns.md#1`)
2. `Dockerfile.aarch64` if arm is in scope (rhel9: intentionally absent until RHEL9 arm64 work — allowlist [intentionally-absent])
3. Shared `root/` tree — verify every sed/package path exists on the new distro
4. Regenerate `package_versions.txt` equivalent for the variant
5. Smoke test per `testing-patterns.md`
6. `readme-vars.yml`/docs update (new tag row) — requires upstream-style builder or manual var edit

## Upstream Sync Procedure (codified 2026-09-01 — see projectRules §Fork Maintenance, ADR 2026-09-01)
Baseline: `upstream/fedora44`. Cadence: monthly soft ceiling; HARD trigger = before any tagged image build.

```bash
# 0. fetch + prune (phantom-ref trap, F73)
git fetch upstream --prune

# 1. conflict inventory WITHOUT touching the worktree
git merge-tree --write-tree rhel9 upstream/fedora44

# 2. drift count
git rev-list --count rhel9..upstream/fedora44

# 3. image-affecting test (non-empty ⇒ full rebuild + boot matrix + NRP smoke)
git diff --name-only rhel9..upstream/fedora44 -- root/ 'Dockerfile*'

# 4. merge (diff3 — per-command, no config change) and resolve ONLY per
#    scripts/delta-allowlist.txt + the re-derivation guide
#    (memory-bank/tasks/2026-09/reconcile-delta-reference.patch)
git -c merge.conflictStyle=diff3 merge upstream/fedora44

# 5. commit: sync: upstream/fedora44 @ <sha> (<n> commits)
#    body: image-affecting? touched paths? QA result?

# 6. budget gate (CI enforces this on rhel9 — must pass)
scripts/upstream-delta.sh upstream/fedora44 HEAD

# 7. push (always fast-forward — branch is protected: no force, no delete)
git push origin rhel9
```

Never: rebase, `git merge upstream/master` (dual-lineage trap), `-X union`, hand-editing generated files, `git revert -m 1` on a merge that may be re-landed (re-merge trap).

## Reconciled build facts (2026-09-01)
- **c9** = `a4e303101691` — built on this host (entitled RHEL 9.8) from `rhel9@85c605c` via `podman build -f Dockerfile.rhel9`; heavy layers cached from the c8 build (only `COPY /root` tail rebuilt, ~1 min).
- NRP smoke (recon deploys, torn down after): CPU `slu-rhel9-recon-cpu` on exp-19-11.sdsc (Ready, gnome-shell×2, svc-dbus+svc-de stable, ingress 401→200) · GPU `slu-rhel9-recon-gpu` on k8s-haosu-15.sdsc (RTX 2080 Ti driver 595.91.07, Ready 0 restarts, NVENC 13.0 init+bound+stream via in-pod synthetic client `/tmp/opencode/gpu_stream_test.py` — run with `/lsiopy/bin/python`, host has no websockets module).
- Note: NRP free scheduling placed BOTH recon pods on SDSC nodes (not SLU nautilus nodes) — the pool is cross-institution (F59).
