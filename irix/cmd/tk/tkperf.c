#include "tkm.h"
#include "unistd.h"
#include "memory.h"
#include "stdlib.h"
#include "stdio.h"
#include "strings.h"
#include "getopt.h"
#include "ulocks.h"
#include "mutex.h"
#include "time.h"
#include "signal.h"
#include "sys/times.h"

static void client_obtain(void *, tk_set_t, tk_set_t, tk_disp_t, tk_set_t *);
static void client_return(tkc_state_t, void *, tk_set_t, tk_set_t, tk_disp_t);
tkc_ifstate_t iface = {
	client_obtain,
	client_return
};
static void server_recall(void *o, tks_ch_t h, tk_set_t r, tk_disp_t);
static void server_recalled(void *, tk_set_t, tk_set_t);
tks_ifstate_t svriface = {
	server_recall,
	server_recalled,
	NULL
};
#define MYNODE 0
TKC_DECL(cs, TK_MAXTOKENS);
TKS_DECL(ss, TK_MAXTOKENS);


int
main(int argc, char **argv)
{
	int c, i, j;
	int maxtokens = 10;
	struct tms tm;
	clock_t start, ti;
	tk_set_t ts, tsall, gotten;
	tk_singleton_t t1;
	int nloops = 100000;
	double tif;

	while ((c = getopt(argc, argv, "t:")) != EOF) 
		switch (c) {
		case 't':
			maxtokens = atoi(optarg);
			if (maxtokens > TK_MAXTOKENS)
				maxtokens = TK_MAXTOKENS;
			break;
		}

	tkc_init();
	tks_init();

	/* create client */
	tkc_create("client", cs, ss, &iface, maxtokens, TK_NULLSET, NULL);
	tks_create("server", ss, cs, &svriface, maxtokens, NULL);

	tsall = TK_NULLSET;
	for (i = 0; i < maxtokens; i++)
		TK_COMBINE_SET(tsall, TK_MAKE(i, TK_WRITE));

	/* prime pump */
	tkc_acquire(cs, tsall, &gotten);
	tkc_release(cs, tsall);

	/* test acquire1/release1 */
	t1 = TK_MAKE_SINGLETON(0, TK_WRITE);
        start = times(&tm);
        for (i = 0; i < nloops; i++) {
                tkc_acquire1(cs, t1);
                tkc_release1(cs, t1);
        }
        ti = times(&tm) - start;
        tif = (ti*1000.0)/(clock_t)CLK_TCK;
        printf("tkperf:time for %d acquire1/release1 %f mS or %f uS each\n",
                nloops, tif, (tif*1000.0)/nloops);

	ts = TK_NULLSET;
	for (j = 0; j < maxtokens; j++) {
		TK_COMBINE_SET(ts, TK_MAKE(j, TK_WRITE));
		start = times(&tm);
		for (i = 0; i < nloops; i++) {
			tkc_acquire(cs, ts, &gotten);
			tkc_release(cs, ts);
		}
		ti = times(&tm) - start;
		tif = (ti*1000.0)/(clock_t)CLK_TCK;
		printf("tkperf:time for %d acquire/release of %d tokens %f mS or %f uS each\n",
			nloops, j+1, tif, (tif*1000.0)/nloops);
	}

	return 0;
}

/* ARGSUSED */
static void
client_obtain(void *o,
	tk_set_t obtain,
	tk_set_t toreturn,
	tk_disp_t why,
	tk_set_t *refused)
{
	tk_set_t got, already;

	if (toreturn != TK_NULLSET)
		tks_return(o, MYNODE, toreturn, TK_NULLSET, TK_NULLSET,
			why);
	tks_obtain(o, MYNODE, obtain, &got, NULL, &already);
	*refused = TK_SUB_SET(obtain, TK_ADD_SET(got, already));
}

/* ARGSUSED */
static void
client_return(tkc_state_t ci, void *o, tk_set_t revoke, tk_set_t eh, tk_disp_t which)
{
	tks_return(o, MYNODE, revoke, TK_NULLSET, eh, which);
	tkc_returned(cs, revoke, TK_NULLSET);
}

/* ARGSUSED */
static void
server_recall(void *o, tks_ch_t h, tk_set_t r, tk_disp_t which)
{
	tkc_recall(o, r, which);
}

/* ARGSUSED */
static void
server_recalled(void *o, tk_set_t r, tk_set_t refused)
{
	abort();
}

/*
 * get client handle from within token module - used for TRACING and
 * local client ONLY.
 */
/* ARGSUSED */
tks_ch_t
getch_t(void *o)
{
	return 0;
	/* NOTREACHED */
}
