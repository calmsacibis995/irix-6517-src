/* fga002.h - Force Computers FGA-002 gate array */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01i,22sep92,rrr  added support for c++
01h,06aug92,ccc  added DMA definitions.
01g,13jul92,caf  added function declarations.
01f,26may92,rrr  the tree shuffle
01e,04oct91,rrr  passed through the ansification filter
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01d,24oct90,yao  added missing register definitions.allow for ASM & reg offset.
01c,05oct90,shl  added copyright notice.
		 made #endif ANSI style.
01b,06sep89,jcf  fixed CTRL15 definitions.
01a,14feb89,jcf  written.
*/

#ifndef __INCfga002h
#define __INCfga002h

#ifdef __cplusplus
extern "C" {
#endif

#ifdef 	_ASMLANGUAGE
#define	CAST
#define	CAST_INT
#else
#define	CAST	(char *)
#define	CAST_INT	(int *)
#endif	/* _ASMLANGUAGE */

#define	FGA_ADRS(reg)	(CAST (FGA_BASE_ADRS + (reg * FGA_REG_INTERVAL)))
#define	FGA_ADRS_INT(reg) (CAST_INT (FGA_BASE_ADRS + (reg * FGA_REG_INTERVAL)))

/* register definitions */

    /* interrupt control registers */
#define	FGA_ICRMBOX0	FGA_ADRS(0x000)  	/* mailbox 0 */
#define	FGA_ICRMBOX1	FGA_ADRS(0x004)  	/* mailbox 1 */
#define	FGA_ICRMBOX2	FGA_ADRS(0x008)  	/* mailbox 2 */
#define	FGA_ICRMBOX3	FGA_ADRS(0x00C)  	/* mailbox 3 */
#define	FGA_ICRMBOX4	FGA_ADRS(0x010)  	/* mailbox 4 */
#define	FGA_ICRMBOX5	FGA_ADRS(0x014)  	/* mailbox 5 */
#define	FGA_ICRMBOX6	FGA_ADRS(0x018)  	/* mailbox 6 */
#define	FGA_ICRMBOX7	FGA_ADRS(0x01C)  	/* mailbox 7 */

#define	FGA_ICRTIM0	FGA_ADRS(0x220)  	/* timer */

#define	FGA_ICRFMB0REF	FGA_ADRS(0x240)  	/* FMB0 refused */
#define	FGA_ICRFMB1REF	FGA_ADRS(0x244)  	/* FMB1 refused */
#define	FGA_ICRFMB0MES	FGA_ADRS(0x248)  	/* FMB0 message */
#define	FGA_ICRFMB1MES	FGA_ADRS(0x24C)  	/* FMB1 message */

#define	FGA_ICRDMANORM	FGA_ADRS(0x230)  	/* DMA normal */
#define	FGA_ICRDMAERR	FGA_ADRS(0x234)  	/* DMA error */

#define	FGA_ICRPARITY	FGA_ADRS(0x258)  	/* parity */

#define	FGA_ICRVME1	FGA_ADRS(0x204)  	/* VMEIRQ 1 */
#define	FGA_ICRVME2	FGA_ADRS(0x208)  	/* VMEIRQ 2 */
#define	FGA_ICRVME3	FGA_ADRS(0x20C)  	/* VMEIRQ 3 */
#define	FGA_ICRVME4	FGA_ADRS(0x210)  	/* VMEIRQ 4 */
#define	FGA_ICRVME5	FGA_ADRS(0x214)  	/* VMEIRQ 5 */
#define	FGA_ICRVME6	FGA_ADRS(0x218)  	/* VMEIRQ 6 */
#define	FGA_ICRVME7	FGA_ADRS(0x21C)  	/* VMEIRQ 7 */

    /* extended interrupt control registers */
#define	FGA_ICRABORT	FGA_ADRS(0x280)  	/* abort */
#define	FGA_ICRACFAIL	FGA_ADRS(0x284)  	/* acfail */
#define	FGA_ICRSYSFAIL	FGA_ADRS(0x288)  	/* sysfail */

#define	FGA_ICRLOCAL0	FGA_ADRS(0x28C)  	/* local 0 */
#define	FGA_ICRLOCAL1	FGA_ADRS(0x290)  	/* local 1 */
#define	FGA_ICRLOCAL2	FGA_ADRS(0x294)  	/* local 2 */
#define	FGA_ICRLOCAL3	FGA_ADRS(0x298)  	/* local 3 */
#define	FGA_ICRLOCAL4	FGA_ADRS(0x29C)  	/* local 4 */
#define	FGA_ICRLOCAL5	FGA_ADRS(0x2A0)  	/* local 5 */
#define	FGA_ICRLOCAL6	FGA_ADRS(0x2A4)  	/* local 6 */
#define	FGA_ICRLOCAL7	FGA_ADRS(0x2A8)  	/* local 7 */


    /* control registers */
#define	FGA_CTL1	FGA_ADRS(0x238)  	/* arbitration */
#define	FGA_CTL2	FGA_ADRS(0x23C)  	/* parity */
#define	FGA_CTL3 	FGA_ADRS(0x250)  	/* bus width,vec */
#define FGA_CTL4	FGA_ADRS(0x254)  	/* boot decode,ack */
#define	FGA_CTL5	FGA_ADRS(0x264)  	/* mbox VME acc */
#define FGA_CTL6	FGA_ADRS(0x270)  	/* local DSACK */
#define	FGA_CTL7	FGA_ADRS(0x274)  	/* VME timeout */
#define	FGA_CTL8	FGA_ADRS(0x278)  	/* arb, acfail */
#define	FGA_CTL9	FGA_ADRS(0x27C)  	/* sysReset */
#define FGA_CTL10	FGA_ADRS(0x2C0)  	/* memory size */
#define FGA_CTL11	FGA_ADRS(0x2C4)  	/* memory decode */
#define FGA_CTL12	FGA_ADRS(0x32C)  	/* VME release */
#define FGA_CTL13	FGA_ADRS(0x350)  	/* TEST ONLY!!! */
#define FGA_CTL14	FGA_ADRS(0x354) 	/* EPROM access */
#define	FGA_CTL15	FGA_ADRS(0x358) 	/* slave rmw */
#define FGA_CTL16	FGA_ADRS(0x35C) 	/* bus unaligned rmw */
						/* timeout,parity    */

#define	FGA_LOCALIACK	FGA_ADRS(0x334)		/* IACK for L5-L8 */

#define	FGA_ENAMCODE	FGA_ADRS(0x2B4)		/* dual port RAM */

#define	FGA_MAINUM	FGA_ADRS(0x2C8)		/* A23-A16 LOCAL */
#define	FGA_MAINUU	FGA_ADRS(0x2CC)		/* A31-A24 LOCAL */

#define	FGA_VMEPAGE	FGA_ADRS(0x200)		/* A31-A28 VME */
#define	FGA_BOTTOMPAGEU	FGA_ADRS(0x2D0)		/* A27-A20 VME */
#define	FGA_BOTTOMPAGEL	FGA_ADRS(0x2D4)		/* A19-A12 VME */
#define	FGA_TOPPAGEU	FGA_ADRS(0x2D8)		/* A27-A20 VME */
#define	FGA_TOPPAGEL	FGA_ADRS(0x2DC)		/* A19-A12 VME */

#define	FGA_MYVMEPAGE	FGA_ADRS(0x2FC)		/* A15-A8 VME */

#define	FGA_FMBCTL	FGA_ADRS(0x338)		/* FMB control */
#define	FGA_FMBAREA	FGA_ADRS(0x33C)		/* A31-A24 VME */

#define	FGA_ISABORT	FGA_ADRS(0x4C8)		/* abort status */
#define	FGA_ISACFAIL	FGA_ADRS(0x4CC)		/* acfail status */
#define	FGA_ISSYSFAIL	FGA_ADRS(0x4D0)		/* sysfail status */

#define FGA_MBOX0	FGA_ADRS(0x80000)	/* Mailbox 0 */
#define FGA_MBOX1	FGA_ADRS(0x80004)  	/* Mailbox 1 */
#define FGA_MBOX2	FGA_ADRS(0x80008)  	/* Mailbox 2 */
#define FGA_MBOX3	FGA_ADRS(0x8000C)  	/* Mailbox 3 */
#define FGA_MBOX4	FGA_ADRS(0x80010)  	/* Mailbox 4 */
#define FGA_MBOX5	FGA_ADRS(0x80014)  	/* Mailbox 5 */
#define FGA_MBOX6	FGA_ADRS(0x80018)  	/* Mailbox 6 */
#define FGA_MBOX7	FGA_ADRS(0x8001C)  	/* Mailbox 7 */

    /* auxiliary registers */
#define FGA_AUXPINCTL	FGA_ADRS(0x260) 	/* control */
#define FGA_AUXFIFWEX	FGA_ADRS(0x268) 	/* fifo write time */
#define FGA_AUXFIFREX	FGA_ADRS(0x26C) 	/* fifo read time */

#define FGA_AUXSRCSTART FGA_ADRS(0x340) 	/* src start */
#define FGA_AUXDSTSTART FGA_ADRS(0x344) 	/* dst start */
#define FGA_AUXSRCTERM  FGA_ADRS(0x348) 	/* src termination */
#define FGA_AUXDSTTERM  FGA_ADRS(0x34C) 	/* dst termination */

    /* timer register */
#define FGA_TIM0PRELOAD FGA_ADRS(0x300) 	/* time preload */
#define FGA_TIM0CTL	FGA_ADRS(0x310) 	/* time control */
#define FGA_TIM0COUNT	FGA_ADRS(0xC00) 	/* timer0 counter */

    /* DMA registers */
#define FGA_DMASRCATT	FGA_ADRS(0x320) 	/* DMA src attr */
#define FGA_DMADSTATT	FGA_ADRS(0x324) 	/* DMA dst attr */
#define FGA_DMAGENERAL	FGA_ADRS(0x328) 	/* DMA general */
#define FGA_DMASRCDST	FGA_ADRS(0x4EC) 	/* DMA mode */
#define FGA_DMASRCADR	FGA_ADRS_INT(0x500) 	/* DMA src addr */
#define FGA_DMADSTADR	FGA_ADRS_INT(0x504) 	/* DMA dst addr */
#define FGA_DMATRFCNT	FGA_ADRS_INT(0x508) 	/* DMA trans count */

    /* interrupt status register */
#define FGA_ISLOCAL0	FGA_ADRS(0x480)
#define FGA_ISLOCAL1	FGA_ADRS(0x484)
#define FGA_ISLOCAL2	FGA_ADRS(0x488)
#define FGA_ISLOCAL3	FGA_ADRS(0x48C)
#define FGA_ISLOCAL4	FGA_ADRS(0x490)
#define FGA_ISLOCAL5	FGA_ADRS(0x494)
#define FGA_ISLOCAL6	FGA_ADRS(0x498)
#define FGA_ISLOCAL7	FGA_ADRS(0x49C)

#define FGA_ISTIM0	FGA_ADRS(0x4A0) 	/* timer0 */
#define FGA_ISDMANORM	FGA_ADRS(0x4B0) 	/* DMA termination */
#define FGA_ISDMAERR	FGA_ADRS(0x4B4) 	/* DMA error */

#define FGA_ISFB0REF	FGA_ADRS(0x4B8) 	/* FMB0 refused */
#define FGA_ISFB1REF	FGA_ADRS(0x4BC) 	/* FMB1 refused */
#define FGA_ISFB0MES	FGA_ADRS(0x4E0) 	/* FMB0 message */
#define FGA_ISFB1MES	FGA_ADRS(0x4E4) 	/* FMB1 message */

#define FGA_ISPARITY	FGA_ADRS(0x4C0)

#define FGA_DMARUNCTL	FGA_ADRS(0x4C4)
#define FGA_ABORTPIN	FGA_ADRS(0x4D4)
#define FGA_ACFAILPIN	FGA_ADRS(0x4D8)
#define FGA_SFAILPIN	FGA_ADRS(0x4DC)

    /* reset status register */
#define FGA_RSVMECALL	FGA_ADRS(0x4F0) 	/* VME */
#define FGA_RSKEYRES	FGA_ADRS(0x4F4) 	/* KEY */
#define FGA_RSCPUCALL	FGA_ADRS(0x4F8) 	/* CPU */
#define FGA_RSLOCSW	FGA_ADRS(0x4FC) 	/* local switch */
#define FGA_SOFTRESCALL FGA_ADRS(0xE00) 	/* software */

    /* miscellanous registers */
#define FGA_LIOTIMING	FGA_ADRS(0x330) 	/* local IO timing */

#define FGA_PTYLL	FGA_ADRS(0x400) 	/* PERR address */
#define FGA_PTYLM	FGA_ADRS(0x404) 	/* PERR address */
#define FGA_PTYUM	FGA_ADRS(0x408) 	/* PERR address */
#define FGA_PTYUU	FGA_ADRS(0x40C) 	/* PERR address */
#define FGA_PTYATT	FGA_ADRS(0x410) 	/* access status */
#define FGA_IDENT	FGA_ADRS(0x41C) 	/* rev/ident reg */

#define FGA_SPECIAL	FGA_ADRS(0x420) 	/* SPECIAL reg */
#define FGA_SPECIALENA  FGA_ADRS(0x424) 	/* SPECIAL enable */

#define FGA_FMBCH0	FGA_ADRS(0xC0000) 	/* FMB0 channel */
#define FGA_FMBCH1	FGA_ADRS(0xC0004) 	/* FMB1 channel*/

/* Interrupt Vector Offsets */

#define FGA_INT_MBOX0		0x00
#define FGA_INT_MBOX1		0x01
#define FGA_INT_MBOX2		0x02
#define FGA_INT_MBOX3		0x03
#define FGA_INT_MBOX4		0x04
#define FGA_INT_MBOX5		0x05
#define FGA_INT_MBOX6		0x06
#define FGA_INT_MBOX7		0x07
#define FGA_INT_TIMER		0x20
#define FGA_INT_FMB1REF		0x24
#define FGA_INT_FMB0REF		0x25
#define FGA_INT_FMB1		0x26
#define FGA_INT_FMB0		0x27
#define FGA_INT_ABORT		0x28
#define FGA_INT_ACFAIL		0x29
#define FGA_INT_SYSFAIL		0x2a
#define FGA_INT_DMAERR		0x2b
#define FGA_INT_DMANORM		0x2c
#define FGA_INT_PARITY		0x2d
#define FGA_INT_LOCAL0		0x30
#define FGA_INT_LOCAL1		0x31
#define FGA_INT_LOCAL2		0x32
#define FGA_INT_LOCAL3		0x33
#define FGA_INT_LOCAL4		0x34		/* or external */
#define FGA_INT_LOCAL5		0x35		/* or external */
#define FGA_INT_LOCAL6		0x36		/* or external */
#define FGA_INT_LOCAL7		0x37		/* or external */


/* Register Masks */

    /* ICR */
#define	FGA_ICR_ENABLE		0x8	/* enable interrupt */
#define	FGA_ICR_LEVEL_MASK	0x7	/* interrupt level mask */

    /* following defines are only for extended interrupt control registers */
#define	FGA_ICR_AUTOCLEAR	0x10	/* autoclear interrupt */
#define	FGA_ICR_ACTIVITY	0x20	/* active high = 1 */
#define	FGA_ICR_EDGE		0x40	/* 1 = edge sensitive */

    /* CTL1 */
#define	FGA_CTL1_COPROC_ASYNC	0x0	/* asynchronous */
#define	FGA_CTL1_COPROC_0WAIT	0x1	/* zero wait states */
#define	FGA_CTL1_COPROC_1WAIT	0x2	/* one wait states */
#define	FGA_CTL1_COPROC_2WAIT	0x3	/* two wait states */
#define	FGA_CTL1_SCON		0x4	/* internal arbiter select */
#define	FGA_CTL1_SUPERVISOR	0x8	/* supervisor access only */

    /* CTL2 */
#define	FGA_CTL2_BYTESTROBE	0x1	/* all bytestrobe lines asserted on R */
#define	FGA_CTL2_CSDROPTION	0x2	/* CSDRP active in R/W cycles */
#define	FGA_CTL2_VMERESCALLENA	0x4	/* VME ResetCall enable */
#define	FGA_CTL2_PARITYOUTENA	0x8	/* parity enable */

    /* CTL3 */
#define	FGA_CTL3_VMEBUS16	0x1	/* VME bus 16 bit wide */
#define	FGA_CTL3_VSBENA		0x2	/* VSB bus decoding enable */
#define	FGA_CTL3_VEC_MASK	0xc	/* bit 7,6 of interrupt vec number */

    /* CTL5 */
#define	FGA_CTL5_AUXOPTIONA	0x1	/* AUXOPTIONA enabled */
#define	FGA_CTL5_AUXOPTIONB	0x2	/* AUXOPTIONB enabled */
#define	FGA_CTL5_VME_A16_USER	0x4	/* VME access for mailbox int. */
#define	FGA_CTL5_VME_A16_SUP	0x8	/* VME access for mailbox int. */
#define	FGA_CTL5_VME_A16_BOTH	0xc	/* VME access for mailbox int. */

    /* CTL7 */
#define	FGA_CTL7_RAT_1US	0x1	/* VME release timeout */
#define	FGA_CTL7_RAT_2US	0x2	/* VME release timeout */
#define	FGA_CTL7_RAT_4US	0x3	/* VME release timeout */
#define	FGA_CTL7_RAT_8US	0x4	/* VME release timeout */
#define	FGA_CTL7_RAT_16US	0x5	/* VME release timeout */
#define	FGA_CTL7_RAT_32US	0x6	/* VME release timeout */
#define	FGA_CTL7_RAT_64US	0x7	/* VME release timeout */
#define	FGA_CTL7_NO_RELEASE_ON_BCLR 0x8	/* don't release bus on BCLR* */

    /* CTL 8 */
#define	FGA_CTL8_ACFAIL		0x1	/* ACFAIL handler */
#define	FGA_CTL8_FAIR_ARB	0x2	/* VME bus fair arbitration */
#define	FGA_CTL8_SOFTSYSFAILINIT 0x4	/* Asserts SYSFLTOVME-pin to 0 */
#define	FGA_CTL8_BOOTSYSFAILINIT 0x8	/* INACTIVE */

    /* CTL 9 */
#define	FGA_CTL9_0WAIT		0x1	/* EPROM wait states */
#define	FGA_CTL9_1WAIT		0x2	/* EPROM wait states */
#define	FGA_CTL9_2WAIT		0x3	/* EPROM wait states */
#define	FGA_CTL9_3WAIT		0x4	/* EPROM wait states */
#define	FGA_CTL9_4WAIT		0x5	/* EPROM wait states */
#define	FGA_CTL9_5WAIT		0x6	/* EPROM wait states */
#define	FGA_CTL9_6WAIT		0x7	/* EPROM wait states */
#define	FGA_CTL9_RESET_BUS	0x8	/* SYSRESET propagates to VMEBUS */

    /* CTL 15 */
#define	FGA_CTL15_SHAREDRMW	0x1	/* slave RMW cycles are supported */
#define	FGA_CTL15_CINHLIO	0x2	/* do cache 0xff800000 - 0xfff00000 */
#define	FGA_CTL15_CINH16	0x4	/* do cache 0xfcxxxxxx, 0xfexxxxxx */
#define	FGA_CTL15_CINHOFFBOARD	0x8	/* do cache off board */
#define	FGA_CTL15_BURST_1WAIT	0x10	/* one waitstate burst cycles */
#define	FGA_CTL15_BURST_TWO	0x20	/* two transfers per burst */
#define	FGA_CTL15_VSB_TOUT_DIS	0x60	/* VSB bus error timeout disabled */
#define	FGA_CTL15_VSB_TOUT_16	0x80	/* VSB bus error timeout : 16 us */
#define	FGA_CTL15_VSB_TOUT_1000	0x40	/* VSB bus error timeout : 1000 us */
#define	FGA_CTL15_VSB_TOUT_64000 0x00	/* VSB bus error timeout : 64000 us */

    /* LOCALIACK */
#define FGA_LOCALIACK_INTERNAL	0x00	/* FGA responds with internal vector */
#define FGA_LOCALIACK_NONE	0x01	/* FGA ignores interrupt */
#define FGA_LOCALIACK_EXT_1US	0x02	/* FGA passes ext. vec., 1us response */
#define FGA_LOCALIACK_EXT_500NS	0x03	/* FGA passes ext. vec.,.5us response */
#define FGA_LOCALIACK_LOCAL5	0x03	/* LOCAL 5 MASK */
#define FGA_LOCALIACK_LOCAL6	0x0c	/* LOCAL 6 MASK */
#define FGA_LOCALIACK_LOCAL7	0x30	/* LOCAL 7 MASK */
#define FGA_LOCALIACK_LOCAL8	0xc0	/* LOCAL 8 MASK */

    /* DMA Source/Destination Attribute Register */
#define	FGA_DMA_MAIN_MEMORY	0xc0	/* MAIN MEMORY		32 bit */
#define	FGA_DMA_SECOND_BUS_32	0xe0	/* Secondary Bus	32 bit */
#define	FGA_DMA_SECOND_BUS_16	0xe8	/* Secondary Bus	16 bit */
#define	FGA_DMA_SECOND_BUS_8	0xf0	/* Secondary Bus	 8 bit */
#define	FGA_DMA_VMEBUS_32	0x00	/* VMEbus		32 bit */
#define	FGA_DMA_VMEBUS_16	0x80	/* VMEbus		16 bit */
#define	FGA_DMA_VMEBUS_8	0x40	/* VMEbus		 8 bit */
#define	FGA_DMA_AUX_BUS		0xc8	/* AUX bus		 8 bit */

    /* DMA General Control Register */
#define	FGA_DMA_SRC_NO_COUNT	0x80	/* Source register does not count */
#define	FGA_DMA_SRC_COUNTS	0x00	/* Source register counts up */
#define	FGA_DMA_DST_NO_COUNT	0x40	/* Destination reg does not count */
#define	FGA_DMA_DST_COUNTS	0x00	/* Destination register counts up */
#define	FGA_DMA_ENABLE		0x01	/* enable DMA controller */

    /* DMA Run Control Register */
#define	FGA_DMA_START		0x01	/* start the DMA operation */
#define	FGA_DMA_STOP		0x00	/* stop the DMA operation */

    /* AUX Source/Destination Start Register */
#define	FGA_ASSACK_1CLK		0x00	/* AUXACK pin asserted... 1 clock */
#define	FGA_ASSACK_2CLK		0x10	/* 		 2 clockcycles */
#define	FGA_ASSACK_3CLK		0x20	/*		 3 clockcycles */
#define	FGA_ASSACK_4CLK		0x30	/*		 4 clockcycles */
#define	FGA_ASSACK_5CLK		0x40	/*		 5 clockcycles */
#define	FGA_ASSACK_6CLK		0x50	/*		 6 clockcycles */
#define	FGA_ASSACK_7CLK		0x60	/*		 7 clockcycles */
#define	FGA_ASSACK_8CLK		0x70	/*		 8 clockcycles */
#define	FGA_ASSACK_9CLK		0x80	/*		 9 clockcycles */
#define	FGA_ASSACK_10CLK	0x90	/*		10 clockcycles */
#define	FGA_ASSACK_11CLK	0xa0	/*		11 clockcycles */
#define	FGA_ASSACK_12CLK	0xb0	/*		12 clockcycles */
#define	FGA_ASSACK_AUXREQ	0xc0	/*		AUXREQ pin asserted */
#define	FGA_ASSACK_NORDY	0xd0	/* AUXREQ asserted, AUXRDY released */
#define	FGA_ASSACK_DATARD	0xe0	/* data read into FIFO */
#define	FGA_ASSACK_AUXRDY	0xf0	/* AUXRDY pin asserted */

#define FGA_RDY_1CLK		0x00	/* READY after... 1 clockcycle */
#define FGA_RDY_2CLK		0x01	/*		 2 clockcycles */
#define FGA_RDY_3CLK		0x02	/*		 3 clockcycles */
#define FGA_RDY_4CLK		0x03	/*		 4 clockcycles */
#define FGA_RDY_5CLK		0x04	/*		 5 clockcycles */
#define FGA_RDY_6CLK		0x05	/*		 6 clockcycles */
#define FGA_RDY_7CLK		0x06	/*		 7 clockcycles */
#define FGA_RDY_8CLK		0x07	/*		 8 clockcycles */
#define FGA_RDY_9CLK		0x08	/*		 9 clockcycles */
#define FGA_RDY_10CLK		0x09	/*		10 clockcycles */
#define FGA_RDY_11CLK		0x0a	/*		11 clockcycles */
#define FGA_RDY_12CLK		0x0b	/*		12 clockcycles */
#define FGA_RDY_AUXREQ		0x0c	/*		AUXREQ pin asserted */
#define FGA_RDY_NORDY		0x0d	/* AUXREQ asserted, AUXRDY released */
#define FGA_RDY_DATARD		0x0e	/* data read into FIFO */
#define FGA_RDY_AUXRDY		0x0f	/* AUXRDY pin asserted */

    /* AUX Source/Destination Term Register */
#define FGA_RELACK_1CLK		0x00	/* AUXACK pin released... 1 clock */
#define FGA_RELACK_2CLK		0x10	/* (after READY) 2 clockcycles */
#define FGA_RELACK_3CLK		0x20	/*		 3 clockcycles */
#define FGA_RELACK_4CLK		0x30	/*		 4 clockcycles */
#define FGA_RELACK_5CLK		0x40	/*		 5 clockcycles */
#define FGA_RELACK_6CLK		0x50	/*		 6 clockcycles */
#define FGA_RELACK_7CLK		0x60	/*		 7 clockcycles */
#define FGA_RELACK_8CLK		0x70	/*		 8 clockcycles */
#define FGA_RELACK_9CLK		0x80	/*		 9 clockcycles */
#define FGA_RELACK_10CLK	0x90	/*		10 clockcycles */
#define FGA_RELACK_11CLK	0xa0	/*		11 clockcycles */
#define FGA_RELACK_12CLK	0xb0	/*		12 clockcycles */
#define FGA_RELACK_AUXRDY	0xc0	/*		AUXRDY pin asserted */
#define FGA_RELACK_NOTALLOWED	0xd0	/* 		DO NOT USE */
#define FGA_RELACK_DATARD	0xe0	/* 		data read into FIFO */
#define FGA_RELACK_VALIDRDY	0xf0	/* 		valid READY */

#define FGA_NEWCYC_1CLK		0x00	/* NEWCYCLE starts... 1 clockcycle */
#define FGA_NEWCYC_2CLK		0x01	/* (after READY) 2 clockcycles */
#define FGA_NEWCYC_3CLK		0x02	/*		 3 clockcycles */
#define FGA_NEWCYC_4CLK		0x03	/*		 4 clockcycles */
#define FGA_NEWCYC_5CLK		0x04	/*		 5 clockcycles */
#define FGA_NEWCYC_6CLK		0x05	/*		 6 clockcycles */
#define FGA_NEWCYC_7CLK		0x06	/*		 7 clockcycles */
#define FGA_NEWCYC_8CLK		0x07	/*		 8 clockcycles */
#define FGA_NEWCYC_9CLK		0x08	/*		 9 clockcycles */
#define FGA_NEWCYC_10CLK	0x09	/*		10 clockcycles */
#define FGA_NEWCYC_11CLK	0x0a	/*		11 clockcycles */
#define FGA_NEWCYC_12CLK	0x0b	/*		12 clockcycles */
#define FGA_NEWCYC_AUXRDY	0x0c	/*		AUXRDY pin asserted */
#define FGA_NEWCYC_AUXACK	0x0d	/* 		AUXACK pin asserted */
#define FGA_NEWCYC_DATARD	0x0e	/* 		data read into FIFO */
#define FGA_NEWCYC_VALIDRDY	0x0f	/* 		valid READY */

    /* AUX Pin Control */
#define	FGA_AUXPIN_AUTOREQ	0x08	/* autorequest enabled */
#define	FGA_AUXPIN_NOAUTOREQ	0x00	/* autorequest disabled */
#define	FGA_AUXPIN_RDY_HIGH	0x04	/* AUXRDY active high */
#define	FGA_AUXPIN_RDY_LOW	0x00	/* AUXRDY active low */
#define	FGA_AUXPIN_ACK_HIGH	0x02	/* AUXACK active high */
#define	FGA_AUXPIN_ACK_LOW	0x00	/* AUXACK active low */
#define	FGA_AUXPIN_REQ_HIGH	0x01	/* AUXREQ active high */
#define	FGA_AUXPIN_REQ_LOW	0x00	/* AUXREQ active low */

    /* AUX FIFO Write/Read Timing Register */
#define	FGA_AUXFIFO_TIMING0	0x00	/* AUXFIFO Wr/Rd Timing 0 */
#define FGA_AUXFIFO_TIMING1	0x01	/* AUXFIFO Wr/Rd Timing 1 */
#define FGA_AUXFIFO_TIMING2	0x02	/* AUXFIFO Wr/Rd Timing 2 */
#define FGA_AUXFIFO_TIMING3	0x03	/* AUXFIFO Wr/Rd Timing 3 */
#define FGA_AUXFIFO_TIMING4	0x04	/* AUXFIFO Wr/Rd Timing 4 */
#define FGA_AUXFIFO_TIMING5	0x05	/* AUXFIFO Wr/Rd Timing 5 */
#define FGA_AUXFIFO_TIMING6	0x06	/* AUXFIFO Wr/Rd Timing 6 */
#define FGA_AUXFIFO_TIMING7	0x07	/* AUXFIFO Wr/Rd Timing 7 */
#define FGA_AUXFIFO_TIMING8	0x08	/* AUXFIFO Wr/Rd Timing 8 */
#define FGA_AUXFIFO_TIMING9	0x09	/* AUXFIFO Wr/Rd Timing 9 */
#define FGA_AUXFIFO_TIMING10	0x0a	/* AUXFIFO Wr/Rd Timing 10 */
#define FGA_AUXFIFO_TIMING11	0x0b	/* AUXFIFO Wr/Rd Timing 11 */
#define FGA_AUXFIFO_TIMING12	0x0c	/* AUXFIFO Wr/Rd Timing 12 */
#define FGA_AUXFIFO_TIMING13	0x0d	/* AUXFIFO Wr/Rd Timing 13 */
#define FGA_AUXFIFO_TIMING14	0x0e	/* AUXFIFO Wr/Rd Timing 14 */
#define FGA_AUXFIFO_TIMING15	0x0f	/* AUXFIFO Wr/Rd Timing 15 */

    /* ENAMCODE */
#define	FGA_ENAMCODE_EXTUSRDAT_R	0x2	/* DPR R  access with AM 0x09 */
#define	FGA_ENAMCODE_EXTUSRDAT_RW	0x3	/* DPR RW access with AM 0x09 */
#define	FGA_ENAMCODE_EXTUSRPGM_R	0x8	/* DPR R  access with AM 0x0a */
#define	FGA_ENAMCODE_EXTUSRPGM_RW	0xc	/* DPR RW access with AM 0x0a */
#define	FGA_ENAMCODE_EXTSUPDAT_R	0x20	/* DPR R  access with AM 0x0d */
#define	FGA_ENAMCODE_EXTSUPDAT_RW	0x30	/* DPR RW access with AM 0x0d */
#define	FGA_ENAMCODE_EXTSUPPGM_R	0x80	/* DPR R  access with AM 0x0e */
#define	FGA_ENAMCODE_EXTSUPPGM_RW	0xc0	/* DPR RW access with AM 0x0e */

    /* FMBCTL */
#define	FGA_FMBCTL_SLOT_MASK		0x1f	/* slot # mask */
#define	FGA_FMBCTL_CHANNEL0		0x20	/* channel 0 enabled */
#define	FGA_FMBCTL_CHANNEL1		0x40	/* channel 1 enabled */
#define	FGA_FMBCTL_BOTH			0x80	/* FMB access w/ AM 0x0d/0x09 */

/* function declarations */

#ifndef _ASMLANGUAGE
#if defined(__STDC__) || defined(__cplusplus)

#if	CPU_FAMILY == I960
IMPORT  STATUS  sysIntDisable ();
IMPORT  STATUS  sysIntEnable ();
IMPORT	int	sysBusIntAck ();
IMPORT  STATUS  sysBusIntGen ();
IMPORT  STATUS  sysMailboxEnable ();
#else	/* CPU_FAMILY == I960 */
IMPORT  STATUS  sysIntDisable (int intLevel);
IMPORT  STATUS  sysIntEnable (int intLevel);
IMPORT  int	sysBusIntAck (int intLevel);
IMPORT  STATUS  sysBusIntGen (int level, int vector);
IMPORT  STATUS  sysMailboxEnable (char *mailboxAdrs);
#endif  /* CPU_FAMILY == I960 */

IMPORT  STATUS  sysMailboxConnect (FUNCPTR routine, int arg);

#else	/* __STDC__ */

IMPORT  STATUS  sysIntDisable ();
IMPORT  STATUS  sysIntEnable ();
IMPORT  int	sysBusIntAck ();
IMPORT  STATUS  sysBusIntGen ();
IMPORT  STATUS  sysMailboxEnable ();
IMPORT  STATUS  sysMailboxConnect ();

#endif  /* __STDC__ */

#endif  /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCfga002h */
