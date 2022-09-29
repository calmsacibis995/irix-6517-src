/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Interface table routines for the remote snooping deamon
 *
 *	$Revision: 1.13 $
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

#include <bstring.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include "expr.h"
#include "macros.h"
#include "heap.h"
#include "protocol.h"
#include "scope.h"
#include "snoopd.h"
#include "snooper.h"
#include "strings.h"

/*
 * Table of interfaces available
 */
struct iftab *iftable;
static struct iftab *iftail;

static char enp[] = "enp";
static char mtu[] = "MTU";

#define DEFAULT_MTU	4096

void
iftable_init(void)
{
	int s, n;
	char buf[8192];
	struct ifconf ifc;
	struct ifreq *ifr;
	struct iftab *ift, *prev;
	Protocol *pr;
	Symbol *mtusym;
	struct hostent *hp;
#define	ifr_sin(ifr)	((struct sockaddr_in *) &(ifr)->ifr_addr)

	/* Open a socket */
#ifdef sun
	s = socket(PF_INET, SOCK_DGRAM, 0);
#else
	s = socket(PF_RAW, SOCK_RAW, RAWPROTO_SNOOP);
#endif
	if (s < 0)
		return;

	/* Fill in information about our interfaces */
	ifc.ifc_len = sizeof buf;
	ifc.ifc_buf = buf;
	if (ioctl(s, SIOCGIFCONF, (caddr_t) &ifc) < 0)
		return;
	iftail = iftable = 0;
	n = ifc.ifc_len / sizeof *ifr;
	for (ifr = ifc.ifc_req; --n >= 0; ifr++) {
		if (ifr->ifr_addr.sa_family != AF_INET)
			continue;
		ift = znew(struct iftab);
		(void) strncpy(ift->if_ifname, ifr->ifr_name, RAW_IFNAMSIZ);
		pr = getrawprotobyif(ift->if_ifname);
		if (pr == 0) {
			delete(ift);
			continue;
		}
		ift->if_rawproto = pr;
		mtusym = pr_lookupsym(pr, mtu, constrlen(mtu));
		if (mtusym != 0 && mtusym->sym_type == SYM_NUMBER)
			ift->if_mtu = mtusym->sym_val;
		else
			ift->if_mtu = DEFAULT_MTU;
		ift->if_addr.s_addr = ifr_sin(ifr)->sin_addr.s_addr;
		hp = gethostbyaddr(&ift->if_addr, sizeof ift->if_addr, AF_INET);
		ift->if_hostname = strdup(hp ? hp->h_name
					  : inet_ntoa(ift->if_addr));
		ift->if_file = 0;
		ift->if_prev = iftail;
		ift->if_next = 0;
		if (iftail != 0)
			iftail->if_next = ift;
		else
			iftable = ift;
		iftail = ift;
	}
	close(s);
#undef	ifr_sin
}

/*
 * Match an interface string to one of our interfaces
 */
struct iftab *
iftable_match(char *interface)
{
	struct iftab *ift;
	struct hostent *hp;
	u_long addr;

	if (iftable == 0)
		return 0;

	if (interface == 0)
		return iftable;

	/* Match file name */
	if (access(interface, 0) == 0) {
		/* Create a new interface for this file */
		ift = znew(struct iftab);
		ift->if_file = 1;
		ift->if_filename = strdup(interface);
		ift->if_prev = iftail;
		ift->if_next = 0;
		iftail->if_next = ift;
		iftail = ift;
		return ift;
	}

	/* Match by interface name or host name */
	for (ift = iftable; ift != 0; ift = ift->if_next) {
		if (ift->if_file != 0)
			continue;
		if (strcmp(ift->if_ifname, interface) == 0 ||
		    strcmp(ift->if_hostname, interface) == 0)
			return ift;
	}

	/* Match by address */
	hp = gethostbyname(interface);
	if (hp != 0) {
		bcopy(hp->h_addr, &addr, sizeof addr);
		for (ift = iftable; ift != 0; ift = ift->if_next) {
			if (ift->if_file != 0)
				continue;
			if (addr == ift->if_addr.s_addr)
				return ift;
		}
	}

	return 0;
}

/*
 * Interface snooping routines
 */
enum snoopstat
errno_to_snoopstat(int error)
{
	switch (error) {
	  case 0:
		return SNOOP_OK;
	  case EPERM:
	  case EACCES:
		return SNOOPERR_PERM;
	  case EAGAIN:
	  case ENFILE:
	  case EMFILE:
	  case EADDRNOTAVAIL:
	  case ENOBUFS:
		return SNOOPERR_AGAIN;
	  case EINVAL:
		return SNOOPERR_INVAL;
	  case ENXIO:
		return SNOOPERR_NOIF;
	  default:
		return SNOOPERR_IO;
	}
}

enum snoopstat
snoop_status(struct iftab *ift)
{
	if (ift->if_snooper == 0)
		return SNOOP_OK;
	return errno_to_snoopstat(ift->if_snooper->sn_error);
}

int
snoop_open(struct iftab *ift)
{
	Snooper *sn;
	ExprError error;
	int save, on;
	extern struct options opts;

	if (ift->if_snooper != 0) {
		ift->if_clients++;
		return 0;
	}

	/*
	 * Create a snooper for the given interface.
	 */
	if (ift->if_file == 0) {
#ifdef sun
		sn = sunsnooper(ift->if_ifname, opts.opt_queuelimit);
#else
		sn = localsnooper(ift->if_ifname, 0, opts.opt_queuelimit);
#endif
		if (sn == 0)
			return errno;
	} else {
		Symbol *mtusym;

		sn = tracesnooper(ift->if_filename, "r", 0);
		if (sn == 0)
			return errno;
		ift->if_rawproto = sn->sn_rawproto;
		mtusym = pr_lookupsym(sn->sn_rawproto, mtu, constrlen(mtu));
		if (mtusym != 0 && mtusym->sym_type == SYM_NUMBER)
			ift->if_mtu = mtusym->sym_val;
		else
			ift->if_mtu = DEFAULT_MTU;
	}

	/*
	 * Add a promiscuous filter.
	 */
	if (sn_add(sn, 0, &error) < 0) {
		save = sn->sn_error;
		sn_destroy(sn);
		return save;
	}

	/*
	 * Initialize a packet buffer
	 */
	pb_init(&ift->if_pb, opts.opt_packetbufsize, sn->sn_packetsize);

	/*
	 * Hack to wait for ENP-10L board to reset
	 */
	if (strncmp(ift->if_ifname, enp, (sizeof enp) - 1) == 0)
		sleep(1);

	/*
	 * Set the snoop socket non-blocking
	 */
	on = 1;
	(void) ioctl(sn->sn_file, FIONBIO, &on);

	ift->if_snooper = sn;
	ift->if_snooplen = ift->if_mtu;
	ift->if_clients = 1;
	return 0;
}

void
snoop_start(struct iftab *ift)
{
	if (ift->if_snooper != 0 && ift->if_started++ == 0)
		sn_startcapture(ift->if_snooper);
}

void
snoop_setsnooplen(struct iftab *ift, u_int snooplen)
{
	if (ift->if_snooper == 0)
		return;

	ift->if_snooplen = MIN(snooplen, ift->if_mtu);
	sn_setsnooplen(ift->if_snooper, ift->if_snooplen);
}

void
snoop_seterrflags(struct iftab *ift, u_short flags)
{
	Snooper *sn;

	sn = ift->if_snooper;
	if (sn == 0)
		return;
	if (flags == 0) {
		if (--ift->if_errclients == 0) {
			ift->if_errflags = 0;
			sn_seterrflags(sn, 0);
		}
	} else {
		ift->if_errclients++;
		if ((flags & ift->if_errflags) == flags)
			return;
		ift->if_errflags |= flags;
		sn_seterrflags(sn, ift->if_errflags);
	}
}

void
snoop_freefilter(struct iftab *ift, struct filter *f)
{
	f->f_promisc = 0;
	if (f->f_errflags != 0) {
		snoop_seterrflags(ift, 0);
		f->f_errflags = 0;
	}
	if (f->f_expr) {
		ex_destroy(f->f_expr);
		f->f_expr = 0;
	}
}

int
snoop_update(struct iftab *ift, Expr *ex, ExprError *err, struct filter *f)
{
	struct sfset sfs;
	Snooper *sn;

	sfs_init(&sfs);
	sn = ift->if_snooper;
	if (!sfs_compile(&sfs, ex, sn->sn_rawproto, sn->sn_rawhdrpad, err))
		return 0;

	if (sfs.sfs_errflags != f->f_errflags) {
		if (f->f_errflags != 0)
			snoop_seterrflags(ift, 0);
		snoop_seterrflags(ift, sfs.sfs_errflags);
		f->f_errflags = sfs.sfs_errflags;
	}

	ex_destroy(f->f_expr);
	f->f_expr = ex;
	f->f_promisc = (ex == 0);
	return 1;
}

int
snoop_test(struct iftab *ift, u_short errflags, struct filter *f,
	   SnoopPacket *sp, int len)
{
	if (sp->sp_flags & SN_ERROR) {
		if (sp->sp_flags & (errflags | f->f_errflags))
			return 1;
	} else {
		if (f->f_promisc
		    || f->f_expr != 0
		    && SN_TEST(ift->if_snooper, f->f_expr, sp, len)) {
			return 1;
		}
	}
	return 0;
}

int
snoop_read(struct iftab *ift, u_int readcount)
{
	Snooper *sn = ift->if_snooper;
	int pc, cc;
	Packet *p;

	for (pc = 0; pc < readcount; pc++) {
		p = pb_get(&ift->if_pb);
		cc = sn_read(sn, &p->p_sp, sn->sn_packetsize);
		if (cc > 0) {
			p->p_len = cc;
			continue;
		}

		/* Return a zero length packet on EOF */
		if (cc == 0 && ift->if_file != 0) {
			p->p_len = 0;
			sn_stopcapture(sn);
			return pc + 1;
		}

		(void) pb_put(&ift->if_pb, p);
		if (cc < 0) {
			switch (sn->sn_error) {
			    case EINTR:
			    case EWOULDBLOCK:
				return pc;

			    default:
				return cc;
			}
		}
		break;
	}
	return pc;
}

void
snoop_stop(struct iftab *ift)
{
	if (ift->if_snooper != 0 && --ift->if_started == 0)
		sn_stopcapture(ift->if_snooper);
}

void
snoop_close(struct iftab *ift)
{
	Snooper *sn;

	if (--ift->if_clients != 0)
		return;

	sn = ift->if_snooper;
	sn_stopcapture(sn);
	(void) sn_shutdown(sn, SNSHUTDOWN_READ);
	pb_finish(&ift->if_pb);
	sn_destroy(sn);
	ift->if_snooper = 0;
	ift->if_snooplen = 0;
	ift->if_errflags = 0;

	/*
	 * Hack to wait for ENP-10L board to reset
	 */
	if (strncmp(ift->if_ifname, enp, (sizeof enp) - 1) == 0)
		sleep(1);

	if (ift->if_file != 0) {
		if (ift->if_prev != 0)
			ift->if_prev->if_next = ift->if_next;
		if (ift->if_next != 0)
			ift->if_next->if_prev = ift->if_prev;
		if (iftail == ift)
			iftail = ift->if_prev;
		if (iftable == ift)
			iftable = ift->if_next;
		delete(ift->if_hostname);
		delete(ift);
	}
}
