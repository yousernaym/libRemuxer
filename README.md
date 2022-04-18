A C++ library that converts tracker modules and sid files to midi and/or wav. Used by [Remuxer](https://github.com/yousernaym/remuxer).

### Module conversion
For conversion to midi, [libmikmod](https://github.com/sezero/mikmod) is used to get information about the notes as well as volume-altering effects, volume envelopes and sample lenghts to determine when notes stop playing. Also takes into account variouus relevant effects like note repeats, arpeggios, pattern jumping etc.  

For conversion to wav, libmikmod was replaced with [libopenmpt](https://lib.openmpt.org/libopenmpt) for more accurote playback and better sound quality. libmikmod is still used for note extraction because this is more complicated to change and it works fine.

### SID conversion
[libsidplayfp](https://github.com/yousernaym/libsidplayfp) is used to emulate playback of sid files in a C64 environment. The source code was modified to get access to the sid registers with information about note frequencies, volume envelopes etc (some of this was later implemented in the official repo).