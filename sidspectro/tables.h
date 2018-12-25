#pragma once

#include <string>
typedef std::string string;

const int OP_NONE = 1;
const int OP_ADC = 2;
const int OP_AND = 3;
const int OP_ASL = 4;
const int OP_BCC = 5;
const int OP_BCS = 6;
const int OP_BEQ = 7;
const int OP_BIT = 8;
const int OP_BMI = 9;
const int OP_BNE = 10;
const int OP_BPL = 11;
const int OP_BRK = 12;
const int OP_BVC = 13;
const int OP_BVS = 14;
const int OP_CLC = 15;
const int OP_CLD = 16;
const int OP_CLI = 17;
const int OP_CLV = 18;
const int OP_CMP = 19;
const int OP_CPX = 20;
const int OP_CPY = 21;
const int OP_DEC = 22;
const int OP_DEX = 23;
const int OP_DEY = 24;
const int OP_EOR = 25;
const int OP_INC = 26;
const int OP_INX = 27;
const int OP_INY = 28;
const int OP_JMP = 29;
const int OP_JSR = 30;
const int OP_LDA = 31;
const int OP_LDX = 32;
const int OP_LDY = 33;
const int OP_LSR = 34;
const int OP_NOP = 35;
const int OP_ORA = 36;
const int OP_PHA = 37;
const int OP_PLA = 38;
const int OP_PHP = 39;
const int OP_PLP = 40;
const int OP_ROL = 41;
const int OP_ROR = 42;
const int OP_RTI = 43;
const int OP_RTS = 44;
const int OP_SBC = 45;
const int OP_SEC = 46;
const int OP_SED = 47;
const int OP_SEI = 48;
const int OP_STA = 49;
const int OP_STX = 50;
const int OP_STY = 51;
const int OP_TAX = 52;
const int OP_TAY = 53;
const int OP_TSX = 54;
const int OP_TXA = 55;
const int OP_TXS = 56;
const int OP_TYA = 57;
const int OP_LAX = 58; // undocumented
const int OP_RLA = 59; // undocumented
const int OP_EMU = 60; // special opcode to execute something emulator specific
const int AMODE_IMP = 1;
const int AMODE_ACC = 2;
const int AMODE_ABS = 3;
const int AMODE_ZP = 4;
const int AMODE_IMM = 5;
const int AMODE_ABSX = 6;
const int AMODE_ABSY = 7;
const int AMODE_INDX = 8;
const int AMODE_INDY = 9;
const int AMODE_ZPX = 10;
const int AMODE_ZPY = 11;
const int AMODE_REL = 12;
const int AMODE_IND = 13;

struct Opcodes
{
	string opsstr;
	int count;
	int amode;
	int op;
	Opcodes(const string &_opsstr, int _count, int _amode, int _op)
		: opsstr(_opsstr), count(_count), amode(_amode), op(_op) {}
};

extern Opcodes opcodes[];

// converted from my C-Code which uses SDL

extern int     SDLK_F1;
extern int     SDLK_F2;
extern int     SDLK_F3;
extern int     SDLK_F4;
extern int     SDLK_F5;
extern int     SDLK_F6;
extern int     SDLK_F7;
extern int     SDLK_F8;
extern int     SDLK_a;
extern int     SDLK_b;
extern int     SDLK_c;
extern int     SDLK_d;
extern int     SDLK_e;
extern int     SDLK_f;
extern int     SDLK_g;
extern int     SDLK_h;
extern int     SDLK_i;
extern int     SDLK_j;
extern int     SDLK_k;
extern int     SDLK_l;
extern int     SDLK_m;
extern int     SDLK_n;
extern int     SDLK_o;
extern int     SDLK_p;
extern int     SDLK_q;
extern int     SDLK_r;
extern int     SDLK_s;
extern int     SDLK_t;
extern int     SDLK_u;
extern int     SDLK_v;
extern int     SDLK_w;
extern int     SDLK_y;
extern int     SDLK_z;
extern int     SDLK_BACKSPACE;
extern int     SDLK_TAB;
extern int     SDLK_RETURN;
extern int     SDLK_ESCAPE;
extern int     SDLK_SPACE;
extern int     SDLK_CTRL;
extern int     SDLK_ALT;
extern int     SDLK_RSHIFT; // not supported unfortunately
extern int     SDLK_LSHIFT;
extern int     SDLK_0;
extern int     SDLK_1;
extern int     SDLK_2;
extern int     SDLK_3;
extern int     SDLK_4;
extern int     SDLK_5;
extern int     SDLK_6;
extern int     SDLK_7;
extern int     SDLK_8;
extern int     SDLK_9;
extern int     SDLK_KP0;
extern int     SDLK_KP1;
extern int     SDLK_KP2;
extern int     SDLK_KP3;
extern int     SDLK_KP4;
extern int     SDLK_KP5;
extern int     SDLK_KP6;
extern int     SDLK_KP7;
extern int     SDLK_KP8;
extern int     SDLK_KP9;
extern int     SDLK_UP;
extern int     SDLK_DOWN;
extern int     SDLK_RIGHT;
extern int     SDLK_LEFT;
extern int     SDLK_PLUS;
extern int     SDLK_MINUS;
extern int     SDLK_SLASH;
extern int     SDLK_COMMA;
extern int     SDLK_PERIOD;
extern int     SDLK_COLON;
extern int     SDLK_SEMICOLON;

extern int     SDLK_LEFTBRACKET; // at
extern int     SDLK_RIGHTBRACKET; // Star
extern int     SDLK_BACKSLASH; // Arrow up

struct Keys
{
	int scancode;
	int row = 0;
	int bit = 4;
	int shift = false;
	Keys(int _scancode, int _row, int _bit, int _shift)
		: scancode(_scancode), row(_row), bit(_bit), shift(_shift) {}
};

extern Keys keys[];
extern int basicrom[];
extern int kernelrom[];
extern int charrom[];
extern int meminit[];
