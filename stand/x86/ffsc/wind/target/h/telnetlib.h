/* telnetLib.h - telnet library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
-------------------
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
01e,26may92,rrr  the tree shuffle
01d,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
01c,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01b,08aug90,shl  added INCtelnetLibh to #endif.
01a,10oct86,dnw  written
*/

#ifndef __INCtelnetLibh
#define __INCtelnetLibh

#ifdef __cplusplus
extern "C" {
#endif

/* Definitions for the TELNET protocol. */

#define	IAC	255		/* interpret as command: */
#define	DONT	254		/* you are not to use option */
#define	DO	253		/* please, you use option */
#define	WONT	252		/* I won't use option */
#define	WILL	251		/* I will use option */
#define	SB	250		/* interpret as subnegotiation */
#define	GA	249		/* you may reverse the line */
#define	EL	248		/* erase the current line */
#define	EC	247		/* erase the current character */
#define	AYT	246		/* are you there */
#define	AO	245		/* abort output--but let prog finish */
#define	IP	244		/* interrupt process--permanently */
#define	BREAK	243		/* break */
#define	DM	242		/* data mark--for connect. cleaning */
#define	NOP	241		/* nop */
#define	SE	240		/* end sub negotiation */
#define EOR     239             /* end of record (transparent mode) */

#define SYNCH	242		/* for telfunc calls */


/* telnet options */

#define TELOPT_BINARY	0	/* 8-bit data path */
#define TELOPT_ECHO	1	/* echo */
#define	TELOPT_RCP	2	/* prepare to reconnect */
#define	TELOPT_SGA	3	/* suppress go ahead */
#define	TELOPT_NAMS	4	/* approximate message size */
#define	TELOPT_STATUS	5	/* give status */
#define	TELOPT_TM	6	/* timing mark */
#define	TELOPT_RCTE	7	/* remote controlled transmission and echo */
#define TELOPT_NAOL 	8	/* negotiate about output line width */
#define TELOPT_NAOP 	9	/* negotiate about output page size */
#define TELOPT_NAOCRD	10	/* negotiate about CR disposition */
#define TELOPT_NAOHTS	11	/* negotiate about horizontal tabstops */
#define TELOPT_NAOHTD	12	/* negotiate about horizontal tab disposition */
#define TELOPT_NAOFFD	13	/* negotiate about formfeed disposition */
#define TELOPT_NAOVTS	14	/* negotiate about vertical tab stops */
#define TELOPT_NAOVTD	15	/* negotiate about vertical tab disposition */
#define TELOPT_NAOLFD	16	/* negotiate about output LF disposition */
#define TELOPT_XASCII	17	/* extended ascic character set */
#define	TELOPT_LOGOUT	18	/* force logout */
#define	TELOPT_BM	19	/* byte macro */
#define	TELOPT_DET	20	/* data entry terminal */
#define	TELOPT_SUPDUP	21	/* supdup protocol */
#define	TELOPT_SUPDUPOUTPUT 22	/* supdup output */
#define	TELOPT_SNDLOC	23	/* send location */
#define	TELOPT_TTYPE	24	/* terminal type */
#define	TELOPT_EOR	25	/* end or record */
#define TELOPT_EXOPL	255	/* extended-options-list */

/* sub-option qualifiers */

#define	TELQUAL_IS	0	/* option is... */
#define	TELQUAL_SEND	1	/* send option */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void 	telnetInTask (int sock, int ptyMfd);
extern void 	telnetInit (void);
extern void 	telnetOutTask (int sock, int ptyMfd);
extern void 	telnetd (void);

#else	/* __STDC__ */

extern void 	telnetInTask ();
extern void 	telnetInit ();
extern void 	telnetOutTask ();
extern void 	telnetd ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCtelnetLibh */
