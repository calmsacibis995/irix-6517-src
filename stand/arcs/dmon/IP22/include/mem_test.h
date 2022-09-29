#ident	"$Id: mem_test.h,v 1.1 1994/07/20 23:48:52 davidl Exp $"
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


#ifndef LOCORE
#include "sys/types.h"
#endif !LOCORE


/*------------------------------------------------------------------------+
| register usage                                                          |
+------------------------------------------------------------------------*/
#define	RetAdr		s7


/*------------------------------------------------------------------------+
| MISC.                                                                   |
+------------------------------------------------------------------------*/
#define	MemSize		0x02000000
#define	BANK_INC	4
#define	NUM_OF_BANKS	8
#define	LAST_BANK	0x1C
#define	BANK_SIZE	0x20

#define	BLOCK_SIZE	scacheLineSize	/* defined in pdiag.h */
#define LINE_SIZE	scacheLineSize
#define	CACHE_SIZE	scacheSetSize	/* use for hashing funct */
#define	PAGE_SIZE	pageSize

#define	ECC_MASK	0x7F

#define PASS		0
#define FAIL		1


/*------------------------------------------------------------------------+
| failure code.                                                           |
+------------------------------------------------------------------------*/
#define	CONST_ERR	1		/* stuck at one or zero fault    */
#define	AINA_ERR	2		/* addressing fault              */

#define	FILL_ERR	5		/* line fill error.              */
#define	FLUSH_ERR	6		/* line flush error.             */
#define	K0MOD_ERR	7		/* cache data error.             */
#define	K1MOD_ERR	8		/* mem changed from writing to K0*/

#define ECC_ERR		9		/* bad data from ecc array.      */
#define EGEN_ERR	10		/* bad generated ecc.            */
#define SYND_ERR	11		/* bad generated syndrom.        */
#define SBEDET_ERR	12		/* single-bit error detection err*/
#define MBEDET_ERR	13		/* multi-bit  error detection err*/

#define SBC_MEMERR_MASK 0xF             /* least-signif 4 bits           */

#define	CPU_IMASK	(SR_IEC|SR_IBIT3) /* IP2 (sbc interrupt) only 	 */

/*------------------------------------------------------------------------+
| define constant                                                         |
+------------------------------------------------------------------------*/
#define ABLES		0xAAAAAAAA
#define FIVES		0x55555555
#define	END_PAT		0xDEADBEEF


/*------------------------------------------------------------------------+
| CPU INTERRUPT PENDING DEF                                               |
+------------------------------------------------------------------------*/
#define	SR_EXTINT	0x00000400	/* SR external interrupt pending */
#define	SR_FPUINT	0x00000800	/* SR fpu interrupt pending      */
#define	SR_TMRINT	0x00001000	/* SR timer interrupt pending    */


/*------------------------------------------------------------------------+
| define address space                                                    |
+------------------------------------------------------------------------*/
#define	CPU_BRDADDR	0x3		/* do nt respend to any address  */
#define	MEM_BRDADDR	0xAAAAA800	/* respond to 0x002xxxxxxx       */

#define	BASE_ADDR	0xA0000000	/* base address for memory test  */

#define	BRD_SLOT_1	0x00010000	/* board slot 1                  */
#define	BRD_SLOT_2	0x00020000	/* board slot 2                  */
#define	BRD_SLOT_3	0x00030000	/* board slot 3                  */
#define	BRD_SLOT_4	0x00040000	/* board slot 4                  */
#define	BRD_SLOT_5	0x00050000	/* board slot 5                  */
#define	BRD_SLOT_6	0x00060000	/* board slot 6                  */

#define CPU_SLOT_NO	((BRD_SLOT_1) >> 16)
#define MEM_SLOT_NO	((BRD_SLOT_3) >> 16)


/*------------------------------------------------------------------------+
| Define macros                                                           |
+------------------------------------------------------------------------*/
#define MAP_ADDRESS(exec_mode, first_addr, last_addr) \
	sll	first_addr, 3; \
	srl	first_addr, 5; \
	sll	first_addr, 2; 	/* align to word address */ \
	sll	last_addr, 3; \
	srl	last_addr, 5; \
	sll	last_addr, 2; 	/* align to word address */ \
	li	v0, K0BASE; \
	and	v1, exec_mode, D_MASK; \
	beq	v1, D_CACHED, 1f; \
	nop; \
	li	v0, K1BASE; \
1: \
	or	first_addr, v0; \
	or	last_addr, v0;
	

#define	MemIBit		3		/* use bit3 interrupt for ecc error */
#define MemIMask	(1 << MemIBit)	/* cpu's bc interrupt vector mask */

#define EccIntr		((MemIBit << 5)| MemIntEna | EnaSErrInt | EnaMErrDet)
#define	EccOn		(EnaSErrLocCmp | EnaSErrCorrect | EccIntr)

/*------------------------------------------------------------------------+
|                                                                         |
+------------------------------------------------------------------------*/
#define	DisableEcc(mslot)		\
	and	v0, mslot, 0x0f;	\
	sll	v0, 16;			\
	sw	zero, CSR_MEMCTL(v0);

#define	GetCpuSlot(Reg)			\
	lw	Reg, CSR_CTLMISC;	\
	srl	Reg, 24

#define	EnableExternIntr		\
	mfc0	v0, C0_SR;		\
	nop;				\
	or	v0, CPU_IMASK;		\
	mtc0	v0, C0_SR;		\
	nop

#define	EnableEcc(mslot)		\
	/* lw	v0, CSR_CTLMISC; */	\
	sll	v1, mslot, 16;		\
	/* sll	v0, 4; */		\
	/* srl	v0, 28; */		\
	li	v0, 1;	/* presume cpu brd in slot 1 */ \
	sll	v0, 7;			\
	or	v0, EccOn;		\
	sw	v0, CSR_MEMCTL(v1);

#define SetBankMask(mslot)		\
	sll	v1, mslot, 16;		\
	li	v0, 0x80;		\
	sw	v0, CSR_MEMECC(v1);

#define	ClrIVect			\
	not	v0, zero;		\
	sw	v0, (CSR_IVECTCLR)|(1<<16);

#define	SetIVectMask(bmask)		\
	li	v0, bmask;		\
	sw	v0, (CSR_IVECTMASK)|(1<<16);


/*------------------------------------------------------------------------+
| macro to print number of passes.                                        |
+------------------------------------------------------------------------*/
#define	PrintPasses(pmsg, count)		\
	la	a0, pmsg;			\
	jal	puts;				\
	nop;					\
	and	a0, count, 0x7fff;		\
	jal	putdec;				\
	move	a1, zero


/*------------------------------------------------------------------------+
| print memory test's title.                                              |
+------------------------------------------------------------------------*/
#define	PrintTitle(tmsg,tmode,brdmsg,slotreg)	\
	la	a0, tmsg;			\
	jal	puts;				\
	nop;					\
	jal	date;				\
	nop;					\
	la	a0, tmode;			\
	jal	puts;				\
	nop;					\
	la	a0, brdmsg;			\
	jal	puts;				\
	nop;					\
	and	a0, slotreg, 0x0f;		\
	jal	putdec;				\
	li	a1, 2


/*------------------------------------------------------------------------+
| load cache.                                                             |
+------------------------------------------------------------------------*/
#define	CacheLoad(start, end)			\
	la	a0, start;			\
	la	a1, end;			\
	jal	cache_load;			\
	nop


#ifndef LOCORE
/*------------------------------------------------------------------------+
| Define tests tables:                                                    |
+------------------------------------------------------------------------*/
typedef struct test_table {
    int   run_flag;
    int   (*test_rtn)();
    char  *test_desc;
}   TestTable;


typedef struct status {			/* diagnostic's status struct    */
    u_int verb;				/* verbose flag                  */
    u_int act;				/* action flag                   */
    u_int err;				/* error flag                    */
}   DFlag;

#endif !LOCORE
