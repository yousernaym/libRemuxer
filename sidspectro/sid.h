#pragma once
#include "sound.h"

const int // waveforms
wvtest = 0,
wvpulse = 1,
wvsawtooth = 2,
wvtriangle = 3,
wvnoise = 4,
wvnone = 5,
wvring = 6,
wvpulsetriangle = 7,
wvpulsesawtooth = 8;

class SIDChannel
{
	friend class SID6581;
private:
	float frequency = 0;
	float oscamplitude = 0;
	float pulsewidth = 0;
	unsigned char amplitude = 0;
	float damplitude = 0;
	float a = 0;
	float d = 0;
	float s = 0;
	float r = 0;
	int waveform = wvnone;
	int gate;
	int phaseadsr;
	float phase = 0.;
	int rv = 0x7FFFF8; //noise shift register
public:
	SIDChannel();
	void SIDChannel::BuildTables();
	float NextSample();
};



////////////////////////////////////
//SID6581
////////////////////////////////////

class SID6581
{
private:
	float playposition = 0;
	float writeposition = 0;
	unsigned char regs[32];
	unsigned char *data;
	float cyclespersecond;
	SIDChannel channel[3];
	bool enabled = false;
	LoopSoundBuffer soundbuffer;
	float starttime = 0; // soundbuffer.GetTime();
public:
	SID6581(float _cyclespersecond, unsigned char *_data);
	void Reset();
	float NextSample();
	void Update(int count);
	int Read(int addr);
	void Write(int index, int value);
};
