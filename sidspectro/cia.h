// -------------------------------------------------
// ------------------ CIA 6526----------------------
// -------------------------------------------------


#pragma once
#include <functional>

class CIA6526
{
public:
	std::function<void()>interruptfunc;
	int no;
	unsigned char regs[16];
	int iflags;
	int timeralatch;
	int timerblatch;
	int inttimerblatch;
	int clockcounter;
	bool interrupt;

	CIA6526(std::function<void(void)> _interruptfunc, int _no);
	void Reset();
	void TriggerInterrupt();
	void ClearInterrupt();
	void CalcSteps(int counts);
	int Read(int addr);
	void Write(int addr, int x);
};