A dynamic C++ library that converts tracker modules and sid files to midi and wav. Used by [Remuxer](https://github.com/yousernaym/remuxer).

## How the code works
### Modules
For conversion to midi, libmikmod is used to get information about the notes as well as volume-altering effects, volume envelopes and sample lenghts to determine when notes stop playing. Also takes into account variouus relevant effects like note repats, arpeggios, pattern jumping etc.  

For conversion to wav, libmikmod was replaced with libopenmpt for more accurote playback and better sound quality. libmikmod is still used for note extraction because it works fine and is more complicated to change.

### SID files
libsidplayfp is used to emulate playback of sid files in a C64 environment. The source code was modified to get access to the sid registers with information about note frequencies, volume envelopes etc.