#!/usr/bin/env python3
"""用 AOM 官方 libaom 测试向量校验 AV1 解码结果的像素级一致性。

默认向量来自 storage.googleapis.com/aom-test-data，缓存与解码临时文件均落在
D:/tools/av1_conformance（可用 --cache-dir 覆盖）。每个 .ivf.md5 文件给出该
向量解码后每帧 i420 数据的 MD5；脚本用 ffmpeg 解码并逐帧核对。
"""
import argparse
import hashlib
import re
import subprocess
import sys
import urllib.request
from pathlib import Path

BASE_URL = 'https://storage.googleapis.com/aom-test-data/'
DEFAULT_CACHE = Path('D:/tools/av1_conformance')
VECTORS = ([f'av1-1-b8-00-quantizer-{i:02d}' for i in range(10)] +
           [f'av1-1-b10-00-quantizer-{i:02d}' for i in range(10)])
FRAME_RE = re.compile(r'-(\d+)x(\d+)-(\d+)\.i420$')
DEPTH_RE = re.compile(r'-b(\d+)-')


def fetch(base: str, name: str, cache: Path) -> Path:
    path = cache / name
    if not path.exists():
        data = urllib.request.urlopen(base + name, timeout=60).read()
        path.write_bytes(data)
    return path


def parse_expected(md5_path: Path):
    expected = []
    for line in md5_path.read_text(errors='replace').splitlines():
        parts = line.split()
        if len(parts) != 2:
            continue
        m = FRAME_RE.search(parts[1])
        if not m:
            continue
        expected.append((int(m.group(3)), parts[0], int(m.group(1)), int(m.group(2))))
    expected.sort()
    if not expected:
        raise ValueError(f'{md5_path} 中没有可识别的 i420 帧 MD5')
    return expected


def frame_md5(data: bytes, size: int):
    out = []
    for off in range(0, len(data), size):
        if len(data) - off < size:
            break
        out.append(hashlib.md5(data[off:off + size]).hexdigest())
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--cache-dir', type=Path, default=DEFAULT_CACHE)
    ap.add_argument('--base-url', default=BASE_URL)
    ap.add_argument('--ffmpeg', default='ffmpeg')
    ap.add_argument('--vectors', nargs='*', default=VECTORS)
    ap.add_argument('--prepare-only', action='store_true',
                    help='只下载向量和 MD5，不解码')
    args = ap.parse_args()

    cache = args.cache_dir
    cache.mkdir(parents=True, exist_ok=True)
    failures = 0

    for name in args.vectors:
        ivf = fetch(args.base_url, name + '.ivf', cache)
        md5 = fetch(args.base_url, name + '.ivf.md5', cache)
        if args.prepare_only:
            print(f'PREPARED {name}')
            continue
        expected = parse_expected(md5)
        dm = DEPTH_RE.search(name)
        depth = int(dm.group(1)) if dm else 8
        pixel_fmt = 'yuv420p10le' if depth > 8 else 'yuv420p'
        w, h = expected[0][2], expected[0][3]
        frame_size = w * h * 3 if depth > 8 else w * h * 3 // 2
        out = cache / f'{name}.i420'
        cmd = [args.ffmpeg, '-hide_banner', '-loglevel', 'error',
               '-i', str(ivf), '-f', 'rawvideo', '-pix_fmt', pixel_fmt,
               '-y', str(out)]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            print(f'FAIL {name}: ffmpeg 解码失败: {r.stderr.strip()[-200:]}')
            failures += 1
            continue
        got = frame_md5(out.read_bytes(), frame_size)
        want = [item[1] for item in expected]
        if got == want:
            print(f'PASS {name} ({len(got)} frames)')
        else:
            bad = [i for i, (a, b) in enumerate(zip(got, want)) if a != b]
            print(f'FAIL {name}: 帧数 got={len(got)} want={len(want)} 差异帧={bad[:8]}')
            failures += 1
        out.unlink(missing_ok=True)

    if args.prepare_only:
        return 0
    print(f'AV1 CONFORMANCE: {"PASS" if failures == 0 else "FAIL"} '
          f'({len(args.vectors) - failures}/{len(args.vectors)})')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
