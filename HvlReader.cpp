#include "HvlReader.h"

#include <algorithm>
#include <cmath>
#include <string>

// hvl_replay.h defines its own BOOL/TEXT types that conflict with <windows.h>.
// Un-define the Windows macros first, then include the hvl header in a C-linkage block.
#ifdef TEXT
#undef TEXT
#endif
#ifdef BOOL
#undef BOOL
#endif
extern "C"
{
    #include "hvl/hvl_replay.h"
}

// <windows.h> defines min/max as macros, which
// breaks std::min / std::max. Drop them — we use the std versions below.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// ---------------------------------------------------------------------------
// Constant: Amiga Paula clock (PAL). Replicated here for clarity; the macro
// AMIGA_PAULA_PAL_CLK is the same value defined in hvl_replay.h.
// ---------------------------------------------------------------------------
static const float HVL_PAULA_CLK = (float)AMIGA_PAULA_PAL_CLK;  // 3 546 895 Hz

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

HvlReader::HvlReader(Song &song) : SongReader(song, /*stereo=*/true)
{
    hvl_InitReplayer();   // fills global lookup tables; cheap, idempotent
}

HvlReader::~HvlReader()
{
    if (ht)
    {
        hvl_FreeTune(ht);
        ht = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Pitch helper: Amiga period + waveform length -> MIDI note number
//
// The HVL synthesiser plays a synthesised waveform whose fundamental period
// is (4 * 2^waveLength) bytes.  The sample replay rate per byte is:
//   delta = PAULA_CLK / (audioPeriod * sampleRate)      [Q16 fixed-point]
// So the fundamental audio frequency is:
//   freq = PAULA_CLK / (audioPeriod * 4 * 2^waveLength)   [Hz]
// Mapped to MIDI via A4 = 440 Hz = note 69:
//   midi = round( 12 * log2(freq / 440) + 69 )
// ---------------------------------------------------------------------------
int HvlReader::periodToMidi(int audioPeriod, int waveLength) const
{
    if (audioPeriod <= 0)
        return 0;

    int waveBytes = 4 * (1 << waveLength);   // fundamental waveform length in bytes
    float freq    = HVL_PAULA_CLK / ((float)audioPeriod * (float)waveBytes);
    int   midi    = (int)(12.0f * log2f(freq / 440.0f) + 69.0f + 0.5f);
    return std::max(1, std::min(127, midi));
}

// ---------------------------------------------------------------------------
// beginProcess — load the file, set up timing, size the tick arrays.
// Throws std::string on failure so the dispatcher in libRemuxer.cpp falls
// through to the next reader.
// ---------------------------------------------------------------------------
void HvlReader::beginProcess(UserArgs &args)
{
    SongReader::beginProcessing(args);   // copies args, clears audioBuffer + wav

    // Audio frame geometry: one PAL frame = sampleRate/50 stereo int16 pairs.
    const int samplesPerFrame = sampleRate / FRAME_RATE;   // 48000/50 = 960
    const int stereoSamples   = samplesPerFrame * 2;       // L+R interleaved

    renderBuf.resize(stereoSamples);
    setAudioBufferSize(stereoSamples);   // float audioBuffer for wav

    // Synthetic tempo that gives exactly 50 ticks/second:
    //   tempo=125 BPM × ticksPerBeat=24 / 60 = 50 ticks/s
    song.songData->ticksPerBeat          = 24;
    song.songData->tempoEvents[0].time   = 0;
    song.songData->tempoEvents[0].tempo  = 125.0;
    song.songData->numTempoEvents        = 1;
    // resolutionScale in createNoteList = 480 / ticksPerBeat = 20

    // Attempt to load the tune.  hvl_LoadTune validates the "THX" / "HVL"
    // magic bytes and returns NULL on unknown formats — that is our format gate.
    //   defstereo=4  → classic Amiga LRRL stereo panning
    ht = hvl_LoadTune(userArgs.inputPath, (uint32)sampleRate, 4);
    if (!ht)
        throw std::string("Not an HVL/AHX file: ") + userArgs.inputPath;

    // Subsong handling (mirrors SidReader).
    int totalSubsongs = (int)ht->ht_SubsongNr + 1;   // 0..SubsongNr are valid
    args.numSubSongs  = totalSubsongs;

    int selected = userArgs.subSong;
    if (selected < 0 || selected >= totalSubsongs)
        selected = 0;
    args.subSong = selected;
    hvl_InitSubsong(ht, (uint32)selected);

    // Channel / track setup.
    numChannels = (int)ht->ht_Channels;
    song.tracks.resize(numChannels);
    prevTrackPeriod.assign(numChannels, 0);
    usedInstruments.clear();

    for (int c = 0; c < numChannels; c++)
    {
        song.tracks[c].ticks.reserve(30000);   // avoid repeated reallocs
        song.tracks[c].ticks.clear();

        if (!userArgs.insTrack)
            sprintf_s(song.songData->tracks[c + 1].name,
                      MAX_TRACKNAME_LENGTH, "Channel %i", c + 1);
    }

    // Reset per-conversion state.
    frameIndex  = 0;
    isFadingOut = false;
    fadeFrame   = 0;
}

// ---------------------------------------------------------------------------
// process — called in a loop by Form1 until it returns -1.
// Decodes one 50 Hz frame: renders audio, samples voice state, emits ticks.
// ---------------------------------------------------------------------------
float HvlReader::process()
{
    // 1. Decode one audio frame into renderBuf (int16 interleaved stereo).
    //    buf1 = left  (byte 0, stride 4)
    //    buf2 = right (byte 2, stride 4)
    hvl_DecodeFrame(ht,
        (char *)renderBuf.data(),
        (char *)renderBuf.data() + 2,
        4);

    // 2. Sample voice state and build one tick per channel.
    for (int c = 0; c < numChannels; c++)
    {
        // Carry the previous tick forward as the default for this frame.
        RunningTickInfo tick = frameIndex > 0 ? song.tracks[c].ticks.back()
                                              : RunningTickInfo{};

        const struct hvl_voice &v = ht->ht_Voices[c];
        int vol    = (int)v.vc_VoiceVolume;   // 0..64
        int period = (int)v.vc_AudioPeriod;   // Amiga period (hardware units)
        int trackP = (int)v.vc_TrackPeriod;   // note index 0..60 (set on new step)

        if (vol > 0 && period > 0)
        {
            int midi = periodToMidi(period, (int)v.vc_WaveLength);
            int ins  = v.vc_Instrument
                           ? (int)(v.vc_Instrument - ht->ht_Instruments)
                           : 0;

            // A new note starts when:
            //  a) the voice was silent last frame (volume rose from 0), OR
            //  b) the track-step note index changed (new note in the pattern
            //     — this fires regardless of whether pitch is different, so
            //     same-pitch note repeats are caught via vc_TrackPeriod; only
            //     same-pitch same-step retriggers within one row are missed,
            //     which is an acceptable limitation for this engine).
            bool newNote = (tick.vol == 0) || (trackP != prevTrackPeriod[c]);

            if (newNote)
                tick.noteStart = frameIndex;

            tick.notePitch = midi;
            tick.ins       = ins;
            tick.vol       = vol;
            usedInstruments.insert(ins);
        }
        else
        {
            tick.vol = 0;   // volume=0 → note off (boundary in createNoteList)
        }

        prevTrackPeriod[c] = trackP;
        song.tracks[c].ticks.push_back(tick);
    }

    frameIndex++;

    // 3. Detect song-end → begin fade-out.
    if (!isFadingOut && ht->ht_SongEndReached)
    {
        isFadingOut = true;
        fadeFrame   = 0;
    }

    // 4. Apply fade-out scale to the float audio buffer.
    if (userArgs.audioPath[0] != 0)
    {
        const int fadeTotalFrames = FADE_OUT_S * FRAME_RATE;

        // Convert int16 renderBuf -> float audioBuffer.
        for (size_t i = 0; i < renderBuf.size(); i++)
            audioBuffer[i] = renderBuf[i] / 32768.0f;

        if (isFadingOut)
        {
            float scale = (float)(fadeTotalFrames - fadeFrame) / (float)fadeTotalFrames;
            if (scale < 0.0f) scale = 0.0f;
            for (float &s : audioBuffer)
                s *= scale;
        }

        wav.addSamples(audioBuffer);
    }

    // 5. Return progress or signal completion.
    if (isFadingOut)
    {
        if (++fadeFrame >= FADE_OUT_S * FRAME_RATE)
            return -1.0f;   // done

        // Small declining progress during fade so the bar keeps moving.
        float fadeProg = (float)(FADE_OUT_S * FRAME_RATE - fadeFrame)
                       / (float)(FADE_OUT_S * FRAME_RATE);
        return std::max(0.01f, fadeProg);
    }

    // Normal progress: fraction of song positions traversed.
    if (ht->ht_PositionNr > 0)
    {
        float prog = (float)ht->ht_PosNr / (float)ht->ht_PositionNr;
        return std::max(0.01f, std::min(0.99f, prog));
    }
    return 0.5f;   // unknown length fallback
}

// ---------------------------------------------------------------------------
// endProcessing — save WAV, name instrument tracks, build MIDI note list.
// ---------------------------------------------------------------------------
void HvlReader::endProcessing()
{
    SongReader::endProcessing();   // saves WAV if audioPath is set

    if (userArgs.insTrack && ht)
    {
        // Name each compacted track from the HVL instrument name, in
        // usedInstruments order (matches Song::createNoteList's compaction).
        int t = 1;
        for (int ins : usedInstruments)
        {
            if (ins > 0 && ins <= (int)ht->ht_InstrumentNr)
            {
                // ins_Name is TEXT[] = char[], guaranteed null-terminated by the loader.
                const char *name = ht->ht_Instruments[ins].ins_Name;
                if (name[0] != '\0')
                    sprintf_s(song.songData->tracks[t].name,
                              MAX_TRACKNAME_LENGTH, "%s", name);
                else
                    sprintf_s(song.songData->tracks[t].name,
                              MAX_TRACKNAME_LENGTH, "Instrument %i", ins);
            }
            t++;
        }
    }

    song.createNoteList(userArgs, &usedInstruments);

    hvl_FreeTune(ht);
    ht = nullptr;
}
