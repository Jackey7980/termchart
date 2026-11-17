#!/usr/bin/env python3
"""verify_hevc_inter.py —— H.265 帧间（P 片）编码器的外部解码门禁。

自包含：直接编译 tools/hevc_inter_probe.cpp 与 src/image/codec/hevc/*.cpp，
不依赖已构建的库；再用真实 ffmpeg 软解逐像素对拍（不是自家编码器自往返）。

这是帧间档（阶段二）最强的正确性门禁 —— 逐像素一致性：
  编译期 -DTERMCHART_HEVC_RECON_DUMP + 运行期 TERMCHART_HEVC_RECO_YUV 让编码端
  把【自己的重建帧】落盘（YUV420，多帧拼接），与 ffmpeg 解码输出逐字节比较，
  必须 neq=0。CABAC / 语法 / 候选 / MV / MC 任何一端偏一角都会立刻暴露，且与 QP
  无关 —— 量化噪声在两端完全相同。按帧 × 平面（Y/U/V）分别统计，打印首差异坐标。

为什么要专门固化这个门禁（血泪教训）：
  · 帧间档出过 4 个「两端自洽但与 ffmpeg 分叉」的隐性 bug，全部只靠这条门禁抓到：
    ① pred_mode_flag 发反（HEVC 0=INTER / 1=INTRA）；
    ② merge_idx 是首 bin 走 ctx、后续 bypass 的 unary，不是全 ctx；
    ③ merge 候选必须「成对去重」而非对全部已推候选去重；
    ④ Z 序块可用性必须用 4x4 最小块的 Z-scan 地址（min_tb_addr_zs）比较，
       「yN>y0 即不可用」是错的（CU 按 Z 序递归，(8,8) 先于 (16,0) 解码）。
  · 只比第一帧 Y 会漏掉全部 P 帧错误 —— 必须逐帧、逐 Y/U/V 三平面比。
  · U/V 恒定（如 code 形态恒 128）会掩盖 MV 错误，不能据此判定 MV 正确。
  · 调试开关 TERMCHART_HEVC_INTER_TRACE（每 CU 决策）、TERMCHART_HEVC_DUMP_BINS
    很有用；force 隔离法（全 skip / 全 mvp0 / 全 intra）可快速定位模式相关 bug。

矩阵：形态 × QP × 尺寸 × 帧数，外加跨 CTB 与多帧累积两组专项：
  · 形态 still(shi 理想全 skip) / shift2 / mixed(skip+AMVP 混合) / round(双向位移)
    / fade(亮度台阶，inter 拼不过退帧内) / noise(帧内为主) / flat / code(8x8 块常量，
    任何错位拷贝都落到别的块常量，可直接反推解码端在 CU 做了什么)。
  · 尺寸含跨 CTB 行（256x128 / 192x64），专门覆盖 A0/B0 的 Z-scan 可用性分支。

另有一层参数集门禁：`ffmpeg -bsf:v trace_headers` 核对 slice_type 序列
（首帧=2 IDR、其后全=1 P）、尺寸、SAO 关闭，证明 P 片真编进码流而非 IDR 伪装。

用法：
  python tools/verify_hevc_inter.py             # 完整矩阵
  python tools/verify_hevc_inter.py --quick     # CI 冒烟（mixed/round × QP10/26）

退出码：0 全通过；1 有失败；2 环境不满足（ffmpeg/CXX 缺失，显式失败）。
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
SHAPES = ["still", "shift2", "mixed", "round", "fade", "noise", "flat", "code"]
QPS = [0, 10, 26, 40]
W, H = 128, 64
NFRAMES = 5
# 首差异坐标最多打印这么多个，避免刷屏
MAX_DIFF_SAMPLE = 8


def find_ffmpeg() -> str | None:
    exe = shutil.which("ffmpeg")
    if exe:
        return exe
    p = Path(os.environ.get("APPDATA", "")) / (
        "Roaming/Python/Python314/site-packages/imageio_ffmpeg/binaries")
    hits = sorted(p.glob("ffmpeg*.exe")) if p.exists() else []
    return str(hits[0]) if hits else None


def find_cxx(explicit: str | None) -> str | None:
    if explicit:
        return explicit
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


def run(args: list[str], env=None):
    return subprocess.run(args, cwd=ROOT, capture_output=True, env=env)


def build_probe(cxx: str, out: Path) -> bool:
    srcs = [str(PROBE_SRC), "src/image/hevc_encode.cpp",
            "src/image/codec/bitstream/ep_nal.cpp"]
    srcs += [str(p) for p in sorted((ROOT / "src/image/codec/hevc").glob("*.cpp"))]
    r = run([cxx, "-std=c++20", "-O2", "-DTERMCHART_STATIC",
             "-DTERMCHART_HEVC_RECON_DUMP",
             "-Iinclude", "-Isrc", "-Isrc/image", "-o", str(out), *srcs])
    if r.returncode != 0:
        sys.stderr.write(r.stderr.decode(errors="replace")[-2000:])
    return r.returncode == 0


def field_vals(text: str, name: str) -> list[int]:
    """从 trace_headers 的「字段名 ... = 值」行里取出所有出现值。"""
    out: list[int] = []
    for line in text.splitlines():
        if name in line and " = " in line:
            try:
                out.append(int(line.rsplit(" = ", 1)[1].strip()))
            except ValueError:
                pass
    return out


def inter_case(probe: Path, ff: str, tmp: Path, shape: str, qp: int,
               w: int, h: int, nf: int) -> dict | None:
    """编一条多帧流，用 ffmpeg 真解；返回逐帧×平面的 neq 明细与首差异坐标。

    TERMCHART_HEVC_RECO_YUV 让编码端落盘【自己的重建帧】（YUV420，全帧拼接），
    这是最强门禁 —— 与 ffmpeg 解码输出逐字节比较。dump 缺失一律当失败，绝不
    当成「一致」放过，否则整条门禁会静默失效。
    """
    stem = f"i_{shape}_q{qp}_{w}x{h}_f{nf}"
    f265 = tmp / (stem + ".265")
    reco = tmp / (stem + ".reco.yuv")
    dec = tmp / (stem + ".dec.yuv")
    env = dict(os.environ, TERMCHART_HEVC_RECO_YUV=str(reco))
    enc = run([str(probe), str(qp), str(w), str(h), shape, str(f265), str(nf), "1"], env)
    if enc.returncode != 0 or not f265.exists():
        print(f"  ENC FAIL {shape} QP{qp} {w}x{h} f{nf}: "
              f"{enc.stderr.decode(errors='replace')[:160]}")
        return None
    if not reco.exists():
        print(f"  RECON DUMP MISSING {shape} QP{qp} {w}x{h} f{nf} "
              f"(check TERMCHART_HEVC_RECO_YUV support)")
        return None
    d = run([ff, "-y", "-loglevel", "error", "-i", str(f265),
             "-f", "rawvideo", "-pix_fmt", "yuv420p", str(dec)])
    if d.returncode != 0 or not dec.exists():
        print(f"  DEC FAIL {shape} QP{qp} {w}x{h} f{nf}: "
              f"{d.stderr.decode(errors='replace')[:160]}")
        return None

    rdat, ddat = reco.read_bytes(), dec.read_bytes()
    fsize = w * h * 3 // 2
    expect = nf * fsize
    if len(rdat) != expect or len(ddat) != expect:
        print(f"  SIZE MISMATCH {shape} QP{qp} {w}x{h} f{nf}: "
              f"reco={len(rdat)} dec={len(ddat)} expected={expect} "
              f"(解码帧数不足？P 片可能解不出)")
        return None

    planes = (("Y", 0, w * h), ("U", w * h, w * h // 4),
              ("V", w * h + w * h // 4, w * h // 4))
    per_frame: list[dict[str, int]] = []
    first: tuple[int, str, int, int, int, int] | None = None
    total = 0
    for f in range(nf):
        base = f * fsize
        row: dict[str, int] = {}
        for plane, off, size in planes:
            a0 = base + off
            n = 0
            for i in range(a0, a0 + size):
                if rdat[i] != ddat[i]:
                    n += 1
                    if first is None:
                        k = i - a0
                        first = (f, plane, k % w, k // w, rdat[i], ddat[i])
            row[plane] = n
            total += n
        per_frame.append(row)
    return {"neq": total, "per_frame": per_frame, "first": first,
            "bytes": f265.stat().st_size}


def verify_trace_headers(ff: str, f265: Path, w: int, h: int, nf: int) -> list[str]:
    """参数集门禁：证明码流里首帧是 IDR、其后全是 P 片，尺寸/SAO 正确。"""
    tr = run([ff, "-hide_banner", "-loglevel", "info", "-i", str(f265),
              "-c:v", "copy", "-bsf:v", "trace_headers", "-f", "null", "-"])
    text = tr.stderr.decode(errors="replace")
    problems: list[str] = []
    st = field_vals(text, "slice_type")
    # 期望 [2(IDR), 1(P), 1(P), ...]。trace_headers 有时只在参数集层重复字段，
    # 故取前 nf 个即可；不足说明没解出足够片。
    if len(st) < nf:
        problems.append(f"trace_headers: slice_type 只有 {len(st)} 个 < 帧数 {nf}")
    want = [2] + [1] * (nf - 1)
    if st[:nf] != want:
        problems.append(f"trace_headers: slice_type 期望 {want} 实得 {st[:nf]}")
    for name, val in (("pic_width_in_luma_samples", w),
                      ("pic_height_in_luma_samples", h)):
        got = field_vals(text, name)
        if not got or any(v != val for v in got):
            problems.append(f"trace_headers: {name} 期望 {val} 实得 {got}")
    sao = field_vals(text, "sample_adaptive_offset_enabled_flag")
    if sao and any(v != 0 for v in sao):
        problems.append(f"trace_headers: 帧间档 SAO 应为 0 实得 {sao}")
    poc = field_vals(text, "slice_pic_order_cnt_lsb")
    if len(poc) < nf - 1:
        problems.append(f"trace_headers: P 片 POC 只有 {len(poc)} 个 < {nf - 1}")
    return problems


def check_case(probe: Path, ff: str, tmp: Path, shape: str, qp: int,
               w: int, h: int, nf: int) -> tuple[bool, int]:
    res = inter_case(probe, ff, tmp, shape, qp, w, h, nf)
    label = f"{shape:8s} QP{qp:2d} {w:3d}x{h:<3d} f{nf}"
    if res is None:
        print(f"  {label}  CASE FAIL")
        return False, 1
    if res["neq"] != 0:
        f, plane, x, y, rv, dv = res["first"]
        frames = " ".join(
            f"{i}:Y{r['Y']}/U{r['U']}/V{r['V']}"
            for i, r in enumerate(res["per_frame"]) if r["Y"] or r["U"] or r["V"])
        print(f"  {label}  RECON MISMATCH neq={res['neq']}  "
              f"first=(f{f},{plane},{x},{y},reco={rv},dec={dv})  [{frames}]")
        return False, 1
    print(f"  {label}  OK  bytes={res['bytes']:5d}  neq=0  frames={nf}")
    return True, 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true", help="CI 冒烟（mixed/round × QP10/26）")
    ap.add_argument("--cxx", default=None)
    ap.add_argument("--ffmpeg", default=None)
    args = ap.parse_args()

    ff = args.ffmpeg or find_ffmpeg()
    if not ff:
        print("FAIL: ffmpeg not found (explicit failure, no silent fallback)")
        return 2
    cxx = find_cxx(args.cxx)
    if not cxx:
        print("FAIL: C++ compiler not found")
        return 2

    tmp = Path(tempfile.mkdtemp(prefix="termchart_inter_"))
    probe = tmp / ("probe.exe" if sys.platform == "win32" else "probe")
    if not build_probe(cxx, probe):
        print("FAIL: probe build failed")
        return 2

    shapes = ["mixed", "round"] if args.quick else SHAPES
    qps = [10, 26] if args.quick else QPS

    bad = 0
    cases = 0
    print(f"MODE: inter oracle shapes={shapes} qps={qps} "
          f"size={W}x{H} frames={NFRAMES}"
          f"{' (quick)' if args.quick else ''}")

    # 主矩阵
    for shape in shapes:
        for qp in qps:
            ok, nbad = check_case(probe, ff, tmp, shape, qp, W, H, NFRAMES)
            cases += 1
            bad += nbad

    # 跨 CTB 行专项：A0/B0 的 Z-scan 可用性分支只在多 CTB 行走到（曾在此出 bug）
    if not args.quick:
        print("MULTI-CTB (Z-scan availability):")
        for w, h in ((256, 128), (192, 64)):
            for shape in ("mixed", "round"):
                ok, nbad = check_case(probe, ff, tmp, shape, 26, w, h, 5)
                cases += 1
                bad += nbad
        print("LONG SEQUENCE (error accumulation):")
        for shape in ("mixed", "round"):
            ok, nbad = check_case(probe, ff, tmp, shape, 26, W, H, 8)
            cases += 1
            bad += nbad

    # 参数集门禁：证明 slice_type 序列确实是 1×IDR + (n-1)×P
    trace_stem = f"i_mixed_q26_{W}x{H}_f{NFRAMES}"
    tp = verify_trace_headers(ff, tmp / (trace_stem + ".265"), W, H, NFRAMES)
    for p in tp:
        print("  TRACE HEADERS MISMATCH:", p)
    bad += len(tp)
    print("TRACE HEADERS:", "PASS" if not tp else "FAIL", "cases=1")

    print("HEVC INTER ORACLE:", "FAIL" if bad else "PASS",
          f"cases={cases} bad={bad}")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
