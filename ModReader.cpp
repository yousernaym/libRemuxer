#include "ModReader.h"
#include <sstream>
#include <vector>
#include <string>
#include <iostream>

struct Loop
{
	int startP;
	int startR;
	int loops;
	bool jump;
};

struct CellInfo
{
	int ins = 0;
	int note = 0;
	int eff[UNI_LAST];
	int effValues[UNI_LAST][MAX_EFFECT_VALUES];
	int numEffs = 0;
	int noteStartOffset = 0;
	int noteEndOffset = -1;
	int retriggerOffset = -1;
	int volSlideVel = 0;
	int volSlideVelScale = 0;
};


struct RunningCellInfo
{
	int ins;
	int note;
	//int silentNote;
	int noteStartT;
	double noteStartS;
	//int volEnvStartT;
	double volEnvStartS;
	//int sampleStartT;
	double sampleStartS;
	bool loopSample;
	double sampleLength;
	int volEnvEnd;
	int startVol;
	//int vol;
	bool samplePlaying;
	bool volEnvEnded;
	int repeatsLeft;
	int offset;
	//int volVelocity;
	//int volVelScale;
	//int noteStartOffset;
	//int noteEndOffset;
	int effValues[UNI_LAST][MAX_EFFECT_VALUES];
};

int ModReader::curSongSpeed;
int ModReader::ptnDelay;
Song *ModReader::song;
Marshal_Song *ModReader::marSong;

void ModReader::init()
{
	MikMod_RegisterDriver(&drv_wav);
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

BOOL ModReader::loadFile(Song &_song, char *path, BOOL mixdown, BOOL insTrack)
{
	song = &_song;
	marSong = song->marSong;
	vector<RunningCellInfo> runningRowInfo;
	MODULE *module = 0;
	string cmdLine = "file=" + string(mixdownFilename);
	md_device = 1;
	if (MikMod_Init(cmdLine.c_str()))
	{
		//fprintf(stderr, "Could not initialize Mikmod, reason: %s\n",
			//MikMod_strerror(MikMod_errno));
		string err = (string)"Could not initialize Mikmod, reason: " + (string)MikMod_strerror(MikMod_errno);
		OutputDebugStringA(err.c_str());
		return FALSE;
	}
	module = Player_Load(path, 128, 0);
	if (module)
	{
		song->tracks.resize(module->numchn);
		for (unsigned i = 0; i < song->tracks.size(); i++)
		{
			song->tracks[i].ticks.reserve(30000);
			song->tracks[i].ticks.resize(1);
		}


		module->loop = false;
		//"Running status"-----------------
		vector<RunningCellInfo> runningRowInfo;
		runningRowInfo.resize(module->numchn);
		//----------------------------------

		//Marshall data--------------------------
		marSong->minPitch = MAX_PITCHES;
		marSong->maxPitch = 0;
		//marSong->ticksPerMeasure = module->sngspd * 16; //assuming 4 rows per beat
		curSongSpeed = module->initspeed;

		//int curSongBpm = module->inittempo;
		double rowDur = getRowDur(module->inittempo, module->initspeed);

		marSong->tempoEvents[0].tempo = module->inittempo;
		marSong->tempoEvents[0].time = 0;
		marSong->numTempoEvents = 1;

		marSong->numTracks = 0;//module->numins;
		for (int i = 0; i < MAX_MIDITRACKS; i++)
			marSong->tracks[i].numNotes = 0;
		//-----------------------------------

		if (insTrack)	//One track per instrument/sample
		{
			if (module->instruments)
			{
				for (int i = 0; i < module->numins; i++)
					strcpy_s(marSong->tracks[i + 1].name, MAX_TRACKNAME_LENGTH, module->instruments[i].insname);
			}
			else
			{
				for (int i = 0; i < module->numsmp; i++)
					strcpy_s(marSong->tracks[i + 1].name, MAX_TRACKNAME_LENGTH, module->samples[i].samplename);
			}
		}
		else	//One track per mod channel
		{
			for (int i = 0; i < module->numchn; i++)
			{
				std::ostringstream s;
				s << "Channel " << i;
				strcpy_s(marSong->tracks[i + 1].name, MAX_TRACKNAME_LENGTH, s.str().c_str());
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
			ptnDelay = 0;
			for (int r = 0; r < module->pattrows[pattern]; r++)
			{
				bool newPtnDelayFx = false;
				readRowFx();

				double tickDur = rowDur / curSongSpeed;
				//Loop through channels/tracks
				for (int t = 0; t < module->numchn; t++)
				{

					RunningTickInfo *curTick = &song->tracks[t].ticks[timeT];
					//curTick->vol = song->tracks[t].getPrevTick(timeT);
					BYTE *track = module->tracks[pattern*module->numchn + t];
					bool volNewNote = false;
					int cellRep, cellLen;
					CellInfo curCellInfo;// = curRowInfo[t];
					getCellRepLen(track[runningRowInfo[t].offset], cellRep, cellLen);
					if (runningRowInfo[t].repeatsLeft == 0 && ptnDelay == 0)
					{  //Current cell should not be repeated, read cell at new row
						runningRowInfo[t].repeatsLeft = cellRep;
					}

					int rowDataOffset = 1 + runningRowInfo[t].offset;

					//curCellInfo.note = curCellInfo.ins = curCellInfo.numEffs = 0;
					while (rowDataOffset < cellLen + runningRowInfo[t].offset)
					{
						int opCode = track[rowDataOffset++];
						if (opCode == UNI_NOTE)
							curCellInfo.note = track[rowDataOffset++] + 1;
						else if (opCode == UNI_INSTRUMENT)
						{
							curCellInfo.ins = track[rowDataOffset++] + 1;

							//if (!runningRowInfo[t].note && runningRowInfo[t].silentNote && runningRowInfo[t].sampleEnded)

							//if (marSong->numTracks < curCellInfo.ins + 1)
							//marSong->numTracks = curCellInfo.ins + 1;
						}
						else
						{
							curCellInfo.eff[curCellInfo.numEffs] = opCode;
							for (int i = 0; i < unioperands[opCode]; i++)
								curCellInfo.effValues[opCode][i] = track[rowDataOffset++];
							curCellInfo.numEffs++;
						}
					}

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
								marSong->tempoEvents[marSong->numTempoEvents].tempo = value;
								marSong->tempoEvents[marSong->numTempoEvents].time = timeT;
								if (++marSong->numTempoEvents >= MAX_TEMPOEVENTS)
									return FALSE;
							}
							rowDur = getRowDur(marSong->tempoEvents[marSong->numTempoEvents - 1].tempo, curSongSpeed);
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
							if (ptnDelay == 0 && subeff == 0xe)
							{
								ptnDelay = value + 1;
								newPtnDelayFx = true;
							}
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
						else if (curCellInfo.eff[i] == UNI_XMEFFECTEA || curCellInfo.eff[i] == UNI_XMEFFECTEB)
						{
							curCellInfo.volSlideVelScale = 1;
							if (curCellInfo.eff[i] == UNI_XMEFFECTEA)
								curCellInfo.volSlideVel = effValues[0];
							else //UNI_XMEFFECTEB
								curCellInfo.volSlideVel = -effValues[0];
						}
						else if (curCellInfo.eff[i] == UNI_PTEFFECTE || curCellInfo.eff[i] == UNI_S3MEFFECTD)
						{
							int value = effValues[0];
							if (curCellInfo.eff[i] == UNI_PTEFFECTE)
							{
								int subEff = (value & 0xf0) >> 4;
								if ((value & 0xf0) == (0xa << 4))
								{
									curCellInfo.volSlideVelScale = 1;
									curCellInfo.volSlideVel = value & 0xf;
								}
								else if ((value & 0xf0) == (0xb << 4))
								{
									curCellInfo.volSlideVel = -(value & 0xf);
									curCellInfo.volSlideVelScale = 1;
								}
							}
							else if (curCellInfo.eff[i] == UNI_S3MEFFECTD)
							{
								curCellInfo.volSlideVelScale = 1;
								if ((value & 0xf) == 0xf)
									curCellInfo.volSlideVel = value >> 4;
								else //value & 0xf0 == 0xf0
									curCellInfo.volSlideVel = -value;
							}
							else //if (value > 0)
							{

							}
						}
					}
					if (ptnDelay <= 1)
					{
						if (runningRowInfo[t].repeatsLeft == 0)
							runningRowInfo[t].offset += cellLen;
						else
							runningRowInfo[t].repeatsLeft--;
					}
					//----------------------------------
					if (ptnDelay == 0 || newPtnDelayFx)
					{
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
					}

					//write volume and notes in current cell ticks
					song->tracks[t].ticks.resize(song->tracks[t].ticks.size() + curSongSpeed);
					for (int i = 0; i < curSongSpeed; i++)
					{
						//Volume sliding effects
						if (i == 0 && curCellInfo.volSlideVelScale == 1 || i > 0 && curCellInfo.volSlideVelScale > 1)
						{
							curTick->vol += curCellInfo.volSlideVel;
							if (curTick->vol < 0)
								curTick->vol = 0;
						}

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

						//Mak beginning of new notes
						if (ptnDelay == 0 || newPtnDelayFx)
						{
							if (curCellInfo.noteStartOffset == i && curCellInfo.ins && runningRowInfo[t].samplePlaying)
								curTick->noteStart = timeT + i;
							if ((curCellInfo.noteStartOffset == i || curCellInfo.retriggerOffset > 0 && i > 0 && (i % curCellInfo.retriggerOffset) == 0)
								&& curCellInfo.note > 0 && !runningRowInfo[t].volEnvEnded && curTick->vol)
							{
								curTick->noteStart = timeT + i;
								curTick->notePitch = curCellInfo.note;
							}
						}
						if (curCellInfo.noteEndOffset == i)
							curTick->noteStart = -1;

						//Copy current tick to next tick
						*(++curTick) = *(curTick - 1);
					}
				}
				if (ptnDelay > 0)
					ptnDelay--;
				if (!ptnStart || ptnJump >= 0)
				{
					timeT += curSongSpeed; //*(ptnDelay + 1);
					timeS += rowDur; //*(ptnDelay + 1);
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
		marSong->songLengthT = timeT;


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

void ModReader::readRowFx()
{

}

void ModReader::getCellRepLen(BYTE replen, int &repeat, int &length)
{
	repeat = (replen >> 5) & 7;
	length = replen & 31;
}

//bool addNote(vector<RunningCellInfo> &row, vector<TrackNotes> &notes, bool modInsTrack, int channel, int noteEnd)
//{
//	Marshal_Note note;
//	note.pitch = row[channel].note;
//	note.start = row[channel].noteStartT; 
//	note.stop = noteEnd;
//	unsigned  track = modInsTrack ? row[channel].ins : channel + 1; //add 1 to channel because first track is reserved (for "global"?)
//	if (track >= notes.size())
//		notes.resize(track + 1);
//	notes[track].insert(note);
//	if (notes[track].size() >= MAX_TRACKNOTES)
//		return false;
//	//marMod.tracks[ins].notes[marMod.tracks[ins].numNotes++] = notesElem;
//	//if (marMod.numTracks < track + 1)
//		//marMod.numTracks = ins + 1;
//	//if (marMod.tracks[ins].numNotes >= MAX_TRACKNOTES)
//		//return FALSE;
//	return true;
//}

//bool addNote(vector<RunningCellInfo> &row, vector<TrackNotes> &notes, bool modInsTrack, int channel, int noteEnd)

double ModReader::getRowDur(double tempo, double speed)
{
	double spb = 1 / (tempo / 60); //seconds per beat
	double spr = spb / 4; //seconds per row
	return spr * speed / 6;
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

//ModReader::ModReader()
//{
//}
//
//
//ModReader::~ModReader()
//{
//}
