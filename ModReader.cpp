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
	marSong = song.marSong;
	MikMod_RegisterDriver(&drv_nos);
	MikMod_RegisterAllLoaders();
	if (MikMod_Init(""))
	{
		std::string err = (std::string)"Could not initialize Mikmod, reason: " + (std::string)MikMod_strerror(MikMod_errno);
		OutputDebugStringA(err.c_str());
		throw "MikMod_Init failed";
	}
}

ModReader::~ModReader()
{
	if (module)
	{
		Player_Free(module);
		module = 0;
	}
	MikMod_Exit();
}



void ModReader::updateCell(RunningTickInfo &firstTick, const CellInfo &cellInfo, RunningCellInfo &runningCellInfo)
{
	int note = cellInfo.note;
	if (note > 0)
	{ //a new note was played
		runningCellInfo.samplePlaying = true;
		int smpIndex = -1;
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
		if (module->instruments)
		{
			INSTRUMENT *instrument = nullptr;
			if (ins > 0 && ins - 1 < module->numins)
			{
				instrument = &module->instruments[ins - 1];
				smpIndex = instrument->samplenumber[note - 1];
			}
			
			if (instrument && smpIndex != UWORD(-1))
			{
				if (instrument->volflg == 1  //volenv on but not sustain or loop
					&& instrument->volenv[instrument->volpts - 1].val == 0)  //last env point = 0
				{
					runningCellInfo.volEnvEnd = instrument->volenv[instrument->volpts - 1].pos;
				}
				runningCellInfo.startVol = instrument->globvol;
			}
			else
				runningCellInfo.samplePlaying = false;
		}
		else
			smpIndex = ins - 1;
		if (smpIndex >= 0 && smpIndex < module->numsmp)
		{
			SAMPLE sample = module->samples[smpIndex];
			runningCellInfo.loopSample = (sample.flags & SF_LOOP) == SF_LOOP;

			runningCellInfo.sampleLength = sample.length < cellInfo.sampleOffset ? 0 : sample.length - cellInfo.sampleOffset;
			if (runningCellInfo.sampleLength == 0 || runningCellInfo.loopSample && sample.loopend > 0 && sample.loopend <= cellInfo.sampleOffset)
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
			int note = module->instruments != NULL ? module->instruments[runningCellInfo.ins - 1].samplenote[curTick.notePitch] : curTick.notePitch;
			double freqRatio = 1 / pow(semitone, note - 48);
			if (elapsedSampleS > runningCellInfo.sampleLength * freqRatio / 8363)
			{
				curTick.noteStart = -1;
				runningCellInfo.samplePlaying = false;
			}
		}
		if (runningCellInfo.volEnvEnd > 0)
		{ //vol env is active, not looping and ends with 0
		  //check if volenv has ended
			if (elapsedVolEnvS > runningCellInfo.volEnvEnd / 47.647)
			{
				curTick.noteStart = -1;
				runningCellInfo.volEnvEnded = true;
			}
		}
		int cellTick = t % curSongSpeed;

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

void ModReader::readNextCell(BYTE *track, CellInfo &cellInfo, RunningCellInfo &runningCellInfo)
{
	int cellRep, cellLen;
	getCellRepLen(track[runningCellInfo.offset], cellRep, cellLen);
	if (runningCellInfo.repeatsLeft == 0)
	{  //Current cell should not be repeated, read cell at new row
		runningCellInfo.repeatsLeft = cellRep;
		cellInfo = CellInfo();
		int rowDataOffset = 1 + runningCellInfo.offset;
		while (rowDataOffset < cellLen + runningCellInfo.offset)
		{
			int opCode = track[rowDataOffset++];
			if (opCode <= 0 || opCode >= UNI_LAST)
				throw "Invalid opCode";
						
			if (opCode == UNI_NOTE)
			{
				int note = track[rowDataOffset++] + 1;
				
				// Midi only supports < 128 as pitch, so treat higher values as note off.
				// .it seems to use 247 as some sort of fadeout-ish note off.
				if (note >= 128) 
					note = 0;
				cellInfo.note = note;
				runningCellInfo.note = note;
			}
			else if (opCode == UNI_INSTRUMENT)
				cellInfo.ins = track[rowDataOffset++] + 1;
			else
			{
				cellInfo.eff[cellInfo.numEffs] = opCode;
				for (int i = 0; i < MikMod_GetOperandCount(opCode); i++)
					cellInfo.effValues[opCode][i] = track[rowDataOffset++];
				cellInfo.numEffs++;
			}
		}
		runningCellInfo.offset += cellLen;
	}
	else
		runningCellInfo.repeatsLeft--;
}

//Returns false to indicate end of song if a BXX command jumps backwards, otherwise true
bool ModReader::readCellFx(RunningTickInfo &firstTick, CellInfo &cellInfo, RunningCellInfo &runningCellInfo, int pattern, int row)
{
	for (int i = 0; i < cellInfo.numEffs; i++)
	{
		int effValues[MAX_EFFECT_VALUES];
		int eff = cellInfo.eff[i];
		for (int j = 0; j < MAX_EFFECT_VALUES; j++)
		{
			//If effect value is not zero or effect type does not have memory of its last value, update the running value
			if (cellInfo.effValues[eff][j] != 0 || eff != UNI_XMEFFECTA && eff != UNI_XMEFFECTEA && eff != UNI_XMEFFECTEB && eff != UNI_S3MEFFECTD && eff != UNI_PTEFFECT9)
				runningCellInfo.effValues[cellInfo.eff[i]][j] = cellInfo.effValues[cellInfo.eff[i]][j];
			effValues[j] = runningCellInfo.effValues[cellInfo.eff[i]][j];
		}

		if (cellInfo.eff[i] == UNI_PTEFFECTF || cellInfo.eff[i] == UNI_S3MEFFECTA || cellInfo.eff[i] == UNI_S3MEFFECTT)
		{ //Speed/tempo changes
			int value = effValues[0];
			if (value <= 0x1f || cellInfo.eff[i] == UNI_S3MEFFECTA)
				curSongSpeed = value;
			else
			{
				marSong->tempoEvents[marSong->numTempoEvents].tempo = value;
				marSong->tempoEvents[marSong->numTempoEvents].time = timeT;
				if (++marSong->numTempoEvents >= MAX_TEMPOEVENTS)
					throw (std::string)"Too many tempo events.";
			}
			rowDur = getRowDur(marSong->tempoEvents[marSong->numTempoEvents - 1].tempo, curSongSpeed);
		}
		else if (cellInfo.eff[i] == UNI_KEYOFF || cellInfo.eff[i] == UNI_KEYFADE)
		{
			if (runningCellInfo.volEnvEnd == 0) //Volume envelope disabled, just set volume to 0
				firstTick.vol = 0;
			//TODO: if volume envelope is active, a note off should let it pass beyond sustain point.
		}
		else if (cellInfo.eff[i] == UNI_PTEFFECT0)
		{
			int value = effValues[0];
			int value1 = (value >> 4) & 0xf;
			int value2 = value & 0xf;
			if (value1 > 0 || value2 > 0)
			{
				cellInfo.retriggerOffset = 1;
				cellInfo.arpPitches[0] = runningCellInfo.note;
				cellInfo.arpPitches[1] = runningCellInfo.note + value1;
				cellInfo.arpPitches[2] = runningCellInfo.note + value2;
			}
		}
		else if (cellInfo.eff[i] == UNI_PTEFFECT9)
		{
			cellInfo.sampleOffset = effValues[0] * 256;
		}
		else if (cellInfo.eff[i] == UNI_PTEFFECTB)
		{
			ptnJump = effValues[0];
			if (ptnJump <= pattern)
				return false;
		}
		else if (cellInfo.eff[i] == UNI_PTEFFECTD)
		{
			if (ptnJump == -1)
				ptnJump = pattern + 1;
			ptnStart = effValues[0];
		}
		else if (cellInfo.eff[i] == UNI_PTEFFECTC)
			firstTick.vol = effValues[0];
		else if (cellInfo.eff[i] == UNI_PTEFFECTE)
		{
			int subeff = (effValues[0] >> 4) & 0xf;
			int value = effValues[0] & 0xf;
			if (value < curSongSpeed)
			{
				if (subeff == 0x9)
					cellInfo.retriggerOffset = value;
				if (subeff == 0xc)
					cellInfo.noteEndOffset = value;
				else if (subeff == 0xd)
					cellInfo.noteStartOffset = value;
			}
			if (subeff == 0xe)
				ptnDelay = value;
			if (subeff == 0x6)
			{
				if (value == 0)
				{
					runningCellInfo.loop.startR = row;
				}
				else
				{
					if (runningCellInfo.loop.loops == 0)
						runningCellInfo.loop.loops = value;
					else
						runningCellInfo.loop.loops--;
					if (runningCellInfo.loop.loops > 0)
					{
						ptnJump = pattern;
						ptnStart = runningCellInfo.loop.startR;
					}
				}
			}
		}
		if (cellInfo.eff[i] == UNI_XMEFFECTA || cellInfo.eff[i] == UNI_PTEFFECTA || cellInfo.eff[i] == UNI_S3MEFFECTD && (effValues[0] & 0xf) != 0xf && (effValues[0] & 0xf0) != 0xf0)
		{
			cellInfo.volSlideVelScale = curSongSpeed - 1;
			int value = effValues[0];
			//if (value > 0 || cellInfo.eff[i] <= UNI_PTEFFECTE)
			cellInfo.volSlideVel = value > 0xf ? (value >> 4) : -value;
		}
		else if (cellInfo.eff[i] == UNI_XMEFFECTEA || cellInfo.eff[i] == UNI_XMEFFECTEB)
		{
			cellInfo.volSlideVelScale = 1;
			if (cellInfo.eff[i] == UNI_XMEFFECTEA)
				cellInfo.volSlideVel = effValues[0];
			else //UNI_XMEFFECTEB
				cellInfo.volSlideVel = -effValues[0];
		}
		else if (cellInfo.eff[i] == UNI_PTEFFECTE || cellInfo.eff[i] == UNI_S3MEFFECTD)
		{
			int value = effValues[0];
			if (cellInfo.eff[i] == UNI_PTEFFECTE)
			{
				int subEff = (value & 0xf0) >> 4;
				if ((value & 0xf0) == (0xa << 4))
				{
					cellInfo.volSlideVelScale = 1;
					cellInfo.volSlideVel = value & 0xf;
				}
				else if ((value & 0xf0) == (0xb << 4))
				{
					cellInfo.volSlideVel = -(value & 0xf);
					cellInfo.volSlideVelScale = 1;
				}
			}
			else if (cellInfo.eff[i] == UNI_S3MEFFECTD)
			{
				cellInfo.volSlideVelScale = 1;
				if ((value & 0xf) == 0xf)
					cellInfo.volSlideVel = value >> 4;
				else //value & 0xf0 == 0xf0
					cellInfo.volSlideVel = -value;
			}
			else //if (value > 0)
			{

			}
		}
	}
	return true;
}

void ModReader::getCellRepLen(BYTE replen, int &repeat, int &length)
{
	repeat = (replen >> 5) & 7;
	length = replen & 31;
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
	std::string cmdLine;
	BOOL mixdown = userArgs.audioPath[0] > 0;
	md_device = 2; //nosound libmikmod driver

	if (mixdown)
	{
		std::ifstream file(userArgs.inputPath, std::ios::binary);
		omptModule = std::make_unique<openmpt::module>(file);
		file.close();
		omptModule->ctl_set_text("play.at_end", "continue");
	}		
	
	module = Player_Load(userArgs.inputPath, 64, 0);

	if (module)
	{
		song.tracks.resize(module->numchn);
		for (unsigned i = 0; i < song.tracks.size(); i++)
		{
			song.tracks[i].ticks.reserve(30000);
			song.tracks[i].ticks.resize(1);
		}
		curRowInfo.resize(module->numchn);
		runningRowInfo.resize(module->numchn);

		//Marshall data--------------------------
		//marSong->ticksPerMeasure = module->sngspd * 16; //assuming 4 rows per beat
		curSongSpeed = module->initspeed;

		//int curSongBpm = module->inittempo;
		rowDur = getRowDur(module->inittempo, module->initspeed);

		marSong->tempoEvents[0].tempo = module->inittempo;
		marSong->tempoEvents[0].time = 0;
		marSong->numTempoEvents = 1;

		marSong->numTracks = 0;//module->numins;
		for (int i = 0; i < MAX_MIDITRACKS; i++)
			marSong->tracks[i].numNotes = 0;
		//-----------------------------------

		if (userArgs.insTrack)	//One track per instrument/sample
		{
			if (module->instruments)
			{
				for (int i = 0; i < module->numins; i++)
				{
					if (module->instruments[i].insname != NULL)
						strcpy_s(marSong->tracks[i + 1].name, MAX_TRACKNAME_LENGTH, module->instruments[i].insname);
					else
						marSong->tracks[i + 1].name[0] = NULL;
				}
			}
			else
			{
				for (int i = 0; i < module->numsmp; i++)
				{
					if (module->samples[i].samplename != NULL)
						strcpy_s(marSong->tracks[i + 1].name, MAX_TRACKNAME_LENGTH, module->samples[i].samplename);
					else
						marSong->tracks[i + 1].name[0] = NULL;
				}
			}
		}
		else	//One track per mod channel
		{
			for (int i = 0; i < module->numchn; i++)
			{
				std::ostringstream s;
				s << "Channel " << i + 1;
				strcpy_s(marSong->tracks[i + 1].name, MAX_TRACKNAME_LENGTH, s.str().c_str());
			}
		}

		extractNotes();
		
		Player_Free(module);
		module = 0;
		marSong->songLengthT = timeT;
	}
	else
	{
		std::ostringstream err;
		err << "Could not load module, reason: " << MikMod_strerror(MikMod_errno);
		OutputDebugStringA(err.str().c_str());
		throw err.str();
	}
	marSong->ticksPerBeat = 24;
	if (song.tracks.empty())
		throw "Empty song";
	song.createNoteList(userArgs);
}


void ModReader::extractNotes()
{
	//Loop through patterns
	for (int sequenceIndex = 0; sequenceIndex < module->numpos; sequenceIndex++)
	{
		ptnJump = -1;
		int pattern = module->patterns[module->positions[sequenceIndex]];
		
		for (int t = 0; t < module->numchn; t++)
			runningRowInfo[t].offset = runningRowInfo[t].repeatsLeft = 0;
		//Loop through rows
		int numRows = module->pattrows[pattern];
		for (int rowIndex = 0; rowIndex < numRows; rowIndex++)
		{
			ptnDelay = 0;
			//Loop through channels/tracks
			for (int trackIndex = 0; trackIndex < module->numchn; trackIndex++)
			{
				BYTE* track = module->tracks[pattern * module->numchn + trackIndex];
				readNextCell(track, curRowInfo[trackIndex], runningRowInfo[trackIndex]);
		
				if (ptnJump >= 0 || rowIndex >= ptnStart)
				{
					if (!readCellFx(song.tracks[trackIndex].ticks[timeT], curRowInfo[trackIndex], runningRowInfo[trackIndex], sequenceIndex, rowIndex))
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
			for (int t = 0; t < module->numchn; t++)
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
				sequenceIndex = ptnJump - 1;
				break;
			}
		}
	}
}

float ModReader::process()
{
	if (!userArgs.audioPath[0])
		return -1;
	
	float songPosS = (float)omptModule->get_position_seconds();
	std::size_t count = omptModule->read_interleaved_stereo(sampleRate, audioBuffer.size() / 2, audioBuffer.data());
	
	if (isFadingOut)
	{
		float fadeFactor = (float)((FadeOutTimeS + timeS - songPosS) / FadeOutTimeS);
		for (auto& sample : audioBuffer)
			sample *= fadeFactor;
		wav.addSamples(audioBuffer);
		return songPosS > (FadeOutTimeS + timeS) ? -1.f : 1.f;
	}

	wav.addSamples(audioBuffer);

	if (count == 0) //end of song
	{	//If there is audio at song end, loop and fade out
		//return -1;
		if (std::any_of(audioBuffer.begin(), audioBuffer.end(), [](SampleType sample) {return sample != 0; }))
		{
			isFadingOut = true;
			return 1;
		}
		else
			return -1;
	}
	else
		return (float)min(1, songPosS / timeS);
}

void ModReader::endProcessing()
{
	SongReader::endProcessing();
	omptModule.reset();
}
