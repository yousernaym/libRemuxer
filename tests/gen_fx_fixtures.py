#!/usr/bin/env python3
"""Generate FX.S3M and FX.IT as semantic twins of FX.XM for openmpt-based fixture tests."""

from __future__ import annotations

import struct
from pathlib import Path

OUT_DIR = Path(__file__).resolve().parent.parent / "test-files"

# OpenMPT-space notes (XM file note + 12).
NOTES = [61, 62, 63, 64, 65, 66, 67, 68, 69, 71, 72, 89, 73, 74, 75, 77, 79, 80]
CH14_N1 = 76
CH15_N1 = 78
ROWS = 64
CHANNELS = 18

# S3M / IT effect letters (A=1)
FX_D, FX_G, FX_J, FX_K, FX_L, FX_Q, FX_S = 4, 7, 10, 11, 12, 17, 19


def ompt_to_s3m_note(n: int) -> int:
    if n == 0xFF:
        return 254
    if n in (0xFE, 0xFD):
        return 254
    pc = (n - 1) % 12
    octv = (n - 1) // 12
    return (octv << 4) | (pc + 1)


def build_cells(s3m: bool) -> list[list[dict]]:
    cells: list[list[dict]] = [[{} for _ in range(CHANNELS)] for _ in range(ROWS)]

    def setc(row: int, ch: int, **kw):
        cells[row][ch].update(kw)

    for ch, note in enumerate(NOTES):
        setc(0, ch, note=note, ins=1 if ch != 1 else 2)

    setc(0, 0, fx=FX_S, param=0xC1)
    setc(1, 2, vol=0)  # set volume 0
    setc(0, 3, fx=FX_S, param=0xD1)
    setc(1, 3, note=0xFF)
    setc(1, 4, note=0xFF)
    setc(1, 5, note=0xFF)
    setc(2, 5, ins=2)

    setc(0, 6, fx=FX_D, param=0x02)
    for r in range(1, 7):
        setc(r, 6, fx=FX_D, param=0x00)

    if s3m:
        setc(0, 7, fx=FX_D, param=0x02)
        for r in range(1, 7):
            setc(r, 7, fx=FX_D, param=0x00)
    else:
        setc(0, 7, volcmd="voldown", vol=2)
        for r in range(1, 7):
            setc(r, 7, volcmd="voldown", vol=2)

    setc(1, 8, fx=FX_L, param=0x02)
    for r in range(2, 8):
        setc(r, 8, fx=FX_L, param=0x00)

    setc(1, 9, fx=FX_K, param=0x02)
    for r in range(2, 8):
        setc(r, 9, fx=FX_K, param=0x00)

    setc(0, 10, fx=FX_D, param=0xF2)
    for r in range(1, 32):
        setc(r, 10, fx=FX_D, param=0xF0)

    setc(0, 11, fx=FX_J, param=0x25)
    setc(1, 11, note=0xFF)
    setc(1, 12, note=NOTES[12], ins=1)
    setc(0, 13, fx=FX_Q, param=0x02)
    setc(1, 13, note=0xFF)
    setc(1, 14, note=CH14_N1, ins=1, fx=FX_G, param=0x00)
    setc(1, 15, note=CH15_N1, ins=1, fx=FX_L, param=0x00)

    if s3m:
        setc(0, 16, fx=FX_D, param=0x1F)
        setc(1, 16, fx=FX_D, param=0xF1)
        setc(2, 16, fx=FX_D, param=0x10)
        setc(0, 17, vol=0)
        setc(1, 17, fx=FX_D, param=0x10)
    else:
        # Classic IT volume column only encodes fine slides 0..9 (not 0..15).
        # Classic IT volume column only encodes slide amounts 0..9 (XM uses F).
        setc(0, 16, volcmd="volup", vol=9)
        setc(1, 16, volcmd="voldown", vol=9)
        setc(2, 16, volcmd="volup", vol=1)
        setc(0, 17, volcmd="volume", vol=0)
        setc(1, 17, volcmd="volup", vol=1)

    return cells


def pack_s3m_pattern(cells: list[list[dict]]) -> bytes:
    out = bytearray()
    for row in range(ROWS):
        for ch in range(CHANNELS):
            c = cells[row][ch]
            if not c:
                continue
            what = ch & 0x1F
            body = bytearray()
            note = c.get("note")
            ins = c.get("ins", 0)
            if note is not None or ins:
                what |= 0x20
                body.append(255 if note is None else ompt_to_s3m_note(note))
                body.append(ins & 0xFF)
            if "vol" in c and "volcmd" not in c:
                what |= 0x40
                body.append(max(0, min(64, c["vol"])))
            if "fx" in c:
                what |= 0x80
                body.append(c["fx"] & 0xFF)
                body.append(c.get("param", 0) & 0xFF)
            out.append(what)
            out.extend(body)
        out.append(0)
    packed = bytes(out)
    return struct.pack("<H", len(packed) + 2) + packed


def write_s3m(path: Path) -> None:
    cells = build_cells(s3m=True)
    pattern = pack_s3m_pattern(cells)
    sample = bytes([0x40] * 10)

    ord_num, ins_num, pat_num = 2, 2, 1
    after_header = 96 + ord_num + ins_num * 2 + pat_num * 2 + 32
    buf = bytearray(after_header)

    def pad16():
        while len(buf) % 16:
            buf.append(0)

    def pp(off: int) -> int:
        return off // 16

    pad16()
    ins1_off = len(buf)
    buf.extend(b"\x00" * 80)
    pad16()
    ins2_off = len(buf)
    buf.extend(b"\x00" * 80)
    pad16()
    pat_off = len(buf)
    buf.extend(pattern)
    pad16()
    smp1_off = len(buf)
    buf.extend(sample)
    pad16()
    smp2_off = len(buf)
    buf.extend(sample)
    pad16()

    def make_ins(smp_off: int, loop: bool, title: str) -> bytes:
        ptr = pp(smp_off)
        hdr = bytearray(80)
        hdr[0] = 1
        hdr[1:13] = b"SAMPLE.RAW\x00\x00"
        hdr[13] = (ptr >> 16) & 0xFF
        struct.pack_into("<H", hdr, 14, ptr & 0xFFFF)
        struct.pack_into("<III", hdr, 0x10, 10, 0, 10 if loop else 0)
        hdr[0x1C] = 64
        hdr[0x1F] = 1 if loop else 0
        struct.pack_into("<I", hdr, 0x20, 8363)
        t = title.encode("ascii")[:28]
        hdr[0x30 : 0x30 + len(t)] = t
        hdr[0x4C:0x50] = b"SCRS"
        return bytes(hdr)

    buf[ins1_off : ins1_off + 80] = make_ins(smp1_off, True, "loop")
    buf[ins2_off : ins2_off + 80] = make_ins(smp2_off, False, "oneshot")

    hdr = bytearray(96)
    name = b"FX fixture"
    hdr[0 : len(name)] = name
    hdr[28] = 0x1A
    hdr[29] = 16
    struct.pack_into("<HHHHHH", hdr, 32, ord_num, ins_num, pat_num, 0, 0x1301, 2)
    hdr[44:48] = b"SCRM"
    hdr[48] = 64
    hdr[49] = 6
    hdr[50] = 125
    hdr[51] = 48
    hdr[53] = 252
    for i in range(32):
        hdr[64 + i] = i if i < 18 else 255
    assert len(hdr) == 96
    buf[0:96] = hdr
    buf[96] = 0
    buf[97] = 255
    struct.pack_into("<HH", buf, 98, pp(ins1_off), pp(ins2_off))
    struct.pack_into("<H", buf, 102, pp(pat_off))
    for i in range(32):
        buf[104 + i] = 0x08 if i < 18 else 0

    path.write_bytes(bytes(buf))
    print(f"Wrote {path} ({len(buf)} bytes)")


def it_volume_column(volcmd: str | None, vol: int | None) -> int | None:
    if volcmd is None and vol is None:
        return None
    if volcmd == "volume" or (volcmd is None and vol is not None):
        return max(0, min(64, int(vol or 0)))
    base = {"fineup": 65, "finedown": 75, "volup": 85, "voldown": 95}[volcmd]
    return base + min(int(vol or 0), 9)


def pack_it_pattern(cells: list[list[dict]]) -> bytes:
    out = bytearray()
    for row in range(ROWS):
        for ch in range(CHANNELS):
            c = cells[row][ch]
            if not c:
                continue
            note = c.get("note")
            ins = c.get("ins")
            volcmd = c.get("volcmd")
            vol = c.get("vol") if ("vol" in c or volcmd) else None
            fx = c.get("fx")
            param = c.get("param")
            volb = it_volume_column(volcmd, vol if vol is not None else (0 if volcmd == "volume" else None))
            if volcmd is None and "vol" in c:
                volb = it_volume_column("volume", c["vol"])

            it_note = None
            if note is not None:
                it_note = 255 if note == 0xFF else (254 if note in (0xFE, 0xFD) else note)

            mask = 0
            body = bytearray()
            if it_note is not None:
                mask |= 1
                body.append(it_note & 0xFF)
            if ins is not None:
                mask |= 2
                body.append(ins & 0xFF)
            if volb is not None:
                mask |= 4
                body.append(volb & 0xFF)
            if fx is not None:
                mask |= 8
                body.append(fx & 0xFF)
                body.append((param or 0) & 0xFF)
            if mask == 0:
                continue
            out.append(0x80 | ((ch + 1) & 0x7F))
            out.append(mask)
            out.extend(body)
        out.append(0)

    packed = bytes(out)
    return struct.pack("<HH4x", len(packed), ROWS) + packed


def write_it(path: Path) -> None:
    cells = build_cells(s3m=False)
    pattern = pack_it_pattern(cells)
    sample = bytes([0x40] * 10)

    numord, numins, numsmp, numpat = 2, 2, 2, 1
    INS_SIZE, SMP_HDR = 554, 80
    base = 0xC0 + numord + 4 * numins + 4 * numsmp + 4 * numpat
    ins_off = base
    smp_hdr_off = ins_off + INS_SIZE * numins
    smp_data1 = smp_hdr_off + SMP_HDR * numsmp
    smp_data2 = smp_data1 + 10
    pat_off = smp_data2 + 10

    instr_blob = bytearray()
    for i in range(numins):
        ins = bytearray(INS_SIZE)
        ins[0:4] = b"IMPI"
        for n in range(120):
            ins[0x40 + n * 2] = n
            ins[0x40 + n * 2 + 1] = i + 1
        instr_blob.extend(ins)

    def make_smp_hdr(data_offset: int, loop: bool, name: bytes) -> bytes:
        h = bytearray(SMP_HDR)
        h[0:4] = b"IMPS"
        h[4:16] = b"SAMPLE.RAW\x00\x00"
        h[0x11] = 64
        h[0x12] = 0x01 | (0x10 if loop else 0)
        h[0x13] = 64
        h[0x14 : 0x14 + len(name)] = name[:26]
        h[0x2E] = 1
        struct.pack_into("<III", h, 0x30, 10, 0, 10 if loop else 0)
        struct.pack_into("<I", h, 0x3C, 8363)
        struct.pack_into("<I", h, 0x48, data_offset)
        return bytes(h)

    hdr = bytearray(192)  # 0xC0 IT header
    hdr[0:4] = b"IMPM"
    name = b"FX fixture"
    for i, b in enumerate(name):
        hdr[4 + i] = b
    struct.pack_into("<HHHH", hdr, 0x20, numord, numins, numsmp, numpat)
    struct.pack_into("<HH", hdr, 0x28, 0x0217, 0x0200)
    struct.pack_into("<HH", hdr, 0x2C, 0x0008, 0)
    hdr[0x30] = 128
    hdr[0x31] = 48
    hdr[0x32] = 6
    hdr[0x33] = 125
    hdr[0x34] = 128
    for i in range(64):
        hdr[0x40 + i] = 32 if i < CHANNELS else 160
        hdr[0x80 + i] = 64 if i < CHANNELS else 0
    assert len(hdr) == 192

    buf = bytearray()
    buf.extend(hdr)
    buf.extend(bytes([0, 0xFF]))
    buf.extend(struct.pack(f"<{numins}I", *[ins_off + i * INS_SIZE for i in range(numins)]))
    buf.extend(struct.pack(f"<{numsmp}I", *[smp_hdr_off + i * SMP_HDR for i in range(numsmp)]))
    buf.extend(struct.pack("<I", pat_off))
    assert len(buf) == base
    buf.extend(instr_blob)
    buf.extend(make_smp_hdr(smp_data1, True, b"loop") + make_smp_hdr(smp_data2, False, b"oneshot"))
    buf.extend(sample + sample)
    assert len(buf) == pat_off
    buf.extend(pattern)

    path.write_bytes(bytes(buf))
    print(f"Wrote {path} ({len(buf)} bytes)")


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    write_s3m(OUT_DIR / "FX.S3M")
    write_it(OUT_DIR / "FX.IT")


if __name__ == "__main__":
    main()
