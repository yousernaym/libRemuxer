A C++ library that converts tracker modules and sid files to midi and/or wav. Used by [Remuxer](https://github.com/yousernaym/remuxer).

### Module conversion
[libopenmpt](https://lib.openmpt.org/libopenmpt) is used for both MIDI note extraction and WAV rendering. Note extraction uses libopenmpt pattern data together with a small set of Visual Music read-only accessors in the openmpt fork for instrument and sample metadata.

### SID conversion
[libsidplayfp](https://github.com/yousernaym/libsidplayfp) is used to emulate playback of sid files in a C64 environment. The source code was modified to get access to the sid registers with information about note frequencies, volume envelopes etc (some of this was later implemented in the official repo).
