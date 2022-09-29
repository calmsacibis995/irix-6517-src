/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.26 88/02/08 
 */


/*
 * subr_kudp.c
 * Subroutines to do UDP/IP sendto and recvfrom in the kernel
 */
#include "sys/debug.h"
#include "sys/sysmacros.h"
#include "sys/param.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/mbuf.h"
#include "sys/errno.h"
#include "sys/immu.h"
#include "sys/pda.h"
#include "sys/systm.h"
#include "sys/mac_label.h"
#include "net/if.h"
#include "net/route.h"
#include "netinet/in.h"
#include "netinet/in_systm.h"
#include "netinet/ip.h"
#include "netinet/in_pcb.h"
#include "netinet/ip_var.h"
#include "netinet/udp.h"
#include "netinet/udp_var.h"
#include "types.h"
#include "xdr.h"
#include "auth.h"
#include "clnt.h"


/*
 * General kernel udp stuff.
 * The routines below are used by both the client and the server side
 * rpc code.
 */

/*
 * Kernel recvfrom.
 * Pull address mbuf and data mbuf chain off socket receive queue.
 */
struct mbuf *
ku_recvfrom(so, from, lpp)
	struct socket *so;
	struct sockaddr_in *from;
	mac_label **lpp;
{
	register struct mbuf	*m;
	register struct mbuf	*m0;
	struct mbuf		*nextrecord;
	register struct sockbuf	*sb = &so->so_rcv;
	register int		len = 0;

	SOCKET_LOCK(so);
	m = sb->sb_mb;
	if (m == NULL) {
		SOCKET_UNLOCK(so);
		return (m);
	}
	nextrecord = m->m_act;

	if (from)
		*from = *mtod(m, struct sockaddr_in *);
	if (lpp)
		*lpp = mtod(m, struct sockaddrlbl *)->sal_lbl;

	/*
	 * Advance to the data part of the packet,
	 * freeing the address part (and rights if present).
	 */
	for (m0 = m; m0 && m0->m_type != MT_DATA && m0->m_type != MT_HEADER; ) {
		sbfree(sb, m0);
		m0 = m_free(m0);
	}
	if (m0 == NULL) {
		printf("ku_recvfrom: no body!\n");
		sb->sb_mb = nextrecord;
		SOCKET_UNLOCK(so);
		return (m0);
	}

	/*
	 * Transfer ownership of the remainder of the packet
	 * record away from the socket and advance the socket
	 * to the next record.  Calculate the record's length
	 * while we're at it.
	 */
	for (m = m0; m; m = m->m_next) {
		sbfree(sb, m);
		len += m->m_len;
	}
	sb->sb_mb = nextrecord;

	SOCKET_UNLOCK(so);
	return (m0);
}

/*
 * Kernel sendto.
 * Set addr and send off via UDP.
 * Used on Client side ONLY
 */
int
ku_sendto_mbuf(so, m, addr)
	struct socket *so;
	struct mbuf *m;
	struct sockaddr_in *addr;
{
	register struct inpcb *inp = sotoinpcb(so);
	struct inaddrpair ipr;
	int error;

	SOCKET_LOCK(so);

	if (error = in_pcbsetaddr(inp, addr, &ipr)) {
		printf("pcbsetaddr failed %d\n", error);
		m_freem(m);
	} else {
		error = udp_output(inp, m, &ipr, NULL);
	}

	SOCKET_UNLOCK(so);
	return (error);
}
