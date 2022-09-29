/*
 * ipfilter.c
 *
 * Screen inbound IP packets against filterdaemon's access control list.
 * Screening is based on source and destination IP address, IP protocol, and
 * source and destination ports.
 *
 * Copyright 1990, Silicon Graphics, Inc. 
 * All Rights Reserved.
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

#include <string.h>
#include "tcp-param.h"
#include "sys/param.h"
#include "sys/sema.h"
#include "sys/hashing.h"

#include "sys/debug.h"
#include "sys/systm.h"
#include "sys/mbuf.h"
#include "sys/errno.h"
#include "sys/par.h"
#include "sys/idbgentry.h"
#include "sys/conf.h"
#include "sys/socketvar.h"
#include "sys/atomic_ops.h"

#include "net/if.h"

#include "in.h"
#include "in_systm.h"
#include "in_var.h"
#include "ip.h"
#include "ip_var.h"
#include "ip_icmp.h"
#include "tcp.h"
#include "udp.h"
#include "igmp.h"

#include "sys/ioctl.h"
#include "net/netisr.h"
#include "ipfilter.h"
#include "sys/capability.h"
#include "sys/sockd.h"

/*
 * Local definitions
 */
#define IPF_FREE	0	/* verdict field value to denote free entry */
	/* MUST be the value ZERO since it that is an unused verdict value */

#define IPFILTER_BULLET_PROOF 1  /* extra queue entry check for satefy */

sema_t ipfilterq_wait;

mrlock_t ipfilterq_mrlock, ipkfilter_mrlock;

#define IPFILTER_INITLOCKS() { \
	mrinit(&ipkfilter_mrlock, "ipkf_lock"); \
	mrinit(&ipfilterq_mrlock, "ipfq_lock"); \
	initnsema(&ipfilterq_wait, 0, "ipfltwait"); \
}
#define IPFILTER_FREELOCKS() { \
	mrfree(&ipkfilter_mrlock); \
	mrfree(&ipfilterq_mrlock); \
	freesema(&ipfilterq_wait); \
}
#define IPKFILTER_MRRLOCK()	mraccess(&ipkfilter_mrlock)
#define IPKFILTER_MRWLOCK()	mrupdate(&ipkfilter_mrlock)
#define IPKFILTER_MRUNLOCK()	mrunlock(&ipkfilter_mrlock)

#define IPFILTERQ_MRRLOCK()	mraccess(&ipfilterq_mrlock)
#define IPFILTERQ_MRWLOCK()	mrupdate(&ipfilterq_mrlock)
#define IPFILTERQ_MRUNLOCK()	mrunlock(&ipfilterq_mrlock)

/*
 * IP packet filtering global variables
 */
int ipfilterdevflag = 0;
struct ipkernelfilter *ipkfq_head;  /* current head of cache */
struct ifqueue	ipfiltqueue;   	    /* for packets awaiting daemon verdict */
/*
 * has daemon been active?
 */
int ipfilter_active_flag = 0;

/*
 * IP packet filtering external variables
 */
extern struct hashinfo hashinfo_inaddr;
extern int 	ipfilterflag;	    /* activity state of daemon and filtering*/

/*
 * define IP packet behavior when daemon is killed or dies.
 * configurable in /var/sysgen/mtune/kernel
 */
extern int ipfilterd_inactive_behavior;
extern int ipfilter_ttl;

extern struct ifqueue 	ipintrq;
/*
 * search depth in kernel cache before MRU reordering happens
 */
extern int filtercache_search_depth;
extern struct ipkernelfilter ipfiltercache[]; /* kernel IP filter cache */
extern int numipkflt;		      /* size of kernel filter cache */

/*
 * IP packet filtering external procedures
 */
extern void ipfiltertimer(void);

/*
 * Local macro definition to jump to next protocol in packet
 */
#define nextproto(x, y, t)              ((t)((__psint_t)(x) + (__psint_t)(y)))

#ifdef DEBUG
#include "sys/idbgentry.h"

int ipfilter_idbgflag = 0;

#define idbg_ipfilt_vmask 0x7  /* Mask to avoid vnames index overflow */

char *idbg_ipfilt_vnames[] = {
	"FREE",          /* 0 => Free entry */
	"ACCEPTIT",  /* 1 Accept this packet */
	"DROPIT",    /* 2 Drop this packet with NO notification */
	"DROPNOTIFY" /* 3 Drop this packet BUT send ICMP error to sender */
 	"GRABIT"     /* 4 Grab this packet rather than forwarding it */
};

struct in_addr_bytes {
	unsigned char b[4];
};

struct idbg_ipproto {
	unsigned char proto_value;
	char *proto_name;
};

/* IPPROTO values are sparse within 8-bit space.
 * IPPROTO_MAX does not fit in the storage space
 * of the first element of this structure, and
 * attempting to do so triggers a compilation warning.
 */
struct idbg_ipproto idbg_ipproto_names[] = {
	{IPPROTO_IP, "IP"},
	{IPPROTO_ICMP, "ICMP"},
	{IPPROTO_IGMP, "IGMP"},
	{IPPROTO_GGP, "GGP"},
	{IPPROTO_ENCAP, "ENCAP"},
	{IPPROTO_ST, "ST"},
	{IPPROTO_TCP, "TCP"},
	{IPPROTO_UCL, "UCL"},			/* UCL */
	{IPPROTO_EGP, "EGP"},
	{IPPROTO_IGP, "IGP"},		/* any private interior gateway */
	{IPPROTO_BBN_RCC_MON, "BBN_RCC_MON"},	/* BBN RCC Monitoring */
	{IPPROTO_NVP_II, "NVP_II"},		/* Network Voice Protocol */
	{IPPROTO_PUP, "PUP"},
	{IPPROTO_ARGUS, "ARGUS"},		/* ARGUS */
	{IPPROTO_EMCON, "EMCON"},		/* EMCON */
	{IPPROTO_XNET, "XNET"},			/* Cross Net Debugger */
	{IPPROTO_CHAOS, "CHAOS"},		/* Chaos */
	{IPPROTO_UDP, "UDP"},
	{IPPROTO_MUX, "MUX"},			/* Multiplexing */
	{IPPROTO_DCN_MEAS, "DCN_MEAS"},		/* DCN Measurement Subsystems*/
	{IPPROTO_HMP, "HMP"},			/* Host Monitoring */
	{IPPROTO_PRM, "PRM"},			/* Packet Radio Measurement */
	{IPPROTO_IDP, "IDP"},
	{IPPROTO_TRUNK_1, "TRUNK_1"},		/* Trunk-1 */
	{IPPROTO_TRUNK_2, "TRUNK_2"},		/* Trunk-2 */
	{IPPROTO_LEAF_1, "LEAF_1"},		/* Leaf-1 */
	{IPPROTO_LEAF_2, "LEAF_2"},		/* Leaf-2 */
	{IPPROTO_RDP, "RDP"},			/* Reliable Data Protocol */
	{IPPROTO_IRTP, "IRTP"},		/* Internet Reliable Transaction */
	{IPPROTO_TP, "TP"},
	{IPPROTO_NETBLT, "NETBLT"},	/* Bulk Data Transfer Protocol */
	{IPPROTO_MFE_NSP, "MFE_NSP"},	/* MFE Network Services Protocol */
	{IPPROTO_MERIT_INP, "MERIT_INP"},	/* MERIT Internodal Protocol */
	{IPPROTO_SEP, "SEP"},		/* Sequential Exchange Protocol */
	{IPPROTO_3PC, "3PC"},		/* Third Party Connect Protocol */
	{IPPROTO_IDPR, "IDPR"},	/* Inter-Domain Policy Routing Protocol */
	{IPPROTO_XTP, "XTP"},
	{IPPROTO_DDP, "DDP"},		/* Datagram Delivery Protocol */
	{IPPROTO_IDPR_CMTP, "IDPR_CMTP"}, /* IDPR Control Message Trans Prot */
	{IPPROTO_TPPP, "TP++"},			/* TP++ Transport Protocol */
	{IPPROTO_IL, "IL"},			/* IL Transport Protocol */
	{IPPROTO_RSVP, "RSVP"},
	{IPPROTO_CFTP, "CFTP"},			/* CFTP */
	{IPPROTO_HELLO, "HELLO"},
	{IPPROTO_SAT_EXPAK, "SAT_EXPAK"},	/* SATNET and Backroom EXPAK */
	{IPPROTO_KRYPTOLAN, "KRYPTOLAN"},	/* Kryptolan */
	{IPPROTO_RVD, "RVD"},		/* MIT Remote Virtual Disk Protocol */
	{IPPROTO_IPPC, "IPPC"},		/* Internet Pluribus Packet Core */
	{IPPROTO_SAT_MON, "SAT_MON"},	/* SATNET Monitoring */
	{IPPROTO_VISA, "VISA"},		/* VISA Protocol */
	{IPPROTO_IPCV, "IPCV"},		/* Internet Packet Core Utility */
	{IPPROTO_CPNX, "CPNX"},		/* Comp Protocol Network Executive*/
	{IPPROTO_CPHB, "CPHB"},		/* Computer Protocol Heart Beat */
	{IPPROTO_WSN, "WSN"},			/* Wang Span Network */
	{IPPROTO_PVP, "PVP"},			/* Packet Video Protocol */
	{IPPROTO_BR_SAT_MON, "BR_SAT_MON"}, /* Backroom SATNET Monitoring */
	{IPPROTO_ND, "ND"},
	{IPPROTO_WB_MON, "WB_MON"},		/* WIDEBAND Monitoring */
	{IPPROTO_WB_EXPAK, "WB_EXPAK"},		/* WIDEBAND EXPAK */
	{IPPROTO_EON,"EON"},
	{IPPROTO_VMTP, "VMTP"},			/* VMTP */
	{IPPROTO_SECURE_VMTP, "SECURE_VMTP"},	/* SECURE-VMTP */
	{IPPROTO_VINES, "VINES"},		/* VINES */
	{IPPROTO_TTP, "TTP"},			/* TTP */
	{IPPROTO_NSFNET_IGP, "NSFNET_IGP"},	/* NSFNET-IGP */
	{IPPROTO_DGP, "DGP"},		/* Dissimilar Gateway Protocol */
	{IPPROTO_TCF, "TCF"},			/* TCF */
	{IPPROTO_IGRP, "IGRP"},			/* IGRP */
	{IPPROTO_OSPF, "OSPF"},
	{IPPROTO_SPRITE_RPC, "SPRITE_RPC"},	/* Sprite RPC Protocol */
	{IPPROTO_LARP, "LARP"},		/* Locus Address Resolution Protocol */
	{IPPROTO_MTP, "MTP"},		/* Multicast Transport Protocol */
	{IPPROTO_AX25, "AX25"},		/* AX.25 Frames */
	{IPPROTO_SWIPE, "SWIPE"},
	{IPPROTO_MICP, "MICP"},		/* Mobile Internetworking Control Pro*/
	{IPPROTO_AES_SP3_D, "AES_SP3_D"},	/* AES Security Protocol 3-D */
	{IPPROTO_ETHERIP, "ETHERIP"},	/* Ethernet-within-IP Encapsulation */
	{IPPROTO_ENCAPHDR, "ENCAPHDR"},		/* Encapsulation Header */
	{IPPROTO_RAW, "RAW"}
};

#define IDBG_IPPROT_NAMES_MAX	(sizeof idbg_ipproto_names / sizeof *idbg_ipproto_names)

char *
idbg_ipfilt_pnames(unsigned char proto)
{
	unsigned short ix;

	for (ix=0; ix < IDBG_IPPROT_NAMES_MAX; ix++) {
		if (proto == idbg_ipproto_names[ix].proto_value) { /* match */

			return (idbg_ipproto_names[ix].proto_name);
		}
	}
	return "???";	/* unknown protocol */
}

void
idbg_ipfilt_print(struct ipkernelfilter *ipkf_p)
{
	struct in_addr_bytes src, dst;
	int i;

	qprintf("  entry 0x%x, next 0x%x, ifnet 0x%x, idle %d\n",
		ipkf_p, ipkf_p->next, ipkf_p->interface, ipkf_p->idle);

	qprintf("   verdict %d [%s], proto %d [%s],",
		ipkf_p->verdict,
		idbg_ipfilt_vnames[(ipkf_p->verdict & idbg_ipfilt_vmask)],
		ipkf_p->proto,
		idbg_ipfilt_pnames(ipkf_p->proto)
		);

	switch(ipkf_p->proto) {

	  case IPPROTO_ICMP:
		qprintf(" icmp_type %d, icmp_code %d\n",
			ipkf_p->u1.s_port, ipkf_p->u2.d_port);
		break;

	  case IPPROTO_IGMP:
		qprintf(" igmp_type %d, igmp_code %d\n",
			ipkf_p->u1.s_port, ipkf_p->u2.d_port);
		break;

	  default: /* IPPROTO_TCP and IPPROTO_UDP also land here */
		qprintf(" s_port %d, d_port %d\n",
			ipkf_p->u1.s_port, ipkf_p->u2.d_port);
		break;
	}

	bcopy (&ipkf_p->src_addr, &src, sizeof(int));
	bcopy (&ipkf_p->dst_addr, &dst, sizeof(int));

	for (i=0; i < sizeof(int); i++) {
		src.b[i] &= 0xFF;
		dst.b[i] &= 0xFF;
	}
       qprintf("   src_addr %d.%d.%d.%d [0x%x], dst_addr %d.%d.%d.%d [0x%x]\n",
		src.b[0], src.b[1], src.b[2], src.b[3],ipkf_p->src_addr.s_addr,
		dst.b[0], dst.b[1], dst.b[2], dst.b[3],ipkf_p->dst_addr.s_addr
		);

	return;
}

/*
 * idbg procedure, dynamically registered, which prints the IP filter cache
 *
 * kp ipfilt addr  -- print kernel IP filter cache from symmon
 */
void
idbg_ipfilt(__psint_t addr)
{
	int i;
	register struct ipkernelfilter *ipkf;

	qprintf(" ipfilter_active_flag %d ipfilterd_inactive_behavior %d\n",
		ipfilter_active_flag, ipfilterd_inactive_behavior);

	qprintf(" numipkflt %d ipfilter_ttl %d filtercache_search_depth %d\n",
		numipkflt, ipfilter_ttl, filtercache_search_depth);

	qprintf(" ipfiltqueue 0x%x ipkfq_head 0x%x\n\n",
		&ipfiltqueue, ipkfq_head);

	if (!ipfilter_active_flag) {
		qprintf("kernel ipfilter not active\n");
		return;
	}

	if (addr == -1) { /* dump entire ip filter cache */

		ipkf = ipkfq_head;
		while (ipkf) {
			idbg_ipfilt_print(ipkf);
			ipkf = ipkf->next;
			if (ipkf == ipkfq_head) { /* back to head so quit */
				break;
			}
		}
		qprintf(" \n\nipfiltercache array \n\n");
		for (i = 0; i < (numipkflt ); i++) {
			idbg_ipfilt_print(&ipfiltercache[i]);
		}
	} else { /* use the supplied address */
		idbg_ipfilt_print((struct ipkernelfilter *)addr);
	}
	return;
}

/*
 * Print out an IP filter cache entry
 */
void
ipfilter_idbg(void)
{
	if (ipfilter_idbgflag == 0) { /* register the debug proc */
		ipfilter_idbgflag = 1;
		idbg_addfunc("ipfilt", idbg_ipfilt);
	}
	return;
}
#endif /* DEBUG */

/*
 * hash lookup procedure to check if this packet is for this system.
 * Returns 1 if for us and zero otherwise.
 *
 * ipfilter_kernel_match(struct hashbucket *h, struct in_addr *key,
 *  struct ifnet *arg1, caddr_t arg2)
 */
/* ARGSUSED */
int
ipfilter_kernel_match(struct hashbucket *h, caddr_t key, caddr_t arg1,
	caddr_t arg2)
{
	struct in_ifaddr *ia = (struct in_ifaddr *)h;
	struct in_addr *addr = (struct in_addr *)key;
	int match = 0;

	if (h->flags & HTF_INET) {
		if (IA_SIN(ia)->sin_addr.s_addr == addr->s_addr)
			match = 1;
	}
	return match;
}

/* 
 * Screen inbound packets against access control database. 
 */
int
ipfilter_kernel(struct ifnet *ifp, struct mbuf *m)
{
	register struct ipkernelfilter *ipkf;
	register struct mbuf *ipfq, *m1;
	struct ip *iphd, *iphdq;
	struct ifheader *hdr;
	u_short dport, qdport, sport, qsport;
	int hlen;
	struct in_ifaddr *ia;
#if (_MIPS_SZPTR == 64)
	__psint_t ifhdr;
#endif

	iphd = mtod(m, struct ip *);
	/*
 	 * Loopback packets are always accepted.
 	 */
	ia = (struct in_ifaddr *)hash_lookup(&hashinfo_inaddr,
		ipfilter_kernel_match, (caddr_t)(&(iphd->ip_src)),
		(caddr_t)ifp, (caddr_t)0);

	if (ia) { /* it's for us */
		return IPF_ACCEPTIT;
	}
	/*
	 * When daemon has died or been killed, obey configurable
	 * ipfilterd inactive behavior.
	 */
	if (!ipfilter_active_flag) {
		if (ipfilterd_inactive_behavior) {
			m_freem(m);
			return IPF_DROPIT;
		} else {
			return IPF_ACCEPTIT;
		}
	}
	/*
 	 * Accept all fragments without filtering, but not the head -
	 * the fragments are useless without the fragment head, 
 	 * and the head will contain port info needed for screening.
 	 */
	if (iphd->ip_off & IP_OFFMASK) {
		return IPF_ACCEPTIT;
	}

	hlen = iphd->ip_hl << 2;
	switch(iphd->ip_p) {
	  case IPPROTO_TCP: {
		struct tcphdr *tcphd = nextproto(iphd, hlen, struct tcphdr *);
		dport = tcphd->th_dport;
		sport = tcphd->th_sport;
		}
		break;
	  case IPPROTO_UDP: {
		struct udphdr *udphd = nextproto(iphd, hlen, struct udphdr *);
		dport = udphd->uh_dport;
		sport = udphd->uh_sport;
		}
		break;
	  case IPPROTO_ICMP: {
		struct icmp *icmpp = nextproto(iphd, hlen, struct icmp *);
		sport = (u_short)(icmpp->icmp_type);
		dport = (u_short)(icmpp->icmp_code);
		}
		break;
	  case IPPROTO_IGMP: {
		struct igmp *igmpp = nextproto(iphd, hlen, struct igmp *);
		sport = (u_short)(igmpp->igmp_type);
		dport = (u_short)(igmpp->igmp_code);
		}
		break;
	  default:
		dport = sport = 0;
		break;
	}
        /*
         * Return 0 if can keep it, 1 otherwise (dropped because it hit
         * a filter, or was placed on the ioctl pending queue). If it hit
         * a filter, and our search depth is greater than the
         * MRU-invoking depth (filtercache_search_depth),
         * put that entry at the head of the filter cache.
         * Refresh timer on hits.
         */
	IPKFILTER_MRRLOCK();

        for(ipkf = ipkfq_head; ipkf; ipkf = ipkf->next) {

		if (ipkf->verdict == IPF_FREE) { /* skip unused entry */
			continue;
                }

                if ((sport == ipkf->srcport) &&
                   (dport == ipkf->dstport) &&
                   (ifp == ipkf->interface) &&
                   (iphd->ip_src.s_addr == ipkf->src_addr.s_addr) &&
                   (iphd->ip_dst.s_addr == ipkf->dst_addr.s_addr) &&
                   (iphd->ip_p == ipkf->proto)) {
			/*
			 * FOUND MATCHING filter for this packet so keep
			 * holding write lock prior to vaoid refreshing the
			 * idle timeout indicator of this entry by the timer
			 */
			atomicClearUint(&(ipkf->idle), 0);

			/* return disposition of this packet */
                        switch (ipkf->verdict) {

                          case IPF_ACCEPTIT:
				IPKFILTER_MRUNLOCK();
                                return IPF_ACCEPTIT;

                          case IPF_GRABIT:
				IPKFILTER_MRUNLOCK();
				return IPF_GRABIT;

                          case IPF_DROPNOTIFY:
                          case IPF_DROPIT:
                          default:
				IPKFILTER_MRUNLOCK();
                                m_freem(m);
                                return IPF_DROPIT;
                        }
                }
        }
	IPKFILTER_MRUNLOCK();

	/*
 	 * Add this request to the pending ioctl queue, if there's room 
	 * and if we haven't queued it yet for a previous packet (walk 
	 * through queue checking for matches without actually touching queue).
 	 */
	IPFILTERQ_MRRLOCK();

	if (IF_QFULL(&ipfiltqueue)) {
		IPFILTERQ_MRUNLOCK();
		m_freem(m);
		return IPF_DROPIT;
	}
	ipfq = ipfiltqueue.ifq_head;
	while(ipfq) {
		hdr = mtod(ipfq, struct ifheader *);
                iphdq = nextproto(hdr, sizeof(struct ifheader), struct ip *);
		hlen = iphdq->ip_hl << 2;
		if((iphd->ip_src.s_addr == iphdq->ip_src.s_addr) &&
		   (iphd->ip_dst.s_addr == iphdq->ip_dst.s_addr) &&
                   (ifp == hdr->ifh_ifp) &&
		   (iphd->ip_p == iphdq->ip_p)) { 

        		switch(iphdq->ip_p) {
        		  case IPPROTO_TCP: {
                		struct tcphdr *tcphd = nextproto(iphdq, hlen,
							struct tcphdr *);
                		qdport = tcphd->th_dport;
                		qsport = tcphd->th_sport;
				}
                		break;
        		  case IPPROTO_UDP: {
                		struct udphdr *udphd = nextproto(iphdq, hlen,
				    			struct udphdr *);
                		qdport = udphd->uh_dport;
                		qsport = udphd->uh_sport;
				}
                		break;
        		  case IPPROTO_ICMP: {
                		struct icmp *icmpp = nextproto(iphdq, hlen,
							struct icmp *);
                		qsport = (u_short)(icmpp->icmp_type);
                		qdport = (u_short)(icmpp->icmp_code);
				}
                		break;
        		  case IPPROTO_IGMP: {
                		struct igmp *igmpp = nextproto(iphdq, hlen,
							struct igmp *);
                		qsport = (u_short)(igmpp->igmp_type);
                		qdport = (u_short)(igmpp->igmp_code);
				}
                		break;
			  default:
				qdport = qsport = 0;
                		break;
        		}
			if((dport == qdport) && (sport == qsport)) {
				IPFILTERQ_MRUNLOCK();
				m_freem(m);
				return(IPF_DROPIT);
			}
		}
		ipfq = ipfq->m_act;
	}
	IPFILTERQ_MRUNLOCK();

	/*
 	 * Originally assumed we always had space to stick an ifheader,
	 * because one was just taken off it in ipintr()...
	 * This assumption was wrong when IF_DEQUEHEADER throws away the first
	 * mbuf in a chain (which happens for eisa token ring) so must fix
	 * that here by doing extra checking.
	 */
	hlen = sizeof(struct ifheader);

#if (_MIPS_SZPTR == 64)
	ifhdr = (__psint_t)m + (m->m_off - sizeof(struct ifheader));
	/*
	 * In 64-bit address poiner size check if header is 8 byte aligned
	 * and if not the distance from 8 byte boundary
	 */
	if ((ifhdr & 0x7) != 0)	{ /* Not 8 byte aligned */
		hlen += (ifhdr & 0x7);
	}
#endif /* _MIPS_SZPTR */

	if ((!M_HASCL(m) && (m->m_off >= hlen + MMINOFF)) ||
	    (M_HASCL(m) && ((mtod(m, long) - m->m_farg) >= hlen))) {

		M_ADJ(m, -hlen);
		IF_INITHEADER(mtod(m, caddr_t), ifp, hlen);
	}
	else {
		hlen = sizeof(struct ifheader);
		MGET(m1, M_DONTWAIT, MT_HEADER);
		if (m1 == (struct mbuf *)0) {
			m_freem(m);
			return IPF_DROPIT;
		}
		M_INITIFHEADER(m1, ifp, hlen);
		m1->m_next = m;
		m = m1;
		if ((m = m_pullup(m, hlen + sizeof(struct ip))) == 0) {
			return IPF_DROPIT;
		}
	}

	IPFILTERQ_MRWLOCK();
	IF_ENQUEUE_NOLOCK(&ipfiltqueue, m);
	IPFILTERQ_MRUNLOCK();

	while (cvsema(&ipfilterq_wait))
	  ; /* wait for v sema operation */
	return IPF_DROPIT;
}

/* 
 * IP Filtering ioctl handler 
 */
#define COPYSIZE sizeof(struct ipfilter_ioctl)

/*ARGSUSED*/
int
ipfilterioctl(int fd, int cmd, caddr_t addr)
{
	u_char	data[COPYSIZE + 2];

	if (!_CAP_ABLE(CAP_NETWORK_MGT))
		return EPERM;
	if (!ipfilter_active_flag)
		return EACCES;
        if (copyin(addr, (caddr_t)data, COPYSIZE))
		return EFAULT;

	switch  (cmd) {

	  case ZIOCIPFILTER: {
		struct ipfilter_ioctl *ipf_i;
		struct mbuf 		*m;
		struct ifheader 	*ifh;
		struct ip 		*iphd;
		int 			hlen;
		struct ipkernelfilter 	*ipkf, *ipkf_prev;
		struct ipkernelfilter 	*oldest_ipkf, *oldest_ipkf_prev;
                char *cp, *ep;
		u_int oldest_idle;

		ipf_i = (struct ipfilter_ioctl *)data;

		if (ipf_i->verdict == 0) { /* during initialization */
			goto findanother;
		}
		/*
		 * Find a free ipkernelfilter entry otherwise we use the tail
		 * of the listqueue. We copy the verdict and other information
		 * into that entry and make it the head of the list.
		 *
		 * If verdict was to drop the packet then call m_freem()
 		 * to release the mbuf associated with the packet.
		 * Next we look at the ioctl pending queue for the next packet
		 * to screen. If none are available we sleep otherwise if one
		 * is found we then copy it's data into ipf_i and complete
		 * the ioctl operation.
 		 */
		IPKFILTER_MRWLOCK();

		/* 
 		 * Find a filter cache entry to use; We'd like it to be the
		 * first free entry if one exists OR the oldest entry if all
		 * are in use OR if noe are found that meet that criteria
		 * and we're about to run off the end of the list we use
		 * the last cache entry.
		 *
		 * NOTE: This code does NOT require that the cache list be
		 * ordered strictly LRU in order of appearance since during
		 * our scan we first look for a free entry, next we'll use
		 * the oldest entry if no free one's are found IF it
		 * is older than the last entry on the list.
		 */
		ipkf = ipkf_prev = ipkfq_head;
		oldest_ipkf = oldest_ipkf_prev = (struct ipkernelfilter *)0;
		oldest_idle = 0;

		while (ipkf) {

			if (ipkf->verdict == IPF_FREE) { /* free entry */
				goto got_one;
			}
			if (ipkf->next == 0) { /* about to run off end */
				break;
			}
			/* note oldest entry during filter cache scan */
			if (ipkf->idle > oldest_idle) {
				oldest_idle = ipkf->idle;
				oldest_ipkf_prev = ipkf_prev;
				oldest_ipkf = ipkf;
			}

			/* save previous entry's address */
			ipkf_prev = ipkf;
			ipkf = ipkf->next;
		}
		/*
		 * We ran off the end of the list so check if we have found
		 * the oldest entry AND it's older than the last ipfilter
		 * cache entry on the list before using it. If it's younger
		 * we'll use the last entry.
		 */
		if (oldest_ipkf && (oldest_ipkf->idle > ipkf->idle)) {

			ipkf_prev = oldest_ipkf_prev;
			ipkf = oldest_ipkf;
		}
got_one:
		/* got an entry to use */
		IPFILTERQ_MRWLOCK();
		IF_DEQUEUE_NOLOCK(&ipfiltqueue, m);
		IPFILTERQ_MRUNLOCK();

		ASSERT(m != 0);
		ifh = mtod(m, struct ifheader *);
		iphd = nextproto(ifh, ifh->ifh_hdrlen, struct ip *);

		hlen = iphd->ip_hl<<2;

		/* keep holding write lock to update 'ipkf' entry */
		ipkf->dst_addr.s_addr = iphd->ip_dst.s_addr;
		ipkf->src_addr.s_addr = iphd->ip_src.s_addr;
		ipkf->proto = iphd->ip_p;
		ipkf->verdict = ipf_i->verdict;
                ipkf->interface = ifh->ifh_ifp;
		ipkf->idle = 0;

                switch(iphd->ip_p) {
                  case IPPROTO_TCP: {
                        struct tcphdr *tcphd = nextproto(iphd, hlen,
							struct tcphdr *);
                        ipkf->dstport = tcphd->th_dport;
                        ipkf->srcport = tcphd->th_sport;
			}
                        break;
                  case IPPROTO_UDP: {
                        struct udphdr *udphd = nextproto(iphd, hlen,
							struct udphdr *);
                        ipkf->dstport = udphd->uh_dport;
                        ipkf->srcport = udphd->uh_sport;
			}
                        break;
                  case IPPROTO_ICMP: {
                        struct icmp *icmpp = nextproto(iphd, hlen,
							struct icmp *);
                        ipkf->srcport = (u_short)icmpp->icmp_type;
                        ipkf->dstport = (u_short)icmpp->icmp_code;
			}
                        break;
                  case IPPROTO_IGMP: {
                        struct igmp *igmpp = nextproto(iphd, hlen,
							struct igmp *);
                        ipkf->srcport = (u_short)igmpp->igmp_type;
                        ipkf->dstport = (u_short)igmpp->igmp_code;
			}
                        break;
		  default:
                        ipkf->srcport = 0;
                        ipkf->dstport = 0;
                        break;
                }
		/* 
 		 * NOTE: 'ipkf' points to either:
		 * 1. the last entry in the LRU managed queue OR
		 * 2. first free entry (if the LRU cache isn't filled) OR
		 *
		 * 'ipkf' can NOT be NULL since we check for the case before
		 * we run off the list and set 'ipkf' to be the last entry.
		 * So we now make this entry the new queue head. This old
		 * code is bogus since you don't have the write lock!
 		 */
		if (ipkf != ipkfq_head) {
			/*
			 * Make 'ipkf' the new queue head, this assumes
			 * previous entry in list is 'ipkf_prev'.
			 */
			ipkf_prev->next = ipkf->next;
			ipkf->next = ipkfq_head;
			ipkfq_head = ipkf;
		}

		/* Now, act on verdict */
		switch (ipkf->verdict) {

		case IPF_ACCEPTIT:
		case IPF_GRABIT:

			/* drop filter cache lock */
			IPKFILTER_MRUNLOCK();

			/*
			 * TTL was changed above, so recompute checksum
			 * before enqueueing to IP input queue.
			 * Set proper mbuf length and offset for checksum.
			 */
			m->m_off += sizeof(struct ifheader);
			m->m_len -= sizeof(struct ifheader);
			iphd->ip_sum = 0;
			iphd->ip_sum = in_cksum(m, hlen);

			m->m_off -= sizeof(struct ifheader);
			m->m_len += sizeof(struct ifheader);
			network_input(m, AF_INET, 0);
			break;

		case IPF_DROPIT:
		case IPF_DROPNOTIFY: 
		default:
			/* drop filter cache lock */
			IPKFILTER_MRUNLOCK();
			m_freem(m);
			break;
		}
		/*
 		 * See if there are any other packets waiting for screening.
		 * If so, complete the ioctl by grabbing the info and returning
		 * it to the daemon. If not sleep until ipintr() calls wakeup()
		 * for another packet.
 		 */
findanother:
		IPFILTERQ_MRRLOCK();
		while(((m = ipfiltqueue.ifq_head) == 0) && 
						ipfilter_active_flag) {
			IPFILTERQ_MRUNLOCK();
			if (psema(&ipfilterq_wait, (PZERO+1))) {
				return EINTR;
			}
			IPFILTERQ_MRRLOCK();
		}
		if (!ipfilter_active_flag) {
			IPFILTERQ_MRUNLOCK();
			return EACCES;
		}
                ifh = mtod(m, struct ifheader *);
		iphd = nextproto(ifh, ifh->ifh_hdrlen, struct ip *);
		bzero((caddr_t)ipf_i, sizeof(struct ipfilter_ioctl));
		bcopy((caddr_t)iphd, (caddr_t)ipf_i->packet, IPFILTERSIZE);

                ep = (caddr_t)((__psunsigned_t)ipf_i->ifname + IFNAMSIZE - 1);
		(void) strncpy(ipf_i->ifname, ifh->ifh_ifp->if_name,IFNAMSIZE);
                for (cp = ipf_i->ifname; cp < ep && *cp; cp++)
                        ; /* complete copying name */

		sprintf(cp, "%d", ifh->ifh_ifp->if_unit);
		IPFILTERQ_MRUNLOCK();
		break;
	  }
	  default:
		return EOPNOTSUPP;
	}
        /*
         * Copy any data to user, size was
         * already set and checked above.
         */
        if (copyout(data, addr, COPYSIZE))
		return EFAULT;
        return 0;
}

/* 
 * Initialize kernel filter cache when filtering is enabled 
 */
#define ipfilter_timeout	sockd_timeout	/* changes to sockd_timeout */

void
ipkernelfilter_init(void)
{
	register struct ipkernelfilter *ipk, *ipkp;
	int i;

	ipkfq_head = (struct ipkernelfilter *)ipfiltercache;
        bzero ((caddr_t)ipfiltercache,
               (sizeof(struct ipkernelfilter) * numipkflt));
	ipk = ipkp = ipkfq_head;

	for (i = 0; i < (numipkflt - 1); i++, ipkp++) {
		ipkp->next = ++ipk;
	}

	ipfiltqueue.ifq_maxlen = IFQ_MAXLEN;
	IPFILTER_INITLOCKS();

	/* XXX back to timeout after converting to mr spin lock  huy */
        ipfilter_timeout(ipfiltertimer, (caddr_t) 0, HZ);

#ifdef DEBUG
	ipfilter_idbg();
#endif
	return;
}

/* ARGSUSED */
int
ipfilteropen(dev_t *devp, int flag, int otyp, struct cred *crp)
{
	if(ipfilter_active_flag) {
		return EBUSY;
	}
	ipkernelfilter_init();
	ipfilterflag = 1;		/* set on first open, stays set */
	ipfilter_active_flag = 1;	/* current state of daemon activity */
	return 0;
}

/* ARGSUSED */
int
ipfilterclose(dev_t dev, int flag, int otyp, struct cred *crp)
{
	struct mbuf *m;

	IPKFILTER_MRWLOCK();
	ipfilter_active_flag = 0;
        bzero ((caddr_t)ipfiltercache,
               (sizeof(struct ipkernelfilter) * numipkflt));
	IPKFILTER_MRUNLOCK();

	/* drain and release mbuf's on fileter queue */
	IPFILTERQ_MRWLOCK();
next:
	IF_DEQUEUE_NOLOCK(&ipfiltqueue, m);
	if (m) {
		m_freem(m);
		goto next;
	}
	IPFILTERQ_MRUNLOCK();

	while (cvsema(&ipfilterq_wait));
	IPFILTER_FREELOCKS();
	return(0);
}

/*
 * age and recover idle cache entries
 */
void
ipfiltertimer(void)
{
	register struct ipkernelfilter *ipkf, *ipkf_prev;
	int depth;
	u_int oldest_idle;
	/*
	 * Fix for 264076; bail out if we're not active.  Locks are
	 * dynamically freed in 5.3, and we can't try to access them if
	 * the daemon is not running.
	 * On uniprocessor machines we are called from the clock handler so
	 * we lower the spl to 'splnet' but no lower to protect the values
	 * in the loop. On multi-processors we're called from a clock process
	 * so no spl protection is required.
	 */
	/*
	 * Only age entries and schedule new timeout if daemon is active
	 * Hold the list write lock when aging cache entries.
	 */
	IPKFILTER_MRWLOCK();

	if (!ipfilter_active_flag) { /* inactive so exit now */
		goto endit;
	}
	/*
	 * user daemon active; So age in-use filter cache entries
	 */
       	ipfilter_timeout(ipfiltertimer, (caddr_t)0, HZ);
	oldest_idle = 0;
	depth = 0;

        for (ipkf = ipkf_prev = ipkfq_head; ipkf; 
	     ipkf_prev = ipkf, ipkf = ipkf->next) {

               	if (ipkf->verdict == IPF_FREE) { /* skip free entry */
			depth++;
			continue;
                }
		if (ipkf->idle > ipfilter_ttl) { /* free it */
			ipkf->verdict = IPF_FREE;
			depth++;
			continue;
		}

		/* age this entry */
		ipkf->idle++;

		if (ipkf->idle > oldest_idle) { /* save oldest idle value */
			oldest_idle = ipkf->idle;
		}
		/*
		 * if depth greater than that specified AND this isn't
		 * the oldest entry, move to the front of the LRU list.
		 * Also we must check the following cases:
		 * 1. The entry isn't the head of the list OR
		 * 2. The previous and current pointers aren't same
		 * 3. We are creating a circularly linked list
		 */
		if ((ipkf->idle < oldest_idle) &&
		    (depth > filtercache_search_depth)) {

			if (ipkf != ipkfq_head) {
				/*
				 * Maintain as LRU cache the first
				 * 'filtercache_search_depth' entries,
				 * so move 'ipkf' entry to the head
				 *
				 * NOTE: Added some bullet proofing
				 * here to avoid getting a circularly
				 * linked list, aka bug 427672!
				 */
#ifdef IPFILTER_BULLET_PROOF
				if ((ipkf_prev != ipkf) &&
				    (ipkf_prev != ipkf->next) &&
				    (ipkfq_head != ipkf->next)) {

					ipkf_prev->next = ipkf->next;
					ipkf->next = ipkfq_head;
					ipkfq_head = ipkf;
					ipkf = ipkf_prev;
				}
#else
				ipkf_prev->next = ipkf->next;
                               	ipkf->next = ipkfq_head;
                               	ipkfq_head = ipkf;
				ipkf = ipkf_prev;
#endif /* IPFILTER_BULLET_PROOF */
			}
		}
		depth++;
	}
endit:
	IPKFILTER_MRUNLOCK();

        return;
}

