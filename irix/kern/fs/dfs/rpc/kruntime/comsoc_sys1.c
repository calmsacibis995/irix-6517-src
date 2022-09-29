/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: comsoc_sys1.c,v 1.6 1998/04/21 19:37:05 lmc Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif

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
 * $Log: comsoc_sys1.c,v $
 * Revision 1.6  1998/04/21 19:37:05  lmc
 * Change a printf to a debug statement so we notice when we hit it. (Temporary)
 *
 * Revision 1.5  1998/03/24  16:21:50  lmc
 * Fixed missing comment characters lost when the ident lines were
 * ifdef'd for the kernel.  Also now calls rpc_icrash_init() for
 * initializing symbols, and the makefile changed to build rpc_icrash.c
 * instead of dfs_icrash.c.
 *
 * Revision 1.4  1998/03/23  17:26:46  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 1.3  1998/03/22 02:51:00  lmc
 * These changes make krpc work for at least the simple case.  Threads
 * are now sthreads, but for the select code to work we needed a
 * uthread structure.  This is dummied up in the listener code.
 *
 * Revision 65.3  1997/12/16  17:30:17  lmc
 * SOCKET_LOCK and SOCKET_UNLOCK only take one parameter now.  Also changed
 * polltime to match what is in the kudzu select.c code.
 *
 * Revision 65.2  1997/11/06  19:56:51  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/24  14:29:54  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:35  jdoak
 * Initial revision
 *
 * Revision 64.1  1997/02/14  19:45:30  dce
 * *** empty log message ***
 *
 * Revision 1.3  1996/10/18  18:07:28  vrk
 * Fixed a wild goto statement in rpc__socket_select(). The for loop control
 * variable was not incremented properly resulting in indexing past the valid
 * number of elements(RPC_C_SERVER_MAX_SOCKETS) in fndsoc array. This resulted
 * in corruption of rpc_g_dg_pkt_pool.pkt_mutex resulting in a panic in
 * mutex_queue().
 *
 * Revision 1.2  1996/06/15  03:00:29  brat
 * Changed references to curproc to access curprocp directly instead of via uarea
 * to make it work even for kernel threads which have no u area.
 *
 * Revision 1.1  1995/09/15  18:05:11  dcebuild
 * Initial revision
 *
 * Revision 1.1.63.4  1994/06/10  20:54:02  devsrc
 * 	cr10871 - fix copyright
 * 	[1994/06/10  14:59:48  devsrc]
 *
 * Revision 1.1.63.3  1994/05/27  15:35:35  tatsu_s
 * 	Fixed the corrupted iov in rpc__socket_recvmsg().
 * 	[1994/05/05  18:00:12  tatsu_s]
 * 
 * 	DG multi-buffer fragments.
 * 	Added rpc__socket_recvmsg().
 * 	[1994/04/29  18:53:49  tatsu_s]
 * 
 * Revision 1.1.63.2  1994/02/02  21:48:50  cbrooks
 * 	OT9855 code cleanup breaks KRPC
 * 	[1994/02/02  20:59:53  cbrooks]
 * 
 * Revision 1.1.63.1  1994/01/21  22:31:16  cbrooks
 * 	RPC Code Cleanup
 * 	[1994/01/21  20:32:56  cbrooks]
 * 
 * Revision 1.1.61.1  1993/10/05  13:41:39  root
 * 	Add port restriction support for krpc - this is porting
 * 	the code in runtime/comsoc_bsd.c  (rpc__socket_bind)
 * 	to comsoc_sys.c
 * 	[1993/10/05  13:29:52  root]
 * 
 * Revision 1.1.4.2  1993/06/10  19:21:37  sommerfeld
 * 	Initial HPUX RP version.
 * 	[1993/06/03  21:58:44  kissel]
 * 
 * 	Port to HPUX 9.0.
 * 	[1992/10/16  22:11:31  toml]
 * 
 * 	06/16/92 tmm Created from COSL drop
 * 	[1992/06/18  18:30:26  tmm]
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**
**  NAME:
**
**      comsoc_osc.c
**
**  FACILITY:
**
**      Example Kernel Port Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Implementations of various socket abstraction interfaces used by the
**  kernel runtime version of the macros in com/comsoc.h
**
**  Basically, these are just kernel callable versions of the standard
**  user invoked kernel interfaces.  This is based on the BSD version of
**  socket code.  To make this into a working version, the select
**  code in rpc__socket_select needs to be added; typically this is done
**  by starting with code extracted from select() in the operating system
**  being ported to.
**  Instead of passing file descriptors, a kernel socket handle is used.
**  Additionally, errors are returned through an error argument.
**
*/

/*
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	Base: src/rpc/kruntime/BSD44_TEMPLATE/comsoc_sys.c, , dce_osf, dce.75d (Berkeley) 8/22/91
 */

/* changes for porting to sgi
 * check if bsd44 -> bsd43 conversion is required for SGI line 262
 *	 if need to have a SOCKET_LOCK before calling fn on line 803
 */

#include <sys/types.h>
#include <sys/pcb.h>                                               
#include <sys/cmn_err.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/vsocket.h>

#include <commonp.h>
#include <com.h>
#include <comprot.h>
#include <comnaf.h>
#include <comp.h>

#include <sys/param.h>
#ifdef PRE64
#include <sys/user.h>
#endif
#include <sys/proc.h>
#include <sys/file.h>


/*
 * Includes and declarations for in-kernel select implementation.
 */
#include <sys/time.h>               /* for timeval */
#include <sys/systm.h>              /* for selwait */
/*
#include <net/netmp.h>
*/

#ifdef	PRIVATE
#undef PRIVATE
#define PRIVATE
#endif

/*
#define COM_DEBUG
*/
/*
extern struct timeval time;
*/

/* ======================================================================== */

/*
 * What we think a socket's buffering is in case rpc__socket_set_bufs()
 * fails miserably.  This is system dependent if you want it to be correct.
 * for irix - set to values in kern/bsd/sys/socket.h
 */

#define RPC_C_SOCK_GUESSED_RCVBUF    0x1001
#define RPC_C_SOCK_GUESSED_SNDBUF    0x1002

/* ======================================================================== */

INTERNAL rpc_socket_error_t getsockopt_nck _DCE_PROTOTYPE_((
        bhv_desc_t * /*behavior -> socket*/,
        int  /*level*/, 
        int  /*name*/,
        char * /*val*/,
        int * /*valsize*/
    ));


INTERNAL rpc_socket_error_t sockargs_nck _DCE_PROTOTYPE_((
        struct mbuf ** /*aname*/,
        struct sockaddr * /*name*/,
        int  /*namelen*/,
        int  /*type*/
    ));

/* ========================================================================= */


INTERNAL rpc_socket_error_t getsockopt_nck
(
    bhv_desc_t *bd,
    int level,
    int name,
    char *val,
    int *avalsize
)
{
    struct mbuf *m = NULL;
    rpc_socket_error_t error;
    int valsize;

    if (val)
        valsize = *avalsize;
    else
        valsize = 0;
    error = sogetopt(bd, level, name, &m);
    if (error)
        goto bad;
    if (val && valsize && m != NULL) {
        if (valsize > m->m_len)
            valsize = m->m_len;
        bcopy(mtod(m, caddr_t), val, (u_int)valsize);

        *avalsize = valsize;
    }
bad:
    if (m != NULL)
        (void) m_free(m);
    return (error);
}

INTERNAL rpc_socket_error_t setsockopt_nck
(
    rpc_socket_t sock,
    int level, 
    int name,
    char *val,
    int valsize
)
{
    struct mbuf *m = NULL;
    rpc_socket_error_t error;
/*
    bhv_desc_t	*bd;
    bd = &((sock)->so_bhv);
    bhv_desc_init(bd, &sock, NULL, NULL);
*/
    bhv_desc_init(&((sock)->so_bhv), sock, NULL, NULL);

    if (valsize > MLEN)
        return(EINVAL);
    if (val) {
        m = m_get(M_WAIT, MT_SOOPTS);
        if (m == NULL)
            return(ENOBUFS);
        bcopy(val, mtod(m, caddr_t), (u_int)valsize);

        m->m_len = valsize;
    }
    error = sosetopt(&((sock)->so_bhv), level, name, m);
    return (error);
}

INTERNAL rpc_socket_error_t sockargs_nck
(
    struct mbuf **aname,
    struct sockaddr *name,
    int namelen, 
    int type
)
{
    struct mbuf *m;

    if (namelen > MLEN)
        return (EINVAL);
    m = m_get(M_WAIT, type);
    if (m == NULL)
        return (ENOBUFS);
    m->m_len = namelen;
    bcopy(name, mtod(m, caddr_t), (u_int)namelen);

    *aname = m;
  /* irix does not need this conversion    */
#ifndef	SGIMIPS
    /* HPUX does not need this bsd44 -> bsd43 sockaddr conversion.
     * this code isn't really correctly portable anyway. */
    if (type == MT_SONAME) {
        register struct sockaddr *sa = mtod(m, struct sockaddr *);
#if defined(COMPAT_43) && BYTE_ORDER != BIG_ENDIAN
        if (sa->sa_family == 0 && sa->sa_len < AF_MAX)
            sa->sa_family = sa->sa_len;
#endif
        sa->sa_len = namelen;
    }
#endif /* HPUX */
    return (RPC_C_SOCKET_OK);
}

/* ======================================================================== */

/*
 * R P C _ _ S O C K E T _ O P E N
 *
 * Create a new socket for the specified Protocol Sequence.
 * The new socket has blocking IO semantics.
 *
 * (see BSD UNIX socket(2)).
 * 
 * - irix note
 * note that call to socreate allocates a "mbuf" structure, since it is
 * not followed by a call to makesockvp (as done in the system - socket
 * call), there is no corresponding file and vnode entry for this 
 * socket. 
 */

PRIVATE rpc_socket_error_t rpc__socket_open
(
    rpc_protseq_id_t protseq,
    rpc_socket_t *sock
)
{
    rpc_socket_error_t error;
    struct vsocket *vso = NULL;

#ifdef COM_DEBUG
    cmn_err(CE_NOTE, "rpc__socket_open() called");
#endif
    /* defines in rpc/runtime/comp.h */
    error = socreate(
                (int) RPC_PROTSEQ_INQ_NAF_ID(protseq),
                sock,
                (int) RPC_PROTSEQ_INQ_NET_IF_ID(protseq),
                (int) RPC_PROTSEQ_INQ_NET_PROT_ID(protseq) );
    /*  Initialize the behavior associated with this socket.  lvector
	is the basic socket operations. */
    if (!error) {
	bhv_desc_init(&((*sock)->so_bhv), sock, NULL, NULL);
    }

    if (error)
        RPC_DBG_GPRINTF(("(rpc__socket_open) error = %d\n", error));
    return (error);
}


/*
 * R P C _ _ S O C K E T _ O P E N _ B A S I C
 *
 * A special version of socket_open that is used *only* by 
 * the low level initialization code when it is trying to 
 * determine what network services are supported by the host OS.
 */

PRIVATE rpc_socket_error_t rpc__socket_open_basic
(
    rpc_naf_id_t naf,
    rpc_network_if_id_t net_if,
    rpc_network_protocol_id_t net_prot,
    rpc_socket_t *sock
)
{
    rpc_socket_error_t error;

#ifdef COM_DEBUG
    cmn_err(CE_NOTE, "rpc__socket_open_basic() called");
#endif
    error = socreate((int) naf, sock, (int) net_if, (int) net_prot );
    /* Don't bother initializing a behavior here because the socket
	is only used briefly and then closed.  There is no reading/writing
	or select on the socket */
    return (error);
}


/*
 * R P C _ _ S O C K E T _ C L O S E
 *
 * Close (destroy) a socket.
 *
 * (see BSD UNIX close(2)).
 */

PRIVATE rpc_socket_error_t rpc__socket_close
(
    rpc_socket_t sock
)
{
    rpc_socket_error_t error;

#ifdef COM_DEBUG
    cmn_err(CE_NOTE, "rpc__socket_close() called");
#endif
    error = soclose(sock);
    if (error)
        RPC_DBG_GPRINTF(("(rpc__socket_open) error = %d\n", error));
    return (error);
}


/*
 * R P C _ _ S O C K E T _ B I N D
 *
 * Bind a socket to a specified local address.
 *    The sobind() call expects a behavior descriptor, but
 *	only uses to find the socket.
 *
 * (see BSD UNIX bind(2)).
 */

PRIVATE rpc_socket_error_t rpc__socket_bind
(
    rpc_socket_t sock,
    rpc_addr_p_t addr
)
{
    struct mbuf *nam;
    rpc_socket_error_t error;
    unsigned32 status;
    rpc_addr_p_t temp_addr=NULL;
    bhv_desc_init(&((sock)->so_bhv), sock, NULL, NULL);

#ifdef COM_DEBUG
    cmn_err(CE_NOTE, "rpc__socket_bind() called");
#endif
    error = sockargs_nck(&nam, (struct sockaddr *)&addr->sa, addr->len, MT_SONAME);
    if (error)
        goto bad;


    if (! RPC_PROTSEQ_TEST_PORT_RESTRICTION (addr -> rpc_protseq_id))
    {
        error = sobind(&(sock->so_bhv), nam);	/* does its own splnet */
    }
    else{
              /*
         * Port restriction is in place.  If the address has a well-known
         * endpoint, then do a simple bind.
         */

        unsigned_char_t *endpoint;
        unsigned char c;

        rpc__naf_addr_inq_endpoint (addr, &endpoint, &status);

        c = endpoint[0];               /* grab first char */
        rpc_string_free (&endpoint, &status);

        if (c != '\0')       /* test for null string */
        {
            error = sobind(&((sock)->so_bhv), nam);
        }                               /* well-known endpoint */

        else
        {
            /*
             * Port restriction is in place and the address doesn't have a
             * well-known endpoint.  Try to bind until we hit a good port,
             * or exhaust the retry count.
             *
             * Make a copy of the address to work in; if we hardwire an
             * endpoint into our caller's address, later logic could infer
             * that it is a well-known endpoint.
             */

            unsigned32 i;
            boolean found;
            for (i = 0, found = false;
                 (i < RPC_PORT_RESTRICTION_INQ_N_TRIES (addr->rpc_protseq_id))
                 && !found;
                 i++)
            {
                unsigned_char_p_t port_name;

                rpc__naf_addr_overcopy (addr, &temp_addr, &status);

                if (status != rpc_s_ok)
                {
                    error = RPC_C_SOCKET_EIO;
                    break;
                }

                rpc__naf_get_next_restricted_port
                    (temp_addr -> rpc_protseq_id, &port_name, &status);

                if (status != rpc_s_ok)
                {
                    error = RPC_C_SOCKET_EIO;
                    break;
                }

                rpc__naf_addr_set_endpoint (port_name, &temp_addr, &status);

                if (status != rpc_s_ok)
                {
                    error = RPC_C_SOCKET_EIO;
                    rpc_string_free (&port_name, &status);
                    break;
                }
                m_freem(nam);
                error = sockargs_nck(&nam, (struct sockaddr *)&temp_addr->sa, 
                                     temp_addr->len, MT_SONAME);
                if (error)
                    goto bad;

                if (sobind(&((sock)->so_bhv), nam) == 0)
                {
                    found = true;
                    error = RPC_C_SOCKET_OK;
                }
                else
                    error = RPC_C_SOCKET_EIO;
                rpc_string_free (&port_name, &status);
            }                           /* for i */

            if (!found)
            {
                error = RPC_C_SOCKET_EADDRINUSE;
            }
        }                               /* no well-known endpoint */
    }

    m_freem(nam);
bad:
    if (error)
        RPC_DBG_GPRINTF(("(rpc__socket_bind) error = %d\n", error));
    return (error);
}

static void
copyiovec(rpc_socket_iovec_p_t rpc_iovp, struct iovec *irix_iovp, int count)
{
int i;

	for(i=0; i< count; i++){
		irix_iovp[i].iov_base = (void *)rpc_iovp[i].base;
		irix_iovp[i].iov_len = (size_t)rpc_iovp[i].len;
	}
}


/*
 * R P C _ _ S O C K E T _ S E N D M S G
 *
 * Send a message over a given socket.  An error code as well as the
 * actual number of bytes sent are returned.  Notice that this routine
 * just uses sosend() (which *always* copies the data to a mbuf) so you
 * can safely send stack based (automatic variables) data buffers.
 *
 * THIS VERSION ISN'T REQUIRED TO WORK AT INTERRUPT LEVEL.
 *
 * (see BSD UNIX sendmsg(2)).
 */

PRIVATE rpc_socket_error_t rpc__socket_sendmsg
(
    rpc_socket_t sock,
    rpc_socket_iovec_p_t iov,   /* array of bufs of data to send */
    int iov_len,                /* number of bufs */
    rpc_addr_p_t addr,          /* addr of receiver */
    int *cc                    /* returned number of bytes actually sent */
)
{
    rpc_socket_error_t error;
    struct mbuf *to = NULL;
    struct uio auio;
    struct iovec *aiov = NULL;
    long len;
    int i;
    unsigned int uzero = 0;
    bhv_desc_init(&((sock)->so_bhv), sock, NULL, NULL);
    

#ifdef COM_DEBUG
    cmn_err(CE_NOTE, "rpc__socket_sendmsg() called");
#endif

    aiov = (struct iovec *) kern_malloc(sizeof(struct iovec) * MSG_MAXIOVLEN);

    if ((u_int)iov_len >= MSG_MAXIOVLEN) {
        error = EMSGSIZE;
        goto bad;
    }


#if 0
    /* don't corrupt the passed iov */
    bcopy((caddr_t)iov,(caddr_t)aiov, iov_len * sizeof (struct iovec));
    /* replace it by copy in a loop to take care of diff in iovec structs */
#endif
    copyiovec(iov, aiov, iov_len);

    auio.uio_iov = aiov;
    auio.uio_iovcnt = iov_len;
    auio.uio_segflg = UIO_SYSSPACE;
#ifndef	SGIMIPS
    auio.uio_rw = UIO_WRITE;
#endif
    auio.uio_offset = 0;        /* XXX */
    auio.uio_resid = 0;
    for (i = 0; i < iov_len; i++, iov++) {
        if (iov->len < uzero){
            error = EINVAL;
	    goto bad;
	}
        auio.uio_resid += iov->len;
    }

    error = sockargs_nck(&to, (struct sockaddr *)&addr->sa, addr->len, MT_SONAME);
    if (error)
        goto bad;
    len = auio.uio_resid;

    /*
     * Typically, qsave is setup at this point to deal with EINTR vs RESTARTSYS.
     * For now I'm assuming that this isn't an issue since all our sockets
     * *should* be set to non-blocking mode, hence we shouldn't sleep.
     */

    error = sosend(&((sock)->so_bhv), to, &auio, 0, NULL);

#ifdef SGIMIPS
    /*  Parameter is defined as an int.  Shouldn't be any overflow */
    *cc = (int)(len - auio.uio_resid);
#else
    *cc = len - auio.uio_resid;
#endif
bad:
    if (to)
        m_freem(to);
    if (aiov)
	kern_free(aiov);
    if (error)
        RPC_DBG_GPRINTF(("(rpc__socket_sendmsg) error = %d\n", error));
    return (error);
}


/*
 * R P C _ _ S O C K E T _ R E C V F R O M
 *
 * Recieve the next buffer worth of information from a socket.  A
 * rpc_addr_t appropriate for the NAF corresponding to this socket must
 * be provided.  addr.len must set to the actual size of addr.sa.  This
 * operation fills in addr.sa and sets addr.len to the new size of the
 * field.  An error status as well as the actual number of bytes received
 * are also returned.
 * 
 * THIS VERSION ISN'T REQUIRED TO WORK AT INTERRUPT LEVEL.
 *
 * (see BSD UNIX recvfrom(2)).
 */

PRIVATE rpc_socket_error_t rpc__socket_recvfrom
(
    rpc_socket_t sock,
    byte_p_t buf,           /* buf for rcvd data */
    int len,                /* len of above buf */
    rpc_addr_p_t addr,      /* addr of sender, really in/out */
    int *cc                /* returned number of bytes actually rcvd */
)
{
    rpc_socket_error_t error;
    struct mbuf *from = NULL;
    rpc_socket_iovec_t iov;
    struct uio auio;
    bhv_desc_init(&((sock)->so_bhv), sock, NULL, NULL);


#ifdef COM_DEBUG
    cmn_err(CE_NOTE, "rpc__socket_recvfrom() called");
#endif
    iov.base = buf;
    iov.len = len;
    auio.uio_iov = (struct iovec *)&iov;
    auio.uio_iovcnt = 1;
    auio.uio_segflg = UIO_SYSSPACE;
#ifndef	SGIMIPS
    auio.uio_rw = UIO_READ;
#endif
    auio.uio_offset = 0;        /* XXX */
    auio.uio_resid = len;

    /*
     * Typically, qsave is setup at this point to deal with EINTR vs RESTARTSYS.
     * For now I'm assuming that this isn't an issue since all our sockets
     * *should* be set to non-blocking mode, hence we shouldn't sleep.
     */

    error = soreceive(&((sock)->so_bhv), &from, &auio, 0, NULL);

    if (error)
        goto bad;
#ifdef SGIMIPS
    /*  Parameter is defined as an int.  Shouldn't be any overflow */
    *cc = (int)(len - auio.uio_resid);
#else
    *cc = len - auio.uio_resid;
#endif

#ifdef COMPAT_43
        mtod(from, struct osockaddr *)->sa_family =
            mtod(from, struct sockaddr *)->sa_family;
#endif
    if (addr != NULL) {
        if (addr->len > from->m_len)      /* ??? */
            addr->len = from->m_len;
        (void) bcopy(mtod(from, caddr_t), (caddr_t)&addr->sa, (unsigned)addr->len);

    }

bad:
    if (from)
        m_freem(from);
    if (error && (error != EWOULDBLOCK))
        RPC_DBG_GPRINTF(("(rpc__socket_recvfrom) error = %d\n", error));
    return (error);
}

/*
 * R P C _ _ S O C K E T _ R E C V M S G
 *
 * Recieve the next buffers worth of information from a socket.  A
 * rpc_addr_t appropriate for the NAF corresponding to this socket must
 * be provided.  addr.len must set to the actual size of addr.sa.  This
 * operation fills in addr.sa and sets addr.len to the new size of the
 * field.  An error status as well as the actual number of bytes received
 * are also returned.
 * 
 * THIS VERSION ISN'T REQUIRED TO WORK AT INTERRUPT LEVEL.
 *
 * (see BSD UNIX recvmsg(2)).
 */

PRIVATE rpc_socket_error_t rpc__socket_recvmsg
(
    rpc_socket_t sock,
    rpc_socket_iovec_p_t iov,   /* array of bufs for rcvd data */
    int iov_len,                /* number of bufs */
    rpc_addr_p_t addr,          /* addr of sender; really in/out */
    int *cc                     /* returned number of bytes actually rcvd */
)
{
    rpc_socket_error_t error;
    struct mbuf *from = NULL;
    struct uio auio;
    struct iovec *aiov = NULL;
    long len;
    int i;
    unsigned int uzero = 0;
    bhv_desc_init(&((sock)->so_bhv), sock, NULL, NULL);


#ifdef COM_DEBUG
    cmn_err(CE_NOTE, "rpc__socket_recvmsg() called");
#endif

    aiov = (struct iovec *) kern_malloc(sizeof(struct iovec) * MSG_MAXIOVLEN);

    if ((u_int)iov_len >= MSG_MAXIOVLEN) {
        error = EMSGSIZE;
        goto bad;
    }

#if 0
    /* don't corrupt the passed iov */
    bcopy((caddr_t)iov, (caddr_t)aiov, iov_len * sizeof (struct iovec));
    /* replace it by copy in a loop to take care of diff in iovec structs */
#endif
    copyiovec(iov, aiov, iov_len);

    auio.uio_iov = aiov;
    auio.uio_iovcnt = iov_len;
    auio.uio_segflg = UIO_SYSSPACE;
#ifndef	SGIMIPS
    auio.uio_rw = UIO_READ;
#endif
    auio.uio_offset = 0;        /* XXX */
    auio.uio_resid = 0;
    for (i = 0; i < iov_len; i++, iov++) {
        if (iov->len < uzero){
            error = EINVAL;
	    goto bad;
	}
        auio.uio_resid += iov->len;
    }

    len = auio.uio_resid;

    /*
     * Typically, qsave is setup at this point to deal with EINTR vs RESTARTSYS.
     * For now I'm assuming that this isn't an issue since all our sockets
     * *should* be set to non-blocking mode, hence we shouldn't sleep.
     */

    error = soreceive(&((sock)->so_bhv), &from, &auio, 0, NULL);
    if (error)
        goto bad;
#ifdef SGIMIPS
    /*  Parameter is defined as an int.  Shouldn't be any overflow */
    *cc = (int)(len - auio.uio_resid);
#else
    *cc = len - auio.uio_resid;
#endif

#ifdef COMPAT_43
        mtod(from, struct osockaddr *)->sa_family =
            mtod(from, struct sockaddr *)->sa_family;
#endif
    if (addr != NULL) {
        if (addr->len > from->m_len)      /* ??? */
            addr->len = from->m_len;
        (void) bcopy(mtod(from, caddr_t),
            (caddr_t)&addr->sa, (unsigned)addr->len);
    }

bad:
    if (from)
        m_freem(from);
    if (aiov)
	kern_free(aiov);
    if (error && (error != EWOULDBLOCK))
        RPC_DBG_GPRINTF(("(rpc__socket_recvmsg) error = %d\n", error));
    return (error);
}

/*
 * R P C _ _ S O C K E T _ I N Q _ A D D R
 *
 * Return the local address associated with a socket.  A rpc_addr_t
 * appropriate for the NAF corresponding to this socket must be provided.
 * addr.len must set to the actual size of addr.sa.  This operation fills
 * in addr.sa and sets addr.len to the new size of the field.
 *
 * !!! NOTE: You should use rpc__naf_desc_inq_addr() !!!
 *
 * This routine is indended for use only by the internal routine:
 * rpc__naf_desc_inq_addr().  rpc__socket_inq_addr() only has the
 * functionality of BSD UNIX getsockname() which doesn't (at least not
 * on all systems) return the local network portion of a socket's address.
 * rpc__naf_desc_inq_addr() returns the complete address for a socket.
 *
 * (see BSD UNIX getsockname(2)).
 */

PRIVATE rpc_socket_error_t rpc__socket_inq_endpoint
(
    rpc_socket_t sock,
    rpc_addr_p_t addr  /* really in/out */
)
{
    struct mbuf *m = NULL;
    rpc_socket_error_t error;
    int s;
    bhv_desc_init(&((sock)->so_bhv), sock, NULL, NULL);

#ifdef COM_DEBUG
    cmn_err(CE_NOTE, "rpc__socket_inq_endpoint() called");
#endif
    m = m_getclr(M_WAIT, MT_SONAME);
    if (m == NULL) {
        error = ENOBUFS;
        goto bad1;
    }
    SOCKET_LOCK(sock);
    error = (*sock->so_proto->pr_usrreq)(sock, PRU_SOCKADDR,
                        (struct mbuf *)0, m, (struct mbuf *)0);
    SOCKET_UNLOCK(sock);
    if (error) {
        goto bad;
    }
    if (addr->len > m->m_len)
        addr->len = m->m_len;
#ifdef COMPAT_43
        mtod(m, struct osockaddr *)->sa_family =
            mtod(m, struct sockaddr *)->sa_family;
#endif
    bcopy(mtod(m, caddr_t), (caddr_t)&addr->sa, (u_int)addr->len);

bad:
    m_freem(m);
bad1:
    if (error)
        RPC_DBG_GPRINTF(("(rpc__socket_inq_addr) error=%d\n", error));
    return (error);
}


/*
 * R P C _ _ S O C K E T _ S E T _ B R O A D C A S T
 *
 * Enable broadcasting for the socket (as best it can).
 * Used only by Datagram based Protocol Services.
 */

PRIVATE rpc_socket_error_t rpc__socket_set_broadcast(rpc_socket_t sock)
{
    rpc_socket_error_t error = RPC_C_SOCKET_OK;

#ifdef SO_BROADCAST
    int setsock_val = 1;

    bhv_desc_init(&((sock)->so_bhv), sock, NULL, NULL);
#ifdef COM_DEBUG
    cmn_err(CE_NOTE, "rpc__socket_set_broadcast() called");
#endif
    error = setsockopt_nck(sock, SOL_SOCKET, SO_BROADCAST, 
            (char *)&setsock_val, sizeof(setsock_val));
    if (error) 
        RPC_DBG_GPRINTF(("(rpc__socket_set_broadcast) error = %d\n", error));
#endif
    return (error);
}


/*
 * R P C _ _ S O C K E T _ S E T _ B U F S
 *
 * Set the socket's send and receive buffer sizes and return the new
 * values.  Typically, it does the best job that it can upon failures.
 *
 * If for some reason your system is screwed up and defines SOL_SOCKET
 * and SO_SNDBUF, but doesn't actually support the SO_SNDBUF and SO_RCVBUF
 * operations AND using them would result in nasty behaviour (i.e. they
 * don't just return some error code), define NO_SO_SNDBUF.
 */

PRIVATE rpc_socket_error_t rpc__socket_set_bufs
(
    rpc_socket_t sock,
    unsigned32 txsize,
    unsigned32 rxsize,
    unsigned32 *ntxsize,
    unsigned32 *nrxsize
)
{
    int sizelen;
    rpc_socket_error_t error;
    bhv_desc_init(&((sock)->so_bhv), sock, NULL, NULL);


#ifdef COM_DEBUG
    cmn_err(CE_NOTE, "rpc__socket_set_bufs() called");
#endif
#if (defined (SOL_SOCKET) && defined(SO_SNDBUF)) && !defined(NO_SO_SNDBUF)
    /*
     * Set the new sizes.
     */

    error = setsockopt_nck(sock, SOL_SOCKET, SO_SNDBUF, 
					(char *)&txsize, sizeof(txsize));
    if (error) {
        RPC_DBG_GPRINTF(("(rpc__socket_set_bufs) WARNING: set sndbuf (%d) failed - error = %d\n", 
            txsize, error));
    }

    error = setsockopt_nck(sock, SOL_SOCKET, SO_RCVBUF, (char *)&rxsize, sizeof(rxsize));
    if (error) {
        RPC_DBG_GPRINTF(("(rpc__socket_set_bufs) WARNING: set rcvbuf (%d) failed - error = %d\n", 
            rxsize, error));
    }

    /*
     * Get the new sizes.  If this fails, just return some guessed sizes.
     */

    sizelen = sizeof *ntxsize;
    error = getsockopt_nck(&((sock)->so_bhv), SOL_SOCKET, SO_SNDBUF, (char *)ntxsize, &sizelen);
    if (error) {
        RPC_DBG_GPRINTF(("(rpc__socket_set_bufs) WARNING: get sndbuf failed - error = %d\n", 
            error));
        *ntxsize = RPC_C_SOCK_GUESSED_SNDBUF;
    }

    sizelen = sizeof *nrxsize;
    error = getsockopt_nck(&((sock)->so_bhv), SOL_SOCKET, SO_RCVBUF, (char *)nrxsize, &sizelen);
    if (error) {
        RPC_DBG_GPRINTF(("(rpc__socket_set_bufs) WARNING: get rcvbuf failed - error = %d\n", 
            error));
        *nrxsize = RPC_C_SOCK_GUESSED_RCVBUF;
    }
#else
    *ntxsize = RPC_C_SOCK_GUESSED_SNDBUF;
    *nrxsize = RPC_C_SOCK_GUESSED_RCVBUF;
#endif
    return (RPC_C_SOCKET_OK);
}


/*
 * R P C _ _ S O C K E T _ S E T _ N B I O
 *
 * Set a socket to non-blocking mode.
 * 
 * Return rpc_c_socket_ok on success, otherwise an error value.
 */

PRIVATE rpc_socket_error_t rpc__socket_set_nbio
(
    rpc_socket_t sock
)
{

    rpc_socket_error_t error = RPC_C_SOCKET_OK;
    int s;

#ifdef COM_DEBUG
    cmn_err(CE_NOTE, "rpc__socket_set_nbio() called");
#endif
    SOCKET_LOCK(sock);
    sock->so_state |= SS_NBIO;
    SOCKET_UNLOCK(sock);

    return (error);
}


/*
 * R P C _ _ S O C K E T _ S E T _ C L O S E _ O N _ E X E C
 *
 *
 * Set a socket to a mode whereby it is not inherited by a spawned process
 * executing some new image. This is possibly a no-op on some systems.
 *
 * Return rpc_c_socket_ok on success, otherwise an error value.
 *
 * This is a no-op because rpc_socket_t's aren't part of a process'
 * state (i.e. they are not in the file table)
 */

PRIVATE rpc_socket_error_t rpc__socket_set_close_on_exec
(
    rpc_socket_t sock
)
{
    return (RPC_C_SOCKET_OK);
}

/*
 * R P C _ _ S O C K E T _ G E T P E E R N A M E
 *
 * Get name of connected peer.
 * This is used only by Connection oriented Protocol Services.
 *
 */

PRIVATE rpc_socket_error_t rpc__socket_getpeername 
(
    rpc_socket_t sock,
    rpc_addr_p_t addr
)
{
    DIE("(rpc__socket_getpeername) Not Supported");

  /* to hush up the compiler */
    return 0; 
}

/*
 * R P C _ _ S O C K E T _ G E T _ I F _ I D
 *
 * Get socket network interface id (socket type).
 *
 */

PRIVATE rpc_socket_error_t rpc__socket_get_if_id 
(
    rpc_socket_t        sock,
    rpc_network_if_id_t *network_if_id
)
{
    int optlen;
    bhv_desc_init(&((sock)->so_bhv), sock, NULL, NULL);

    
#ifdef COM_DEBUG
    cmn_err(CE_NOTE, "rpc__socket_get_if_id() called");
#endif
    optlen = sizeof(rpc_network_if_id_t);
    
    return (getsockopt_nck (&((sock)->so_bhv),
                        SOL_SOCKET,
                        SO_TYPE,
                        (char *)network_if_id,
                        &optlen) == -1  ? -1 : RPC_C_SOCKET_OK);
}

/*
 * R P C _ _ S O C K E T _ N O W R I T E B L O C K _ W A I T
 *
 * Wait until the a write on the socket should succeed without
 * blocking.  If tmo is NULL, the wait is unbounded, otherwise
 * tmo specifies the max time to wait. rpc_c_socket_etimedout
 * if a timeout occurs.  This operation in not cancellable.
 */

rpc_socket_error_t rpc__socket_nowriteblock_wait
(
    rpc_socket_t sock,
    struct timeval *tmo
)
{
    rpc_socket_sel_t    ssel;
    rpc_socket_sel_t    fssel;
    int                 nfnd;
    rpc_socket_error_t  error;
    bhv_desc_init(&((sock)->so_bhv), sock, NULL, NULL);


    ssel.so = sock;
    ssel.events = POLLOUT;

    /*  Need to decide where to put a dummy uthread structure for
	the last parameter.    XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX*/
    debug("Write noblock called!!!\n");
    error = rpc__socket_select(1, &ssel, &nfnd, &fssel, tmo, 0);
    if (error)
        return error;

    if (nfnd == 0)
        return ETIMEDOUT;

    return 0;
}

