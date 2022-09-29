/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *  gfx/VENICE/lib/libvenice/globals.h $Revision: 1.1 $
 */

extern struct venice_info VEN_info;
extern int ven_gfxfd;
extern int ven_pipenum;
extern int ven_pipeMapped;
extern volatile unsigned int *ven_gfxvaddr;
extern volatile unsigned int *ven_pipeaddr;
extern struct venice_dg2_eeprom ven_eeprom;
extern int ven_eeprom_valid;

extern void venice_dg2initXilinx(void);
extern void venice_set_vcreg(int, int, int);
extern int venice_get_vcreg(int);
extern void venice_finish_vdrc_sram(int, int, int, int);
extern int load_4000_serial_byte(void (*)(int), int (*)(void), void (*)(int),
		int (*)(void), int(*)(void), int, unsigned char*);
extern int load_3000_serial_byte(void (*)(int), void (*)(int), int(*)(void),
		void (*)(int), int(*)(void), int, unsigned char*);
extern void venice_dg2initVof(int verbose);

extern int ven_type;

extern int get_ven_type(void);
extern int fcg_jt_select_chain(char *);
extern int fcg_jt_send_data(int, char *, char *);
extern int fcg_jt_send_instruction(int, char *, char *);

#define VEN_NOT_KNOWN	-1
#define VEN_MPG		0
#define VEN_FCG		1

#define VEN_TYPE() ((ven_type == VEN_NOT_KNOWN)? get_ven_type() : ven_type)

#ifdef DEBUG
extern void ttyprintf(char *fmt, ...);
#define xprintf ttyprintf
#else
/* ARGSUSED */
void xprintf(char *str, ...)
{
}
#endif /* DEBUG */
/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *  gfx/VENICE/lib/libvenice/globals.c $Revision: 1.1 $
 */

#ifdef _KERNEL
#include "sys/types.h"
#include "sys/sysmacros.h"
#include "sys/buf.h"		 /* needed for B_READ/WRITE */
#include "sys/systm.h"
#include "sys/psw.h"
#include "sys/cmn_err.h"
#include "sys/invent.h"
#include "sys/edt.h"
#include "sys/reg.h"
#include "sys/errno.h"
#include "sys/sysinfo.h"
#include "sys/immu.h"
#include "sys/param.h"
#include "sys/pfdat.h"
#include "sys/gfx.h"
#include "sys/rrm.h"
#include "sys/venice.h"
#include "VENICE/gl/cpcmds.h"
#include "VENICE/gl/mc.out.h"
#include "sys/debug.h"
#define sginap(x)	DELAY(x*1000)
#else
#include <sys/gfx.h>
#include <sys/venice.h>
#endif
#include "limits.h"	/* for INT_MAX */
#include <voftypedefs.h>
#include <vofdefs.h>
#include <dg2_eeprom.h>
#include <dg2_encoder.h>


struct venice_info VEN_info;
int ven_gfxfd;
int ven_pipenum;
int ven_pipeMapped = 0;
volatile unsigned int *ven_gfxvaddr;
volatile unsigned int *ven_pipeaddr;
struct venice_dg2_eeprom ven_eeprom;
int ven_eeprom_valid;


#if defined(IP19) || defined(IP21) || defined(IP25)
int ven_type = VEN_FCG;
#else
int ven_type = VEN_MPG;
#endif


/*
 *  stuff from jtag.h (to keep airey quiet)
 */
typedef	char jvector_t;
#define jvsize(n)    ((unsigned)((n)+7)>>3)

#ifdef	notdef
static unsigned int xsave[10][9];
#endif

extern int vof_control_reg_value, vof_clock_value, vof_outsync_level;
extern int vif_genlock;
extern int ie_config_lo, ie_config_hi;
extern int hrate_picoseconds;

extern unsigned int vif_bporch_clamp_shadow;

extern void venice_load_clockDivider(int hrate_picoseconds);

#define DEF_TILES_PER_LINE	16
#define DEF_BASE_TILE_ADDR	0
#define DEF_PIXEL_SIZE		0
#define DEF_NUM_RMS		1

#define DG_WRITE_IE_REG(addr, data) \
{                                               \
    venice_set_vcreg(V_DG_VDRC_CNTRL, 0x0, 0);          \
    venice_set_vcreg(V_DG_VDRC_IE_WRITE, ((addr << 1) & 0x7f) | \
	((data << 7) & 0x80) | 0x1, 0); \
    venice_set_vcreg(V_DG_VDRC_CNTRL, DG_VDRC_DID_NEXTBYTE_EN, 0); \
    venice_set_vcreg(V_DG_VDRC_IE_WRITE, ((data >> 1) & 0x1f), 0);\
}

#define SET_RGBDAC_ADDR(addr16)                         \
{                                                       \
   unsigned int lo, hi;                                 \
   lo = addr16 &0xff;                                   \
   lo = lo | ((lo<<8)&0xff00) | ((lo<<16)&0xff0000);    \
   hi = (addr16 >>8) &0xff;                             \
   hi = hi | ((hi<<8)&0xff00) | ((hi<<16)&0xff0000);    \
   venice_set_vcreg(V_DG_RGBDAC_ADDRPTR_LO, lo, 0); \
   venice_set_vcreg(V_DG_RGBDAC_ADDRPTR_HI, hi, 0); \
}

#define SET_CURSOR_ADDR(addr16)                         \
{                                                       \
   unsigned int lo, hi;                                 \
   lo = addr16 &0xff;                                   \
   lo = lo | ((lo<<8)&0xff00);                          \
   hi = (addr16 >>8) &0xff;                             \
   hi = hi | ((hi<<8)&0xff00);                          \
   venice_set_vcreg(V_DG_CURSOR_ADDRPTR_LO, lo, 0);         \
   venice_set_vcreg(V_DG_CURSOR_ADDRPTR_HI, hi, 0);         \
}

#define SET_ALPHADAC_ADDR(addr16)           \
{                                               \
   unsigned int lo, hi;                                 \
   lo = addr16 &0xff;                                   \
   lo = lo | ((lo<<8)&0xff00) | ((lo<<16)&0xff0000);    \
   hi = (addr16 >>8) &0xff;                             \
   hi = hi | ((hi<<8)&0xff00) | ((hi<<16)&0xff0000);    \
   venice_set_vcreg(V_DG_ALPHADAC_ADDRPTR_LO, lo, 0); \
   venice_set_vcreg(V_DG_ALPHADAC_ADDRPTR_HI, hi, 0); \
}

typedef struct {
    short blue;
    short green;
    short red;
} cmap_slot_t;


static struct {
    int w, h;		/* width and height */
    int hotx, hoty;	/* hot spot */
    int cper;		/* characters per row */
    unsigned char bitmap[512]; /* bitmap */
} glyph = {
    16, 16,
    0, 0,
    2,
    0x80, 0x00,
    0xC0, 0x00,
    0xE0, 0x00,
    0xF0, 0x00,
    0xF8, 0x00,
    0xFC, 0x00,
    0xFE, 0x00,
    0xFF, 0x00,
    0xF8, 0x00,
    0xD8, 0x00,
    0x8C, 0x00,
    0x0C, 0x00,
    0x06, 0x00,
    0x06, 0x00,
    0x03, 0x00,
    0x03, 0x00,
    0x0
};


static cmap_slot_t venice_def_cmap[256] = {
    {0x0,	0x0,	0x0},
    {0x0,	0x0,	0xff},
    {0x0,	0xff,	0x0},
    {0x0,	0xff,	0xff},
    {0xff,	0x0,	0x0},
    {0xff,	0x0,	0xff},
    {0xff,	0xff,	0x0},
    {0xff,	0xff,	0xff},
    {0x55,	0x55,	0x55},
    {0x71,	0x71,	0xc6},
    {0x71,	0xc6,	0x71},
    {0x38,	0x8e,	0x8e},
    {0xc6,	0x71,	0x71},
    {0x8e,	0x38,	0x8e},
    {0x8e,	0x8e,	0x38},
    {0xaa,	0xaa,	0xaa},
    {0x9e,	0x71,	0x4c},
    {0x80,	0x80,	0x80},
    {0xc6,	0xc6,	0xc6},
    {0x42,	0x42,	0x42},
    {0x80,	0x9f,	0xa5},
    {0xca,	0xd7,	0xda},
    {0x46,	0x57,	0x5b},
    {0x9f,	0x9f,	0x9f},
    {0x27,	0x27,	0x27},
    {0xd7,	0xd7,	0xd7},
    {0x57,	0x57,	0x57},
    {0x7f,	0x0,	0x0},
    {0xd5,	0xd5,	0xd5},
    {0x3f,	0x0,	0x0},
    {0x2a,	0x2a,	0x2a},
    {0x0,	0x0,	0x0},
    {0xa,	0xa,	0xa},
    {0x14,	0x14,	0x14},
    {0x1e,	0x1e,	0x1e},
    {0x28,	0x28,	0x28},
    {0x33,	0x33,	0x33},
    {0x3d,	0x3d,	0x3d},
    {0x47,	0x47,	0x47},
    {0x51,	0x51,	0x51},
    {0x5b,	0x5b,	0x5b},
    {0x66,	0x66,	0x66},
    {0x70,	0x70,	0x70},
    {0x7a,	0x7a,	0x7a},
    {0x84,	0x84,	0x84},
    {0x8e,	0x8e,	0x8e},
    {0x99,	0x99,	0x99},
    {0xa3,	0xa3,	0xa3},
    {0xad,	0xad,	0xad},
    {0xb7,	0xb7,	0xb7},
    {0xc1,	0xc1,	0xc1},
    {0xcc,	0xcc,	0xcc},
    {0xd6,	0xd6,	0xd6},
    {0xe0,	0xe0,	0xe0},
    {0xea,	0xea,	0xea},
    {0xf4,	0xf4,	0xf4},
    {0x0,	0x0,	0x0},
    {0x0,	0x24,	0x0},
    {0x0,	0x48,	0x0},
    {0x0,	0x6d,	0x0},
    {0x0,	0x91,	0x0},
    {0x0,	0xb6,	0x0},
    {0x0,	0xda,	0x0},
    {0x0,	0xff,	0x0},
    {0x0,	0x0,	0x3f},
    {0x0,	0x24,	0x3f},
    {0x0,	0x48,	0x3f},
    {0x0,	0x6d,	0x3f},
    {0x0,	0x91,	0x3f},
    {0x0,	0xb6,	0x3f},
    {0x0,	0xda,	0x3f},
    {0x0,	0xff,	0x3f},
    {0x0,	0x0,	0x7f},
    {0x0,	0x24,	0x7f},
    {0x0,	0x48,	0x7f},
    {0x0,	0x6d,	0x7f},
    {0x0,	0x91,	0x7f},
    {0x0,	0xb6,	0x7f},
    {0x0,	0xda,	0x7f},
    {0x0,	0xff,	0x7f},
    {0x0,	0x0,	0xbf},
    {0x0,	0x24,	0xbf},
    {0x0,	0x48,	0xbf},
    {0x0,	0x6d,	0xbf},
    {0x0,	0x91,	0xbf},
    {0x0,	0xb6,	0xbf},
    {0x0,	0xda,	0xbf},
    {0x0,	0xff,	0xbf},
    {0x0,	0x0,	0xff},
    {0x0,	0x24,	0xff},
    {0x0,	0x48,	0xff},
    {0x0,	0x6d,	0xff},
    {0x0,	0x91,	0xff},
    {0x0,	0xb6,	0xff},
    {0x0,	0xda,	0xff},
    {0x0,	0xff,	0xff},
    {0x3f,	0x0,	0x0},
    {0x3f,	0x24,	0x0},
    {0x3f,	0x48,	0x0},
    {0x3f,	0x6d,	0x0},
    {0x3f,	0x91,	0x0},
    {0x3f,	0xb6,	0x0},
    {0x3f,	0xda,	0x0},
    {0x3f,	0xff,	0x0},
    {0x3f,	0x0,	0x3f},
    {0x3f,	0x24,	0x3f},
    {0x3f,	0x48,	0x3f},
    {0x3f,	0x6d,	0x3f},
    {0x3f,	0x91,	0x3f},
    {0x3f,	0xb6,	0x3f},
    {0x3f,	0xda,	0x3f},
    {0x3f,	0xff,	0x3f},
    {0x3f,	0x0,	0x7f},
    {0x3f,	0x24,	0x7f},
    {0x3f,	0x48,	0x7f},
    {0x3f,	0x6d,	0x7f},
    {0x3f,	0x91,	0x7f},
    {0x3f,	0xb6,	0x7f},
    {0x3f,	0xda,	0x7f},
    {0x3f,	0xff,	0x7f},
    {0x3f,	0x0,	0xbf},
    {0x3f,	0x24,	0xbf},
    {0x3f,	0x48,	0xbf},
    {0x3f,	0x6d,	0xbf},
    {0x3f,	0x91,	0xbf},
    {0x3f,	0xb6,	0xbf},
    {0x3f,	0xda,	0xbf},
    {0x3f,	0xff,	0xbf},
    {0x3f,	0x0,	0xff},
    {0x3f,	0x24,	0xff},
    {0x3f,	0x48,	0xff},
    {0x3f,	0x6d,	0xff},
    {0x3f,	0x91,	0xff},
    {0x3f,	0xb6,	0xff},
    {0x3f,	0xda,	0xff},
    {0x3f,	0xff,	0xff},
    {0x7f,	0x0,	0x0},
    {0x7f,	0x24,	0x0},
    {0x7f,	0x48,	0x0},
    {0x7f,	0x6d,	0x0},
    {0x7f,	0x91,	0x0},
    {0x7f,	0xb6,	0x0},
    {0x7f,	0xda,	0x0},
    {0x7f,	0xff,	0x0},
    {0x7f,	0x0,	0x3f},
    {0x7f,	0x24,	0x3f},
    {0x7f,	0x48,	0x3f},
    {0x7f,	0x6d,	0x3f},
    {0x7f,	0x91,	0x3f},
    {0x7f,	0xb6,	0x3f},
    {0x7f,	0xda,	0x3f},
    {0x7f,	0xff,	0x3f},
    {0x7f,	0x0,	0x7f},
    {0x7f,	0x24,	0x7f},
    {0x7f,	0x48,	0x7f},
    {0x7f,	0x6d,	0x7f},
    {0x7f,	0x91,	0x7f},
    {0x7f,	0xb6,	0x7f},
    {0x7f,	0xda,	0x7f},
    {0x7f,	0xff,	0x7f},
    {0x7f,	0x0,	0xbf},
    {0x7f,	0x24,	0xbf},
    {0x7f,	0x48,	0xbf},
    {0x7f,	0x6d,	0xbf},
    {0x7f,	0x91,	0xbf},
    {0x7f,	0xb6,	0xbf},
    {0x7f,	0xda,	0xbf},
    {0x7f,	0xff,	0xbf},
    {0x7f,	0x0,	0xff},
    {0x7f,	0x24,	0xff},
    {0x7f,	0x48,	0xff},
    {0x7f,	0x6d,	0xff},
    {0x7f,	0x91,	0xff},
    {0x7f,	0xb6,	0xff},
    {0x7f,	0xda,	0xff},
    {0x7f,	0xff,	0xff},
    {0xbf,	0x0,	0x0},
    {0xbf,	0x24,	0x0},
    {0xbf,	0x48,	0x0},
    {0xbf,	0x6d,	0x0},
    {0xbf,	0x91,	0x0},
    {0xbf,	0xb6,	0x0},
    {0xbf,	0xda,	0x0},
    {0xbf,	0xff,	0x0},
    {0xbf,	0x0,	0x3f},
    {0xbf,	0x24,	0x3f},
    {0xbf,	0x48,	0x3f},
    {0xbf,	0x6d,	0x3f},
    {0xbf,	0x91,	0x3f},
    {0xbf,	0xb6,	0x3f},
    {0xbf,	0xda,	0x3f},
    {0xbf,	0xff,	0x3f},
    {0xbf,	0x0,	0x7f},
    {0xbf,	0x24,	0x7f},
    {0xbf,	0x48,	0x7f},
    {0xbf,	0x6d,	0x7f},
    {0xbf,	0x91,	0x7f},
    {0xbf,	0xb6,	0x7f},
    {0xbf,	0xda,	0x7f},
    {0xbf,	0xff,	0x7f},
    {0xbf,	0x0,	0xbf},
    {0xbf,	0x24,	0xbf},
    {0xbf,	0x48,	0xbf},
    {0xbf,	0x6d,	0xbf},
    {0xbf,	0x91,	0xbf},
    {0xbf,	0xb6,	0xbf},
    {0xbf,	0xda,	0xbf},
    {0xbf,	0xff,	0xbf},
    {0xbf,	0x0,	0xff},
    {0xbf,	0x24,	0xff},
    {0xbf,	0x48,	0xff},
    {0xbf,	0x6d,	0xff},
    {0xbf,	0x91,	0xff},
    {0xbf,	0xb6,	0xff},
    {0xbf,	0xda,	0xff},
    {0xbf,	0xff,	0xff},
    {0xff,	0x0,	0x0},
    {0xff,	0x24,	0x0},
    {0xff,	0x48,	0x0},
    {0xff,	0x6d,	0x0},
    {0xff,	0x91,	0x0},
    {0xff,	0xb6,	0x0},
    {0xff,	0xda,	0x0},
    {0xff,	0xff,	0x0},
    {0xff,	0x0,	0x3f},
    {0xff,	0x24,	0x3f},
    {0xff,	0x48,	0x3f},
    {0xff,	0x6d,	0x3f},
    {0xff,	0x91,	0x3f},
    {0xff,	0xb6,	0x3f},
    {0xff,	0xda,	0x3f},
    {0xff,	0xff,	0x3f},
    {0xff,	0x0,	0x7f},
    {0xff,	0x24,	0x7f},
    {0xff,	0x48,	0x7f},
    {0xff,	0x6d,	0x7f},
    {0xff,	0x91,	0x7f},
    {0xff,	0xb6,	0x7f},
    {0xff,	0xda,	0x7f},
    {0xff,	0xff,	0x7f},
    {0xff,	0x0,	0xbf},
    {0xff,	0x24,	0xbf},
    {0xff,	0x48,	0xbf},
    {0xff,	0x6d,	0xbf},
    {0xff,	0x91,	0xbf},
    {0xff,	0xb6,	0xbf},
    {0xff,	0xda,	0xbf},
    {0xff,	0xff,	0xbf},
    {0xff,	0x0,	0xff},
    {0xff,	0x24,	0xff},
    {0xff,	0x48,	0xff},
    {0xff,	0x6d,	0xff},
    {0xff,	0x91,	0xff},
    {0xff,	0xb6,	0xff},
    {0xff,	0xda,	0xff},
    {0xff,	0xff,	0xff}
};

unsigned char g1_7[] = {
0,4,7,8,10,11,12,14,15,16,17,18,19,20,20,21,22,23,24,24,
25,26,27,27,28,29,29,30,31,31,32,33,33,34,34,35,36,36,37,37,
38,38,39,40,40,41,41,42,42,43,43,44,44,45,45,46,46,47,47,48,
48,49,49,49,50,50,51,51,52,52,53,53,54,54,54,55,55,56,56,57,
57,57,58,58,59,59,59,60,60,61,61,61,62,62,63,63,63,64,64,65,
65,65,66,66,66,67,67,68,68,68,69,69,69,70,70,71,71,71,72,72,
72,73,73,73,74,74,74,75,75,75,76,76,76,77,77,77,78,78,78,79,
79,79,80,80,80,81,81,81,82,82,82,83,83,83,84,84,84,85,85,85,
86,86,86,87,87,87,87,88,88,88,89,89,89,90,90,90,91,91,91,91,
92,92,92,93,93,93,94,94,94,94,95,95,95,96,96,96,96,97,97,97,
98,98,98,98,99,99,99,100,100,100,100,101,101,101,102,102,102,102,103,103,
103,104,104,104,104,105,105,105,105,106,106,106,107,107,107,107,108,108,108,108,
109,109,109,109,110,110,110,111,111,111,111,112,112,112,112,113,113,113,113,114,
114,114,114,115,115,115,115,116,116,116,116,117,117,117,117,118,118,118,118,119,
119,119,119,120,120,120,120,121,121,121,121,122,122,122,122,123,123,123,123,124,
124,124,124,125,125,125,125,126,126,126,126,127,127,127,127,128,128,128,128,128,
129,129,129,129,130,130,130,130,131,131,131,131,132,132,132,132,132,133,133,133,
133,134,134,134,134,135,135,135,135,135,136,136,136,136,137,137,137,137,138,138,
138,138,138,139,139,139,139,140,140,140,140,140,141,141,141,141,142,142,142,142,
142,143,143,143,143,144,144,144,144,144,145,145,145,145,145,146,146,146,146,147,
147,147,147,147,148,148,148,148,148,149,149,149,149,150,150,150,150,150,151,151,
151,151,151,152,152,152,152,153,153,153,153,153,154,154,154,154,154,155,155,155,
155,155,156,156,156,156,156,157,157,157,157,158,158,158,158,158,159,159,159,159,
159,160,160,160,160,160,161,161,161,161,161,162,162,162,162,162,163,163,163,163,
163,164,164,164,164,164,165,165,165,165,165,166,166,166,166,166,167,167,167,167,
167,168,168,168,168,168,169,169,169,169,169,170,170,170,170,170,170,171,171,171,
171,171,172,172,172,172,172,173,173,173,173,173,174,174,174,174,174,175,175,175,
175,175,175,176,176,176,176,176,177,177,177,177,177,178,178,178,178,178,179,179,
179,179,179,179,180,180,180,180,180,181,181,181,181,181,182,182,182,182,182,182,
183,183,183,183,183,184,184,184,184,184,184,185,185,185,185,185,186,186,186,186,
186,186,187,187,187,187,187,188,188,188,188,188,188,189,189,189,189,189,190,190,
190,190,190,190,191,191,191,191,191,192,192,192,192,192,192,193,193,193,193,193,
194,194,194,194,194,194,195,195,195,195,195,195,196,196,196,196,196,197,197,197,
197,197,197,198,198,198,198,198,198,199,199,199,199,199,199,200,200,200,200,200,
201,201,201,201,201,201,202,202,202,202,202,202,203,203,203,203,203,203,204,204,
204,204,204,205,205,205,205,205,205,206,206,206,206,206,206,207,207,207,207,207,
207,208,208,208,208,208,208,209,209,209,209,209,209,210,210,210,210,210,210,211,
211,211,211,211,211,212,212,212,212,212,212,213,213,213,213,213,213,214,214,214,
214,214,214,215,215,215,215,215,215,216,216,216,216,216,216,217,217,217,217,217,
217,218,218,218,218,218,218,219,219,219,219,219,219,220,220,220,220,220,220,220,
221,221,221,221,221,221,222,222,222,222,222,222,223,223,223,223,223,223,224,224,
224,224,224,224,225,225,225,225,225,225,225,226,226,226,226,226,226,227,227,227,
227,227,227,228,228,228,228,228,228,229,229,229,229,229,229,229,230,230,230,230,
230,230,231,231,231,231,231,231,232,232,232,232,232,232,232,233,233,233,233,233,
233,234,234,234,234,234,234,234,235,235,235,235,235,235,236,236,236,236,236,236,
236,237,237,237,237,237,237,238,238,238,238,238,238,238,239,239,239,239,239,239,
240,240,240,240,240,240,240,241,241,241,241,241,241,242,242,242,242,242,242,242,
243,243,243,243,243,243,244,244,244,244,244,244,244,245,245,245,245,245,245,245,
246,246,246,246,246,246,247,247,247,247,247,247,247,248,248,248,248,248,248,248,
249,249,249,249,249,249,250,250,250,250,250,250,250,251,251,251,251,251,251,251,
252,252,252,252,252,252,252,253,253,253,253,253,253,254,254,254,254,254,254,254,
255,255,255,255,};

void venice_dg2_resetDacClk(void);
void venice_dg2initIMPS(void);
void venice_dg2initXMAPRegs(void);
void venice_dg2initRegs(void);
void venice_dg2initVlist(void);
void venice_dg2initStopVof(void);
void venice_dg2initVof(int verbose);
void venice_dg2resetVof(void);
void venice_dg2initStartVof(void);
void venice_dg2resetRM(void);
void venice_center_dpots(void);
void venice_dg2initResetXmap(void);

/* ARGSUSED */
void venice_dg2init(int dummy, int verbose, int doVlist)
{


    /*
     * The textport PROM will always use the most recently setmon'd DG2 vof.
     * We do not call any libvs2 functions, so that the prom & kernel textport
     * vof can be more lean.
     */
    VEN_info.going_to_vs2 = 0;

    xprintf("\r");

    venice_dg2initStopVof();
    /*
     * Disable the encoder reset, to avoid bus contention.  Must do this after
     * we stop vof, but must also do this before twiddling any other registers.
     */
    venice_set_vcreg(V_DG_ENCODER_RESET_L, 0x1, 0);

    venice_dg2initXilinx();
    venice_dg2initVof(verbose);
    venice_dg2initRegs();
    /*
     * Reset the VOF after venice_dg2initRegs(), because the XMAP fifo's
     * appear to get some data transferred in when the DI_CNTRL or VDRC
     * are reset and/or initialized.
     */
    venice_dg2resetVof();

    if (doVlist) {
	/*
	 * Kernel will start the VOF after initializing the VLIST, to provide 
	 * for a syncrhonous VLIST startup.  PROM will just start the VOF 
	 * running.
	 */
        venice_dg2initStartVof();
    } else {
	/*
	 * Diagnostic entry point.  Start up the VOF.
	 */
        venice_set_vcreg(V_DG_VOF_CNTRL, (vof_control_reg_value & 0xfe), 0);

        sginap(2);
	/*
	 * Reset DAC clocks directly (i.e. not on the VLIST).
	 */
	venice_dg2_resetDacClk();
    }
    /*
     * Wait a minimum of 100msec, then re-write the vif's back porch clamp 
     * value; each write to the back porch clamp register causes the sync tip 
     * clamps to briefly overlap, which resets the genlock detect circuit.
     */
    sginap(11);
    venice_set_vcreg(V_DG_VIF_BPORCH_CLAMP_EXTERNAL, 
	vif_bporch_clamp_shadow, 0);
}

void venice_dg2_resetDacClk(void)
{
    venice_set_vcreg(V_DG_CLK_RESET_L, 1, 0);
    venice_set_vcreg(V_DG_CLK_RESET_L, 0, 0);
    venice_set_vcreg(V_DG_CLK_RESET_L, 1, 0);
}

void venice_dg2initIMPS(void)
{
    int i;

    /* Load IMP DIDS thru vdrc */
    for (i = V_DG_IE_DID_BASE; i < V_DG_IE_ODID_BASE; i++) {
        DG_WRITE_IE_REG(i, 0x0);
    }

    /* Load IMP overlays thru vdrc */
    for (i = V_DG_IE_ODID_BASE; i < V_DG_IE_CONFIG_LO; i++) {
        DG_WRITE_IE_REG(i, 0x0);
    }
    /*
     * ie_config_lo, ie_config_hi are computed in venice_calcframebuffer().
     */
    DG_WRITE_IE_REG(V_DG_IE_CONFIG_LO, ie_config_lo);

    DG_WRITE_IE_REG(V_DG_IE_CONFIG_HI, ie_config_hi);
}


void venice_dg2initXMAPRegs(void)
{
    int i;
    int cmapWord;
    unsigned int    keySelectValue;

    for (i = 0; i < 32; i++) {
	/*
	 * Do a broadcast write to all xmaps, and enable 12 bits of colorindex
	 * for all DID's.  Use a base address of 0 for all colormaps.
	 */
	venice_set_vcreg((V_DG_XMAP_DID_BASE(0xa) + i), 0xc, 0);
    }

    for (i = 0; i < 8; i++) {
	/*
	 * Do a broadcast write to all xmaps, and map each overlay to
	 * a colormap entry of the same value.  for all overlay ID's.
	 */
	venice_set_vcreg((V_DG_XMAP_ODID_BASE(0xa) + i), (i << 4), 0);
    }

    /*
     * Set pop-up overlay color to white.
     */
    venice_set_vcreg(V_DG_XMAP_PDID(0xa), 0x10, 0);

    /*
     * Set underlay color to black.
     */
    venice_set_vcreg(V_DG_XMAP_UDID(0xa), 0x0, 0);
    /* 
     * Set default colormap for slots 0..255, repeat thru 32767.
     */
    venice_set_vcreg(V_DG_XMAP_CMAPADDR(10), 0, 0);	/* set base address to 0 */

    /*
     * NOTE: xmap colorindex color packing is reversed from the standard
     * GL ordering.
     */
    for (i = 0; i < DG_XMAP_CMAP_SIZE; i++) {
	cmapWord = (((venice_def_cmap[i%256].red & 0xff) << 22) |
		    ((venice_def_cmap[i%256].green  & 0xff) << 12) |
		    ((venice_def_cmap[i%256].blue  & 0xff << 2)));
	venice_set_vcreg(V_DG_XMAP_CMAPDATA(10), cmapWord, 0);
    }

    /*
     * Program input genlock stuff.
     *
     * V_DG_KEY_SEL: use hrate divider clock, not VS2 clock (unless using VS2); 
     *		     use MSB of alpha for encoder keying.
     *
     * V_DG_ADC_SEL1: Don't select the calibration inputs for digitization
     *	              (use external genlock sync source instead).
     * 
     * V_DG_ADC_DISABLE: Enable the adc for sync digitization.
     * 
     * V_DG_AGC_OFF: Turn on the ADC's AGC.
     * 
     * V_DG_GENLOCK: Lock to internal hrate divider (loadvof() may override)
     * 
     * V_DG_GENLOCK_LEVEL: preload setting for genlock level (0-3)
     */
    if (VEN_info.going_to_vs2)
	/*
	 * This sets bit 3 (option_csync_sel), causing the csync signal
	 * from VS2 to be selected. 
	 */
	keySelectValue = 0x4;
    else
	/*
	 * This clears bit 3 (option_csync_sel), selecting the on-board
	 * programmable h rate divider as the sync source. 
	 */
	keySelectValue = 0x0;

    venice_set_vcreg(V_DG_KEY_SEL, keySelectValue, 0);
    venice_set_vcreg(V_DG_ADC_SEL1, 0x0, 0);
    venice_set_vcreg(V_DG_ADC_DISABLE, 0x0, 0);
    venice_set_vcreg(V_DG_AGC_OFF, 0x0, 0);

    /*
     * Don't select the clock generated by the encoder.
     */
    venice_set_vcreg(V_DG_ENC_ADCCLK_SEL, 0x0, 0); 

    /*
     * genlock source
     * 0 = internal 1 = external
     */
    venice_set_vcreg(V_DG_GENLOCK, vif_genlock, 0);

    /*
     * TTL or 1V output sync level
     */
    venice_set_vcreg(V_DG_TTL_SYNC_SEL, vof_outsync_level, 0);

    /*
     * Program the XMAP pixel size register based upon the current
     * pixel density as computed by venice_calcframebuffer().  We need to do
     * it in this function, in case the XMAP's get reset.
     */
    switch (VEN_info.pixel_density) {
	case PIX42_RGBA10:
	    /* 
	     * XMAP (8/10) - clear this bit, for 10 bit/component operation.
	     */
	    venice_set_vcreg(V_DG_XMAP_810(ALL), 0x0, 0);
	    /* 
	     * enable the send ALPHA bit 
	     */
	    venice_set_vcreg(V_DG_XMAP_ALPHA(ALL), 0x1, 0);

	    break;
	case PIX32_RGB10:
	    /* 
	     * XMAP (8/10) - clear this bit, for 10 bit/component operation.
	     */
	    venice_set_vcreg(V_DG_XMAP_810(ALL), 0x0, 0);
	    /* 
	     * clear the send ALPHA bit 
	     */
	    venice_set_vcreg(V_DG_XMAP_ALPHA(ALL), 0x0, 0);
	    break;
	case PIX26_RGB8:
	    /* 
	     * XMAP (8/10) - set this bit, for 8 bit/component operation.
	     */
	    venice_set_vcreg(V_DG_XMAP_810(ALL), 0x1, 0);
	    /* 
	     * clear the send ALPHA bit.
	     */
	    venice_set_vcreg(V_DG_XMAP_ALPHA(ALL), 0x0, 0);
	    break;
	default:
	    xprintf("venice_dg2initXMAPRegs(): illegal pixel density %d\n");
	    xprintf("venice_dg2initXMAPRegs(): defaulting to RGB 10\n");
	    venice_set_vcreg(V_DG_XMAP_810(ALL), 0x0, 0);
	    venice_set_vcreg(V_DG_XMAP_ALPHA(ALL), 0x0, 0);
	    break;
    }
    /*
     * Always clear the LSB0 bit; this causes the XMAP6 parts on RE to 
     * replicate the top two MSB's as the LSB's when we are in 8 bit/component
     * operation.  When we are in 10 bit/component operation, this bit
     * setting is ignored.  We plan on fixing this functionality (replicate
     * msb's when transmitting < full functional range) in future generations
     * of XMAP hardware, so it is correct to fix it here as well.
     */
    venice_set_vcreg(V_DG_XMAP_LSB0(ALL), 0x0, 0);

    /*
     * We must reload the programmable clock divider after the XMAPs have been
     * reset.
     */
    venice_load_clockDivider(hrate_picoseconds);
}


/* ARGSUSED */
void
venice_move_cursor(int x, int y)
{
#define	DOUBLE(x)	((x)<<8 | (x))
    int cx, cy;

    cx = x + VEN_info.vof_head_info[0].vof_file_info.cursor_fudge_x -
	glyph.hotx - VEN_info.vof_head_info[0].pan_x;
    cy = y + VEN_info.vof_head_info[0].vof_file_info.cursor_fudge_y -
	glyph.hoty - VEN_info.vof_head_info[0].pan_y;
    
    /* change y since GL and video use different coordinates */

    SET_CURSOR_ADDR(V_DG_CURSOR_XLOW);

    venice_set_vcreg(V_DG_CURSOR_REGDATA, DOUBLE(cx&0xff), 0);
    venice_set_vcreg(V_DG_CURSOR_REGDATA, DOUBLE(cx>>8), 0);

    venice_set_vcreg(V_DG_CURSOR_REGDATA, DOUBLE(cy&0xff), 0);
    venice_set_vcreg(V_DG_CURSOR_REGDATA, DOUBLE(cy>>8), 0);
#undef	DOUBLE
}


void venice_dg2initRegs(void)
{
    int i, vdrc_pixelSize;

    
    /* make sure vwalk and hwalk are disabled */
    venice_set_vcreg(V_DG_INITREG, 0, 0);

    venice_dg2initXMAPRegs();
    venice_dg2initIMPS();

    /* VDRC mode register: VEN_info.rm_count, disp_pix_size = 32 */
    /*
     * Program the VDRC mode register based upon the current
     * pixel density as computed by venice_calcframebuffer().
     *
     * We'll just look up what pixel density we need here, and program
     * the VDRC mode regsiter outside the switch statement (so that we only
     * have to compute it once).
     */
    switch (VEN_info.pixel_density) {
	case PIX42_RGBA10:
	    vdrc_pixelSize = DG_VDRC_PIXSIZE_42;
	    break;
	case PIX32_RGB10:
	    vdrc_pixelSize = DG_VDRC_PIXSIZE_32;
	    break;
	case PIX26_RGB8:
	    vdrc_pixelSize = DG_VDRC_PIXSIZE_26;
	    break;
	default:
	    xprintf("venice_dg2initRegs(): illegal pixel density %d\n");
	    xprintf("venice_dg2initRegs(): defaulting to RGB 10\n");
	    vdrc_pixelSize = DG_VDRC_PIXSIZE_32;
	    break;
    }

    /*
     * The VDRC mode varies depending on whether VS2 is running.  When in VS2
     * mode, the VDRC expects channel numbers to precede the VDRC record
     * which actually uses that record.  The VS2 software generates the
     * modified VDRC records; however, the standard VDRC does not, so do not
     * set the VS2 mode flag. 
     */
    {
	unsigned        modeSetting;

	modeSetting = ((((VEN_info.rm_count - 1) << 2) & 0xc) | vdrc_pixelSize);
	venice_set_vcreg(V_DG_VDRC_MODE, modeSetting, 0);
    }

    /* di_cntl_reset true */
	venice_set_vcreg(V_DG_DI_CNTL_RESET_L, 0x1, 0);
	venice_set_vcreg(V_DG_DI_CNTL_RESET_L, 0x0, 0);
    /* di_cntl_reset false */
	venice_set_vcreg(V_DG_DI_CNTL_RESET_L, 0x1, 0);

    /* Disable swap ready */
    venice_set_vcreg(V_DG_SWAP_READY, 0x1, 0);

    /*
     * initialize RGB dac
     * 5 way interleave, turn on altgamma
     * enable sync on desired channels, use CBLNK for PLL source
     */
    SET_RGBDAC_ADDR(V_DG_RGBDAC_CMD_0);
    venice_set_vcreg(V_DG_RGBDAC_REGDATA, 0xd0d0d0, 0);
    SET_RGBDAC_ADDR(V_DG_RGBDAC_CMD_1);
    venice_set_vcreg(V_DG_RGBDAC_REGDATA, 0x000000, 0);

    /*
     * initialize alpha dac
     */
    SET_ALPHADAC_ADDR(V_DG_ALPHADAC_CMD_0);
    venice_set_vcreg(V_DG_ALPHADAC_REGDATA, 0xc0, 0);
    SET_ALPHADAC_ADDR(V_DG_ALPHADAC_CMD_1);
    venice_set_vcreg(V_DG_ALPHADAC_REGDATA, 0x00, 0);

    /*
     * other RGB and alpha dac initialization
     */
    SET_RGBDAC_ADDR(V_DG_RGBDAC_PIX_READ_MASK_LO);
    venice_set_vcreg(V_DG_RGBDAC_REGDATA, 0xffffff, 0);
    SET_RGBDAC_ADDR(V_DG_RGBDAC_PIX_READ_MASK_HI);
    venice_set_vcreg(V_DG_RGBDAC_REGDATA, 0xffffff, 0);

    SET_ALPHADAC_ADDR(V_DG_ALPHADAC_PIX_READ_MASK_LO);
    venice_set_vcreg(V_DG_ALPHADAC_REGDATA, 0xffffff, 0);
    SET_ALPHADAC_ADDR(V_DG_ALPHADAC_PIX_READ_MASK_HI);
    venice_set_vcreg(V_DG_ALPHADAC_REGDATA, 0xffffff, 0);

    SET_RGBDAC_ADDR(V_DG_RGBDAC_PIX_BLINK_MASK_LO);
    venice_set_vcreg(V_DG_RGBDAC_REGDATA, 0x000000, 0);
    SET_RGBDAC_ADDR(V_DG_RGBDAC_PIX_BLINK_MASK_HI);
    venice_set_vcreg(V_DG_RGBDAC_REGDATA, 0x000000, 0);

    SET_ALPHADAC_ADDR(V_DG_ALPHADAC_PIX_BLINK_MASK_LO);
    venice_set_vcreg(V_DG_ALPHADAC_REGDATA, 0x000000, 0);
    SET_ALPHADAC_ADDR(V_DG_ALPHADAC_PIX_BLINK_MASK_HI);
    venice_set_vcreg(V_DG_ALPHADAC_REGDATA, 0x000000, 0);

    /* 
     * Load linear ramp into primary pallettes; map 10 bits into 8 bits.
     */
    SET_RGBDAC_ADDR(0x0);
    for (i = 0; i < DG_DAC_PRIMRAM_SIZE; i++) {
#ifdef BETTER_GAMMA
	/* Load a reasonable gamma table into all 1024 slots */
	unsigned char val = 255.0*pow(i/1023.0,1.0/1.7)+0.5;
#else
	unsigned char val = g1_7[i];
#endif
        venice_set_vcreg(V_DG_RGBDAC_PRIMDATA, (val | (val<<8) | (val<<16)), 0);
    }
    SET_RGBDAC_ADDR(0x0);
    /* load the alt gamma data with 1.7 also... only 8 bits */
    for (i = 0; i < DG_DAC_ALTRAM_SIZE; i++) {
#ifdef BETTER_GAMMA
	/* Load a reasonable gamma table into all 256 slots */
	unsigned char val = 255.0*pow(i/255.0,1.0/1.7)+0.5;
#else
	unsigned char val = (255*i)/DG_DAC_ALTRAM_SIZE;
#endif
        venice_set_vcreg(V_DG_RGBDAC_ALTDATA, (val | (val<<8) | (val<<16)), 0);
    }
    SET_ALPHADAC_ADDR(0x0);
    for (i = 0; i < (DG_DAC_PRIMRAM_SIZE >> 2); i++) {
        venice_set_vcreg(V_DG_ALPHADAC_PRIMDATA, i, 0);
        venice_set_vcreg(V_DG_ALPHADAC_PRIMDATA, i, 0);
        venice_set_vcreg(V_DG_ALPHADAC_PRIMDATA, i, 0);
        venice_set_vcreg(V_DG_ALPHADAC_PRIMDATA, i, 0);
    }

    /* 
     * Load linear ramp (reverse) into alt pallettes;  8 bits.
     */
    SET_RGBDAC_ADDR(0x0);
    for (i = DG_DAC_ALTRAM_SIZE-1; i >= 0; i--) {
        venice_set_vcreg(V_DG_RGBDAC_ALTDATA, (i | (i<<8) | (i<<16)), 0);
    }
    SET_ALPHADAC_ADDR(0x0);
    for (i = DG_DAC_ALTRAM_SIZE-1; i >= 0; i--) {
        venice_set_vcreg(V_DG_ALPHADAC_ALTDATA, i, 0);
    }

    SET_RGBDAC_ADDR(V_DG_RGBDAC_OVLAY_READ_MASK);
    /* Enable RGB overlay planes for GE10 arcs prom */
    venice_set_vcreg(V_DG_RGBDAC_REGDATA, 0xffffff, 0);

    /* Disable Alpha overlay planes */
    SET_RGBDAC_ADDR(V_DG_ALPHADAC_OVLAY_READ_MASK);
    venice_set_vcreg(V_DG_ALPHADAC_REGDATA, 0x000000, 0);

    /* Disable overlay blink */
    SET_RGBDAC_ADDR(V_DG_RGBDAC_OVLAY_BLINK_MASK);
    venice_set_vcreg(V_DG_RGBDAC_REGDATA, 0x000000, 0);
    SET_ALPHADAC_ADDR(V_DG_ALPHADAC_OVLAY_BLINK_MASK);
    venice_set_vcreg(V_DG_ALPHADAC_REGDATA, 0x000000, 0);

    /* Disable testreg */
    SET_RGBDAC_ADDR(V_DG_RGBDAC_TESTREG);
    venice_set_vcreg(V_DG_RGBDAC_REGDATA, 0x000000, 0);
    SET_ALPHADAC_ADDR(V_DG_ALPHADAC_TESTREG);
    venice_set_vcreg(V_DG_ALPHADAC_REGDATA, 0x000000, 0);

    /* CURSOR */

    /* init command register (5:1 muxing, glyph enable) = 0x48 */
    SET_CURSOR_ADDR(0x0);
    venice_set_vcreg(V_DG_CURSOR_REGDATA, 0x4848, 0);
  

    /* load glyph */
    SET_CURSOR_ADDR(0x0);
    {
	int j;
	unsigned char *b = glyph.bitmap;

	for ( i=0 ; i<glyph.h ; i++ )  {
	    for ( j=0 ; j<glyph.cper ; j++,b++ )  {
		venice_set_vcreg(V_DG_CURSOR_RAMDATA, (*b<<8) | *b, 0);
	    }
	    for (  ; j<8 ; j++ )  {
		venice_set_vcreg(V_DG_CURSOR_RAMDATA, 0x0, 0);
	    }
	}
	for ( j=8*(64-i) ; j-- > 0 ; )
	    venice_set_vcreg(V_DG_CURSOR_RAMDATA, 0x0, 0);
    }

    /* init position */
    /* x position = 0x380 (default) */
    /* xlow byte = 0x80, xhigh byte = 0x03   */
    /* y position = 0x200 (default) */
    venice_move_cursor(0x380, 0x200);

    /*  Write first 4 locations of overlay planes (RGB DACS) */
    SET_RGBDAC_ADDR(0x0100);
    venice_set_vcreg(V_DG_RGBDAC_OVLDATA,0x000000, 0);
    venice_set_vcreg(V_DG_RGBDAC_OVLDATA,0x0000ff, 0); /* red */
    venice_set_vcreg(V_DG_RGBDAC_OVLDATA,0x0000ff, 0); /* red */
    venice_set_vcreg(V_DG_RGBDAC_OVLDATA,0x0000ff, 0); /* red */

    /*
     * Center the digital pots.
     */
    venice_center_dpots();
}

void venice_dg2initVlist(void)
{
    int saddr;

    /* PERM SUBROUTINE - return */
    venice_set_vcreg(STAGE_PERM_STUB, STAGE_TAG_RETURN, 0);

    /* write PERM vlist entry */
    venice_set_vcreg(STAGE_VLIST_PERM_ENTRY, STAGE_TAG_CALL(STAGE_PERM_STUB),0);

    /* enable vlist (won't actually start until the VOF is fired up) */
    venice_set_vcreg(V_DG_INITREG, V_DG_VWALK_ENABLE, 0);

    /* build the dac clock reset sequence in the DG staging area */
    saddr = STAGE_DAC_CLK_RESET;
    venice_set_vcreg(saddr++, STAGE_TAG_ADDR(V_DG_CLK_RESET_L), 0);
    venice_set_vcreg(saddr++, 1, 0);
    venice_set_vcreg(saddr++, 0, 0);
    venice_set_vcreg(saddr++, 1, 0);
    venice_set_vcreg(saddr++, STAGE_TAG_RETURN, 0);
}

/*
 * This should stop vof and vdrc at vertical retrace, but can't because it
 * can be called from the diag branch of code, which can't depend on having
 * the kernel initialized VLIST in place.
 */
void venice_dg2initStopVof(void) 
{
    venice_set_vcreg( 0xb100, 0x1, 0); /* stop vof */
}

/*
 * venice_dg2resetVof() resets the IMPs and XMAPs so that all the video fifo
 * pointers are zeroed.  The VOF is NOT started, so that when gfxinit calls
 * the GFX_START ioctl, the kernel can start the VOF for us after the VLIST
 * has been initialized (allowing for a synchronous VLIST startup).
 */
void venice_dg2resetVof(void) 
{
    /* 
     * Reset RMs, reset XMAPs, then restore the programmable hrate divider
     * by calling venice_dg2initXMAPRegs() (which we need to do anyway, 
     * because all of the XMAP register contents get hammered when the XMAP
     * is reset).
     */
    venice_dg2resetRM();
    venice_dg2initIMPS();
    venice_dg2initResetXmap();
    venice_dg2initXMAPRegs();
}

/*
 * Only gets called by PROM and diags; should never get called by a libvenice
 * function which needs to have the VLIST running.
 */
void venice_dg2initStartVof(void) 
{
    venice_dg2resetVof();
    venice_set_vcreg(V_DG_VOF_CNTRL, (vof_control_reg_value & 0xfe), 0);
}

/*
 * Reset XMAPs by writing to the V_DG_INITREG register.
 */
void
venice_dg2initResetXmap(void)
{
 /* The following state is clobbered by xmap_reset:
  *
  *	sig_sel = signature mux address, addr 0x31
  *	sig_en  = signature enable bit,  addr 0x34
  *	send_8_bits_ = 8/10 bits per component, addr 0x35
  *	send_alpha = expect alpha to be sent,   addr 0x36
  *	zero_lsbs = 
  *	  zero 2 lsbs of 10-bit xmap output when send_8_bits is true, addr 0x39
  *	qlsbs = Q[3:0]
  *	q4    = Q[4]
  *	q5    = Q[5]
  *	q6    = Q[6]
  *
  *  The only other XMAP6 state affected by reset is
  *  the vfifo read and write pointers, which are set to 0.
  *  They are neither readable nor writeable on the vcbus.
  */

    venice_set_vcreg(V_DG_INITREG, V_DG_XMAP_RESET, 0);
    sginap(5);
    venice_set_vcreg(V_DG_INITREG, 0, 0);
}

#define CHAIN_LENGTH    (40*4)

/* ARGSUSED */
static void rm_reset(int rm) 
{
    jvector_t v[jvsize(CHAIN_LENGTH)];
#define memset(p, v, n) {int i; for(i = 0; i < n; i++) p[i] = v; }

    static char *name[] = {	"VENICE_RM0", "VENICE_RM1", 
				"VENICE_RM2", "VENICE_RM3"};

    int _jt_select_chain(char *);
    int _jt_send_instruction(int, char *, char *);

    memset(v, 0x88, sizeof v);          /* reset */
    _jt_select_chain(name[rm]);
    _jt_send_instruction(CHAIN_LENGTH, v, (jvector_t *)0);
    memset(v, 0xff, sizeof v);          /* bypass */
    _jt_send_instruction(CHAIN_LENGTH, v, (jvector_t *)0);
#undef memset
}

void venice_dg2resetRM(void)
{
    int i;

    /*
     * Only initialize for as many RM's as we have in the system.
     * VEN_info.rm_count is read in from the DG2 hardware by the kernel
     */
    for(i = 0; i < VEN_info.rm_count; i++) {
	rm_reset(i);
    }
}

/*
 * Use JTAG to probe for the DG2 board.  Returns TRUE if it's there, FALSE
 * otherwise.
 */
int venice_dg_present(void) 
{
    jvector_t v[jvsize(1000)];
    int id;

    int _jt_select_chain(char *);
    int _jt_send_data(int, char *, char *);
    int vector_to_int(int *, jvector_t *);

    _jt_select_chain("VENICE_DG");
    _jt_send_data(1000, v, v);
    vector_to_int(&id, v);

    return (id == 0x1808057);
}

#define RED	0
#define GREEN	1
#define BLUE 	2
#define ALPHA 	3
#define SYNC 	4
#define DOWN	0
#define UP	1
#define SELECT    0x0
#define DESELECT  0x1
#define DPOT_MAX_COUNT  100    /* from xicor spec sheet */ 
#define DPOT_TWEAK_VAL   93    /* from empirical science */

/*
 * The RGB dpot adjust feature is decidely non-linear, due to a parallel
 * resistive network implementation (instead of a pure voltage divider).
 */
void venice_center_dpots(void)
{
    int i, dpot_setting;

    venice_set_vcreg(V_DG_DPOT_UD, DOWN, 0);
    venice_set_vcreg(V_DG_RED_DPOT_SEL, SELECT, 0);
    venice_set_vcreg(V_DG_GREEN_DPOT_SEL, SELECT, 0);	
    venice_set_vcreg(V_DG_BLUE_DPOT_SEL, SELECT, 0);	
    venice_set_vcreg(V_DG_ALPHA_DPOT_SEL, SELECT, 0);	
    venice_set_vcreg(V_DG_SYNC_DPOT_SEL, SELECT, 0);	

    /* 
     * See if the user has provided a dpot setting to override 
     * our empirically derived default.
     */
    dpot_setting = DPOT_TWEAK_VAL;

    for (i = 0; i < DPOT_MAX_COUNT; i++) {
        venice_set_vcreg(V_DG_SERIAL_CLOCK, 0x1, 0);
        sginap(1);
    }

    /*
     * Leave the sync dpot zeroed (if we ever want to center it in the
     * adjustable range, we'd have to make a change to the vof.mcs file).
     */
    venice_set_vcreg(V_DG_SYNC_DPOT_SEL, DESELECT, 0);	

    venice_set_vcreg(V_DG_DPOT_UD, UP, 0);
    for (i = 0; i < dpot_setting; i++) {
        venice_set_vcreg(V_DG_SERIAL_CLOCK, 0x1, 0);
        sginap(1);
    }
}

/* ARGSUSED */
void
set_dac_cmd2(int rgbcmd2, int alphacmd2)
{
    SET_RGBDAC_ADDR(V_DG_RGBDAC_CMD_2);
    venice_set_vcreg(V_DG_RGBDAC_REGDATA, rgbcmd2, 0);
    SET_ALPHADAC_ADDR(V_DG_ALPHADAC_CMD_2);
    venice_set_vcreg(V_DG_ALPHADAC_REGDATA, alphacmd2, 0);
}


void venice_grok_dg2_eeprom_error(int errcode);

/* read the contents of eeprom.
 * returns 1 if OK, 0 if bad.
 */

/* ARGSUSED */
int
venice_get_dg2_eeprom(venice_dg2_eeprom_t *eeprom)
{
    int i;
    unsigned char *p, ch;
    unsigned int size, chksum;



    p = (unsigned char *) eeprom;
    for (i = 0; i < 16; i++) {
	*p++ = (unsigned char) venice_get_vcreg(DG_EEPROM_BASE_ADDR + i);
    }

    if ((eeprom->prom_id & 0x0000ffff) != DG2_EEPROM_ID) {
	xprintf("ERROR: RealityEngine configuration eeprom contents invalid (id)\n");
	xprintf("exp 0x%x, read 0x%x\n", DG2_EEPROM_ID, eeprom->prom_id);
	/* id word is wrong */
	return(0);
    }

    if ((eeprom->prom_id & 0xffff0000) != 0) {
	xprintf("WARNING: RealityEngine configuration eeprom contents altered\n");
    }

    if (eeprom->prom_revision != DG2_EEPROM_REV) {
	if ((eeprom->prom_revision >= DG2_EEPROM_REV_ONE)
	 && (eeprom->prom_revision < DG2_EEPROM_REV)) {
	    xprintf("RealityEngine configuration eeprom contents obsolete; run setmon -x to fix.\n");
	} else {
	    xprintf("ERROR: RealityEngine configuration eeprom contents invalid (rev)\n");
	    xprintf("exp 0x%x, read 0x%x\n", DG2_EEPROM_REV, eeprom->prom_revision);
	}
	/* revision is wrong */
	return(0);
    }

    size = eeprom->prom_length;
    if (size > DG_EEPROM_SIZE) {
	xprintf("ERROR: RealityEngine configuration eeprom contents invalid (size)\n");
	xprintf("exp 0x%x, read 0x%x\n", DG_EEPROM_SIZE, size);
	/* fundamental error - part is not big enough */
	return(0);
    }

    if (size > sizeof(*eeprom))
	size = sizeof(*eeprom);

    chksum = 0;
    for (i = 16; i < size; i++) {
	ch = (unsigned char) venice_get_vcreg(DG_EEPROM_BASE_ADDR + i);
	*p++ = ch;
	chksum += ch;
    }

    if (chksum != eeprom->prom_checksum) {
	xprintf("ERROR: RealityEngine configuration eeprom checksum invalid\n");
	xprintf("exp 0x%x, read 0x%x\n", chksum, eeprom->prom_checksum);
	/* checksum is wrong */
	return(0);
    }

    return(1);
}



int _jt_select_chain(char *chain)
{
    return(fcg_jt_select_chain(chain));
}


int _jt_send_instruction(int nbits, char *inst, char *ret)
{
    return(fcg_jt_send_instruction(nbits, inst, ret));
}


int _jt_send_data(int nbits, char *data, char *ret)
{
    return(fcg_jt_send_data(nbits, data, ret));
}


int vector_to_int(int *x, jvector_t *v)
{
    return *x = v[0] | (v[1] << 8) | (v[2] << 16) | (v[3] << 24);
}

int get_ven_type(void) { return VEN_FCG; }
/*
 *  venice_jtag.c - venice jtag driver
 */


#ifndef JTAGDEFS
#define JTAGDEFS
#define STALL()	sginap(0)

struct jproc_vector {
    int (*jt_init)(void);
    int (*jt_select_chain)(char *);
    int (*jt_soft_reset)(void);
    int (*jt_hard_reset)(void);
    int (*jt_test_thyself)(void);
    int (*jt_send_instruction)(int, jvector_t *, jvector_t *);
    int (*jt_send_data)(int, jvector_t *, jvector_t *);
};
#endif


extern volatile unsigned int *ven_pipeaddr;

/*
 *  mirage_jtag.c - mirage jtag driver
 */

#if !defined(IP19) && !defined(IP21) && !defined(IP25)
#include "sys/venice_fcg.h"
#endif

#ifndef JTAGDEFS
#define JTAGDEFS

struct jproc_vector {
    int (*jt_init)(void);
    int (*jt_select_chain)(char *);
    int (*jt_soft_reset)(void);
    int (*jt_hard_reset)(void);
    int (*jt_test_thyself)(void);
    int (*jt_send_instruction)(int, jvector_t *, jvector_t *);
    int (*jt_send_data)(int, jvector_t *, jvector_t *);
};
#endif

struct jtag {
    int control;	/* jtag control register */
    int fill0;
    int data;		/* jtag data */
    int fill1;
    int tms;		/* jtag tms */
    int fill2;
    int status;		/* jtag status */
    int fill3;
};


extern volatile struct fcg *fcg_base_va;	

#define FCG_SETUP_JTAG				\
	volatile struct jtag *JTAG; 		\
	JTAG = (struct jtag *)((unsigned long)(fcg_base_va) + (0x1500 << 3));

static struct fcg_chain {
    char *chain_name;
    int  bd_select;
} fcg_chain[] = {
	"VENICE_RM0", FCG_CTL_JSEL_RM0,
	"VENICE_RM1", FCG_CTL_JSEL_RM1,
	"VENICE_RM2", FCG_CTL_JSEL_RM2,
	"VENICE_RM3", FCG_CTL_JSEL_RM3,
	"VENICE_DG",  FCG_CTL_JSEL_DG,
	"VENICE_GE",  FCG_CTL_JSEL_GE,
};

#define N_CHAINS	6


fcg_jt_init(void) {
    return 0;
}

static int fcg_bd_id;

fcg_jt_soft_reset(void) {
    FCG_SETUP_JTAG;

    /* send a pattern of 5 1's */
    JTAG->tms = 0x01f;
    /* slip into 1 bit shift mode */
    JTAG->control = fcg_bd_id | FCG_CTL_JTAG_SHIFT1;
    JTAG->data = 0;
    while(!(JTAG->status&FCG_JTAG_READY_MASK)) sginap(0);
    return 0;
}

fcg_jt_hard_reset(void) {
    /* not supported */
    return -1;
}

/*
 *  fcg_jt_test_thyself() - controller self test
 */
fcg_jt_test_thyself(void) {
    return 0;
}

fcg_jt_select_chain(char *chain) {
    int i;
    FCG_SETUP_JTAG;

    xprintf("JTAG == 0x%x, fcg_base_va == 0x%x\n", JTAG, fcg_base_va);
    
    for(i = 0; i < N_CHAINS; i++)
	if (!strcmp(fcg_chain[i].chain_name, chain)) goto found;
    return -1;
found:
    JTAG->control = fcg_bd_id = fcg_chain[i].bd_select;
    return 0;
}

int
fcg_jt_send_instruction(int nbits, char *inst, char *ret) {
    int nbytes = nbits/8;
    int once = 0;
    FCG_SETUP_JTAG;

    if (nbits <= 0) return -1;

    /* assert TAP is in Run-Test-Idle state */

    /*
     * goto to Shift-IR state and after data is shifted goto Pause-IR
     * 1 -> 1 -> 0 -> 0 -> ... -> 1 -> 0
     */
    JTAG->control = fcg_bd_id;
    JTAG->tms = 0x13;

    nbits &= 7;
    while(nbytes--) {
	JTAG->data = *inst++;
	while(!(JTAG->status&FCG_JTAG_READY_MASK)) sginap(0);
	if (ret) *ret++ = JTAG->data;
	/*
	 * hang out in Pause-IR for 2 clocks then go back to Shift-IR
	 * then go back to Pause-IR
	 * 0 -> 0 -> 1 -> 0 -> ... -> 1 -> 0
	 */
	if (!once) {
	    JTAG->tms = 0x14;
	    once++;
	}
    }
    /* do residual bits in 1 bit mode */
    if (nbits) {
	int d = *inst;
	int r = 0;
	int s = 8-nbits;
	JTAG->control = fcg_bd_id | FCG_CTL_JTAG_SHIFT1;
	while(nbits--) {
	    r >>= 1;
	    JTAG->data = d&1;
	    while(!(JTAG->status&FCG_JTAG_READY_MASK)) sginap(0);
	    r |= JTAG->data&0x80;
	    d >>= 1;
	    if (!once) {
		JTAG->tms = 0x14;
		once++;
	    }
	}
	if (ret) *ret = r >> s;
    }
    /*
     * go from Pause-IR to Exit2-IR to Update-IR to Run-Test-Idle
     * 1 -> 1 -> 0 -> ...
     */
    JTAG->control = fcg_bd_id | FCG_CTL_JTAG_SHIFT1;
    JTAG->tms = 3;
    JTAG->data = 0;
    while(!(JTAG->status&FCG_JTAG_READY_MASK)) sginap(0);
    return 0;
}

int
fcg_jt_send_data(int nbits, char *data, char *ret) {
    int nbytes = nbits/8;
    int once = 0;
    FCG_SETUP_JTAG;

    if (nbits <= 0) return -1 ;

    /* assert TAP is in Run-Test-Idle state */

    /*
     * goto to Shift-DR state and after data is shifted goto Pause-DR
     * 0 -> 1 -> 0 -> 0 -> ... -> 1 -> 0
     */
    JTAG->control = fcg_bd_id;      /* make sure we are in 8-bit mode */
    JTAG->tms = 0x12;

    nbits &= 7;
    while(nbytes--) {
	JTAG->data = *data++;
	while(!(JTAG->status&FCG_JTAG_READY_MASK)) sginap(0);
	if (ret) *ret++ = JTAG->data;
	/*
	 * hang out in Pause-DR for 2 clocks then go back to Shift-DR
	 * then go back to Pause-IR
	 * 0 -> 0 -> 1 -> 0 -> ... -> 1 -> 0
	 */
	if (!once) {
	    JTAG->tms = 0x14;   /* XXX only need to do this once */
	    once++;
	}
    }
    /* do residual bits in 1 bit mode */
    if (nbits) {
	int d = *data;
	int r = 0;
	int s = 8-nbits;
	JTAG->control = fcg_bd_id | FCG_CTL_JTAG_SHIFT1;
	while(nbits--) {
	    r >>= 1;
	    JTAG->data = d&1;
	    while(!(JTAG->status&FCG_JTAG_READY_MASK)) sginap(0);
	    r |= JTAG->data&0x80;
	    d >>= 1;
	    if (!once) {
		JTAG->tms = 0x14;
		once++;
	    }
	}
	if (ret) *ret = r >> s;
    }
    /*
     * go from Pause-DR to Exit2-DR to Update-DR to Run-Test-Idle
     * 1 -> 1 -> 0 -> ...
     */
    JTAG->control = fcg_bd_id | FCG_CTL_JTAG_SHIFT1;
    JTAG->tms = 3;
    JTAG->data = 0;
    while(!(JTAG->status&FCG_JTAG_READY_MASK)) sginap(0);
    return 0;
}

struct jproc_vector fcg_jt_proc_vector = {
    fcg_jt_init,
    fcg_jt_select_chain,
    fcg_jt_soft_reset,
    fcg_jt_hard_reset,
    fcg_jt_test_thyself,
    fcg_jt_send_instruction,
    fcg_jt_send_data,
};


void
venice_rmfifo(int rm, int pgib, int taib, int pgta) {
    static char *rm_code[] = { "VENICE_RM0",
			"VENICE_RM1",
			"VENICE_RM2",
			"VENICE_RM3", };

    /* offsets into the scan chain */
    static int ib_offset[5] = { 72, 55, 38, 21, 4, };
    static int ta_offset[5] = { 80, 63, 46, 29, 12, };

			 /* MM    MM    DB    PA	*/
    static char inst[] = {0xff, 0xff, 0xfc, 0xfb,	/* E */
			  0xff, 0xff, 0xfc, 0xfb,	/* D */
			  0xff, 0xff, 0xfc, 0xfb,	/* C */
			  0xff, 0xff, 0xfc, 0xfb,	/* B */
			  0xff, 0xff, 0xfc, 0xfb,};	/* A */
#define FIFO_CHAIN_LEN 85
    char data[FIFO_CHAIN_LEN];
    int ib_value, ta_value, i, j;

    ib_value = (((pgib >> 2)&0xf) << 3) | ((taib >> 2)&7);
    ta_value = pgta&0xf;

#define setbit(v, n, b)	(v[n>>3] = (v[(n)>>3] & ~(1<<((n)&7))) | ((b)<<((n)&7)))
    for(i = 0; i < 5; i++) {
	/* IB is 7 bits */
	for (j = 0; j < 7; j++)
	    setbit(data, ib_offset[i]+j, (ib_value >> j)&1);

	/* TA is 4 bits */
	for (j = 0; j < 7; j++)
	    setbit(data, ta_offset[i]+j, (ta_value >> j)&1);
    }
#undef setbit
    _jt_select_chain(rm_code[rm]);
    _jt_send_instruction(8*sizeof inst, inst, (char *)0);
    _jt_send_data(FIFO_CHAIN_LEN, data, (char *)0);
    /*
    xprintf("imprev %d\nibrev %d\n", venice_rm_imprev(), venice_rm_ibrev());
    */
}


int
venice_fifo_hack(int verbose,
    int fifoopt, int fifo_pgib, int fifo_taib, int fifo_pgta)
{
    int rm;

    if (!fifoopt) {
	fifo_pgib = 112;
	fifo_taib = 24;
	fifo_pgta = 19;
    }

    if (verbose)
    	xprintf("rmfifo %d %d %d\n", fifo_pgib, fifo_taib, fifo_pgta);
    for(rm = 0; rm < VEN_info.rm_count; rm++)
	venice_rmfifo(rm, fifo_pgib, fifo_taib, fifo_pgta);

    return 0;
}


#if 1
char *rm_chain[] = { "VENICE_RM0", "VENICE_RM1", "VENICE_RM2", "VENICE_RM3" };

static void xmemset(char *p, int v, int n) {int i; for(i = 0; i < n; i++) p[i] = v; }

#define IMP_REV0	0x0c30c30b	/* imp6 */
#define IMP_REV1	0x1451450b	/* imp6b */
#define IMP_REV2	0x0810810b	/* imp7 */

#define IB_REV0		0x0180a057	/* ib1 */
#define IB_REV1		0x1180a057	/* ib1a */

#define IBcount		5
#define IMPcount	20

int impoff[20] = {     0,      32,      64,      96,
		   366+0,  366+32,  366+64,  366+96,
		   732+0,  732+32,  732+64,  732+96,
		  1098+0, 1098+32, 1098+64, 1098+96,
		  1464+0, 1464+32, 1464+64, 1464+96,
		 };
int iboff[5] = {128, 366+128, 732+128, 1098+128, 1464+128 };

static
extract(char *v, int o, int n) {
#define getbit(v,x) (((v)[(x)>>3] >> ((x)&7))&1)
    int i, x = 0;
    for(i = 0; i < n; i++)
	x |= (getbit(v,o+i) << i);
    return x;
#undef getbit
}

int
venice_imprev(int rm) {
    int id, i;
    char v[2048/8];
    int rev = 2;

    _jt_select_chain(rm_chain[rm]);
    /* make sure instruction is idcode */
    xmemset(v, 0x11, 40/2);
    _jt_send_instruction(40*4, v, v);
    _jt_send_data(2048, v, v);
    for (i = 0; i < IMPcount; i++) {
	id = extract(v, impoff[i], 32);
	if (id == IMP_REV1 && rev == 2) rev = 1;
	else if (id == IMP_REV0) {
	    rev = 0;
	    break;
	}
    }
    return rev;
}

int
venice_rm_imprev(void) {
    int RMcount = VEN_info.rm_count;
    int rm;
    int rev = 2;

    for(rm = 0; rm < RMcount; rm++) {
	int r = venice_imprev(rm);
	if (rev == 2 && r == 1) rev = 1;
	else if (r == 0) {
	    rev = 0;
	    break;
	}
    }
    return rev;
}

int
venice_ibrev(int rm) {
    int id, i;
    char v[2048/8];
    int rev = 1;


    _jt_select_chain(rm_chain[rm]);
    /* make sure instruction is idcode */
    xmemset(v, 0x11, 40/2);
    _jt_send_instruction(40*4, v, v);
    _jt_send_data(2048, v, v);
    for (i = 0; i < IBcount; i++) {
	id = extract(v, iboff[i], 32);
	if (id == IB_REV0) {
	    rev = 0;
	    break;
	}
    }
    return rev;
}

int
venice_rm_ibrev(void) {
    int RMcount = VEN_info.rm_count;
    int rm;
    int rev = 1;

    for(rm = 0; rm < RMcount; rm++) {
	int r = venice_ibrev(rm);
	if (r == 0) {
	    rev = 0;
	    break;
	}
    }
    return rev;
}

/* ARGSUSED */
int venice_pgrev(int rm) { return 0; }	/* XXX */
/* ARGSUSED */
int venice_tdrev(int rm) { return 1; }	/* XXX */
/* ARGSUSED */
int venice_tarev(int rm) { return 0; }	/* XXX */
int venice_bdrev(int rm); /* prototype */

int
venice_rm_rev(int rm) {
    return  venice_bdrev(rm) << 20 |
	    venice_pgrev(rm) << 16 |
	    venice_tarev(rm) << 12 |
	    venice_tdrev(rm) <<  8 |
	    venice_ibrev(rm) <<  4 |
	    venice_imprev(rm) << 0;
}

int
venice_ifrev(void) {  return get_ven_type() ? 0 : 1; }	/* XXX */

static char *ge_chain = "VENICE_GE";

#define CP_REV0		0x6d6d6d6d	/* cp2.0 */
#define CP_REV1		0x7d7d7d7d	/* cp2.1 */
static struct cpvec {
    int length;
    int cp_off;
} cpvec[] =  { 17*4, 32*16,		/* GE8 */
	       25*4, 32*24,		/* GE10 */
	       13*4, 32*12,};		/* VTX */

int
venice_cprev(void) {
    int id, i;
    char v[2048/8];
    int rev;
    int gerev;

    if (!get_ven_type()) {
	/* mpg */
	gerev = 0;
    } else {
	/* IP19, but is it a VTX? */
	volatile unsigned *r = (unsigned *) (ulong)((ven_pipeaddr == 0) ?
			(unsigned *)(ulong)0x020000 : ven_pipeaddr);

	if (((r[(0x01503<<1)] >> 4)&0x7) == 0x7)
	    gerev = 1;
	else
	    gerev = 2;
    }
    _jt_select_chain(ge_chain);
    /* make sure instruction is idcode */
    /* ugh i860 is 2, everything else is 1 */
    xmemset(v, 0x11, 40/2);
    /* patch i860s */
    for(i = 0; i < 12; i++)
	v[i] = 0x21;
    _jt_send_instruction(cpvec[gerev].length, v, v);	/* XXX */
    _jt_send_data(2048, v, v);
    id = extract(v, cpvec[gerev].cp_off, 32);
    if (id == CP_REV0) rev = 0;
    else rev = 1;
    return rev;
}

int
venice_gefrev(void) { return 1; }

int
venice_ge_rev(void) {
    return venice_ifrev() << 8 |
	   venice_cprev() << 4 |
	   venice_gefrev() << 0;
}

#define BDREV_OFF	7
int pgoff[] = { 4574, 3628, 2682, 1736, 790 };


/*
 * rm board revision is encoded in the BDREV[1..0] of PGA
 */
venice_bdrev(int rm) {
    int id;
    char v[8192/8];

    _jt_select_chain(rm_chain[rm]);
    /* make sure instruction is sample */
    xmemset(v, 0x22, 40/2);
    _jt_send_instruction(40*4, v, v);
    _jt_send_data(8192, v, v);
    id = extract(v, pgoff[0]+BDREV_OFF, 2);
    return id == 3;
}
#endif

#define TRUE	1
#define FALSE	0

int  venice_loadsection_dg2(int fd, int addr, int cnt, int magic);
static void venice_load_vof_edgeMem(vof_data_t *vofData);
static void venice_load_vof_verticalInfo(vof_data_t *vofData);

#define NO_PAGE_CROSSINGS	0
#define PAGE_CROSSINGS		1
#define ARRAY_SIZE(arr)              ((int) (sizeof(arr) / sizeof(arr[0])))
#define DEFAULT_VDRC_CHANNEL	0

int sramByteCount = 0;

#define venice_write_vdrc_sram_byte(byte) {		\
	venice_set_vcreg(V_DG_VDRC_DATA, byte, 0);	\
	++sramByteCount; }

extern int venice_rm_imprev(void);
static void venice_load_vdrc_sram(vof_data_t *vofData);

int vof_control_reg_value, vof_clock_value, vof_hwalk_length;
int vif_genlock, vof_outsync_level;
unsigned int vif_sync_width_internal; 
unsigned int vif_gensrc_cntrl_internal; 
unsigned int vif_bporch_clamp_internal, vif_bporch_clamp_shadow;
int hrate_picoseconds;

/*
 * We can determine what the V_DG_IE_CONFIG_LO register needs to be set to
 * after reading in the VOF file.  We'll set it in venice_calcframebuffer(), 
 * and use it in venice_dg2initIMPS().  Ditto for IE_CONFIG_HI.
 */
int ie_config_lo, ie_config_hi;

static unsigned char edgeAddr[] = {
    DG_VOF_CSYNC_0,
    DG_VOF_CSYNC_1,
    DG_VOF_CSYNC_2,
    DG_VOF_CSYNC_3,
    DG_VOF_CSYNC_4,
    DG_VOF_CSYNC_5,
    DG_VOF_CSYNC_6,
    DG_VOF_CBLANK_0,
    DG_VOF_CBLANK_1,
    DG_VOF_CBLANK_2,
    DG_VOF_STEREO,
    DG_VOF_HCURSOR_0,
    DG_VOF_HCURSOR_1,
    DG_VOF_HCURSOR_2,
    DG_VOF_HCURSOR_3,
    DG_VOF_VCURSOR_0,
    DG_VOF_VCURSOR_1,
    DG_VOF_VID_ADDR_REQ_0,
    DG_VOF_VID_ADDR_REQ_1,
    DG_VOF_ACTIVE_VIDEO_0,
    DG_VOF_ACTIVE_VIDEO_1,
    DG_VOF_VWALK_0,
    DG_VOF_VWALK_1,
    DG_VOF_HWALK_0,
    DG_VOF_HWALK_1,
};

#define venice_set_vof_vcreg(reg, data, flag) venice_set_vcreg(reg, data, flag)

/*
 * Configure a signal edge in VOF edge memory (if it is active).
 *
 * The vofc compiler will set the uppermost bit of the edge hcount 
 * if the signal edge does not exist on this line for this format.  
 */
void
set_edgeMem_for_edge(int line, int edge, unsigned short hcount)
{
    unsigned char highbyte;

    if (hcount != DEFHCNT) {

	/*
	 * Write 13 bits of edge memory address (lower byte, then upper 
	 * byte).  Then write to the edge memory and edge data 
	 * registers to latch this signal edge into memory at the 
	 * desired locations for this (linetype, hcount).
	 */

	venice_set_vof_vcreg(V_DG_VOF_CNTRL, 
		(DG_EDGE_LOW_EDGEMEM_ADDR | DG_VOF_STOP), 0);
	venice_set_vof_vcreg(V_DG_VERT_SEQ_COUNTER, (hcount & 0xff), 0);

	highbyte = ( (line << 1) | ((hcount >> 8) & 0x1) );
	venice_set_vof_vcreg(V_DG_VOF_CNTRL, 
		(DG_EDGE_HIGH_EDGEMEM_ADDR | DG_VOF_STOP), 0);
	venice_set_vof_vcreg(V_DG_VERT_SEQ_COUNTER, highbyte, 0);

	venice_set_vof_vcreg(V_DG_EDGE_MEMORY_ADDR, edgeAddr[edge], 0);
#ifdef JAK_DEBUG
	xprintf("jak:  wrote %#x (%d + %d) to vertSeqCntr, %#x to edge memory addr\n", 
		hcount | (line << 8), 
		hcount & 0xff, 
		highbyte >> 1, 
		edgeAddr[edge]);
#endif
	venice_set_vof_vcreg(V_DG_EDGE_MEMORY_DATA, 0xff, 0);
    }
}

/*
 * Load the edge memory.
 */
static void venice_load_vof_edgeMem(vof_data_t *vofData)
{
    int line, edge, length;
    unsigned char highbyte;

    /*
     * Write all possible edge position values (compiler will set hcount to 
     * 0x8000 for any line types's unused edges).  Do the following algorithm
     * to clear edge memory:
     *
     * for all possible line types (0..15)
     *
     *  for all possible line lengths (0..511)
     *
     *    setup 13 bit edge memory address with a pair of writes to the 
     *    V_DG_VERT_SEQ_COUNTER register.  The edge memory address is
     *    composed as: 
     *
     *    upper byte <4..0> = line type <3..0>, hcount <8>
     *    lower byte <7..0> = hcount <7..0>
     *
     *    The upper byte, lower byte edge memory address register latching is 
     *    controlled by a bit in the V_DG_VOF_CNTRL register.
     *
     *    for all possible signal edges (0..24)
     *
     *	    write edge memory address register with signal edge value (0..24)
     *      if (edge is active for this line type, signal edge, & hcount)
     *	      write 0xff to edge memory data register 
     *      else
     *	      write 0x0 to edge memory data register 
     *    end
     *  end
     *
     * Then load specific edges by writing their hcount values (and the line
     * types where they occur) into the edge memory directly.
     */

    /*
     * Clear the edge memories first. 
     */
    for (line = 0; line < VOF_EDGE_DEFINITION_SIZE; line++) {
      for (length = 0; length <= 511; length++) {
	/*
	 * Write 13 bits of edge memory address (lower byte, then upper byte).
	 */
	venice_set_vof_vcreg(V_DG_VOF_CNTRL, 
		(DG_EDGE_LOW_EDGEMEM_ADDR | DG_VOF_STOP), 0);
	venice_set_vof_vcreg(V_DG_VERT_SEQ_COUNTER, (length & 0xff), 0);

	highbyte = ( (line << 1) | ((length >> 8) & 0x1) );
	venice_set_vof_vcreg(V_DG_VOF_CNTRL, 
		(DG_EDGE_HIGH_EDGEMEM_ADDR | DG_VOF_STOP), 0);
	venice_set_vof_vcreg(V_DG_VERT_SEQ_COUNTER, highbyte, 0);

#ifdef VERILOG_DUMP
	for (edge = 19; edge < 21; edge++) {
#else
	for (edge = 0; edge < 25; edge++) {
#endif
	    /*
	     * The vofc compiler will set the uppermost bit of the edge hcount 
	     * if the signal edge does not exist on this line for this format.  
	     *
	     * Thus, we can simply test for equality between the signal edge
	     * hcount (read in from the VOF ucode file) and the length count
	     * indexed above).  If they are equal, we have an edge, and we need
	     * to set the edge memory bit.  Otherwise, we don't, and we must
	     * clear the edge memory bit.
	     *
	     * We only call venice_set_vof_vcreg() when an active edge is
	     * encountered.  Otherwise, we call venice_set_vcreg(), to avoid 
	     * burdening the verilog input model with unecessary writes.
	     */

	    venice_set_vof_vcreg(V_DG_EDGE_MEMORY_ADDR, edgeAddr[edge], 0);
	    venice_set_vof_vcreg(V_DG_EDGE_MEMORY_DATA, 0, 0);
	}
      }
    }

    /*
     * Now set the active edges.
     */
    for (line = 0; line < VOF_EDGE_DEFINITION_SIZE; line++) {
#ifdef JAK_DEBUG
      xprintf("jak -- looking at line type %d\n", 
		line);
#endif


#ifdef VERILOG_DUMP
	for (edge = 19; edge < 21; edge++) {
#else
	for (edge = 0; edge < 25; edge++) {
#endif
	    set_edgeMem_for_edge(line, edge,
		vofData->signal_edge_table[line][edge].edge[0]);
	    set_edgeMem_for_edge(line, edge,
		vofData->signal_edge_table[line][edge].edge[1]);
	    set_edgeMem_for_edge(line, edge,
		vofData->signal_edge_table[line][edge].edge[2]);
	    set_edgeMem_for_edge(line, edge,
		vofData->signal_edge_table[line][edge].edge[3]);

	}
    }
}

/*
 * Load:	the vertical state duration table, 
 * 		the line type table,
 * 		the line length table.
 *
 * V_DG_VERT_SEQ_COUNTER serves as the address register for the line type 
 * lut memory.
 *
 * The line length memory is addressed by the output of the line type
 * memory, which is written to via the V_DG_VERT_SEQ_COUNTER register.
 *
 * So, we first use the line type lut to indirectly address the line 
 * length lut memory, then write the line type memory entry.
 *
 * Afterwards, we load the line type lut memory (whose contents have been
 * trashed by the line length memory load) and the vertical state memory.
 *
 * There are VOF_STATE_TABLE_SIZE possible vertical states, but only 
 * VOF_EDGE_DEFINITION_SIZE line types,
 * so there are:
 *
 *    VOF_STATE_TABLE_SIZE possible vertical state durations,
 *    VOF_STATE_TABLE_SIZE possible line type lut entries,
 *    VOF_EDGE_DEFINITION_SIZE possible line lengths.
 */
static void venice_load_vof_verticalInfo(vof_data_t *vofData)
{
    int i;
    unsigned char low, high;

    /*
     * Make sure that writes to the V_DG_VERT_SEQ_COUNTER go to the correct
     * address latch (should be the 'low' byte).
     */
    venice_set_vof_vcreg(V_DG_VOF_CNTRL, 
	(DG_EDGE_LOW_EDGEMEM_ADDR | DG_VOF_STOP), 0);

    for (i = 0; i < VOF_STATE_TABLE_SIZE; i++) {
	/*
	 * Setup address into the line type memory.
	 */
	venice_set_vof_vcreg(V_DG_VERT_SEQ_COUNTER, i, 0);

	/*
	 * Load line length memory first.
	 */
	if (i < VOF_EDGE_DEFINITION_SIZE) {
	    /*
	     * Setup address into the line length memory by writing into the
	     * line type memory.
	     */
	    venice_set_vof_vcreg(V_DG_LINE_TYPE_LUT, i, 0);
	    /*
	     * Finally, write the line length info into both chunks.  
	     */
	    {
		int             lineLength;

		/*
		 * First subtract one (1) vid clock to account for the XILINX
		 * implementation that counts from zero (instead of one). 
		 *
		 * Do not make this change for any zero length line lengths. 
		 */
		lineLength = vofData->line_length_table[i];
		if (lineLength != 0)
		    lineLength--;
		high = ((lineLength >> 8) & 0x1);
		low = (lineLength & 0xff);
		venice_set_vof_vcreg(V_DG_LINE_LENGTH_LOW, low, 0);
		venice_set_vof_vcreg(V_DG_LINE_LENGTH_HIGH, high, 0);
	    }

	    if (i < 8) {
		/*
		 * Write the display_screen look up table, by flipping a
		 * control register bit.  XXX use a real #define from venice.h
		 */
		venice_set_vof_vcreg(V_DG_VOF_CNTRL, 
		    (DG_DSLUT_SELECT | DG_EDGE_LOW_EDGEMEM_ADDR | DG_VOF_STOP),
		    0);

		high = ((vofData->display_screen_table[i] >> 8) & 0xf);
		low = (vofData->display_screen_table[i] & 0xff);
		venice_set_vof_vcreg(V_DG_LINE_LENGTH_LOW, low, 0);
		venice_set_vof_vcreg(V_DG_LINE_LENGTH_HIGH, high, 0);
		/*
		 * Restore the register bit setting, so that the next
		 * line length lookup table write will succeed.
		 */
		venice_set_vof_vcreg(V_DG_VOF_CNTRL, 
		    (DG_EDGE_LOW_EDGEMEM_ADDR | DG_VOF_STOP), 0);
	    }
	}
	/*
	 * Now write the real line type lut value for the i'th location,
	 * followed by the line duration value (number of times to repeat this
	 * line definition when iterating through a video state transition
	 * sequence).
	 */
	venice_set_vof_vcreg(V_DG_LINE_TYPE_LUT,
	    (vofData->line_type_table[i] & 0xf), 0);
	venice_set_vof_vcreg(V_DG_LINE_DURATION, 
	    vofData->state_duration_table[i] , 0);
#ifdef JAK_DEBUG
	xprintf("jak: loading state %d with type = %d (==%d & 0xf), duration = %d\n", 
		i, 
		vofData->line_type_table[i] & 0xf, 
		vofData->line_type_table[i], 
		vofData->state_duration_table[i]);
#endif
    }
}


extern void venice_clock_serial_dataBit(int);

void
venice_load_clockDivider(int hrate_picosec)
{
    int dividerVal, Nval, Aval;
    int i;

    /*
     * Keep a reference copy of hrate_picosec handy so that vof_loadsection_body
     * can compute the internal sync pulse width in vidclocks when we are
     * going to do an internal genlock.
     *
     * This value is also needed so that venice_load_clockDivider can be called
     * by venice_dg2resetVof() (resetting the XMAPs causes the programmable
     * clock divider to latch bogus serial bus data).
     */
    hrate_picoseconds = hrate_picosec;

    dividerVal = (hrate_picosec + (DG2_BASE_PERIOD / 2)) / DG2_BASE_PERIOD;
    if ((venice_get_vcreg(V_DG_BOARD_ID) & 0x3fffffff) >= DG_P4_BOARD_PRESENT) {
	Nval = dividerVal / DG2_DIVIDE_VAL;
	Aval = dividerVal - (Nval * DG2_DIVIDE_VAL);
    } else {
	Nval = dividerVal / DG2_P2_P3_DIVIDE_VAL;
	Aval = dividerVal - (Nval * DG2_P2_P3_DIVIDE_VAL);
    }
    if (Aval > Nval) {
	if (((Aval - Nval)) > 63) {
	    Nval++;
	    Aval = 0;
	} else {
	    /*
	     * The part would clamp Aval to Nval anyway, even if we programmed
	     * a higher value.
	     */
	    Aval = Nval;
	}
    }

    /* 
     * Disable the clock divider latch while we shift in data.  Enable the
     * serial clock.
     */
    venice_set_vcreg(V_DG_H_DIV_EN, 0x0, 0);
    venice_set_vcreg(V_DG_SERIAL_CLK_EN_L, 0x0, 0);

    for (i = DG2_NCOUNTERSIZE; i > 0; i--)
    {
	if(Nval & (1<<(i-1))) {
	    venice_clock_serial_dataBit(1);
	} else {
	    venice_clock_serial_dataBit(0);
	}

    }

    for (i = DG2_ACOUNTERSIZE; i > 0; i--)
    {
	if(Aval & (1<<(i-1))) {
	    venice_clock_serial_dataBit(1);
	} else {
	    venice_clock_serial_dataBit(0);
	}
    }
    /*
     * Clock in a control bit indicating that we wish to load the 
     * divide by N, divide by A registers.
     */
    venice_clock_serial_dataBit(0);

    /* 
     * Set, then clear the clock divider's latch control bit to latch the data.
     */
    venice_set_vcreg(V_DG_H_DIV_EN, 0x1, 0);
    venice_set_vcreg(V_DG_H_DIV_EN, 0x0, 0);
}

void venice_set_vif_gensrc_cntrl(int reg_value, int genlock)
{
    if (genlock) {
	if ((venice_get_vcreg(V_DG_BOARD_ID) & 0x3fffffff) >= DG_P4_BOARD_PRESENT) {
	    /*
	     * For P4 DG2's, we must invert the 'incoming TTL
	     * level sync' bit in the VIF gensrc control register,
	     * but only when external genlock is being selected.
	     */
	    if (reg_value & DG2_4V_LEVEL_SYNC_SOURCE) {
		reg_value &= ~DG2_4V_LEVEL_SYNC_SOURCE;
	    } else {
		reg_value |= DG2_4V_LEVEL_SYNC_SOURCE;
	    }
	}
    }
    else {
	/*
	 * For internal genlock, always set this bit.
	 */
	reg_value = reg_value | DG2_4V_LEVEL_SYNC_SOURCE;
    }

    venice_set_vcreg(V_DG_VIF_GENSRC_CNTRL_EXTERNAL, reg_value, 0);
}

/*
 * Stuff DG2 board parameters required to run this video format
 * into various DG2 registers.
 */
int
venice_loadsection_dg2_top(unsigned int *param_block)
{
    /*
     * Set the programmable clock to the standard 1280 x 1024 60hz value,
     * because the edge memory write cycles must use a video clock whose
     * period*5 is >= 300ns.  Most clock rates are okay, only NTSC is slow
     * enough (40ns * 5 = 200ns) to cause edge memory loading problems.
     *
     * Wait for a minimum of 10 msec so that the internal PLL can settle.
     *
     * Once the edge memory has been loaded, we set the clock frequency
     * back to the proper value before enabling video.
     */
    venice_set_vcreg(V_DG_PROG_CLK, 0x14, 0);
    sginap(2);

    vof_clock_value = param_block[DG2_PROG_CLK];

    venice_set_vcreg(V_DG_CLK_RESET_L, 1, 0);
    venice_set_vcreg(V_DG_CLK_RESET_L, 0, 0);
    venice_set_vcreg(V_DG_CLK_RESET_L, 1, 0);

    /*
     * Program the clock divider chip with the two divider values
     * required for this format.
     */
    venice_load_clockDivider(param_block[DG2_N_DIVIDER_LINERATE]);

    vof_control_reg_value = param_block[DG2_VOF_CONTROL_REG];
    if (VEN_info.going_to_vs2) {
	vof_control_reg_value |= DG_VOF_DVI_BLANK_L;
    }

    /*
     * Program the VIF registers with values read in from the board
     * parameter section.  We set it up as though we are going to do
     * an external lock; if we need to go internal, we'll make that 
     * decision in venice_loadsection_dg2_body(), because the flag
     * word containing the 'doGenlock' parameter is contained in the
     * body portion of the vof.
     */
    venice_set_vcreg(V_DG_VIF_SYNC_WIDTH_EXTERNAL, 
	param_block[DG2_VIF_SYNC_WIDTH], 0);

    /*
     * When using the textport in VS2 mode, the H-Phase must be 
     * advanced as much as possible to ensure pixels reach the 
     * VS2's FIFOs when needed.
     * 
     * If the VS2 is not in use, apply the standard calculated
     * value.
     */
    if (VEN_info.going_to_vs2) {
	venice_set_vcreg(V_DG_VIF_HPHASE_LOOPGAIN, 
	    0xcc, 0);
    } else {
	venice_set_vcreg(V_DG_VIF_HPHASE_LOOPGAIN, 
	    param_block[DG2_VIF_HPHASE_LOOPGAIN], 0);
    }

    venice_set_vcreg(V_DG_VIF_MCOUNTER, 
	param_block[DG2_VIF_MCOUNTER], 0);

    venice_set_vcreg(V_DG_VIF_HORZ_LENGTH, 
	param_block[DG2_VIF_HORZ_LENGTH], 0);

    venice_set_vif_gensrc_cntrl(param_block[DG2_VIF_GENSRC_CNTRL], 1);

    venice_set_vcreg(V_DG_VIF_BPORCH_CLAMP_EXTERNAL, 
	param_block[DG2_VIF_BPORCH_CLAMP], 0);

    /*
     * Save the back porch clamp value, so that it can be re-written one
     * more time after all the DG2 initialization is done; each write to
     * the back porch clamp register causes the sync tip clamps to briefly
     * overlap, which resets the genlock detect circuit.
     */
    vif_bporch_clamp_shadow  = param_block[DG2_VIF_BPORCH_CLAMP];

    venice_set_vcreg(V_DG_VIF_FINE_PHASE_ADJ, 
	param_block[DG2_VIF_FINE_PHASE_ADJ], 0);

    venice_set_vcreg(V_DG_VIF_MAGIC_HIGH, 
	param_block[DG2_VIF_MAGIC_HIGH], 0);

    /*
     * Save the internal genlock params to be used when 
     * venice_dg2_loadsection_body() makes the decision to tweak  the vif
     * registers to go from external genlock (which we've set up here) to
     * internal genlock.
     */

    vif_sync_width_internal = param_block[DG2_VIF_SYNC_WIDTH_INT];
    vif_gensrc_cntrl_internal = param_block[DG2_VIF_GENSRC_CNTRL_INT];
    vif_bporch_clamp_internal = param_block[DG2_VIF_BPORCH_CLAMP_INT];


    return(0);
}

int
venice_loadsection_dg2_body(vof_data_t *vofData, int genlockOverride)
{
    int vof_syncon_red, vof_syncon_green, vof_syncon_blue, vof_syncon_alpha;
    int rgbcmd2, alphacmd2;

    extern void set_dac_cmd2(int, int);


    int venice_calcframebuffer(vof_data_t *, venice_vof_file_info_t *);

    /*
     * Set the VOFSTOP bit in the control register; tell the kernel about it.
     */
    venice_set_vof_vcreg(V_DG_VOF_CNTRL, DG_VOF_STOP, 0);


    /*
     * Load the edge memory.
     */
    venice_load_vof_edgeMem(vofData);

    /*
     * Load:	the vertical state duration table, 
     * 		the line type table,
     * 		the line length table.
     */
    venice_load_vof_verticalInfo(vofData);

    /*
     * genlockOverride is provided so that setmon/setvideo may override the
     * vof_file_info.flags genlock setting.
     */
    if (genlockOverride) {
	vofData->vof_file_info.flags |= VOF_F_DO_GENLOCK;
    }

    /*
     * If in VS2 mode, we want to set to non-BNC genlock.
     */
    if (VEN_info.going_to_vs2) {
	vofData->vof_file_info.flags &= ~VOF_F_DO_GENLOCK;
    }

    if (vofData->vof_file_info.flags & VOF_F_DO_GENLOCK) {
	venice_set_vcreg(V_DG_GENLOCK, 1, 0); /* lock to incoming sync */
	vif_genlock = 1;
    } else {
	venice_set_vcreg(V_DG_GENLOCK, 0, 0); /* lock to internal hrate */
	vif_genlock = 0;

	venice_set_vcreg(V_DG_VIF_SYNC_WIDTH, 
	    vif_sync_width_internal, 0);

	venice_set_vif_gensrc_cntrl(vif_gensrc_cntrl_internal, 0);

	venice_set_vcreg(V_DG_VIF_BPORCH_CLAMP, 
	    vif_bporch_clamp_internal, 0);
	/*
	 * Save the back porch clamp value, so that it can be re-written one
	 * more time after all the DG2 initialization is done; each write to
	 * the back porch clamp register causes the sync tip clamps to briefly
	 * overlap, which "arms" the genlock detect circuit.
	 */
	vif_bporch_clamp_shadow  = vif_bporch_clamp_internal;
    }

    /*
     * TTL or 1V output sync level
     */
    if (vofData->vof_file_info.flags & VOF_F_TTL_OUTSYNC)
	vof_outsync_level = 1;
    else
	vof_outsync_level = 0;

    /*
     * Sync on which outputs?
     */
    vof_syncon_red = vof_syncon_green = vof_syncon_blue = vof_syncon_alpha = 1;
    if (vofData->vof_file_info.flags & VOF_F_NOSYNC_RED)
	vof_syncon_red = 0;
    if (vofData->vof_file_info.flags & VOF_F_NOSYNC_GREEN)
	vof_syncon_green = 0;
    if (vofData->vof_file_info.flags & VOF_F_NOSYNC_BLUE)
	vof_syncon_blue = 0;
    if (vofData->vof_file_info.flags & VOF_F_NOSYNC_ALPHA)
	vof_syncon_alpha = 0;

    rgbcmd2 = 0x080808;
    if (vof_syncon_red)   rgbcmd2 |= 0x000080;
    if (vof_syncon_green) rgbcmd2 |= 0x008000;
    if (vof_syncon_blue)  rgbcmd2 |= 0x800000;
    alphacmd2 = 0x08;
    if (vof_syncon_alpha) alphacmd2 |= 0x80;

    set_dac_cmd2(rgbcmd2, alphacmd2);

    /*
     * shadow RGB and alpha command regs in kernel
     *
     * The kernel's VENICE_SET_VOF ioctl does update rgbcmd2, alphacmd2, and
     * GL_VIDEO_REG, so the only problem is that the kernel textport will not
     * set these variables.  I don't view this as a problem, since you can't
     * really do anything interesting with the kernel textport, and as soon as
     * gfxinit runs, the shadowed variables will get initialized.
     */
#ifdef  _KERNEL
    /*
     * XXX this is what should be done from kernel textport code, but it can't
     *     (kernel textport doesn't have a bdata handle)
     */
#ifdef  MOOT_POINT
    {
        struct venice_data *bdata = (struct venice_data *) gfxp->gx_bdata;

        bdata->dg2shadow.rgbdac_cmd2 = rgbcmd2;
        bdata->dg2shadow.alphadac_cmd2 = alphacmd2;
	/*
	 * XXX would also have to update GL_VIDEO_REG too, but it's moot!
	 */
    }
#endif
#endif

    /*
     * Tell the kernel about the VOF.  This is not performed in the VS2 case
     * because the vof_file_info structure is populated elsewhere by VS2
     * routines because the vof in vofData is one of unusual construction
     * that bears little resemblance to the output produced by head 0. 
     */
    if (!VEN_info.going_to_vs2)
	VEN_info.vof_head_info[0].vof_file_info = vofData->vof_file_info;

    /*
     * Save the hwalk length value in a global variable, for use by
     * DG2 diags.  
     */
    vof_hwalk_length = vofData->vof_file_info.hwalk_length;
    /*
     * Check to see if we have a Rev L vc bus prom; if so, we must
     * constrain hwalk to a maximum of two ops per hlist event 
     * (which actually turns out to be 3 ops, due to hw pipelining) to avoid
     * hitting a vc bus write cycle arbitration bug which causes the hlist
     * to execute during active video (and thus, causes colormap loads to 
     * screw up).
     *
     * If we ever add another vc prom revision, we will have to check for
     * all vc bus prom revisions here, since rev L proms just return a garbage
     * value.
     */
    if ((venice_get_vcreg(V_DG_SERIAL_CLK_EN_L) & DG_VCPROM_MASK) 
	!= DG_REVM_VCPROM) {
	if (vof_hwalk_length > 2) {
#ifdef DEBUG
	    xprintf("venice_loadsection_dg2_body: revL vc prom detected.\n");
	    xprintf("\tconstraining hwalk length of %d to 2.\n",
		vof_hwalk_length);
#endif
	    vof_hwalk_length = vofData->vof_file_info.hwalk_length = 2;
	}
    } 
    if (!venice_calcframebuffer(vofData, &vofData->vof_file_info))
	return(1);

    /*
     * Compute & load the contents of the VDRC SRAM.
     */
    venice_load_vdrc_sram(vofData);

    /*
     * Set the clock value to its real value after the edge memories
     * have been loaded, but before the vof is started.  Wait for
     * a minimum of 10 msec so that the internal PLL can settle.
     */
    venice_set_vcreg(V_DG_PROG_CLK, vof_clock_value, 0);
    sginap(2);


    return(0);
}

/*
 * Compute the pan offsets which allow a lower resolution video format
 * to roam around a framebuffer which is rendered in a larger resolution.
 */
void
venice_clamp_dg2_pan(int *vof_ILoffset, int *vof_baseTileAddr, 
			int *vof_yoffset)
{
    int moduloFive, pixPerTile, yTileSkip;
    int bother;
    int heightToCheck;

    moduloFive = VEN_info.vof_head_info[0].pan_x % 5;
    if (moduloFive != 0) {
	/*
	 * Bitch, than clamp.
	 */
	xprintf("venice_clamp_dg2_pan: pan_x not a multiple of five.\n");
	VEN_info.vof_head_info[0].pan_x -= moduloFive;
    }

    pixPerTile = 80 * VEN_info.rm_count;

    if (VEN_info.gfx_info.xpmax > 
      (VEN_info.tiles_per_line * pixPerTile)) {
	 xprintf("venice_clamp_dg2_pan: illegal xpmax value %d, max = %d\n",
	     VEN_info.gfx_info.xpmax, (VEN_info.tiles_per_line * pixPerTile) );
    }

    /*
     * Check to see if we've panned beyond the rightmost edge of the  
     * rendered framebuffer.  If so, constrain the pan_x parameter.
     */
    if ((VEN_info.vof_head_info[0].pan_x + 
      VEN_info.vof_head_info[0].vof_file_info.vof_width) >
      VEN_info.gfx_info.xpmax) {

	VEN_info.vof_head_info[0].pan_x =
	  (VEN_info.gfx_info.xpmax -
           VEN_info.vof_head_info[0].vof_file_info.vof_width);

	if (VEN_info.vof_head_info[0].pan_x < 0) {
	    xprintf("venice_clamp_dg2_pan:\n");
	    xprintf("video format horizontal resolution %d exceeds available # of tiles/line %d.\n",
           	VEN_info.vof_head_info[0].vof_file_info.vof_width,
	  	VEN_info.tiles_per_line);
	    VEN_info.vof_head_info[0].pan_x = 0;
	}
    }
    /*
     * Check to see if we've panned beyond the topmost edge of the  
     * rendered framebuffer.  If so, constrain the pan_y parameter.
     */
    if (VEN_info.vof_head_info[0].vof_file_info.flags & VOF_F_PIXREP_X_2) {
	heightToCheck = VEN_info.vof_head_info[0].vof_file_info.vof_height / 2;
    } else {
	heightToCheck = VEN_info.vof_head_info[0].vof_file_info.vof_height;
    }
    if ((VEN_info.vof_head_info[0].pan_y + 
      heightToCheck) >
      VEN_info.gfx_info.ypmax) {

	VEN_info.vof_head_info[0].pan_y = VEN_info.gfx_info.ypmax -
           heightToCheck;

	if (VEN_info.vof_head_info[0].pan_y < 0) {
	    xprintf("video format vertical resolution %d exceeds frame buffer resolution %d.\n",
           	heightToCheck,
		VEN_info.gfx_info.ypmax);
	    VEN_info.vof_head_info[0].pan_y = 0;
	}
    }
    /*
     * Now compute the vof_baseTileAddr, vof_ILoffset, and  vof_yoffset 
     * parameters.
     */

    yTileSkip = VEN_info.vof_head_info[0].pan_y >> 3;

    /*
     * Optimizer problem?  *vof_baseTileAddr was not getting the right
     * value until I forced the value to be copied into a temporary stack
     * variable.  bother, indeed.
     */
    bother = ((VEN_info.vof_head_info[0].pan_x / pixPerTile) +
			 (yTileSkip * VEN_info.tiles_per_line));
    *vof_baseTileAddr = bother;
    /*
     * vof_yoffset =	# of lines to skip within an 8 line tile.
     * vof_ILoffset =	# of 10 pixel spans to skip within a tile.
     */
    *vof_yoffset = VEN_info.vof_head_info[0].pan_y & 7;
    *vof_ILoffset = ((VEN_info.vof_head_info[0].pan_x % pixPerTile) / 10);
}


/*
 * Write a singular video request to VDRC sram; optionally add two extra bytes
 * which encode the per-request ILoffset and length values.
 */
void
venice_write_video_request(	int tileAddress, 
				int y, 
				int tileCount,
				int length,
				int moreRequests,
				int moreData,
				int ILoffset, 
				int channel)
{
    unsigned char byteVal;

    /*
     * The three bytes for the video data request are as follows:
     *
     * 7   6   5   4   3   2   1   0
     *
     * A2  A1  A0 <TileCount 3..0> 0
     * A10 A9  A8  A7  A6  A5  A4  A3
     * <Ch 2..0> Cont More Y2  Y1  Y0
     *
     * NOTE: VDRC tileCount = actual number of tiles - 1
     *
     * (Cont -> additional video data requests or imp register loads
     * follow (for the current data request from the VOF).  This flag is set
     * if moreRequests is TRUE.
     *
     * (More -> two additional bytes containing ilOffset & length
     * follow this record). This flag is set if moreData is TRUE.
     *
     * Ch 2..0 = channel number (VS2), will be ignored, set to 0.
     */
    byteVal = (
	(((tileCount - 1) << 1) & 0x1e) | ((tileAddress << 5) & 0xe0)
	    );
    venice_write_vdrc_sram_byte(byteVal);

    byteVal = ((tileAddress >> 3) & 0xff);
    venice_write_vdrc_sram_byte(byteVal);

    byteVal = y & 7 | 
	    ((channel & 7) << 5);
    if (moreData)
	byteVal |= 0x8;
    if (moreRequests)
	byteVal |= 0x10;
    venice_write_vdrc_sram_byte(byteVal);

    if (moreData) {
	byteVal = ILoffset & 0x1f;
	venice_write_vdrc_sram_byte(byteVal);
	byteVal = length & 0xff;
	venice_write_vdrc_sram_byte(byteVal);
    }
}

/*
 * Recursive function which scans for buggy DRAM page crossings, and breaks up
 * the video request into multiple video requests, if necessary.
 */
void
venice_video_request(int tileAddress, int tileCount, int y, int length, 
    int tilesPerPage, int vof_ILoffset, int isFirst, int hasOldImps, 
    int channel)
{
    int bumpTileCount, bumpLength;

    if (vof_ILoffset > 0)
	++tileCount;
    /*
     * The IMP always fetches pairs of tiles; the page crossing bug only 
     * occurs when the page crossing occurs in between the pairs of fetches;
     * (i.e. fetches which straddle the page boundary are okay).
     *
     * Only do the page crossing workaround if one (or more) RM's have the
     * older version of the IB IMP chips.
     */
    if (((tileAddress & 1) == 0) && (hasOldImps == TRUE)) {
	/*
	 * Find the nearest page crossing for this request.
	 */
	if (((tileAddress % tilesPerPage) + tileCount) > tilesPerPage) {

#ifdef DEBUG_PAGE_CROSSING
	    if ((y & 7) == 0)
xprintf("y = %d, tile addr = %d, modulo tilesPerPage = %d\n",
y, tileAddress, (tileAddress % tilesPerPage) );

#endif
#ifdef REQUEST_TO_PAGE_BOUNDARY_BUSTED_WITH_FOUR_RMS
	    /*
	     * We're going to cross a nasty page boundary.  Break up the
	     * request so that we request up to the page boundary.
	     */
	    bumpTileCount = (tilesPerPage - (tileAddress % tilesPerPage));
#else
	    /*
	     * We seem to incur additional page crossing lookup errors on four
	     * RM systems, even though the tilesPerPage should be calibrated
	     * for 320 pixels per tile on four RM systems.
	     *
	     * So, a simpler method is just to request one tile on the first
	     * request, which allows the IMP to fetch the remaining tiles on
	     * this line without incurring the page crossing bug (which happens
	     * when the initial tile address is even).  Note that this 
	     * workaround ONLY works if the framebuffer is anchored to (0,0)
	     * in tile address space (which it has been, and forever will be).
	     *
	     * VS2, on the other hand, will NOT anchor to (0,0), but the same
	     * workaround should apply (although I doubt we'll run VS2's with
	     * old RM's).
	     */
	    bumpTileCount = 1;
#endif
	    if (vof_ILoffset > 0) {
		++bumpTileCount;
	    } 
	    bumpLength = (bumpTileCount * (80 * VEN_info.rm_count)) / 10;
	    bumpLength += vof_ILoffset;

	    venice_write_video_request(tileAddress, y, 
		bumpTileCount, bumpLength, 
		TRUE, TRUE, vof_ILoffset, 
		channel);
	    /*
	     * Now adjust the tileAddress, length, and vof_ILoffset parameters,
	     * then call venice_video_request() recursively to finish the job.
	     *
	     * Subtract what we've requested from the tileCount and length
	     * variables, add what we requested to the tileAddress, and set 
	     * the vof_ILoffset parameter to zero.
	     */
	    tileCount -= bumpTileCount;
	    tileAddress += bumpTileCount;
	    length -= bumpLength;
	    vof_ILoffset = 0;
	    venice_video_request(tileAddress, tileCount, y, length, 
		tilesPerPage, vof_ILoffset, FALSE, FALSE, channel);
	} else {
	    if (isFirst) {
		venice_write_video_request(tileAddress, y, tileCount, length, 
		    FALSE, FALSE, vof_ILoffset, channel);
	    } else {
		/*
		 * This could be the result of a recursive call, in which case 
		 * the line length and ILoffset parameters will be different 
		 * from those stored in the global table at the end of VDRC 
		 * sram, so set the moreData parameter to TRUE.
		 */
		venice_write_video_request(tileAddress, y, tileCount, length, 
		    FALSE, TRUE, vof_ILoffset, channel);
	    }
	}
    } else {
	if (isFirst) {
	    venice_write_video_request(tileAddress, y, tileCount, length, 
		FALSE, FALSE, vof_ILoffset, channel);
	} else {
	    /*
	     * This could be the result of a recursive call, in which case 
	     * the line length and ILoffset parameters will be different 
	     * from those stored in the global table at the end of VDRC 
	     * sram, so set the moreData parameter to TRUE.
	     */
	    venice_write_video_request(tileAddress, y, tileCount, length, 
		FALSE, TRUE, vof_ILoffset, channel);
	}
    }
}

/* ARGSUSED */
static void venice_load_vdrc_sram(vof_data_t *vofData)
{
    int vof_ILoffset;
    int vof_baseTileAddr;
    int vof_yoffset;
    int length;
    int tileAddress;
    int tilesPerPage;
    int tilesPerRequest;
    int i, y;
    int offsetSize;
    int hasOldImps;
    int imprev;

    /*
     * Reset the sram address register, so that subsequent writes to the
     * vdrc data register will auto-increment through the address space of
     * the vdrc sram.  Currently, the reset requires a low to high transition.
     *
     * We leave the bit high, so that writes to the VDRC data register will
     * be routed to the vdrc SRAM.
     */
    sramByteCount = 0;
    venice_set_vcreg(V_DG_VDRC_CNTRL, 0x0, 0);
    venice_set_vcreg(V_DG_VDRC_CNTRL, DG_VDRC_MEM_ACCESS, 0);

    {

    /*
     * Call venice_clamp_dg2_pan() to compute the pan offsets which allow a 
     * lower resolution video format to roam around a framebuffer which is 
     * rendered in a larger resolution.
     *
     * Next, compute the line length to be stored at the end
     * of VDRC SRAM, in the global offset/length table.  Since there are
     * 10 xmaps, the pixel request length must be in units of tens of pixels.
     *
     * To ease the VDRC counter implementation, we also add the number of 
     * pixels/10 included in the vof_ILoffset parameter.
     */
    venice_clamp_dg2_pan(&vof_ILoffset, &vof_baseTileAddr, &vof_yoffset);

    /*
     * Round up the vof's active video length to a 10 pixel boundary, since 
     * the VDRC can only request pixels on 10 pixel boundaries (and thus the
     * length we supply must be a multiple of 10).
     *
     * We do this by always adding 5 to the vof supplied width; integer 
     * truncation (from the divide by 10 below) will not harm a format whose
     * width was already divisible by 10, but will cause those formats who
     * were divisible by 5 (but not by 10) to round up to the next 10 pixel
     * boundary.
     */
    length = ((VEN_info.vof_head_info[0].vof_file_info.vof_width + 5) / 10) +
	    vof_ILoffset;

    /*
     * tilesPerRequest is derived from the length and the number of RM's.
     */
    {
	int	divisor = (80 * VEN_info.rm_count);
	
	tilesPerRequest = ((length * 10) + (divisor - 1))/divisor;
    }
    
    switch (VEN_info.pixel_depth) {
	case V_DG_IE_CONFIG_LARGEPIX:
	    tilesPerPage = 8;
	    break;
	case V_DG_IE_CONFIG_MEDPIX:
	    tilesPerPage = 16;
	    break;
	case V_DG_IE_CONFIG_SMALLPIX:
	    tilesPerPage = 32;
	    break;
    }

    /*
     * Check to see if we have old IB IMP chips which require the page crossing
     * workaround.
     */
    imprev = venice_rm_imprev();
    if (imprev == 0) {
	hasOldImps = TRUE;
    } else {
	hasOldImps = FALSE;
    }
    /*
     * For each line to be requested for any video format,
     *  compute the 11 bit tile address
     *  call venice_video_request, which will:
     *    compute the tile count for this line's pixel request
     *    check for DRAM page crossings; break up requests: if necessary.
     *    write multiple records to SRAM as needed.
     *
     * For old style stereo formats, request the top half's lines first, then 
     * the bottom half's lines.  
     */
    if ((VEN_info.vof_head_info[0].vof_file_info.flags & VOF_F_OLD_STEREO) ||
        (VEN_info.vof_head_info[0].vof_file_info.flags & VOF_F_SPLITSCREEN)) {
	/*
	 * Top half's lines.  We have to assume that there are 1024 rendered
	 * lines in the frame buffer.  The number of lines to scan out can be
	 * 492 (for canonical old style stereo) or 512 (for the newer version
	 * of old style stereo), or 512 for split screen 1280x1024_30i.
	 */
	for (y = (VEN_info.vof_head_info[0].pan_y + 1023), i = 0;
	    i < VEN_info.vof_head_info[0].vof_file_info.lines_in_field[0]; 
	    y--, i++) {

	    tileAddress = (((VEN_info.tiles_per_line * (y >> 3)) + 
	     VEN_info.baseTileAddr + vof_baseTileAddr) & 0x7ff);

	    /*
	     * Issue a video request; this function will break up this request 
	     * into multiple video requests, if we would otherwise hit the DRAM
	     * page crossing bug.
	     */
	    venice_video_request(tileAddress, tilesPerRequest, y, 
		length, tilesPerPage, vof_ILoffset, TRUE, hasOldImps, 
		DEFAULT_VDRC_CHANNEL);
	}
        /*
         * Bottom half's lines.
         */
        for (y = (VEN_info.vof_head_info[0].pan_y + 
	    (VEN_info.vof_head_info[0].vof_file_info.lines_in_field[1] - 1)); 
	    y >= 0; y--) {

	    tileAddress = (((VEN_info.tiles_per_line * (y >> 3)) + 
	     VEN_info.baseTileAddr + vof_baseTileAddr) & 0x7ff);

	    /*
	     * Issue a video request; this function will break up this request 
	     * into multiple video requests, if we would otherwise hit the DRAM
	     * page crossing bug.
	     */
	    venice_video_request(tileAddress, tilesPerRequest, y, 
		length, tilesPerPage, vof_ILoffset, TRUE, hasOldImps, 
		DEFAULT_VDRC_CHANNEL);
	}
    } else if 
	(VEN_info.vof_head_info[0].vof_file_info.flags & VOF_F_SEQ_RGB) {
	    if (imprev <= 1) {
		xprintf("venice_load_vdrc_sram(): sequential RGB requested, but old imps detected.\n");
		xprintf("(sequential swizzling of R, G, B onto the blue channel will not be possible).\n");
	    }
	    if 
	    (VEN_info.vof_head_info[0].vof_file_info.flags & VOF_F_INTERLACED) {
		int             lineCount;
		int             componentIndex, fieldIndex;

		/* 
		 * Field Sequential RGB will have 6 fields; request each
		 * interlaced field three times.
		 */
		for (componentIndex = 0; componentIndex < 2; componentIndex++) {
		    int startY;
		    int offsetToFirstLineOfField;

		    /*
		     * For interlaced formats, the first line of this field
		     * may need to be adjusted.  This is because the first
		     * line of this field may not be the line that shows up
		     * first on the output screen:  this is the definition 
		     * of interlacing (lines of different fields 
		     * intermingle as they are sent to the screen according
		     * to a regular pattern). 
		     *
		     * Therefore, calculate the offset from the top line of
		     * the frame buffer to the first line of the field. 
		     */
		    if (componentIndex == 0)
			offsetToFirstLineOfField = 1;
		    else
			offsetToFirstLineOfField = 0;

		    /*
		     * Calculate the initial line of this field.  Take the
		     * base of the window (pan_y) and add the height 
		     * (vof_height, biased by -1 to account for the fact that 
		     * we count line numbers beginning with 0).  From that 
		     * line number, back off the offsetToFirstLineOfField to 
		     * get the initial line number.
		     */
		    startY =
			VEN_info.vof_head_info[0].pan_y +
			(VEN_info.vof_head_info[0].vof_file_info.vof_height - 1)
			 - offsetToFirstLineOfField;
		    /*
		     * Now request the same interlaced field three times
		     * (once for each of (r, g, b))
		     */
		    for (fieldIndex = 0; fieldIndex < 3; fieldIndex++) {

			/* Create a record for each line in this field. */
			for (lineCount = 0, y = startY;
			     lineCount < VEN_info.vof_head_info[0].vof_file_info.lines_in_field[fieldIndex]; lineCount++, y -= 2) {
			    tileAddress = 
			      (((VEN_info.tiles_per_line * (y >> 3)) +
				VEN_info.baseTileAddr + 
				vof_baseTileAddr) & 0x7ff);

			    /*
			     * Issue a video request; this function will break 
			     * up this request into multiple video requests, 
			     * if we would otherwise hit the DRAM page 
			     * crossing bug. Actually, since sequential RGB
			     * requires IMP7's (which have the component 
			     * swizzle feature), this is moot.
			     */
			    venice_video_request(tileAddress,
						 tilesPerRequest,
						 y,
						 length,
						 tilesPerPage,
						 vof_ILoffset,
						 TRUE,
						 hasOldImps, 
						 DEFAULT_VDRC_CHANNEL);
			}
		    }
	        }
	    } else {

		int fieldIndex;

		 /*
		  * (Three identical fields must be requested; DG will swizzle
		  *  the red/green/blue channels in the necessary sequence).
		  *
		  * For each line to be requested for non-interlaced seq. rgb,
		  *  compute the 11 bit tile address
		  *  call venice_video_request, which will:
		  *    compute the tile count for this line's pixel request
		  *    check for DRAM page crossings; break up requests: if 
		  *    necessary.
		  *    write multiple records to SRAM as needed.
		  */
	      for (fieldIndex = 0; fieldIndex < 3; fieldIndex++) {
		for (y = (VEN_info.vof_head_info[0].pan_y + 
		  VEN_info.vof_head_info[0].vof_file_info.vof_height -1); 
		  y >= vof_yoffset; y--) {

		    tileAddress = (((VEN_info.tiles_per_line * (y >> 3)) + 
		     VEN_info.baseTileAddr + vof_baseTileAddr) & 0x7ff);

		    /*
		     * Issue a video request; this function will break up this 
		     * request into multiple video requests, if we would otherwise 
		     * hit the DRAM * page crossing bug.
		     */
		    venice_video_request(tileAddress, tilesPerRequest, y, 
			length, tilesPerPage, vof_ILoffset, TRUE, hasOldImps, 
			DEFAULT_VDRC_CHANNEL);
		}
	      }
	    }
	} else if
	(((VEN_info.vof_head_info[0].vof_file_info.flags & VOF_F_PIXREP_X_2) &&
	(VEN_info.vof_head_info[0].vof_file_info.flags & VOF_F_DO_FRAMELOCK)) ||
	(VEN_info.vof_head_info[0].vof_file_info.flags & VOF_F_STEREO)) {

	    int fieldIndex;

	     /*
	      * For framelocked, field replicated formats, request the
	      * number of lines specified in the format twice.
	      *
	      * For each line to be requested for this video format,
	      *  compute the 11 bit tile address
	      *  call venice_video_request, which will:
	      *    compute the tile count for this line's pixel request
	      *    check for DRAM page crossings; break up requests: if 
	      *    necessary.
	      *    write multiple records to SRAM as needed.
	      */
	    for (fieldIndex = 0; fieldIndex < 2; fieldIndex++) {
		for (y = (VEN_info.vof_head_info[0].pan_y + 
		  (VEN_info.vof_head_info[0].vof_file_info.vof_height / 2) -1); 
		  y >= vof_yoffset; y--) {

		    tileAddress = (((VEN_info.tiles_per_line * (y >> 3)) + 
		     VEN_info.baseTileAddr + vof_baseTileAddr) & 0x7ff);

		    /*
		     * Issue a video request; this function will break up this 
		     * request into multiple video requests, if we would 
		     * otherwise hit the DRAM * page crossing bug.  
		     */
		    venice_video_request(tileAddress, tilesPerRequest, y, 
			length, tilesPerPage, vof_ILoffset, TRUE, hasOldImps, 
			DEFAULT_VDRC_CHANNEL);
		}
	    }
	} else if (VEN_info.vof_head_info[0].vof_file_info.flags & 
	      VOF_F_PIXREP_X_2) {
	     /*
	      * For each line to be requested for this video format,
	      *  compute the 11 bit tile address
	      *  call venice_video_request, which will:
	      *    compute the tile count for this line's pixel request
	      *    check for DRAM page crossings; break up requests: if 
	      *    necessary.
	      *    write multiple records to SRAM as needed.
	      *  call venice_video_request again (such that the same line
	      *  will be requested twice).
	      */
	    for (y = (VEN_info.vof_head_info[0].pan_y + 
	      VEN_info.vof_head_info[0].vof_file_info.vof_height/2 -1); 
	      y >= vof_yoffset; y--) {

		tileAddress = (((VEN_info.tiles_per_line * (y >> 3)) + 
		 VEN_info.baseTileAddr + vof_baseTileAddr) & 0x7ff);

		/*
		 * Issue a video request; this function will break up this 
		 * request into multiple video requests, if we would 
		 * otherwise hit the DRAM * page crossing bug.  
		 * 
		 * Then call it again to effect vertical pixel replication.
		 * Without some magic, however, the cursor chip will get 
		 * pulsed on every line, causing the pointer to appear
		 * squashed to half its normal height.  To avoid this
		 * problem, create the VDRC with channel numbers that 
		 * alternate between the channel which toggles the cursor
		 * pulse (DEFAULT_VDRC_CHANNEL) and one that doesn't; the
		 * VOF must define the DVI_PARAMETERS section properly
		 * to enable cursor on DEFAULT_VDRC_CHANNEL and not on
		 * any others.
		 */
		venice_video_request(tileAddress, tilesPerRequest, y, 
		    length, tilesPerPage, vof_ILoffset, TRUE, hasOldImps, 
		    DEFAULT_VDRC_CHANNEL);

		venice_video_request(tileAddress, tilesPerRequest, y, 
		    length, tilesPerPage, vof_ILoffset, TRUE, hasOldImps, 
		    DEFAULT_VDRC_CHANNEL + 1);
	    }
	} else {
	    /*
	     * It's not an old-style stereo format, nor a sequential rgb format,
	     * nor a framelocked field replicated format, nor a pixel replicated
	     * format, so it's either a standard interlaced format or a 
	     * standard non-interlaced format.
	     *
	     * For interlaced formats, request the odd field first, then the
	     * even field.  
	     *
	     * Even field line numbers are (0 2 4 ... (n - 1)),
	     *  Odd field line numbers are (1 3 5 ... (n - 2)),
	     *    where n must not be divisible by two.
	     */
	    if 
	    (VEN_info.vof_head_info[0].vof_file_info.flags & VOF_F_INTERLACED) {
		int             lineCount;
		int             fieldIndex;

		/* Look at all fields */
		for (fieldIndex = 0; 
		  fieldIndex < 
		  VEN_info.vof_head_info[0].vof_file_info.fields_per_frame;
		  fieldIndex++) {
		    int             startY;
		    int             offsetToFirstLineOfField;

		    if 
		      (VEN_info.vof_head_info[0].vof_file_info.fields_per_frame
			== 1) {

			/*
			 * For non-interlaced formats, no offset is needed:  
			 * each line of output as sent to the screen 
			 * corresponds directly with each line in the frame 
			 * buffer. 
			 */
			offsetToFirstLineOfField = 0;
		    } else {

			/*
			 * For interlaced formats, the first line of this field
			 * may need to be adjusted.  This is because the first
			 * line of this field may not be the line that shows up
			 * first on the output screen:  this is the definition 
			 * of interlacing (lines of different fields 
			 * intermingle as they are sent to the screen 
			 * according to a regular pattern). 
			 *
			 * Therefore, calculate the offset from the top line of the
			 * frame buffer to the first line of the field. 
			 */
			if (fieldIndex == VEN_info.vof_head_info[0].vof_file_info.field_with_uppermost_line)
			    offsetToFirstLineOfField = 0;
			else
			    offsetToFirstLineOfField = 
				VEN_info.vof_head_info[0].vof_file_info.fields_per_frame - 1;
		    }

		    /*
		     * Calculate the initial line of this field.  Take the
		     * base of the window (pan_y) and add the height 
		     * (vof_height, biased by -1 to account for the fact that 
		     * we count line numbers beginning with 0).  From that 
		     * line number, back off the offsetToFirstLineOfField to 
		     * get the initial line number.
		     */
		    startY =
			VEN_info.vof_head_info[0].pan_y +
			(VEN_info.vof_head_info[0].vof_file_info.vof_height - 1) -
			offsetToFirstLineOfField;

		    /* Create a record for each line in this field. */
		    for (lineCount = 0, y = startY;
			 lineCount < VEN_info.vof_head_info[0].vof_file_info.lines_in_field[fieldIndex];
			 lineCount++, y -= VEN_info.vof_head_info[0].vof_file_info.fields_per_frame) {
			tileAddress = (((VEN_info.tiles_per_line * (y >> 3)) +
			     VEN_info.baseTileAddr + vof_baseTileAddr) & 0x7ff);

			/*
			 * Issue a video request; this function will break up
			 * this request into multiple video requests, if we 
			 * would otherwise hit the DRAM page crossing bug. 
			 */
			venice_video_request(tileAddress,
					     tilesPerRequest,
					     y,
					     length,
					     tilesPerPage,
					     vof_ILoffset,
					     TRUE,
					     hasOldImps, 
					     DEFAULT_VDRC_CHANNEL);
		    }
		}
	    } else {
		 /*
		  * For each line to be requested for this video format,
		  *  compute the 11 bit tile address
		  *  call venice_video_request, which will:
		  *    compute the tile count for this line's pixel request
		  *    check for DRAM page crossings; break up requests: if 
		  *    necessary.
		  *    write multiple records to SRAM as needed.
		  */
		for (y = (VEN_info.vof_head_info[0].pan_y + 
		  VEN_info.vof_head_info[0].vof_file_info.vof_height -1); 
		  y >= vof_yoffset; y--) {

		    tileAddress = (((VEN_info.tiles_per_line * (y >> 3)) + 
		     VEN_info.baseTileAddr + vof_baseTileAddr) & 0x7ff);

		    /*
		     * Issue a video request; this function will break up this 
		     * request into multiple video requests, if we would 
		     * otherwise hit the DRAM * page crossing bug.
		     */
		    venice_video_request(tileAddress, tilesPerRequest, y, 
			length, tilesPerPage, vof_ILoffset, TRUE, hasOldImps, 
			DEFAULT_VDRC_CHANNEL);
		}
	    }
	}

	/*
	 * Zero out the remainder of the VDRC SRAM, up to the global table.
	 */

	if ((venice_get_vcreg(V_DG_BOARD_ID) & 0x3fffffff) >= DG_P2_BOARD_PRESENT) {
	    offsetSize = VDRC_OFFSET_TABLE_START;
	} else {
	    offsetSize = VDRC_OFFSET_TABLE_START_P0;
	}

	venice_finish_vdrc_sram(sramByteCount, offsetSize, vof_ILoffset, length);

    }

    /*
     * Clear the memory access bit.
     */
    venice_set_vcreg(V_DG_VDRC_CNTRL, 0, 0);
}

int
venice_calcframebuffer(vof_data_t *vofData, venice_vof_file_info_t *vofDisplay)
{
    short               t_width, t_height, t_size;
    int                tileWidth;
    int			i, j;


    /*
     * We ALWAYS get managed area from eeprom; ONLY way to change it
     */
    if (ven_eeprom_valid) {

	    VEN_info.gfx_info.xpmax = ven_eeprom.display_surface_width;
	    VEN_info.gfx_info.ypmax = ven_eeprom.display_surface_height;
	    VEN_info.vof_head_info[0].pan_x = ven_eeprom.pan_x;
	    VEN_info.vof_head_info[0].pan_y = ven_eeprom.pan_y;
	    VEN_info.vof_head_info[0].active = 1;
	    VEN_info.vof_head_info[0].cursor_enable = 1;
    } else {
	/*
	 * We're supposed to load from EEPROM, but it's bad.
	 * Make up some reasonable defaults and use them.
	 */
	VEN_info.gfx_info.xpmax = vofDisplay->vof_width;
	VEN_info.gfx_info.ypmax = vofDisplay->vof_height;
	VEN_info.vof_head_info[0].pan_x = 0;
	VEN_info.vof_head_info[0].pan_y = 0;
	VEN_info.vof_head_info[0].active = 1;
	VEN_info.vof_head_info[0].cursor_enable = 1;
    }

    /*
     * XXX If and when we choose to implement panning, the hardcoded ilOffset
     * here will need to change; perhaps a call to venice_clamp_dg2_pan() will
     * need to be made here, rather than in venice_load_vdrc_sram().
     */
    VEN_info.baseTileAddr = 0;
    VEN_info.ilOffset = 0;
    /*
     * XXX What about ven_eeprom.digipots[0..4]
     */

    /*
     * Copy the user's incoming vof_file_info structure into VEN_info; it
     * will either be the eeprom's 'vof_file_info' contents, or the 
     * vof_file_info contents read in from a dynamically loaded VOF.
     * This copy is unnecessary for VS2 mode which elsewhere populates the 
     * structure with a version that reflect's the VS2's head 0 content.
     */
    if (!VEN_info.going_to_vs2)
        VEN_info.vof_head_info[0].vof_file_info = *vofDisplay;

    /*
     * Copy the VOF's display screen table into the VEN_info struct, so
     * that the kernel's info structure will be updated with the valid
     * contents.  The display screen table is used to dynamically switch
     * the MCO cursor from screen to screen.
     */
    for (i = 0; i < 8; i++) {
	VEN_info.DG2_display_screen_table[i] = 
	    vofData->display_screen_table[i];
    }

    /*
     * Take kernel info and use it to initialize system
     */
    tileWidth = 80 * VEN_info.rm_count;
    t_width = (VEN_info.gfx_info.xpmax + (tileWidth - 1)) / tileWidth;
    t_height = (VEN_info.gfx_info.ypmax + 7) >> 3;
    t_size = t_width * t_height;

    if (t_size <= 512) {
	VEN_info.pixel_depth = V_DG_IE_CONFIG_LARGEPIX;
    } else if (t_size <= 1024) {
	VEN_info.pixel_depth = V_DG_IE_CONFIG_MEDPIX;
    } else if (t_size <= 2048) {
	VEN_info.pixel_depth = V_DG_IE_CONFIG_SMALLPIX;
    } else {
	xprintf("calcframebuffer: not enough tiles for the framebuffer\n");
	return(0);
    }
    VEN_info.tiles_per_line = t_width;

    /*
     * Use the vofc prepared table of pixel densities to determine what 
     * pixel density this VOF can operate at, given the HW configuration
     * we are running with.
     */
    switch (VEN_info.rm_count) {
	case 1:
	    i = 0;
	    break;
	case 2:
	    i = 1;
	    break;
	case 4:
	    i = 2;
	    break;
    }
    for (j = 2; j >= 0; j--) {
        if (vofDisplay->pix_density[i][j])
	    break;
    }

    switch (j) {
	case 0:
	    VEN_info.pixel_density = PIX26_RGB8;
	    break;
	case 1:
	    VEN_info.pixel_density = PIX32_RGB10;
	    break;
	case 2:
	    VEN_info.pixel_density = PIX42_RGBA10;
	    break;
	default:
	    xprintf(
"calcframebuffer: no pixel density will work, defaulting to 8bit RGB.\n");
	    VEN_info.pixel_density = PIX26_RGB8;
	    break;
    }

#ifdef DEBUG_PIXEL_DENSITY
    switch (VEN_info.pixel_density) {
	case PIX42_RGBA10:
	    xprintf("computed VEN_info.pixel_density = PIX42_RGBA10\n");
	    break;
	case PIX32_RGB10:
	    xprintf("computed VEN_info.pixel_density = PIX32_RGB10\n");
	    break;
	case PIX26_RGB8:
	    xprintf("computed VEN_info.pixel_density = PIX26_RGB8\n");
	    break;
    }
#endif


    VEN_info.hwalk_length = vofDisplay->hwalk_length;


    /*
     * Compute the proper value of the V_DG_IE_CONFIG_LO register,
     * and store in a (sigh) global, for abuse by venice_dg2initIMPS().
     *
     * Also tell the kernel what it is, so that it can turn on the global
     * stereo twiddling, if need be.
     */
    ie_config_lo = VEN_info.pixel_density;

    if ((vofDisplay->flags & VOF_F_STEREO) ||
        (vofDisplay->flags & VOF_F_OLD_STEREO)) {
        ie_config_lo |= V_DG_IE_CONFIG_GSTEREO;
    }
    if (vofDisplay->flags & VOF_F_SEQ_RGB) {
	ie_config_hi = (V_DG_IE_CONFIG_SWIZZLE_BLUE | VEN_info.pixel_depth);
    } else {
	ie_config_hi = VEN_info.pixel_depth;
    }


    return(1);
}


#include "sys/gfx.h"
#include "venice.h"
#include "vofdefs.h"
#include "voftypedefs.h"
#include "dg2_eeprom.h"
venice_dg2_eeprom_t backup_vof = {
	0,	/* prom_id */
	0,	/* prom_length */
	0,	/* prom_checksum */
	0,	/* prom_revision */
	1280,	/* display_surface_width */
	1024,	/* display_surface_height */
	0,	/* going_to_vs2 */
	0,	/* pan_x */
	0,	/* pan_y */
	{
		0,	/* digipots[0] */
		0,	/* digipots[1] */
		0,	/* digipots[2] */
		0,	/* digipots[3] */
		0,	/* digipots[4] */
	},
	{
		20,	/* normal_vof_top[0] */
		15649452,	/* normal_vof_top[1] */
		48,	/* normal_vof_top[2] */
		18,	/* normal_vof_top[3] */
		228,	/* normal_vof_top[4] */
		0,	/* normal_vof_top[5] */
		4,	/* normal_vof_top[6] */
		128,	/* normal_vof_top[7] */
		55,	/* normal_vof_top[8] */
		32,	/* normal_vof_top[9] */
		84,	/* normal_vof_top[10] */
		2,	/* normal_vof_top[11] */
		16,	/* normal_vof_top[12] */
		2,	/* normal_vof_top[13] */
		19,	/* normal_vof_top[14] */
		192,	/* normal_vof_top[15] */
		86,	/* normal_vof_top[16] */
	},
	{
		{
		2,	/* normal_vof_body.state_duration_table[0] */
		1,	/* normal_vof_body.state_duration_table[1] */
		13,	/* normal_vof_body.state_duration_table[2] */
		1,	/* normal_vof_body.state_duration_table[3] */
		19,	/* normal_vof_body.state_duration_table[4] */
		1,	/* normal_vof_body.state_duration_table[5] */
		255,	/* normal_vof_body.state_duration_table[6] */
		255,	/* normal_vof_body.state_duration_table[7] */
		255,	/* normal_vof_body.state_duration_table[8] */
		255,	/* normal_vof_body.state_duration_table[9] */
		3,	/* normal_vof_body.state_duration_table[10] */
		1,	/* normal_vof_body.state_duration_table[11] */
		1,	/* normal_vof_body.state_duration_table[12] */
		2,	/* normal_vof_body.state_duration_table[13] */
		1,	/* normal_vof_body.state_duration_table[14] */
		0,	/* normal_vof_body.state_duration_table[15] */
		0,	/* normal_vof_body.state_duration_table[16] */
		0,	/* normal_vof_body.state_duration_table[17] */
		0,	/* normal_vof_body.state_duration_table[18] */
		0,	/* normal_vof_body.state_duration_table[19] */
		0,	/* normal_vof_body.state_duration_table[20] */
		0,	/* normal_vof_body.state_duration_table[21] */
		0,	/* normal_vof_body.state_duration_table[22] */
		0,	/* normal_vof_body.state_duration_table[23] */
		0,	/* normal_vof_body.state_duration_table[24] */
		0,	/* normal_vof_body.state_duration_table[25] */
		0,	/* normal_vof_body.state_duration_table[26] */
		0,	/* normal_vof_body.state_duration_table[27] */
		0,	/* normal_vof_body.state_duration_table[28] */
		0,	/* normal_vof_body.state_duration_table[29] */
		0,	/* normal_vof_body.state_duration_table[30] */
		0,	/* normal_vof_body.state_duration_table[31] */
		0,	/* normal_vof_body.state_duration_table[32] */
		0,	/* normal_vof_body.state_duration_table[33] */
		0,	/* normal_vof_body.state_duration_table[34] */
		0,	/* normal_vof_body.state_duration_table[35] */
		0,	/* normal_vof_body.state_duration_table[36] */
		0,	/* normal_vof_body.state_duration_table[37] */
		0,	/* normal_vof_body.state_duration_table[38] */
		0,	/* normal_vof_body.state_duration_table[39] */
		0,	/* normal_vof_body.state_duration_table[40] */
		0,	/* normal_vof_body.state_duration_table[41] */
		0,	/* normal_vof_body.state_duration_table[42] */
		0,	/* normal_vof_body.state_duration_table[43] */
		0,	/* normal_vof_body.state_duration_table[44] */
		0,	/* normal_vof_body.state_duration_table[45] */
		0,	/* normal_vof_body.state_duration_table[46] */
		0,	/* normal_vof_body.state_duration_table[47] */
		0,	/* normal_vof_body.state_duration_table[48] */
		0,	/* normal_vof_body.state_duration_table[49] */
		0,	/* normal_vof_body.state_duration_table[50] */
		0,	/* normal_vof_body.state_duration_table[51] */
		0,	/* normal_vof_body.state_duration_table[52] */
		0,	/* normal_vof_body.state_duration_table[53] */
		0,	/* normal_vof_body.state_duration_table[54] */
		0,	/* normal_vof_body.state_duration_table[55] */
		0,	/* normal_vof_body.state_duration_table[56] */
		0,	/* normal_vof_body.state_duration_table[57] */
		0,	/* normal_vof_body.state_duration_table[58] */
		0,	/* normal_vof_body.state_duration_table[59] */
		0,	/* normal_vof_body.state_duration_table[60] */
		0,	/* normal_vof_body.state_duration_table[61] */
		0,	/* normal_vof_body.state_duration_table[62] */
		0,	/* normal_vof_body.state_duration_table[63] */
		},
		{
		0,	/* normal_vof_body.line_type_table[0] */
		1,	/* normal_vof_body.line_type_table[1] */
		2,	/* normal_vof_body.line_type_table[2] */
		3,	/* normal_vof_body.line_type_table[3] */
		2,	/* normal_vof_body.line_type_table[4] */
		4,	/* normal_vof_body.line_type_table[5] */
		5,	/* normal_vof_body.line_type_table[6] */
		5,	/* normal_vof_body.line_type_table[7] */
		5,	/* normal_vof_body.line_type_table[8] */
		5,	/* normal_vof_body.line_type_table[9] */
		5,	/* normal_vof_body.line_type_table[10] */
		6,	/* normal_vof_body.line_type_table[11] */
		7,	/* normal_vof_body.line_type_table[12] */
		2,	/* normal_vof_body.line_type_table[13] */
		8,	/* normal_vof_body.line_type_table[14] */
		0,	/* normal_vof_body.line_type_table[15] */
		0,	/* normal_vof_body.line_type_table[16] */
		0,	/* normal_vof_body.line_type_table[17] */
		0,	/* normal_vof_body.line_type_table[18] */
		0,	/* normal_vof_body.line_type_table[19] */
		0,	/* normal_vof_body.line_type_table[20] */
		0,	/* normal_vof_body.line_type_table[21] */
		0,	/* normal_vof_body.line_type_table[22] */
		0,	/* normal_vof_body.line_type_table[23] */
		0,	/* normal_vof_body.line_type_table[24] */
		0,	/* normal_vof_body.line_type_table[25] */
		0,	/* normal_vof_body.line_type_table[26] */
		0,	/* normal_vof_body.line_type_table[27] */
		0,	/* normal_vof_body.line_type_table[28] */
		0,	/* normal_vof_body.line_type_table[29] */
		0,	/* normal_vof_body.line_type_table[30] */
		0,	/* normal_vof_body.line_type_table[31] */
		0,	/* normal_vof_body.line_type_table[32] */
		0,	/* normal_vof_body.line_type_table[33] */
		0,	/* normal_vof_body.line_type_table[34] */
		0,	/* normal_vof_body.line_type_table[35] */
		0,	/* normal_vof_body.line_type_table[36] */
		0,	/* normal_vof_body.line_type_table[37] */
		0,	/* normal_vof_body.line_type_table[38] */
		0,	/* normal_vof_body.line_type_table[39] */
		0,	/* normal_vof_body.line_type_table[40] */
		0,	/* normal_vof_body.line_type_table[41] */
		0,	/* normal_vof_body.line_type_table[42] */
		0,	/* normal_vof_body.line_type_table[43] */
		0,	/* normal_vof_body.line_type_table[44] */
		0,	/* normal_vof_body.line_type_table[45] */
		0,	/* normal_vof_body.line_type_table[46] */
		0,	/* normal_vof_body.line_type_table[47] */
		0,	/* normal_vof_body.line_type_table[48] */
		0,	/* normal_vof_body.line_type_table[49] */
		0,	/* normal_vof_body.line_type_table[50] */
		0,	/* normal_vof_body.line_type_table[51] */
		0,	/* normal_vof_body.line_type_table[52] */
		0,	/* normal_vof_body.line_type_table[53] */
		0,	/* normal_vof_body.line_type_table[54] */
		0,	/* normal_vof_body.line_type_table[55] */
		0,	/* normal_vof_body.line_type_table[56] */
		0,	/* normal_vof_body.line_type_table[57] */
		0,	/* normal_vof_body.line_type_table[58] */
		0,	/* normal_vof_body.line_type_table[59] */
		0,	/* normal_vof_body.line_type_table[60] */
		0,	/* normal_vof_body.line_type_table[61] */
		0,	/* normal_vof_body.line_type_table[62] */
		0,	/* normal_vof_body.line_type_table[63] */
		},
		{
		334,	/* normal_vof_body.line_length_table[0] */
		334,	/* normal_vof_body.line_length_table[1] */
		334,	/* normal_vof_body.line_length_table[2] */
		334,	/* normal_vof_body.line_length_table[3] */
		334,	/* normal_vof_body.line_length_table[4] */
		334,	/* normal_vof_body.line_length_table[5] */
		334,	/* normal_vof_body.line_length_table[6] */
		334,	/* normal_vof_body.line_length_table[7] */
		334,	/* normal_vof_body.line_length_table[8] */
		0,	/* normal_vof_body.line_length_table[9] */
		0,	/* normal_vof_body.line_length_table[10] */
		0,	/* normal_vof_body.line_length_table[11] */
		0,	/* normal_vof_body.line_length_table[12] */
		0,	/* normal_vof_body.line_length_table[13] */
		0,	/* normal_vof_body.line_length_table[14] */
		0,	/* normal_vof_body.line_length_table[15] */
		},
		{
			{
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][0] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][1] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][2] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][3] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][4] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][5] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][6] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][7] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][8] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][9] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][10] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][11] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][12] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][13] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][14] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][15] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][16] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][17] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][18] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][19] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][20] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][21] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][22] */
		{{33,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][23] */
		{{315,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[0][24] */
			},
			{
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][0] */
		{{24,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][1] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][2] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][3] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][4] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][5] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][6] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][7] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][8] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][9] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][10] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][11] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][12] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][13] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][14] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][15] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][16] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][17] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][18] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][19] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][20] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][21] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][22] */
		{{33,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][23] */
		{{315,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[1][24] */
			},
			{
		{{0,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][0] */
		{{24,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][1] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][2] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][3] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][4] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][5] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][6] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][7] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][8] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][9] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][10] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][11] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][12] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][13] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][14] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][15] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][16] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][17] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][18] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][19] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][20] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][21] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][22] */
		{{33,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][23] */
		{{315,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[2][24] */
			},
			{
		{{0,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][0] */
		{{24,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][1] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][2] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][3] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][4] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][5] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][6] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][7] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][8] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][9] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][10] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][11] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][12] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][13] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][14] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][15] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][16] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][17] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][18] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][19] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][20] */
		{{33,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][21] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][22] */
		{{33,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][23] */
		{{315,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[3][24] */
			},
			{
		{{0,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][0] */
		{{24,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][1] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][2] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][3] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][4] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][5] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][6] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][7] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][8] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][9] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][10] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][11] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][12] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][13] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][14] */
		{{60,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][15] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][16] */
		{{245,250,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][17] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][18] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][19] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][20] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][21] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][22] */
		{{33,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][23] */
		{{315,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[4][24] */
			},
			{
		{{0,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][0] */
		{{24,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][1] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][2] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][3] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][4] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][5] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][6] */
		{{72,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][7] */
		{{328,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][8] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][9] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][10] */
		{{0,5,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][11] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][12] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][13] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][14] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][15] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][16] */
		{{245,250,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][17] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][18] */
		{{60,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][19] */
		{{316,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][20] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][21] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][22] */
		{{33,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][23] */
		{{315,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[5][24] */
			},
			{
		{{0,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][0] */
		{{24,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][1] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][2] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][3] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][4] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][5] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][6] */
		{{72,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][7] */
		{{328,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][8] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][9] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][10] */
		{{0,5,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][11] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][12] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][13] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][14] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][15] */
		{{316,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][16] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][17] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][18] */
		{{60,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][19] */
		{{316,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][20] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][21] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][22] */
		{{33,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][23] */
		{{315,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[6][24] */
			},
			{
		{{0,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][0] */
		{{24,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][1] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][2] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][3] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][4] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][5] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][6] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][7] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][8] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][9] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][10] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][11] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][12] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][13] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][14] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][15] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][16] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][17] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][18] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][19] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][20] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][21] */
		{{315,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][22] */
		{{33,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][23] */
		{{315,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[7][24] */
			},
			{
		{{0,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][0] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][1] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][2] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][3] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][4] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][5] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][6] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][7] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][8] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][9] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][10] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][11] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][12] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][13] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][14] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][15] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][16] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][17] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][18] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][19] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][20] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][21] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][22] */
		{{33,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][23] */
		{{315,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[8][24] */
			},
			{
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][0] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][1] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][2] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][3] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][4] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][5] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][6] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][7] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][8] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][9] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][10] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][11] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][12] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][13] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][14] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][15] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][16] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][17] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][18] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][19] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][20] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][21] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][22] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][23] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[9][24] */
			},
			{
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][0] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][1] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][2] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][3] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][4] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][5] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][6] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][7] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][8] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][9] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][10] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][11] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][12] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][13] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][14] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][15] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][16] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][17] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][18] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][19] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][20] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][21] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][22] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][23] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[10][24] */
			},
			{
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][0] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][1] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][2] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][3] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][4] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][5] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][6] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][7] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][8] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][9] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][10] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][11] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][12] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][13] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][14] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][15] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][16] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][17] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][18] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][19] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][20] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][21] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][22] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][23] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[11][24] */
			},
			{
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][0] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][1] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][2] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][3] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][4] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][5] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][6] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][7] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][8] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][9] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][10] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][11] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][12] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][13] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][14] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][15] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][16] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][17] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][18] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][19] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][20] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][21] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][22] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][23] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[12][24] */
			},
			{
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][0] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][1] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][2] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][3] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][4] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][5] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][6] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][7] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][8] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][9] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][10] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][11] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][12] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][13] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][14] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][15] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][16] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][17] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][18] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][19] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][20] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][21] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][22] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][23] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[13][24] */
			},
			{
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][0] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][1] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][2] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][3] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][4] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][5] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][6] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][7] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][8] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][9] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][10] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][11] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][12] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][13] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][14] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][15] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][16] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][17] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][18] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][19] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][20] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][21] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][22] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][23] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[14][24] */
			},
			{
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][0] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][1] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][2] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][3] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][4] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][5] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][6] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][7] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][8] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][9] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][10] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][11] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][12] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][13] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][14] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][15] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][16] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][17] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][18] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][19] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][20] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][21] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][22] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][23] */
		{{32768,32768,32768,32768,},},	/* normal_vof_body.signal_edge_table[15][24] */
			},
		},
		{
		833,	/* normal_vof_body.display_screen_table[0] */
		833,	/* normal_vof_body.display_screen_table[1] */
		833,	/* normal_vof_body.display_screen_table[2] */
		833,	/* normal_vof_body.display_screen_table[3] */
		833,	/* normal_vof_body.display_screen_table[4] */
		833,	/* normal_vof_body.display_screen_table[5] */
		833,	/* normal_vof_body.display_screen_table[6] */
		833,	/* normal_vof_body.display_screen_table[7] */
		},
		{
		1280,	/* normal_vof_body.vof_file_info.vof_width */
		1024,	/* normal_vof_body.vof_file_info.vof_height */
		330,	/* normal_vof_body.vof_file_info.cursor_fudge_x */
		0,	/* normal_vof_body.vof_file_info.cursor_fudge_y */
		0,	/* normal_vof_body.vof_file_info.flags */
		0,	/* normal_vof_body.vof_file_info.unused */
		1,	/* normal_vof_body.vof_file_info.fields_per_frame */
		0,	/* normal_vof_body.vof_file_info.field_with_uppermost_line */
		3,	/* normal_vof_body.vof_file_info.hwalk_length */
		60,	/* normal_vof_body.vof_file_info.vof_framerate */
		1,	/* normal_vof_body.vof_file_info.monitor_type */
		1680,	/* normal_vof_body.vof_file_info.vof_total_width */
		1065,	/* normal_vof_body.vof_file_info.vof_total_height */
		16,	/* normal_vof_body.vof_file_info.encoder_x_offset */
		2,	/* normal_vof_body.vof_file_info.encoder_y_offset */
		{
			{
			1,	/* normal_vof_body.vof_file_info.pix_density[0][0] */
			1,	/* normal_vof_body.vof_file_info.pix_density[0][1] */
			1,	/* normal_vof_body.vof_file_info.pix_density[0][2] */
			},
			{
			1,	/* normal_vof_body.vof_file_info.pix_density[1][0] */
			1,	/* normal_vof_body.vof_file_info.pix_density[1][1] */
			1,	/* normal_vof_body.vof_file_info.pix_density[1][2] */
			},
		},
		{
			1024,	/* normal_vof_body.vof_file_info.lines_in_field[0] */
			0,	/* normal_vof_body.vof_file_info.lines_in_field[1] */
			0,	/* normal_vof_body.vof_file_info.lines_in_field[2] */
			0,	/* normal_vof_body.vof_file_info.lines_in_field[3] */
			0,	/* normal_vof_body.vof_file_info.lines_in_field[4] */
			0,	/* normal_vof_body.vof_file_info.lines_in_field[5] */
			0,	/* normal_vof_body.vof_file_info.lines_in_field[6] */
			0,	/* normal_vof_body.vof_file_info.lines_in_field[7] */
		},
		},	/* end of vof_file_info_t initializations */
	},	/* end of vof_body_t initializations */
		/* end of aggregate initialization */
};
#include <gl/xilinx.h>

/*
 * Function:	 venice_clock_serial_dataBit()
 * 
 * Returns: 	 void
 *
 * Clocks the given parameter (a '1' or a '0') into the DG2's serial data 
 * pipeline by first latching it into the serial data register, then by forcing
 * a low -> high transition on the DG2's serial clock input (wired up to all 
 * xilii's CCLK) with a write to the dedicated register V_DG_SERIAL_CLOCK.
 * 
 * Writes to V_DG_SERIAL_CLOCK will strobe the clock low for 5 microseconds,
 * causing any xilinx in configuration mode to accept the serial data presented
 * on its serial data input.
 *
 * Only those xilii which have been put into the 'program' state will accept
 * the configuration data.  This function is the same for all xilii on the DG2.
 */
void
venice_clock_serial_dataBit(int value)
{
    /*
     * Latch the data, then write to the serial clock.
     */
    if (value) {
        venice_set_vcreg(V_DG_SER_BUS, V_DG_SER_DATA_MASK, 0);
    } else {
        venice_set_vcreg(V_DG_SER_BUS, 0, 0);
    }
    venice_set_vcreg(V_DG_SERIAL_CLOCK, 1, 0);
}

void
venice_clock_serial_dataByte(int value);
#if 0
{
    int i;
    value <<= 3;  /* since mask == 8 XXXX ugly */

    for(i = 0; i < 8; i++) {
        venice_set_vcreg(V_DG_SER_BUS, V_DG_SER_DATA_MASK & value, 0);
	venice_set_vcreg(V_DG_SERIAL_CLOCK, 1, 0);
	value >>= 1;
    }
}
#endif

/*
 * Function:	 venice_wait_configMem_clear()
 * 
 * Returns: 	 1 if configuration memory clearing is successful,
 *		-1 otherwise.
 *
 * If a 3000 series or 4000 series xilinx part has the INIT pin wired up,
 * you can poll it until it goes high to determine when the memory clear
 * operation has completed.
 *
 * This function simply sleeps for a clock tick (10 msec), then returns 1.
 * The maximum time for configuration memory clearing is ~750 usec for the 
 * biggest 3000 series xilinx, and ~100 usec for the biggest 4000 series xilinx.
 *
 * Also, after the INIT pin goes high, you must wait 4 usec before issuing the
 * first CCLK to the 4000 series parts (this is NOT in the data book yet).
 *
 * So, wait a LONG time.  sginap(2), because an sginap(1) might return
 * immediately if the sginap request lines up with the CPU 10ms clock tick.
 *
 * This function is the same for all xilii on the DG2 (unlees the INIT pins
 * get hooked up).
 */
int
venice_wait_configMem_clear(void)
{
    sginap(2); /* wait 2, in case we are aligned with a clock tick. */
    return(1);
}

/*
 * Function:	 venice_write_vfc_reset()
 * 
 * Returns: 	 void
 *
 * Writes the given parameter (a '1' or a '0') to the VFC's reset pin.
 *
 * We sleep for 10 ms after writing it (the minimum amount of time we can
 * sleep with the sginap call) so that we can meet a minimum 6 usec RESET
 * low chip requirement.
 */
void
venice_write_vfc_reset(int value)
{
    venice_set_vcreg(V_DG_VFC_RESET_L, (value & 0x1), 0);
    sginap(2); /* wait 2, in case we are aligned with a clock tick. */
}

/*
 * Function:	 venice_write_vfc_doneProg()
 * 
 * Returns: 	 void
 *
 * Writes the given parameter (a '1' or a '0') to the VFC's doneProg pin.
 *
 * We sleep for 10 ms after writing it (the minimum amount of time we can
 * sleep with the sginap call) so that we can meet a minimum 6 usec doneProg
 * low chip requirement.
 */
void
venice_write_vfc_doneProg(int value)
{
    venice_set_vcreg(V_DG_VFC_PROG_L, (value & 0x1), 0);
    sginap(2); /* wait 2, in case we are aligned with a clock tick. */
}

/*
 * Function:	 venice_read_vfc_doneProg()
 * 
 * Returns: 	 int
 *
 * Reads the state of the VFC's doneProg pin, and returns this value. 
 */
int
venice_read_vfc_doneProg(void)
{
    unsigned int doneStatus;

    doneStatus = venice_get_vcreg(V_DG_XILINX_DONE_PROG);

    if ((V_DG_VFC_CTRL_DONE_MASK & doneStatus) == V_DG_VFC_CTRL_DONE_MASK ) {
	return(1);
    } else {
	return(0);
    }
}


venice_load_vfc_xilinx(unsigned char *buffer) {

    venice_set_vcreg(V_DG_VFC_RDBCK_TRIG, 0x1, 0);
    venice_set_vcreg(V_DG_SERIAL_CLK_EN_L, 0x0, 0);

    return load_3000_serial_byte(
	venice_write_vfc_reset,
	venice_write_vfc_doneProg,
	venice_wait_configMem_clear,
	venice_clock_serial_dataByte,
	venice_read_vfc_doneProg,
	XC3064,
	buffer);
}

/*
 * Function:	 venice_write_vdrc_reset()
 * 
 * Returns: 	 void
 *
 * Writes the given parameter (a '1' or a '0') to the VDRC's reset pin.
 *
 * We sleep for 10 ms after writing it (the minimum amount of time we can
 * sleep with the sginap call) so that we can meet a minimum 6 usec RESET
 * low chip requirement.
 */
void
venice_write_vdrc_reset(int value)
{
    venice_set_vcreg(V_DG_VDRC_RESET_L, (value & 0x1), 0);
    sginap(2); /* wait 2, in case we are aligned with a clock tick. */
}

/*
 * Function:	 venice_write_vdrc_doneProg()
 * 
 * Returns: 	 void
 *
 * Writes the given parameter (a '1' or a '0') to the VDRC's doneProg pin.
 *
 * We sleep for 10 ms after writing it (the minimum amount of time we can
 * sleep with the sginap call) so that we can meet a minimum 6 usec doneProg
 * low chip requirement.
 */
void
venice_write_vdrc_doneProg(int value)
{
    venice_set_vcreg(V_DG_VDRC_PROG_L, (value & 0x1), 0);
    sginap(2); /* wait 2, in case we are aligned with a clock tick. */
}

/*
 * Function:	 venice_read_vdrc_doneProg()
 * 
 * Returns: 	 int
 *
 * Reads the VDRC's doneProg status, and returns this value.  
 */
int
venice_read_vdrc_doneProg(void)
{
    unsigned int doneStatus;

    doneStatus = venice_get_vcreg(V_DG_XILINX_RDBCK_2);

    if ((V_DG_VDRC_CTRL_DONE_MASK & doneStatus) == V_DG_VDRC_CTRL_DONE_MASK ) {
	return(1);
    } else {
	return(0);
    }
}

venice_load_vdrc_xilinx(unsigned char *buffer) {

    venice_set_vcreg(V_DG_VDRC_RDBCK_TRIG, 0x1, 0);
    venice_set_vcreg(V_DG_SERIAL_CLK_EN_L, 0x0, 0);

    return load_3000_serial_byte(
	venice_write_vdrc_reset,
	venice_write_vdrc_doneProg,
	venice_wait_configMem_clear,
	venice_clock_serial_dataByte,
	venice_read_vdrc_doneProg,
	XC3064,
	buffer);
}

/*
 * Function:	 venice_write_vof_program()
 * 
 * Returns: 	 void
 *
 * Writes the given parameter (a '1' or a '0') to the VOF's PROG pin.
 */
void
venice_write_vof_program(int value)
{
    venice_set_vcreg(V_DG_VOF_PROG_L, (value & 0x1), 0);
}

/*
 * Function:	 venice_check_vof_frame_status()
 * 
 * Returns: 	 int
 *
 * Reads from the VOF's INIT pin, and returns its value.  A 'High' state
 * indicates that the current configuration memory frame is good.
 *
 * The INIT pin is currently not wired, so this is a dummy function instead
 * which always returns a TRUE ('High') condition.
 */
int
venice_check_vof_frame_status(void)
{
    return(1); /* INIT (frame okay) is an active high signal */
}

/*
 * Function:	 venice_read_vof_done()
 * 
 * Returns: 	 int
 *
 * Reads the VOF's DONE status, and returns this value.  
 */
int
venice_read_vof_done(void)
{
    unsigned int doneStatus;

    doneStatus = venice_get_vcreg(V_DG_XILINX_DONE_PROG);

    if ((V_DG_VOF_CTRL_DONE_MASK & doneStatus) == V_DG_VOF_CTRL_DONE_MASK ) {
	return(1);
    } else {
	return(0);
    }
}

venice_load_vof_xilinx(unsigned char *buffer) {

    venice_set_vcreg(V_DG_XILINX_RDBCK_TRIG, 0xf, 0);
    venice_set_vcreg(V_DG_SERIAL_CLK_EN_L, 0x0, 0);

    return load_4000_serial_byte(
	venice_write_vof_program,
	venice_wait_configMem_clear,
	venice_clock_serial_dataByte,
	venice_check_vof_frame_status,
	venice_read_vof_done,
	XC4005,
	buffer);
}

/*
 * Function:	 venice_write_vif_program()
 * 
 * Returns: 	 void
 *
 * Writes the given parameter (a '1' or a '0') to the VIF's PROG pin.
 */
void
venice_write_vif_program(int value)
{
    venice_set_vcreg(V_DG_VIF_PROG_L, (value & 0x1), 0);
}

/*
 * Function:	 venice_check_vif_frame_status()
 * 
 * Returns: 	 int
 *
 * Reads from the VIF's INIT pin, and returns its value.  A 'High' state
 * indicates that the current configuration memory frame is good.
 *
 * The INIT pin is currently not wired, so this is a dummy function instead
 * which always returns a TRUE ('High') condition.
 */
int
venice_check_vif_frame_status(void)
{
    return(1); /* INIT (frame okay) is an active high signal */
}

/*
 * Function:	 venice_read_vif_done()
 * 
 * Returns: 	 int
 *
 * Reads the VIF's DONE program status, and returns this value.
 */
int
venice_read_vif_done(void)
{
    unsigned int doneStatus;

    doneStatus = venice_get_vcreg(V_DG_XILINX_DONE_PROG);

    if ((V_DG_VIF_CTRL_DONE_MASK & doneStatus) == V_DG_VIF_CTRL_DONE_MASK ) {
	return(1);
    } else {
	return(0);
    }
}

venice_load_vif_xilinx(unsigned char *buffer) {

    venice_set_vcreg(V_DG_XILINX_RDBCK_TRIG, 0xf, 0);
    venice_set_vcreg(V_DG_SERIAL_CLK_EN_L, 0x0, 0);

    return load_4000_serial_byte(
	venice_write_vif_program,
	venice_wait_configMem_clear,
	venice_clock_serial_dataByte,
	venice_check_vif_frame_status,
	venice_read_vif_done,
	XC4003,
	buffer);
}

/*
 * Function:	 venice_write_di_ctrl_program()
 * 
 * Returns: 	 void
 *
 * Writes the given parameter (a '1' or a '0') to the DI_CONTROL's PROG pin.
 */
void
venice_write_di_ctrl_program(int value)
{
    venice_set_vcreg(V_DG_DI_CNTRL_PROG_L, (value & 0x1), 0);
}

/*
 * Function:	 venice_check_di_ctrl_frame_status()
 * 
 * Returns: 	 int
 *
 * Reads from the DI_CONTROL's INIT pin, and returns its value.  A 'High' state
 * indicates that the current configuration memory frame is good.
 *
 * The INIT pin is currently not wired, so this is a dummy function instead
 * which always returns a TRUE ('High') condition.
 */
int
venice_check_di_ctrl_frame_status(void)
{
    return(1); /* INIT (frame okay) is an active high signal */
}

/*
 * Function:	 venice_read_di_ctrl_done()
 * 
 * Returns: 	 int
 *
 * Reads the DI_CONTROL's DONE program status, and returns this value.
 */
int
venice_read_di_ctrl_done(void)
{
    unsigned int doneStatus;

    doneStatus = venice_get_vcreg(V_DG_XILINX_DONE_PROG);

    if ((V_DG_DI_CTRL_DONE_MASK & doneStatus) == V_DG_DI_CTRL_DONE_MASK ) {
	return(1);
    } else {
	return(0);
    }
}

venice_load_di_ctrl_xilinx(unsigned char *buffer) {

    venice_set_vcreg(V_DG_XILINX_RDBCK_TRIG, 0xf, 0);
    venice_set_vcreg(V_DG_SERIAL_CLK_EN_L, 0x0, 0);

    return load_4000_serial_byte(
	venice_write_di_ctrl_program,
	venice_wait_configMem_clear,
	venice_clock_serial_dataByte,
	venice_check_di_ctrl_frame_status,
	venice_read_di_ctrl_done,
	XC4003,
	buffer);
}



