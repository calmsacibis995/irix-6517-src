#ident	"$Id: pdiag.h,v 1.1 1994/07/20 22:55:04 davidl Exp $"
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


/*------------------------------------------------------------------------+
| Special register Usage:                                                 |
+------------------------------------------------------------------------*/
#define CnfgStatusReg	k1		/* hw config & test status reg	 */
#define Bevhndlr	fp		/* Bev exception handler vector  */
#define VectorReg	k0		/* User Bev handler vector  	 */
#define ExceptReg	v1		/* Used in default handler	 */


/*------------------------------------------------------------------------+
| System Configuration & Test Status Register.                            |
|   Format:                                                               |
|                                                                         |
|         31 28  27  24 23  20 19  16 15  12 11  08 07 06      00         |
|        +-------------+------+------+------+------+--+----------+        |
|        | IOA0 | IOA1 | MEM0 | MEM1 | MEM2 | MEM3 |CU| STATUS   |        |
|        +-------------+------+------+------+------+--+----------+        |
|                                                                         |
|	 CnfgStatusReg<31..8> 	-- 4-bit slot number for each boad.       |
|        CU			-- FPU usability. Set for usable.	  |
|                                                                         |
|   STATUS bits:                                                          |
|                                                                         |
|           6      5       4     3      2      1       0                  |
|        +------+-------------+------+------+------+----------+           |
|        |      |  ERR_MODE   | ECC  |  ML  |  TA  | ERR_FLAG |           |
|        +------+-------------+------+------+------+----------+           |
|                                                                         |
|        ERR_FLAG	Test error flag: set for error.                   |
|        TA		Set for test active (test in progress).           |
|        ML		Menu Level: 0-main, 1-test module menu.           |
|        ECC		Set to enable memory ECC during test.             |
|        ERR_MODE	On error mode: 0-exit, 1-continue, 2-loop, 3-??.  |
+------------------------------------------------------------------------*/

/*
 * Hardware Configuration Fields
 */
#define	CUMask		0x00000080

/*
 * Test Status Bits 
 */
#define ERRFLAG_MASK	0x00000001
#define TAFLAG_MASK	0x00000002
#define MLFLAG_MASK	0x00000004
#define ECCFLAG_MASK	0x00000008
#define ERRMODE_MASK	0x00000030

#define ERRFLAG_SHIFTCNT 	0
#define TAFLAG_SHIFTCNT		1
#define MLFLAG_SHIFTCNT		2
#define ECCFLAG_SHIFTCNT	3
#define ERRMODE_SHIFTCNT	4

/*
 * ON_ERROR control modes 
 */
#define ERR_EXIT	0x00000000
#define ERR_CONTINUE	0x00000010
#define ERR_LOOP	0x00000020
#define ERR_QUERY	0x00000030
#define ERR_IGNORE	0x00000030

#define LAST_ERRMODE	2		/* last error mode implemented */

/*
 * MONITOR FLAG (Bit field control states)
 *	- A nvram based variable: _DIAG_FLAG
 *	- A 1 in a bit indicates "yes" or "true", a 0 "no" or "false".
 *
 * Bit 0 - Main memory initialized.
 */
#define DBIT_MEMINIT	0x00000001	/* Main memory initialized */


/*
 * Test Data Patterns
 */
#define aces	0xaaaaaaaa
#define ables	aces
#define fives	0x55555555
#define ones	0xffffffff
#define zeros	0x00000000

/*
 * Memory Test Mode Parameter (set in memory test table)
 *
 * Bit 0	- ECC flag: 0-ON, 1-OFF
 * Bit 1	- DATA CACHED Flag: 0-UNCACHED (kseg1), 1-CACHED (kseg0)
 */
#define ECCMODE_MASK	0x1
#define ECC_OFF		0x0
#define ECC_ON		0x1

#define D_MASK		0x2
#define D_UNCACHED	0x0
#define D_CACHED	0x2


/*------------------------------------------------------------------------+
| verbosity level                                                         |
+------------------------------------------------------------------------*/
#define	VER_SILENT	0		/* don't print any messages      */
#define	VER_ERROR	1		/* shows error messages          */
#define	VER_DETAIL	2		/* print detail messages         */
#define	VER_DEFAULT	VER_ERROR	/* default verbose setting       */


/*------------------------------------------------------------------------+
| action flag                                                             |
+------------------------------------------------------------------------*/
#define	HOE		0x010000	/* halt-on-error                 */
#define	SOE		0x020000	/* skip-on-error (test)          */
#define	LOE		0x040000	/* loop-on-error                 */


/*------------------------------------------------------------------------+
| run flag                                                                |
+------------------------------------------------------------------------*/
#define	RNC		0
#define RC		1		# code loaded and run from cache.
#define ECC		2		# Ecc enable
#define	DK0		4


/*------------------------------------------------------------------------+
| Stack modes                                                             |
+------------------------------------------------------------------------*/
#define	MODE_NORMAL	0		/* executing on normal stack     */
#define	MODE_FAULT	1		/* executing on fault stack      */

#define	EXCEPT_NORM	1		/* normal exception              */
#define	EXCEPT_UTLB	2		/* UTLB exception                */
#define	EXCEPT_BRKPT	3		/* break point exception         */

#define	BTPROM_STACK	0xa0010000	/* sys boot prom monitor stack   */

#ifndef PROM_STACK
#define	PROM_STACK	BTPROM_STACK	/* 64Kb - 0x500 for global var    */
#endif

#define	STACK_TOP	PROM_STACK
#define	EXSTKSZ		1024

#define	FAULT_STACK	STACK_TOP	/* exception stack = stack top   */
#define	DIAG_STACK	(FAULT_STACK-1024)

#define	MEM_START	(STACK_TOP + 16)
#define	MemStart	MEM_START
#define BUSERR_PATTERN	0xdeadbeef


#ifndef	LOCORE
typedef struct sys_config {
    unsigned int  brd_type;		/* board type                    */
    unsigned int  brd_cspace;		/* board control space           */
    unsigned int  brd_aspace;		/* boad address space            */
}   SysConfig;
#endif



/*------------------------------------------------------------------------+
| Jal to Prom code from any space.                                        |
+------------------------------------------------------------------------*/
#define	Jal(rtn)			\
	jal	70f;			\
	nop;				\
	b	71f;			\
	nop;				\
70:;					\
	la	v0, rtn;		\
	j	v0;			\
	nop;				\
71:;


#define JalToK0(rtn)			\
	la	ra, 70f;		\
	la	v0, rtn-K0SIZE;		\
	j	v0;			\
	nop;				\
70:;


/*------------------------------------------------------------------------+
| assembly puthex macro:                                                  |
|  ex: PUTHEX(x)                                                          |
+------------------------------------------------------------------------*/

#define	PutHex(reg)			\
	nop;				\
	jal	80f;			\
	nop;				\
	b	81f;			\
	nop;				\
80:;					\
	move	a0, reg;		\
	la	v0, puthex;		\
	j	v0;			\
	nop;				\
81:;


/*------------------------------------------------------------------------+
| assembly print macro:                                                   |
|  print the literal string _text which must be quoted in the call.       |
|  ex: PRINT("Hello world\r\n")                                           |
+------------------------------------------------------------------------*/

#define	Print(_msg)			\
	nop;				\
	jal	90f;			\
	nop;				\
	b	91f;			\
	nop;				\
90:;					\
	la	a0, 92f;		\
	la	v0, puts;		\
	j	v0;			\
	nop;				\
91:;					\
	nop;				\
	.data;				\
92:;					\
	.align	2;			\
	.asciiz	_msg;			\
	.text;				\
	.align	2;


/*------------------------------------------------------------------------+
| Macro for changing code from non-cache to cacheable instruction fetch.  |
+------------------------------------------------------------------------*/
#define	RunCache			\
	la	a0,100f-K0SIZE;	\
	j	a0;			\
	nop;				\
100:;

#define	RunNonCache			\
	la	a0,101f;	\
	j	a0;			\
	nop;				\
101:;


/*
 * Interrupt Handling Macros
 *
 * VectorReg is a register which contains user installed 
 * fault vector. It is always reset to zero on exception.
 */
#define INSTALLVECTOR(_fault_vector) \
	la	Bevhndlr, _bev_handler; \
	la	VectorReg, _fault_vector; 

#define RESETVECTOR() \
	la	Bevhndlr, _bev_handler; \
	move	VectorReg,zero;


#define CRLF \
	PUTS(_crlf);

