#include "mod2midi.h"

void freeMod()
{
	if (module)
	{
		if (Player_Active())
			Player_Stop();
		Player_Free(module);
		module = 0;
	}
}

void getCellRepLen(BYTE replen, int &repeat, int &length)
{
	repeat = (replen >> 5) & 7;
	length = replen & 31;
}

BOOL addNote(Marshal_Module &marMod, int pitch, int ins, int noteStart, int noteEnd)
{
	Marshal_Note notesElem;
	notesElem.pitch = pitch;
	notesElem.start = noteStart;
	notesElem.stop = noteEnd;
	marMod.tracks[ins].notes[marMod.tracks[ins].numNotes++] = notesElem;
	if (marMod.numTracks < ins + 1)
		marMod.numTracks = ins + 1;
	if (marMod.tracks[ins].numNotes >= MAX_TRACKNOTES)
		return FALSE;
	return TRUE;
}

double getRowDur(double tempo, double speed)
{
	double spb = 1 / (tempo / 60); //seconds per beat
	double spr = spb / 4; //seconds per row
	return spr * speed / 6;
}

//----------------------------------------
//From munitrk.c:

UWORD unioperands[UNI_LAST]={
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
