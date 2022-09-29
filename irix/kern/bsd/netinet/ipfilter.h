/*
 * ipfilter.h - definitions used for IP-level packet filtering
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

#ifndef __NETINET_IP_FILTER_H__
#define __NETINET_IP_FILTER_H__
#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.11 $"

/* verdict of filtering code */
#define IPF_ACCEPTIT	1
#define IPF_DROPIT	2
#define IPF_DROPNOTIFY	3		/* send ICMP error to sender */
#define IPF_GRABIT	4

#define IPFILTERSIZE	64	   /* maximum data to filter; catches options */
#define IFNAMSIZE       16         /* match name size in ifreq struct */

struct ipfilter_ioctl {
	u_char			packet[IPFILTERSIZE];
        char                    ifname[IFNAMSIZE];
	u_char			verdict;
};


#define ZIOCIPFILTER _IOWR('z', 127, struct ipfilter_ioctl) /* filter result */


struct ipkernelfilter {
        struct ipkernelfilter   *next;          /* link */
        struct in_addr          src_addr;       /* source IP addr */
        struct in_addr          dst_addr;       /* dest IP addr */
        struct ifnet            *interface;     /* interface packet uses */
	u_int			idle;		/* entry idle time */

        union {
                u_short         s_port;         /* source port */
                u_short         icmp_type;      /* icmp type */
                u_short         igmp_type;      /* igmp type */
        } u1;
        union {
                u_short         d_port;         /* dest port */
                u_short         icmp_code;      /* icmp code */
                u_short         igmp_code;      /* igmp code */
        } u2;
        u_char                  proto;          /* IP protocol number */
        u_char                  verdict;        /* accept or drop */
};
#define srcport         u1.s_port
#define icmptype        u1.icmp_type
#define igmptype        u1.igmp_type
#define dstport         u2.d_port
#define icmpcode        u2.icmp_code
#define igmpcode        u2.igmp_code

/* Definitions for stateful filtering of input/output */
extern struct mbuf * (*ipf_process)(struct mbuf *in_pkt, int if_index, int flags);

/* Valid values for flags defined here */
#define IPF_IN		0	/* packet arrived */
#define IPF_OUT		1	/* packet to be sent */


#ifdef __cplusplus
}
#endif
#endif /* !__NETINET_IP_FILTER_H__ */
