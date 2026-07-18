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
    // Per-track pass descriptor. channel = the channel to render. Per-channel mode saves it whole
    // as track `midiTrack`; per-instrument mode saves the whole channel WAV shared by its
    // instrument tracks (midiTrack unused).
    struct TrackPass
    {
        int midiTrack;
        int channel;
    };

    struct hvl_tune *ht = nullptr;

    int   numChannels   = 0;
    int   frameIndex    = 0;    // current tick (== frame number; 50 frames/s PAL)
    bool  isFadingOut   = false;
    int   fadeFrame     = 0;    // frames elapsed since fade-out began
    int   selectedSubsong = 0;

    const int FRAME_RATE    = 50;  // PAL
    const int FADE_OUT_S    = 7;   // fade-out duration in seconds

    std::set<int>     usedInstruments;     // instrument indices played (1-based)
    std::vector<int>  prevTrackPeriod;     // vc_TrackPeriod from the previous frame

    // Interleaved int16 buffer written by hvl_DecodeFrame before float conversion
    std::vector<short> renderBuf;

    bool  notesFinalized = false;
    int   curPass = 0;   // 0 = main pass (note extraction + mixdown); >=1 = track passes
    float passFrac = 0;  // progress within the current pass (0..1)
    std::vector<TrackPass> trackPasses;

    int periodToMidi(int audioPeriod, int waveLength) const;

    void  finalizeNotes();
    void  buildTrackPasses();
    void  startTrackPass(int passIndex);
    void  decodeFrameForPass(const TrackPass &tp);  // mutes voices output-only, then mixes one frame
    bool  renderMainChunk();   // extracts ticks + renders mixdown; true when the main pass is done
    bool  renderTrackChunk();  // renders one muted track pass frame; true when the pass is done

public:
    HvlReader(Song &song);
    ~HvlReader();

    void  beginProcess(UserArgs &args);   // throws std::string on non-HVL file
    float process() override;
    void  endProcessing() override;
};
