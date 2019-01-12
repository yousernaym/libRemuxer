#include "ModReader.h"
#include <sstream>
#include <vector>
#include <string>
#include <iostream>
#include <assert.h>

void ModReader::sInit()
{
	MikMod_RegisterDriver(&drv_wav);
	MikMod_RegisterDriver(&drv_nos);
	MikMod_RegisterAllLoaders();
	md_mode |= DMODE_SOFT_MUSIC | DMODE_HQMIXER | DMODE_16BITS | DMODE_INTERP;
	//MikMod_RegisterDriver(&drv_win);
	/*MikMod_RegisterLoader(&load_xm);
	MikMod_RegisterLoader(&load_mod);
	MikMod_RegisterLoader(&load_it);
	MikMod_RegisterLoader(&load_s3m);
	MikMod_RegisterLoader(&load_stm);*/
	/* initialize the library */
}

ModReader::ModReader(Song &_song) : SongReader(_song)
{
	marSong = song.marSong;
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
			INSTRUMENT instrument = module->instruments[ins - 1];
			smpIndex = instrument.samplenumber[note - 1];
			if (smpIndex != UWORD(-1))
			{
				if (module->instruments[ins - 1].volflg == 1  //volenv on but not sustain or loop
					&& instrument.volenv[instrument.volpts - 1].val == 0)  //last env point = 0
				{
					runningCellInfo.volEnvEnd = instrument.volenv[instrument.volpts - 1].pos;
				}
				runningCellInfo.startVol = instrument.globvol;
			}
			else
				runningCellInfo.samplePlaying = false;
		}
		else
			smpIndex = ins - 1;
		if (smpIndex >= 0)
		{
			SAMPLE sample = module->samples[smpIndex];
			runningCellInfo.loopSample = (sample.flags & SF_LOOP) == SF_LOOP;

			runningCellInfo.sampleLength = sample.length < cellInfo.sampleOffset ? 0 : sample.length - cellInfo.sampleOffset;
			if (runningCellInfo.sampleLength == 0 || runningCellInfo.loopSample && sample.loopend > 0 && sample.loopend <= cellInfo.sampleOffset)
				runningCellInfo.samplePlaying = false;
		}
		
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
					
		if ((cellInfo.noteStartOffset == t || //Note starts at current tick (first tick of cell or start is offset using edx command)
			cellInfo.retriggerOffset > 0 && t % cellInfo.retriggerOffset == 0 ||  //Retrigger effect (e9x)
			cellInfo.noteStartOffset > 0 && t % curSongSpeed != 0 && t % cellInfo.noteStartOffset == 0)  //If pattern delay is used, note can be replayed on repeats if edx is used.
			&& cellInfo.note > 0 && !runningCellInfo.volEnvEnded && curTick.vol && runningCellInfo.samplePlaying)
		{
			curTick.noteStart = timeT + t;
			curTick.notePitch = cellInfo.note;
		}
		if (cellInfo.noteEndOffset == t)
			curTick.noteStart = -1;
			
		if (curTick.vol == 0)
			curTick.noteStart = -1;
		//Mark start of new note because volume went from 0 to >0
		//Copy current tick to next tick
		*track.getNextTick(timeT+t) = curTick;
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
			if (opCode == UNI_NOTE)
				cellInfo.note = track[rowDataOffset++] + 1;
			else if (opCode == UNI_INSTRUMENT)
				cellInfo.ins = track[rowDataOffset++] + 1;
			else
			{
				cellInfo.eff[cellInfo.numEffs] = opCode;
				for (int i = 0; i < unioperands[opCode]; i++)
				{
					assert(opCode < UNI_LAST && i < MAX_EFFECT_VALUES);
					cellInfo.effValues[opCode][i] = track[rowDataOffset++];
				}
				cellInfo.numEffs++;
			}
		}
		runningCellInfo.offset += cellLen;
	}
	else
		runningCellInfo.repeatsLeft--;
}

void ModReader::readCellFx(RunningTickInfo &firstTick, CellInfo &cellInfo, RunningCellInfo &runningCellInfo, int songPos, int row)
{
	for (int i = 0; i < cellInfo.numEffs; i++)
	{
		int effValues[MAX_EFFECT_VALUES];
		int eff = cellInfo.eff[i];
		for (int j = 0; j < MAX_EFFECT_VALUES; j++)
		{
			//If effect value is not zero or effect type does note have memory of its last value, update the running value
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
					throw (string)"Too many tempo events.";
			}
			rowDur = getRowDur(marSong->tempoEvents[marSong->numTempoEvents - 1].tempo, curSongSpeed);
		}
		else if (cellInfo.eff[i] == UNI_KEYOFF || cellInfo.eff[i] == UNI_KEYFADE)
		{
			if (runningCellInfo.volEnvEnd == 0) //Volume envelope disabled, just set volume to 0
				firstTick.vol = 0;
			//TODO: if volume envelope is active, a note off should let it pass beyond sustain point.
		}
		else if (cellInfo.eff[i] == UNI_PTEFFECT9)
		{
			cellInfo.sampleOffset = effValues[0] * 256;
		}
		else if (cellInfo.eff[i] == UNI_PTEFFECTB)
		{
			ptnJump = effValues[0];
			if (ptnJump <= songPos)
				ptnJump = -1;
		}
		else if (cellInfo.eff[i] == UNI_PTEFFECTD)
		{
			if (ptnJump == -1)
				ptnJump = songPos + 1;
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
						ptnJump = songPos;
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

void ModReader::beginProcessing(Args args)
{
	string cmdLine;
	BOOL mixdown = g_args.audioPath[0] > 0;

	md_device = 2; //nosound driver
	if (mixdown)
	{
		md_device = 1; //wav writer
		cmdLine = "file=" + string(g_args.audioPath);
	}

	if (MikMod_Init(cmdLine.c_str()))
	{
		string err = (string)"Could not initialize Mikmod, reason: " + (string)MikMod_strerror(MikMod_errno);
		OutputDebugStringA(err.c_str());
		assert(false);
	}
	module = Player_Load(g_args.inputPath, 64, 0);

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

		if (g_args.insTrack)	//One track per instrument/sample
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
				ostringstream s;
				s << "Channel " << i + 1;
				strcpy_s(marSong->tracks[i + 1].name, MAX_TRACKNAME_LENGTH, s.str().c_str());
			}
		}

		//ZeroMemory(&loop, sizeof(loop));

		//int timeT = 0;
		//double timeS = 0;
		//int ptnStart = 0;


		//Loop through patterns
		for (int p = 0; p < module->numpos; p++)
		{
			ptnJump = -1;
			int pattern = module->patterns[module->positions[p]];
			//Loop through rows
			for (int t = 0; t < module->numchn; t++)
				runningRowInfo[t].offset = runningRowInfo[t].repeatsLeft = 0;
			for (int r = 0; r < module->pattrows[pattern]; r++)
			{
				ptnDelay = 0;
				//Loop through channels/tracks
				for (int t = 0; t < module->numchn; t++)
				{
					BYTE *track = module->tracks[pattern*module->numchn + t];
					readNextCell(track, curRowInfo[t], runningRowInfo[t]);
					if (ptnJump >= 0 || r >= ptnStart)
						readCellFx(song.tracks[t].ticks[timeT], curRowInfo[t], runningRowInfo[t], p, r);
				}
				if (ptnJump == -1)
				{
					if (r < ptnStart)
						continue;
					else
						ptnStart = 0;
				}
				tickDur = rowDur / curSongSpeed;
				//Loop through channels/tracks
				for (int t = 0; t < module->numchn; t++)
				{
					song.tracks[t].ticks.resize(song.tracks[t].ticks.size() + curSongSpeed * (ptnDelay + 1));
					RunningTickInfo *curTick = &song.tracks[t].ticks[timeT];
					updateCell(*curTick, curRowInfo[t], runningRowInfo[t]);
					updateCellTicks(song.tracks[t], curRowInfo[t], runningRowInfo[t]);

				}

				/*if (!ptnStart || ptnJump >= 0)
				{*/
				timeT += curSongSpeed * (ptnDelay + 1);
				timeS += rowDur * (ptnDelay + 1);
				//}
				if (ptnJump >= 0)
				{
					p = ptnJump - 1;
					break;
				}
				/*if (loop.jump)
				{
				loop.jump = false;
				p = loop.startP - 1;
				ptnStart = loop.startR;
				break;
				}*/
			}
		}
		marSong->songLengthT = timeT;

		if (g_args.audioPath)
		{
			module->loop = false; //Don't allow backwards loops
			Player_Start(module);
		}

	}
	else
	{
		MikMod_Exit();
		ostringstream err;
		err << "Could not load module, reason: " << MikMod_strerror(MikMod_errno);
		OutputDebugStringA(err.str().c_str());
		throw err.str();
	}
	marSong->ticksPerBeat = 24;
	song.createNoteList();
}

float ModReader::process()
{
	if (Player_Active() && module->sngtime < timeS * 1024) //break if last pattern has a pattern-break
	{
		MikMod_Update();
		return (float)module->sngpos / module->numpos;
	}
	else
	{
		Player_Stop();
		Player_Free(module);
		module = 0;
		MikMod_Exit();
		return -1;
	}
}

//----------------------------------------
//From munitrk.c:

const UWORD unioperands[UNI_LAST] = {
	0, /* not used */
	1, /* UNI_NOTE */
	1, /* UNI_INSTRUMENT */
	1, /* UNI_PTEFFECT0 */
	1, /* UNI_PTEFFECT1 */
	1, /* UNI_PTEFFECT2 */
	1, /* UNI_PTEFFECT3 */
	1, /* UNI_PTEFFECT4 */
	1, /* UNI_PTEFFECT5 */
	1, /* UNI_PTEFFECT6 */
	1, /* UNI_PTEFFECT7 */
	1, /* UNI_PTEFFECT8 */
	1, /* UNI_PTEFFECT9 */
	1, /* UNI_PTEFFECTA */
	1, /* UNI_PTEFFECTB */
	1, /* UNI_PTEFFECTC */
	1, /* UNI_PTEFFECTD */
	1, /* UNI_PTEFFECTE */
	1, /* UNI_PTEFFECTF */
	1, /* UNI_S3MEFFECTA */
	1, /* UNI_S3MEFFECTD */
	1, /* UNI_S3MEFFECTE */
	1, /* UNI_S3MEFFECTF */
	1, /* UNI_S3MEFFECTI */
	1, /* UNI_S3MEFFECTQ */
	1, /* UNI_S3MEFFECTR */
	1, /* UNI_S3MEFFECTT */
	1, /* UNI_S3MEFFECTU */
	0, /* UNI_KEYOFF */
	1, /* UNI_KEYFADE */
	2, /* UNI_VOLEFFECTS */
	1, /* UNI_XMEFFECT4 */
	1, /* UNI_XMEFFECT6 */
	1, /* UNI_XMEFFECTA */
	1, /* UNI_XMEFFECTE1 */
	1, /* UNI_XMEFFECTE2 */
	1, /* UNI_XMEFFECTEA */
	1, /* UNI_XMEFFECTEB */
	1, /* UNI_XMEFFECTG */
	1, /* UNI_XMEFFECTH */
	1, /* UNI_XMEFFECTL */
	1, /* UNI_XMEFFECTP */
	1, /* UNI_XMEFFECTX1 */
	1, /* UNI_XMEFFECTX2 */
	1, /* UNI_ITEFFECTG */
	1, /* UNI_ITEFFECTH */
	1, /* UNI_ITEFFECTI */
	1, /* UNI_ITEFFECTM */
	1, /* UNI_ITEFFECTN */
	1, /* UNI_ITEFFECTP */
	1, /* UNI_ITEFFECTT */
	1, /* UNI_ITEFFECTU */
	1, /* UNI_ITEFFECTW */
	1, /* UNI_ITEFFECTY */
	2, /* UNI_ITEFFECTZ */
	1, /* UNI_ITEFFECTS0 */
	2, /* UNI_ULTEFFECT9 */
	2, /* UNI_MEDSPEED */
	0, /* UNI_MEDEFFECTF1 */
	0, /* UNI_MEDEFFECTF2 */
	0, /* UNI_MEDEFFECTF3 */
	2, /* UNI_OKTARP */
};


