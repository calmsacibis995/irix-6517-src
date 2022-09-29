/* zbufSockLib.h - zeroCopy buffer socket interface library header */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,08nov94,dzb  written.
*/

#ifndef __INCzbufSockLibh
#define __INCzbufSockLibh

#ifdef __cplusplus
extern "C" {
#endif

/* includes */

#include "zbuflib.h"
#include "sys/socket.h"

/* typedefs */

typedef struct				/* ZBUF_SOCK_FUNC */
    {
    FUNCPTR	libInitRtn;		/* zbufLibInit()	*/
    FUNCPTR	sendRtn;		/* zbufSockSend()	*/
    FUNCPTR	sendtoRtn;		/* zbufSockSendto()	*/
    FUNCPTR	bufSendRtn;		/* zbufSockBufSend()	*/
    FUNCPTR	bufSendtoRtn;		/* zbufSockBufSend()	*/
    FUNCPTR	recvRtn;		/* zbufSockRecv()	*/
    FUNCPTR	recvfromRtn;		/* zbufSockRecvfrom()	*/
    } ZBUF_SOCK_FUNC;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS	zbufSockLibInit (void);
extern int	zbufSockSend (int s, ZBUF_ID zbufId, int zbufLen, int flags);
extern int	zbufSockSendto (int s, ZBUF_ID zbufId, int zbufLen, int flags,
                    struct sockaddr *to, int tolen);
extern int	zbufSockBufSend (int s, char *buf, int bufLen,
                    VOIDFUNCPTR freeRtn, int freeArg, int flags);
extern int	zbufSockBufSendto (int s, char *buf, int bufLen,
                    VOIDFUNCPTR freeRtn, int freeArg, int flags,
                    struct sockaddr *to, int tolen);
extern ZBUF_ID	zbufSockRecv (int s, int flags, int *pLen);
extern ZBUF_ID	zbufSockRecvfrom (int s, int flags, int *pLen,
                    struct sockaddr *from, int *pFromLen);

#else	/* __STDC__ */

extern STATUS	zbufSockLibInit ();
extern int	zbufSockSend ();
extern int	zbufSockSendto ();
extern int	zbufSockBufSend ();
extern int	zbufSockBufSendto ();
extern ZBUF_ID	zbufSockRecv ();
extern ZBUF_ID	zbufSockRecvfrom ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCzbufSockLibh */
