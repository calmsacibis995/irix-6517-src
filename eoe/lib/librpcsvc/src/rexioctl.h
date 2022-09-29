/* @(#)ioctl.h	1.2 87/05/15 3.2/4.3NFSSRC */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)ioctl.h	7.1 (Berkeley) 6/4/86
 */

/*
 * Ioctl definitions
 */
# include <sys/termio.h>

struct tchars {
	char	t_intrc;	/* interrupt */
	char	t_quitc;	/* quit */
	char	t_startc;	/* start output */
	char	t_stopc;	/* stop output */
	char	t_eofc;		/* end-of-file */
	char	t_brkc;		/* input delimiter (like nl) */
};
struct ltchars {
	char	t_suspc;	/* stop process signal */
	char	t_dsuspc;	/* delayed stop process signal */
	char	t_rprntc;	/* reprint line */
	char	t_flushc;	/* flush output (toggles) */
	char	t_werasc;	/* word erase */
	char	t_lnextc;	/* literal next character */
};

/*
 * Structure for TIOCGETP and TIOCSETP ioctls.
 */

struct sgttyb {
	char	sg_ispeed;		/* input speed */
	char	sg_ospeed;		/* output speed */
	char	sg_erase;		/* erase character */
	char	sg_kill;		/* kill character */
	short	sg_flags;		/* mode flags */
};

/*
 * Pun for SUN.
 */
struct ttysize {
	unsigned short	ts_lines;
	unsigned short	ts_cols;
	unsigned short	ts_xxx;
	unsigned short	ts_yyy;
};

#define		TANDEM		0x00000001	/* send stopc on out q full */
#define		CBREAK		0x00000002	/* half-cooked mode */
#define		LCASE		0x00000004	/* simulate lower case */
#define		B_ECHO		0x00000008	/* echo input */
#define		CRMOD		0x00000010	/* map \r to \r\n on output */
#define		RAW		0x00000020	/* no i/o processing */
#define		ODDP		0x00000040	/* get/send odd parity */
#define		EVENP		0x00000080	/* get/send even parity */
#define		ANYP		0x000000c0	/* get any parity/send none */
#define		NLDELAY		0x00000300	/* \n delay */
#define			B_NL0	0x00000000
#define			B_NL1	0x00000100	/* tty 37 */
#define			B_NL2	0x00000200	/* vt05 */
#define			B_NL3	0x00000300
#define		TBDELAY		0x00000c00	/* horizontal tab delay */
#define			B_TAB0	0x00000000
#define			B_TAB1	0x00000400	/* tty 37 */
#define			B_TAB2	0x00000800
#ifndef sgi
#define		XTABS		0x00000c00	/* expand tabs on output */
#endif
#define		CRDELAY		0x00003000	/* \r delay */
#define			B_CR0	0x00000000
#define			B_CR1	0x00001000	/* tn 300 */
#define			B_CR2	0x00002000	/* tty 37 */
#define			B_CR3	0x00003000	/* concept 100 */
#define		VTDELAY		0x00004000	/* vertical tab delay */
#define			B_FF0	0x00000000
#define			B_FF1	0x00004000	/* tty 37 */
#define		BSDELAY		0x00008000	/* \b delay */
#define			B_BS0	0x00000000
#define			B_BS1	0x00008000
#define		ALLDELAY	(NLDELAY|TBDELAY|CRDELAY|VTDELAY|BSDELAY)
#define		CRTBS		0x00010000	/* do backspacing for crt */
#define		PRTERA		0x00020000	/* \ ... / erase */
#define		CRTERA		0x00040000	/* " \b " to wipe out char */
#define		TILDE		0x00080000	/* hazeltine tilde kludge */
#define		MDMBUF		0x00100000	/* start/stop output on carrier intr */
#define		LITOUT		0x00200000	/* literal output */
#define		B_TOSTOP	0x00400000	/* SIGSTOP on background output */
#ifndef sgi
#define		FLUSHO		0x00800000	/* flush output to terminal */
#endif
#define		NOHANG		0x01000000	/* no SIGHUP on carrier drop */
#define		L001000		0x02000000
#define		CRTKIL		0x04000000	/* kill line with " \b " */
#define		PASS8		0x08000000
#define		CTLECH		0x10000000	/* echo control chars as ^X */
#ifndef sgi
#define		PENDIN		0x20000000	/* tp->t_rawq needs reread */
#endif
#define		DECCTQ		0x40000000	/* only ^Q starts after ^S */
#define		B_NOFLSH	0x80000000	/* no output flush on signal */
