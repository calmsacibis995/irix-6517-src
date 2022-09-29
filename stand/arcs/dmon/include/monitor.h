#ident	"$Id: monitor.h,v 1.1 1994/07/20 22:55:02 davidl Exp $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


/*
 * Machine Specific Defines
 */
#undef	NVRAM_BASE
#undef	DUART0_BASE
#undef	LED_BASE

#ifdef RC6280
#define LED_BASE	(LED_REG_R6300 | K1BASE)
#define DUART0_BASE	(DUART0_BASE_R6300 | K1BASE)
#define NVRAM_BASE	(TODC_CLOCK_ADDR_R6300 | K1BASE)

#define K1MEM_MAXMB	384		/* max main memory in k1seg */
#define K1MEM_MAX	(384<<20)	/* max main memory in k1seg */
#define HIMEM_BASE	(512<<20)	/* base address of memory above 384MB */
#endif 

#ifdef RB3125
#define LED_BASE	(LED_REG_R3200 | K1BASE)
#define DUART0_BASE	(DUART0_BASE_R3200 | K1BASE)
#define NVRAM_BASE	(TODC_CLOCK_ADDR_R3200 | K1BASE)

#define K1MEM_MAXMB	384		/* max main memory in k1seg */
#define K1MEM_MAX	(384<<20)	/* max main memory in k1seg */
#endif 

#ifdef IP24
#define TODC_CLOCK_ADDR_R4230   0x1fbe0000      /* MK48T02 on aftershock */
#define NVRAM_BASE      (TODC_CLOCK_ADDR_R4230 | K1BASE)
#define LED_BASE	0xbbfd9870
#define PROM_RESET	0xa0000000 | 0x1fc00000
#endif

#if !defined(IP24) & defined(IP22)
#define TODC_CLOCK_ADDR_R4230   0x1fbe0000      /* MK48T02 on aftershock */
#define NVRAM_BASE      (TODC_CLOCK_ADDR_R4230 | K1BASE)
#define LED_BASE	0xbbfd9870
#define PROM_RESET	0xa0000000 | 0x1fc00000
#endif

/*
 * ASCII characters
 */
#define char_BL		0x07	/* bell */
#define char_BS		0x08	/* backspace */
#define char_NL		0x0A	/* new line */
#define char_CR		0x0D	/* carriage return */
#define char_DEL	0x7f	/* delete */
#define char_TAB	0x09	/* tab */
#define char_ESC	0x1B	/* escape */
#define char_CTRLC	0x03	/* ctrl-C */
#define char_SP		0x20	/* ' ' */
#define char_DASH	0x2D	/* '-' */

#define char_A 		0x41	/* 'A' */
#define char_B		0x42	/* 'B' */
#define char_C		0x43	/* 'C' */
#define char_D		0x44	/* 'D' */
#define char_E		0x45	/* 'E' */
#define char_F 		0x46	/* 'F' */
#define char_G 		0x47	/* 'G' */
#define char_H		0x48	/* 'H' */
#define char_I		0x49	/* 'I' */
#define char_J		0x4A	/* 'J' */
#define char_K		0x4B	/* 'K' */
#define char_L		0x4C	/* 'L' */
#define char_M		0x4D	/* 'M' */
#define char_N		0x4E	/* 'N' */
#define char_O		0x4F	/* 'O' */
#define char_P		0x50	/* 'P' */
#define char_Q		0x51	/* 'Q' */
#define char_R		0x52	/* 'R' */
#define char_S		0x53	/* 'S' */
#define char_T		0x54	/* 'T' */
#define char_U		0x55	/* 'U' */
#define char_V 		0x56	/* 'V' */
#define char_W 		0x57	/* 'W' */
#define char_X		0x58	/* 'X' */
#define char_Y		0x59	/* 'Y' */
#define char_Z		0x5A	/* 'Z' */

#define char_a 		0x61	/* 'a' */
#define char_f 		0x66	/* 'f' */
#define char_y 		0x79	/* 'y' */

#define char_0 		0x30	/* '0' */
#define char_1 		0x31	/* '1' */
#define char_2 		0x32	/* '2' */
#define char_3 		0x33	/* '3' */
#define char_4 		0x34	/* '4' */
#define char_5 		0x35	/* '5' */
#define char_6 		0x36	/* '6' */
#define char_7 		0x37	/* '7' */
#define char_9 		0x39	/* '9' */

/*
 * Address conversion macros
 */
#ifndef LANGUAGE_C
#define X_TO_K0(x)	(((x)&0x1FFFFFFF)|K0BASE) /* x to kseg0	 	 */
#define X_TO_K1(x)	(((x)&0x1FFFFFFF)|K1BASE) /* x to kseg1	 	 */
#define X_TO_PHYS(x)	((x)&0x1FFFFFFF) 	  /* x to physical 	 */
#else
#define X_TO_K0(x)	(((unsigned)(x)&0x1FFFFFFF)|K0BASE) /* x to kseg0 */
#define X_TO_K1(x)	(((unsigned)(x)&0x1FFFFFFF)|K1BASE) /* x to kseg1 */
#define X_TO_PHYS(x)	((unsigned)(x)&0x1FFFFFFF)	    /* x to physical */
#endif 

/*
 * Execution Control
 */
#define UNCACHED	0
#define CACHED		1
#define KSEG_MASK	0xE0000000	/* 3 MSB */

#define TESTSZ_MAX	1024*8	/* max code size as default for cache_load() */

#define RUN_ONETEST	0	/* test execution mode */
#define RUN_ALLTESTS	1
#define RUN_ALLMODS	2	/* run all test modules */

/*
 * Default values 
 */
#define ERRLIMIT_DEFAULT 16
#define ERRMODE_DEFAULT  ERR_EXIT	/* exit on error */
#define LOOPLMT_DEFAULT  1
#define C0ERR_DEFAULT	 0x0F		/* ignore all parity err exceptions */

#define N_DFLT		 0xdeadbeef	/* default for numeric input */

/*
 * Display Control
 */
#define COLUMN_WIDTH	40	/* menu column width */


/*
 * Test Table
 */
#define TBL_ENDMARK	0

#ifdef LANGUAGE_C
typedef struct test_tbl {
	char		*test_title;	/* test description string */
	int		(*testptr)();	/* ptr to test routine */
	unsigned	code_size;	/* (bytes) used when icached=CACHED */
	unsigned	icached;	/* UNCACHED or CACHED */
	unsigned	bev_off;	/* set to clear C0_SR_BEV bit */
	unsigned	parm1;		/* parameter passed to test routine */
} TEST_TBL;
#endif 

/*
 * Warning: Always update the following defines when changing
 * the test table structure.
 */
#define TTBLENTRY_SIZE	24		/* bytes */

#define OFFSET_TITLE	0		/*  offset within the record in bytes */
#define OFFSET_TEST	OFFSET_TITLE  +4
#define OFFSET_CODESZ	OFFSET_TEST   +4
#define OFFSET_ICACHED  OFFSET_CODESZ +4
#define OFFSET_BEVOFF	OFFSET_ICACHED+4
#define OFFSET_PARM1	OFFSET_BEVOFF +4


/*
 * Command Table
 */
#ifdef LANGUAGE_C
typedef struct cmd_tbl {
	char		*cmdstr;	/* command string ptr */
	unsigned	cmdlen;		/* number of chars for command lookup */
	int		(*cmdptr)();	/* command handler */
	unsigned	cmd_icached;	/* UNCACHED or CACHED */ 
	unsigned	parm1;		/* parameter passed to command hndlr */
} CMD_TBL;
#endif 

#define CTBLENTRY_SIZE	20

#define OFFSET_CMDSTR		0
#define OFFSET_CMDLEN		OFFSET_CMDSTR	+4
#define OFFSET_CMDPTR		OFFSET_CMDLEN	+4
#define OFFSET_CMDICACHED	OFFSET_CMDPTR	+4
#define OFFSET_CMDPARM1 	OFFSET_CMDICACHED+4


/*
 * NVRAM Based Variables
 *
 * M/6000: 2KB NVRAM addressed in word boundary. Last 8 bytes are TODC. 
 * In SA/prom/prom.h, it defines:
 * 	Byte 0 to 49 for MIPS os use, 50 - 1023 potential use by MIPS
 *	Byte 1024 - 1024+512 for OEM use
 *
 * The diags monitor uses space after the OEM area: 0x0600 - end.
 * TODO: Reserve this space for diags monitor use.
 *
 * The following three lines are defined in mk48t02.h
 *
 *	#define TODC_NVRAM_SIZE         0x7f8   -* NVRAM size on this chip *-
 *	#define TODC_MEM_OFFSET         0x3     -* NVRAM byte offset *-
 *	#define TODC_MEMX(x)            (x<<2)  -* For byte access *-
 *
 * 	TODC_CLOCK_ADDR[] is defined in saio/machaddr.c
 * 	TODC_CLOCK_ADDR_R6300 is defined in cpu_board.h
 */

#ifdef	MIPSEB
#define NVRAM_BASE_BYTE		(NVRAM_BASE + 3)
#endif	/*MIPSEB*/
#ifdef	MIPSEL
#define NVRAM_BASE_BYTE		NVRAM_BASE
#endif	/*MIPSEL*/

#define NVRAM_BYTE0(x)	(NVRAM_BASE_BYTE+((x)<<2)+0)
#define NVRAM_BYTE1(x)	(NVRAM_BASE_BYTE+((x)<<2)+4)
#define NVRAM_BYTE2(x)	(NVRAM_BASE_BYTE+((x)<<2)+8)
#define NVRAM_BYTE3(x)	(NVRAM_BASE_BYTE+((x)<<2)+12)

#define NVWORD		4
#define NVHWORD		2
#define NVBYTE		1

#ifdef	R4230
#define NVDIAG_BASE 	0x0700
#elif defined(RC6380MP)
#define NVDIAG_BASE 	0x0400
#else
#define NVDIAG_BASE 	0x0600
#endif

#ifdef RC6380MP
#define NVDIAG_END	0x05f8
#else
#define NVDIAG_END	0x07f8		/* last 8 bytes are TODC */
#endif /*RC6380MP*/

/*
 * Main memory size in bytes
 */
#define _MEMSIZE	NVDIAG_BASE
#define _MEMSIZE_LEN	4

/*
 * First address for memory tests 
 */
#define _FIRSTADDR	(_MEMSIZE+_MEMSIZE_LEN)
#define _FIRSTADDR_LEN	4

/*
 * Size of memory to be tested by memory tests
 */
#define _LASTADDR	(_FIRSTADDR+_FIRSTADDR_LEN)
#define _LASTADDR_LEN	4 

/*
 * Monitor Flag
 * - A bit field for various hardware states.
 *   A 1 in a bit indicates "yes" or "true", a 0 "no" or "false".
 * - See "pdiag.h" for bit mask definition.
 *
 * Bit 0 - Main memory initialized
 */
#define _DIAG_FLAG	(_LASTADDR +_LASTADDR_LEN)
#define _DIAG_FLAG_LEN	4

/*
 * Sys Config & test status variable: Saved copy of CnfgStatusReg
 * See "pdiag.h" for bit field definition.
 */
#define _CNFGSTATUS	(_DIAG_FLAG+_DIAG_FLAG_LEN)
#define _CNFGSTATUS_LEN	4

/*
 * C0_ERROR Register's I Mask
 */
#define _C0ERRIMASK	(_CNFGSTATUS+_CNFGSTATUS_LEN)
#define _C0ERRIMASK_LEN	4

/*
 * Loop limit: 0 - loop forever
 */
#define _LOOPLMT	(_C0ERRIMASK+_C0ERRIMASK_LEN)
#define _LOOPLMT_LEN	4

/*
 * Current pass count
 */
#define _PASSCNT	(_LOOPLMT+_LOOPLMT_LEN)
#define _PASSCNT_LEN	4

/* 
 * Error limit
 */
#define _ERRORLMT	(_PASSCNT+_PASSCNT_LEN)
#define _ERRORLMT_LEN	4

/* 
 * Total error count 
 */
#define _ERRORCNT	(_ERRORLMT+_ERRORLMT_LEN)
#define _ERRORCNT_LEN	4

/*
 * Current test number
 */
#define _TESTINDX	(_ERRORCNT+_ERRORCNT_LEN)
#define _TESTINDX_LEN	1

/*
 * Current module number in execution of run all modules
 */
#define _MODINDX	(_TESTINDX+_TESTINDX_LEN)
#define _MODINDX_LEN	1

/*
 * Test execution mode: 
 *	0 - run a single test in a module
 *	1 - run the entire test suite in a module
 *	2 - run all test modules
 */
#define _RUNMODE	(_MODINDX+_MODINDX_LEN)
#define _RUNMODE_LEN	1

/*
 * Ptr to current test module name
 */
#define _MODNAME	(_RUNMODE+_RUNMODE_LEN)
#define _MODNAME_LEN	4

/*
 * Ptr to the current test module table 
 */
#define _TTBLPTR	(_MODNAME+_MODNAME_LEN)
#define _TTBLPTR_LEN	4

/*
 * ptr to the command table in use
 */
#define _CMDTBLPTR	(_TTBLPTR+_TTBLPTR_LEN)
#define _CMDTBLPTR_LEN	4

/*
 * nvram stack pointer
 */
#define _NV_SP		(_CMDTBLPTR+_CMDTBLPTR_LEN)
#define _NV_SP_LEN	4

/*
 * console i/o buffer
 *	Command string buffer
 */
#define _NVBUF		(_NV_SP+_NV_SP_LEN)
#define _NVBUF_LEN	50

/*
 * console i/o buffer
 *	Temporary command string buffer
 */
#define _NVBUF2		(_NVBUF+_NVBUF_LEN)
#define _NVBUF2_LEN	50

/*
 * Offset of last byte used
 */
#define NVDIAG_LAST	(_NVBUF2+_NVBUF2_LEN)

#if	defined(IP24) | defined(IP22)
#if ((NVDIAG_END-NVDIAG_LAST) < 0x40)
#include "ERROR -- Non-Volatile RAM overflow (less than 64b for nvram stack)"
#endif 
#else	
#if ((NVDIAG_END-NVDIAG_LAST) < 0x100)
#include "ERROR -- Non-Volatile RAM overflow (less than 256b for nvram stack)"
#endif 
#endif

/*
 * Byte offset of nv-stack-base & nv-stack-end
 */
#define NVSTACK_BASE	(NVDIAG_END-8)
#define NVSTACK_END	(NVDIAG_LAST+4)


/* 
 * NVRAM read/write macros
 */

/* 
 * Load NVRAM value into _reg 
 *
 * x - byte index
 */

#define LB_NVRAM(_reg,x) \
        lbu     _reg,NVRAM_BYTE0(x); \
        move	zero,zero;		/* in case noreorder enabled */

/*
 * Use Big Endian
 *
 * warning: v1 used
 */
#define LH_NVRAM(_reg,x) \
        lbu     _reg,NVRAM_BYTE0(x); \
        lbu     v1,NVRAM_BYTE1(x); \
	sll	_reg,8; \
	or	_reg,v1;
	
/*
 * Use Big Endian
 *
 * warning: v1 used
 */
#define LW_NVRAM(_reg,x) \
        lbu     _reg,NVRAM_BYTE0(x); \
        lbu     v1,NVRAM_BYTE1(x); \
	sll	_reg,8; \
	or	_reg,v1; \
        lbu     v1,NVRAM_BYTE2(x); \
	sll	_reg,8; \
	or	_reg,v1; \
        lbu     v1,NVRAM_BYTE3(x); \
	sll	_reg,8; \
	or	_reg,v1;
	
 
/* 
 * store _reg into NVRAM 
 */

#define SB_NVRAM(_reg,x) \
        sb	_reg,NVRAM_BYTE0(x); \
        move	zero,zero;		/* in case noreorder enabled */

/*
 * Use Big Endian
 */
#define SH_NVRAM(_reg,x) \
   	ror	_reg,8; \
        sb	_reg,NVRAM_BYTE0(x); \
   	rol	_reg,8; \
        sb	_reg,NVRAM_BYTE1(x); \
        move	zero,zero;		/* in case noreorder enabled */

/*
 * Use Big Endian
 */
#define SW_NVRAM(_reg,x) \
   	rol	_reg,8; \
        sb	_reg,NVRAM_BYTE0(x); \
   	rol	_reg,8; \
        sb	_reg,NVRAM_BYTE1(x); \
   	rol	_reg,8; \
        sb	_reg,NVRAM_BYTE2(x); \
   	rol	_reg,8; \
        sb	_reg,NVRAM_BYTE3(x); \
        move	zero,zero;		/* in case noreorder enabled */

/* 
 * set address of NVRAM byte x in _reg
 */

#ifdef NVRAM_ADDR
#include "error - NVRAM_ADDR() macro already defined in other files "
#else

#define NVRAM_ADDR(_reg,x) \
        li      _reg,NVRAM_BYTE0(x);

#endif /* NVRAM_ADDR */


/*
 * PUSH a word on nvram stack
 *
 * registers used:	v1, t0 & t1
 */
#define PUSH_NVRAM(_reg) \
	LW_NVRAM(t0,_NV_SP) \
     	rol     _reg,8; \
        sb      _reg,(t0); \
        rol     _reg,8; \
        sb      _reg,4(t0); \
        rol     _reg,8; \
        sb      _reg,8(t0); \
        rol     _reg,8; \
        sb      _reg,12(t0); \
        move	zero,zero;		/* in case noreorder enabled */ \
	subu	t0,16; \
	NVRAM_ADDR(t1, NVSTACK_END) \
	bgeu	t0,t1,1f; \
        move	zero,zero;		/* in case noreorder enabled */ \
	j	_nvstack_overflow; \
        move	zero,zero;		/* in case noreorder enabled */ \
1:	SW_NVRAM(t0,_NV_SP)
	
/*
 * POP a word from nvram stack
 *
 * registers used:	v1, t0 & t1
 *
 */
#define POP_NVRAM(_reg) \
	LW_NVRAM(t0,_NV_SP) \
	addu	t0,16; \
	NVRAM_ADDR(t1, NVSTACK_BASE) \
	bleu	t0,t1,1f; \
        move	zero,zero;		/* in case noreorder enabled */ \
	j	_nvstack_underflow; \
        move	zero,zero;		/* in case noreorder enabled */ \
1:	lbu     _reg,(t0); \
        lbu     t1,4(t0); \
        sll     _reg,8; \
        or      _reg,t1; \
        lbu     t1,8(t0); \
        sll     _reg,8; \
        or      _reg,t1; \
        lbu     t1,12(t0); \
        sll     _reg,8; \
        or      _reg,t1; \
	SW_NVRAM(t0,_NV_SP)

/*
 * I/O MACROS
 */
#define	PRINT(_msg)		\
	la	a0, 90f;	\
	jal	_puts;		\
	move	zero, zero;	/* in case noreorder enabled */ \
	.data;			\
90:;				\
	.align	2;		\
	.asciiz	_msg;		\
	.text;			\
	.align	2;

#define PUTS(_msg)		\
   	la	a0,_msg;	\
   	jal	_puts;		\
	move	zero, zero;	/* in case noreorder enabled */

#define	PUTHEX(reg)		\
	move	a0,reg;		\
	jal	puthex;		\
	move	zero, zero;	/* in case noreorder enabled */

#define PUTUDEC(_reg)		\
   	move	a0, _reg;	\
   	move 	a1, zero;	\
   	jal	putudec;	\
	move	zero, zero;	/* in case noreorder enabled */

#define PUTDEC(_reg)	 	\
   	move	a0, _reg;	\
   	move 	a1, zero;	\
   	jal	putdec;	 	\
	move	zero, zero;	/* in case noreorder enabled */

/*
 * SET_LED() & SET_LEDS() - Light up the leds
 *
 * Uses registers a0, v0, v1 & ra.
 */
#define SET_LED(CODE)		\
	li	a0, CODE;	\
	jal	set_leds;	\
	move	zero, zero;	/* in case noreorder enabled */

#define  SET_LEDS(_reg)	 	\
	move	a0, _reg;	\
	jal	set_leds;	\
	move	zero, zero;	/* in case noreorder enabled */


/* end */
