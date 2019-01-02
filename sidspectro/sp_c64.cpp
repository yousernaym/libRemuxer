// -------------------------------------------------
// ---------------------- C64 ----------------------
// -------------------------------------------------#

#include "cia.h"
#include "c64.h"
#include "tables.h"

using namespace std::placeholders;
//var keyboard = new KeyboardInput();
//C64 c64;

C64::C64() : cpu(*this) , cia1(cpu, 2, true) , cia2(cpu, 2, false) , vic(mem, cpu) , sid(cyclespersecond, 0)
{
	//mem.resize(0x10000);
	/*for (int i = 0; i < screen.width*screen.height; i++)
	{
		imagedata.data[(i << 2) + 0] = 0x00;
		imagedata.data[(i << 2) + 1] = 0x00;
		imagedata.data[(i << 2) + 2] = 0x00;
		imagedata.data[(i << 2) + 3] = 0xFF;
	}*/
}

void C64::Reset()
{
	count = 0;
	basic_in = true;
	kernel_in = true;
	char_in = false;
	io_in = true;
	ddr = 0x1F; // mem[0] // every important part is writeable
	pr = 0x0; // mem[1]
	cpu.Reset();
	sid.Reset();
	cia1.Reset();
	cia2.Reset();
	vic.Reset();
	for(int i=0; i<0x10000; i++)
		mem[i] = 0x0;
}


int C64::Read(unsigned addr)
{
	addr &= 0xFFFF;
	switch(addr>>12) {
	case 0x0:
		if (addr <= 1)
		{
			if (addr == 0)
				return ddr|0;
			if (addr == 1)
				return ((ddr & pr) | (~ddr & 0x17))|0;
		}
		break;
		
	case 0xA:
	case 0xB:
		if (basic_in)
			return basicrom[addr&0x1FFF]|0;
		break;
		
	case 0xD:
		if (char_in)
			return charrom[addr&0x0FFF]|0;
		if (io_in) {
			if ((addr >= 0xD000) && (addr <= 54271)) return vic.Read(addr&0x3F)|0; else
			if ((addr >= 0xD400) && (addr <= 55295)) return sid.Read(addr&31); else
			if ((addr >= 0xD800) && (addr <= 0xDBFF)) return vic.colorram[addr&0x3FF]|0; else
			if ((addr >= 0xDC00) && (addr <= 56575)) return cia1.Read(addr&0xF)|0; else
			if ((addr >= 0xDD00) && (addr <= 0xDFFF)) return cia2.Read(addr&0xF)|0;
			//DebugMessage("IO: " + addr);
		}
		break;

	case 0xE:
	case 0xF:
		if (kernel_in)
			return kernelrom[addr&0x1FFF]|0;
		break;
	}

	return mem[addr];
}

void C64::Write(unsigned addr, int x)
{
	addr &= 0xFFFF;
	x = x & 0xFF;

	switch(addr>>12) {
	case 0x0:
		if (addr <= 1)
		{
			switch(addr)
			{
			case 0:
				ddr = x;
				break;
			case 1:
				pr = x;
				int port = (~ddr) | pr;
				basic_in = (port & 3) == 3;
				kernel_in = (port & 2) != 0;
				char_in = ((port&3) != 0) && (!(port & 4));
				io_in = ((port&3)!= 0) && ((port&4)!=0);
				break;
			}
			//DebugMessage("access " + basic_in + " " + kernel_in + " " + char_in + " " + io_in);
			return;
		}
		break;

	case 0xD:
		if (io_in)
		{
			if ((addr >= 0xD000) && (addr <= 54271)) vic.Write(addr&0x3F, x); else
			if ((addr >= 0xD800) && (addr <= 0xDBFF)) vic.colorram[addr&0x3FF] = x;
			if ((addr >= 0xDD00) && (addr <= 57343)) 
			{
				cia2.Write(addr&0xF, x);
				vic.SetBank((~cia2.regs[0]) & 3);
			} else
			if ((addr >= 0xDC00) && (addr <= 56575)) cia1.Write(addr&0xF, x); else
			if ((addr >= 54272) && (addr <= 55295)) 
			{
				//sid.Update(count);
				sid.Write(addr&31, x);
			}
			//DebugMessage("IO Region " + addr);
			return;	
		}
		break;
	}
	mem[addr] = x;
}

void C64::MainLoop()
{
	int diff = 0;
	int steps = 4000;
	while(steps--)
	{
		diff = cpu.Step(5);
		count += diff;
		cia2.CalcSteps(diff);
		cia1.CalcSteps(diff);
		vic.CalcSteps(diff);
	}
	//sid.Update(count);

	//double currenttime = sid.soundbuffer.GetTime();
	//double elapsedtime = currenttime - starttime;
	//double wait = count/1000. - elapsedtime*1000.;
	//if (wait < 0)
	//{
	//	if (wait < -1000) // reset, we are too far off
	//	{
	//		sid.starttime = elapsedtime;
	//		count = (int)(elapsedtime*cyclespersecond);
	//	}
	//	wait = 0;
	//}
	/*if (typeof sidfile != 'undefined')
	{
		LoadSIDFile(sidfile);        
		sidfile = undefined;
	}*/
	//window.setTimeout(MainLoop.bind(this), wait);
}


unsigned C64::GenCopy8(unsigned dest, unsigned src, unsigned memoffset)
{
	Write(memoffset++, 0xAD); // LDA ABS
	Write(memoffset++, src&0xFF);
	Write(memoffset++, src>>8);

	Write(memoffset++, 0x8D); // STA ABS
	Write(memoffset++, dest&0xFF);
	Write(memoffset++, dest>>8);

	return memoffset;
}

unsigned C64::GenCopy16(unsigned dest, unsigned src, unsigned memoffset)
{
	Write(memoffset++, 0xAD); // LDA ABS
	Write(memoffset++, src&0xFF);
	Write(memoffset++, src>>8);

	Write(memoffset++, 0x8D); // STA ABS
	Write(memoffset++, dest&0xFF);
	Write(memoffset++, dest>>8);

	Write(memoffset++, 0xAD); // LDA ABS
	Write(memoffset++, (src+1)&0xFF);
	Write(memoffset++, (src+1)>>8);

	Write(memoffset++, 0x8D); // STA ABS
	Write(memoffset++, (dest+1)&0xFF);
	Write(memoffset++, (dest+1)>>8);

	return memoffset;
}

int C64::GenStore(int addr, int memoffset)
{
	Write(memoffset++, 0x8D); // STA ABS
	Write(memoffset++, addr&0xFF);
	Write(memoffset++, addr>>8);
	return memoffset;
}

int C64::GenLoad(int addr, int memoffset)
{
	Write(memoffset++, 0xAD); // LDA ABS
	Write(memoffset++, addr&0xFF);
	Write(memoffset++, addr>>8);
	return memoffset;
}



int C64::GenJSR(int addr, int memoffset)
{
	Write(memoffset++, 0x20); // JSR
	Write(memoffset++, addr&0xFF);
	Write(memoffset++, addr>>8);
	return memoffset;
}


int C64::GenLDAi(int x, int memoffset)
{
	Write(memoffset++, 0xA9); // lda IMM
	Write(memoffset++, x);
	return memoffset;
}


void C64::LoadSIDFile(const SIDFile &sidfile)
{
	if (sidfile.data.size() == 0) return;
	//sidfile.Print();

// ---------------------------------------------
	Reset();
// ---------------------------------------------

	if (sidfile.magicid == "RSID")
	{
/*
	var steps = 2000000;
	while(steps--)
	{
		var diff = cpu.Step(3);
		count += diff;
		cia2.CalcSteps(diff);
		cia1.CalcSteps(diff);
		vic.CalcSteps(diff);
	}
*/
	}
	for(int i=sidfile.offset; i<(int)sidfile.data.size(); i++)
	{
		mem[sidfile.loadaddr - sidfile.offset + i] = sidfile.data[i];
	}

	int memoffset = 1024;	
	if (sidfile.startpage != 0)
		memoffset = sidfile.startpage<<8;
	else
	if ((sidfile.loadaddr >> 8) < (memoffset >> 8))
	{
		memoffset = ((sidfile.endaddr)&0xFF00) + 0x100;
	}
	cpu.PC = memoffset;

// ---------------------------------------------

	if (sidfile.magicid == "RSID")
	{
/*
	var steps = 2000000;
	while(steps--)
	{
		var diff = cpu.Step(3);
		count += diff;
		cia2.CalcSteps(diff);
		cia1.CalcSteps(diff);
		vic.CalcSteps(diff);
	}
*/

	memoffset = GenJSR(0xFF84, memoffset); // intialize IO devices

	// set VIC raster lines
	memoffset = GenLDAi(0x9B, memoffset);
	memoffset = GenStore(0xD011, memoffset);

	memoffset = GenLDAi(0x37, memoffset);
	memoffset = GenStore(0xD012, memoffset);


//	vic.Write(0x12, 0x37);
//	vic.Write(0x11, 0x9B);
	memoffset = GenLDAi(sidfile.startsong-1, memoffset);
	memoffset = GenJSR(sidfile.initaddr, memoffset);
	Write(memoffset++, 0xB7); // EMU: should emulate endless loop


	cpu.PC = sidfile.initaddr;
	Write(0, 0x2F);
	Write(1, 0x37);
	return;
	}

// ---------------------------------------------


	// 0 = 50 Hz PAL, 60 Hz NTSC
	// 1 = CIA 1 timer interrupt default 60 Hz
	int speed = 0; 
	if (sidfile.startsong > 32)
	{
		speed = (sidfile.speed >> 31) & 1;
	} else
	{
		speed = (sidfile.speed >> (sidfile.startsong-1)) & 1;
	}
	
	int store = memoffset+240;
	Write(memoffset++, 0x78); // SEI

	// set VICII raster to line 0 for PSIDs
/*
	memoffset = GenLDAi(0x1B, memoffset);
	memoffset = GenStore(0xD011, memoffset);
	memoffset = GenLDAi(0x00, memoffset);
	memoffset = GenStore(0xD012, memoffset);
*/

	// execute init
	memoffset = GenLDAi(sidfile.startsong-1, memoffset);
	memoffset = GenJSR(sidfile.initaddr, memoffset);


	// activate loop
	if (sidfile.playaddr != 0)
	{
		memoffset = GenCopy16(store, 0xFFFE, memoffset);

		// activate cia timer and interrupt
		memoffset = GenLDAi(0x1, memoffset);
		memoffset = GenStore(56320+14, memoffset);
	}
	memoffset = GenCopy16(0xFFFE, store+2, memoffset);

	Write(memoffset++, 0x58); // CLI
	Write(memoffset++, 0xB7); // EMU: should emulate endless loop

	// endless loop
	Write(memoffset++, 0x4C); // jmp
	Write(memoffset++, (memoffset-2) & 0xFF); // low
	Write(memoffset++, (memoffset-3)>>8); // high


// ------ Interrupt Handler ------

	mem[store+2] = memoffset&0xFF;
	mem[store+3] = memoffset>>8;

	memoffset = GenCopy16(0xFFFE, store, memoffset);

	memoffset = GenLDAi(0x0, memoffset);

	// execute by using jsr
	memoffset = GenJSR(sidfile.playaddr, memoffset);
	memoffset = GenCopy16(0xFFFE, store+2, memoffset);

	// reset interrupt
	Write(memoffset++, 0xCE); // dec
	Write(memoffset++, 0x19);
	Write(memoffset++, 0xd0);

	// reset interrupt
	memoffset =  GenLoad(0xDC0D, memoffset);
	Write(memoffset++, 0x40); // rti

/*
	cia1.timeralatch = 0x411A; // 60Hz // 42C7???
	cia1.regs[4] = 0x1A; // low byte counter
	cia1.regs[5] = 0x41; // high byte counter
*/

	cia1.timeralatch = 0x4E20; // 50Hz 
	cia1.regs[4] = 0x20; // low byte counter
	cia1.regs[5] = 0x4E; // high byte counter

	cia1.iflags = 1; // interrupt enabled

	if (sidfile.magicid == "PSID")
	{
		basic_in = false;
		kernel_in = false;
		char_in = false;
		io_in = true;
	} else
	{
		Write(0, 0x2F);
		Write(1, 0x37);
		//cia1.regs[14] = 1;
		//vic.regs[0x1a] = 0;
	}
	/*
	for(var i=0; i<100; i++)
	{
	cpu.Disassemble();
	cpu.Step(1);
	}
	*/

}

float C64::getTime()
{
	return count / cyclespersecond;
}