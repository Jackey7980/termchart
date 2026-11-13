#!/usr/bin/env python3
"""verify_hevc_tu_sizes.py —— TU 尺寸 4/8/16/32 + 自动档的外部解码门禁。

自包含：直接编译 tools/hevc_tu_probe.cpp 与 src/image/codec/hevc/*.cpp，
不依赖已构建的库；再用真实 ffmpeg 软解逐像素对拍（不是自家编码器自往返）。

矩阵：形态 × QP × 档位(2/3/4/5/0)。每条码流做两层门禁：
  1) 逐像素一致性（最强）：编译期 -DTERMCHART_HEVC_RECON_DUMP 让编码端落盘重建帧，
     与 ffmpeg 解码输出逐字节比较，neq 必须为 0。CABAC 跑偏会立刻暴露，且与 QP
     无关——量化噪声在两端完全相同。
  2) 源 Y/U/V 的 PSNR / maxdiff（辅助，衡量编码质量）：结构性错位表现为
     PSNR<25 / md>120。--force-skip 刻意禁用 RD 择优，QP40 的量化噪声会让 PSNR
     降到 14~24 dB，故该模式只跑第 1 条；语法覆盖由 0/10/26/40 全 QP 保证。

码率断言分三层：
  · base 模式：同一 QP 下自动档总码率必须小于任何单一固定档（原始契约）。
  · skip 模式：收益断言 —— 开启后的自动档必须小于【关闭 skip 时的最佳固定档】。
  · 两种模式都额外跑「物理上界断言」：自动档 SPS 的 MaxTb=32 + depth=1 让每个
    8x8 CU 都要写 split_transform_flag（固定 4x4 档隐式分裂不写），一帧
    (W/8)*(H/8)=128 bin ≈ 16 B 是参数集固有开销；允许 auto 相对同模式最佳固定档
    最多高出这个上界 + 2 B。QP26 skip 档实测多 9 B，正落在其中，不是决策退化。
    为让数字反超去调折扣会把其它 QP 抬高 14~19%（实测），两层 RDO 也抬高 2.1%，
    均已回退，不做为过断言而劣化的调参。

另有一层参数集门禁：`ffmpeg -bsf:v trace_headers` 逐字段核对 RExt 档的
general_profile_idc / sps·pps_range_extension_flag / transform_skip_context_enabled_flag /
log2_max_transform_skip_block_size_minus2，Main 档核对不写 extension。

transform_skip 覆盖：`--transform-skip` 打开自适应 skip（大 TU 档会写 RExt
参数集），`--force-skip` 进一步强制亮度与色度每个候选 TU 都走 skip=1
（专门验证 4/8/16/32 及色度 TU 的 transform_skip_flag=1 残差语法与两段式逆量化）。

用法：
  python tools/verify_hevc_tu_sizes.py            # 完整矩阵
  python tools/verify_hevc_tu_sizes.py --quick   # CI 冒烟（2 形态 x 2 QP）
  python tools/verify_hevc_tu_sizes.py --quick --transform-skip
  python tools/verify_hevc_tu_sizes.py --quick --force-skip

退出码：0 全通过；1 有失败；2 环境不满足（ffmpeg/CXX 缺失，显式失败）。
"""
from __future__ import annotations

import argparse
import math
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROBE_SRC = ROOT / "tools" / "hevc_tu_probe.cpp"
TUS = [2, 3, 4, 5, 0]  # 0 = 自动档
FIXED_TUS = [2, 3, 4, 5]
QPS = [0, 10, 26, 40]
SHAPES = ["flat", "grad", "checker", "hstep", "one8", "dots"]
W, H = 128, 64
PLANES = [("Y", 0, W * H), ("U", W * H, W * H + W * H // 4),
          ("V", W * H + W * H // 4, W * H + W * H // 2)]


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


def verify_parameter_sets(probe: Path, ff: str, tmp: Path, env,
                          skip: bool) -> list[str]:
    """ffmpeg trace_headers 逐字段核对参数集（不只是「能解码」）。

    RExt 档断言 general_profile_idc=4、sps/pps_range_extension_flag=1、
    transform_skip_context_enabled_flag=0、minus2 等于声明的档位；
    Main 档断言 profile=1、transform_skip 关闭、两个 extension 都不写。
    """
    cases = [(tu, 26, "flat") for tu in TUS] if skip else [(2, 26, "flat")]
    problems: list[str] = []
    for tu, qp, shape in cases:
        stem = f"trace_{'on' if skip else 'off'}_tu{tu}"
        f265 = tmp / (stem + ".265")
        prefix = tmp / (stem + "_recon")
        e = dict(env, TERMCHART_HEVC_RECON_PATH=str(prefix))
        enc = run([str(probe), str(qp), str(W), str(H), shape, str(f265), str(tu),
                   "1" if skip else "0"], e)
        if enc.returncode != 0 or not f265.exists():
            problems.append(f"{stem}: 编码失败")
            continue
        tr = run([ff, "-hide_banner", "-loglevel", "info", "-i", str(f265),
                  "-c:v", "copy", "-bsf:v", "trace_headers", "-f", "null", "-"])
        text = tr.stderr.decode(errors="replace")
        rext = skip and (tu == 0 or tu >= 3)
        want = {
            "general_profile_idc": 4 if rext else 1,
            "transform_skip_enabled_flag": 1 if skip else 0,
            "sps_extension_present_flag": 1 if rext else 0,
            "pps_extension_present_flag": 1 if rext else 0,
        }
        if rext:
            want["sps_range_extension_flag"] = 1
            want["pps_range_extension_flag"] = 1
            want["transform_skip_context_enabled_flag"] = 0
            want["log2_max_transform_skip_block_size_minus2"] = (5 if tu == 0 else tu) - 2
        for name, val in want.items():
            got = field_vals(text, name)
            if not got or any(v != val for v in got):
                problems.append(f"{stem}: {name} 期望 {val} 实得 {got}")
        if not rext and field_vals(text, "log2_max_transform_skip_block_size_minus2"):
            problems.append(f"{stem}: Main 档不应出现 pps_range_extension 的 minus2")
    return problems


def encode_decode(probe: Path, ff: str, tmp: Path, shape: str, qp: int, tu: int,
                  env, skip: bool = False,
                  decode: bool = True) -> tuple[int, list[tuple[str, float, int]], int] | None:
    stem = f"tu{tu}_{shape}_{qp}" + ("_ts" if skip else "")
    f265 = tmp / (stem + ".265")
    # 每条码流用独立前缀，避免 dump 的 "ab" 追加把上一帧带进来。
    prefix = tmp / (stem + "_recon")
    e = dict(env, TERMCHART_HEVC_RECON_PATH=str(prefix))
    r = run([str(probe), str(qp), str(W), str(H), shape, str(f265), str(tu),
             "1" if skip else "0"], e)
    if r.returncode != 0 or not f265.exists():
        print(f"  ENC FAIL tu={tu} {shape} QP{qp}: {r.stderr.decode(errors='replace')[:160]}")
        return None
    if not decode:
        return f265.stat().st_size, [], 0
    yuv = tmp / (stem + ".265.yuv")
    dec = tmp / (stem + ".dec.yuv")
    r = run([ff, "-y", "-loglevel", "error", "-skip_loop_filter", "all",
             "-i", str(f265), "-f", "rawvideo", "-pix_fmt", "yuv420p", str(dec)])
    if r.returncode != 0 or not dec.exists():
        print(f"  DEC FAIL tu={tu} {shape} QP{qp}: {r.stderr.decode(errors='replace')[:160]}")
        return None
    src, dst = yuv.read_bytes(), dec.read_bytes()
    if len(dst) < W * H * 3 // 2:
        print(f"  SHORT DEC tu={tu} {shape} QP{qp}: {len(dst)}")
        return None
    stats = []
    for name, a, b in PLANES:
        s, d = src[a:b], dst[a:b]
        se = sum((x - y) ** 2 for x, y in zip(s, d))
        psnr = 99.0 if se == 0 else 10 * math.log10(255 * 255 / (se / len(s)))
        md = max(abs(x - y) for x, y in zip(s, d))
        stats.append((name, psnr, md))
    # recon（编码端重建）与 ffmpeg 解码输出逐字节比较。-1 表示 dump 缺失，
    # 不能当成「一致」放过，否则整条最强门禁会静默失效。
    yp, cp = Path(str(prefix) + ".yuv"), Path(str(prefix) + ".chroma.yuv")
    if not (yp.exists() and cp.exists()):
        return f265.stat().st_size, stats, -1
    recon = yp.read_bytes() + cp.read_bytes()
    n = W * H * 3 // 2
    if len(recon) < n or len(dst) < n:
        return f265.stat().st_size, stats, -1
    neq = sum(a != b for a, b in zip(recon[:n], dst[:n]))
    return f265.stat().st_size, stats, neq


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true", help="CI 冒烟矩阵")
    ap.add_argument("--transform-skip", action="store_true",
                    help="打开 transform_skip（大 TU 档写 RExt 参数集）")
    ap.add_argument("--force-skip", action="store_true",
                    help="强制每个候选 TU 都 transform_skip=1（隐含 --transform-skip）")
    ap.add_argument("--cxx", default=None)
    ap.add_argument("--ffmpeg", default=None)
    args = ap.parse_args()
    skip = args.transform_skip or args.force_skip

    ff = args.ffmpeg or find_ffmpeg()
    if not ff:
        print("FAIL: ffmpeg not found (explicit failure, no silent fallback)")
        return 2
    cxx = find_cxx(args.cxx)
    if not cxx:
        print("FAIL: C++ compiler not found")
        return 2

    # quick 冒烟里，skip 档故意换用 dots（稀疏亮点）——只有它会在自适应档
    # 真正选出 transform_skip=1，从而覆盖 flag=1 的残差语法；base 档仍走 flat/grad。
    shapes = (["flat", "dots"] if skip else SHAPES[:2]) if args.quick else SHAPES
    qps = [10, 26] if args.quick else QPS
    tmp = Path(tempfile.mkdtemp(prefix="termchart_tu_"))
    probe = tmp / ("probe.exe" if sys.platform == "win32" else "probe")
    if not build_probe(cxx, probe):
        print("FAIL: probe build failed")
        return 2

    # 逐像素门禁要用「编码端关 DBF + 解码端 -skip_loop_filter all」这一对，
    # 否则编码端重建与解码端输出本来就会差几个像素。
    env = dict(os.environ, TERMCHART_HEVC_NO_DBLOCK="1")
    if args.force_skip:
        env["TERMCHART_HEVC_FORCE_SKIP"] = "1"
    print(f"MODE: transform_skip={'on' if skip else 'off'}"
          f"{' (forced)' if args.force_skip else ''} "
          f"shapes={shapes} qps={qps}")
    bad = 0
    # 参数集逐字段核对：能解码只证明「语法自洽」，trace_headers 才证明
    # RExt profile 与 log2_max_transform_skip_block_size_minus2 真被写对。
    trace_problems = verify_parameter_sets(probe, ff, tmp, env, skip)
    for p in trace_problems:
        print("  TRACE HEADERS MISMATCH:", p)
    bad += len(trace_problems)
    print("TRACE HEADERS:", "PASS" if not trace_problems else "FAIL",
          f"cases={len(TUS) if skip else 1}")
    # skip 开启时额外编一份「关闭 skip 的固定档」作码率对照（只编码、不解码：
    # 逐像素门禁由主矩阵和 base 模式的独立 run 覆盖）。
    compare_base = skip and not args.force_skip
    totals: dict[int, int] = {t: 0 for t in TUS}
    for qp in qps:
        per_qp = {t: 0 for t in TUS}
        base_per_qp: dict[int, int] = {t: 0 for t in FIXED_TUS}
        for shape in shapes:
            for tu in TUS:
                res = encode_decode(probe, ff, tmp, shape, qp, tu, env, skip)
                if res is None:
                    bad += 1
                    continue
                size, stats, neq = res
                totals[tu] += size
                per_qp[tu] += size
                if neq != 0:
                    bad += 1
                    print(f"  RECON MISMATCH tu={tu} {shape} QP{qp} neq={neq}")
                # --force-skip 刻意关掉 RD 择优，QP40 的量化噪声本就会让 PSNR 降到
                # 14~24 dB；该模式靠更强的逐像素一致性门禁判定语法正确性。
                if not args.force_skip:
                    lim_md = 60 if qp <= 10 else 120
                    ok = all(p >= 25 and md <= lim_md for _, p, md in stats)
                    if not ok:
                        bad += 1
                        print(f"  BAD tu={tu} {shape} QP{qp} {stats}")
                if compare_base and tu in base_per_qp:
                    base = encode_decode(probe, ff, tmp, shape, qp, tu, env, False,
                                         decode=False)
                    if base is None:
                        bad += 1
                    else:
                        base_per_qp[tu] += base[0]
        best = min(per_qp[t] for t in FIXED_TUS)
        if args.force_skip:
            # 强制 skip 把每个 TU 的率失真都拉到「不做变换」，TU 尺寸的分裂收益
            # 被削平，自动档不再有优势——这是 debug 模式自身的性质，不是回归。
            print(f"  QP{qp:2d} totals fixed {[per_qp[t] for t in FIXED_TUS]} "
                  f"auto={per_qp[0]} (auto-better check skipped: forced skip)")
            continue
        # 自动档 SPS 的 MaxTb=32 + max_transform_hierarchy_depth_intra=1 使每个
        # 8x8 CU 都必须显式写 split_transform_flag，而固定 4x4 档是隐式分裂、
        # 不写这些 bin。一帧 (W/8)*(H/8)=128 个 bin ≈ 16 B，这是参数集层面的
        # 固有开销，不是 TU 决策退化。QP26 实测 auto 比同模式固定 4x4 多 9 B，
        # 正落在其中。用「上界 + 2 B 余量」把这条边界变成可验证判据，而不是
        # 为让数字反超去调折扣（实测那样会把其它 QP 总码率抬高 14~19%）。
        cu8 = (W // 8) * (H // 8)
        structural_bound = best + (cu8 + 7) // 8 + 2
        if per_qp[0] > structural_bound:
            print(f"  AUTO EXCEEDS STRUCTURAL BOUND QP{qp}: auto={per_qp[0]} "
                  f"best_fixed={best} bound={structural_bound}")
            bad += 1
        if compare_base:
            base_best = min(base_per_qp.values())
            if per_qp[0] >= base_best:
                print(f"  NO TS GAIN QP{qp}: skip auto={per_qp[0]} "
                      f"best no-skip fixed={base_best}")
                bad += 1
            else:
                print(f"  QP{qp:2d} fixed {[per_qp[t] for t in FIXED_TUS]} "
                      f"auto={per_qp[0]} no-skip best fixed={base_best} "
                      f"(TS gain {100.0 * (per_qp[0] - base_best) / base_best:+.1f}%)")
        elif per_qp[0] >= best:
            print(f"  AUTO NOT BETTER QP{qp}: auto={per_qp[0]} best_fixed={best}")
            bad += 1
        else:
            print(f"  QP{qp:2d} totals fixed {[per_qp[t] for t in FIXED_TUS]} "
                  f"auto={per_qp[0]} (best fixed {best}, "
                  f"{100.0 * (per_qp[0] - best) / best:+.1f}%)")

    best_total = min(totals[t] for t in FIXED_TUS)
    print(f"TOTAL fixed {[totals[t] for t in FIXED_TUS]} auto={totals[0]} "
          f"best_fixed={best_total}")
    print("TU SIZES ORACLE:", "FAIL" if bad else "PASS", f"bad={bad}")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
