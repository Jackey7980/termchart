#!/usr/bin/env python3
"""HEVC 多参考帧（L0 = POC-1, POC-2）真实解码门禁。

与 verify_hevc_inter.py 同源：自编译 probe → 编码端重建落盘 → ffmpeg 软解 →
逐帧 × Y/U/V 逐字节比对（neq 必须为 0）。本脚本额外断言：

  ① RPS 真的声明了两个负参考（trace_headers 的 num_negative_pics = [1,2,2,...]，
     首 P 帧只有 POC-1，其后两参考）；
  ② ref_idx=1 真的被运动搜索选中（cycle2 亮度交替形态下，参考前一帧必然
     叉开 80 亮度、参考前两帧同相位 MV=0，ref=1 应占多数）；
  ③ cycle2 形态下多参考码流显著小于单参考（证明不是"空壳语法"）。

默认关闭 hevc_multi_ref 的既有整数/亚像素基线由 verify_hevc_inter.py 与
verify_hevc_subpel.py 分别守护，本脚本只验证开启后的新路径。
"""
import argparse
import os
import re
import sys
import tempfile
from pathlib import Path

import verify_hevc_inter as base

W, H = base.W, base.H
NF = 8
SHAPES = ["cycle2", "mixed", "round", "shift2", "noise", "code"]
QPS = [10, 26]


def encode(probe, ff, tmp, shape, qp, multiref, frames=NF, trace=False, prefix="mr"):
    stem = f"{prefix}_{shape}_q{qp}"
    f265 = tmp / (stem + ".265")
    reco = tmp / (stem + ".r.yuv")
    dec = tmp / (stem + ".d.yuv")
    env = dict(os.environ, TERMCHART_HEVC_RECO_YUV=str(reco))
    if multiref:
        env["TERMCHART_HEVC_INTER_MULTIREF"] = "1"
    if trace:
        env["TERMCHART_HEVC_INTER_TRACE"] = "1"
    e = base.run([str(probe), str(qp), str(W), str(H), shape, str(f265), str(frames), "1"], env)
    if e.returncode or not f265.exists() or not reco.exists():
        return None, None, f"encode failed: {e.stderr.decode(errors='replace')[:120]}"
    d = base.run([ff, "-y", "-loglevel", "error", "-i", str(f265), "-f", "rawvideo",
                  "-pix_fmt", "yuv420p", str(dec)])
    if d.returncode or not dec.exists():
        return None, None, f"decode failed: {d.stderr.decode(errors='replace')[:120]}"
    r, c = reco.read_bytes(), dec.read_bytes()
    neq = sum(1 for a, b in zip(r, c) if a != b)
    return (neq, f265.stat().st_size), e.stderr.decode(errors="replace"), None


def trace_rps(ff, f265):
    tr = base.run([ff, "-hide_banner", "-loglevel", "info", "-i", str(f265),
                   "-c:v", "copy", "-bsf:v", "trace_headers", "-f", "null", "-"])
    return base.field_vals(tr.stderr.decode(errors="replace"), "num_negative_pics")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true", help="CI 冒烟（cycle2/mixed × QP26）")
    ap.add_argument("--cxx", default=None)
    ap.add_argument("--ffmpeg", default=None)
    args = ap.parse_args()

    ff = args.ffmpeg or base.find_ffmpeg()
    cxx = base.find_cxx(args.cxx)
    if not ff or not cxx:
        print("FAIL: ffmpeg / C++ compiler not found (fail-loud)")
        return 2

    tmp = Path(tempfile.mkdtemp(prefix="termchart_multiref_"))
    probe = tmp / ("probe.exe" if sys.platform == "win32" else "probe")
    if not base.build_probe(cxx, probe):
        print("FAIL: probe build failed")
        return 2

    shapes = ["cycle2", "mixed"] if args.quick else SHAPES
    qps = [26] if args.quick else QPS
    bad = cases = 0
    print(f"MODE: multiref oracle shapes={shapes} qps={qps} size={W}x{H} "
          f"frames={NF}{' (quick)' if args.quick else ''}")

    for shape in shapes:
        for qp in qps:
            res, _, err = encode(probe, ff, tmp, shape, qp, True)
            cases += 1
            if err:
                print(f"  {shape} QP{qp} FAIL {err}")
                bad += 1
                continue
            neq, size = res
            status = "OK" if neq == 0 else "BAD"
            print(f"  {shape:8s} QP{qp:<2d} {status} neq={neq:6d} bytes={size}")
            if neq:
                bad += 1

    # ① RPS 两个负参考（首 P 帧 1 个、其后 2 个）
    res, _, err = encode(probe, ff, tmp, "mixed", 26, True)
    if err:
        bad += 1
    else:
        stem = f"mr_mixed_q26"
        rps = trace_rps(ff, tmp / (stem + ".265"))
        ok = len(rps) >= NF - 1 and rps[0] == 1 and all(v == 2 for v in rps[1:NF - 1])
        print(f"RPS negative pics: {rps[:NF-1]} -> {'PASS' if ok else 'FAIL'}")
        if not ok:
            bad += 1

    # ② ref_idx=1 真被选中 + ③ 码率收益（cycle2 亮度交替）
    res, trace, err = encode(probe, ff, tmp, "cycle2", 26, True, trace=True)
    ref_counts = {}
    for line in trace.splitlines():
        m = re.search(r"\bref=(\d+)", line)
        if m:
            ref_counts[m.group(1)] = ref_counts.get(m.group(1), 0) + 1
    n_ref1 = ref_counts.get("1", 0)
    total = sum(ref_counts.values())
    ratio = n_ref1 / total if total else 0.0
    print(f"ref_idx=1 selected: {n_ref1}/{total} ({ratio:.1%})")
    if ratio < 0.5:
        print("FAIL: ref_idx=1 not meaningfully selected (multi-ref is a shell)")
        bad += 1

    if not args.quick:
        mr_neq, mr_size = res if not err else (1, 1 << 30)
        _, _, _ = encode(probe, ff, tmp, "cycle2", 26, False, prefix="sr")
        sr_f = tmp / "sr_cycle2_q26.265"
        sr_size = sr_f.stat().st_size if sr_f.exists() else 0
        print(f"cycle2 bitrate: single={sr_size} multi={mr_size}")
        if not (sr_size and mr_size < sr_size):
            print("FAIL: multi-ref bitrate not smaller on alternating-luma content")
            bad += 1

    print("HEVC MULTIREF ORACLE:", "FAIL" if bad else "PASS", f"cases={cases} bad={bad}")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
