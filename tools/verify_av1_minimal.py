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
OBMC_PROBE_SRC = ROOT / "tools" / "av1_obmc_probe.cpp"
PARTITION_PROBE_SRC = ROOT / "tools" / "av1_partition_probe.cpp"
DEBLOCK_PROBE_SRC = ROOT / "tools" / "av1_deblock_probe.cpp"
FRAME_PROBE_SRC = ROOT / "tools" / "av1_frame_probe.cpp"

# 8x16 4:2:0 一帧 = 128 亮度 + 32 U + 32 V。
FRAME_BYTES_8X16 = 192
# 16x16 4:2:0 一帧 = 256 亮度 + 64 U + 64 V。
FRAME_BYTES_16X16 = 384


def run(cmd: list[str]) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, capture_output=True)


def build_probe() -> tuple[Path, Path, Path, Path, Path, Path, Path] | None:
    TMP.mkdir(parents=True, exist_ok=True)
    probe = TMP / "probe.exe"
    intra_probe = TMP / "probe_stage4.exe"
    inter_probe = TMP / "probe_stage5.exe"
    obmc_probe = TMP / "probe_obmc.exe"
    partition_probe = TMP / "probe_partition.exe"
    deblock_probe = TMP / "probe_deblock.exe"
    frame_probe = TMP / "probe_frame.exe"
    srcs = [str(ROOT / "src/image/av1_encode.cpp")]
    srcs += sorted(glob.glob(str(ROOT / "src/image/codec/av1/*/*.cpp")))
    srcs.append(str(ROOT / "src/image/codec/bitstream/ep_nal.cpp"))
    base = ["g++", "-std=c++20", "-O0", "-DTERMCHART_STATIC",
            "-I" + str(ROOT / "include"), "-I" + str(ROOT / "src"),
            "-I" + str(ROOT / "src/image")]
    lib = str(ROOT / "build/ninja/libtermchart.a")
    for source, output in ((PROBE_SRC, probe), (INTRA_PROBE_SRC, intra_probe),
                           (INTER_PROBE_SRC, inter_probe),
                           (OBMC_PROBE_SRC, obmc_probe),
                           (PARTITION_PROBE_SRC, partition_probe),
                           (DEBLOCK_PROBE_SRC, deblock_probe),
                           (FRAME_PROBE_SRC, frame_probe)):
        r = run([*base, "-o", str(output), str(source), *srcs, lib])
        if r.returncode:
            sys.stderr.write(r.stderr.decode(errors="replace")[-2000:])
            return None
    return (probe, intra_probe, inter_probe, obmc_probe, partition_probe,
            deblock_probe, frame_probe)


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
    (probe, intra_probe, inter_probe, obmc_probe, partition_probe,
     deblock_probe, frame_probe) = probes

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

    # 4b) 阶段五亚像素：逐块 NEWMV 扫全 1/8 像素相位（亮度 + 4:2:0 色度一起对拍）。
    # 探针第 5 参数 >= 0 时走 NEWMV 语法，MV 由码流里的 MVD 差分携带。
    subpel = [(0, 0, 0)]
    subpel += [(f, 0, 0) for f in range(1, 8)]
    subpel += [(0, f, 0) for f in range(1, 8)]
    subpel += [(f, f, 0) for f in range(1, 8)]
    subpel += [(3, -5, 0), (7, 9, 0), (8, 3, 0), (13, -7, 0), (5, 14, 0),
               (-9, 5, 0), (2, 7, 0), (9, -9, 0)]
    # 其余三种帧级插值滤波器（平滑 / 锐利 / 双线性）各扫一组相位。
    for filt in (1, 2, 3):
        subpel += [(f, 0, filt) for f in (0, 1, 3, 4, 7)]
        subpel += [(0, f, filt) for f in (0, 1, 3, 4, 7)]
        subpel += [(3, -5, filt), (filt, filt, filt), (9, -9, filt)]
    subpel_failures = []
    for mv_x, mv_y, filt in subpel:
        r = run([str(inter_probe), str(mv_x), str(mv_y), str(ivf),
                 str(expected), "0", str(filt)])
        if r.returncode == 0:
            r = run(["ffmpeg", "-hide_banner", "-loglevel", "error",
                     "-i", str(ivf), "-f", "rawvideo", "-pix_fmt", "yuv420p",
                     "-y", str(yuv)])
        if r.returncode or not yuv.exists():
            subpel_failures.append(f"mv=({mv_x},{mv_y}) f={filt} DECODE_FAIL")
            continue
        got, want = yuv.read_bytes(), expected.read_bytes()
        _, diff = compare(got, want, FRAME_BYTES_8X16)
        if diff or len(got) != FRAME_BYTES_8X16:
            subpel_failures.append(f"mv=({mv_x},{mv_y}) f={filt} "
                                   f"mismatches={len(diff)} first={diff[:6]}")
    ok = ok and not subpel_failures
    print(f"AV1 INTER ORACLE (stage5 NEWMV subpel): "
          f"{'PASS' if not subpel_failures else 'FAIL'} "
          f"checked={len(subpel)} failed={len(subpel_failures)}")
    for line in subpel_failures[:12]:
        print("  " + line)

    # 4c) NEWMV + 亮度残差：预测之外的残差符号（txtp_inter3 布尔 + eob/系数）
    # 也必须被解码端认账，残差叠加后的重建要逐像素一致。
    res_cases = [(0, 0), (3, 5), (-7, 2), (9, -9), (1, 14)]
    res_failures = []
    for mv_x, mv_y in res_cases:
        r = run([str(inter_probe), str(mv_x), str(mv_y), str(ivf),
                 str(expected), "0", "0", "1"])
        if r.returncode == 0:
            r = run(["ffmpeg", "-hide_banner", "-loglevel", "error",
                     "-i", str(ivf), "-f", "rawvideo", "-pix_fmt", "yuv420p",
                     "-y", str(yuv)])
        if r.returncode or not yuv.exists():
            res_failures.append(f"mv=({mv_x},{mv_y}) DECODE_FAIL")
            continue
        got, want = yuv.read_bytes(), expected.read_bytes()
        _, diff = compare(got, want, FRAME_BYTES_8X16)
        if diff or len(got) != FRAME_BYTES_8X16:
            res_failures.append(f"mv=({mv_x},{mv_y}) "
                                f"mismatches={len(diff)} first={diff[:6]}")
    ok = ok and not res_failures
    print(f"AV1 INTER ORACLE (stage5 NEWMV residual): "
          f"{'PASS' if not res_failures else 'FAIL'} "
          f"checked={len(res_cases)} failed={len(res_failures)}")
    for line in res_failures[:12]:
        print("  " + line)

    # 阶段六：using_qmatrix + 8x8 权重表。KEY 与 INTER 两帧都带非零残差，
    # 权重错一组、帧头 4 bit 选错、反量化 (dq*qm+16)>>5 错位都会逐像素暴露。
    qm_failures = []
    for qm in (0, 3, 7, 11, 14):
        args = [str(inter_probe), "3", "0", str(ivf), str(expected),
                "1", "0", "1"] + ["0"] * 17 + [str(qm)]
        r = run(args)
        if r.returncode == 0:
            r = run(["ffmpeg", "-hide_banner", "-loglevel", "error",
                     "-i", str(ivf), "-f", "rawvideo", "-pix_fmt", "yuv420p",
                     "-y", str(yuv)])
        if r.returncode or not yuv.exists():
            qm_failures.append(f"qm={qm} DECODE_FAIL")
            continue
        got, want = yuv.read_bytes(), expected.read_bytes()
        _, diff = compare(got, want, FRAME_BYTES_8X16)
        if diff or len(got) != FRAME_BYTES_8X16:
            qm_failures.append(f"qm={qm} mismatches={len(diff)} first={diff[:6]}")
    ok = ok and not qm_failures
    print(f"AV1 TRANSFORM ORACLE (stage6 qmatrix): "
          f"{'PASS' if not qm_failures else 'FAIL'} checked=5 failed={len(qm_failures)}")
    for line in qm_failures[:12]:
        print("  " + line)

    # 4d) 帧级全局运动：帧头把槽 0 的 GMV 类型写成 TRANSLATION，mat[0]/mat[1] 用
    # subexp 承载平移量（high_precision_mv ⇒ bits=9 / shift=13）。块仍走 GLOBALMV，
    # 因此块 MV 完全由帧头 GMV 决定 —— 这套 subexp 与 dav1d 区间划分必须逐位一致。
    gmv_cases = [(0, 0), (1, 0), (0, 1), (3, 5), (-7, 2), (9, -9), (1, 14),
                 (8, 0), (-1, -1), (15, -11), (-15, 15), (31, -3)]
    gmv_failures = []
    for mv_x, mv_y in gmv_cases:
        r = run([str(inter_probe), str(mv_x), str(mv_y), str(ivf),
                 str(expected), "-1", "0", "0", "1"])
        if r.returncode == 0:
            r = run(["ffmpeg", "-hide_banner", "-loglevel", "error",
                     "-i", str(ivf), "-f", "rawvideo", "-pix_fmt", "yuv420p",
                     "-y", str(yuv)])
        if r.returncode or not yuv.exists():
            gmv_failures.append(f"mv=({mv_x},{mv_y}) DECODE_FAIL")
            continue
        got, want = yuv.read_bytes(), expected.read_bytes()
        _, diff = compare(got, want, FRAME_BYTES_8X16)
        if diff or len(got) != FRAME_BYTES_8X16:
            gmv_failures.append(f"mv=({mv_x},{mv_y}) "
                                f"mismatches={len(diff)} first={diff[:6]}")
    ok = ok and not gmv_failures
    print(f"AV1 INTER ORACLE (stage5 frame GMV translation): "
          f"{'PASS' if not gmv_failures else 'FAIL'} "
          f"checked={len(gmv_cases)} failed={len(gmv_failures)}")
    for line in gmv_failures[:12]:
        print("  " + line)

    # 4e) 双参考复合（AVG）：帧头打开 switchable_comp_refs 后，块语法多一个
    # is_compound 位；ref[0]=LAST（槽 0 的 TRANSLATION）、ref[1]=GOLDEN（槽 1 的
    # IDENTITY），两路预测按 (a+b+1)>>1 平均，与单参考平移预测可区分。
    comp_cases = [(0, 0), (1, 0), (3, 5), (-7, 2), (9, -9), (1, 14), (-15, 15)]
    comp_failures = []
    for mv_x, mv_y in comp_cases:
        r = run([str(inter_probe), str(mv_x), str(mv_y), str(ivf),
                 str(expected), "-1", "0", "0", "1", "1"])
        if r.returncode == 0:
            r = run(["ffmpeg", "-hide_banner", "-loglevel", "error",
                     "-i", str(ivf), "-f", "rawvideo", "-pix_fmt", "yuv420p",
                     "-y", str(yuv)])
        if r.returncode or not yuv.exists():
            comp_failures.append(f"mv=({mv_x},{mv_y}) DECODE_FAIL")
            continue
        got, want = yuv.read_bytes(), expected.read_bytes()
        _, diff = compare(got, want, FRAME_BYTES_8X16)
        if diff or len(got) != FRAME_BYTES_8X16:
            comp_failures.append(f"mv=({mv_x},{mv_y}) "
                                 f"mismatches={len(diff)} first={diff[:6]}")
    ok = ok and not comp_failures
    print(f"AV1 INTER ORACLE (stage5 compound AVG): "
          f"{'PASS' if not comp_failures else 'FAIL'} "
          f"checked={len(comp_cases)} failed={len(comp_failures)}")
    for line in comp_failures[:12]:
        print("  " + line)

    # 4f) 楔形掩码复合：16 个 wedge_index × 2 个 sign 的子集，逐像素验证
    # comp_group_idx / compound_type / wedge_idx / wedge_sign 语法与掩码混合。
    wedge_cases = [(0, 0, 0, 0), (3, 5, 0, 1), (-7, 2, 1, 0), (9, -9, 1, 1),
                   (1, 14, 2, 0), (-15, 15, 6, 1), (5, 3, 10, 0), (-3, -6, 15, 1)]
    wedge_failures = []
    for mv_x, mv_y, idx, sign in wedge_cases:
        r = run([str(inter_probe), str(mv_x), str(mv_y), str(ivf),
                 str(expected), "-1", "0", "0", "1", "1", "1",
                 str(idx), str(sign)])
        if r.returncode == 0:
            r = run(["ffmpeg", "-hide_banner", "-loglevel", "error",
                     "-i", str(ivf), "-f", "rawvideo", "-pix_fmt", "yuv420p",
                     "-y", str(yuv)])
        if r.returncode or not yuv.exists():
            wedge_failures.append(f"mv=({mv_x},{mv_y}) idx={idx} s={sign} DECODE_FAIL")
            continue
        got, want = yuv.read_bytes(), expected.read_bytes()
        _, diff = compare(got, want, FRAME_BYTES_8X16)
        if diff or len(got) != FRAME_BYTES_8X16:
            wedge_failures.append(f"mv=({mv_x},{mv_y}) idx={idx} s={sign} "
                                  f"mismatches={len(diff)} first={diff[:6]}")
    ok = ok and not wedge_failures
    print(f"AV1 INTER ORACLE (stage5 compound wedge): "
          f"{'PASS' if not wedge_failures else 'FAIL'} "
          f"checked={len(wedge_cases)} failed={len(wedge_failures)}")
    for line in wedge_failures[:12]:
        print("  " + line)

    # 4f') 帧级 ROTZOOM/AFFINE 全局运动：warp 8x8 亮度 + GMV 导出 MV 的色度。
    gm_cases = [(2, 0, 0, 0, 0, 0, 0), (2, 256, 128, 0, 0, 32, -16),
                (3, 0, 0, 0, 0, 0, 0), (3, 256, 128, 64, 32, 32, 16),
                (3, -128, 64, -32, 16, -32, -16)]
    gm_failures = []
    for gm in gm_cases:
        r = run([str(inter_probe), "0", "0", str(ivf), str(expected),
                 "-1", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0",
                 "0", *[str(v) for v in gm]])
        if r.returncode == 0:
            r = run(["ffmpeg", "-hide_banner", "-loglevel", "error",
                     "-i", str(ivf), "-f", "rawvideo", "-pix_fmt", "yuv420p",
                     "-y", str(yuv)])
        if r.returncode or not yuv.exists():
            gm_failures.append(f"gm={gm} DECODE_FAIL")
            continue
        got, want = yuv.read_bytes(), expected.read_bytes()
        _, diff = compare(got, want, FRAME_BYTES_8X16)
        if diff or len(got) != FRAME_BYTES_8X16:
            gm_failures.append(f"gm={gm} mismatches={len(diff)} first={diff[:6]}")
    ok = ok and not gm_failures
    print(f"AV1 INTER ORACLE (stage5 frame GM warp): "
          f"{'PASS' if not gm_failures else 'FAIL'} "
          f"checked={len(gm_cases)} failed={len(gm_failures)}")
    for line in gm_failures[:12]:
        print("  " + line)

    # 4g') DIFFWTD：掩码由两路中间预测的差值推导，mask_type 位控制舍入方向。
    dw_cases = [(0, 0, 0), (1, 0, 1), (3, 5, 0), (-7, 2, 1),
                (9, -9, 0), (1, 14, 1), (-15, 15, 0), (11, -13, 1)]
    dw_failures = []
    for mv_x, mv_y, sign in dw_cases:
        r = run([str(inter_probe), str(mv_x), str(mv_y), str(ivf),
                 str(expected), "-1", "0", "0", "1", "1", "2", "0", str(sign)])
        if r.returncode == 0:
            r = run(["ffmpeg", "-hide_banner", "-loglevel", "error",
                     "-i", str(ivf), "-f", "rawvideo", "-pix_fmt", "yuv420p",
                     "-y", str(yuv)])
        if r.returncode or not yuv.exists():
            dw_failures.append(f"mv=({mv_x},{mv_y}) s={sign} DECODE_FAIL")
            continue
        got, want = yuv.read_bytes(), expected.read_bytes()
        _, diff = compare(got, want, FRAME_BYTES_8X16)
        if diff or len(got) != FRAME_BYTES_8X16:
            dw_failures.append(f"mv=({mv_x},{mv_y}) s={sign} "
                               f"mismatches={len(diff)} first={diff[:6]}")
    ok = ok and not dw_failures
    print(f"AV1 INTER ORACLE (stage5 compound DIFFWTD): "
          f"{'PASS' if not dw_failures else 'FAIL'} "
          f"checked={len(dw_cases)} failed={len(dw_failures)}")
    for line in dw_failures[:12]:
        print("  " + line)

    # 4g'') DISTWTD：enable_order_hint + enable_jnt_comp，comp_group_idx=0 的
    # joint 组，d0==d1 ⇒ jnt_ctx=3；权重取自 quant_dist_lookup_table[0][1]=7。
    dist_cases = [(0, 0), (3, 5), (-7, 2), (9, -9), (1, 14), (-15, 15), (11, -13)]
    dist_failures = []
    for mv_x, mv_y in dist_cases:
        r = run([str(inter_probe), str(mv_x), str(mv_y), str(ivf),
                 str(expected), "-1", "0", "0", "1", "1", "3", "0", "0"])
        if r.returncode == 0:
            r = run(["ffmpeg", "-hide_banner", "-loglevel", "error",
                     "-i", str(ivf), "-f", "rawvideo", "-pix_fmt", "yuv420p",
                     "-y", str(yuv)])
        if r.returncode or not yuv.exists():
            dist_failures.append(f"mv=({mv_x},{mv_y}) DECODE_FAIL")
            continue
        got, want = yuv.read_bytes(), expected.read_bytes()
        _, diff = compare(got, want, FRAME_BYTES_8X16)
        if diff or len(got) != FRAME_BYTES_8X16:
            dist_failures.append(f"mv=({mv_x},{mv_y}) "
                                 f"mismatches={len(diff)} first={diff[:6]}")
    ok = ok and not dist_failures
    print(f"AV1 INTER ORACLE (stage5 compound DISTWTD): "
          f"{'PASS' if not dist_failures else 'FAIL'} "
          f"checked={len(dist_cases)} failed={len(dist_failures)}")
    for line in dist_failures[:12]:
        print("  " + line)

    # 4g) inter-intra：单参考 + 帧内模式混合，含 VERT/HOR/SMOOTH 的非均匀权重
    # 与 wedge 掩码两种路径。
    ii_cases = [(3, 5, 0, 0, 0), (9, -9, 1, 0, 0), (3, 5, 2, 0, 0),
                (-7, 2, 3, 0, 0), (9, -9, 0, 1, 0), (3, 5, 1, 1, 2),
                (-7, 2, 2, 1, 6), (11, -13, 3, 1, 15)]
    ii_failures = []
    for mv_x, mv_y, mode, wedge, idx in ii_cases:
        r = run([str(inter_probe), str(mv_x), str(mv_y), str(ivf),
                 str(expected), "-1", "0", "0", "1", "0", "0", "0", "0",
                 "1", str(mode), str(wedge), str(idx)])
        if r.returncode == 0:
            r = run(["ffmpeg", "-hide_banner", "-loglevel", "error",
                     "-i", str(ivf), "-f", "rawvideo", "-pix_fmt", "yuv420p",
                     "-y", str(yuv)])
        if r.returncode or not yuv.exists():
            ii_failures.append(f"mv=({mv_x},{mv_y}) m={mode} w={wedge} DECODE_FAIL")
            continue
        got, want = yuv.read_bytes(), expected.read_bytes()
        _, diff = compare(got, want, FRAME_BYTES_8X16)
        if diff or len(got) != FRAME_BYTES_8X16:
            ii_failures.append(f"mv=({mv_x},{mv_y}) m={mode} w={wedge} i={idx} "
                               f"mismatches={len(diff)} first={diff[:6]}")
    ok = ok and not ii_failures
    print(f"AV1 INTER ORACLE (stage5 inter-intra): "
          f"{'PASS' if not ii_failures else 'FAIL'} "
          f"checked={len(ii_cases)} failed={len(ii_failures)}")
    for line in ii_failures[:12]:
        print("  " + line)

    # 4h) OBMC：8x16 帧上下两个 NEWMV 块，下块 motion_mode=MM_OBMC。上块 A 的
    # MV 预测作为邻居分量，与下块自身的预测在前 (8*3)>>2 = 6 行按规范掩码软混合；
    # 色度块高 4，混合前 3 行。A/B 的 MV 不同时，混合区出现中间值，纯 mv_b 预测
    # 或纯 mv_a 预测都会对不上。
    obmc_cases = [(0, 0, 0, 0), (0, 8, 0, 0), (8, 0, 0, 0), (0, -8, 0, 0),
                  (0, 3, 0, 0), (3, 0, 0, 0), (0, 0, 0, 8), (0, 5, 0, -5),
                  (-7, 2, 9, -9), (0, 0, 8, 3), (5, -3, 2, 7)]
    obmc_failures = []
    for mv_ax, mv_ay, mv_bx, mv_by in obmc_cases:
        r = run([str(obmc_probe), str(mv_ax), str(mv_ay), str(mv_bx),
                 str(mv_by), str(ivf), str(expected)])
        if r.returncode == 0:
            r = run(["ffmpeg", "-hide_banner", "-loglevel", "error",
                     "-i", str(ivf), "-f", "rawvideo", "-pix_fmt", "yuv420p",
                     "-y", str(yuv)])
        if r.returncode or not yuv.exists():
            obmc_failures.append(f"a=({mv_ax},{mv_ay}) b=({mv_bx},{mv_by}) "
                                 f"DECODE_FAIL")
            continue
        got, want = yuv.read_bytes(), expected.read_bytes()
        # 两帧 8x16 = 384 字节；前 192 是 KEY 帧，后 192 是 OBMC 帧。
        diff = [i for i in range(min(384, len(got), len(want)))
                if got[i] != want[i]]
        if len(got) != 384 or len(want) != 384:
            diff.append(-1)
        if diff:
            obmc_failures.append(f"a=({mv_ax},{mv_ay}) b=({mv_bx},{mv_by}) "
                                 f"mismatches={len(diff)} first={diff[:6]}")
    ok = ok and not obmc_failures
    print(f"AV1 INTER ORACLE (stage5 OBMC): "
          f"{'PASS' if not obmc_failures else 'FAIL'} "
          f"checked={len(obmc_cases)} failed={len(obmc_failures)}")
    for line in obmc_failures[:12]:
        print("  " + line)

    # 逐块 warp：motion_mode=MM_WARP 三符号路径，邻居 MV 相同 ⇒ IDENTITY。
    warp_cases = [(0, 0), (0, 3), (3, 0), (-7, 2), (9, -9)]
    warp_failures = []
    for mv_x, mv_y in warp_cases:
        r = run([str(obmc_probe), str(mv_x), str(mv_y), str(mv_x), str(mv_y),
                 str(ivf), str(expected), "1", "1", "1"])
        if r.returncode == 0:
            r = run(["ffmpeg", "-hide_banner", "-loglevel", "error",
                     "-i", str(ivf), "-f", "rawvideo", "-pix_fmt", "yuv420p",
                     "-y", str(yuv)])
        if r.returncode or not yuv.exists():
            warp_failures.append(f"mv=({mv_x},{mv_y}) DECODE_FAIL")
            continue
        got, want = yuv.read_bytes(), expected.read_bytes()
        diff = [i for i in range(min(len(got), len(want))) if got[i] != want[i]]
        if diff or len(got) != 384:
            warp_failures.append(f"mv=({mv_x},{mv_y}) "
                                 f"mismatches={len(diff)} first={diff[:6]}")
    ok = ok and not warp_failures
    print(f"AV1 INTER ORACLE (stage5 block warp): "
          f"{'PASS' if not warp_failures else 'FAIL'} "
          f"checked={len(warp_cases)} failed={len(warp_failures)}")
    for line in warp_failures[:12]:
        print("  " + line)

    # 5) 16x16 帧：非 8x8 尺寸的多层语法。64x64 / 32x32 层的半个块越过帧边界，
    # 规范要求解码端隐式取 SPLIT 且不读符号；真正读划分符号的是 16x16 层
    # （dav1d bl=3）的 10 符号 partition_cdf。块模式、16x16 亮度 + 8x8 色度
    # 预测以及 16x16 整帧 MC 都在这一尺寸首次出现。
    part_cases = [(0, m, 0, 0) for m in range(13)]
    part_cases += [(1, 9, x, y) for x, y in ((0, 0), (1, 0), (0, 1), (3, 5),
                                             (-7, 2), (9, -9), (1, 14),
                                             (-15, 15), (13, -7))]
    # PARTITION_SPLIT 的四块流：y_mode 参数当作模式组合序号，覆盖 kfym 邻居
    # 上下文折叠与 SMOOTH 族邻居边界类型两条路径。
    part_cases += [(2, p, 0, 0) for p in range(6)]
    part_failures = []
    for mode, ymode, mv_x, mv_y in part_cases:
        r = run([str(partition_probe), str(mode), str(ymode), str(mv_x),
                 str(mv_y), str(ivf), str(expected)])
        if r.returncode == 0:
            r = run(["ffmpeg", "-hide_banner", "-loglevel", "error",
                     "-i", str(ivf), "-f", "rawvideo", "-pix_fmt", "yuv420p",
                     "-y", str(yuv)])
        if r.returncode or not yuv.exists():
            part_failures.append(
                f"mode={mode} y={ymode} mv=({mv_x},{mv_y}) DECODE_FAIL")
            continue
        got, want = yuv.read_bytes(), expected.read_bytes()
        frames = 2 if mode == 1 else 1
        limit = FRAME_BYTES_16X16 * frames
        _, diff = compare(got, want, limit)
        if diff or len(got) != limit or len(want) != limit:
            part_failures.append(f"mode={mode} y={ymode} mv=({mv_x},{mv_y}) "
                                 f"mismatches={len(diff)} first={diff[:6]}")
    ok = ok and not part_failures
    print(f"AV1 PARTITION ORACLE (16x16 frame): "
          f"{'PASS' if not part_failures else 'FAIL'} "
          f"checked={len(part_cases)} failed={len(part_failures)}")
    for line in part_failures[:12]:
        print("  " + line)

    # 16x16 / 4x8x8 子块分区 + 右下 MM_WARP：三个邻居 MV 参与 PARAMETERS 拟合，
    # 这里刻意让上/左/左上 MV 各不相同，才能验到非 IDENTITY 的 affine warp 采样。
    warp_split_cases = [
        (0, 0, 0, 0, 0, 0, 0, 0),
        (0, 0, 0, 3, 3, 0, 3, 3),
        (0, 0, 0, 8, 8, 0, 8, 8),
        (0, 0, 8, 0, 0, 8, 8, 8),
        (2, -2, -4, 6, 6, -4, -2, 2),
        (0, 0, -8, 0, 0, -8, -8, -8),
    ]
    warp_split_failures = []
    for m0x, m0y, m1x, m1y, m2x, m2y, m3x, m3y in warp_split_cases:
        r = run([str(partition_probe), "3", "0", str(m0x), str(m0y),
                 str(m1x), str(m1y), str(m2x), str(m2y), str(m3x), str(m3y),
                 str(ivf), str(expected)])
        if r.returncode == 0:
            r = run(["ffmpeg", "-hide_banner", "-loglevel", "error",
                     "-i", str(ivf), "-f", "rawvideo", "-pix_fmt", "yuv420p",
                     "-y", str(yuv)])
        if r.returncode or not yuv.exists():
            warp_split_failures.append(
                f"mvs=({m0x},{m0y})({m1x},{m1y})({m2x},{m2y})({m3x},{m3y}) "
                f"DECODE_FAIL {r.stderr.decode(errors='replace')[-200:]}")
            continue
        got, want = yuv.read_bytes(), expected.read_bytes()
        _, diff = compare(got, want, FRAME_BYTES_16X16 * 2)
        if diff or len(got) != FRAME_BYTES_16X16 * 2:
            warp_split_failures.append(
                f"mvs=({m0x},{m0y})({m1x},{m1y})({m2x},{m2y})({m3x},{m3y}) "
                f"mismatches={len(diff)} first={diff[:6]}")
    ok = ok and not warp_split_failures
    print(f"AV1 PARTITION ORACLE (16x16 split block warp): "
          f"{'PASS' if not warp_split_failures else 'FAIL'} "
          f"checked={len(warp_split_cases)} failed={len(warp_split_failures)}")
    for line in warp_split_failures[:12]:
        print("  " + line)

    # 阶段七：环路去块滤波。帧头 loop_filter_level[0]>0，块边界 y=8 的
    # 4 宽边缘走 6/8/16 之外的常规 4 抽头路径；sharpness 切 I/E 阈值。
    # sharpness>0 的边界强度在本机 ffmpeg/libdav1d 上与规范 E/I 表存在边界
    # 差异（HEV 判定临界的样点），该子集改由单测覆盖；这里锁 sharpness=0。
    deblock_cases = [(lv, 0) for lv in (1, 4, 15, 16, 24, 32, 48, 63)]
    deblock_failures = []
    for level, sharpness in deblock_cases:
        r = run([str(deblock_probe), str(level), str(sharpness), str(ivf),
                 str(expected)])
        if r.returncode == 0:
            r = run(["ffmpeg", "-hide_banner", "-loglevel", "error",
                     "-i", str(ivf), "-f", "rawvideo", "-pix_fmt", "yuv420p",
                     "-y", str(yuv)])
        if r.returncode or not yuv.exists():
            deblock_failures.append(f"lf={level},{sharpness} DECODE_FAIL")
            continue
        got, want = yuv.read_bytes(), expected.read_bytes()
        _, diff = compare(got, want, FRAME_BYTES_8X16)
        if diff or len(got) != FRAME_BYTES_8X16:
            deblock_failures.append(
                f"lf={level},{sharpness} mismatches={len(diff)} first={diff[:6]}")
    ok = ok and not deblock_failures
    print(f"AV1 POSTPROC ORACLE (stage7 deblock): "
          f"{'PASS' if not deblock_failures else 'FAIL'} "
          f"checked={len(deblock_cases)} failed={len(deblock_failures)}")
    for line in deblock_failures[:12]:
        print("  " + line)

    # 整帧：真实 Bitmap → 4:2:0 → RDO 帧内预测 → IVF → libdav1d 逐像素对拍。
    # 当前仅登记预测-only 模式（残差熵在多块纹理帧上尚未逐像素闭合）。
    frame_cases = [(64, 64, 100, 1, 1), (128, 64, 100, 1, 1),
                   (64, 128, 120, 1, 1), (64, 192, 100, 1, 0),
                   (192, 64, 100, 1, 1)]
    frame_failures = []
    for w, h, q, rdo, part in frame_cases:
        tag = f"{w}x{h}_q{q}_r{rdo}p{part}"
        r = run([str(frame_probe), str(w), str(h), str(q), str(rdo),
                 str(part), str(ivf), str(expected), "0", "1"])
        if r.returncode == 0:
            r = run(["ffmpeg", "-hide_banner", "-loglevel", "error",
                     "-i", str(ivf), "-f", "rawvideo", "-pix_fmt", "yuv420p",
                     "-y", str(yuv)])
        if r.returncode:
            frame_failures.append(f"{tag} DECODE_FAIL")
            continue
        got, want = yuv.read_bytes(), expected.read_bytes()
        frame_bytes = w * h * 3 // 2
        _, diff = compare(got, want, frame_bytes)
        if diff or len(got) != frame_bytes:
            frame_failures.append(
                f"{tag} mismatches={len(diff)} first={diff[:6]}")
    ok = ok and not frame_failures
    print(f"AV1 FRAME ORACLE (prediction-only): "
          f"{'PASS' if not frame_failures else 'FAIL'} "
          f"checked={len(frame_cases)} failed={len(frame_failures)}")
    for line in frame_failures[:12]:
        print("  " + line)

    print(f"AV1 MINIMAL ORACLE: {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
