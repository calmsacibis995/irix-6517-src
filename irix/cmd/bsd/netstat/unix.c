/*

 * Copyright (c) 1983,1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */

#ifndef lint
static char sccsid[] = "@(#)unix.c	5.5 (Berkeley) 2/7/88";
#endif /* not lint */

/*
 * Display protocol blocks in the unix domain.
 */
#include <stdio.h>
#define _KMEMUSER	/*get definition of "struct file" and "struct socket" */
#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/un.h>
#if (_MIPS_SZLONG == 32)
#define ino_t ino64_t
#endif
#include <sys/unpcb.h>
#if (_MIPS_SZLONG == 32)
#undef	ino_t
#endif

#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/var.h>
#include <sys/fsid.h>
#include <sys/fstyp.h>
#include <sys/file.h>
#include <unistd.h>
#include <sys/mac_label.h>
#include <sys/so_dac.h>
#include <sys/vsocket.h>

#include "netstat.h"

void
unixpr(ns_off_t vaddr, ns_off_t unpaddr, struct protosw *unixsw)
{
	int count = 0;
	struct unpcb uentry;
	struct unpcb *unp;
	struct socket sock, *so = &sock;

	if (unpaddr == 0) {
		fprintf(stderr, "unpcb_list not in namelist.\n");
		return;
	}
	klseek(kmem, unpaddr, L_SET);
	if (read(kmem, &uentry, sizeof(uentry)) != sizeof(uentry)) {
		fprintf(stderr,"size mismatch on unpcb_list\n");
		return;
	}

	for (unp = (struct unpcb *)uentry.unp_lnext;
	     unp != (struct unpcb *)unpaddr; unp = uentry.unp_lnext) {
		klseek(kmem, (ns_off_t)unp, L_SET);
		if (read(kmem, (char *)&uentry, sizeof(uentry)) !=
		     sizeof(uentry)) {

			printf("???\n");
			break;
		}
		if (uentry.unp_socket) {
			klseek(kmem, (ns_off_t)uentry.unp_socket, L_SET);
			if (read(kmem, so, sizeof (*so)) != sizeof (*so))
				continue;
			if ((so->so_type != SOCK_DGRAM) && (so->so_type !=
				SOCK_STREAM)) {
				continue;
			}
			unixdomainpr(so, (caddr_t)uentry.unp_socket, &uentry);
		}
	}
}

static	char *socktype[] =
    { "#0", "dgram", "stream", "raw", "rdm", "seqpacket" };

void
unixdomainpr(register struct socket *so, caddr_t soaddr, struct unpcb *unp)
{
	struct mbuf mbuf, *m;
	struct sockaddr_un *sa;
	static int first = 1;
#ifdef sgi
	mac_label slabel;
	struct soacl	sacl;
#endif

	if (unp->unp_addr) {
		m = &mbuf;
		klseek(kmem, (ns_off_t)unp->unp_addr, L_SET);
		if (read(kmem, (char *)m, sizeof (*m)) != sizeof (*m))
			m = (struct mbuf *)0;
		if (m) {
			/* assumes cluster never used to hold sockaddr */
			sa = (struct sockaddr_un *)m->m_dat;
		}
	} else
		m = (struct mbuf *)0;
#ifdef _SESMGR
	if (havemac) {
		(void)fetchlabel(&slabel, (ns_off_t)so->so_label);
		if (! mac_dom(&plabel, &slabel))
			return;
		klseek(kmem, (ns_off_t)so->so_acl, 0);
		kread(kmem, &sacl, sizeof(struct soacl));
	}
#endif
	if (first) {
		printf("Active UNIX domain sockets\n");
#ifdef _SESMGR
		if (lflag)
			printf("%-8.8s ", "Label" );
#endif
		printf(
#if _MIPS_SZLONG == 64
"%-16.16s %-6.6s %-6.6s %-6.6s %16.16s %16.16s %16.16s %16.16s %-25.25s ",
#else
"%-8.8s %-6.6s %-6.6s %-6.6s %8.8s %8.8s %8.8s %8.8s %-25.25s ",
#endif
		       "Address", "Type", "Recv-Q", "Send-Q",
		       "Vnode", "Conn", "Refs", "Nextref", "Addr");
#ifdef _SESMGR
		if (lflag)
			printf("%-8.8s %s \n", "Souid", "Soacl");
		else
#endif
		    putchar('\n');
		first = 0;
	}
#ifdef _SESMGR
	if (lflag)
		labelpr(&slabel);
#endif
#if _MIPS_SZLONG == 64
	printf("%16lx %-6.6s %6d %6d %16lx %16lx %16lx %16lx",
#else
	printf("%8x %-6.6s %6d %6d %8x %8x %8x %8x",
#endif
	    soaddr, socktype[so->so_type], so->so_rcv.sb_cc, so->so_snd.sb_cc,
	    unp->unp_vnode, unp->unp_conn,
	    unp->unp_refs, unp->unp_nextref);
	if (m && (sa->sun_family == AF_UNIX))
		printf(" %-25.*s ", (int)(m->m_len - sizeof(sa->sun_family)),
		       sa->sun_path);
#ifdef _SESMGR
	else
		printf(" %25s ", "");
	if (lflag) {
		souidpr(so->so_uid);
		soaclpr(&sacl);
	}
#endif
	putchar('\n');
}
