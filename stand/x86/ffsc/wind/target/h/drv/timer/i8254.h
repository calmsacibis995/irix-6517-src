/* i8254.h - intel 8254 timer header */

/*
modification history
--------------------
01f,22sep92,rrr  added support for c++
01e,26may92,rrr  the tree shuffle
01d,28oct91,wmd  added #pragmas as defined by Intel.
01c,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01b,20apr90,ajm  merged in MIPS BSP version for now, must resolve later.
01a,20aug91,del  installed.
*/

#if CPU_FAMILY==MIPS

#ifndef __INCi8254h
#define __INCi8254h

#ifdef __cplusplus
extern "C" {
#endif

/*

* INTERNAL
* The output of counter2 drives the input to counters 0 and 1.
* This mandates setup of counter2 before counter 0 and 1 can be
* programmed.  Counter 2's input is being driven at 3.6864Mhz by
* the STAR card.

*/

#define PAD 3

#ifndef	_ASMLANGUAGE

typedef struct {
	unsigned char	counter0;unsigned char pad0[PAD];/* counter 0 	*/
	unsigned char	counter1;unsigned char pad1[PAD];/* counter 1 	*/
	unsigned char	counter2;unsigned char pad2[PAD];/* counter 2 	*/
	unsigned char	cntrl_word;unsigned char pad3[PAD];/* control word */
}TIMER;

#endif	/* _ASMLANGUAGE */

/*
* control word definitions
*/

#define	CW_BCDMD	0x1		/* operate in BCD mode		*/
#define	CW_COUNTLCH	0x00		/* counter latch command	*/
#define	CW_LSBYTE	0x10		/* r/w least signif. byte only	*/
#define	CW_MSBYTE	0x20		/* r/w most signif. byte only	*/
#define	CW_BOTHBYTE	0x30		/* r/w 16 bits, lsb then msb	*/
#define	CW_READBK	0xc0		/* read-back command		*/
#define	CW_MODE(x)	((x)<<1)	/* set mode to x		*/
#define	CW_SELECT(x)	((x)<<6)	/* select counter x		*/

/*
* Mode defs
*/

#define	MD_TERMCOUNT	0x0		/* interrupt on terminal count	*/
#define	MD_HWONESHOT	0x1		/* hw retriggerable one shot	*/
#define	MD_RATEGEN	0x2		/* rate generator		*/
#define	MD_SQUAREWV	0x3		/* square wave generator	*/
#define	MD_SWTRIGSB	0x4		/* software triggered strobe	*/
#define	MD_HWTRIGSB	0x5		/* hardware triggered strobe	*/

#endif	/* INCi8254h */

#else	/* CPU_FAMILY==MIPS */


/******************************************************************/
/* 		Copyright (c) 1989, Intel Corporation

   Intel hereby grants you permission to copy, modify, and
   distribute this software and its documentation.  Intel grants
   this permission provided that the above copyright notice
   appears in all copies and that both the copyright notice and
   this permission notice appear in supporting documentation.  In
   addition, Intel grants this permission provided that you
   prominently mark as not part of the original any modifications
   made to this software or documentation, and that the name of
   Intel Corporation not be used in advertising or publicity
   pertaining to distribution of the software or the documentation
   without specific, written prior permission.

   Intel Corporation does not warrant, guarantee or make any
   representations regarding the use of, or the results of the use
   of, the software and documentation in terms of correctness,
   accuracy, reliability, currentness, or otherwise; and you rely
   on the software, documentation and results solely at your own
   risk.							  */
/******************************************************************/
/*-------------------------------------------------------------*/
/*
 * i8254.h header for 82c54-2 timer/counter
 *
 */
/*-------------------------------------------------------------*/

#if CPU_FAMILY == I960
#pragma align 1
#endif

typedef volatile struct
    {
    unsigned char counter_0;
    unsigned char counter_1;
    unsigned char counter_2;
    unsigned char control_word;
    } I82C54;

#if CPU_FAMILY == I960
#pragma align 0
#endif

/* Control Word Format */
#define SC(sc)		((sc)<<6)
#define RW(rw)		((rw)<<4)
#define MODE(m)		((m)<<1)
#define BCD		(1)

/* Control Words */

#define SEL_CNT_0	0x00
#define SEL_CNT_1	0x40
#define SEL_CNT_2	0x80
#define READ_BACK	0xc0
#define READ_BACK_0	0xc2
#define READ_BACK_1	0xc4
#define READ_BACK_2	0xc8

#define RW_LATCH	0x00
#define RW_LO_BYTE	0x10
#define RW_HI_BYTE	0x20
#define RW_HI_LO_BYTES	0x30

#define WRT_CNT_0_LO	(SEL_CNT_0 | RW_LO_BYTE)
#define WRT_CNT_1_LO	(SEL_CNT_1 | RW_LO_BYTE)
#define WRT_CNT_2_LO	(SEL_CNT_2 | RW_LO_BYTE)
#define WRT_CNT_0_HI	(SEL_CNT_0 | RW_HI_BYTE)
#define WRT_CNT_1_HI	(SEL_CNT_1 | RW_HI_BYTE)
#define WRT_CNT_2_HI	(SEL_CNT_2 | RW_HI_BYTE)
#define WRT_CNT_0_HI_LO	(SEL_CNT_0 | RW_HI_LO_BYTES)
#define WRT_CNT_1_HI_LO	(SEL_CNT_1 | RW_HI_LO_BYTES)
#define WRT_CNT_2_HI_LO	(SEL_CNT_2 | RW_HI_LO_BYTES)

/* Readback Commands */
#define READBACK     (3<<6)
#define LATCH_COUNT  (1<<4)
#define LATCH_STATUS (1<<5)
#define CNT_0        (1<<1)
#define CNT_1        (1<<2)
#define CNT_2        (1<<3)

/* Status Bits */
#define OUTPUT_BIT      (1<<7)
#define NULL_COUNT_BIT  (1<<6)
#define RW1_BIT         (1<<5)
#define RW0_BIT         (1<<4)
#define M2_BIT          (1<<3)
#define M1_BIT          (1<<2)
#define M0_BIT          (1<<1)
#define BCD_BIT         (1<<0)

#ifdef __cplusplus
}
#endif

#endif /* __INCi8254h */
