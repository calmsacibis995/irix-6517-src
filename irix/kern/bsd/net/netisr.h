/*
 * Copyright (c) 1980, 1986 Regents of the University of California.
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
 *	@(#)netisr.h	7.4 (Berkeley) 6/27/88
 */
#ident "$Revision: 3.26 $"

#ifdef __cplusplus
extern "C" { 
#endif

/*
 * This is the eternal interface to network input.
 * At initialization, call network_input_setup for each
 * address family, and then network_input on each packet.
 */

#ifdef _KERNEL
extern void netisr_init(void);
struct ifnet;
struct route;
typedef void (network_input_t)(struct mbuf*, struct route *);

#define NETPROC_MORETOCOME 0x1000 /* hint for batching */

extern int network_input(struct mbuf *m, int af, int flags);
extern void network_input_setup(int af, network_input_t func);

void init_max_netprocs(void);

/*
 * For backward source compatibility of drivers
 */
#define NETISR_IP	0	/* for AF_INET */
#define NETISR_RAW	1	/* for AF_UNSPEC */

extern void schednetisr(int anisr);

#endif /* _KERNEL */
#ifdef __cplusplus
}
#endif 
