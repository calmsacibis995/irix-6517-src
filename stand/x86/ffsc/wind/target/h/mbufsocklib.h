/* mbufSockLib.h - mbuf socket interface library header */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,08nov94,dzb  written.
*/

#ifndef __INCmbufSockLibh
#define __INCmbufSockLibh

#ifdef __cplusplus
extern "C" {
#endif

/* includes */

#include "mbuflib.h"

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void *	_mbufSockLibInit (void);
extern int	_mbufSockSend (int s, MBUF_ID mbufId, int mbufLen, int flags);
extern int	_mbufSockSendto (int s, MBUF_ID mbufId, int mbufLen, int flags,
                    struct sockaddr *to, int tolen);
extern int	_mbufSockBufSend (int s, char *buf, int bufLen,
                    VOIDFUNCPTR freeRtn, int freeArg, int flags);
extern int	_mbufSockBufSendto (int s, char *buf, int bufLen,
                    VOIDFUNCPTR freeRtn, int freeArg, int flags,
                    struct sockaddr *to, int tolen);
extern MBUF_ID	_mbufSockRecv (int s, int flags, int *pLen);
extern MBUF_ID	_mbufSockRecvfrom (int s, int flags, int *pLen,
                    struct sockaddr *from, int *pFromLen);
 
#else	/* __STDC__ */

extern void *	_mbufSockLibInit ();
extern int	_mbufSockSend ();
extern int	_mbufSockSendto ();
extern int	_mbufSockBufSend ();
extern int	_mbufSockBufSendto ();
extern MBUF_ID	_mbufSockRecv ();
extern MBUF_ID	_mbufSockRecvfrom ();
 
#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCmbufSockLibh */
