#pragma once

class LoopSoundBuffer
{
public:
	int samples;
	int sampleslen;;
	float *buffer;
	float starttime;
	int pos;
//public:
	LoopSoundBuffer(int samples, int sampleslen);
	void PlayBuffer(int pos);
	void OnEnded();
	void WebkitSetup();
	void DummySetup();
	double GetTime();
	void Refill();
};