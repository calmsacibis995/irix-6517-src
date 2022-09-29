/* vbic.h - SBE VBIC Chip Set header */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01e,22sep92,rrr  added support for c++
01d,26may92,rrr  the tree shuffle
01c,04oct91,rrr  passed through the ansification filter
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01b,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01a,10sep89,rld  original SBE release.
*/

/*
This header file defines the register layout of the SBE VMEbus
Interface Controller chip (VBIC) and supplies alpha-numeric
equivalences for register bit definitions.
*/

#ifndef __INCvbich
#define __INCvbich

#ifdef __cplusplus
extern "C" {
#endif

#ifdef	_ASMLANGUAGE
#define VBIC_ADRS(reg)	(VBIC + (reg*VBIC_REG_OFFSET))
#else
#define VBIC_ADRS(reg)	((char *) VBIC + (reg*VBIC_REG_OFFSET))
#endif	/* _ASMLANGUAGE */

#define VBIC_VECTNMI	VBIC_ADRS(0x00)	/* NMI vector */
#define VBIC_VECTLIR1	VBIC_ADRS(0x01)	/* Local vectors */
#define VBIC_VECTLIR2	VBIC_ADRS(0x02)
#define VBIC_VECTLIR3	VBIC_ADRS(0x03)
#define VBIC_VECTLIR4	VBIC_ADRS(0x04)
#define VBIC_VECTLIR5	VBIC_ADRS(0x05)
#define VBIC_VECTLIR6	VBIC_ADRS(0x06)
#define VBIC_VECTLIR7	VBIC_ADRS(0x07)
#define VBIC_VECTLIR8	VBIC_ADRS(0x08)
#define VBIC_VECTLIR9	VBIC_ADRS(0x09)
#define VBIC_VECTLI10	VBIC_ADRS(0x0a)
#define VBIC_VECTVIG	VBIC_ADRS(0x0b)	/* VMEbus Interrupt Generator */
#define VBIC_VECTMSM	VBIC_ADRS(0x0c)	/* Millisecond Marker */
#define VBIC_VECTVME	VBIC_ADRS(0x0d)	/* VMEbus interrupter for VIR1-7 */

#define VBIC_IRMASKA	VBIC_ADRS(0x0e)	/* Interrupt enable register A */
#define	M_LIR1		0x02	  /* Enable level 1 interrupt */
#define	M_LIR2		0x04	  /* Enable level 2 interrupt */
#define	M_LIR3		0x08	  /* Enable level 3 interrupt */
#define	M_LIR4		0x10	  /* Enable level 4 interrupt */
#define	M_LIR5		0x20	  /* Enable level 5 interrupt */
#define	M_LIR6		0x40	  /* Enable level 6 interrupt */
#define	M_LIR7		0x80	  /* Enable level 7 interrupt */

#define VBIC_IRMASKB	VBIC_ADRS(0x0f)	/* Interrupt enable register B */
#define	M_LIR8		0x01	  /* Enable level 8 interrupt */
#define	M_LIR9		0x02	  /* Enable level 9 interrupt */
#define	M_LIR10		0x04	  /* Enable level 10 interrupt */
#define	M_MSM		0x40	  /* Enable MSM interrupt */
#define	M_VIG		0x80	  /* Enable VIG interrupt */

#define VBIC_IRMASKC	VBIC_ADRS(0x10)	/* Interrupt enable register C */
#define	M_VIR1		0x02	  /* Enable VME level 1 interrupt */
#define	M_VIR2		0x04	  /* Enable VME level 2 interrupt */
#define	M_VIR3		0x08	  /* Enable VME level 3 interrupt */
#define	M_VIR4		0x10	  /* Enable VME level 4 interrupt */
#define	M_VIR5		0x20	  /* Enable VME level 5 interrupt */
#define	M_VIR6		0x40	  /* Enable VME level 6 interrupt */
#define	M_VIR7		0x80	  /* Enable VME level 7 interrupt */

#define VBIC_IRSCANA	VBIC_ADRS(0x11)	/* Interrupt Request status A */
#define	S_LIR1		0x02	  /* Asserting level 1 intr */
#define	S_LIR2		0x04	  /* Asserting level 2 intr */
#define	S_LIR3		0x08	  /* Asserting level 3 intr */
#define	S_LIR4		0x10	  /* Asserting level 4 intr */
#define	S_LIR5		0x20	  /* Asserting level 5 intr */
#define	S_LIR6		0x40	  /* Asserting level 6 intr */
#define	S_LIR7		0x80	  /* Asserting level 7 intr */

#define VBIC_IRSCANB	VBIC_ADRS(0x12)	/* Interrupt Request status B */
#define	S_LIR8		0x01	  /* Asserting level 8 intr */
#define	S_LIR9		0x02	  /* Asserting level 9 intr */
#define	S_LIR10		0x04	  /* Asserting level 10 intr */
#define	S_MSM		0x40	  /* Asserting MSM intr */
#define	S_VIG		0x80	  /* Asserting VIG intr */

#define VBIC_IRSCANC	VBIC_ADRS(0x13)	/* VIR1-7 Int Request status C */
#define	S_VIR1		0x02	  /* Asserting VME level 1 intr */
#define	S_VIR2		0x04	  /* Asserting VME level 2 intr */
#define	S_VIR3		0x08	  /* Asserting VME level 3 intr */
#define	S_VIR4		0x10	  /* Asserting VME level 4 intr */
#define	S_VIR5		0x20	  /* Asserting VME level 5 intr */
#define	S_VIR6		0x40	  /* Asserting VME level 6 intr */
#define	S_VIR7		0x80	  /* Asserting VME level 7 intr */

#define VBIC_VBICCTL	VBIC_ADRS(0x14)	/* VBIC Control */
#define	VBC_NONMIV	0	  /* NMI uses VECTNMI as vector */
#define	VBC_NMIV	0x80	  /* NMI is auto-vectored */

#define VBIC_ICLIR1	VBIC_ADRS(0x15)	/* Interruptor Config */
#define VBIC_ICLIR2	VBIC_ADRS(0x16)
#define VBIC_ICLIR3	VBIC_ADRS(0x17)
#define VBIC_ICLIR4	VBIC_ADRS(0x18)
#define VBIC_ICLIR5	VBIC_ADRS(0x19)
#define VBIC_ICLIR6	VBIC_ADRS(0x1a)
#define VBIC_ICLIR7	VBIC_ADRS(0x1b)
#define VBIC_ICLIR8	VBIC_ADRS(0x1c)
#define VBIC_ICLIR9	VBIC_ADRS(0x1d)
#define VBIC_ICLIR10	VBIC_ADRS(0x1e)
#define	ICL_AUTOV	0x80	  /* Auto-vectored interrupt */
#define	ICL_EXTV	0x40	  /* External vectored interrupt */
#define	ICL_EDGE	0x20	  /* Edge sensitive interrupter */
#define	ICL_LEVEL	0x00	  /* Level sensitive interrupter */
#define	ICL_HIGH	0x10	  /* Active high / rising edge */
#define	ICL_LOW		0x00	  /* Active low / falling edge */

#define VBIC_ICVIR1	 VBIC_ADRS(0x1f)	/* VME Interruptor Config */
#define VBIC_ICVIR2	 VBIC_ADRS(0x20)
#define VBIC_ICVIR3	 VBIC_ADRS(0x21)
#define VBIC_ICVIR4	 VBIC_ADRS(0x22)
#define VBIC_ICVIR5	 VBIC_ADRS(0x23)
#define VBIC_ICVIR6	 VBIC_ADRS(0x24)
#define VBIC_ICVIR7	 VBIC_ADRS(0x25)

#define VBIC_ICVIG	VBIC_ADRS(0x26)	/* VIG Interrupter Config */
#define VBIC_ICMSM	VBIC_ADRS(0x27)	/* MSM Interruptor Config */
#define VBIC_MSMCTL	VBIC_ADRS(0x28)	/* Millisecond Marker Control */
#define	MSM_IRQEN	0x80	  /* MSM interrupter is enabled */
#define	MSM_HALTED	0x40	  /* MSM is halted */
#define	MSM_MSEC1	0x20	  /* MSM is in 1ms mode */
#define	MSM_TEST	0x10	  /* MSM is in test mode */
#define	MSM_NORMAL	2	  /* cmd: set to normal (~test) */
#define	MSM_SEC1	3	  /* cmd: 1ms interrupt mode */
#define	MSM_SEC10	4	  /* cmd: 10ms interrupt mode */
#define	MSM_STOP	5	  /* cmd: stops MSM counter */
#define	MSM_GO		6	  /* cmd: starts MSM counter */
#define	MSM_INIT	7	  /* cmd: sets MSM counter to 0 */

#define VBIC_VIRSID1	VBIC_ADRS(0x29)	/* StsID latched from VME level 1 */
#define VBIC_VIRSID2	VBIC_ADRS(0x2a)
#define VBIC_VIRSID3	VBIC_ADRS(0x2b)
#define VBIC_VIRSID4	VBIC_ADRS(0x2c)
#define VBIC_VIRSID5	VBIC_ADRS(0x2d)
#define VBIC_VIRSID6	VBIC_ADRS(0x2e)
#define VBIC_VIRSID7	VBIC_ADRS(0x2f)

#define VBIC_RSTCTL	VBIC_ADRS(0x30)	/* Software Reset Control */
#define	VBIC_RESET	0x6a		/* cmd: resets VBIC */
#define VBIC_SLOTSTAT	VBIC_ADRS(0x37)	/* Slot-1 status */
#define	SLOT1		0x80		/* slot 1 jumper installed */
#define VBIC_VOUTCTL	VBIC_ADRS(0x40)	/* VME Interrupt Output Control */
#define	VIG_GEN_1	0x01		/* Generate bus intr level 1 */
#define	VIG_GEN_2	0x02		/* Generate bus intr level 2 */
#define	VIG_GEN_3	0x03		/* Generate bus intr level 3 */
#define	VIG_GEN_4	0x04		/* Generate bus intr level 4 */
#define	VIG_GEN_5	0x05		/* Generate bus intr level 5 */
#define	VIG_GEN_6	0x06		/* Generate bus intr level 6 */
#define	VIG_GEN_7	0x07		/* Generate bus intr level 7 */
#define	VIG_PEND_1	0x02		/* Pending bus intr level 1 */
#define	VIG_PEND_2	0x04		/* Pending bus intr level 2 */
#define	VIG_PEND_3	0x08		/* Pending bus intr level 3 */
#define	VIG_PEND_4	0x10		/* Pending bus intr level 4 */
#define	VIG_PEND_5	0x20		/* Pending bus intr level 5 */
#define	VIG_PEND_6	0x40		/* Pending bus intr level 6 */
#define	VIG_PEND_7	0x80		/* Pending bus intr level 7 */

#define VBIC_VOUTSID1	VBIC_ADRS(0x41)	/* StsID to send for VME level 1 */
#define VBIC_VOUTSID2	VBIC_ADRS(0x42)	/* StsID to send for VME level 2 */
#define VBIC_VOUTSID3	VBIC_ADRS(0x43)	/* StsID to send for VME level 3 */
#define VBIC_VOUTSID4	VBIC_ADRS(0x44)	/* StsID to send for VME level 4 */
#define VBIC_VOUTSID5	VBIC_ADRS(0x45)	/* StsID to send for VME level 5 */
#define VBIC_VOUTSID6	VBIC_ADRS(0x46)	/* StsID to send for VME level 6 */
#define VBIC_VOUTSID7	VBIC_ADRS(0x47)	/* StsID to send for VME level 7 */

#define VBIC_VIGCTL	 VBIC_ADRS(0x48)	/* VMEbus Intr Generator Control */
#define	VIG_ENABLE_1	 0x02		/* Enable VME int ACK interrupt */
#define	VIG_ENABLE_2	 0x04
#define	VIG_ENABLE_3	 0x08
#define	VIG_ENABLE_4	 0x10
#define	VIG_ENABLE_5	 0x20
#define	VIG_ENABLE_6	 0x40
#define	VIG_ENABLE_7	 0x80

#define VBIC_VIASR	VBIC_ADRS(0x49)	/* VMEbus Intr Ack Status */
#define	SR_IACK_1	0x02		/* Level 1 IACK completed */
#define	SR_IACK_2	0x04		/* Level 2 IACK completed */
#define	SR_IACK_3	0x08		/* Level 3 IACK completed */
#define	SR_IACK_4	0x10		/* Level 4 IACK completed */
#define	SR_IACK_5	0x20		/* Level 5 IACK completed */
#define	SR_IACK_6	0x40		/* Level 6 IACK completed */
#define	SR_IACK_7	0x80		/* Level 7 IACK completed */

#define VBIC_SRCLR	VBIC_ADRS(0x4a)	/* VIASR Bit CLear */
#define	SRCLR_1		0x01		/* Clear bit 1 of VIASR */
#define	SRCLR_2		0x02		/* Clear bit 2 of VIASR */
#define	SRCLR_3		0x03		/* Clear bit 3 of VIASR */
#define	SRCLR_4		0x04		/* Clear bit 4 of VIASR */
#define	SRCLR_5		0x05		/* Clear bit 5 of VIASR */
#define	SRCLR_6		0x06		/* Clear bit 6 of VIASR */
#define	SRCLR_7		0x07		/* Clear bit 7 of VIASR */

#define VBIC_SYSCTL	VBIC_ADRS(0x4b)	/* System Control Register */
#define	BUS_TIME_16	0x00		/* 16us timeout period */
#define	BUS_TIME_32	0x40		/* 32us timeout period */
#define	BUS_TIME_64	0x80		/* 64us timeout period */
#define	BUS_TIME_128	0xc0		/* 128us timeout period */
#define	BUS_ARB_RRS	0x00		/* Round robin */
#define	BUS_ARB_PRRS	0x20		/* Priority round robin */
#define	BUS_ARB_PRI	0x30		/* Absolute priority */
#define	BUS_LVL_0	0x00		/* Use bus request level 0 */
#define	BUS_LVL_1	0x04		/* Use bus request level 1 */
#define	BUS_LVL_2	0x08		/* Use bus request level 2 */
#define	BUS_LVL_3	0x0c		/* Use bus request level 3 */
#define	BUS_REL_ROR	0x00		/* Release bus on request */
#define	BUS_REL_RWD	0x02		/* Release bus when done */

#ifdef __cplusplus
}
#endif

#endif /* __INCvbich */
