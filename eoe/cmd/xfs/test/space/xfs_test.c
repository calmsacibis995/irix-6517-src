#ident	"$Revision: 1.56 $"

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/var.h>
#define _KERNEL	1
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/user.h>
#include <sys/grio.h>
#undef _KERNEL
#include <sys/vnode.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <bstring.h>
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_mount.h"
#include "xfs_print.h"
#include "xfs_alist.h"
#include "xfs_alloc_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_btree.h"
#include "xfs_ialloc.h"
#include "xfs_dinode.h"
#include "xfs_inode_item.h"
#include "xfs_inode.h"
#include "xfs_alloc.h"
#include "xfs_bmap.h"
#include "xfs_rtalloc.h"
#include "xfs_dir.h"
#include "sim.h"

#define MAXFACT	1000
struct factor
{
	int fact;
	xfs_extlen_t fmin;
	xfs_extlen_t fmax;
} factors[MAXFACT];
int nfactor = 0;

struct mm
{
	xfs_extlen_t fmin;
	xfs_extlen_t fmax;
} *tfacts;
int tfact;

xfs_rfsblock_t nblocks;
struct cred crz;

void init_tfact(void);
void usage(void);

/*
 * This is here because it was simpler to add it than to remove the calls.
 */
xfs_fsblock_t
xfs_alloc_extent(xfs_trans_t *tp, xfs_fsblock_t bno, xfs_extlen_t len,
	xfs_alloctype_t type, xfs_extlen_t total, xfs_extlen_t minleft,
	int wasdel)
{
	xfs_extlen_t	rlen;
	xfs_fsblock_t	rval;

	return xfs_alloc_vextent(tp, bno, len, len, &rlen, type, total, minleft,
		wasdel, 0, 1);
}

/*
 * options:
 *		-f (device is regular file)
 *		-q (quiet)
 *		-r (print seed)
 *		-s (set seed)
 *		-v (volume)
 */
main(int argc, char **argv)
{
	xfs_agnumber_t agno;
	int agpct;
	xfs_agblock_t agsize;
	xfs_ino_t aino = NULLFSINO;
	int allocpct;
	int bigmax;
	xfs_fsblock_t bno;
	xfs_fileoff_t boff;
	int c;
	char cmode;
	xfs_dfsbno_t dbno;
	xfs_dfiloff_t dboff;
	int done;
	xfs_drtbno_t drtstart;
	int exactpct;
	xfs_alloctype_t f;
	int fact;
	int failures;
	int fflag;
	xfs_fsblock_t firstblock;
	int flags;
	xfs_bmap_free_t flist;
	xfs_extlen_t fmax;
	xfs_extlen_t fmin;
	xfs_bmap_free_item_t *fp;
	int i;
	xfs_ino_t ino;
	xfs_inode_t *ip;
	int isv;
	int j;
	int junk;
	xfs_extlen_t len;
	char line[80];
	xfs_bmbt_irec_t *maps;
	xfs_extlen_t maxlen;
	xfs_rfsblock_t maxoff;
	xfs_extlen_t minlen;
	xfs_extlen_t mod;
	int mode;
	xfs_mount_t *mp;
	xfs_agnumber_t nags;
	int nearpct;
	int nmap;
	int nmapi;
	int onmap;
	extern char *optarg;
	extern int optind;
	xfs_ino_t pino;
	xfs_inode_t *pip;
	xfs_extlen_t prod;
	int qflag;
	xfs_agblock_t ragbno;
	xfs_agnumber_t ragno;
	int random_seed;
	xfs_fsblock_t rbno;
	int rflag;
	int rt;
	xfs_rtblock_t rtstart;
	xfs_extlen_t rtlen;
	char rw;
	xfs_sb_t *sbp;
	char *sflag;
	sim_init_t si;
	int startpct;
	int totalops;
	xfs_trans_t *tp;
	int vallocpct;
	int vflag;
	buf_t *ialloc_context;
	boolean_t call_again;
	char *log;

	bigmax = allocpct = vallocpct = totalops = 0;
	exactpct = nearpct = agpct = startpct = 0;
	failures = fflag = qflag = rflag = vflag = 0;
	sflag = NULL;
	log = NULL;
	bzero(&si, sizeof(si));
	while ((c = getopt(argc, argv, "fqrs:l:v")) != EOF) {
		switch (c) {
		case 'f':
			fflag = 1;
			break;
		case 'l':
			log = optarg;
			xlog_debug = 1;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 's':
			sflag = optarg;
			break;
		case 'v':
			vflag = 1;
			break;
		case '?':
			usage();
		}
	}
	if (argc - optind != 1)
		usage();
	if (vflag) {
		si.volname = argv[optind];
	} else {
		si.dname = argv[optind];
		si.disfile = fflag;
	}
	si.logname = log;
	si.lisfile = fflag;
	xfs_sim_init(&si);
	mp = xfs_mount(si.ddev, si.logdev, si.rtdev);
	sbp = &mp->m_sb;
	if (sbp->sb_magicnum != XFS_SB_MAGIC) {
		fprintf(stderr, "%s: magic number %d is wrong\n", si.dname,
			sbp->sb_magicnum);
		exit(1);
	}
	nblocks = sbp->sb_dblocks;
	nags = sbp->sb_agcount;
	agsize = sbp->sb_agblocks;
	init_tfact();
	if (sflag)
		srandom(random_seed = atoi(sflag));
	else
		srandom(random_seed = getpid() ^ gtime());
	if (rflag)
		printf("random seed=%d\n", random_seed);
rops:
	for (i = 1; i <= totalops; i++) {
		tp = xfs_trans_alloc(mp, 0);
		c = random() % 100;
		if (c < allocpct + vallocpct) {
			if (aino != NULLFSINO) {
				boff = random();	/* offset in file */
				j = random() % tfact;
				c = (int)(tfacts[j].fmax - tfacts[j].fmin + 1);
				len = random() % c + tfacts[j].fmin;
				xfs_trans_reserve(tp, len + 10, BBTOB(128),
						  si.rtdev ? len : 0, 0);
				nmapi = len;
				if (nmapi > XFS_BMAP_MAX_NMAP)
					nmapi = XFS_BMAP_MAX_NMAP;
				maps = malloc(nmapi * sizeof(*maps));
				ip = xfs_trans_iget(mp, tp, aino,
						    XFS_ILOCK_EXCL);
				nmap = nmapi;
				XFS_BMAP_INIT(&flist, &firstblock);
				firstblock = xfs_bmapi(tp, ip, boff, len,
					XFS_BMAPI_WRITE, firstblock, len, maps,
					&nmap, &flist);
				if (!qflag) {
					printf(
		"xfs_bmapi(%lld[%d:%d],%lld,%d,%s,%s,%d,%d) returned %lld\n",
						ino, XFS_INO_TO_AGNO(mp, ino),
						XFS_INO_TO_AGINO(mp, ino),
						(xfs_dfiloff_t)boff,
						len, "XFS_BMAPI_WRITE",
						"NULLFSBLOCK", len, nmapi,
						(xfs_dfsbno_t)firstblock);
					for (j = 0; j < nmap; j++) {
						xfs_extlen_t c;
						xfs_fileoff_t o;
						char *p;
						xfs_fsblock_t s;

						o = maps[j].br_startoff;
						s = maps[j].br_startblock;
						c = maps[j].br_blockcount;
						p = xfs_print_startblock(mp, s);
						printf(
				"\t%d: offset=%lld, block=%s, count=%d\n",
							j, (xfs_dfiloff_t)o,
							p, c);
					}
				}
				xfs_bmap_finish(&tp, &flist, firstblock, 0);
				xfs_trans_commit(tp, 0);
				free(maps);
				continue;
			}
			isv = c >= allocpct;
			c = random() % 100;
			if (c < exactpct && alist_nallocs) {
				f = XFS_ALLOCTYPE_THIS_BNO;
				c = random() % alist_nallocs;
				get_alloced(c, &bno, &len, &rt, 0);
				bno += len;
			} else if (c < exactpct + nearpct) {
				f = XFS_ALLOCTYPE_NEAR_BNO;
				bno = random() % nblocks;
			} else if (c < exactpct + nearpct + agpct) {
				f = XFS_ALLOCTYPE_THIS_AG;
				bno = (random() % nags) * agsize;
			} else if (c < exactpct + nearpct + agpct + startpct) {
				f = XFS_ALLOCTYPE_START_AG;
				bno = random() % nblocks;
			} else {
				f = XFS_ALLOCTYPE_ANY_AG;
				bno = 0;
			}
			j = random() % tfact;
			c = (int)(tfacts[j].fmax - tfacts[j].fmin + 1);
			if (isv) {
				minlen = random() % c + tfacts[j].fmin;
				maxlen = random() % c + tfacts[j].fmin;
				if (minlen > maxlen) {
					len = maxlen;
					maxlen = minlen;
					minlen = len;
				}
				xfs_trans_reserve(tp, maxlen + 10, BBTOB(128),
						  0, 0);
				rbno = xfs_alloc_vextent(tp, bno, minlen,
							 maxlen, &len, f,
							 maxlen, 0, 0, 0, 1);
			} else {
				len = random() % c + tfacts[j].fmin;
				xfs_trans_reserve(tp, len + 10, BBTOB(128),
						  0, 0);
				rbno = xfs_alloc_extent(tp, bno, len, f,
					len, 0, 0);
			}
			if (rbno == NULLFSBLOCK) {
				ragno = NULLAGNUMBER;
				ragbno = NULLAGBLOCK;
			} else {
				ragno = XFS_FSB_TO_AGNO(mp, rbno);
				ragbno = XFS_FSB_TO_AGBNO(mp, rbno);
			}
			if (!qflag) {
				if (isv)
					printf(
"%d: xfs_alloc_vextent(%lld[%d:%d],%d,%d,%c,%d,0,0,0,1) returned %lld[%d:%d],%d\n",
						i, bno,
						XFS_FSB_TO_AGNO(mp, bno),
						XFS_FSB_TO_AGBNO(mp, bno),
						minlen, maxlen, "astSnx"[f],
						maxlen, (xfs_dfsbno_t)rbno,
						ragno, ragbno, len);
				else
					printf(
	"%d: xfs_alloc_extent(%lld[%d:%d],%d,%c,%d,0,0) returned %lld[%d:%d]\n",
						i, bno,
						XFS_FSB_TO_AGNO(mp, bno),
						XFS_FSB_TO_AGBNO(mp, bno),
						len, "astSnx"[f], len,
						(xfs_dfsbno_t)rbno, ragno,
						ragbno);
			}
			if (rbno == NULLFSBLOCK)
				failures++;
			else
				add_alloced(rbno, len, 0);
		} else if (alist_nallocs) {
			if (aino != NULLFSINO) {
			} else {
				c = random() % alist_nallocs;
				get_alloced(c, &bno, &len, &rt, 1);
				xfs_trans_reserve(tp, 10, BBTOB(128), 0, 0);
				j = xfs_free_extent(tp, bno, len);
				if (!qflag)
					printf(
			"%d: xfs_free_extent(%lld[%d:%d],%d) returned %d\n",
						i, (xfs_dfsbno_t)bno,
						XFS_FSB_TO_AGNO(mp, bno),
						XFS_FSB_TO_AGBNO(mp, bno), len,
						j);
				if (!j) {
					add_alloced(bno, len, 0);
					failures++;
				}
			}
		}
		xfs_trans_commit(tp, 0);
	}
	done = 0;
	while (!done && fgets(line, sizeof(line), stdin)) {
		char ty;

		tp = xfs_trans_alloc(mp, 0);
		switch (line[0]) {
		case 'a':
			switch (line[1]) {
			case 'a':
				sscanf(&line[2], " %d", &len);
				bno = 0;
				f = XFS_ALLOCTYPE_ANY_AG;
				break;
			case 'n':
				sscanf(&line[2], " %lld %d", &dbno, &len);
				bno = dbno;
				f = XFS_ALLOCTYPE_NEAR_BNO;
				break;
			case 's':
				sscanf(&line[2], " %lld %d", &dbno, &len);
				bno = dbno;
				f = XFS_ALLOCTYPE_START_AG;
				break;
			case 'S':
				sscanf(&line[2], " %lld %d", &dbno, &len);
				bno = dbno;
				f = XFS_ALLOCTYPE_START_BNO;
				break;
			case 't':
				sscanf(&line[2], " %d %d", &agno, &len);
				bno = XFS_AGB_TO_FSB(mp, agno, 0);
				f = XFS_ALLOCTYPE_THIS_AG;
				break;
			case 'x':
				sscanf(&line[2], " %lld %d", &dbno, &len);
				bno = dbno;
				f = XFS_ALLOCTYPE_THIS_BNO;
				break;
			default:
				goto badcmd;
			}
			xfs_trans_reserve(tp, len + 10, BBTOB(128), 0, 0);
			rbno = xfs_alloc_extent(tp, bno, len, f, len, 0, 0);
			if (rbno == NULLFSBLOCK) {
				ragno = NULLAGNUMBER;
				ragbno = NULLAGBLOCK;
			} else {
				ragno = XFS_FSB_TO_AGNO(mp, rbno);
				ragbno = XFS_FSB_TO_AGBNO(mp, rbno);
			}
			if (!qflag) {
				printf(
	"xfs_alloc_extent(%lld[%d:%d],%d,%c,%d,0,0) returned %lld[%d:%d]\n",
					(xfs_dfsbno_t)bno,
					XFS_FSB_TO_AGNO(mp, bno),
					XFS_FSB_TO_AGBNO(mp, bno), len,
					"astSnx"[f], len, (xfs_dfsbno_t)rbno,
					ragno, ragbno);
			}
			if (rbno == NULLFSBLOCK)
				failures++;
			else
				add_alloced(rbno, len, 0);
			break;
		case 'b':
			/* b[rwd] ino bno nb nv */
			sscanf(&line[1], " %c %lld %lld %d %d", &rw, &ino,
				&dboff, &len, &nmap);
			boff = dboff;
			xfs_trans_reserve(tp, len + 10, BBTOB(128),
				si.rtdev ? len : 0, 0);
			ip = xfs_trans_iget(mp, tp, ino, XFS_ILOCK_EXCL);
			maps = malloc(nmap * sizeof(*maps));
			onmap = nmap;
			switch (rw) {
			case 'd':
				flags = XFS_BMAPI_WRITE | XFS_BMAPI_DELAY;
				break;
			case 'r':
				flags = 0;
				break;
			case 'w':
				flags = XFS_BMAPI_WRITE;
				break;
			default:
				goto badcmd;
			}
			XFS_BMAP_INIT(&flist, &firstblock);
			firstblock = xfs_bmapi(tp, ip, boff, len, flags,
				firstblock, len, maps, &onmap, &flist);
			printf(
		"xfs_bmapi(%lld[%d:%d],%lld,%d,%d,%s,%d,%d) returned %lld\n",
				ino, XFS_INO_TO_AGNO(mp, ino),
				XFS_INO_TO_AGINO(mp, ino), (xfs_dfiloff_t)boff,
				len, flags, "NULLFSBLOCK", len, nmap,
				(xfs_dfsbno_t)firstblock);
			for (i = 0; i < onmap; i++) {
				xfs_extlen_t c;
				xfs_fileoff_t o;
				char *p;
				xfs_fsblock_t s;

				o = maps[i].br_startoff;
				s = maps[i].br_startblock;
				c = maps[i].br_blockcount;
				p = xfs_print_startblock(mp, s);
				printf(
				"\t%d: offset=%lld, block=%s, count=%d\n",
					i, (xfs_dfiloff_t)o, p, c);
			}
			for (fp = flist.xbf_first; fp; fp = fp->xbfi_next) {
				char *p;

				p = xfs_print_startblock(mp,
					fp->xbfi_startblock);
				printf("\tflist: block=%s, count=%d\n",
					p, fp->xbfi_blockcount);
			}
			xfs_bmap_finish(&tp, &flist, firstblock, 0);
			free(maps);
			break;
		case 'f':
			sscanf(&line[1], " %lld %d", &dbno, &len);
			bno = dbno;
			xfs_trans_reserve(tp, 10, BBTOB(128), 0, 0);
			j = xfs_free_extent(tp, bno, len);
			if (!qflag)
				printf(
				"xfs_free_extent(%lld[%d:%d],%d) returned %d\n",
					(xfs_dfsbno_t)bno,
					XFS_FSB_TO_AGNO(mp, bno),
					XFS_FSB_TO_AGBNO(mp, bno), len, j);
			if (!j)
				failures++;
			break;
		case 'F':
			sscanf(&line[1], " %lld %d", &dbno, &len);
			bno = dbno;
			xfs_trans_reserve(tp, 10, BBTOB(128), 0, 0);
			xfs_rtfree_extent(tp, bno, len);
			if (!qflag)
				printf("xfs_rtfree_extent(%lld,%d)\n",
					(xfs_dfsbno_t)bno, len);
			break;
		case 'i':
			switch (line[1]) {
			case 'a':
				sscanf(&line[2], " %lld %c", &pino, &cmode);
				switch (cmode) {
				case 'b':
					mode = S_IFBLK;
					break;
				case 'c':
					mode = S_IFCHR;
					break;
				case 'd':
					mode = S_IFDIR;
					break;
				case 'f':
					mode = S_IFIFO;
					break;
				case 'l':
					mode = S_IFLNK;
					break;
				case 'n':
					mode = S_IFNAM;
					break;
				case 'r':
					mode = S_IFREG;
					break;
				case 's':
					mode = S_IFSOCK;
					break;
				default:
					goto badcmd;
				}
				xfs_trans_reserve(tp, 10, BBTOB(128), 0, 0);
				pip = xfs_trans_iget(mp, tp, pino,
					XFS_ILOCK_EXCL);
				pino = pip->i_ino;
				ialloc_context = NULL;
				call_again = 0;
				ip = xfs_ialloc(tp, pip, mode, 1, si.ddev,
					&crz, &ialloc_context, &call_again);
				if (call_again) {
					xfs_trans_bhold(tp, ialloc_context);
					xfs_trans_commit(tp, 0);
					tp = xfs_trans_alloc(mp, 0);
					xfs_trans_reserve(tp, 10, BBTOB(128),
							  0, 0);
					xfs_trans_bjoin (tp, ialloc_context);
					ip = xfs_ialloc(tp, pip, mode, 1, 
						si.ddev, &crz, &ialloc_context,
						&call_again);
				}
				ino = ip ? ip->i_ino : NULLFSINO;
				if (!qflag)
					printf(
			"xfs_ialloc(%lld[%d:%d],%x) returned %lld[%d:%d]\n",
						pino,
						XFS_INO_TO_AGNO(mp, pino), 
						XFS_INO_TO_AGINO(mp, pino),
						mode, ino,
						XFS_INO_TO_AGNO(mp, ino),
						XFS_INO_TO_AGINO(mp, ino));
				if (ino == NULLFSINO)
					failures++;
				break;
			case 'f':
				sscanf(&line[2], " %lld", &ino);
				xfs_trans_reserve(tp, 10, BBTOB(128), 0, 0);
				xfs_difree(tp, ino);
				if (!qflag)
					printf(
					"xfs_difree(%lld[%d:%d])\n",
						ino, XFS_INO_TO_AGNO(mp, ino),
						XFS_INO_TO_AGINO(mp, ino));
				break;
			case 's':
				sscanf(&line[2], " %lld", &aino);
				break;
			case 'S':
				aino = NULLFSINO;
				break;
			case 'x':
				sscanf(&line[2], " %lld %x", &ino, &c);
				xfs_trans_reserve(tp, 10, BBTOB(128), 0, 0);
				ip = xfs_trans_iget(mp, tp, ino,
					XFS_ILOCK_EXCL);
				ip->i_d.di_flags = c;
				xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
				break;
			default:
				goto badcmd;
			}
			break;
		case 'p':
			switch (line[1]) {
			case ' ':
			case '\n':
				xfs_print(mp);
				break;
			case 'a':
				printf("a = %d\n", allocpct);
				break;
			case 'b':
				sscanf(&line[2], " %d", &agno);
				xfs_print_bno(mp, agno);
				break;
			case 'c':
				sscanf(&line[2], " %d", &agno);
				xfs_print_cnt(mp, agno);
				break;
			case 'd':
				sscanf(&line[2], " %lld", &dbno);
				bno = dbno;
				xfs_print_block(mp, bno);
				break;
			case 'D':
				sscanf(&line[2], " %lld", &ino);
				ip = xfs_iget(mp, tp, ino, XFS_ILOCK_SHARED);
				xfs_dir_print(tp, ip);
				xfs_iput(ip, XFS_ILOCK_SHARED);
				break;
			case 'f':
				sscanf(&line[2], " %d", &agno);
				xfs_print_flist(mp, agno);
				break;
			case 'g':
				sscanf(&line[2], " %d", &agno);
				xfs_print_agf(mp, agno);
				xfs_print_agi(mp, agno);
				break;
			case 'I':
				sscanf(&line[2], " %lld", &ino);
				xfs_iflush_all(mp, 0);
				xfs_print_inode(mp, ino);
				break;
			case 'j':
				sscanf(&line[2], " %lld", &ino);
				ip = xfs_iget(mp, tp, ino, XFS_ILOCK_SHARED);
				xfs_iprint(ip);
				xfs_iput(ip, XFS_ILOCK_SHARED);
				break;
			case 'l':
				print_alist();
				break;
			case 'n':
				printf("n = %d\n", nearpct);
				break;
			case 'r':
				for (i = 0; i < nfactor; i++)
					printf("r = %d,%d,%d\n",
						factors[i].fact,
						factors[i].fmin,
						factors[i].fmax);
				break;
			case 'R':
				sscanf(&line[2], " %lld %d", &drtstart, &rtlen);
				rtstart = drtstart;
				xfs_rtprint_range(mp, tp, rtstart, rtlen);
				break;
			case 's':
				xfs_print_sb(mp);
				break;
			case 'S':
				printf("s = %d\n", startpct);
				break;
			case 't':
				printf("t = %d\n", agpct);
				break;
			case 'v':
				printf("a = %d\n", vallocpct);
				break;
			case 'x':
				printf("x = %d\n", exactpct);
				break;
			case 'X':
				xfs_rtprint_summary(mp, tp);
				break;
			case 'z':
				printf("failed ops = %d\n", failures);
				break;
			default:
				goto badcmd;
			}
			break;
		case 'q':
			done = 1;
			break;
		case 'R':
			nfactor = 0;
			init_tfact();
			break;
		case 'r':
			sscanf(&line[1], " %d %d %d", &fact, &fmin, &fmax);
			if (fact >= 1 && fmin >= 1 &&
			    fmax >= 1 && fmin <= fmax) {
				factors[nfactor].fact = fact;
				factors[nfactor].fmin = fmin;
				factors[nfactor].fmax = fmax;
				nfactor++;
				init_tfact();
			}
			break;
		case 's':
			sscanf(&line[1], " %c %d", &ty, &len);
			switch (ty) {
			case 'a':
				allocpct = (int)len;
				break;
			case 'n':
				nearpct = (int)len;
				break;
			case 's':
				startpct = (int)len;
				break;
			case 't':
				agpct = (int)len;
				break;
			case 'v':
				vallocpct = (int)len;
				break;
			case 'x':
				exactpct = (int)len;
				break;
			default:
				goto badcmd;
			}
			break;
		case 'S':
			bflush(si.ddev);
			break;
		case 't':
			sscanf(&line[1], " %d", &totalops);
			xfs_trans_cancel(tp, 0);
			goto rops;
		case 'u':
			/* u ino bno nb */
			sscanf(&line[1], " %lld %lld %d", &ino, &dboff, &len);
			boff = dboff;
			xfs_trans_reserve(tp, len + 10, BBTOB(128), 0, 0);
			ip = xfs_trans_iget(mp, tp, ino, XFS_ILOCK_EXCL);
			XFS_BMAP_INIT(&flist, &firstblock);
			firstblock = xfs_bunmapi(tp, ip, boff, len, 0,
				firstblock, &flist, &junk);
			printf(
		"xfs_bunmapi(%lld[%d:%d],%lld,%d,%d,%s) returned %lld,%d\n",
				ino, XFS_INO_TO_AGNO(mp, ino),
				XFS_INO_TO_AGINO(mp, ino), (xfs_dfiloff_t)boff,
				len, 0, "NULLFSBLOCK",
				(xfs_dfsbno_t)firstblock, junk);
			for (fp = flist.xbf_first; fp; fp = fp->xbfi_next) {
				char *p;

				p = xfs_print_startblock(mp,
					fp->xbfi_startblock);
				printf("\tflist: block=%s, count=%d\n",
					p, fp->xbfi_blockcount);
			}
			xfs_bmap_finish(&tp, &flist, firstblock, 0);
			break;
		case 'v':
			switch (line[1]) {
			case 'a':
				sscanf(&line[2], " %d %d %d %d", &minlen,
					&maxlen, &mod, &prod);
				bno = 0;
				f = XFS_ALLOCTYPE_ANY_AG;
				break;
			case 'n':
				sscanf(&line[2], " %lld %d %d %d %d", &dbno,
					&minlen, &maxlen, &mod, &prod);
				bno = dbno;
				f = XFS_ALLOCTYPE_NEAR_BNO;
				break;
			case 's':
				sscanf(&line[2], " %lld %d %d %d %d", &dbno,
					&minlen, &maxlen, &mod, &prod);
				bno = dbno;
				f = XFS_ALLOCTYPE_START_AG;
				break;
			case 'S':
				sscanf(&line[2], " %lld %d %d %d %d", &dbno,
					&minlen, &maxlen, &mod, &prod);
				bno = dbno;
				f = XFS_ALLOCTYPE_START_BNO;
				break;
			case 't':
				sscanf(&line[2], " %d %d %d %d %d", &agno,
					&minlen, &maxlen, &mod, &prod);
				bno = XFS_AGB_TO_FSB(mp, agno, 0);
				f = XFS_ALLOCTYPE_THIS_AG;
				break;
			case 'x':
				sscanf(&line[2], " %lld %d %d %d %d", &dbno,
					&minlen, &maxlen, &mod, &prod);
				bno = dbno;
				f = XFS_ALLOCTYPE_THIS_BNO;
				break;
			default:
				goto badcmd;
			}
			xfs_trans_reserve(tp, maxlen + 10, BBTOB(128), 0, 0);
			rbno = xfs_alloc_vextent(tp, bno, minlen, maxlen, &len,
				f, maxlen, 0, 0, mod, prod);
			if (rbno == NULLFSBLOCK) {
				ragno = NULLAGNUMBER;
				ragbno = NULLAGBLOCK;
			} else {
				ragno = XFS_FSB_TO_AGNO(mp, rbno);
				ragbno = XFS_FSB_TO_AGBNO(mp, rbno);
			}
			if (!qflag) {
				printf(
"xfs_alloc_vextent(%lld[%d:%d],%d,%d,%c,%d,0,0,%d,%d) returned %lld[%d:%d],%d\n",
					(xfs_dfsbno_t)bno,
					XFS_FSB_TO_AGNO(mp, bno),
					XFS_FSB_TO_AGBNO(mp, bno), minlen,
					maxlen, "astSnx"[f], maxlen, mod, prod,
					(xfs_dfsbno_t)rbno, ragno, ragbno, len);
			}
			if (rbno == NULLFSBLOCK)
				failures++;
			else
				add_alloced(rbno, len, 0);
			break;
		case 'V':
			switch (line[1]) {
			case 'a':
				sscanf(&line[2], " %d %d %d", &minlen, &maxlen,
					&prod);
				bno = 0;
				f = XFS_ALLOCTYPE_ANY_AG;
				break;
			case 'n':
				sscanf(&line[2], " %lld %d %d %d", &dbno,
					&minlen, &maxlen, &prod);
				bno = dbno;
				f = XFS_ALLOCTYPE_NEAR_BNO;
				break;
			case 'x':
				sscanf(&line[2], " %lld %d %d %d", &dbno,
					&minlen, &maxlen, &prod);
				bno = dbno;
				f = XFS_ALLOCTYPE_THIS_BNO;
				break;
			default:
				goto badcmd;
			}
			xfs_trans_reserve(tp, 10, BBTOB(128), maxlen, 0);
			rbno = xfs_rtallocate_extent(tp, bno, minlen, maxlen,
				&len, f, 0, prod);
			if (!qflag) {
				printf(
	"xfs_rtallocate_extent(%lld,%d,%d,%c,%d,0,%d) returned %lld,%d\n",
					(xfs_dfsbno_t)bno, minlen, maxlen,
					"a---nx"[f], maxlen, prod,
					(xfs_dfsbno_t)rbno, len);
			}
			if (rbno == NULLFSBLOCK)
				failures++;
			else
				add_alloced(rbno, len, 1);
			break;
		case '?':
			printf("commands:\n");
			printf("\ta{ansStx} [bno/agno] len\tallocate blocks\n");
			printf("\tb {rwd} ino bno nb nmap\tbmap blocks\n");
			printf("\tf bno len\tfree blocks\n");
			printf("\tF bno len\tfree rt blocks\n");
			printf("\tia pino mode\tallocate inode\n");
			printf("\tif ino\tfree inode\n");
			printf("\tis ino\tset inode for bmap\n");
			printf("\tiS\tclear inode for bmap\n");
			printf("\tix ino flagsval\tset di_flags\n");
			printf("\tp{ abcfgIlnrRstvxXz}\tprint information\n");
			printf("\tq\tquit\n");
			printf("\tR\tclear allocation factors\n");
			printf("\tr fact min max\tset allocation factors\n");
			printf("\ts{anstvx} val\tset control value\n");
			printf("\tS\tsync\n");
			printf("\tt ops\tdo automatic operations\n");
			printf("\tu ino bno nb\tbunmap blocks\n");
			printf(
"\tv{ansStx} [bno/agno] minlen maxlen mod prod\tvariable allocate blocks\n");
			printf(
	"\tV{atx} bno minlen maxlen prod\tvariable allocate rt blocks\n");
			printf("\t?\thelp message\n");
			break;
badcmd:
		default:
			fprintf(stderr, "unrecognized cmd %s\n", line);
			break;
		}
		xfs_trans_commit(tp, 0);
	}
	xfs_umount(mp);
	dev_close(si.ddev);
	exit(0);
}

void
init_tfact()
{
	int i, j, c;

	if (tfacts)
		free(tfacts);
	for (i = 0, tfact = 0; i < nfactor; i++)
		tfact += factors[i].fact;
	if (nfactor == 0)
		tfact = 1;
	tfacts = malloc(tfact * sizeof(struct mm));
	c = 0;
	for (i = 0; i < nfactor; i++) {
		for (j = 0; j < factors[i].fact; j++) {
			tfacts[c].fmin = factors[i].fmin;
			tfacts[c].fmax = factors[i].fmax;
			c++;
		}
	}
	if (nfactor == 0) {
		tfacts[c].fmin = 1;
		tfacts[c].fmax = (xfs_extlen_t)nblocks; 
	}
}

void
usage()
{
	fprintf(stderr, "usage: xfs_test [-fq] [-r|-s seed] [-v volume|filesys]\n");
	exit(1);
}
