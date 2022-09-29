/*
 * Copyright 1996 Silicon Graphics, Inc.  All rights reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */
#include <sys/types.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <ksys/kern_heap.h>
#include "bsd/sys/tcpipstats.h"
#include <sys/mbuf.h>
#include <sys/mbuf_private.h>
#include <sys/socketvar.h>
#include <bsd/sys/hashing.h>
#undef copyin
#undef copyout
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/raw.h>
#include <net/raw_cb.h>
#include <bsd/sys/protosw.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/in_var.h>

#include "bsd/net/if_arp.h"
#include "bsd/netinet/if_ether.h"
#include "sys/unpcb.h"

#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>

#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_fsm.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <sys/vsocket.h>

#ifdef R10000
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>
#endif 

/* #define DEBUG_MBUF_TIMING 1 */
/* #define DEBUG_IPCHKSUM_TIMING 1 */

#define	VD	(void (*)())

/* mbuf per cpu listvector printing procedures */
void	idbg_ckna(__psint_t);
void	idbg_cnetstats(__psint_t);

/* tcp/ip related statistics */
void	idbg_icmpstats(__psint_t);
void	idbg_igmpstats(__psint_t);
void    idbg_inpcbstats(__psint_t);
void	idbg_ipstats(__psint_t);
void	idbg_mrtstats(__psint_t);
void    idbg_pinpcbstat(__psint_t, int);
void    idbg_ginpcbstats(void);
void    idbg_dinpcbstats(void);
void    idbg_pcbhashinfo(__psint_t);
void	idbg_udpstats(__psint_t);
void	idbg_sockstats(void);
void	idbg_tcpstats(__psint_t);

void	idbg_tcpcb(__psint_t);

void	idbg_protosw(__psint_t);

void	idbg_rawif(__psint_t);
void	idbg_rawcb(__psint_t);

void	idbg_hashtable(__psint_t);

void	idbg_ifaddr(__psint_t);
void	idbg_ifaddrlist(__psint_t);

void	idbg_ifnet(__psint_t);
void	idbg_ifreq(__psint_t);

void	idbg_arpcom(__psint_t);
void	idbg_etherarp(__psint_t);
void	idbg_llinfo(__psint_t);
void	idbg_rtentry(__psint_t);
void	idbg_route(__psint_t);
void	idbg_routerinfo(__psint_t);
void	idbg_sockaddr(__psint_t);
void	idbg_sockaddr_raw(__psint_t);
void	idbg_radixnode(__psint_t);
void	idbg_radixtree(void);
void	idbg_radixsubtree(__psint_t);

void	print_arpcom(struct arpcom *ac);
void	print_etherarp(struct ether_arp *ar);

void	print_in_bcast(struct in_bcast *in);
void	print_in_ifaddr(struct in_ifaddr *ia);
void	print_in_ifaddrlist(struct in_ifaddr *ia);

void	print_in_multi_list(struct in_multi *inm);
void	print_router_info_list(struct router_info *rip);
void	in_addr_to_string(struct in_addr *in, unsigned char *b);

void	idbg_in_alias(__psint_t);
void	idbg_in_bcast(__psint_t);

void	idbg_in_ifaddr(__psint_t);
void	idbg_in_multi(__psint_t);

void    idbg_inpcb_problem(void);
void	idbg_inpcb(__psint_t addr);

#if defined(DEBUG) && defined(DEBUG_IPCHKSUM_TIMING) && defined(R10000)
void	idbg_chksum1(__psint_t);
void	idbg_chksum2(__psint_t);
void	idbg_chksum3(__psint_t);
void	idbg_chksum4(__psint_t);
#endif /* defined(DEBUG_IPCHKSUM_TIMING) && defined(R10000) */

void	idbg_mbuf(__psint_t);
void	idbg_mbcache(__psint_t);
void	idbg_mbstats(__psint_t);
void	idbg_mshake(__psint_t);

#if defined(DEBUG) && defined(DEBUG_MBUF_TIMING) && defined(R10000)
void	idbg_mbuft1(__psint_t);
void	idbg_mbuft2(__psint_t);
void	idbg_mbuft3(__psint_t);
void	idbg_mbuft4(__psint_t);
void	idbg_mbuftlk(__psint_t);
#endif /* defined(DEBUG_MBUF_TIMING) && defined(R10000) */

void	idbg_pcblist(__psint_t);
void	idbg_soc(__psint_t);
void	idbg_unp(__psint_t);

void	print_snoopfilter(struct snoopfilter *, int);
void	print_route(struct route *);
void	print_sockaddr(char *, struct sockaddr *);
void	print_sockaddr_raw(char*, struct sockaddr_raw *);
void	print_sockaddr_dl(char *, struct sockaddr_dl *);
void	idbg_vsock(struct vsocket *v);

extern void rsvp_dump(void *);
extern void in_pcb_sockstats(struct sockstat *);

extern void _printflags(unsigned int, char **, char *);

static struct bsdidbg_funcs_s {
	char	*name;
	void	(*func)(int);
	char	*help;
} bsdidbg_funcs[] = { /* names must be unique within first 11 characters */
	"arpcom",   VD idbg_arpcom, "Print common arp header structure",

#if defined(DEBUG) && defined(DEBUG_IPCHKSUM_TIMING) && defined(R10000)
	"chksum1", VD idbg_chksum1, "IP chksum 1518 byte 64-bit align timing",
	"chksum2", VD idbg_chksum2, "IP chksum 1518 byte 32-bit align timing",
	"chksum3", VD idbg_chksum3, "IP chksum 4096 byte 64-bit align timing",
	"chksum4", VD idbg_chksum4, "IP chksum NBPP byte 64-bit align timing",
#endif /* defined(DEBUG_IPCHKSUM_TIMING) && defined(R10000) */

	"etherarp", VD idbg_etherarp, "Print ethernet arp structure",
	"llinfo", VD idbg_llinfo, "Print llinfo arp structure",
	"hashtable", VD idbg_hashtable, "Print entries in generic hash table",
	"ifaddr",   VD idbg_ifaddr, "Print ifaddr structure",
	"ifaddrlist",       VD idbg_ifaddrlist, "Print ifaddress list",
	"ifnet",    VD idbg_ifnet, "Print ifnet structure",
	"ifreq",    VD idbg_ifreq, "Print ifreq structure",
	"inalias",  VD idbg_in_alias, "Print in_alias structure",
	"inbcast",  VD idbg_in_bcast, "Print in_bcast structure",
	"inifaddr", VD idbg_in_ifaddr,
		 "Print global in_ifaddr list of struct in_ifaddr",
	"inmulti",  VD idbg_in_multi, "Print struct in_multi",
	"inpcb_problem", VD idbg_inpcb_problem, "Print PCBs with high refcnts",
	"inpcb",    VD idbg_inpcb, "Print in_pcb struct",
	"inphashinfo",VD idbg_pcbhashinfo,"Print in_pcb hash list information",

	"mbuf",	VD idbg_mbuf, "Print mbuf contents",
	"mbcache",  VD idbg_mbcache, "Print mbuf cache structures",
	"mshake",   VD idbg_mshake, "Shake mbuf caches",

#if defined(DEBUG) && defined(DEBUG_MBUF_TIMING) && defined(R10000)
	"mbuft1",	VD idbg_mbuft1, "Mbuf perf m_get & m_free times",
	"mbuft2",	VD idbg_mbuft2, "Mbuf perf m_get & m_free times",
	"mbuft3",	VD idbg_mbuft3, "Mbuf perf m_get/m_free pair times",
	"mbuft4",	VD idbg_mbuft4, "Mbuf perf serial m_get/m_free times",
	"mbuftlk",	VD idbg_mbuftlk, "Mbuf perf of mutex_spinlock times",
#endif /* defined(DEBUG_MBUF_TIMING) && defined(R10000) */

	"radixnode",	VD idbg_radixnode, "Print radix node",
	"radixtree",	VD idbg_radixtree, "Dump radix tree",
	"radixsubtree", VD idbg_radixsubtree, "Dump radix subtree",
	"rtentry",    VD idbg_rtentry, "Print rtentry structure",
	"route",    VD idbg_route, "Print route structure",
	"routerinfo",       VD idbg_routerinfo, "Print router info structure",
	"sockaddr", VD idbg_sockaddr, "Print sockaddr structure",
	"sockaddrr",     VD idbg_sockaddr_raw, "Print raw sock addr structure",
	"pcblist",  VD idbg_pcblist, "Print in_pcb list",
	"protosw",  VD idbg_protosw, "Print protocol switch table entry",
	"rawcb",    VD idbg_rawcb,"Print raw protocol control block structure",
	"rawif",    VD idbg_rawif, "Print rawif interface structure",

	"sock",     VD idbg_soc, "Print socketvar struct",
	"tcpcb",    VD idbg_tcpcb, "Print TCP control block",
	"ckna",	VD idbg_ckna,
	 "Print Kernel Network Activity (kna) structure for each cpu",
	"cnetstats",VD idbg_cnetstats,
	 "Print all tcp/ip statistics given a kna address",
	"mbstats",  VD idbg_mbstats, "Print system mbuf statistics",
	"icmpstat",	VD idbg_icmpstats,
	 "Print ICMP statistics given an icmppstat structure address",
	"igmpstat",	VD idbg_igmpstats,
	 "Print IGMP statistics given an igmpstat structure address",
	"ipstats",	VD idbg_ipstats,
	 "Print IP statistics given an ipstat structure address",
	"inp_stats",	VD idbg_inpcbstats,
	 "Print inpcb statistics given an inpcbstat structure address",
	"ginp_stats",	VD idbg_ginpcbstats,
	 "Print inpcb statistics for all CPUs",
	"dinp_stats",	VD idbg_dinpcbstats,
	 "Print inpcb distribution statistics",
	"mrtstats",	VD idbg_mrtstats,
	 "Print Multicast routing statistics given a mrtstat struct address",
	"sockstats",	VD idbg_sockstats,
	 "Compute and print general socket statistics",
	"tcpstats",	VD idbg_tcpstats,
	 "Print TCP statistics given a tcpstat structure address",
	"udpstats",	VD idbg_udpstats,
	 "Print UDP statistics given an udpstat structure address",
	"unpcb",	VD idbg_unp,
	 "Print an AF_UNIX control block",
	"rsvp",		VD rsvp_dump,
	 "Print rsvp statistics given a psif structure address",
	"vsock",	VD idbg_vsock,
	 "Print a vsocket",
	/* end */
	0,	0,	0
};

static char *socketTypes[] = {
	"#0 ?",
	"DGRAM",
	"STREAM",
	"COTS_ORD",
	"RAW",
	"RDM",
	"SEQPACKET",
};

static char *tab_sotype[] = {
	"ZERO/ILLEGAL",			/* 0x00000 */
	"SOCK_DGRAM/NC_TPI_CLTS",	/* 0x00001 */
	"SOCK_STREAM/NC_TPI_COTS",	/* 0x00002 */
	"NC_TPI_COTS_ORD",	/* 0x00003 */
	"SOCK_RAW/NC_TPI_RAW",		/* 0x00004 */
	"SOCK_RDM",			/* 0x00005 */
	"SOCK_SEQPACKET",		/* 0x00006 */
};

static char *tab_tcpstates[] = {
	"TCPS_CLOSED",			/* 0x00000 */
	"TCPS_LISTEN",			/* 0x00001 */
	"TCPS_SYN_SENT",		/* 0x00002 */
	"TCPS_SYN_RECEIVED",		/* 0x00003 */
	"TCPS_ESTABLISHED",		/* 0x00004 */
	"TCPS_CLOSE_WAIT",		/* 0x00005 */
	"TCPS_FIN_WAIT_1",		/* 0x00006 */
	"TCPS_CLOSING",			/* 0x00007 */
	"TCPS_LAST_ACK",		/* 0x00008 */
	"TCPS_FIN_WAIT_2",		/* 0x00009 */
	"TCPS_TIME_WAIT",		/* 0x0000A */
};

static char *tab_sbflags[] = {
	"LOCK",	/* 0x01 */
	"WANT",	/* 0x02 */
	"WAIT",	/* 0x04 */
	"SEL",	/* 0x08 */
};

static char *tab_sostate[] = {
	"NOFDREF",			/* 0x00001 */
	"ISCONNECTED",			/* 0x00002 */
	"ISCONNECTING",			/* 0x00004 */
	"ISDISCONNECTING",		/* 0x00008 */
	"CANTSENDMORE",			/* 0x00010 */
	"CANTRCVMORE",			/* 0x00020 */
	"RCVATMARK",			/* 0x00040 */
	"ISBOUND/PRIV",			/* 0x00080 */
	"NBIO",				/* 0x00100 */
	"ASYNC",			/* 0x00200 */
	"CLOSED",			/* 0x00400 */
	"MBUFWAIT",			/* 0x00800 */
	"ISCONFIRMING",			/* 0x01000 */
	"WANT_CALL",	 		/* 0x02000 */
	"TPISOCKET/CONN_IND_SENT",	/* 0x04000 */
	"XTP",				/* 0x08000 */
	"CLOSING",			/* 0x10000 */
	"SOFREE",			/* 0x20000 */
	"PRIVCHECKED",			/* 0x40000 */
};

static char *tab_sooptions[] = {
	"DEBUG",	/* 0x0001 */
	"ACCEPTCONN",	/* 0x0002 */
	"REUSEADDR",	/* 0x0004 */
	"KEEPALIVE",	/* 0x0008 */
	"DONTROUTE",	/* 0x0010 */
	"BROADCAST",	/* 0x0020 */
	"USELOOPBACK",	/* 0x0040 */
	"LINGER",	/* 0x0080 */
	"OOBINLINE",	/* 0x0100 */
	"REUSEPORT",	/* 0x0200 */
	"IMASOCKET",	/* 0x0400 */
	"0x0800",
	"CHAMELEON",	/* 0x1000 */
	"0x2000",
};

static char *ifflags[] = {
	"IFF_UP",		/* 0x1 interface is up */
	"IFF_BROADCAST",	/* 0x2 broadcast address valid */
	"IFF_DEBUG",		/* 0x4 turn on debugging */
	"IFF_LOOPBACK",		/* 0x8 is a loopback net */
	"IFF_POINTOPOINT",	/* 0x10 interface is point-to-point link */
	"IFF_NOTRAILERS",	/* 0x20 avoid use of trailers */
	"IFF_RUNNING",		/* 0x40 resources allocated */
	"IFF_NOARP",		/* 0x80 no address resolution protocol */
	"IFF_PROMISC",		/* 0x100 receive all packets */
	"IFF_ALLMULTI",		/* 0x200 receive all multicast packets */
	"IFF_FILTMULTI",	/* 0x400 need to filter multicast packets */
				/* Also MIPS ABI protocols on interface */
	"IFF_MULTICAST",	/* 0x800 supports multicast */
	"IFF_CKSUM",		/* 0x1000 does checksumming */
	"IFF_ALLCAST",		/* 0x2000 does SRCROUTE broadcast */
	"IFF_PRIVATE",		/* 0x8000 MIPS ABI - do not advertise */
	"IFF_LINK0",		/* 0x10000 driver private */
	"IFF_LINK1",		/* 0x20000 driver private */
	"IFF_LINK2",		/* 0x40000 driver private */
	0
};

/*
 * ia_addrflags field in 'struct in_ifaddr' (defined in in_var.h)
 */
static char *tab_in_ifaddr_iaddrflags[] = {
	"IADDR_PRIMARY", /* 0x1 Primary address for interface */
	"IADDR_ALIAS",	 /* 0x2 Alias address for interface */
	 0
};

/*
 * 'struct ifaddr' ia_flags field (defined in if.h)
 */
static char *tab_rtflags[] = {
	"RTF_UP",		/* 0x1 route usable */
	"RTF_GATEWAY",		/* 0x2 destination is a gateway */
	"RTF_HOST",		/* 0x4 host entry (net otherwise) */
	"RTF_REJECT",		/* 0x8 host or net unreachable */
	"RTF_DYNAMIC",		/* 0x10 created dynamic (by redirect)*/
	"RTF_MODIFIED",		/* 0x20 mod dynamically (by redirect)*/
	"RTF_DONE",		/* 0x40 message confirmed */
	"RTF_MASK",		/* 0x80 subnet mask present */
	"RTF_CLONING",		/* 0x100 generate new routes on use */
	"RTF_XRESOLVE", 	/* 0x200 extern daemon resolves name*/
	"RTF_LLINFO",		/* 0x400 generated by ARP or ESIS */
	"RTF_STATIC",		/* 0x800 manually added */
	"RTF_BLACKHOLE",	/* 0x1000 discard pkts,during updates*/
	"RTF_PROTO2",		/* 0x4000 proto specific routing flag*/
	"RTF_PROTO1",		/* 0x8000 proto specific routing flag*/
	"RTF_CKSUM",		/* 0x10000 chksum for this route */
	 0
};

char	*tab_hashflags[] = {
	"HTF_NOTINTABLE",	/* 0x0001 Entry not in hash table yet */
	"HTF_INET",		/* 0x0002 Internet hash node */
	"HTF_MULTICAST",	/* 0x0004 Multicast hash node */
	"HTF_BROADCAST",	/* 0x0008 Broadcast hash node */
	"HTF_INPCB",		/* 0x0010 PCB hash node */
	0
};

static char *affamily[] = {
	"AF_UNSPEC",	/* 0 unspecified */
	"AF_UNIX",	/* 1 local to host (pipes, portals) */
	"AF_INET",	/* 2 internetwork: UDP, TCP, etc. */
	"AF_IMPLINK",	/* 3 arpanet imp addresses */
	"AF_PUP",	/* 4 pup protocols: e.g. BSP */
	"AF_CHAOS",	/* 5 mit CHAOS protocols */
	"AF_NS",	/* 6 XEROX NS protocols */
	"AF_ISO",	/* 7 ISO protocols */
	"AF_ECMA",	/* 8 European computer manufacturers */
	"AF_DATAKIT",	/* 9 datakit protocols */
	"AF_CCITT",	/* 10 CCITT protocols, X.25 etc */
	"AF_SNA",	/* 11 IBM SNA */
	"AF_DECnet",	/* 12 DECnet */
	"AF_DLI",	/* 13 DEC Direct data link interface */
	"AF_LAT",	/* 14 LAT */
	"AF_HYLINK",	/* 15 NSC Hyperchannel */
	"AF_APPLETALK",	/* 16 Apple Talk */
	"AF_ROUTE",	/* 17 Internal Routing Prot AND AF_NIT Nw i/f Tap */
	"AF_RAW",	/* 18 Raw link layer I/F AND IEEE 802.2, ISO 8802 */
	"psuedo_AF_XTP", /* 19 eXpress Transfer Protocol (no AF)  AF_OSI */
	"AF_X25",	/* 20 CCITT X.25 in particular */
	"AF_OSINET",	/* 21 AFI = 47, IDI = 4 */
	"AF_GOSIP",	/* 22 U.S. Government OSI */
	"AF_SDL",	/* 23 SGI Data Link for DLPI */
	"AF_INET6",	/* 24 Internet Protocol V6 */
	"AF_LINK",	/* 25 Link layer interface (BSD 4.4Lite) */
	0
};

#define SA_MAX_FAMILY	24	/* size of affamily string array of pointers */

static char *rawfamily[] = {
	"RAWPROTO_RAW",	/* 0 default */
	"RAWPROTO_DRAIN", /* 1 drain protocol */
	"RAWPROTO_SNOOP", /* 2 snoop protocol */
	 0
};

static char *protoswfamily[] = {
	"PR_ATOMIC",	/* 0x01 */
	"PR_ADDR", /* 0x02 */
	"PR_CONNREQUIRED", /* 0x04 */
	"PR_WANTRCVD",     /* 0x08 */
	"PR_RIGHTS",       /* 0x10 */
	"PR_DESTADDROPT",  /* 0x20 */
	 0
};

static char *tcpstates[] = {
	"TCPS_CLOSED",          /* 0 closed */
	"TCPS_LISTEN",          /* 1 listening for connection */
	"TCPS_SYN_SENT",        /* 2 active, have sent syn */
	"TCPS_SYN_RECEIVED",    /* 3 have send and received syn */
	"TCPS_ESTABLISHED",     /* 4 established */
	"TCPS_CLOSE_WAIT",      /* 5 rcvd fin, waiting for close */
	"TCPS_FIN_WAIT_1",      /* 6 have closed, sent fin */
	"TCPS_CLOSING",         /* 7 closed xchd FIN; await FIN ACK */
	"TCPS_LAST_ACK",        /* 8 had fin and close; await FIN ACK */
	"TCPS_FIN_WAIT_2",      /* 9 have closed, fin is acked */
	"TCPS_TIME_WAIT",       /* 10 in 2*msl quiet wait after close */
};

static char *tcpflags[] = {
	"TF_ACKNOW",            /* 0x001 ack peer immediately */
	"TF_DELACK",            /* 0x002 ack, but try to delay it */
	"TF_NODELAY",           /* 0x004 don't delay packets to coalesce */
	"TF_NOOPT",             /* 0x008 don't use tcp options */
	"TF_SENTFIN",           /* 0x010 have sent FIN */
	"TF_REQ_SCALE",         /* 0x020 have/will request window scaling */
	"TF_RCVD_SCALE",        /* 0x040 other side has requested scaling */
	"TF_REQ_TSTMP",         /* 0x080 have/will request timestamps */
	"TF_RCVD_TSTMP",        /* 0x100 a timestamp was received in SYN */
};

#define IGMP_STATE_MAX	6	/* size of igmp state string array of ptrs */

static char *igmp_state_tab[] = {
	"IGMP_NOSTATE",			/* 0 Unspecified */
	"IGMP_DELAYING_MEMBER",		/* 1 Delaying member */
	"IGMP_IDLE_MEMBER",		/* 2 Idle member */
	"IGMP_LAZY_MEMBER",		/* 3 Lazy member */
	"IGMP_SLEEPING_MEMBER",		/* 4 Sleeping member */
	"IGMP_AWAKENING_MEMBER", 	/* 5 Awakening member */
	 0
};

#define IGMP_ROUTER_TAB_MAX  0x1  /* Mask for maximum index */
static char *igmp_router_tab[] = {
	"IGMP_OLD_ROUTER",	/* 0x0 */
	"IGMP_NEW_ROUTER",	/* 0x1 */
	0
};

static char *mbufTypes[] = {
	"MT_FREE",
	"MT_DATA",
	"MT_HEADER",
	"MT_SOCKET",
	"MT_PCB",
	"MT_RTABLE",
	"MT_HTABLE",
	"MT_ATABLE",
	"MT_SONAME",
	"MT_SAT",
	"MT_SOOPTS",
	"MT_FTABLE",
	"MT_RIGHTS",
	"MT_IFADDR",
	"MT_DN_DRBUF",
	"MT_DN_BLK",
	"MT_DN_BD",
	"MT_IPMOPTS",
	"MT_MRTABLE",
	"MT_SOACL"
};

char *idbg_inp_flags[] = {
	"RECVOPTS",		/* 0x1 */
	"RECVRETOPTS",		/* 0x2 */
	"RECVDSTADDR",		/* 0x4 */
	"HDRINCL"		/* 0x8 */
};

char *idbg_inp_hashflags[] = {
	"CLTS",		/* 0x1 */
	"LISTEN",	/* 0x2 */
	"MOVED"		/* 0x4 */
};

/*
 * Initialization routine.
 */
void
bsdidbginit(void)
{

	struct bsdidbg_funcs_s *p;

	for (p = bsdidbg_funcs; p->func; p++)
		idbg_addfunc(p->name, (void (*)())(p->func));
}

static void
printafam(u_char len, u_char fam)
{
	qprintf("   len 0x%x, ", len);

	if (fam >= sizeof(affamily)/sizeof(affamily[0]))
		qprintf("AF-0x%x ", fam);
	else
		qprintf(affamily[fam]);
	return;
}

void
print_arpcom(struct arpcom *ac)
{
	int l;
	unsigned char buf[20];

	qprintf(" arpcom: 0x%x, ifnet 0x%x\n", ac, &(ac->ac_if));
	qprintf("  ac_enaddr: ");
	for (l=0; l < 6; l++) {
		qprintf("%02x ", ac->ac_enaddr[l]);
	}
	qprintf("\n");
	in_addr_to_string((struct in_addr *)(&(ac->ac_ipaddr)), buf);
	qprintf("  ac_ipaddr: %s\n", buf);
	return;
}

void
print_etherarp(struct ether_arp *ar)
{
	int l;
	unsigned char buf[20];

	qprintf(" ether_arp: 0x%x\n", ar);
  qprintf("  arphdr: ar_hrd(hw addr format) %d, ar_pro(prot addr format) %d\n",
		ar->ea_hdr.ar_hrd, ar->ea_hdr.ar_pro);
	qprintf("  arphdr: ar_hln(hw len) %d, ar_pln(prot len) %d\n",
		ar->ea_hdr.ar_hln, ar->ea_hdr.ar_pln);
	qprintf("  arphdr: ar_op(req) %d\n", ar->ea_hdr.ar_op);

	qprintf("  arp_sha(sender hw addr): ");
	for (l=0; l < 6; l++) {
		qprintf("%02x ", ar->arp_sha[l]);
	}
	qprintf("\n");

	qprintf("  arp_spa(sender prot addr): ");
	for (l=0; l < 4; l++) {
		qprintf("%02x ", ar->arp_sha[l]);
	}
	in_addr_to_string((struct in_addr *)(&(ar->arp_sha[0])), buf);
	qprintf(":  arp_spa ipaddr: %s\n", buf);

	qprintf("  arp_tha(target hw addr): ");
	for (l=0; l < 6; l++) {
		qprintf("%02x ", ar->arp_tha[l]);
	}
	qprintf("\n");

	qprintf("  arp_tpa(target prot addr): ");
	for (l=0; l < 4; l++) {
		qprintf("%02x ", ar->arp_tpa[l]);
	}
	in_addr_to_string((struct in_addr *)(&(ar->arp_tpa[0])), buf);
	qprintf(":  arp_tpa ipaddr: %s\n", buf);
	return;
}

void
print_llinfoarp(struct llinfo_arp *llp)
{
	qprintf(" llinfo_arp: 0x%p\n", llp);
	qprintf("    la_next: 0x%p\n", llp->la_next);
	qprintf("    la_prev: 0x%p\n", llp->la_prev);
	qprintf("      la_rt: 0x%p\n", llp->la_rt);
	qprintf("    la_hold: 0x%p\n", llp->la_hold);
	qprintf("   la_asked: %ld\n", llp->la_asked);
	return;
}

/*
 * Stores a printable string version of an internet address
 * into the buffer 'b' of lenth 'len'.
 */
void
in_addr_to_string(struct in_addr *in, unsigned char *b)
{
	unsigned char n, *p;
	int i;

	p = (u_char *)in;
	for (i = 0; i < 4; i++) {
		if (i)
			*b++ = '.';
		n = *p;
		if (n > 99) {
			*b++ = '0' + (n / 100);
			n %= 100;
		}
		if (*p > 9) {
			*b++ = '0' + (n / 10);
			n %= 10;
		}
		*b++ = '0' + n;
		p++;
	}
	*b = 0;
	return;
}

#ifdef INET6
static const char *
inet_ntop(int af, char *src, char *dst)
{
	char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"], *tp;
	struct { int base, len; } best, cur;
	u_int words[sizeof(struct in6_addr) / sizeof(int16_t)];
	int i;

	if (af ==  AF_INET) {
		sprintf(dst, "%u.%u.%u.%u", src[0], src[1], src[2], src[3]);
		return (dst);
	}

	ASSERT(af == AF_INET6);

	/*
	 * Preprocess:
	 *	Copy the input (bytewise) array into a wordwise array.
	 *	Find the longest run of 0x00's in src[] for :: shorthanding.
	 */
	bzero(words, sizeof(words));
	for (i = 0; i < sizeof(struct in6_addr); i++)
		words[i / 2] |= (src[i] << ((1 - (i % 2)) << 3));
	best.base = -1;
	cur.base = -1;
	for (i = 0; i < (sizeof(struct in6_addr) / sizeof(int16_t)); i++) {
		if (words[i] == 0) {
			if (cur.base == -1)
				cur.base = i, cur.len = 1;
			else
				cur.len++;
		} else {
			if (cur.base != -1) {
				if (best.base == -1 || cur.len > best.len)
					best = cur;
				cur.base = -1;
			}
		}
	}
	if (cur.base != -1) {
		if (best.base == -1 || cur.len > best.len)
			best = cur;
	}
	if (best.base != -1 && best.len < 2)
		best.base = -1;

	/*
	 * Format the result.
	 */
	tp = tmp;
	for (i = 0; i < (sizeof(struct in6_addr) / sizeof(int16_t)); i++) {
		/* Are we inside the best run of 0x00's? */
		if (best.base != -1 && i >= best.base &&
		    i < (best.base + best.len)) {
			if (i == best.base)
				*tp++ = ':';
			continue;
		}
		/* Are we following an initial run of 0x00s or any real hex? */
		if (i != 0)
			*tp++ = ':';
		/* Is this address an encapsulated IPv4? */
		if (i == 6 && best.base == 0 &&
		    (best.len == 6 || (best.len == 5 && words[5] == 0xffff))) {
			/* Append v4 addr in dot dot notation */
			sprintf(tp, "%u.%u.%u.%u", src[12], src[13],
			  src[14], src[15]);
			tp += strlen(tp);
			break;
		}
		sprintf(tp, "%x", words[i]);
		tp += strlen(tp);
	}
	/* Was it a trailing run of 0x00's? */
	if (best.base != -1 && (best.base + best.len) ==
	  (sizeof(struct in6_addr) / sizeof(int16_t)))
		*tp++ = ':';
	*tp++ = '\0';

	strcpy(dst, tmp);
	return (dst);
}
#endif


void
print_ifaddr(struct ifaddr *ifa)
{
	qprintf("  ifaddr 0x%x, ifa_next 0x%x\n", ifa, ifa->ifa_next);
	qprintf("  ifa_start_inifaddr 0x%x, ifa_ifp 0x%x\n",
		ifa->ifa_start_inifaddr, ifa->ifa_ifp);

	print_sockaddr("  ifa_addr ", ifa->ifa_addr);
	print_sockaddr("  ifa_dstaddr ", ifa->ifa_dstaddr);
	print_sockaddr("  ifa_netmask ",(struct sockaddr *)(ifa->ifa_netmask));
	printflags(ifa->ifa_flags, tab_rtflags, "  ifa_flags ");
	qprintf("\n");

   qprintf("  ifa_rtrequest 0x%x, ifa_refcnt %d(0x%x), ifa_metric %d(0x%x)\n",
		ifa->ifa_rtrequest,
		ifa->ifa_refcnt, ifa->ifa_refcnt,
		ifa->ifa_metric, ifa->ifa_metric);
	return;
}

void
print_in_bcast(struct in_bcast *ia)
{
	int l;
	unsigned char buf[20];

	qprintf(" in_bcast 0x%x\n", ia);
	qprintf("  hashaddr 0x%x, next 0x%x, prev 0x%x\n",
			&(ia->hashbucket),
			ia->hashbucket.next,
			ia->hashbucket.prev);
	qprintf("  hash crc %d(0x%x)",
		ia->hashbucket.crc, ia->hashbucket.crc);
	printflags(ia->hashbucket.flags, tab_hashflags, " hash flags");
	qprintf("\n");

	qprintf("  hash key 0x");
	for (l=0; l < HASH_KEYSIZE; l++) {
		qprintf("%02x", ia->hashbucket.key[l]);
	}
	in_addr_to_string((struct in_addr *)(&(ia->hashbucket.key)), buf);
	qprintf(", ip addr %s\n", buf);

	in_addr_to_string((struct in_addr *)(&(ia->ia_net)), buf);
	qprintf("  ia_net 0x%04x, ip addr %s\n", ia->ia_net, buf);

	in_addr_to_string((struct in_addr *)(&(ia->ia_netmask)), buf);
	qprintf("  ia_netmask 0x%04x, ip addr %s\n", ia->ia_netmask, buf);

	in_addr_to_string((struct in_addr *)(&(ia->ia_subnet)), buf);
	qprintf("  ia_subnet 0x%04x, ip addr %s\n", ia->ia_subnet, buf);

	in_addr_to_string((struct in_addr *)(&(ia->ia_subnetmask)), buf);
	qprintf("  ia_subnetmask 0x%04x, ip addr %s\n",
		ia->ia_subnetmask, buf);

	in_addr_to_string(&(ia->ia_netbroadcast), buf);
	qprintf("  ia_netbroadcast 0x%04x, ip addr %s\n",
		ia->ia_netbroadcast.s_addr, buf);

	print_sockaddr("  ia_addr ", (struct sockaddr *)(&(ia->ia_addr)));
	print_sockaddr("  ia_dstaddr ",(struct sockaddr *)(&(ia->ia_dstaddr)));

	qprintf("  ifp 0x%x, refcnt %d\n", ia->ifp, ia->refcnt);
	qprintf("\n");
	return;
}

void
print_ifaddrlist(struct ifaddr *ifp)
{
	register struct ifaddr *cur_ifp, *next_ifp;

	cur_ifp = ifp;
	while (cur_ifp) {
		print_ifaddr(cur_ifp);
		next_ifp = cur_ifp->ifa_next;
		cur_ifp = next_ifp;
	}
}

void
print_in_ifaddr(struct in_ifaddr *ia)
{
	int l;
	unsigned char buf[20];

	qprintf(" in_ifaddr 0x%x\n", ia);

	qprintf("  hashaddr 0x%x, next 0x%x, prev 0x%x\n",
			&(ia->ia_hashbucket),
			ia->ia_hashbucket.next,
			ia->ia_hashbucket.prev);
	qprintf("  hash crc %d(0x%x)",
		ia->ia_hashbucket.crc, ia->ia_hashbucket.crc);
	printflags(ia->ia_hashbucket.flags, tab_hashflags, " hash flags");
	qprintf("\n");

	qprintf("  hash key 0x");
	for (l=0; l < HASH_KEYSIZE; l++) {
		qprintf("%02x", ia->ia_hashbucket.key[l]);
	}
	in_addr_to_string((struct in_addr *)(&(ia->ia_hashbucket.key)), buf);
	qprintf(", ip addr %s\n", buf);

	print_ifaddr(&(ia->ia_ifa));
#ifdef LATER
	print_sockaddr("  ia_addr ", (struct sockaddr *)(&(ia->ia_addr)));
	print_sockaddr("  ia_dstaddr ",(struct sockaddr *)(&(ia->ia_dstaddr)));
	print_sockaddr("  ia_sockmask ",
		       (struct sockaddr *)(&(ia->ia_sockmask)));
#endif
	in_addr_to_string((struct in_addr *)(&(ia->ia_net)), buf);
	qprintf("  ia_net 0x%x, ip addr %s\n", ia->ia_net, buf);

	in_addr_to_string((struct in_addr *)(&(ia->ia_netmask)), buf);
	qprintf("  ia_netmask 0x%x, ip addr %s\n", ia->ia_netmask, buf);

	in_addr_to_string((struct in_addr *)(&(ia->ia_subnet)), buf);
	qprintf("  ia_subnet 0x%x, ip addr %s\n", ia->ia_subnet, buf);

	in_addr_to_string((struct in_addr *)(&(ia->ia_subnetmask)), buf);
	qprintf("  ia_subnetmask 0x%x, ip addr %s\n", ia->ia_subnetmask, buf);

	in_addr_to_string(&(ia->ia_netbroadcast), buf);
	qprintf("  ia_netbroadcast 0x%x, ip addr %s\n",
		ia->ia_netbroadcast.s_addr, buf);

	printflags(ia->ia_addrflags, tab_in_ifaddr_iaddrflags,
		   "  ia_addrflags ");
	qprintf("\n");
	return;
}

void
print_in_ifaddrlist(struct in_ifaddr *ia)
{
	print_in_ifaddr(ia);
	return;
}

void
print_in_multi_list(struct in_multi *inm)
{
	unsigned char buf[20];
	int l;

	qprintf(" in_multi 0x%x, inm_ifp 0x%x\n", inm, inm->inm_ifp);
	qprintf("  hashaddr 0x%x, next 0x%x, prev 0x%x\n",
		&(inm->hashbucket),
		inm->hashbucket.next,
		inm->hashbucket.prev);

	qprintf("  hash crc %d(0x%x) ",
		inm->hashbucket.crc, inm->hashbucket.crc);
	printflags(inm->hashbucket.flags, tab_hashflags, " hash flags");
	qprintf("\n");

	qprintf("  hash key ");
	for (l=0; l < HASH_KEYSIZE; l++) {
		qprintf("%02x", inm->hashbucket.key[l]);
	}
	in_addr_to_string((struct in_addr *)(&(inm->hashbucket.key)), buf);
		qprintf(",  ip addr %s\n", buf);

	qprintf("  inm_refct %d, inm_ifp 0x%x\n",
		inm->inm_refcount, inm->inm_ifp);

	qprintf("  inm_addr(in_addr) 0x%08x,", inm->inm_addr);
		in_addr_to_string(&(inm->inm_addr), buf);
		qprintf(" ip addr %s\n", buf);

	if (inm->inm_state < IGMP_STATE_MAX) {
		qprintf("  inm_timer %d, inm_state 0x%x <%s>\n",
			inm->inm_timer, inm->inm_state,
			igmp_state_tab[inm->inm_state]);
	} else {
		qprintf("  inm_timer %d, inm_state 0x%x, Invalid igmp_state\n",
			inm->inm_timer, inm->inm_state);
	}
	if (inm->inm_rti) {
		print_router_info_list(inm->inm_rti);
	} else {
		qprintf("  inm_rti 0x%x\n", inm->inm_rti);
	}
	return;
}

void
print_router_info_list(struct router_info *rip)
{
	while (rip) {
		qprintf("  router_info 0x%x, next 0x%x\n",
			rip, rip->next);
		qprintf("   ifp 0x%x, time(age threshold) %d\n",
			rip->ifp, rip->time);
		qprintf("   type %d <%s>\n",
			rip->type,
			igmp_router_tab[rip->type & IGMP_ROUTER_TAB_MAX]);

		rip = rip->next;
	}
	return;
}

void
print_sockaddr(char *tag, struct sockaddr *sap)
{
	unsigned char buf[50], *p;
	register u_short af, i, port;

	if (sap == 0)
		return;
	qprintf("%s 0x%x\n", tag, sap);

#ifdef _HAVE_SA_LEN
	printafam(sap->sa_len, sap->sa_family);
	af = sap->sa_family;
#else
	af = (sap->sa_family & 0xFF);
	printafam(sap->sa_family>>8, af); /* len, af_family */
#endif
	if (af == AF_LINK) {
		print_sockaddr_dl("  sockaddr_dl ",
			(struct sockaddr_dl *)sap);
		return;
	}

	if (af == AF_INET) {
		in_addr_to_string(
	(struct in_addr *)(&(((struct sockaddr_in*)sap)->sin_addr.s_addr)),
			buf);

		port = ((struct sockaddr_in *)sap)->sin_port;

		qprintf(" port 0x%x(%d), ip_addr %s (0x%08x)\n",
			port,
			port,
			buf,
			(u_long)((struct sockaddr_in*)sap)->sin_addr.s_addr);
#ifdef INET6
	} else if (af == AF_INET6) {
		inet_ntop(af,
	(void *)(&(((struct sockaddr_in6 *)sap)->sin6_addr.s6_addr)),
			(char *)buf);
		port = ((struct sockaddr_in6 *)sap)->sin6_port;
		qprintf(" port 0x%x(%d), ip6_addr %s\n", port, port, buf);
#endif
	} else {
		while (*tag == ' ')
			qprintf("%c",*tag++);
		qprintf("\n");
	}
	p = (unsigned char *)sap;
	qprintf("   sockaddr hex: ");
#ifdef INET6
	for (i=0; i < _FAKE_SA_LEN_DST(sap); i++) {
#else
	for (i=0; i < sizeof(struct sockaddr); i++) {
#endif
		qprintf("%02x ", *p);
		p++;
	}
	qprintf("\n");
	return;
}

void
print_sockaddr_dl(char *tag, struct sockaddr_dl *sadl)
{
	register u_short i, len;
	register unsigned char *p;

	qprintf("   %s 0x%x\n", tag, sadl);
	if (sadl) {
qprintf("   sdl_index %d, sdl_type %d, sdl_nlen %d, sdl_alen %d, sdl_slen %d\n",
		sadl->sdl_index, sadl->sdl_type,
		sadl->sdl_nlen, sadl->sdl_alen,
		sadl->sdl_slen);

		if ((len = sadl->sdl_nlen) > 0) {
			qprintf("   link_name ");
			for (i=0; i < len; i++) {
				qprintf("%c", sadl->sdl_data[i]);
			}
			qprintf("\n");
		}
		qprintf("   raw sockaddr_dl ");
		p = (unsigned char *)sadl;
		for (i=0; i < sizeof(struct sockaddr_dl); i++) {
			qprintf("%02x ", *p);
			p++;
		}
		qprintf("\n");
	}
	return;
}

void
print_sockaddr_raw(char *tag, struct sockaddr_raw *sarp)
{
	register u_short i;
	register char *p;

	qprintf("%s 0x%x ", tag, sarp);
	if (sarp) {
#ifdef _HAVE_SA_LEN
		printafam(sarp->sr_len,sarp->sr_family);
#else
		printafam(sarp->sr_family>>8,sarp->sr_family);
#endif
		qprintf("\n");

		for (p = tag; *p == ' '; p++)
			qprintf("%c",*p);
		qprintf("    sr_local: sr_port=%d sr_ifname=",
			sarp->sr_port);
		for (i=0; i< RAW_IFNAMSIZ; i++){
			qprintf("%c", sarp->sr_u.sru_local.srl_ifname[i]);
		}
		qprintf("\n");

		for (p = tag; *p == ' '; p++)
			qprintf("%c",*p);
		qprintf("    sr_foreign: srf_addr=");
		for (i=0; i < RAW_MAXADDRLEN; i++)
			qprintf("%02x ", sarp->sr_u.sru_foreign.srf_addr[i]);
	}
	qprintf("\n");
	return;
}

static char *
rtflags_to_str(short rtflags)
{
	static char str[256];

	str[0] = '\0';
	if (rtflags & RTF_UP) {
		strcat(str, "RTF_UP ");
	}
	if (rtflags & RTF_GATEWAY) {
		strcat(str, "RTF_GATEWAY ");
	}
	if (rtflags & RTF_HOST) {
		strcat(str, "RTF_HOST ");
	}
	if (rtflags & RTF_REJECT) {
		strcat(str, "RTF_REJECT ");
	}
	if (rtflags & RTF_DYNAMIC) {
		strcat(str, "RTF_DYNAMIC ");
	}
	if (rtflags & RTF_MODIFIED) {
		strcat(str, "RTF_MODIFIED ");
	}
	if (rtflags & RTF_DONE) {
		strcat(str, "RTF_DONE ");
	}
	if (rtflags & RTF_MASK) {
		strcat(str, "RTF_MASK ");
	}
	if (rtflags & RTF_CLONING) {
		strcat(str, "RTF_CLONING ");
	}
	if (rtflags & RTF_XRESOLVE) {
		strcat(str, "RTF_XRESOLVE ");
	}
	if (rtflags & RTF_LLINFO) {
		strcat(str, "RTF_LLINFO ");
	}
	if (rtflags & RTF_STATIC) {
		strcat(str, "RTF_STATIC ");
	}
	if (rtflags & RTF_BLACKHOLE) {
		strcat(str, "RTF_BLACKHOLE ");
	}
	if (rtflags & RTF_HOSTALIAS) {
		strcat(str, "RTF_HOSTALIAS ");
	}
	if (rtflags & RTF_PROTO2) {
		strcat(str, "RTF_PROTO2 ");
	}
	if (rtflags & RTF_PROTO1) {
		strcat(str, "RTF_PROTO1 ");
	}
	return(str);
}

void
print_rtmetrics(struct rtentry *rt)
{
	qprintf("     rmx_locks: %d\n", rt->rt_rmx.rmx_locks);
	qprintf("       rmx_mtu: %d\n", rt->rt_rmx.rmx_mtu);
	qprintf("  rmx_hopcount: %d\n", rt->rt_rmx.rmx_hopcount);
	qprintf("    rmx_expire: %d\n", rt->rt_rmx.rmx_expire);
	qprintf("  rmx_recvpipe: %d\n", rt->rt_rmx.rmx_recvpipe);
	qprintf("  rmx_sendpipe: %d\n", rt->rt_rmx.rmx_sendpipe);
	qprintf("  rmx_ssthresh: %d\n", rt->rt_rmx.rmx_ssthresh);
	qprintf("       rmx_rtt: %d\n", rt->rt_rmx.rmx_rtt);
	qprintf("    rmx_rttvar: %d\n", rt->rt_rmx.rmx_rttvar);
	qprintf("    rmx_pksent: %d\n", rt->rt_rmx.rmx_pksent);
}

void
print_rtentry(struct rtentry *rt)
{
	print_sockaddr("  rt_gateway", rt->rt_gateway);
	print_sockaddr("  rt_key", rt_key(rt));

	qprintf("  rt_flags  %s (0x%x), rt_refcnt %d, rt_use %d\n",
		rtflags_to_str(rt->rt_flags),
		rt->rt_flags,
		rt->rt_refcnt,
		rt->rt_use);

	qprintf("  rt_ifp 0x%x, rt_ifa 0x%x\n",
		rt->rt_ifp,
		rt->rt_ifa);

	qprintf("  rt_genmask 0x%x, rt_llinfo 0x%x, rt_gwroute 0x%x\n",
		rt->rt_genmask,
		rt->rt_llinfo,
		rt->rt_gwroute);
	qprintf("  rt_srcifa 0x%x\n", rt->rt_srcifa);
	print_rtmetrics(rt);
}

void
print_route(struct route *rp)
{
	qprintf(" rtentry: ro_rt 0x%x\n", rp->ro_rt);
	if (rp->ro_rt != 0) {
		print_rtentry(rp->ro_rt);
	}
	print_sockaddr(" rtentry: ro_dst", &rp->ro_dst);
	return;
}

void
print_radixnode(struct radix_node *rn)
{
	unsigned char buf[50];

	qprintf("  >> RADIX_NODE: 0x%x\n",	rn);
	qprintf("   list of mask: 0x%x\n", rn->rn_mklist);
	qprintf("   parent pointer: 0x%x\n", rn->rn_p);
	qprintf("   radix_node flags: ");
	if (rn->rn_flags & RNF_ACTIVE)
		qprintf("RNF_ACTIVE ");
	if (rn->rn_flags & RNF_NORMAL)
		qprintf("RNF_NORMAL ");
	if (rn->rn_flags & RNF_ROOT)
		qprintf("RNF_ROOT ");
	qprintf("\n");
	if (rn->rn_b > 0) {
		qprintf("   Node: bit offset %d, Compare from byte %d, bmask=0x%02x\n",
			rn->rn_b, rn->rn_off, (unsigned char)rn->rn_bmask);
		qprintf("         left subtree=0x%x, right subtree=0x%x\n",
			rn->rn_l, rn->rn_r);
	} else {
		struct sockaddr_in *sin = (struct sockaddr_in*)rn->rn_key;
		ushort af;

		qprintf("   Leaf:");
#ifdef _HAVE_SA_LEN
		printafam(sin->sin_len, sin->sin_family);
		af = sin->sin_family;
#else
		af = (sin->sin_family & 0xFF);
		printafam(sin->sin_family>>8, af); /* len, af_family */
#endif
		qprintf(" rn_key=0x%p, netmask=0x%x, Dupedkey=0x%x\n",
			rn->rn_key, rn->rn_mask, rn->rn_dupedkey);
		in_addr_to_string(
	(struct in_addr *)(&(((struct sockaddr_in*)rn->rn_key)->sin_addr.s_addr)),
			buf);
		qprintf("         ip_addr %s\n", buf);
	}
	return;
}

void
print_radixtree(struct radix_node *rn)
{
	(void)print_radixnode(rn);
	if (rn->rn_b > 0) {
		if (rn->rn_l)
			(void)print_radixtree(rn->rn_l);
		if (rn->rn_r)
			(void)print_radixtree(rn->rn_r);
	}
	return;
}

void
print_snoopfilter(struct snoopfilter *sfp, int index)
{
	register unsigned short i;

	if (index != -1)
		qprintf("   Snoopfilter table entry %d\n", index);

	qprintf("   sf_allocated %d, sf_active %d, sf_promisc %d\n",
	  sfp->sf_allocated, sfp->sf_active, sfp->sf_promisc);

	qprintf("   sf_allmulti %d, sf_index %d, sf_port %d\n",
	  sfp->sf_allmulti, sfp->sf_index, sfp->sf_port);

	for (i=0; i < SNOOP_FILTERLEN; i=i+3) {
		if (i >= SNOOP_FILTERLEN) break;
		qprintf("   sf_mask[%d,%d,%d] 0x%x 0x%x, 0x%x\n",
			i, i+1, i+2,
			sfp->sf_mask[i], sfp->sf_mask[i+1], sfp->sf_mask[i+2]);
	}

	for (i=0; i < SNOOP_FILTERLEN; i=i+3) {
		if (i >= SNOOP_FILTERLEN) break;
		qprintf("   sf_match[%d,%d,%d] 0x%x 0x%x, 0x%x\n",
			i, i+1, i+2,
			sfp->sf_match[i], sfp->sf_match[i+1],
			sfp->sf_match[i+2]);
	}
	qprintf("\n");
	return;
}

/*
 *  kp protosw addr
 *	where  addr < 0  -- print a protocol switch table entry
 */
void
idbg_protosw(__psint_t addr)
{
    register struct protosw *pr = (struct protosw *)addr;

    if (addr == -1L) {
	qprintf("No protosw address specified\n");
	return;
    }

    if (addr < 0) {

	qprintf("  pr_type %d, pr_domain 0x%x, pr_protocol %d\n",
		pr->pr_type, pr->pr_domain, pr->pr_protocol);

	qprintf("  pr_flags 0x%x \n", pr->pr_flags);
	_printflags(pr->pr_flags, protoswfamily, 0); qprintf("\n");

	qprintf("  pr_input 0x%x, pr_output 0x%x\n",
		pr->pr_input, pr->pr_output);
	qprintf("  pr_ctlinput 0x%x, pr_ctloutput 0x%x, pr_usrreq 0x%x\n",
		pr->pr_ctlinput, pr->pr_ctloutput, pr->pr_usrreq);

	qprintf("  pr_init 0x%x, pr_fasttimo 0x%x, pr_slowtimo 0x%x, pr_drain 0x%x\n",
		pr->pr_init, pr->pr_fasttimo, pr->pr_slowtimo, pr->pr_drain);
	}
	return;
}

/*
 *  kp rawcb addr
 *	where  addr < 0  -- print the rawif struct contents
 */
void
idbg_rawcb(__psint_t addr)
{
    register struct rawcb *rcb = (struct rawcb *)addr;

    if (addr == -1L) {
	qprintf("No rawcb specified\n");
	return;
    }

    if (addr < 0) {
	qprintf("  rcb_next 0x%x, rcb_prev 0x%x, rcb_socket 0x%x\n",
		rcb->rcb_next, rcb->rcb_prev, rcb->rcb_socket);

	print_sockaddr("  rcb_faddr ",rcb->rcb_faddr);

	print_sockaddr("  rcb_laddr ",rcb->rcb_laddr);

	qprintf("  rcb_proto: sp_family ");
	printafam(0, rcb->rcb_proto.sp_family);
	qprintf(" sp_protocol %d\n", rcb->rcb_proto.sp_protocol);

	qprintf("  rcb_pcb 0x%x rcb_options(mbuf) 0x%x\n",
		rcb->rcb_pcb, rcb->rcb_options);

	qprintf("  rcb_route "); print_route(&(rcb->rcb_route));

	qprintf("  rcb_flags 0x%x ", rcb->rcb_flags);

	qprintf("  rcb_refcnt %d, rcb_moptions(mbuf) 0x%x\n",
		rcb->rcb_refcnt, rcb->rcb_moptions);
      }
	return;
}

/*
 *  kp rawif addr
 *	where  addr < 0  -- print the rawif struct contents
 */
void
idbg_rawif(__psint_t addr)
{
    register struct rawif *rp = (struct rawif *)addr;
    register unsigned short i;
    u_char bchar;

    if (addr == -1L) {
	qprintf("No rawif specified\n");
	return;
    }

    if (addr < 0) {

	qprintf("  rif_next 0x%x rif_ifp 0x%x\n", rp->rif_next, rp->rif_ifp);

	print_sockaddr_raw("  rif_name: ",&(rp->rif_name));

	qprintf("  rif_addr 0x%x, rif_broadaddr 0x%x, rif_addrlen %d\n",
		rp->rif_addr, rp->rif_broadaddr, rp->rif_addrlen);

	if (rp->rif_addr && rp->rif_addrlen) {
		qprintf("  rif_addr: 0x");
		for (i=0; i < rp->rif_addrlen; i++) {
			bchar = *((u_char *)(rp->rif_addr + i));
			qprintf("%x", bchar);
		}
		qprintf("\n");
	}
	if (rp->rif_broadaddr && rp->rif_addrlen) {
		qprintf("  rif_broadaddr: 0x");
		for (i=0; i < rp->rif_addrlen; i++) {
			bchar = *((u_char *)(rp->rif_broadaddr + i));
			qprintf("%x", bchar);
		}
		qprintf("\n");
	}

	qprintf("  rif_hdrlen %d, rif_hdrpad %d rif_padhdrlen %d\n",
		rp->rif_hdrlen, rp->rif_hdrpad, rp->rif_padhdrlen);
	qprintf("  rif_hdrgrains %d rif_srcoff %d rif_dstoff %d\n",
		rp->rif_hdrgrains, rp->rif_srcoff, rp->rif_dstoff);

	qprintf("  rif_stats\n");
	qprintf("    snoopstats: ss_seq %d ss_ifdrops %d ss_sbdrops %d\n",
		rp->rif_stats.rs_snoop.ss_seq,
		rp->rif_stats.rs_snoop.ss_ifdrops,
		rp->rif_stats.rs_snoop.ss_sbdrops);

	qprintf("    drainstats: ds_ifdrops %d ds_sbdrops %d\n",
		rp->rif_stats.rs_drain.ds_ifdrops,
		rp->rif_stats.rs_drain.ds_sbdrops);

	qprintf("  rif_open socket counts\n");
	for (i=0; i < RAWPROTO_MAX; i++) {
		qprintf(" rif_open[%s(%d)], openct %d\n",
			rawfamily[i], i, rp->rif_open[i]);
	}
	qprintf("\n");

	qprintf("  rif_snooplen %d rif_errsnoop %d rif_errport %d\n",
		rp->rif_snooplen, rp->rif_errsnoop, rp->rif_errport);

	qprintf("  rif_promisc %d rif_allmulti %d\n",
		rp->rif_promisc, rp->rif_allmulti);

	qprintf("  Snoopfilters: rif_sfveclen %d\n", rp->rif_sfveclen);

	for (i=0; i < SNOOP_MAXFILTERS; i++) {
		print_snoopfilter(&(rp->rif_sfvec[i]), i);
	}

	qprintf("  Snoopfilter matching last packet 0x%x\n", rp->rif_matched);
	if (rp->rif_matched) {
		print_snoopfilter(rp->rif_matched, -1);
	}

	qprintf("  Drain protocol state dm_minport %d, dm_maxport %d, dm_toport %d\n",
		rp->rif_drainmap.dm_minport,
		rp->rif_drainmap.dm_maxport,
		rp->rif_drainmap.dm_toport);
	}
	return;
}

/*
 *  kp hashtable addr
 *	where addr < 0  -- print the hashtable structure
 */
void
idbg_hashtable(__psint_t addr)
{
	struct hashtable *ht;
	struct hashbucket *h, *head;
	int i;

	if (addr == -1L) {
		qprintf("No hashtable address specified\n");
		return;
	} else {

		ht = (struct hashtable *)addr;
		if (addr >= 0) {
			qprintf("Invalid hashtable address specified\n");
			return;
		}
	}
	for (i=0; i < HASHTABLE_SIZE; i++) {

		head = (struct hashbucket *)(&ht[i].next);

		if (ht[i].next != head) { /* something on chain */
		  qprintf(" HTSlot %d(0x%x), bucket_p 0x%x, mrsplock_p 0x%x\n",
			i, i, head, &(ht[i].lock));
		}

		for (h=ht[i].next; h != head; h = h->next) {

			if (h->flags & HTF_INET) {
				print_in_ifaddr((struct in_ifaddr *)h);
			} else {
				if (h->flags & HTF_BROADCAST) {
					print_in_bcast((struct in_bcast *)h);
				} else {
					if (h->flags & HTF_MULTICAST) {
						print_in_multi_list(
							(struct in_multi *)h);
					} else {
						qprintf(
						" Unknown hash bucket type\n");
					}
				}
			}
		}
	}
	return;
}

/*
 *  kp ifaddr addr
 *	where addr < 0  -- print the ifaddr structure list
 *	where addr is ABSENT we walk and print the global struct in_ifaddr list
 */
void
idbg_ifaddr(__psint_t addr)
{
	register struct ifaddr *ifa;

	if (addr == -1L) {
		qprintf("No ifaddr specified\n");
	} else {
		ifa = (struct ifaddr *)addr;
		if (addr < 0) {
			print_ifaddrlist(ifa);
		} else {
			qprintf("Invalid ifaddr specified\n");
		}
	}
	return;
}

/*
 *  kp ifaddrlist addr
 *	where addr < 0  -- print the ifaddr structure list
 *	where addr is ABSENT we walk and print the global struct in_ifaddr list
 */
void
idbg_ifaddrlist(__psint_t addr)
{
    register struct ifaddr *ifa;

    if (addr == -1L) {
        qprintf("No ifaddr specified\n");
    } else {

        ifa = (struct ifaddr *)addr;
        if (addr < 0) {
            print_ifaddrlist(ifa);
        } else {
            qprintf("Invalid ifaddr specified\n");
        }
    }
    return;
}

/*
 *  kp ifnet addr
 *	where  addr < 0  -- print the ifnet struct contents
 */
void
idbg_ifnet(__psint_t addr)
{
    register struct ifnet *ifp = (struct ifnet *)addr;

    if (addr == -1L) {
	qprintf("No ifnet specified\n");
	return;
    }

    if (addr < 0) {

	qprintf("  if_name \"%s\" if_unit %d if_mtu %d if_next 0x%x\n",
		ifp->if_name, ifp->if_unit, ifp->if_mtu, ifp->if_next);

	qprintf("  if_flags 0x%x ", ifp->if_flags);
	_printflags(ifp->if_flags, ifflags, 0);
	qprintf("\n");

	qprintf("  if_index %d, if_timer %d, if_type %d\n",
		ifp->if_index, ifp->if_timer, ifp->if_type);

	qprintf("  if_addrlen %d, if_hdrlen %d, in_ifaddr 0x%x\n",
		ifp->if_addrlen, ifp->if_hdrlen, ifp->in_ifaddr);

#ifdef INET6
	qprintf("  in6_ifaddr 0x%x\n", ifp->in6_ifaddr);
	qprintf("  if_ndtype 0x%x  in6_ifaddr 0x%x\n", ifp->if_ndtype,
		ifp->if_site6);
#endif

	qprintf("  if_metric %d(0x%x), if_baudrate %d(0x%x)\n",
		ifp->if_metric, ifp->if_metric,
		if_getbaud(ifp->if_baudrate), if_getbaud(ifp->if_baudrate));

	qprintf("  if_snd.ifq_head 0x%x ifq_tail 0x%x\n",
		ifp->if_snd.ifq_head, ifp->if_snd.ifq_tail);
	qprintf("        ifq_len %d ifq_maxlen %d ifq_drops %d\n",
	  ifp->if_snd.ifq_len, ifp->if_snd.ifq_maxlen, ifp->if_snd.ifq_drops);
	qprintf("  if_snd: ifq_lock addr= 0x%x value= 0x%x\n",
		&(ifp->if_snd.ifq_lock), ifp->if_snd.ifq_lock);
	qprintf("  proc addrs: if_output 0x%x if_ioctl 0x%x\n",
		ifp->if_output, ifp->if_ioctl);
	qprintf("  proc addrs: if_reset 0x%x if_watchdog 0x%x\n",
		ifp->if_reset, ifp->if_watchdog);
	qprintf("  proc addrs: if_rtrequest 0x%x\n", ifp->if_rtrequest);

	qprintf("  if_data interface statistics\n");
	qprintf("    if_ipackets %d if_ierrors %d if_opackets %d\n",
		ifp->if_ipackets, ifp->if_ierrors, ifp->if_opackets);
	qprintf("    if_oerrors %d if_collisions %d\n",
		ifp->if_ierrors, ifp->if_collisions);

	qprintf("  if_data MIB-II statistics\n");
	qprintf("    if_lastchange %d if_ibytes %d if_obytes %d\n",
		ifp->if_lastchange, ifp->if_ibytes, ifp->if_obytes);
	qprintf("    if_imcasts %d if_omcasts %d if_iqdrops %d\n",
		ifp->if_imcasts, ifp->if_omcasts, ifp->if_iqdrops);
	qprintf("    if_noproto %d if_odrops %d\n",
		ifp->if_noproto, ifp->if_odrops);

	qprintf("  if_sec 0x%x\n", ifp->if_sec);

	qprintf("  if_mutex addr 0x%x\n", &(ifp->if_mutex));
	qprintf("  if_recvspace %u, if_sendspace %u\n", ifp->if_recvspace, 
		ifp->if_sendspace);
    }
}

/*
 *  kp ifreq addr
 *	where  addr < 0  -- print the ifreq structure
 */
void
idbg_ifreq(__psint_t addr)
{
    register struct ifreq *ifp = (struct ifreq *)addr;

    if (addr == -1L) {
	qprintf("No ifreq specified\n");
    } else {

      if (addr < 0) {

          ifp = (struct ifreq *)addr;
          qprintf(" ifreqp 0x%x, ifr_name %s\n", ifp, ifp->ifr_name);
          print_sockaddr(" ifr_addr ", &(ifp->ifr_addr));
          qprintf(" ifru_flags 0x%x, ifru_metric %d\n",
		ifp->ifr_flags, ifp->ifr_metric);
      }
    }
}

/*
 *  kp inalias addr
 *	where addr < 0  -- print the struct in_alias
 */
void
idbg_in_alias(__psint_t addr)
{
    register struct in_aliasreq *ia;

    if (addr == -1L) {
            qprintf("Invalid in_alias specified\n");
    } else {
        if (addr < 0) {
		ia = (struct in_aliasreq *)addr;

		qprintf(" in_alias 0x%x, ifra_name %s\n", ia, ia->ifra_name);
		print_sockaddr("  ifra_addr ",
			       (struct sockaddr *)(&(ia->ifra_addr)));
		print_sockaddr("  ifra_broadaddr ",
			       (struct sockaddr *)(&(ia->ifra_broadaddr)));
		print_sockaddr("  ifra_mask ",
			       (struct sockaddr *)(&(ia->ifra_mask)));
        } else {
		qprintf("Invalid in_alias specified\n");
        }
    }
    return;
}

/*
 *  kp inbcast addr
 *	where addr < 0  -- print the struct in_bcast
 */
void
idbg_in_bcast(__psint_t addr)
{
    register struct in_bcast *ib;

    if (addr == -1L) {
            qprintf("Invalid in_bcast specified\n");
    } else {
        if (addr < 0) {
		ib = (struct in_bcast *)addr;
		print_in_bcast(ib);
        } else {
		qprintf("Invalid in_bcast specified\n");
        }
    }
    return;
}

/*
 *  kp inifaddr addr
 *	where addr < 0  -- print the struct in_ifaddr
 *	where addr is ABSENT we walk and print the global struct in_ifaddr list
 */
void
idbg_in_ifaddr(__psint_t addr)
{
    register struct in_ifaddr *ia;

    if (addr == -1L) {
        qprintf("No in_ifaddr address specified\n");
    } else {

        ia = (struct in_ifaddr *)addr;
        if (addr < 0) {
            print_in_ifaddr(ia);
        } else {
            qprintf("Invalid in_ifaddr specified\n");
        }
    }
    return;
}

/*
 *  kp inmulti addr
 *	where addr < 0  -- print the struct in_multi
 *	where addr is ABSENT we walk and print the list of struct in_multi's
 */
void
idbg_in_multi(__psint_t addr)
{
    register struct in_multi *inm;

    if (addr == -1L) {
        qprintf("No in_multi address specified\n");
    } else {

        inm = (struct in_multi *)addr;
        if (addr < 0) {
            print_in_multi_list(inm);
        } else {
            qprintf("Invalid in_multi specified\n");
        }
    }
    return;
}

void
idbg_arpcom(__psint_t addr)
{
    if (addr == -1L) {
	qprintf("No arpcom structure address specified\n");
	return;
    }
    if (addr < 0) {
	register struct arpcom *ac = (struct arpcom *)addr;

	print_arpcom(ac);
    }
    return;
}

/*
 *  kp ckna cpuid
 *  Where 'cpuid' is the small integer designating a particular cpus desired.
 *
 *  For each cpu print the address of the kna structure which holds the
 *  tcp/ip statistics blocks.
 *  Alternatively 'cpuid' is the index into the pdaindr array of structures
 *  for the particular cpu desired.
 *  The entire network statistics for a cpu can be printed out via the 
 *  'kp cnetstats' command supplying the kna address for this cpu.
 */
void
idbg_ckna(__psint_t addr)
{
	register struct kna *kna_ptr;
	register int i, min, max;

	if (addr == -1L) { /* no cpuid specified so dump them all */
		min = 0;
		max = (int)maxcpus;
	} else {
		min = addr;
		max = min + 1;

		if (min < 0) {
			min = 0;
			max = maxcpus;
		} else {
			if (max > (u_short)maxcpus) {
				min = 0;
				max = maxcpus;
			}
		}
	}
	for (i = min; i < max; i++) {

		kna_ptr = (struct kna *)pdaindr[i].pda->knaptr;

		qprintf("  pdaindr ix %d, cpuid %d, knaptr 0x%x\n",
			i, pdaindr[i].CpuId, kna_ptr);
		qprintf("   ipstat 0x%x, udpstat 0x%x\n",
				&(kna_ptr->ipstat), &(kna_ptr->udpstat));
		qprintf("   tcpstat 0x%x\n", &(kna_ptr->tcpstat));
		qprintf("   icmpstat 0x%x, igmpstat 0x%x\n",
			&(kna_ptr->icmpstat), &(kna_ptr->igmpstat));
		qprintf("   inpcbstat 0x%x, mrtstat 0x%x\n",
			&(kna_ptr->pcbstat), &(kna_ptr->mrtstat));
	}
	return;
}

void
idbg_etherarp(__psint_t addr)
{
    if (addr == -1L) {
	qprintf("No ether arp structure address specified\n");
	return;
    }
    if (addr < 0) {
	register struct ether_arp *ar = (struct ether_arp *)addr;

	print_etherarp(ar);
    }
    return;
}

void
idbg_llinfo(__psint_t addr)
{
    struct llinfo_arp *la;

    if (addr == -1L) {
	/*
	 * Print all of the llinfo_arp structures
	 */
	for (la = llinfo_arp.la_next; la != &llinfo_arp; la = la->la_next) {
		print_llinfoarp(la);
	}
    } else if (addr < 0) {
	la = (struct llinfo_arp *)addr;
	/*
	 * print a specified llinfo_arp structure
	 */
	print_llinfoarp(la);
    } else {
	qprintf("Invalid llinfo_arp address\n");
    }
    return;
}

/*
 *  Print the ip statistics
 */
void
idbg_ipstats(__psint_t addr)
{
	register struct ipstat *ip;

	if (addr == -1L) { /* no address specified so exit */
		qprintf(" no ipstruct address supplied\n");
		return;
	}

	ip = (struct ipstat *)addr;

qprintf("  ips_total %d, ips_badsum %d, ips_tooshort %d, ips_toosmall %d\n",
		ip->ips_total, ip->ips_badsum,
		ip->ips_tooshort, ip->ips_toosmall);

qprintf("  ips_badhlen %d, ips_badhlen %d, ips_fragments %d, ips_fragdropped %d\n",
		ip->ips_badhlen, ip->ips_badlen,
		ip->ips_fragments, ip->ips_fragdropped);

qprintf("  ips_fragtimeout %d, ips_forward %d, ips_cantforward %d, ips_redirectsent %d\n",
		ip->ips_fragtimeout, ip->ips_forward,
		ip->ips_cantforward, ip->ips_redirectsent);

qprintf("  ips_noproto %d, ips_delivered %d, ips_localout %d, ips_odropped %d\n",
		ip->ips_noproto, ip->ips_delivered,
		ip->ips_localout, ip->ips_odropped);

qprintf("  ips_reassembled %d, ips_fragmented %d, ips_ofragments %d, ips_cantfrag %d\n",
		ip->ips_reassembled, ip->ips_fragmented,
		ip->ips_ofragments, ip->ips_cantfrag);

	qprintf("  ips_badoptions %d, ips_noroute %d\n",
		ip->ips_badoptions, ip->ips_noroute);
	return;
}

/*
 *  Print the multicast routing statistics
 */
void
idbg_mrtstats(__psint_t addr)
{
	register struct mrtstat *mr;

	if (addr == -1L) { /* no address specified so exit */
		qprintf(" no mrtstat address supplied\n");
		return;
	}

	mr = (struct mrtstat *)addr;

qprintf("  mfc_lookups %d, mfc_misses %d, upcalls %d, no_route %d\n",
		mr->mrts_mfc_lookups, mr->mrts_mfc_misses,
		mr->mrts_upcalls, mr->mrts_no_route);

qprintf("  bad_tunnel %d, cant_tunnel %d, wrong_if %d, upq_ovflw %d\n",
		mr->mrts_bad_tunnel, mr->mrts_cant_tunnel,
		mr->mrts_wrong_if, mr->mrts_upq_ovflw);

qprintf("  cache_cleanups %d, drop_sel %d, q_overflow %d, pkt2large %d\n",
		mr->mrts_cache_cleanups, mr->mrts_drop_sel,
		mr->mrts_q_overflow, mr->mrts_pkt2large);

	qprintf("  upq_sockfull %d\n", mr->mrts_upq_sockfull);
	return;
}

/* mbuf cluster field shorthands */
#define	m_p		m_u.m_us.mu_p
#define m_refp		m_u.m_us.mu_refp
#define m_ref		m_u.m_us.mu_ref
#define m_size		m_u.m_us.mu_size

/*
 * Print an mbuf.
 */
void
idbg_mbuf(__psint_t addr)
{
	struct mbuf *m = (struct mbuf *) addr;

	if (addr == -1L) {
		qprintf("No mbuf specified\n");
		return;
	}

	qprintf("m 0x%x type %d (%s), ", m, m->m_type,
		((unsigned short) m->m_type >=
		(sizeof(mbufTypes)/sizeof(mbufTypes[0]))) ?
		"?" : mbufTypes[m->m_type]);

#if (_MIPS_SZPTR == 64)
	qprintf(" node %d ", m->m_index);
#endif /* _MIPS_SZPTR == 64 */

	qprintf("m_flags %b\n", m->m_flags,
	"\20\1M_CLUSTER\2M_CKSUMMED\3M_BCAST\4M_MCAST\5M_SHARED\6M_LOANED");

	qprintf("len %d, offset %d (0x%x), mtod 0x%x\n",
		m->m_len, m->m_off, m->m_off, mtod(m, __psint_t));
	qprintf("  next 0x%x, act 0x%x\n", m->m_next, m->m_act);
	qprintf("  freefunc 0x%x, farg 0x%x, dupfunc 0x%x, darg 0x%x\n",
		m->m_freefunc, m->m_farg, m->m_dupfunc, m->m_darg);
	qprintf("  p 0x%x refp 0x%x ref %d size %d\n",
		m->m_p, m->m_refp, m->m_ref, m->m_size);
}

extern uint mbdrain, mbshake, mbpages, mbmaxpages;

extern struct mb *mbcache[MBMAXCPU];
extern int m_count(struct mbuf *);

/*
 * Print the mb structures.
 */
/* ARGSUSED */
void
idbg_mbcache(__psint_t addr)
{
	struct mb *mb;
	int i;

	qprintf("num nodes %d, mbpages %d, mbmaxpages %d\n",
		numnodes, mbpages, mbmaxpages);
	qprintf("mbdrain %d, mbshake %d\n", mbdrain, mbshake);
	qprintf("\n");

	for (i = 0; i < MBNUM; i++) {

		mb = mbcache[i];
		/* intentionally don't attempt to acquire mb_lock spinlock */

		qprintf(" mbcache[%d] 0x%x, mb_index %d, mb_lock 0x%x\n",
			i, mb, mb->mb_index, mb->mb_lock);
		qprintf("  mb_sml 0x%x, smlfree %d, mb_smltot %d\n",
			mb->mb_sml, m_count(mb->mb_sml), mb->mb_smltot);
		qprintf("  mb_med 0x%x, medfree %d, mb_medtot %d\n",
			mb->mb_med, m_count(mb->mb_med), mb->mb_medtot);
		qprintf("  mb_big 0x%x, bigfree %d, mb_bigtot %d\n",
			mb->mb_big, m_count(mb->mb_big), mb->mb_bigtot);

		qprintf("  mb_smlfree %d, mb_medfree %d, mb_bigfree %d\n",
			mb->mb_smlfree, mb->mb_medfree, mb->mb_bigfree);
		qprintf("  mb_drops %d, mb_wait %d\n",
			mb->mb_drops, mb->mb_wait);
		qprintf("  mbpcbtot %d, mbpcbbytes %d, mcbtot %d, mcbbytes %d\n",
			mb->mb_pcbtot, mb->mb_pcbbytes, mb->mb_mcbtot, 
			mb->mb_mcbbytes);

		qprintf("  mb_smlzone 0x%x, zone_total_pages %d\n",
			mb->mb_smlzone, mb->mb_smlzone->zone_total_pages);
		qprintf("  mb_medzone 0x%x, zone_total_pages %d\n",
			mb->mb_medzone, mb->mb_medzone->zone_total_pages);
		qprintf("  mb_sv 0x%x\n", &(mb->mb_sv));
	}
}

extern void m_allocstats(struct mbstat *);
/*
 *  Print the mbuf statistics
 */
/* ARGSUSED */
void
idbg_mbstats(__psint_t addr)
{
	struct mbstat mbstat;
	int total;
	u_short i;

	m_allocstats(&mbstat);

	total = 0;
	for (i=0; i < MT_MAX; i++) {
		total += mbstat.m_mtypes[i];
	}
	qprintf("  mbufs alloc %d, m_mbufs %d, m_clusters %d, m_clfree %d\n",
		total, mbstat.m_mbufs, mbstat.m_clusters, mbstat.m_clfree);
	qprintf("  m_drops %d, m_wait %d, m_drain %d\n",
		mbstat.m_drops, mbstat.m_wait, mbstat.m_drain);

	i = 0;
	while (i < MT_MAX) {
		qprintf("  type %s(%d) cnt %d; type %s(%d) cnt %d\n",
			mbufTypes[i], i, mbstat.m_mtypes[i],
			mbufTypes[i+1], i+1, mbstat.m_mtypes[i+1]);
		i += 2;
	}
	return;
}

extern int m_shake(int level);

/* ARGSUSED */
void
idbg_mshake(__psint_t addr)
{
	m_shake(SHAKEMGR_MEMORY);
}

#if defined(DEBUG) && defined(DEBUG_MBUF_TIMING) && defined(R10000)
/*
 * External functions to get R10K cycle counts and cache invalidates
 */
extern uint _get_count(void);
extern void __cache_wb_inval(void *address, long length);

/*
 *  Commmon mbuf test code
 *  Execute and print execution time of various combinations of mbuf interfaces
 */
extern struct mbuf *m_get(int, int);
extern struct mbuf *m_free(struct mbuf *);

#define IDBG_MBUF_MINCT 10
#define IDBG_MBUF_MAXCT 50

int mbuf_cycle_int = 5555;		/* LEGO 180 MHz */
/* int mbuf_cycle_int = 5555;*/		/* LEGO 180 MHz OR SPEEDO 180 MHz */
/* int mbuf_cycle_int = 5130;*/		/* LEGO 195 MHz */

void
idbg_mbuf_tstc(int invalidate, int mbuf_loop_ct)
{
	struct mbuf *m;
	int i, s;
	long getavg, getmin, getmax;
	long freeavg, freemin, freemax, get, get_end, free, free_end;
	long get_times[IDBG_MBUF_MAXCT], free_times[IDBG_MBUF_MAXCT];

	for (i = 0; i < IDBG_MBUF_MAXCT; i++) {
		get_times[i] = free_times[i] = 0;
	}

	if (invalidate == 0) {

		freemin = getmin = 999999;
		freemax = getmax = getavg = freeavg = 0;

		for (i = 0; i < mbuf_loop_ct; i++) {

			s = splhi();
			get = (long)(_get_count());
			m = m_get(M_DONTWAIT, MT_DATA);
			get_end = (long)(_get_count());
			splx(s);
			get_times[i] = get = (get_end - get);

			if (get < getmin) {
				getmin = get;
			}
			if (get > getmax) {
				getmax = get;
			}
			getavg += get;

			s = splhi();
			free = (long)(_get_count());
			m_free(m);
			free_end = (long)(_get_count());
			splx(s);
			free_times[i] = free = (free_end - free);

			if (free < freemin) {
				freemin = free;
			}
			if (free > freemax) {
				freemax = free;
			}
			freeavg += free;
		}
		getavg /= mbuf_loop_ct;
		freeavg /= mbuf_loop_ct;

		qprintf("Warm Cache: Loop count %d\n", mbuf_loop_ct);

		qprintf(" m_get MLEN bytes; min %d, max %d, avg %d\n",
			(getmin * mbuf_cycle_int) / 1000,
			(getmax * mbuf_cycle_int) / 1000,
			(getavg * mbuf_cycle_int) / 1000);

		for (i = 0; i < mbuf_loop_ct; i++) {
			get_times[i] = (get_times[i] * mbuf_cycle_int)/1000;
		}
		for (i = 0; i < mbuf_loop_ct; i+=10) {
		  qprintf("  m_get times: %d %d %d %d %d %d %d %d %d %d\n",
			get_times[i], get_times[i+1], get_times[i+2],
			get_times[i+3], get_times[i+4], get_times[i+5],
			get_times[i+6], get_times[i+7], get_times[i+8],
			get_times[i+9]);
		}

		qprintf(" m_free MLEN Bytes; min %d, max %d, avg %d\n",
			(freemin * mbuf_cycle_int) / 1000,
			(freemax * mbuf_cycle_int) / 1000,
			(freeavg * mbuf_cycle_int) / 1000);

		for (i = 0; i < mbuf_loop_ct; i++) {
			free_times[i] = (free_times[i] * mbuf_cycle_int)/1000;
		}
		for (i = 0; i < mbuf_loop_ct; i+=10) {
		  qprintf("  m_free times: %d %d %d %d %d %d %d %d %d %d\n",
			free_times[i], free_times[i+1], free_times[i+2],
			free_times[i+3], free_times[i+4], free_times[i+5],
			free_times[i+6], free_times[i+7], free_times[i+8],
			free_times[i+9]);
		}

		qprintf("\n");

		freemin = getmin = 999999;
		freemax = getmax = getavg = freeavg = 0;

		for (i = 0; i < mbuf_loop_ct; i++) {

			s = splhi();
			get = (long)(_get_count());
			m = m_vget(M_DONTWAIT, 2048, MT_DATA);
			get_end = (long)(_get_count());
			splx(s);
			get_times[i] = get = (get_end - get);

			if (get < getmin) {
				getmin = get;
			}
			if (get > getmax) {
				getmax = get;
			}
			getavg += get;

			s = splhi();
			free = (long)(_get_count());
			m_free(m);
			free_end = (long)(_get_count());
			splx(s);
			free_times[i] = free = (free_end - free);

			if (free < freemin) {
				freemin = free;
			}
			if (free > freemax) {
				freemax = free;
			}
			freeavg += free;
		}

		getavg /= mbuf_loop_ct;
		freeavg /= mbuf_loop_ct;

		qprintf(" m_vget 2048 bytes; min %d, max %d, avg %d\n",
			(getmin * mbuf_cycle_int) / 1000,
			(getmax * mbuf_cycle_int) / 1000,
			(getavg * mbuf_cycle_int) / 1000);

		for (i = 0; i < mbuf_loop_ct; i++) {
			get_times[i] = (get_times[i] * mbuf_cycle_int)/1000;
		}
		for (i = 0; i < mbuf_loop_ct; i+=10) {
		  qprintf("  m_vget times: %d %d %d %d %d %d %d %d %d %d\n",
			get_times[i], get_times[i+1], get_times[i+2],
			get_times[i+3], get_times[i+4], get_times[i+5],
			get_times[i+6], get_times[i+7], get_times[i+8],
			get_times[i+9]);
		}

		qprintf(" m_free 2048 bytes; min %d, max %d, avg %d\n",
			(freemin * mbuf_cycle_int) / 1000,
			(freemax * mbuf_cycle_int) / 1000,
			(freeavg * mbuf_cycle_int) / 1000);
		for (i = 0; i < mbuf_loop_ct; i++) {
			free_times[i] = (free_times[i] * mbuf_cycle_int)/1000;
		}
		for (i = 0; i < mbuf_loop_ct; i+=10) {
		  qprintf("  m_free times: %d %d %d %d %d %d %d %d %d %d\n",
			free_times[i], free_times[i+1], free_times[i+2],
			free_times[i+3], free_times[i+4], free_times[i+5],
			free_times[i+6], free_times[i+7], free_times[i+8],
			free_times[i+9]);
		}
		qprintf("\n");

		freemin = getmin = 999999;
		freemax = getmax = getavg = freeavg = 0;

		for (i = 0; i < mbuf_loop_ct; i++) {

			s = splhi();
			get = (long)(_get_count());
			m = m_vget(M_DONTWAIT, NBPP, MT_DATA);
			get_end = (long)(_get_count());
			splx(s);
			get_times[i] = get = (get_end - get);

			if (get < getmin) {
				getmin = get;
			}
			if (get > getmax) {
				getmax = get;
			}
			getavg += get;

			s = splhi();
			free = (long)(_get_count());
			m_free(m);
			free_end = (long)(_get_count());
			splx(s);
			free_times[i] = free = (free_end - free);

			if (free < freemin) {
				freemin = free;
			}
			if (free > freemax) {
				freemax = free;
			}
			freeavg += free;
		}

		getavg /= mbuf_loop_ct;
		freeavg /= mbuf_loop_ct;

		qprintf(" m_vget NBPP bytes; min %d, max %d, avg %d\n",
			(getmin * mbuf_cycle_int) / 1000,
			(getmax * mbuf_cycle_int) / 1000 ,
			(getavg * mbuf_cycle_int) / 1000);

		for (i = 0; i < mbuf_loop_ct; i++) {
			get_times[i] = (get_times[i] * mbuf_cycle_int)/1000;
		}
		for (i = 0; i < mbuf_loop_ct; i+=10) {
		  qprintf("  m_vget times: %d %d %d %d %d %d %d %d %d %d\n",
			get_times[i], get_times[i+1], get_times[i+2],
			get_times[i+3], get_times[i+4], get_times[i+5],
			get_times[i+6], get_times[i+7], get_times[i+8],
			get_times[i+9]);
		}

		qprintf(" m_free NBPP; min %d, max %d, avg %d\n",
			(freemin * mbuf_cycle_int) / 1000,
			(freemax * mbuf_cycle_int) / 1000,
			(freeavg * mbuf_cycle_int) / 1000);
		for (i = 0; i < mbuf_loop_ct; i++) {
			free_times[i] = (free_times[i] * mbuf_cycle_int)/1000;
		}
		for (i = 0; i < mbuf_loop_ct; i+=10) {
		  qprintf("  m_free times: %d %d %d %d %d %d %d %d %d %d\n",
			free_times[i], free_times[i+1], free_times[i+2],
			free_times[i+3], free_times[i+4], free_times[i+5],
			free_times[i+6], free_times[i+7], free_times[i+8],
			free_times[i+9]);
		}
	} else {
		freemin = getmin = 999999;
		freemax = getmax = getavg = freeavg = 0;

		for (i = 0; i < mbuf_loop_ct; i++) {

			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
			s = splhi();
			get = (long)(_get_count());
			m = m_get(M_DONTWAIT, MT_DATA);
			get_end = (long)(_get_count());
			splx(s);
			get_times[i] = get = (get_end - get);

			if (get < getmin) {
				getmin = get;
			}
			if (get > getmax) {
				getmax = get;
			}
			getavg += get;

			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
			s = splhi();
			free = (long)(_get_count());
			m_free(m);
			free_end = (long)(_get_count());
			splx(s);
			free_times[i] = free = (free_end - free);

			if (free < freemin) {
				freemin = free;
			}
			if (free > freemax) {
				freemax = free;
			}
			freeavg += free;
		}
		getavg /= mbuf_loop_ct;
		freeavg /= mbuf_loop_ct;

		qprintf("Cold Cache: Loop count %d\n", mbuf_loop_ct);
		qprintf(" m_get MLEN bytes; min %d, max %d, avg %d\n",
			(getmin * mbuf_cycle_int) / 1000,
			(getmax * mbuf_cycle_int) / 1000,
			(getavg * mbuf_cycle_int) / 1000);

		for (i = 0; i < mbuf_loop_ct; i++) {
			get_times[i] = (long)
			 ((get_times[i] * (long)mbuf_cycle_int)/(long)1000);
		}
		for (i = 0; i < mbuf_loop_ct; i+=10) {
		  qprintf("  m_get times: %d %d %d %d %d %d %d %d %d %d\n",
			get_times[i], get_times[i+1], get_times[i+2],
			get_times[i+3], get_times[i+4], get_times[i+5],
			get_times[i+6], get_times[i+7], get_times[i+8],
			get_times[i+9]);
		}

		qprintf(" m_free MLEN bytes; min %d, max %d, avg %d\n",
			(freemin * mbuf_cycle_int) / 1000,
			(freemax * mbuf_cycle_int) / 1000,
			(freeavg * mbuf_cycle_int) / 1000);
		for (i = 0; i < mbuf_loop_ct; i++) {
			free_times[i] = (free_times[i] * mbuf_cycle_int)/1000;
		}
		for (i = 0; i < mbuf_loop_ct; i+=10) {
		  qprintf("  m_free times: %d %d %d %d %d %d %d %d %d %d\n",
			free_times[i], free_times[i+1], free_times[i+2],
			free_times[i+3], free_times[i+4], free_times[i+5],
			free_times[i+6], free_times[i+7], free_times[i+8],
			free_times[i+9]);
		}
		qprintf("\n");

		freemin = getmin = 999999;
		freemax = getmax = getavg = freeavg = 0;

		for (i = 0; i < mbuf_loop_ct; i++) {

			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
			s = splhi();
			get = (long)(_get_count());
			m = m_vget(M_DONTWAIT, 2048, MT_DATA);
			get_end = (long)(_get_count());
			splx(s);
			get_times[i] = get = (get_end - get);

			if (get < getmin) {
				getmin = get;
			}
			if (get > getmax) {
				getmax = get;
			}
			getavg += get;

			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
			s = splhi();
			free = (long)(_get_count());
			m_free(m);
			free_end = (long)(_get_count());
			splx(s);
			free_times[i] = free = (free_end - free);

			if (free < freemin) {
				freemin = free;
			}
			if (free > freemax) {
				freemax = free;
			}
			freeavg += free;
		}
		getavg /= mbuf_loop_ct;
		freeavg /= mbuf_loop_ct;

		qprintf(" m_vget 2048 bytes; min %d, max %d, avg %d\n",
			(getmin * mbuf_cycle_int) / 1000,
			(getmax * mbuf_cycle_int) / 1000,
			(getavg * mbuf_cycle_int) / 1000);

		for (i = 0; i < mbuf_loop_ct; i++) {
			get_times[i] = (long)
			 ((get_times[i] * (long)mbuf_cycle_int)/(long)1000);
		}
		for (i = 0; i < mbuf_loop_ct; i+=10) {
		  qprintf("  m_vget times: %d %d %d %d %d %d %d %d %d %d\n",
			get_times[i], get_times[i+1], get_times[i+2],
			get_times[i+3], get_times[i+4], get_times[i+5],
			get_times[i+6], get_times[i+7], get_times[i+8],
			get_times[i+9]);
		}

		qprintf(" m_free 2048 bytes; min %d, max %d, avg %d\n",
			(freemin * mbuf_cycle_int) / 1000,
			(freemax * mbuf_cycle_int) / 1000,
			(freeavg * mbuf_cycle_int) / 1000);
		for (i = 0; i < mbuf_loop_ct; i++) {
			free_times[i] = (free_times[i] * mbuf_cycle_int)/1000;
		}
		for (i = 0; i < mbuf_loop_ct; i+=10) {
		  qprintf("  m_free times: %d %d %d %d %d %d %d %d %d %d\n",
			free_times[i], free_times[i+1], free_times[i+2],
			free_times[i+3], free_times[i+4], free_times[i+5],
			free_times[i+6], free_times[i+7], free_times[i+8],
			free_times[i+9]);
		}
		qprintf("\n");

		freemin = getmin = 999999;
		freemax = getmax = getavg = freeavg = 0;

		for (i = 0; i < mbuf_loop_ct; i++) {

			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
			s = splhi();
			get = (long)(_get_count());
			m = m_vget(M_DONTWAIT, NBPP, MT_DATA);
			get_end = (long)(_get_count());
			splx(s);
			get_times[i] = get = (get_end - get);

			if (get < getmin) {
				getmin = get;
			}
			if (get > getmax) {
				getmax = get;
			}
			getavg += get;

			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
			s = splhi();
			free = (long)(_get_count());
			m_free(m);
			free_end = (long)(_get_count());
			splx(s);
			free_times[i] = free = (free_end - free);

			if (free < freemin) {
				freemin = free;
			}
			if (free > freemax) {
				freemax = free;
			}
			freeavg += free;
		}
		getavg /= mbuf_loop_ct;
		freeavg /= mbuf_loop_ct;

		qprintf(" m_vget NBPP bytes; min %d, max %d, avg %d\n",
			(getmin * mbuf_cycle_int) / 1000,
			(getmax * mbuf_cycle_int) / 1000,
			(getavg * mbuf_cycle_int) / 1000);

		for (i = 0; i < mbuf_loop_ct; i++) {
			get_times[i] = (long)
			 ((get_times[i] * (long)mbuf_cycle_int)/(long)1000);
		}
		for (i = 0; i < mbuf_loop_ct; i+=10) {
		  qprintf("  m_vget times: %d %d %d %d %d %d %d %d %d %d\n",
			get_times[i], get_times[i+1], get_times[i+2],
			get_times[i+3], get_times[i+4], get_times[i+5],
			get_times[i+6], get_times[i+7], get_times[i+8],
			get_times[i+9]);
		}

		qprintf(" m_free NBPP bytes; min %d, max %d, avg %d\n",
			(freemin * mbuf_cycle_int) / 1000,
			(freemax * mbuf_cycle_int) / 1000,
			(freeavg * mbuf_cycle_int) / 1000);
		for (i = 0; i < mbuf_loop_ct; i++) {
			free_times[i] = (free_times[i] * mbuf_cycle_int)/1000;
		}
		for (i = 0; i < mbuf_loop_ct; i+=10) {
		  qprintf("  m_free times: %d %d %d %d %d %d %d %d %d %d\n",
			free_times[i], free_times[i+1], free_times[i+2],
			free_times[i+3], free_times[i+4], free_times[i+5],
			free_times[i+6], free_times[i+7], free_times[i+8],
			free_times[i+9]);
		}
	}
	return;
}

/*
 *  kp mbuft1 invalidateCache
 *  Execute and print execution time of mbuf interfaces
 */
void
idbg_mbuft1(__psint_t x)
{
	int cache = (x == 0) ? 0 : 1;

	idbg_mbuf_tstc(cache, IDBG_MBUF_MINCT);
	return;
}

/*
 *  kp mbuft2 invalidateCache
 *  Execute and print execution time of mbuf interfaces
 */
void
idbg_mbuft2(__psint_t x)
{
	int cache = (x == 0) ? 0 : 1;

	idbg_mbuf_tstc(cache, IDBG_MBUF_MAXCT);
	return;
}

/*
 *  kp mbuft3
 *  Execute and print execution times of paired m_free(m_get()) calls
 *  NOTE: Only warm cache execution timed.
 */
/* ARGSUSED */
void
idbg_mbuft3(__psint_t x)
{
	/* struct mbuf *m; */
	int i, s;
	long getavg, getmin, getmax, get, get_end;
	long get_times[IDBG_MBUF_MINCT];
	long mbuf_loop_ct = IDBG_MBUF_MINCT;

	getmin = 999999; getavg = getmax = 0;
	for (i = 0; i < mbuf_loop_ct; i++) {

		s = splhi();
		get = (long)(_get_count());
		m_free(m_get(M_DONTWAIT, MT_DATA));
		get_end = (long)(_get_count());
		splx(s);
		get_times[i] = get = (get_end - get);

		if (get < getmin) {
			getmin = get;
		}
		if (get > getmax) {
			getmax = get;
		}
		getavg += get;
	}
	getavg /= mbuf_loop_ct;

	qprintf("Warm Cache: Loop count %d\n", mbuf_loop_ct);

	qprintf(" m_free(m_get) Pair MLEN bytes; min %d, max %d, avg %d\n",
		(getmin * mbuf_cycle_int) / 1000,
		(getmax * mbuf_cycle_int) / 1000,
		(getavg * mbuf_cycle_int) / 1000);

	for (i = 0; i < mbuf_loop_ct; i++) {
	 get_times[i] = (long)
	   ((get_times[i] * (long)mbuf_cycle_int)/(long)1000);
	}
	for (i = 0; i < mbuf_loop_ct; i+=10) {
	qprintf("  m_free(m_get) Pair times: %d %d %d %d %d %d %d %d %d %d\n",
		get_times[i], get_times[i+1], get_times[i+2],
		get_times[i+3], get_times[i+4], get_times[i+5],
		get_times[i+6], get_times[i+7], get_times[i+8],
		get_times[i+9]);
	}
	qprintf("\n");

	getmin = 999999; getavg = getmax = 0;
	for (i = 0; i < mbuf_loop_ct; i++) {

		s = splhi();
		get = (long)(_get_count());
		m_free(m_vget(M_DONTWAIT, MLEN, MT_DATA));
		get_end = (long)(_get_count());
		splx(s);
		get_times[i] = get = (get_end - get);

		if (get < getmin) {
			getmin = get;
		}
		if (get > getmax) {
			getmax = get;
		}
		getavg += get;
	}
	getavg /= mbuf_loop_ct;

	qprintf(" m_free(m_vget) Pair MLEN bytes; min %d, max %d, avg %d\n",
		(getmin * mbuf_cycle_int) / 1000,
		(getmax * mbuf_cycle_int) / 1000,
		(getavg * mbuf_cycle_int) / 1000);

	for (i = 0; i < mbuf_loop_ct; i++) {
	 get_times[i] = (long)
	   ((get_times[i] * (long)mbuf_cycle_int)/(long)1000);
	}
	for (i = 0; i < mbuf_loop_ct; i+=10) {
	qprintf("  m_free(m_vget) Pair times: %d %d %d %d %d %d %d %d %d %d\n",
		get_times[i], get_times[i+1], get_times[i+2],
		get_times[i+3], get_times[i+4], get_times[i+5],
		get_times[i+6], get_times[i+7], get_times[i+8],
		get_times[i+9]);
	}
	qprintf("\n");

	getmin = 999999; getavg = getmax = 0;
	for (i = 0; i < mbuf_loop_ct; i++) {

		s = splhi();
		get = (long)(_get_count());
		m_free(m_vget(M_DONTWAIT, 2048, MT_DATA));
		get_end = (long)(_get_count());
		splx(s);
		get_times[i] = get = (get_end - get);

		if (get < getmin) {
			getmin = get;
		}
		if (get > getmax) {
			getmax = get;
		}
		getavg += get;
	}
	getavg /= mbuf_loop_ct;

	qprintf(" m_free(m_vget) Pair 2048 bytes; min %d, max %d, avg %d\n",
		(getmin * mbuf_cycle_int) / 1000,
		(getmax * mbuf_cycle_int) / 1000,
		(getavg * mbuf_cycle_int) / 1000);
	for (i = 0; i < mbuf_loop_ct; i++) {
		get_times[i] = (long)
			((get_times[i] * (long)mbuf_cycle_int)/(long)1000);
	}
	for (i = 0; i < mbuf_loop_ct; i+=10) {
   qprintf("  m_free(m_vget) Pair times: %d %d %d %d %d %d %d %d %d %d\n",
		get_times[i], get_times[i+1], get_times[i+2],
		get_times[i+3], get_times[i+4], get_times[i+5],
		get_times[i+6], get_times[i+7], get_times[i+8],
		get_times[i+9]);
	}
	qprintf("\n");

	getmin = 999999; getavg = getmax = 0;
	for (i = 0; i < mbuf_loop_ct; i++) {

		s = splhi();
		get = (long)(_get_count());
		m_free(m_vget(M_DONTWAIT, NBPP, MT_DATA));
		get_end = (long)(_get_count());
		splx(s);
		get_times[i] = get = (get_end - get);

		if (get < getmin) {
			getmin = get;
		}
		if (get > getmax) {
			getmax = get;
		}
		getavg += get;
	}
	getavg /= mbuf_loop_ct;

	qprintf(" m_free(m_vget) Pair NBPP bytes; min %d, max %d, avg %d\n",
		(getmin * mbuf_cycle_int) / 1000,
		(getmax * mbuf_cycle_int) / 1000,
		(getavg * mbuf_cycle_int) / 1000);
	for (i = 0; i < mbuf_loop_ct; i++) {
		get_times[i] =
		  (long)((get_times[i] * (long)mbuf_cycle_int)/(long)1000);
	}
	for (i = 0; i < mbuf_loop_ct; i+=10) {
	qprintf("  m_free(m_vget) Pair times: %d %d %d %d %d %d %d %d %d %d\n",
		get_times[i], get_times[i+1], get_times[i+2],
		get_times[i+3], get_times[i+4], get_times[i+5],
		get_times[i+6], get_times[i+7], get_times[i+8],
		get_times[i+9]);
	}
	return;
}

/*
 * kp mbuft4_cmn
 * Execute and print execution times of 10 consequtive m_get calls
 * followed by an equal number of m_free calls.
 * Both warm and cold cache execution options supported.
 */
void
idbg_mbuft4_cmn(int cache)
{
	struct mbuf *m_get_ptrs[IDBG_MBUF_MINCT], *m;
	int i, s;
	long getavg, getmin, getmax, get, get_end;
	long get_times[IDBG_MBUF_MINCT];
	long freeavg, freemin, freemax, free, free_end;
	long free_times[IDBG_MBUF_MINCT], cmb_times[IDBG_MBUF_MINCT];

	long mbuf_loop_ct = IDBG_MBUF_MINCT;

	/*
	 * m_get and m_free for MLEN Bytes
	 */
	getmin = 999999; getavg = getmax = 0;
	for (i = 0; i < mbuf_loop_ct; i++) {

		if (cache) {
			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
		}
		s = splhi();
		get = (long)(_get_count());
		m = m_get(M_DONTWAIT, MT_DATA);
		get_end = (long)(_get_count());
		splx(s);
		m_get_ptrs[i] = m;
		if (m)
			get = (get_end - get);
		else
			get = 0;

		get_times[i] = get;
		if (get < getmin) {
			getmin = get;
		}
		if (get > getmax) {
			getmax = get;
		}
		getavg += get;
	}
	getavg /= mbuf_loop_ct;

	freemin = 999999; freeavg = freemax = 0;
	for (i = 0; i < mbuf_loop_ct; i++) {

		if (m_get_ptrs[i] == 0) {
			free_times[i] = 0;
			continue;
		}
		m = m_get_ptrs[i];
		if (cache) {
			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
		}
		s = splhi();
		free = (long)(_get_count());
		m_free(m);
		free_end = (long)(_get_count());
		splx(s);
		free_times[i] = free = (free_end - free);

		if (free < freemin) {
			freemin = free;
		}
		if (free > freemax) {
			freemax = free;
		}
		freeavg += free;
	}
	freeavg /= mbuf_loop_ct;

	if (cache) {
		qprintf("Cold Cache: Loop count %d\n", mbuf_loop_ct);
	} else {
		qprintf("Warm Cache: Loop count %d\n", mbuf_loop_ct);
	}
	qprintf(" m_get MLEN bytes; min %d, max %d, avg %d\n",
		(getmin * mbuf_cycle_int) / 1000,
		(getmax * mbuf_cycle_int) / 1000,
		(getavg * mbuf_cycle_int) / 1000);
	for (i = 0; i < mbuf_loop_ct; i++) {
		get_times[i] = (long)
			((get_times[i] * (long)mbuf_cycle_int)/(long)1000);
	}
	for (i = 0; i < mbuf_loop_ct; i+=10) {
	qprintf("  m_get times: %d %d %d %d %d %d %d %d %d %d\n",
		get_times[i], get_times[i+1], get_times[i+2],
		get_times[i+3], get_times[i+4], get_times[i+5],
		get_times[i+6], get_times[i+7], get_times[i+8],
		get_times[i+9]);
	}
	qprintf(" m_free MLEN bytes; min %d, max %d, avg %d\n",
		(freemin * mbuf_cycle_int) / 1000,
		(freemax * mbuf_cycle_int) / 1000,
		(freeavg * mbuf_cycle_int) / 1000);
	for (i = 0; i < mbuf_loop_ct; i++) {
		free_times[i] = (long)
			((free_times[i] * (long)mbuf_cycle_int)/(long)1000);
	}
	for (i = 0; i < mbuf_loop_ct; i+=10) {
	qprintf("  m_free times: %d %d %d %d %d %d %d %d %d %d\n",
		free_times[i], free_times[i+1], free_times[i+2],
		free_times[i+3], free_times[i+4], free_times[i+5],
		free_times[i+6], free_times[i+7], free_times[i+8],
		free_times[i+9]);
	}
	for (i = 0; i < mbuf_loop_ct; i++) {
		cmb_times[i] = (get_times[i]+free_times[i]);
	}
	for (i = 0; i < mbuf_loop_ct; i+=10) {
	qprintf("  m_get+m_free times: %d %d %d %d %d %d %d %d %d %d\n",
		cmb_times[i], cmb_times[i+1], cmb_times[i+2],
		cmb_times[i+3], cmb_times[i+4], cmb_times[i+5],
		cmb_times[i+6], cmb_times[i+7], cmb_times[i+8],
		cmb_times[i+9]);
	}
	qprintf("\n");

	/*
	 * m_vget and m_free for MLEN Bytes
	 */
	getmin = 999999; getavg = getmax = 0;
	for (i = 0; i < mbuf_loop_ct; i++) {

		if (cache) {
			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
		}
		s = splhi();
		get = (long)(_get_count());
		m = m_vget(M_DONTWAIT, MLEN, MT_DATA);
		get_end = (long)(_get_count());
		splx(s);
		m_get_ptrs[i] = m;
		if (m)
			get = (get_end - get);
		else
			get = 0;

		get_times[i] = get;

		if (get < getmin) {
			getmin = get;
		}
		if (get > getmax) {
			getmax = get;
		}
		getavg += get;
	}
	getavg /= mbuf_loop_ct;

	freemin = 999999; freeavg = freemax = 0;
	for (i = 0; i < mbuf_loop_ct; i++) {

		if (m_get_ptrs[i] == 0) {
			free_times[i] = 0;
			continue;
		}
		m = m_get_ptrs[i];
		if (cache) {
			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
		}
		s = splhi();
		free = (long)(_get_count());
		m_free(m);
		free_end = (long)(_get_count());
		splx(s);
		free_times[i] = free = (free_end - free);

		if (free < freemin) {
			freemin = free;
		}
		if (free > freemax) {
			freemax = free;
		}
		freeavg += free;
	}
	freeavg /= mbuf_loop_ct;

	qprintf(" m_vget MLEN bytes; min %d, max %d, avg %d\n",
		(getmin * mbuf_cycle_int) / 1000,
		(getmax * mbuf_cycle_int) / 1000,
		(getavg * mbuf_cycle_int) / 1000);
	for (i = 0; i < mbuf_loop_ct; i++) {
		get_times[i] = (long)
			((get_times[i] * (long)mbuf_cycle_int)/(long)1000);
	}
	for (i = 0; i < mbuf_loop_ct; i+=10) {
	qprintf("  m_vget times: %d %d %d %d %d %d %d %d %d %d\n",
		get_times[i], get_times[i+1], get_times[i+2],
		get_times[i+3], get_times[i+4], get_times[i+5],
		get_times[i+6], get_times[i+7], get_times[i+8],
		get_times[i+9]);
	}

	qprintf(" m_free MLEN bytes; min %d, max %d, avg %d\n",
		(freemin * mbuf_cycle_int) / 1000,
		(freemax * mbuf_cycle_int) / 1000,
		(freeavg * mbuf_cycle_int) / 1000);
	for (i = 0; i < mbuf_loop_ct; i++) {
		free_times[i] = (long)
			((free_times[i] * (long)mbuf_cycle_int)/(long)1000);
	}
	for (i = 0; i < mbuf_loop_ct; i+=10) {
	qprintf("  m_free times: %d %d %d %d %d %d %d %d %d %d\n",
		free_times[i], free_times[i+1], free_times[i+2],
		free_times[i+3], free_times[i+4], free_times[i+5],
		free_times[i+6], free_times[i+7], free_times[i+8],
		free_times[i+9]);
	}
	for (i = 0; i < mbuf_loop_ct; i++) {
		cmb_times[i] = (get_times[i]+free_times[i]);
	}
	for (i = 0; i < mbuf_loop_ct; i+=10) {
    qprintf("  m_vget+m_free times: %d %d %d %d %d %d %d %d %d %d\n",
		cmb_times[i], cmb_times[i+1], cmb_times[i+2],
		cmb_times[i+3], cmb_times[i+4], cmb_times[i+5],
		cmb_times[i+6], cmb_times[i+7], cmb_times[i+8],
		cmb_times[i+9]);
	}
	qprintf("\n");

	/*
	 * m_vget and m_free for 2048 Bytes
	 */
	getmin = 999999; getavg = getmax = 0;
	for (i = 0; i < mbuf_loop_ct; i++) {

		if (cache) {
			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
		}
		s = splhi();
		get = (long)(_get_count());
		m = m_vget(M_DONTWAIT, 2048, MT_DATA);
		get_end = (long)(_get_count());
		splx(s);
		m_get_ptrs[i] = m;
		if (m)
			get = (get_end - get);
		else
			get = 0;

		get_times[i] = get;

		if (get < getmin) {
			getmin = get;
		}
		if (get > getmax) {
			getmax = get;
		}
		getavg += get;
	}
	getavg /= mbuf_loop_ct;

	freemin = 999999; freeavg = freemax = 0;
	for (i = 0; i < mbuf_loop_ct; i++) {

		if (m_get_ptrs[i] == 0) {
			free_times[i] = 0;
			continue;
		}
		if (cache) {
			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
		}
		m = m_get_ptrs[i];
		s = splhi();
		free = (long)(_get_count());
		m_free(m);
		free_end = (long)(_get_count());
		splx(s);
		free_times[i] = free = (free_end - free);

		if (free < freemin) {
			freemin = free;
		}
		if (free > freemax) {
			freemax = free;
		}
		freeavg += free;
	}
	freeavg /= mbuf_loop_ct;

	qprintf(" m_vget 2048 bytes; min %d, max %d, avg %d\n",
		(getmin * mbuf_cycle_int) / 1000,
		(getmax * mbuf_cycle_int) / 1000,
		(getavg * mbuf_cycle_int) / 1000);
	for (i = 0; i < mbuf_loop_ct; i++) {
		get_times[i] = (long)
			((get_times[i] * (long)mbuf_cycle_int)/(long)1000);
	}
	for (i = 0; i < mbuf_loop_ct; i+=10) {
	qprintf("  m_vget times: %d %d %d %d %d %d %d %d %d %d\n",
		get_times[i], get_times[i+1], get_times[i+2],
		get_times[i+3], get_times[i+4], get_times[i+5],
		get_times[i+6], get_times[i+7], get_times[i+8],
		get_times[i+9]);
	}
	qprintf(" m_free 2048 bytes; min %d, max %d, avg %d\n",
		(freemin * mbuf_cycle_int) / 1000,
		(freemax * mbuf_cycle_int) / 1000,
		(freeavg * mbuf_cycle_int) / 1000);
	for (i = 0; i < mbuf_loop_ct; i++) {
		free_times[i] = (long)
			((free_times[i] * (long)mbuf_cycle_int)/(long)1000);
	}
	for (i = 0; i < mbuf_loop_ct; i+=10) {
	qprintf("  m_free times: %d %d %d %d %d %d %d %d %d %d\n",
		free_times[i], free_times[i+1], free_times[i+2],
		free_times[i+3], free_times[i+4], free_times[i+5],
		free_times[i+6], free_times[i+7], free_times[i+8],
		free_times[i+9]);
	}
	for (i = 0; i < mbuf_loop_ct; i++) {
		cmb_times[i] = (get_times[i]+free_times[i]);
	}
	for (i = 0; i < mbuf_loop_ct; i+=10) {
   qprintf("  m_vget+m_free times: %d %d %d %d %d %d %d %d %d %d\n",
		cmb_times[i], cmb_times[i+1], cmb_times[i+2],
		cmb_times[i+3], cmb_times[i+4], cmb_times[i+5],
		cmb_times[i+6], cmb_times[i+7], cmb_times[i+8],
		cmb_times[i+9]);
	}
	qprintf("\n");

	/*
	 * m_vget and m_free for NBPP
	 */
	getmin = 999999; getavg = getmax = 0;
	for (i = 0; i < mbuf_loop_ct; i++) {

		if (cache) {
			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
		}
		s = splhi();
		get = (long)(_get_count());
		m = m_vget(M_DONTWAIT, NBPP, MT_DATA);
		get_end = (long)(_get_count());
		splx(s);
		m_get_ptrs[i] = m;
		if (m)
			get = (get_end - get);
		else
			get = 0;

		get_times[i] = get;

		if (get < getmin) {
			getmin = get;
		}
		if (get > getmax) {
			getmax = get;
		}
		getavg += get;
	}
	getavg /= mbuf_loop_ct;

	freemin = 999999; freeavg = freemax = 0;
	for (i = 0; i < mbuf_loop_ct; i++) {

		if (m_get_ptrs[i] == 0) {
			free_times[i] = 0;
			continue;
		}
		if (cache) {
			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
		}
		m = m_get_ptrs[i];
		s = splhi();
		free = (long)(_get_count());
		m_free(m);
		free_end = (long)(_get_count());
		splx(s);
		free_times[i] = free = (free_end - free);

		if (free < freemin) {
			freemin = free;
		}
		if (free > freemax) {
			freemax = free;
		}
		freeavg += free;
	}
	freeavg /= mbuf_loop_ct;

	qprintf(" m_vget NBPP bytes; min %d, max %d, avg %d\n",
		(getmin * mbuf_cycle_int) / 1000,
		(getmax * mbuf_cycle_int) / 1000,
		(getavg * mbuf_cycle_int) / 1000);
	for (i = 0; i < mbuf_loop_ct; i++) {
		get_times[i] = (long)
			((get_times[i] * (long)mbuf_cycle_int)/(long)1000);
	}
	for (i = 0; i < mbuf_loop_ct; i+=10) {
	qprintf("  m_vget times: %d %d %d %d %d %d %d %d %d %d\n",
		get_times[i], get_times[i+1], get_times[i+2],
		get_times[i+3], get_times[i+4], get_times[i+5],
		get_times[i+6], get_times[i+7], get_times[i+8],
		get_times[i+9]);
	}

	qprintf(" m_free NBPP bytes; min %d, max %d, avg %d\n",
		(freemin * mbuf_cycle_int) / 1000,
		(freemax * mbuf_cycle_int) / 1000,
		(freeavg * mbuf_cycle_int) / 1000);
	for (i = 0; i < mbuf_loop_ct; i++) {
		free_times[i] = (long)
			((free_times[i] * (long)mbuf_cycle_int)/(long)1000);
	}
	for (i = 0; i < mbuf_loop_ct; i+=10) {
	qprintf("  m_free times: %d %d %d %d %d %d %d %d %d %d\n",
		free_times[i], free_times[i+1], free_times[i+2],
		free_times[i+3], free_times[i+4], free_times[i+5],
		free_times[i+6], free_times[i+7], free_times[i+8],
		free_times[i+9]);
	}
	for (i = 0; i < mbuf_loop_ct; i++) {
		cmb_times[i] = (get_times[i]+free_times[i]);
	}
	for (i = 0; i < mbuf_loop_ct; i+=10) {
    qprintf("  m_vget+m_free times: %d %d %d %d %d %d %d %d %d %d\n",
		cmb_times[i], cmb_times[i+1], cmb_times[i+2],
		cmb_times[i+3], cmb_times[i+4], cmb_times[i+5],
		cmb_times[i+6], cmb_times[i+7], cmb_times[i+8],
		cmb_times[i+9]);
	}
	return;
}

/*
 * kp mbuft4 cache
 * Execute and print execution times of 10 consequtive m_get calls
 * followed by an qequal number of m_free calls.
 * NOTE: Only warm cache execution timed.
 */
void
idbg_mbuft4(__psint_t x)
{
	int cache = (x == 0) ? 0 : 1;
	idbg_mbuft4_cmn(cache);
	return;
}

/*
 * kp mbuftlk
 * Execute and print execution times of 10 pairs of
 * mutex_spinlock/mutex_spinunlock calls.
 * NOTE: Both warm(x=0) and cold(x=1) cache execution times supported.
 */
void
idbg_mbuftlk(__psint_t x)
{
	int i, s, s_lfvec;
	lock_t lock;
	long getavg, getmin, getmax, get, get_end;
	long get_times[IDBG_MBUF_MINCT];
	long freeavg, freemin, freemax, free, free_end;
	long free_times[IDBG_MBUF_MINCT];
	int cache = (x == 0) ? 0 : 1;
	long mbuf_loop_ct = IDBG_MBUF_MINCT;
	spinlock_init(&lock, 0);

	getmin = 999999; getavg = getmax = 0;
	freemin = 999999; freeavg = freemax = 0;

	for (i = 0; i < mbuf_loop_ct; i++) {

		if (cache) {
			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
		}
		s = splhi();
		get = (long)(_get_count());
		s_lfvec = mutex_spinlock(&lock);
		get_end = (long)(_get_count());
		splx(s);
		get_times[i] = get = (get_end - get);
		if (get < getmin) {
			getmin = get;
		}
		if (get > getmax) {
			getmax = get;
		}
		getavg += get;

		if (cache) {
			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
		}
		s = splhi();
		free = (long)(_get_count());
		mutex_spinunlock(&lock, s_lfvec);
		free_end = (long)(_get_count());
		splx(s);
		free_times[i] = free = (free_end - free);
		if (free < freemin) {
			freemin = free;
		}
		if (free > freemax) {
			freemax = free;
		}
		freeavg += free;
	}
	getavg /= mbuf_loop_ct;
	freeavg /= mbuf_loop_ct;

	if (cache) {
		qprintf(" mutex_spinlock Cold cache; min %d, max %d, avg %d\n",
			(getmin * mbuf_cycle_int) / 1000,
			(getmax * mbuf_cycle_int) / 1000,
			(getavg * mbuf_cycle_int) / 1000);
	} else {
		qprintf(" mutex_spinlock Warm cache; min %d, max %d, avg %d\n",
			(getmin * mbuf_cycle_int) / 1000,
			(getmax * mbuf_cycle_int) / 1000,
			(getavg * mbuf_cycle_int) / 1000);
	}
	for (i = 0; i < mbuf_loop_ct; i++) {
		get_times[i] = (long)
			((get_times[i] * (long)mbuf_cycle_int)/(long)1000);
	}
	for (i = 0; i < mbuf_loop_ct; i+=10) {
	qprintf("  mutex_spinlock: %d %d %d %d %d %d %d %d %d %d\n",
		get_times[i], get_times[i+1], get_times[i+2],
		get_times[i+3], get_times[i+4], get_times[i+5],
		get_times[i+6], get_times[i+7], get_times[i+8],
		get_times[i+9]);
	}

	qprintf(" mutex_spinunlock: min %d, max %d, avg %d\n",
		(freemin * mbuf_cycle_int) / 1000,
		(freemax * mbuf_cycle_int) / 1000,
		(freeavg * mbuf_cycle_int) / 1000);
	for (i = 0; i < mbuf_loop_ct; i++) {
		free_times[i] = (long)
			((free_times[i] * (long)mbuf_cycle_int)/(long)1000);
	}
	for (i = 0; i < mbuf_loop_ct; i+=10) {
	qprintf("  mutex_spinunlock times: %d %d %d %d %d %d %d %d %d %d\n",
		free_times[i], free_times[i+1], free_times[i+2],
		free_times[i+3], free_times[i+4], free_times[i+5],
		free_times[i+6], free_times[i+7], free_times[i+8],
		free_times[i+9]);
	}
}
#endif /* defined(DEBUG_MBUF_TIMING) && defined(R10000) */

#if defined(DEBUG) && defined(DEBUG_IPCHKSUM_TIMING) && defined(R10000)
/*
 * External functions to get R10K cycle counts and cache invalidates
 * and IP checksum procedures
 */
extern __uint32_t in_cksum_sub(u_short *, int, __uint32_t);
extern uint _get_count(void);
extern void __cache_wb_inval(void *address, long length);

/*
 * Execute and print execution times of calling the in_chsum procedure
 * on specified block size
 * NOTE: Both warm and cold timings supported
 */

#define IDBG_CHKSUM_MINCT 10
void
idbg_chksum_cmn(int cache, int odd_align, int size)
{
	unsigned long *p, *page;
	int i, s;
	long getavg, getmin, getmax, get, get_end;
	long get_times[IDBG_CHKSUM_MINCT];
	long chksum_loop_ct = IDBG_CHKSUM_MINCT;

	if ((page = (unsigned long *)kvpalloc(1, 0, 0)) == NULL) {
		printf("idbg_chksum_cmn: kvpalloc failed\n");
		return;
	}
	p = page;
	if ((size <= 0) || (size > NBPP)) {
		size = NBPP;
	}
	if (odd_align) {
		p = page + sizeof(int);  
		if (size == NBPP) {
			size = NBPP - sizeof(int);
		}
	}
	/*
	 * in_cksum_sub() cycle counts for 'size' bytes
	 */
	getmin = 999999; getavg = getmax = 0;
	for (i = 0; i < chksum_loop_ct; i++) {

		if (cache) {
			__cache_wb_inval((void *)K0BASE, 16*1024*1024);
		}
		s = splhi();
		get = (long)(_get_count());

		(void)in_cksum_sub((u_short *)p, size, 0);

		get_end = (long)(_get_count());
		splx(s);
		get_times[i] = get = (get_end - get);
		if (get < getmin) {
			getmin = get;
		}
		if (get > getmax) {
			getmax = get;
		}
		getavg += get;
	}
	getavg /= chksum_loop_ct;

	if (cache) {
		qprintf("Cold Cache: Odd Align %d, buf addr 0x%x, loopCt %d\n",
			odd_align, p, chksum_loop_ct);
	} else {
		qprintf("Warm Cache: Odd Align %d, buf addr 0x%x, loopCt %d\n",
			odd_align, p, chksum_loop_ct);
	}
	qprintf(" in_cksum_sub: %d bytes; min %d, max %d, avg %d\n",
		size,
		(getmin * mbuf_cycle_int) / 1000,
		(getmax * mbuf_cycle_int) / 1000,
		(getavg * mbuf_cycle_int) / 1000);
	for (i = 0; i < chksum_loop_ct; i++) {
		get_times[i] = (long)
			((get_times[i] * (long)mbuf_cycle_int)/(long)1000);
	}
	for (i = 0; i < chksum_loop_ct; i+=10) {
	qprintf(" in_cksum_sub: %d %d %d %d %d %d %d %d %d %d\n",
		get_times[i], get_times[i+1], get_times[i+2],
		get_times[i+3], get_times[i+4], get_times[i+5],
		get_times[i+6], get_times[i+7], get_times[i+8],
		get_times[i+9]);
	}
	kvpfree(p, 1);
	return;
}

/*
 * kp chksum1 cache
 * Execute and print execution times of calling the in_cksum_sub procedure
 * to compute the IP checksum on a 1518 byte block with 64-bit starting buffer
 * address alignment.
 * NOTE: Both warm and cold timings supported
 */
void
idbg_chksum1(__psint_t x)
{
	int cache = (x == 0) ? 0 : 1;
	idbg_chksum_cmn(cache, 0, 1518);
	return;
}

/*
 * kp chksum2 cache
 * Execute and print execution times of calling the in_cksum_sub procedure
 * to compute the IP checksum on a 1518 byte block with 32-bit starting buffer
 * address alignment.
 * NOTE: Both warm and cold timings supported
 */
void
idbg_chksum2(__psint_t x)
{
	int cache = (x == 0) ? 0 : 1;
	idbg_chksum_cmn(cache, 1, 1518);
	return;
}

/*
 * kp chksum3 cache
 * Execute and print execution times of calling the in_cksum_sub procedure
 * to compute the IP checksum on a 4K block with 64-bit starting buffer
 * address alignment.
 * NOTE: Both warm and cold timings supported
 */
void
idbg_chksum3(__psint_t x)
{
	int cache = (x == 0) ? 0 : 1;
	idbg_chksum_cmn(cache, 0, 4096);
	return;
}

/*
 * kp chksum4 cache
 * Execute and print execution times of calling the in_cksum_sub procedure
 * to compute the IP checksum on a page size block with 64-bit starting buffer
 * address alignment.
 * NOTE: Both warm and cold timings supported
 */
void
idbg_chksum4(__psint_t x)
{
	int cache = (x == 0) ? 0 : 1;
	idbg_chksum_cmn(cache, 0, NBPP);
	return;
}
#endif /* defined(DEBUG) && defined(DEBUG_IPCHKSUM_TIMING) && defined(R10000)*/

/*
 *  Print the udp statistics
 */
void
idbg_udpstats(__psint_t addr)
{
	register struct udpstat *udp;

	if (addr == -1L) { /* no address specified so exit */
		qprintf(" no udpstruct address supplied\n");
		return;
	}

	udp = (struct udpstat *)addr;

qprintf("  udps_ipackets %d, udps_hdrops %d, udps_badsum %d, udps_badlen %d\n",
		udp->udps_ipackets, udp->udps_hdrops,
		udp->udps_badsum, udp->udps_badlen);

qprintf("  udps_noport %d, udps_noportbcast %d, udps_fullsock %d, udps_opackets %d\n",
		udp->udps_noport, udp->udps_noportbcast,
		udp->udps_fullsock, udp->udps_opackets);

	return;
}

/*
 *  Print the tcp statistics
 */
void
idbg_sockstats(void)
{
	struct sockstat sockstat;
	int i;

	in_pcb_sockstats(&sockstat);
	qprintf(" open sockets\n");
	for (i=0; i < TCPSTAT_MAX_SOCKTYPES; i++) {
		qprintf("  type %s, count %d\n",
			tab_sotype[i], sockstat.open_sock[i]);
	}
	qprintf(" tcp connections\n");
	for (i=0; i < TCPSTAT_MAX_TCPSTATES; i++) {
		qprintf("  state %s, count %d\n",
			tab_tcpstates[i], sockstat.tcp_sock[i]);
	}
	return;
}

/*
 *  Print the tcp statistics
 */
void
idbg_tcpstats(__psint_t addr)
{
	register struct tcpstat *tcp;

	if (addr == -1L) { /* no address specified so exit */
		qprintf(" no tcpstruct address supplied\n");
		return;
	}

	tcp = (struct tcpstat *)addr;

qprintf("  tcps_connattempt %d, tcps_accepts %d, tcps_connects %d, tcps_drops %d\n",
		tcp->tcps_connattempt, tcp->tcps_accepts,
		tcp->tcps_connects, tcp->tcps_drops);

qprintf("  tcps_conndrops %d, tcps_closed %d, tcps_segstimed %d, tcps_rttupdated %d\n",
		tcp->tcps_conndrops, tcp->tcps_closed,
		tcp->tcps_segstimed, tcp->tcps_rttupdated);

qprintf("  tcps_delack %d, tcps_timeoutdrop %d, tcps_rexmttimeo %d, tcps_persisttimeo %d\n",
		tcp->tcps_delack, tcp->tcps_timeoutdrop,
		tcp->tcps_rexmttimeo, tcp->tcps_persisttimeo);

qprintf("  tcps_keeptimeo %d, tcps_keepprobe %d, tcps_keepdrops %d, tcps_sndtotal %d\n",
		tcp->tcps_keeptimeo, tcp->tcps_keepprobe,
		tcp->tcps_keepdrops, tcp->tcps_sndtotal);

qprintf("  tcps_sndpack %d, tcps_sndbyte %d, tcps_sndrexmitpack %d, tcps_sndrexmitbyte %d\n",
		tcp->tcps_sndpack, tcp->tcps_sndbyte,
		tcp->tcps_sndrexmitpack, tcp->tcps_sndrexmitbyte);

qprintf("  tcps_listendrop %d, tcps_badsyn %d, tcps_synpurge %d, persistdrop %d\n",
		tcp->tcps_listendrop, tcp->tcps_badsyn,
		tcp->tcps_synpurge, tcp->tcps_persistdrop);

	return;
}

/*
 *  Print the icmp statistics
 */
void
idbg_icmpstats(__psint_t addr)
{
	register struct icmpstat *icmp;
	register int i;

	if (addr == -1L) { /* no address specified so exit */
		qprintf(" no icmpstruct address supplied\n");
		return;
	}
	icmp = (struct icmpstat *)addr;

	qprintf("  icps_error %d, icps_oldshort %d, icps_oldicmp %d\n",
		icmp->icps_error, icmp->icps_oldshort, icmp->icps_oldicmp);

	for (i=0; i < ICMP_MAXTYPE+1; i+=3) {
		if (i >= ICMP_MAXTYPE+1)
			continue;
		qprintf("  icps_outhist[%d,%d,%d] = (%d, %d, %d)\n",
			i, i+1, i+2,
			icmp->icps_outhist[i],
			icmp->icps_outhist[i+1],
			icmp->icps_outhist[i+2]);
	}

	qprintf("  icps_badcode %d, icps_tooshort %d, icps_checksum %d\n",
		icmp->icps_badcode, icmp->icps_tooshort, icmp->icps_checksum);

	qprintf("  icps_badlen %d, icps_reflect %d\n",
		icmp->icps_badlen, icmp->icps_reflect);

	for (i=0; i < ICMP_MAXTYPE+1; i+=3) {
		if (i >= ICMP_MAXTYPE+1)
			continue;
		qprintf("  icps_inhist[%d,%d,%d] = (%d, %d, %d)\n",
			i, i+1, i+2,
			icmp->icps_inhist[i],
			icmp->icps_inhist[i+1],
			icmp->icps_inhist[i+2]);
	}
	return;
}

/*
 *  Print the igmp statistics
 */
void
idbg_igmpstats(__psint_t addr)
{
	register struct igmpstat *igmp;

	if (addr == -1L) { /* no address specified so exit */
		qprintf(" no igmpstruct address supplied\n");
		return;
	}
	igmp = (struct igmpstat *)addr;

qprintf("  igps_rcv_total %d, igps_rcv_tooshort %d, igps_rcv_badsum %d\n",
	igmp->igps_rcv_total,
	igmp->igps_rcv_tooshort,
	igmp->igps_rcv_badsum);

qprintf("  igps_rcv_queries %d, igps_rcv_badqueries %d, igps_rcv_reports %d\n",
	igmp->igps_rcv_queries,
	igmp->igps_rcv_badqueries,
	igmp->igps_rcv_reports);

qprintf("  igps_rcv_badreports %d, igps_rcv_ourreports %d, igps_snd_reports %d\n",
	igmp->igps_rcv_badreports,
	igmp->igps_rcv_ourreports,
	igmp->igps_snd_reports);

	return;
}

/*
 *  Print all inpcb statistics
 */
void
idbg_ginpcbstats(void)
{
	int i;
	register struct kna *knaptr;
	struct inpcbstat tot;
	int j;

	for (j = 0; j < NUM_PCBSTAT; j++) {
		bzero(&tot, sizeof(tot));
		for (i = 0; i < maxcpus; i++) {

			knaptr = (struct kna *)pdaindr[i].pda->knaptr;
			if (knaptr == (struct kna *)0) {
				continue;
			}
			tot.inpcb_notify += knaptr->pcbstat[j].inpcb_notify;
			tot.inpcb_lookup += knaptr->pcbstat[j].inpcb_lookup;
			tot.inpcb_regular += knaptr->pcbstat[j].inpcb_regular;
			tot.inpcb_wildcard +=knaptr->pcbstat[j].inpcb_wildcard;
			tot.inpcb_found_cache += 
				knaptr->pcbstat[j].inpcb_found_cache;
			tot.inpcb_found_chain += 
				knaptr->pcbstat[j].inpcb_found_chain;
			tot.inpcb_searched_all += 
				knaptr->pcbstat[j].inpcb_searched_all;
			tot.inpcb_success += knaptr->pcbstat[j].inpcb_success;
			tot.inpcb_failed += knaptr->pcbstat[j].inpcb_failed;
			tot.inpcb_bind += knaptr->pcbstat[j].inpcb_bind;
			tot.inpcb_listen += knaptr->pcbstat[j].inpcb_listen;
			tot.inpcb_connect += knaptr->pcbstat[j].inpcb_connect;
			tot.inpcb_connectbyaddr += 
				knaptr->pcbstat[j].inpcb_connectbyaddr;
			tot.inpcb_setaddr += knaptr->pcbstat[j].inpcb_setaddr;
			tot.inpcb_setaddrx +=knaptr->pcbstat[j].inpcb_setaddrx;
			tot.inpcb_curpcbs += knaptr->pcbstat[j].inpcb_curpcbs;
		}

		idbg_pinpcbstat((__psint_t)(&tot), j);
	}
}

void
idbg_inpcbstats(__psint_t addr)
{
	struct inpcbstat *inpcb = (struct inpcbstat *)addr;
	int i;

	for (i = 0; i < NUM_PCBSTAT; i++) {
		idbg_pinpcbstat((__psint_t)inpcb, i);
		inpcb++;
	}
	return;
}

void
idbg_pcbhashinfo(__psint_t addr)
{
	struct inpcb *inp = (struct inpcb *)addr;
	int i;

	for (i = 0; i < inp->inp_tablesz; i++) {
		struct in_pcbhead *hinp = &inp->inp_table[i];
		qprintf("0x%lx[%d] next=0x%x, prev=0x%x, mutex=0x%x\n", 
			hinp, i, hinp->hinp_next, hinp->hinp_prev,
			hinp->hinp_sem);
	}
}

/*
 * Distribution statistics for PCBs
 */
void
idbg_dinpcbstats(void)
{
#ifndef DEBUG
	qprintf("PCB stats not maintained in non-DEBUG kernel\n");
#else
	int i, j;
	struct inpcb *h[3], *inp;
	char *strs[2] = { "TCP", "UDP" };

	h[0] = &tcb;
	h[1] = &udb;

	for (j = 0; j < 2; j++) {
		int zmax = 0, zmin = 1024*1024;
		int ztotal = 0;
		inp = h[j];
		for (i = 0; i < inp->inp_tablesz; i++) {
			if (zmax < inp->inp_dstats[i])
				zmax = inp->inp_dstats[i];
			if (zmin > inp->inp_dstats[i])
				zmin = inp->inp_dstats[i];
			ztotal = ztotal + inp->inp_dstats[i];
		}

		qprintf("%s: %d buckets, %d PCBs, shortest=%d, longest=%d, avg=%d\n",
			strs[j], inp->inp_tablesz, ztotal, zmin, zmax, 
			ztotal / inp->inp_tablesz);
	}
#endif
	return;
}

/*
 *  Print the inpcb statistics
 */
void
idbg_pinpcbstat(__psint_t addr, int idx)
{
	register struct inpcbstat *inpcb;
	char *str;

	if (addr == -1L) { /* no address specified so exit */
		qprintf(" no inpcbstat struct address supplied\n");
		return;
	}

	str = (idx == UDP_PCBSTAT) ? "UDP" : "TCP";
	inpcb = (struct inpcbstat *)addr;
	qprintf("%s: curpcbs %u, calls in_pcblookup %u, regular %u\n",
		str,
		inpcb->inpcb_curpcbs,
		inpcb->inpcb_lookup,
		inpcb->inpcb_regular);

       qprintf("   wildcard %u, listen %u, failed %u\n", 
		inpcb->inpcb_wildcard,
		inpcb->inpcb_listen,
		inpcb->inpcb_failed);

       qprintf("   found_cache %u, found_chain %u, search_all %u success %u\n",
		inpcb->inpcb_found_cache,
		inpcb->inpcb_found_chain,
		inpcb->inpcb_searched_all,
		inpcb->inpcb_success);

	qprintf("   bind %u, connect %u, connectbyaddr %u\n",
		inpcb->inpcb_bind,
		inpcb->inpcb_connect,
		inpcb->inpcb_connectbyaddr);

	qprintf("   notify %u, setaddr %u, setaddrx %u\n",
		inpcb->inpcb_notify,
		inpcb->inpcb_setaddr,
		inpcb->inpcb_setaddrx);
	return;
}

/*
 *  kp cnetstats kna
 *  Print the cpu tcp/ip statistics given a kna structure address
 *  associated with a cpu. The kna structure which holds the tcp/ip
 *  statistics block for a particular cpu.
 */
void
idbg_cnetstats(__psint_t addr)
{
	register struct kna *kna_ptr;
	register struct ipstat *ip;
	register struct udpstat *udp;
	register struct tcpstat *tcp;
	register struct icmpstat *icmp;
	register struct igmpstat *igmp;
	register struct inpcbstat *inpcb;
	register struct mrtstat *mr;

	if (addr == -1L) { /* no address specified so exit */
		qprintf(" no kna address supplied\n");
		return;
	}

	kna_ptr = (struct kna *)addr;

	ip = &(kna_ptr->ipstat);
	idbg_ipstats((__psint_t)ip);

	udp = &(kna_ptr->udpstat);
	idbg_udpstats((__psint_t)udp);

	tcp = &(kna_ptr->tcpstat);
	idbg_tcpstats((__psint_t)tcp);

	icmp = &(kna_ptr->icmpstat);
	idbg_icmpstats((__psint_t)icmp);

	igmp = &(kna_ptr->igmpstat);
	idbg_igmpstats((__psint_t)igmp);

	inpcb = &(kna_ptr->pcbstat[0]);
	idbg_inpcbstats((__psint_t)inpcb);

	mr = &(kna_ptr->mrtstat);
	idbg_mrtstats((__psint_t)mr);
	return;
}

/*
 * sockbuf - print the contents of the socketbuf structure
 */
static void
sockbuf(register struct sockbuf *s, char *prefix1)
{
	qprintf("  %scc %d hiwat %d lowat %d timeo %d mbuf 0x%x flags ",
	     prefix1, s->sb_cc, s->sb_hiwat, s->sb_lowat,
	     (unsigned)s->sb_timeo, s->sb_mb);
	_printflags((unsigned)s->sb_flags, tab_sbflags, 0);
	qprintf("\n");
}

void
idbg_soc(__psint_t addr)
{
    extern void idbg_bhv(__psint_t);
    if (addr == -1L) {
	qprintf("No socket or socketbuf specified\n");
	return;
    }

    if (addr > 0) {
	struct sockbuf *s = (struct sockbuf *) (addr | K0BASE);

	qprintf("struct sockbuf for 0x%x\n", (__psint_t) s);
	sockbuf(s, "");

    } else if (addr < 0) {
	register struct socket *s = (struct socket *) addr;

	qprintf("  type %d (%s) state ", s->so_type,
	    ((unsigned short) s->so_type >=
		    (sizeof(socketTypes)/sizeof(socketTypes[0]))) ?
		"?" : socketTypes[s->so_type]);
	_printflags((unsigned)s->so_state, tab_sostate, 0);
	qprintf("\n  options ");
	_printflags((unsigned)s->so_options, tab_sooptions, 0);
	qprintf("\n");
	qprintf("  linger %d pcb 0x%x protosw 0x%x holds %d\n",
			s->so_linger, s->so_pcb, s->so_proto, s->so_holds);
	sockbuf(&s->so_rcv, "rcv: ");
	sockbuf(&s->so_snd, "snd: ");
	qprintf("  head 0x%x q0 0x%x q0len %d q 0x%x qlen %d qlimit %d\n",
		s->so_head, s->so_q0, s->so_q0len, s->so_q, s->so_qlen,
		s->so_qlimit);
	qprintf("  q0prev 0x%x qprev 0x%x\n",s->so_q0prev, s->so_qprev);
	qprintf("  timeout %d, error %d, pgid %d, oobmark %d, pollhead 0x%x\n",
		s->so_timeo, s->so_error, s->so_pgid, s->so_oobmark, &s->so_ph);
	qprintf("  so_sem 0x%x  so_qlock 0x%x  refcnt %d\n", &s->so_sem, 
			&s->so_qlock, s->so_refcnt);
	qprintf("  so_data 0x%x  so_sesmgr_data 0x%x\n", s->so_data, 
		s->so_sesmgr_data);
	qprintf("  so_callout_arg 0x%x\n", s->so_callout_arg);
	qprintf("  so_bhv:\n");
	idbg_bhv((__psint_t)&s->so_bhv);
#ifdef __SO_LOCK_DEBUG
	qprintf("  so_locker 0x%x  so_unlocker 0x%x\n", 
		s->so_locker,s->so_unlocker);
	qprintf("  so_lockthread 0x%x  so_unlockthread 0x%x\n", 
		s->so_lockthread,s->so_unlockthread);
#endif
    }
}

void
idbg_pcblist(__psint_t addr)
{
    struct in_pcbhead *hinp;
    int hash;
    if (addr == -1L) {
        qprintf("No inet PCB specified\n");
        return;
    }
    if (addr < 0) {
	struct inpcb *head = (struct inpcb *)addr;
	struct inpcb *inp;

	for (hash = 0; hash < head->inp_tablesz; hash++) {
		hinp = &head->inp_table[hash];

		inp = hinp->hinp_next;
		for (; inp != (struct inpcb *)hinp; inp = inp->inp_next) {
			qprintf("\n");
			idbg_inpcb((__psint_t)inp);
		}
	}
    }
}

void
idbg_inpcb_problem(void)
{
    struct inpcb *inp;
	struct in_pcbhead *hinp;
	int hash;

    for (hash = 0; hash < tcb.inp_tablesz; hash++) {
	hinp = &tcb.inp_table[hash];
	inp = hinp->hinp_next;
	for (; inp != (struct inpcb *)hinp; inp = inp->inp_next) {
		if (inp->inp_refcnt > 1) {
			qprintf("\n");
			idbg_inpcb((__psint_t)inp);
		}
	}
    }
}

void
idbg_inpcb(__psint_t addr)
{
    if (addr == -1L) {
	qprintf("No inet PCB specified\n");
	return;
    }

    if (addr < 0) {
	register struct inpcb *p = (struct inpcb *) addr;

	qprintf("  next 0x%x, prev 0x%x, PCB head 0x%x\n",
	    p->inp_next, p->inp_prev, p->inp_head);
	qprintf("  hashflags ");
	_printflags((unsigned)p->inp_hashflags, idbg_inp_hashflags, 0);
	qprintf("\n");
	qprintf("  refcnt %d\n",p->inp_refcnt);
	if (p == p->inp_head) {
		qprintf("  wildport %d\n", p->inp_wildport);
		qprintf("  tablesz %d  hashfun 0x%x\n", 
			p->inp_tablesz, p->inp_hashfun);
		qprintf("  table 0x%x\n",p->inp_table);
		return;
	}
	qprintf("  foreign_addr %x, fport %d, local_addr %x, lport %d\n",
	    p->inp_faddr.s_addr, p->inp_fport,
	    p->inp_laddr.s_addr, p->inp_lport);
	qprintf("  flags ");
	_printflags((unsigned)p->inp_flags, idbg_inp_flags, 0);
	qprintf("\n");
	qprintf("  socket 0x%x, proto PCB 0x%x, IP opts 0x%x\n",
	    p->inp_socket, p->inp_ppcb, p->inp_options);
	qprintf("  mopts 0x%x, hhead 0x%x\n", p->inp_moptions, p->inp_hhead);

	qprintf("  route "); print_route(&(p->inp_route));

	qprintf("  IP ttl %d, tos 0x%x\n", p->inp_ip_ttl, p->inp_ip_tos);
	qprintf("  hashval %d\n", p->inp_hashval);
#ifdef _PCB_DEBUG
	qprintf("  inp_trace 0x%x\n", p->inp_trace);
#endif
    }
}

void
idbg_route(__psint_t addr)
{
    if (addr == -1L) {
	qprintf("No route specified\n");
	return;
    }
    if (addr < 0) {
	register struct route *p = (struct route *)addr;

	qprintf("  route 0x%x\n", p);
	print_route(p);
    }
    return;
}

void
idbg_radixnode(__psint_t addr)
{
    if (addr == -1L) {
	qprintf("No radix node specified\n");
	return;
    }
    if (addr < 0) {
	register struct radix_node *p = (struct radix_node *)addr;

	qprintf("  radix node 0x%x\n", p);
	print_radixnode(p);
    }
    return;
}

void
idbg_radixtree()
{
    struct radix_node *node;

    node = rt_tables[AF_INET]->rnh_treetop;
    qprintf("  Radixtree treetop: 0x%x\n", node);
    print_radixtree(node);
    return;
}

void
idbg_radixsubtree(__psint_t addr)
{
    struct radix_node *node = (struct radix_node *)addr;

    qprintf("  Radixsubtree starting from: 0x%x\n", node);
    print_radixtree(node);
}

void
idbg_rtentry(__psint_t addr)
{
    if (addr == -1L) {
	qprintf("No route entry specified\n");
	return;
    }
    if (addr < 0) {
	register struct rtentry *p = (struct rtentry *)addr;

	qprintf("  route entry 0x%x\n", p);
	print_rtentry(p);
    }
    return;
}

extern struct router_info *igmp_router_info_head;

void
idbg_routerinfo(__psint_t addr)
{
	register struct router_info *rip;

	if (addr == -1L) {
		rip = igmp_router_info_head;
	} else {
		if (addr < 0) {
			rip = (struct router_info *)addr;
		} else { /* bogus address */
			qprintf("Invalid router_info address specified\n");
			return;
		}
	}
	print_router_info_list(rip);
	return;
}

void
idbg_sockaddr(__psint_t addr)
{
	register struct sockaddr *sap;

	if (addr == -1L) {
		qprintf("Invalid sockaddr address specified\n");
	} else {
		if (addr < 0) {
			sap = (struct sockaddr *)addr;
			print_sockaddr("sockaddr", sap);
		} else { /* bogus address */
			qprintf("Invalid sockaddr address specified\n");
		}
	}
	return;
}

void
idbg_sockaddr_raw(__psint_t addr)
{
	if (addr == -1L) {
		qprintf("Invalid sockaddr raw address specified\n");
	} else {
		if (addr < 0) {
			print_sockaddr("sockaddr_raw", (struct sockaddr *)addr);
		} else { /* bogus address */
			qprintf("Invalid sockaddr raw address specified\n");
		}
	}
	return;
}

void
idbg_tcpcb(__psint_t addr)
{
    if (addr == -1L) {
        qprintf("No TCP PCB specified\n");
        return;
    }

    if (addr < 0) {
        register struct tcpcb *p = (struct tcpcb *) addr;

        qprintf("  next 0x%x, prev 0x%x\n",
		p->linkage.ih_next, p->linkage.ih_prev);
        qprintf("  state 0x%x [", p->t_state);
	if (p->t_state < 0 || p->t_state >= TCP_NSTATES) {
		qprintf(" 0x%x ");
	} else {
        	qprintf("%s", tcpstates[p->t_state]);
	}

        qprintf("], flags 0x%x [", p->t_flags);
        _printflags(p->t_flags, tcpflags, 0);
        qprintf("]\n");
        qprintf("  incpb 0x%x template 0x%x\n", p->t_inpcb, p->t_template);
	qprintf("  rxtcur 0x%x rxtshift 0x%x dupacks 0x%x force 0x%x\n",
		p->t_rxtcur, p->t_rxtshift, p->t_dupacks, p->t_force);
        qprintf("  timers: rexmt 0x%x persist 0x%x\n",
                p->t_timer[0], p->t_timer[1]);
        qprintf("          keep 0x%x 2msl 0x%x mutexp 0x%x\n",
                p->t_timer[2], p->t_timer[3],  p->t_timer[4]);
        qprintf("  maxseg0 0x%x gen 0x%x\n", p->t_maxseg, p->t_tcpgen);
    }
}

void
idbg_unp(__psint_t addr)
{
    if (addr == -1L) {
        qprintf("No AF_UNIX PCB specified\n");
        return;
    }

    if (addr < 0) {
        register struct unpcb *p = (struct unpcb *) addr;

        qprintf("  socket 0x%x, vnode 0x%x\n", p->unp_socket, p->unp_vnode);
        qprintf("  next 0x%x, prevp 0x%x\n", p->unp_next, p->unp_prevp);
	qprintf("  ino %d  conn 0x%x   refs 0x%x\n",p->unp_ino, p->unp_conn,
		p->unp_refs);

	qprintf("  nextref 0x%x   addr 0x%x  cc %d  mbcnt %d\n",
		p->unp_nextref, p->unp_addr, p->unp_cc, p->unp_mbcnt);

        qprintf("  lnext 0x%x, lprev 0x%x\n", p->unp_lnext, p->unp_lprev);
	qprintf("  refcnt %d\n",p->unp_refcnt);
    }
}

void
idbg_vsock(struct vsocket *v)
{

    qprintf ("\nvsocket 0x%lx chain 0x%x\n\n", v, v->vs_bh.bh_first);
    if (v->vs_bh.bh_first) {
	qprintf ("\t   obj\t\tops\n\t0x%lx\t0x%lx\t0x%lx\n",
	    v->vs_bh.bh_first->bd_vobj, v->vs_bh.bh_first->bd_ops);
	qprintf ("\t   data\t\tchain\n\t0x%lx\t0x%lx\n",
	    v->vs_bh.bh_first->bd_pdata, v->vs_bh.bh_first);
    } else {
	qprintf ("Unconnected!\n");
    }
    qprintf ("\t   type\tproto\tdom\tref\n\t%d\t%d\t%d\t%d\n",
	v->vs_type, v->vs_protocol, v->vs_domain, v->vs_refcnt);
}

