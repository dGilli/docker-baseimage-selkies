# 180918_proot-apps-yama-scope2

## Objective
GH #6 regression: "the proot app installer from selkies is not working again — when I want to install blender it opens the terminal, downloads something, but the app never shows up." Restore the dashboard app-install → launcher → run chain for **all** NRP node classes, and remove the supply-chain drift that had silently changed the tooling.

## Outcome
- ✅ Root cause: node kernel YAMA `ptrace_scope=2` (admin-only) + container default cap set (no `CAP_SYS_PTRACE`) → every proot call `ptrace(TRACEME): EPERM`. Download step (no proot) succeeded; guest `/install` + `run` died → no launcher entry. Node-dependent (local scope 0, fullerton scope 0/1, UCSC fiona8-0 scope 2) — explains "worked before".
- ✅ Fix merged: PR #25 → `rhel9` as `e38aa7b` (squash; CI green incl. delta gate + hadolint). Image `docker.io/dgilli/selkies-rhel9:papps-fix` = `10c646b3` (+ `:latest`).
- ✅ **Live E2E on the previously-failing node** (fiona8-0, scope 2, GTX 1080 Ti): user browser — install blender (2.3 GB) → `blender-pa.desktop` in launcher + Desktop (abc-owned) → launch → **Blender 5.2.1 LTS, Cycles CUDA device = GTX 1080 Ti, 1726 MiB render allocation** (bonus: proot-apps 0.4.0 `nvidia_binds` GPU passthrough now works) → close → clean teardown (0 chain, GPU released, 0 root-owned files).
- ✅ Local regression: filezilla install/run/render via the full dashboard chain on scope 0 (R1 behavior preserved, bwrap stub intact); forced scope-2 (sudo branch + ownership normalization incl. SIGKILL self-heal) and scope-3 (clean error) branch matrix.
- ✅ Supply-chain pins: proot-apps **0.4.0** (was floating `releases/latest` — had silently moved 0.3.2→0.4.0; 0.5.0 untested), `python-xlib` **0.33** (selkies 348bc4f's git dep repo was deleted upstream — build-breaking 404 found mid-task; PyPI 0.33 is byte-identical to the fork's vendored copy).
- ⏳ Open user decisions: production pin bump (`v5-llvmpipe`=c9 is pre-fix; user is running `:papps-fix`) · proot-apps 0.5.0 evaluation (separate task) · GH #6 closure + board sync.

## Files Modified
- `deploy/nrp/selkies-rhel9.yaml.template` — container `securityContext.capabilities.add: [SYS_PTRACE]` (minimal; PSA-unlabeled ns verified) + header docs (YAMA finding, F55 seccomp-only caveat)
- `root/etc/s6-overlay/s6-rc.d/init-selkies-config/run` (already allowlisted) — proot wrapper: rename real binary to `proot-real`, install YAMA-routing wrapper (`dirname $0`-derived); scope 0/1 direct · scope 2 via passwordless sudo with env passthrough (guest runs as root — shipped proot has no `-u/-g`) · scope 3 clear error; exit-time ownership normalization (rootfs via `-R` parse + `/config/{.local,Desktop,.config,.cache}`) on normal return + TERM/INT trap; re-wrapped after every bootstrap copy
- `Dockerfile` (already allowlisted) — pin `PAPPS_RELEASE=0.4.0` (no more `releases/latest` float); pin `python-xlib==0.33` replacing the dead git URL; `cd selkies-*` → explicit dir name
- `deploy/nrp/apply-nrp-e2e.sh` — PSA step comment + die message (rootful **and** cap-add)
- Upstream generated files taken verbatim (delta-gate requirement; bot commits e9db972/2a00333/3043036/0d5ee26 landed on f44 mid-PR): `Jenkinsfile`, `package_versions.txt`, `.github/workflows/{external_trigger,external_trigger_scheduler,greetings,package_trigger_scheduler}.yml`
- **Not touched**: `selkies-proot` (bwrap-stub loop unchanged), frontend, s6 services, GPU/M3 path

## Patterns Applied
- **Distro-aware shared-tree extension** — wrapper lives in the already-allowlisted `init-selkies-config/run` bootstrap, variant-guarded (`[ -d /proot-apps ]`), no-op on other variants
- **Runtime patching over upstream-file modification** (R1 bwrap-stub pattern) — proot-apps is upstream tarball content; the interception point is the fork-owned init script
- **Delta-gate discipline** — gate failure at PR time was upstream drift on *generated* files (take baseline verbatim), not fork content; budget unchanged (8 modified allowlisted)
- **Live-node diagnosis before code** — the scope-2 facts (YAMA, CapEff, Seccomp, NoNewPrivs, AppArmor, sudo probe, file-cap EPERM test) were all gathered in the failing pod, which shaped the fix (and killed the file-cap option)

## Verification Detail
- In-pod facts (fiona8-0 pre-fix): `ptrace_scope=2`; `CapEff=CapBnd=a80425fb`; agent `CapEff=0`; `Seccomp: 0`; `NoNewPrivs: 0`; `ls -Z` = `?` (AppArmor node); root+abc proot both EPERM; `setcap cap_sys_ptrace+ep` → abc exec `EPERM` (AppArmor mediation); `sudo -n true` OK
- post-fix pod (same node): `CapEff=a80c25fb` (= +`0x80000` SYS_PTRACE); wrapper + `proot-real` ELF pair present; proot-apps `pversion` 0.4.0
- Forced-branch matrix (local scope-0 image, sed-pinned wrapper copies): direct install → host `/config` entry abc-owned; scope-2 guest `id` → `uid=0`; guest-root `/install` → entries root-owned during run → **abc after exit norm()**; SIGKILL residue (4 root files) → **0** after next guest-root run; scope-3 → clean error RC=1
- Blender E2E (user browser, scope-2 pod): launcher entry + desktop shortcut present; `proot-apps run` chain = `st → proot-apps(abc) → proot(abc) → sudo → sh -c(RF/norm) → proot-real(root) → /entrypoint(root)`; `nvidia-smi --query-compute-apps` = 1726 MiB guest allocation during/after Cycles F12; close → chain gone, memory released, 0 root-owned files
- Concurrent-install race observed (operator + user installing blender simultaneously): `tar: Cannot stat` on one side; state converged (DOWNLOADING marker is not a lock) — documented limitation, no code change

## Artifacts
- PR: https://github.com/dGilli/docker-baseimage-selkies/pull/25 (squash-merged `e38aa7b`)
- Image: `docker.io/dgilli/selkies-rhel9:papps-fix` = `10c646b392e2` (+ `:latest`); production pin still `v5-llvmpipe` (c9) pending user decision
- Live pod: `slu-rhel9-e2e-6b48d56875-7l6zw` (fiona8-0.calit2.uci.edu, GTX 1080 Ti 580.159.04, Xorg+NVIDIA DDX)
- Evidence: /tmp/opencode/{papps-qa.png (scope-0 filezilla), smoke-final.png (final-build filezilla), blender-live.png (Blender 5.2.1 + Cycles CUDA device dialog), papps/{032,040,050} (proot-apps tarballs + diffs)}
- Findings: F79 (full record); related: F53 (dashboard badge cosmetic), F54/F56 (bwrap stub, R1 facts), F55 (seccomp-only ptrace assumption), F57 (emptyDir /config)
