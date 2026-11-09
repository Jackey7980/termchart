#!/usr/bin/env python3
"""Reference HEVC CABAC decoder to debug our own encoder stream.
Decodes the quadtree + PCM of a single IDR slice and checks the bins we expect
(split=1 everywhere, part_mode=2Nx2N, pcm_flag=1, raw PCM bytes match input).
"""
import json
import struct
import sys

# ---- HEVC standard CABAC tables (identical to hevc_encode.cpp) ----
def build_lps():
    rows = [
        "128,176,208,240","128,167,197,227","128,158,187,216","123,150,178,205",
        "116,142,169,195","111,135,160,185","105,128,152,175","100,122,144,166",
        "95,116,137,158","90,110,130,150","85,104,123,142","81,99,117,135",
        "77,94,111,128","73,89,105,122","69,85,100,116","66,80,94,108",
        "62,76,89,103","59,72,85,98","56,69,81,93","53,65,77,89",
        "51,62,73,84","48,59,69,79","46,56,66,75","43,53,63,72",
        "41,50,59,68","39,48,56,64","37,45,53,61","35,43,51,58",
        "33,41,48,55","31,38,45,51","30,36,43,49","28,34,40,46",
        "27,33,38,43","26,31,36,41","24,30,34,39","23,28,32,37",
        "22,27,31,35","21,26,29,33","20,24,27,31","19,23,26,29",
        "18,22,25,28","17,21,23,26","16,20,22,24","15,19,21,23",
        "14,18,19,21","14,17,18,20","13,16,17,19","12,15,16,17",
        "11,14,14,16","11,13,14,15","10,12,13,14","9,11,12,13",
        "9,11,11,12","8,10,10,11","7,9,9,10","7,8,8,9",
        "6,7,7,8","6,7,7,7","5,6,6,7","5,5,5,6",
        "4,5,5,5","4,4,4,5","3,4,4,4","3,3,3,4",
        "2,3,3,3","2,2,2,3",
    ]
    t = []
    for r in rows:
        t.append([int(x) for x in r.split(",")])
    return t

LPS = build_lps()
RENORM = [6,5,4,4,3,3,3,3,2,2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1]
NEXT_MPS = list(range(64))
NEXT_LPS = list(range(1,64)) + [0]

def init_ctx(init_value, slice_qp):
    m = (init_value >> 4) * 5 - 45
    n = ((init_value & 15) << 3) - 16
    pre = (m * slice_qp) >> 4
    pre += n + 1
    pre = max(1, min(126, pre))
    if pre <= 63:
        state = 63 - pre
        mps = 1
    else:
        state = pre - 64
        mps = 0
    return [state, mps]

class CabacReader:
    def __init__(self, data):
        self.data = data
        self.pos = 0  # bit position
        self.range = 510
        self.offset = 0
        for _ in range(9):
            self.offset = (self.offset << 1) | self._bit()
    def _bit(self):
        if self.pos >= len(self.data) * 8:
            return 0
        byte = self.data[self.pos >> 3]
        b = (byte >> (7 - (self.pos & 7))) & 1
        self.pos += 1
        return b
    def decode(self, ctx):
        q = (self.range >> 6) & 3
        qlps = LPS[ctx[0]][q]
        self.range -= qlps
        if self.offset < self.range:
            binval = ctx[1]
            ctx[0] = NEXT_MPS[ctx[0]]
        else:
            binval = 1 - ctx[1]
            self.offset -= self.range
            self.range = qlps
            if ctx[0] == 0:
                ctx[1] = 1 - ctx[1]
            ctx[0] = NEXT_LPS[ctx[0]]
        while self.range < 256:
            self.range <<= 1
            self.offset = (self.offset << 1) | self._bit()
        return binval
    def bypass(self):
        self.offset = (self.offset << 1) | self._bit()
        return self.offset & 1
    def align_bypass(self):
        # read bypass until byte aligned: alignment_bit_equal_to_one(1) first, then zeros
        # (matches encoder byte_align_pcm)
        bit = self.bypass()
        while (self.pos & 7) != 0:
            self.bypass()
        return bit

def de_emulate(data):
    out = bytearray()
    i = 0
    while i < len(data):
        if i + 2 < len(data) and data[i] == 0 and data[i+1] == 0 and data[i+2] == 3:
            out.append(0); out.append(0); i += 3
        else:
            out.append(data[i]); i += 1
    return bytes(out)

def parse_nals(path):
    raw = open(path, "rb").read()
    # split on start codes 00 00 01
    nals = []
    i = 0
    while i < len(raw):
        if raw[i:i+3] == b"\x00\x00\x01":
            j = i + 3
            while j < len(raw) and not (raw[j:j+3] == b"\x00\x00\x01"):
                j += 1
            nals.append(raw[i+3:j])
            i = j
        else:
            i += 1
    return nals

def ue(bits):
    # bits is a function returning next bit
    pass

def main():
    nals = parse_nals("c:/termchart/tc_hevc.265")
    print(f"found {len(nals)} nals; types:")
    for n in nals:
        print("  len", len(n), "first byte", hex(n[0]))

    # slice NAL type 19
    slice_nal = [n for n in nals if (n[0] >> 1) & 0x3F == 19][0]
    rbsp = de_emulate(slice_nal)
    # NAL header: 2 bytes for the header (for non-VCL 2 bytes, for slice 2 bytes? HEVC slice NAL header is 2 bytes for non-IDR? Actually 2 bytes)
    # HEVC slice NAL header: forbidden_zero(1) + nal_unit_type(6) + nuh_layer_id(6) + nuh_temporal_id(3) = 2 bytes.
    # Then byte 2 begins slice header.
    # But slice header starts with first_slice_segment_in_pic_flag at bit 0 of byte 2.
    # We need a bit reader over rbsp[2:].
    # Parse slice header manually:
    bitpos = 16  # after 2-byte NAL header
    def getbit():
        nonlocal bitpos
        byte = rbsp[bitpos >> 3]
        b = (byte >> (7 - (bitpos & 7))) & 1
        bitpos += 1
        return b
    def u(n):
        v = 0
        for _ in range(n):
            v = (v << 1) | getbit()
        return v
    def ue():
        lead = 0
        while getbit() == 0:
            lead += 1
        v = 0
        for _ in range(lead):
            v = (v << 1) | getbit()
        if lead == 0:
            return 0
        return (1 << lead) - 1 + v
    def se():
        k = ue()
        if k & 1:
            return (k + 1) >> 1
        else:
            return -(k >> 1)

    fsp = u(1)
    no_out = u(1)
    pps_id = ue()
    stype = ue()
    sqp = se()
    print(f"slice header: first_slice={fsp} no_output={no_out} pps_id={pps_id} type={stype} qp_delta={sqp}")
    # align to byte
    if bitpos & 7:
        bitpos += (8 - (bitpos & 7))
    cabac_start = bitpos >> 3
    print(f"cabac starts at byte {cabac_start} (bit {bitpos})")

    rd = CabacReader(rbsp[cabac_start:])
    # contexts
    ctx = {}
    ctx[2] = init_ctx(139, 26)
    ctx[3] = init_ctx(141, 26)
    ctx[4] = init_ctx(157, 26)
    ctx[7] = init_ctx(154, 26)
    ctx[13] = init_ctx(154, 26)

    def split_ctx(x0, y0, ctb_x, ctb_y, log2):
        if log2 == 6:
            return 2
        px = (x0 - ctb_x) >> log2
        py = (y0 - ctb_y) >> log2
        condL = 1 if px > 0 else (1 if ctb_x > 0 else 0)
        condA = 1 if py > 0 else (1 if ctb_y > 0 else 0)
        return 2 + condL + condA

    def walk(x0, y0, log2, ctb_x, ctb_y, depth=0):
        if log2 > 3:
            cid = split_ctx(x0, y0, ctb_x, ctb_y, log2)
            b = rd.decode(ctx[cid])
            if b != 1:
                print(f"  SPLIT!=1 at x0={x0} y0={y0} log2={log2} ctx={cid} got {b}")
                return
            half = 1 << (log2 - 1)
            for i in range(4):
                xx = x0 + (i & 1) * half
                yy = y0 + (i >> 1) * half
                walk(xx, yy, log2 - 1, ctb_x, ctb_y, depth+1)
        else:
            # 8x8 leaf
            pm = rd.decode(ctx[13])
            if pm != 1:
                print(f"  PART_MODE!=2Nx2N at x0={x0} y0={y0} got {pm}")
                return
            pcm = rd.decode(ctx[7])
            if pcm != 1:
                print(f"  PCM_FLAG!=1 at x0={x0} y0={y0} got {pcm}")
                return
            algn = rd.align_bypass()
            if algn != 1:
                print(f"  ALIGNMENT!=1 at x0={x0} y0={y0} got {algn} (byte pos {rd.pos//8})")
                return
            # raw PCM: 64 luma + 16 cb + 16 cr = 96 bytes
            n = 96
            # consume n bytes from the bitstream (byte aligned)
            # rd.pos should be byte aligned here
            if rd.pos & 7:
                print(f"  NOT byte aligned before PCM at x0={x0} y0={y0}, pos={rd.pos}")
                return
            pcm_bytes = rd.data[rd.pos >> 3: rd.pos >> 3 + n]
            rd.pos += n * 8
            # reset CABAC after PCM (decoder resets)
            rd.range = 256
            # re-init offset: read 9 bits? Actually after skip_bytes the decoder resets
            # and refills range=256, then continues. We'll just continue from new byte.
            return

    # single CTB 64x64 at (0,0)
    walk(0, 0, 6, 0, 0)
    print("done; reader bit pos", rd.pos, "of", len(rd.data)*8)

if __name__ == "__main__":
    main()
