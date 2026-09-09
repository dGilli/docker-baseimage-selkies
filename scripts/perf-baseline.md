# M3 GPU Streaming Performance Baseline

**Measured**: 2026-09-09 on NRP (Tesla V100-PCIE-16GB, driver 580.159.04)
**Image**: m3-preview-23 (Xorg + NVIDIA DDX + NVENC)

## Pipeline Latency Breakdown

```
User input → [server] → [network] → [client] → visible
              31ms p50    ~15ms       ~14ms
              (total ~60ms E2E)
```

### Server-side (measured in-pod)
| Stage | Latency | Notes |
|-------|---------|-------|
| X damage event → capture | ~0ms | MIT-SHM is a direct memory read (same-user, F76) |
| Frame scheduling (60fps target) | 0–16.7ms | Depends on where in the frame cycle the damage lands |
| NVENC encode (1080p, CRF 23) | ~3ms | V100, H.264, I420 Limited Range |
| **Total server-side (input→frame out)** | **12–31ms** | p50=31ms, min=12ms (6 samples) |

### Network (WebSocket, NRP ingress)
| Component | Est. | Notes |
|-----------|------|-------|
| Pod → ingress (haproxy) | ~2ms | Same node or intra-cluster |
| Ingress → user (internet) | ~10–20ms | Depends on user location |
| **Total** | **~15ms** | Measured via browser `performance.now()` deltas |

### Client (browser)
| Component | Est. | Notes |
|-----------|------|-------|
| WebSocket receive → decode | ~5ms | H.264 decode (hardware if available) |
| Canvas render → composite | ~5–10ms | Depends on browser GPU |
| **Total** | **~14ms** | |

## Throughput (active screen, full-screen top)
| Metric | Value | Notes |
|--------|-------|-------|
| Encode FPS (sustained) | 8–14 | Damage-based; 60fps target, actual limited by screen change rate |
| Avg frame size (active) | 0.6–21 KB | Depends on change magnitude |
| Peak frame size (keyframe) | 21 KB | I-frame at 1080p |
| Avg bitrate (active) | 0.04–1 Mbps | Very low (damage-based, not full-frame video) |
| GPU encoder utilization | 0–4% | NVENC is fast; not a bottleneck |
| GPU SM utilization | 0–4% | Desktop GL is on the GPU (Xorg DDX) |

## Idle (static desktop)
| Metric | Value | Notes |
|--------|-------|-------|
| Frames in 10s | 6 | Damage tracking: only encodes on change |
| GPU enc | 0% | Correct — no work |
| Bandwidth | ~0 | Minimal keepalive |

## Optimization Levers (for future latency work)

| Lever | Current | Potential | Effort | Impact |
|-------|---------|-----------|--------|--------|
| Target FPS | 30 (default) / 60 (test) | 60 (production) | Low (env var) | Halves frame-wait: 33ms→16ms |
| Transport | WebSocket | WebRTC | High (selkies config) | ~5–10ms lower (UDP, no TCP head-of-line) |
| Encode mode | CRF 23 (quality) | CBR + lower buffer | Low | More consistent frame timing |
| Damage threshold | 15 frames / 20 blocks | Lower (5/10) | Low (env) | Faster response to small changes |
| Resolution | 1080p | 720p (optional) | Low | Faster encode, less bandwidth |
| Client decode | Software (browser) | Hardware (VideoDecoder API) | Medium | ~5ms faster client-side |
| GPU DRI3 zero-copy | MIT-SHM (copy) | DRI3 (zero-copy) | High (pixelflux) | Eliminates capture copy (~1–2ms) |

## Regression Test

Run `scripts/perf-regression.sh` after each image build / deployment:
- Pass: server-side p50 < 50ms, NVENC active, 0 encoding errors
- Warn: p50 50–80ms (acceptable but investigate)
- Fail: p50 > 80ms or NVENC not active (CPU fallback detected)
