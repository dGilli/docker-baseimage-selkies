#!/usr/bin/env bash
# upstream-delta.sh — report + gate the SLU fork delta vs the upstream baseline.
#
# Usage: scripts/upstream-delta.sh [base-ref] [head-ref]
#   base-ref  default: upstream/fedora44   (run `git fetch upstream --prune` first)
#   head-ref  default: HEAD
#
# Classification of `git diff --name-status base head`:
#   A  additive (ours)            — always allowed (the additive zone)
#   M  modified upstream file     — must be in [modified] and NOT in [generated]
#   D  baseline file removed      — must be in [intentionally-absent]
#   R  rename                     — reported as NOTE (review manually)
# Plus a mode-change check (a lost +x on an s6 run script = silent service
# death at boot) and the upstream drift count (commits we do not have yet).
#
# Exit: 0 = delta within budget, 1 = violation, 2 = usage/env error.
set -euo pipefail

BASE="${1:-upstream/fedora44}"
HEAD="${2:-HEAD}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ALLOWLIST="$SCRIPT_DIR/delta-allowlist.txt"

if [ ! -f "$ALLOWLIST" ]; then
  echo "FATAL: allowlist not found: $ALLOWLIST" >&2
  exit 2
fi
if ! git cat-file -e "${BASE}^{commit}" 2>/dev/null; then
  echo "FATAL: base ref '$BASE' unknown (run: git fetch upstream --prune)" >&2
  exit 2
fi

modified_allow="$(awk '/^\[modified\]/{f=1;next} /^\[/{f=0} f && NF' "$ALLOWLIST")"
absent_allow="$(awk '/^\[intentionally-absent\]/{f=1;next} /^\[/{f=0} f && NF' "$ALLOWLIST")"
generated="$(awk '/^\[generated\]/{f=1;next} /^\[/{f=0} f && NF' "$ALLOWLIST")"

status="$(git diff --name-status "$BASE" "$HEAD")"
drift="$(git rev-list --count "${HEAD}..${BASE}" 2>/dev/null || echo '?')"

fail=0
add_count=0
mod_count=0

while IFS=$'\t' read -r st path; do
  [ -n "$st" ] || continue
  case "$st" in
    M)
      mod_count=$((mod_count + 1))
      if grep -qxF "$path" <<<"$generated"; then
        echo "VIOLATION: hand-modified GENERATED file (take baseline verbatim): $path"
        fail=1
      elif grep -qxF "$path" <<<"$modified_allow"; then
        echo "ok(M): $path"
      else
        echo "VIOLATION: modified upstream file NOT in delta allowlist [modified]: $path"
        echo "        add it to scripts/delta-allowlist.txt WITH justification in the same PR"
        fail=1
      fi
      ;;
    A)
      add_count=$((add_count + 1))
      ;;
    D)
      if grep -qxF "$path" <<<"$absent_allow"; then
        echo "ok(D): $path (intentionally absent)"
      else
        echo "VIOLATION: baseline file removed but not listed in [intentionally-absent]: $path"
        fail=1
      fi
      ;;
    R*)
      echo "NOTE(rename): $path — review manually"
      ;;
    *)
      echo "NOTE($st): $path — unclassified status, review manually"
      ;;
  esac
done <<<"$status"

modes="$(git diff --summary "$BASE" "$HEAD" | grep 'mode change' || true)"
if [ -n "$modes" ]; then
  echo "VIOLATION: mode change vs baseline (s6 run scripts must stay 0755):"
  echo "$modes"
  fail=1
fi

echo
echo "=== delta report: $HEAD vs $BASE ==="
echo "upstream drift (baseline commits we lack): $drift"
echo "additive (ours): $add_count | modified (allowlist): $mod_count"
if [ "$fail" -eq 0 ]; then
  echo "RESULT: PASS — delta within budget"
else
  echo "RESULT: FAIL — see violations above"
  exit 1
fi
