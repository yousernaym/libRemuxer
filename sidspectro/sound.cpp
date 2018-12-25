// -------------------------------------------------
// ---------------- Sound Device -------------------
// -------------------------------------------------

/* This object provides a loop sound buffer and a timer */
#include "sound.h"

LoopSoundBuffer::LoopSoundBuffer(int _samples, int _sampleslen)
{
	samples = _samples;
	sampleslen = _sampleslen;
	DummySetup();
}

//LoopSoundBuffer MozillaSetup = function()
//{
//	audio = new Audio();
//	audio.mozSetup(1, samples);
//	buffer = new Float32Array(sampleslen);
//	lastoffset = 0;
//	GetTime = function(){return audio.mozCurrentSampleOffset()/samples;};
//	Refill();
//}

void LoopSoundBuffer::PlayBuffer(int pos)
{
	//int idx = pos%4;
	//source[idx] = context.createBufferSource(); // creates a sound source

	//soundBuffer[idx] = context.createBuffer(1, sampleslen/4, samples);
	//var buffer = soundBuffer[idx].getChannelData(0);
	//var offset = (pos%4) * sampleslen/4;
	//for(int i=0; i<sampleslen/4; i++)
	//{
	//	buffer[i] = buffer[i + offset];
	//}
	//source[idx].buffer = soundBuffer[idx];
	//source[idx].connect(context.destination);
	//source[idx].onended = OnEnded.bind(this);
	//source[idx].start(pos*(sampleslen/4)/samples);
}


void LoopSoundBuffer::OnEnded()
{
	PlayBuffer(pos);
	pos++;
}


void LoopSoundBuffer::WebkitSetup()
{
	//GetTime = function(){return context.currentTime;};

	//source = [0,0,0,0];
	//soundBuffer = [0,0,0,0];

	//// Version 1, works also in Firefox 25
	//pos = 1;
	//buffer = new Float32Array(sampleslen)
	//PlayBuffer(0);
	//OnEnded();

	// Version 2, works only in Chrome
	/*
	soundBuffer = context.createBuffer(1, sampleslen, samples);
	buffer = soundBuffer.getChannelData(0);
	source = context.createBufferSource(); // creates a sound source
	source.buffer = soundBuffer;
	source.loop = true;
	source.connect(context.destination);	
	source.start(0);
*/
}

void LoopSoundBuffer::DummySetup()
{
	buffer = new float[sampleslen];
	starttime = 0;// new Date().getTime();
	pos = 1;
}

double GetTime()
{
	return 0;// timet();
}

void LoopSoundBuffer::Refill()
{
	//var currentoffset = audio.mozCurrentSampleOffset();
	//var lastoffset = lastoffset;	
	//var diff = currentoffset+samples - lastoffset;
	//if (diff < 0) return;
	//var locbuffer = new Float32Array(diff);
	//var offset = Math.floor(lastoffset%(sampleslen));
	//for(int i=0; i<diff; i++) 
	//{
	//	locbuffer[i] = buffer[offset];
	//	offset++;
	//	if (offset >= sampleslen) offset = 0;
	//}
	//lastoffset = lastoffset + diff;
	//var written = audio.mozWriteAudio(locbuffer);
	////console.log(written);
	////console.log(audio.mozCurrentSampleOffset());
	//window.setTimeout(Refill.bind(this), 200);
}

