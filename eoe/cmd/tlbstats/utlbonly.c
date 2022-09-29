#ident "$Revision: 1.3 $"
#include "stdio.h"
#include "unistd.h"
#include "stdlib.h"
#include "tlbstats.h"

/*
 * TLB module for a utlb miss handler that assumes
 * that all misses can be filled using the std R3000/IRIX4.0
 * utlbmiss handler
 */
#define FIRSTCOST	10
typedef struct tlb_s {
	vaddr t_vpn;
	int t_valid;
} tlb_t;
args_t utlbonlyparms[] = {
	{ "entries", OPT_ENTRIES, "56" },
	{ "wired", OPT_WIRED, "7" },
	{ "cachehit", OPT_CACHEHIT, "97" },
	{ "mincachepenalty", OPT_MINCACHEPENALTY, "24" },
	{ "maxcachepenalty", OPT_MAXCACHEPENALTY, "40" },
	{ "1stpenalty", OPT_FIRSTPENALTY, "10" },
	{ "pagesize", OPT_PAGESIZE, "4096" },
	{ "maps2", OPT_MAPS2, "FALSE" },
	{ "v", OPT_VERBOSE, "FALSE" },
	{ 0 }
};

static tlb_t *tlb;
static dw nrefs;

void
utlbonly_init(tlbparms_t *tp)
{
	tlb = calloc(tp->t_nentries + tp->t_nwired, sizeof(tlb_t));
	ZERO(tp->t_fills);
	ZERO(tp->t_memmisses);
	ZERO(tp->t_1stfills);
	ZERO(tp->t_1stcost);
	if (tp->t_1stpenalty == 0)
		tp->t_1stpenalty = FIRSTCOST;
	ZERO(nrefs);
}

void
utlbonly_print(tlbparms_t *tp)
{
	double mincost, maxcost;

	mincost = DOUBLE(tp->t_1stcost) +
			(DOUBLE(tp->t_memmisses) * tp->t_mincachepenalty);
	maxcost = DOUBLE(tp->t_1stcost) +
			(DOUBLE(tp->t_memmisses) * tp->t_maxcachepenalty);
	printf(" Total refs %.7g", DOUBLE(nrefs));
	printf(" Total fills %.7g", DOUBLE(tp->t_fills));
	printf(" Total cost min %.7g cycles max %.7g cycles\n",
		mincost, maxcost);
	printf(" Memory misses %.7g\n", DOUBLE(tp->t_memmisses));
}


int
utlbonly_probe(tlbparms_t *tp, vaddr v)
{
	register int i, nentries;
	vaddr vpn;

	INC1(nrefs);
	vpn = v >> tp->t_pageshift;
	if (tp->t_flags & TLB_MAPS2)
		vpn &= ~1;
	nentries = tp->t_nentries + tp->t_nwired;
	for (i = 0; i < nentries; i++) {
		if (tlb[i].t_vpn == vpn && tlb[i].t_valid)
			return(i);
	}
	return(-1);
}

int
utlbonly_fill(tlbparms_t *tp, vaddr v)
{
	register int i, nentries;
	vaddr vpn;
	int memcost;
	float missrate;
	static int misscntr = 0;

	vpn = v >> tp->t_pageshift;
	if (tp->t_flags & TLB_MAPS2)
		vpn &= ~1;
	nentries = tp->t_nentries + tp->t_nwired;
	do {
		i = rand() % nentries;
	} while (i < tp->t_nwired);
	if (verbose)
		printf("Inserting v 0x%x vpn 0x%x at index %d\n", v, vpn, i);
	tlb[i].t_vpn = vpn;
	tlb[i].t_valid = 1;
	INC1(tp->t_fills);

	/*
	 * compute cost 
	 * Basic 10 instructions with 1 lw
	 */
	memcost = 0;
	if (tp->t_cachehit != 100) {
		missrate = 100.0 / (float)(100 - tp->t_cachehit);
		if ((float) ++misscntr >= missrate) {
			misscntr = 0;
			INC1(tp->t_memmisses);
		}
	}
	INC(tp->t_1stcost, tp->t_1stpenalty);
	return(0);
}
