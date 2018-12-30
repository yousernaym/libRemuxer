// -------------------------------------------------
// ---------------------- CPU ----------------------
// -------------------------------------------------
#include "tables.h"
#include "sp_cpu.h"
#include "misc.h"
#include "c64.h"
using namespace std::placeholders;

MOS6502::MOS6502(C64 &_c64) : c64(_c64) //std::function<int(int)>_read, std::function<void(int, int)> _Write, unsigned char *_mem)
{
	mem = c64.mem;
	//Write = std::bind(&C64::Write, c64, _1, _2);
	//Read = std::bind(&C64::Read, c64, _1);
	Reset();

	for(int i=0; i<256; i++)
	{
		ops[i] = opcodes[i].op | (opcodes[i].amode<<8) | (opcodes[i].count<<16);
		if (opcodes[i].count == 0) 
			ops[i] = opcodes[i].op | (opcodes[i].amode<<8) | (1<<16);
		if (
				(opcodes[i].op == OP_STA) ||
				(opcodes[i].op == OP_STX) ||
				(opcodes[i].op == OP_STY)) 
			ops[i] |= 0x1000000;
		if (
				(opcodes[i].amode != AMODE_ACC) &&
				(opcodes[i].amode != AMODE_IMM) &&
				(opcodes[i].amode != AMODE_IMP) &&
				(opcodes[i].amode != AMODE_REL) &&
				(opcodes[i].amode != AMODE_IND)
				)
		{
			if (
					(opcodes[i].op == OP_STA) ||
					(opcodes[i].op == OP_INC) ||
					(opcodes[i].op == OP_DEC) ||
					(opcodes[i].op == OP_ASL) ||
					(opcodes[i].op == OP_LSR) ||
					(opcodes[i].op == OP_STX) ||
					(opcodes[i].op == OP_STY) ||
					(opcodes[i].op == OP_ROL) ||
					(opcodes[i].op == OP_ROR)
					) ops[i] |= 0x2000000;
		}

	}
}

void MOS6502::Reset()
{
	AC = 0x0;
	XR = 0x0;
	YR = 0x0;
	PC = c64.Read(0xFFFC) + (c64.Read(0xFFFD)<<8);
	SP = 0xFF;
	PutStatus(32);
}

void MOS6502::SetPC(int offset)
{
	PC = offset&0xFFFF;
}

int MOS6502::GetStatus()
{
	int x = 0x0;
	if (Carry_Flag) x |= 1;
	if (Zero_Flag)  x |= 2;
	if (IRQ_Disable_Flag)  x |= 4;
	if (Decimal_Flag) x |= 8;
	if (Brk_Flag) x |= 16;
	x |= 32; // unused flag
	if (Overflow_Flag) x |= 64;
	if (Negative_Flag) x |= 128;
	return x;
}

void MOS6502::PutStatus(int x)
{
	Unused_Flag = true;
	Carry_Flag = (x&1)?true:false;
	Zero_Flag = (x&2)?true:false;
	IRQ_Disable_Flag = (x&4)?true:false;
	Decimal_Flag = (x&8)?true:false;
	Brk_Flag = (x&16)?true:false;
	Overflow_Flag = (x&64)?true:false;
	Negative_Flag = (x&128);
}

//void MOS6502::Disassemble() 
//{
//	int address_mode = 0x0, x = 0x0;
//	int address = 0x0;
//	s = "";
//
//	/*
//	printf("AC: %u\n",AC);
//
//	printf("XR: %u YR: %u\n",XR, YR);
//	printf("PC: %u SP: %u\n",PC, SP);
//
//	printf("Carry Flag: %u\n",Carry_Flag);
//	printf("Zero Flag: %u\n",Zero_Flag);
//	printf("Overflow Flag: %u\n",Overflow_Flag);
//	printf("Negative Flag: %u\n",Negative_Flag);
//*/
//	address_mode = opcodes[Read(PC)].amode;
//	int s = opcodes[Read(PC)].opsstr + " ";
//	
//	switch (address_mode)
//	{
//	case AMODE_IMP:            
//		break;
//
//	case AMODE_ACC:
//		s += "A";
//		break;
//
//	case AMODE_ABS:
//		address = (Read(PC+2) << 8) | Read(PC+1);
//		s += address;
//		break;
//
//	case AMODE_IMM:
//		x = Read(PC+1);
//		s += "#" + x;
//		break;
//
//	case AMODE_ZP:
//		address = Read(PC+1);
//		s += address;
//		break;
//
//	case AMODE_ABSX:
//		address = (Read(PC+2) << 8) + Read(PC+1);
//		s += address + ",x";
//		break;
//
//	case AMODE_ABSY:
//		address = (Read(PC+2) << 8) + Read(PC+1);
//		s += address + ",y";
//		break;
//
//	case AMODE_ZPX:
//		address = (Read(PC+1)+XR) & 0xFF;
//		s += address + ",x";
//		break;
//
//	case AMODE_ZPY:
//		address = (Read(PC+1)+YR) & 0xFF;
//		s += address + ",y";
//		break;
//
//	case AMODE_REL:
//		x = Read(PC+1);
//		s += (x<<24)>>24;
//		break;
//
//	case AMODE_IND:
//		address = (Read(PC+2) << 8) | Read(PC+1);
//		s += "(" + address + ")";      
//		break;
//
//	case AMODE_INDX:
//		address = (Read(PC+1) + XR) & 0xFF;
//		s += "(" + address + ",x)";      
//		break;
//
//	case AMODE_INDY:
//		address = Read(PC+1);
//		s += "(" + address + "),y";
//		break;
//	}
//	DebugMessage(PC + ": " + s);
//}

void MOS6502::MaskableInterrupt()
{
	//DebugMessage("interrupt");
	if (IRQ_Disable_Flag) return;
	c64.Write(256|SP,  ((PC) >> 8) & 0xFF);
	SP = (SP-1)&0xFF;
	c64.Write(256|SP,  (PC) & 0xFF);
	SP = (SP-1)&0xFF;
	Brk_Flag = false;
	c64.Write(256|SP,  GetStatus());
	SP = (SP-1)&0xFF;
	IRQ_Disable_Flag = true;
	PC = c64.Read(0xFFFE) | (c64.Read(0xFFFF) << 8);
}

void MOS6502::NonMaskableInterrupt()
{    
	c64.Write(256|SP,  ((PC) >> 8) & 0xFF);
	SP = (SP-1)&0xFF;
	c64.Write(256|SP,  (PC) & 0xFF);
	SP = (SP-1)&0xFF;
	Brk_Flag = false;
	c64.Write(256|SP,  GetStatus());
	SP = (SP-1)&0xFF;
	IRQ_Disable_Flag = true;
	PC = c64.Read(0xFFFA) | (c64.Read(0xFFFB) << 8);
}

int MOS6502::Step(int steps) 
{
	int x = 0x0;
	int address = 0x0;
	int stack = 0x0; // used by PLP and RTI
	int help = 0x0, help2 = 0x0; //used in some operations like adc and cmp
	int count = 0;
	int command = 0;
	while(steps--)
	{
		command = c64.Read(PC);
		//command = PC<0xA000?mem[PC]:c64.Read(PC);
		int op = ops[command];
		int address_mode = (op>>8) & 0xFF;
		int code = op&0xFF;
		count += (op>>16)&0xFF;
		
		switch (address_mode)
		{
		case AMODE_IMP:
			PC += 1;
			break;

		case AMODE_ACC:
			x = AC;
			PC += 1;
			break;

		case AMODE_ABS:
			address = (c64.Read(PC+2) << 8) | c64.Read(PC+1);
			if (!(op & 0x1000000))
				x = c64.Read(address);
			PC += 3;
			break;

		case AMODE_IMM:
			if (!(op & 0x1000000))
				x = c64.Read(PC+1);
			PC += 2;
			break;

		case AMODE_ZP:
			address = c64.Read(PC+1);
			if (!(op & 0x1000000))
				x = c64.Read(address);
			PC += 2;
			break;

		case AMODE_ABSX:
			address = ((c64.Read(PC+2) << 8) | c64.Read(PC+1)) + XR;
			if (!(op & 0x1000000))
				x = c64.Read(address);
			PC += 3;
			break;

		case AMODE_ABSY:
			address = ((c64.Read(PC+2) << 8) | c64.Read(PC+1)) + YR;
			if (!(op & 0x1000000)) 
				x = c64.Read(address);
			PC += 3;
			break;

		case AMODE_ZPX:
			address = (c64.Read(PC+1)+XR) & 0xFF;
			if (!(op & 0x1000000))
				x = c64.Read(address);
			PC += 2;
			break;

		case AMODE_ZPY:
			address = (c64.Read(PC+1)+YR) & 0xFF;
			if (!(op & 0x1000000))
				x = c64.Read(address);
			PC += 2;
			break;

		case AMODE_REL:
			x = c64.Read(PC+1);
			PC += 2;
			break;

		case AMODE_IND:
			address = (c64.Read(PC+2) << 8) | c64.Read(PC+1);
			address = (( c64.Read( (address+1) & 0xFF | address&0xFF00 ) ) << 8) | c64.Read(address);
			address &= 0xFFFF;
			PC += 3;
			break;

		case AMODE_INDX:
			address = (c64.Read(PC+1) + XR) & 0xFF;
			address = (c64.Read(address+1) << 8) | c64.Read(address);
			if (!(op & 0x1000000)) 
				x = c64.Read(address);       
			PC += 2;
			break;

		case AMODE_INDY:
			address = c64.Read(PC+1);
			address = (c64.Read(address+1) << 8) + (c64.Read(address)+YR);
			if (!(op & 0x1000000))
				x = c64.Read(address);
			PC += 2;
			break;
		}

		switch  (code)
		{
		case 0:
			//DebugMessage("Error: unknwon opcode " + command + " at " + PC);
			//throw "Unknown opcode";
			break;
		case OP_CMP:
			help = AC-x;
			Zero_Flag = help==0;
			Negative_Flag = (help & 128);
			Carry_Flag = help >= 0;
			break;

		case OP_CPX:
			help = XR-x;
			Zero_Flag =  help==0;
			Negative_Flag = (help & 128);
			Carry_Flag = help >= 0;
			break;

		case OP_CPY:
			help = YR-x;
			Zero_Flag =  help==0;
			Negative_Flag = (help & 128);
			Carry_Flag = help >= 0;
			break;

		case OP_LDA:
			AC = x;
			Zero_Flag =  x==0;
			Negative_Flag = (x & 128);
			break;
			
		case OP_STA:
			x = AC;
			break;

		case OP_LDX:
			XR = x;
			Zero_Flag =  x==0;
			Negative_Flag = (x & 128);
			break;

		case OP_LAX:
			//DebugMessage((string)"Error: unknwon opcode " + command + " at " + PC);

			AC = x;
			XR = x;
			Zero_Flag =  x==0;
			Negative_Flag = (x & 128);
			break;


		case OP_STX:
			x = XR;
			break;

		case OP_LDY:
			YR = x;
			Zero_Flag =  x==0;
			Negative_Flag = (x & 128) ;
			break;

		case OP_STY:
			x = YR;
			break;

		case OP_AND:
			AC = AC & x;
			Zero_Flag =  AC==0;
			Negative_Flag = (AC & 128) ;
			break;

		case OP_EOR:
			AC = AC ^ x;
			Zero_Flag =  AC==0;
			Negative_Flag = (AC & 128) ;
			break;

		case OP_ORA:
			AC = AC | x;
			Zero_Flag =  AC==0;
			Negative_Flag = (AC & 128) ;
			break;

		case OP_ADC:
			if (Decimal_Flag)
			{
				help = (AC & 0x0f) + (x & 0x0f);		// Calculate lower nybble
				if (Carry_Flag) help++;
				if (help > 9)
					help = help + 6;									// BCD fixup for lower nybble

				help2 = (AC >> 4) + (x >> 4);							// Calculate upper nybble
				if (help > 0x0f) 
					help2 = help2 + 1;
				int help3 = AC + x;
				if (Carry_Flag)
					help3 = help3 + 1;

				Zero_Flag = (help3 & 255) == 0;

				Negative_Flag = (help2 & 8); // Only highest bit used
				Overflow_Flag = (((help2 << 4) ^ AC) & 0x80) && !(((AC ^ x) & 0x80) != 0);

				if (help2 > 9) help2 = help2 + 6;									// BCD fixup for upper nybble
				Carry_Flag = help2 > 0x0f;										// Set carry flag
				AC = (help2 << 4) | (help & 0x0f);							// Compose result
				

			} 
			else 
			{
				help = AC + x;
				if (Carry_Flag) help++;
				Overflow_Flag = (((~(AC ^ x) & 0x80) ) != 0) && (((AC ^ help) & 0x80) != 0 );
				AC = help & 0xFF;
				Zero_Flag = AC==0;
				Negative_Flag = (AC & 128) ;
				Carry_Flag = help > 0xFF;
			}
			break;

		case OP_SBC:
			if (Decimal_Flag)
			{
				int low = 0x0, high = 0x0;
				low = (AC & 0x0f) - (x & 0x0f);		// Calculate lower nybble
				if (!Carry_Flag)
					low--;

				help2 = (AC >> 4) - (x >> 4);							// Calculate upper nybble
				if ((low & 0x10) != 0)
				{
					low = low - 6;											// BCD fixup for lower nybble
					help2 = help2 - 1;
				}
				if ((help2 & 0x10) != 0)
					help2 = help2 - 6;									// BCD fixup for upper nybble
				int help3 = AC - x;
				if (!Carry_Flag) 
					help3 = help3 - 1;

				Carry_Flag = help3 < 0x100;									// Set flags
				Overflow_Flag = (((AC ^ help3) & 0x80) != 0) & (((AC ^ x) & 0x80) != 0);
				Zero_Flag = (help3 & 255) == 0;
				Negative_Flag = (help3 & 128);
				AC = (help2 << 4) | (low & 0x0f);							// Compose result
			}
			else 
			{
				help = AC - x;
				if (!Carry_Flag)
					help--;
				Overflow_Flag = (((AC ^ help) & 0x80) ) && (((AC ^ x) & 0x80) );
				AC = help & 0xFF;
				Carry_Flag = help >= 0;
				Zero_Flag =  AC==0;
				Negative_Flag = (AC & 128) ;
			}
			break;

		case OP_BIT:
			Negative_Flag = (x & 0x80);
			Overflow_Flag = (x & 0x40);
			Zero_Flag = (x & AC ) == 0;
			break;

		case OP_INC:
			x = (x+1)&0xFF;
			Zero_Flag =  x==0;
			Negative_Flag = (x & 128);
			break;

		case OP_INX:
			XR = (XR+1)&0xFF;
			Zero_Flag =  XR==0;
			Negative_Flag = (XR & 128);
			break;

		case OP_INY:
			YR = (YR+1)&0xFF;
			Zero_Flag =  YR==0;
			Negative_Flag = (YR & 128) ;
			break;

		case OP_DEC:
			x = (x-1)&0xFF;
			Zero_Flag =  x==0;
			Negative_Flag = (x & 128);
			break;

		case OP_DEX:
			XR = (XR-1)&0xFF;
			Zero_Flag =  XR==0;
			Negative_Flag = (XR & 128);
			break;

		case OP_DEY:
			YR = (YR-1)&0xFF;
			Zero_Flag =  YR==0;
			Negative_Flag = (YR & 128);
			break;

		case OP_ASL:
			Carry_Flag = (x & 128) != 0;
			x = (x << 1)&0xFF;
			Zero_Flag = x==0;
			Negative_Flag = (x & 128);
			break;

		case OP_LSR:
			Carry_Flag = (x & 1) != 0;
			x = (x >> 1)&0xFF;
			Zero_Flag = x==0;
			Negative_Flag = 0;
			break;

		case OP_ROL:
			help = x << 1;
			if (Carry_Flag)
				help |= 1;
			Carry_Flag = help > 255;
			x = help & 0xFF;
			Zero_Flag =  x==0;
			Negative_Flag = (x & 128);
			break;

		case OP_RLA:
			help = x << 1;
			if (Carry_Flag) help |= 1;
			Carry_Flag = help > 255;
			x = help & 0xFF;
			AC &= x;			
			Zero_Flag =  x==0;
			Negative_Flag = (x & 128);
			break;

		case OP_ROR:
			help = x & 1;        
			x = (x >> 1)&0xFF;
			if (Carry_Flag) x = x | 128;
			Carry_Flag = help != 0;
			Zero_Flag = x==0;
			Negative_Flag = (x & 128);
			break;

		case OP_PHA:
			c64.Write(256|SP, AC);
			SP = (SP-1)&0xFF;
			break;

		case OP_PLA:
			SP = (SP+1)&0xFF;
			AC = c64.Read(256|SP);
			Zero_Flag = AC==0;
			Negative_Flag = (AC & 128);
			break;

		case OP_PHP:
			c64.Write(256|SP, GetStatus() | 16);
			SP = (SP-1)&0xFF;
			break;

		case OP_PLP:
			SP = (SP+1)&0xFF;
			stack = c64.Read(256|SP);
			PutStatus(stack);
			break;

		case OP_JMP:
			PC = address;
			break;

		case OP_JSR:
			c64.Write( 256|SP, ((PC-1) >> 8) & 0xFF);
			SP = (SP-1)&0xFF;
			c64.Write( 256|SP, (PC-1) & 0xFF);
			SP = (SP-1)&0xFF;
			PC = address;
			break;

		case OP_RTS:
			SP = (SP+1)&0xFF;
			PC = c64.Read(256|SP);
			SP = (SP+1)&0xFF;
			PC |= (c64.Read(256|SP) << 8);
			PC++;
			break;

		case OP_RTI:
			SP = (SP+1)&0xFF;
			stack = c64.Read(256|SP);
			PutStatus(stack);
			SP = (SP+1)&0xFF;
			PC = c64.Read(256|SP);
			SP = (SP+1)&0xFF;
			PC = PC | (c64.Read(256|SP) << 8);
			break;

		case OP_BRK:
			c64.Write(256|SP,  ((PC+1) >> 8) & 0xFF);
			SP = (SP-1)&0xFF;
			c64.Write(256|SP,  (PC+1) & 0xFF);
			SP = (SP-1)&0xFF;
			Brk_Flag = true;
			c64.Write(256|SP,  GetStatus());
			SP = (SP-1)&0xFF;
			IRQ_Disable_Flag = true;
			PC = c64.Read(0xFFFE) | (c64.Read(0xFFFF) << 8);
			break;

		case OP_BNE:
			if (!Zero_Flag) {PC += ((x<<24)>>24); count++;}
			break;

		case OP_BCC:
			if (!Carry_Flag) {PC += ((x<<24)>>24); count++;}
			count++; // ???
			break;

		case OP_BEQ:
			if (Zero_Flag) {PC += ((x<<24)>>24); count++;}
			break;

		case OP_BMI:
			if (Negative_Flag) {PC += ((x<<24)>>24); count++;}
			break;

		case OP_BVC:
			if (!Overflow_Flag) {PC += ((x<<24)>>24); count++;}
			break;

		case OP_BCS:
			if (Carry_Flag) {PC += ((x<<24)>>24); count++;}
			break;

		case OP_BPL:
			if (!Negative_Flag) {PC += ((x<<24)>>24); count++;}
			break;

		case OP_BVS:
			if (Overflow_Flag) {PC += ((x<<24)>>24); count++;}
			break;

		case OP_TAX:
			XR = AC;
			Zero_Flag =  AC==0;
			Negative_Flag = (AC & 128) ;
			break;

		case OP_TAY:
			YR = AC;
			Zero_Flag =  AC==0;
			Negative_Flag = (AC & 128) ;
			break;

		case OP_TXA:
			AC = XR;
			Zero_Flag =  AC==0;
			Negative_Flag = (AC & 128) ;
			break;

		case OP_TYA:
			AC = YR;
			Zero_Flag =  AC==0;
			Negative_Flag = (AC & 128) ;
			break;

		case OP_TXS:
			SP = XR;
			break;

		case OP_TSX:
			XR = SP;
			Zero_Flag =  XR==0;
			Negative_Flag = (XR & 128) ;
			break;

		case OP_CLC:
			Carry_Flag = false;
			break;

		case OP_CLD:
			Decimal_Flag = false;
			break;

		case OP_CLI:
			IRQ_Disable_Flag = false;
			break;

		case OP_CLV:
			Overflow_Flag = false;
			break;

		case OP_SEC:
			Carry_Flag = true;
			break;

		case OP_SED:
			Decimal_Flag = true;
			break;

		case OP_SEI:
			IRQ_Disable_Flag = true;
			break;

		case OP_EMU:
			PC--;
			return count + (steps)*4; // fast endless loop
			break;
		}
		if (address_mode == AMODE_ACC)
			AC = x&0xFF; 
		else if (op & 0x2000000) 
			c64.Write(address, x);

		/*
	switch (address_mode)
	{
		case AMODE_ACC:
			AC = x&0xFF;
			break;
		case AMODE_IMM:
		case AMODE_IMP:
		case AMODE_REL:
		case AMODE_IND:
			break;

		default:
			switch  (code)
			{
				case  OP_STA:
				case  OP_INC:
				case  OP_DEC:
				case  OP_ASL:
				case  OP_LSR:
				case  OP_STX:
				case  OP_STY:
				case  OP_ROL:
				case  OP_ROR:
					c64.Write(address, x);
					break;
			}
			break;
		}
*/
		//if (x < 0) DebugMessage("Error x");
		//if (x > 0xFF) DebugMessage("Error x");
		/*
		if (PC < 0) DebugMessage("Error PC");
		if (AC < 0) DebugMessage("Error AC");
		if (XR < 0) DebugMessage("Error XR");
		if (YR < 0) DebugMessage("Error YR");
		if (SP < 0) DebugMessage("Error SP");
		if (PC > 0xFFFF) DebugMessage("Error PC");
		if (AC > 0xFF) DebugMessage("Error AC");
		if (XR > 0xFF) DebugMessage("Error XR");
		if (YR > 0xFF) DebugMessage("Error YR");
		if (SP > 0xFF) DebugMessage("Error SP");
		*/
		/*
		PC &= 0xFFFF;
		AC &= 0xFF;
		XR &= 0xFF;
		YR &= 0xFF;
		SP &= 0xFF;
		*/
	} // while steps
	return count;
}