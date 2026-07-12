"""Generate 64x64 void-and-cluster blue noise and emit a C++ header."""
import numpy as np
from pathlib import Path

N = 64
SIGMA = 1.5
SEED = 42

rng = np.random.default_rng(SEED)

yy, xx = np.mgrid[0:N, 0:N]
dx = np.minimum(xx, N - xx)
dy = np.minimum(yy, N - yy)
kernel = np.exp(-(dx.astype(np.float64) ** 2 + dy.astype(np.float64) ** 2) / (2 * SIGMA**2))
kernel[0, 0] = 0.0


def energy_field(binary: np.ndarray) -> np.ndarray:
    return np.fft.irfft2(np.fft.rfft2(binary.astype(np.float64)) * np.fft.rfft2(kernel), s=(N, N))


ones = np.zeros((N, N), dtype=np.uint8)
initial = N * N // 10
r0, c0 = int(rng.integers(N)), int(rng.integers(N))
ones[r0, c0] = 1
E = energy_field(ones)

for _ in range(initial - 1):
    masked = np.where(ones == 0, E, np.inf)
    flat = int(np.argmin(masked))
    r, c = divmod(flat, N)
    ones[r, c] = 1
    E += np.roll(np.roll(kernel, r, axis=0), c, axis=1)

rank = np.full((N, N), -1, dtype=np.int32)

proto = ones.copy()
tmp = proto.copy()
E = energy_field(tmp)
removal = []
while tmp.any():
    masked = np.where(tmp == 1, E, -np.inf)
    flat = int(np.argmax(masked))
    r, c = divmod(flat, N)
    removal.append((r, c))
    tmp[r, c] = 0
    E -= np.roll(np.roll(kernel, r, axis=0), c, axis=1)

for i, (r, c) in enumerate(reversed(removal)):
    rank[r, c] = i

filled = np.zeros((N, N), dtype=np.uint8)
for r, c in reversed(removal):
    filled[r, c] = 1
E = energy_field(filled)
next_rank = len(removal)
while next_rank < N * N:
    masked = np.where(filled == 0, E, np.inf)
    flat = int(np.argmin(masked))
    r, c = divmod(flat, N)
    rank[r, c] = next_rank
    filled[r, c] = 1
    E += np.roll(np.roll(kernel, r, axis=0), c, axis=1)
    next_rank += 1

assert (rank >= 0).all()
bn = np.clip(((rank.astype(np.float64) + 0.5) / (N * N) * 256.0), 0, 255).astype(np.uint8)

out = Path(__file__).resolve().parents[1] / "src" / "renderer" / "device" / "BlueNoise64.hpp"
lines = [
    "#pragma once",
    "",
    "// Precomputed 64x64 void-and-cluster blue noise (R8 thresholds).",
    "// Generated offline; tiled with wrap for ordered dithering.",
    "",
    "#include <cstdint>",
    "",
    "namespace ASCIIgL {",
    "namespace BlueNoise {",
    "",
    "constexpr unsigned kSize = 64;",
    "constexpr unsigned kByteCount = kSize * kSize;",
    "",
    "inline constexpr std::uint8_t kThresholds[kByteCount] = {",
]
vals = bn.reshape(-1)
for i in range(0, len(vals), 16):
    chunk = vals[i : i + 16]
    lines.append("    " + ", ".join(f"{int(v):3d}" for v in chunk) + ",")
lines.append("};")
lines.append("")
lines.append("}  // namespace BlueNoise")
lines.append("}  // namespace ASCIIgL")
lines.append("")
out.write_text("\n".join(lines), encoding="utf-8")

f = np.abs(np.fft.fftshift(np.fft.fft2(bn.astype(np.float64) / 255.0)))
cy = cx = N // 2
Y, X = np.ogrid[:N, :N]
dist = np.sqrt((X - cx) ** 2 + (Y - cy) ** 2)
low = f[(dist > 0) & (dist < 4)].mean()
mid = f[(dist >= 8) & (dist < 16)].mean()
print(f"Wrote {out} ({out.stat().st_size} bytes)")
print(f"min/max/mean: {int(bn.min())} {int(bn.max())} {float(bn.mean()):.2f}")
print(f"spectral low={low:.3f} mid={mid:.3f} (want mid > low)")
