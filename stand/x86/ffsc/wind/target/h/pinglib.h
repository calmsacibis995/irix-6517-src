/* pingLib.h - Packet InterNet Grouper (PING) library header */

/* Copyright 1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,25oct94,dzb  written.
*/

#ifndef __INCpingLibh
#define __INCpingLibh

#ifdef __cplusplus
extern "C" {
#endif

/* includes */

#include "vwmodnum.h"
#include "hostlib.h"
#include "inetlib.h"
#include "semlib.h"
#include "wdlib.h"

/* defines */

#define PING_MAXPACKET		4096	/* max packet size */
#define PING_INTERVAL		1	/* default packet interval in seconds */
#define PING_TMO		5	/* default packet timeout in seconds */
#define ICMP_PROTO		1	/* icmp socket proto id */
#define ICMP_TYPENUM		20	/* icmp proto type identifier */
#define PING_TASK_NAME_LEN	20	/* max ping Tx task name length */

/* status codes */

#define	S_pingLib_NOT_INITIALIZED		(M_pingLib | 1)
#define	S_pingLib_TIMEOUT			(M_pingLib | 2)
 
/* flags */

#define	PING_OPT_SILENT		0x1	/* work silently */
#define	PING_OPT_DONTROUTE	0x2	/* dont route option */
#define	PING_OPT_DEBUG		0x4	/* print debugging messages */

/* typedefs */

typedef struct pingStat				/* PING_STAT */
    {
    struct pingStat *	statNext;		/* next struct in list */
    int			pingFd;                 /* socket file descriptor */
    char		toHostName [MAXHOSTNAMELEN + 2];/* name to ping - PAD */
    char		toInetName [INET_ADDR_LEN];/* IP addr to ping */
    u_char		bufTx [PING_MAXPACKET];	/* transmit buffer */
    u_char		bufRx [PING_MAXPACKET];	/* receive buffer */
    struct icmp	*	pBufIcmp;		/* ptr to icmp */
    ulong_t *		pBufTime;		/* ptr to time */
    int			dataLen;		/* size of data portion */
    int			numPacket;		/* total # of packets to send */
    int			numTx;			/* number of packets sent */
    int			numRx;			/* number of packets received */
    int			idTx;			/* id of Tx task */
    int			idRx;			/* id of Rx task */
    int			idTimeout;		/* id of Timeout task */
    int			clkTick;		/* sys clock ticks per second */
    int			tMin;			/* min RT time (ms) */
    int			tMax;			/* max RT time (ms) */
    int			tSum;			/* sum of all times */
    SEM_ID		semIdTimeout;		/* timeout task sem */
    WDOG_ID		wdTimeout;		/* timeout task watchdog */
    int			flags;			/* option flags */
    } PING_STAT;

/* forward declarations */
 
#if defined(__STDC__) || defined(__cplusplus)
 
extern STATUS	 pingLibInit (void);
extern STATUS	 ping (char *host, int numPackets, ulong_t options);

#else   /* __STDC__ */

extern STATUS	pingLibInit ();
extern STATUS	ping ();
 
#endif  /* __STDC__ */
 
#ifdef __cplusplus
}
#endif
 
#endif /* __INCpingLibh */
