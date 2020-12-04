A dynamic C++ library that converts tracker modules and sid files to midi and wav. Used by [Remuxer](https://github.com/yousernaym/remuxer).

## How the code works
### Modules
For conversion to midi, libmikmod is used to get information about notes as well as volume-altering effects, envelopes and sample lenghts to determine when notes stop playing. Also taken into account are effects like playback speed and pattern jumping.
For conversion to wav the built-in functionality of libmikmod is used.  

### SID
libsidplayfp is used to emulate playback of sid files in a C64 environment.  
The audio samples are then saved to a wav file.  
For conversion to midi, libsidplayfp was modified to make it possible to monitor the sid registers with information about note frequencies, volume envelopes etc.