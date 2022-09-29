/*
 * bufview - file system buffer cache activity monitor
 */

#define _KMEMUSER

#include <sys/types.h>
#include <sys/times.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <sys/sysmp.h>
#include <sys/param.h>
#include <sys/ksa.h>
#include <sys/sysinfo.h>
#include <sys/sysget.h>
#include <sys/syssgi.h>
#include <assert.h>
#include <values.h>
#include <dirent.h>
#include <stdio.h>
#include <mntent.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include "display.h"
#include "bv.h"
#include "utils.h"
#include "machine.h"

#define KMEM	"/dev/kmem"

slist_t shead, spark;

char header_aggregate[] =
"   VNUMBER NAME                 DEVICE  FSTYP  NBUF   SIZE DELWRI    LOW   HIGH";
/*
 012345678901234567890123456789012345678901234567890123456789012345678901234567
          10        20        30        40        50        60        70
 */

#define Buf_data_format \
	"%10llx %-20.20s %-8.8s %-4.4s %5d %6.6s %6.6s %6.6s %6.6s"

#define Buf_ctl_format \
	"    system                      %-8.8s %-4.4s %5d %6.6s %6.6s %6.6s %6.6s"

/*
 012345678901234567890123456789012345678901234567890123456789012345678901234567
          10        20        30        40        50        60        70
 */
char header_separate[] =
"   VNUMBER NAME/REF      DEVICE  FSTYP   BUF   SIZE OFFSET        AGE FLAGS";

#define Buf_data_format_separate \
	"%10llx %-13.13s %-8.8s %-4.4s %5d %6.6s %6.6s %10d %s"

#define Buf_ctl_format_separate \
	"    system     %2d        %-8.8s %-4.4s %5d %6.6s %6.6s %10d %s"


static  char fmt[MAX_COLS + 2];

/* useful externals */
extern int	errno;
extern char	*sys_errlist[];

extern char	*myname;

static int kmem;
static off64_t v_offset;

static float	irix_ver;		/* for easy numeric comparison */

static bnode_t	*bnodebase;
static binfo_t	*binfobase;

int	pagesize;


/*
 * The following arrays are mostly filled in by get_system_info.
 */

char	*sysbuf_names[] = {
	"J Bufs  ",		/* total # of buffers */
	"P Mem   ",		/* system memory */
	"P SMem  ",		/* memory used currently by system buffers */
	"J Sys   ",		/* # of buffers mapping system data */
	NULL
};

char	*databuf_names[] = {
	"J Data  ",		/* # of buffers mapping file data */
	"P DMin  ",		/* minimum amount of memory file buffers */
				/* can consume without regard for others */
	"P DMem  ",		/* memory used currently by file buffers */
	"P MaxR  ",		/* availrmem */
	"P MaxS  ",		/* availsmem */
	NULL
};

char	*emptybuf_names[] = {
	"J Empty ",		/* # of empty buffers */
	"P FrMin ",		/* min_free_pages * pagesize */
	"P MFree ",		/* free memory */
	"P MFreD ",		/* free memory that caches file data */
	"J Inact ",		/* # of inactive buffers */
	"K deact ",		/* buffers inactivated per second */
	NULL
};

char	*getbuf_names[] = {
	"K gtblk ",		/* getblk calls per second */
	"K found ",		/* buffers found in cache by getblk, per sec */
	"K relse ",		/* buffers released because of getblk calls */
	"K write ",		/* delwri buffers written "  "   "   " */
	"K stky  ",		/* buffers refs decremented "  "   "   " */
	"K react ",		/* buffers un-inactivated per second */
	NULL
};

/*
 * The bflag_list is ordered according to the enum in buf.h. The
 * bufview flags must be first and in enum order since this table
 * is indexed for flag comparisons.
 */

bprune_t bflag_list[] = {
	{ 0, BV_FS_NONE, 1, ""},	
	{ 0, BV_FS_INO, B_FS_INO, "ino"},
	{ 0, BV_FS_INOMAP, B_FS_INOMAP, "inomap"},
	{ 0, BV_FS_DIR_BTREE, B_FS_DIR_BTREE, "dir_bt"},
	{ 0, BV_FS_MAP, B_FS_MAP, "map"},
	{ 0, BV_FS_ATTR_BTREE, B_FS_ATTR_BTREE, "attr_bt"},
	{ 0, BV_FS_AGI, B_FS_AGI, "agi"},
	{ 0, BV_FS_AGF, B_FS_AGF, "agf"},
	{ 0, BV_FS_AGFL, B_FS_AGFL, "agfl"},
	{ 0, BV_FS_DQUOT, B_FS_DQUOT, "dquot"},
	{ 1, B_DELWRI, B_DELWRI, "dw"},
	{ 1, B_BUSY, B_BUSY, "bsy"},
	{ 1, B_SWAP, B_SWAP, "swp"},
	{ 1, B_DELALLOC, B_DELALLOC, "da"},
	{ 1, B_ASYNC, B_ASYNC, "as"},
	{ 1, B_NFS_UNSTABLE, B_NFS_UNSTABLE, "nc"},
	{ 1, B_NFS_ASYNC, B_NFS_ASYNC, "na"},
	{ 1, B_INACTIVE, B_INACTIVE, "inact"},
	{ 1, 0, 0, NULL } ,
};

#define STAT0	0
#define STAT1	1
#define STAT2	2
#define STAT3	3
#define STAT4	4
#define STAT5	5

/* abbreviated process states */
static char *state_abbrev[] =
{ "sys", "data", "empty", "inactive", NULL };

#define BS_METADATA	0
#define BS_DATA		1
#define BS_EMPTY	2
#define BS_INACTIVE	3
#define BS_BUSY		4

/*
 * buf_state
 *	map the buffer state to an integer.
 *	used as an index into state_abbrev[]
 *	as well as an "order" key
 */
static int
buf_state(binfo_t *bp)
{
	int flags = bp->binfo_flags;

	if (flags & B_PAGEIO)
		return BS_DATA;

	if (flags & B_INACTIVE)
		return BS_INACTIVE;

	if (bp->binfo_dev == (dev_t)NODEV)
		return BS_EMPTY;

	return BS_METADATA;
}

/*
 * allocate space for:
 *
 *	binfo array
 *	bnode_t pointers to the above (used for sorting)
 *
 *	Exactly n_bufs binfos are allocated, and to make things simple,
 * 	exactly n_bufs sort bins will be allocated.  The sort bins are
 *	where per-vnode and per-device buffers values are accumulated.
 */
void
allocate_buf_tables(void)
{
	extern int n_bufs;

	if (bnodebase != NULL)
		return;

	if (n_bufs <= 0) {
		fprintf(stderr, "%s: nbufs %d?\n", myname, n_bufs);
		exit(1);
	}

	binfobase = (binfo_t *) malloc(n_bufs * sizeof(binfo_t));

	bnodebase = (bnode_t *)malloc(n_bufs * sizeof(bnode_t));

	if (binfobase == NULL || bnodebase == NULL) {
		(void) fprintf(stderr,
			"%s: malloc: out of memory, n_buf = %d\n",
			myname, n_bufs);
		exit (1);
	}
}

int n_bufs;
struct getblkstats *ogetblkp, *ngetblkp;
int getblkstatsz;

int
machine_init(void)
{
	char		tmpbuf[20];

	pagesize = getpagesize();

	if ((kmem = open(KMEM, O_RDONLY)) == -1)
	{
		perror(KMEM);
		return -1;
	}

	if ((v_offset = sysmp(MP_KERNADDR, MPKA_VAR)) == -1)
	{
		perror("sysmp(MP_KERNADDR, MPKA_VAR)");
		return -1;
	}

	getblkstatsz = (int)sysmp(MP_SASZ, MPSA_BUFINFO);
	ngetblkp = calloc(1, getblkstatsz);
	ogetblkp = calloc(1, getblkstatsz);

	/*
	 * n_buf is the first integer in struct v
	 */
	(void) getkval(v_offset, (int *) &n_bufs, sizeof(n_bufs), "!v.v_buf");

	allocate_buf_tables();

	return (0);
}

int min_file_pages;
int min_free_pages;
long nticks, oticks, ticks;

void
get_system_info(si)
	struct system_info *si;
{
	int		i;
	struct rminfo	realmem;
	struct getblkstats *getblkp;
	sgt_cookie_t	ck;
	struct tms	tbuf;
	extern void stg_sread(char *, void *, int);

	nticks = times(&tbuf);
	ticks = nticks - oticks;
	oticks = nticks;

	if (ticks < HZ)
		ticks = HZ;

	SGT_COOKIE_INIT(&ck);
	getblkp = ogetblkp;
	ogetblkp = ngetblkp;
	ngetblkp = getblkp;

	if (sysget(SGT_BUFINFO, (char *)getblkp, getblkstatsz,
		   SGT_READ|SGT_SUM, &ck) < 0)
	{
		perror("sysget");
		fprintf(stderr, "SGT_READ of SGT_BUFINFO failed\n");
		exit(1);
	}

	if (sysmp(MP_SAGET, MPSA_RMINFO, &realmem, sizeof(realmem)) == -1) {
		perror("sysmp(MP_SAGET, MPSA_RMINFO)");
		return;
	}

	stg_sread("min_file_pages",
		  (void *)&min_file_pages, sizeof(min_file_pages));
	stg_sread("min_free_pages",
		  (void *)&min_free_pages, sizeof(min_free_pages));

	/* first (buffer) column */
	sysbuf_stats[STAT0] = n_bufs;
	sysbuf_stats[STAT1] = realmem.physmem;
	sysbuf_stats[STAT2] = realmem.bufmem;
	sysbuf_stats[STAT3] = system_info.si_metabufs;

	databuf_stats[STAT0] = system_info.si_fsbufs;
	databuf_stats[STAT1] = min_file_pages;
	databuf_stats[STAT2] = realmem.chunkpages;
	databuf_stats[STAT3] = realmem.ravailrmem;
	databuf_stats[STAT4] = realmem.availsmem;

	emptybuf_stats[STAT0] = system_info.si_emptybufs;
	emptybuf_stats[STAT1] = min_free_pages;
	emptybuf_stats[STAT2] = realmem.freemem;
	emptybuf_stats[STAT3] = realmem.freemem - realmem.emptymem;
	emptybuf_stats[STAT4] = system_info.si_inactivebufs;
	emptybuf_stats[STAT5] = (ngetblkp->inactive - ogetblkp->inactive) /
								(ticks/HZ);

	getbuf_stats[STAT0] = (ngetblkp->getblks - ogetblkp->getblks) /
								(ticks/HZ);
	getbuf_stats[STAT1] = (ngetblkp->getfound - ogetblkp->getfound) /
								(ticks/HZ);
	getbuf_stats[STAT2] =
		(ngetblkp->getfreerelse - ogetblkp->getfreerelse) / (ticks/HZ);

	getbuf_stats[STAT3] =
		(ngetblkp->getfreedelwri - ogetblkp->getfreedelwri) /
								(ticks/HZ);
	getbuf_stats[STAT4] = (ngetblkp->getfreeref - ogetblkp->getfreeref) /
								(ticks/HZ);
	getbuf_stats[STAT5] = (ngetblkp->active - ogetblkp->active) /
								(ticks/HZ);
#ifdef notdef
	getbuf_stats[STAT5] =
		(ngetblkp->getfreeempty - ogetblkp->getfreeempty) / (ticks/HZ);
#endif

	return;
}

/*
 * this code is 'borrowed' from osview
 */
typedef struct kname {
	char *	kn_name;
	int	kn_count;
} kname_t;

kname_t knames[] = {
	{ KSYM_MIN_FILE_PAGES,	-1 },
	{ KSYM_MIN_FREE_PAGES,	-1 },
	{ KSYM_PDWRIMEM,	-1 },
};
int num_knames = sizeof(knames) / sizeof(struct kname);

void
stg_sread(char *s, void *buf, int len)
{
	sgt_cookie_t ck;
	sgt_info_t info;
	int i;
	int flags;

	for (i = 0; i < num_knames; i++) {
		if (!strcmp(s, knames[i].kn_name)) {
			break;
		}
	}
	if (i == num_knames) {
		fprintf(stderr, "stg_sread: %s is unknown\n", s);
		quit(2);
	}
	
	if (knames[i].kn_count < 0) {

		/* Find out how many of these exist */

		SGT_COOKIE_INIT(&ck);
		SGT_COOKIE_SET_KSYM(&ck, knames[i].kn_name);
		if (sysget(SGT_KSYM, (char *)&info, sizeof(info), SGT_INFO, &ck) < 0) {
			perror("sysget");
			fprintf(stderr, "sgt_sread: SGT_INFO of %s failed\n",s);
			quit(2);
		}
		knames[i].kn_count = info.si_num;
	}
			
	SGT_COOKIE_INIT(&ck);
	SGT_COOKIE_SET_KSYM(&ck, knames[i].kn_name);

	if (knames[i].kn_count > 1) {

		/* There appear to be copies of this object on each cell.
		 * We will get the sum.
		 */

		flags = SGT_READ | SGT_SUM;
	} else
		flags = SGT_READ;

	if (sysget(SGT_KSYM, buf, len, flags, &ck) < 0) {
		perror("sysget");
		fprintf(stderr, "stg_sread: SGT_READ for %s failed\n", s);
		quit(2);
	}
}

void
count_nodes(bnode_t *bp, int *cnt)
{
	assert(bp);

	if (bp->bn_prev)
		count_nodes(bp->bn_prev, cnt);
	(*cnt)++;
	if (bp->bn_next)
		count_nodes(bp->bn_next, cnt);
}

void
btree_sort_add(bnode_t **head, bnode_t *bp)
{
	bnode_t *nodep;
	slist_t *slp;
	int direction;

	bp->bn_next = bp->bn_prev = NULL;

	if ((nodep = *head) == NULL)
	{
		*head = bp;
		return;
	}

	for (slp = shead.sl_next; slp != &shead; slp = slp->sl_next)
	{
		direction = (*slp->sl_routine)(nodep, bp, slp->sl_tag);

		if (direction == SL_CMP_GT) {
			if (nodep->bn_prev) {
				btree_sort_add(&nodep->bn_prev, bp);
			} else {
				nodep->bn_prev = bp;
			}
			return;
		}

		if (direction == SL_CMP_LT) {
			if (nodep->bn_next) {
				btree_sort_add(&nodep->bn_next, bp);
			} else {
				nodep->bn_next = bp;
			}
			return;
		}
	}

	fprintf(stderr, "no comparisons -- bp %x nodep %x\n", bp, nodep);

	exit(1);
}

/*
 * Here we're walking through the bnode_t tree that's sorted in
 * bn_key order.  We're going to strip the nodes from the ends
 * of the tree and put them onto *put, sorted in output order.
 */
void
sort_bnode_tree(bnode_t *nodep, bnode_t **put)
{
	bnode_t *bp;

	if (bp = nodep->bn_prev)
		sort_bnode_tree(bp, put);

	if (bp = nodep->bn_next)
		sort_bnode_tree(bp, put);
	
	btree_sort_add(put, nodep);
}

int collate_line;
#include "screen.h"

void
print_tree(bnode_t *nodep)
{
	bnode_t *bp;

	if (bp = nodep->bn_prev)
		print_tree(bp);

	Move_to(0, collate_line++);
	printf("%x / %llx: %d [prev %x next %x]",
		nodep, nodep->bn_key, nodep->bn_bufcnt,
		nodep->bn_prev, nodep->bn_next);

	if (bp = nodep->bn_next)
		print_tree(bp);
}

int
btree_collate_add(bnode_t **head, bnode_t *bp)
{
	bnode_t *nodep;

	assert(bp->bn_prev == NULL);
	assert(bp->bn_next == NULL);

	if ((bst.bflags || bst.bvtype) &&
	    !(bp->bn_flags & bst.bflags) &&
	     !(REMAPFLAG & bst.bvtype))
		return 0;

	if ((nodep = *head) == NULL)
	{
		*head = bp;
		bp->bn_bufcnt = 1;
		return 1;
	}

	if (bp->bn_key < nodep->bn_key)
	{
		if (nodep->bn_prev)
			return btree_collate_add(&nodep->bn_prev, bp);
		else {
			nodep->bn_prev = bp;
			bp->bn_bufcnt = 1;
			return 1;
		}
	}

	if (bp->bn_key > nodep->bn_key)
	{
		if (nodep->bn_next)
			return btree_collate_add(&nodep->bn_next, bp);
		else {
			nodep->bn_next = bp;
			bp->bn_bufcnt = 1;
			return 1;
		}
	}

	/*
	 * Got a match -- add data.
	 */
	nodep->bn_mem += bp->bn_mem;
	nodep->bn_dmem += bp->bn_dmem;
	nodep->bn_bufcnt++;

	/*
	 * If buffer representing nodep was locked at the time the
	 * kernel looked at it, the file name will be a construct.
	 * Patch it now.
	 */
	if ((nodep->bn_flags & B_PAGEIO) && !strcmp(nodep->bn_fname, "??"))
	{
		strncpy(nodep->bn_fname, bp->bn_fname, NE_NAME);
		nodep->bn_fname[NE_NAME] = '\0';
	}

	if (nodep->bn_low > bp->bn_low)
		nodep->bn_low = bp->bn_low;
	if (nodep->bn_high < bp->bn_high)
		nodep->bn_high = bp->bn_high;
	return 0;
}

/*
 * First pass of sort.  Push binfo data into bpnodes.
 * Generate simple sort key, then build tree in key order.
 * If we're aggregating info, it is done here.
 *
 * Later, the bpnodes get sorted for output.
 */
void
collate_buf_table(
	binfo_t *binfop,
	bnode_t *bnodep,
	bnode_t **bhp,
	struct system_info *si)
{
	int i;
	binfo_t *bp;
	bnode_t *bn;
	extern void dev_to_names(dev_t, bnode_t *);
	extern char *get_fsname(int);
	extern int dev_list_rebuilt;

	*bhp = NULL;

	si->si_busers =
	si->si_fusers =
	si->si_fsbufs =
	si->si_metabufs =
	si->si_inactivebufs =
	si->si_busybufs =
	si->si_emptybufs = 0;

	si->si_fssize =
	si->si_fsdsize =
	si->si_metasize =
	si->si_metadsize = 0;

	bzero(bnodep, n_bufs * sizeof(*bnodep));

	for (bp = binfop, bn = bnodep, i = 0; i < n_bufs; i++, bp++, bn++)
	{
		int64_t key;

		if (bp->binfo_flags & B_BUSY)
			si->si_busybufs++;

		switch (buf_state(bp))
		{

		case BS_DATA:
			si->si_fsbufs++;
			si->si_fssize += bp->binfo_size;

			if (bp->binfo_flags & B_DELWRI)
			{
				si->si_fsdsize += bp->binfo_size;
				bn->bn_dmem = bp->binfo_size;
			}

			if (bst.fsdata)
			{
				bn->bn_mem = bp->binfo_size;
				bn->bn_low = BBTOB(bp->binfo_offset);
				bn->bn_high = BBTOB(bp->binfo_offset) +
								bp->binfo_size;
				bn->bn_flags = bp->binfo_flags;
				bn->bn_bvtype = bp->binfo_bvtype;

				dev_to_names(bp->binfo_dev, bn);
				if (prune_dev(bn->bn_devname))
					continue;

				strncpy(bn->bn_fname, bp->binfo_name, NE_NAME);
				bn->bn_fname[NE_NAME] = '\0';

				bn->bn_fstype = get_fsname(bp->binfo_fstype);
				bn->bn_vnumber = bp->binfo_vnumber;
				bn->bn_start = bp->binfo_start;

				if (bst.separate)
				{
					key = bp->binfo_start;
					bn->bn_key = (key << 32) | i;
				}
				else
				{
					bn->bn_key = bp->binfo_vp;
				}

				if (btree_collate_add(bhp, bn))
				{
					si->si_busers++;
					si->si_fusers++;
				}
			}
			break;

		case BS_METADATA:
			si->si_metabufs++;
			si->si_metasize += bp->binfo_size;

			if (bp->binfo_flags & B_DELWRI)
			{
				si->si_metadsize += bp->binfo_size;
				bn->bn_dmem = bp->binfo_size;
			}

			if (bst.system)
			{
				bn->bn_mem = bp->binfo_size;
				bn->bn_low = BBTOB(bp->binfo_blkno);
				bn->bn_high = bn->bn_low + bp->binfo_size;
				bn->bn_flags = bp->binfo_flags;
				bn->bn_bvtype = bp->binfo_bvtype;

				dev_to_names(bp->binfo_dev, bn);
				if (prune_dev(bn->bn_devname))
					continue;

				bn->bn_start = bp->binfo_start;
				bn->bn_fpass = bp->binfo_fpass;

				if (bst.separate)
				{
					key = bp->binfo_start;
					bn->bn_key = (key << 32) | i;
				}
				else
				{
					bn->bn_key = bp->binfo_dev;
				}

				if (bn->bn_key && btree_collate_add(bhp, bn))
					si->si_busers++;
			}
			break;

		case BS_INACTIVE:
			si->si_inactivebufs++;
			if (bst.bflags & B_INACTIVE)
			{
				key = bp->binfo_start;
				bn->bn_key = (key << 32) | i;
				bn->bn_mem = bp->binfo_size;
				bn->bn_low = bp->binfo_blkno;
				bn->bn_high = bp->binfo_blkno
						+ BTOBB(bp->binfo_size);
				bn->bn_flags = bp->binfo_flags;
				bn->bn_bvtype = bp->binfo_bvtype;
				bn->bn_vnumber = 0;
				bn->bn_start = bp->binfo_start;

				if (btree_collate_add(bhp, bn))
					si->si_busers++;
			}
			break;

		case BS_EMPTY:
			si->si_emptybufs++;
			break;

		}
	}
}

void
get_buffer_info(struct system_info *si)
{
	bnode_t		*bag;
	static char	first_screen = 1;

	/* read all the buffer structures */
	if (syssgi(SGI_BUFINFO, 0, n_bufs, binfobase) < 0) {
		perror("syssgi(SGI_BUFINFO)");
		exit(1);
	}

	collate_buf_table(binfobase, bnodebase, &bag, si);
	si->si_bufs = NULL;

	if (bag)
	{
		sort_bnode_tree(bag, &si->si_bufs);
	}

	first_screen = 0;
}

char   *fstype_names;
int	fstype_index;

#define FS_EXTRA 10
#define MAX_INDEX	4096

char *
get_fsname(int index)
{
	char *fsname;

	if (index > MAX_INDEX)
	{
		fprintf(stderr, "fs index %d\n", index);
		return "huh?";
	}

	if (index >= fstype_index)
	{
		if (fstype_names)
			free(fstype_names);
		fstype_index = index + FS_EXTRA;
		fstype_names = calloc(FSTYPSZ, fstype_index);
	}

	fsname = fstype_names + (FSTYPSZ * index);

	if (*fsname == '\0')
	{
		if (sysfs(GETFSTYP, index, fsname))
			strcpy(fsname, "???");
		else
			*(fsname+4) = '\0';
	}

	return fsname;
}

/*
 * bufview keeps a list of the mounted file system names,
 * which are displayed in the DEVICE column.
 * The list of devices and their corresponding names are
 * first built via getmntent().  If a buffer contains a
 * device not on the list, the list is rebuilt (in case there's
 * a newly-mounted file system).  If that doesn't work, a fake
 * device name is created for it.
 */
mnt_entry_t *mnt_head;

void
devlist_init(void)
{
	FILE *f;
	struct mntent *mntent;
	struct stat statb;
	mnt_entry_t *me;
	char namebuf[256];
	char *cp, *cp_slash;
	int i;
	int fd;

	mnt_head = NULL;

	f = setmntent("/etc/mtab", "r");

	while (mntent = getmntent(f))
	{
#ifdef notdef
		fprintf(stderr, "stat %s %s\n",
			mntent->mnt_dir,
			mntent->mnt_fsname);
#endif

		if (stat(mntent->mnt_dir, &statb))
		{
			fprintf(stderr, "couldn't stat %s\n", mntent->mnt_dir);
			perror("stat");
			continue;
		}

		if (statb.st_dev == 0)
			continue;

		me = malloc(sizeof(mnt_entry_t));

		i = strlen(mntent->mnt_type);
		if (i > FSTYPSZ)
			i = FSTYPSZ;
		strncpy(me->me_fstype, mntent->mnt_type, i);
		me->me_fstype[3] = '\0';

		strcpy(namebuf, mntent->mnt_fsname);
		for (cp = namebuf; *cp; cp++)
			;

		cp_slash = NULL;
		for (i = 0; i < ME_NAME; cp--, i++)
		{
			if (cp == namebuf)
				break;
			/*
			 * remember last pathname slash
			 */
			if (*cp == '/')
				cp_slash = cp;
		}

		if (cp_slash && cp != namebuf)
			cp = cp_slash+1;

		strncpy(me->me_name, (const char *)cp, ME_NAME);
		me->me_name[ME_NAME] = '\0';
		me->me_dev = statb.st_dev;
		me->me_next = mnt_head;
		mnt_head = me;

#ifdef notdef
		new_message(MT_standout, "fsname %s dir %s\n",
			mntent->mnt_fsname,
			mntent->mnt_dir);
#endif
	}

	(void) endmntent(f);
}

int dev_list_rebuilt;

void
dev_to_names(dev_t dev, bnode_t *bn)
{
	mnt_entry_t *me;
	char nameb[64];

	for ( ; ; )
	{
		for (me = mnt_head; me; me = me->me_next)
		{
			if (me->me_dev == dev)
			{
				bn->bn_devname = me->me_name;
				bn->bn_fstype = me->me_fstype;
				return;
			}
		}

		if (dev_list_rebuilt)
			break;
		dev_list_rebuilt = 1;

		devlist_destroy();
		devlist_init();
	}

	me = malloc(sizeof(mnt_entry_t));

	sprintf(nameb, "%x", dev);
	strncpy(me->me_name, nameb, ME_NAME);
	me->me_name[ME_NAME] = '\0';

	me->me_next = mnt_head;
	strcpy(me->me_fstype, "???");
	me->me_dev = dev;
	mnt_head = me;

#ifdef notdef
	new_message(MT_standout, "me_name %s dev %x\n",
			me->me_name, me->me_dev);
#endif

	bn->bn_devname = me->me_name;
	bn->bn_fstype = me->me_fstype;
}

/*
 * bufview allows the caller to specify which devices buffes to display
 * (the default is all).
 * If devices are specified, they're put on the dphead list.
 * If anything's on the list, and a particular device isn't,
 * the buffer won't get displayed.
 */

typedef struct dev_prune {
	struct dev_prune *d_next;
	char		 *d_name;
} dev_prune_t;

dev_prune_t *dphead;

int
prune_dev(char *devname)
{
	dev_prune_t *dp;
	int rv = dphead ? 1 : 0;

	for (dp = dphead; dp; dp = dp->d_next)
	{
		if (!strcmp(devname, dp->d_name))
			return 0;
	}
	return rv;
}

int
display_devs(void)
{
	int n = 0;

	if (dphead)
	{
		dev_prune_t *dp;

		n += printf(" Display devices:");
		for (dp = dphead; dp; dp = dp->d_next)
		{
			n += printf(" %s", dp->d_name);
		}
		n += printf(".");
	}
	return n;
}

void
display_only_dev(char *dpname)
{
	char *cp;
	dev_prune_t *dp;

	for (dp = dphead; dp; dp = dp->d_next)
	{
		if (!strcmp(dpname, dp->d_name))
			return;
	}

	cp = malloc(strlen(dpname)+1);
	dp = malloc(sizeof(dev_prune_t));
	strcpy(cp, dpname);
	dp->d_name = cp;
	dp->d_next = dphead;
	dphead = dp;
}

void
display_all_devs(void)
{
	dev_prune_t *dp;

	while (dp = dphead)
	{
		dphead = dp->d_next;
		free(dp->d_name);
		free(dp);
	}
}

void
devlist_destroy(void)
{
	mnt_entry_t *me;

	while (me = mnt_head)
	{
		mnt_head = me->me_next;
		free(me);
	}
}

/*
 * Routines to format a buffer display line.
 * There are separate format routines for data and fsctl buffers;
 * each routine then choosed between aggregate or itemized mode.
 */
char *
format_data_bnode(bnode_t *bp)
{
	/* format this entry */

	if (bst.separate)
	{
		sprintf(fmt,
			Buf_data_format_separate,
			bp->bn_vnumber,
			bp->bn_fname,
			bp->bn_devname,
			bp->bn_fstype,
			(int)bp->bn_key & 0xffffff,
			konvert(bp->bn_mem),
			konvert(bp->bn_low),
			bp->bn_start,
			display_flags(bp->bn_flags, 
					REMAPFLAG, " "));
	}
	else
	{
		/* format this entry */
		sprintf(fmt,
			Buf_data_format,
			bp->bn_vnumber,
			bp->bn_fname,
			bp->bn_devname,
			bp->bn_fstype,
			bp->bn_bufcnt,
			konvert(bp->bn_mem),
			konvert(bp->bn_dmem),
			konvert(bp->bn_low),
			konvert(bp->bn_high));
	}

	/* return the result */
	return (fmt);
}

char *
format_ctl_bnode(bnode_t *bp)
{
	/* format this entry */

	if (bst.separate)
	{
		sprintf(fmt,
			Buf_ctl_format_separate,
			bp->bn_fpass,
			bp->bn_devname,
			bp->bn_fstype,
			(int)bp->bn_key & 0xffffff,
			konvert(bp->bn_mem),
			konvert(bp->bn_low),
			bp->bn_start,
			display_flags(bp->bn_flags,
					REMAPFLAG, " "));
	}
	else
	{
		/*
		 * Printing aggregated buffer data.
		 */
		sprintf(fmt,
			Buf_ctl_format,
			bp->bn_devname,
			bp->bn_fstype,
			bp->bn_bufcnt,
			konvert(bp->bn_mem),
			konvert(bp->bn_dmem),
			konvert(bp->bn_low),
			konvert(bp->bn_high));
	}

	/* return the result */
	return (fmt);
}

static int display_line;
static int display_n;

void
display_bnodes(bnode_t *bp, int (*display_buf)())
{
	assert(bp);

	if (!bp) {
		fprintf(stderr, "in display_bnodes without a bnode\n");
		exit(1);
	}

	if (bp->bn_next) {
		display_bnodes(bp->bn_next, display_buf);
		if (display_n <= 0)
			return;
	}

	if (bp->bn_flags & B_PAGEIO)
	{
		(*display_buf)(display_line++, format_data_bnode(bp));
	}
	else
	{
		(*display_buf)(display_line++, format_ctl_bnode(bp));
	}

	if (--display_n <= 0)
		return;

	if (bp->bn_prev)
		display_bnodes(bp->bn_prev, display_buf);
}

/*
 * print up to 'active_users' bnodes, in size order
 * XXX	Debugging.
 */
void
display_buffer_users(int active_users, int (*display_buf)())
{
	int cnt = 0;
	display_n = active_users;
	display_line = 0;

	if (system_info.si_bufs && (active_users > 0))
		display_bnodes( system_info.si_bufs,
				display_buf);
}

/*
 *  getkval(offset, ptr, size, refstr) - get a value out of the kernel.
 *	"offset" is the byte offset into the kernel for the desired value,
 *  	"ptr" points to a buffer into which the value is retrieved,
 *  	"size" is the size of the buffer (and the object to retrieve),
 *  	"refstr" is a reference string used when printing error meessages,
 *	    if "refstr" starts with a '!', then a failure on read will not
 *  	    be fatal (this may seem like a silly way to do things, but I
 *  	    really didn't want the overhead of another argument).
 */

int
getkval(off64_t offset, int *ptr, int size, char *refstr)
{
	if (lseek64(kmem, offset, SEEK_SET) == -1) {
		if (*refstr == '!')
			refstr++;
		(void) fprintf(stderr, "%s: %s: lseek to %s: %s\n",
				myname, KMEM, refstr, strerror(errno));
		exit(0);
	}
	if (read(kmem, (char *) ptr, size) == -1) {
		if (*refstr == '!')
			return (0);
		else {
			(void) fprintf(stderr,
				"%s: %s: reading %s at 0x%llx: %s [%d]\n",
				myname, KMEM, refstr, offset,
				strerror(errno), errno);
			exit(0);
		}
	}
	return (1);
}


sltag_t
s_nbuf(bnode_t *t, bnode_t *bp, sltag_t tag)
{
	int64_t n;

	assert(tag != 0);

	if (n = (t->bn_bufcnt - bp->bn_bufcnt)) {
		return (n > 0) ? tag : -tag;
	}

	return 0;
}

sltag_t
s_size(bnode_t *t, bnode_t *bp, sltag_t tag)
{
	int64_t n;

	assert(tag != 0);

	if (n = (t->bn_mem - bp->bn_mem)) {
		return (n > 0) ? tag : -tag;
	}

	return 0;
}

sltag_t
s_key(bnode_t *t, bnode_t *bp, sltag_t tag)
{
	int64_t n;

	assert(tag != 0);

	if (n = (t->bn_key - bp->bn_key)) {
		return (n > 0) ? tag : -tag;
	}

	assert(0);

	return 0;
}

sltag_t
s_age(bnode_t *t, bnode_t *bp, sltag_t tag)
{
	int64_t n;

	assert(tag != 0);

	if (n = (t->bn_start - bp->bn_start)) {		/* for now */
		return (n > 0) ? tag : -tag;
	}

	return 0;
}

#define SL_NBUF	0
#define SL_SIZE	1
#define SL_AGE	2
#define SL_KEY	3

slist_t sort_list[] = {
	{ s_nbuf, SL_CMP_GT, SF_AGG,
		"m", /* most */
		"l", /* least */
		NULL, NULL} ,
	{ s_size, SL_CMP_GT, SF_NONE,
		"b", /* biggest */
		"s", /* smallest */
		NULL, NULL} ,
	{ s_age, SL_CMP_GT, SF_IND,
		"n", /* newest */
		"o", /*oldest */
		NULL, NULL} ,
	{ s_key, SL_CMP_GT, SF_NONE,
		" generic sort key", " ",
		NULL, NULL} ,
	{ NULL, SL_CMP_EQ, SF_NONE,
		" ", " ",
		NULL, NULL} ,
};

char dorder_buffer[MAX_COLS];

char *
display_order(void)
{
	char *to = dorder_buffer;
	const char *from;
	slist_t *sp;
	int cnt;
	int first = 1;

	for (sp = shead.sl_next; sp != &shead; sp = sp->sl_next)
	{
		if (sp->sl_tag == SL_CMP_GT)
			from = sp->sl_sup;
		else
			from = sp->sl_inf;

		if (*from == ' ')
			break;

		if (!first)
			*to++ = '.';
		else
			first = 0;

		cnt = strlen(from);
		strncpy(to, from, cnt);
		to += cnt;
	}
	*to++ = '.';
	*to = '\0';

	return dorder_buffer;
}

void
slist_delete(slist_t *sentry)
{
	slist_t *sp = sentry->sl_prev;
	slist_t *sn = sentry->sl_next;

	sp->sl_next = sn;
	sn->sl_prev = sp;
}

void
slist_insert(slist_t *slp, slist_t *sentry)
{
	slist_t *sn;

	sn = slp->sl_next;
	slp->sl_next = sentry;
	sentry->sl_next = sn;
	sentry->sl_prev = slp;
	sn->sl_prev = sentry;
}

/*
 * Inialize, by hand, the sort list for buffer printing.
 */
void
sortlist_init(void)
{
	shead.sl_next = shead.sl_prev = &shead;
	spark.sl_next = spark.sl_prev = &spark;

	slist_insert(&shead, &sort_list[SL_KEY]);
	slist_insert(&shead, &sort_list[SL_SIZE]);
	slist_insert(&shead, &sort_list[SL_NBUF]);

	slist_insert(&spark, &sort_list[SL_AGE]);
}

void
slist_use_only_attr(short attr)
{
	slist_t *sp;
	short attr_not;

	switch (attr)
	{
	case SF_AGG:
		bst.separate = 0;
		bst.bflags = 0;
		bst.bvtype = 0;
		attr_not = SF_IND;
		break;
	case SF_IND:
		bst.separate = 1;
		attr_not = SF_AGG;
		break;
	default:
		return;
	}

	reset_display();

again:
	for (sp = shead.sl_next; sp != &shead; sp = sp->sl_next)
	{
		if (sp->sl_attr & attr_not)
		{
			slist_delete(sp);
			slist_insert(&spark, sp);
			goto again;
		}
	}

	assert(shead.sl_next != &shead);
}

char *
sortlist_reorder(char sort_tag)
{
	slist_t *s_entry;

	if (sort_tag == ' ')	/* this is our stop char */
		return (char *)NULL;

	for (s_entry = &sort_list[0]; s_entry->sl_tag != SL_CMP_EQ; s_entry++)
	{
		if (sort_tag == s_entry->sl_sup[0])
		{
			slist_delete(s_entry);
			s_entry->sl_tag = SL_CMP_GT;
			if (s_entry->sl_attr & (SF_AGG|SF_IND))
			{
				slist_use_only_attr(s_entry->sl_attr);
			}
			slist_insert(&shead, s_entry);
			return s_entry->sl_sup;
		}
		if (sort_tag == s_entry->sl_inf[0])
		{
			slist_delete(s_entry);
			s_entry->sl_tag = SL_CMP_LT;
			if (s_entry->sl_attr & (SF_AGG|SF_IND))
			{
				slist_use_only_attr(s_entry->sl_attr);
			}
			slist_insert(&shead, s_entry);
			return s_entry->sl_inf;
		}
	}

	return (char *)NULL;
}

char *
sortlist_bprune(char *fptr)
{
	bprune_t *bprune;

	for (bprune = bflag_list; bprune->bp_flag; bprune++)
	{
		if (!strcmp(fptr, bprune->bp_match))
		{
			if (bprune->bp_index)
				bst.bflags |= bprune->bp_remap;
			else
				bst.bvtype |= bprune->bp_remap;
			slist_use_only_attr(SF_IND);
			return (char *)bprune->bp_match;
		}
	}

	return (char *)NULL;
}

char bflags_buffer[MAX_COLS];

char *
display_flags(uint64_t flags, uint64_t bvtype, char *separator)
{
	char *to = bflags_buffer;
	bprune_t *bprune;
	int cnt;
	int first = 1;


	for (bprune = bflag_list; bprune->bp_flag; bprune++)
	{

		if ((bprune->bp_index && (bprune->bp_remap & flags))
		     || ((!bprune->bp_index) && (bprune->bp_remap & bvtype)))
		{
			if (!first)
			{
				cnt = strlen(separator);
				strncpy(to, separator, cnt);
				to += cnt;
			} else {
				first = 0;
			}

			cnt = strlen(bprune->bp_match);
			strncpy(to, bprune->bp_match, cnt);
			to += cnt;
		}
	}

	*to = '\0';

	return bflags_buffer;
}
