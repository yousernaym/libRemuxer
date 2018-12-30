// -------------------------------------------------
// ---------------------- SID ----------------------
// -------------------------------------------------
#include "sid.h"
#include <cmath>

//use strict"
int samples = 44100;

const int NOISETABLESIZE = 256;
int noiseLSB[NOISETABLESIZE];
int noiseMSB[NOISETABLESIZE];
int noiseMID[NOISETABLESIZE];

void initNoise()
{
	for (int i = 0; i < NOISETABLESIZE; i++)
	{
		noiseLSB[i] = ((((i >> (7 - 2)) & 0x04) | ((i >> (4 - 1)) & 0x02)
			| ((i >> (2 - 0)) & 0x01))) & 0xFF;
		noiseMID[i] = ((((i >> (13 - 8 - 4)) & 0x10)
			| ((i << (3 - (11 - 8))) & 0x08))) & 0xFF;
		noiseMSB[i] = ((((i << (7 - (22 - 16))) & 0x80)
			| ((i << (6 - (20 - 16))) & 0x40)
			| ((i << (5 - (16 - 16))) & 0x20))) & 0xFF;
	}
}

/* noise magic */
int NSHIFT(int v, int n)
{ 
	return (((v) << (n)) | ((((v) >> (23 - (n))) ^ (v >> (18 - (n)))) & ((1 << (n)) - 1)));
}

int NVALUE(int v)
{
	return (noiseLSB[v & 0xFF] | noiseMID[(v >> 8) & 0xFF] | noiseMSB[(v >> 16) & 0xFF]);
}


int attackrate[] = { 2, 8, 16, 24, 38, 56, 68, 80, 100, 250, 500, 800, 1000, 3000, 5000, 8000 };
int decayrate[] = { 6, 24, 48, 72, 114, 168, 204, 240, 300, 750, 1500, 2400, 3000, 9000, 15000, 24000 };
int releaserate[] = { 6, 24, 48, 72, 114, 168, 204, 240, 300, 750, 1500, 2400, 3000, 9000, 15000, 24000 };

const int phattack=0, phdecay=1, phsustain=2, phrelease=3, phidle=4; // phase
const int gopen=0, gclosed=1; // gate

SIDChannel::SIDChannel()
{
	gate = gclosed;
	phaseadsr = phattack;
	BuildTables();
}

void SIDChannel::BuildTables()
{
	/*
	wavetable00 = new Array(2);
	wavetable10 = new Array(4096);
	wavetable20 = new Array(4096);
	wavetable30 = new Array(4096);
	wavetable40 = new Array(8192);
	wavetable50 = new Array(8192);
	wavetable60 = new Array(8192);
	wavetable70 = new Array(8192);

	for(var i=0; i<4096; i++)
	{
		wavetable10[i] = (i < 2048 ? i << 4 : 0xffff - (i << 4))&0xFFFF;
			wavetable20[i] = (i << 3) & 0xFFFF;
			wavetable30[i] = waveform30_8580[i] << 7;
		wavetable40[i+4096] = 0x7FFFF;
		wavetable50[i+4096] = 0;
		wavetable60[i+4096] = 0;
		wavetable70[i+4096] = 0;
	}
*/
}


float SIDChannel::NextSample()
{
	float y = 0;
	/*
	var ampl = 0x0;
	var f = 0;
	var pw = 0;
	switch(waveform)
	{
		case wvpulsesawtooth:
				if (f <= pw) ampl = 0x0000; else ampl = f >>> 17;
			break;
			case wvsawtooth:
				ampl = f >>> 17;
			break;
			case wvring:
				//ampl ^= pv->vprev->f & 0x80000000;
			if (f < 0x80000000) ampl = f >>> 16; else ampl = 0xffff - (f >>> 16);
			break;
			case wvtriangle:
				if (f < 0x80000000) ampl = f >>> 16; else ampl = 0xffff - (f >> 16);
			break;
			case wvpulsetriangle:
			if (f <= pw) ampl = 0x0000; else if (f < 0x80000000) ampl = f >>> 16; else ampl = 0xffff - (f >>> 16);
			break;
		case wvnoise:
			//ampl = ((DWORD)NVALUE(NSHIFT(pv->rv, pv->f >> 28))) << 7;
			break;
			case wvpulse:
				if (f >= pw) ampl = 0x7fff;
			break;
	}

*/

	float phase = this->phase - floor(this->phase);
	float oldphase = floor(phase);
	phase += (float)frequency/samples;
	int newphase = floor(phase);
	
	switch(waveform)
	{
	case wvnone:
		return 0.;

	case wvsawtooth:
		oscamplitude = phase;
		break;

	case wvpulsesawtooth:
		if (phase < pulsewidth) oscamplitude = 0; else oscamplitude = phase;
		break;

	case wvpulsetriangle:
		if (phase < pulsewidth) oscamplitude = 0.; else
		if (phase < 0.5) oscamplitude = phase * 2.; else oscamplitude = 1.-(phase-0.5) * 2.;
		break;

	case wvtriangle:
	case wvring:
		if (phase < 0.5) oscamplitude = phase * 2.; else oscamplitude = 1.-(phase-0.5) * 2.;
		break;

	case wvnoise:
		oscamplitude = NVALUE(NSHIFT(rv, (int)floor(phase*16))) / 256.f;
		if (oldphase != newphase)
		{
			rv = NSHIFT(rv, 16);
		}
		break;

	case wvpulse:
		if (phase < pulsewidth) oscamplitude = 1; else oscamplitude = 0;
		break;

	}
	amplitude += damplitude;

	switch(phaseadsr)
	{
	case phattack:
		if (gate == gopen)
		{
			a--;
			if (a <= 0)
			{
				phaseadsr = phdecay;
				amplitude = 1;
				damplitude = -(1. - s)/d;
			}
		} else {
			damplitude = -amplitude/r;
			phaseadsr = phrelease;
		}
		break;

	case phdecay:
		if (gate == gopen)
		{
			d--;
			if (d <= 0)
			{
				phaseadsr = phsustain;
				damplitude = 0;
				amplitude = s;
			}
		} else {
			damplitude = (float)-amplitude/r;
			phaseadsr = phrelease;
		}
		break;

	case phsustain:
		if (gate == gopen)
		{
			damplitude = 0;
			amplitude = s;
		} else {
			damplitude = -amplitude/r;
			phaseadsr = phrelease;
		}
		break;

	case phrelease:
		r--;
		if (r <= 0)
		{
			r = 0;
			waveform = wvnone;
			damplitude = 0;
			amplitude = 0;
		}
		break;
	}
	y += ((oscamplitude-0.5f)*amplitude);
	return y;
}

bool SIDChannel::isPlaying()
{
	return gate == gopen;
}
////////////////////////////////////
//SID6581
////////////////////////////////////

SID6581::SID6581(float _cyclespersecond, unsigned char *_data) : soundbuffer(samples, samples * 2)
{
	//_SIDCreate();
	cyclespersecond = _cyclespersecond;
	data = _data;
	Reset();
	enabled = true;
}

void SID6581::Reset()
{
	for(int i=0; i<32; i++)
		Write(i, 0);
}


float SID6581::NextSample()
{
	float y = 0;
	y += channel[0].NextSample();
	y += channel[1].NextSample();
	y += channel[2].NextSample();
	y *= (regs[24]&15)/15.f;
	return y*0.3f;
}

//var SIDbufferpos = 60000;
//var SIDoldcount = 0;
void SID6581::Update(int count)
{
	float currenttime = count / cyclespersecond;
	float currentsample = currenttime*samples;
	float oldsample = starttime*samples;
	starttime = currenttime;
	for(int i=(int)floor(oldsample); i<(int)floor(currentsample); i++)
	{
		soundbuffer.buffer[(i+60000)%(soundbuffer.sampleslen)] = NextSample();
		if ((i&127) == 0)
		{
			int column = ((i>>7)%600)*4;
			//var data = data;
			for(int j=0; j<400; j++)
			{
				int offset = 2400*j+column; 
				data[offset+0] = 0x00; 
				data[offset+1] = 0x00; 
				data[offset+2] = 0x00; 
				data[offset+3] = 0xFF; 
			}
			int freq = (int)floor(channel[0].frequency*0.3)+1;
			unsigned char ampl = channel[0].amplitude*255;
			if (freq <= 399) 
			{
				int offset = 2400 * (399 - freq) + column;
				data[offset+0] = ampl;
				data[offset+2400] = (unsigned char)(ampl*0.5);
				data[offset-2400] = (unsigned char)(ampl*0.5);
			}
			freq = (int)floor(channel[1].frequency*0.3)+1;
			ampl = channel[1].amplitude*255;
			if (freq <= 399)
			{
				int offset = 2400*(399-freq)+column+1;
				data[offset] = ampl;
				data[offset+2400] = (unsigned char)(ampl*0.5);
				data[offset-2400] = (unsigned char)(ampl*0.5);
			}
			freq = (int)floor(channel[2].frequency*0.3)+1;
			ampl = channel[2].amplitude*255;
			if (freq <= 399) 
			{
				int offset = 2400*(399-freq)+column+2;
				data[offset] = ampl;
				data[offset+2400] = (unsigned char)(ampl*0.5);
				data[offset-2400] = (unsigned char)(ampl*0.5);
			}
		}
	}
}

int SID6581::Read(int addr)
{
	//	return _SIDRead(addr);
	return 0;
}


void SID6581::Write(int index, int value)
{
	//	_SIDWrite(index, value);
	//	return;

	int oldvalue = regs[index];
	int i = (int)floor(index/7.f); // which channel
	int f;
	switch(index)
	{
	case 24:
		regs[index] = value;
		break;

	case 0: case 1: case 2: case 3: 
	case 7: case 8: case 9: case 10:
	case 14: case 15: case 16: case 17:
		regs[index] = value;
		
		f = regs[0+i*7] | (regs[1+i*7]<<8);
		if (f == 0) channel[i].frequency = 0.01;
		else channel[i].frequency = f * 0.060959458;
		channel[i].pulsewidth = ((regs[3+i*7] & 15)*256 + regs[2+i*7])/40.95/100.;

		//channel[i].pw = (regs[2+i*7] + ((regs[3+i*7]&0xF)<<8)) * 0x100100;
		//fs = regs[0+i*7] | (regs[1+i*7]<<8);
		// = pv->s->speed1
		break;

	case 6: case 13: case 20:
		regs[index] = value;
		if (channel[i].phaseadsr != phsustain) channel[i].s = (regs[index] >> 4)/15.;
		if (channel[i].phaseadsr != phrelease) channel[i].r = (releaserate[regs[6+i*7] & 15]/1000.)*samples;
		break;

	case 5: case 12: case 19:
		regs[index] = value;
		if (channel[i].phaseadsr != phattack) channel[i].a = (attackrate[regs[5+i*7] >> 4]/1000.)*samples;
		if (channel[i].phaseadsr != phdecay) channel[i].d = (decayrate[regs[5+i*7] & 15]/1000.)*samples;
		break;

	case 4: case 11: case 18:
		regs[index] = value;
		if (value&0x08)
		{
			channel[i].waveform = wvtest; 
			// TODO reset some stuff
		}
		else
		{
			switch ((value >> 4) & 0xF)
			{
			case 4:
				channel[i].waveform = wvpulse;
				break;
			case 2:
				channel[i].waveform = wvsawtooth;
				break;
			case 1:
				if (value & 4) channel[i].waveform = wvring; else channel[i].waveform = wvtriangle;
				break;
			case 8:
				channel[i].waveform = wvnoise;
				break;
			case 5:
				channel[i].waveform = wvpulsetriangle;
				break;
			case 6:
				channel[i].waveform = wvpulsesawtooth;
				break;
			case 0:
			default:
				channel[i].waveform = wvnone;
				break;
			}
		}
		if (i == 0) { Write(0, regs[0]); Write(1, regs[1]); }
		if (i == 1) { Write(7, regs[7]); Write(8, regs[8]);  }
		if (i == 2) { Write(14, regs[14]); Write(15, regs[15]); }
		/*
		switch(channel[i].phaseadsr)
		{
			case phattack:
			case phdecay:
			case phsustain:
				if (value&1) 
				
			break;
		}
*/
		if ((value & 1) == 0)
			channel[i].gate = gclosed;
		else
		{
			if ((oldvalue & 1) == 0)
			{
				channel[i].a = (attackrate[regs[5 + i * 7] >> 4] / 1000.)*samples;
				channel[i].d = (decayrate[regs[5 + i * 7] & 15] / 1000.)*samples;
				channel[i].s = (regs[6 + i * 7] >> 4) / 15.;
				channel[i].r = (releaserate[regs[6 + i * 7] & 15] / 1000.)*samples;

				channel[i].phaseadsr = phattack;
				channel[i].damplitude = 1. / channel[i].a;
				channel[i].amplitude = 0;
				channel[i].gate = gopen;
			}
		}
		break;
	}
	regs[index] = value;
	return;
}
