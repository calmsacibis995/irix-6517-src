/*
 * fsr.h
 *
 * declarations for public routines in lib.c
 * 
 * $Revision: 1.12 $
 */

#define DEVFSCTL        "/dev/fsctl"

#include "libefs.h"
#include <sys/fsctl.h>

extern int getex(struct efs_mount *mp, struct efs_dinode *di,
		struct fscarg *fap, int locked);
extern void freeex(struct fscarg *fap);

extern fsc_init(int (*func)(const char *, ...), int, int);
/* fsctl ioctls */
extern fsc_ilock(struct fscarg *fap);
extern void fsc_iunlock(struct fscarg *fap);
extern fsc_icommit(struct fscarg *fap);
#define fsc_tstfree(dev, bn)            fsc_tstfunc(TSTFREE, dev, bn)
#define fsc_tstalloc(dev, bn)           fsc_tstfunc(TSTALLOC, dev, bn)
extern fsc_tstfunc(int wha, dev_t dev, efs_daddr_t bn);
#define fsc_bfree(dev, bn, len)         fsc_bitfunc(BFREE, dev, bn, len)
#define fsc_balloc(dev, bn, len)        fsc_bitfunc(BALLOC, dev, bn, len)
extern fsc_bitfunc(int wha, dev_t dev, efs_daddr_t bn, int len);

extern efs_daddr_t freebetween(dev_t dev, efs_daddr_t startbn,
		efs_daddr_t beforebn, int wantlen, 
		/* RETURN */ efs_daddr_t *prebn, int *foundlen);
extern efs_daddr_t freefs(struct efs_mount *mp, int cg, int wantlen,
		/* RETURN */ int *gotlen);
extern efs_daddr_t freecg(struct efs_mount *mp, int cg, int wantlen,
		/* RETURN */ int *gotlen);

extern int movex(struct efs_mount *mp, struct fscarg *fap,
			off_t offset, efs_daddr_t bn, int len);
extern int slidex(struct efs_mount *mp, struct fscarg *fap,
			off_t offset, efs_daddr_t newbn, int length,
			int slidesize);
extern int copy(int fd, efs_daddr_t frombn, efs_daddr_t tobn, int len);
extent *offtoex(extent *ex, int ne, off_t offset);
extern int vecmovex(struct efs_mount *mp, struct fscarg *fap,
			int vne, extent *vecex, off_t off, int nb);
extern int moveix(struct efs_mount *mp, struct fscarg *fap, efs_daddr_t newbn,
			int ni);
extern void fsc_close(void);

#define getnblocks(ex, ne) (int)((ex)[(ne) -1].ex_offset \
				+ (ex)[(ne) - 1].ex_length)

extern char *devnm(dev_t dev);

/* statistics */
struct efsstats {
	int     tpieces;
	int     tbb;
	int     tholes;
	int     tfreebb;
};
extern int filepieces(extent *ex, int ne);
extern void fspieces(struct efs_mount *mp, struct efsstats *es);

/* debugging */
extern int exverify(struct efs *fs, struct fscarg *fap);
extern int invalidblocks(struct efs *fs, efs_daddr_t bn, int len);
extern void fprexf(FILE *fp, struct fscarg *fap);
extern void fprex(FILE *fp, int ne, extent *ex, int nie, extent *ix);
extern int fsrprintf(const char *, ...);

/* free space reclamation */
extern efs_daddr_t total_frag_free(void);
extern int ex_in_first_frags(efs_daddr_t start, int frag_cnt);

/* free extent cache */
extern void fe_insert(efs_daddr_t start, int length);
extern void get_frags(dev_t dev, int ncg, efs_daddr_t firstbn,
			efs_daddr_t cgfsize, efs_daddr_t cgisize);
extern void sort_frags_by_value(void);
extern void sort_frags_by_location(int cnt);
extern void inc_alloc_counter(efs_daddr_t bn);
extern void get_frag_vals(int frag_num, efs_daddr_t *startp, efs_daddr_t *nfree,
				efs_daddr_t *length);
extern int count_frags(efs_daddr_t total);
extern void free_frag_structures(void);

extern void fe_clear(void);
extern void fe_init_cnts(void);
void print_free(dev_t dev, efs_daddr_t startbn, efs_daddr_t beforebn);
