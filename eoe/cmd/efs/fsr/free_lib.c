#ident "$Revision: 1.7 $"

/*
 * routines for processing information about file system free
 * space.
 */

#include "fsr.h"

/*
 * The frag structure describes the fragmented file system free space.
 * Every frag records the number of free blocks and the number of
 * different extents in a frag.  This information is used to locate the
 * most cost-effective free areas to de-frag.
 *
 * frag, last_frag, frag_size, and frag_cnt are used to build up a list
 * of all frags in a file system.
 */
struct frag {
	efs_daddr_t fr_start;
	efs_daddr_t fr_nfree;
	efs_daddr_t fr_nalloc;
};

static struct frag *frag_tbl, *last_frag;

static int frag_size;
static int frag_cnt;

extern int lbsize;
extern int max_ext_size;
static int free_frag_size;

static void addfree(efs_daddr_t start, efs_daddr_t length);
static int find_frag(efs_daddr_t bn, int cnt);
static int frag_cmp(const void *fr1, const void *fr2);
static int frag_cmp_bn(const void *fr1, const void *fr2);
static void increase_frag_list(void);

#define ASSERT(x)

/*
 * addfree adds a free extent to the frag table.  Frags with only one
 * free extent aren't considered worth defragmenting and are thrown away
 * if the list gets large.  Free extents larger than the frag size are
 * ignored.
 *
 * addfree assumes that free extents are handed to it in ascending
 * order by block number.
 */
static void
addfree(efs_daddr_t start, efs_daddr_t length)
{
	static int local_frag_cnt;

	ASSERT(!(last_frag && last_frag->fr_start > start));

	if (last_frag && start < last_frag->fr_start + free_frag_size) {
		local_frag_cnt++;
		last_frag->fr_nfree += length;
		if (last_frag->fr_nfree > free_frag_size)
			last_frag->fr_nfree =  free_frag_size;
		return;
	}
	if (length >= free_frag_size)
		return;

	/* we aren't interested in defragmenting when there's only one
	 * free extent in a base extent.  If local_frag_cnt is one,
	 * backtrack frag_cnt so that we only get new stuff.
	 */
	/* Note: We might want to get more aggressive if we don't
	 * have much work to do, so this might be the wrong time to do
	 * this.  Doing it later, however, requires storing more data on
	 * each segment.
	 */
	if (local_frag_cnt == 1 && frag_cnt > 1000)
		frag_cnt--;
	if (frag_cnt >= frag_size)
		increase_frag_list();
	local_frag_cnt = 1;
	last_frag = &frag_tbl[frag_cnt++];
	last_frag->fr_start = start;
	last_frag->fr_nalloc = 0;
	last_frag->fr_nfree = length;
}

#define NFREE_CHNK 1000

/*
 * increase_frag_list -- the frag list grows to hold as many chunks as
 * necessary
 */
static void
increase_frag_list(void)
{
	frag_tbl = realloc(frag_tbl,
				(frag_size + NFREE_CHNK) * sizeof(*frag_tbl));
	if (!frag_tbl) {
		perror("couldn't malloc space for frag table");
		exit(1);
	}
	frag_size += NFREE_CHNK;
}

/*
 * frag_cmp -- a sort routine that orders frags by the expected gain
 * from removing all allocated extents, adjusted by the
 * cost of moving the extents (nfree/nalloc).
 */
static int
frag_cmp(const void *fr1, const void *fr2)
{
#define FR1 ((struct frag *)fr1)
#define FR2 ((struct frag *)fr2)
	long pr1, pr2;

	pr1 = (long)(FR2->fr_nalloc * FR1->fr_nfree);
	pr2 = (long)(FR1->fr_nalloc * FR2->fr_nfree);

	return pr2 - pr1;

#undef FR1
#undef FR2
}

/*
 * frag_cmp_bn -- sort the frags by block number
 */
static int
frag_cmp_bn(const void *fr1, const void *fr2)
{
	return ((struct frag *)fr1)->fr_start - ((struct frag *)fr2)->fr_start;
}

#ifdef notdef
/*
 * dump_frag_list -- an unused debugging routine
 */
void
dump_frag_list(void)
{
	int i;

	for (i = 0; i < frag_cnt; i++)
		fsrprintf("%d %d %d\n",
			frag_tbl[i].fr_nfree, frag_tbl[i].fr_nalloc,
			frag_tbl[i].fr_start);
}
#endif

/*
 * get_frags -- a routine that scans the entire file system to determine
 * where free space can be reclaimed.  As a side-effect, it fills the
 * cache of free extents to reduce search time during reorganization.
 */
void
get_frags(dev_t dev, int ncg, efs_daddr_t firstbn,
		 efs_daddr_t cgfsize, efs_daddr_t cgisize)
{
	int cg;
	efs_daddr_t start;
	efs_daddr_t end = firstbn;

	if (free_frag_size == 0)
		free_frag_size = max_ext_size;
	fe_clear();
	for (cg = 0; cg < ncg; cg++) {
		start = end + cgisize;
		end += cgfsize;
		while (start < end) {
			efs_daddr_t length = fsc_tstfree(dev, start);
			if (length > 0) {
				addfree(start, length);
				fe_insert(start, length);
				start += length;
			}
			length = fsc_tstalloc(dev, start);
			if (length > 0)
				start += length;
		}
	}
	fe_init_cnts();
}

/*
 * sort frags by their value in reclaiming free space.
 */
void
sort_frags_by_value(void)
{
	if (frag_tbl)
		qsort(frag_tbl, frag_cnt, sizeof(*frag_tbl), frag_cmp);
}

/*
 * sort frags by their location on disk
 */
void
sort_frags_by_location(int cnt)
{
	if (frag_tbl)
		qsort(frag_tbl, cnt, sizeof(*frag_tbl), frag_cmp_bn);
}

/*
 * find_frag -- find the frag containing bn if it exists, otherwise return -1.
 */
static int
find_frag(efs_daddr_t bn, int cnt)
{
	int bottom = 0;
	int top = cnt - 1;
	int middle = (top + bottom) / 2;

	if (cnt == 0)
		return -1;

	/* Set top to the last entry less than or equal to bn */

	/* Is top already correct? */
	if (frag_tbl[top].fr_start <= bn) {
		bottom = top;

	/* Is the value present? */
	} else if (frag_tbl[bottom].fr_start > bn) {
		return -1;

	/* we now have bottom <= bn < top, maintain and squeeze */
	} while (bottom < middle) {
		if (frag_tbl[middle].fr_start < bn) {
			bottom = middle;
		} else {
			top = middle;
		}
		middle = (top + bottom) / 2;
	}

	if (bn >= frag_tbl[bottom].fr_start
		&& bn < frag_tbl[bottom].fr_start + free_frag_size)
		return bottom;
	return -1;
}

/*
 * inc_alloc_counter -- increment the alloc counter for the given block
 */
void
inc_alloc_counter(efs_daddr_t bn)
{
	int f = find_frag(bn, frag_cnt);

	if (f != -1)
		frag_tbl[f].fr_nalloc++;
}

/*
 * total_frag_free -- return the total number of free blocks in frags.
 */
efs_daddr_t
total_frag_free(void)
{
	efs_daddr_t total = 0;
	int i;

	for (i = 0; i < frag_cnt; i++)
		total += frag_tbl[i].fr_nfree;
	return total;
}

/*
 * get_frag_vals -- return information about a frag.
 */
void
get_frag_vals(int frag_num, efs_daddr_t *startp, efs_daddr_t *nfree,
	efs_daddr_t *length)
{
	if (frag_num >= frag_cnt) {
		*startp = -1;
	} else {
		*startp = frag_tbl[frag_num].fr_start;
		*nfree = frag_tbl[frag_num].fr_nfree;
		*length = free_frag_size;
	}
}

/*
 * count_frags -- return the number of frags that must be freed to reclaim
 * "total" blocks
 */
int
count_frags(efs_daddr_t total)
{
	struct frag *fp = frag_tbl;

	while (total > 0)
		total -= fp++->fr_nfree;
	
	return fp - frag_tbl;
}

/*
 * ex_in_first_frags -- boolean function that determines whether a given
 * extent is in an initial (sorted) segment of the frag table.
 */
int
ex_in_first_frags(efs_daddr_t start, int frag_cnt)
{
	return find_frag(start, frag_cnt) != -1;
}

/*
 * free_frag_structures -- frees up the frag structures and prepares for
 * a next round.
 */
void
free_frag_structures(void)
{
	free(frag_tbl);
	frag_tbl = 0;
	frag_cnt = 0;
	frag_size = 0;
	last_frag = 0;
}
