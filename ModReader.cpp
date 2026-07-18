#include "ModReader.h"
#include <sstream>
#include <vector>
#include <string>
#include <iostream>
#include <assert.h>
#include <algorithm>
#include <fstream>

ModReader::ModReader(Song& song) : SongReader(song, true, 480)
{
	songData = song.songData;
}

ModReader::~ModReader()
{
}



void ModReader::updateCell(RunningTickInfo &firstTick, const CellInfo &cellInfo, RunningCellInfo &runningCellInfo)
{
	int note = cellInfo.note;
	if (note > 0)
	{ //a new note was played
		runningCellInfo.samplePlaying = true;
		int smpIndex = 0; //1-based libopenmpt sample index (0 = none)
		int ins;
		if (cellInfo.ins)
		{  //ins + note was specified
			runningCellInfo.volEnvEnd = 0;
			runningCellInfo.startVol = 64;
			ins = cellInfo.ins;
		}
		else
			ins = runningCellInfo.ins;
		firstTick.ins = ins;
		int openmptNote = note - PITCH_OFFSET; //back to 1..128

		if (omptModule->vm_has_instruments())
		{
			if (ins > 0)
				smpIndex = omptModule->vm_get_note_sample(ins, openmptNote);

			if (ins > 0 && smpIndex > 0)
			{
				if (omptModule->vm_get_vol_env_one_shot_zero_end(ins)) //volenv on but not sustain or loop, last point = 0
					runningCellInfo.volEnvEnd = omptModule->vm_get_vol_env_end_tick(ins);
				runningCellInfo.startVol = omptModule->vm_get_instrument_global_vol(ins);
			}
			else
				runningCellInfo.samplePlaying = false;
		}
		else
			smpIndex = ins; //sample-based module: instrument number is the (1-based) sample number

		if (smpIndex > 0)
		{
			double sampleLength = (double)omptModule->vm_get_sample_length(smpIndex);
			runningCellInfo.loopSample = omptModule->vm_get_sample_loops(smpIndex);
			runningCellInfo.sampleC5Speed = omptModule->vm_get_sample_c5speed(smpIndex);
			long long loopEnd = omptModule->vm_get_sample_loop_end(smpIndex);

			runningCellInfo.sampleLength = sampleLength < cellInfo.sampleOffset ? 0 : sampleLength - cellInfo.sampleOffset;
			if (runningCellInfo.sampleLength == 0 || (runningCellInfo.loopSample && loopEnd > 0 && loopEnd <= (long long)cellInfo.sampleOffset))
				runningCellInfo.samplePlaying = false;
		}
		else
			runningCellInfo.samplePlaying = false;

		runningCellInfo.sampleStartS = timeS;
		if (cellInfo.ins)
			runningCellInfo.ins = cellInfo.ins;
	}
	if (cellInfo.ins) //Ins but not nessecarily note was specified, so running ins info should be used. Just reset volume and envelope start.
	{
		firstTick.vol = runningCellInfo.startVol;
		runningCellInfo.volEnvStartS = timeS;
		runningCellInfo.volEnvEnded = false;
	}
}


void ModReader::updateCellTicks(Song::Track &track, const CellInfo &cellInfo, RunningCellInfo &runningCellInfo)
{
	//write volume and notes in current cell's ticks
	for (int t = 0; t < curSongSpeed*(ptnDelay+1); t++)
	{
		RunningTickInfo &curTick = track.ticks[timeT+t];
		RunningTickInfo &prevTick = *track.getPrevTick(timeT+t);

		//Volume sliding effects
		if (t == 0 && cellInfo.volSlideVelScale == 1 || t > 0 && cellInfo.volSlideVelScale > 1)
		{
			curTick.vol += cellInfo.volSlideVel;
			if (curTick.vol < 0)
				curTick.vol = 0;
		}
		if (curTick.vol > 0 && prevTick.vol == 0 && prevTick.ins > 0)
			curTick.noteStart = timeT + t;

		//Check sample and envelope
		double tickTimeS = timeS + tickDur * t;
		double elapsedSampleS = tickTimeS - runningCellInfo.sampleStartS;
		double elapsedVolEnvS = tickTimeS - runningCellInfo.volEnvStartS;
		if (!runningCellInfo.loopSample && runningCellInfo.ins > 0)
		{ //sample is not looping
		  //check if sample has ended
			int openmptNote = curTick.notePitch - PITCH_OFFSET;
			int playedNote = omptModule->vm_has_instruments() ? omptModule->vm_get_note_map(runningCellInfo.ins, openmptNote) : openmptNote;
			double freqRatio = 1 / pow(semitone, playedNote - OMPT_NOTE_MIDDLEC);
			double c5Speed = runningCellInfo.sampleC5Speed > 0 ? runningCellInfo.sampleC5Speed : 8363;
			if (elapsedSampleS > runningCellInfo.sampleLength * freqRatio / c5Speed)
			{
				curTick.noteStart = -1;
				runningCellInfo.samplePlaying = false;
			}
		}
		if (runningCellInfo.volEnvEnd > 0)
		{ //vol env is active, not looping and ends with 0
		  //check if volenv has ended. OpenMPT envelope ticks == player ticks, so tickDur converts to seconds.
			if (elapsedVolEnvS > runningCellInfo.volEnvEnd * tickDur)
			{
				curTick.noteStart = -1;
				runningCellInfo.volEnvEnded = true;
			}
		}

		if ((cellInfo.noteStartOffset == t && cellInfo.note > 0 || //Note starts at current tick (first tick of cell or start is offset using edx command)
			cellInfo.retriggerOffset > 0 && t % cellInfo.retriggerOffset == 0 ||  //Retrigger effect (e9x)
			cellInfo.noteStartOffset > 0 && t % curSongSpeed != 0 && t % cellInfo.noteStartOffset == 0 && cellInfo.note > 0)  //If pattern delay is used, note can be replayed on repeats if edx is used.
			&& !runningCellInfo.volEnvEnded && curTick.vol && runningCellInfo.samplePlaying)
		{
			curTick.noteStart = timeT + t;
			int arpStep = t % 3;
			curTick.notePitch = cellInfo.arpPitches[0] > 0 ? cellInfo.arpPitches[arpStep] : runningCellInfo.note;
		}
		if (cellInfo.noteEndOffset == t)
			curTick.noteStart = -1;

		if (curTick.vol == 0)
			curTick.noteStart = -1;

		//Copy current tick to next tick
		auto nextTick = track.getNextTick(timeT + t);
		*nextTick = curTick;

		//If this cell has an arpeggio effect, reset next tick with a new, unarpeggiated note
		if (cellInfo.arpPitches[0] > 0)
		{
			nextTick->notePitch = cellInfo.arpPitches[0];
			nextTick->noteStart = timeT + t + 1;
		}
	}
}

void ModReader::readCell(int pattern, int row, int channel, CellInfo &cellInfo, RunningCellInfo &runningCellInfo)
{
	cellInfo = CellInfo();

	int rawNote = omptModule->get_pattern_row_channel_command(pattern, row, channel, openmpt::module::command_note);
	if (rawNote >= OMPT_NOTE_MIN && rawNote <= OMPT_NOTE_MAX)
	{ //a real note
		cellInfo.note = rawNote + PITCH_OFFSET;
		runningCellInfo.note = cellInfo.note;
	}
	else if (rawNote == OMPT_NOTE_KEYOFF || rawNote == OMPT_NOTE_NOTECUT || rawNote == OMPT_NOTE_FADE)
		cellInfo.keyOff = true;

	cellInfo.ins = omptModule->get_pattern_row_channel_command(pattern, row, channel, openmpt::module::command_instrument);
	cellInfo.volcmd = omptModule->get_pattern_row_channel_command(pattern, row, channel, openmpt::module::command_volumeffect);
	cellInfo.vol = omptModule->get_pattern_row_channel_command(pattern, row, channel, openmpt::module::command_volume);
	cellInfo.command = omptModule->get_pattern_row_channel_command(pattern, row, channel, openmpt::module::command_effect);
	cellInfo.param = omptModule->get_pattern_row_channel_command(pattern, row, channel, openmpt::module::command_parameter);
}

//Returns false to indicate end of song if a position jump goes backwards, otherwise true
bool ModReader::readCellFx(RunningTickInfo &firstTick, CellInfo &cellInfo, RunningCellInfo &runningCellInfo, int order, int row)
{
	//Note-column note-off (=== / ^^^ / ~~~)
	if (cellInfo.keyOff && runningCellInfo.volEnvEnd == 0) //Volume envelope disabled, just set volume to 0
		firstTick.vol = 0;
	//TODO: if volume envelope is active, a note off should let it pass beyond sustain point.

	//---- Effect column (CMD_*) ----
	int command = cellInfo.command;
	int param = cellInfo.param;
	switch (command)
	{
	case CMD_SPEED_:
		curSongSpeed = param;
		rowDur = getRowDur(songData->tempoEvents[songData->numTempoEvents - 1].tempo, curSongSpeed);
		break;
	case CMD_TEMPO_:
		songData->tempoEvents[songData->numTempoEvents].tempo = param;
		songData->tempoEvents[songData->numTempoEvents].time = timeT;
		if (++songData->numTempoEvents >= MAX_TEMPOEVENTS)
			throw (std::string)"Too many tempo events.";
		rowDur = getRowDur(songData->tempoEvents[songData->numTempoEvents - 1].tempo, curSongSpeed);
		break;
	case CMD_KEYOFF_:
		if (runningCellInfo.volEnvEnd == 0) //Volume envelope disabled, just set volume to 0
			firstTick.vol = 0;
		break;
	case CMD_ARPEGGIO_:
	{
		int value1 = (param >> 4) & 0xf;
		int value2 = param & 0xf;
		if (value1 > 0 || value2 > 0)
		{
			cellInfo.retriggerOffset = 1;
			cellInfo.arpPitches[0] = runningCellInfo.note;
			cellInfo.arpPitches[1] = runningCellInfo.note + value1;
			cellInfo.arpPitches[2] = runningCellInfo.note + value2;
		}
		break;
	}
	case CMD_OFFSET_:
	{
		int value = param;
		if (value == 0) //offset has memory
			value = runningCellInfo.offsetMem;
		else
			runningCellInfo.offsetMem = value;
		cellInfo.sampleOffset = value * 256;
		break;
	}
	case CMD_POSITIONJUMP_:
		ptnJump = param;
		if (ptnJump <= order)
			return false;
		break;
	case CMD_PATTERNBREAK_:
		if (ptnJump == -1)
			ptnJump = order + 1;
		ptnStart = param;
		break;
	case CMD_VOLUME_:
		firstTick.vol = param;
		break;
	case CMD_RETRIG_:
	{
		int interval = param & 0xf;
		if (interval > 0 && interval < curSongSpeed)
			cellInfo.retriggerOffset = interval;
		break;
	}
	case CMD_VOLUMESLIDE_:
	{
		int value = param;
		if (value == 0) //volume slide has memory
			value = runningCellInfo.volSlideMem;
		else
			runningCellInfo.volSlideMem = value;
		int hi = (value >> 4) & 0xf;
		int lo = value & 0xf;
		if (hi == 0xf && lo != 0) //fine slide down (DFx)
		{
			cellInfo.volSlideVelScale = 1;
			cellInfo.volSlideVel = -lo;
		}
		else if (lo == 0xf && hi != 0) //fine slide up (DxF)
		{
			cellInfo.volSlideVelScale = 1;
			cellInfo.volSlideVel = hi;
		}
		else //normal slide, applied every tick after the first
		{
			cellInfo.volSlideVelScale = curSongSpeed - 1;
			cellInfo.volSlideVel = hi > 0 ? hi : -lo;
		}
		break;
	}
	case CMD_MODCMDEX_:
	case CMD_S3MCMDEX_:
	{
		int sub = (param >> 4) & 0xf;
		int value = param & 0xf;
		bool s3m = command == CMD_S3MCMDEX_;
		int loopSub = s3m ? 0xB : 0x6; //pattern loop nibble differs between Exx (MOD/XM) and Sxx (S3M/IT)
		if (!s3m && sub == 0x9 && value < curSongSpeed) //E9x retrigger (S3M/IT use Qxy = CMD_RETRIG)
			cellInfo.retriggerOffset = value;
		else if (sub == 0xC && value < curSongSpeed) //note cut (ECx / SCx)
			cellInfo.noteEndOffset = value;
		else if (sub == 0xD && value < curSongSpeed) //note delay (EDx / SDx)
			cellInfo.noteStartOffset = value;
		else if (sub == 0xE) //pattern delay (EEx / SEx)
			ptnDelay = value;
		else if (sub == loopSub) //pattern loop (E6x / SBx)
		{
			if (value == 0)
				runningCellInfo.loop.startR = row;
			else
			{
				if (runningCellInfo.loop.loops == 0)
					runningCellInfo.loop.loops = value;
				else
					runningCellInfo.loop.loops--;
				if (runningCellInfo.loop.loops > 0)
				{
					ptnJump = order;
					ptnStart = runningCellInfo.loop.startR;
				}
			}
		}
		else if (!s3m && sub == 0xA) //EAx fine volume slide up
		{
			cellInfo.volSlideVelScale = 1;
			cellInfo.volSlideVel = value;
		}
		else if (!s3m && sub == 0xB) //EBx fine volume slide down
		{
			cellInfo.volSlideVelScale = 1;
			cellInfo.volSlideVel = -value;
		}
		break;
	}
	}

	//---- Volume column (VOLCMD_*) ----
	switch (cellInfo.volcmd)
	{
	case VOLCMD_VOLUME_:
		firstTick.vol = cellInfo.vol;
		break;
	case VOLCMD_VOLSLIDEUP_:
		cellInfo.volSlideVelScale = curSongSpeed - 1;
		cellInfo.volSlideVel = cellInfo.vol;
		break;
	case VOLCMD_VOLSLIDEDOWN_:
		cellInfo.volSlideVelScale = curSongSpeed - 1;
		cellInfo.volSlideVel = -cellInfo.vol;
		break;
	case VOLCMD_FINEVOLUP_:
		cellInfo.volSlideVelScale = 1;
		cellInfo.volSlideVel = cellInfo.vol;
		break;
	case VOLCMD_FINEVOLDOWN_:
		cellInfo.volSlideVelScale = 1;
		cellInfo.volSlideVel = -cellInfo.vol;
		break;
	}
	return true;
}

double ModReader::getRowDur(double tempo, double speed)
{
	double spb = 1 / (tempo / 60); //seconds per beat
	double spr = spb / 4; //seconds per row
	return spr * speed / 6;
}

void ModReader::beginProcessing(const UserArgs &args)
{
	SongReader::beginProcessing(args);
	isFadingOut = false;

	//Single libopenmpt instance is used for both note extraction and audio rendering.
	//Throws openmpt::exception if the file is not a module; libRemuxer.cpp then falls back to the other readers.
	std::ifstream file(userArgs.inputPath, std::ios::binary);
	omptModule = std::make_unique<openmpt::module_ext>(file);
	file.close();
	omptModule->ctl_set_text("play.at_end", "continue");
	interactive = static_cast<openmpt::ext::interactive*>(omptModule->get_interface(openmpt::ext::interactive_id));

	int numChannels = omptModule->get_num_channels();
	song.tracks.resize(numChannels);
	for (unsigned i = 0; i < song.tracks.size(); i++)
	{
		song.tracks[i].ticks.reserve(30000);
		song.tracks[i].ticks.resize(1);
	}
	curRowInfo.resize(numChannels);
	runningRowInfo.resize(numChannels);

	//MIDI output data-----------------------
	curSongSpeed = omptModule->get_current_speed();
	double initTempo = omptModule->get_current_tempo2();
	rowDur = getRowDur(initTempo, curSongSpeed);

	songData->tempoEvents[0].tempo = initTempo;
	songData->tempoEvents[0].time = 0;
	songData->numTempoEvents = 1;

	songData->numTracks = 0;
	for (int i = 0; i < MAX_MIDITRACKS; i++)
		songData->tracks[i].numNotes = 0;
	//-----------------------------------

	if (userArgs.insTrack)	//One track per instrument/sample
	{
		const std::vector<std::string> names = omptModule->vm_has_instruments() ? omptModule->get_instrument_names() : omptModule->get_sample_names();
		for (size_t i = 0; i < names.size() && i + 1 < MAX_MIDITRACKS; i++)
		{
			if (!names[i].empty())
				strcpy_s(songData->tracks[i + 1].name, MAX_TRACKNAME_LENGTH, names[i].c_str());
			else
				songData->tracks[i + 1].name[0] = '\0';
		}
	}
	else	//One track per mod channel
	{
		for (int i = 0; i < numChannels; i++)
		{
			std::ostringstream s;
			s << "Channel " << i + 1;
			strcpy_s(songData->tracks[i + 1].name, MAX_TRACKNAME_LENGTH, s.str().c_str());
		}
	}

	extractNotes();

	songData->ticksPerBeat = 24;
	if (song.tracks.empty())
		throw "Empty song";
	buildTempoSegs();       //must precede createNoteList: it scales tempoEvents[].time in place
	song.createNoteList(userArgs);

	//Build the pass list now that note counts are known.
	//Mixdown pass renders first (only if an audio path was requested; -t without -a supplies its own).
	passList.clear();
	curPass = 0;
	passFraction = 0;
	if (userArgs.audioPath[0])
		passList.push_back({ 0, -1 });
	if (trackAudioRequested())
	{
		if (userArgs.insTrack)
		{
			//Per-instrument: render each channel that carries any note; the whole channel WAV is
			//shared by the instrument tracks that play on it (the app gates by note ownership).
			int numChannels = (int)song.tracks.size();
			for (int c = 0; c < numChannels; c++)
				if (!buildInstrumentRuns(song.tracks[c]).empty())
					passList.push_back({ -1, c });
		}
		else
		{
			//Per-channel: one whole-track WAV per non-empty track (channel = track - 1).
			for (int t = 1; t < songData->numTracks; t++)
				if (songData->tracks[t].numNotes > 0)
					passList.push_back({ t, t - 1 });
		}
	}
	if (!passList.empty() && passList[0].channel >= 0)
		setupTrackPass(passList[0].channel); //no mixdown pass: start straight on the first channel pass
}

//Piecewise-linear tick->seconds over the tempo map, snapshotted while tempoEvents[].time is still
//in pre-scale (24-tpb) ticks (createNoteList later multiplies these times by resolutionScale).
void ModReader::buildTempoSegs()
{
	tempoSegs.clear();
	for (int i = 0; i < songData->numTempoEvents; i++)
	{
		int startTick = songData->tempoEvents[i].time;
		double startS = 0;
		if (!tempoSegs.empty())
		{
			const TempoSeg& prev = tempoSegs.back();
			startS = prev.startS + (startTick - prev.startTick) * prev.secPerTick;
		}
		tempoSegs.push_back({ startTick, startS, 2.5 / songData->tempoEvents[i].tempo });
	}
}

double ModReader::tickToSeconds(int tick) const
{
	const TempoSeg* seg = &tempoSegs.front();
	for (const TempoSeg& s : tempoSegs)
	{
		if (s.startTick <= tick)
			seg = &s;
		else
			break;
	}
	return seg->startS + (tick - seg->startTick) * seg->secPerTick;
}


void ModReader::extractNotes()
{
	int numOrders = omptModule->get_num_orders();
	int numChannels = omptModule->get_num_channels();
	//Loop through orders
	for (int order = 0; order < numOrders; order++)
	{
		ptnJump = -1;
		int pattern = omptModule->get_order_pattern(order);
		if (pattern < 0)
			continue; //skip / stop marker or invalid order

		//Loop through rows
		int numRows = omptModule->get_pattern_num_rows(pattern);
		if (numRows <= 0)
			continue;
		for (int rowIndex = 0; rowIndex < numRows; rowIndex++)
		{
			ptnDelay = 0;
			//Loop through channels/tracks
			for (int trackIndex = 0; trackIndex < numChannels; trackIndex++)
			{
				readCell(pattern, rowIndex, trackIndex, curRowInfo[trackIndex], runningRowInfo[trackIndex]);

				if (ptnJump >= 0 || rowIndex >= ptnStart)
				{
					if (!readCellFx(song.tracks[trackIndex].ticks[timeT], curRowInfo[trackIndex], runningRowInfo[trackIndex], order, rowIndex))
						return;
				}
			}
			if (ptnJump == -1)
			{
				if (rowIndex < ptnStart)
					continue;
				else
					ptnStart = 0;
			}

			tickDur = rowDur / curSongSpeed;
			//Loop through channels/tracks
			for (int t = 0; t < numChannels; t++)
			{
				song.tracks[t].ticks.resize(song.tracks[t].ticks.size() + curSongSpeed * (ptnDelay + 1));
				RunningTickInfo* curTick = &song.tracks[t].ticks[timeT];
				updateCell(*curTick, curRowInfo[t], runningRowInfo[t]);
				updateCellTicks(song.tracks[t], curRowInfo[t], runningRowInfo[t]);
			}

			timeT += curSongSpeed * (ptnDelay + 1);
			timeS += rowDur * (ptnDelay + 1);

			if (ptnJump >= 0)
			{
				order = ptnJump - 1;
				break;
			}
		}
	}
}

//Renders one audio chunk of the current pass into wav; sets passFraction (0..1 within the pass)
//and returns true when the current pass has fully rendered.
bool ModReader::renderPassChunk()
{
	float songPosS = (float)omptModule->get_position_seconds();
	std::size_t count = omptModule->read_interleaved_stereo(sampleRate, audioBuffer.size() / 2, audioBuffer.data());

	if (isFadingOut)
	{
		float fadeFactor = (float)((FadeOutTimeS + timeS - songPosS) / FadeOutTimeS);
		for (auto& sample : audioBuffer)
			sample *= fadeFactor;
		wav.addSamples(audioBuffer);
		passFraction = 1;
		return songPosS > (FadeOutTimeS + timeS);
	}

	wav.addSamples(audioBuffer);

	if (count == 0) //end of song
	{	//If there is audio at song end, loop and fade out
		if (std::any_of(audioBuffer.begin(), audioBuffer.end(), [](SampleType sample) {return sample != 0; }))
		{
			isFadingOut = true;
			passFraction = 1;
			return false;
		}
		passFraction = 1;
		return true;
	}
	passFraction = (float)min(1, songPosS / timeS);
	return false;
}

//Prepare playback state for a channel render pass: seek to start, reset fade, mute all but `channel`.
void ModReader::setupTrackPass(int channel)
{
	omptModule->set_position_seconds(0.0); //re-simulates play state from start; mute statuses persist across seeks
	isFadingOut = false;
	wav.clearSamples();

	int numChannels = omptModule->get_num_channels();
	for (int c = 0; c < numChannels; c++)
		interactive->set_channel_mute_status(c, c != channel);
}

float ModReader::process()
{
	if (curPass >= (int)passList.size())
		return -1;

	bool passComplete = renderPassChunk();
	if (passComplete)
	{
		const Pass& p = passList[curPass];
		if (p.channel < 0)
			saveMixdownNow();
		else if (userArgs.insTrack)
			saveChannelPass(p.channel, song.tracks[p.channel],
				[this](int tick) { return tickToSeconds(tick); },
				[](int ins) { return Song::instrumentTrack(ins, nullptr); });
		else
			saveTrackWav(p.midiTrack, song.songData->tracks[p.midiTrack].name);
		curPass++;
		if (curPass >= (int)passList.size())
			return -1;
		setupTrackPass(passList[curPass].channel);
		passFraction = 0;
	}

	return clampProgress((curPass + passFraction) / (float)passList.size());
}

void ModReader::endProcessing()
{
	SongReader::endProcessing();
	omptModule.reset();
}
