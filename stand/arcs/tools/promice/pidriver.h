/*	pidriver.h - Edit 6

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/* P R O M I C E   D E V I C E   I N T E R F A C E  */

/* PROMICE Commands*/

#define	PI_BR	0x03		/* used for establishing baudrate */
#define	PI_ID 	0x00		/* establish unit ID */

#define	PI_LP	0x00		/* load data pointer */
#define	PI_WR	0x01		/* write data to promice */
#define	PI_RD	0x02		/* read data from promice */
#define	PI_RS	0x03		/* restart unit */
#define	PI_MO	0x04		/* set mode */
#define	PI_TS	0x05		/* test ram */
#define	PI_RT	0x06		/* reset target */
#define	PI_TP	0x07		/* test PromICE */
#define	PI_MB	0x07		/* modify byte via req/ack */
#define	PI_CX	0x08		/* set communications link */
#define	PI_CW	0x09		/* write com data */
#define	PI_CR	0x0A		/* read com data */
#define	PI_CM	0x0B		/* change com mode */
#define	AI_BR	0x0C		/* AI break-point */
#define	AI_HT	0x0D		/* AI hardware trap */
#define	PI_EX	0x0E		/* extended command */
#define	PI_VS	0x0F		/* read version */

/* Command byte modifiers for various commands */

#define	CM_PICOM	0x80	/* PiCOM bit in com commands */
#define	CM_SEMU		0x80	/* set emulation size in MODE */
#define	CM_WHBYTS	0x80	/* write high bytes in PI_TP */
#define	CM_AICTRD	0x80	/* AI comm test and read */
#define	CM_MBREAD	0x40	/* do read in modbyte command */
#define	CM_ASYNC	0x40	/* async response */
#define	CM_CHANGE	0x40	/* change mode per data */
#define	CM_AITTY	0x40	/* put AI in tty mode */
#define	CM_CINIT	0x40	/* initialize control area */
#define	CM_RPORTS	0x40	/* read port in PI_TP */
#define	CM_AICTST	0x40	/* do AI comm test in PI_TP */
#define	CM_TPOW		0x40	/* target power or load mode */
#define	CM_NORSP	0x20	/* No response bit for all commands */
#define	CM_TACT		0x20	/* target is running */
#define	CM_INTT		0x10	/* interrupt the target */
#define	CM_FILLC	0x10	/* do fill instead of PI_TS */
#define	CM_FAIL		0x10	/* command failed (in RSP) */
#define	CM_SERN		0x10	/* return ser# not version */
#define	CM_AIREG	0x10	/* AI manipulation in MODE */
#define	CM_AITST	0x10	/* AI test in PI_TP */


/* Mode byte flags for PI_MO command - stored in PXMODE1 */

#define	MO_FAST		0x80	/* response at full baud rate */
#define	MO_SLOW		0x80	/* respond at limited baud rate */
#define	MO_XTND		0x40	/* extended response required */
#define	MO_MORE		0x20	/* more mode to follow */
#define	MO_PPXN		0x10	/* parallel port on */
#define	MO_PPXO		0x08	/* parallel port off */
#define	MO_PPGO		0x04	/* fast parallel port load */
#define	MO_AUTO		0x02	/* enable auto-reset */
#define	MO_LOAD		0x01	/* put unit in load mode */
#define	MO_EMU		0x00	/* back to emulate mode */

/* if MO_MORE then second mode byte - stored in PXMODE2 */

#define	M2_TAINT	0x80	/* int target on hostint */
#define	M2_TARST	0x40	/* reset target on hostint */
#define	M2_AIGOF	0x10	/* run aitty in fast mode */
#define	M2_AIRCI	0x08	/* no perchar rcv int */
#define	M2_AIRST	0x04	/* reset AItty on hostint */
#define	M2_NOTIM	0x02	/* no timer in promice */
#define	M2_LIGHT	0x01	/* no run light blink */

/* Mode byte flags for PI_CM command */

#define	MC_COMON	0x80	/* turn link on */
#define	MC_ASYNC	0x40	/* global async read */
#define	MC_REQH		0x20	/* reqest is hi asserted */
#define	MC_ACKH		0x10	/* ack is hi asserted */
#define	MC_INTH		0x08	/* int to target is hi asserted */
#define	MC_AIRCI	0x04	/* do rcv char int on AI */
#define	MC_GXINT	0x02	/* global command completion int */
#define	MC_EXINL	0x01	/* set target int length */


/* AI control register addresses */

#define	AI_CLOCKA	0x38	/* clock address latch A */
#define	AI_CLOCKB	0x39	/* clock address latch B */
#define	AI_CLOCKC	0x3A	/* clock address latch C */
#define	AI_CLOCKD	0x3B	/* clock data latch */
#define	AI_READAT	0x3C	/* read target data */
#define	AI_READST	0x3D	/* read status register */
#define	AI_DELAY0	0x3E	/* delay select bit 0 */
#define	AI_SLAVES	0x3F	/* slave select */
#define	AI_COMPON	0xB8	/* comparator on/off bit */
#define	AI_NORMAL	0xB9	/* normal comm. mode */
#define	AI_BURST0	0xBA	/* burst mode select bit 0 */
#define	AI_DELAY1	0xBB	/* delay select bit 1 */
#define	AI_BURST1	0xBC	/* burst mode select bit 1 */
#define	AI_OVRFLO	0xBD	/* host data overflow bit */
#define	AI_SLAVEW	0xBE	/* slave write enable */
#define	AI_MASTRW	0xBF	/* master write enable */
#define	AI_BITCHG	0x40	/* mask for bit change */
#define	AI_READIT	0xF7	/* mask for register read */

/* PiCOM status bits */

#define	PS_TDA	0x01		/* target data available */
#define	PS_HDA	0x02		/* host data available */
#define	PS_BUSY	0x10		/* interface busy */
#define	PS_ENB	0x80		/* interface enabled */

/* RemoteView - monitor commands */

#define	RV_HI		0x7E	/* sign on */
#define	RV_EH		0x5A	/* reply */
#define	RV_LP		0x00	/* load pointer */
#define	RV_WR		0x01	/* write */
#define	RV_RD		0x02	/* read */
#define	RV_TT		0x03	/* test PROMICE */
#define	RV_DR		0x04	/* read data regs */
#define	RV_AR		0x14	/* read address regs */
#define	RV_SR		0x24	/* read special regs */
#define	RVm_IR		0x80	/* refer to internal RAM */
#define	RVm_IS		0x10	/* refer to code space */

/* driver function prototypes */

#ifdef ANSI
void picmd(char id,char cmd,char ct,char d0,char d1,char d2,char d3,char d4);
void pirsp(void);
long pi_cmd(void);
long pi_rsp(void);
long pi_open(void);
long pi_close(void);
long pi_write(void);
long pi_read(void);
void pi_sleep(short time);
void pi_tochk(void);

#else
void picmd();
void pirsp();
long pi_cmd();
long pi_rsp();
long pi_open();
long pi_close();
long pi_write();
long pi_read();
void pi_sleep();
void pi_tochk();
#endif
