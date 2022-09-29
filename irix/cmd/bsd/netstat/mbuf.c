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
static char sccsid[] = "@(#)mbuf.c 5.5 (Berkeley) 2/7/88 plus MULTICAST 1.1";
#endif /* not lint */

#include <stdio.h>
#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <curses.h>
#include <string.h>
#include <bstring.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/sysmp.h>
#include <sys/tcpipstats.h>
#include "netstat.h"
#include "cdisplay.h"

#define	YES	1

extern int	pagesize;
#undef CLBYTES
#define CLBYTES		pagesize

struct kna kna;
struct	mbstat mbstat, zmbstat;

extern char *pluraly(int);
extern char *plural(int);
extern char *plurales(int);

#define MBUF 1
#define OTHER 2
static struct mbtypes {
	int	mt_type;
	int	mt_flag;
	char	*(*mt_func)(int);
	char	*mt_name;
} mbtypes[] = {
	{ MT_DATA,	MBUF, plural, "data mbuf" },
	{ MT_HEADER,	MBUF, plural, "packet header mbuf" },
	{ MT_SONAME,	MBUF, plural, "socket name mbuf" },
	{ MT_SOOPTS,	MBUF, plural, "socket option mbuf" },
	{ MT_FTABLE,	MBUF, plural, "fragment reassembly queue header mbuf" },
	{ MT_RIGHTS,	MBUF, plural, "access rights mbuf" },
	{ MT_MRTABLE,	MBUF, plural, "multicast routing mbuf" },
	{ MT_IPMOPTS,	MBUF, plural, "internet multicast option mbuf" },
	{ MT_SOCKET,	OTHER, plural, "socket structure" },
	{ MT_PCB,	OTHER, plural, "protocol control block" },
	{ MT_RTABLE,	OTHER, plural, "routing data structure" },
	{ MT_HTABLE,	OTHER, pluraly, "IMP host table entr" },
	{ MT_ATABLE,	OTHER, plural, "address resolution table" },
	{ MT_SAT,	OTHER, plural, "security audit trail buffer" },
	{ MT_IFADDR,	OTHER, plurales, "interface address" },
	{ MT_DN_DRBUF,	OTHER, plural, "4DDN driver buffer" },
	{ MT_DN_BLK,	OTHER, plural, "4DDN block allocator" },
		/* { MT_DN_BD,	"4DDN board-mbuf conversion" }, (OBSOLETE) */
	{ MT_MCAST_RINFO, OTHER, plural, "multicast route info structure" },
	{ MT_SESMGR,	OTHER, plural, "trusted networking session manager structure" },
	{ 0, 0 }
};

int nmbtypes = MT_MAX;
u_int seen[256];	/* "have we seen this type yet?" */

static u_int total_shown, shown[256];

#define	HOWMANY(n, u)	(((n)+(u)-1)/(u))
#define	ROUNDUP(n, u)	(HOWMANY(n, u) * (u))

/*
 * Print mbuf statistics.
 */
void
mbpr(ns_off_t mbaddr)
{
	register int totmem, totfree;
	register int i, err;
	register struct mbtypes *mp;
	int mcbpages, pcbpages;
	int totpages, freepages;

	if (nmbtypes > MT_MAX) {
		fprintf(stderr, "unexpected change to mbstat; check source\n");
		return;
	}
	err = sysmp(MP_SAGET, MPSA_TCPIPSTATS, &kna, sizeof(struct kna));
	if (err < 0) {
		fprintf(stderr, "mbpr: failed sysmp MP_SAGET\n");
		return;
	}
	mbstat = kna.mbstat;

	printf("networking memory allocation:\n");
	printf("%lld of %lld mbuf%s in use\n",
		mbstat.m_mbufs - mbstat.m_mtypes[MT_FREE],
		mbstat.m_mbufs,
		plural(mbstat.m_mbufs));
	for (mp = mbtypes; mp->mt_name; mp++) {
		if (mp->mt_flag == MBUF) {
			if (mbstat.m_mtypes[mp->mt_type]) {
				seen[mp->mt_type] = YES;
				printf("\t%llu %s%s allocated\n",
				       mbstat.m_mtypes[mp->mt_type],
				       mp->mt_name, 
				       mp->mt_func(mbstat.m_mtypes[mp->mt_type]));
			}
			seen[MT_FREE] = YES;
		}
	}
	printf("%lld other structure%s in use\t\t\t\n",
		mbstat.m_pcbtot + mbstat.m_mcbtot,
		plural(mbstat.m_pcbtot + mbstat.m_mcbtot));
	for (mp = mbtypes; mp->mt_name; mp++) {
		if (mp->mt_flag == OTHER) {
			if (mbstat.m_mtypes[mp->mt_type]) {
				seen[mp->mt_type] = YES;
				printf("\t%llu %s%s allocated\n",
				       mbstat.m_mtypes[mp->mt_type],
				       mp->mt_name, 
				       mp->mt_func(mbstat.m_mtypes[mp->mt_type]));
			}
		}
	}
	for (i = 0; i < nmbtypes; i++)
		if (!seen[i] && mbstat.m_mtypes[i]) {
			printf("\t%llu <type %d> allocated\n",
			       mbstat.m_mtypes[i], i);
		}

	totmem = mbstat.m_clusters * CLBYTES;	/* all memory is in clusters */
	mcbpages = ROUNDUP(mbstat.m_mcbbytes, CLBYTES);
	pcbpages = ROUNDUP(mbstat.m_pcbbytes, CLBYTES);

	totmem += pcbpages + mcbpages;

	pcbpages /= MCLBYTES;
	mcbpages /= MCLBYTES;

	totfree = mbstat.m_clfree * CLBYTES;

	printf("%u total page%s allocated to networking data\n",
		totmem / CLBYTES,
		plural(totmem / CLBYTES));

	printf("\t%llu/%llu mapped mbuf page%s in use\n",
	       mbstat.m_clusters - mbstat.m_clfree, mbstat.m_clusters,
	       plural(mbstat.m_clusters));

	printf("\t%u page%s of PCBs in use\n",
	       pcbpages, plural(pcbpages));

	printf("\t%u page%s of other networking data in use\n",
	       mcbpages, plural(mcbpages));

	totpages = pcbpages + mcbpages + mbstat.m_clusters;
	freepages = mbstat.m_clfree;

	printf("\t%u Kbytes allocated to network (%d%% in use)\n",
	       totmem / 1024, (totpages - freepages) * 100 / totpages);
	printf("%llu request%s for mbuf memory denied\n", mbstat.m_drops,
		plural(mbstat.m_drops));
	printf("%llu request%s for mbuf memory delayed\n", mbstat.m_wait,
		plural(mbstat.m_wait));
	printf("%llu call%s to protocol drain routines\n", mbstat.m_drain,
		plural(mbstat.m_drain));
	printf("%llu request%s for non-mbuf memory denied\n", mbstat.m_mcbfail,
		plural(mbstat.m_mcbfail));
}

void
initmb(void)
{
	bzero(&zmbstat, sizeof(zmbstat));
}

void
zeromb(void)
{
	zmbstat = mbstat;
}

void
sprmb(int y, int *y_off_ptr)
{
	int y_off = ck_y_off(6+total_shown, y, y_off_ptr);
	register int i, err;
	register int totmem, totfree;
	register struct mbtypes *mp;
	struct	mbstat tmbstat, *mbsp;
	int mcbpages, pcbpages;
	char buf[30];
	int totpages, freepages;
#define COL0 0

	mbsp = (cmode == DELTA) ? &mbstat : &zmbstat;

	err = sysmp(MP_SAGET, MPSA_TCPIPSTATS, &kna, sizeof(struct kna));
	if (err < 0) {
		fail("sprmb: failed sysmp MP_SAGET\n");
		return;
	}
	tmbstat = kna.mbstat;

	totmem = tmbstat.m_clusters * CLBYTES; /* all memory is in clusters */
	mcbpages = ROUNDUP(tmbstat.m_mcbbytes, CLBYTES);
	pcbpages = ROUNDUP(tmbstat.m_pcbbytes, CLBYTES);
	totmem += pcbpages + mcbpages;

	pcbpages /= MCLBYTES;
	mcbpages /= MCLBYTES;

	move_print(&y, &y_off, 1, COL0,
		   "%10u total page%s in use by network\t\t",
		    totmem / CLBYTES, plural(totmem / CLBYTES));

	move_print(&y, &y_off, 1, COL0,
		   "\t%5u out of %u mapped mbuf pages in use\t\t",
		   tmbstat.m_clusters - tmbstat.m_clfree, tmbstat.m_clusters);

	move_print(&y, &y_off, 1, COL0,
		   "\t%5u page%s of PCBs in use\t\t",
		   pcbpages, plural(pcbpages));

	move_print(&y, &y_off, 1, COL0,
		   "\t%5u page%s of other networking data in use\t\t",
		   mcbpages, plural(mcbpages));

	totfree = tmbstat.m_clfree * CLBYTES;

	totpages = pcbpages + mcbpages + mbstat.m_clusters;
	freepages = mbstat.m_clfree;

	move_print(&y, &y_off, 1, COL0,
		   "%10u Kbytes allocated to network (%d%% in use)\t\t",
		   totmem / 1024, (totpages - freepages) * 100 / totpages);

	move_print(&y, &y_off, 1, COL0,
		   "%10u request%s for mbuf memory denied",
		   tmbstat.m_drops - mbsp->m_drops,
		   plural(tmbstat.m_drops - mbsp->m_drops));

	move_print(&y, &y_off, 1, COL0,
		   "%10u request%s for mbuf memory delayed",
		   tmbstat.m_wait - mbsp->m_wait,
		   plural(tmbstat.m_wait - mbsp->m_wait));

	move_print(&y, &y_off, 1, COL0,
		   "%10u call%s to protocol drain routines",
		   tmbstat.m_drain - mbsp->m_drain,
		   plural(tmbstat.m_drain - mbsp->m_drain));

	move_print(&y, &y_off, 1, COL0,
		   "%10u request%s for non-mbuf memory denied",
		   tmbstat.m_mcbfail - mbsp->m_mcbfail,
		   plural(tmbstat.m_mcbfail - mbsp->m_mcbfail));


	move_print(&y, &y_off, 1, COL0,
		   "%7u out of %u mbuf%s in use\t",
		   tmbstat.m_mbufs - tmbstat.m_mtypes[MT_FREE],
		   tmbstat.m_mbufs,
		   plural(tmbstat.m_mbufs));

	bzero(seen, sizeof(seen));
	for (mp = mbtypes;  mp->mt_name; mp++) {
		if (mp->mt_flag == MBUF) {
			seen[mp->mt_type] = YES;
			if (tmbstat.m_mtypes[mp->mt_type] == 0
			    && !shown[mp->mt_type])
				continue;
			move_print(&y, &y_off, 1, COL0,
				   "%16u %s%s allocated\t\t\t\t",
				   tmbstat.m_mtypes[mp->mt_type],
				   mp->mt_name,
				   mp->mt_func(tmbstat.m_mtypes[mp->mt_type]));
			if (!shown[mp->mt_type]) {
				shown[mp->mt_type] = YES;
				total_shown++;
			}
		}
	}
	move_print(&y, &y_off, 1, COL0,
		   "%7u other structure%s in use\t\t\t",
		   tmbstat.m_pcbtot + tmbstat.m_mcbtot,
		   plural(tmbstat.m_pcbtot + tmbstat.m_mcbtot));

	for (mp = mbtypes;  mp->mt_name; mp++) {
		if (mp->mt_flag == OTHER) {
			seen[mp->mt_type] = YES;
			if (tmbstat.m_mtypes[mp->mt_type] == 0
			    && !shown[mp->mt_type])
				continue;
			move_print(&y, &y_off, 1, COL0,
				   "%16u %s%s allocated\t\t\t\t",
				   tmbstat.m_mtypes[mp->mt_type],
				   mp->mt_name,
				   mp->mt_func(tmbstat.m_mtypes[mp->mt_type]));
			if (!shown[mp->mt_type]) {
				shown[mp->mt_type] = YES;
				total_shown++;
			}
		}
	}
	seen[MT_FREE] = YES;

	for (i = 0; i < nmbtypes; i++) {
		if (!seen[i] && (tmbstat.m_mtypes[i]
				 || shown[mp->mt_type])) {
			move_print(&y, &y_off, 1, COL0,
				   "%16u allocated to type %d\t\t",
				   tmbstat.m_mtypes[i], i);
			if (!shown[mp->mt_type]) {
				shown[mp->mt_type] = YES;
				total_shown++;
			}
		}
	}
	mbstat = tmbstat;
}
