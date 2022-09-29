/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: dgutl.c,v 65.7 1998/03/24 16:00:10 lmc Exp $";
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
 * $Log: dgutl.c,v $
 * Revision 65.7  1998/03/24 16:00:10  lmc
 * Fix some of the #ifdef's for SGI_RPC_DEBUG.  Also typecast a few
 * variables to fix warnings.
 *
 * Revision 65.6  1998/03/23  17:28:53  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.5  1998/03/21 19:15:07  lmc
 * Changed the use of "#ifdef DEBUG" to "defined(SGIMIPS) &&
 * defined(SGI_RPC_DEBUG)" to turn on debugging in rpc.
 * This allows debug rpc messages to be turned on without
 * getting the kernel debugging that comes with "DEBUG".
 * "DEBUG" can change the size of some kernel data structures.
 *
 * Revision 65.4  1998/01/07  17:21:15  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:57:18  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:17:00  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.428.2  1996/02/18  00:04:10  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:56:33  marty]
 *
 * Revision 1.1.428.1  1995/12/08  00:20:26  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:33 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/1  1995/01/16  19:16 UTC  markar
 * 	Merging Local RPC mods
 * 
 * 	HP revision /main/markar_local_rpc/1  1995/01/05  19:29 UTC  markar
 * 	Implementing Local RPC bypass mechanism
 * 	[1995/12/07  23:59:23  root]
 * 
 * Revision 1.1.426.4  1994/06/21  21:54:06  hopkins
 * 	More serviceability
 * 	[1994/06/08  21:32:20  hopkins]
 * 
 * Revision 1.1.426.3  1994/01/24  18:34:22  cbrooks
 * 	Fixed typos
 * 	[1994/01/24  18:32:53  cbrooks]
 * 
 * Revision 1.1.426.2  1994/01/21  22:58:13  cbrooks
 * 	Code Cleanup
 * 	[1994/01/21  22:57:03  cbrooks]
 * 
 * Revision 1.1.426.1  1994/01/21  22:37:32  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:57:41  cbrooks]
 * 
 * Revision 1.1.3.4  1993/01/13  18:11:35  mishkin
 * 	Clear 2nd header flags in rpc__dg_xmit_error_body_pkt() and
 * 	rpc__dg_xmit_hdr_only_pkt().  (Hygiene.)
 * 	[1992/12/11  22:07:08  mishkin]
 * 
 * Revision 1.1.3.3  1993/01/03  23:25:05  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:06:38  bbelch]
 * 
 * Revision 1.1.3.2  1992/12/23  20:50:02  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:38:23  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:10:31  devrcs
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
**      dgutl.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Utility routines for the NCA RPC datagram protocol implementation.
**
**
*/

#include <dg.h>
#include <dgrq.h>
#include <dgxq.h>

/* ========================================================================= */

#ifdef RPC_DG_PLOG

    /*
     * Stuff for the packet log.
     */

#ifndef RPC_C_DG_PKT_LOG_SIZE
#  define RPC_C_DG_PKT_LOG_SIZE 256
#endif

#define MAX_LOGGED_PKT_BODY_LEN 16

typedef struct {
    rpc_clock_t         timestamp;
    unsigned32          lossy_action;
    rpc_dg_raw_pkt_hdr_t hdr;
    byte_t              body[MAX_LOGGED_PKT_BODY_LEN];
} pktlog_elt_t, *pktlog_elt_p_t;

/*
 * Don't make the pkt log buffer INTERNAL (static) we want it to show
 * up in the symbol table.
 */

PRIVATE pktlog_elt_p_t rpc_g_dg_pkt_log;
PRIVATE unsigned32 rpc_g_dg_pkt_log_bytes;

#ifdef STATIC_DG_PKTLOG
INTERNAL pktlog_elt_t _pkt_log[RPC_C_DG_PKT_LOG_SIZE];
#endif

INTERNAL unsigned16 pkt_log_index;

#endif

/* ========================================================================= */

#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG))

/*
 * Buffer for "rpc__dg_act_seq_string".
 *
 * No lock requirements.
 */

INTERNAL char act_seq_string_buff[64]; 

#ifndef NO_SPRINTF
#  define RPC__DG_ACT_SEQ_SPRINTF   sprintf
#else
#  define RPC__DG_ACT_SEQ_SPRINTF   rpc__dg_act_seq_sprintf
#endif

#endif /* _D_E_B_U_G */

/* ========================================================================= */

#ifdef RPC_DG_PLOG

/*
 * R P C _ _ D G _ P L O G _ P K T
 *
 * Add a pkt to the pkt log.  Save only the first MAX_LOGGED_PKT_BODY_LEN
 * bytes of the pkt body.
 */

PRIVATE void rpc__dg_plog_pkt
#ifdef _DCE_PROTO_
(
    rpc_dg_raw_pkt_hdr_p_t hdrp,
    rpc_dg_pkt_body_p_t bodyp,
    boolean32 recv,
    unsigned32 lossy_action     /* (0)drop, (1)?, (2)rexmit, (3)normal */
)
#else
(hdrp, bodyp, recv, lossy_action)
rpc_dg_raw_pkt_hdr_p_t hdrp;
rpc_dg_pkt_body_p_t bodyp;
boolean32 recv;
unsigned32 lossy_action;    /* (0)drop, (1)?, (2)rexmit, (3)normal */
#endif
{
    pktlog_elt_p_t pp;

    if (rpc_g_dg_pkt_log == NULL)
    {
        rpc_g_dg_pkt_log_bytes = RPC_C_DG_PKT_LOG_SIZE * sizeof(pktlog_elt_t);
#ifdef STATIC_DG_PKTLOG
        rpc_g_dg_pkt_log = _pkt_log;
#else
        RPC_MEM_ALLOC(rpc_g_dg_pkt_log, pktlog_elt_p_t,
                rpc_g_dg_pkt_log_bytes,
                RPC_C_MEM_DG_PKTLOG, RPC_C_MEM_NOWAIT);
        if (rpc_g_dg_pkt_log == NULL)
            return;
        /* b_z_e_r_o_(rpc_g_dg_pkt_log, rpc_g_dg_pkt_log_bytes);*/
        memset(rpc_g_dg_pkt_log, 0, rpc_g_dg_pkt_log_bytes);
#endif
    }

    pp = &rpc_g_dg_pkt_log[pkt_log_index];

    pkt_log_index = (pkt_log_index + 1) % RPC_C_DG_PKT_LOG_SIZE;

    pp->timestamp = rpc__clock_stamp();
    pp->lossy_action = lossy_action;

    /*b_c_o_p_y_(hdrp, &pp->hdr, sizeof(rpc_dg_raw_pkt_hdr_t));*/
    memmove( &pp->hdr, hdrp, sizeof(rpc_dg_raw_pkt_hdr_t));

#ifndef MISPACKED_HDR
    /* b_c_o_p_y_ (bodyp, ((char *) pp->body), 
          MIN(MAX_LOGGED_PKT_BODY_LEN, ((rpc_dg_pkt_hdr_p_t) hdrp)->len));*/

    memmove( pp->body, bodyp, 
          MIN(MAX_LOGGED_PKT_BODY_LEN, ((rpc_dg_pkt_hdr_p_t) hdrp)->len)) ;
#else

/* b_c_o_p_y(bodyp, ((char *) pp->body), 
       MIN(MAX_LOGGED_PKT_BODY_LEN, ...extracted from raw hdr... hdrp->len));*/

    memmove( pp->body, bodyp, 
          MIN(MAX_LOGGED_PKT_BODY_LEN, ...extracted from raw hdr... hdrp->len));
#endif

    if (recv)
#ifndef MISPACKED_HDR
        ((rpc_dg_pkt_hdr_p_t)(&pp->hdr))->_rpc_vers |= 0x80;
#else
        set bit in raw hdr
#endif
}


/*
 * R P C _ _ D G _ P L O G _ D U M P
 *
 * Dump the packet log.
 */

void (*rpc__dg_plog_dump_addr)(void) = rpc__dg_plog_dump; /* !!! dde hack */

PRIVATE void rpc__dg_plog_dump(void)
{
    unsigned16 i;
    unsigned32 st;
    static char *lossy_action = "d?r "; /* (0)drop, (1)?, (2)rexmit, (3)normal */

    RPC_LOCK_ASSERT(0);

    if (rpc_g_dg_pkt_log == NULL)
    {
	RPC_DBG_PRINTF(rpc_e_dbg_dg_pktlog, 1,
            ("rpc__dg_plog_dump called, but DG Pkt Logging never enabled\n") );
        return;
    }

    RPC_DBG_PRINTF(rpc_e_dbg_dg_pktlog, 1,
	("tstamp   ver ptyp f1 f2     seq/fnum/sn    ihnt ahnt  len              interface/ver/op                            activity                  sboot                    object              drep   at\n") );
    RPC_DBG_PRINTF(rpc_e_dbg_dg_pktlog, 1,
	("---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n") );

    for (i = 0; i < RPC_C_DG_PKT_LOG_SIZE; i++)
    {
        pktlog_elt_p_t p = &rpc_g_dg_pkt_log[i];
        unsigned_char_p_t obj, iface, act;
        rpc_dg_pkt_hdr_p_t hdrp;

#ifndef MISPACKED_HDR
        hdrp = (rpc_dg_pkt_hdr_p_t) &p->hdr;
#else
        hdrp = converted local rep of raw hdr
#endif

        if (p->timestamp == 0)
            break;

        uuid_to_string(&hdrp->object, &obj, &st);
        uuid_to_string(&hdrp->if_id, &iface, &st);
        uuid_to_string(&hdrp->actuid, &act, &st);

        RPC_DBG_PRINTF(rpc_e_dbg_dg_pktlog, 1,
	    ("%08x %c%c%1u %-4.4s %02x %02x %08x/%04x/%04x %04x %04x %4d %s/%02u/%03u %s %9u %s %02x%02x%02x %02x\n",
                p->timestamp,
                (hdrp->_rpc_vers & 0x80) ? 'R' : lossy_action[p->lossy_action],
                ((i + 1) % RPC_C_DG_PKT_LOG_SIZE == pkt_log_index) ? '*' : ' ',
                hdrp->_rpc_vers & 0x7f, 
                rpc__dg_pkt_name(RPC_DG_HDR_INQ_PTYPE(hdrp)),
                hdrp->flags, hdrp->flags2,
                hdrp->seq, hdrp->fragnum,
                hdrp->serial_hi << 8 | hdrp->serial_lo,
                hdrp->ihint, hdrp->ahint,
                hdrp->len, 
                iface, hdrp->if_vers, hdrp->opnum, act, 
                hdrp->server_boot, obj, 
                hdrp->drep[0], hdrp->drep[1], hdrp->drep[2], 
                hdrp->auth_proto) );

        rpc_string_free(&obj, &st);
        rpc_string_free(&act, &st);
        rpc_string_free(&iface, &st);
    }
}

#endif


/*
 * R P C _ _ D G _ A C T _ S E Q _ S T R I N G
 *
 * Return a pointer to a printed packet's activity-UID/seq/fragnum string.
 *
 * No lock requirements.
 */

PRIVATE char *rpc__dg_act_seq_string
#ifdef _DCE_PROTO_
(
    rpc_dg_pkt_hdr_p_t hdrp
)
#else
(hdrp)
rpc_dg_pkt_hdr_p_t hdrp;
#endif
{
#if !defined(SGI_RPC_DEBUG) && !defined(DEBUG)

    return("");

#else
    extern int RPC__DG_ACT_SEQ_SPRINTF(char *, const char *, ...);

    RPC__DG_ACT_SEQ_SPRINTF(act_seq_string_buff, "%s, %lu.%u",
            rpc__uuid_string(&hdrp->actuid), hdrp->seq, hdrp->fragnum);
    return(act_seq_string_buff);

#endif /* D_E_B_U_G_ */
}


/*
 * R P C _ _ D G _ P K T _ N A M E  
 *
 * Return the string name for a type of packet.  This can't simply be
 * a variable because of the vagaries of global libraries.
 *
 * No lock requirements.
 */

PRIVATE char *rpc__dg_pkt_name
#ifdef _DCE_PROTO_
(
    rpc_dg_ptype_t ptype
)
#else
(ptype)
rpc_dg_ptype_t ptype;
#endif
{
#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG))

    return("");

#else

    static char *names[RPC_C_DG_PT_MAX_TYPE + 1] = {
        "request",
        "ping",
        "response",
        "fault",
        "working",
        "nocall",
        "reject",
        "ack",
        "quit",
        "fack",
        "quack"
    };

    return((int) ptype > RPC_C_DG_PT_MAX_TYPE ? "BOGUS PACKET TYPE" : names[(int) ptype]);

#endif /* D_E_B_U_G_ */
}


/*
 * R P C _ _ D G _ X M I T _ P K T
 *
 * Send the packet described by the iov to the specified addr using the
 * specified socket.  
 *
 * This is intended as a utility routine for non-critical processing.
 *
 * The DG packet hdr is always iov[0] and may require MISPACKED_HDR
 * processing;  iov[1..(iovlen-1)] are optional.  Note that iov[0] might
 * be a pointer to an "rpc_dg_pkt_hdr_t" or an "rpc_dg_raw_pkt_hdr_t".
 * On a non-MISPACKED_HDR machine, there's no difference between these
 * two structures so there's nothing special we have to do.  On a
 * MISPACKED_HDR machine, we can tell which kind was passed by looking
 * at iov[0].len -- if it's the size of a raw packet header, iov[0].base
 * points to a raw packet header and should be left alone; otherwise
 * it's the non-raw flavor and must be compressed.
 * 
 * The output boolean is set according to the success of sendmsg.
 */

PRIVATE void rpc__dg_xmit_pkt
#ifdef _DCE_PROTO_
(
    rpc_socket_t sock,
    rpc_addr_p_t addr,
    rpc_socket_iovec_p_t iov,
    int iovlen,
    boolean *b
)
#else
(sock, addr, iov, iovlen, b)
rpc_socket_t sock;
rpc_addr_p_t addr;
rpc_socket_iovec_p_t iov;
int iovlen;
boolean *b;
#endif
{
#ifdef MISPACKED_HDR
    rpc_dg_raw_pkt_hdr_t raw_hdr;
    rpc_dg_pkt_hdr_p_t hdrp;
#endif
    rpc_socket_error_t serr;
    int sendcc = 0;
    int sentcc;
    int i;

#ifdef MISPACKED_HDR
    if (iov[0].len != RPC_C_DG_RAW_PKT_HDR_SIZE)
    {
        /* !!! ...compress hdr pointed to by iov[0] into raw_hdr... !!! */
        hdrp = (iov)[0].base;
        compress_hdr(hdrp, &raw_hdr);
        (iov)[0].base = (caddr_t) &raw_hdr;
    }
#endif

    for (i = 0; i < iovlen; i++)
        sendcc += iov[i].len;

    *b = true;
    RPC_DG_SOCKET_SENDMSG_OOL(sock, iov, iovlen, addr, &sentcc, &serr);
    if (RPC_SOCKET_IS_ERR(serr) || sentcc != sendcc) 
    {
        RPC_DBG_GPRINTF(("(rpc__dg_xmit_pkt) sendmsg failed, sendcc = %d, sentcc = %d, error = %d\n", 
            sendcc, sentcc, RPC_SOCKET_ETOI(serr)));
        *b = false;
    }

    RPC_DG_STATS_INCR(pkts_sent);
    RPC_DG_STATS_INCR(pstats[RPC_DG_HDR_INQ_PTYPE(((rpc_dg_pkt_hdr_p_t) iov[0].base))].sent);

#ifdef MISPACKED_HDR
    iov[0].base = hdrp;
#endif
}


/*
 * R P C _ _ D G _ X M I T _ H D R _ O N L Y _ P K T
 *
 * Send a header only packet of the specified packet type.  Create a
 * new packet header using the provided prototype header.  This is intended
 * as a utility routine for non-critical processing.
 */

PRIVATE void rpc__dg_xmit_hdr_only_pkt
#ifdef _DCE_PROTO_
(
    rpc_socket_t sock,
    rpc_addr_p_t addr,
    rpc_dg_pkt_hdr_p_t hdrp,
    rpc_dg_ptype_t ptype
)
#else
(sock, addr, hdrp, ptype)
rpc_socket_t sock;
rpc_addr_p_t addr;
rpc_dg_pkt_hdr_p_t hdrp;
rpc_dg_ptype_t ptype;
#endif
{
    rpc_socket_iovec_t iov[1];
    rpc_dg_pkt_hdr_t hdr;
    boolean b;

    /*
     * Create a pkt header initialized with the prototype's contents.
     */

    hdr = *hdrp;

    RPC_DG_HDR_SET_VERS(&hdr);
    RPC_DG_HDR_SET_PTYPE(&hdr, ptype);
    RPC_DG_HDR_SET_DREP(&hdr);

    hdr.flags       = 0;
    hdr.flags2      = 0;
    hdr.len         = 0;

    /*
     * Setup the iov and send the packet.
     */

    iov[0].base = (byte_p_t) &hdr;
    iov[0].len  = RPC_C_DG_RAW_PKT_HDR_SIZE;

    rpc__dg_xmit_pkt(sock, addr, iov, 1, &b);
}


/*
 * R P C _ _ D G _ X M I T _ E R R O R _ B O D Y _ P K T
 *
 * Transmit a packet that only has a packet header and a error status
 * body.  Create a new packet header using the provided prototype header.
 * This routine deals handles any necessary MISPACKED_HDR processing
 * for the error status body.
 */

PRIVATE void rpc__dg_xmit_error_body_pkt
#ifdef _DCE_PROTO_
(
    rpc_socket_t sock,
    rpc_addr_p_t addr,
    rpc_dg_pkt_hdr_p_t hdrp,
    rpc_dg_ptype_t ptype,
    unsigned32 errst
)
#else
(sock, addr, hdrp, ptype, errst)
rpc_socket_t sock;
rpc_addr_p_t addr;
rpc_dg_pkt_hdr_p_t hdrp;
rpc_dg_ptype_t ptype;
unsigned32 errst;
#endif
{
    rpc_socket_iovec_t iov[2];
    rpc_dg_pkt_hdr_t hdr;
#ifndef MISPACKED_HDR
    rpc_dg_epkt_body_t body;
#else
    rpc_dg_raw_epkt_body_t body;
#endif
    boolean b;

    /*
     * Create a pkt header initialized with the prototype's contents.
     */

    hdr = *hdrp;

    RPC_DG_HDR_SET_VERS(&hdr);
    RPC_DG_HDR_SET_PTYPE(&hdr, ptype);
    RPC_DG_HDR_SET_DREP(&hdr);

    hdr.flags       = 0;
    hdr.flags2      = 0;
    hdr.len         = RPC_C_DG_RAW_EPKT_BODY_SIZE;


    /*
     * Create the error body packet's body.
     */

#ifndef MISPACKED_HDR
    body.st = errst;
#else
#error "extract and pack the 32 bit status code into the body" /*!!! */
#endif

    /*
     * Setup the iov and send the packet.
     */

    iov[0].base = (byte_p_t) &hdr;
    iov[0].len  = RPC_C_DG_RAW_PKT_HDR_SIZE;
    iov[1].base = (byte_p_t) &body;
    iov[1].len  = hdr.len;

    rpc__dg_xmit_pkt(sock, addr, iov, 2, &b);

    RPC_DBG_GPRINTF(("(rpc__dg_xmit_call_error_body_pkt) \"%s\" - st 0x%x sent\n",
        rpc__dg_pkt_name(ptype), errst));
}


/*
 * R P C _ _ D G _ M G M T _ I N Q _ C A L L S _ S E N T
 *
 */

PRIVATE unsigned32 rpc__dg_mgmt_inq_calls_sent(void)
{
    return (rpc_g_dg_stats.calls_sent);
}


/*
 * R P C _ _ D G _ M G M T _ I N Q _ C A L L S _ R C V D
 *
 */

PRIVATE unsigned32 rpc__dg_mgmt_inq_calls_rcvd(void)
{
    return (rpc_g_dg_stats.calls_rcvd);
}


/*
 * R P C _ _ D G _ M G M T _ I N Q _ P K T S _ S E N T
 *
 */

PRIVATE unsigned32 rpc__dg_mgmt_inq_pkts_sent(void)
{
    return (rpc_g_dg_stats.pkts_sent);
}


/*
 * R P C _ _ D G _ M G M T _ I N Q _ P K T S _ R C V D
 *
 */

PRIVATE unsigned32 rpc__dg_mgmt_inq_pkts_rcvd(void)
{
    return (rpc_g_dg_stats.pkts_rcvd);
}


#ifndef _KERNEL

/*
 * R P C _ _ D G _ S T A T S _ P R I N T
 *
 */

PRIVATE void rpc__dg_stats_print(void)
{
    unsigned16 i;

    RPC_DBG_PRINTF(rpc_e_dbg_stats, 1,
	("RPC DG Protocol Statistics\n") );
    RPC_DBG_PRINTF(rpc_e_dbg_stats, 1,
	("--------------------------------------------------------\n") );
    RPC_DBG_PRINTF(rpc_e_dbg_stats, 1,
	("Calls sent:            %9lu\n", rpc_g_dg_stats.calls_sent) );
    RPC_DBG_PRINTF(rpc_e_dbg_stats, 1,
	("Calls rcvd:            %9lu\n", rpc_g_dg_stats.calls_rcvd) );
    RPC_DBG_PRINTF(rpc_e_dbg_stats, 1,
	("Pkts sent:             %9lu\n", rpc_g_dg_stats.pkts_sent) );
    RPC_DBG_PRINTF(rpc_e_dbg_stats, 1,
	("Pkts rcvd:             %9lu\n", rpc_g_dg_stats.pkts_rcvd) );
    RPC_DBG_PRINTF(rpc_e_dbg_stats, 1,
	("Broadcasts sent:       %9lu\n", rpc_g_dg_stats.brds_sent) );
    RPC_DBG_PRINTF(rpc_e_dbg_stats, 1,
	("Dups sent:             %9lu\n", rpc_g_dg_stats.dups_sent) );
    RPC_DBG_PRINTF(rpc_e_dbg_stats, 1,
	("Dups rcvd:             %9lu\n", rpc_g_dg_stats.dups_rcvd) );
    RPC_DBG_PRINTF(rpc_e_dbg_stats, 1,
	("Out of orders rcvd:    %9lu\n", rpc_g_dg_stats.oo_rcvd) );

    RPC_DBG_PRINTF(rpc_e_dbg_stats, 1,
	("\nBreakdown by packet type               sent            rcvd\n") );
    RPC_DBG_PRINTF(rpc_e_dbg_stats, 1,
	("------------------------------------------------------------------\n") );

    for (i = 0; i <= RPC_C_DG_PT_MAX_TYPE; i++)
    {
        RPC_DBG_PRINTF(rpc_e_dbg_stats, 1,
	    ("(%02u) %-10s                   %9lu             %9lu\n",
                i, rpc__dg_pkt_name(i),
                rpc_g_dg_stats.pstats[i].sent, 
                rpc_g_dg_stats.pstats[i].rcvd) );
    }
}

#endif /* ! _KERNEL */


/*
 * R P C _ _ D G _ U U I D _ H A S H
 *
 * A significantly faster (though likely not as good) version of uuid_hash() 
 * (this is the NCS 1.5.1 implementation).
 */

PRIVATE unsigned16 rpc__dg_uuid_hash
#ifdef _DCE_PROTO_
(
    uuid_p_t uuid
)
#else
(uuid)
uuid_p_t uuid;
#endif
{
    unsigned32 *lp = (unsigned32 *) uuid;
    unsigned32 hash = lp[0] ^ lp[1] ^ lp[2] ^ lp[3];

    return ((hash >> 16) ^ hash);
}
