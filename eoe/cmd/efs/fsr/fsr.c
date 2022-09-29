#ident "$Revision: 1.30 $"

/*
 * fsr - file system reorganizer
 *
 * fsr [-d] [-v] [-n] [-s] [-g] [-t mins] [-f leftf] [-m mtab]
 * fsr [-d] [-v] [-n] [-s] [-g] efsdev | dir | file ...
 * 
 * If invoked in the first form fsr does the following: starting with the
 * dev/inum specified in /etc/fsrlast this reorgs each reg file in each file
 * system found in /etc/mtab.  After 2 hours of this we record the current
 * dev/inum in /etc/fsrlast.  If there is no /etc/fsrlast fsr starts at the
 * top of /etc/mtab.
 *
 *	-g		print to syslog (default if stdout not a tty)
 *	-m mtab		use something other than /etc/mtab
 *	-t time		how long to run
 *	-f leftoff	use this instead of /etc/fsrlast
 *
 *	-v		verbose. more -v's more verbose
 *	-d		debug. print even more
 *	-n		do nothing.  only interesting with -v.  Not
 *			effective with in mtab mode.
 *	-s		print statistics only.
 *	-M		Pack metadata only.
 *	-b lbsize	Set the logical blocks size for re-org.
 *	-p passes	Number of passes before terminating global re-org.
 *
 * If invoked in the second form fsr reorganizes the specified
 * EFS file systems, directories, and files.
 *
 * In either form the -s option means to print statistics only.
 *
 */

#include "fsr.h"
#include <sys/wait.h>
#include <diskinfo.h>
#include <mntent.h>
#include <signal.h>
#include <getopt.h>

int vflag;
extern int gflag;
static int Mflag;
static int nflag;
int dflag = 1;
static int sflag;
extern int lbsize;
extern int max_ext_size;
static int pagesize;
static int npasses = 100;
static int packbigmoves;

static double slide_ratio = 0.01;

static time_t howlong = 7200;		/* default seconds of reorganizing */
static char *leftofffile = "/var/tmp/.fsrlast";/* where we left off last time */
static char *mtab = "/etc/mtab";
static time_t endtime;
static time_t starttime;

struct idata *idata_tbl;
static int icount;

#define ID_DIR 0x1
#define ID_CONTIG 0x2
#define ID_FIXED 0x4
#define ID_INRANGE 0x8
#define ID_INVALID 0x10

#define contig_inrange(idp) (((idp)->id_flag & (ID_INRANGE|ID_CONTIG)) \
				== (ID_INRANGE|ID_CONTIG))
struct idata {
	efs_daddr_t id_target;
	int id_flag;
	int id_ne;
	int id_nie;
	int id_nblocks;
	int id_iosave;
	extent *id_ex;
	extent *id_ix;
	efs_ino_t id_num;
};

static void packfile(struct fscarg *fap);
static void packbig(struct fscarg *fap);
static void moveinplace(struct fscarg *fap, efs_daddr_t tobn);

static int inorder(extent *ex, int ne);
static int multcg(extent *ex, int ne);
static int isfreebetween(dev_t dev,extent *ex, int ne);
static off_t iscontig(extent *ex, int ne, off_t offset);
static void extremebn(extent *ex, int ne, efs_daddr_t *lowbn,
	efs_daddr_t *highbn);

static void fsrfile(struct fscarg *fap);
static void fsrdir(char *dirname);
static void fsrfs(void);
static void fsrfilei(efs_ino_t ino);

static void initallfs(char *mtab);
static void fsrallfs(int howlong, char *leftofffile);

static void prstats(struct efsstats *es, char *fsname);
static void juststats(void);

static int get_fs_layout(void);
static int ilcmp_by_bn_and_contig(const void *i1, const void *i2);
static void order_files(efs_daddr_t minbn, efs_daddr_t length,
		struct idata **list, int count);
static int pack_file2(struct idata *idp, efs_daddr_t bbase,
	efs_daddr_t offset, int nblocks);
static void make_room_and_pack(efs_daddr_t bbase, struct idata *idp);
static void relocate(struct idata *idp,
		efs_daddr_t pbase, int nblocks, int iblks);
static efs_daddr_t findfreeholey(int *length, int maxlength,
		efs_daddr_t pbase, int nblocks);
static void update_inode(struct idata *idp, struct fscarg *fap);
static int iosave(time_t atime, time_t mtime, struct idata *idp);
static int efs_ilock(struct fscarg *fap);
static int ex_comp(struct idata *idp, struct fscarg *fap);
static void getstats(struct efsstats *es);
static void start_file_at_base(struct idata *idp, efs_daddr_t bbase);
static void free_layout(void);
#ifdef notdef
static void printinode(struct idata *idp);
#endif
static int file_in_place(struct idata *idp, efs_daddr_t bbase);
static int real_getiblocks(extent *ex, int ne);
static int contig_count(struct idata **list, int tindex, int count);
static void range_clear(int iext, efs_daddr_t bbase, efs_daddr_t nblocks,
			int mask, int result);
static efs_daddr_t inplace(struct idata *idp,
	efs_daddr_t offset, efs_daddr_t base);
static int efs_movex(struct idata *idp, efs_daddr_t bbase,
		efs_daddr_t offset, int count);
static int efs_moveix(struct idata *idp, efs_daddr_t base, int ni);
static void fsrall_cleanup(int timeout);
static void clr_idata(struct idata *idp);
static void add_alloc_space(int, extent *);
static void pack_mdata(void);
static void pack_fragments(void);
static void coalesce_free_space(void);
static int in_first_frags(struct idata *idp, int frag_cnt);
static efs_daddr_t free_search(int length, efs_daddr_t pbase, int nblocks);
static void make_contig(struct fscarg *fap);
static int align_extents(struct fscarg *fap, int x, int level);

static void fe_setmin(int min);

/* we only relocate files with data */
#define relocatable(di) ((S_ISREG((di)->di_mode) \
			 || S_ISLNK((di)->di_mode) \
			 || S_ISDIR((di)->di_mode)) \
			    && (di)->di_numextents > 0)
#define getiblocks(ex, ne) ( !(ex) ? 0 : real_getiblocks((ex), (ne)) )
/* figure out where the first block of the file is */
#define firstbn(idp) (efs_daddr_t)((idp)->id_ex->ex_bn)
#define lastbn(idp) (efs_daddr_t)((idp)->id_ex->ex_bn \
		+ getnblocks((idp)->id_ex, (idp)->id_ne) - 1)

#define ICOMMIT_RETRY 5

static struct efs_mount *mp;

void
aborter(void)
{
	exit(1);
}

int
main(int argc, char **argv)
{
	struct stat sb, sb2;
	char *argname;
	int c;
	int rw;
	char *cp;

	setlinebuf(stdout);

	gflag = ! isatty(0);

	sigset(SIGABRT, aborter);

	while ((c = getopt(argc, argv, "p:e:MgsdnvTt:f:m:b:N:F")) != -1 )
		switch (c) {
		case 'M':
			Mflag = 1;
			break;
		case 'g':
			gflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'v':
			++vflag;
			break;
		case 'd':
			dflag = 1;
			break;
		case 's':		/* frag stats only */
			sflag = 1;
			break;
		case 't':
			howlong = atoi(optarg);
			break;
		case 'f':
			leftofffile = optarg;
			break;
		case 'm':
			mtab = optarg;
			break;
		case 'b':
			lbsize = atoi(optarg);
			break;
		case 'p':
			npasses = atoi(optarg);
			break;
		default:
			fprintf(stderr,
			  "usage: %s [-snvg] [-t mins] [-f leftf] [-m mtab]\n",
			  argv[0]);
			fprintf(stderr,
			  "usage: %s [-snvg] [efsdev | dir | file] ...\n",
			  argv[0]);
			exit(1);
		}
	if (vflag)
		setbuf(stdout, NULL);

	/* init the efs "lib" */
	efs_init(fsrprintf);

	if (nflag)
		rw = O_RDONLY;
	else
		rw = O_RDWR;

	starttime = time(0);

	pagesize = sysconf(_SC_PAGESIZE) / BBSIZE;

	if (optind < argc) {
		/* init the pseudo-driver */
		if (fsc_init(fsrprintf, dflag, vflag)) {
			fprintf(stderr, "%s: open(/dev/fsctl): %s\n", argv[0],
				strerror(errno));
			exit(1);
		}
		/* TODO look through all names before leaping into any... */
		for (; optind < argc; optind++) {
			argname = argv[optind];
			if (lstat(argname, &sb) < 0) {
				fprintf(stderr, "%s: lstat(%s) failed\n",
					argv[0], argname);
				continue;
			}
			if (S_ISLNK(sb.st_mode) && stat(argname, &sb2) == 0 &&
			    (S_ISBLK(sb2.st_mode) || S_ISCHR(sb2.st_mode)))
				sb = sb2;
			if (S_ISBLK(sb.st_mode)) {
				cp = devnm(sb.st_rdev);
				if (cp == NULL || stat(cp, &sb2) < 0 || 
				    !S_ISCHR(sb2.st_mode)) {
					fprintf(stderr, "%s: no char dev???\n",
						argname);
					continue;
				}
				sb = sb2;
				argname = cp;
			}
			if (S_ISCHR(sb.st_mode)) {
				mp = efs_mount(argname, rw);
				if (mp == 0)
					exit(1);
				if (sflag) {
					struct efsstats es;
					fspieces(mp, &es);
					prstats(&es, argname);
				} else
					fsrfs();
				efs_umount(mp);
			} else if (S_ISDIR(sb.st_mode) || S_ISREG(sb.st_mode)) {
				if (sflag) {
					printf(
					"%s: sorry, stats only for char dev\n",
						argname);
				} else {
					cp = devnm(sb.st_dev);
					if (cp == NULL) {
						fprintf(stderr,
							"%s: no char dev???\n",
							argname);
						continue;
					}
					mp = efs_mount(cp, rw);
					if (mp == 0)
						exit(1);
					if (S_ISDIR(sb.st_mode))
						fsrdir(argname);
					else
						fsrfilei(sb.st_ino);
					efs_umount(mp);
				}
			} else {
				printf(
			"%s: not fsys dev, dir, or reg file, ignoring\n",
					argname);
			}
		}
	} else {
		initallfs(mtab);
		if (sflag) 
			juststats();
		else
			fsrallfs(howlong, leftofffile);
	}
	return 0;
}

#define NMOUNT 64
static int numfs;
static char *fsvb[NMOUNT];	/* block dev name */
static char *fsvc[NMOUNT];	/* char dev name */
static struct efsstats before[NMOUNT];
static int npass[NMOUNT];

/*
 * initallfs -- read the mount table and set up an internal form
 */
static void
initallfs(char *mtab)
{
	FILE *fp;
	struct mntent *mp;
	int mi;
	char *cp;
	struct stat sb;

	fp = setmntent(mtab, "r");
	if (fp == NULL) {
		fsrprintf("setmntent(%s) failed\n", mtab);
		exit(1);
	}
	/* find all rw efs file systems whose raw matches fsname */
	for (mi = 0; (mp=getmntent(fp)) != NULL && mi < NMOUNT; ) {
		int rw = 0;
		char *raw = NULL;
		struct stat sbr;

		if (strcmp(mp->mnt_type, "efs") != 0 ||
		    stat(mp->mnt_fsname, &sb) == -1 ||
		    !S_ISBLK(sb.st_mode))
			continue;

		cp = strtok(mp->mnt_opts,",");
		do {
			if (strcmp("rw", cp) == 0)
				rw++;
		} while ((cp = strtok(NULL, ",")) != NULL);
		if (rw == 0) {
			if (vflag)
				fsrprintf("%s not mounted rw, skipping...\n",
					mp->mnt_fsname);
			continue;
		}
		raw = findrawpath(mp->mnt_fsname);
		if (raw == NULL) {
			if (vflag)
				fsrprintf("%s no raw device found, skipping...\n",
					  mp->mnt_fsname);
			continue;
		}

		if (stat(raw, &sbr) == -1) {
			if (vflag)
				fsrprintf(
				"%s: stat(%s) failed: %s. skipping...\n",
				mp->mnt_fsname, raw, strerror(errno));
			continue;
		}

		if (S_ISCHR(sbr.st_mode)) {
			/*
			 * Used to check that the dev_t matched, but that
			 * doesn't work any more.  Just have to trust 
			 * findrawpath.
			 */
			fsvb[mi] = strdup(mp->mnt_fsname);
			fsvc[mi] = strdup(raw);
			if (fsvb[mi] == NULL || fsvc[mi] == NULL) {
				fsrprintf("strdup(%s) failed\n",mp->mnt_fsname);
				exit(1);
			}
			if (vflag)
		 		fsrprintf("%s raw=%s\n", fsvb[mi], fsvc[mi]);
			mi++;
		} else {
			if (vflag)
				fsrprintf("%s bad raw device, skipping...\n",
					mp->mnt_fsname);
		}
	}
	numfs = mi;
	endmntent(fp);
	if (numfs == 0) {
		fsrprintf("no rw efs file systems in mtab\n");
		exit(0);
	}
}

/*
 * getstats -- Collect stats for all file systems
 */
static void
getstats(struct efsstats *es)
{
	int mi;

	for (mi = 0; mi < numfs; mi++) {
		mp = efs_mount(fsvc[mi], O_RDONLY);
		if (mp == 0)
			exit(1);
		fspieces(mp, &es[mi]);
		efs_umount(mp);
	}
}

/*
 * juststats -- don't re-org, just print stats
 */
static void
juststats(void)
{
	int mi;

	getstats(before);

	for (mi = 0; mi < numfs; mi++)
		prstats(&before[mi], fsvb[mi]);
}

/*
 * prstats -- print stats contained in "es" for "fsname"
 */
static void
prstats(struct efsstats *es, char *fsname)
{
	static int printedheader;

	if (!printedheader) {
		fsrprintf(" %%frag  %%free  fs\n");
		printedheader = 1;
	}
	fsrprintf("%6.2f %6.2f  %s\n",
		es->tbb == 0 ? 0 :
		(float)es->tpieces * 100/(float)es->tbb,
		es->tfreebb == 0 ? 0 :
		(float)es->tholes * 100 / (float)es->tfreebb,
		fsname);
}

static int fsrall_mi;

/*
 * fsrallfs -- reorg everything for "howlong" secs, starting with the
 *		contents of leftofffile.
 */
static void
fsrallfs(int howlong, char *leftofffile)
{
	int startdev;
	int fd;
	char *fsname;	/* enhance readibility */
#define	LITTLEBUF 64
	char buf[LITTLEBUF];
	int mdonly = Mflag;
	
	fsrprintf(" -m %s -t %d -f %s ...\n", mtab, howlong, leftofffile);

	endtime = starttime + howlong;

	/* where'd we leave off last time? */
	fd = open(leftofffile, O_RDONLY);
	if (fd == -1) {
		if (errno != ENOENT)
			fsrprintf(
			    "open(%s, O_RDONLY) failed: %s, starting with %s\n",
			    leftofffile, strerror(errno), *fsvb);
		startdev = 0;
	} else {
		if (read(fd, buf, LITTLEBUF) == -1) {
			fsrprintf("could not read %s, starting with %s\n",
				leftofffile, *fsvb);
			startdev = 0;
		} else {
			for (startdev = 0; startdev < numfs; startdev++) {
				fsname = fsvb[startdev];
				if (strncmp(buf,fsname,strlen(fsname)) == 0
				    && buf[strlen(fsname)] == ' ')
					break;
			}
			if (startdev == numfs)
				startdev = 0;
		}
		close(fd);
	}

	/* reorg for 'howlong' -- checked in 'fsrfs' */
	for (fsrall_mi = startdev; endtime > time(0); fsrall_mi++) {
		pid_t pid;
		if (fsrall_mi == numfs)
			fsrall_mi = 0;
		if (npass[fsrall_mi] == npasses)
			break;
		if (npasses > 1 && !npass[fsrall_mi])
			Mflag = 1;
		else
			Mflag = mdonly;
		pid = fork();
		switch(pid) {
		case -1:
			fsrprintf("couldn't fork sub process:");
			exit(1);
			break;
		case 0:
			/* init the pseudo-driver */
			if (fsc_init(fsrprintf, dflag, vflag)) {
				fsrprintf("open(/dev/fsctl): %s\n",
						strerror(errno));
				exit(1);
			}
			fsname = fsvc[fsrall_mi];		/* raw dev */
			mp = efs_mount(fsname, O_RDWR);
			if (mp == 0)
				exit(1);
			fsrfs();
			efs_umount(mp);
			exit(0);
			break;
		default:
		    {
			pid_t ret_pid;
			int status;

			for (ret_pid = waitpid(pid, &status, 0);
			     ret_pid <= 0;
			     ret_pid = waitpid(pid, &status, 0))
			{
				if (ret_pid == -1 && errno != EINTR)
					break;
			}
			break;
		    }
		}
		npass[fsrall_mi]++;
	}
	fsrall_cleanup(endtime <= time(0));
}

/*
 * fsrall_cleanup -- close files, print next starting location, etc.
 */
static void
fsrall_cleanup(int timeout)
{
	int fd;
	char buf[LITTLEBUF];

	fsc_close();	/* release /dev/fsctl */

	/* record where we left off */
	fd = open(leftofffile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd == -1)
		fsrprintf("open(%s) failed: %s\n", leftofffile,strerror(errno));
	else {
		if (timeout) {
			sprintf(buf, "%s %d\n", fsvb[fsrall_mi], 0);
			if (write(fd, buf, strlen(buf)) == -1)
				fsrprintf("write(%s) failed: %s\n", leftofffile,
						strerror(errno));
			close(fd);
		}
	}

	if (timeout)
		fsrprintf("fsr completed %d passes in %d seconds\n",
			npass[fsrall_mi], time(0) - endtime + howlong);

}

/*
 * fsrfs -- reorganize a file system
 */
static void
fsrfs(void)
{
	if (vflag)
		fsrprintf("fsrfs [%s] 0x%x\n", mp->m_devname, mp->m_dev);

	if (get_fs_layout())
		return;
	pack_mdata();
	if (!Mflag) {
		if (endtime)
			packbigmoves = 300;
		pack_fragments();
		coalesce_free_space();
	}
	free_layout();
}

/*
 * coalesce_free_space -- pull together scattered free space
 *
 *	First, we scan the file system looking for potential extents
 *		 with lots of scattered free space.
 *	Then, we determine how many extents we would have to move to
 *		compress the space.
 *	Then, we move the most profitable extents.
 */
static void
coalesce_free_space(void)
{
	struct idata *idp, *idpmvs, *idpmax;
	efs_daddr_t total;
	int frag_tot;
	int frag_num;
	int icountsv;

	/*
	 * Find the fragmentation, and the fragmenting extents.
	 */
	get_frags(mp->m_dev, mp->m_fs->fs_ncg, EFS_CGIMIN(mp->m_fs, 0),
			mp->m_fs->fs_cgfsize, mp->m_fs->fs_cgisize);

	for (idp = idata_tbl, idpmax = &idata_tbl[icount];
	     idp < idpmax;
	     idp++)
	{
		add_alloc_space(idp->id_ne, idp->id_ex);
		if (idp->id_nie)
			add_alloc_space(idp->id_nie, idp->id_ix);
	}

	/*
	 * Find the most profitable extents to clear, and skim the cream
	 */
	total = total_frag_free();

	/* This will never increase total, unless total is < 30000 */
	total = 30000 * (float)total / (total + 30000);

	sort_frags_by_value();
	frag_tot = count_frags(total);

	if (frag_tot == 0)
		return;

	/*
	 * If we're running against a time limit, do a little each pass
	 */
	if (endtime && frag_tot > 300)
		frag_tot = 300;

	/*
	 * Re-sort the frags for faster searching
	 */
	sort_frags_by_location(frag_tot);

	/*
	 * Determine which inodes intersect the selected frags, to cut
	 * down future search time.
	 */
	for (idpmvs = idata_tbl; in_first_frags(idpmvs, frag_tot); idpmvs++)
		;

	for (idp = idpmvs + 1; idp < idpmax; idp++)
	{
		if (in_first_frags(idp, frag_tot)) {
			struct idata tmp;

			tmp = *idp;
			*idp = *idpmvs;
			*idpmvs++ = tmp;
		}
	}

	icountsv = icount;
	icount = idpmvs - idata_tbl;

	/*
	 * clear the best frags
	 */
	for (frag_num = 0; frag_num < frag_tot; frag_num++) {
		efs_daddr_t start, length, nfree;
		get_frag_vals(frag_num, &start, &nfree, &length);
		if (start < 0)
			continue;
		if (fsc_tstfree(mp->m_dev, start) <= 0)
			continue;
		range_clear(0, start, length, ID_DIR, ID_DIR);
	}
	icount = icountsv;
	/*
	 * Clean up
	 */
	free_frag_structures();
}

/*
 * get_fs_layout -- Scan the file system and collect data regarding the disk
 * layout
 */
static int
get_fs_layout(void)
{
	int ncg = mp->m_fs->fs_ncg;
	struct fscarg fa;
	struct efs_dinode *di, *inos;
	efs_ino_t imaxcnt;

	efs_ino_t ino;
	efs_ino_t icylcnt;
	int cyl;

	/* allocate arrays for each of the per cylinder group structures */

	icylcnt = 0;
	idata_tbl = 0;
	ino = 0;
	fa.fa_dev = mp->m_dev;
 
	imaxcnt = mp->m_fs->fs_ncg * mp->m_fs->fs_cgisize * EFS_INOPBB
						- mp->m_fs->fs_tinode + 1000;
	/* Allocate enough data space for all inodes */
	idata_tbl = malloc(imaxcnt * sizeof(idata_tbl[0]));
	if (!idata_tbl)
		return errno ? errno : EINVAL;

	for (cyl = 0; cyl < ncg; cyl++) {
		/* read in the inodes for a cylinder group.
		 * for each relocation candidate, extract the relevant
		 * information and add it to the cylinder group's list 
		 */
		inos = efs_igetcg(mp, cyl);
		if (inos == 0)
			return -1;

		for (di = inos, icylcnt = EFS_INOPBB * mp->m_fs->fs_cgisize;
		     icylcnt--;
		     ino++, di++) {
			struct idata *idp;

			fa.fa_ino = ino;
			/* NEEDSWORK: indirs can change, causing getex
			 * to fail */
			if (!relocatable(di) || !getex(mp, di, &fa, 0))
				continue;

			if (icount >= imaxcnt) {
				/* Oops, need more room */
				imaxcnt += 100;
				idata_tbl = realloc(idata_tbl,
					imaxcnt * sizeof(idata_tbl[0]));
				if (!idata_tbl) {
					free(inos);
					return errno ? errno : EINVAL;
				}
			}
			idp = &idata_tbl[icount];
			idp->id_flag = 0;
			idp->id_num = ino;
			idp->id_nblocks = getnblocks(fa.fa_ex, fa.fa_ne);
			if (iscontig(fa.fa_ex, fa.fa_ne, 0) == idp->id_nblocks)
				idp->id_flag |= ID_CONTIG;
			if (S_ISDIR(di->di_mode))
				idp->id_flag |= ID_DIR;
			idp->id_ne = fa.fa_ne;
			idp->id_nie = fa.fa_nie;
			idp->id_ex = fa.fa_ex;
			idp->id_ix = fa.fa_ix;
			idp->id_iosave = iosave(di->di_atime,
						di->di_mtime, idp);
			idp->id_target = 0;

			icount++;
		}
		free(inos);
	}
	/* trim the idata table to the actual size of the array */
	idata_tbl = realloc(idata_tbl, (icount + 1) * sizeof(idata_tbl[0]));
	if (!idata_tbl)
		return errno ? errno : EINVAL;

	return 0;
}

/*
 * free_layout -- Free malloc'd data structures.
 */
static void
free_layout(void)
{
	int i;

	for (i = 0; i < icount; i++) {
		if (idata_tbl[i].id_ex) {
			free(idata_tbl[i].id_ex);
			idata_tbl[i].id_ex = 0;
		}
		if (idata_tbl[i].id_ix) {
			free(idata_tbl[i].id_ix);
			idata_tbl[i].id_ix = 0;
		}
	}
	icount = 0;
	free(idata_tbl);
	idata_tbl = 0;
	fe_clear();
}

/*
 * pack_mdata -- pack metadata tightly into the beginning of its cylinders
 *
 *	In each cg, we
 *		1) Count the metadata.
 *		2) Determine an optimal packing for the metadata in the cg.
 *		3) Move any conflicting files out of the way.
 *		4) Copy in the directories
 *		5) Fill the gaps with indirect extents.
 */
static void
pack_mdata(void)
{
	int cg;
	struct idata *idp = idata_tbl;
	struct idata **idirdata
		= malloc(EFS_INOPBB * mp->m_fs->fs_cgisize * sizeof(*idirdata));

	if (!idirdata) {
		perror("Couldn't allocate space for directories");
		return;
	}
	for (cg = 0; cg < mp->m_fs->fs_ncg; cg++) {
		struct idata *mdip = idp;
		struct idata **idpp, **idpmax;
		efs_daddr_t dirblks = 0;
		efs_daddr_t mdblks = 0;
		efs_ino_t dirs = 0;
		int cleared = 0;
		efs_daddr_t b0 = EFS_CGIMIN(mp->m_fs, cg) + mp->m_fs->fs_cgisize;

		/*
		 * Count the metadata -- dirs and indirs
		 */
		while (idp < &idata_tbl[icount]
		       && EFS_ITOCG(mp->m_fs, idp->id_num) == cg)
		{
			mdblks += getiblocks(idp->id_ix, idp->id_nie);
			if (idp->id_flag & ID_DIR) {
				idirdata[dirs++] = idp;
				dirblks += idp->id_nblocks;
			}
			idp++;
		}

		/*
		 * Determine the optimal order
		 */
		if (dirs != 0) {
			order_files(b0, dirblks, idirdata, dirs);
			dirblks = idirdata[dirs - 1]->id_target
					+ idirdata[dirs - 1]->id_nblocks - b0;
		}

		/*
		 * Move dirs in place, sweeping all before them
		 */
		for (idpp = idirdata, idpmax = &idirdata[dirs];
		     idpp < idpmax;
		     idpp++)
		{
			if (file_in_place(*idpp, (*idpp)->id_target))
				continue;
			if (!cleared)
				range_clear(0, b0, dirblks,
					ID_INRANGE|ID_CONTIG|ID_DIR,
					ID_INRANGE|ID_CONTIG|ID_DIR);
			cleared = 1;
			start_file_at_base(*idpp, (*idpp)->id_target);
		}

		/*
		 * Clear room for indirs
		 */
		if (mdblks > 0)
			range_clear(0, b0 + dirblks, mdblks, 0, 1);
		mdblks += dirblks + 1;

		/*
		 * Squeeze indirs into the gaps
		 */
		while (mdip < idp) {
			int ie;

			for (ie = 0; ie < mdip->id_nie; ie++) {
				efs_daddr_t ixbase = mdip->id_ix[ie].ex_bn;
				efs_daddr_t ixlength = mdip->id_ix[ie].ex_length;
				if (ixbase < b0
				    || ixbase + ixlength > mdblks + b0)
				{
					efs_daddr_t newbn;
					int nfree;

					if (vflag)
						fsrprintf(
						"pack_ix: %d %d(%d) %d(%d)\n",
						mdip->id_num,
						ixbase, ixlength,
						b0, mdblks);
					newbn = freebetween(mp->m_dev, b0,
					      b0 + mdblks - 1, ixlength,
					      0, &nfree);
					if (nfree >= ixlength)
					      (void)efs_moveix(mdip, newbn, ie);
				}
			}
			mdip++;
		}
	}
	free(idirdata);
}

/*
 * idcmp_by_iosave -- qsort compare routine that sorts by expected
 *		return from repacking
 */
static int
idcmp_by_iosave(const void *i1, const void *i2)
{
	return ((struct idata *)i2)->id_iosave
		  - ((struct idata *)i1)->id_iosave;
}

/*
 * ilock_and_pack_big -- lock a file, and if static, repack it.
 */
static void
ilock_and_pack_big(struct idata *idp)
{
	struct fscarg fa;

	fa.fa_dev = mp->m_dev;
	fa.fa_ino = idp->id_num;

	if (!efs_ilock(&fa)) {
		clr_idata(idp);
		return;
	}

	if (idp->id_nblocks != getnblocks(fa.fa_ex, fa.fa_ne)
	    || idp->id_ne != fa.fa_ne
	    || idp->id_nie != fa.fa_nie
	    || !ex_comp(idp, &fa)) {
		if (vflag)
			fsrprintf("file moved: %d\n", idp->id_num);
		clr_idata(idp);
		fsc_iunlock(&fa);
		return;
	}

	packbig(&fa);
	update_inode(idp, &fa);
	fsc_iunlock(&fa);
}

/*
 * pack_fragments -- find the most profitable fragments and repack them
 */
static void
pack_fragments(void)
{
	struct idata *idp;
	struct idata *idppos;
	struct idata *idpmax;
	int maxfree;
	int ipack;

	/*
	 * start by moving potential candidates to the head of the list
	 * This step isn't strictly necessary, but qsort doesn't handle
	 * arrays with lots of equal members very well.
	 */
	idpmax = &idata_tbl[icount];

	for (idp = idata_tbl; idp < idpmax; idp++)
		if (idp->id_iosave <= 0)
			break;
	for (idppos = idp + 1; idppos < idpmax; idppos++)
	{
		struct idata idptmp;
		if (idppos->id_iosave <= 0)
			continue;
		idptmp = *idppos;
		*idppos = *idp;
		*idp++ = idptmp;
	}

	/*
	 * Now that we have located potential candidates, sort them in
	 * order of importance.
	 */
	ipack = idp - idata_tbl;
	qsort(idata_tbl, ipack, sizeof(*idata_tbl), idcmp_by_iosave);

	/* add a sentinel, because they're fun */
	idata_tbl[icount].id_iosave = 0;

	/*
	 * Start by trying to find contiguous homes for all the
	 * candidates
	 */
	maxfree = mp->m_fs->fs_cgfsize - mp->m_fs->fs_cgisize;
	for (idp = idata_tbl; idp < &idata_tbl[ipack]; idp++) {
		efs_daddr_t newbn;
		int length = idp->id_nblocks;

		if (maxfree < length)
			continue;
		newbn = findfreeholey(&length, length, 0, 0);
		if (newbn == -1)
			maxfree = length - 1;
		else {
			efs_movex(idp, newbn, 0, length);
			idp->id_iosave = 0;
			if (packbigmoves) {
				if (packbigmoves == 1)
					return;
				else {
					int dec = (length + max_ext_size - 1)
							/ max_ext_size;
					if (packbigmoves > dec)
						packbigmoves -= dec;
					else
						packbigmoves = 1;
				}
			}
		}
	}

	/*
	 * Re-sort to get rid of the files packed in the last step.
	 */
	qsort(idata_tbl, ipack, sizeof(*idata_tbl), idcmp_by_iosave);
	/*
	 * Reduce fragmentation where possible.  This step will
	 * cease to move things when the free space is too fragmented
	 * to improve things further.
	 */
	for (idp = idata_tbl; packbigmoves != 1 && idp->id_iosave > 0; idp++)
		ilock_and_pack_big(idp);
}

/*
 * iosave -- Estimate the amount to be saved by reorg'ing the file.
 * Assume files are read sequentially, and that access and mod time
 * estimate the read/write ratio.
 */
static int
iosave(time_t atime, time_t mtime, struct idata *idp)
{
	int io_after_pack;
	int io_to_read = 0;
	int nreads;
	int x;

	for (x = 0; x < idp->id_ne; x++) {
		int offset = idp->id_ex[x].ex_offset;

		/*
		 * Alignment problems increase I/O
		 */
		io_to_read++;
		if (offset & (pagesize - 1))
			io_to_read++;
		if (offset & (lbsize - 1))
			io_to_read++;
		if (offset % max_ext_size)
			io_to_read++;
		if (x < idp->id_ne - 1
		    && idp->id_ex[x].ex_length + idp->id_ex[x].ex_bn
						!= idp->id_ex[x+1].ex_bn)
			io_to_read++;
	}

	if (io_to_read == idp->id_ne)
		return 0;

	/*
	 * we need estimates of current behavior, behavior when packed,
	 * and the cost of packing.
	 */
	io_after_pack = (idp->id_nblocks + max_ext_size - 1) / max_ext_size;

	if (atime >= starttime) {
		nreads = atime - mtime;
	} else {
		nreads = (mtime - starttime) / (atime - starttime);
	}
	return nreads * (io_to_read - io_after_pack)
		- (io_to_read + io_after_pack * 2);
}

/*
 * ilcmp_by_bn_and_contig -- qsort compare that orders files for tight packing.
 *
 *		If files are contiguous, and in the desired range, they
 *		 sort first.
 *		CI files are sorted by block order, since we'll try not
 *			to move them.
 *		non-CI files are sorted by size, so we fill gaps with
 *			large files first.
 */
static int
ilcmp_by_bn_and_contig(const void *i1, const void *i2)
{
	struct idata *idp1 = *(struct idata **)i1;
	struct idata *idp2 = *(struct idata **)i2;
	int ci1 = contig_inrange(idp1);
	int ci2 = contig_inrange(idp2);
	int nb1, nb2;

	/* put contiguous files in the list first, and try to fit others
	 * around them.
	 */
	if (ci1 != ci2)
		return ci2 - ci1;
	
	if (ci1)
		return idp1->id_ex->ex_bn - idp2->id_ex->ex_bn;

	nb1 = idp1->id_nblocks;
	nb2 = idp2->id_nblocks;
	if (nb1 != nb2)
		return nb1 - nb2;

	return idp1->id_num - idp2->id_num;
}

#ifdef notdef
/*
 * This data structure and the following routines implement a binary
 * tree to find idata structures quickly.  This may not be necessary with
 * the current high-level algorithms used by fsr, but avoids quadratic
 * behavior during tight packing.
 */
struct itree {
	struct idata **idp;
	struct itree *left;
	struct itree *right;
	struct itree *parent;
};

static struct itree *itpfree, *itproot, *itpbase;

/*
 * make_itree_sub -- a recursive procedure for make_itree that assumes
 *		the tree space has already been allocated.
 */
static struct itree *
make_itree_sub(struct idata **list, int first, int last, struct itree *parent)
{
	int middle = (first + last) / 2;
	struct itree *ihere = itpfree++;
	ihere->parent = parent;
	ihere->idp = &list[middle];
	if (first < middle)
		ihere->left  = make_itree_sub(list, first, middle - 1, ihere);
	if (middle < last)
		ihere->right = make_itree_sub(list, middle + 1, last, ihere);
	return ihere;
}

/*
 * make_itree -- entry point for allocating and filling tree
 */
static void
make_itree(struct idata **list, int first, int last)
{
	if (first > last)
		return;
	itpbase = itpfree = calloc(last - first + 1, sizeof(*itpfree));
	itproot = make_itree_sub(list, first, last, 0);
}

/*
 * free_itree -- I suppose this really needs an explanation, but I don't
 *		feel like writing one.
 */
static void
free_itree(void)
{
	free(itpbase);
	itproot = itpfree = itpbase = 0;
}

/*
 * get_less_than_or_equal -- Find the largest node with a number of
 *		blocks less than or equal to val.
 */
static struct itree *
get_less_than_or_equal(struct itree *itp, int val)
{
	efs_daddr_t bn;
	struct itree *right;

	if (!itp)
		return NULL;
	bn = itp->idp[0]->id_nblocks;
	if (bn == val)
		return itp;
	if (bn > val)
		return get_less_than_or_equal(itp->left, val);
	right = get_less_than_or_equal(itp->right, val);
	if (right)
		return right;
	return itp;
}

/*
 * delete_itree -- remove a node from the tree
 */
static void
delete_itree(struct itree *dnode)
{
	struct itree *del_node;
	struct itree *orphan;

	/* if the node has only one child, delete the node.  Otherwise
	 * delete its successor, and copy the successor's key into the
	 * node.
	 */
	if (!dnode->left || !dnode->right)
		del_node = dnode;
	else {
		/* find the successor */
		for (del_node = dnode->right;
		     del_node->left;
		     del_node = del_node->left)
		     /* no body home */;
	}

	/* the node to be deleted has at most one child, keep track of it */
	orphan = del_node->left ? del_node->left : del_node->right;

	/* if there's a child, give it a new parent */
	if (orphan)
		orphan->parent = del_node->parent;

	/* if we're deleting the root, point the root at the child */
	if (!del_node->parent)
		itproot = orphan;

	/* otherwise connect the orphan where the deleted node used to be */
	else if (del_node == del_node->parent->left)
		del_node->parent->left = orphan;
	else
		del_node->parent->right = orphan;

	/* if the node deleted wasn't the node passed in, copy the
	 * deleted node's key */
	if (dnode != del_node)
		dnode->idp = del_node->idp;
}


/*
 * fill_gap -- find a file to fill the gap.  This assumes that an itree
 *		has been set up by previous calls.
 */
static int
fill_gap(struct idata **list, efs_daddr_t gap)
{
	struct itree *found;
	int index;

	found = get_less_than_or_equal(itproot, gap);
	if (!found)
		return -1;
	index = found->idp - list;
	delete_itree(found);
	return index;
}

#endif

/*
 * fill_gap -- find a file to fill the gap.
 */
static int
fill_gap_sorted(struct idata **list, efs_daddr_t gap, int first, int last)
{
	efs_daddr_t minimax = 0;
	int index = -1;
	int i;

	for (i = first; i <= last; i++) {
		if (!list[i])
			continue;
		if (list[i]->id_nblocks > gap)
			break;
		if (list[i]->id_nblocks >= minimax) {
			minimax = list[i]->id_nblocks;
			index = i;
		}
	}
	return index;
}

/*
 * fill_gap -- find a file to fill the gap.
 */
static int
fill_gap(struct idata **list, efs_daddr_t gap, int first, int last)
{
	efs_daddr_t minimax = 0;
	int index = -1;
	int i;

	if (last - first > 1000)
		last = first + 1000;

	for (i = first; i <= last; i++) {
		if (!list[i])
			continue;
		if (list[i]->id_nblocks < minimax || list[i]->id_nblocks > gap)
			continue;
		minimax = list[i]->id_nblocks;
		index = i;
	}
	return index;
}

/*
 * order_files -- Given a list of files to pack and a starting point,
 *	figure out where to put them.  Attempt to minimize data movement.
 *
 *	Mark all files in the list that lie within the prescribed range.
 *	Sort contiguous, inrange files to the head of the list.
 *	Find gaps in the list, and fill them with the largest files we
 *		can squeeze in.
 *	Slide inrange files into place only if the gaps are
 *		comparatively large.
 */
static void
order_files(efs_daddr_t minbn, efs_daddr_t length, struct idata **list,
	int count)
{
	int order_index;
	struct idata **olist = malloc(count * sizeof(*olist));
	efs_daddr_t nextbn;
	int tindex;
	int fixed;
	efs_daddr_t maxbn = minbn + length - 1;
	
	if (!olist)
		return;
	for (tindex = 0; tindex < count; tindex++)
		if (firstbn(list[tindex]) >= minbn
		    && lastbn(list[tindex]) <= maxbn)
			list[tindex]->id_flag |= ID_INRANGE;
	/*
	 * Sort the files so that contiguous files are at the head of
	 * the list in order of increasing current position, and that
	 * out-of-place and non-contiguous files follow in order of
	 * increasing size.
	 */
	qsort(list, count, sizeof(*list), ilcmp_by_bn_and_contig);

	/*
	 * The initial segment of the list consists of "in-place" files.
	 * Figure out how many there are.
	 */
	for (fixed = 0; fixed < count; fixed++)
		if (!contig_inrange(list[fixed]))
			break;
	/*
	 * Fill in gaps.  If the gap is large enough to fill_index with a file,
	 * do so, starting the search from the end of the list (the
	 * largest files).  If the remaining gap is large enough
	 * compared to the following sequence of contiguous files, slide
	 * them into the gap.  Otherwise, leave the gap alone.
	 */
	nextbn = minbn;
	order_index = 0;
	/* make_itree(list, fixed, count - 1); */
	for (tindex = 0; tindex < fixed; tindex++) {
		int i;
		int flag;
		int nblocks;
		int nfiles;
		int fill_index;
		efs_daddr_t gap;

		if (!list[tindex])
			continue;

		assert(firstbn(list[tindex]) >= nextbn);

		for (gap = firstbn(list[tindex]) - nextbn;
		     gap > 0;
		     gap = firstbn(list[tindex]) - nextbn)
		{
			fill_index = fill_gap_sorted(list, gap,
							fixed, count -1);
			
			if (fill_index < 0)
				fill_index = fill_gap(list,
						gap, tindex + 1, fixed - 1);

			if (fill_index < 0)
				break;

			assert(list[fill_index]->id_nblocks <= gap);
			olist[order_index++] = list[fill_index];
			list[fill_index]->id_target = nextbn;
			nextbn += list[fill_index]->id_nblocks;
			list[fill_index] = 0;
		}

		assert(gap >= 0);

		/*
		 * Determine the size of the following segment, and
		 * figure out whether any of the files in the segment can
		 * fill the gap.
		 */
		nfiles = contig_count(list, tindex, fixed);
		nblocks = lastbn(list[tindex + nfiles - 1])
					- firstbn(list[tindex]);

		/*
		 * compare the gap size to the following contiguous
		 * segment, and decide whether sliding is worthwhile.
		 */
		if (gap > nblocks * slide_ratio) {
			flag = 0;
		} else {
			flag = ID_FIXED;
			nextbn += gap;
		}
		/*
		 * Slide if indicated, else mark fixed.
		 */
		for (i = 0; i < nfiles; i++) {
			assert(list[tindex]);
			olist[order_index++] = list[tindex];
			list[tindex]->id_target = nextbn;
			list[tindex]->id_flag |= flag;
			nextbn += list[tindex]->id_nblocks;
			list[tindex++] = 0;
		}
		tindex--;
	}
	/*
	 * Clean up the itree
	 */
	/* free_itree(); */

	/*
	 * Tack any files that didn't fit onto the end of the list.
	 */
	for (tindex = fixed; tindex < count; tindex++)
		if (list[tindex]) {
			nextbn += list[tindex]->id_nblocks;
			olist[order_index++] = list[tindex];
			list[tindex]->id_target = nextbn;
		}

	assert(order_index == count);

	/* we created a new list, copy it back and free the temp */
	bcopy(olist, list, count * sizeof(*list));
	free(olist);
}

/*
 * contig_count -- determine the length of contiguous allocation
 *
 *	Also, return the index of the last file in the contiguous stretch
 *	that could fill the gap.
 */
static int
contig_count(struct idata **list, int tindex, int count)
{
	int nf;
	struct idata **idpp = &list[tindex + 1];

	assert(list[tindex]);
	assert(list[tindex]->id_flag & ID_CONTIG);

	for (nf = tindex + 1; nf < count; nf++, idpp++) {
		if (!idpp[0])
			break;
		if (lastbn(idpp[-1]) + 1 != firstbn(idpp[0]))
			break;
	}

	return nf - tindex;
}

#ifdef notdef
/*
 * debugging print routine
 */
static void
printinode(struct idata *idp)
{
	int i;
	fsrprintf("i %d f %x nb %d %d", idp->id_num, idp->id_flag,
		idp->id_nblocks);
	fsrprintf(" ne %d", idp->id_ne);
	fsrprintf(" ni %d", idp->id_nie);
	fsrprintf(" sv %d", idp->id_iosave);
	for (i = 0; i < idp->id_nie; i++) {
		if ((i % 6) == 0)
			fsrprintf("\nx");
		fsrprintf(" %d(%d)", idp->id_ix[i].ex_bn,
			idp->id_ix[i].ex_length);
	}
	for (i = 0; i < idp->id_ne; i++) {
		if ((i % 6) == 0)
			fsrprintf("\ne");
		fsrprintf(" %d(%d)", idp->id_ex[i].ex_bn,
				idp->id_ex[i].ex_length);
	}
	fsrprintf("\n");
}
#endif

/*
 * file_in_place -- determine whether a file is packed at bbase.  If a
 *	file is marked as "fixed", leave it where is, regardless.
 */
static int
file_in_place(struct idata *idp, efs_daddr_t bbase)
{
	if (idp->id_flag & (ID_FIXED|ID_INVALID))
		return 1;
	return bbase == idp->id_ex->ex_bn && (idp->id_flag & ID_CONTIG);
}

/*
 * start_file_at_base -- relocate a file to bbase
 */
static void
start_file_at_base(struct idata *idp, efs_daddr_t bbase)
{
	int nfree;
	int nblocks;

	if (endtime && endtime < time(0)) {
		fsrall_cleanup(1);
		exit(0);
	}
	nblocks = idp->id_nblocks;
	/*
	 * If another file got in while we were processing,
	 * treat the files that don't fit as misfits, and leave
	 * the global layout intact.
	 */
	nfree = fsc_tstfree(mp->m_dev, bbase);
	if (nfree >= nblocks) {
		pack_file2(idp, bbase, 0, nblocks);
	} else {
		make_room_and_pack(bbase, idp);
	}
}

/*
 * ex_comp -- figure out whether a file has moved
 */
static int
ex_comp(struct idata *idp, struct fscarg *fap)
{
	int i;
	extent *iex, *fex;

	iex = idp->id_ex;
	fex = fap->fa_ex;
	for (i = 0; i < idp->id_ne; i++, iex++, fex++)
		if (iex->ex_bn != fex->ex_bn
		    || iex->ex_length != fex->ex_length)
			return 0;
	iex = idp->id_ix;
	fex = fap->fa_ix;
	for (i = 0; i < idp->id_nie; i++, iex++, fex++)
		if (iex->ex_bn != fex->ex_bn
		    || iex->ex_length != fex->ex_length)
			return 0;
	return 1;
}

/*
 * efs_movex -- Pack a file contiguously into a free disk area beginning
 *		at bbase
 */
static int
efs_movex(struct idata *idp, efs_daddr_t bbase,
		efs_daddr_t offset, int count)
{
	struct fscarg fa;
	int error;

	fa.fa_dev = mp->m_dev;
	fa.fa_ino = idp->id_num;

	if (!efs_ilock(&fa)) {
		clr_idata(idp);
		return 0;
	}

	if (idp->id_nblocks != getnblocks(fa.fa_ex, fa.fa_ne)
	    || idp->id_ne != fa.fa_ne
	    || idp->id_nie != fa.fa_nie
	    || !ex_comp(idp, &fa)) {
		if (vflag)
			fsrprintf("file moved: %d\n", idp->id_num);
		clr_idata(idp);
		fsc_iunlock(&fa);
		return 0;
	}

	error = movex(mp, &fa, offset, bbase, count);

	update_inode(idp, &fa);
	fsc_iunlock(&fa);

	return error;
}

/*
 * pack_file2 -- move a file into free space, retry if the commits fail
 */
static int
pack_file2(struct idata *idp, efs_daddr_t bbase,
	efs_daddr_t offset, int count)
{
	int retry;

	for (retry = 0; retry < ICOMMIT_RETRY; retry++) {
		int error;
		error = efs_movex(idp, bbase, offset, count);
		if (error != EBUSY)
			return error;
		if (vflag)
			fsrprintf("EBUSY %d\n", idp->id_num);
	}
	return EBUSY;
}

/*
 * efs_ilock -- Lock an inode and reread the information pertaining to
 *		it from disk.
 */
static int
efs_ilock(struct fscarg *fap)
{
	struct efs_dinode *di;
	if (fsc_ilock(fap) == -1) {
		if (vflag)
			fsrprintf("ioctl(ILOCK i%d 0x%x) errno=%d\n",
				fap->fa_ino, fap->fa_dev, errno);
		if (errno == EINVAL) {
			if (vflag)
				fsrprintf("ino=%d invalid...\n", fap->fa_ino);
			return 0;
		}
		exit(1);
	}

	/* get file info straight out of the file system device */
	di = efs_iget(mp, fap->fa_ino);
	if (di == 0) {
		fsc_iunlock(fap);
		exit(1);
	}

	if (!relocatable(di) || !getex(mp, di, fap, 1)) {
		fsc_iunlock(fap);
		return 0;
	}

	return 1;
}

/*
 * make_room_and_pack -- Move a file to bbase, clearing out space for it
 *	if necessary.
 */
static void
make_room_and_pack(efs_daddr_t bbase, struct idata *idp)
{
	efs_daddr_t nblocks = idp->id_nblocks;
	efs_daddr_t offset = 0;

	do {
		efs_daddr_t nfree;
		efs_daddr_t ninplace;

		ninplace = inplace(idp, offset, bbase);

		if (ninplace == 0) {
			for (;;) {
				range_clear(1, bbase, nblocks, 0, 1);
				nfree = fsc_tstfree(mp->m_dev, bbase);
				nfree = MIN(nfree, nblocks);
				if (nfree > 0 || nblocks <= max_ext_size)
					break;
				nblocks = max_ext_size;
			}
			if (nfree <= 0) {
				if (vflag) {
					fsrprintf(
					"make_room failed: %d %d %d %d\n",
					idp->id_num, bbase - offset, bbase,
					nfree);
					print_free(mp->m_dev,
						bbase, bbase + nblocks);
				}
				return;
			}
			if (pack_file2(idp, bbase, offset, nfree))
				return;
			ninplace = nfree;
		}
		offset += ninplace;
		bbase += ninplace;
		nblocks -= ninplace;
	} while (offset < idp->id_nblocks);
}

/*
 * range_conflict -- determine whether any files following idp conflict
 *	with a target block range.
 */
static struct idata *
range_conflict(struct idata *idp, efs_daddr_t base, efs_daddr_t nblocks,
	int iblks)
{
	struct idata *idpmax = &idata_tbl[icount];

	for (; idp < idpmax; idp++) {
		int x;

		if (idp->id_flag & ID_FIXED)
			continue;
		for (x = 0; x < idp->id_ne; x++) {
			if ((base < (long)idp->id_ex[x].ex_bn
						+ (long)idp->id_ex[x].ex_length
			     && base + nblocks > (long)idp->id_ex[x].ex_bn)
			    || base == (long)idp->id_ex[x].ex_bn
					+ (long)idp->id_ex[x].ex_length)
			{
				return idp;
			}
		}
		if (!iblks)
			continue;
		for (x = 0; x < idp->id_nie; x++) {
			if (base < (long)idp->id_ix[x].ex_bn
					+ (long)idp->id_ix[x].ex_length
			    && base + nblocks > (long)idp->id_ix[x].ex_bn)
				return idp;
		}
	}
	return 0;
}

/*
 * range_clear -- filter files from a target block range.
 */
static void
range_clear(int iblks, efs_daddr_t base, efs_daddr_t nblocks, int mask,
	int result)
{
	struct idata *idp = idata_tbl;

	while (idp = range_conflict(idp, base, nblocks, iblks)) {
		/*
		 * NEEDSWORK: The filter here doesn't work correctly.
		 * If any file fails the filter, all files are moved.
		 */
		if ((idp->id_flag & mask) != result) {
			relocate(idp, base, nblocks, iblks);
			if (fsc_tstfree(mp->m_dev, base) >= nblocks)
				return;
		}
		idp++;
	}
}

/*
 * inplace -- Determine how many blocks of a file are in place, starting
 * at "offset" in file, and comparing to "base."
 */
static efs_daddr_t
inplace(struct idata *idp, efs_daddr_t offset, efs_daddr_t base)
{
	efs_daddr_t bcnt = 0;
	int ne = idp->id_ne;
	extent *ex = idp->id_ex;

	for (; ne--; ex++)
		if ((efs_daddr_t)ex->ex_offset <= offset)
			break;
	if (ne++ < 0)
		return 0;
	if ((efs_daddr_t)ex->ex_offset > offset) {
		if (base - ex[-1].ex_bn != offset - ex[-1].ex_offset)
			return 0;
		bcnt = ex->ex_offset - offset;
	}
	
	while (base == ex->ex_bn) {
		bcnt += ex->ex_length;
		base += ex->ex_length;
		if (!ne--)
			break;
		ex++;
	}
	return bcnt;
}

/*
 * relocate_sub -- Move all extents (direct and indirect) conflicting
 *	with a target area.
 */
static int
relocate_sub(struct idata *idp,
	     efs_daddr_t pbase, int nblocks, int iblks)
{
	int x;
	int error;
	efs_daddr_t bn;
	int length;

	if ((idp->id_flag & ID_CONTIG) && idp->id_target == firstbn(idp))
		goto iblk_test;

	for (x = 0; x < idp->id_ne; x++) {
		if (pbase < (long)idp->id_ex[x].ex_bn
					+ (long)idp->id_ex[x].ex_length
			    && pbase + nblocks > (long)idp->id_ex[x].ex_bn)
		{
			off_t offset = idp->id_ex[x].ex_offset;

			if (idp->id_target) {
				int nfree;

				nfree = fsc_tstfree(mp->m_dev, idp->id_target);
				if (nfree >= idp->id_nblocks) {
					error = pack_file2(idp, idp->id_target,
							0, idp->id_nblocks);
					return error;
				}
			}

			length = idp->id_ex[x].ex_length;

			bn = findfreeholey(&length, length, pbase, nblocks);
			if (bn == -1 && iblks && length > 0)
				bn = findfreeholey(&length, length,
					pbase, nblocks);
			if (bn == -1) {
				if (vflag)
				    fsrprintf(
				    "couldn't relocate: %d %d(%d) vs %d(%d)\n",
					idp->id_num, idp->id_ex[x].ex_bn,
					idp->id_ex[x].ex_length,
					pbase, nblocks);
				if (length == 0)
					return ENOSPC;
				continue;
			}
			if (vflag)
			    fsrprintf("relocate: %d %d(%d) v %d(%d)\n",
				idp->id_num, idp->id_ex[x].ex_bn,
				length, pbase, nblocks);
			error = efs_movex(idp, bn, offset, length);
			if (error)
				return error;
		} else if (pbase == (long)idp->id_ex[x].ex_bn
				+ (long)idp->id_ex[x].ex_length)
		{
			/* this seemingly useless code truncates
			 * in-memory files */
			struct fscarg fa;

			fa.fa_dev = mp->m_dev;
			fa.fa_ino = idp->id_num;

			if (efs_ilock(&fa)) {
				update_inode(idp, &fa);
				fsc_iunlock(&fa);
			} else
				clr_idata(idp);
		}
	}

 iblk_test:
	if (!iblks)
		return 0;

	for (x = 0; x < idp->id_nie; x++) {
		if (pbase < (long)idp->id_ix[x].ex_bn
					+ (long)idp->id_ix[x].ex_length
			    && pbase + nblocks > (long)idp->id_ix[x].ex_bn)
		{
			int length = idp->id_ix[x].ex_length;

			bn = findfreeholey(&length, length, pbase, nblocks);
			if (bn == -1)
				continue;
			if (vflag)
			   fsrprintf("relocatei: %d %d(%d) v %d(%d)\n",
				idp->id_num, idp->id_ix[x].ex_bn,
				idp->id_ix[x].ex_length, pbase, nblocks);
			error = efs_moveix(idp, bn, x);
			if (error)
				return error;
		}
	}
	return 0;
}

/*
 * efs_moveix -- Move the specified indirect block to base.
 */
static int
efs_moveix(struct idata *idp, efs_daddr_t base, int ni)
{
	struct fscarg fa;
	int error;

	fa.fa_dev = mp->m_dev;
	fa.fa_ino = idp->id_num;

	if (!efs_ilock(&fa)) {
		clr_idata(idp);
		return -1;
	}

	if (idp->id_nblocks != getnblocks(fa.fa_ex, fa.fa_ne)
	    || idp->id_ne != fa.fa_ne
	    || idp->id_nie != fa.fa_nie
	    || !ex_comp(idp, &fa)) {
		if (vflag)
			fsrprintf("file moved: %d\n", idp->id_num);
		clr_idata(idp);
		fsc_iunlock(&fa);
		return 1;
	}

	if (vflag)
		   fsrprintf("moveix: %d %d(%d) -> %d\n",
			idp->id_num, idp->id_ix[ni].ex_bn,
			idp->id_ix[ni].ex_length, base);
	error = moveix(mp, &fa, base, ni);

	update_inode(idp, &fa);
	fsc_iunlock(&fa);

	return error;
}

/*
 * relocate -- Move all extents (direct and indirect) conflicting
 *	with a target area.  Retry on errors.
 */
static void
relocate(struct idata *idp,
	     efs_daddr_t pbase, int nblocks, int iblks)
{
	int retry;

	if (idp->id_flag & ID_INVALID)
		return;
	for (retry = 0; retry < ICOMMIT_RETRY; retry++) {
		int error;
		error = relocate_sub(idp, pbase, nblocks, iblks);
		if (error != EBUSY) {
			if (error && vflag)
				fsrprintf("relocate %d error %d\n",
						idp->id_num,error);
			return;
		}
		if (vflag)
			fsrprintf("EBUSY %d\n", idp->id_num);
	}
}

/*
 * The fe_* routines implement a heap cache to accelerate free space
 * searches.  This is necessary only because the the fsctl interfaces to
 * do this are so slow.
 */
struct free_ext {
	efs_daddr_t start;
	int length;
};

/* this is a heap, so it gets numbered from 1 */
#define FE_SIZE 128
static struct free_ext fe_heap[FE_SIZE + 1];
static int fe_cnt;
static const int fe_min_cnt = 20;
static int fe_minsize;
static int fe_pages;
static dev_t fe_dev;

#define FE_LEFT(x)	(2 * x)
#define FE_RIGHT(x)	(2 * x + 1)

/*
 * fe_clear -- initialize the heap before adding entries
 */
void
fe_clear(void)
{
	fe_minsize = 0;
	fe_cnt = 0;
	fe_pages = 0;
	fe_dev = mp->m_dev;
}

/*
 * fe_setmin -- reduce the size of the minimal block noted in the heap
 */
static void
fe_setmin(int min)
{
	if (min < fe_minsize)
		fe_minsize = min;
}

/*
 * fe_init_cnts -- Initialize heap stats
 */
void
fe_init_cnts(void)
{
	int i;
	int fe_page_cnt = 0;

	for (i = 1; i <= fe_cnt; i++)
		fe_page_cnt += fe_heap[i].length;
	fe_pages = fe_page_cnt * 0.8;
	fe_minsize = fe_heap[1].length;
}

/*
 * fe_bubble -- move the contents of a heap location to its correct spot
 * in the heap.
 */
static void
fe_bubble(int location)
{
	while (location > 1
	       && fe_heap[location/2].length > fe_heap[location].length)
	{
		struct free_ext fe_tmp;
		fe_tmp = fe_heap[location/2];
		fe_heap[location/2] = fe_heap[location];
		fe_heap[location] = fe_tmp;
		location /= 2;
	}
}

/*
 * fe_pop -- remove a node from the heap.  Add a new one if specified.
 */
static void
fe_pop(int node, efs_daddr_t start, int length)
{
	while (FE_RIGHT(node) < fe_cnt + 1) {
		int new = fe_heap[FE_RIGHT(node)].length
					< fe_heap[FE_LEFT(node)].length
			  ? FE_RIGHT(node) : FE_LEFT(node);
		fe_heap[node] = fe_heap[new];
		node = new;
	}
	if (FE_LEFT(node) < fe_cnt + 1) {
		fe_heap[node] = fe_heap[FE_LEFT(node)];
		node = FE_LEFT(node);
	}
	if (length) {
		fe_heap[node].start = start;
		fe_heap[node].length = length;
		fe_pages += length;
		fe_bubble(node);
	} else if (node < --fe_cnt + 1) {
		fe_heap[node] = fe_heap[fe_cnt + 1];
		fe_bubble(node);
	}
}

/*
 * fe_insert -- add a node to the heap, making room if necessary
 */
void
fe_insert(efs_daddr_t start, int length)
{
	if (fe_cnt < FE_SIZE) {
		fe_heap[++fe_cnt].start = start;
		fe_heap[fe_cnt].length = length;
		fe_pages += length;
		fe_bubble(fe_cnt);
	} else if (fe_heap[1].length < length)
		fe_pop(1, start, length);
}

/*
 * fe_reduce -- extract space from a node.  Remove the node if it
 *	becomes too small, otherwise reorder the heap.
 */
static void
fe_reduce(int node, int length)
{
	if (fe_heap[node].length - length < fe_minsize) {
		fe_pages -= fe_heap[node].length;
		fe_pop(node, 0, 0);
	} else {
		fe_heap[node].length -= length;
		fe_heap[node].start += length;
		fe_pages -= length;
		fe_bubble(node);
	}
}

/*
 * fe_findfree -- scan the heap linearly, looking for free space
 */
static int
fe_findfree(int len, efs_daddr_t pbase, int nblocks)
{
	int i;

	for (i = 1; i <= fe_cnt; i++) {
		if (pbase
		    && pbase < fe_heap[i].start + fe_heap[i].length
		    && fe_heap[i].start < pbase + nblocks)
			continue;
		if (fe_heap[i].length >= len)
			return i;
	}
	return 0;
}

/*
 * fe_findmax -- find the largest block in the heap
 */
fe_findmax(efs_daddr_t pbase, int nblocks)
{
	int i;
	int j = 0;
	int max = 0;

	for (i = 1; i <= fe_cnt; i++) {
		if (pbase
		    && pbase < fe_heap[i].start + fe_heap[i].length
		    && fe_heap[i].start < pbase + nblocks)
			continue;
		if (fe_heap[i].length > max) {
			j = i;
			max = fe_heap[i].length;
		}
	}
	return j;
}

/*
 * refresh_fe_heap -- Fill the heap with free extents.
 */
static void
refresh_fe_heap(void)
{
	int cg;

	fe_clear();

	for (cg = 0; cg < mp->m_fs->fs_ncg; cg++) {
		efs_daddr_t start = EFS_CGIMIN(mp->m_fs, cg) + mp->m_fs->fs_cgisize;
		int length;

		while ((length = fsc_tstfree(mp->m_dev, start)) >= 0)
		{
			if (length > 0) {
				fe_insert(start, length);
				start += length;
			}
			length = fsc_tstalloc(mp->m_dev, start);
			if (length > 0)
				start += length;
		}
	}

	fe_init_cnts();
}

static efs_daddr_t
findfreeholey_sub(int *lengthp, int maxlength, efs_daddr_t pbase, int nblocks)
{
	int fe_index;
	efs_daddr_t bn;

	/* if the heap is played out, refill it */
	if (mp->m_dev != fe_dev || fe_cnt < fe_min_cnt || fe_pages <= 0)
		refresh_fe_heap();

	if (maxlength < fe_minsize)
		return free_search(*lengthp, pbase, nblocks);

	/* first, try for it all */
	fe_index = fe_findfree(maxlength, pbase, nblocks);
	if (fe_index != 0) {
		bn = fe_heap[fe_index].start;
		*lengthp = maxlength;
		fe_reduce(fe_index, maxlength);
		return bn;
	}

	/* next, look for the biggest segment */
	fe_index = fe_findmax(pbase, nblocks);
	if (fe_heap[fe_index].length >= *lengthp) {
		bn = fe_heap[fe_index].start;
		*lengthp = fe_heap[fe_index].length;
		fe_reduce(fe_index, *lengthp);
		return bn;
	}

	/* not long enough, return length of longest segment */
	*lengthp = fe_heap[fe_index].length;
	return -1;
}

/*
 * findfreeholey -- Find free space.  We don't particularly care where.
 */
static efs_daddr_t
findfreeholey(int *lengthp, int maxlength, efs_daddr_t pbase, int nblocks)
{
	int length = *lengthp;
	efs_daddr_t bn;
	
	do {
		*lengthp = length;
		bn = findfreeholey_sub(lengthp, maxlength, pbase, nblocks);
	} while (bn != -1 && fsc_tstfree(mp->m_dev, bn) < length);

	return bn;
}

/*
 * free_search -- Find free space.  Someplace other than the target range.
 */
static efs_daddr_t
free_search(int length, efs_daddr_t pbase, int nblocks)
{
	int cyl;
	efs_daddr_t newbn;
	int gotlen;
	int ecg = EFS_BBTOCG(mp->m_fs, pbase);

	static last_found;
	static last_found_cnt;

	ecg = pbase ? EFS_BBTOCG(mp->m_fs, pbase) : 0;

	if (last_found_cnt++ > 25) {
		last_found = 0;
		last_found_cnt = 0;
	}
	/* find room right after the target cylinder */
	for (cyl = 0; cyl < mp->m_fs->fs_ncg; cyl++) {
		int rcyl = cyl + last_found;

		if (rcyl >= mp->m_fs->fs_ncg)
			rcyl -= mp->m_fs->fs_ncg;

		if (ecg == rcyl)
			continue;
		newbn = freecg(mp, rcyl, length, &gotlen);
		if (newbn != -1 && gotlen >= length) {
			last_found = rcyl;
			return newbn;
		}
	}
	newbn = freebetween(mp->m_dev, pbase + nblocks,
		      EFS_CGIMIN(mp->m_fs, ecg + 1),
		      length, 0, &gotlen);
	if (newbn != -1 && gotlen >= length)
		return newbn;
	newbn = freebetween(mp->m_dev, 
		      EFS_CGIMIN(mp->m_fs, ecg) + mp->m_fs->fs_cgisize,
		      pbase, length, 0, &gotlen);
	if (newbn != -1 && gotlen >= length)
		return newbn;
	return -1;
}

/*
 * update_inode -- copy relevent fields from the fsc to the inode
 */
static void
update_inode(struct idata *idp, struct fscarg *fap)
{
	idp->id_nblocks = getnblocks(fap->fa_ex, fap->fa_ne);
	idp->id_flag &= ~(ID_CONTIG|ID_FIXED);

	if (iscontig(fap->fa_ex, fap->fa_ne, 0) != idp->id_nblocks) {
		; /* EMPTY nothing ever happens here */
	} else if (idp->id_target == firstbn(idp)) {
		idp->id_flag |= ID_CONTIG|ID_FIXED;
	} else {
		idp->id_flag |= ID_CONTIG;
	}

	/* switch extent info between idp and fap */
	idp->id_ne = fap->fa_ne;
	idp->id_nie = fap->fa_nie;
	if (idp->id_ex)
		free(idp->id_ex);
	if (idp->id_ix)
		free(idp->id_ix);
	idp->id_ex = fap->fa_ex;
	idp->id_ix = fap->fa_ix;
}

/*
 * reorganize by directory hierarchy.
 * Stay in dev (a restriction based on structure of this program -- either
 * call efs_{n,u}mount() around each file, something smarter or this)
 */
static void
fsrdir(char *dirname)
{
	DIR *dirp;
	struct dirent *dp;
	struct stat sb;
	char *cp, path[MAXPATHLEN];	/* XXX cd to avoid this limit */
	/*
	 * foreach dirent
	 *	if REG fsrfile
	 *	else if DIR recurse
	 */
	if (vflag)
		fsrprintf("fsrdir dirname=%s\n", dirname);

	dirp = opendir(dirname);
	if (dirp == NULL) {
		fsrprintf("opendir(%s) failed\n", dirname);
		return;
	}

	strcpy(path, dirname);
	strcat(path, "/");
	cp = index(path, '\0');
	while ((dp=readdir(dirp)) != NULL) {
		if (dp->d_name[0] == '.') {
			if (dp->d_name[1] == '.' && dp->d_name[2] == '\0')
				continue;
		}
		strcpy(cp, dp->d_name);
		if (vflag > 1)
			fsrprintf("fsrdir [%s]\n", path);
		if (lstat(path, &sb) < 0 || sb.st_dev != mp->m_dev)
			continue;
		fsrfilei(dp->d_ino);
	}
	rewinddir(dirp);
	while ((dp=readdir(dirp)) != NULL) {
		if (dp->d_name[0] == '.') {
			if (dp->d_name[1] == '\0')
				continue;
			if (dp->d_name[1] == '.' && dp->d_name[2] == '\0')
				continue;
		}
		strcpy(cp, dp->d_name);
		if (lstat(path, &sb) < 0 || sb.st_dev != mp->m_dev)
			continue;
		if (S_ISDIR(sb.st_mode))
			fsrdir(path);
	}
	closedir(dirp);
}

/*
 * fsrfilei -- reorganize a particular inode
 */
static void
fsrfilei(efs_ino_t ino)
{
	struct fscarg fa;

	fa.fa_dev = mp->m_dev;
	fa.fa_ino = ino;
	fsrfile(&fa);
}

/*
 * fsrfile(fa_dev, fa_ino)
 *
 * reorganize a file
 *
 * return value from (*packfunc) is a botch
 */
static void
fsrfile(struct fscarg *fap)
{
	struct efs_dinode *di;
	void (*packfunc)(struct fscarg *fap);

	/* ILOCK the inode */
	if (fsc_ilock(fap) == -1) {
		if (errno == EBUSY) {
			if (vflag)
				fsrprintf("ino=%d in use...\n", fap->fa_ino);
			return;
		}
		if (errno == EINVAL) {
			if (vflag)
				fsrprintf("ino=%d invalid...\n", fap->fa_ino);
			return;
		}
		if (vflag)
			fsrprintf("ioctl(ILOCK i%d 0x%x) errno=%d\n",
				fap->fa_ino, fap->fa_dev, errno);
		exit(1);
	}

	/* get file info straight out of the file system device */
	di = efs_iget(mp, fap->fa_ino);
	if (di == 0)
		exit(1);
	if (!(S_ISDIR(di->di_mode) || S_ISREG(di->di_mode))
	    || di->di_numextents < 1 || !getex(mp, di, fap, 1)) {
		fsc_iunlock(fap);
		return;
	}

	/* "rack 'em, stack 'em, and pack 'em" */
	if ((long)getnblocks(fap->fa_ex, fap->fa_ne)
	     < mp->m_fs->fs_cgfsize - mp->m_fs->fs_cgisize)
		packfunc = packfile;
	else
	 	packfunc = packbig;
	
	/* IUNLOCK unless we couldn't reaquire after a timed out ILOCK */
	packfunc(fap);
	fsc_iunlock(fap);
	freeex(fap);
}

/*
 * Pack a file to a new location:
 *	0th) fully packed in cg of inode
 *	1st preference) front of cg
 *	2nd) pack together in place
 *	3rd) end of cg
 *	4th) anywhere else in the fs
 *	5th) pull out the page-size cookie cutter...
 * If someone beats us to the desired blocks we just bug out and
 * return feeling good about at least trying.  EFAULTs, EINVALs: exit(1)
 */
static void
packfile(struct fscarg *fap)
{
	int nfree, nblocks = getnblocks(fap->fa_ex, fap->fa_ne);
	efs_daddr_t lowbn, highbn, prebn, newbn, b0;
	struct efs *fs = mp->m_fs;

	if (vflag)
		fsrprintf("packfile: %d\n", fap->fa_ino);
	if (nflag)
		return;

	/* if the data is separated from the inode, and there's space in
	 * the inode's cylinder group, move it in.
	 */
	b0 = EFS_CGIMIN(fs, EFS_ITOCG(fs, fap->fa_ino)) + fs->fs_cgisize;
	if (EFS_ITOCG(fs, fap->fa_ino) != EFS_BBTOCG(fs, fap->fa_ex->ex_bn)) {
		newbn = freebetween(fap->fa_dev, b0,
		      b0 + fs->fs_cgfsize - fs->fs_cgisize, nblocks, 0, &nfree);
		if (newbn == -1)
			return;
		if (nfree >= nblocks) {
			movex(mp, fap, 0, newbn, nblocks);
			return;
		}
	}

	extremebn(fap->fa_ex, fap->fa_ne, &lowbn, &highbn);
	prebn = nfree = 0;

	/* if we're not at the beginning of a cylinder group, try to
	 * move toward the beginning */
	if (b0 != lowbn) {

		newbn = freebetween(fap->fa_dev, b0, lowbn, nblocks,
			&prebn, &nfree);
		/* If there's space before the lowest block in the
		 * earliest cylinder group holding data for the file,
		 * move it there.
		 */
		if (newbn != -1 && nfree >= nblocks) {
			movex(mp, fap, 0, newbn, nblocks);
			return;
		}
		/* otherwise, if there's free space ahead of the file,
		 * slide it over
		 */
		if (prebn != 0 && fap->fa_ne == 1) {
			slidex(mp, fap, 0, prebn, nblocks, lowbn - prebn);
			return;
		}
	}
	if (fap->fa_ne == 1) /* single extent file? nothing else to try */
		return;
	/*
	 * ...can extents be pulled together in place?  Or...
	 */
	if (iscontig(fap->fa_ex, fap->fa_ne, 0) == nblocks)
		return;
	if (!multcg(fap->fa_ex,fap->fa_ne) &&
	    inorder(fap->fa_ex,fap->fa_ne) &&
	    isfreebetween(fap->fa_dev, fap->fa_ex, fap->fa_ne)) {
		moveinplace(fap, prebn ? prebn : lowbn);
		return;
	}

	/*
	 * ...is there a single large hole later in the cg? Or...
	 */
	newbn = freebetween(fap->fa_dev, lowbn,
		EFS_CGIMIN(fs, EFS_BBTOCG(fs, lowbn) + 1), nblocks, 0, &nfree);
	if (newbn == -1)
		return;
	if (nfree >= nblocks) {
		movex(mp, fap, 0, newbn, nblocks);
		return;
	}

	/*
	 * ...is there a single hole anywhere in the fs? Or...
	 */
	newbn = freefs(mp, EFS_BBTOCG(fs, lowbn), nblocks, &nfree);
	if (newbn == -1)
		return;
	if (nfree >= nblocks) {
		movex(mp, fap, 0, newbn, nblocks);
		return;
	}

	/*
	 * ...at least try to page-size each extent.
	 */
	packbig(fap);
}

#define MAX_EXT 0
#define LB	1
#define PAGE	2

/*
 * packbig -- reduce fragmentation in a file.  First, try to increase
 * the file's alignment.  Then check to see whether that has freed up a
 * contiguous area to plop it in.
 */
static void
packbig(struct fscarg *fap)
{
	if (vflag)
		fsrprintf("packbig: %d\n", fap->fa_ino);
	if (nflag)
		return;
	if (align_extents(fap, 0, MAX_EXT) == 0)
		make_contig(fap);
}

/*
 * align_extents -- align the file, starting at offset "x", to the
 *	specified level of granularity
 */
static int
align_extents(struct fscarg *fap, int x, int level)
{
	int to_off;
	int alignment;
	int sublevel;
	int err;

	switch (level) {
	case PAGE:
		alignment = pagesize;
		to_off = fap->fa_ex[x].ex_offset + lbsize;
		sublevel = -1;
		break;
	case LB:
		alignment = lbsize;
		to_off = fap->fa_ex[x].ex_offset + max_ext_size;
		sublevel = PAGE;
		break;
	case MAX_EXT:
		alignment = max_ext_size;
		to_off = getnblocks(fap->fa_ex, fap->fa_ne);
		sublevel = LB;
		break;
	default:
		return 1;
	}

	fe_setmin(alignment);

	if (to_off > getnblocks(fap->fa_ex, fap->fa_ne))
		to_off = getnblocks(fap->fa_ex, fap->fa_ne);
	
	while (x < fap->fa_ne - 1 && (int)fap->fa_ex[x].ex_offset < to_off)
	{
		int length;
		efs_daddr_t bn;

		/*
		 * try to end extents on "alignment" boundaries"
		 */
		assert((int)fap->fa_ex[x].ex_offset % alignment == 0);
		if ((int)fap->fa_ex[x].ex_length % alignment == 0) {
			x++;
			continue;
		}
		length = alignment;
		if (length + (int)fap->fa_ex[x].ex_offset > to_off)
			length = to_off - fap->fa_ex[x].ex_offset;

		bn = findfreeholey(&length, length, 0, 0);

		/* If free space is too fragmented, then try to align
		 * with smaller blocks.
		 */
		if (bn == -1) {
			err = align_extents(fap, x, sublevel);
			if (err)
				return err;
			while (++x < fap->fa_ne - 1
			       && (int)fap->fa_ex[x].ex_offset % alignment)
				;
			continue;
		}

		err = movex(mp, fap, fap->fa_ex[x].ex_offset, bn, length);
		if (err)
			return -1;

		/*
		 * See whether we've exceeded the budget for this pass.
		 */
		if (packbigmoves)
			if (packbigmoves == 1)
				return 1;
			else
				packbigmoves--;
	}
	return 0;
}

/*
 * make_contig -- pull discontiguous extents of an aligned file together
 */
static void
make_contig(struct fscarg *fap)
{
	for (;;) {
		int x;
		int mvlen;
		int ncontigs = 1;
		efs_daddr_t bn;
		int err;
		int nmax;

		int *nc = malloc(sizeof(int) * fap->fa_ne);

		if (!nc)
			return;
		bzero(nc, sizeof(int) * fap->fa_ne);

		/*
		 * Don't test the last extent for size or contiguity
		 */
		for (x = 0; x < fap->fa_ne - 1; x++) {
			/*
			 * if the extents aren't maximal, don't bother
			 */
			if (fap->fa_ex[x].ex_length != max_ext_size) {
				free(nc);
				return;
			}
			/*
			 * Skip the contiguous extents
			 */
			if (fap->fa_ex[x].ex_length + fap->fa_ex[x].ex_bn
			    == fap->fa_ex[x+1].ex_bn)
				continue;
			nc[ncontigs++] = x + 1;
		}
		if (ncontigs == 1) {
			/* Everything's contiguous */
			free(nc);
			return;
		}

		mvlen = getnblocks(fap->fa_ex, fap->fa_ne);
		bn = findfreeholey(&mvlen, mvlen, 0, 0);
		if (bn != -1) {
			/*
			 * There's room for the whole thing, use it!
			 */
			err = movex(mp, fap, 0, bn, mvlen);
			if (err && vflag)
				fsrprintf("make_contig %d attempt failed: %d\n",
					fap->fa_ino, err);
			free(nc);
			return;
		}
		nmax = mvlen / max_ext_size;

		/*
		 * look for a discontinuity we can join
		 */
		for (x = 0; x < ncontigs - 2; x++) {
			/*
			 * A difference of 1 yields the size of the
			 * contiguity, we need to bring two together.
			 */
			if (nc[x + 2] - nc[x] <= nmax) {
				int offset = nc[x] * max_ext_size;
				mvlen = nmax * max_ext_size;
				if (mvlen + offset
				     > getnblocks(fap->fa_ex, fap->fa_ne))
				{
					mvlen = getnblocks(fap->fa_ex,
								fap->fa_ne)
						- offset;
				}
				bn = findfreeholey(&mvlen, mvlen, 0, 0);
				if (bn == -1)
					continue;
				err = movex(mp, fap, offset, bn, mvlen);
				if (err && vflag)
					fsrprintf(
					"make_contig %d attempt failed: %d\n",
					fap->fa_ino, err);
				if (err)
					return;
				if (packbigmoves)
					if (packbigmoves == 1)
						return;
					else
						packbigmoves--;
				break;
			}
		}
		free(nc);
		/*
		 * Didn't find anything to do, so stop.
		 */
		if (x == ncontigs - 2)
			break;
	}
}

/*
 * special purpose: assumes extents are all in same cg, in order,
 * and intervening space free
 */
static void
moveinplace(struct fscarg *fap, efs_daddr_t tobn)
{
	int ne = fap->fa_ne;
	extent *ex0, *ex;

	ex0 = ex = malloc(ne * sizeof(extent));
	if (ex0 == NULL) {
		fsrprintf("moveinplace() malloc(ne=%d) failed\n", ne);
		return;
	}
	bcopy(fap->fa_ex, ex, ne * sizeof(extent));
	for (; ne--; ex++) {
		if (ex->ex_bn != tobn)
			if (slidex(mp, fap, ex->ex_offset, tobn, ex->ex_length,
			    ex->ex_bn - tobn)) {
				free(ex0);
				return;
			}
		tobn = tobn + ex->ex_length;
	}
	free(ex0);
}

/* ------------------------------------------------------------------------- */
/*
 * Compute some things about a list of extents
 */

static void
extremebn(extent *ex, int ne, /* RET */ efs_daddr_t *lowbn, efs_daddr_t *highbn)
{
	*lowbn = MAXINT;
	*highbn = 0;

	for (; ne--; ex++) {
		if ((long)ex->ex_bn < *lowbn)
			*lowbn = ex->ex_bn;
		if ((long)(ex->ex_bn + ex->ex_length) - 1 > *highbn)
			*highbn = ex->ex_bn + ex->ex_length - 1;
	}
}

#define INDIVISIBLE(x, y) ((y & (y - 1)) ? (x % y) : (x & (y - 1)))

/*
 * return offset of first discontiguous extent or
 *        block size of file if [offset,lastoff) is the last part of the file
 */
static off_t
iscontig(extent *ex, int ne, off_t offset)
{
	int nblocks = getnblocks(ex, ne);
	struct efs *fs = mp->m_fs;
	efs_daddr_t nextbn;

	assert(offset < nblocks);

	while ((int)(ex->ex_offset + ex->ex_length) < offset)
		ex++;

	while ((int)(ex->ex_offset + ex->ex_length) < nblocks) {
		/* (not last ex) */
		nextbn = ex->ex_bn + ex->ex_length;
		if (INDIVISIBLE((int)ex->ex_length, pagesize))
			return ex->ex_offset;
		if (INDIVISIBLE((int)ex->ex_length, max_ext_size))
			return ex->ex_offset;
		ex++;
		if (nextbn != EFS_CGIMIN(fs,EFS_BBTOCG(fs,nextbn)+1) &&
		    nextbn != ex->ex_bn)
			return ex->ex_offset;
	}
	return nblocks;
}


multcg(extent *ex, int ne)
{
	int cg = EFS_BBTOCG(mp->m_fs, ex->ex_bn);

	for (++ex; --ne; ++ex)
		if (EFS_BBTOCG(mp->m_fs, ex->ex_bn) != cg)
			return 1;
	return 0;
}

isfreebetween(dev_t dev, extent *ex, int ne)
{
	for (; --ne; ++ex)
		if (fsc_tstfree(dev, ex->ex_bn + ex->ex_length) !=
			(ex+1)->ex_bn - ex->ex_bn + ex->ex_length)
				return 0;
	return 1;
}

inorder(extent *ex, int ne)
{
	for (; --ne > 1; ++ex)
		if (ex[1].ex_bn < ex[0].ex_bn)
			return 0;
	return 1;
}

/*
 * Count the number of indirect extent blocks
 */
static int
real_getiblocks(extent *ex, int ne)
{
	int total = 0;
	while (--ne >= 0)
		total += ex[ne].ex_length;
	return total;
}

/*
 * clr_idata -- remove the inode from consideration
 */
static void
clr_idata(struct idata *idp)
{
	if (vflag)
		printf("marking %d invalid\n", idp->id_num);
	idp->id_flag |= ID_INVALID;
}

/*
 * add_alloc_space -- note the presence of extents in frags
 */
static void
add_alloc_space(int cnt, extent *ex)
{
	while (cnt--)
		inc_alloc_counter((ex++)->ex_bn);
}

/*
 * in_first_frags -- check whether an inode conflicts with a frag we
 *		mean to move.
 */
static int
in_first_frags(struct idata *idp, int frag_cnt)
{
	int i;

	if (idp->id_flag & ID_CONTIG)
		return ex_in_first_frags(idp->id_ex->ex_bn, frag_cnt);
	for (i = 0; i < idp->id_ne; i++)
		if (ex_in_first_frags(idp->id_ex[i].ex_bn, frag_cnt))
			return 1;
	return 0;
}
