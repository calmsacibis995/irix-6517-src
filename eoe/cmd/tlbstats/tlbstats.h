#ident "$Revision: 1.3 $"
#include "assert.h"
#include "dw.h"

/* command args */
#define OPT_ALGORITHM		1
#define OPT_ENTRIES		2
#define OPT_WIRED		3
#define OPT_CACHEHIT		4
#define OPT_MINCACHEPENALTY 	5
#define OPT_PAGESIZE		6
#define OPT_MAPS2		7
#define OPT_VERBOSE		8
#define OPT_FIRSTPENALTY	9
#define OPT_ONEFIVEPENALTY	10
#define OPT_SECONDPENALTY	11
#define OPT_MAXCACHEPENALTY 	12
typedef struct {
	char *name;
	short id;
	char *defvalue;
} args_t;

typedef unsigned vaddr;

/*
 * Set of paramters governing simulation
 */
typedef struct tlbparms_s {
	int t_nentries;		/* # entries */
	int t_nwired;		/* # wired entries */
	int t_flags;		/* various boolean options */
	int t_pagesize;		/* each tlb maps pagesize bytes */
	int t_pageshift;	/* shift to get vpn */
	int t_cachehit;		/* percentage hit rate for page table
				 * entry fetches
				 */
	int t_mincachepenalty;	/* min # cycles penalty on a miss */
	int t_maxcachepenalty;	/* max # cycles penalty on a miss */
	dw t_fills;		/* # of fills */
	dw t_memmisses;	/* total # memory misses */
	dw t_1stfills;	/* # pages entered in utlb */
	dw t_1stcost;
	int t_1stpenalty;	/* # cycles per utlb fill */

	/* used in irix4.0 algorithm */
	dw t_15fills;	/* # 1.5 level fills */
	dw t_15cost;
	int t_15penalty;	/* # cycles per 1.5 fill */
	dw t_2ndfills;	/* # page tables entered */
	dw t_2ndcost;
	int t_2ndpenalty;	/* # cycles per 2nd fill */
} tlbparms_t;

/* definitions of t_flags */
#define TLB_MAPS2	0x0001	/* entry maps 2 contiguous virtual pages */

typedef struct tlbmodule_s {
	char *t_name;
	void (*t_tlbinit)(tlbparms_t *);
	int (*t_tlbprobe)(tlbparms_t *, vaddr);
	int (*t_tlbfill)(tlbparms_t *, vaddr);
	void (*t_tlbprint)(tlbparms_t *);
	args_t *t_parms;
} tlbmodule_t;

/* flag values passed to tlbinsert */
#define TLB_RANDOM	0x0001	/* insert random */
#define TLB_INDEX	0x0002	/* insert at index */
extern int verbose;

/*
 * Modules
 */
void utlbonly_init(tlbparms_t *);
void utlbonly_print(tlbparms_t *);
int utlbonly_probe(tlbparms_t *, vaddr);
int utlbonly_fill(tlbparms_t *, vaddr);
extern args_t utlbonlyparms[];

void irix40_init(tlbparms_t *);
void irix40_print(tlbparms_t *);
int irix40_probe(tlbparms_t *, vaddr);
int irix40_fill(tlbparms_t *, vaddr);
extern args_t irix40r3kparms[];
extern args_t irix40r4kparms[];
