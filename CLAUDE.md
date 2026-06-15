# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Role

`libRemuxer` is the native C++ engine (`libRemuxer.dll`) that converts tracker modules, SID files, and HVL
files into MIDI + WAV. It is the bottom layer of **Remuxer** (see [../AGENTS.md](../AGENTS.md)): a git
submodule nested inside the Remuxer submodule (separate repo: `yousernaym/libRemuxer`).

It is consumed only by Remuxer's C# front-end, which P/Invokes the exported C functions
([../Remuxer/LibRemuxer.cs](../Remuxer/LibRemuxer.cs)). It does **not** link against Visual Music directly;
that separation exists because of the libsidplayfp/MonoGame license conflict.

## The C Contract (`libRemuxer.h`)

Five exported functions drive a begin -> process* -> end lifecycle, called in order by the C# side:

| Export | What it does |
|---|---|
| `initLib()` | Allocates fixed-size internal buffers. |
| `beginProcessing(UserArgs&)` | Loads the input file and extracts everything needed. Tries `ModReader` first; if it throws, falls back to `HvlReader`, then `SidReader`. Returns `FALSE` if all fail. |
| `process()` | Called repeatedly to render audio frame-by-frame. Returns progress `0..1`, or `-1` when finished. |
| `endProcessing()` | Finalizes: builds the note list and saves WAV/MIDI files. |
| `closeLib()` | Frees buffers from `initLib()`. |

`UserArgs` carries the input/audio/midi paths, the per-instrument flag, SID song length, and sub-song index.
`numSubSongs` and `subSong` are out params written back through `UserArgs&` so the C# app can prompt the user
to pick a SID/HVL sub-song.

Only `UserArgs` crosses the C# / C++ boundary. Keep its `#pragma pack(push, 8)` layout in sync with Remuxer's
C# `[StructLayout]` mirror in [../Remuxer/Program.cs](../Remuxer/Program.cs) and the P/Invoke declaration in
[../Remuxer/LibRemuxer.cs](../Remuxer/LibRemuxer.cs). `SongData`, `SongTrack`, `SongNote`, and `TempoEvent`
are internal buffers used to write MIDI and WAV outputs.

## Code Map

Only the handful of files in the repo root are Visual-Music-authored project code; everything in subdirectories
is vendored. All paths are relative to this directory.

| File | Responsibility |
|---|---|
| [libRemuxer.cpp](libRemuxer.cpp) | Exported entry points plus global state (`songData`, readers, `songReader`). The mod-first/HVL/SID fallback dispatch lives here. |
| [libRemuxer.h](libRemuxer.h) | C ABI exports, `UserArgs`, internal MIDI-output structs, and `MAX_*` size limits. |
| [SongReader.h](SongReader.h) | Abstract base for the readers; owns `UserArgs`, `audioBuffer`, and `Wav`. 48 kHz. |
| [ModReader.h](ModReader.h) / [ModReader.cpp](ModReader.cpp) | Tracker-module conversion (MOD/XM/S3M/IT...). The bulk of the effect-handling logic. |
| [HvlReader.h](HvlReader.h) / [HvlReader.cpp](HvlReader.cpp) | HVL/AHX conversion. |
| [SidReader.h](SidReader.h) / [SidReader.cpp](SidReader.cpp) | SID conversion via C64 emulation. |
| [Song.h](Song.h) / [Song.cpp](Song.cpp) | Accumulates per-track/per-tick note state, turns it into `SongNote`s, and hand-writes the MIDI file. |
| [FileFormat.h](FileFormat.h) / [FileFormat.cpp](FileFormat.cpp) | Binary-writing base class shared by `Song` and `Wav`. |
| [Wav.h](Wav.h) | Accumulates float samples, normalizes to -1 dB, writes the RIFF/WAV file. |

## Conversion Flow

Both readers fill `Song::tracks[ch].ticks[]` with `RunningTickInfo` (pitch/start/vol/instrument per tick).
`Song::createNoteList()` collapses runs of ticks into `SongNote`s (start/stop/pitch/channel), rescales to
480 ticks/beat, and `saveMidiFile()` writes a standard MIDI format-1 file by hand.

Modules use libmikmod for pattern/note extraction and libopenmpt for WAV rendering. SID uses libsidplayfp and
samples SID chip note state during `process()`. HVL/AHX uses the HVL replay code.

## Vendored Third-Party Code

The subdirectories are large external libraries, all built as their own projects within the repo-root
`VisualMusic.sln` and linked into this DLL. Change only what Visual Music requires:

- `mikmod/` - libmikmod, note extraction.
- `openmpt/` - libopenmpt, module audio rendering.
- `libsidplayfp/` - a fork modified to expose `NoteState` / `getNoteState()` so SID registers can be read per voice.

## Build & Output

[libRemuxer.vcxproj](libRemuxer.vcxproj) builds a DynamicLibrary with the v143 toolset, C++20 on x64, statically
linked CRT, and Spectre mitigation on Release x64. Build via the repo-root `VisualMusic.sln` so dependency
projects are built first; see [../../../AGENTS.md](../../../AGENTS.md) for the whole picture.
