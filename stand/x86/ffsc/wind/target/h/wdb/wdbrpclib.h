/* wdbRpcLib.h - header file for remote debug agents RPC interface */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,03jun95,ms	added wdbRpcNotifyConnect.
01b,05apr95,ms  new data types.
01a,06feb95,ms	written.
*/

#ifndef __INCwdbRpcLibh
#define __INCwdbRpcLibh

/* includes */

#include "wdb/wdbcommiflib.h"

/* data types */

typedef struct		/* hidden */
    {
    WDB_COMM_IF *	pCommIf;	/* communication interface */
    XDR			xdrs;		/* XDR stream */
    UINT32 *		pInBuf;		/* input buffer */
    int			numBytesIn;	/* # bytes recv'ed */
    UINT32 *		pOutBuf;	/* output buffer */
    u_int		numBytesOut;	/* # bytes sent */
    u_int		bufSize;	/* size of buffers in bytes */
    u_int		xid;		/* transport ID */
    struct sockaddr_in	notifyAddr;	/* address to send event data */
    struct sockaddr_in	replyAddr;	/* address to send reply */
    } WDB_XPORT;

/* function prototypes */

#if defined(__STDC__)

extern void	wdbRpcXportInit	 	(WDB_XPORT *pXport, WDB_COMM_IF *pIf,
				  	 caddr_t pInBuf, caddr_t pOutBuf,
				  	 u_int bufSize);
extern BOOL	wdbRpcRcv	 	(void *pXport, struct timeval *tv);
extern BOOL	wdbRpcGetArgs	 	(void *pXport, BOOL (*inProc)(),
				  	 caddr_t args);
extern void     wdbRpcReply      	(void *pXport, BOOL (*inProc)(),
				  	 caddr_t addr);
extern void     wdbRpcReplyErr   	(void *pXport, uint_t errStatus);
extern void     wdbRpcResendReply	(void *pXport);
extern void     wdbRpcNotifyHost	(void *pXport);
extern void	wdbRpcNotifyConnect	(void *pXport);

#else	/* __STDC__ */

extern void	wdbRpcXportInit		();
extern BOOL	wdbRpcRcv		();
extern void     wdbRpcIn		();
extern BOOL	wdbRpcGetArgs		();
extern void     wdbRpcReply		();
extern void     wdbRpcReplyErr		();
extern void     wdbRpcResendReply	();
extern void     wdbRpcNotifyHost	();
extern void	wdbRpcNotifyConnect	();

#endif	/* __STDC__ */

#endif /* __INCwdbRpcLibh */


