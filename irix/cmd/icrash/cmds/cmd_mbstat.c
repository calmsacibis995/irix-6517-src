#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_mbstat.c,v 1.10 1999/05/25 19:21:38 tjm Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mbuf.h>
#include <sys/tcpipstats.h>
#include <klib/klib.h>
#include "icrash.h"

static struct mbtypes {
	int mt_type;
	char    *mt_name;
} mbtypes[] = {
	{ MT_DATA,  "data" },
	{ MT_HEADER,    "packet headers" },
	{ MT_SOCKET,    "socket structures" },
	{ MT_PCB,   	"protocol control blocks" },
	{ MT_RTABLE,    "routing table entries" },
	{ MT_HTABLE,    "IMP host table entries" },
	{ MT_ATABLE,    "address resolution tables" },
	{ MT_FTABLE,    "fragment reassembly queue headers" },
	{ MT_SONAME,    "socket names and addresses" },
	{ MT_SOOPTS,    "socket options" },
	{ MT_RIGHTS,    "access rights" },
	{ MT_IFADDR,    "interface addresses" },
	{ MT_DN_DRBUF,  "4DDN driver buffers" },
	{ MT_DN_BLK,    "4DDN block allocator" },
	{ MT_DN_BD, 	"4DDN board-mbuf conversions" },
	{ MT_IPMOPTS,   "internet multicast options" },
	{ MT_MRTABLE,   "multicast routing structures" },
	{ MT_SAT,   	"security audit trail buffers" },
	{ 0, 0 }
};

/*
 * mbstat_cmd()
 *
 *   Use the 'netstat -m' output to print out the list of mbuf
 *   statistics. Some code comes directly from the
 *   irix/cmd/bsd/netstat/mbuf.c module.
 */
int
mbstat_cmd(command_t cmd)
{
	int i, totmbufs, totmem, totfree, nmbtypes;
	FILE *ofp;
	struct mbstat *mbs;
	register struct mbtypes *mp;
	int seen[256];

	mbs = (struct mbstat*)kl_alloc_block(sizeof (struct mbstat), K_TEMP);

	if (get_mbstat(mbs)) {
		fprintf(cmd.ofp, "No mbuf statistics available!\n");
		kl_print_error();
	} 
	else {
		nmbtypes = sizeof(mbs->m_mtypes) / sizeof(mbs->m_mtypes[0]);
		fprintf(cmd.ofp,
			"%d/%d mbufs in use:\n",
			mbs->m_mbufs - mbs->m_mtypes[MT_FREE], mbs->m_mbufs);
		totmbufs = 0;
		for (mp = mbtypes; mp->mt_name; mp++) {
			if (mbs->m_mtypes[mp->mt_type]) {
				seen[mp->mt_type] = TRUE;
				fprintf(cmd.ofp, "\t%u mbufs allocated to %s\n",
				mbs->m_mtypes[mp->mt_type], mp->mt_name);
				totmbufs += mbs->m_mtypes[mp->mt_type];
			}
		}

		seen[MT_FREE] = TRUE;
		for (i = 0; i < nmbtypes; i++) {
			if (!seen[i] && mbs->m_mtypes[i]) {
				fprintf(cmd.ofp, "\t%u mbufs allocated to <mbuf type %d>\n",
					mbs->m_mtypes[i], i);
				totmbufs += mbs->m_mtypes[i];
			}
		}

		if (totmbufs != mbs->m_mbufs - mbs->m_mtypes[MT_FREE]) {
			fprintf(cmd.ofp, "*** %d mbufs missing ***\n",
				(mbs->m_mbufs - mbs->m_mtypes[MT_FREE]) - totmbufs);
		}

		fprintf(cmd.ofp, "%u/%u mapped pages in use\n",
			mbs->m_clusters - mbs->m_clfree, mbs->m_clusters);

		totmem = mbs->m_clusters * pagesz;
		totfree = mbs->m_mtypes[MT_FREE] * mbufconst.m_msize;

		fprintf(cmd.ofp, "%u Kbytes allocated to network (%d%% in use)\n",
			totmem / 1024, (totmem - totfree) * 100 / totmem);
		fprintf(cmd.ofp, "%u requests for memory denied\n", mbs->m_drops);
		fprintf(cmd.ofp, "%u requests for memory delayed\n", mbs->m_wait);
		fprintf(cmd.ofp, "%u calls to protocol drain routines\n", mbs->m_drain);
	}
	return(0);
}

#define _MBSTAT_USAGE "[-w outfile]"

/*
 * mbstat_usage() -- Print the usage string for the 'mbstat' command.
 */
void
mbstat_usage(command_t cmd)
{
	CMD_USAGE(cmd, _MBSTAT_USAGE);
}

/*
 * mbstat_help() -- Print the help information for the 'mbstat' command.
 */
void
mbstat_help(command_t cmd)
{
	CMD_HELP(cmd, _MBSTAT_USAGE,
		"Dump out the mbuf statistics in the corefile.");
}

/*
 * mbstat_parse() -- Parse the command line arguments for 'mbstat'.
 */
int
mbstat_parse(command_t cmd)
{
	return (C_WRITE|C_FALSE);
}
