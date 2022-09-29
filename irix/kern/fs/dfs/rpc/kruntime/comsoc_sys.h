/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 * @HP_COPYRIGHT@
 */
/*
 * HISTORY
 * $Log: comsoc_sys.h,v $
 * Revision 65.3  1998/03/09 19:15:50  lmc
 * Added uthread_t * to rpc__soo_select call.  This passes around a pointer
 * to a dummy uthread structure which is used for the pollrotor lock, etc.
 * Threads are now sthreads, so there is no process name to set.
 *
 * Revision 65.1  1997/10/24  14:29:54  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:35  jdoak
 * Initial revision
 *
 * Revision 64.1  1997/02/14  19:45:31  dce
 * *** empty log message ***
 *
 * Revision 1.1  1995/07/27  16:45:00  dcebuild
 * Initial revision
 *
 * Revision 1.1.74.3  1994/06/10  20:54:05  devsrc
 * 	cr10871 - fix copyright
 * 	[1994/06/10  14:59:51  devsrc]
 *
 * Revision 1.1.74.2  1994/05/27  15:35:37  tatsu_s
 * 	DG multi-buffer fragments.
 * 	Added RPC_C_MAX_IOVEC_LEN and RPC_SOCKET_RECVMSG().
 * 	[1994/04/29  18:54:03  tatsu_s]
 * 
 * Revision 1.1.74.1  1994/01/21  22:31:19  cbrooks
 * 	RPC Code Cleanup
 * 	[1994/01/21  20:32:58  cbrooks]
 * 
 * Revision 1.1.6.2  1993/07/19  19:38:10  zeliff
 * 	HP port of DFS
 * 	[1993/07/19  18:29:32  zeliff]
 * 
 * Revision 1.1.4.3  1993/07/16  21:01:44  kissel
 * 	*** empty log message ***
 * 	[1993/06/21  14:46:06  kissel]
 * 
 * 	Initial GAMERA branch
 * 	[1993/03/29  13:17:28  mgm]
 * 
 * Revision 1.1.2.2  1993/06/04  21:25:41  kissel
 * 	Initial HPUX RP version.
 * 	[1993/06/04  21:22:07  kissel]
 * 
 * Revision 1.1.2.2  1992/06/18  18:35:12  tmm
 * 	06/16/92 tmm Created from COSL drop.
 * 	[1992/06/18  18:30:32  tmm]
 * 
 * $EndLog$
 */
#ifndef _COMSOC_SYS_H
#define _COMSOC_SYS_H
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**
**  NAME:
**
**      comsoc_sys.h
**
**  FACILITY:
**
**      OSF1 OSC Kernel Port Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Alternate socket abstraction definitions (still uses com/comsoc.h too).
**
*/

#include <dce/dce.h>
#include <ksys/behavior.h>

/*
 * A handle to a socket.  The implementation of this type is considered
 * to be private to this package.
 */


typedef struct socket *rpc_socket_t;       /* a kernel socket handle */

/*
 * A public function for comparing two socket handles.
 */

#define RPC_SOCKET_IS_EQUAL(s1, s2)             (s1 == s2)


/*
 * This package's error type and values.  The implementation of this
 * type is considered to be private to this package.
 */

typedef int rpc_socket_error_t;                 /* a UNIX errno */

/*
 * The maximum number of iov elements which can be sent through
 * sendmsg is MSG_IOVLEN-1.
 */
#define RPC_C_MAX_IOVEC_LEN (MSG_MAXIOVLEN - 1)

/*
 * Public error constants and functions for comparing errors.
 * The _ETOI_ (error-to-int) function converts a socket error to a simple
 * integer value that can be used in error mesages.
 */

#define RPC_C_SOCKET_OK           0             /* a successful error value */
#define RPC_C_SOCKET_EWOULDBLOCK  EWOULDBLOCK   /* operation would block */
#define RPC_C_SOCKET_EINTR        EINTR         /* OPERATION WAS interrupted */
#define RPC_C_SOCKET_EADDRINUSE   EADDRINUSE    /* address was in use (see bind) */
#define RPC_C_SOCKET_ETIMEDOUT    ETIMEDOUT     /* connection request timed out */
#define RPC_C_SOCKET_EIO          EIO           /* "catch-all"... */

#define RPC_SOCKET_IS_ERR(serr)                 (serr != RPC_C_SOCKET_OK)
#define RPC_SOCKET_ERR_EQ(serr, e)              (serr == e)
#define RPC_SOCKET_ETOI(serr)                   (serr)


/*
 * Macros for performance critical operations.
 */

#define RPC_SOCKET_SENDMSG(sock, iovp, iovlen, addrp, ccp, serrp) \
    { \
        *(serrp) = rpc__socket_sendmsg(sock, iovp, iovlen, addrp, ccp); \
    }

#define RPC_SOCKET_RECVFROM(sock, buf, buflen, from, ccp, serrp) \
    { \
        *(serrp) = rpc__socket_recvfrom(sock, buf, buflen, from, ccp); \
    }

#define RPC_SOCKET_RECVMSG(sock, iovp, iovlen, addrp, ccp, serrp) \
    { \
        *(serrp) = rpc__socket_recvmsg(sock, iovp, iovlen, addrp, ccp); \
    }

/*
 * Some special stuff the this version of select.
 */

#include <sys/poll.h>               /* for POLL* defines */

typedef struct {
    rpc_socket_t    so;
    short           events;         /* POLL{NORM,OUT,PRI} */
} rpc_socket_sel_t, *rpc_socket_sel_p_t;


/*
 * R P C _ _ S O C K E T _ S E L E C T
 *
 * In-kernel useable version of select for sockets.
 */
PRIVATE rpc_socket_error_t rpc__socket_select _DCE_PROTOTYPE_ ((
        int /*nsoc*/,
        rpc_socket_sel_t * /*soc*/,
        int * /*nfsoc*/,
        rpc_socket_sel_t * /*fsoc*/,
        struct timeval * /*tmo*/,
        uthread_t * /* ut - dummy uthread structure */
    ));


#endif  /* COMSOC_SYS_H */
