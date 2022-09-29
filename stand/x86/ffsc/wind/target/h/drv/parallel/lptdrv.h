/* lptDrv.h - IBM-PC LPT */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,13oct94,hdn	 written.
*/

#ifndef	__INClptDrvh
#define	__INClptDrvh

#ifdef __cplusplus
extern "C" {
#endif


#ifndef _ASMLANGUAGE

typedef struct			/* LPT_DEV */
    {
    DEV_HDR	devHdr;
    BOOL	created;	/* true if this device has been created */
    BOOL	autofeed;	/* enable autofeed */
    USHORT	data;		/* data register */
    USHORT	stat;		/* status register */
    USHORT	ctrl;		/* control register */
    char	*pBuf;		/* pointer to buffer */
    int		size;		/* number of bytes to transfer */
    int 	throughput;	/* through put (bytes/sec) */
    int 	retryCnt;	/* retry count */
    int 	intCnt;		/* interrupts */
    int 	secCnt;		/* seconds */
    int 	delays;		/* number of delays for STROBE */
    } LPT_DEV;


/* IO address (LPT) */

#define LPT1_BASE_ADRS		0x3bc
#define LPT2_BASE_ADRS		0x378
#define LPT3_BASE_ADRS		0x278
#define N_LPT_CHANNELS		3

/* register definitions */

#define CAST
#define LPT_REG_ADDR_INTERVAL   1       /* address diff of adjacent regs. */
#define LPT_ADRS(base,reg)   (CAST (base+(reg*LPT_REG_ADDR_INTERVAL)))

#define LPT_DATA(base)	LPT_ADRS(base,0x00)	/* data reg. */
#define LPT_STAT(base)	LPT_ADRS(base,0x01)	/* status reg. */
#define LPT_CTRL(base)	LPT_ADRS(base,0x02)	/* control reg. */


/* control register */

#define C_STROBE	0x01
#define C_AUTOFEED	0x02
#define C_NOINIT	0x04
#define C_SELECT	0x08
#define C_ENABLE	0x10
#define C_INPUT		0x20

/* status register */

#define S_NODERR	0x08
#define S_SELECT	0x10
#define S_PERR		0x20
#define S_NOACK		0x40
#define S_NOBUSY	0x80

/* ioctl functions */

#define LPT_SETCONTROL	0
#define LPT_GETSTATUS	1


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

STATUS	lptDrv (int vector, int level);
STATUS	lptDevCreate (char *name, int channel);

#else

STATUS	lptDrv ();
STATUS	lptDevCreate ();

#endif  /* __STDC__ */


#endif	/* _ASMLANGUAGE */


#ifdef __cplusplus
}
#endif

#endif	/* __INClptDrvh */
