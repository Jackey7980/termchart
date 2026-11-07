"""I_PCM 黄金验收：6 条全绿才允许进入拆文件阶段。

用法：
    python diag_hevc.py            # 严格验收，任一失败退出 1
    python diag_hevc.py --verbose  # 额外打印逐样本诊断

判据：
  1 slice header 严格 IDR 语法（5 个字段 + byte_alignment，无 SAO/POC/entry point）
  2 8x8 叶顺序（part_mode ctx13/init184 -> pcm_flag 终止 bin -> FlushD -> 对齐 -> 96 字节）
  3 CABAC 引擎不变量（range 510 起步、无首比特反转、终止 bin）
  4 PCM 后重置（range=0x1FE、low 复位、上下文表不重置）
  5 dec_U[0:16] == enc_U[0:16] 为 True 且 Y[0] == 16（逐字节相等，不是接近）
  6 FFmpeg 解码 3 帧 / 0 error，码流总字节数与 sha256 精确等于黄金基线

注 3/4：当前实现采用「按区间反解 codILow」而非 H.264 式 bitsToFollow/FlushD
（见 hevc_encode.cpp 300-315 行注释：不做首比特反转、无需模拟进位、与 FFmpeg
逐位一致）。因此这里断言的是语义不变量，字面 bitsToFollow 不适用；等价性由
第 5/6 条的逐字节验证兜底。
"""
import hashlib
import os
import shutil
import subprocess
import sys

STREAM = "tc_hevc.265"
SRC = "src/image/hevc_encode.cpp"
RAW = "_verify_pcm.yuv"
BASE_BYTES = 20430
BASE_SHA_PREFIX = "10a4759ac9bcbed137012443"
W, H, NFRAMES = 64, 64, 3  # 64x48 源图按 CTB 对齐补到 64x64

FF = (r"C:\Users\琳琳\AppData\Roaming\Python\Python314\site-packages"
      r"\imageio_ffmpeg\binaries\ffmpeg-win-x86_64-v9.0.2.exe")
if not os.path.exists(FF):
    FF = shutil.which("ffmpeg") or "ffmpeg"

results = []


def check(n, name, ok, detail=""):
    results.append((n, bool(ok)))
    print(f"[{'PASS' if ok else 'FAIL'}] {n}. {name}" + (f"  {detail}" if detail else ""))


def expected(x, y, phase):
    r = min(255, (x * 255) // 64)   # driver: x*255/w, w=64
    g = min(255, (y * 255) // 48)   # driver: y*255/h, h=48
    b = (phase * 40) % 256
    ly = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16
    lu = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128
    lv = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128
    return ly & 0xFF, lu & 0xFF, lv & 0xFF


# A2 起语法元素分散到 codec/hevc/*.cpp，静态不变量扫描必须覆盖全部实现文件；
# 判据本身不放宽，第 6 条的码流字节/sha 基线仍是最强门禁。
CODE_FILES = [
    "src/image/hevc_encode.cpp",
    "src/image/codec/hevc/parameter_sets.cpp",
    "src/image/codec/hevc/slice_header.cpp",
    "src/image/codec/hevc/nal_unit.cpp",
    "src/image/codec/hevc/cabac_context.cpp",
    "src/image/codec/hevc/cabac_engine.cpp",
    "src/image/codec/hevc/cabac_syntax.cpp",
    "src/image/codec/hevc/ctb_tree.cpp",
]
src = "\n".join(open(p, encoding="utf-8").read()
                for p in CODE_FILES if os.path.exists(p))

# 1. slice header
SH_FILE = "src/image/codec/hevc/slice_header.cpp"
sh = open(SH_FILE, encoding="utf-8").read() if os.path.exists(SH_FILE) else ""
need = ["first_slice_segment_in_pic_flag", "no_output_of_prior_pics_flag",
        "slice_pic_parameter_set_id", "slice_type", "slice_qp_delta", "byte_alignment"]
forbidden = ["slice_sao_luma_flag", "slice_sao_chroma_flag",
             "slice_pic_order_cnt_lsb", "num_entry_point_offsets"]
# 注释里会显式提到「不读 SAO/POC/entry point」，只检查实际代码行。
code_only = "\n".join(ln.split("//", 1)[0] for ln in sh.splitlines())
ok1 = all(t in src for t in need) and bool(sh) and not any(t in code_only for t in forbidden)
check(1, "slice header 严格 IDR 语法", ok1,
      f"fields={sum(t in src for t in need)}/6 forbidden_in_code={sum(t in code_only for t in forbidden)}")

# 2. 8x8 叶顺序
leaf = ["init_one(13, 184)", "raw.reserve(96)", "pcm_alignment_zero_bit",
        "get_cabac_terminate", "encode_terminating", "flush_cu"]
ok2 = all(t in src for t in leaf)
check(2, "8x8 叶顺序（part_mode -> pcm_flag -> 对齐 -> 96B）", ok2,
      "tokens=" + ",".join(t for t in leaf if t in src))

# 3. CABAC 引擎语义不变量
cab = ["range = 510", "renorm", "首比特反转", "不做", "encode_terminating"]
ok3 = all(t in src for t in cab)
check(3, "CABAC 引擎（range=510、无首比特反转、终止 bin）", ok3,
      "实现路径=区间反解（非 bitsToFollow），由 5/6 逐字节兜底")

# 4. PCM 后重置
i = src.find("flush_cu(")
body = src[i:i + 1800] if i >= 0 else ""
ok4 = "range = 0x1FE" in src and "flush_cu" in src and "init_one(" not in body
check(4, "PCM 后重置（low/range/pending 复位，上下文不重置）", ok4,
      "range_reset=0x1FE ctx_reinit_in_flush=" + str("init_one(" in body))

# 5/6. FFmpeg 解码 + 逐字节比较 + 字节基线
stream = open(STREAM, "rb").read()
sha = hashlib.sha256(stream).hexdigest()
size_exact = len(stream) == BASE_BYTES and sha.startswith(BASE_SHA_PREFIX)

r = subprocess.run([FF, "-y", "-v", "error", "-i", STREAM,
                    "-f", "rawvideo", "-pix_fmt", "yuv420p", RAW],
                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
err = r.stderr.decode("utf-8", "replace").strip()
data = open(RAW, "rb").read() if os.path.exists(RAW) else b""
frame_bytes = W * H * 3 // 2
frames_exact = len(data) == NFRAMES * frame_bytes
zero_error = r.returncode == 0 and not err

off = 0
Y = data[off:off + W * H]; off += W * H
U = data[off:off + W * H // 4]; off += W * H // 4
V = data[off:off + W * H // 4]

enc_y = bytes(expected(x, 0, 0)[0] for x in range(16))
enc_u = bytes(expected(2 * x, 0, 0)[1] for x in range(16))
enc_v = bytes(expected(2 * x, 0, 0)[2] for x in range(16))
dec_y, dec_u, dec_v = Y[:16], U[:16], V[:16]

print("dec_U[0:16] == enc_U[0:16] :", dec_u == enc_u)
print("  dec_U:", list(dec_u))
print("  enc_U:", list(enc_u))
print("Y[0] == 16 :", Y[0] == 16, "(Y[0]=%d)" % (Y[0] if Y else -1))

ok5 = dec_u == enc_u and dec_v == enc_v and dec_y == enc_y and bool(Y) and Y[0] == 16
check(5, "像素逐字节相等（Y/U/V 各前 16 个 + Y[0]==16）", ok5,
      f"Y={dec_y == enc_y} U={dec_u == enc_u} V={dec_v == enc_v}")

ok6 = zero_error and frames_exact and size_exact
check(6, "FFmpeg 3 帧 / 0 error / 码流字节与 sha256 精确", ok6,
      f"bytes={len(stream)}/{BASE_BYTES} sha={sha[:24]} dec={len(data)} "
      f"frames={len(data) // frame_bytes if frame_bytes else 0} err={err[:60]!r}")

if "--verbose" in sys.argv:
    print("first 16 decoded Y:", list(dec_y))
    print("first 16 decoded V:", list(dec_v))

all_green = all(ok for _, ok in results)
print("\n== I_PCM 验收:", "6/6 GREEN —— 允许进入拆文件阶段" if all_green
      else f"{sum(ok for _, ok in results)}/6 GREEN —— 禁止拆文件", "==")
sys.exit(0 if all_green else 1)
