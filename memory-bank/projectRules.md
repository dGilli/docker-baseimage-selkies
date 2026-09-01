# Project Rules

## Generated Files (never hand-edit)
- `README.md` — rendered from `readme-vars.yml` by the upstream builder (`CONTRIBUTING.md:15-27`). Edit `readme-vars.yml`.
- `Jenkinsfile` — product of the upstream pipeline builder (`CONTRIBUTING.md:18`).
- `package_versions.txt` — package inventory snapshot; regenerate per variant, don't hand-tweak.

## Dockerfile Conventions
- One logical step per `RUN`, chained with `&&`, each step prefixed with an `echo "**** step name ****"` banner (see `Dockerfile:6-19`).
- Package lists alphabetized, one per line, indented under `apt-get install`/equivalent.
- Keep the selkies commit pin in sync across all occurrences (`Dockerfile:19` and `Dockerfile:470`).
- Any change to `Dockerfile` must be mirrored to `Dockerfile.aarch64` (and to the RHEL9 counterparts once they exist), with `arm64v8-` base tag prefixes where applicable.
- Multi-stage: put source-builds in named build stages; only copy artifacts into the runtime stage.
- Cleanup at the end of the runtime stage (purge build deps, clear package cache, `/tmp`) — follow `Dockerfile:559-570`.

## Runtime / s6 Conventions
- App user is `abc` (uid 1000 on Debian; uid 911 on the EL/UBI9 base — PLAN v4 base stage), `HOME=/config`; drop privileges with `s6-setuidgid abc`.
- Scripts under `root/etc/s6-overlay/s6-rc.d/*/run` use `#!/usr/bin/with-contenv bash`.
- Service naming: `init-*` = one-shot (declare `type: oneshot`), `svc-*` = long-running; ordering via `dependencies.d/` markers, membership via `user/contents.d/`.
- User-facing config goes through env vars with `${VAR:-default}`; document new vars in `readme-vars.yml`.

## General
- No fake/mock/stub data in image build scripts or services.
- Prefer extending existing stages/services/files over adding parallel ones (reuse over creation).
- Keep distro-specific code isolated: where a script must differ between Debian and RHEL9, branch on a distro signal (e.g., `/etc/os-release` `ID`) rather than duplicating whole files — except the Dockerfiles themselves, which stay per-variant by convention.
- Commits: one logical change per commit. **Subject style = upstream house style** (user preference, 2026-09-01, F74):
  - **Code commits**: lowercase imperative, concise, **no** `variant:` prefix, **no** milestone framing, **no** em-dash — e.g. `add SLU-owned RHEL UBI9 base + Xvfb/openbox/selkies stack`, `restore svc-dbus system bus for rhel9 to fix GNOME 40 power indicator crashes`. Version numbers may be backticked.
  - **Memory Bank journal commits**: heading line is the FIXED string `Bot Updating Memory Bank` (mimics upstream bot commits like `Bot Updating Package Versions`); body = short summary line, then details.
  - **Sync commits**: `sync: upstream/fedora44 @ <sha> (<n> commits)` (mechanical convention — see build-deployment §Upstream Sync Procedure).
  - **Body**: keep detailed (what/why, findings references, provenance SHAs, task-doc pointer). A subject reword NEVER trims the body.

## Fork Maintenance (rhel9 baseline = upstream/fedora44, since 2026-09-01 — ADR + task 010901)
- **Branch model**: `rhel9` = the only integration branch (f44 + delta); feature branches off `rhel9`, merged via PR (0-approval gate = the record). `rhel9-dev` = frozen MVP revert reference (protected). NO local mirrors of upstream branches — use `upstream/*` refs and always `git fetch --prune` (phantom-ref trap, F73).
- **Delta budget**: modified-upstream files are capped at the allowlist in `scripts/delta-allowlist.txt` (currently 5). Adding a 6th requires editing the allowlist in the SAME PR with justification (the allowlist's git history is the delta audit). RHEL-specific content is ALWAYS additive (`Dockerfile.rhel9`, `root-base/`, `deploy/`, `package_versions_rhel9.txt`, `memory-bank/`, `scripts/`, new `root/` files).
- **Generated files** — take the baseline verbatim, never hand-edit: `Jenkinsfile`, `README.md`, `package_versions.txt`, `jenkins-vars.yml`, `Dockerfile`, `Dockerfile.aarch64`, upstream `.github/workflows/*`. (`.github/workflows/fork-maintenance.yml` is OURS — additive.)
- **Guard predicates**: content-based only — `/etc/redhat-release`, file-exists, binary-exists. `dnf` is NOT a discriminator (present on f44 AND UBI9 — that's why the old DEV_MODE gate vanished). Every shared-tree hunk must be a no-op on the f44 baseline; test guards against trixie/f44/UBI9 fixtures.
- **Sync**: `git merge upstream/fedora44` — never rebase, never `git merge upstream/master` (dual-lineage trap: it will look mechanically clean and drag the Debian tree in), never `-X union`. Sync BEFORE any tagged image build. Commit: `sync: upstream/fedora44 @ <sha> (<n> commits)` + body (image-affecting? paths? QA?). Image-affecting test: `git diff --name-only <old>..<new> -- root/ 'Dockerfile*'` non-empty ⇒ full rebuild + boot matrix + NRP smoke; bot-only ⇒ no rebuild.
- **History**: integration-branch history = curated milestone commits (not raw dev history, not one blob); milestone IDs are USER-ASSIGNED (never invented); `pre-<name>` tags are build anchors; NEVER `git revert -m 1` on a merge that may be re-landed (re-merge trap — silent no-op) — rollback = reset to the pre-tag.
- **GitHub (this fork)**: `gh` commands need `--repo dGilli/docker-baseimage-selkies` (fork-parent context bug, F70); no `gh pr merge --ff` — local `git merge --ff-only` + push (GitHub auto-closes the PR, F70); rulesets API 500s on personal repos — use legacy branch protection (F69); upstream push is DISABLED — never.
