#include "mod2midi.h"
#include <sstream>
#include <string>

//#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable : 4996)

Marshal_Song marshalSong;
//Notes notes;
char mixdownFilename[500];

char *getModMixdownFilename_intptr()
{
	return mixdownFilename;
}

BOOL initLib(char *_mixdownFilename)
{
	strcpy(mixdownFilename, _mixdownFilename);
	ZeroMemory(&marshalSong, sizeof(Marshal_Song));

	MikMod_RegisterDriver(&drv_wav);
	MikMod_RegisterAllLoaders();
	//MikMod_RegisterDriver(&drv_win);
	/*MikMod_RegisterLoader(&load_xm);
	MikMod_RegisterLoader(&load_mod);
	MikMod_RegisterLoader(&load_it);
	MikMod_RegisterLoader(&load_s3m);
	MikMod_RegisterLoader(&load_stm);*/
	/* initialize the library */
	md_mode |= DMODE_SOFT_MUSIC | DMODE_HQMIXER | DMODE_16BITS | DMODE_INTERP;
    
	marshalSong.tempoEvents = new Marshal_TempoEvent[MAX_TEMPOEVENTS];
	marshalSong.tracks = new Marshal_Track[MAX_MIDITRACKS];
	for (int t = 0; t < MAX_MIDITRACKS; t++)
	{
		ZeroMemory(&marshalSong.tracks[t], sizeof(Marshal_Track));
		marshalSong.tracks[t].name = new char[100];
		marshalSong.tracks[t].notes = new Marshal_Note[MAX_TRACKNOTES];
		//for (int n = 0; n < MAX_MIDITRACKS; n++)
			//ZeroMemory(&marshalModule.tracks[t].notes[n], sizeof(Marshal_Note));
	}
	
	return TRUE;
}

#define safeDeleteArray(x) {if (x) delete[] x;}
void exitLib()
{
	safeDeleteArray(marshalSong.tempoEvents);
	if (marshalSong.tracks)
	{
		for (int t = 0; t < MAX_MIDITRACKS; t++)
		{
			safeDeleteArray(marshalSong.tracks[t].name);
			safeDeleteArray(marshalSong.tracks[t].notes);
		}
		safeDeleteArray(marshalSong.tracks);
	}
	remove(mixdownFilename);
}

struct Loop
{
	int startP;
	int startR;
	int loops;
	bool jump;
};

//const int MAX_EFFECTS_PER_CELL = 10;

struct CellInfo
{
	int ins;
	int note;
	int eff[UNI_LAST];
	int effValues[UNI_LAST][MAX_EFFECT_VALUES];
	int numEffs;
	int repeatsLeft;
	int offset;
	int noteStartOffset;
	int noteEndOffset = -1;
	int retriggerOffset = -1;
	int volSlideVel;
	int volSlideVelScale;
};

BOOL loadFile(char *path, Marshal_Song &marSong, BOOL mixdown, BOOL insTrack)
{
	marSong = marshalSong;
	Song song;
	if (!song.importMod(path, marSong, mixdown, insTrack))
		return FALSE;
	if (!song.createNoteList(marSong, insTrack))
		return FALSE;
	return TRUE;
}

BOOL Song::importMod(char *path, Marshal_Song &marSong, BOOL mixdown, BOOL insTrack)
{
	MODULE *module = 0;
	static char cmdLine[500];
	strcpy(cmdLine, "file=");
	strcat(cmdLine, mixdownFilename);
	md_device = 1;
	if (MikMod_Init(cmdLine))
	{
		fprintf(stderr, "Could not initialize Mikmod, reason: %s\n",
			MikMod_strerror(MikMod_errno));
		return FALSE;
	}
	module = Player_Load(path, 128, 0);
	if (module)
	{
		tracks.resize(module->numchn);
		for (unsigned i = 0; i < tracks.size(); i++)
		{
			tracks[i].ticks.reserve(30000);
			tracks[i].ticks.resize(1);
		}

		
		module->loop = false;
		//"Running status"-----------------
		vector<RunningCellInfo> runningRowInfo;
		runningRowInfo.resize(module->numchn);
		//----------------------------------

		//Marshall data--------------------------
		marSong.minPitch = MAX_PITCHES;
		marSong.maxPitch = 0;
		//marSong.ticksPerMeasure = module->sngspd * 16; //assuming 4 rows per beat
		int curSongSpeed = module->initspeed;
				
		//int curSongBpm = module->inittempo;
		double rowDur = getRowDur(module->inittempo, module->initspeed);

		marSong.tempoEvents[0].tempo = module->inittempo;
		marSong.tempoEvents[0].time = 0;
		marSong.numTempoEvents = 1;

		marSong.numTracks = 0;//module->numins;
		for (int i = 0; i < MAX_MIDITRACKS; i++)
			marSong.tracks[i].numNotes = 0;
		//-----------------------------------

		if (insTrack)	//Track per instrument/sample
		{
			if (module->instruments)
			{
				for (int i = 0; i < module->numins; i++)
					strcpy(marSong.tracks[i + 1].name, module->instruments[i].insname);
			}
			else
			{
				for (int i = 0; i < module->numsmp; i++)
					strcpy(marSong.tracks[i + 1].name, module->samples[i].samplename);
			}
		}
		else	//Track per mod channel
		{
			for (int i = 0; i < module->numchn; i++)
			{
				std::ostringstream s;
				s << "Channel " << i;
				strcpy(marSong.tracks[i + 1].name, s.str().c_str());
			}
		}

		Loop loop;
		ZeroMemory(&loop, sizeof(loop));

		int timeT = 0;
		double timeS = 0;
		int ptnStart = 0;


		//Loop through patterns
		for (int p = 0; p < module->numpos; p++)
		{
			int ptnJump = -1;
			int pattern = module->patterns[module->positions[p]];
			vector<CellInfo> curRowInfo(MAX_MODTRACKS);
			//Loop through rows
			for (int r = 0; r < module->pattrows[pattern]; r++)
			{
				int ptnDelay = 0;
				//curSongSpeed = readSpeed();
				double tickDur = rowDur / curSongSpeed;
				//Loop through channels/tracks
				for (int t = 0; t < module->numchn; t++)
				{
					
					RunningTickInfo *curTick = &tracks[t].ticks[timeT];
					//curTick->vol = tracks[t].getPrevTick(timeT);
					BYTE *track = module->tracks[pattern*module->numchn + t];
					bool volNewNote = false;
					int cellRep, cellLen;
					CellInfo &curCellInfo = curRowInfo[t];
					if (curCellInfo.repeatsLeft == 0)
					{ //Current row should not be repeated, read cell at new row
						getCellRepLen(track[curCellInfo.offset], cellRep, cellLen);
						int rowDataOffset = 1 + curCellInfo.offset;
						curCellInfo.repeatsLeft = cellRep;
						curCellInfo.note = curCellInfo.ins = curCellInfo.numEffs = 0;
						while (rowDataOffset < cellLen + curCellInfo.offset)
						{
							int opCode = track[rowDataOffset++];
							if (opCode == UNI_NOTE)
								curCellInfo.note = track[rowDataOffset++] + 1;
							else if (opCode == UNI_INSTRUMENT)
							{
								curCellInfo.ins = track[rowDataOffset++] + 1;

								//if (!runningRowInfo[t].note && runningRowInfo[t].silentNote && runningRowInfo[t].sampleEnded)

								//if (marSong.numTracks < curCellInfo.ins + 1)
								//marSong.numTracks = curCellInfo.ins + 1;
							}
							else
							{
								curCellInfo.eff[curCellInfo.numEffs] = opCode;
								for (int i = 0; i < unioperands[opCode]; i++)
									curCellInfo.effValues[opCode][i] = track[rowDataOffset++];
								curCellInfo.numEffs++;
							}
						}
						curRowInfo[t].offset += cellLen;
					}
					else
						curRowInfo[t].repeatsLeft--;

					//
					if (ptnJump == -1)
					{
						if (r < ptnStart)
							continue;
						else
							ptnStart = 0;
					}

					//int noteStartOffset = runningRowInfo[t].noteStartOffset, noteEndOffset = runningRowInfo[t].noteEndOffset;
					//int noteStartOffset = 0, noteEndOffset = 0;
					//if (noteEndOffset != 0)
					//curCellInfo.note = -1;
					//runningRowInfo[t].volVelocity = 0;

					//check all effects----------------
					for (int i = 0; i < curCellInfo.numEffs; i++)
					{
						int effValues[MAX_EFFECT_VALUES];
						int eff = curCellInfo.eff[i];
						for (int j = 0; j < MAX_EFFECT_VALUES; j++)
						{
							//If effect value is not zero or effect type is not a volume effect with memory of its value, update the running value
							if (curCellInfo.effValues[eff][j] != 0 || eff != UNI_XMEFFECTA && eff != UNI_XMEFFECTEA && eff != UNI_XMEFFECTEB && eff != UNI_S3MEFFECTD)
								runningRowInfo[t].effValues[curCellInfo.eff[i]][j] = curCellInfo.effValues[curCellInfo.eff[i]][j];
							effValues[j] = runningRowInfo[t].effValues[curCellInfo.eff[i]][j];
						}


						if (curCellInfo.eff[i] == UNI_PTEFFECTF || curCellInfo.eff[i] == UNI_S3MEFFECTA || curCellInfo.eff[i] == UNI_S3MEFFECTT)
						{ //Speed/tempo changes
							int value = effValues[0];
							if (value <= 0x1f || curCellInfo.eff[i] == UNI_S3MEFFECTA)
								curSongSpeed = value;
							else
							{
								marSong.tempoEvents[marSong.numTempoEvents].tempo = value;
								marSong.tempoEvents[marSong.numTempoEvents].time = timeT;
								if (++marSong.numTempoEvents >= MAX_TEMPOEVENTS)
									return FALSE;
							}
							rowDur = getRowDur(marSong.tempoEvents[marSong.numTempoEvents - 1].tempo, curSongSpeed);
						}
						else if (curCellInfo.eff[i] == UNI_KEYOFF || curCellInfo.eff[i] == UNI_KEYFADE)
							curCellInfo.note = -1;
						else if (curCellInfo.eff[i] == UNI_PTEFFECTB)
						{
							ptnJump = effValues[0];
							if (ptnJump <= p)
								ptnJump = -1; //TODO: End song here?
						}
						else if (curCellInfo.eff[i] == UNI_PTEFFECTD)
						{
							ptnJump = p + 1;
							ptnStart = effValues[0];
						}
						else if (curCellInfo.eff[i] == UNI_PTEFFECTC)
							curTick->vol = effValues[0];
						else if (curCellInfo.eff[i] == UNI_PTEFFECTE)
						{
							int subeff = (effValues[0] >> 4) & 0xf;
							int value = effValues[0] & 0xf;
							if (value < curSongSpeed)
							{
								if (subeff == 0x9)
									curCellInfo.retriggerOffset = value;
								if (subeff == 0xc)
								{
									curCellInfo.noteEndOffset = value;
									//if (curCellInfo.note>0) //note will end next row
									//noteEndOffset = -curSongSpeed + value;
									//else if (!curCellInfo.note) //note will end this row
									//runningRowInfo[t].noteEndOffset = value;

								}
								else if (subeff == 0xd)
									curCellInfo.noteStartOffset = value;
							}
							if (subeff == 0xe)
								ptnDelay = value;
							if (subeff == 0x6)
							{
								if (value == 0)
								{
									loop.startP = p;
									loop.startR = r;
								}
								else
								{
									if (loop.loops == 0)
										loop.loops = value;
									else
										loop.loops--;
									if (loop.loops > 0)
									{
										ptnJump = loop.startP;
										ptnStart = loop.startR;
									}
								}
							}
						}
						if (curCellInfo.eff[i] == UNI_XMEFFECTA || curCellInfo.eff[i] == UNI_PTEFFECTA || curCellInfo.eff[i] == UNI_S3MEFFECTD && (effValues[0] & 0xf) != 0xf && (effValues[0] & 0xf0) != 0xf0)
						{
							curCellInfo.volSlideVelScale = curSongSpeed - 1;
							int value = effValues[0];
							//if (value > 0 || curCellInfo.eff[i] <= UNI_PTEFFECTE)
							curCellInfo.volSlideVel = value > 0xf ? (value >> 4) : -value;
						}
						else if (curCellInfo.eff[i] == UNI_XMEFFECTEA || curCellInfo.eff[i] == UNI_XMEFFECTEB || curCellInfo.eff[i] == UNI_PTEFFECTE || curCellInfo.eff[i] == UNI_S3MEFFECTD)
						{
							curCellInfo.volSlideVelScale = 1;
							int value = effValues[0];
							if (curCellInfo.eff[i] == UNI_PTEFFECTE)
							{
								if ((value & 0xf0) == (0xa << 4))
									curCellInfo.volSlideVel = value & 0xf;
								else if ((value & 0xf0) == (0xb << 4))
									curCellInfo.volSlideVel = -(value & 0xf);
							}
							else if (curCellInfo.eff[i] == UNI_S3MEFFECTD)
							{
								if ((value & 0xf) == 0xf)
									curCellInfo.volSlideVel = value >> 4;
								else //value & 0xf0 == 0xf0
									curCellInfo.volSlideVel = -value;
							}
							else //if (value > 0)
							{
								if (curCellInfo.eff[i] == UNI_XMEFFECTEA)
									curCellInfo.volSlideVel = value;
								else //UNI_XMEFFECTEB
									curCellInfo.volSlideVel = -value;
							}
						}
					}
					//----------------------------------
					int curCellNote = curCellInfo.note;

					if (curCellNote)
					{ //a new note/note-off was played
						if (curCellInfo.ins && curCellNote > 0)
						{  //ins + note was specified
							int smpIndex = -1;
							runningRowInfo[t].volEnvEnd = 0;
							runningRowInfo[t].startVol = 64;
							runningRowInfo[t].samplePlaying = true;
							curTick->ins = curCellInfo.ins;
							if (module->instruments)
							{
								INSTRUMENT instrument = module->instruments[curCellInfo.ins - 1];
								smpIndex = instrument.samplenumber[curCellNote - 1];
								if (smpIndex != UWORD(-1))
								{
									if (module->instruments[curCellInfo.ins - 1].volflg == 1  //volenv on but not sustain or loop
										&& instrument.volenv[instrument.volpts - 1].val == 0)  //last env point = 0
									{
										runningRowInfo[t].volEnvEnd = instrument.volenv[instrument.volpts - 1].pos;
									}
									runningRowInfo[t].startVol = instrument.globvol;
								}
								else
								{
									curCellNote = -1;
									runningRowInfo[t].samplePlaying = false;
								}
							}
							else
								smpIndex = curCellInfo.ins - 1;
							if (smpIndex >= 0)
							{
								SAMPLE sample = module->samples[smpIndex];
								if (sample.length == 0)
								{
									curCellNote = -1;
									runningRowInfo[t].samplePlaying = false;
								}
								runningRowInfo[t].loopSample = (sample.flags & SF_LOOP) == SF_LOOP;
								runningRowInfo[t].sampleLength = sample.length;
							}
						}

						if (curCellNote > 0) //note but not necessarily ins. Sample loop and length props already known, but sample start is reset.
						{
							//runningRowInfo[t].silentNote = curCellNote;
							//runningRowInfo[t].sampleStartT = timeT;
							runningRowInfo[t].sampleStartS = timeS;
							//runningRowInfo[t].noteStartT = timeT + curCellInfo.noteStartOffset;
							//runningRowInfo[t].noteStartS = timeS;
							if (curCellInfo.ins)
								runningRowInfo[t].ins = curCellInfo.ins;
						}

					}
					if (curCellInfo.ins)
					{
						curTick->vol = runningRowInfo[t].startVol;
						//runningRowInfo[t].volEnvStartT = timeT;
						runningRowInfo[t].volEnvStartS = timeS;
						runningRowInfo[t].volEnvEnded = false;
					}

					//write volume and notes in current cell ticks
					tracks[t].ticks.resize(tracks[t].ticks.size() + curSongSpeed);
					for (int i = 0; i < curSongSpeed; i++)
					{
						//Volume sliding effects
						if (i == 0 && curCellInfo.volSlideVelScale == 1 || i > 0 && curCellInfo.volSlideVelScale > 1)
							curTick->vol += curCellInfo.volSlideVel;

						//Check sample and envelope
						double tickTimeS = timeS + tickDur * i;
						double elapsedSampleS = tickTimeS - runningRowInfo[t].sampleStartS;
						double elapsedVolEnvS = tickTimeS - runningRowInfo[t].volEnvStartS;
						if (!runningRowInfo[t].loopSample)
						{ //sample is not looping
						  //check if sample has ended
							double freqRatio = 1046.50 / (440 * pow(2.0, (curTick->notePitch - 59) / 12.0));
							if (elapsedSampleS > runningRowInfo[t].sampleLength * freqRatio / 44100)
							{
								//curTick->notePitch = -1; //note off
								curTick->noteStart = -1;
								runningRowInfo[t].samplePlaying = false;
							}
						}
						if (runningRowInfo[t].volEnvEnd > 0)
						{ //vol env is active, not looping and ends with 0
						  //check if volenv has ended
							if (elapsedVolEnvS > runningRowInfo[t].volEnvEnd / 47.647)
							{
								curTick->noteStart = -1; 
								runningRowInfo[t].volEnvEnded = true;
							}
						}

						
						if (curCellInfo.noteStartOffset == i && curCellInfo.ins && runningRowInfo[t].samplePlaying)
							curTick->noteStart = timeT + i;
						if ((curCellInfo.noteStartOffset == i || curCellInfo.retriggerOffset == i)
							&& curCellInfo.note > 0 && !runningRowInfo[t].volEnvEnded && curTick->vol)
						{
							curTick->noteStart = timeT + i;
							curTick->notePitch = curCellInfo.note;
						}
						if (curCellInfo.noteEndOffset == i)
							curTick->noteStart = -1;
							
						//Copy current tick to next tick
						*(++curTick) = *(curTick - 1);
					}
				}
				if (!ptnStart || ptnJump >= 0)
				{
					timeT += curSongSpeed * (ptnDelay + 1);
					timeS += rowDur * (ptnDelay + 1);
				}
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
		marSong.songLengthT = timeT;


//	error:
		if (mixdown)
		{
			Player_Start(module);
			int i = 0;
			while (Player_Active() && module->sngtime < timeS * 1024) //break if last pattern has a pattern-break
			{
				//std::cerr << i++ << std::endl;
				MikMod_Update();
			}
			Player_Stop();
		}
		Player_Free(module);
		module = 0;
		MikMod_Exit();
		
	}
	else
	{
		MikMod_Exit();
		fprintf(stderr, "Could not load module, reason: %s\n",
			MikMod_strerror(MikMod_errno));
		return FALSE;
	}

	return TRUE;
}
BOOL Song::createNoteList(Marshal_Song &marSong, BOOL insTrack)
{
	notes.reserve(MAX_MIDITRACKS);
	
	for (unsigned i = 0; i < tracks.size(); i++)
	{
		for (unsigned j = 0; j < tracks[i].ticks.size(); j++)
		{
			RunningTickInfo curTick = tracks[i].ticks[j];
			RunningTickInfo prevTick = tracks[i].getPrevTick(j);
			//Add new note?
			if (prevTick.notePitch >= 0 && 
				prevTick.noteStart >= 0 &&
				(curTick.noteStart != prevTick.noteStart ||
					curTick.vol > 0 && prevTick.vol == 0))
			{  
				Marshal_Note note;
				note.pitch = prevTick.notePitch;
				note.start = prevTick.noteStart;
				note.stop = j;
				unsigned track = insTrack ? prevTick.ins : i + 1; //add 1 to channel because first track is reserved for "global" (modules can't have ins indices <1
				if (track >= notes.size())
					notes.resize(track + 1);
				notes[track].insert(note);
				if (notes[track].size() >= MAX_TRACKNOTES)
					return FALSE;

				if (marSong.maxPitch < note.pitch)
					marSong.maxPitch = note.pitch;
				if (marSong.minPitch > note.pitch)
					marSong.minPitch = note.pitch;
			}

		}
	}
	//Copy notes to marshal struct
	for (unsigned t = 0; t < notes.size(); t++)
	{
		marSong.numTracks = notes.size();
		marSong.tracks[t].numNotes = notes[t].size();
		int i = 0;
		for (TrackNotes::iterator it = notes[t].begin(); it != notes[t].end(); it++)
			marSong.tracks[t].notes[i++] = *it;
	}
	return TRUE;
}
