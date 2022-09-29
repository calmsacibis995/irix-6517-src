/*
 *  This file defines the interface between bufview and the machine-dependent
 *  module.
 */

#include <mntent.h>
#include <sys/fstyp.h>
#include <sys/buf.h>

extern char *sysbuf_names[];
extern char *databuf_names[];
extern char *emptybuf_names[];
extern char *getbuf_names[];

extern int *sysbuf_stats;
extern int *databuf_stats;
extern int *databuf_stats;
extern int *emptybuf_stats;
extern int *getbuf_stats;

extern int	pagesize;


/*
 * the system_info struct is filled in by a machine dependent routine.
 */

struct system_info
{
    struct bnode *si_bufs;	/* pointer to bnode tree */
    int		si_busers;	/* number of bnodes */
    int		si_fusers;	/* number of bnodes */
    int		si_fsbufs;	/* number of file-system buffers */
    int		si_metabufs;	/* number of fs ctl buffers */
    int		si_emptybufs;	/* number of empty buffers */
    int		si_inactivebufs;/* number of inactive buffers */
    int		si_busybufs;	/* number of busy buffers */
    __int64_t	si_fssize;	/* mem size of file-system buffers */
    __int64_t	si_fsdsize;	/* mem size of delwri file-system buffers */
    __int64_t	si_metasize;	/* mem size of fs ctl buffers */
    __int64_t	si_metadsize;	/* mem size of delwri fs ctl buffers */
};

extern struct system_info system_info;

extern int n_bufs;

#define NE_NAME 24

typedef struct bnode {
	struct bnode *	bn_prev;
	struct bnode *	bn_next;
	uint64_t	bn_key;		/* for sorting -- vnumber or dev */
	uint64_t	bn_vnumber;
	int64_t		bn_mem;
	int64_t		bn_dmem;
	int64_t		bn_low;
	int64_t		bn_high;
	char		bn_fname[NE_NAME+1];
	char		bn_fpass;
	char		bn_bvtype;
	clock_t		bn_start;
	int		bn_bufcnt;
	char	*	bn_devname;
	char	*	bn_fstype;
	uint64_t	bn_flags;
} bnode_t;

#define ME_NAME 8

typedef struct mnt_entry {
	struct mnt_entry *	me_next;
	char			me_name[ME_NAME+1];
	char			me_fstype[FSTYPSZ];
	dev_t			me_dev;
} mnt_entry_t;

/*
 * the buffer_select struct tells buffer sorting routines what
 * buffers we are interested in seeing
 */

struct buffer_select
{
	int system;		/* show system buffers? */
	int fsdata;		/* show data buffers? */
	int separate;		/* don't show aggregated buffers */
	uint64_t bflags;	/* show only those buffers with these flags */
	uint64_t bvtype;	/* show only those remapped bvtypes */
};

extern struct buffer_select bst;
typedef int sltag_t;

typedef struct slist {
	sltag_t		(*sl_routine)(bnode_t *, bnode_t *, sltag_t);
	sltag_t		sl_tag;
	short		sl_attr;
	char	*	sl_sup;
	char	*	sl_inf;
	struct slist *	sl_next;
	struct slist *	sl_prev;
} slist_t;

#define SL_CMP_EQ	0
#define SL_CMP_LT	-1
#define SL_CMP_GT	1

#define	SF_NONE		0x0
#define	SF_AGG		0x1	/* aggregate buffer info */
#define	SF_IND		0x2	/* independent buffer stats */

extern sltag_t s_nbuf(bnode_t *, bnode_t *, sltag_t cmp);
extern sltag_t s_size(bnode_t *, bnode_t *, sltag_t cmp);
extern sltag_t s_key(bnode_t *, bnode_t *, sltag_t cmp);
extern sltag_t s_age(bnode_t *, bnode_t *, sltag_t cmp);

typedef struct bprune {
	int		bp_index;
	uint64_t	bp_remap;
	uint64_t	bp_flag;
	const char *	bp_match;
} bprune_t;

/*
 * Flags for buffer usage statistics (bufview).
 * Stored in b_bvtype.
 */
#define BV_FS_NONE		0x00000LL
#define BV_FS_INO		0x00001LL
#define BV_FS_INOMAP		0x00002LL
#define BV_FS_DIR_BTREE		0x00004LL
#define BV_FS_MAP		0x00008LL
#define BV_FS_ATTR_BTREE	0x00010LL
#define BV_FS_AGI		0x00020LL
#define BV_FS_AGF		0x00040LL
#define BV_FS_AGFL		0x00080LL
#define BV_FS_DQUOT		0x00100LL

#define REMAPFLAG (bflag_list[bp->bn_bvtype].bp_remap)

/* routines defined by the machine dependent module */

char *format_header();
void sortlist_init(void);
char *sortlist_reorder(char);
char *sortlist_bprune(char *);

int prune_dev(char *);
int display_devs(void);
void display_only_dev(char *);
void display_all_devs(void);

void devlist_destroy(void);
