/*	RvStruct.h - Edit 0.2

	RomView Version 1.1a
	Copyright (C) 1990-2 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - Data structures for RomView support software
	
*/
#ifdef PIRV
typedef struct
	{
	short	flags;			/* misc flags */
#define	RBIN	0x0001		/* install this one */
#define	RBEX	0x0002		/* uninstall this one */
#define	RBSY	0x0004		/* break point active */
#define	RBSU	0x0008		/* suspended */
#define	RBCT	0x0010		/* counting down */
#define	RBVL	0x0020		/* conditional break */
	long	bad;			/* break address */
	unsigned char code[8];	/* instruction code */
	long	count;			/* count down or value */
	long	vad;			/* variable address */
	} RVBREAK;


extern	RVBREAK		rxbreak[RVC_NBRK]; /* all the break points */
extern	long		rxnbrk;			/* break point number */
extern	long		rxstep;			/* number of steps to take */
extern	long		rxlink;			/* link address */
extern	long		rxdstart;		/* dump start address */
extern	long		rxdend;			/* dump end address */
extern	long		rxistart;		/* inst dump start address */
extern	long		rxiend;			/* inst dump end address */
extern	long		rxstkp;			/* stack pointer */
extern	long		rxstks;			/* stack size */
extern	long		rxwords;		/* word size for edit/dump */
extern	long		rxestart;		/* edit start address */
extern	long		rxelist[RVC_LST];	/* edit list */
extern	long		rxelcnt;			/* element count */
extern	char		rxbbf[RVC_BS];	/* big buffer - file data */
extern	long		rxbbc;			/* data count in `bbf` */
extern	char		rxxbf[RVC_BS];	/* transfer buffer out - ROM data */
extern	long		rxxbc;			/* data count in `xbf` */
extern	char		rxybf[RVC_BS];	/* transfer buffer in - ROM data */
extern	long		rxybc;			/* data count in `ybf` */
extern	char		*rxdbf;			/* buffer pointer for the driver */
extern	long		rxdbc;			/* count for driver transfer */
extern	long		rxxloc;			/* xbuf load pointer */
extern	long		rxyloc;			/* ybuf load pointer */
extern	long		rxcloc;			/* current load pointer */
extern	short		rxdmask;		/* data register transfer mask */
extern	short		rxamask;		/* address register transfer mask */
extern	short		rxrsize;		/* size of register data expected */
extern	char		*rxreg;			/* register name */
extern	long		rxrval;			/* register value */
extern	long		rxuseri;		/* arbitrary user input */
extern	long		rxstart;		/* user program start address */

extern	long		rvseva;			/* RemoteView monitor save area address */
extern	long		rvxqa;			/* RemoteView monitor xequte area */
extern	long		rvmons;			/* RemoteView monitor code start */
extern	long		rvmone;			/* RemoteView monitor code end */
extern	long		rvbrkc;			/* code for setting break */

extern	long		rvjump;			/* mask for jump */
extern	long		rvcall;			/* mask for call */
extern	long		rvpcts;			/* pc to stack operation */
extern	long		rvstpc;			/* stack to pc operation */
extern	long		rvmtac;			/* move memory to accumulator */
extern	long		rvactm;			/* move accumulator to memory */

extern	short	rxflags;			/* internal flags */
#define	RxWR	0x0001				/* write register value */
#define	RxUI	0x0002				/* user input present */
#define	RxIS	0x0004				/* dump code space */
#define	RxIR	0x0008				/* dump internal ram */
#define	RxIO	0x0010				/* internal operation */
#define RxJC	0x0020				/* jump or call operation */
#define	RxJY	0x0040				/* jump taken */

extern	short	rvflags;			/* misc internal flags */
#define RvUP	0x0001				/* link is up */
#define RvAI	0x0002				/* AiCOM link */
#define RvPI	0x0004				/* PiCOM link */
#define	RvGO	0x0008				/* user program running */
#define	RvBP	0x0010				/* at break point */
#define	RvIB	0x0020				/* buffer has code in it */
#define RvAS	0x8000				/* use async i/o */
extern char	rxproto;				/* protocol flag */
#endif
