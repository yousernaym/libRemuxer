#!/usr/bin/env python3
"""Minimal S3M / IT writers used to generate semantic twins of the hand-authored XM fixtures.

Shared by gen_fx_fixtures.py and gen_mod_transport_fixtures.py. These write just enough of each
format for libopenmpt to load the module and report the same pattern/order layout as the XM
original; they are not general-purpose tracker writers.

Cells are dicts in "OpenMPT space" so one description can drive both formats:

    {"note": 61, "ins": 1, "fx": FX_D, "param": 0x02, "volcmd": "voldown", "vol": 2}

`note` is an OpenMPT note number (XM file note + 12); `fx` is an S3M/IT effect letter index (A=1).
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field

# S3M / IT effect letters (A=1).
FX_A, FX_B, FX_C, FX_D, FX_G, FX_J, FX_K, FX_L, FX_Q, FX_S, FX_T = 1, 2, 3, 4, 7, 10, 11, 12, 17, 19, 20

ORDER_END = 0xFF


@dataclass
class Sample:
    """One PCM sample. `data` defaults to a constant-level blob of `length` bytes."""

    length: int = 10
    loop: bool = True
    name: str = "loop"
    data: bytes | None = None

    def blob(self) -> bytes:
        return self.data if self.data is not None else bytes([0x40] * self.length)


@dataclass
class Module:
    """Format-agnostic description of a fixture module."""

    patterns: list[list[list[dict]]]  # [pattern][row][channel] -> cell dict
    orders: list[int]  # pattern index per order slot (ORDER_END is appended)
    channels: int = 18
    rows: int = 64
    speed: int = 6
    tempo: int = 125
    samples: list[Sample] = field(default_factory=lambda: [Sample()])
    title: str = "FX fixture"

    def order_bytes(self) -> bytes:
        return bytes(list(self.orders) + [ORDER_END])


def empty_pattern(rows: int, channels: int) -> list[list[dict]]:
    return [[{} for _ in range(channels)] for _ in range(rows)]


# ---------------------------------------------------------------------------
# Note encoding
# ---------------------------------------------------------------------------

# S3M note byte: (octave << 4) | semitone with semitone in 0..11 (ST3 / OpenMPT writer).
# OpenMPT Load_s3m: ompt = (s3m&0x0F) + 12*(s3m>>4) + 12 + NOTE_MIN  (NOTE_MIN=1)
# → ompt = low + 12*oct + 13, so x = ompt - 13 encodes as (x//12)<<4 | (x%12). C-0 is 0x00.
def ompt_to_s3m_note(n: int) -> int:
    if n == 0xFF or n in (0xFE, 0xFD):
        return 254  # s3mNoteOff
    x = n - 13
    if x < 0:
        raise ValueError(f"OpenMPT note {n} too low for S3M")
    octv = x // 12
    low = x % 12
    return (octv << 4) | low


# OpenMPT Load_it: file note < 0x80 → note += NOTE_MIN (1).
def ompt_to_it_note(n: int) -> int:
    if n == 0xFF:
        return 255  # NOTE_KEYOFF
    if n in (0xFE, 0xFD):
        return 254  # NOTE_NOTECUT
    return n - 1


def it_volume_column(volcmd: str | None, vol: int | None) -> int | None:
    if volcmd is None and vol is None:
        return None
    if volcmd == "volume" or (volcmd is None and vol is not None):
        return max(0, min(64, int(vol or 0)))
    base = {"fineup": 65, "finedown": 75, "volup": 85, "voldown": 95}[volcmd]
    return base + min(int(vol or 0), 9)


# ---------------------------------------------------------------------------
# S3M
# ---------------------------------------------------------------------------


def pack_s3m_pattern(cells: list[list[dict]], rows: int, channels: int) -> bytes:
    out = bytearray()
    for row in range(rows):
        for ch in range(channels):
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


def write_s3m(path, mod: Module) -> None:
    patterns = [pack_s3m_pattern(p, mod.rows, mod.channels) for p in mod.patterns]
    orders = mod.order_bytes()

    ord_num, ins_num, pat_num = len(orders), len(mod.samples), len(patterns)
    after_header = 96 + ord_num + ins_num * 2 + pat_num * 2 + 32
    buf = bytearray(after_header)

    def pad16():
        while len(buf) % 16:
            buf.append(0)

    def pp(off: int) -> int:
        return off // 16

    # Layout: instrument headers, then patterns, then sample data — each 16-byte aligned
    # because S3M addresses them all as "parapointers" (offset / 16).
    ins_offs = []
    for _ in range(ins_num):
        pad16()
        ins_offs.append(len(buf))
        buf.extend(b"\x00" * 80)
    pat_offs = []
    for pattern in patterns:
        pad16()
        pat_offs.append(len(buf))
        buf.extend(pattern)
    smp_offs = []
    for sample in mod.samples:
        pad16()
        smp_offs.append(len(buf))
        buf.extend(sample.blob())
    pad16()

    def make_ins(smp_off: int, sample: Sample) -> bytes:
        ptr = pp(smp_off)
        hdr = bytearray(80)
        hdr[0] = 1
        hdr[1:13] = b"SAMPLE.RAW\x00\x00"
        hdr[13] = (ptr >> 16) & 0xFF
        struct.pack_into("<H", hdr, 14, ptr & 0xFFFF)
        struct.pack_into("<III", hdr, 0x10, sample.length, 0, sample.length if sample.loop else 0)
        hdr[0x1C] = 64
        hdr[0x1F] = 1 if sample.loop else 0
        struct.pack_into("<I", hdr, 0x20, 8363)
        t = sample.name.encode("ascii")[:28]
        hdr[0x30 : 0x30 + len(t)] = t
        hdr[0x4C:0x50] = b"SCRS"
        return bytes(hdr)

    for off, sample_off, sample in zip(ins_offs, smp_offs, mod.samples):
        buf[off : off + 80] = make_ins(sample_off, sample)

    hdr = bytearray(96)
    name = mod.title.encode("ascii")[:28]
    hdr[0 : len(name)] = name
    hdr[28] = 0x1A
    hdr[29] = 16
    struct.pack_into("<HHHHHH", hdr, 32, ord_num, ins_num, pat_num, 0, 0x1301, 2)
    hdr[44:48] = b"SCRM"
    hdr[48] = 64
    hdr[49] = mod.speed
    hdr[50] = mod.tempo
    hdr[51] = 48
    hdr[53] = 252
    for i in range(32):
        hdr[64 + i] = i if i < mod.channels else 255
    assert len(hdr) == 96
    buf[0:96] = hdr
    buf[96 : 96 + ord_num] = orders
    struct.pack_into(f"<{ins_num}H", buf, 96 + ord_num, *[pp(o) for o in ins_offs])
    struct.pack_into(f"<{pat_num}H", buf, 96 + ord_num + ins_num * 2, *[pp(o) for o in pat_offs])
    pan_off = 96 + ord_num + ins_num * 2 + pat_num * 2
    for i in range(32):
        buf[pan_off + i] = 0x08 if i < mod.channels else 0

    path.write_bytes(bytes(buf))
    print(f"Wrote {path} ({len(buf)} bytes)")


# ---------------------------------------------------------------------------
# IT
# ---------------------------------------------------------------------------


def pack_it_pattern(cells: list[list[dict]], rows: int, channels: int) -> bytes:
    out = bytearray()
    for row in range(rows):
        for ch in range(channels):
            c = cells[row][ch]
            if not c:
                continue
            note = c.get("note")
            ins = c.get("ins")
            fx = c.get("fx")
            param = c.get("param")
            volb = it_volume_column(c.get("volcmd"), c.get("vol"))

            it_note = None
            if note is not None:
                it_note = ompt_to_it_note(note)

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
    return struct.pack("<HH4x", len(packed), rows) + packed


def write_it(path, mod: Module) -> None:
    patterns = [pack_it_pattern(p, mod.rows, mod.channels) for p in mod.patterns]
    orders = mod.order_bytes()

    numord, numsmp, numpat = len(orders), len(mod.samples), len(patterns)
    numins = numsmp  # one instrument per sample, mapping every note to that sample
    INS_SIZE, SMP_HDR = 554, 80
    base = 0xC0 + numord + 4 * numins + 4 * numsmp + 4 * numpat
    ins_off = base
    smp_hdr_off = ins_off + INS_SIZE * numins
    smp_data_offs = []
    cursor = smp_hdr_off + SMP_HDR * numsmp
    for sample in mod.samples:
        smp_data_offs.append(cursor)
        cursor += sample.length
    pat_offs = []
    for pattern in patterns:
        pat_offs.append(cursor)
        cursor += len(pattern)

    instr_blob = bytearray()
    for i in range(numins):
        ins = bytearray(INS_SIZE)
        ins[0:4] = b"IMPI"
        for n in range(120):
            ins[0x40 + n * 2] = n
            ins[0x40 + n * 2 + 1] = i + 1
        instr_blob.extend(ins)

    def make_smp_hdr(data_offset: int, sample: Sample) -> bytes:
        h = bytearray(SMP_HDR)
        h[0:4] = b"IMPS"
        h[4:16] = b"SAMPLE.RAW\x00\x00"
        h[0x11] = 64
        h[0x12] = 0x01 | (0x10 if sample.loop else 0)
        h[0x13] = 64
        name = sample.name.encode("ascii")[:26]
        h[0x14 : 0x14 + len(name)] = name
        h[0x2E] = 1
        struct.pack_into("<III", h, 0x30, sample.length, 0, sample.length if sample.loop else 0)
        struct.pack_into("<I", h, 0x3C, 8363)
        struct.pack_into("<I", h, 0x48, data_offset)
        return bytes(h)

    hdr = bytearray(192)  # 0xC0 IT header
    hdr[0:4] = b"IMPM"
    name = mod.title.encode("ascii")[:26]
    for i, b in enumerate(name):
        hdr[4 + i] = b
    struct.pack_into("<HHHH", hdr, 0x20, numord, numins, numsmp, numpat)
    struct.pack_into("<HH", hdr, 0x28, 0x0217, 0x0200)
    struct.pack_into("<HH", hdr, 0x2C, 0x0008, 0)
    hdr[0x30] = 128
    hdr[0x31] = 48
    hdr[0x32] = mod.speed
    hdr[0x33] = mod.tempo
    hdr[0x34] = 128
    for i in range(64):
        hdr[0x40 + i] = 32 if i < mod.channels else 160
        hdr[0x80 + i] = 64 if i < mod.channels else 0
    assert len(hdr) == 192

    buf = bytearray()
    buf.extend(hdr)
    buf.extend(orders)
    buf.extend(struct.pack(f"<{numins}I", *[ins_off + i * INS_SIZE for i in range(numins)]))
    buf.extend(struct.pack(f"<{numsmp}I", *[smp_hdr_off + i * SMP_HDR for i in range(numsmp)]))
    buf.extend(struct.pack(f"<{numpat}I", *pat_offs))
    assert len(buf) == base
    buf.extend(instr_blob)
    for off, sample in zip(smp_data_offs, mod.samples):
        buf.extend(make_smp_hdr(off, sample))
    for sample in mod.samples:
        buf.extend(sample.blob())
    for pattern in patterns:
        buf.extend(pattern)
    assert len(buf) == cursor

    path.write_bytes(bytes(buf))
    print(f"Wrote {path} ({len(buf)} bytes)")
