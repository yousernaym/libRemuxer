// -------------------------------------------------
// ------------------ CIA 6526----------------------
// -------------------------------------------------


#pragma once
#include <functional>
#include "sp_cpu.h"

class CIA6526
{
private:
	bool maskable;
	MOS6502 &cpu;
public:
	//std::function<void()>interruptfunc;
	int no;
	unsigned char regs[16];
	int iflags;
	int timeralatch;
	int timerblatch;
	int inttimerblatch;
	int clockcounter;
	bool interrupt;

	CIA6526(MOS6502 &_cpu, int _no, bool _maskable);
	void Reset();
	void TriggerInterrupt();
	void ClearInterrupt();
	void CalcSteps(int counts);
	int Read(int addr);
	void Write(int addr, int x);
};