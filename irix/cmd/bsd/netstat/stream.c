/*
 * Print streams memory statistics
 */
#ident "$Revision: 1.19 $"

#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stream.h>
#include <sys/strstat.h>
#include <sys/var.h>
#include <sys/sysmp.h>

#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <curses.h>
#include <string.h>
#include <bstring.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>

#include "netstat.h"
#include "cdisplay.h"

/*
 * NOTICE: Following header definitions were STOLEN from kernel <sys/strsubr.h>
 * Simply trying to include strstubr.h causes lots of error due to having
 * to suck in lots of other kernel internal related header files. Too much
 * confusion and LOTS of work for one simple structure definition.
 *
 * Structure to keep track of resources that have been allocated
 * for streams - an array of these are kept, one entry per
 * resource.  This is used by crash to dump the data structures.
 */
struct strinfo {
	void	*sd_head;	/* head of in-use list */
	int	sd_cnt;		/* total # allocated */
};

#define DYN_STREAM	0	/* for stream heads */
#define DYN_QUEUE	1	/* for queues */
#define DYN_MSGBLOCK	2	/* for message blocks */
#define DYN_LINKBLK	3	/* for mux links */
#define DYN_STREVENT	4	/* for stream event cells */
#define DYN_QBAND	5	/* for qband structures */
#define NDYNAMIC	6	/* # data types dynamically allocated */

/*
 * NOTICE: Following definitions were STOLEN from kernel <io/streams/stream.c>
 * Simply trying to more them into strstubr.h causes lots of error due to
 * having to suck in lots of other kernel internal related header files.
 * Too much confusion and LOTS of work for one simple set of defines.
 */
#define STR_MSG_INDEX		0 /* msgb structures */
#define STR_MD_INDEX		1
#define STR_BUF64_INDEX		2
#define STR_BUF256_INDEX	3
#define STR_BUF512_INDEX	4
#define STR_BUF2K_INDEX		5
#define STR_PAGE_INDEX		6
#define STR_MAX_INDEX		7

static unsigned int nstream, nqueue, nlinkblk, nseevent, nqbinfo;
static unsigned int nmblock, ndblock;
static unsigned int nbuf64, nbuf256, nbuf512, nbuf2048, nbufpage;
static int str_page_curX, str_min_pagesX, str_max_pagesX;
static struct strstat cstrst, zstrst;
static struct strinfo streaminfo[NDYNAMIC];

static struct label {
	char	*txt;
	alcdat	*dat;
	alcdat	*zero;
	unsigned int *v;
} labels[] = {
	{"streams",	&cstrst.stream,  &zstrst.stream, &nstream},
	{"queues",	&cstrst.queue,   &zstrst.queue, &nqueue},
	{"msg blks",	&cstrst.buffer[STR_MSG_INDEX],
		&zstrst.buffer[STR_MSG_INDEX], &nmblock},
	{"msg/dblks",	&cstrst.buffer[STR_MD_INDEX],
		&zstrst.buffer[STR_MD_INDEX], &ndblock},
	{"bufsz64",	&cstrst.buffer[STR_BUF64_INDEX],
		&zstrst.buffer[STR_BUF64_INDEX], &nbuf64},
	{"bufsz256",	&cstrst.buffer[STR_BUF256_INDEX],
		&zstrst.buffer[STR_BUF256_INDEX], &nbuf256},
	{"bufsz512",	&cstrst.buffer[STR_BUF512_INDEX],
		&zstrst.buffer[STR_BUF512_INDEX], &nbuf512},
	{"bufsz2048",	&cstrst.buffer[STR_BUF2K_INDEX],
		&zstrst.buffer[STR_BUF2K_INDEX], &nbuf2048},
	{"bufszPAGE",	&cstrst.buffer[STR_PAGE_INDEX],
		&zstrst.buffer[STR_PAGE_INDEX], &nbufpage},
	{"link blks",	&cstrst.linkblk, &zstrst.linkblk, &nlinkblk},
	{"events",	&cstrst.strevent,&zstrst.strevent, &nseevent},
	{"qbinfo",	&cstrst.qbinfo, &zstrst.qbinfo, &nqbinfo},
};

#define NUMCTRS (sizeof(labels)/sizeof(labels[0]))

extern void get_strpagesct(int *, int *, int *);

/*
 * Compute the number of available stream related data structures.
 * This must be computed as this value is dynamic.
 * NOTE: The absolute value must be returned as in DELTA mode the
 * values could become negative since the 'cstrst' structure is replaced
 * by the beginning values at the start of the timing interval.
 */
void
avail_strcts()
{
	nstream = abs(streaminfo[DYN_STREAM].sd_cnt - cstrst.stream.use);
	nseevent = abs(streaminfo[DYN_STREVENT].sd_cnt - cstrst.strevent.use);
	nqueue = abs(streaminfo[DYN_QUEUE].sd_cnt - cstrst.queue.use);
	nlinkblk = abs(streaminfo[DYN_LINKBLK].sd_cnt - cstrst.linkblk.use);
	ndblock = nmblock = nqbinfo = 0;
	nbuf64 = nbuf256 = nbuf512 = nbuf2048 = nbufpage = 0;
}

void
streampr(ns_off_t strinfo)
{
	int i;
	alcdat *sp;
	struct label *lp;

	i = sysmp(MP_SAGET, MPSA_STREAMSTATS, &cstrst, sizeof(struct strstat));
	if (i < 0) {
	   (void)fail("streampr: failed sysmp MPSA_STREAMSTATS; errno %d\n",
		   errno);
	}
	avail_strcts();
	get_strpagesct(&str_page_curX, &str_min_pagesX, &str_max_pagesX);

	printf("\n%-11s\n", "Streams Memory Utilization");
	printf("\tCur page ct %5d\n", str_page_curX);
	printf("\tMin page ct %5d\n", str_min_pagesX);
	printf("\tMax page ct %5d\n\n", str_max_pagesX);

	printf("\n%-11s %8s %8s %8s\n",
		"Resource", "   InUse", " MaxUsed", "Failures");
	for (i = 0, lp = &labels[0]; i < NUMCTRS; i++,lp++) {
		sp = lp->dat;
		printf("%-11s %8d %8d %8d\n",
		       lp->txt, sp->use, sp->max, sp->fail);
	}
}

void
initstream(void)
{
	bzero((char*)&zstrst, sizeof(zstrst));
}

void
zerostream()
{
	zstrst = cstrst;
}

void
sprstrm(int y, int *y_off_ptr, ns_off_t strinfo)
{
	int y_off = ck_y_off(7, y, y_off_ptr);
	alcdat *sp;
	struct label *lp;
	int i;

	i = sysmp(MP_SAGET, MPSA_STREAMSTATS, &cstrst, sizeof(struct strstat));
	if (i < 0) {
		(void)fail(
		 "sprstrm: failed sysmp MPSA_STREAMSTATS; errno %d\n", errno);
	}
	avail_strcts();
	get_strpagesct(&str_page_curX, &str_min_pagesX, &str_max_pagesX);

	move_print(&y, &y_off, 1, 0,
		   Cfirst ? "%-11s" : "",
		   "Streams Memory Utilization");
	move_print(&y, &y_off, 1, 0,
		"\tCurrent page count %8d", str_page_curX);
	move_print(&y, &y_off, 1, 0,
		"\tMinimum page count %8d", str_min_pagesX);
	move_print(&y, &y_off, 2, 0,
		"\tMaximum page count %8d", str_max_pagesX);

	move_print(&y, &y_off, 1, 0,
		   Cfirst ? "%-11s %8s %8s %8s" : "",
		   "Resource", "   InUse", " MaxUsed", "Failures");
	for (i = 0, lp = &labels[0]; i < NUMCTRS; i++, lp++) {
		sp = lp->dat;
		move_print(&y, &y_off, 1, 0, "%-11s %8d %8d %8d",
		 lp->txt, sp->use, sp->max, (sp->fail - lp->zero->fail));
	}
	if (cmode == DELTA)
		zstrst = cstrst;
}
