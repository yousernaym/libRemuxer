# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Role

`libRemuxer` is the **native C++ engine** (`libRemuxer.dll`) that does the actual work of converting tracker
modules and SID files into MIDI + WAV. It is the bottom layer of **Remuxer** (see [../CLAUDE.md](../CLAUDE.md)):
a git submodule nested inside the Remuxer submodule (separate repo: `yousernaym/libRemuxer`).

It is consumed only by Remuxer's C# front-end, which P/Invokes the exported C functions
([../Remuxer/LibRemuxer.cs](../Remuxer/LibRemuxer.cs)). It does **not** link against Visual Music directly —
that separation exists because of the libsidplayfp/MonoGame license conflict.

## The C contract (`libRemuxer.h`)

Five exported functions drive a begin → process* → end lifecycle, called in order by the C# side:

| Export | What it does |
|---|---|
| `initLib()` | Allocates all the fixed-size marshal buffers (one-time). |
| `beginProcessing(UserArgs&)` | Loads the input file and extracts everything needed. **Tries `ModReader` first; if it throws, falls back to `SidReader`.** Returns `FALSE` if both fail. |
| `process()` | Called repeatedly to render audio frame-by-frame. Returns progress `0..1`, or `-1` when finished. |
| `endProcessing()` | Finalizes — builds the note list and saves the WAV/MIDI files. |
| `closeLib()` | Frees the buffers from `initLib`. |

`UserArgs` carries the input/audio/midi paths, the per-instrument flag, SID song length and sub-song index.
**`numSubSongs` and `subSong` are out-params** written back through the `UserArgs&` so the C# app can prompt
the user to pick a SID sub-song.

## Code map (the Visual-Music-authored files)

Only the handful of files in the repo root are project code; everything in subdirectories is vendored
(see below). All paths are relative to this directory.

| File | Responsibility |
|---|---|
| [libRemuxer.cpp](libRemuxer.cpp) | The exported entry points + global state (`marshalSong`, the two readers, `songReader` pointer). The mod-first/sid-fallback dispatch lives here. |
| [libRemuxer.h](libRemuxer.h) | C ABI: the exports plus the `Marshal_*` / `UserArgs` marshaling structs and `MAX_*` size limits. |
| [SongReader.h](SongReader.h) | Abstract base for the two readers — owns the `UserArgs`, the `audioBuffer`, and the `Wav`. 48 kHz. |
| [ModReader.h](ModReader.h) / [ModReader.cpp](ModReader.cpp) | Tracker-module conversion (MOD/XM/S3M/IT…). The bulk of the effect-handling logic. |
| [SidReader.h](SidReader.h) / [SidReader.cpp](SidReader.cpp) | SID conversion via C64 emulation. |
| [Song.h](Song.h) / [Song.cpp](Song.cpp) | Accumulates per-track/per-tick note state, turns it into `Marshal_Note`s, and hand-writes the MIDI file. |
| [FileFormat.h](FileFormat.h) / [FileFormat.cpp](FileFormat.cpp) | Binary-writing base class (`writeLE`/`writeBE`/chunks) shared by `Song` and `Wav`. |
| [Wav.h](Wav.h) | Accumulates float samples, normalizes to −1 dB, writes the RIFF/WAV file. |

## How conversion works

The two formats share the `Song`/`Wav` output path but extract notes very differently — and the **timing of
note extraction vs. audio rendering is asymmetric**:

- **Modules (`ModReader`)** use *two* libraries on purpose (see [README.md](README.md)): **libmikmod** parses the
  pattern data to recover notes — including effects, volume envelopes and sample lengths needed to know when a
  note actually stops — while **libopenmpt** renders the audio for the WAV (better quality than mikmod).
  Note extraction happens entirely up-front in `beginProcessing()` (`extractNotes()` walks every
  pattern/row/channel synchronously); `process()` afterwards only pumps openmpt for audio. mikmod runs on its
  "nosound" driver (`drv_nos`) — it's a parser here, not a player. Much of `ModReader.cpp` is per-effect
  handling: speed/tempo, arpeggio, retrigger, pattern jump/break/loop, pattern delay, note delay/cut, volume
  slides, sample offset.

- **SID (`SidReader`)** emulates the file in a C64 via **libsidplayfp** (a fork — see below). Here `process()`
  does double duty: it plays audio *and*, each tick, samples the SID chip registers via the fork's
  `getNoteState()` to read frequency/waveform/volume/gate for each of the 3 voices → 3 tracks. The note list is
  built in `endProcessing()`. Frequency → MIDI pitch is a `log2` mapping; default length is 300 s + 7 s fadeout.
  Requires the C64 ROMs in [roms/](roms/) (kernal/basic/chargen) at runtime.

Both readers fill `Song::tracks[ch].ticks[]` with `RunningTickInfo` (pitch/start/vol/instrument per tick).
`Song::createNoteList()` then collapses runs of ticks into `Marshal_Note`s (start/stop/pitch/channel), rescales
to 480 ticks/beat, computes min/max pitch, and `saveMidiFile()` writes a standard **MIDI format-1** file by hand
(MThd/MTrk chunks, variable-length delta times, tempo + time-sig meta events on track 0, note on/off on 1+).

## Marshaling contract — keep in sync with the C# side

`initLib()` pre-allocates everything as **fixed-size arrays** sized by the `MAX_*` constants in
[libRemuxer.h](libRemuxer.h) (`MAX_TEMPOEVENTS`, `MAX_MIDITRACKS`, `MAX_TRACKNOTES`, `MAX_TRACKNAME_LENGTH`);
the readers fill these in place and throw a `std::string` if a limit is exceeded. The `Marshal_*` and `UserArgs`
structs are `#pragma pack(push, 8)` and are read directly by Remuxer's C# `[StructLayout]` mirrors — **any
change to their field layout, order, or the pack value must be matched in
[../Remuxer/LibRemuxer.cs](../Remuxer/LibRemuxer.cs) or the P/Invoke boundary breaks silently.**

## Vendored third-party code — treat as upstream

The subdirectories are large external libraries, all built as their own projects within the repo-root
`VisualMusic.sln` and linked into this DLL. Change only what Visual Music requires:

- **`mikmod/`** — libmikmod (nested submodule), note extraction.
- **`openmpt/`** — libopenmpt, module audio rendering (pulls in mpg123/ogg/vorbis).
- **`libsidplayfp/`** — a **fork** of libsidplayfp (nested submodule) modified to expose `NoteState` /
  `getNoteState()` so the SID registers can be read per voice — this accessor is the crux of SID→MIDI and is the
  one piece of "vendored" code that is genuinely ours. (Some vcxproj `ClInclude` entries still point at an older
  `libsidplayfp-readNotes\` path; the active x64 include path is `libsidplayfp\src`.)

## Build & output

[libRemuxer.vcxproj](libRemuxer.vcxproj) builds a **DynamicLibrary** with the v143 toolset, C++20 on x64
(the real target; Win32 is C++17 and effectively legacy), statically linked CRT, Spectre mitigation on
Release x64. It links the vendored `.lib`s (libmikmod, libopenmpt + ogg/vorbis/mpg123, libsidplayfp). The
post-build copies [roms/](roms/) next to the eventual exe; Remuxer's own post-build then gathers
`libRemuxer.dll` (and the other native DLLs) beside `Remuxer.exe`. Build via the repo-root `VisualMusic.sln`
so the dependency projects are built first — see [../../../CLAUDE.md](../../../CLAUDE.md) for the whole picture.
