/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: rpcmem_krpc.h,v $
 * Revision 65.2  1997/10/20 19:16:28  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.97.2  1996/02/18  23:46:48  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:45:27  marty]
 *
 * Revision 1.1.97.1  1995/12/08  00:15:26  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:56:29  root]
 * 
 * Revision 1.1.95.4  1994/03/18  18:33:03  tom
 * 	Add protocol_version type to support protocol version in binding handle.
 * 	[1994/03/18  16:32:35  tom]
 * 
 * Revision 1.1.95.3  1994/02/02  21:49:04  cbrooks
 * 	OT9855 code cleanup breaks KRPC
 * 	[1994/02/02  21:00:26  cbrooks]
 * 
 * Revision 1.1.95.2  1994/01/28  23:09:17  burati
 * 	rpc_c_mem_dg_epac changed to uppercase for code cleanup
 * 	[1994/01/27  18:24:19  burati]
 * 
 * 	Big EPAC changes - add rpc_c_mem_dg_epac (dlg_bl1)
 * 	[1994/01/24  20:59:24  burati]
 * 
 * Revision 1.1.95.1  1994/01/21  22:32:27  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:37  cbrooks]
 * 
 * Revision 1.1.7.1  1993/09/28  21:57:59  weisman
 * 	Changes for endpoint restriction
 * 	[1993/09/28  18:18:29  weisman]
 * 
 * Revision 1.1.4.3  1993/01/03  22:36:51  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:53:26  bbelch]
 * 
 * Revision 1.1.4.2  1992/12/23  19:40:13  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:54:11  zeliff]
 * 
 * Revision 1.1.2.2  1992/05/01  17:56:35  rsalz
 * 	"Changed as part of rpc6 drop."
 * 	[1992/05/01  17:51:08  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:15:14  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/
#ifndef _RPCMEM_KRPC_H
#define _RPCMEM_KRPC_H	1

/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**
**  NAME:
**
**      rpcmem_krpc.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Various macros and data for runtime memory allocation in a kernel RPC
**  environment.
**
**
*/


/*
 * Memory Allocation.
 *
 * The runtime dynamically allocates several classes of objects, each
 * with differing sizes and lifetimes.  There are a few structures that
 * are allocated in the "critical path" performance wise.  While the
 * runtime will likely be caching some of these structures, the performance
 * of the memory allocator should be of some concern.
 * 
 * Note that some structures are allocated by the listener task.  In
 * a kernel environment where the listener is implemented as a network
 * software interrupt handler, the memory allocators must be capable
 * of being called from an interrupt handler.  Additionally, the interface
 * we use allows one to specify blocking or non-blocking allocations,
 * non-blocking mode being required for interrupt level operations (which
 * obviously must be prepared to deal with a "no memory available"
 * indication).
 * 
 * We want to maintain a flexible interface to cope with the variety
 * of environments that this code may be ported to and to provide us
 * with hooks for determining how memory is being used.  The interface
 * we create and use is the same as that of OSF/1 NET_MALLOC (which is
 * like BSD4.4 kernel malloc).  We can always make RPC_MEM_ALLOC()
 * "smarter" and maintain a pool for various objects including a fast
 * allocation / free scheme layered on top of malloc(), etc.
 * 
 * If calling the allocator in a blocking mode (RPC_MEM_WAITOK), one
 * should be prepaired that the process may yield the processor (i.e.
 * make sure that you aren't violating the runtime's mutex lock rules).
 *
 * See the comments below about USE_ALTERNATE_RPC_MEM_MACROS for tailoring
 * this for your system.
 */

#define RPC_C_MEM_FL    0x8000      /* fixed len object flag */

#define RPC_MEM_FL_OBJ(objid)       ((objid) | RPC_C_MEM_FL)
#define RPC_MEM_VL_OBJ(objid)       ((objid))

#define RPC_MEM_IS_FL(obj)          ((obj) & RPC_C_MEM_FL)
#define RPC_MEM_OBJ_ID(obj)         ((obj) & ~RPC_C_MEM_FL)


/*
 * Types of RPC memory objects that we allocate.
 */

#define RPC_C_MEM_AVAIL                            0
#define RPC_C_MEM_DG_CHAND          RPC_MEM_FL_OBJ(1)   /* rpc_dg_handle_client_t */
#define RPC_C_MEM_DG_SHAND          RPC_MEM_FL_OBJ(2)   /* rpc_dg_handle_server_t */
#define RPC_C_MEM_DG_CCALL          RPC_MEM_FL_OBJ(3)   /* rpc_dg_ccall_t */
#define RPC_C_MEM_DG_SCALL          RPC_MEM_FL_OBJ(4)   /* rpc_dg_scall_t */
#define RPC_C_MEM_DG_PKT_POOL_ELT   RPC_MEM_FL_OBJ(5)   /* rpc_dg_pkt_pool_t */
#define RPC_C_MEM_DG_SELACK_MASK    RPC_MEM_VL_OBJ(6)   /* selective ack mask array */
#define RPC_C_MEM_DG_RAWPKT         RPC_MEM_FL_OBJ(7)   /* rpc_dg_raw_pkt_t */
#define RPC_C_MEM_DG_PKTBODY        RPC_MEM_FL_OBJ(8)   /* rpc_dg_pkt_body_t */
#define RPC_C_MEM_DG_PKTLOG         RPC_MEM_FL_OBJ(9)   /* from dg pktlog() */
#define RPC_C_MEM_DG_CCTE          RPC_MEM_FL_OBJ(10)   /* rpc_dg_cct_elt_t */
#define RPC_C_MEM_DG_SCTE          RPC_MEM_FL_OBJ(11)   /* rpc_dg_sct_elt_t */
#define RPC_C_MEM_IF_RGY_ENTRY     RPC_MEM_FL_OBJ(12)   /* rpc_if_info_t */
#define RPC_C_MEM_UTIL             RPC_MEM_VL_OBJ(13)   /* from rpc_util_malloc() */
#define RPC_C_MEM_SOCK_INFO        RPC_MEM_FL_OBJ(14)   /* rpc_sock_info_t */
#define RPC_C_MEM_IOVE_LIST        RPC_MEM_FL_OBJ(15)   /* rpc_iove_list_elt_t */
#define RPC_C_MEM_DG_LOSSY         RPC_MEM_VL_OBJ(16)   /* misc stuff for dglossy.c */
#define RPC_C_MEM_V1_PKTBODY       RPC_MEM_FL_OBJ(17)   /* pkt buff rpc__pre_v2_iface_server_call */
#define RPC_C_MEM_V1_STUB          RPC_MEM_VL_OBJ(18)   /* rpc1_{alloc,free}_pkt() */
#define RPC_C_MEM_IF_TYPE_INFO     RPC_MEM_FL_OBJ(19)   /* rpc_if_type_info_t */
#define RPC_C_MEM_PROTSEQ          RPC_MEM_FL_OBJ(20)   /* rpc_protseq_t */
#define RPC_C_MEM_RPC_ADDR         RPC_MEM_VL_OBJ(21)   /* rpc_addr + naf data */
#define RPC_C_MEM_DG_CLIENT_REP    RPC_MEM_FL_OBJ(22)   /* rpc_dg_client_rep_t */
#define RPC_C_MEM_DG_MAINT         RPC_MEM_FL_OBJ(23)   /* maintain liveness structure */
#define RPC_C_MEM_STRING_BINDING   RPC_MEM_FL_OBJ(24)   /* rpc_string_binding_t */
#define RPC_C_MEM_STRING_UUID      RPC_MEM_FL_OBJ(25)   /* no real type - uuid.idl */
#define RPC_C_MEM_ENDPOINT         RPC_MEM_FL_OBJ(26)   /* rpc_endpoint_t */
#define RPC_C_MEM_NETADDR          RPC_MEM_FL_OBJ(27)   /* rpc_netaddr_t */
#define RPC_C_MEM_NETWORK_OPTIONS  RPC_MEM_FL_OBJ(28)   /* rpc_network_options_t */
#define RPC_C_MEM_PROTSEQ_VEC      RPC_MEM_VL_OBJ(29)   /* rpc_protseq_vector_t */
#define RPC_C_MEM_BINDING_VEC      RPC_MEM_VL_OBJ(30)   /* rpc_binding_vector_t */
#define RPC_C_MEM_PTHREAD_CTX      RPC_MEM_FL_OBJ(31)   /* pthread_thread_ctx_t */
#define RPC_C_MEM_RPC_ADDR_VEC     RPC_MEM_FL_OBJ(32)   /* rpc_addr + naf data */
#define RPC_C_MEM_IF_ID            RPC_MEM_FL_OBJ(33)   /* rpc_if_id_t */
#define RPC_C_MEM_IF_ID_VECTOR     RPC_MEM_FL_OBJ(34)   /* rpc_id_id_vector_t */
#define RPC_C_MEM_PROTSEQ_VECTOR   RPC_MEM_FL_OBJ(35)   /* rpc_protseq_vector_t         */
#define RPC_C_MEM_STRING           RPC_MEM_VL_OBJ(36)   /* various string types         */
#define RPC_C_MEM_MGR_EPV          RPC_MEM_VL_OBJ(37)   /* manager epv                  */
#define RPC_C_MEM_OBJ_RGY_ENTRY    RPC_MEM_FL_OBJ(38)   /* rpc_obj_rgy_entry_t          */
#define RPC_C_MEM_EP_ENTRY         RPC_MEM_FL_OBJ(39)   /* endpoint mapper struct       */
#define RPC_C_MEM_UUID_VECTOR      RPC_MEM_VL_OBJ(40)   /* uuid_vector_t                */
#define RPC_C_MEM_THREAD_CONTEXT   RPC_MEM_FL_OBJ(41)   /* rpc_thread_context_t         */
#define RPC_C_MEM_JMPBUF_HEAD      RPC_MEM_FL_OBJ(42)   /* pthread_jmpbuf_p_t */
#define RPC_C_MEM_STATS_VECTOR     RPC_MEM_VL_OBJ(43)   /* rpc_stats_vector             */
#define RPC_C_MEM_CTHREAD_POOL     RPC_MEM_FL_OBJ(44)   /* cthread_pool_elt_t           */
#define RPC_C_MEM_CTHREAD_CTBL     RPC_MEM_VL_OBJ(45)   /* cthread_elt_t[]              */
#define RPC_C_MEM_CTHREAD_QETBL    RPC_MEM_VL_OBJ(46)   /* cthread_queue_elt_t[]        */
#define RPC_C_MEM_DG_SOCK_POOL_ELT RPC_MEM_FL_OBJ(47)   /* rpc_dg_sock_pool_elt_t */
#define RPC_C_MEM_PORT_RESTRICT_LIST \
                                   RPC_MEM_FL_OBJ(48)    /* rpc_port_restriction_list_t  */
#define RPC_C_MEM_PORT_RANGE_ELEMENTS \
                                   RPC_MEM_VL_OBJ(49)    /* rpc_port_range_element_t     */
#define RPC_C_MEM_DG_EPAC          RPC_MEM_VL_OBJ(50)    /* used for oversized PACs         */
#define RPC_C_MEM_PROTOCOL_VERSION  \
                                   RPC_MEM_FL_OBJ(51)    /* rpc_protocol_version_t          */

#define RPC_C_MEM_MAX_TYPES        52                   /* i.e. 0 : (max_types - 1) */



typedef struct 
{
    unsigned32 inuse;         /* number currently allocated */
    unsigned32 calls;         /* total ever allocated */
    unsigned32 fails;         /* denied alloc requests */
    unsigned32 tsize;         /* size allocated for this type (max size if var len obj) */
} rpc_mem_stats_elt_t, *rpc_mem_stats_elt_p_t;

EXTERNAL rpc_mem_stats_elt_t rpc_g_mem_stats[];

/*
 * Values for the 'flags' argument to RPC_MEM_ALLOC
 */

#define RPC_C_MEM_WAITOK    0
#define RPC_C_MEM_NOWAIT    1


/*
 * Map the RPC_MEM_ operations to either INLINE or Out-of-line
 * implementations.  The default is Out-of-line, but this can be
 * changed via the system specific configuration file.
 */

#ifdef RPC_MEM_DEFAULT_INLINE
#  define RPC_MEM_ALLOC(addr, cast, size, type, flags) \
        RPC_MEM_ALLOC_IL(addr, cast, size, type, flags)

#  define RPC_MEM_REALLOC(addr, cast, size, type, flags) \
        RPC_MEM_REALLOC_IL(addr, cast, size, type, flags)

#  define RPC_MEM_FREE(addr, type) \
        RPC_MEM_FREE_IL(addr, type)
#else
#  define RPC_MEM_ALLOC(addr, cast, size, type, flags) \
        (addr) = (cast) rpc__mem_alloc(size, type, flags)

#  define RPC_MEM_REALLOC(addr, cast, size, type, flags) \
        (addr) = (cast) rpc__mem_realloc(size, type, flags)

#  define RPC_MEM_FREE(addr, type) \
        rpc__mem_free((pointer_t)(addr), type)
#endif /* RPC_MEM_DEFAULT_INLINE */

/*
 * NOTE: sysconf.h includes a system specific file for the memory allocation
 * routines.
 *
 * The default macros use kalloc(size) and kfree(addr,size).  If your system 
 * doesn't have these, either:
 *      a) modify your sysconf.h to redefine kalloc/kfree to something else
 *          (and continue to use the default macros)
 *      b) modify your sysconf.h to define USE_ALTERNATE_RPC_MEM_MACROS
 *          and provide an alternate implementation of the following macros
 *          there.
 *      c) provide a platform specific implementation/wrapper of kalloc/kfree
 *          elsewhere; (and continue to use the default macros)
 */

#ifndef USE_ALTERNATE_RPC_MEM_MACROS

/* Define a "rpc_mem_kchunk" to save the length while preserving alignment */
typedef struct 
{ 
    unsigned32 len; 
#ifdef	SGIMIPS

#if _MIPS_SZLONG == 64
    unsigned32 reserved; /* 64-bit alignment on banyan */
#endif

#endif
    /* data follows */ 
} rpc_mem_kchunk;

/*
 * R P C _ M E M _ A L L O C _ I L
 * 
 * (addr) == NULL iff "no memory available"
 *
 * Sample usage:
 *      rpc_dg_ccall_p_t ccall;
 *      RPC_MEM_ALLOC(ccall, rpc_dg_ccall_p_t, sizeof *rpc_dg_ccall_p_t, 
 *              rpc_c_mem_dg_ccall, rpc_c_mem_nowait);
 *      if (ccall == NULL)
 *          alloc failed
 *
 * Note that we just die if the alloc fails (since most callers don't
 * yet check the return value).
 *
 * For now, we'll count on the compiler being smart enough to detect
 * the compile time constant expression for fixed / var len checks
 * (for code size and execution speed).
 *
 * For now, everything is done at non-interrupt level, so ignore
 * the (possibly inaccurate) flags and always do a blocking alloc.
 *
 * The objects fetched by RPC_MEM_ALLOC are of variable length,
 * but are largely < 256 bytes. Native zones may replace the kalloc
 * facility. Mbufs always come from native blocks (clusters).
 */

#define RPC_MEM_ALLOC_IL(addr, cast, size, type, flags) \
{ \
    rpc_g_mem_stats[RPC_MEM_OBJ_ID(type)].calls++; \
    if (RPC_MEM_IS_FL(type)) { \
        if (rpc_g_mem_stats[RPC_MEM_OBJ_ID(type)].tsize == 0) \
            rpc_g_mem_stats[RPC_MEM_OBJ_ID(type)].tsize = size; \
        else if (rpc_g_mem_stats[RPC_MEM_OBJ_ID(type)].tsize != size) \
            DIE("RPC_MEM_ALLOC - not fixed len"); \
        (addr) = (cast) KALLOC(size); \
    } else { \
        if ((size) > rpc_g_mem_stats[RPC_MEM_OBJ_ID(type)].tsize) \
            rpc_g_mem_stats[RPC_MEM_OBJ_ID(type)].tsize = (size); \
        (addr) = (cast) KALLOC((size)+sizeof(rpc_mem_kchunk)); \
        ((rpc_mem_kchunk *)(addr))->len = ((size)+sizeof(rpc_mem_kchunk)); \
        (addr) = (cast)(((rpc_mem_kchunk *)(addr))+1); \
    } \
    if (addr) { \
        rpc_g_mem_stats[RPC_MEM_OBJ_ID(type)].inuse++; \
    } else { \
        rpc_g_mem_stats[RPC_MEM_OBJ_ID(type)].fails++; \
        DIE("(RPC_MEM_ALLOC)"); \
    } \
}


/*
 * R P C _ M E M _ F R E E _ I L
 *
 * Sample useage:
 *      ...
 *      RPC_MEM_FREE(ccall, rpc_c_mem_dg_ccall);
 */

#define RPC_MEM_FREE_IL(addr, type) \
{ \
    if (RPC_MEM_IS_FL(type)) \
        KFREE((caddr_t)(addr), rpc_g_mem_stats[RPC_MEM_OBJ_ID(type)].tsize); \
    else \
        KFREE((caddr_t)(((rpc_mem_kchunk *)(addr))-1), (((rpc_mem_kchunk *)(addr))-1)->len); \
    --rpc_g_mem_stats[RPC_MEM_OBJ_ID(type)].inuse; \
}


/*
 * R P C _ M E M _ R E A L L O C _ I L
 * 
 * (addr) == NULL iff "no memory available"
 *
 * Sample usage:
 *      rpc_dg_ccall_p_t ccall;
 *      RPC_MEM_REALLOC(ccall, rpc_dg_ccall_p_t, sizeof *rpc_dg_ccall_p_t, 
 *              rpc_c_mem_dg_ccall, rpc_c_mem_nowait);
 *      if (ccall == NULL)
 *          alloc failed
 *
 * Note that we just raise an exception if the realloc fails (since most
 * callers don't yet check the return value).  In any case, this will
 * probably always be the correct thing to do in a user space NCK
 * implementation).
 *
 * No one really uses this, right?
 */

#define RPC_MEM_REALLOC_IL(addr, cast, size, type, flags) \
{ \
    panic("RPC_MEM_REALLOC_IL"); \
}


#endif /* USE_ALTERNATE_RPC_MEM_MACROS */


EXTERNAL pointer_t rpc__mem_alloc _DCE_PROTOTYPE_ ((
        unsigned32 /* size */, 
        unsigned32 /* type */, 
        unsigned32 /* flags */
    ));

EXTERNAL pointer_t rpc__mem_realloc _DCE_PROTOTYPE_ ((
        pointer_t  /* addr */,
        unsigned32 /* size */, 
        unsigned32 /* type */, 
        unsigned32 /* flags */
    ));

EXTERNAL void rpc__mem_free  _DCE_PROTOTYPE_ ((
        pointer_t /* addr */,
        unsigned32 /* type */
    ));

#endif /* _RPCMEM_KRPC_H */
