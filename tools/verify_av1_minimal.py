#!/usr/bin/env python3
"""AV1 一期门禁：自建 IVF 经 ffmpeg/libdav1d 真解码，逐像素 recon==dec。

分两步：
  1. 8x8 平坦帧（一期最小纵切）——锁住最小语法与平坦重建；
  2. 8x16 双块帧全模式扫描——块 A 带残差给出**非平坦上边界**，块 B skip=1 纯预测，
     于是块 B 的每个像素都直接检验方向预测（含 angle_delta 的 3 度步进）、
     边界滤波强度/类型与上采样决策是否与解码器一致。
"""
from __future__ import annotations

import glob
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TMP = ROOT / "build" / "av1_oracle"
PROBE_SRC = ROOT / "tools" / "av1_min_probe.cpp"
INTRA_PROBE_SRC = ROOT / "tools" / "av1_intra_probe.cpp"
INTER_PROBE_SRC = ROOT / "tools" / "av1_inter_probe.cpp"

# 8x16 4:2:0 一帧 = 128 亮度 + 32 U + 32 V。
FRAME_BYTES_8X16 = 192


def run(cmd: list[str]) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, capture_output=True)


def build_probe() -> tuple[Path, Path, Path] | None:
    TMP.mkdir(parents=True, exist_ok=True)
    probe = TMP / "probe.exe"
    intra_probe = TMP / "probe_stage4.exe"
    inter_probe = TMP / "probe_stage5.exe"
    srcs = [str(ROOT / "src/image/av1_encode.cpp")]
    srcs += sorted(glob.glob(str(ROOT / "src/image/codec/av1/*/*.cpp")))
    srcs.append(str(ROOT / "src/image/codec/bitstream/ep_nal.cpp"))
    base = ["g++", "-std=c++20", "-O0", "-DTERMCHART_STATIC",
            "-I" + str(ROOT / "include"), "-I" + str(ROOT / "src"),
            "-I" + str(ROOT / "src/image")]
    lib = str(ROOT / "build/ninja/libtermchart.a")
    for source, output in ((PROBE_SRC, probe), (INTRA_PROBE_SRC, intra_probe),
                           (INTER_PROBE_SRC, inter_probe)):
        r = run([*base, "-o", str(output), str(source), *srcs, lib])
        if r.returncode:
            sys.stderr.write(r.stderr.decode(errors="replace")[-2000:])
            return None
    return probe, intra_probe, inter_probe


def decode(probe: Path, ivf: Path, expected: Path, ffmpeg_out: Path,
           extra: list[str], ffmpeg: str) -> subprocess.CompletedProcess:
    r = run([str(probe), str(ivf), str(expected), *extra])
    if r.returncode:
        return r
    return run([ffmpeg, "-hide_banner", "-loglevel", "error", "-i", str(ivf),
                "-f", "rawvideo", "-pix_fmt", "yuv420p", "-y", str(ffmpeg_out)])


def compare(got: bytes, want: bytes, limit: int) -> tuple[int, list[int]]:
    n = min(limit, len(got), len(want))
    diff = [i for i in range(n) if got[i] != want[i]]
    if len(got) != len(want):
        diff.append(-1)
    return n, diff


def main() -> int:
    probes = build_probe()
    if probes is None:
        return 2
    probe, intra_probe, inter_probe = probes

    ivf = TMP / "frame.ivf"
    expected = TMP / "expected.yuv"
    yuv = TMP / "frame.yuv"

    # 1) 8x8 平坦帧
    r = decode(probe, ivf, expected, yuv, [], "ffmpeg")
    if r.returncode or not yuv.exists():
        sys.stderr.write(r.stderr.decode(errors="replace")[-800:])
        return 4
    got, want = yuv.read_bytes(), expected.read_bytes()
    n, diff = compare(got, want, 96)
    ok = not diff and len(got) == 96
    print(f"AV1 MINIMAL ORACLE (8x8 flat): {'PASS' if ok else 'FAIL'} "
          f"mismatches={len(diff)} first={diff[:8]}")
    if not ok:
        print("  got :", " ".join(f"{b:02x}" for b in got[:64]))
        print("  want:", " ".join(f"{b:02x}" for b in want[:64]))
        return 1

    # 2) 8x16 全模式 / 角度扫描
    modes = list(range(13))
    failures = []
    checked = 0
    for mode in modes:
        angles = range(-3, 4) if 1 <= mode <= 8 else [0]
        for angle in angles:
            checked += 1
            r = decode(probe, ivf, expected, yuv, [str(mode), str(angle)],
                       "ffmpeg")
            if r.returncode or not yuv.exists():
                failures.append(f"mode={mode} angle={angle} DECODE_FAIL")
                continue
            got, want = yuv.read_bytes(), expected.read_bytes()
            n, diff = compare(got, want, FRAME_BYTES_8X16)
            if diff:
                row = diff[0] // 8 if diff[0] >= 0 else -1
                failures.append(f"mode={mode} angle={angle} "
                                f"mismatches={len(diff)} first_row={row} "
                                f"first={diff[:6]}")
    ok = not failures
    print(f"AV1 INTRA ORACLE (8x16 modes/angles): {'PASS' if ok else 'FAIL'} "
          f"checked={checked} failed={len(failures)}")
    for line in failures[:12]:
        print("  " + line)

    # 3) 阶段四：filter intra 五档 + CFL 联合符号/幅值组合
    stage4 = [(0, 0, f, 0, 0, 0, 0) for f in range(5)]
    stage4 += [(9, 0, -1, 13, u, v, signs)  # 13 = CFL_PRED
               for signs in range(8)
               for u, v in ((0, 3), (5, 0), (7, 9), (15, 15))]
    stage4_failures = []
    for case in stage4:
        r = decode(intra_probe, ivf, expected, yuv,
                   [str(x) for x in case], "ffmpeg")
        if r.returncode or not yuv.exists():
            stage4_failures.append(f"case={case} DECODE_FAIL")
            continue
        got, want = yuv.read_bytes(), expected.read_bytes()
        _, diff = compare(got, want, FRAME_BYTES_8X16)
        if diff:
            stage4_failures.append(f"case={case} mismatches={len(diff)} "
                                   f"first={diff[:6]}")
    ok = ok and not stage4_failures
    print(f"AV1 INTRA ORACLE (stage4 filter-intra/CFL): "
          f"{'PASS' if not stage4_failures else 'FAIL'} "
          f"checked={len(stage4)} failed={len(stage4_failures)}")
    for line in stage4_failures[:12]:
        print("  " + line)

    # 4) 阶段五：KEY + INTER_FRAME 两帧码流，参考帧重建逐像素对拍
    r = run([str(inter_probe), "0", "0", str(ivf), str(expected)])
    if r.returncode == 0:
        r = run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-i", str(ivf),
                 "-f", "rawvideo", "-pix_fmt", "yuv420p", "-y", str(yuv)])
    inter_ok = False
    if r.returncode == 0 and yuv.exists():
        got, want = yuv.read_bytes(), expected.read_bytes()
        _, diff = compare(got, want, FRAME_BYTES_8X16)
        inter_ok = not diff and len(got) == FRAME_BYTES_8X16
        if not inter_ok:
            print(f"  inter mismatches={len(diff)} first={diff[:6]} "
                  f"got={len(got)} want={len(want)}")
    else:
        sys.stderr.write(r.stderr.decode(errors="replace")[-400:])
    ok = ok and inter_ok
    print(f"AV1 INTER ORACLE (KEY+INTER 2 frames): "
          f"{'PASS' if inter_ok else 'FAIL'}")
    print(f"AV1 MINIMAL ORACLE: {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
