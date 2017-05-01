#include "mod2midi.h"
#include <sstream>
#include <string>

//#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable : 4996)

MODULE *module = 0;
Marshal_Module marshalModule;
//Notes notes;
char mixdownFilename[500];

char *getModMixdownFilename_intptr()
{
	return mixdownFilename;
}

BOOL initMikmod(char *_mixdownFilename)
{
	strcpy(mixdownFilename, _mixdownFilename);
	ZeroMemory(&marshalModule, sizeof(Marshal_Module));

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
    
	marshalModule.tempoEvents = new Marshal_TempoEvent[MAX_TEMPOEVENTS];
	marshalModule.tracks = new Marshal_Track[MAX_MIDITRACKS];
	for (int t = 0; t < MAX_MIDITRACKS; t++)
	{
		ZeroMemory(&marshalModule.tracks[t], sizeof(Marshal_Track));
		marshalModule.tracks[t].name = new char[100];
		marshalModule.tracks[t].notes = new Marshal_Note[MAX_TRACKNOTES];
		//for (int n = 0; n < MAX_MIDITRACKS; n++)
			//ZeroMemory(&marshalModule.tracks[t].notes[n], sizeof(Marshal_Note));
	}
	
	return TRUE;
}

#define safeDeleteArray(x) {if (x) delete[] x;}
void exitMikmod()
{
	safeDeleteArray(marshalModule.tempoEvents);
	if (marshalModule.tracks)
	{
		for (int t = 0; t < MAX_MIDITRACKS; t++)
		{
			safeDeleteArray(marshalModule.tracks[t].name);
			safeDeleteArray(marshalModule.tracks[t].notes);
		}
		safeDeleteArray(marshalModule.tracks);
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
	//int noteEndOffset; //non-zero if there's information in current row that note will end this row
	int eff[UNI_LAST];
	int effValues[UNI_LAST][MAX_EFFECT_VALUES];
	int numEffs;
	int repeatsLeft;
	int offset;
	//int noteStartOffset;
	//int noteEndOffset;
};

BOOL loadMod(char *path, Marshal_Module &marMod, BOOL mixdown, BOOL modInsTrack)
{
	marMod = marshalModule;
	module = 0;
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
		module->loop = false;
		//"Running status"-----------------
		vector<RunningCellInfo> runningRowInfo;
		runningRowInfo.resize(module->numchn);
		//----------------------------------
			
		//Marshall data--------------------------
		marMod.minPitch = MAX_PITCHES;
		marMod.maxPitch = 0;
		//marMod.ticksPerMeasure = module->sngspd * 16; //assuming 4 rows per beat
		int curSongSpeed = module->initspeed;
		//int curSongBpm = module->inittempo;
		double rowDur = getRowDur(module->inittempo, module->initspeed);

		marMod.tempoEvents[0].tempo = module->inittempo;
		marMod.tempoEvents[0].time = 0;
		marMod.numTempoEvents = 1;
		
		marMod.numTracks = 0;//module->numins;
		for (int i=0;i<MAX_MIDITRACKS;i++)
			marMod.tracks[i].numNotes = 0;
		//-----------------------------------
		Notes notes;
		notes.reserve(MAX_MIDITRACKS);
		
		if (modInsTrack)	//Track per instrument/sample
		{	
			if (module->instruments)
			{
				for (int i=0;i<module->numins;i++)
					strcpy(marMod.tracks[i+1].name, module->instruments[i].insname);
			}
			else
			{
				for (int i=0;i<module->numsmp;i++)
					strcpy(marMod.tracks[i+1].name, module->samples[i].samplename);
			}
		}
		else	//Track per mod channel
		{	
			for (int i=0;i<module->numchn;i++)
			{
				std::ostringstream s;
				s << "Channel " << i;
				strcpy(marMod.tracks[i+1].name, s.str().c_str());
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
				//Loop through channels/tracks
				for (int t = 0; t < module->numchn; t++)
				{
					BYTE *track = module->tracks[pattern*module->numchn+t];
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
								curCellInfo.note = track[rowDataOffset++]+1;
							else if (opCode == UNI_INSTRUMENT)
							{
								curCellInfo.ins = track[rowDataOffset++]+1;
							
								//if (!runningRowInfo[t].note && runningRowInfo[t].silentNote && runningRowInfo[t].sampleEnded)
									
								//if (marMod.numTracks < curCellInfo.ins + 1)
									//marMod.numTracks = curCellInfo.ins + 1;
							}
							else 
							{
								curCellInfo.eff[curCellInfo.numEffs] = opCode;
								for (int i=0;i<unioperands[opCode];i++)
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
					int noteStartOffset = 0, noteEndOffset = 0;
					//if (noteEndOffset != 0)
						//curCellInfo.note = -1;
					runningRowInfo[t].volVelocity = 0;
					
					//check all effects----------------
					for (int i=0;i<curCellInfo.numEffs;i++)
					{ 
						int effValues[MAX_EFFECT_VALUES];
						int eff = curCellInfo.eff[i];
						for (int j=0;j<MAX_EFFECT_VALUES;j++)
						{
							//If effect value is not zero or effect type has no memory of its value, update the running value (only volume effects are considered)
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
								marMod.tempoEvents[marMod.numTempoEvents].tempo = value;
								marMod.tempoEvents[marMod.numTempoEvents].time = timeT;
								if (++marMod.numTempoEvents >= MAX_TEMPOEVENTS)
									return FALSE;
							}
							rowDur = getRowDur(marMod.tempoEvents[marMod.numTempoEvents-1].tempo, curSongSpeed);
						}
						else if (curCellInfo.eff[i] == UNI_KEYOFF || curCellInfo.eff[i] == UNI_KEYFADE)
							curCellInfo.note = -1;
						else if (curCellInfo.eff[i] == UNI_PTEFFECTB)
						{
							ptnJump = effValues[0];
							if (ptnJump <= p)
								ptnJump = -1;
						}
						else if (curCellInfo.eff[i] == UNI_PTEFFECTD)
						{
							ptnJump = p + 1;
							ptnStart = effValues[0];
						}
						else if (curCellInfo.eff[i] == UNI_PTEFFECTC)
							runningRowInfo[t].vol = effValues[0];
						else if (curCellInfo.eff[i] == UNI_PTEFFECTE)
						{
							int subeff = (effValues[0] >> 4) & 0xf;
							int value = effValues[0] & 0xf;
							if (value < curSongSpeed)
							{
								if (subeff == 0xc)
								{
									noteEndOffset = value;
									//if (curCellInfo.note>0) //note will end next row
										//noteEndOffset = -curSongSpeed + value;
									//else if (!curCellInfo.note) //note will end this row
										//runningRowInfo[t].noteEndOffset = value;

								}
								else if (subeff == 0xd)
									noteStartOffset = value;
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
							runningRowInfo[t].volVelScale = curSongSpeed - 1;
							int value = effValues[0];
							//if (value > 0 || curCellInfo.eff[i] <= UNI_PTEFFECTE)
							runningRowInfo[t].volVelocity = value > 0xf ? (value>>4) : -value;
						}
						else if (curCellInfo.eff[i] == UNI_XMEFFECTEA || curCellInfo.eff[i] == UNI_XMEFFECTEB || curCellInfo.eff[i] == UNI_PTEFFECTE || curCellInfo.eff[i] == UNI_S3MEFFECTD)
						{
							runningRowInfo[t].volVelScale = 1;
							int value = effValues[0];
							if (curCellInfo.eff[i] == UNI_PTEFFECTE)
							{
								if ((value & 0xf0) == (0xa << 4))
									runningRowInfo[t].volVelocity = value & 0xf;
								else if ((value & 0xf0) == (0xb << 4))
									runningRowInfo[t].volVelocity = -(value & 0xf);
							}
							else if (curCellInfo.eff[i] == UNI_S3MEFFECTD)
							{
								if ((value & 0xf) == 0xf)
									runningRowInfo[t].volVelocity = value >> 4;
								else //value & 0xf0 == 0xf0
									runningRowInfo[t].volVelocity = -value;
							}
							else //if (value > 0)
							{
								if (curCellInfo.eff[i] == UNI_XMEFFECTEA)
									runningRowInfo[t].volVelocity = value;
								else //UNI_XMEFFECTEB
									runningRowInfo[t].volVelocity = -value;
							}
						}
					}
					//----------------------------------
					int curCellNote = curCellInfo.note;
										
					if (runningRowInfo[t].vol <= 0 && runningRowInfo[t].note)
					{
						curCellNote = -1; //note off
					}
					if (runningRowInfo[t].vol > 0 && !runningRowInfo[t].note && runningRowInfo[t].silentNote) //note was silent but now has volume so start new note
						volNewNote = true;
					
					if (curCellInfo.ins && !curCellNote)
					{	//if ins but no note specified, reset vol and volenv and if previous note had ended, begin new note
						runningRowInfo[t].vol = runningRowInfo[t].startVol;
						runningRowInfo[t].volEnvStartS = timeS;
						runningRowInfo[t].volEnvStartT = timeT;
						if (!runningRowInfo[t].note && runningRowInfo[t].silentNote)
							volNewNote = true;
					}

					if (curCellNote || volNewNote)
					{ //a new note/note-off was played or a silent note became audible
						if (curCellInfo.ins && curCellNote > 0)
						{  //ins + note was specified
							int smpIndex;
							runningRowInfo[t].volEnvEnd = 0;
							runningRowInfo[t].vol = runningRowInfo[t].startVol = 64;
							 if (module->instruments)
							{
								INSTRUMENT instrument = module->instruments[curCellInfo.ins-1];
								smpIndex = instrument.samplenumber[curCellNote-1];
								if (smpIndex != UWORD(-1))
								{
									if (module->instruments[curCellInfo.ins-1].volflg == 1  //volenv on but not sustain or loop
										&& instrument.volenv[instrument.volpts-1].val == 0)  //last env point = 0
									{
										runningRowInfo[t].volEnvEnd = instrument.volenv[instrument.volpts-1].pos; 
									}
									runningRowInfo[t].startVol = instrument.globvol;
								}
								else
									curCellNote = -1;
							}
							else
								smpIndex = curCellInfo.ins-1;
							if (curCellNote > 0)
							{
								SAMPLE sample = module->samples[smpIndex];
								if (sample.length == 0)
									curCellNote = -1;
								runningRowInfo[t].loopSample = (sample.flags & SF_LOOP) == SF_LOOP;
								runningRowInfo[t].sampleLength = sample.length;
							}
						}
						//runningRowInfo[t].vol = runningRowInfo[t].startVol;

						if (curCellNote > 0)
						{
							if (marMod.maxPitch < curCellNote)
								marMod.maxPitch = curCellNote;
							if (marMod.minPitch > curCellNote && curCellNote != -1)
								marMod.minPitch = curCellNote;
						}
												
						if (runningRowInfo[t].note)
						{ //End previous note
							int noteEndT = timeT;
							if (curCellNote == -1) //no new note starts at this row, so if there's an offset to note-end it should be used for the running note. Otherwise it should be used for the note that starts (and ends) at this row.
								noteEndT += noteEndOffset;
							if (!addNote(runningRowInfo, notes, modInsTrack, t, noteEndT))
								goto error;
						}
						
						if (curCellNote > 0)
						{
							runningRowInfo[t].silentNote = curCellNote;
							runningRowInfo[t].sampleStartT = timeT;
							runningRowInfo[t].sampleStartS = timeS;
							runningRowInfo[t].noteStartT = timeT + noteStartOffset;
							runningRowInfo[t].noteStartS = timeS;
							if (curCellInfo.ins)
								runningRowInfo[t].ins = curCellInfo.ins;
						}
						if (curCellInfo.ins)
						{
							runningRowInfo[t].volEnvStartT = timeT;
							runningRowInfo[t].volEnvStartS = timeS;
						}
						if (volNewNote)
						{
							curCellNote = runningRowInfo[t].silentNote;
							runningRowInfo[t].noteStartT = timeT + noteStartOffset;
							runningRowInfo[t].noteStartS = timeS;
						}
						
						runningRowInfo[t].note = max(curCellNote, 0);
						if (curCellNote > 0 && noteEndOffset)
							curCellNote = -1; //note ends before next row
						else
							curCellNote = 0;
					}
					
					//Step forward one row
					double timeSPlus1 = timeS + rowDur * (ptnDelay + 1);
					int timeTPlus1 = timeT + curSongSpeed * (ptnDelay + 1);
					if (runningRowInfo[t].note || runningRowInfo[t].silentNote)
					{ //a note is running
						bool hadVolume = runningRowInfo[t].vol > 0;
						double elapsedNoteS = timeSPlus1 - runningRowInfo[t].noteStartS;
						double elapsedSampleS = timeSPlus1 - runningRowInfo[t].sampleStartS;
						double elapsedVolEnvS = timeSPlus1 - runningRowInfo[t].volEnvStartS;
						if (!curCellNote && noteEndOffset > 0)
							curCellNote = -1;
						
						if (!runningRowInfo[t].loopSample)
						{ //sample is not looping
							//check if sample has ended
							double freqRatio = 1046.50 / (440 * pow(2.0, (runningRowInfo[t].note-59)/12.0));
							if (curCellNote == 0 && elapsedSampleS > runningRowInfo[t].sampleLength * freqRatio / 44100)
							{
								curCellNote = -1; //note off
								runningRowInfo[t].silentNote = 0;
								//runningRowInfo[t].sampleEnded = true;
							}
						}
						if (runningRowInfo[t].volEnvEnd > 0)
						{ //vol env is active, not looping and ends with 0
							//check if volenv has ended
							if (elapsedVolEnvS > runningRowInfo[t].volEnvEnd / 47.647)
							{
								curCellNote = -1; //note off
								runningRowInfo[t].silentNote = 0;
							}
						}
						if (runningRowInfo[t].volVelocity != 0)
						{  //voluume up/down effect
							runningRowInfo[t].vol += runningRowInfo[t].volVelocity * runningRowInfo[t].volVelScale;
						}
						if (runningRowInfo[t].vol <= 0 && runningRowInfo[t].note)
							curCellNote = -1; //note off
						
					}
					
					//POSSIBLE BUG: It's possible for runningRowInfo[t].note to be non-zero. Should it be possible?
					if (curCellNote == -1 && runningRowInfo[t].note)
					{  //note ends before next row
						int noteEndT = noteEndOffset ? timeT + noteEndOffset : timeTPlus1;
						if (!addNote(runningRowInfo, notes, modInsTrack, t, noteEndT))
							goto error;
						runningRowInfo[t].note = 0;
						//curCellNote = 0;
					}
					//if (!runningRowInfo[t].note)
						//runningRowInfo[t].vol = 0;
					
					//runningRowInfo[t].noteStartOffset = noteStartOffset;
					//runningRowInfo[t].noteEndOffset = noteEndOffset;
				}
				if (!ptnStart || ptnJump >= 0)
				{
					timeT += curSongSpeed * (ptnDelay + 1);
					timeS += rowDur * (ptnDelay + 1);
				}
				if (ptnJump >= 0)
				{
					p = ptnJump-1;
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
		marMod.songLengthT = timeT;
		//Song ended, add all running notes and set their end time to end of song
		for (unsigned t=0;t<runningRowInfo.size();t++)
		{
			if (runningRowInfo[t].note)
			{
				if (!addNote(runningRowInfo, notes, modInsTrack, t, timeT))
					goto error;
			}
		}
		//Copy notes to marshal struct
		for (unsigned t = 0; t < notes.size(); t++)
		{
			marMod.numTracks = notes.size();
			marMod.tracks[t].numNotes = notes[t].size();
			int i = 0;
			for (TrackNotes::iterator it = notes[t].begin(); it != notes[t].end(); it++)
				marMod.tracks[t].notes[i++] = *it;
		}
	
		error:
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
