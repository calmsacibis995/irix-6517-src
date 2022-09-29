/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef MGRAS_DIAG_H

#define	MGRAS_DIAG_H
#define	MCO_DIAG_H


/*
 *  $Revision: 1.19 $
 */
#include "sys/types.h"
#include <sys/cpu.h>

void compare(char *, int, unsigned int, unsigned int, unsigned int, __uint32_t *errors);
#define COMPARE(str,addr,exp,rcv,mask,errors) \
	compare(str,addr,exp,rcv,mask,&errors)

/* Misc data that used to be in the header files.
 */
#define         MCO_VC2_TEST                             0
#define         MCO_VC2_ADDR_BUS_TEST                    1
#define         MCO_VC2_DATA_BUS_TEST                    2
#define         MCO_VC2_ADDR_UNIQ_TEST                   3
#define         MCO_VC2_PATRN_TEST                       4
#define         MCO_VC2_REG_TEST                         5
#define		MCO_VC2_SRAM_64			  	 6

#define         MCO_7162_CLRPAL_WALK_BIT_TEST            7
#define         MCO_7162_CLRPAL_ADDRUNIQ_TEST            8
#define         MCO_7162_CLRPAL_PATRN_TEST               9
#define         MCO_7162_CNTRL_REG_TEST                 10
#define         MCO_7162_MODE_REG_TEST                  11
#define         MCO_7162_ADDR_REG_TEST                  12

#define         MCO_473_CLRPAL_WALK_BIT_TEST            13
#define         MCO_473_CLR_PLLT_ADDR_UNIQ_TEST         14
#define         MCO_473_CLR_PLLT_PATRN_TEST             15
#define         MCO_473_OVRLY_PLLT_WALK_BIT_TEST        16
#define         MCO_473_OVRLY_PLLT_ADDR_UNIQ_TEST       17
#define         MCO_473_OVRLY_PLLT_PATRN_TEST           18
#define         MCO_473_ADDR_REG_TEST                   19

#define         MCO_INDEX_REG_TEST                      20
#define         MCO_ISOS_READREG_TEST                   21
#define         MCO_ISOS_CHECKSUM_TEST			22
#define         MCO_DACA_CRC_TEST			23
#define         MCO_DACB_CRC_TEST			24

#define OS_VGA (MCO_CTL1_OSMODEA | MCO_CTL1_OSMODEB)
#define IS_VGA 0x00

#define OS_MINI (MCO_CTL1_OSMODEA | MCO_CTL1_OSMODEB)
#define IS_MINI (MCO_CTL1_ISMODEC)

#define OS_FIELD (MCO_CTL1_OSMODEC)
#define IS_FIELD (MCO_CTL1_ISMODEB)

#define REFRESH_START	36

typedef struct _Test_Info {
	char *TestStr;
	char *ErrStr;
} _Test_Info;

extern int mco_report_passfail(_Test_Info *Test, int errors);
extern void msg_printf(int msglevel, char *fmt, ...);
extern __uint32_t _mco_initfpga(char *ucodefilename);
#ifdef IP30
extern __uint32_t _sr_mco_initfpga(char *ucodefilename);
#endif	/* IP30 */
extern void us_delay(uint);
extern void bzero(void *, size_t);
extern void bzero(void *, size_t);
extern int sprintf(char *, const char *, ...);
extern void atohex(const char *, int *);
extern char *atob(const char *cp, int *iptr);

extern void mco_7162SetRGB(mgras_hw *, __uint32_t, __uint32_t, __uint32_t);

#define REPORT_PASS_OR_FAIL(Test, errors) \
	return mco_report_passfail(Test,errors)

#define CORRUPT_DCB_BUS(base) {				\
        msg_printf(DBG, "Corrupting the DCB Bus...\n");	\
	MCO_ADV7162_SETADDR(base, MCO_ADV7162A, MCO_ADV7162_ADDR);    \
        mco_7162SetRGB(base, 0xc3, 0x3c, 0x5a);	\
}

typedef struct ramDAC7162Def {
    unsigned int id;     /* 0x79 */
    unsigned int addr;   /* MCO_ADV7162A or MCO_ADV7162B */
    unsigned int mode;   /* MCO_ADV7162_MODE_INIT */
    unsigned int cmd1;   /* MCO_ADV7162_CMD1_INIT */
    unsigned int cmd2;   /* MCO_ADV7162_CMD2_INIT */
    unsigned int cmd3;   /* MCO_ADV7162_CMD3_INIT */
    unsigned int cmd4;   /* MCO_ADV7162_CMD4_INIT */
    unsigned int cmd5;   /* MCO_ADV7162_CMD5_INIT */
    unsigned int pllr;
    unsigned int pllv;
    unsigned int pllctl; /* s, psel, vsel, rsel, PLL_ENA */
    int freq;            /* DesiredFrequencyDAC0--75.6MHz is written as 756000 */
    unsigned int curs;   /* MCO_ADV7162_CURS_DISABLE */
    unsigned short *lut;
} ramDAC7162Def;

typedef struct ramDAC473Def {
    unsigned int addr;   /* MCO_ADV473A or MCO_ADV473B */
    unsigned int cmd;
    unsigned short *lut;
} ramDAC473Def;

typedef struct VC2Def {
    unsigned int flag;	  /* see #defines above */
    unsigned int  w, h;	  /* monitor resolution */
    unsigned short *ftab;	  /* video timing frame table */
    unsigned int ftab_len;	  /* length (in shorts) of *ftab */
    unsigned int ftab_addr;        /* VC2_VID_FRAME_TAB_ADDR  */
    unsigned short *ltab;	  /* video timing line table */
    unsigned int ltab_len;	  /* length (in shorts) of *ltab */
    unsigned int config;
} ramVC2Def;

typedef struct mcoConfigDef {
    char *AlteraName;
    __uint32_t MgrasTimingSelect;
    struct ramDAC7162Def *mcoDAC7162A;
    struct ramDAC7162Def *mcoDAC7162B;
    struct ramDAC473Def  *mcoDAC473A;
    struct ramDAC473Def  *mcoDAC473B;
    struct ramDAC473Def  *mcoDAC473C;
    struct VC2Def *mcoVC2;
    unsigned int cntrl1;
    unsigned int cntrl2;
    unsigned int cntrl3;
} mcoConfigDef;

struct VOFtable {
    char *name;		/* User typable MCO format */
    __uint32_t TimingToken;
};

/***************************************************************************
 *									   *
 *									   *
 *	1572 External PLL Clock Definitions 			           *
 *									   *
 *									   *
 ***************************************************************************/

#define		ICS_PLL		(0xD0)


/***************************************************************************
 *									   *
 *									   *
 *	7162 DAC Definitions 				           *
 *									   *
 *									   *
 ***************************************************************************/



#define		DAC7162_ID		0x79
#define		DAC7162_REVISION	0x1 

#define		DAC7162_ID_OLD		0x76
#define		DAC7162_REVISION_OLD	0x0 

#define		VC2_RAMSIZE	0x2000 

#define         BLANK   {"", ""}

extern _Test_Info MCOBackEnd[];

extern __uint32_t mco_patrn[12];
extern ushort_t mco_walk_1_0[34];
extern __uint32_t mco_walk_1_0_32_rss[64];
extern __uint32_t  mco_walk_1_0_32[8];
extern ushort_t mco_walking_patrn[16];

#define HQ3_GIOBUS_TEST			0
#define HQ3_HQBUS_TEST			1
#define HQ3_REGISTERS_TEST		2	
#define HQ3_TLB_TEST			3	
#define HQ3_REIF_CTX_TEST		4	
#define HQ3_HAG_CTX_TEST		5	
#define HQ3_UCODE_DATA_BUS_TEST		6	
#define HQ3_UCODE_ADDR_BUS_TEST		7	
extern ushort_t walk_1_0[34];
extern __uint32_t  walk_1_0_32[8];
extern __uint32_t mco_patrn[12];

/***************************************************************************
 *									   *
 *									   *
 *	7162 DAC Crc declarations     				           *
 *									   *
 *									   *
 ***************************************************************************/

/***************************************************************************
 *									   *
 *									   *
 *	ADV473 DAC Definitions 				           *
 *									   *
 *									   *
 ***************************************************************************/

#define MCO_ADV473_SETOVRLAYWR(base, device, ovrlay_address) \
    MCO_ADV473_SETADDR(base, device, MCO_ADV473_OWR); \
    MCO_IO_WRITE(base,ovrlay_address)

#define MCO_ADV473_SETOVRLAYRD(base, device, ovrlay_address) \
    MCO_ADV473_SETADDR(base, device, MCO_ADV473_ORD); \
    MCO_IO_WRITE(base,ovrlay_address)

/* ADV473 Address Register MUST be set before using this macro! */
#define mco_473SetRGB(base,r,g,b)				\
	{							\
		MCO_IO_WRITE(base, (r & 0xff));			\
		MCO_IO_WRITE(base, (g & 0xff));			\
		MCO_IO_WRITE(base, (b & 0xff));			\
	}

/* ADV473 Address Register MUST be set before using this macro! */
#define mco_473GetRGB(base,r,g,b)			\
	{						\
		MCO_IO_READ(base, r);		\
		MCO_IO_READ(base, g);		\
		MCO_IO_READ(base, b);		\
	}

/* ADV473 Address Register MUST be set before using this macro! */
#define mco_473SetCtrl(base, device, data)			\
	{							\
		MCO_SETINDEX(base, device, MCO_ADV473_CTRL);		\
		MCO_IO_WRITE(base, data);			\
	}

/* ADV473 Address Register MUST be set before using this macro! */
#define mco_473GetCtrl(base, device, Rcv) 			\
	{								\
		MCO_SETINDEX(base, device, MCO_ADV473_CTRL);		\
		MCO_IO_READ(base, Rcv);					\
	}

#define mco_473SetModeReg(base,device,data)		\
	{						\
		MCO_ADV473_SETADDR(base, device, MCO_ADV473_MODE);	\
		MCO_IO_WRITE(base,(data & 0xff));			\
	}

#define mco_473GetModeReg(base, device, Rcv)			\
	{								\
		MCO_ADV473_SETADDR(base, device, MCO_ADV473_MODE);	\
		MCO_IO_READ(base, Rcv);				\
	}

#define mco_473SetAddrReg(base, device, addr)		\
	{							\
		MCO_ADV473_SETADDR(base, device, MCO_ADV473_RAMWR);	\
		MCO_IO_WRITE(base,(addr & 0xff));			\
	}

#define mco_473GetAddrReg(base, device, Rcv)				\
	{								\
		MCO_ADV473_SETADDR(base, device, MCO_ADV473_RAMRD);	\
		MCO_IO_READ(base, Rcv);				\
	}

#define mco_473SetAddrCmd(base, device, addr, data)			\
	{							\
		mco_473SetAddrReg(base, device, addr);			\
		MCO_SETINDEX(base, device, MCO_ADV473_CTRL);		\
		MCO_IO_WRITE(base, data);			\
	}

#define mco_473GetAddrCmd(base, device, addr, Rcv) 			\
	{								\
		mco_473SetAddrReg(base, device, addr);		\
		MCO_SETINDEX(base, device, MCO_ADV473_CTRL);		\
		MCO_IO_READ(base, Rcv);				\
	}

/***************************************************************************
 *									   *
 *									   *
 *	MCO Miscellaneous Diag Definitions 			           *
 *									   *
 *									   *
 ***************************************************************************/

#define		MCO_ISOS_TESTREG	0x40

#define		MCO_RIS_RESULT	BIT(0) 	/* Red Input Swizzle Test Result */
#define		MCO_GIS_RESULT	BIT(1) 	/* Green Input Swizzle Test Result */
#define		MCO_BIS_RESULT	BIT(2) 	/* Blue Input Swizzle Test Result */
#define		MCO_OS1_RESULT	BIT(4) 	/* Output Swizzle 1 Test Result */
#define		MCO_OS2_RESULT	BIT(5) 	/* Output Swizzle 2 Test Result */
#define		MCO_OS3_RESULT	BIT(6) 	/* Output Swizzle 3 Test Result */
#define		MCO_OS4_RESULT	BIT(7) 	/* Output Swizzle 4 Test Result */


#define	MCO_SWIZZLE_RESULT_MASK 0xF7

/***************************************************************************
 *									   *
 *									   *
 *	VC2 Definitions 				           *
 *									   *
 *									   *
 ***************************************************************************/

#define		_1600_1200_60		0x156
#define		_640_480_NTSC		0x150

#define	FRAME_BOTTOM	0x0

/*
 * VC2 index register values. (Stolen from gfx/kern/sys/vc2.h)
 */

#define VC2_VIDEO_ENTRY_PTR     0
#define VC2_CURS_ENTRY_PTR      1
#define VC2_CURS_X_LOC          2
#define VC2_CURS_Y_LOC          3
#define VC2_CURRENT_CURS_X_LOC  4
#define VC2_DID_ENTRY_PTR       5
#define VC2_SCAN_LENGTH         6
#define VC2_RAM_ADDR            7
#define VC2_VT_FRAME_PTR        8
#define VC2_VT_LINESEQ_PTR      9
#define VC2_VT_LINES_IN_RUN     10
#define VC2_VERTICAL_LINE_CTR   11
#define VC2_CURS_TABLE_PTR      12
#define VC2_WORKING_CURS_Y      13
#define VC2_DID_FRAME_PTR       14
#define VC2_DID_LINE_TABLE_PTR  15
#define VC2_DC_CONTROL          16
#define VC2_CONFIG              31

/*
 * VC2 CONFIG register bits (Stolen from gfx/kern/sys/vc2.h)
 */

#define VC2_RESET       BIT(0)
#define VC2_SLOW_CLOCK  BIT(1)
#define VC2_CUR_ERR  	BIT(2)
#define VC2_DID_ERR  	BIT(3)
#define VC2_VTG_ERR  	BIT(4)
#define VC2_REVISION_SHIFT      5
#define VC2_REVISION_MASK       (0xf << VC2_REVISION_SHIFT)
#define VC2_TEMP_HIBYTE_SHIFT	8
#define VC2_TEMP_HIBYTE_MASK	(0xff << VC2_TEMP_HIBYTE_SHIFT)


/*
 * VC2 DC_CONTROL register bits (Stolen from gfx/kern/sys/vc2.h)
 */

#define VC2_ENA_VINTR           BIT(0)
#define VC2_ENA_DISPLAY         BIT(1)
#define VC2_ENA_VIDEO           BIT(2)
#define VC2_ENA_DIDS            BIT(3)
#define VC2_ENA_CURS_FUNCTION   BIT(4)
#define VC2_ENA_GENSYNC         BIT(5)
#define VC2_ENA_INTERLACE       BIT(6)
#define VC2_ENA_CURS_DISPLAY    BIT(7)
#define VC2_ENA_CROSS_CURS      BIT(8)
#define VC2_CURS_GLYPH_64       BIT(9)
#define VC2_GENLOCK_SELECT      BIT(10)

/*
 *  Define the VC2 memory map
 *  (Stolen from gfx/kern/sys/vc2.h)
 *
 *  These address are 16 bit WORD addresses
 */

#define VC2_VID_LINE_TAB_ADDR		0x0
#define VC2_VID_FRAME_TAB_ADDR		0x400 /* 2K bytes for Vid line tab   */

/* MCO-specific Macros  (Stolen from gfx/kern/sys/vc2.h) */
#define MCO_VC2_GETREG(base,index,data16)                                \
    MCO_VC2_INDEX_WRITE(base, index);					\
    MCO_VC2_DATA_READ(base, data16);

#ifdef NOTYET
#define MCO_VC2_GETREG(base,index,data16)                                \
    base->mco_mpa1.mco.vc2index = index;                                     \
    data16 = base->mco_mpa1.mco.vc2data;
#endif /* NOTYET */


#define MCO_VC2_SETRAM64(mgbase,data)  {             			\
	mgbase->mco_mpa1.mco.vc2ram64 = data;					\
}

#define MCO_VC2_GETRAM64(mgbase,data) {             			\
	data = mgbase->mco_mpa1.mco.vc2ram64;					\
}

#define MCO_VC2_STARTQNDCLOCK {				\
	MCO_SETINDEX(mgbase, MCO_CONTROL2, 0);		\
	MCO_IO_WRITE(mgbase, 0x5);			\
}

#define MCO_WAIT_FOR_NOT_VBLANK() {		\
	ushort_t	lncount;	\
	do {				\
		mgras_vc3GetReg(mgbase,VC3_VERT_LINE_COUNT,lncount); \
	} while (lncount < 51);	\
}

#define MCO_WAIT_FOR_VBLANK() {		\
	ushort_t	lncount;	\
	do {				\
		mgras_vc3GetReg(mgbase,VC3_VERT_LINE_COUNT,lncount); \
	} while (lncount != 50); \
}

#ifdef NOTYET
#define MCO_WAIT_FOR_VBLANK() {		\
	ushort_t	lncount;	\
	do {				\
		MCO_VC2_GETREG(mgbase,VC2_VERTICAL_LINE_CTR,lncount); \
	} while (lncount > FRAME_BOTTOM+10); \
}
#endif /* NOTYET */

/***************************************************************************
 *									   *
 *									   *
 *	ipaste declarations     				           *
 *									   *
 *									   *
 ***************************************************************************/

extern unsigned char *ipaste_data;
extern char ipaste_filename[256];

/***************************************************************************
 *									   *
 *									   *
 *	MCO format declarations     				           *
 *									   *
 *									   *
 ***************************************************************************/

/* Defines for MGRAS VC3 formats */
#define		_1280_960_60_MCO_4VGA	0x103
#define		_1288_961_60_MCO_VGA_MIN	0x157
#define		_1608_1201_60_MCO_SVGA_MIN	0x158
#define		_1600_1200_60_MCO_SVGA_4SCRNTST	0x159

/* Defines for MCO VC2 formats */
#define		_MINI_640x480_60	0x157
#define		_MINI_800x600_60	0x158
#define		_four_test_800x600_60	0x159

#endif /* MGRAS_DIAG_H */
