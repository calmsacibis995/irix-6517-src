/**************************************************************************
 *                                                                        *
 *                Copyright (C) 1995, Silicon Graphics, Inc.              *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.3 $"
#ifdef __cplusplus
extern "C" {
#endif

/*
 * Macros for handling TCP/IP overlay. Used only by the kernel.
 * They are in a separate file because they shouldn't be exported
 * to /usr/include. 
 *
 * BSD code reuses the space of TCP/IP headers to avoid dynamic 
 * memory allocation, mainly for linkage pointers.  32-bit and 64-bit
 * kernels have different pointer sizes and need different macros.  
 */

/* 
 * Ip fragments are chained by two pointers. In 32-bit mode, ih_src and 
 * ih_dst are reused as space for these pointers.  In 64-bit mode, the
 * chaining pointers are stored in front of ipasfrag, because there isn't
 * enough free room inside ipasfrag.  ip_input.c:ip_reass_adjm() ensures
 * that the space in front of ipasfrag is safe to use. ipq and ipasfrag are
 * casted to each other.  Hence, all ipq's must also have enough free room 
 * in the front.
 */

#if (_MIPS_SZLONG != 32)
#	define MM  (~(sizeof(void*) - 1))
#	define IPF_NEXT(x) (*((struct ipasfrag**)(((__psint_t)(x) & MM) - 8)))
#	define IPF_PREV(x) (*((struct ipasfrag**)(((__psint_t)(x) & MM) - 16)))
#	define IPQ_NEXT(x) IPF_NEXT(x)
#	define IPQ_PREV(x) IPF_PREV(x)
#else
#	define IPF_NEXT(x) ((x)->ipf_next)
#	define IPF_PREV(x) ((x)->ipf_prev)
#	define IPQ_NEXT(x) ((x)->ipq_next)
#	define IPQ_PREV(x) ((x)->ipq_prev)
#endif

/* 
 * We need two pointers to put a packet on a doubly linked list for TCP
 * reassembly.  Struct ipovly provides space for these two pointers by
 * reusing fields in the IP header. 
 * 
 * The 32-bit kernel assumes that tcpiphdr is 4-byte aligned.  It 
 * reuses the first 8 bytes of tcpiphdr for two pointers.  The 64-bit
 * kernel continues to assume that tcpiphdr is 4-byte aligned.  However, it
 * does not further assume that tcpiphdr is 8-byte aligned; it stores an 
 * 8-byte pointers as two 4-byte words to avoid alignment exceptions.
 */

#if (_MIPS_SZLONG == 32)
#define TCP_NEXT_GET(x) 	((x)->ti_next)
#define TCP_NEXT_PUT(x,v) 	((x)->ti_next) = v
#define TCP_PREV_GET(x) 	((x)->ti_prev)
#define TCP_PREV_PUT(x,v) 	((x)->ti_prev) = v
#else

/* get a 64-bit value of type t from two 32-bit fields */
#define DWORD_GET(f1,f2,t) ((t)((__uint64_t)(*(__uint32_t*)&(f2)) | \
	(__uint64_t)(*(__uint32_t*)&(f1)) << 32))
/* put a 64-bit field into two 32-bit fields */
#define DWORD_PUT(f1,f2,p) {\
	*(__uint32_t*)&(f2) = (__uint32_t)((__uint64_t)(p) & 0xffffffff);\
	*(__uint32_t*)&(f1) = (__uint32_t)((__uint64_t)(p) >> 32 & 0xffffffff);}

/* pointer to next tcpip fragment; reuse ti_next, ti_prev */
#define TCP_NEXT_GET(x) DWORD_GET((x)->ti_next, (x)->ti_prev, struct tcpiphdr *)
#define TCP_NEXT_PUT(x, p) DWORD_PUT((x)->ti_next, (x)->ti_prev, p)

/* pointer to previous tcpip fragment; reuse ti_src, ti_dst*/
#define	TCP_PREV_GET(x) DWORD_GET((x)->ti_src, (x)->ti_dst, struct tcpiphdr *)
#define TCP_PREV_PUT(x, p) DWORD_PUT((x)->ti_src, (x)->ti_dst, p)
#endif

/*
 * We want to avoid doing m_pullup on incoming packets but that
 * means avoiding dtom on the tcp reassembly code.  That in turn means
 * keeping an mbuf pointer in the reassembly queue (since we might
 * have a cluster).  As a quick hack, ti_sport, ti_dport, and ti_ack 
 * (which are no longer needed once we've located the tcpcb) are
 * overlaid with an mbuf pointer.
 */

#if (_MIPS_SZLONG != 32)
#define REASS_MBUF_PUT(ti,m) DWORD_PUT((ti)->ti_sport, (ti)->ti_ack, m)
#define REASS_MBUF_GET(ti) DWORD_GET((ti)->ti_sport, (ti)->ti_ack, struct mbuf*)
#else
#define REASS_MBUF_PUT(ti,m) (ti)->ti_ack = (__uint32_t)(m)
#define REASS_MBUF_GET(ti) ((struct mbuf *)((ti)->ti_ack))
#endif
#ifdef __cplusplus
}
#endif
