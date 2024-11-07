/* ======================================================================== */
/* ========================= LICENSING & COPYRIGHT ======================== */
/* ======================================================================== */
/*
 *                                  MUSASHI
 *                                Version 3.32
 *
 * A portable Motorola M680x0 processor emulation engine.
 * Copyright Karl Stenerud.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */



/* ======================================================================== */
/* ================================ INCLUDES ============================== */
/* ======================================================================== */

#include "sys.h"
#include "debug.h"
#include "m68k.h"

#ifndef uint32
#define uint32 uint
#endif

#ifndef uint16
#define uint16 unsigned short
#endif

#ifndef DECL_SPEC
#define DECL_SPEC
#endif

/* ======================================================================== */
/* ============================ GENERAL DEFINES =========================== */
/* ======================================================================== */

/* unsigned int and int must be at least 32 bits wide */
#undef uint
#define uint unsigned int

/* Bit Isolation Functions */
#define BIT_0(A)  ((A) & 0x00000001)
#define BIT_1(A)  ((A) & 0x00000002)
#define BIT_2(A)  ((A) & 0x00000004)
#define BIT_3(A)  ((A) & 0x00000008)
#define BIT_4(A)  ((A) & 0x00000010)
#define BIT_5(A)  ((A) & 0x00000020)
#define BIT_6(A)  ((A) & 0x00000040)
#define BIT_7(A)  ((A) & 0x00000080)
#define BIT_8(A)  ((A) & 0x00000100)
#define BIT_9(A)  ((A) & 0x00000200)
#define BIT_A(A)  ((A) & 0x00000400)
#define BIT_B(A)  ((A) & 0x00000800)
#define BIT_C(A)  ((A) & 0x00001000)
#define BIT_D(A)  ((A) & 0x00002000)
#define BIT_E(A)  ((A) & 0x00004000)
#define BIT_F(A)  ((A) & 0x00008000)
#define BIT_10(A) ((A) & 0x00010000)
#define BIT_11(A) ((A) & 0x00020000)
#define BIT_12(A) ((A) & 0x00040000)
#define BIT_13(A) ((A) & 0x00080000)
#define BIT_14(A) ((A) & 0x00100000)
#define BIT_15(A) ((A) & 0x00200000)
#define BIT_16(A) ((A) & 0x00400000)
#define BIT_17(A) ((A) & 0x00800000)
#define BIT_18(A) ((A) & 0x01000000)
#define BIT_19(A) ((A) & 0x02000000)
#define BIT_1A(A) ((A) & 0x04000000)
#define BIT_1B(A) ((A) & 0x08000000)
#define BIT_1C(A) ((A) & 0x10000000)
#define BIT_1D(A) ((A) & 0x20000000)
#define BIT_1E(A) ((A) & 0x40000000)
#define BIT_1F(A) ((A) & 0x80000000)

/* These are the CPU types understood by this disassembler */
#define TYPE_68000 1
#define TYPE_68010 2
#define TYPE_68020 4
#define TYPE_68030 8
#define TYPE_68040 16

#define M68000_ONLY		TYPE_68000

#define M68010_ONLY		TYPE_68010
#define M68010_LESS		(TYPE_68000 | TYPE_68010)
#define M68010_PLUS		(TYPE_68010 | TYPE_68020 | TYPE_68030 | TYPE_68040)

#define M68020_ONLY 	TYPE_68020
#define M68020_LESS 	(TYPE_68010 | TYPE_68020)
#define M68020_PLUS		(TYPE_68020 | TYPE_68030 | TYPE_68040)

#define M68030_ONLY 	TYPE_68030
#define M68030_LESS 	(TYPE_68010 | TYPE_68020 | TYPE_68030)
#define M68030_PLUS		(TYPE_68030 | TYPE_68040)

#define M68040_PLUS		TYPE_68040


/* Extension word formats */
#define EXT_8BIT_DISPLACEMENT(A)          ((A)&0xff)
#define EXT_FULL(A)                       BIT_8(A)
#define EXT_EFFECTIVE_ZERO(A)             (((A)&0xe4) == 0xc4 || ((A)&0xe2) == 0xc0)
#define EXT_BASE_REGISTER_PRESENT(A)      (!BIT_7(A))
#define EXT_INDEX_REGISTER_PRESENT(A)     (!BIT_6(A))
#define EXT_INDEX_REGISTER(A)             (((A)>>12)&7)
#define EXT_INDEX_PRE_POST(A)             (EXT_INDEX_PRESENT(A) && (A)&3)
#define EXT_INDEX_PRE(A)                  (EXT_INDEX_PRESENT(A) && ((A)&7) < 4 && ((A)&7) != 0)
#define EXT_INDEX_POST(A)                 (EXT_INDEX_PRESENT(A) && ((A)&7) > 4)
#define EXT_INDEX_SCALE(A)                (((A)>>9)&3)
#define EXT_INDEX_LONG(A)                 BIT_B(A)
#define EXT_INDEX_AR(A)                   BIT_F(A)
#define EXT_BASE_DISPLACEMENT_PRESENT(A)  (((A)&0x30) > 0x10)
#define EXT_BASE_DISPLACEMENT_WORD(A)     (((A)&0x30) == 0x20)
#define EXT_BASE_DISPLACEMENT_LONG(A)     (((A)&0x30) == 0x30)
#define EXT_OUTER_DISPLACEMENT_PRESENT(A) (((A)&3) > 1 && ((A)&0x47) < 0x44)
#define EXT_OUTER_DISPLACEMENT_WORD(A)    (((A)&3) == 2 && ((A)&0x47) < 0x44)
#define EXT_OUTER_DISPLACEMENT_LONG(A)    (((A)&3) == 3 && ((A)&0x47) < 0x44)


/* Opcode flags */
#if M68K_COMPILE_FOR_MAME == OPT_ON
#define SET_OPCODE_FLAGS(x)	g_opcode_type = x;
#define COMBINE_OPCODE_FLAGS(x) ((x) | g_opcode_type | DASMFLAG_SUPPORTED)
#else
#define SET_OPCODE_FLAGS(x)
#define COMBINE_OPCODE_FLAGS(X) (X)
#endif


/* ======================================================================== */
/* =============================== PROTOTYPES ============================= */
/* ======================================================================== */

/* Read data at the PC and increment PC */
uint  read_imm_8(void);
uint  read_imm_16(void);
uint  read_imm_32(void);

/* Read data at the PC but don't imcrement the PC */
uint  peek_imm_8(void);
uint  peek_imm_16(void);
uint  peek_imm_32(void);

/* make signed integers 100% portably */
static int make_int_8(int value);
static int make_int_16(int value);
static int make_int_32(int value);

/* make a string of a hex value */
static char* make_signed_hex_str_8(uint val);
static char* make_signed_hex_str_16(uint val);
static char* make_signed_hex_str_32(uint val);

/* make string of ea mode */
static char* get_ea_mode_str(uint instruction, uint size);

char* get_ea_mode_str_8(uint instruction);
char* get_ea_mode_str_16(uint instruction);
char* get_ea_mode_str_32(uint instruction);

/* make string of immediate value */
static char* get_imm_str_s(uint size);
static char* get_imm_str_u(uint size);

char* get_imm_str_s8(void);
char* get_imm_str_s16(void);
char* get_imm_str_s32(void);

/* Stuff to build the opcode handler jump table */
static void  build_opcode_table(void);
static int   valid_ea(uint opcode, uint mask);
static int DECL_SPEC compare_nof_true_bits(const void *aptr, const void *bptr);

/* used to build opcode handler jump table */
typedef struct
{
	void (*opcode_handler)(void); /* handler function */
	uint mask;                    /* mask on opcode */
	uint match;                   /* what to match after masking */
	uint ea_mask;                 /* what ea modes are allowed */
} opcode_struct;



/* ======================================================================== */
/* ================================= DATA ================================= */
/* ======================================================================== */

/* Opcode handler jump table */
static void (*g_instruction_table[0x10000])(void);
/* Flag if disassembler initialized */
static int  g_initialized = 0;

/* Address mask to simulate address lines */
static unsigned int g_address_mask = 0xffffffff;

static char g_dasm_str[100]; /* string to hold disassembly */
static char g_helper_str[100]; /* string to hold helpful info */
static uint g_cpu_pc;        /* program counter */
static uint g_cpu_ir;        /* instruction register */
static uint g_cpu_type;
static uint g_opcode_type;
static const unsigned char* g_rawop;
static uint g_rawbasepc;

/* used by ops like asr, ror, addq, etc */
static const uint g_3bit_qdata_table[8] = {8, 1, 2, 3, 4, 5, 6, 7};

static const uint g_5bit_data_table[32] =
{
	32,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
};

static const char *const g_cc[16] =
{"t", "f", "hi", "ls", "cc", "cs", "ne", "eq", "vc", "vs", "pl", "mi", "ge", "lt", "gt", "le"};

static const char *const g_cpcc[64] =
{/* 000    001    010    011    100    101    110    111 */
	  "f",  "eq", "ogt", "oge", "olt", "ole", "ogl",  "or", /* 000 */
	 "un", "ueq", "ugt", "uge", "ult", "ule",  "ne",   "t", /* 001 */
	 "sf", "seq",  "gt",  "ge",  "lt",  "le",  "gl"  "gle", /* 010 */
  "ngle", "ngl", "nle", "nlt", "nge", "ngt", "sne",  "st", /* 011 */
	  "?",   "?",   "?",   "?",   "?",   "?",   "?",   "?", /* 100 */
	  "?",   "?",   "?",   "?",   "?",   "?",   "?",   "?", /* 101 */
	  "?",   "?",   "?",   "?",   "?",   "?",   "?",   "?", /* 110 */
	  "?",   "?",   "?",   "?",   "?",   "?",   "?",   "?"  /* 111 */
};

static const char *const g_mmuregs[8] =
{
	"tc", "drp", "srp", "crp", "cal", "val", "sccr", "acr"
};

static const char *const g_mmucond[16] =
{
	"bs", "bc", "ls", "lc", "ss", "sc", "as", "ac",
	"ws", "wc", "is", "ic", "gs", "gc", "cs", "cc"
};

/* ======================================================================== */
/* =========================== UTILITY FUNCTIONS ========================== */
/* ======================================================================== */

#define LIMIT_CPU_TYPES(ALLOWED_CPU_TYPES)	\
	if(!(g_cpu_type & ALLOWED_CPU_TYPES))	\
	{										\
		if((g_cpu_ir & 0xf000) == 0xf000)	\
			d68000_1111();					\
		else d68000_illegal();				\
		return;								\
	}

static uint dasm_read_imm_8(uint advance)
{
	uint result;
	if (g_rawop)
		result = g_rawop[g_cpu_pc + 1 - g_rawbasepc];
	else
		result = m68k_read_disassembler_16(g_cpu_pc & g_address_mask) & 0xff;
	g_cpu_pc += advance;
	return result;
}

static uint dasm_read_imm_16(uint advance)
{
	uint result;
	if (g_rawop)
		result = (g_rawop[g_cpu_pc + 0 - g_rawbasepc] << 8) |
		          g_rawop[g_cpu_pc + 1 - g_rawbasepc];
	else
		result = m68k_read_disassembler_16(g_cpu_pc & g_address_mask) & 0xffff;
	g_cpu_pc += advance;
	return result;
}

static uint dasm_read_imm_32(uint advance)
{
	uint result;
	if (g_rawop)
		result = (g_rawop[g_cpu_pc + 0 - g_rawbasepc] << 24) |
		         (g_rawop[g_cpu_pc + 1 - g_rawbasepc] << 16) |
		         (g_rawop[g_cpu_pc + 2 - g_rawbasepc] << 8) |
		          g_rawop[g_cpu_pc + 3 - g_rawbasepc];
	else
		result = m68k_read_disassembler_32(g_cpu_pc & g_address_mask) & 0xffffffff;
	g_cpu_pc += advance;
	return result;
}

#define read_imm_8()  dasm_read_imm_8(2)
#define read_imm_16() dasm_read_imm_16(2)
#define read_imm_32() dasm_read_imm_32(4)

#define peek_imm_8()  dasm_read_imm_8(0)
#define peek_imm_16() dasm_read_imm_16(0)
#define peek_imm_32() dasm_read_imm_32(0)

/* Fake a split interface */
#define get_ea_mode_str_8(instruction) get_ea_mode_str(instruction, 0)
#define get_ea_mode_str_16(instruction) get_ea_mode_str(instruction, 1)
#define get_ea_mode_str_32(instruction) get_ea_mode_str(instruction, 2)

#define get_imm_str_s8() get_imm_str_s(0)
#define get_imm_str_s16() get_imm_str_s(1)
#define get_imm_str_s32() get_imm_str_s(2)

#define get_imm_str_u8() get_imm_str_u(0)
#define get_imm_str_u16() get_imm_str_u(1)
#define get_imm_str_u32() get_imm_str_u(2)

static int sext_7bit_int(int value)
{
	return (value & 0x40) ? (value | 0xffffff80) : (value & 0x7f);
}


/* 100% portable signed int generators */
static int make_int_8(int value)
{
	return (value & 0x80) ? value | ~0xff : value & 0xff;
}

static int make_int_16(int value)
{
	return (value & 0x8000) ? value | ~0xffff : value & 0xffff;
}

static int make_int_32(int value)
{
	return (value & 0x80000000) ? value | ~0xffffffff : value & 0xffffffff;
}

/* Get string representation of hex values */
static char* make_signed_hex_str_8(uint val)
{
	static char str[20];

	val &= 0xff;

	if(val == 0x80)
		sys_sprintf(str, "-$80");
	else if(val & 0x80)
		sys_sprintf(str, "-$%x", (0-val) & 0x7f);
	else
		sys_sprintf(str, "$%x", val & 0x7f);

	return str;
}

static char* make_signed_hex_str_16(uint val)
{
	static char str[20];

	val &= 0xffff;

	if(val == 0x8000)
		sys_sprintf(str, "-$8000");
	else if(val & 0x8000)
		sys_sprintf(str, "-$%x", (0-val) & 0x7fff);
	else
		sys_sprintf(str, "$%x", val & 0x7fff);

	return str;
}

static char* make_signed_hex_str_32(uint val)
{
	static char str[20];

	val &= 0xffffffff;

	if(val == 0x80000000)
		sys_sprintf(str, "-$80000000");
	else if(val & 0x80000000)
		sys_sprintf(str, "-$%x", (0-val) & 0x7fffffff);
	else
		sys_sprintf(str, "$%x", val & 0x7fffffff);

	return str;
}


/* make string of immediate value */
static char* get_imm_str_s(uint size)
{
	static char str[15];
	if(size == 0)
		sys_sprintf(str, "#%s", make_signed_hex_str_8(read_imm_8()));
	else if(size == 1)
		sys_sprintf(str, "#%s", make_signed_hex_str_16(read_imm_16()));
	else
		sys_sprintf(str, "#%s", make_signed_hex_str_32(read_imm_32()));
	return str;
}

static char* get_imm_str_u(uint size)
{
	static char str[15];
	if(size == 0)
		sys_sprintf(str, "#$%x", read_imm_8() & 0xff);
	else if(size == 1)
		sys_sprintf(str, "#$%x", read_imm_16() & 0xffff);
	else
		sys_sprintf(str, "#$%x", read_imm_32() & 0xffffffff);
	return str;
}

/* Make string of effective address mode */
static char* get_ea_mode_str(uint instruction, uint size)
{
	static char b1[64];
	static char b2[64];
	static char* mode = b2;
	uint extension;
	uint base;
	uint outer;
	char base_reg[4];
	char index_reg[8];
	uint preindex;
	uint postindex;
	uint comma = 0;
	uint temp_value;

	/* Switch buffers so we don't clobber on a double-call to this function */
	mode = mode == b1 ? b2 : b1;

	switch(instruction & 0x3f)
	{
		case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
		/* data register direct */
			sys_sprintf(mode, "D%d", instruction&7);
			break;
		case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
		/* address register direct */
			sys_sprintf(mode, "A%d", instruction&7);
			break;
		case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
		/* address register indirect */
			sys_sprintf(mode, "(A%d)", instruction&7);
			break;
		case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
		/* address register indirect with postincrement */
			sys_sprintf(mode, "(A%d)+", instruction&7);
			break;
		case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
		/* address register indirect with predecrement */
			sys_sprintf(mode, "-(A%d)", instruction&7);
			break;
		case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
		/* address register indirect with displacement*/
			sys_sprintf(mode, "(%s,A%d)", make_signed_hex_str_16(read_imm_16()), instruction&7);
			break;
		case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
		/* address register indirect with index */
			extension = read_imm_16();

			if(EXT_FULL(extension))
			{
				if(EXT_EFFECTIVE_ZERO(extension))
				{
					sys_strcpy(mode, "0");
					break;
				}
				base = EXT_BASE_DISPLACEMENT_PRESENT(extension) ? (EXT_BASE_DISPLACEMENT_LONG(extension) ? read_imm_32() : read_imm_16()) : 0;
				outer = EXT_OUTER_DISPLACEMENT_PRESENT(extension) ? (EXT_OUTER_DISPLACEMENT_LONG(extension) ? read_imm_32() : read_imm_16()) : 0;
				if(EXT_BASE_REGISTER_PRESENT(extension))
					sys_sprintf(base_reg, "A%d", instruction&7);
				else
					*base_reg = 0;
				if(EXT_INDEX_REGISTER_PRESENT(extension))
				{
					sys_sprintf(index_reg, "%c%d.%c", EXT_INDEX_AR(extension) ? 'A' : 'D', EXT_INDEX_REGISTER(extension), EXT_INDEX_LONG(extension) ? 'l' : 'w');
					if(EXT_INDEX_SCALE(extension))
						sys_sprintf(index_reg+sys_strlen(index_reg), "*%d", 1 << EXT_INDEX_SCALE(extension));
				}
				else
					*index_reg = 0;
				preindex = (extension&7) > 0 && (extension&7) < 4;
				postindex = (extension&7) > 4;

				sys_strcpy(mode, "(");
				if(preindex || postindex)
					sys_strcat(mode, "[");
				if(base)
				{
					if (EXT_BASE_DISPLACEMENT_LONG(extension))
					{
						sys_strcat(mode, make_signed_hex_str_32(base));
					}
					else
					{
						sys_strcat(mode, make_signed_hex_str_16(base));
					}
					comma = 1;
				}
				if(*base_reg)
				{
					if(comma)
						sys_strcat(mode, ",");
					sys_strcat(mode, base_reg);
					comma = 1;
				}
				if(postindex)
				{
					sys_strcat(mode, "]");
					comma = 1;
				}
				if(*index_reg)
				{
					if(comma)
						sys_strcat(mode, ",");
					sys_strcat(mode, index_reg);
					comma = 1;
				}
				if(preindex)
				{
					sys_strcat(mode, "]");
					comma = 1;
				}
				if(outer)
				{
					if(comma)
						sys_strcat(mode, ",");
					sys_strcat(mode, make_signed_hex_str_16(outer));
				}
				sys_strcat(mode, ")");
				break;
			}

			if(EXT_8BIT_DISPLACEMENT(extension) == 0)
				sys_sprintf(mode, "(A%d,%c%d.%c", instruction&7, EXT_INDEX_AR(extension) ? 'A' : 'D', EXT_INDEX_REGISTER(extension), EXT_INDEX_LONG(extension) ? 'l' : 'w');
			else
				sys_sprintf(mode, "(%s,A%d,%c%d.%c", make_signed_hex_str_8(extension), instruction&7, EXT_INDEX_AR(extension) ? 'A' : 'D', EXT_INDEX_REGISTER(extension), EXT_INDEX_LONG(extension) ? 'l' : 'w');
			if(EXT_INDEX_SCALE(extension))
				sys_sprintf(mode+sys_strlen(mode), "*%d", 1 << EXT_INDEX_SCALE(extension));
			sys_strcat(mode, ")");
			break;
		case 0x38:
		/* absolute short address */
			sys_sprintf(mode, "$%x.w", read_imm_16());
			break;
		case 0x39:
		/* absolute long address */
			sys_sprintf(mode, "$%x.l", read_imm_32());
			break;
		case 0x3a:
		/* program counter with displacement */
			temp_value = read_imm_16();
			sys_sprintf(mode, "(%s,PC)", make_signed_hex_str_16(temp_value));
			sys_sprintf(g_helper_str, "; ($%x)", (make_int_16(temp_value) + g_cpu_pc-2) & 0xffffffff);
			break;
		case 0x3b:
		/* program counter with index */
			extension = read_imm_16();

			if(EXT_FULL(extension))
			{
				if(EXT_EFFECTIVE_ZERO(extension))
				{
					sys_strcpy(mode, "0");
					break;
				}
				base = EXT_BASE_DISPLACEMENT_PRESENT(extension) ? (EXT_BASE_DISPLACEMENT_LONG(extension) ? read_imm_32() : read_imm_16()) : 0;
				outer = EXT_OUTER_DISPLACEMENT_PRESENT(extension) ? (EXT_OUTER_DISPLACEMENT_LONG(extension) ? read_imm_32() : read_imm_16()) : 0;
				if(EXT_BASE_REGISTER_PRESENT(extension))
					sys_strcpy(base_reg, "PC");
				else
					*base_reg = 0;
				if(EXT_INDEX_REGISTER_PRESENT(extension))
				{
					sys_sprintf(index_reg, "%c%d.%c", EXT_INDEX_AR(extension) ? 'A' : 'D', EXT_INDEX_REGISTER(extension), EXT_INDEX_LONG(extension) ? 'l' : 'w');
					if(EXT_INDEX_SCALE(extension))
						sys_sprintf(index_reg+sys_strlen(index_reg), "*%d", 1 << EXT_INDEX_SCALE(extension));
				}
				else
					*index_reg = 0;
				preindex = (extension&7) > 0 && (extension&7) < 4;
				postindex = (extension&7) > 4;

				sys_strcpy(mode, "(");
				if(preindex || postindex)
					sys_strcat(mode, "[");
				if(base)
				{
					sys_strcat(mode, make_signed_hex_str_16(base));
					comma = 1;
				}
				if(*base_reg)
				{
					if(comma)
						sys_strcat(mode, ",");
					sys_strcat(mode, base_reg);
					comma = 1;
				}
				if(postindex)
				{
					sys_strcat(mode, "]");
					comma = 1;
				}
				if(*index_reg)
				{
					if(comma)
						sys_strcat(mode, ",");
					sys_strcat(mode, index_reg);
					comma = 1;
				}
				if(preindex)
				{
					sys_strcat(mode, "]");
					comma = 1;
				}
				if(outer)
				{
					if(comma)
						sys_strcat(mode, ",");
					sys_strcat(mode, make_signed_hex_str_16(outer));
				}
				sys_strcat(mode, ")");
				break;
			}

			if(EXT_8BIT_DISPLACEMENT(extension) == 0)
				sys_sprintf(mode, "(PC,%c%d.%c", EXT_INDEX_AR(extension) ? 'A' : 'D', EXT_INDEX_REGISTER(extension), EXT_INDEX_LONG(extension) ? 'l' : 'w');
			else
				sys_sprintf(mode, "(%s,PC,%c%d.%c", make_signed_hex_str_8(extension), EXT_INDEX_AR(extension) ? 'A' : 'D', EXT_INDEX_REGISTER(extension), EXT_INDEX_LONG(extension) ? 'l' : 'w');
			if(EXT_INDEX_SCALE(extension))
				sys_sprintf(mode+sys_strlen(mode), "*%d", 1 << EXT_INDEX_SCALE(extension));
			sys_strcat(mode, ")");
			break;
		case 0x3c:
		/* Immediate */
			sys_sprintf(mode, "%s", get_imm_str_u(size));
			break;
		default:
			sys_sprintf(mode, "INVALID %x", instruction & 0x3f);
	}
	return mode;
}



/* ======================================================================== */
/* ========================= INSTRUCTION HANDLERS ========================= */
/* ======================================================================== */
/* Instruction handler function names follow this convention:
 *
 * d68000_NAME_EXTENSIONS(void)
 * where NAME is the name of the opcode it handles and EXTENSIONS are any
 * extensions for special instances of that opcode.
 *
 * Examples:
 *   d68000_add_er_8(): add opcode, from effective address to register,
 *                      size = byte
 *
 *   d68000_asr_s_8(): arithmetic shift right, static count, size = byte
 *
 *
 * Common extensions:
 * 8   : size = byte
 * 16  : size = word
 * 32  : size = long
 * rr  : register to register
 * mm  : memory to memory
 * r   : register
 * s   : static
 * er  : effective address -> register
 * re  : register -> effective address
 * ea  : using effective address mode of operation
 * d   : data register direct
 * a   : address register direct
 * ai  : address register indirect
 * pi  : address register indirect with postincrement
 * pd  : address register indirect with predecrement
 * di  : address register indirect with displacement
 * ix  : address register indirect with index
 * aw  : absolute word
 * al  : absolute long
 */

static void d68000_illegal(void)
{
	sys_sprintf(g_dasm_str, "dc.w $%04x; ILLEGAL", g_cpu_ir);
}

static void d68000_1010(void)
{
	sys_sprintf(g_dasm_str, "dc.w    $%04x; opcode 1010", g_cpu_ir);
}


static void d68000_1111(void)
{
	sys_sprintf(g_dasm_str, "dc.w    $%04x; opcode 1111", g_cpu_ir);
}


static void d68000_abcd_rr(void)
{
	sys_sprintf(g_dasm_str, "abcd    D%d, D%d", g_cpu_ir&7, (g_cpu_ir>>9)&7);
}


static void d68000_abcd_mm(void)
{
	sys_sprintf(g_dasm_str, "abcd    -(A%d), -(A%d)", g_cpu_ir&7, (g_cpu_ir>>9)&7);
}

static void d68000_add_er_8(void)
{
	sys_sprintf(g_dasm_str, "add.b   %s, D%d", get_ea_mode_str_8(g_cpu_ir), (g_cpu_ir>>9)&7);
}


static void d68000_add_er_16(void)
{
	sys_sprintf(g_dasm_str, "add.w   %s, D%d", get_ea_mode_str_16(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_add_er_32(void)
{
	sys_sprintf(g_dasm_str, "add.l   %s, D%d", get_ea_mode_str_32(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_add_re_8(void)
{
	sys_sprintf(g_dasm_str, "add.b   D%d, %s", (g_cpu_ir>>9)&7, get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_add_re_16(void)
{
	sys_sprintf(g_dasm_str, "add.w   D%d, %s", (g_cpu_ir>>9)&7, get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_add_re_32(void)
{
	sys_sprintf(g_dasm_str, "add.l   D%d, %s", (g_cpu_ir>>9)&7, get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_adda_16(void)
{
	sys_sprintf(g_dasm_str, "adda.w  %s, A%d", get_ea_mode_str_16(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_adda_32(void)
{
	sys_sprintf(g_dasm_str, "adda.l  %s, A%d", get_ea_mode_str_32(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_addi_8(void)
{
	char* str = get_imm_str_s8();
	sys_sprintf(g_dasm_str, "addi.b  %s, %s", str, get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_addi_16(void)
{
	char* str = get_imm_str_s16();
	sys_sprintf(g_dasm_str, "addi.w  %s, %s", str, get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_addi_32(void)
{
	char* str = get_imm_str_s32();
	sys_sprintf(g_dasm_str, "addi.l  %s, %s", str, get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_addq_8(void)
{
	sys_sprintf(g_dasm_str, "addq.b  #%d, %s", g_3bit_qdata_table[(g_cpu_ir>>9)&7], get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_addq_16(void)
{
	sys_sprintf(g_dasm_str, "addq.w  #%d, %s", g_3bit_qdata_table[(g_cpu_ir>>9)&7], get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_addq_32(void)
{
	sys_sprintf(g_dasm_str, "addq.l  #%d, %s", g_3bit_qdata_table[(g_cpu_ir>>9)&7], get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_addx_rr_8(void)
{
	sys_sprintf(g_dasm_str, "addx.b  D%d, D%d", g_cpu_ir&7, (g_cpu_ir>>9)&7);
}

static void d68000_addx_rr_16(void)
{
	sys_sprintf(g_dasm_str, "addx.w  D%d, D%d", g_cpu_ir&7, (g_cpu_ir>>9)&7);
}

static void d68000_addx_rr_32(void)
{
	sys_sprintf(g_dasm_str, "addx.l  D%d, D%d", g_cpu_ir&7, (g_cpu_ir>>9)&7);
}

static void d68000_addx_mm_8(void)
{
	sys_sprintf(g_dasm_str, "addx.b  -(A%d), -(A%d)", g_cpu_ir&7, (g_cpu_ir>>9)&7);
}

static void d68000_addx_mm_16(void)
{
	sys_sprintf(g_dasm_str, "addx.w  -(A%d), -(A%d)", g_cpu_ir&7, (g_cpu_ir>>9)&7);
}

static void d68000_addx_mm_32(void)
{
	sys_sprintf(g_dasm_str, "addx.l  -(A%d), -(A%d)", g_cpu_ir&7, (g_cpu_ir>>9)&7);
}

static void d68000_and_er_8(void)
{
	sys_sprintf(g_dasm_str, "and.b   %s, D%d", get_ea_mode_str_8(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_and_er_16(void)
{
	sys_sprintf(g_dasm_str, "and.w   %s, D%d", get_ea_mode_str_16(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_and_er_32(void)
{
	sys_sprintf(g_dasm_str, "and.l   %s, D%d", get_ea_mode_str_32(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_and_re_8(void)
{
	sys_sprintf(g_dasm_str, "and.b   D%d, %s", (g_cpu_ir>>9)&7, get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_and_re_16(void)
{
	sys_sprintf(g_dasm_str, "and.w   D%d, %s", (g_cpu_ir>>9)&7, get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_and_re_32(void)
{
	sys_sprintf(g_dasm_str, "and.l   D%d, %s", (g_cpu_ir>>9)&7, get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_andi_8(void)
{
	char* str = get_imm_str_u8();
	sys_sprintf(g_dasm_str, "andi.b  %s, %s", str, get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_andi_16(void)
{
	char* str = get_imm_str_u16();
	sys_sprintf(g_dasm_str, "andi.w  %s, %s", str, get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_andi_32(void)
{
	char* str = get_imm_str_u32();
	sys_sprintf(g_dasm_str, "andi.l  %s, %s", str, get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_andi_to_ccr(void)
{
	sys_sprintf(g_dasm_str, "andi    %s, CCR", get_imm_str_u8());
}

static void d68000_andi_to_sr(void)
{
	sys_sprintf(g_dasm_str, "andi    %s, SR", get_imm_str_u16());
}

static void d68000_asr_s_8(void)
{
	sys_sprintf(g_dasm_str, "asr.b   #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_asr_s_16(void)
{
	sys_sprintf(g_dasm_str, "asr.w   #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_asr_s_32(void)
{
	sys_sprintf(g_dasm_str, "asr.l   #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_asr_r_8(void)
{
	sys_sprintf(g_dasm_str, "asr.b   D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_asr_r_16(void)
{
	sys_sprintf(g_dasm_str, "asr.w   D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_asr_r_32(void)
{
	sys_sprintf(g_dasm_str, "asr.l   D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_asr_ea(void)
{
	sys_sprintf(g_dasm_str, "asr.w   %s", get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_asl_s_8(void)
{
	sys_sprintf(g_dasm_str, "asl.b   #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_asl_s_16(void)
{
	sys_sprintf(g_dasm_str, "asl.w   #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_asl_s_32(void)
{
	sys_sprintf(g_dasm_str, "asl.l   #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_asl_r_8(void)
{
	sys_sprintf(g_dasm_str, "asl.b   D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_asl_r_16(void)
{
	sys_sprintf(g_dasm_str, "asl.w   D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_asl_r_32(void)
{
	sys_sprintf(g_dasm_str, "asl.l   D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_asl_ea(void)
{
	sys_sprintf(g_dasm_str, "asl.w   %s", get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_bcc_8(void)
{
	uint temp_pc = g_cpu_pc;
	sys_sprintf(g_dasm_str, "b%-2s     $%x", g_cc[(g_cpu_ir>>8)&0xf], temp_pc + make_int_8(g_cpu_ir));
}

static void d68000_bcc_16(void)
{
	uint temp_pc = g_cpu_pc;
	sys_sprintf(g_dasm_str, "b%-2s     $%x", g_cc[(g_cpu_ir>>8)&0xf], temp_pc + make_int_16(read_imm_16()));
}

static void d68020_bcc_32(void)
{
	uint temp_pc = g_cpu_pc;
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "b%-2s     $%x; (2+)", g_cc[(g_cpu_ir>>8)&0xf], temp_pc + read_imm_32());
}

static void d68000_bchg_r(void)
{
	sys_sprintf(g_dasm_str, "bchg    D%d, %s", (g_cpu_ir>>9)&7, get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_bchg_s(void)
{
	char* str = get_imm_str_u8();
	sys_sprintf(g_dasm_str, "bchg    %s, %s", str, get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_bclr_r(void)
{
	sys_sprintf(g_dasm_str, "bclr    D%d, %s", (g_cpu_ir>>9)&7, get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_bclr_s(void)
{
	char* str = get_imm_str_u8();
	sys_sprintf(g_dasm_str, "bclr    %s, %s", str, get_ea_mode_str_8(g_cpu_ir));
}

static void d68010_bkpt(void)
{
	LIMIT_CPU_TYPES(M68010_PLUS);
	sys_sprintf(g_dasm_str, "bkpt #%d; (1+)", g_cpu_ir&7);
}

static void d68020_bfchg(void)
{
	uint extension;
	char offset[3];
	char width[3];

	LIMIT_CPU_TYPES(M68020_PLUS);

	extension = read_imm_16();

	if(BIT_B(extension))
		sys_sprintf(offset, "D%d", (extension>>6)&7);
	else
		sys_sprintf(offset, "%d", (extension>>6)&31);
	if(BIT_5(extension))
		sys_sprintf(width, "D%d", extension&7);
	else
		sys_sprintf(width, "%d", g_5bit_data_table[extension&31]);
	sys_sprintf(g_dasm_str, "bfchg   %s {%s:%s}; (2+)", get_ea_mode_str_8(g_cpu_ir), offset, width);
}

static void d68020_bfclr(void)
{
	uint extension;
	char offset[3];
	char width[3];

	LIMIT_CPU_TYPES(M68020_PLUS);

	extension = read_imm_16();

	if(BIT_B(extension))
		sys_sprintf(offset, "D%d", (extension>>6)&7);
	else
		sys_sprintf(offset, "%d", (extension>>6)&31);
	if(BIT_5(extension))
		sys_sprintf(width, "D%d", extension&7);
	else
		sys_sprintf(width, "%d", g_5bit_data_table[extension&31]);
	sys_sprintf(g_dasm_str, "bfclr   %s {%s:%s}; (2+)", get_ea_mode_str_8(g_cpu_ir), offset, width);
}

static void d68020_bfexts(void)
{
	uint extension;
	char offset[3];
	char width[3];

	LIMIT_CPU_TYPES(M68020_PLUS);

	extension = read_imm_16();

	if(BIT_B(extension))
		sys_sprintf(offset, "D%d", (extension>>6)&7);
	else
		sys_sprintf(offset, "%d", (extension>>6)&31);
	if(BIT_5(extension))
		sys_sprintf(width, "D%d", extension&7);
	else
		sys_sprintf(width, "%d", g_5bit_data_table[extension&31]);
	sys_sprintf(g_dasm_str, "bfexts  %s {%s:%s}, D%d; (2+)", get_ea_mode_str_8(g_cpu_ir), offset, width, (extension>>12)&7);
}

static void d68020_bfextu(void)
{
	uint extension;
	char offset[3];
	char width[3];

	LIMIT_CPU_TYPES(M68020_PLUS);

	extension = read_imm_16();

	if(BIT_B(extension))
		sys_sprintf(offset, "D%d", (extension>>6)&7);
	else
		sys_sprintf(offset, "%d", (extension>>6)&31);
	if(BIT_5(extension))
		sys_sprintf(width, "D%d", extension&7);
	else
		sys_sprintf(width, "%d", g_5bit_data_table[extension&31]);
	sys_sprintf(g_dasm_str, "bfextu  %s {%s:%s}, D%d; (2+)", get_ea_mode_str_8(g_cpu_ir), offset, width, (extension>>12)&7);
}

static void d68020_bfffo(void)
{
	uint extension;
	char offset[3];
	char width[3];

	LIMIT_CPU_TYPES(M68020_PLUS);

	extension = read_imm_16();

	if(BIT_B(extension))
		sys_sprintf(offset, "D%d", (extension>>6)&7);
	else
		sys_sprintf(offset, "%d", (extension>>6)&31);
	if(BIT_5(extension))
		sys_sprintf(width, "D%d", extension&7);
	else
		sys_sprintf(width, "%d", g_5bit_data_table[extension&31]);
	sys_sprintf(g_dasm_str, "bfffo   %s {%s:%s}, D%d; (2+)", get_ea_mode_str_8(g_cpu_ir), offset, width, (extension>>12)&7);
}

static void d68020_bfins(void)
{
	uint extension;
	char offset[3];
	char width[3];

	LIMIT_CPU_TYPES(M68020_PLUS);

	extension = read_imm_16();

	if(BIT_B(extension))
		sys_sprintf(offset, "D%d", (extension>>6)&7);
	else
		sys_sprintf(offset, "%d", (extension>>6)&31);
	if(BIT_5(extension))
		sys_sprintf(width, "D%d", extension&7);
	else
		sys_sprintf(width, "%d", g_5bit_data_table[extension&31]);
	sys_sprintf(g_dasm_str, "bfins   D%d, %s {%s:%s}; (2+)", (extension>>12)&7, get_ea_mode_str_8(g_cpu_ir), offset, width);
}

static void d68020_bfset(void)
{
	uint extension;
	char offset[3];
	char width[3];

	LIMIT_CPU_TYPES(M68020_PLUS);

	extension = read_imm_16();

	if(BIT_B(extension))
		sys_sprintf(offset, "D%d", (extension>>6)&7);
	else
		sys_sprintf(offset, "%d", (extension>>6)&31);
	if(BIT_5(extension))
		sys_sprintf(width, "D%d", extension&7);
	else
		sys_sprintf(width, "%d", g_5bit_data_table[extension&31]);
	sys_sprintf(g_dasm_str, "bfset   %s {%s:%s}; (2+)", get_ea_mode_str_8(g_cpu_ir), offset, width);
}

static void d68020_bftst(void)
{
	uint extension;
	char offset[3];
	char width[3];

	LIMIT_CPU_TYPES(M68020_PLUS);

	extension = read_imm_16();

	if(BIT_B(extension))
		sys_sprintf(offset, "D%d", (extension>>6)&7);
	else
		sys_sprintf(offset, "%d", (extension>>6)&31);
	if(BIT_5(extension))
		sys_sprintf(width, "D%d", extension&7);
	else
		sys_sprintf(width, "%d", g_5bit_data_table[extension&31]);
	sys_sprintf(g_dasm_str, "bftst   %s {%s:%s}; (2+)", get_ea_mode_str_8(g_cpu_ir), offset, width);
}

static void d68000_bra_8(void)
{
	uint temp_pc = g_cpu_pc;
	sys_sprintf(g_dasm_str, "bra     $%x", temp_pc + make_int_8(g_cpu_ir));
}

static void d68000_bra_16(void)
{
	uint temp_pc = g_cpu_pc;
	sys_sprintf(g_dasm_str, "bra     $%x", temp_pc + make_int_16(read_imm_16()));
}

static void d68020_bra_32(void)
{
	uint temp_pc = g_cpu_pc;
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "bra     $%x; (2+)", temp_pc + read_imm_32());
}

static void d68000_bset_r(void)
{
	sys_sprintf(g_dasm_str, "bset    D%d, %s", (g_cpu_ir>>9)&7, get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_bset_s(void)
{
	char* str = get_imm_str_u8();
	sys_sprintf(g_dasm_str, "bset    %s, %s", str, get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_bsr_8(void)
{
	uint temp_pc = g_cpu_pc;
	sys_sprintf(g_dasm_str, "bsr     $%x", temp_pc + make_int_8(g_cpu_ir));
	SET_OPCODE_FLAGS(DASMFLAG_STEP_OVER);
}

static void d68000_bsr_16(void)
{
	uint temp_pc = g_cpu_pc;
	sys_sprintf(g_dasm_str, "bsr     $%x", temp_pc + make_int_16(read_imm_16()));
	SET_OPCODE_FLAGS(DASMFLAG_STEP_OVER);
}

static void d68020_bsr_32(void)
{
	uint temp_pc = g_cpu_pc;
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "bsr     $%x; (2+)", temp_pc + read_imm_32());
	SET_OPCODE_FLAGS(DASMFLAG_STEP_OVER);
}

static void d68000_btst_r(void)
{
	sys_sprintf(g_dasm_str, "btst    D%d, %s", (g_cpu_ir>>9)&7, get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_btst_s(void)
{
	char* str = get_imm_str_u8();
	sys_sprintf(g_dasm_str, "btst    %s, %s", str, get_ea_mode_str_8(g_cpu_ir));
}

static void d68020_callm(void)
{
	char* str;
	LIMIT_CPU_TYPES(M68020_ONLY);
	str = get_imm_str_u8();

	sys_sprintf(g_dasm_str, "callm   %s, %s; (2)", str, get_ea_mode_str_8(g_cpu_ir));
}

static void d68020_cas_8(void)
{
	uint extension;
	LIMIT_CPU_TYPES(M68020_PLUS);
	extension = read_imm_16();
	sys_sprintf(g_dasm_str, "cas.b   D%d, D%d, %s; (2+)", extension&7, (extension>>6)&7, get_ea_mode_str_8(g_cpu_ir));
}

static void d68020_cas_16(void)
{
	uint extension;
	LIMIT_CPU_TYPES(M68020_PLUS);
	extension = read_imm_16();
	sys_sprintf(g_dasm_str, "cas.w   D%d, D%d, %s; (2+)", extension&7, (extension>>6)&7, get_ea_mode_str_16(g_cpu_ir));
}

static void d68020_cas_32(void)
{
	uint extension;
	LIMIT_CPU_TYPES(M68020_PLUS);
	extension = read_imm_16();
	sys_sprintf(g_dasm_str, "cas.l   D%d, D%d, %s; (2+)", extension&7, (extension>>6)&7, get_ea_mode_str_32(g_cpu_ir));
}

static void d68020_cas2_16(void)
{
/* CAS2 Dc1:Dc2,Du1:Dc2:(Rn1):(Rn2)
f e d c b a 9 8 7 6 5 4 3 2 1 0
 DARn1  0 0 0  Du1  0 0 0  Dc1
 DARn2  0 0 0  Du2  0 0 0  Dc2
*/

	uint extension;
	LIMIT_CPU_TYPES(M68020_PLUS);
	extension = read_imm_32();
	sys_sprintf(g_dasm_str, "cas2.w  D%d:D%d, D%d:D%d, (%c%d):(%c%d); (2+)",
		(extension>>16)&7, extension&7, (extension>>22)&7, (extension>>6)&7,
		BIT_1F(extension) ? 'A' : 'D', (extension>>28)&7,
		BIT_F(extension) ? 'A' : 'D', (extension>>12)&7);
}

static void d68020_cas2_32(void)
{
	uint extension;
	LIMIT_CPU_TYPES(M68020_PLUS);
	extension = read_imm_32();
	sys_sprintf(g_dasm_str, "cas2.l  D%d:D%d, D%d:D%d, (%c%d):(%c%d); (2+)",
		(extension>>16)&7, extension&7, (extension>>22)&7, (extension>>6)&7,
		BIT_1F(extension) ? 'A' : 'D', (extension>>28)&7,
		BIT_F(extension) ? 'A' : 'D', (extension>>12)&7);
}

static void d68000_chk_16(void)
{
	sys_sprintf(g_dasm_str, "chk.w   %s, D%d", get_ea_mode_str_16(g_cpu_ir), (g_cpu_ir>>9)&7);
	SET_OPCODE_FLAGS(DASMFLAG_STEP_OVER);
}

static void d68020_chk_32(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "chk.l   %s, D%d; (2+)", get_ea_mode_str_32(g_cpu_ir), (g_cpu_ir>>9)&7);
	SET_OPCODE_FLAGS(DASMFLAG_STEP_OVER);
}

static void d68020_chk2_cmp2_8(void)
{
	uint extension;
	LIMIT_CPU_TYPES(M68020_PLUS);
	extension = read_imm_16();
	sys_sprintf(g_dasm_str, "%s.b  %s, %c%d; (2+)", BIT_B(extension) ? "chk2" : "cmp2", get_ea_mode_str_8(g_cpu_ir), BIT_F(extension) ? 'A' : 'D', (extension>>12)&7);
}

static void d68020_chk2_cmp2_16(void)
{
	uint extension;
	LIMIT_CPU_TYPES(M68020_PLUS);
	extension = read_imm_16();
	sys_sprintf(g_dasm_str, "%s.w  %s, %c%d; (2+)", BIT_B(extension) ? "chk2" : "cmp2", get_ea_mode_str_16(g_cpu_ir), BIT_F(extension) ? 'A' : 'D', (extension>>12)&7);
}

static void d68020_chk2_cmp2_32(void)
{
	uint extension;
	LIMIT_CPU_TYPES(M68020_PLUS);
	extension = read_imm_16();
	sys_sprintf(g_dasm_str, "%s.l  %s, %c%d; (2+)", BIT_B(extension) ? "chk2" : "cmp2", get_ea_mode_str_32(g_cpu_ir), BIT_F(extension) ? 'A' : 'D', (extension>>12)&7);
}

static void d68040_cinv(void)
{
	LIMIT_CPU_TYPES(M68040_PLUS);
	switch((g_cpu_ir>>3)&3)
	{
		case 0:
			sys_sprintf(g_dasm_str, "cinv (illegal scope); (4)");
			break;
		case 1:
			sys_sprintf(g_dasm_str, "cinvl   %d, (A%d); (4)", (g_cpu_ir>>6)&3, g_cpu_ir&7);
			break;
		case 2:
			sys_sprintf(g_dasm_str, "cinvp   %d, (A%d); (4)", (g_cpu_ir>>6)&3, g_cpu_ir&7);
			break;
		case 3:
			sys_sprintf(g_dasm_str, "cinva   %d; (4)", (g_cpu_ir>>6)&3);
			break;
	}
}

static void d68000_clr_8(void)
{
	sys_sprintf(g_dasm_str, "clr.b   %s", get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_clr_16(void)
{
	sys_sprintf(g_dasm_str, "clr.w   %s", get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_clr_32(void)
{
	sys_sprintf(g_dasm_str, "clr.l   %s", get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_cmp_8(void)
{
	sys_sprintf(g_dasm_str, "cmp.b   %s, D%d", get_ea_mode_str_8(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_cmp_16(void)
{
	sys_sprintf(g_dasm_str, "cmp.w   %s, D%d", get_ea_mode_str_16(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_cmp_32(void)
{
	sys_sprintf(g_dasm_str, "cmp.l   %s, D%d", get_ea_mode_str_32(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_cmpa_16(void)
{
	sys_sprintf(g_dasm_str, "cmpa.w  %s, A%d", get_ea_mode_str_16(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_cmpa_32(void)
{
	sys_sprintf(g_dasm_str, "cmpa.l  %s, A%d", get_ea_mode_str_32(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_cmpi_8(void)
{
	char* str = get_imm_str_s8();
	sys_sprintf(g_dasm_str, "cmpi.b  %s, %s", str, get_ea_mode_str_8(g_cpu_ir));
}

static void d68020_cmpi_pcdi_8(void)
{
	char* str;
	LIMIT_CPU_TYPES(M68010_PLUS);
	str = get_imm_str_s8();
	sys_sprintf(g_dasm_str, "cmpi.b  %s, %s; (2+)", str, get_ea_mode_str_8(g_cpu_ir));
}

static void d68020_cmpi_pcix_8(void)
{
	char* str;
	LIMIT_CPU_TYPES(M68010_PLUS);
	str = get_imm_str_s8();
	sys_sprintf(g_dasm_str, "cmpi.b  %s, %s; (2+)", str, get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_cmpi_16(void)
{
	char* str;
	str = get_imm_str_s16();
	sys_sprintf(g_dasm_str, "cmpi.w  %s, %s", str, get_ea_mode_str_16(g_cpu_ir));
}

static void d68020_cmpi_pcdi_16(void)
{
	char* str;
	LIMIT_CPU_TYPES(M68010_PLUS);
	str = get_imm_str_s16();
	sys_sprintf(g_dasm_str, "cmpi.w  %s, %s; (2+)", str, get_ea_mode_str_16(g_cpu_ir));
}

static void d68020_cmpi_pcix_16(void)
{
	char* str;
	LIMIT_CPU_TYPES(M68010_PLUS);
	str = get_imm_str_s16();
	sys_sprintf(g_dasm_str, "cmpi.w  %s, %s; (2+)", str, get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_cmpi_32(void)
{
	char* str;
	str = get_imm_str_s32();
	sys_sprintf(g_dasm_str, "cmpi.l  %s, %s", str, get_ea_mode_str_32(g_cpu_ir));
}

static void d68020_cmpi_pcdi_32(void)
{
	char* str;
	LIMIT_CPU_TYPES(M68010_PLUS);
	str = get_imm_str_s32();
	sys_sprintf(g_dasm_str, "cmpi.l  %s, %s; (2+)", str, get_ea_mode_str_32(g_cpu_ir));
}

static void d68020_cmpi_pcix_32(void)
{
	char* str;
	LIMIT_CPU_TYPES(M68010_PLUS);
	str = get_imm_str_s32();
	sys_sprintf(g_dasm_str, "cmpi.l  %s, %s; (2+)", str, get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_cmpm_8(void)
{
	sys_sprintf(g_dasm_str, "cmpm.b  (A%d)+, (A%d)+", g_cpu_ir&7, (g_cpu_ir>>9)&7);
}

static void d68000_cmpm_16(void)
{
	sys_sprintf(g_dasm_str, "cmpm.w  (A%d)+, (A%d)+", g_cpu_ir&7, (g_cpu_ir>>9)&7);
}

static void d68000_cmpm_32(void)
{
	sys_sprintf(g_dasm_str, "cmpm.l  (A%d)+, (A%d)+", g_cpu_ir&7, (g_cpu_ir>>9)&7);
}

static void d68020_cpbcc_16(void)
{
	uint extension;
	uint new_pc = g_cpu_pc;
	LIMIT_CPU_TYPES(M68020_PLUS);
	extension = read_imm_16();
	new_pc += make_int_16(read_imm_16());
	sys_sprintf(g_dasm_str, "%db%-4s  %s; %x (extension = %x) (2-3)", (g_cpu_ir>>9)&7, g_cpcc[g_cpu_ir&0x3f], get_imm_str_s16(), new_pc, extension);
}

static void d68020_cpbcc_32(void)
{
	uint extension;
	uint new_pc = g_cpu_pc;
	LIMIT_CPU_TYPES(M68020_PLUS);
	extension = read_imm_16();
	new_pc += read_imm_32();
	sys_sprintf(g_dasm_str, "%db%-4s  %s; %x (extension = %x) (2-3)", (g_cpu_ir>>9)&7, g_cpcc[g_cpu_ir&0x3f], get_imm_str_s16(), new_pc, extension);
}

static void d68020_cpdbcc(void)
{
	uint extension1;
	uint extension2;
	uint new_pc = g_cpu_pc;
	LIMIT_CPU_TYPES(M68020_PLUS);
	extension1 = read_imm_16();
	extension2 = read_imm_16();
	new_pc += make_int_16(read_imm_16());
	sys_sprintf(g_dasm_str, "%ddb%-4s D%d,%s; %x (extension = %x) (2-3)", (g_cpu_ir>>9)&7, g_cpcc[extension1&0x3f], g_cpu_ir&7, get_imm_str_s16(), new_pc, extension2);
}

static void d68020_cpgen(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "%dgen    %s; (2-3)", (g_cpu_ir>>9)&7, get_imm_str_u32());
}

static void d68020_cprestore(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	if (((g_cpu_ir>>9)&7) == 1)
	{
		sys_sprintf(g_dasm_str, "frestore %s", get_ea_mode_str_8(g_cpu_ir));
	}
	else
	{
		sys_sprintf(g_dasm_str, "%drestore %s; (2-3)", (g_cpu_ir>>9)&7, get_ea_mode_str_8(g_cpu_ir));
	}
}

static void d68020_cpsave(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	if (((g_cpu_ir>>9)&7) == 1)
	{
		sys_sprintf(g_dasm_str, "fsave   %s", get_ea_mode_str_8(g_cpu_ir));
	}
	else
	{
		sys_sprintf(g_dasm_str, "%dsave   %s; (2-3)", (g_cpu_ir>>9)&7, get_ea_mode_str_8(g_cpu_ir));
	}
}

static void d68020_cpscc(void)
{
	uint extension1;
	uint extension2;
	LIMIT_CPU_TYPES(M68020_PLUS);
	extension1 = read_imm_16();
	extension2 = read_imm_16();
	sys_sprintf(g_dasm_str, "%ds%-4s  %s; (extension = %x) (2-3)", (g_cpu_ir>>9)&7, g_cpcc[extension1&0x3f], get_ea_mode_str_8(g_cpu_ir), extension2);
}

static void d68020_cptrapcc_0(void)
{
	uint extension1;
	uint extension2;
	LIMIT_CPU_TYPES(M68020_PLUS);
	extension1 = read_imm_16();
	extension2 = read_imm_16();
	sys_sprintf(g_dasm_str, "%dtrap%-4s; (extension = %x) (2-3)", (g_cpu_ir>>9)&7, g_cpcc[extension1&0x3f], extension2);
}

static void d68020_cptrapcc_16(void)
{
	uint extension1;
	uint extension2;
	LIMIT_CPU_TYPES(M68020_PLUS);
	extension1 = read_imm_16();
	extension2 = read_imm_16();
	sys_sprintf(g_dasm_str, "%dtrap%-4s %s; (extension = %x) (2-3)", (g_cpu_ir>>9)&7, g_cpcc[extension1&0x3f], get_imm_str_u16(), extension2);
}

static void d68020_cptrapcc_32(void)
{
	uint extension1;
	uint extension2;
	LIMIT_CPU_TYPES(M68020_PLUS);
	extension1 = read_imm_16();
	extension2 = read_imm_16();
	sys_sprintf(g_dasm_str, "%dtrap%-4s %s; (extension = %x) (2-3)", (g_cpu_ir>>9)&7, g_cpcc[extension1&0x3f], get_imm_str_u32(), extension2);
}

static void d68040_cpush(void)
{
	LIMIT_CPU_TYPES(M68040_PLUS);
	switch((g_cpu_ir>>3)&3)
	{
		case 0:
			sys_sprintf(g_dasm_str, "cpush (illegal scope); (4)");
			break;
		case 1:
			sys_sprintf(g_dasm_str, "cpushl  %d, (A%d); (4)", (g_cpu_ir>>6)&3, g_cpu_ir&7);
			break;
		case 2:
			sys_sprintf(g_dasm_str, "cpushp  %d, (A%d); (4)", (g_cpu_ir>>6)&3, g_cpu_ir&7);
			break;
		case 3:
			sys_sprintf(g_dasm_str, "cpusha  %d; (4)", (g_cpu_ir>>6)&3);
			break;
	}
}

static void d68000_dbra(void)
{
	uint temp_pc = g_cpu_pc;
	sys_sprintf(g_dasm_str, "dbra    D%d, $%x", g_cpu_ir & 7, temp_pc + make_int_16(read_imm_16()));
	SET_OPCODE_FLAGS(DASMFLAG_STEP_OVER);
}

static void d68000_dbcc(void)
{
	uint temp_pc = g_cpu_pc;
	sys_sprintf(g_dasm_str, "db%-2s    D%d, $%x", g_cc[(g_cpu_ir>>8)&0xf], g_cpu_ir & 7, temp_pc + make_int_16(read_imm_16()));
	SET_OPCODE_FLAGS(DASMFLAG_STEP_OVER);
}

static void d68000_divs(void)
{
	sys_sprintf(g_dasm_str, "divs.w  %s, D%d", get_ea_mode_str_16(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_divu(void)
{
	sys_sprintf(g_dasm_str, "divu.w  %s, D%d", get_ea_mode_str_16(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68020_divl(void)
{
	uint extension;
	LIMIT_CPU_TYPES(M68020_PLUS);
	extension = read_imm_16();

	if(BIT_A(extension))
		sys_sprintf(g_dasm_str, "div%c.l  %s, D%d:D%d; (2+)", BIT_B(extension) ? 's' : 'u', get_ea_mode_str_32(g_cpu_ir), extension&7, (extension>>12)&7);
	else if((extension&7) == ((extension>>12)&7))
		sys_sprintf(g_dasm_str, "div%c.l  %s, D%d; (2+)", BIT_B(extension) ? 's' : 'u', get_ea_mode_str_32(g_cpu_ir), (extension>>12)&7);
	else
		sys_sprintf(g_dasm_str, "div%cl.l %s, D%d:D%d; (2+)", BIT_B(extension) ? 's' : 'u', get_ea_mode_str_32(g_cpu_ir), extension&7, (extension>>12)&7);
}

static void d68000_eor_8(void)
{
	sys_sprintf(g_dasm_str, "eor.b   D%d, %s", (g_cpu_ir>>9)&7, get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_eor_16(void)
{
	sys_sprintf(g_dasm_str, "eor.w   D%d, %s", (g_cpu_ir>>9)&7, get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_eor_32(void)
{
	sys_sprintf(g_dasm_str, "eor.l   D%d, %s", (g_cpu_ir>>9)&7, get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_eori_8(void)
{
	char* str = get_imm_str_u8();
	sys_sprintf(g_dasm_str, "eori.b  %s, %s", str, get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_eori_16(void)
{
	char* str = get_imm_str_u16();
	sys_sprintf(g_dasm_str, "eori.w  %s, %s", str, get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_eori_32(void)
{
	char* str = get_imm_str_u32();
	sys_sprintf(g_dasm_str, "eori.l  %s, %s", str, get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_eori_to_ccr(void)
{
	sys_sprintf(g_dasm_str, "eori    %s, CCR", get_imm_str_u8());
}

static void d68000_eori_to_sr(void)
{
	sys_sprintf(g_dasm_str, "eori    %s, SR", get_imm_str_u16());
}

static void d68000_exg_dd(void)
{
	sys_sprintf(g_dasm_str, "exg     D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_exg_aa(void)
{
	sys_sprintf(g_dasm_str, "exg     A%d, A%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_exg_da(void)
{
	sys_sprintf(g_dasm_str, "exg     D%d, A%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_ext_16(void)
{
	sys_sprintf(g_dasm_str, "ext.w   D%d", g_cpu_ir&7);
}

static void d68000_ext_32(void)
{
	sys_sprintf(g_dasm_str, "ext.l   D%d", g_cpu_ir&7);
}

static void d68020_extb_32(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "extb.l  D%d; (2+)", g_cpu_ir&7);
}

static void d68040_fpu(void)
{
	char float_data_format[8][3] =
	{
		".l", ".s", ".x", ".p", ".w", ".d", ".b", ".p"
	};

	char mnemonic[40];
	uint32 w2, src, dst_reg;
	LIMIT_CPU_TYPES(M68030_PLUS);
	w2 = read_imm_16();

	src = (w2 >> 10) & 0x7;
	dst_reg = (w2 >> 7) & 0x7;

	// special override for FMOVECR
	if ((((w2 >> 13) & 0x7) == 2) && (((w2>>10)&0x7) == 7))
	{
		sys_sprintf(g_dasm_str, "fmovecr   #$%0x, fp%d", (w2&0x7f), dst_reg);
		return;
	}

	switch ((w2 >> 13) & 0x7)
	{
		case 0x0:
		case 0x2:
		{
			switch(w2 & 0x7f)
			{
				case 0x00:	sys_sprintf(mnemonic, "fmove"); break;
				case 0x01:	sys_sprintf(mnemonic, "fint"); break;
				case 0x02:	sys_sprintf(mnemonic, "fsinh"); break;
				case 0x03:	sys_sprintf(mnemonic, "fintrz"); break;
				case 0x04:	sys_sprintf(mnemonic, "fsqrt"); break;
				case 0x06:	sys_sprintf(mnemonic, "flognp1"); break;
				case 0x08:	sys_sprintf(mnemonic, "fetoxm1"); break;
				case 0x09:	sys_sprintf(mnemonic, "ftanh1"); break;
				case 0x0a:	sys_sprintf(mnemonic, "fatan"); break;
				case 0x0c:	sys_sprintf(mnemonic, "fasin"); break;
				case 0x0d:	sys_sprintf(mnemonic, "fatanh"); break;
				case 0x0e:	sys_sprintf(mnemonic, "fsin"); break;
				case 0x0f:	sys_sprintf(mnemonic, "ftan"); break;
				case 0x10:	sys_sprintf(mnemonic, "fetox"); break;
				case 0x11:	sys_sprintf(mnemonic, "ftwotox"); break;
				case 0x12:	sys_sprintf(mnemonic, "ftentox"); break;
				case 0x14:	sys_sprintf(mnemonic, "flogn"); break;
				case 0x15:	sys_sprintf(mnemonic, "flog10"); break;
				case 0x16:	sys_sprintf(mnemonic, "flog2"); break;
				case 0x18:	sys_sprintf(mnemonic, "fabs"); break;
				case 0x19:	sys_sprintf(mnemonic, "fcosh"); break;
				case 0x1a:	sys_sprintf(mnemonic, "fneg"); break;
				case 0x1c:	sys_sprintf(mnemonic, "facos"); break;
				case 0x1d:	sys_sprintf(mnemonic, "fcos"); break;
				case 0x1e:	sys_sprintf(mnemonic, "fgetexp"); break;
				case 0x1f:	sys_sprintf(mnemonic, "fgetman"); break;
				case 0x20:	sys_sprintf(mnemonic, "fdiv"); break;
				case 0x21:	sys_sprintf(mnemonic, "fmod"); break;
				case 0x22:	sys_sprintf(mnemonic, "fadd"); break;
				case 0x23:	sys_sprintf(mnemonic, "fmul"); break;
				case 0x24:	sys_sprintf(mnemonic, "fsgldiv"); break;
				case 0x25:	sys_sprintf(mnemonic, "frem"); break;
				case 0x26:	sys_sprintf(mnemonic, "fscale"); break;
				case 0x27:	sys_sprintf(mnemonic, "fsglmul"); break;
				case 0x28:	sys_sprintf(mnemonic, "fsub"); break;
				case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
							sys_sprintf(mnemonic, "fsincos"); break;
				case 0x38:	sys_sprintf(mnemonic, "fcmp"); break;
				case 0x3a:	sys_sprintf(mnemonic, "ftst"); break;
				case 0x41:	sys_sprintf(mnemonic, "fssqrt"); break;
				case 0x45:	sys_sprintf(mnemonic, "fdsqrt"); break;
				case 0x58:	sys_sprintf(mnemonic, "fsabs"); break;
				case 0x5a:	sys_sprintf(mnemonic, "fsneg"); break;
				case 0x5c:	sys_sprintf(mnemonic, "fdabs"); break;
				case 0x5e:	sys_sprintf(mnemonic, "fdneg"); break;
				case 0x60:	sys_sprintf(mnemonic, "fsdiv"); break;
				case 0x62:	sys_sprintf(mnemonic, "fsadd"); break;
				case 0x63:	sys_sprintf(mnemonic, "fsmul"); break;
				case 0x64:	sys_sprintf(mnemonic, "fddiv"); break;
				case 0x66:	sys_sprintf(mnemonic, "fdadd"); break;
				case 0x67:	sys_sprintf(mnemonic, "fdmul"); break;
				case 0x68:	sys_sprintf(mnemonic, "fssub"); break;
				case 0x6c:	sys_sprintf(mnemonic, "fdsub"); break;

				default:	sys_sprintf(mnemonic, "FPU (?)"); break;
			}

			if (w2 & 0x4000)
			{
				sys_sprintf(g_dasm_str, "%s%s   %s, FP%d", mnemonic, float_data_format[src], get_ea_mode_str_32(g_cpu_ir), dst_reg);
			}
			else
			{
				sys_sprintf(g_dasm_str, "%s.x   FP%d, FP%d", mnemonic, src, dst_reg);
			}
			break;
		}

		case 0x3:
		{
			switch ((w2>>10)&7)
			{
				case 3:		// packed decimal w/fixed k-factor
					sys_sprintf(g_dasm_str, "fmove%s   FP%d, %s {#%d}", float_data_format[(w2>>10)&7], dst_reg, get_ea_mode_str_32(g_cpu_ir), sext_7bit_int(w2&0x7f));
					break;

				case 7:		// packed decimal w/dynamic k-factor (register)
					sys_sprintf(g_dasm_str, "fmove%s   FP%d, %s {D%d}", float_data_format[(w2>>10)&7], dst_reg, get_ea_mode_str_32(g_cpu_ir), (w2>>4)&7);
					break;

				default:
					sys_sprintf(g_dasm_str, "fmove%s   FP%d, %s", float_data_format[(w2>>10)&7], dst_reg, get_ea_mode_str_32(g_cpu_ir));
					break;
			}
			break;
		}

		case 0x4:	// ea to control
		{
			sys_sprintf(g_dasm_str, "fmovem.l   %s, ", get_ea_mode_str_32(g_cpu_ir));
			if (w2 & 0x1000) sys_strcat(g_dasm_str, "fpcr");
			if (w2 & 0x0800) sys_strcat(g_dasm_str, "/fpsr");
			if (w2 & 0x0400) sys_strcat(g_dasm_str, "/fpiar");
			break;
		}

		case 0x5:	// control to ea
		{
			
			sys_strcpy(g_dasm_str, "fmovem.l   ");
			if (w2 & 0x1000) sys_strcat(g_dasm_str, "fpcr");
			if (w2 & 0x0800) sys_strcat(g_dasm_str, "/fpsr");
			if (w2 & 0x0400) sys_strcat(g_dasm_str, "/fpiar");
			sys_strcat(g_dasm_str, ", ");
			sys_strcat(g_dasm_str, get_ea_mode_str_32(g_cpu_ir));
			break;
		}

		case 0x6:	// memory to FPU, list
		{
			char temp[32];

			if ((w2>>11) & 1)	// dynamic register list
			{
				sys_sprintf(g_dasm_str, "fmovem.x   %s, D%d", get_ea_mode_str_32(g_cpu_ir), (w2>>4)&7);
			}
			else	// static register list
			{
				int i;

				sys_sprintf(g_dasm_str, "fmovem.x   %s, ", get_ea_mode_str_32(g_cpu_ir));

				for (i = 0; i < 8; i++)
				{
					if (w2 & (1<<i))
					{
						if ((w2>>12) & 1)	// postincrement or control
						{
							sys_sprintf(temp, "FP%d ", 7-i);
						}
						else			// predecrement
						{
							sys_sprintf(temp, "FP%d ", i);
						}
						sys_strcat(g_dasm_str, temp);
					}
				}
			}
			break;
		}

		case 0x7:	// FPU to memory, list
		{
			char temp[32];

			if ((w2>>11) & 1)	// dynamic register list
			{
				sys_sprintf(g_dasm_str, "fmovem.x   D%d, %s", (w2>>4)&7, get_ea_mode_str_32(g_cpu_ir));
			}
			else	// static register list
			{
				int i;

				sys_sprintf(g_dasm_str, "fmovem.x   ");

				for (i = 0; i < 8; i++)
				{
					if (w2 & (1<<i))
					{
						if ((w2>>12) & 1)	// postincrement or control
						{
							sys_sprintf(temp, "FP%d ", 7-i);
						}
						else			// predecrement
						{
							sys_sprintf(temp, "FP%d ", i);
						}
						sys_strcat(g_dasm_str, temp);
					}
				}

				sys_strcat(g_dasm_str, ", ");
				sys_strcat(g_dasm_str, get_ea_mode_str_32(g_cpu_ir));
			}
			break;
		}

		default:
		{
			sys_sprintf(g_dasm_str, "FPU (?) ");
			break;
		}
	}
}

static void d68000_jmp(void)
{
	sys_sprintf(g_dasm_str, "jmp     %s", get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_jsr(void)
{
	sys_sprintf(g_dasm_str, "jsr     %s", get_ea_mode_str_32(g_cpu_ir));
	SET_OPCODE_FLAGS(DASMFLAG_STEP_OVER);
}

static void d68000_lea(void)
{
	sys_sprintf(g_dasm_str, "lea     %s, A%d", get_ea_mode_str_32(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_link_16(void)
{
	sys_sprintf(g_dasm_str, "link    A%d, %s", g_cpu_ir&7, get_imm_str_s16());
}

static void d68020_link_32(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "link    A%d, %s; (2+)", g_cpu_ir&7, get_imm_str_s32());
}

static void d68000_lsr_s_8(void)
{
	sys_sprintf(g_dasm_str, "lsr.b   #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_lsr_s_16(void)
{
	sys_sprintf(g_dasm_str, "lsr.w   #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_lsr_s_32(void)
{
	sys_sprintf(g_dasm_str, "lsr.l   #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_lsr_r_8(void)
{
	sys_sprintf(g_dasm_str, "lsr.b   D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_lsr_r_16(void)
{
	sys_sprintf(g_dasm_str, "lsr.w   D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_lsr_r_32(void)
{
	sys_sprintf(g_dasm_str, "lsr.l   D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_lsr_ea(void)
{
	sys_sprintf(g_dasm_str, "lsr.w   %s", get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_lsl_s_8(void)
{
	sys_sprintf(g_dasm_str, "lsl.b   #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_lsl_s_16(void)
{
	sys_sprintf(g_dasm_str, "lsl.w   #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_lsl_s_32(void)
{
	sys_sprintf(g_dasm_str, "lsl.l   #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_lsl_r_8(void)
{
	sys_sprintf(g_dasm_str, "lsl.b   D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_lsl_r_16(void)
{
	sys_sprintf(g_dasm_str, "lsl.w   D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_lsl_r_32(void)
{
	sys_sprintf(g_dasm_str, "lsl.l   D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_lsl_ea(void)
{
	sys_sprintf(g_dasm_str, "lsl.w   %s", get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_move_8(void)
{
	char* str = get_ea_mode_str_8(g_cpu_ir);
	sys_sprintf(g_dasm_str, "move.b  %s, %s", str, get_ea_mode_str_8(((g_cpu_ir>>9) & 7) | ((g_cpu_ir>>3) & 0x38)));
}

static void d68000_move_16(void)
{
	char* str = get_ea_mode_str_16(g_cpu_ir);
	sys_sprintf(g_dasm_str, "move.w  %s, %s", str, get_ea_mode_str_16(((g_cpu_ir>>9) & 7) | ((g_cpu_ir>>3) & 0x38)));
}

static void d68000_move_32(void)
{
	char* str = get_ea_mode_str_32(g_cpu_ir);
	sys_sprintf(g_dasm_str, "move.l  %s, %s", str, get_ea_mode_str_32(((g_cpu_ir>>9) & 7) | ((g_cpu_ir>>3) & 0x38)));
}

static void d68000_movea_16(void)
{
	sys_sprintf(g_dasm_str, "movea.w %s, A%d", get_ea_mode_str_16(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_movea_32(void)
{
	sys_sprintf(g_dasm_str, "movea.l %s, A%d", get_ea_mode_str_32(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_move_to_ccr(void)
{
	sys_sprintf(g_dasm_str, "move    %s, CCR", get_ea_mode_str_8(g_cpu_ir));
}

static void d68010_move_fr_ccr(void)
{
	LIMIT_CPU_TYPES(M68010_PLUS);
	sys_sprintf(g_dasm_str, "move    CCR, %s; (1+)", get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_move_fr_sr(void)
{
	sys_sprintf(g_dasm_str, "move    SR, %s", get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_move_to_sr(void)
{
	sys_sprintf(g_dasm_str, "move    %s, SR", get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_move_fr_usp(void)
{
	sys_sprintf(g_dasm_str, "move    USP, A%d", g_cpu_ir&7);
}

static void d68000_move_to_usp(void)
{
	sys_sprintf(g_dasm_str, "move    A%d, USP", g_cpu_ir&7);
}

static void d68010_movec(void)
{
	uint extension;
	char* reg_name;
	char* processor;
	LIMIT_CPU_TYPES(M68010_PLUS);
	extension = read_imm_16();

	switch(extension & 0xfff)
	{
		case 0x000:
			reg_name = "SFC";
			processor = "1+";
			break;
		case 0x001:
			reg_name = "DFC";
			processor = "1+";
			break;
		case 0x800:
			reg_name = "USP";
			processor = "1+";
			break;
		case 0x801:
			reg_name = "VBR";
			processor = "1+";
			break;
		case 0x002:
			reg_name = "CACR";
			processor = "2+";
			break;
		case 0x802:
			reg_name = "CAAR";
			processor = "2,3";
			break;
		case 0x803:
			reg_name = "MSP";
			processor = "2+";
			break;
		case 0x804:
			reg_name = "ISP";
			processor = "2+";
			break;
		case 0x003:
			reg_name = "TC";
			processor = "4+";
			break;
		case 0x004:
			reg_name = "ITT0";
			processor = "4+";
			break;
		case 0x005:
			reg_name = "ITT1";
			processor = "4+";
			break;
		case 0x006:
			reg_name = "DTT0";
			processor = "4+";
			break;
		case 0x007:
			reg_name = "DTT1";
			processor = "4+";
			break;
		case 0x805:
			reg_name = "MMUSR";
			processor = "4+";
			break;
		case 0x806:
			reg_name = "URP";
			processor = "4+";
			break;
		case 0x807:
			reg_name = "SRP";
			processor = "4+";
			break;
		default:
			reg_name = make_signed_hex_str_16(extension & 0xfff);
			processor = "?";
	}

	if(BIT_0(g_cpu_ir))
		sys_sprintf(g_dasm_str, "movec %c%d, %s; (%s)", BIT_F(extension) ? 'A' : 'D', (extension>>12)&7, reg_name, processor);
	else
		sys_sprintf(g_dasm_str, "movec %s, %c%d; (%s)", reg_name, BIT_F(extension) ? 'A' : 'D', (extension>>12)&7, processor);
}

static void d68000_movem_pd_16(void)
{
	uint data = read_imm_16();
	char buffer[40];
	uint first;
	uint run_length;
	uint i;

	buffer[0] = 0;
	for(i=0;i<8;i++)
	{
		if(data&(1<<(15-i)))
		{
			first = i;
			run_length = 0;
			while(i<7 && (data&(1<<(15-(i+1)))))
			{
				i++;
				run_length++;
			}
			if(buffer[0] != 0)
				sys_strcat(buffer, "/");
			sys_sprintf(buffer+sys_strlen(buffer), "D%d", first);
			if(run_length > 0)
				sys_sprintf(buffer+sys_strlen(buffer), "-D%d", first + run_length);
		}
	}
	for(i=0;i<8;i++)
	{
		if(data&(1<<(7-i)))
		{
			first = i;
			run_length = 0;
			while(i<7 && (data&(1<<(7-(i+1)))))
			{
				i++;
				run_length++;
			}
			if(buffer[0] != 0)
				sys_strcat(buffer, "/");
			sys_sprintf(buffer+sys_strlen(buffer), "A%d", first);
			if(run_length > 0)
				sys_sprintf(buffer+sys_strlen(buffer), "-A%d", first + run_length);
		}
	}
	sys_sprintf(g_dasm_str, "movem.w %s, %s", buffer, get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_movem_pd_32(void)
{
	uint data = read_imm_16();
	char buffer[40];
	uint first;
	uint run_length;
	uint i;

	buffer[0] = 0;
	for(i=0;i<8;i++)
	{
		if(data&(1<<(15-i)))
		{
			first = i;
			run_length = 0;
			while(i<7 && (data&(1<<(15-(i+1)))))
			{
				i++;
				run_length++;
			}
			if(buffer[0] != 0)
				sys_strcat(buffer, "/");
			sys_sprintf(buffer+sys_strlen(buffer), "D%d", first);
			if(run_length > 0)
				sys_sprintf(buffer+sys_strlen(buffer), "-D%d", first + run_length);
		}
	}
	for(i=0;i<8;i++)
	{
		if(data&(1<<(7-i)))
		{
			first = i;
			run_length = 0;
			while(i<7 && (data&(1<<(7-(i+1)))))
			{
				i++;
				run_length++;
			}
			if(buffer[0] != 0)
				sys_strcat(buffer, "/");
			sys_sprintf(buffer+sys_strlen(buffer), "A%d", first);
			if(run_length > 0)
				sys_sprintf(buffer+sys_strlen(buffer), "-A%d", first + run_length);
		}
	}
	sys_sprintf(g_dasm_str, "movem.l %s, %s", buffer, get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_movem_er_16(void)
{
	uint data = read_imm_16();
	char buffer[40];
	uint first;
	uint run_length;
	uint i;

	buffer[0] = 0;
	for(i=0;i<8;i++)
	{
		if(data&(1<<i))
		{
			first = i;
			run_length = 0;
			while(i<7 && (data&(1<<(i+1))))
			{
				i++;
				run_length++;
			}
			if(buffer[0] != 0)
				sys_strcat(buffer, "/");
			sys_sprintf(buffer+sys_strlen(buffer), "D%d", first);
			if(run_length > 0)
				sys_sprintf(buffer+sys_strlen(buffer), "-D%d", first + run_length);
		}
	}
	for(i=0;i<8;i++)
	{
		if(data&(1<<(i+8)))
		{
			first = i;
			run_length = 0;
			while(i<7 && (data&(1<<(i+8+1))))
			{
				i++;
				run_length++;
			}
			if(buffer[0] != 0)
				sys_strcat(buffer, "/");
			sys_sprintf(buffer+sys_strlen(buffer), "A%d", first);
			if(run_length > 0)
				sys_sprintf(buffer+sys_strlen(buffer), "-A%d", first + run_length);
		}
	}
	sys_sprintf(g_dasm_str, "movem.w %s, %s", get_ea_mode_str_16(g_cpu_ir), buffer);
}

static void d68000_movem_er_32(void)
{
	uint data = read_imm_16();
	char buffer[40];
	uint first;
	uint run_length;
	uint i;

	buffer[0] = 0;
	for(i=0;i<8;i++)
	{
		if(data&(1<<i))
		{
			first = i;
			run_length = 0;
			while(i<7 && (data&(1<<(i+1))))
			{
				i++;
				run_length++;
			}
			if(buffer[0] != 0)
				sys_strcat(buffer, "/");
			sys_sprintf(buffer+sys_strlen(buffer), "D%d", first);
			if(run_length > 0)
				sys_sprintf(buffer+sys_strlen(buffer), "-D%d", first + run_length);
		}
	}
	for(i=0;i<8;i++)
	{
		if(data&(1<<(i+8)))
		{
			first = i;
			run_length = 0;
			while(i<7 && (data&(1<<(i+8+1))))
			{
				i++;
				run_length++;
			}
			if(buffer[0] != 0)
				sys_strcat(buffer, "/");
			sys_sprintf(buffer+sys_strlen(buffer), "A%d", first);
			if(run_length > 0)
				sys_sprintf(buffer+sys_strlen(buffer), "-A%d", first + run_length);
		}
	}
	sys_sprintf(g_dasm_str, "movem.l %s, %s", get_ea_mode_str_32(g_cpu_ir), buffer);
}

static void d68000_movem_re_16(void)
{
	uint data = read_imm_16();
	char buffer[40];
	uint first;
	uint run_length;
	uint i;

	buffer[0] = 0;
	for(i=0;i<8;i++)
	{
		if(data&(1<<i))
		{
			first = i;
			run_length = 0;
			while(i<7 && (data&(1<<(i+1))))
			{
				i++;
				run_length++;
			}
			if(buffer[0] != 0)
				sys_strcat(buffer, "/");
			sys_sprintf(buffer+sys_strlen(buffer), "D%d", first);
			if(run_length > 0)
				sys_sprintf(buffer+sys_strlen(buffer), "-D%d", first + run_length);
		}
	}
	for(i=0;i<8;i++)
	{
		if(data&(1<<(i+8)))
		{
			first = i;
			run_length = 0;
			while(i<7 && (data&(1<<(i+8+1))))
			{
				i++;
				run_length++;
			}
			if(buffer[0] != 0)
				sys_strcat(buffer, "/");
			sys_sprintf(buffer+sys_strlen(buffer), "A%d", first);
			if(run_length > 0)
				sys_sprintf(buffer+sys_strlen(buffer), "-A%d", first + run_length);
		}
	}
	sys_sprintf(g_dasm_str, "movem.w %s, %s", buffer, get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_movem_re_32(void)
{
	uint data = read_imm_16();
	char buffer[40];
	uint first;
	uint run_length;
	uint i;

	buffer[0] = 0;
	for(i=0;i<8;i++)
	{
		if(data&(1<<i))
		{
			first = i;
			run_length = 0;
			while(i<7 && (data&(1<<(i+1))))
			{
				i++;
				run_length++;
			}
			if(buffer[0] != 0)
				sys_strcat(buffer, "/");
			sys_sprintf(buffer+sys_strlen(buffer), "D%d", first);
			if(run_length > 0)
				sys_sprintf(buffer+sys_strlen(buffer), "-D%d", first + run_length);
		}
	}
	for(i=0;i<8;i++)
	{
		if(data&(1<<(i+8)))
		{
			first = i;
			run_length = 0;
			while(i<7 && (data&(1<<(i+8+1))))
			{
				i++;
				run_length++;
			}
			if(buffer[0] != 0)
				sys_strcat(buffer, "/");
			sys_sprintf(buffer+sys_strlen(buffer), "A%d", first);
			if(run_length > 0)
				sys_sprintf(buffer+sys_strlen(buffer), "-A%d", first + run_length);
		}
	}
	sys_sprintf(g_dasm_str, "movem.l %s, %s", buffer, get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_movep_re_16(void)
{
	sys_sprintf(g_dasm_str, "movep.w D%d, ($%x,A%d)", (g_cpu_ir>>9)&7, read_imm_16(), g_cpu_ir&7);
}

static void d68000_movep_re_32(void)
{
	sys_sprintf(g_dasm_str, "movep.l D%d, ($%x,A%d)", (g_cpu_ir>>9)&7, read_imm_16(), g_cpu_ir&7);
}

static void d68000_movep_er_16(void)
{
	sys_sprintf(g_dasm_str, "movep.w ($%x,A%d), D%d", read_imm_16(), g_cpu_ir&7, (g_cpu_ir>>9)&7);
}

static void d68000_movep_er_32(void)
{
	sys_sprintf(g_dasm_str, "movep.l ($%x,A%d), D%d", read_imm_16(), g_cpu_ir&7, (g_cpu_ir>>9)&7);
}

static void d68010_moves_8(void)
{
	uint extension;
	LIMIT_CPU_TYPES(M68010_PLUS);
	extension = read_imm_16();
	if(BIT_B(extension))
		sys_sprintf(g_dasm_str, "moves.b %c%d, %s; (1+)", BIT_F(extension) ? 'A' : 'D', (extension>>12)&7, get_ea_mode_str_8(g_cpu_ir));
	else
		sys_sprintf(g_dasm_str, "moves.b %s, %c%d; (1+)", get_ea_mode_str_8(g_cpu_ir), BIT_F(extension) ? 'A' : 'D', (extension>>12)&7);
}

static void d68010_moves_16(void)
{
	uint extension;
	LIMIT_CPU_TYPES(M68010_PLUS);
	extension = read_imm_16();
	if(BIT_B(extension))
		sys_sprintf(g_dasm_str, "moves.w %c%d, %s; (1+)", BIT_F(extension) ? 'A' : 'D', (extension>>12)&7, get_ea_mode_str_16(g_cpu_ir));
	else
		sys_sprintf(g_dasm_str, "moves.w %s, %c%d; (1+)", get_ea_mode_str_16(g_cpu_ir), BIT_F(extension) ? 'A' : 'D', (extension>>12)&7);
}

static void d68010_moves_32(void)
{
	uint extension;
	LIMIT_CPU_TYPES(M68010_PLUS);
	extension = read_imm_16();
	if(BIT_B(extension))
		sys_sprintf(g_dasm_str, "moves.l %c%d, %s; (1+)", BIT_F(extension) ? 'A' : 'D', (extension>>12)&7, get_ea_mode_str_32(g_cpu_ir));
	else
		sys_sprintf(g_dasm_str, "moves.l %s, %c%d; (1+)", get_ea_mode_str_32(g_cpu_ir), BIT_F(extension) ? 'A' : 'D', (extension>>12)&7);
}

static void d68000_moveq(void)
{
	sys_sprintf(g_dasm_str, "moveq   #%s, D%d", make_signed_hex_str_8(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68040_move16_pi_pi(void)
{
	LIMIT_CPU_TYPES(M68040_PLUS);
	sys_sprintf(g_dasm_str, "move16  (A%d)+, (A%d)+; (4)", g_cpu_ir&7, (read_imm_16()>>12)&7);
}

static void d68040_move16_pi_al(void)
{
	LIMIT_CPU_TYPES(M68040_PLUS);
	sys_sprintf(g_dasm_str, "move16  (A%d)+, %s; (4)", g_cpu_ir&7, get_imm_str_u32());
}

static void d68040_move16_al_pi(void)
{
	LIMIT_CPU_TYPES(M68040_PLUS);
	sys_sprintf(g_dasm_str, "move16  %s, (A%d)+; (4)", get_imm_str_u32(), g_cpu_ir&7);
}

static void d68040_move16_ai_al(void)
{
	LIMIT_CPU_TYPES(M68040_PLUS);
	sys_sprintf(g_dasm_str, "move16  (A%d), %s; (4)", g_cpu_ir&7, get_imm_str_u32());
}

static void d68040_move16_al_ai(void)
{
	LIMIT_CPU_TYPES(M68040_PLUS);
	sys_sprintf(g_dasm_str, "move16  %s, (A%d); (4)", get_imm_str_u32(), g_cpu_ir&7);
}

static void d68000_muls(void)
{
	sys_sprintf(g_dasm_str, "muls.w  %s, D%d", get_ea_mode_str_16(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_mulu(void)
{
	sys_sprintf(g_dasm_str, "mulu.w  %s, D%d", get_ea_mode_str_16(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68020_mull(void)
{
	uint extension;
	LIMIT_CPU_TYPES(M68020_PLUS);
	extension = read_imm_16();

	if(BIT_A(extension))
		sys_sprintf(g_dasm_str, "mul%c.l %s, D%d:D%d; (2+)", BIT_B(extension) ? 's' : 'u', get_ea_mode_str_32(g_cpu_ir), extension&7, (extension>>12)&7);
	else
		sys_sprintf(g_dasm_str, "mul%c.l  %s, D%d; (2+)", BIT_B(extension) ? 's' : 'u', get_ea_mode_str_32(g_cpu_ir), (extension>>12)&7);
}

static void d68000_nbcd(void)
{
	sys_sprintf(g_dasm_str, "nbcd    %s", get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_neg_8(void)
{
	sys_sprintf(g_dasm_str, "neg.b   %s", get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_neg_16(void)
{
	sys_sprintf(g_dasm_str, "neg.w   %s", get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_neg_32(void)
{
	sys_sprintf(g_dasm_str, "neg.l   %s", get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_negx_8(void)
{
	sys_sprintf(g_dasm_str, "negx.b  %s", get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_negx_16(void)
{
	sys_sprintf(g_dasm_str, "negx.w  %s", get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_negx_32(void)
{
	sys_sprintf(g_dasm_str, "negx.l  %s", get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_nop(void)
{
	sys_sprintf(g_dasm_str, "nop");
}

static void d68000_not_8(void)
{
	sys_sprintf(g_dasm_str, "not.b   %s", get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_not_16(void)
{
	sys_sprintf(g_dasm_str, "not.w   %s", get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_not_32(void)
{
	sys_sprintf(g_dasm_str, "not.l   %s", get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_or_er_8(void)
{
	sys_sprintf(g_dasm_str, "or.b    %s, D%d", get_ea_mode_str_8(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_or_er_16(void)
{
	sys_sprintf(g_dasm_str, "or.w    %s, D%d", get_ea_mode_str_16(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_or_er_32(void)
{
	sys_sprintf(g_dasm_str, "or.l    %s, D%d", get_ea_mode_str_32(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_or_re_8(void)
{
	sys_sprintf(g_dasm_str, "or.b    D%d, %s", (g_cpu_ir>>9)&7, get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_or_re_16(void)
{
	sys_sprintf(g_dasm_str, "or.w    D%d, %s", (g_cpu_ir>>9)&7, get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_or_re_32(void)
{
	sys_sprintf(g_dasm_str, "or.l    D%d, %s", (g_cpu_ir>>9)&7, get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_ori_8(void)
{
	char* str = get_imm_str_u8();
	sys_sprintf(g_dasm_str, "ori.b   %s, %s", str, get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_ori_16(void)
{
	char* str = get_imm_str_u16();
	sys_sprintf(g_dasm_str, "ori.w   %s, %s", str, get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_ori_32(void)
{
	char* str = get_imm_str_u32();
	sys_sprintf(g_dasm_str, "ori.l   %s, %s", str, get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_ori_to_ccr(void)
{
	sys_sprintf(g_dasm_str, "ori     %s, CCR", get_imm_str_u8());
}

static void d68000_ori_to_sr(void)
{
	sys_sprintf(g_dasm_str, "ori     %s, SR", get_imm_str_u16());
}

static void d68020_pack_rr(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "pack    D%d, D%d, %s; (2+)", g_cpu_ir&7, (g_cpu_ir>>9)&7, get_imm_str_u16());
}

static void d68020_pack_mm(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "pack    -(A%d), -(A%d), %s; (2+)", g_cpu_ir&7, (g_cpu_ir>>9)&7, get_imm_str_u16());
}

static void d68000_pea(void)
{
	sys_sprintf(g_dasm_str, "pea     %s", get_ea_mode_str_32(g_cpu_ir));
}

// this is a 68040-specific form of PFLUSH
static void d68040_pflush(void)
{
	LIMIT_CPU_TYPES(M68040_PLUS);

	if (g_cpu_ir & 0x10)
	{
		sys_sprintf(g_dasm_str, "pflusha%s", (g_cpu_ir & 8) ? "" : "n");
	}
	else
	{
		sys_sprintf(g_dasm_str, "pflush%s(A%d)", (g_cpu_ir & 8) ? "" : "n", g_cpu_ir & 7);
	}
}

static void d68000_reset(void)
{
	sys_sprintf(g_dasm_str, "reset");
}

static void d68000_ror_s_8(void)
{
	sys_sprintf(g_dasm_str, "ror.b   #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_ror_s_16(void)
{
	sys_sprintf(g_dasm_str, "ror.w   #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7],g_cpu_ir&7);
}

static void d68000_ror_s_32(void)
{
	sys_sprintf(g_dasm_str, "ror.l   #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_ror_r_8(void)
{
	sys_sprintf(g_dasm_str, "ror.b   D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_ror_r_16(void)
{
	sys_sprintf(g_dasm_str, "ror.w   D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_ror_r_32(void)
{
	sys_sprintf(g_dasm_str, "ror.l   D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_ror_ea(void)
{
	sys_sprintf(g_dasm_str, "ror.w   %s", get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_rol_s_8(void)
{
	sys_sprintf(g_dasm_str, "rol.b   #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_rol_s_16(void)
{
	sys_sprintf(g_dasm_str, "rol.w   #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_rol_s_32(void)
{
	sys_sprintf(g_dasm_str, "rol.l   #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_rol_r_8(void)
{
	sys_sprintf(g_dasm_str, "rol.b   D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_rol_r_16(void)
{
	sys_sprintf(g_dasm_str, "rol.w   D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_rol_r_32(void)
{
	sys_sprintf(g_dasm_str, "rol.l   D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_rol_ea(void)
{
	sys_sprintf(g_dasm_str, "rol.w   %s", get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_roxr_s_8(void)
{
	sys_sprintf(g_dasm_str, "roxr.b  #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_roxr_s_16(void)
{
	sys_sprintf(g_dasm_str, "roxr.w  #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}


static void d68000_roxr_s_32(void)
{
	sys_sprintf(g_dasm_str, "roxr.l  #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_roxr_r_8(void)
{
	sys_sprintf(g_dasm_str, "roxr.b  D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_roxr_r_16(void)
{
	sys_sprintf(g_dasm_str, "roxr.w  D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_roxr_r_32(void)
{
	sys_sprintf(g_dasm_str, "roxr.l  D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_roxr_ea(void)
{
	sys_sprintf(g_dasm_str, "roxr.w  %s", get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_roxl_s_8(void)
{
	sys_sprintf(g_dasm_str, "roxl.b  #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_roxl_s_16(void)
{
	sys_sprintf(g_dasm_str, "roxl.w  #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_roxl_s_32(void)
{
	sys_sprintf(g_dasm_str, "roxl.l  #%d, D%d", g_3bit_qdata_table[(g_cpu_ir>>9)&7], g_cpu_ir&7);
}

static void d68000_roxl_r_8(void)
{
	sys_sprintf(g_dasm_str, "roxl.b  D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_roxl_r_16(void)
{
	sys_sprintf(g_dasm_str, "roxl.w  D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_roxl_r_32(void)
{
	sys_sprintf(g_dasm_str, "roxl.l  D%d, D%d", (g_cpu_ir>>9)&7, g_cpu_ir&7);
}

static void d68000_roxl_ea(void)
{
	sys_sprintf(g_dasm_str, "roxl.w  %s", get_ea_mode_str_32(g_cpu_ir));
}

static void d68010_rtd(void)
{
	LIMIT_CPU_TYPES(M68010_PLUS);
	sys_sprintf(g_dasm_str, "rtd     %s; (1+)", get_imm_str_s16());
	SET_OPCODE_FLAGS(DASMFLAG_STEP_OUT);
}

static void d68000_rte(void)
{
	sys_sprintf(g_dasm_str, "rte");
	SET_OPCODE_FLAGS(DASMFLAG_STEP_OUT);
}

static void d68020_rtm(void)
{
	LIMIT_CPU_TYPES(M68020_ONLY);
	sys_sprintf(g_dasm_str, "rtm     %c%d; (2+)", BIT_3(g_cpu_ir) ? 'A' : 'D', g_cpu_ir&7);
	SET_OPCODE_FLAGS(DASMFLAG_STEP_OUT);
}

static void d68000_rtr(void)
{
	sys_sprintf(g_dasm_str, "rtr");
	SET_OPCODE_FLAGS(DASMFLAG_STEP_OUT);
}

static void d68000_rts(void)
{
	sys_sprintf(g_dasm_str, "rts");
	SET_OPCODE_FLAGS(DASMFLAG_STEP_OUT);
}

static void d68000_sbcd_rr(void)
{
	sys_sprintf(g_dasm_str, "sbcd    D%d, D%d", g_cpu_ir&7, (g_cpu_ir>>9)&7);
}

static void d68000_sbcd_mm(void)
{
	sys_sprintf(g_dasm_str, "sbcd    -(A%d), -(A%d)", g_cpu_ir&7, (g_cpu_ir>>9)&7);
}

static void d68000_scc(void)
{
	sys_sprintf(g_dasm_str, "s%-2s     %s", g_cc[(g_cpu_ir>>8)&0xf], get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_stop(void)
{
	sys_sprintf(g_dasm_str, "stop    %s", get_imm_str_s16());
}

static void d68000_sub_er_8(void)
{
	sys_sprintf(g_dasm_str, "sub.b   %s, D%d", get_ea_mode_str_8(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_sub_er_16(void)
{
	sys_sprintf(g_dasm_str, "sub.w   %s, D%d", get_ea_mode_str_16(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_sub_er_32(void)
{
	sys_sprintf(g_dasm_str, "sub.l   %s, D%d", get_ea_mode_str_32(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_sub_re_8(void)
{
	sys_sprintf(g_dasm_str, "sub.b   D%d, %s", (g_cpu_ir>>9)&7, get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_sub_re_16(void)
{
	sys_sprintf(g_dasm_str, "sub.w   D%d, %s", (g_cpu_ir>>9)&7, get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_sub_re_32(void)
{
	sys_sprintf(g_dasm_str, "sub.l   D%d, %s", (g_cpu_ir>>9)&7, get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_suba_16(void)
{
	sys_sprintf(g_dasm_str, "suba.w  %s, A%d", get_ea_mode_str_16(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_suba_32(void)
{
	sys_sprintf(g_dasm_str, "suba.l  %s, A%d", get_ea_mode_str_32(g_cpu_ir), (g_cpu_ir>>9)&7);
}

static void d68000_subi_8(void)
{
	char* str = get_imm_str_s8();
	sys_sprintf(g_dasm_str, "subi.b  %s, %s", str, get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_subi_16(void)
{
	char* str = get_imm_str_s16();
	sys_sprintf(g_dasm_str, "subi.w  %s, %s", str, get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_subi_32(void)
{
	char* str = get_imm_str_s32();
	sys_sprintf(g_dasm_str, "subi.l  %s, %s", str, get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_subq_8(void)
{
	sys_sprintf(g_dasm_str, "subq.b  #%d, %s", g_3bit_qdata_table[(g_cpu_ir>>9)&7], get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_subq_16(void)
{
	sys_sprintf(g_dasm_str, "subq.w  #%d, %s", g_3bit_qdata_table[(g_cpu_ir>>9)&7], get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_subq_32(void)
{
	sys_sprintf(g_dasm_str, "subq.l  #%d, %s", g_3bit_qdata_table[(g_cpu_ir>>9)&7], get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_subx_rr_8(void)
{
	sys_sprintf(g_dasm_str, "subx.b  D%d, D%d", g_cpu_ir&7, (g_cpu_ir>>9)&7);
}

static void d68000_subx_rr_16(void)
{
	sys_sprintf(g_dasm_str, "subx.w  D%d, D%d", g_cpu_ir&7, (g_cpu_ir>>9)&7);
}

static void d68000_subx_rr_32(void)
{
	sys_sprintf(g_dasm_str, "subx.l  D%d, D%d", g_cpu_ir&7, (g_cpu_ir>>9)&7);
}

static void d68000_subx_mm_8(void)
{
	sys_sprintf(g_dasm_str, "subx.b  -(A%d), -(A%d)", g_cpu_ir&7, (g_cpu_ir>>9)&7);
}

static void d68000_subx_mm_16(void)
{
	sys_sprintf(g_dasm_str, "subx.w  -(A%d), -(A%d)", g_cpu_ir&7, (g_cpu_ir>>9)&7);
}

static void d68000_subx_mm_32(void)
{
	sys_sprintf(g_dasm_str, "subx.l  -(A%d), -(A%d)", g_cpu_ir&7, (g_cpu_ir>>9)&7);
}

static void d68000_swap(void)
{
	sys_sprintf(g_dasm_str, "swap    D%d", g_cpu_ir&7);
}

static void d68000_tas(void)
{
	sys_sprintf(g_dasm_str, "tas     %s", get_ea_mode_str_8(g_cpu_ir));
}

extern char *trapName(uint16_t trap, uint16_t *selector, int follow);

static void d68000_trap(void)
{
   if ((g_cpu_ir&0xf) == 0xf) {
     uint16_t trap = read_imm_16();
     uint16_t selector;
     char *name = trapName(trap, &selector, 0);
     if (selector != 0xffff) {
       char *sname = trapName(trap, &selector, 1);
       sys_sprintf(g_dasm_str, "trap    #$%x (0x%04X, %s, %d, %s)", g_cpu_ir&0xf, trap, name, selector, sname);
     } else {
       sys_sprintf(g_dasm_str, "trap    #$%x (0x%04X, %s)", g_cpu_ir&0xf, trap, name);
     }
   } else {
     sys_sprintf(g_dasm_str, "trap    #$%x", g_cpu_ir&0xf);
   }
}

static void d68020_trapcc_0(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "trap%-2s; (2+)", g_cc[(g_cpu_ir>>8)&0xf]);
	SET_OPCODE_FLAGS(DASMFLAG_STEP_OVER);
}

static void d68020_trapcc_16(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "trap%-2s  %s; (2+)", g_cc[(g_cpu_ir>>8)&0xf], get_imm_str_u16());
	SET_OPCODE_FLAGS(DASMFLAG_STEP_OVER);
}

static void d68020_trapcc_32(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "trap%-2s  %s; (2+)", g_cc[(g_cpu_ir>>8)&0xf], get_imm_str_u32());
	SET_OPCODE_FLAGS(DASMFLAG_STEP_OVER);
}

static void d68000_trapv(void)
{
	sys_sprintf(g_dasm_str, "trapv");
	SET_OPCODE_FLAGS(DASMFLAG_STEP_OVER);
}

static void d68000_tst_8(void)
{
	sys_sprintf(g_dasm_str, "tst.b   %s", get_ea_mode_str_8(g_cpu_ir));
}

static void d68020_tst_pcdi_8(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "tst.b   %s; (2+)", get_ea_mode_str_8(g_cpu_ir));
}

static void d68020_tst_pcix_8(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "tst.b   %s; (2+)", get_ea_mode_str_8(g_cpu_ir));
}

static void d68020_tst_i_8(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "tst.b   %s; (2+)", get_ea_mode_str_8(g_cpu_ir));
}

static void d68000_tst_16(void)
{
	sys_sprintf(g_dasm_str, "tst.w   %s", get_ea_mode_str_16(g_cpu_ir));
}

static void d68020_tst_a_16(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "tst.w   %s; (2+)", get_ea_mode_str_16(g_cpu_ir));
}

static void d68020_tst_pcdi_16(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "tst.w   %s; (2+)", get_ea_mode_str_16(g_cpu_ir));
}

static void d68020_tst_pcix_16(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "tst.w   %s; (2+)", get_ea_mode_str_16(g_cpu_ir));
}

static void d68020_tst_i_16(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "tst.w   %s; (2+)", get_ea_mode_str_16(g_cpu_ir));
}

static void d68000_tst_32(void)
{
	sys_sprintf(g_dasm_str, "tst.l   %s", get_ea_mode_str_32(g_cpu_ir));
}

static void d68020_tst_a_32(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "tst.l   %s; (2+)", get_ea_mode_str_32(g_cpu_ir));
}

static void d68020_tst_pcdi_32(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "tst.l   %s; (2+)", get_ea_mode_str_32(g_cpu_ir));
}

static void d68020_tst_pcix_32(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "tst.l   %s; (2+)", get_ea_mode_str_32(g_cpu_ir));
}

static void d68020_tst_i_32(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "tst.l   %s; (2+)", get_ea_mode_str_32(g_cpu_ir));
}

static void d68000_unlk(void)
{
	sys_sprintf(g_dasm_str, "unlk    A%d", g_cpu_ir&7);
}

static void d68020_unpk_rr(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "unpk    D%d, D%d, %s; (2+)", g_cpu_ir&7, (g_cpu_ir>>9)&7, get_imm_str_u16());
}

static void d68020_unpk_mm(void)
{
	LIMIT_CPU_TYPES(M68020_PLUS);
	sys_sprintf(g_dasm_str, "unpk    -(A%d), -(A%d), %s; (2+)", g_cpu_ir&7, (g_cpu_ir>>9)&7, get_imm_str_u16());
}


// PFLUSH:  001xxx0xxxxxxxxx
// PLOAD:   001000x0000xxxxx
// PVALID1: 0010100000000000
// PVALID2: 0010110000000xxx
// PMOVE 1: 010xxxx000000000
// PMOVE 2: 011xxxx0000xxx00
// PMOVE 3: 011xxxx000000000
// PTEST:   100xxxxxxxxxxxxx
// PFLUSHR:  1010000000000000
static void d68851_p000(void)
{
	char* str;
	uint modes = read_imm_16();

	// do this after fetching the second PMOVE word so we properly get the 3rd if necessary
	str = get_ea_mode_str_32(g_cpu_ir);

	if ((modes & 0xfde0) == 0x2000)	// PLOAD
	{
		if (modes & 0x0200)
		{
	 		sys_sprintf(g_dasm_str, "pload  #%d, %s", (modes>>10)&7, str);
		}
		else
		{
	 		sys_sprintf(g_dasm_str, "pload  %s, #%d", str, (modes>>10)&7);
		}
		return;
	}

	if ((modes & 0xe200) == 0x2000)	// PFLUSH
	{
		sys_sprintf(g_dasm_str, "pflushr %x, %x, %s", modes & 0x1f, (modes>>5)&0xf, str);
		return;
	}

	if (modes == 0xa000)	// PFLUSHR
	{
		sys_sprintf(g_dasm_str, "pflushr %s", str);
	}

	if (modes == 0x2800)	// PVALID (FORMAT 1)
	{
		sys_sprintf(g_dasm_str, "pvalid VAL, %s", str);
		return;
	}

	if ((modes & 0xfff8) == 0x2c00)	// PVALID (FORMAT 2)
	{
		sys_sprintf(g_dasm_str, "pvalid A%d, %s", modes & 0xf, str);
		return;
	}

	if ((modes & 0xe000) == 0x8000)	// PTEST
	{
		sys_sprintf(g_dasm_str, "ptest #%d, %s", modes & 0x1f, str);
		return;
	}

	switch ((modes>>13) & 0x7)
	{
		case 0:	// MC68030/040 form with FD bit
		case 2:	// MC68881 form, FD never set
			if (modes & 0x0100)
			{
				if (modes & 0x0200)
				{
			 		sys_sprintf(g_dasm_str, "pmovefd  %s, %s", g_mmuregs[(modes>>10)&7], str);
				}
				else
				{
			 		sys_sprintf(g_dasm_str, "pmovefd  %s, %s", str, g_mmuregs[(modes>>10)&7]);
				}
			}
			else
			{
				if (modes & 0x0200)
				{
			 		sys_sprintf(g_dasm_str, "pmove  %s, %s", g_mmuregs[(modes>>10)&7], str);
				}
				else
				{
			 		sys_sprintf(g_dasm_str, "pmove  %s, %s", str, g_mmuregs[(modes>>10)&7]);
				}
			}
			break;

		case 3:	// MC68030 to/from status reg
			if (modes & 0x0200)
			{
		 		sys_sprintf(g_dasm_str, "pmove  mmusr, %s", str);
			}
			else
			{
		 		sys_sprintf(g_dasm_str, "pmove  %s, mmusr", str);
			}
			break;

		default:
			sys_sprintf(g_dasm_str, "pmove [unknown form] %s", str);
			break;
	}
}

static void d68851_pbcc16(void)
{
	uint32 temp_pc = g_cpu_pc;

	sys_sprintf(g_dasm_str, "pb%s %x", g_mmucond[g_cpu_ir&0xf], temp_pc + make_int_16(read_imm_16()));
}

static void d68851_pbcc32(void)
{
	uint32 temp_pc = g_cpu_pc;

	sys_sprintf(g_dasm_str, "pb%s %x", g_mmucond[g_cpu_ir&0xf], temp_pc + make_int_32(read_imm_32()));
}

static void d68851_pdbcc(void)
{
	uint32 temp_pc = g_cpu_pc;
	uint16 modes = read_imm_16();

	sys_sprintf(g_dasm_str, "pb%s %x", g_mmucond[modes&0xf], temp_pc + make_int_16(read_imm_16()));
}

// PScc:  0000000000xxxxxx
static void d68851_p001(void)
{
	sys_sprintf(g_dasm_str, "MMU 001 group");
}

/* ======================================================================== */
/* ======================= INSTRUCTION TABLE BUILDER ====================== */
/* ======================================================================== */

/* EA Masks:
800 = data register direct
400 = address register direct
200 = address register indirect
100 = ARI postincrement
 80 = ARI pre-decrement
 40 = ARI displacement
 20 = ARI index
 10 = absolute short
  8 = absolute long
  4 = immediate / sr
  2 = pc displacement
  1 = pc idx
*/

static const opcode_struct g_opcode_info[] =
{
/*  opcode handler             mask    match   ea mask */
	{d68000_1010         , 0xf000, 0xa000, 0x000},
	{d68000_1111         , 0xf000, 0xf000, 0x000},
	{d68000_abcd_rr      , 0xf1f8, 0xc100, 0x000},
	{d68000_abcd_mm      , 0xf1f8, 0xc108, 0x000},
	{d68000_add_er_8     , 0xf1c0, 0xd000, 0xbff},
	{d68000_add_er_16    , 0xf1c0, 0xd040, 0xfff},
	{d68000_add_er_32    , 0xf1c0, 0xd080, 0xfff},
	{d68000_add_re_8     , 0xf1c0, 0xd100, 0x3f8},
	{d68000_add_re_16    , 0xf1c0, 0xd140, 0x3f8},
	{d68000_add_re_32    , 0xf1c0, 0xd180, 0x3f8},
	{d68000_adda_16      , 0xf1c0, 0xd0c0, 0xfff},
	{d68000_adda_32      , 0xf1c0, 0xd1c0, 0xfff},
	{d68000_addi_8       , 0xffc0, 0x0600, 0xbf8},
	{d68000_addi_16      , 0xffc0, 0x0640, 0xbf8},
	{d68000_addi_32      , 0xffc0, 0x0680, 0xbf8},
	{d68000_addq_8       , 0xf1c0, 0x5000, 0xbf8},
	{d68000_addq_16      , 0xf1c0, 0x5040, 0xff8},
	{d68000_addq_32      , 0xf1c0, 0x5080, 0xff8},
	{d68000_addx_rr_8    , 0xf1f8, 0xd100, 0x000},
	{d68000_addx_rr_16   , 0xf1f8, 0xd140, 0x000},
	{d68000_addx_rr_32   , 0xf1f8, 0xd180, 0x000},
	{d68000_addx_mm_8    , 0xf1f8, 0xd108, 0x000},
	{d68000_addx_mm_16   , 0xf1f8, 0xd148, 0x000},
	{d68000_addx_mm_32   , 0xf1f8, 0xd188, 0x000},
	{d68000_and_er_8     , 0xf1c0, 0xc000, 0xbff},
	{d68000_and_er_16    , 0xf1c0, 0xc040, 0xbff},
	{d68000_and_er_32    , 0xf1c0, 0xc080, 0xbff},
	{d68000_and_re_8     , 0xf1c0, 0xc100, 0x3f8},
	{d68000_and_re_16    , 0xf1c0, 0xc140, 0x3f8},
	{d68000_and_re_32    , 0xf1c0, 0xc180, 0x3f8},
	{d68000_andi_to_ccr  , 0xffff, 0x023c, 0x000},
	{d68000_andi_to_sr   , 0xffff, 0x027c, 0x000},
	{d68000_andi_8       , 0xffc0, 0x0200, 0xbf8},
	{d68000_andi_16      , 0xffc0, 0x0240, 0xbf8},
	{d68000_andi_32      , 0xffc0, 0x0280, 0xbf8},
	{d68000_asr_s_8      , 0xf1f8, 0xe000, 0x000},
	{d68000_asr_s_16     , 0xf1f8, 0xe040, 0x000},
	{d68000_asr_s_32     , 0xf1f8, 0xe080, 0x000},
	{d68000_asr_r_8      , 0xf1f8, 0xe020, 0x000},
	{d68000_asr_r_16     , 0xf1f8, 0xe060, 0x000},
	{d68000_asr_r_32     , 0xf1f8, 0xe0a0, 0x000},
	{d68000_asr_ea       , 0xffc0, 0xe0c0, 0x3f8},
	{d68000_asl_s_8      , 0xf1f8, 0xe100, 0x000},
	{d68000_asl_s_16     , 0xf1f8, 0xe140, 0x000},
	{d68000_asl_s_32     , 0xf1f8, 0xe180, 0x000},
	{d68000_asl_r_8      , 0xf1f8, 0xe120, 0x000},
	{d68000_asl_r_16     , 0xf1f8, 0xe160, 0x000},
	{d68000_asl_r_32     , 0xf1f8, 0xe1a0, 0x000},
	{d68000_asl_ea       , 0xffc0, 0xe1c0, 0x3f8},
	{d68000_bcc_8        , 0xf000, 0x6000, 0x000},
	{d68000_bcc_16       , 0xf0ff, 0x6000, 0x000},
	{d68020_bcc_32       , 0xf0ff, 0x60ff, 0x000},
	{d68000_bchg_r       , 0xf1c0, 0x0140, 0xbf8},
	{d68000_bchg_s       , 0xffc0, 0x0840, 0xbf8},
	{d68000_bclr_r       , 0xf1c0, 0x0180, 0xbf8},
	{d68000_bclr_s       , 0xffc0, 0x0880, 0xbf8},
	{d68020_bfchg        , 0xffc0, 0xeac0, 0xa78},
	{d68020_bfclr        , 0xffc0, 0xecc0, 0xa78},
	{d68020_bfexts       , 0xffc0, 0xebc0, 0xa7b},
	{d68020_bfextu       , 0xffc0, 0xe9c0, 0xa7b},
	{d68020_bfffo        , 0xffc0, 0xedc0, 0xa7b},
	{d68020_bfins        , 0xffc0, 0xefc0, 0xa78},
	{d68020_bfset        , 0xffc0, 0xeec0, 0xa78},
	{d68020_bftst        , 0xffc0, 0xe8c0, 0xa7b},
	{d68010_bkpt         , 0xfff8, 0x4848, 0x000},
	{d68000_bra_8        , 0xff00, 0x6000, 0x000},
	{d68000_bra_16       , 0xffff, 0x6000, 0x000},
	{d68020_bra_32       , 0xffff, 0x60ff, 0x000},
	{d68000_bset_r       , 0xf1c0, 0x01c0, 0xbf8},
	{d68000_bset_s       , 0xffc0, 0x08c0, 0xbf8},
	{d68000_bsr_8        , 0xff00, 0x6100, 0x000},
	{d68000_bsr_16       , 0xffff, 0x6100, 0x000},
	{d68020_bsr_32       , 0xffff, 0x61ff, 0x000},
	{d68000_btst_r       , 0xf1c0, 0x0100, 0xbff},
	{d68000_btst_s       , 0xffc0, 0x0800, 0xbfb},
	{d68020_callm        , 0xffc0, 0x06c0, 0x27b},
	{d68020_cas_8        , 0xffc0, 0x0ac0, 0x3f8},
	{d68020_cas_16       , 0xffc0, 0x0cc0, 0x3f8},
	{d68020_cas_32       , 0xffc0, 0x0ec0, 0x3f8},
	{d68020_cas2_16      , 0xffff, 0x0cfc, 0x000},
	{d68020_cas2_32      , 0xffff, 0x0efc, 0x000},
	{d68000_chk_16       , 0xf1c0, 0x4180, 0xbff},
	{d68020_chk_32       , 0xf1c0, 0x4100, 0xbff},
	{d68020_chk2_cmp2_8  , 0xffc0, 0x00c0, 0x27b},
	{d68020_chk2_cmp2_16 , 0xffc0, 0x02c0, 0x27b},
	{d68020_chk2_cmp2_32 , 0xffc0, 0x04c0, 0x27b},
	{d68040_cinv         , 0xff20, 0xf400, 0x000},
	{d68000_clr_8        , 0xffc0, 0x4200, 0xbf8},
	{d68000_clr_16       , 0xffc0, 0x4240, 0xbf8},
	{d68000_clr_32       , 0xffc0, 0x4280, 0xbf8},
	{d68000_cmp_8        , 0xf1c0, 0xb000, 0xbff},
	{d68000_cmp_16       , 0xf1c0, 0xb040, 0xfff},
	{d68000_cmp_32       , 0xf1c0, 0xb080, 0xfff},
	{d68000_cmpa_16      , 0xf1c0, 0xb0c0, 0xfff},
	{d68000_cmpa_32      , 0xf1c0, 0xb1c0, 0xfff},
	{d68000_cmpi_8       , 0xffc0, 0x0c00, 0xbf8},
	{d68020_cmpi_pcdi_8  , 0xffff, 0x0c3a, 0x000},
	{d68020_cmpi_pcix_8  , 0xffff, 0x0c3b, 0x000},
	{d68000_cmpi_16      , 0xffc0, 0x0c40, 0xbf8},
	{d68020_cmpi_pcdi_16 , 0xffff, 0x0c7a, 0x000},
	{d68020_cmpi_pcix_16 , 0xffff, 0x0c7b, 0x000},
	{d68000_cmpi_32      , 0xffc0, 0x0c80, 0xbf8},
	{d68020_cmpi_pcdi_32 , 0xffff, 0x0cba, 0x000},
	{d68020_cmpi_pcix_32 , 0xffff, 0x0cbb, 0x000},
	{d68000_cmpm_8       , 0xf1f8, 0xb108, 0x000},
	{d68000_cmpm_16      , 0xf1f8, 0xb148, 0x000},
	{d68000_cmpm_32      , 0xf1f8, 0xb188, 0x000},
	{d68020_cpbcc_16     , 0xf1c0, 0xf080, 0x000},
	{d68020_cpbcc_32     , 0xf1c0, 0xf0c0, 0x000},
	{d68020_cpdbcc       , 0xf1f8, 0xf048, 0x000},
	{d68020_cpgen        , 0xf1c0, 0xf000, 0x000},
	{d68020_cprestore    , 0xf1c0, 0xf140, 0x37f},
	{d68020_cpsave       , 0xf1c0, 0xf100, 0x2f8},
	{d68020_cpscc        , 0xf1c0, 0xf040, 0xbf8},
	{d68020_cptrapcc_0   , 0xf1ff, 0xf07c, 0x000},
	{d68020_cptrapcc_16  , 0xf1ff, 0xf07a, 0x000},
	{d68020_cptrapcc_32  , 0xf1ff, 0xf07b, 0x000},
	{d68040_cpush        , 0xff20, 0xf420, 0x000},
	{d68000_dbcc         , 0xf0f8, 0x50c8, 0x000},
	{d68000_dbra         , 0xfff8, 0x51c8, 0x000},
	{d68000_divs         , 0xf1c0, 0x81c0, 0xbff},
	{d68000_divu         , 0xf1c0, 0x80c0, 0xbff},
	{d68020_divl         , 0xffc0, 0x4c40, 0xbff},
	{d68000_eor_8        , 0xf1c0, 0xb100, 0xbf8},
	{d68000_eor_16       , 0xf1c0, 0xb140, 0xbf8},
	{d68000_eor_32       , 0xf1c0, 0xb180, 0xbf8},
	{d68000_eori_to_ccr  , 0xffff, 0x0a3c, 0x000},
	{d68000_eori_to_sr   , 0xffff, 0x0a7c, 0x000},
	{d68000_eori_8       , 0xffc0, 0x0a00, 0xbf8},
	{d68000_eori_16      , 0xffc0, 0x0a40, 0xbf8},
	{d68000_eori_32      , 0xffc0, 0x0a80, 0xbf8},
	{d68000_exg_dd       , 0xf1f8, 0xc140, 0x000},
	{d68000_exg_aa       , 0xf1f8, 0xc148, 0x000},
	{d68000_exg_da       , 0xf1f8, 0xc188, 0x000},
	{d68020_extb_32      , 0xfff8, 0x49c0, 0x000},
	{d68000_ext_16       , 0xfff8, 0x4880, 0x000},
	{d68000_ext_32       , 0xfff8, 0x48c0, 0x000},
	{d68040_fpu          , 0xffc0, 0xf200, 0x000},
	{d68000_illegal      , 0xffff, 0x4afc, 0x000},
	{d68000_jmp          , 0xffc0, 0x4ec0, 0x27b},
	{d68000_jsr          , 0xffc0, 0x4e80, 0x27b},
	{d68000_lea          , 0xf1c0, 0x41c0, 0x27b},
	{d68000_link_16      , 0xfff8, 0x4e50, 0x000},
	{d68020_link_32      , 0xfff8, 0x4808, 0x000},
	{d68000_lsr_s_8      , 0xf1f8, 0xe008, 0x000},
	{d68000_lsr_s_16     , 0xf1f8, 0xe048, 0x000},
	{d68000_lsr_s_32     , 0xf1f8, 0xe088, 0x000},
	{d68000_lsr_r_8      , 0xf1f8, 0xe028, 0x000},
	{d68000_lsr_r_16     , 0xf1f8, 0xe068, 0x000},
	{d68000_lsr_r_32     , 0xf1f8, 0xe0a8, 0x000},
	{d68000_lsr_ea       , 0xffc0, 0xe2c0, 0x3f8},
	{d68000_lsl_s_8      , 0xf1f8, 0xe108, 0x000},
	{d68000_lsl_s_16     , 0xf1f8, 0xe148, 0x000},
	{d68000_lsl_s_32     , 0xf1f8, 0xe188, 0x000},
	{d68000_lsl_r_8      , 0xf1f8, 0xe128, 0x000},
	{d68000_lsl_r_16     , 0xf1f8, 0xe168, 0x000},
	{d68000_lsl_r_32     , 0xf1f8, 0xe1a8, 0x000},
	{d68000_lsl_ea       , 0xffc0, 0xe3c0, 0x3f8},
	{d68000_move_8       , 0xf000, 0x1000, 0xbff},
	{d68000_move_16      , 0xf000, 0x3000, 0xfff},
	{d68000_move_32      , 0xf000, 0x2000, 0xfff},
	{d68000_movea_16     , 0xf1c0, 0x3040, 0xfff},
	{d68000_movea_32     , 0xf1c0, 0x2040, 0xfff},
	{d68000_move_to_ccr  , 0xffc0, 0x44c0, 0xbff},
	{d68010_move_fr_ccr  , 0xffc0, 0x42c0, 0xbf8},
	{d68000_move_to_sr   , 0xffc0, 0x46c0, 0xbff},
	{d68000_move_fr_sr   , 0xffc0, 0x40c0, 0xbf8},
	{d68000_move_to_usp  , 0xfff8, 0x4e60, 0x000},
	{d68000_move_fr_usp  , 0xfff8, 0x4e68, 0x000},
	{d68010_movec        , 0xfffe, 0x4e7a, 0x000},
	{d68000_movem_pd_16  , 0xfff8, 0x48a0, 0x000},
	{d68000_movem_pd_32  , 0xfff8, 0x48e0, 0x000},
	{d68000_movem_re_16  , 0xffc0, 0x4880, 0x2f8},
	{d68000_movem_re_32  , 0xffc0, 0x48c0, 0x2f8},
	{d68000_movem_er_16  , 0xffc0, 0x4c80, 0x37b},
	{d68000_movem_er_32  , 0xffc0, 0x4cc0, 0x37b},
	{d68000_movep_er_16  , 0xf1f8, 0x0108, 0x000},
	{d68000_movep_er_32  , 0xf1f8, 0x0148, 0x000},
	{d68000_movep_re_16  , 0xf1f8, 0x0188, 0x000},
	{d68000_movep_re_32  , 0xf1f8, 0x01c8, 0x000},
	{d68010_moves_8      , 0xffc0, 0x0e00, 0x3f8},
	{d68010_moves_16     , 0xffc0, 0x0e40, 0x3f8},
	{d68010_moves_32     , 0xffc0, 0x0e80, 0x3f8},
	{d68000_moveq        , 0xf100, 0x7000, 0x000},
	{d68040_move16_pi_pi , 0xfff8, 0xf620, 0x000},
	{d68040_move16_pi_al , 0xfff8, 0xf600, 0x000},
	{d68040_move16_al_pi , 0xfff8, 0xf608, 0x000},
	{d68040_move16_ai_al , 0xfff8, 0xf610, 0x000},
	{d68040_move16_al_ai , 0xfff8, 0xf618, 0x000},
	{d68000_muls         , 0xf1c0, 0xc1c0, 0xbff},
	{d68000_mulu         , 0xf1c0, 0xc0c0, 0xbff},
	{d68020_mull         , 0xffc0, 0x4c00, 0xbff},
	{d68000_nbcd         , 0xffc0, 0x4800, 0xbf8},
	{d68000_neg_8        , 0xffc0, 0x4400, 0xbf8},
	{d68000_neg_16       , 0xffc0, 0x4440, 0xbf8},
	{d68000_neg_32       , 0xffc0, 0x4480, 0xbf8},
	{d68000_negx_8       , 0xffc0, 0x4000, 0xbf8},
	{d68000_negx_16      , 0xffc0, 0x4040, 0xbf8},
	{d68000_negx_32      , 0xffc0, 0x4080, 0xbf8},
	{d68000_nop          , 0xffff, 0x4e71, 0x000},
	{d68000_not_8        , 0xffc0, 0x4600, 0xbf8},
	{d68000_not_16       , 0xffc0, 0x4640, 0xbf8},
	{d68000_not_32       , 0xffc0, 0x4680, 0xbf8},
	{d68000_or_er_8      , 0xf1c0, 0x8000, 0xbff},
	{d68000_or_er_16     , 0xf1c0, 0x8040, 0xbff},
	{d68000_or_er_32     , 0xf1c0, 0x8080, 0xbff},
	{d68000_or_re_8      , 0xf1c0, 0x8100, 0x3f8},
	{d68000_or_re_16     , 0xf1c0, 0x8140, 0x3f8},
	{d68000_or_re_32     , 0xf1c0, 0x8180, 0x3f8},
	{d68000_ori_to_ccr   , 0xffff, 0x003c, 0x000},
	{d68000_ori_to_sr    , 0xffff, 0x007c, 0x000},
	{d68000_ori_8        , 0xffc0, 0x0000, 0xbf8},
	{d68000_ori_16       , 0xffc0, 0x0040, 0xbf8},
	{d68000_ori_32       , 0xffc0, 0x0080, 0xbf8},
	{d68020_pack_rr      , 0xf1f8, 0x8140, 0x000},
	{d68020_pack_mm      , 0xf1f8, 0x8148, 0x000},
	{d68000_pea          , 0xffc0, 0x4840, 0x27b},
	{d68040_pflush       , 0xffe0, 0xf500, 0x000},
	{d68000_reset        , 0xffff, 0x4e70, 0x000},
	{d68000_ror_s_8      , 0xf1f8, 0xe018, 0x000},
	{d68000_ror_s_16     , 0xf1f8, 0xe058, 0x000},
	{d68000_ror_s_32     , 0xf1f8, 0xe098, 0x000},
	{d68000_ror_r_8      , 0xf1f8, 0xe038, 0x000},
	{d68000_ror_r_16     , 0xf1f8, 0xe078, 0x000},
	{d68000_ror_r_32     , 0xf1f8, 0xe0b8, 0x000},
	{d68000_ror_ea       , 0xffc0, 0xe6c0, 0x3f8},
	{d68000_rol_s_8      , 0xf1f8, 0xe118, 0x000},
	{d68000_rol_s_16     , 0xf1f8, 0xe158, 0x000},
	{d68000_rol_s_32     , 0xf1f8, 0xe198, 0x000},
	{d68000_rol_r_8      , 0xf1f8, 0xe138, 0x000},
	{d68000_rol_r_16     , 0xf1f8, 0xe178, 0x000},
	{d68000_rol_r_32     , 0xf1f8, 0xe1b8, 0x000},
	{d68000_rol_ea       , 0xffc0, 0xe7c0, 0x3f8},
	{d68000_roxr_s_8     , 0xf1f8, 0xe010, 0x000},
	{d68000_roxr_s_16    , 0xf1f8, 0xe050, 0x000},
	{d68000_roxr_s_32    , 0xf1f8, 0xe090, 0x000},
	{d68000_roxr_r_8     , 0xf1f8, 0xe030, 0x000},
	{d68000_roxr_r_16    , 0xf1f8, 0xe070, 0x000},
	{d68000_roxr_r_32    , 0xf1f8, 0xe0b0, 0x000},
	{d68000_roxr_ea      , 0xffc0, 0xe4c0, 0x3f8},
	{d68000_roxl_s_8     , 0xf1f8, 0xe110, 0x000},
	{d68000_roxl_s_16    , 0xf1f8, 0xe150, 0x000},
	{d68000_roxl_s_32    , 0xf1f8, 0xe190, 0x000},
	{d68000_roxl_r_8     , 0xf1f8, 0xe130, 0x000},
	{d68000_roxl_r_16    , 0xf1f8, 0xe170, 0x000},
	{d68000_roxl_r_32    , 0xf1f8, 0xe1b0, 0x000},
	{d68000_roxl_ea      , 0xffc0, 0xe5c0, 0x3f8},
	{d68010_rtd          , 0xffff, 0x4e74, 0x000},
	{d68000_rte          , 0xffff, 0x4e73, 0x000},
	{d68020_rtm          , 0xfff0, 0x06c0, 0x000},
	{d68000_rtr          , 0xffff, 0x4e77, 0x000},
	{d68000_rts          , 0xffff, 0x4e75, 0x000},
	{d68000_sbcd_rr      , 0xf1f8, 0x8100, 0x000},
	{d68000_sbcd_mm      , 0xf1f8, 0x8108, 0x000},
	{d68000_scc          , 0xf0c0, 0x50c0, 0xbf8},
	{d68000_stop         , 0xffff, 0x4e72, 0x000},
	{d68000_sub_er_8     , 0xf1c0, 0x9000, 0xbff},
	{d68000_sub_er_16    , 0xf1c0, 0x9040, 0xfff},
	{d68000_sub_er_32    , 0xf1c0, 0x9080, 0xfff},
	{d68000_sub_re_8     , 0xf1c0, 0x9100, 0x3f8},
	{d68000_sub_re_16    , 0xf1c0, 0x9140, 0x3f8},
	{d68000_sub_re_32    , 0xf1c0, 0x9180, 0x3f8},
	{d68000_suba_16      , 0xf1c0, 0x90c0, 0xfff},
	{d68000_suba_32      , 0xf1c0, 0x91c0, 0xfff},
	{d68000_subi_8       , 0xffc0, 0x0400, 0xbf8},
	{d68000_subi_16      , 0xffc0, 0x0440, 0xbf8},
	{d68000_subi_32      , 0xffc0, 0x0480, 0xbf8},
	{d68000_subq_8       , 0xf1c0, 0x5100, 0xbf8},
	{d68000_subq_16      , 0xf1c0, 0x5140, 0xff8},
	{d68000_subq_32      , 0xf1c0, 0x5180, 0xff8},
	{d68000_subx_rr_8    , 0xf1f8, 0x9100, 0x000},
	{d68000_subx_rr_16   , 0xf1f8, 0x9140, 0x000},
	{d68000_subx_rr_32   , 0xf1f8, 0x9180, 0x000},
	{d68000_subx_mm_8    , 0xf1f8, 0x9108, 0x000},
	{d68000_subx_mm_16   , 0xf1f8, 0x9148, 0x000},
	{d68000_subx_mm_32   , 0xf1f8, 0x9188, 0x000},
	{d68000_swap         , 0xfff8, 0x4840, 0x000},
	{d68000_tas          , 0xffc0, 0x4ac0, 0xbf8},
	{d68000_trap         , 0xfff0, 0x4e40, 0x000},
	{d68020_trapcc_0     , 0xf0ff, 0x50fc, 0x000},
	{d68020_trapcc_16    , 0xf0ff, 0x50fa, 0x000},
	{d68020_trapcc_32    , 0xf0ff, 0x50fb, 0x000},
	{d68000_trapv        , 0xffff, 0x4e76, 0x000},
	{d68000_tst_8        , 0xffc0, 0x4a00, 0xbf8},
	{d68020_tst_pcdi_8   , 0xffff, 0x4a3a, 0x000},
	{d68020_tst_pcix_8   , 0xffff, 0x4a3b, 0x000},
	{d68020_tst_i_8      , 0xffff, 0x4a3c, 0x000},
	{d68000_tst_16       , 0xffc0, 0x4a40, 0xbf8},
	{d68020_tst_a_16     , 0xfff8, 0x4a48, 0x000},
	{d68020_tst_pcdi_16  , 0xffff, 0x4a7a, 0x000},
	{d68020_tst_pcix_16  , 0xffff, 0x4a7b, 0x000},
	{d68020_tst_i_16     , 0xffff, 0x4a7c, 0x000},
	{d68000_tst_32       , 0xffc0, 0x4a80, 0xbf8},
	{d68020_tst_a_32     , 0xfff8, 0x4a88, 0x000},
	{d68020_tst_pcdi_32  , 0xffff, 0x4aba, 0x000},
	{d68020_tst_pcix_32  , 0xffff, 0x4abb, 0x000},
	{d68020_tst_i_32     , 0xffff, 0x4abc, 0x000},
	{d68000_unlk         , 0xfff8, 0x4e58, 0x000},
	{d68020_unpk_rr      , 0xf1f8, 0x8180, 0x000},
	{d68020_unpk_mm      , 0xf1f8, 0x8188, 0x000},
	{d68851_p000         , 0xffc0, 0xf000, 0x000},
	{d68851_pbcc16       , 0xffc0, 0xf080, 0x000},
	{d68851_pbcc32       , 0xffc0, 0xf0c0, 0x000},
	{d68851_pdbcc        , 0xfff8, 0xf048, 0x000},
	{d68851_p001         , 0xffc0, 0xf040, 0x000},
	{0, 0, 0, 0}
};

/* Check if opcode is using a valid ea mode */
static int valid_ea(uint opcode, uint mask)
{
	if(mask == 0)
		return 1;

	switch(opcode & 0x3f)
	{
		case 0x00: case 0x01: case 0x02: case 0x03:
		case 0x04: case 0x05: case 0x06: case 0x07:
			return (mask & 0x800) != 0;
		case 0x08: case 0x09: case 0x0a: case 0x0b:
		case 0x0c: case 0x0d: case 0x0e: case 0x0f:
			return (mask & 0x400) != 0;
		case 0x10: case 0x11: case 0x12: case 0x13:
		case 0x14: case 0x15: case 0x16: case 0x17:
			return (mask & 0x200) != 0;
		case 0x18: case 0x19: case 0x1a: case 0x1b:
		case 0x1c: case 0x1d: case 0x1e: case 0x1f:
			return (mask & 0x100) != 0;
		case 0x20: case 0x21: case 0x22: case 0x23:
		case 0x24: case 0x25: case 0x26: case 0x27:
			return (mask & 0x080) != 0;
		case 0x28: case 0x29: case 0x2a: case 0x2b:
		case 0x2c: case 0x2d: case 0x2e: case 0x2f:
			return (mask & 0x040) != 0;
		case 0x30: case 0x31: case 0x32: case 0x33:
		case 0x34: case 0x35: case 0x36: case 0x37:
			return (mask & 0x020) != 0;
		case 0x38:
			return (mask & 0x010) != 0;
		case 0x39:
			return (mask & 0x008) != 0;
		case 0x3a:
			return (mask & 0x002) != 0;
		case 0x3b:
			return (mask & 0x001) != 0;
		case 0x3c:
			return (mask & 0x004) != 0;
	}
	return 0;

}

/* Used by qsort */
static int DECL_SPEC compare_nof_true_bits(const void *aptr, const void *bptr)
{
	uint a = ((const opcode_struct*)aptr)->mask;
	uint b = ((const opcode_struct*)bptr)->mask;

	a = ((a & 0xAAAA) >> 1) + (a & 0x5555);
	a = ((a & 0xCCCC) >> 2) + (a & 0x3333);
	a = ((a & 0xF0F0) >> 4) + (a & 0x0F0F);
	a = ((a & 0xFF00) >> 8) + (a & 0x00FF);

	b = ((b & 0xAAAA) >> 1) + (b & 0x5555);
	b = ((b & 0xCCCC) >> 2) + (b & 0x3333);
	b = ((b & 0xF0F0) >> 4) + (b & 0x0F0F);
	b = ((b & 0xFF00) >> 8) + (b & 0x00FF);

	return b - a; /* reversed to get greatest to least sorting */
}

/* build the opcode handler jump table */
static void build_opcode_table(void)
{
	uint i;
	uint opcode;
	opcode_struct* ostruct;
	opcode_struct opcode_info[ARRAY_LENGTH(g_opcode_info)];

	sys_memcpy(opcode_info, g_opcode_info, sizeof(g_opcode_info));
	sys_qsort((void *)opcode_info, ARRAY_LENGTH(opcode_info)-1, sizeof(opcode_info[0]), compare_nof_true_bits);

	for(i=0;i<0x10000;i++)
	{
		g_instruction_table[i] = d68000_illegal; /* default to illegal */
		opcode = i;
		/* search through opcode info for a match */
		for(ostruct = opcode_info;ostruct->opcode_handler != 0;ostruct++)
		{
			/* match opcode mask and allowed ea modes */
			if((opcode & ostruct->mask) == ostruct->match)
			{
				/* Handle destination ea for move instructions */
				if((ostruct->opcode_handler == d68000_move_8 ||
					 ostruct->opcode_handler == d68000_move_16 ||
					 ostruct->opcode_handler == d68000_move_32) &&
					 !valid_ea(((opcode>>9)&7) | ((opcode>>3)&0x38), 0xbf8))
						continue;
				if(valid_ea(opcode, ostruct->ea_mask))
				{
					g_instruction_table[i] = ostruct->opcode_handler;
					break;
				}
			}
		}
	}
}



/* ======================================================================== */
/* ================================= API ================================== */
/* ======================================================================== */

/* Disasemble one instruction at pc and store in str_buff */
unsigned int m68k_disassemble(char* str_buff, unsigned int pc, unsigned int cpu_type)
{
	if(!g_initialized)
	{
		build_opcode_table();
		g_initialized = 1;
	}
	switch(cpu_type)
	{
		case M68K_CPU_TYPE_68000:
			g_cpu_type = TYPE_68000;
			g_address_mask = 0x00ffffff;
			break;
		case M68K_CPU_TYPE_68010:
			g_cpu_type = TYPE_68010;
			g_address_mask = 0x00ffffff;
			break;
		case M68K_CPU_TYPE_68EC020:
			g_cpu_type = TYPE_68020;
			g_address_mask = 0x00ffffff;
			break;
		case M68K_CPU_TYPE_68020:
			g_cpu_type = TYPE_68020;
			g_address_mask = 0xffffffff;
			break;
		case M68K_CPU_TYPE_68EC030:
		case M68K_CPU_TYPE_68030:
			g_cpu_type = TYPE_68030;
			g_address_mask = 0xffffffff;
			break;
		case M68K_CPU_TYPE_68040:
		case M68K_CPU_TYPE_68EC040:
		case M68K_CPU_TYPE_68LC040:
			g_cpu_type = TYPE_68040;
			g_address_mask = 0xffffffff;
			break;
    case M68K_CPU_TYPE_DBVZ:
      g_cpu_type = TYPE_68000;
      g_address_mask = 0xffffffff;
      break;
		default:
			return 0;
	}

	g_cpu_pc = pc;
	g_helper_str[0] = 0;
	g_cpu_ir = read_imm_16();
	g_opcode_type = 0;
	g_instruction_table[g_cpu_ir]();
	sys_sprintf(str_buff, "%s%s", g_dasm_str, g_helper_str);
	return COMBINE_OPCODE_FLAGS(g_cpu_pc - pc);
}

char* m68ki_disassemble_quick(unsigned int pc, unsigned int cpu_type)
{
	static char buff[100];
	buff[0] = 0;
	m68k_disassemble(buff, pc, cpu_type);
	return buff;
}

unsigned int m68k_disassemble_raw(char* str_buff, unsigned int pc, const unsigned char* opdata, const unsigned char* argdata, unsigned int cpu_type)
{
	unsigned int result;
	(void)argdata;

	g_rawop = opdata;
	g_rawbasepc = pc;
	result = m68k_disassemble(str_buff, pc, cpu_type);
	g_rawop = NULL;
	return result;
}

/* Check if the instruction is a valid one */
unsigned int m68k_is_valid_instruction(unsigned int instruction, unsigned int cpu_type)
{
	if(!g_initialized)
	{
		build_opcode_table();
		g_initialized = 1;
	}

	instruction &= 0xffff;
	if(g_instruction_table[instruction] == d68000_illegal)
		return 0;

	switch(cpu_type)
	{
		case M68K_CPU_TYPE_68000:
			if(g_instruction_table[instruction] == d68010_bkpt)
				return 0;
			if(g_instruction_table[instruction] == d68010_move_fr_ccr)
				return 0;
			if(g_instruction_table[instruction] == d68010_movec)
				return 0;
			if(g_instruction_table[instruction] == d68010_moves_8)
				return 0;
			if(g_instruction_table[instruction] == d68010_moves_16)
				return 0;
			if(g_instruction_table[instruction] == d68010_moves_32)
				return 0;
			if(g_instruction_table[instruction] == d68010_rtd)
				return 0;
			// Fallthrough
		case M68K_CPU_TYPE_68010:
			if(g_instruction_table[instruction] == d68020_bcc_32)
				return 0;
			if(g_instruction_table[instruction] == d68020_bfchg)
				return 0;
			if(g_instruction_table[instruction] == d68020_bfclr)
				return 0;
			if(g_instruction_table[instruction] == d68020_bfexts)
				return 0;
			if(g_instruction_table[instruction] == d68020_bfextu)
				return 0;
			if(g_instruction_table[instruction] == d68020_bfffo)
				return 0;
			if(g_instruction_table[instruction] == d68020_bfins)
				return 0;
			if(g_instruction_table[instruction] == d68020_bfset)
				return 0;
			if(g_instruction_table[instruction] == d68020_bftst)
				return 0;
			if(g_instruction_table[instruction] == d68020_bra_32)
				return 0;
			if(g_instruction_table[instruction] == d68020_bsr_32)
				return 0;
			if(g_instruction_table[instruction] == d68020_callm)
				return 0;
			if(g_instruction_table[instruction] == d68020_cas_8)
				return 0;
			if(g_instruction_table[instruction] == d68020_cas_16)
				return 0;
			if(g_instruction_table[instruction] == d68020_cas_32)
				return 0;
			if(g_instruction_table[instruction] == d68020_cas2_16)
				return 0;
			if(g_instruction_table[instruction] == d68020_cas2_32)
				return 0;
			if(g_instruction_table[instruction] == d68020_chk_32)
				return 0;
			if(g_instruction_table[instruction] == d68020_chk2_cmp2_8)
				return 0;
			if(g_instruction_table[instruction] == d68020_chk2_cmp2_16)
				return 0;
			if(g_instruction_table[instruction] == d68020_chk2_cmp2_32)
				return 0;
			if(g_instruction_table[instruction] == d68020_cmpi_pcdi_8)
				return 0;
			if(g_instruction_table[instruction] == d68020_cmpi_pcix_8)
				return 0;
			if(g_instruction_table[instruction] == d68020_cmpi_pcdi_16)
				return 0;
			if(g_instruction_table[instruction] == d68020_cmpi_pcix_16)
				return 0;
			if(g_instruction_table[instruction] == d68020_cmpi_pcdi_32)
				return 0;
			if(g_instruction_table[instruction] == d68020_cmpi_pcix_32)
				return 0;
			if(g_instruction_table[instruction] == d68020_cpbcc_16)
				return 0;
			if(g_instruction_table[instruction] == d68020_cpbcc_32)
				return 0;
			if(g_instruction_table[instruction] == d68020_cpdbcc)
				return 0;
			if(g_instruction_table[instruction] == d68020_cpgen)
				return 0;
			if(g_instruction_table[instruction] == d68020_cprestore)
				return 0;
			if(g_instruction_table[instruction] == d68020_cpsave)
				return 0;
			if(g_instruction_table[instruction] == d68020_cpscc)
				return 0;
			if(g_instruction_table[instruction] == d68020_cptrapcc_0)
				return 0;
			if(g_instruction_table[instruction] == d68020_cptrapcc_16)
				return 0;
			if(g_instruction_table[instruction] == d68020_cptrapcc_32)
				return 0;
			if(g_instruction_table[instruction] == d68020_divl)
				return 0;
			if(g_instruction_table[instruction] == d68020_extb_32)
				return 0;
			if(g_instruction_table[instruction] == d68020_link_32)
				return 0;
			if(g_instruction_table[instruction] == d68020_mull)
				return 0;
			if(g_instruction_table[instruction] == d68020_pack_rr)
				return 0;
			if(g_instruction_table[instruction] == d68020_pack_mm)
				return 0;
			if(g_instruction_table[instruction] == d68020_rtm)
				return 0;
			if(g_instruction_table[instruction] == d68020_trapcc_0)
				return 0;
			if(g_instruction_table[instruction] == d68020_trapcc_16)
				return 0;
			if(g_instruction_table[instruction] == d68020_trapcc_32)
				return 0;
			if(g_instruction_table[instruction] == d68020_tst_pcdi_8)
				return 0;
			if(g_instruction_table[instruction] == d68020_tst_pcix_8)
				return 0;
			if(g_instruction_table[instruction] == d68020_tst_i_8)
				return 0;
			if(g_instruction_table[instruction] == d68020_tst_a_16)
				return 0;
			if(g_instruction_table[instruction] == d68020_tst_pcdi_16)
				return 0;
			if(g_instruction_table[instruction] == d68020_tst_pcix_16)
				return 0;
			if(g_instruction_table[instruction] == d68020_tst_i_16)
				return 0;
			if(g_instruction_table[instruction] == d68020_tst_a_32)
				return 0;
			if(g_instruction_table[instruction] == d68020_tst_pcdi_32)
				return 0;
			if(g_instruction_table[instruction] == d68020_tst_pcix_32)
				return 0;
			if(g_instruction_table[instruction] == d68020_tst_i_32)
				return 0;
			if(g_instruction_table[instruction] == d68020_unpk_rr)
				return 0;
			if(g_instruction_table[instruction] == d68020_unpk_mm)
				return 0;
			// Fallthrough
		case M68K_CPU_TYPE_68EC020:
		case M68K_CPU_TYPE_68020:
		case M68K_CPU_TYPE_68030:
		case M68K_CPU_TYPE_68EC030:
			if(g_instruction_table[instruction] == d68040_cinv)
				return 0;
			if(g_instruction_table[instruction] == d68040_cpush)
				return 0;
			if(g_instruction_table[instruction] == d68040_move16_pi_pi)
				return 0;
			if(g_instruction_table[instruction] == d68040_move16_pi_al)
				return 0;
			if(g_instruction_table[instruction] == d68040_move16_al_pi)
				return 0;
			if(g_instruction_table[instruction] == d68040_move16_ai_al)
				return 0;
			if(g_instruction_table[instruction] == d68040_move16_al_ai)
				return 0;
			// Fallthrough
		case M68K_CPU_TYPE_68040:
		case M68K_CPU_TYPE_68EC040:
		case M68K_CPU_TYPE_68LC040:
			if(g_instruction_table[instruction] == d68020_cpbcc_16)
				return 0;
			if(g_instruction_table[instruction] == d68020_cpbcc_32)
				return 0;
			if(g_instruction_table[instruction] == d68020_cpdbcc)
				return 0;
			if(g_instruction_table[instruction] == d68020_cpgen)
				return 0;
			if(g_instruction_table[instruction] == d68020_cprestore)
				return 0;
			if(g_instruction_table[instruction] == d68020_cpsave)
				return 0;
			if(g_instruction_table[instruction] == d68020_cpscc)
				return 0;
			if(g_instruction_table[instruction] == d68020_cptrapcc_0)
				return 0;
			if(g_instruction_table[instruction] == d68020_cptrapcc_16)
				return 0;
			if(g_instruction_table[instruction] == d68020_cptrapcc_32)
				return 0;
			if(g_instruction_table[instruction] == d68040_pflush)
				return 0;
	}
	if(cpu_type != M68K_CPU_TYPE_68020 && cpu_type != M68K_CPU_TYPE_68EC020 &&
	  (g_instruction_table[instruction] == d68020_callm ||
	  g_instruction_table[instruction] == d68020_rtm))
		return 0;

	return 1;
}

typedef struct {
  uint32_t pc, size;
  char *buf;
} m68k_disasm_t;

/*
  for (int i = 0; i < stackp; i++) { \
    debug(1, "XXX", "PUSH DSTACK %3d: 0x%08X ", i, stack[i]); \
  }
*/

#define PUSH(pc) \
  if (0) debug(DEBUG_TRACE, "M68K", "DSTACK push 0x%08X at %d", pc, stackp); \
  stack[stackp++] = pc

#define POP() \
  pc = stack[--stackp]; \
  if (0) debug(DEBUG_TRACE, "M68K", "DSTACK pop  0x%08X from %d", pc, stackp)

void m68k_disassemble_range(unsigned int start, unsigned int end, unsigned int cpu_type) {
  m68k_disasm_t *mem;
  unsigned int pc, opcode, stack[4096];
  uint8_t pad[16];
  int stackp, pop, size, index;
  uint32_t i, total;
  int32_t offset;
  char buf[128];

  mem = sys_calloc((end - start) / 2, sizeof(m68k_disasm_t));
  stackp = 0;
  PUSH(start);
  pop = 1;

  for (;;) {
    if (pop) {
      if (stackp == 0) break;
      POP();
      pop = 0;
    }
    index = (pc - start) / 2;
    if (mem[index].buf) {
      pop = 1;
      continue;
    }

    opcode = m68k_read_memory_16(pc);

    if (opcode == 0x6000) {
      // BRA 16 bits
      offset = (int16_t)m68k_read_memory_16(pc + 2);
      PUSH(pc + 2 + offset);
      pop = 1;
    } else if ((opcode & 0xff00) == 0x6000) {
      // BRA 8 bits
      offset = (int8_t)(opcode & 0xff);
      PUSH(pc + 2 + offset);
      pop = 1;
    } else if ((opcode & 0xfff8) == 0x51c8) {
      // DBRA
      offset = (int16_t)m68k_read_memory_16(pc + 2);
      PUSH(pc + 2 + offset);
    } else if ((opcode & 0xf0ff) == 0x60ff) {
      // Bcc 32 bits
      offset = (int32_t)m68k_read_memory_32(pc + 2);
      PUSH(pc + 2 + offset);
    } else if ((opcode & 0xf0ff) == 0x6000) {
      // Bcc 16 bits
      offset = (int16_t)m68k_read_memory_16(pc + 2);
      PUSH(pc + 2 + offset);
    } else if ((opcode & 0xf000) == 0x6000) {
      // Bcc 8 bits
      offset = (int8_t)(opcode & 0xff);
      PUSH(pc + 2 + offset);
    } else if (opcode == 0x4eb9) {
      // JSR 32 bits
      PUSH((uint32_t)m68k_read_memory_32(pc + 2));
    } else if (opcode == 0x4e75) {
      // RTS
      pop = 1;
    }

    size = m68k_disassemble(buf, pc, cpu_type);
//char buf2[128];
//m68k_make_hex(buf2, pc, size);
//debug(1, "XXX", "%08X: %-20s: %s", pc, buf2, buf);
    mem[index].pc = pc;
    mem[index].size = size;
    mem[index].buf = sys_strdup(buf);
    pc += size;
    if (pc >= end) {
      pop = 1;
    }
  }

  pc = start;
  size = (end - start) / 2;
  for (index = 0; index < size; index++) {
    if (mem[index].buf) {
      total = mem[index].pc - pc;
      if (total > 0) {
        debug(DEBUG_TRACE, "M68K", "gap from %08X to %08X (%d bytes)", pc, mem[index].pc - 1, total);
        for (;;) {
          for (i = 0; i < 16 && pc+i < mem[index].pc; i++) {
            pad[i] = m68k_read_memory_8(pc+i);
          }
          debug_bytes_offset(DEBUG_TRACE, "M68K", pad, i, pc);
          if (i < 16) break;
          pc += 16;
        }
      }
      m68k_make_hex(buf, mem[index].pc, mem[index].size);
      debug(DEBUG_TRACE, "M68K", "%08X: %-20s: %s", mem[index].pc, buf, mem[index].buf);
      pc = mem[index].pc + mem[index].size;
      sys_free(mem[index].buf);
    }
  }

  sys_free(mem);
}

// f028 2215 0008

/* ======================================================================== */
/* ============================== END OF FILE ============================= */
/* ======================================================================== */
