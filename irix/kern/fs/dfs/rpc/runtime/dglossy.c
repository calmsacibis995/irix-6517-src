/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: dglossy.c,v 65.4 1998/03/23 17:28:38 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: dglossy.c,v $
 * Revision 65.4  1998/03/23 17:28:38  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:21:12  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:57:12  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:16:57  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.402.2  1996/02/18  00:03:47  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:56:17  marty]
 *
 * Revision 1.1.402.1  1995/12/08  00:19:58  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:59:05  root]
 * 
 * Revision 1.1.400.3  1994/05/19  21:14:40  hopkins
 * 	Merge with DCE1_1.
 * 	[1994/05/04  19:39:53  hopkins]
 * 
 * 	Serviceability work.
 * 	[1994/05/03  20:19:59  hopkins]
 * 
 * Revision 1.1.400.2  1994/03/15  20:30:49  markar
 * 	     private client socket mods
 * 	[1994/02/22  19:07:48  markar]
 * 
 * Revision 1.1.400.1  1994/01/21  22:37:02  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:05  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:24:21  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:05:30  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:48:33  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:37:15  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:08:53  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      dglossy.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Implementations of rpc__socket_sendmsg() (and friends) that sometimes
**  do bad things.  This code should be portable since it is completely
**  layered on top of the rpc__socket_ and rpc__naf_ APIs.
**
**
*/

#ifdef RPC_DG_LOSSY

#include <dg.h>

/*
 * Stashed data from calls to rpc__socket_sendmsg() and rpc__socket_sendto().
 */

INTERNAL struct {
    rpc_socket_t sock;
    rpc_socket_iovec_p_t iov;
    int iov_len;
    rpc_addr_p_t addr;
} sm_stash;

INTERNAL int rate = 0, dbg_prnt_level = 0;

typedef unsigned32 lossy_stats_t[4];
INTERNAL struct {
    lossy_stats_t   total;
    lossy_stats_t   recent;     /* stats over last LOSSY_RECENT_INTERVAL xmits */
} lossy_stats;

#define LOSSY_RECENT_INTERVAL   32

/* =========================================================================== */

/* 
 * S E T _ L O S S Y _ P A R A M S
 *
 * The rate of loss is determined by the dg_lossy debug switch value.
 * Rather than incur the expense of making the loss level completely
 * general, we'll look specifically for the values 100, 110, 120, 130,
 * or 140, which will indicate the percentage of packets lost/reordered.
 * Adding 1 to any of the above values turns on the debug output.  E.g.
 * specify 130 to lose 30% of the packets, specify 131 to lose 30% of
 * the packets AND be told about it.
 */

INTERNAL void set_lossy_params _DCE_PROTOTYPE_ (( void ));


INTERNAL void set_lossy_params( void )
{
    if (rate != 0)
        return;

    if (RPC_DBG(rpc_es_dbg_dg_lossy, 140))
    {
        rate = 5;   
        dbg_prnt_level = 141;
    }
    else if (RPC_DBG(rpc_es_dbg_dg_lossy, 130))
    {
        rate = 6;   
        dbg_prnt_level = 131;
    }
    else if (RPC_DBG(rpc_es_dbg_dg_lossy, 120))
    {
        rate = 10;   
        dbg_prnt_level = 121;
    }
    else if (RPC_DBG(rpc_es_dbg_dg_lossy, 110))
    {
        rate = 20;   
        dbg_prnt_level = 111;
    }
    else
    {                     
        /*
         * Anything below 10% gets set as 1%.
         */
        rate = 200;   
        dbg_prnt_level = 102;
    }
}


/*
 * M C O P Y
 *
 * Make a copy of a thing into malloc'd storage.
 */

INTERNAL unsigned8 *mcopy _DCE_PROTOTYPE_((
        unsigned8 * /*p*/,
        int  /*len*/
    ));

INTERNAL unsigned8 *mcopy
#ifdef _DCE_PROTO_
(
    unsigned8 *p,
    int len
)
#else
(p, len)
unsigned8 *p;
int len;
#endif
{
    unsigned8 *q;

    RPC_MEM_ALLOC(q, unsigned8 *, len, RPC_C_MEM_DG_LOSSY, RPC_C_MEM_NOWAIT);

    /*b_c_o_p_y(p, q, len);*/

    memmove(q, p, len) ;

    return (q);
}


/* 
 * M C O P Y _ I O V
 *
 * Make a copy of an I/O vector and all it points to into malloc'd storage.
 */

INTERNAL rpc_socket_iovec_p_t mcopy_iov _DCE_PROTOTYPE_((
        rpc_socket_iovec_p_t  /*iov*/,
        int  /*iovlen*/
    ));

INTERNAL rpc_socket_iovec_p_t mcopy_iov
#ifdef _DCE_PROTO_
(
    rpc_socket_iovec_p_t iov,
    int iovlen
)
#else
(iov, iovlen)
rpc_socket_iovec_p_t iov;
int iovlen;
#endif
{
    unsigned16 i;
    rpc_socket_iovec_p_t ciov;

    RPC_MEM_ALLOC(ciov, rpc_socket_iovec_p_t, sizeof(rpc_socket_iovec_t) * iovlen, 
        RPC_C_MEM_DG_LOSSY, RPC_C_MEM_NOWAIT);

    for (i = 0; i < iovlen; i++) {
        ciov[i].base = mcopy(iov[i].base, iov[i].len);
        ciov[i].len  = iov[i].len; 
    }

    return (ciov);
}


/* 
 * F R E E _ I O V
 *
 * Free an I/O vector.
 */

INTERNAL void free_iov _DCE_PROTOTYPE_((
        rpc_socket_iovec_p_t /*iov*/,
        int /*iovlen*/
    ));

INTERNAL void free_iov
#ifdef _DCE_PROTO_
(
    rpc_socket_iovec_p_t iov,
    int iovlen
)
#else
(iov, iovlen)
rpc_socket_iovec_p_t iov;
int iovlen;
#endif
{
    unsigned16 i;

    for (i = 0; i < iovlen; i++)  
        RPC_MEM_FREE(iov[i].base, RPC_C_MEM_DG_LOSSY);

    RPC_MEM_FREE(iov, RPC_C_MEM_DG_LOSSY);
}


/*
 * S T A S H _ S E N D M S G _ P K T
 *
 * Make a copy and stash a packet described by rpc__socket_sendmsg() args.
 */

INTERNAL void stash_sendmsg_pkt _DCE_PROTOTYPE_((
        rpc_socket_t  /*sock*/,
        rpc_socket_iovec_p_t  /*iov*/,
        int  /*iov_len*/,
        rpc_addr_p_t  /*addr*/
    ));

INTERNAL void stash_sendmsg_pkt
#ifdef _DCE_PROTO_
(
    rpc_socket_t sock,
    rpc_socket_iovec_p_t iov,
    int iov_len,
    rpc_addr_p_t addr
)
#else
(sock, iov, iov_len, addr)
rpc_socket_t sock;
rpc_socket_iovec_p_t iov;
int iov_len;
rpc_addr_p_t addr;
#endif
{
    unsigned32 st;

    if (sm_stash.addr != NULL) {
        rpc__naf_addr_free(&sm_stash.addr, &st);
        free_iov(sm_stash.iov, sm_stash.iov_len);
    }

    sm_stash.sock       = sock;
    sm_stash.iov        = mcopy_iov(iov, iov_len);
    sm_stash.iov_len    = iov_len;       
    rpc__naf_addr_copy(addr, &sm_stash.addr, &st);
}


/*
 * X M I T _ S T A S H E D _ S E N D M S G _ P K T
 *
 * Transmit the packet we stashed away earlier.
 */

INTERNAL void xmit_stashed_sendmsg_pkt _DCE_PROTOTYPE_((void));


INTERNAL void xmit_stashed_sendmsg_pkt( void )
{
    int cc;
    rpc_socket_error_t serr;

    if (sm_stash.addr == NULL) 
        return;

    RPC_DBG_PRINTF(rpc_e_dbg_general, dbg_prnt_level,
        ("(xmit_stashed_sendmsg_pkt) Re-xmit\n"));

    serr = rpc__socket_sendmsg(sm_stash.sock, sm_stash.iov, sm_stash.iov_len,
                                sm_stash.addr, &cc);
    if (! RPC_SOCKET_IS_ERR(serr))
    {
        RPC_DG_PLOG_LOSSY_SENDMSG_PKT(sm_stash.iov, sm_stash.iov_len, 2);
    }
}


/* =========================================================================== */

/*
 * R P C _ _ D G _ L O S S Y _ S O C K E T _ S E N D M S G
 *
 * Version of rpc__socket_sendmsg() that sometimes loses, duplicates,
 * or re-orders delivery of packets.  The rate of loss is determined
 * by the dg_lossy debug switch value (see set_lossy_params()).
 */

PRIVATE rpc_socket_error_t rpc__dg_lossy_socket_sendmsg 
#ifdef _DCE_PROTO_
(
    rpc_socket_t sock,
    rpc_socket_iovec_p_t iov,   /* array of bufs of data to send */
    int iov_len,                /* number of bufs */
    rpc_addr_p_t addr,          /* addr of receiver */
    int *cc                    /* returned number of bytes actually sent */
)
#else
(sock, iov, iov_len, addr, cc)
rpc_socket_t sock;
rpc_socket_iovec_p_t iov;   /* array of bufs of data to send */
int iov_len;                /* number of bufs */
rpc_addr_p_t addr;          /* addr of receiver */
int *cc;                    /* returned number of bytes actually sent */
#endif
{
    int i;
    rpc_socket_error_t serr;
                                     
    /*
     * Iff the lossy switch has been set...
     */
       
    if (RPC_DBG(rpc_es_dbg_dg_lossy, 100))
    {
        set_lossy_params();
        if (lossy_stats.total[3] % LOSSY_RECENT_INTERVAL == 0)
        {
            lossy_stats.recent[0] = lossy_stats.recent[1] = 
                lossy_stats.recent[2] = lossy_stats.recent[3] = 0;
        }
        lossy_stats.recent[3]++;
        lossy_stats.total[3]++;
        switch ((int) RPC_RANDOM_GET(0, 10000) % rate)  /* Min/Max values arbitrarily set */
        {
            case 0:                         /* Drop the pkt on the floor */
                RPC_DBG_PRINTF(rpc_e_dbg_general, dbg_prnt_level,
                    ("(rpc__dg_lossy_socket_sendmsg) Drop pkt\n"));
                lossy_stats.recent[0]++;
                lossy_stats.total[0]++;
                *cc = 0;
                for (i = 0; i < iov_len; i++)
                    *cc += iov[i].len;
                RPC_DG_PLOG_LOSSY_SENDMSG_PKT(iov, iov_len, 0);
                return (RPC_C_SOCKET_OK);
            case 1:                         /* Stash the pkt away for later re-xmit */
                RPC_DBG_PRINTF(rpc_e_dbg_general, dbg_prnt_level,
                    ("(rpc__dg_lossy_socket_sendmsg) Stash pkt\n"));
                lossy_stats.recent[1]++;
                lossy_stats.total[1]++;
                stash_sendmsg_pkt(sock, iov, iov_len, addr);
                break;
            case 2:                         /* Re-xmit sendto stashed pkt if we have one */
                lossy_stats.recent[2]++;
                lossy_stats.total[2]++;
                xmit_stashed_sendmsg_pkt();
                break;
        }
    }

    serr = rpc__socket_sendmsg(sock, iov, iov_len, addr, cc);
    if (! RPC_SOCKET_IS_ERR(serr))
    {
        RPC_DG_PLOG_LOSSY_SENDMSG_PKT(iov, iov_len, 3);
    }
    return (serr);
}

#else
/*
 * If RPC_DG_LOSSY is not defined, declare a dummy variable to
 * enable this module to be compiled under strict ansi c standards.
 */
static  char dummy, *_dummy_addr = &dummy;    
#endif /* RPC_DG_LOSSY */
