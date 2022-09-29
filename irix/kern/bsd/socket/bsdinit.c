/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Initialize and miscellany for Irix BSD networking.
 */
#ident "$Revision: 3.36 $"

#include "tcp-param.h"
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/sema.h>
#include <sys/hashing.h>

#include <sys/capability.h>
#include <sys/sat.h>

#include <sys/socketvar.h>

#include <sys/sysmp.h>
#include <sys/mbuf.h>
#include <sys/mac_label.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <sys/sockd.h>
#include <net/netisr.h>
#include <sys/vsocket.h>
#include <ksys/vhost.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/tcpipstats.h>

void
bsd_init(void)
{
	int s;
	static int initialized = 0;
	extern int domaininit(void);
	extern int in_init(void);
	extern struct bsd_kernaddrs bsd_kernaddrs;
	extern char *kend;

	s = splimp();
	if (!initialized) {
		extern zone_t *socket_zone;
		socket_zone = kmem_zone_init(sizeof(struct socket), "socket");
		if (socket_zone == 0) {
			cmn_err_tag(280,CE_PANIC, "can't initialize socket zone");
		}
		loattach();		/* start the loop back interface */
		ifinit();		/* generally initialize interfaces */
		in_init();		/* initialize internet globals */
		domaininit();		/* initialize the domain tables */
		initialized = 1;
		spl0();
		netisr_init();
		sockd_init();
		tpisockd_init();
		bsd_kernaddrs.bk_addr[_KA_END] = kend;
	}
	splx(s);
}

__int32_t	hostid;		/* XXX move this... */

/* ARGSUSED */
int
gethostid(void *uap, rval_t *rvp)
{

	VHOST_GETHOSTID((int *) &rvp->r_val1);
	rvp->r_val1 = rvp->r_val1 >> 32;
	return 0;
}

sethostid(sysarg_t *uap)
{

	if (!_CAP_ABLE(CAP_SYSINFO_MGT))
		return EPERM;
	VHOST_SETHOSTID(*uap);
	_SAT_HOSTID_SET(*uap,0);
	return 0;
}

/* ARGSUSED */
void
phost_gethostid(bhv_desc_t *bdp, int *id)
{
	*id = hostid;
}

/* ARGSUSED */
void
phost_sethostid(bhv_desc_t *bdp, int id)
{
	hostid = id;
}
