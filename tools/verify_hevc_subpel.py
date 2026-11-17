#!/usr/bin/env python3
"""verify_hevc_subpel.py —— HEVC 1/4 像素插值的外部解码门禁。

复用 verify_hevc_inter.py 的机制：自编译 probe（含 interp.cpp），用
TERMCHART_HEVC_INTER_FORCE=mvf:dx,dy 强制每个 P 片 CU 使用【指定的 1/4 像素 MV】，
让编码端用自己的 8-tap/4-tap 滤波器算预测并落盘重建，再用 ffmpeg 真解码
（解码端用它自己的滤波器算同一个 MV 的预测），逐帧 × Y/U/V 比对，neq 必须为 0。

这是验证「我们的插值 ≈ ffmpeg hevcdsp 插值」的最强 oracle：MV 由码流承载，
两端 MV 完全一致，唯一分叉点就是插值系数/取整/边界；任何不一致立即暴露首差异坐标。

分数 MV 覆盖：
  · 1/4  (mvf:1,0) / 3/4 (mvf:3,0) —— 8-tap 非对称系数；
  · 1/2  (mvf:2,0) —————————————— 8-tap 对称系数；
  · 双向分数 (mvf:2,2) / (mvf:1,3) —— hv 两趟滤波；
  · 色度分数 (mvf:1,0 的色度 frac = (1 & 7)=1) —— 4-tap。

用法：
  python tools/verify_hevc_subpel.py            # 全部分数 MV × 2 QP
  python tools/verify_hevc_subpel.py --quick    # 只 mvf:2,0 × QP26
退出码：0 全通过；1 有失败；2 环境不满足。
"""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROBE_SRC = ROOT / "tools" / "hevc_inter_probe.cpp"
# (名字, "dx,dy" in 1/4 luma units)
FRACS = [("h quarter", "1,0"), ("h half", "2,0"), ("h three-quarter", "3,0"),
         ("hv half", "2,2"), ("hv quarter", "1,3"), ("v half", "0,2")]
QPS = [10, 26]
W, H, NF = 128, 64, 3
SHAPE = "mixed"


def find_ffmpeg() -> str | None:
    exe = shutil.which("ffmpeg")
    if exe:
        return exe
    p = Path(os.environ.get("APPDATA", "")) / (
        "Roaming/Python/Python314/site-packages/imageio_ffmpeg/binaries")
    hits = sorted(p.glob("ffmpeg*.exe")) if p.exists() else []
    return str(hits[0]) if hits else None


def find_cxx() -> str | None:
    if os.environ.get("CXX"):
        return os.environ["CXX"]
    for name in ("c++", "g++", "clang++"):
        p = shutil.which(name)
        if p:
            return p
    for p in (Path("C:/Program Files/mingw64/bin/c++.exe"),):
        if p.exists():
            return str(p)
    return None


def run(args, env=None):
    return subprocess.run(args, cwd=ROOT, capture_output=True, env=env)


def build_probe(cxx, out):
    srcs = [str(PROBE_SRC), "src/image/hevc_encode.cpp",
            "src/image/codec/bitstream/ep_nal.cpp"]
    srcs += [str(p) for p in sorted((ROOT / "src/image/codec/hevc").glob("*.cpp"))]
    r = run([cxx, "-std=c++20", "-O2", "-DTERMCHART_STATIC",
             "-DTERMCHART_HEVC_RECON_DUMP", "-Iinclude", "-Isrc", "-Isrc/image",
             "-o", str(out), *srcs])
    if r.returncode != 0:
        sys.stderr.write(r.stderr.decode(errors="replace")[-3000:])
    return r.returncode == 0


def one_case(probe, ff, tmp, frac_name, mv, qp):
    stem = f"sub_{frac_name.replace(' ','_')}_q{qp}"
    f265 = tmp / (stem + ".265")
    reco = tmp / (stem + ".reco.yuv")
    dec = tmp / (stem + ".dec.yuv")
    env = dict(os.environ, TERMCHART_HEVC_RECO_YUV=str(reco),
               TERMCHART_HEVC_INTER_FORCE=f"mvf:{mv}")
    enc = run([str(probe), str(qp), str(W), str(H), SHAPE, str(f265), str(NF), "1"], env)
    if enc.returncode != 0 or not f265.exists():
        return None, f"ENC FAIL {frac_name} QP{qp}: {enc.stderr.decode(errors='replace')[:120]}"
    if not reco.exists():
        return None, f"RECON MISSING {frac_name} QP{qp}"
    d = run([ff, "-y", "-loglevel", "error", "-i", str(f265),
             "-f", "rawvideo", "-pix_fmt", "yuv420p", str(dec)])
    if d.returncode != 0 or not dec.exists():
        return None, f"DEC FAIL {frac_name} QP{qp}: {d.stderr.decode(errors='replace')[:120]}"
    rdat, ddat = reco.read_bytes(), dec.read_bytes()
    fsize = W * H * 3 // 2
    if len(rdat) != NF * fsize or len(ddat) != NF * fsize:
        return None, f"SIZE {frac_name} QP{qp}: reco={len(rdat)} dec={len(ddat)}"
    planes = (("Y", 0, W * H), ("U", W * H, W * H // 4),
              ("V", W * H + W * H // 4, W * H // 4))
    total = 0
    first = None
    per_plane = {"Y": 0, "U": 0, "V": 0}
    for f in range(NF):
        base = f * fsize
        for pname, off, size in planes:
            for i in range(base + off, base + off + size):
                if rdat[i] != ddat[i]:
                    total += 1
                    per_plane[pname] += 1
                    if first is None:
                        k = i - (base + off)
                        first = (f, pname, k % W, k // W, rdat[i], ddat[i])
    return {"neq": total, "per_plane": per_plane, "first": first,
            "bytes": f265.stat().st_size}, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--cxx")
    args = ap.parse_args()

    ff = find_ffmpeg()
    cxx = find_cxx() if not args.cxx else args.cxx
    if not ff or not cxx:
        print(f"环境不满足: ffmpeg={ff} cxx={cxx}", file=sys.stderr)
        return 2

    fracs = [("h half", "2,0")] if args.quick else FRACS
    qps = [26] if args.quick else QPS

    with tempfile.TemporaryDirectory(prefix="tc_subpel_") as td:
        tmp = Path(td)
        probe = tmp / "hevc_subpel_probe"
        if not build_probe(cxx, probe):
            print("PROBE 编译失败", file=sys.stderr)
            return 2
        bad = 0
        for frac_name, mv in fracs:
            for qp in qps:
                res, err = one_case(probe, ff, tmp, frac_name, mv, qp)
                if err:
                    print(f"  {err}")
                    bad += 1
                    continue
                tag = "OK " if res["neq"] == 0 else "BAD"
                pp = res['per_plane']
                print(f"  {tag} {frac_name:16s} mvf:{mv:5s} QP{qp:<2d} "
                      f"neq={res['neq']:6d} [Y={pp['Y']} U={pp['U']} V={pp['V']}] "
                      f"bytes={res['bytes']:6d} first={res['first']}")
                if res["neq"] != 0:
                    bad += 1
        print(f"HEVC SUBPEL ORACLE: {'PASS' if bad == 0 else 'FAIL'} bad={bad}")
        return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
