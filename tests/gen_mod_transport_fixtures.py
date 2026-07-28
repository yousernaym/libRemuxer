#!/usr/bin/env python3
"""Generate S3M / IT semantic twins of the hand-authored test-files/mod-transport/*.XM fixtures.

Each fixture isolates one transport (flow-control) effect. The XM originals are authored in a
tracker; these twins reproduce the same playback semantics with the S3M/IT spelling of the effect,
so ModTransportFixtureTests and ModTransportMidiTests can run the same expectations against all
three format paths through ModReader.

    XM              S3M / IT
    Bxx  position jump   Bxx
    Dxx  pattern break   Cxx
    EEx  pattern delay   SEx
    E6x  pattern loop    SBx
    Fxx  speed / tempo   Axx (speed) + Txx (tempo)

Shared layout with the XM originals: 18 channels, 64 rows/pattern, speed 6, tempo 125, effects in
ch0, note in ch1, instrument 1 = one looping sample (so notes sustain until retriggered or cut).
"""

from __future__ import annotations

from pathlib import Path

from mod_fixture_writer import (
    FX_A,
    FX_B,
    FX_C,
    FX_D,
    FX_S,
    FX_T,
    Module,
    Sample,
    empty_pattern,
    write_it,
    write_s3m,
)

OUT_DIR = Path(__file__).resolve().parent.parent / "test-files" / "mod-transport"

ROWS = 64
CHANNELS = 18
NOTE = 61  # OpenMPT space = XM file note 49 + 12; middle C, MIDI pitch 60
SAMPLES = [Sample(10, True, "loop")]


def module(patterns, orders, title: str) -> Module:
    return Module(
        patterns=patterns,
        orders=orders,
        channels=CHANNELS,
        rows=ROWS,
        speed=6,
        tempo=125,
        samples=SAMPLES,
        title=title,
    )


def pattern(*cells: tuple[int, int, dict]) -> list[list[dict]]:
    """One 64x18 pattern from (row, channel, cell) triples."""
    p = empty_pattern(ROWS, CHANNELS)
    for row, ch, cell in cells:
        p[row][ch].update(cell)
    return p


def note_cell(**extra) -> dict:
    return {"note": NOTE, "ins": 1, **extra}


# B01 on order 0 jumps to order 1, whose row 0 carries the note (module tick 6).
def pattern_jump(s3m: bool) -> Module:
    return module(
        patterns=[
            pattern((0, 0, {"fx": FX_B, "param": 0x01})),
            pattern((0, 1, note_cell())),
        ],
        orders=[0, 1],
        title="pattern jump",
    )


# C01 on order 0 breaks to row 1 of order 1, which plays a note and breaks again — this time with
# B03 in the note channel, so the break's row (1) combines with the jump's order (3).
def pattern_break(s3m: bool) -> Module:
    return module(
        patterns=[
            pattern((0, 0, {"fx": FX_C, "param": 0x01})),
            pattern(
                (1, 0, {"fx": FX_C, "param": 0x01}),
                (1, 1, note_cell(fx=FX_B, param=0x03)),
            ),
            pattern(),  # order 2 is skipped by the jump
            pattern((1, 1, note_cell())),
        ],
        orders=[0, 1, 2, 3],
        title="pattern break",
    )


# SE2 repeats row 0 twice more; the note slides down 4 per tick until it hits zero volume.
# S3M cannot slide from the volume column, so it uses effect D04 (identical in ModReader).
def row_repeat(s3m: bool) -> Module:
    slide = {"fx": FX_D, "param": 0x04} if s3m else {"volcmd": "voldown", "vol": 4}
    return module(
        patterns=[
            pattern(
                (0, 0, {"fx": FX_S, "param": 0xE2}),
                (0, 1, note_cell(**slide)),
            )
        ],
        orders=[0],
        title="row repeat",
    )


# SB0 marks the loop start on row 1 (which carries the note); SB2 on row 2 loops back twice.
def pattern_loop(s3m: bool) -> Module:
    return module(
        patterns=[
            pattern(
                (1, 0, {"fx": FX_S, "param": 0xB0}),
                (1, 1, note_cell()),
                (2, 0, {"fx": FX_S, "param": 0xB2}),
            )
        ],
        orders=[0],
        title="pattern loop",
    )


# A03 shortens row 0 to 3 ticks, so the note on row 1 starts at module tick 3, where T20 also
# drops the tempo from 125 to 32.
def speed_tempo(s3m: bool) -> Module:
    return module(
        patterns=[
            pattern(
                (0, 0, {"fx": FX_A, "param": 0x03}),
                (1, 0, {"fx": FX_T, "param": 0x20}),
                (1, 1, note_cell()),
            )
        ],
        orders=[0],
        title="speed tempo",
    )


FIXTURES = [
    ("pattern-jump-BXX", pattern_jump),
    ("pattern-break-CXX", pattern_break),
    ("row-repeat-SEX", row_repeat),
    ("loop-SBX", pattern_loop),
    ("speed-tempo-AXX-TXX", speed_tempo),
]


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for stem, build in FIXTURES:
        write_s3m(OUT_DIR / f"{stem}.S3M", build(s3m=True))
        write_it(OUT_DIR / f"{stem}.IT", build(s3m=False))


if __name__ == "__main__":
    main()
