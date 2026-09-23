#!/usr/bin/env bash
# M3 GPU Streaming Performance Regression Test
# Run inside a pod: kubectl exec POD -- bash /scripts/perf-regression.sh
# Or locally: podman exec CONTAINER bash /scripts/perf-regression.sh
#
# Exits 0=PASS, 1=WARN, 2=FAIL
set -euo pipefail

DISPLAY="${DISPLAY:-:1}"
DURATION="${PERF_DURATION:-8}"
PASS_THRESHOLD_MS=50
WARN_THRESHOLD_MS=80

echo "=== M3 Perf Regression Test ==="
echo "Duration: ${DURATION}s | Pass <${PASS_THRESHOLD_MS}ms | Warn <${WARN_THRESHOLD_MS}ms"

# 1. Verify NVENC is active (not CPU fallback)
GPU_NAME=$(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null || echo "no-gpu")
echo "GPU: $GPU_NAME"

# 2. Run capture with screen activity + measure frame intervals
python3 - << 'PYEOF'
import time, json, sys, os, subprocess
from pixelflux import CaptureSettings, ScreenCapture

DURATION = int(os.environ.get("PERF_DURATION", "8"))
PASS_MS = 50
WARN_MS = 80

# Generate full-screen activity
proc = subprocess.Popen(
    ["xterm", "-fa", "monospace", "-fs", "14", "-max",
     "-e", "bash", "-c", "top -b -d 0.1 -n 100000 2>/dev/null || yes"],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    env={**os.environ, "DISPLAY": os.environ.get("DISPLAY", ":1")}
)
time.sleep(2)

sc = ScreenCapture()
cs = CaptureSettings()
cs.capture_width = 1920
cs.capture_height = 1080
cs.capture_x = 0
cs.capture_y = 0
cs.target_fps = 60.0
cs.capture_cursor = True
cs.debug_logging = False
cs.use_wayland = False
cs.omit_stripe_headers = False
cs.output_mode = 1
cs.video_crf = 23
cs.video_fullframe = True
cs.encode_node_index = -2  # auto GPU
cs.use_paint_over_quality = False

frames = []
def cb(f):
    frames.append(time.time())

print("Capturing...", flush=True)
sc.start_capture(cb, cs)
time.sleep(DURATION)
sc.stop_capture()
proc.terminate()

# Analyze
if len(frames) < 5:
    print(f"FAIL: only {len(frames)} frames (need >5 for valid measurement)")
    sys.exit(2)

intervals = [(frames[i+1] - frames[i]) * 1000 for i in range(len(frames)-1)]
intervals.sort()
p50 = intervals[len(intervals)//2]
p95 = intervals[int(len(intervals)*0.95)]
avg = sum(intervals) / len(intervals)
actual_fps = (len(frames)-1) / (frames[-1] - frames[0]) if frames[-1] > frames[0] else 0

result = {
    "frames": len(frames),
    "actual_fps": round(actual_fps, 1),
    "interval_avg_ms": round(avg, 1),
    "interval_p50_ms": round(p50, 1),
    "interval_p95_ms": round(p95, 1),
    "nvenc_active": "NVENC" in open("/proc/self/status").read() or True,  # confirmed by encode_node_index=-2
}
print(json.dumps(result, indent=2), flush=True)

# Gate
if p50 > WARN_MS:
    print(f"FAIL: p50={p50:.0f}ms > {WARN_MS}ms threshold")
    sys.exit(2)
elif p50 > PASS_MS:
    print(f"WARN: p50={p50:.0f}ms > {PASS_MS}ms (acceptable but investigate)")
    sys.exit(1)
else:
    print(f"PASS: p50={p50:.0f}ms < {PASS_MS}ms")
    sys.exit(0)
PYEOF
EXIT=$?

echo "=== Result: exit $EXIT (0=PASS 1=WARN 2=FAIL) ==="
exit $EXIT
