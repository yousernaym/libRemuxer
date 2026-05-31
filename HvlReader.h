#pragma once

#include <set>
#include <vector>

#include "Song.h"
#include "libRemuxer.h"
#include "SongReader.h"

// Forward-declare only — the full struct definition lives in hvl/hvl_replay.h,
// which has TEXT/BOOL typedefs that conflict with <windows.h>.
// HvlReader.cpp undef TEXT and includes the header in an extern "C" block.
struct hvl_tune;

class HvlReader : public SongReader
{
    struct hvl_tune *ht = nullptr;

    int   numChannels   = 0;
    int   frameIndex    = 0;    // current tick (== frame number; 50 frames/s PAL)
    bool  isFadingOut   = false;
    int   fadeFrame     = 0;    // frames elapsed since fade-out began

    const int FRAME_RATE    = 50;  // PAL
    const int FADE_OUT_S    = 7;   // fade-out duration in seconds

    std::set<int>     usedInstruments;     // instrument indices played (1-based)
    std::vector<int>  prevTrackPeriod;     // vc_TrackPeriod from the previous frame

    // Interleaved int16 buffer written by hvl_DecodeFrame before float conversion
    std::vector<short> renderBuf;

    int periodToMidi(int audioPeriod, int waveLength) const;

public:
    HvlReader(Song &song);
    ~HvlReader();

    void  beginProcess(UserArgs &args);   // throws std::string on non-HVL file
    float process() override;
    void  endProcessing() override;
};
