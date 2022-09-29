#ident "$Revision: 1.3 $"
#include "stdio.h"
#include "unistd.h"
#include "stdlib.h"
#include "tlbstats.h"

/*
 * IRIX 4.0 handler
 *
 * XXX reset wired entries every once in a while (tlbdrop)
 */

#define NPGPT	512	/* # pages per page table (8 bytes/pte) */
/* max # of pages (4Gb / 4096) */
#define MAXPAGES (1024 * 1024)
#define DOUBLECOST	2000	/* XXX???? */
/*
 * UTLB Miss Cost:r3000
 * Basic 10 instructions with 1 lw
 *
 * UTLB Miss Cost:R4000 (NOT IP17 )
 *
 *      mfc0    k0,C0_CTXT		(1) takes  4 to get result
 *      mfc0    k1,C0_EPC               (1)
 *      lw      k1,0(k0)		(1) plus (4) stall
 *					(no additional stalls for 2nd lw)
 *      lw      k0,8(k0)                (1) (w/ cache hit takes 2 stages)
 * #if IP17
 *      sll     k1,k1,TLBLO_HWBITSHIFT  # Mask off the SW bits from the pte
 *      srl     k1,k1,TLBLO_HWBITSHIFT
 * #endif
 *					(4) stall for lw k1
 *      mtc0    k1,C0_TLBLO_0		(1)
 * #if IP17
 *      sll     k0,k0,TLBLO_HWBITSHIFT  # Mask off the SW bits from the pte
 *      srl     k0,k0,TLBLO_HWBITSHIFT
 * #endif
 *      mtc0    k0,C0_TLBLO_1		(1) (no addional stall)
 *      NOP_1_1                         (1)
 *      c0      C0_WRITER		(1)
 *      eret				(4) till pipe starts filling
 * Total:  20 divide by 2 to get external: 10
 * From newt - it takes 24 internal cycles: 12
 * BUT, the icache is so small if both lines miss the 1st level thats
 * an additional 11 internal clocks (5.5 external) (per line)
 * So we'll take the worst case: 11*2 + 24: 46 external or 23 internal.
 */

/*
 * 1.5 Level Cost:R3000
 *	UTLB cost:5
 *	Exception prologue(IP7):16 cycles to kmiss
 *	Kmiss:60! (each load == 1 cycle)
 *		loads - private.curproc
 *			p->p_segtype
 *			private.curproc	(cached)
 *			p->p_segtbl (part of p_segtype line so cached)
 *			page table addr
 *			load page table entry
 *			private.k1save	(cached)
 *			private.cputype (part of curproc cache line so cached)
 *			private.triggersave	(cached)
 *			private.atsave	(cached)
 * Assume that all loads hit 2nd level (none 1st level)
 * Total 5 + 16 + 60  = 81 (w/o cache miss penalty)
 *
 */
#define ONEFIVECOST	81	/* cost of 1.5 level handler (w/o cache) */

/*
 * TLB module for a utlb miss handler based on Irix 4.0
 * This uses a set of wired entries to hold pointers to pages of page
 * tables.
 * When more than the number of wired entry segments are required
 * it changes to a 2 level scheme.
 */
typedef struct tlb_s {
	vaddr t_vpn;
	int t_valid;
} tlb_t;

/*
 * Defaults for irix4.0 w/ R3000
 * The default min and max penalty are a combination of all of our
 * systems - see tlbstats.1
 */
args_t irix40r3kparms[] = {
	{ "entries", OPT_ENTRIES, "56" },
	{ "wired", OPT_WIRED, "7" },
	{ "cachehit", OPT_CACHEHIT, "97" },
	{ "mincachepenalty", OPT_MINCACHEPENALTY, "24" },
	{ "maxcachepenalty", OPT_MAXCACHEPENALTY, "40" },
	{ "pagesize", OPT_PAGESIZE, "4096" },
	{ "maps2", OPT_MAPS2, "FALSE" },
	{ "v", OPT_VERBOSE, "FALSE" },
	{ "1stpenalty", OPT_FIRSTPENALTY, "10" },
	{ "1.5penalty", OPT_ONEFIVEPENALTY, "0" },
	{ "2ndpenalty", OPT_SECONDPENALTY, "0" },
	{ 0 }
};

/* Defaults for irix4.0 w/ R4000 */
args_t irix40r4kparms[] = {
	{ "entries", OPT_ENTRIES, "40" },
	{ "wired", OPT_WIRED, "7" },
	{ "cachehit", OPT_CACHEHIT, "10" },
	{ "mincachepenalty", OPT_MINCACHEPENALTY, "12" },
	{ "maxcachepenalty", OPT_MAXCACHEPENALTY, "50" },
	{ "pagesize", OPT_PAGESIZE, "4096" },
	{ "maps2", OPT_MAPS2, "TRUE" },
	{ "v", OPT_VERBOSE, "FALSE" },
	{ "1stpenalty", OPT_FIRSTPENALTY, "23" },
	{ "1.5penalty", OPT_ONEFIVEPENALTY, "0" },
	{ "2ndpenalty", OPT_SECONDPENALTY, "0" },
	{ 0 }
};

static tlb_t *tlb;
static signed char *tlbp;
static int wiredhand;

/*
 * Macro to change a vpn to a virtual address to use in the tlb
 * It must be distinct from all user virtual addresses.
 * add 1 incase MAPS2 is set
 */
#define VPNTOSEGVADDR(vpn, tp) \
	(0x80000000 | (((vpn) / NPGPT) << ((tp)->t_pageshift+1)))

static int probepg(tlbparms_t *, vaddr);
static void insert(tlbparms_t *, vaddr, int, int);

void
irix40_init(tlbparms_t *tp)
{
	register int i;

	tlb = calloc(tp->t_nentries + tp->t_nwired, sizeof(tlb_t));
	for (i = 0; i < tp->t_nentries + tp->t_nwired; i++)
		tlb[i].t_vpn = 0xffffffff;
	/* for fast lookups, have a byte per possible page! */
	tlbp = calloc(MAXPAGES, 1);
	for (i = 0; i < MAXPAGES; i++)
		tlbp[i] = -1;
	ZERO(tp->t_fills);
	ZERO(tp->t_memmisses);
	ZERO(tp->t_1stfills);
	ZERO(tp->t_1stcost);
	ZERO(tp->t_2ndfills);
	ZERO(tp->t_2ndcost);
	if (tp->t_2ndpenalty == 0)
		tp->t_2ndpenalty = DOUBLECOST;
	ZERO(tp->t_15fills);
	ZERO(tp->t_15cost);
	if (tp->t_15penalty == 0)
		tp->t_15penalty = ONEFIVECOST + (4 * tp->t_mincachepenalty);
	wiredhand = 0;
}

void
irix40_print(tlbparms_t *tp)
{
	register double mincost, maxcost;
	dw tcost;

	ZERO(tcost);
	ADD(tcost, tp->t_1stcost);
	ADD(tcost, tp->t_15cost);
	ADD(tcost, tp->t_2ndcost);

	mincost = DOUBLE(tcost) +
			(DOUBLE(tp->t_memmisses) * tp->t_mincachepenalty);
	maxcost = DOUBLE(tcost) +
			(DOUBLE(tp->t_memmisses) * tp->t_maxcachepenalty);
	printf("\nTotal Cost (%3d%% pte cache hit) ", tp->t_cachehit);
	printf("min %.7g cycles max %.7g cycles\n", mincost, maxcost);
		
	mincost = DOUBLE(tcost) +
			(DOUBLE(tp->t_1stfills) * tp->t_mincachepenalty);
	maxcost = DOUBLE(tcost) +
			(DOUBLE(tp->t_1stfills) * tp->t_maxcachepenalty);
	printf("Total Cost (  0%% pte cache hit) ", tp->t_cachehit);
	printf("min %.7g cycles max %.7g cycles\n", mincost, maxcost);

	mincost = DOUBLE(tcost) + (0 * tp->t_mincachepenalty);
	maxcost = DOUBLE(tcost) + (0 * tp->t_maxcachepenalty);
	printf("Total Cost (100%% pte cache hit) ", tp->t_cachehit);
	printf("min %.7g cycles max %.7g cycles\n", mincost, maxcost);

	printf(" Memory misses %.7g", DOUBLE(tp->t_memmisses));
	printf(" Memory cost min %.7g max %.7g\n",
		DOUBLE(tp->t_memmisses) * tp->t_mincachepenalty,
		DOUBLE(tp->t_memmisses) * tp->t_maxcachepenalty);

	printf(" 1st Level (utlb) Fills %.7g Cost %.7g cycles (cycles/fill %d)\n",
		DOUBLE(tp->t_1stfills),
		DOUBLE(tp->t_1stcost),
		tp->t_1stpenalty);

	printf(" 1.5 Level Fills %.7g Cost %.7g cycles (cycles/fill %d)\n",
		DOUBLE(tp->t_15fills),
		DOUBLE(tp->t_15cost),
		tp->t_15penalty);

	printf(" 2nd Level Fills %.7g Cost %.7g cycles (cycles/fill %d)\n",
		DOUBLE(tp->t_2ndfills),
		DOUBLE(tp->t_2ndcost),
		tp->t_2ndpenalty);

	printf(" Total entries inserted into TLB %.7g\n", DOUBLE(tp->t_fills));

	printf("\n");
	printf(
" 1.5 Level fills include page table fill(random) and page fill(random)\n");
	printf(
" 2nd Level fills include page table fill(index) and page fill(random)\n");
	printf(
" Assume all 1.5 level loads (4) hit the second level cache but miss first\n");
}

int
irix40_probe(tlbparms_t *tp, vaddr v)
{
	register int i;
	vaddr vpn;

	vpn = v >> tp->t_pageshift;
	if (tp->t_flags & TLB_MAPS2)
		vpn &= ~1;
	if ((i = tlbp[vpn]) > 0) {
#ifdef DEBUG
		if (verbose > 1)
			printf("Matched v 0x%x vpn 0x%x index %d\n",
				v, vpn, i);
#endif
		return(i);
	}
	return(-1);
}

int
irix40_fill(tlbparms_t *tp, vaddr v)
{
	register int i, nentries;
	vaddr vpn;
	int memcost;
	float missrate;
	static int misscntr = 0;

	vpn = v >> tp->t_pageshift;
	if (tp->t_flags & TLB_MAPS2)
		vpn &= ~1;

	/*
	 * see if pagetable in tlb
	 */
	if (probepg(tp, vpn) < 0) {
		/*
		 * Put into wired entries if possible
		 */
		if (wiredhand < tp->t_nwired) {
			/*
			 * 2nd LEVEL (tfault) fill
			 */
			INC1(tp->t_2ndfills);
			insert(tp, VPNTOSEGVADDR(vpn, tp), TLB_INDEX, wiredhand);
			wiredhand++;
			/* insert original page */
			insert(tp, v, TLB_RANDOM, 0);
			INC(tp->t_2ndcost, tp->t_2ndpenalty);
			return(0);
		} else {
			/*
			 * 1.5 LEVEL fill
			 */
			INC1(tp->t_15fills);
			insert(tp, VPNTOSEGVADDR(vpn, tp), TLB_RANDOM, 0);
			/* insert original page */
			insert(tp, v, TLB_RANDOM, 0);
			INC(tp->t_15cost, tp->t_15penalty);
			return(0);
		}
	}

	/*
	 * UTLB fill
	 */
	insert(tp, v, TLB_RANDOM, 0);
	INC1(tp->t_1stfills);

	/*
	 * compute cost 
	 */
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

static void
insert(tlbparms_t *tp, vaddr v, int flags, int index)
{
	register int i, nentries;
	vaddr vpn, ovpn;

	assert(flags);
	vpn = v >> tp->t_pageshift;
	if (tp->t_flags & TLB_MAPS2)
		vpn &= ~1;

	nentries = tp->t_nentries + tp->t_nwired;
	if (flags & TLB_INDEX) {
		assert(index < nentries);
		i = index;
	} else if (flags & TLB_RANDOM) {
		do {
			i = rand() % nentries;
		} while (i < tp->t_nwired);
	}
	if ((ovpn = tlb[i].t_vpn) != 0xffffffff) {
		assert(tlbp[ovpn] == i);
		tlbp[ovpn] = -1;
	}
	if (verbose)
		printf("Inserting v 0x%x vpn 0x%x at index %d\n", v, vpn, i);
	tlb[i].t_vpn = vpn;
	tlbp[vpn] = i;
	INC1(tp->t_fills);
}

/*
 * see if page table for this page is in tlb
 */
static int
probepg(tlbparms_t *tp, vaddr vpn)
{
	return(irix40_probe(tp, VPNTOSEGVADDR(vpn, tp)));
}
