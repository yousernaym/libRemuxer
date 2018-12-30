// -------------------------------------------------
// ------------------ CIA 6526----------------------
// -------------------------------------------------
#include "cia.h"

CIA6526::CIA6526(MOS6502 &_cpu, int _no, bool _maskable) : cpu(_cpu)
{
	//interruptfunc = std::bind(&MOS6502::MaskableInterrupt, c64.cpu);
	maskable = _maskable;
	no = _no;
	// ports, first four registers
	Reset();
}

void CIA6526::Reset()
{
	for (int i = 0; i < 16; i++)
	{
		regs[i] = 0;
		iflags = 0x0;
		timeralatch = 0xFFFF;
		timerblatch = 0xFFFF;
		clockcounter = 0x0;
		interrupt = false;
	}
}

void CIA6526::TriggerInterrupt()
{
	regs[13] |= 0x80;
	interrupt = true;
	
	if (maskable)
		cpu.MaskableInterrupt();
	else
		cpu.NonMaskableInterrupt();
}

void CIA6526::ClearInterrupt()
{
	//if (idr & (1<<7)) // interrupt request
	//{
	//}	
	interrupt = false;
}

void CIA6526::CalcSteps(int counts)
{
	if (regs[14] & 1) 
	{
		//DebugMessage(counts);
		int countera = regs[4] | (regs[5] << 8);
		countera = countera - counts;
		if (countera <= 0)
		{
			//DebugMessage(countera);
			countera += timeralatch;
			if (regs[14] & 8) //1 shot?
			{
				countera = 0x0;
				regs[14] &= ~1;
			}
			if (iflags & 1)
			{
				regs[13] |= 1;
				TriggerInterrupt();
			}
		}
		regs[4] = countera & 0xFF;
		regs[5] = (countera >> 8) & 0xFF;
	}

	if (regs[15] & 1) 
	{
		unsigned counterb = regs[6] | (regs[7] << 8);
		counterb = counterb - counts;
		if (counterb <= 0)
		{
			counterb += timerblatch;
			if (regs[15] & 8)        //1 shot?
			{
				counterb = 0x0;
				regs[15] &= ~1;
			}
			if (iflags & 2)
			{
				regs[13] |= 2;
				TriggerInterrupt();
			}
		}
		regs[6] = counterb & 0xFF;
		regs[7] = (counterb >> 8) & 0xFF;
		}
		/*
		clockcounter = clockcounter + counts;
		if (clockcounter >= 100000)
		{
			clockcounter = 0;
			if (regs[8] == 10)
			{
				regs[8] = 0;
				regs[9]++;
				if ((regs[9] & 15) == 10) regs[9] = (regs[9] & 240) + 16;
				if (regs[9] == 96)
				{
					regs[9] = 0;
					regs[10]++;
				}
			}
		}
	*/
	}

int CIA6526::Read(int addr)
{
	switch(addr) {
	case 0:
		if (no == 1)
		{
			return 0;// keyboard.joystickport;
		}
		else
		{
			return regs[0] | (~regs[2]);
		}
		break;
	case 1:
		if (no == 1)
		{			
			/*regs[1] = 0x0;
			for(int i=0; i<8; i++)
			{
				if ((regs[0] & (1<<i))==0)
				regs[1] |= keyboard.keyboardline[i];
			}
			regs[1] = ~regs[1];*/
			return 0;
		} 
		else
		{
			return regs[1] | (~regs[3]);
		}
		break;
	case 13:
		int temp = regs[13];
		regs[13] = 0x0;
		ClearInterrupt();
		return temp;
	}
	return regs[addr];
}

void CIA6526::Write(int addr, int x)
{
	//DebugMessage("cia write " + addr + " "  + x);

	switch(addr) {
	case 4:
		timeralatch = (timeralatch & 0xFF00) | x;
		break;
	case 5:
		timeralatch = (timeralatch & 0xFF) | (x<<8);
		break;
	case 6:
		timerblatch = (timerblatch & 0xFF00) | x;
		break;
	case 7:
		timerblatch = (timerblatch & 0xFF) | (x<<8);
		break;
	case 13:
		//set iflags
		if (x & 0x80)
			iflags |= x & ~0x80;
		else
			iflags &= ~x;
		break;
	case 14:
		if ((x&1) & !(regs[14]&1))
		{
			regs[4] = timeralatch & 0xFF;
			regs[5] = (timeralatch >> 8) & 0xFF;
		}

		break;
	case 15:
		if ((x&1) & !(regs[15]&1))
		{
			regs[6] = timerblatch & 0xFF;
			regs[7] = (timerblatch >> 8) & 0xFF;
		}
		break;
	}
	//DebugMessage("" + no + ": " + addr + " "  + x);
	regs[addr] = x;
}