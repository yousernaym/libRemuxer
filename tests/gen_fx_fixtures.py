#!/usr/bin/env python3
"""Generate FX.S3M and FX.IT as semantic twins of FX.XM for openmpt-based fixture tests."""

from __future__ import annotations

from pathlib import Path

from mod_fixture_writer import (
    FX_D,
    FX_G,
    FX_J,
    FX_K,
    FX_L,
    FX_Q,
    FX_S,
    Module,
    Sample,
    empty_pattern,
    write_it,
    write_s3m,
)

OUT_DIR = Path(__file__).resolve().parent.parent / "test-files"

# OpenMPT-space notes (XM file note + 12).
NOTES = [61, 62, 63, 64, 65, 66, 67, 68, 69, 71, 72, 89, 73, 74, 75, 77, 79, 80]
CH14_N1 = 76
CH15_N1 = 78
ROWS = 64
CHANNELS = 18


def build_cells(s3m: bool) -> list[list[dict]]:
    cells = empty_pattern(ROWS, CHANNELS)

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

    # Fine vol down 2, then D00 memory (DF0 would be read as normal slide *up* F).
    setc(0, 10, fx=FX_D, param=0xF2)
    for r in range(1, 32):
        setc(r, 10, fx=FX_D, param=0x00)

    setc(0, 11, fx=FX_J, param=0x25)
    setc(1, 11, note=0xFF)
    setc(1, 12, note=NOTES[12], ins=1)
    setc(0, 13, fx=FX_Q, param=0x02)
    setc(1, 13, note=0xFF)
    setc(1, 14, note=CH14_N1, ins=1, fx=FX_G, param=0x00)
    setc(1, 15, note=CH15_N1, ins=1, fx=FX_L, param=0x00)

    # Ch16: match XM vol-column ±F then +1 so volume hits 0 then revives.
    # Use effect-column Dxy (IT vol column only encodes slides 0..9).
    # DF0 = normal slide up F (hi=F,lo=0); D0F = normal slide down F; D10 = up 1.
    setc(0, 16, fx=FX_D, param=0xF0)
    setc(1, 16, fx=FX_D, param=0x0F)
    setc(2, 16, fx=FX_D, param=0x10)
    if s3m:
        setc(0, 17, vol=0)
        setc(1, 17, fx=FX_D, param=0x10)
    else:
        setc(0, 17, volcmd="volume", vol=0)
        setc(1, 17, volcmd="volup", vol=1)

    return cells


def module(s3m: bool) -> Module:
    return Module(
        patterns=[build_cells(s3m)],
        orders=[0],
        channels=CHANNELS,
        rows=ROWS,
        speed=6,
        tempo=125,
        samples=[Sample(10, True, "loop"), Sample(10, False, "oneshot")],
        title="FX fixture",
    )


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    write_s3m(OUT_DIR / "FX.S3M", module(s3m=True))
    write_it(OUT_DIR / "FX.IT", module(s3m=False))


if __name__ == "__main__":
    main()
