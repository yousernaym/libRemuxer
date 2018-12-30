// -------------------------------------------------
// -------------------- VIC II ---------------------
// -------------------------------------------------

#include "vic.h"

VICII::VICII(unsigned char *_mem, MOS6502 &_cpu) : cpu(_cpu)
{
	mem = _mem;
	Reset();
}

void VICII::Reset()
{
	cycles = 0;
	for(int i=0; i<64; i++)
		regs[i] = 0x0;
	for(int i=0; i<1024; i++)
		colorram[i] = 0x0;
	rasterline = 0x0;

	bank = 0;
	ecm = false;
	mcm = false;
	bmm = false;
	r = 0x0;
	h0 = 0x0;
	h1 = 0x0;
	h2 = 0x0;
	h3 = 0x0;
	scrollx = 0x0;
	scrolly = 0x0;
	zeichenram = 0x0;
	charram = 0x0;
	charen = false;
}


int VICII::Read(int addr)
{
	switch(addr) {
	case 0x12:
		return rasterline & 0xFF;
	case 0x11:
		return (regs[addr] & 127) | ((rasterline >> 1) & 128);
	case 0x19:
		return (regs[addr] | 0x70);
	case 0x1A:
		return (regs[addr] | 0xF0);
	case 0x16:
		return (regs[addr] | 0xC0);
	case 0x20: case 0x21: case 0x22: case 0x23:
	case 0x24: case 0x25: case 0x26: case 0x27:
	case 0x28: case 0x29: case 0x2A: case 0x2B:
	case 0x2C: case 0x2D: case 0x2E:
		return regs[addr] | 0xF;
	case 0x2F:
	case 0x30: case 0x31: case 0x32: case 0x33:
	case 0x34: case 0x35: case 0x36: case 0x37:
	case 0x38: case 0x39: case 0x3A: case 0x3B:
	case 0x3C: case 0x3D: case 0x3E:
		return 0xFF;
	}
	return regs[addr];
}

void VICII::SetBank(int _bank)
{
	bank = _bank;
	Write(0x18, regs[0x18]);
}


void VICII::Write(int addr, int x)
{   
	switch(addr)
	{
	case 0x11:
		ecm = (x & (1<<6)) != 0;
		bmm = (x & (1<<5)) != 0;
		scrolly = x & 7;
		break;
	case 0x16:
		mcm = (x & (1<<4)) != 0;
		scrollx = x & 7;
		break;

	case 0x18:
		zeichenram = (x&0xF0)*64 + (bank*16384);
		charram = (x&0xE)*1024 + (bank*16384);
		break;
	case 0x19:
		x = 0x0; // interrupt remove
		break;

	case 0x20:
		r = x & 15;
		break;

	case 0x21:
		h0 = x & 15;
		break;

	case 0x22:
		h1 = x & 15;
		break;

	case 0x23:
		h2 = x & 15;
		break;

	case 0x24:
		h3 = x & 15;
		break;
	}
	regs[addr] = x;
}


void VICII::CalcSteps(int count)
{
	cycles += count;
	if (cycles <= 62) return;

	cycles -= 63;
	rasterline++;

	if (rasterline >= 312)
	{
		rasterline = 0;
	}

	if (regs[0x1a] & 1)
	{
		int lineirq = regs[0x12] + (((regs[0x11] >> 7) & 1) << 8);
		if (rasterline == lineirq)
		{
			//if (regs[0x19] == 0)
			{
				regs[0x19] = 128 | 1;
				cpu.MaskableInterrupt();
			}
		}
	}
}
