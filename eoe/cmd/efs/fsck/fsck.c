#ident "$Revision: 1.99 $"

#include "fsck.h"

#include <sys/dkio.h>
#include <sys/mman.h>
#include <sys/statfs.h>
#include <sys/sysmp.h>
#include <sys/uadmin.h>
#include <sys/uuid.h>
#include <sys/wait.h>
#ifdef AFS
#include <sys/mount.h>
#endif
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>
#include <diskinfo.h>
#include <errno.h>
#include <fcntl.h>
#include <invent.h>
#include <mntent.h>
#include <string.h>
#include <ustat.h>

# define DEBUG fsck_debug

struct externs_per_checker sp;
struct externs_per_checker *sproc_p_one = &sp;
struct externs_per_checker **sproc_pp = &sproc_p_one;

/* patchable flags */
short fsck_debug = 0;
int replsb = 0;
int devsize = 0;
char *rawpath = NULL;
int use_blkio = 0;
int pass1check = 1; 	/* Controls ckinode() prints: no point in repeating
			 * error messages when we're zapping the inode, or
			 * after pass 1, just skipping. */
int lfproblem = 0;	/* Indicates failure to relink unref files */

char *membase = NULL;
char *cachebase;
int memsize = 0;
int cachesize = 0;
int system_memory = 0;
int child_memory = 0;	/* for parallel behaviour: total memory used
			 * by child procs so far */
int one_arg = 0;	/* set if 1 explicit arg. 
			 * If only 1 arg, we want to error exit on some
			 * conditions (such as can't open device) in order to
			 * make mount -c work properly: otherwise, we want to
			 * just note the problem and go on to next fs. */
off_t bmapsz;		/* size of bitmap */
off_t smapsz;		/* size of statemap: LSTATE bits per inode */
off_t lncntsz;		/* size of link count table: one short per inode */

int in_child;

int maxfstabpass = 0;

int	mem_free_size;
int	mem_perm_size;
int	mem_temp_size;

# define MINSECTORS	1
# define MAXSECTORS	2000
# define MINHEADS	1
# define MAXHEADS	2000
# define MINCYLS	1
# define MAXCYLS	32000

/* efs - fsck.c */
char	*fstab =	"/etc/fstab";
char	*lfname =	"lost+found";
char	*multlogdir =	"/etc/fscklogs";
char	savename[MAXPATH];

#ifdef SPROC_TEST
#define MAXSPROC 8
#else
#define MAXSPROC 1
#endif
int nsprocs = 0;
pid_t pids[MAXSPROC];

#ifdef AFS
int nViceFiles;
#endif

barrier_t *mp_barrier;
int mdbcount;

int not_device_ok;	/* for fsck'ing mkfs proto files, if non-zero */
int patch_up_super_only;	/* undocumented; fixup superblock enough to
	be mountable */

u_long		badln;
efs_ino_t	badlncnt[MAXLNCNT];
usema_t		*bcache_lock;
efs_daddr_t	bitmap_blocks;
char		*blkmap;
char		condflag;
efs_daddr_t	data_blocks;
char		devname[25];
struct filecntl	dfile;
char		*disk_bitmap;
usema_t		*dup_lock;
char		fast;
char		fixdone;
char		fixfree;
efs_daddr_t	fmax;
efs_daddr_t	fmin;
efs_ino_t	foundino;
usptr_t		*fsck_arena;
char		gflag;
char		hotroot;
char		id;
efs_daddr_t	inode_blocks;
efs_ino_t	lfdir;
short		*lncntp;
char		lostroot;
efs_ino_t	max_inodes;
char		multflag;
char		nflag;
unsigned	niblk;
efs_ino_t	orphan;
int		pass;
char		pathname[MAXPATH];
char		*pathp;
char		qflag;
char		rawflg;
char		rplyflag;
char		*savedir;
BUFAREA		sblk;
char		sflag;
char		*srchname;
char		*statemap;
char		*thisname;
char		yflag;

int
main(int argc, char **argv)
{
	int i;
	int svargc;
	int ix;
	struct stat statbuf;
	int argsprocs = 1;

	savedir = NULL;
	io_report = 1;

	id = ' ';
	if ('0' <= argv[0][0] && argv[0][0] <= '9')
		id = argv[0][0];

	setbuf(stdin,NULL);
	setbuf(stdout,NULL);
	sync();

	system_memory = get_sysmem();	

	svargc = argc;
	for (i = 1, argc--; argv[i] && *argv[i] == '-'; argc--, i++) {
		switch(argv[i][1]) {
		case 't':
		case 'T':
			i++;
			idprintf("Note: scratch files no longer needed\n");
			break;
		case 's':
			sflag++;
			break;
		case 'S':
		case 'b':	/* obsolete: but some scripts still use some
				   of them; therefore leave here as no-ops. */
			break;
		case 'n':	/* default no answer flag */
		case 'N':
			nflag++;
			yflag = 0;
			break;
		case 'y':	/* default yes answer flag */
		case 'Y':
			yflag++;
			nflag = 0;
			break;
		case 'q':
			qflag++;
			break;
		case 'D':	/* obsolete: dirs now always checked */
			break;
		case 'F':
		case 'f':
			fast++;
			break;
		case 'l' :
			if (*argv[++i] == '-' || --argc <= 0)
				iderrexit("Bad -l option\n");
			strcpy(savename,argv[i]);
			if ((stat(savename,&statbuf) != 0) ||
				(sfiletype(&statbuf) != S_IFDIR) )
			  iderrexit("Can't open salvage directory \"%s\": %s\n",
				      savename, strerror(errno));
			savedir = savename;
			break;
		case 'g' :
			gflag++;
			break;
		case 'c' :
			condflag++;
			break;
		case 'm' :
			multflag++;
			break;
		case 'P':
			if (*argv[++i] == '-' || --argc <= 0)
				iderrexit("Bad -P option\n");
			argsprocs = atoi(argv[i]);
			break;
		case 'I':	/* Ignore errors about the "device" not being a
			device.  This is used when fsck'ing mkfs proto files, for
			putting them on CD's, etc.; not to be documented. */
			not_device_ok = 1;
			break;
		case 'C':	/* not to be documented; do nothing but 'fix'
			the superblock to be "clean", so the kernel will mount it,
			so it's possible to recover some data; don't document because
			kernel may crash if you do this; still, for last ditch
			efforts.... */
			patch_up_super_only = 1;
			break;
		default:
			iderrexit("%c option?\n",*(argv[i]+1));
		}
	}

	if (argsprocs < 0)
		argsprocs = 1;
	else if (argsprocs > MAXSPROC)
		argsprocs = MAXSPROC;

	if (nflag && qflag)
		iderrexit("Incompatible options: -n and -q\n");
	if (nflag && gflag)
		iderrexit("Incompatible options: -n and -g\n");
	if (yflag && gflag)
		iderrexit("Incompatible options: -y and -g\n");
	if (qflag && gflag)
		iderrexit("Incompatible options: -q and -g\n");

	if (multflag && (!yflag && !gflag && !nflag))
		iderrexit("-m allowed only with -y, -g or -n\n");	

	if (multflag && savedir)
		iderrexit("-l not allowed in -m mode\n");

	if (argc == 0) {		/* use default fstab */
		one_arg = 0;
		fsck_from_fstab();
		exit(0);
	}

	ix = svargc - argc;	/* position of first fs argument */
	if (argc == 1)
		one_arg = 1;
	else
		one_arg = 0;
	while (argc > 0) {
		nsprocs = argsprocs;
		if (sbsetup(argv[ix], 0) == YES)
		{
			banner(argv[ix]);
			check(argv[ix]);
		}
		ix++;
		argc--;
	}
	exit(0);
	/* NOTREACHED */
}

/* ARGSUSED */
void
sproc_Phase1(void *foo)
{
	inodearea = membase + superblk.fs_cgisize * BBSIZE * procn;
	mdcache = 0;

	Phase1();
}

void
free_phase1_resources(void)
{
	inodearea = 0;
	if (sproc_p != &sp)
		free(sproc_p);
}

void
sproc_exit(void)
{
	/*
	 * Since individual sprocs have their own buffer areas, we need
	 * to flush them before switching back to the global area.
	 */
	if (inoblk.b_dirty)
		bwrite(inodearea, startib, niblk);
	inoblk.b_dirty = 0;
	flush(&fileblk);

	if (nsprocs > 1) {
		while (lastino > sp.Lastino)
			lastino = test_and_set(&sp.Lastino, lastino);
		test_then_add(&sp.N_blks, n_blks);
		test_then_add(&sp.N_files, n_files);
	} else {
		sp.Lastino = lastino;
		sp.N_blks += n_blks;
		sp.N_files += n_files;
	}

	invalidate_mdcache();

	if (nsprocs > 1) {
		sigignore(SIGCLD);
		barrier(mp_barrier, nsprocs);
	}

	if (procn == 0) {
		int wstate;
		unsigned status = 0;
		while (wait(&wstate) != -1)
			status |= wstate;
		if (status & 0xffff0000)
			exit(status >> 16);	/* right shift?? */
		if (status)
			exit(status);	/* right shift?? */
		sp.Inodearea = inodearea;
		free(sproc_p);
		sproc_p = &sp;
		if (mp_barrier) {
			free_barrier(mp_barrier);
			mp_barrier = 0;
		}
	} else {
		free_phase1_resources();
		exit(0);
	}

	lastino = (EFS_ITOCG(&superblk, lastino) + 1)
			* superblk.fs_cgisize * EFS_INOPBB;

}

/* DH nov 1990: 'pipe input' stuff has been abolished: now that fsck 
 * itself parallelizes, we will dispose of and cease to support the 
 * egregious 'dfsck'
 */
void
check(char *dev)
{
	char *sdev;
	int i;

	devname[0] = '\0';
#ifdef AFS
	nViceFiles = 0;
	sdev = strdup(dev);
#endif
	if (memsetup() == NO)
	{
		ckfini();
		return;
	}

	pass = 1;
	lostroot = 0;

	init_bmap();
	init_bitmask();

	/* parallel-ready data structure */
	sproc_pp = (struct externs_per_checker **)PRDA->usr_prda.fill;

	if (nsprocs > 1)
		mpinit();

	pids[0] = getpid();
	for (i = 1; i < nsprocs; i++) {
		sproc_p = malloc(sizeof(struct externs_per_checker));
		*sproc_p = sp;
		procn = i;
		pids[i] = sproc(sproc_Phase1, PR_SALL);
		if (pids[i] == -1) {
			infanticide();
			nsprocs = 1;
		}
	}
	sproc_p = malloc(sizeof(struct externs_per_checker));
	*sproc_p = sp;
	procn = 0;

	if(!patch_up_super_only) {
		sproc_Phase1(0);
		free_phase1_resources();
		pass1check = 0;

		if (!fast) {
			pass = 2; Phase2();
			if (!lostroot)
			{
				pass = 3; 
				Phase3();
			}
			pass = 4; Phase4();
		}

		if (lostroot)
		{
			idprintf("ROOT DIRECTORY LOST. FILESYSTEM CAN'T BE CLEANED.\n\n");
			iderrexit("It may be possible to salvage files with the -l option.\n\n");
		}
		pass = 5;
		Phase5();

		if (fixfree) {
			Phase6();
		}
		clear_ddot();
		idprintf("%ld files %ld blocks %ld free",
			n_files,n_blks,superblk.fs_tfree);
	}
	else {
		dfile.mod = 1;
		idprintf("Only marking superblock clean, no actual checks; may crash system if mounted");
	}

#ifdef AFS
	if (nViceFiles)
		idprintf("%d Vice files", nViceFiles);
#endif
	idprintf("\n");
	if (dfile.mod) {
		superblk.fs_time = time(0);
		superblk.fs_checksum = efs_supersum(&superblk);
		sbdirty();
	}

	sb_clean();

	/* Flush cached data but do not close files yet.   Closing
	 * files will cause VFS_SETATTRs, which will dirty the system's
	 * buffer cache -- not a good idea when remounting root.
	 */
	flushall();
	if (dfile.mod && hotroot) {
		if (gflag || qflag || reply("REMOUNT ROOT") == YES) {
			idprintf("***** REMOUNTING ROOT . . . *****\n");
			uadmin(A_REMOUNT, 0, 0);
		} else
			idprintf("**** ROOT FILE SYSTEM NOT REMOUNTED ****\n");
	}
	else
	{
		if (!rawflg)
			sync();
		if (dfile.mod && !gflag)
			idprintf("***** FILE SYSTEM WAS MODIFIED *****\n");
	}
	closefiles();

	/* If we couldn't reconnect something, warn */

	if (lfproblem)
	{
		fixdone = 1;
		idprintf("\n  *** WARNING: COULD NOT RECONNECT ALL UNREFERENCED FILES **\n");
		idprintf("*** Clean out lost+found and rerun fsck on this filesystem **\n");
	}
#ifdef AFS
        if (dfile.mod && nViceFiles) {
            /* Modified file system with vice files: force full salvage
             * Salvager recognizes the file FORCESALVAGE in the root of
	     * each partition
	     */
            char pname[100], fname[100], *special;
            int fd;

            idprintf("%s: vice file system partition was modified; forcing full salvage\n", dev);
            unrawname(sdev);	/* mangles sdev */
            special = (char *) strrchr(sdev, '/');
            if (!special++)
		special = dev;
	    /*
	     * Using /etc, rather than /tmp, since
	     * /tmp is a link to /usr/tmp on some systems,
	     * and isn't mounted now
	     */
            strcpy(pname, "/etc/vfsck.");
            strcat(pname, special);
            rmdir(pname);
            mkdir(pname, 0777);
            if (mount(sdev, pname, MS_DATA, "efs", NULL, 0) < 0)
                errexit("Couldn't mount %s on %s to force full salvage!\n%s\n",
			sdev, pname, strerror(errno));
            strcpy(fname, pname);
            strcat(fname, "/FORCESALVAGE");
            fd = open(fname, O_CREAT, 0);
            if (fd == -1)
                errexit("Couldn't create \"%s\" to force full salvage!\n%s: %s\n",
			fname, fname, strerror(errno));
            close(fd);
            umount(sdev);
            rmdir(pname);
        }
#ifdef NEVER
        if (logfile) {
            fsync(fileno(logfile)); /* Since update isn't running */
            fclose(logfile);
            logfile = 0;
        }
#endif
#endif /* AFS */
}

void
sb_clean(void)
{
	long sum;
	int fixit;
	int lastdblock;

	if (!hotroot && (superblk.fs_dirty != EFS_CLEAN)) {
		fixit = 0;
		if (!gflag)
			idprintf("SUPERBLK MARKED DIRTY");
		if (gflag)
			fixit = 1;
		else if (qflag) {
			printf(" - CLEANING\n");
			fixit = 1;
		} else
		if (reply("FIX") == YES)
			fixit = 1;
		if (fixit) {
			superblk.fs_dirty = EFS_CLEAN;
			sum = efs_supersum(&superblk);
			superblk.fs_checksum = sum;
			sbdirty();
		}
	}
	else if (hotroot) { 

		/* If checking root, we must force fs_dirty to EFS_CLEAN since
		 * we are about to remount.
		 */

		superblk.fs_dirty = EFS_CLEAN;
		sum = efs_supersum(&superblk);
		superblk.fs_checksum = sum;
		if (!nflag)
			sbdirty();
	}
	if (replsb)	/* working from replicate since primary was bad */
	{
		fixit = 0;
		if (!gflag)
			idprintf("PRIMARY SUPERBLOCK WAS INVALID");
		if (gflag)
			fixit = 1;
		else if (qflag) {
			printf(" - FIXING\n");
			fixit = 1;
		} else
		if (reply("FIX") == YES)
			fixit = 1;
		if (fixit) {
			sbdirty();
		}
	}

	/* We will now try to retrofit a replicate superblock if there
	 * is not one present & space exists for it.
	 * Note that mkfs does not create a replicate superblock if invoked
	 * in long form: in that form it doesn't look at device size and
	 * there is no implication that the filesystem fills the device.
	 * In an attempt to avoid complaints here in that situation,
	 * we add a little heuristic to the check: if the fs size is more
	 * than half a cylinder-group-size less than device size, we will
	 * leave things alone.
	 */
	if (superblk.fs_replsb == 0  && devsize)
	{
		lastdblock = superblk.fs_firstcg + 
				(superblk.fs_cgfsize * superblk.fs_ncg);
		if ((lastdblock < (devsize - 2)) &&
		    ((devsize - lastdblock) < (superblk.fs_cgfsize / 2)))
		{
			fixit = 0;
			if (!gflag)
				idprintf("SECONDARY SUPERBLOCK MISSING");
			if (gflag)
				fixit = 1;
			else if (qflag) {
				printf(" - FIXING\n");
				fixit = 1;
			} else
			if (reply("FIX") == YES)
				fixit = 1;
			if (fixit) {
				superblk.fs_replsb = devsize - 1;
				sum = efs_supersum(&superblk);
				superblk.fs_checksum = sum;
				sbdirty();
			}
		}

	}

	sum = efs_supersum(&superblk);
	if (superblk.fs_checksum != sum) {
		fixit = 0;
		if (!gflag)
			idprintf("CHECKSUM WRONG IN SUPERBLK");
		if (gflag)
			fixit = 1;
		else if (qflag) {
			printf(" - FIXING\n");
			fixit = 1;
		} else
		if (reply("FIX") == YES)
			fixit = 1;
		if (fixit) {
			superblk.fs_checksum = sum;
			sbdirty();
		}
	}
}

void
blkerr(char *s, efs_daddr_t blk, int severe)
{
	idprintf("%ld %s I=%u\n",blk,s,inum);
	setstate(severe ? TRASH : CLEAR);	/* mark for possible clearing */
}

/* In order to be able to check the ".." entry in each directory, 
 * descend() is passed the parent inum of the directory being descended.
 *("descending" means scanning through all entries in the inode's blocks,
 * decrementing the link count in the link table for the inode of each entry 
 * seen, and descending recursively into the inode of any entry which is 
 * a directory).
 * Current inode & parent inode are passed down through ckinode, chk_ext
 * etc to the final pass func: dirscan() in this case. That in turn calls
 * pass2() for every entry in the block; when the entry is a directory
 * inode, pass2() calls descend() with parent inode set to that of the dir we 
 * were scanning.
 */

void
descend(efs_ino_t pinum)			/* parent inode # */
{
	DINODE *dp;
	char *savname;
	off_t savsize;

	setstate(FSTATE);
	if ((dp = ginode()) == NULL)
		return;
	if (!lfdir)	/* find lost+found as we're treewalking */
	{
		if (!strcmp(thisname, lfname))
			lfdir = inum;
	}
	savname = thisname;
	*pathp++ = '/';
	savsize = filsize;
	filsize = dp->di_size;
	(void) ckinode(dp, DATA, pinum);
	thisname = savname;
	*--pathp = 0;
	filsize = savsize;
}

int
direrr(char *s)
{
	DINODE *dp;
	int n;

	idprintf("%s ",s);
	pinode();
	if ((dp = ginode()) != NULL && ftypeok(dp)) {
		newline();
		idprintf("%s=%s",DIR?"DIR":"FILE",pathname);
		if (DIR) {
			if (dp->di_size > EMPT) {
				if ((n = chkempt(dp)) == NO) {
					printf(" (NOT EMPTY)\n");
				}
				else if (n != SKIP) {
					printf(" (EMPTY)");
					if (!nflag) {
						printf(" -- REMOVED\n");
						return(YES);
					}
					else
						newline();
				}
			}
			else {
				printf(" (EMPTY)");
				if (!nflag) {
					printf(" -- REMOVED\n");
					return(YES);
				}
				else
					newline();
			}
		}
		else if (REG||LNK||CHRLNK||BLKLNK)
		{
			if (!dp->di_size) {
				printf(" (EMPTY)");
				if (!nflag) {
					printf(" -- REMOVED\n");
					return(YES);
				}
				else
					newline();
			}
		}
	}
	else {
		newline();
		idprintf("NAME=%s",pathname);
		if (dp) {
			if (!dp->di_size) {
				printf(" (EMPTY)");
				if (!nflag) {
					printf(" -- REMOVED\n");
					return(YES);
				}
				else
					newline();
			}
			else
				printf(" (NOT EMPTY)\n");
		} else {
			printf(" (NON-EXISTENT)");
			if (!nflag) {
				printf(" -- REMOVED\n");
				return(YES);
			}
			else
				newline();
		}
	}
	if (gflag)
		gerrexit();
	return(reply("REMOVE"));
}


void
adjust(short lcnt)
{
	DINODE *dp;
	int n;

	if ((dp = ginode()) == NULL)
		return;
	if (dp->di_nlink == lcnt) {
		if (((n = linkup()) == NO) || (n == REM))
			clri("UNREF",REM);
	}
	else 
	{
		if (!gflag)
		{
			idprintf("LINK COUNT %s",
				(lfdir==inum)?lfname:(DIR?"DIR":"FILE"));
			pinode();
			newline();
			idprintf("COUNT %d SHOULD BE %d",
				dp->di_nlink,dp->di_nlink-lcnt);
			if (reply("ADJUST") == YES) {
				dp->di_nlink -= lcnt;
				inodirty();
			}
		}
		else
		{
			dp->di_nlink -= lcnt;
			inodirty();
		}
	}
}

int
trunc_exlist(extent *xp, int nex, off_t to, int *tossed)
{
	int ret = 0;
	efs_daddr_t xb;

	*tossed = 0;

	for ( ; --nex >= 0 ; xp++)
	{
		/* chkinode would have cleared the inode, and this
		 * routine could never get called, it the blocks
		 * named by the extent were invalid, so we can skip
		 * error checking here.
		 */

		if ((xb = xp->ex_offset + xp->ex_length) <= to)
			continue;

		while (xp->ex_length && xb > to)
		{
			ret = 1;
			xb--;
			xp->ex_length--;
			unwind_map(xp->ex_bn + xp->ex_length);
		}

		if (xp->ex_length == 0)
		{
			xp->ex_bn = 0;
			xp->ex_offset = 0;
			(*tossed)++;
		}
	}

	return ret;
}

void
truncinode(DINODE *dp, off_t to)	/* truncate inode blocks to ``to'' */
{
	extent *xp;
	int nindirs;
	int nx;
	int xl, n;
	u_long nb;
	int nextents;

	if (!REG)
	{
		clrinode(dp);
		return;
	}

	if (lostroot)
		return;

	if (dp->di_nx == 0)
		return;

	xp = &dp->di_u.di_extents[0];

	if ((nx = dp->di_nx) <= EFS_DIRECTEXTENTS) {
		if (trunc_exlist(xp, nx, to, &nextents))
		{
			inodirty();
			fixdone = 1;
			dp->di_nx -= nextents;
		}
		return;
	}

	/* The offset field of the first extent in the disk inode holds the
	 * number of indirect extents.
	 */
	for (nindirs = xp->ex_offset; --nindirs >= 0; xp++)
	{
		xl = xp->ex_length;
		if (grabextent(xp->ex_bn, xl) == NO)
		{
			clrinode(dp);
			return;
		}

		n = MIN(nx, EXTSPERBB * xl);
		if (trunc_exlist((extent *)extentbuf, n, to, &nextents))
		{
			dp->di_nx -= nextents;
			inodirty();
			fixdone = 1;
			writeextent();

			nb = BTOBB((n - nextents) * sizeof(extent));
			while (nb < xl)
			{
				--xl;
				unwind_map(xp->ex_bn + xl);
			}
			if ((xp->ex_length = xl) == 0)
			{
				xp->ex_bn = 0;
				dp->di_u.di_extents[0].ex_offset--;
			}
		}

		nx -= n;
	}

	/* If file has been truncated from indirect to direct,
	 * pull indirect extents in.
	 */
	if ((nx = dp->di_nx) > 0 && nx <= EFS_DIRECTEXTENTS)
	{
		xp = &dp->di_u.di_extents[0];

		if (xp->ex_length > 1)
		{
			idprintf("ERROR TRUNCATING INODE");
			clrinode(dp);
			return;
		}

		if (grabextent(xp->ex_bn, 1) == NO)
		{
			clrinode(dp);
			return;
		}

		unwind_map(xp->ex_bn);
		bzero(xp, EFS_DIRECTEXTENTS * sizeof(extent));
		memcpy(xp, extentbuf, nx * sizeof(extent));
	}
}

/*
 * clri() --
 * possibly clear the inode,
 * depending on circumstances
 * and user responses.
 */
void
clri(char *s, int flg)
{
	DINODE *dp;
	int n;

	if ((dp = ginode()) == NULL)
		return;

	if (lostroot)
		return;		/* can't clean fs anyway.... */

	/* Silently refuse to clear root inode. If the clear is from phase1
	 * because of bad extent info etc, the bad state will be noted
	 * at the start of phase 2 & appropriate action taken.
	 */
	if (inum == EFS_ROOTINO)
		return;

	if ((!lfdir && (inum == (EFS_ROOTINO + 1))) || (inum == lfdir))
		idprintf("Warning: clearing corrupted lost+found inode!\n");

	if (flg == YES) {
		if (!FIFO || !qflag || nflag) {
			idprintf("%s %s",s,DIR?"DIR":"FILE");
			pinode();
		}
		if (DIR) {
			if (dp->di_size > EMPT) {
				if ((n = chkempt(dp)) == NO) {
					printf(" (NOT EMPTY)\n");
				}
				else if (n != SKIP) {
					printf(" (EMPTY)");
					if (!nflag) {
						printf(" -- REMOVED\n");
						clrinode(dp);
						return;
					}
					else
						newline();
				}
			}
			else {
				printf(" (EMPTY)");
				if (!nflag) {
					printf(" -- REMOVED\n");
					clrinode(dp);
					return;
				}
				else
					newline();
			}
		}
		if (REG||LNK||CHRLNK||BLKLNK) {
			if (!dp->di_size) {
				printf(" (EMPTY)");
				if (!nflag) {
					printf(" -- REMOVED\n");
					clrinode(dp);
					return;
				}
				else
					newline();
			}
			else
				printf(" (NOT EMPTY)\n");
		}
		if (FIFO && !nflag) {
			if (!qflag)
				printf(" -- CLEARED");
			newline();
			clrinode(dp);
			return;
		}
	}

	/* If here, we're clearing something of non-zero length. Shouldn't
	 * ever get here in preen mode, but just in case...
	 */

	if (gflag)
		gerrexit();

	fixdone = 1;

	if (flg == REM || (reply("CLEAR") == YES))
		clrinode(dp);
}

void
clrinode(DINODE *dp)		/* quietly clear inode */
{
	int (*savepfunc)() = pfunc;
	int savepasscheck;

	/* Clearing the root inode can only trash the entire file system. */
	if (inum == EFS_ROOTINO) {
		iderror("Attempting to clear root inode\n");
		exit(1);
	}
	n_files--;
	pfunc = unwind_map;
	savepasscheck = pass1check;
	pass1check = 0;
	ckinode(dp,ADDR,0);
	pass1check = savepasscheck;
	zapino(dp);
	inodirty();
	setstate(USTATE);
	pfunc = savepfunc;
}

/* sbsetup: open the files, read the superblock, and estimate the amount of
 * memory we will need: this is placed in 'memsize'. 
 * Return NO if we can't read the superblock or it fails sanity check.
 * Return NO if the fs is mounted.
 * return NO if -c and the fs is clean.
 */
/* ARGSUSED */
int
sbsetup(char *dev, int rofs)
{
	dev_t rootdev;
	struct ustat ustatarea;
	struct stat statarea;
	int files_in_fs;
	int cgibytes;
	int totsz;
	int sbstate;
	xfs_sb_t xfs_sb;
	long long ldevsize;

	replsb = 0;
	devsize = 0;

	initbarea(&sblk);

	/* 
	 * Try to find the device size, & if it is over 2 gig we must use the 
	 * readb/writeb system calls for I/O.
	 * If not, we use regular read/write for backward compatibility.
	 */
	ldevsize = findsize(dev);
	if (ldevsize < 0LL || ldevsize > 0x7fffffffLL)
		/* if it's big, treat it that way */
		devsize = 0x7fffffffL;
	else
		devsize = (int)ldevsize;
	if (devsize > (0x7fffffff / BBSIZE))
	{
		if ((rawpath = findrawpath(dev)) == NULL)
		{
			if(!not_device_ok) {
				iderror("Can't find equivalent raw device for %s\n", dev);
				if (one_arg)
					exit(FATALEXIT);
				else
					return (NO);
			}
			rawpath = dev;
		}
		use_blkio = 1;
		if ((dfile.rfdes = open(rawpath,O_RDONLY)) < 0) 
		{
			iderror("%s: %s\n", rawpath, strerror(errno));
			if (one_arg)
				exit(FATALEXIT);
			else
				return (NO);
		}
		if (!nflag)
		{
			if ((dfile.wfdes = open(rawpath,O_WRONLY)) < 0)
			{
				iderror("Can't open %s for writing: %s\n",
					 rawpath, strerror(errno));
				if (one_arg)
					exit(FATALEXIT);
				else
				{
					close (dfile.rfdes);
					return (NO);
				}
			}
		}
		else
			dfile.wfdes = -1;
	}
	else
	{
		use_blkio = 0;	/* don't use readb/writeb: maybe BLK device */
		if ((dfile.rfdes = open(dev,O_RDONLY)) < 0) 
		{
			iderror("%s: %s\n", dev, strerror(errno));
			if (one_arg)
				exit(FATALEXIT);
			else
				return (NO);
		}
		if (!nflag)
		{
			if ((dfile.wfdes = open(dev,O_WRONLY)) < 0)
			{
				iderror("Can't open %s for writing: %s\n",
					dev, strerror(errno));
				if (one_arg)
					exit(FATALEXIT);
				else
				{
					close (dfile.rfdes);
					return (NO);
				}
			}
		}
		else
			dfile.wfdes = -1;
	}
	
	if (stat("/", &statarea) < 0)
		iderrexit("Can't stat root: %s\n", strerror(errno));

	rootdev = statarea.st_dev;
	if (stat(dev, &statarea) < 0) {
		iderror("Can't stat \"%s\": %s\n", dev, strerror(errno));
		closefiles();
		return(NO);
	}

	if (nsprocs == 0)
		nsprocs = choose_nsprocs();

	hotroot = 0;
	rawflg = 0;

	if (sfiletype(&statarea) == S_IFCHR) 
		rawflg++;
	else if (sfiletype(&statarea) != S_IFBLK && !not_device_ok) {
		iderror("%s is not a block or character device\n", dev);
		closefiles();
		if (one_arg)
			exit(FATALEXIT);
		else
			return (NO);
	}

	if (rootdev == statarea.st_rdev) 
		hotroot++;
	else if (ustat(statarea.st_rdev,&ustatarea) >= 0) 
	{
		if (!nflag) 
		{
			iderror("%s is a mounted file system, ignored\n",
					dev);
			closefiles();
			return(NO);
		}
	}

	if (nflag)
		hotroot = 0; 	/* If -n, root isn't hot! */
	else if (hotroot)
		sync();
	fixfree = 0;
	dfile.mod = 0;
	n_files = n_blks = 0;
	enddup = &duplist[0];
	lfdir = 0;
	rplyflag = 0;
	mdbcount = 0;
	pass1check = 1;
	lfproblem = 0;
	badln = 0;
	sbstate = -1;

	initbarea(&fileblk);
	initbarea(&inoblk);

	if ((getblk(&sblk, EFS_SUPERBB) == NULL) ||
	    (!IS_EFS_MAGIC(superblk.fs_magic)) ||
	    ((sbstate = sb_sizecheck(&superblk)) != SB_OK))
	{
		if (getsuperblk_xfs(dev, &xfs_sb))
		{
			idprintf("%s contains an XFS filesystem; running fsck is unnecessary.\n", dev);
			closefiles();
			if (one_arg)
				exit(FATALEXIT);
			else
				return (NO);
		}

		if (sbstate == SB_TOOBIG)
		{
			closefiles();
			if (one_arg)
				exit(FATALEXIT);
			else
				return (NO);
		}

		if (!multflag)		/* reduce clutter if parallel */
			idprintf("Primary superblock inaccessible or invalid. Trying secondary.\n");
		if (((replsb = devsize - 1) <= 0) ||
		    (getblk(&sblk, replsb) == NULL) ||
	    	    (!IS_EFS_MAGIC(superblk.fs_magic)) ||
		    (sb_sizecheck(&superblk) != SB_OK)) 
		{
			idprintf("No filesystem on %s\n", dev);
			closefiles();
			if (one_arg)
				exit(FATALEXIT);
			else
				return (NO);
		}
	}

	/* If we're using parallel fsck during bootup, we don't want any
	 * verbosity about clean filesystems on the console, but log it.
	 */
	if (condflag && !replsb && (superblk.fs_dirty == EFS_CLEAN))
	{
		if (multflag)
			logclean(dev);
		else
			printf("  Filesystem is clean: not checking.\n");
		closefiles();
		return (NO);
	}
	superblk.fs_ipcg = superblk.fs_cgisize * EFS_INOPBB;
	inode_blocks = superblk.fs_cgisize * superblk.fs_ncg;
	max_inodes = inode_blocks * EFS_INOPBB - 1;
	bmapsz = howmany(superblk.fs_size, BITSPERBYTE);
	bmapsz = roundup(bmapsz, sizeof *lncntp);
	bitmap_blocks = (bmapsz + BBSIZE - 1) >> BBSHIFT;
	bmapsz = bitmap_blocks * BBSIZE;
	fmin = superblk.fs_firstcg;
	fmax = fmin + (superblk.fs_ncg * superblk.fs_cgfsize);
	data_blocks = fmax - fmin - inode_blocks;

	/* All sizes must round up to 4 byte multiples,
	 * so that later memory is correctly aligned.
	 */

	smapsz = roundup((max_inodes+1) * sizeof(*statemap),sizeof(long));
	lncntsz = roundup((long)(max_inodes+1) * sizeof(*lncntp), sizeof(long));
	cgibytes = superblk.fs_cgisize * BBSIZE;

	/* make sure we have room in the state and linkmaps to
	 * overlay the bitmap */
	totsz = smapsz + lncntsz + bmapsz + nsprocs * cgibytes;
	if (totsz < bmapsz * 2 + nsprocs * cgibytes)
		totsz = bmapsz * 2 + nsprocs * cgibytes;

	memsize = totsz;

	/* totsz is now the total required for static tables. Next we need to 
	 * estimate how much memory is needed for the hash cache to store
	 * directory blocks & inodes. This is very rule-of thumb: we find
	 * the number of files implied by the superblock and guess 1K for
	 * each 15 files, plus 100K of slop. These 'magic numbers' are
	 * just derived from observation of a number of filesystem. Note
	 * that the cache memory is not critical for functionality: if we
	 * happen to underestimate a bit for some particular fs , everything 
	 * still works OK, we just have to go to disk again for some of the
	 * blocks.
	 */

	files_in_fs = max_inodes - superblk.fs_tinode;
	if (files_in_fs <= 0)
		files_in_fs = 0;	/* sanity check! */
	cachesize = (files_in_fs * 100) + 100000;
	if (system_memory && (memsize + cachesize > system_memory))
		cachesize = system_memory - memsize;
	if (cachesize < 0)
		cachesize = 0;
	memsize += cachesize;

	return (YES);
}

/*
 * Allocate working memory.
 * Requirements have been calculated by sbsetup(), and left in memsize.
 */

int
memsetup(void)
{
	char *mbase;

	membase = (char *)realloc(membase, memsize - cachesize);
	if (!membase)
	{
		iderrexit("Can't get memory\n");
	}
	bzero(membase, memsize - cachesize);

	/* Now we set pointers for the various memory areas required:
	 *
	 * 	1) A cgisize buffer "inodearea" for reading in inodes.
	 *	2) A bitmap-sized area "blkmap" for accumulating used block
	 *	   info during pass 1 (and checking against disk freelist
	 *	   in pass 5).
	 *	3) A statemap "statemap"; this has 2 bits per inode and
	 *	   distinguishes 4 states/inode: regular file, directory,
	 *	   unassigned, and "to be cleared" (ie something bad seen).
	 *	4) A link count array "lncntp"; one entry per inode. This
	 *	   is initialized with the inode's link count during pass 1,
	 *	   and decremented on each reference seen during pass 2:
	 *	   thus each entry should be zero after pass 2 if link count
	 *	   is correct.
	 *	
	 *	These pointers are set by starting at the base "membase" of
	 *	our malloc'd memory & incrementing that base each time by the 
	 *	size of the area assigned. Note that the state & link areas are
	 *	not needed after pass 4, so we reuse that space for the
	 *	bitmap read from disk.
	 *	Memory left over after these allocations is used as a block
	 *	cache for directory blocks & directory inodes, to eliminate
	 *	random I/O during the pass 2 treewalk.
	 */

	mbase = membase;
	niblk = superblk.fs_cgisize;
	startib = fmax;
	mbase += niblk * BBSIZE * nsprocs;
	blkmap = mbase;
	mbase += bmapsz;
	statemap = mbase;
	disk_bitmap = mbase;  /* state & link_count not needed in phase 5 */
	mbase += smapsz;
	lncntp = (short *)mbase;
	mbase += lncntsz;
	saveextentlen = 0;
	freealltmpmap();
	cachebase = tmpmap(cachesize);
	if (cachebase == 0)
		cachesize = 0;
	mem_free_size = cachesize;
	mem_perm_size = 0;
	mem_temp_size = 0;
	bcacheinit();
	return YES;
}

int
sb_sizecheck(struct efs *sp)
{
	int d_area;
	int bmsize;

	if (devsize && (sp->fs_size > devsize))
	{
		supersizerr("filesystem larger than device.\n");
		return (SB_TOOBIG);
	}

	if (sp->fs_cgfsize <= sp->fs_cgisize)
	{
		supersizerr("cgfsize %ld <= cgisize %ld\n",
			sp->fs_cgfsize, sp->fs_cgisize);
		return (SB_BAD);
	}

	/* The parameterization fs_size is different if fs has grown */

	d_area = sp->fs_firstcg + sp->fs_ncg*sp->fs_cgfsize;

	if ((sp->fs_magic == EFS_MAGIC) &&
	    (sp->fs_size != d_area))
	{
		supersizerr("fsize %ld != firstcg %d + %dx%ld\n",
			sp->fs_size,
			sp->fs_firstcg, sp->fs_ncg, sp->fs_cgfsize);
		return (SB_BAD);
	}

	if (sp->fs_magic == EFS_NEWMAGIC)
	{
		if (d_area > sp->fs_size)
		{
			supersizerr("incorrect fs_size.\n");
			return (SB_BAD);
		}
	}

	/*
	 * If it seems that the only thing wrong with the superblock
	 * is the bitmap size, allow fsck to proceed and reconstruct
	 * the bit map.
	 */
	bmsize = howmany(sp->fs_size, BITSPERBYTE); /* bytes of bitmap */
	if (sp->fs_bmsize != bmsize) {
		supersizerr("bitmap size %d should be %d\n",
			sp->fs_bmsize, bmsize);
		sp->fs_bmsize = bmsize;
	}
	bmsize = howmany(bmsize, BBSIZE);	/* now BLOCKS of bitmap */

	/* The bitmap overlap test changes for a grown fs since it is now at 
	 * the end of storage, pointed to by fs_bmblock. We retain the old
	 * test for nongrown filesystems.
	 */

	if (!sp->fs_bmblock)	/* not grown */
	{
		if (BITMAPB+bmsize > sp->fs_firstcg)
		{
			supersizerr("bitmap ends at %d, overlaps firstcg %d\n",
			BITMAPB+bmsize, sp->fs_firstcg);
			return (SB_BAD);
		}
	}
	else	
	{
		if (sp->fs_bmblock < d_area)
		{
			supersizerr("bitmap starts at %d, overlaps cylinder group space ending at %d\n",
			sp->fs_bmblock, d_area);
			return (SB_BAD);
		}
	}

	return(SB_OK);
}

/* Print message describing problem with superblock. */
int
supersizerr(char *fmt, ...)
{
	va_list ap;

	if (replsb)
		iderror("Secondary superblock size check: ");
	else
		iderror("Primary superblock size check: ");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	return (NO);
}

DINODE *
ginode(void)
{
	DINODE *dp;
	efs_daddr_t iblk;

	if (inum > max_inodes)
		return(NULL);
	iblk = ITOD(&superblk, inum);
	if (inodearea) {
		if (iblk < startib || iblk >= startib+niblk) {
			if (inoblk.b_dirty)
				bwrite(inodearea, startib, niblk);
			inoblk.b_dirty = 0;
			if (bread(inodearea, iblk, niblk) == NO) {
				startib = fmax;
				return(NULL);
			}
			startib = iblk;
			phase1mdread();
		}
		dp = (DINODE *)&inodearea[(unsigned)((iblk-startib)<<BBSHIFT)];
	} else
	if (getblk(&inoblk,iblk) != NULL)
		dp = inoblk.b_un.b_dinode;
	else
		return(NULL);
	return(dp + ITOO(&superblk, inum));
}

int
reply(char *s)
{
	char line[80];

	rplyflag = 1;
	line[0] = '\0';
	newline();
	idprintf("%s? ",s);
	if (nflag || dfile.wfdes < 0) {
		printf(" no\n\n");
		return(NO);
	}
	if (yflag) {
		printf(" yes\n\n");
		return(YES);
	}
	while (line[0] == '\0') {
		if (getline(stdin,line,sizeof(line)) == EOF)
			errexit("\n");
		newline();
		if (line[0] == 'y' || line[0] == 'Y')
			return(YES);
		if (line[0] == 'n' || line[0] == 'N')
			return(NO);
		idprintf("Answer 'y' or 'n' (yes or no)\n");
		line[0] = '\0';
	}
	return(NO);
}

int
getline(FILE *fp, char *loc, int maxlen)
{
	int n;
	char *p, *lastloc;

	p = loc;
	lastloc = &p[maxlen-1];
	while ((n = getc(fp)) != '\n') {
		if (n == EOF)
			return(EOF);
		if (!isspace(n) && p < lastloc)
			*p++ = n;
	}
	*p = 0;
	return(p - loc);
}

void
mpinit(void)
{
	int i;
	char *fn = "/dev/zero";

	(void)sigset(SIGUSR1, mem_ready);
	sighold(SIGUSR1);
	for (i = 1; i < nsprocs; i++)
		pids[i] = 0;
	putenv("USUSEZEROFORLIBC=1");
	fsck_arena = usinit(fn);
	if (!fsck_arena) {
		perror("can't open mp arena");
		printf("--%s--\n", fn);
		nsprocs = 1;
		return;
	}
	mp_barrier = new_barrier(fsck_arena);
	if (!mp_barrier) {
		perror("can't set mp_barrier");
		nsprocs = 1;
		return;
	}
	sigset(SIGCLD, all_exit);
	if (prctl(PR_SETEXITSIG, SIGCLD) == -1) {
		perror("PR_SETEXITSIG not supported");
		return;
	}
	if (!bcachempinit())
		return;
	if (!dupmpinit())
		return;
}

/*
 * this is the same kind of bitmap
 * used by the efs...
 */
int
maphack(efs_daddr_t blk, int flg)
{
	u_long *p = (u_long *)blkmap;
	unsigned n;
	off_t byte;

	byte = blk >> WORDSHIFT;
	n = bitmask[blk & WORDMASK];

	p += (unsigned)byte;

	switch (flg) {

	case BMSET: /* set */
		if (nsprocs > 1)
			test_then_or(p, n);
		else
			*p |= n;
		break;

	case BMGET: /* get */
		n &= *p;
		break;

	case BMCLR: /* clear */
		if (nsprocs > 1)
			test_then_and(p, ~n);
		else
			*p &= ~n;
		break;
	}

	return ((int)n);
}

int
dolncnt(short val, int flg)
{
	short *sp;

	sp = &lncntp[(unsigned)inum];
	switch(flg) {
		case 0:
			*sp = val;
			break;
		case 2:
			(*sp)--;
	}
	return(*sp);
}

BUFAREA *
getblk(BUFAREA *bp, efs_daddr_t blk)
{
	if (bp->b_bno == blk)
		return(bp);
	flush(bp);
	if (bread(bp->b_un.b_buf,blk,1) != NO) {
		bp->b_bno = blk;
		return(bp);
	}
	bp->b_bno = (efs_daddr_t)-1;
	return(NULL);
}

void
flush(BUFAREA *bp)
{
	int repl;

	if (bp->b_dirty) {
		if (bp == &sblk) {
			if (dfile.wfdes < 0) {
				bp->b_dirty = 0;
				return;
			}
			bp->b_dirty = 0;
			repl = superblk.fs_replsb;
			if (repl) {
				io_report = 0; /* don't complain if can't write
						* repl sb.
						*/

				bwrite(bp->b_un.b_buf, repl, 1); 
				io_report = 1;
			}
			if (bwrite(bp->b_un.b_buf, EFS_SUPERBB, 1) == NO)
				dfile.mod = 1;
			return;
		}
		bwrite(bp->b_un.b_buf,bp->b_bno,1);
		bp->b_dirty = 0;
	}
}

void
inodirty(void)
{
	if (!nflag)
		inoblk.b_dirty = 1;
}

void
fbdirty(void)
{
	if (!nflag)
		fileblk.b_dirty = 1;
}

void
sbdirty(void)
{
	if (!nflag)
		sblk.b_dirty = 1;
}

void
rwerr(char *s, efs_daddr_t blk)
{
	newline();
	idprintf("CANNOT %s: BLK %ld",s,blk);
	if (gflag || (multflag && in_child) || (reply("CONTINUE") == NO))
		iderrexit("Program terminated\n");
}

/*
 * Compute the size of a given inode, given its direct and indirect
 * extents.
 * daveh 10/5/88: added sanity check for ridiculous extents; returns
 * -1 now in that case.
 * daveh 10/9/90: added a lot of safety nets, it can SEGV very easily otherwise.
 *   	Also return -1 if ANYthing fails: this is only used for fixing up
 *	directory sizes; if we can't compute it we want to zap the inode.
 */
long
sumsize(DINODE *dp)
{
	extent *ex;
	static BUFAREA extblk;
	int i, ne, t;
	efs_daddr_t blkno;

	if (dp->di_nx == 0)
		return (0);

	if ((unsigned)dp->di_nx > EFS_MAXEXTENTS)
		return (-1);

	switch (dfiletype(dp)) {
	  case IFREG:
	  case IFDIR:
	  case IFLNK:
	  case IFCHRLNK:
	  case IFBLKLNK:
		break;
	  default:
		return (0);
	}
	ex = &dp->di_u.di_extents[dp->di_nx - 1];
	if (ex == NULL)
		return -1;

	if (dp->di_nx <= EFS_DIRECTEXTENTS)
		return (long)BBTOB(ex->ex_offset + ex->ex_length);

	/*
	 * Figure out where in a particular indirect extent the last extent
	 * for the file lives.
	 */
	ne = dp->di_nx;
	ex = &dp->di_u.di_extents[0];
	for (i = dp->di_u.di_extents[0].ex_offset; --i >= 0; ex++) {
		if (ex == NULL)
			return -1;
		t = BBTOB(ex->ex_length) / sizeof(extent);
		if (t >= ne) {
			/*
			 * Found the correct indirect extent.  Read in the
			 * block that covers that last extent.
			 */
			ne--;
			blkno = ex->ex_bn + ((ne * sizeof(extent)) / BBSIZE);
			if (outrange(blkno))
				return -1;
			if (getblk(&extblk, blkno) == NULL)
				return -1;
			ex = &extblk.b_un.b_ext[ne % (BBSIZE / sizeof(extent))];
			if (ex == NULL)
				return -1;
			return (long)BBTOB(ex->ex_offset + ex->ex_length);
		}
		ne -= t;
	}
	return -1;
}

void
sizechk(DINODE *dp)
{
	off_t nblks, niblks;
	long l;
	int nindirs, i;

	/* Future safety: there may shortly be "squished" symlinks with 
	 * the link data in the inode addr area. Don't clobber these.
	 */
	if ((LNK||CHRLNK||BLKLNK) && (dp->di_nx == 0) && 
	    (dp->di_size <= (sizeof(struct extent) * EFS_DIRECTEXTENTS)))
		return;

	if (DIR && (dp->di_size == 0)) 
	{
		idprintf("ZERO SIZE DIRECTORY I=%u", inum);
		if (!nflag)
		{
			clri("", REM);
			superblk.fs_tinode++;
			sbdirty();
			idprintf("- CLEARED\n");
		}
		else
			idprintf("\n");
		return;

	}

	if (DIR && (dp->di_size & EFS_DIRBMASK)) {
		idprintf("DIRECTORY SIZE ERROR I=%u", inum);
		if (gflag)
			gerrexit();

		if (qflag || reply("FIX") == YES)
		{
			if ((l = sumsize(dp)) != -1)
			{
				dp->di_size = l;
				inodirty();
				if (qflag) idprintf("- FIXED\n");
			}
			else
			{
				printf("  DIRECTORY EXTENTS CORRUPTED.\n");
				if (reply("CANNOT FIX. CLEAR") == YES)
				{
					clri("", REM);
					superblk.fs_tinode++;
					sbdirty();
					return;
				}
			}
		}
	}

	/* DH note: generic 'howmany' macro breaks for params > 0x7fffffff 
	 * so it is replaced with an explicit calculation here!
	 */
	nblks = (dp->di_size / BBSIZE) + ((dp->di_size % BBSIZE) ? 1 : 0);
	if (dp->di_nx > EXTSPERDINODE) {
		nindirs = dp->di_x[0].ex_offset;
		if (nindirs > EFS_DIRECTEXTENTS) { 
		
			/* the inode is skipped in phase 1 when this happens,
			 * clean it up here or we can get a segmentation
			 * violation.  Happens when a inode block gets trashed,
			 * usually. Olson, 4/89
			 */
			idprintf("NUMBER OF EXTENTS TOO LARGE I=%u", inum);
			if (gflag)
				gerrexit();
			if (reply("CLEAR") == YES) {
				clri("", REM);
				superblk.fs_tinode++;
				sbdirty();
			}
			return;
		}
		for (niblks = 0, i = 0; i < nindirs; i++)
			niblks += dp->di_x[i].ex_length;
		nblks += niblks;
	}
	/*
	 * If the number of blocks shown indicated by di_size
	 * is less than the number of blocks found in pass1
	 * (held, in the now-misleadingly-labeled global ``filsize'')
	 * problem could have been caused by system crashing
	 * between writing indirect extents and the inode itself.
	 * If so indicated, salvage by trimmimg back blocks to nblks.
	 */
	if (nblks < filsize && dp->di_size && REG)
	{
#ifdef FSCKDEBUG
		idprintf("NBLKS=%d, NIBLKS=%d, FILSIZE=%d, DI_SIZE=%d, DI_NX=%d, I=%u\n",
			nblks, niblks, filsize, dp->di_size, dp->di_nx, inum);

#endif
		idprintf("POSSIBLE FILE SIZE ERROR I=%u", inum);

		if (gflag)
			gerrexit();

		if (reply("TRUNCATE EXCESS BLOCKS") == YES) 
		{
			truncinode(dp, nblks - niblks);
			sbdirty();
		}

	}
	/*
	 * For all other size-mismatches we want to blow away the file.
	 */
	else if ((nblks != filsize) || (!dp->di_size && dp->di_nx))
	{
#ifdef FSCKDEBUG
		idprintf("NBLKS=%d, NIBLKS=%d, FILSIZE=%d, DI_SIZE=%d, DI_NX=%d, I=%u\n",
			nblks, niblks, filsize, dp->di_size, dp->di_nx, inum);

#endif
		if (DIR) 
			idprintf("POSSIBLE DIR SIZE ERROR I=%u\n\n",
				inum);
		else 
			idprintf("POSSIBLE FILE SIZE ERROR I=%u", inum);

		if (gflag)
			gerrexit();
		if (reply("CLEAR") == YES) 
		{
			clri("", REM);
			superblk.fs_tinode++;
			sbdirty();
		}
	}
}

void
flushall(void)
{
	flush(&fileblk);
	flush(&sblk);
	flush(&inoblk);
}

void
ckfini(void)
{
	flushall();
	closefiles();
}

void
closefiles(void)
{
	close(dfile.rfdes);
	if (dfile.wfdes > 0)
		close(dfile.wfdes);
}

void
pinode(void)
{
	DINODE *dp;
	char *p;
	time_t mtime;
	char uidbuf[200];

	printf(" I=%u ",inum);
	if ((dp = ginode()) == NULL)
		return;
	printf(" OWNER=");
	if (getpw((int)dp->di_uid,uidbuf) == 0) {
		for (p = uidbuf; *p != ':'; p++);
		*p = 0;
		printf("%s ",uidbuf);
	}
	else
		printf("%d ",dp->di_uid);
	printf("MODE=%o\n",dp->di_mode);
	idprintf("SIZE=%ld ",dp->di_size);
	mtime = dp->di_mtime;
	p = ctime(&mtime);
	printf("MTIME=%12.12s %4.4s ",p+4,p+20);
}

/*
 * linkup() --
 */
int
linkup(void)
{
	DINODE *dp;
	int lostdir;
	efs_ino_t *blp;
	int n;

	/* If we can't reconnect for any reason, return "YES", otherwise
	 * callers will zap the inode. We don't want that: anything we
	 * can't reconnect should be left alone; we'll warn user on
	 * termination.
	 */
	if (lostroot)
		return (YES);	/* can't clean fs anyway! */
	if (lfproblem)	
		return (YES);

	if ((dp = ginode()) == NULL)
		return(NO);
	lostdir = DIR;
	if (!FIFO || !qflag || nflag) 
	{
		idprintf("UNREF %s ",lostdir ? "DIR" : "FILE");
		pinode();
	}
	if (DIR) 
	{
		if (dp->di_size > EMPT) 
		{
			if ((n = chkempt(dp)) == NO) 
			{
				printf(" (NOT EMPTY)");
				if (!nflag) 
				{
					printf(" MUST reconnect\n");
					goto connect;
				}
			}
			else if (n != SKIP) 
			{
				printf(" (EMPTY)");
				if (!nflag) 
				{
					printf(" Cleared\n");
					return(REM);
				}
			}
		}
		else 
		{
			printf(" (EMPTY)");
			if (!nflag) 
			{
				printf(" Cleared\n");
				return(REM);
			}
		}
	}

	if (REG||LNK||CHRLNK||BLKLNK)
	{
		if (!dp->di_size) 
		{
			printf(" (EMPTY)");
			if (!nflag) 
			{
				printf(" Cleared\n");
				return(REM);
			}
			else
				newline();
		}
		else
			printf(" (NOT EMPTY)");

	}

	if (FIFO && !nflag) 
	{
		if (!qflag)	printf(" UNREF FIFO -- REMOVED");
		newline();
		return(REM);
	}
	if (FIFO && nflag)
		return(NO);
	if (gflag)
		goto connect;
	if (reply("RECONNECT") == NO)
		return(NO);
connect:
	/* New recovery stuff: if savedir specified we're going to create
	 * copies of nonempty regular files in it under their inums. 
	 * If the copy fails AND there is a lost+found problem, we didn't
	 * manage to save it anywhere: so quit.
	 */
	if (savedir && REG && dp->di_size)
	{
		if ((copyoff(dp, inum) < 0) && lfproblem)
			iderrexit("File inum %d can't be saved either in lost+found or %s\n", inum, savedir);
	}

	orphan = inum;
	if (lfdir == 0) 
	{
		inum = EFS_ROOTINO;
		if ((dp = ginode()) == NULL) 
		{
			inum = orphan;
			if (gflag)
				iderrexit("Can't reconnect: program terminated\n");
			lfproblem = 1;
			return(YES);
		}
		pfunc = findino;
		srchname = lfname;
		filsize = dp->di_size;
		foundino = 0;
		ckinode(dp,DATA,0);
		inum = orphan;
		if ((lfdir = foundino) == 0) 
		{
			lfproblem = 1;
			idprintf("\n  SORRY. NO lost+found DIRECTORY\n\n");
			if (gflag)
				iderrexit("Can't reconnect: program terminated\n");
			return(YES);
		}
	}

	inum = lfdir;
	if ((dp = ginode()) == NULL || !DIR || getstate() != FSTATE) 
	{
		inum = orphan;
		idprintf("\n  SORRY. NO lost+found DIRECTORY\n\n");
		if (gflag)
			iderrexit("Can't reconnect: program terminated\n");
		lfproblem = 1;
		return(YES);
	}
	if (dp->di_size & BBMASK) {
		dp->di_size = roundup(dp->di_size,BBSIZE);
		inodirty();
	}
	filsize = dp->di_size;
	inum = orphan;
	pfunc = mkentry;
	if ((ckinode(dp,DATA,0) & ALTERD) == 0) {
		idprintf("\n  SORRY. NO SPACE IN lost+found DIRECTORY\n\n");
		if (gflag)
			iderrexit("Can't reconnect: program terminated\n");
		lfproblem = 2;
		return(YES);
	}

	/* If the inode we're reconnecting is in the badlink table, remove */

	for (blp = badlncnt; blp < &badlncnt[badln]; blp++)
		if (*blp == inum) 
		{
			*blp = 0L;
			break;
		}

	/* If it's a directory: sanity check the link count: should be at 
	 * least 2 (self and parent). Also change its ".." to lost+found  and 
	 * increment the lost+found link count. We have to fiddle the link
	 * count table entry for lost+found too: it's currently 0 (or should
	 * be) since we've accounted for all its existing connections: but
	 * when we descend the newly connected directory we'll get a declncnt
	 * on lost+found.
	 */
	if (lostdir && (dp = ginode())) {
		if ((dp->di_nlink < 2) || (dp->di_nlink > 100))
			dp->di_nlink = 2;
		inodirty();
		pfunc = chgdd;
		filsize = dp->di_size;
		ckinode(dp,DATA,0);
		inum = lfdir;
		if ((dp = ginode()) != NULL) {
			dp->di_nlink++;
			inodirty();
			setlncnt(1);
		}
		inum = orphan;
		idprintf("DIR I=%u CONNECTED.\n",orphan);
	}

	/* If it's NOT a directory, its link count has to be 1 */

	if (!lostdir && (dp = ginode())) {
		dp->di_nlink = 1;
		inodirty();
	}

	/* Set the link count table entry: we don't want any further 
	 * adjustments for this inode. It has to be set to 1, since 
	 * there'll be a declncnt when we descend the dir.
	 */

	setlncnt(1);
	return(YES);
}

/* This function is called to read a complete indirect extent in one
 * operation, to avoid multiple one-block read calls.
 */
int
grabextent(int blk, unsigned len)
{
	int ret;

	if (outrange(blk))
		return NO;
	io_report = 0;
	if ((ret = bread(extentbuf, blk, len)) == YES)
	{
		saveextentblk = blk;
		saveextentlen = len;
	} else
		saveextentlen = 0;

	io_report = 1;

	return ret;
}

int
writeextent(void)
{
	if (saveextentblk && saveextentlen)
		return bwrite(extentbuf, saveextentblk, saveextentlen);
	return YES;
}

/* NOTE (daveh 8/29/89): Changed to use the new readb & writeb system calls 
 * if size is > 2 gig. Size is now in BLOCKS;
 * The BBSIZE multiplier has been removed from all the bread & bwrite calls.
 * Note daveh 10/20/90: now that there is never a temp file, the need for
 * an fd specifier has gone.
 */

#ifdef IOTRACE
char nbuf[100];
int lfd = 0;
int loglen;
#endif

bread(char *buf, efs_daddr_t blk, int size)
{
	char *cbuf;
	char *error = 0;

	/* Extent buffering: saves reading indirect extents block by block */

	if ((blk >= saveextentblk) && 
	    ((blk + size) <= (saveextentblk + saveextentlen)))
	{
		bcopy(&extentbuf[((blk - saveextentblk) * BBSIZE)],
		      buf,
		      size * BBSIZE);
		return (YES);
	}

	/* dir/inode block cache: try first to get block from cache. Don't
	 * try if we're in pass 5 or later: the cache memory may have been
	 * overwritten by disk bitmap by then, since we don't need the
	 * cache anymore.
	 */
	if ((pass > 1) && (pass < 5) && (size == 1) && (cbuf = bclookup(blk)))
	{
		bcopy(cbuf, buf, BBSIZE);
		return YES;
	}

	if (mdcache && startmdcache <= blk && blk + size <= endmdcache) {
		bcopy(&mdcache[((blk - startmdcache) * BBSIZE)],
		      buf,
		      size * BBSIZE);
		return (YES);
	}

	if (use_blkio)
	{
		if (readb(dfile.rfdes, buf, blk, size) != size)
			error = "BLOCK READ";
	}
	else
	{
		if (lseek(dfile.rfdes,blk<<BBSHIFT,0) < 0) {
			error = "SEEK";
		} else if (eread(dfile.rfdes, buf, size) != YES) {
			error = "READ";
		}
	}

	if (error) {
		if (io_report) {
			perror("fsck");
			rwerr(error, blk);
		}
		return(NO);
	}

#ifdef IOTRACE
	/* IO trace */
	if (lfd == 0)
	{
		if ((lfd = creat("/usr/tmp/fscklog", 0644)) < 0)
		{
			perror("logfile");
			lfd = -1;
		}
	}
	if (lfd > 0)
	{
		bzero(nbuf, 100);
		if (blk > 2)
		    sprintf(nbuf, "%d  %d  %s\n", blk, size, blkis(blk, buf));
		else
		    sprintf(nbuf, "%d  %d  0\n", blk, size);
		loglen = strlen(nbuf);
		if (write(lfd, nbuf, loglen) != loglen)
		{
			perror("log write");
			lfd = -1;
		}
	}
	/* end IO trace */
#endif
	return (YES);
}


bwrite(char *buf, efs_daddr_t blk, int size)
{
	char *cbuf;

	if (dfile.wfdes < 0)
		return(NO);

	if (use_blkio)
	{
		if (writeb(dfile.wfdes, buf, blk, size) != size)
		{
			if (io_report)
				rwerr("WRITE",blk);
			return(NO);
		}
	}
	else
	{
		if (lseek(dfile.wfdes,blk<<BBSHIFT,0) < 0)
		{
			if (io_report)
				rwerr("SEEK",blk);
			return(NO);
		}
		if (ewrite(dfile.wfdes, buf, size) != YES)
		{
			if (io_report)
				rwerr("WRITE",blk);
			return(NO);
		}

	}
	saveextentlen = 0;	/* kill extent cache */
	dfile.mod = 1;

	/* If the block is in the bcache, update the copy there. Don't
	 * try if we're in pass 5 or later: the cache memory may have been
	 * overwritten by disk bitmap by then, since we don't need the
	 * cache anymore.
	 */

	if ((pass > 1) && (pass < 5) && (size == 1) && (cbuf = bclookup(blk)))
		bcopy(buf, cbuf, BBSIZE);

	return (YES);
}

void
idprintf(char *fmt, ...)
{
	va_list ap;

	printf("%c %s", id, devname);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

void
iderror(char *fmt, ...)
{ 
	va_list ap;

	printf("%c %s", id, devname);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

void
iderrexit(char *fmt, ...)
{
	va_list ap;

	printf("%c %s", id, devname);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	exit(FATALEXIT);
}

void
errexit(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	exit(FATALEXIT);
}

void
gerrexit(void)
{
	printf("\nCannot proceed without risk of data loss.\n");
	printf("Manual judgement required. Program terminated.\n\n");
	exit(GSTATEABORT);
}

void
newline(void)
{
	printf("\n");
}

/****************************************************************************/

/* #define BCDEBUG */

/* Simple hashed cache for storing blocks: used to retain directory blocks 
 * and blocks containing directory inodes from phase 1 in order to 
 * hopefully eliminate the need for disk I/O in passes 2 - 4. It uses any
 * memory left over after the bitmap etc have been allocated.
 */


struct bcache {
	struct bcache 	*next;
	unsigned 	blk;
	char		buf[BBSIZE];
	};

struct mdbcache {
	efs_daddr_t startbn;
	efs_daddr_t endbn;
	char    *buffer;
};

struct mdbcache mdbcache[100];

struct bcache *chead[NHASHHEADS] = {0};

#define	BCACHE_LOCKASSERT()

#ifdef BCDEBUG
int bcloads = 0;
int bcgoodloads = 0;
int bclooks = 0;
int bchits = 0;
int bclinks = 0;
#endif

int max_wait_size;
int min_wait_size;
int mem_waits;

void
mem_wait(int size)
{
	if (mem_waits++ == 0) {
		max_wait_size = min_wait_size = size;
	} else {
		if (size > max_wait_size)
			max_wait_size = size;
		if (min_wait_size == 0 || size < min_wait_size)
			min_wait_size = size;
	}
	BCACHE_UNLOCK();
	sigpause(SIGUSR1);
	BCACHE_LOCK();
	mem_waits--;
}

void
mem_wakeup(void)
{
	int i;

	/*
	 * Wake up if we can satisfy a request, or if we can never
	 * satisfy a request
	 */
	if (mem_free_size >= min_wait_size
	    || max_wait_size > mem_free_size + mem_temp_size)
	{
		for (i = 0; i < nsprocs; i++)
			if (pids[i])
				kill(pids[i], SIGUSR1);
	}
}

void
bcacheinit(void)
{
	int i;
#ifdef BCDEBUG
	bcloads = 0;
	bcgoodloads = 0;
	bclooks = 0;
	bchits = 0;
#endif

	for (i = 0; i < NHASHHEADS; i++)
		chead[i] = NULL;
}

void
all_exit(void)
{
	int status;

	wait(&status);
	printf("%d: exiting because of sib %x\n", procn, status);

	exit(1);
}

void
mem_ready(void)
{
	sighold(SIGUSR1);
}

int
mem_reserve(int size, int newstate, int wait)
{
	int ret;
	BCACHE_LOCK();
	ret = _mem_reserve(size, newstate, wait);
	BCACHE_UNLOCK();
	return ret;
}

int
_mem_reserve(int size, int newstate, int wait)
{
	while (wait &&
		nsprocs > 1 &&
		mem_free_size < size &&
		mem_free_size + mem_temp_size >= size)
	{
		mem_wait(size);
	}
	if (mem_free_size < size)
		return 0;
	mem_free_size -= size;
	if (newstate == PERM) {
		mem_perm_size += size;
		if (mem_waits)
			mem_wakeup();
	} else {
		mem_temp_size += size;
	}
	return size;
}

void
mem_release(int size)
{
	BCACHE_LOCK();
	mem_temp_size -= size;
	mem_free_size += size;
	if (mem_waits)
		mem_wakeup();
	BCACHE_UNLOCK();
}

void
mem_preserve(int size)
{
	mem_temp_size -= size;
	mem_perm_size += size;
	if (mem_waits)
		mem_wakeup();
}

int
bcachempinit(void)
{
	bcache_lock = usnewsema(fsck_arena, 1);
	return bcache_lock != 0;
}

char *
bclookup(unsigned blk)
{
	struct bcache *bc;
	int mdoff;

	/* BCACHE_LOCK(); */
#ifdef BCDEBUG
	bclooks++;
#endif
	bc = chead[blk % NHASHHEADS];
	while (bc)
	{
		if (bc->blk == blk)
		{
#ifdef BCDEBUG
			bchits++;
#endif
			/* BCACHE_UNLOCK(); */
			return bc->buf;
		}
		bc = bc->next;
	}

	mdoff = mdb_less_than_or_equal(blk);
	if (mdoff != -1 && blk <= mdbcache[mdoff].endbn) {
		int buf_offset = (blk - mdbcache[mdoff].startbn) * BBSIZE;
		return mdbcache[mdoff].buffer + buf_offset;
	}
	/* BCACHE_UNLOCK(); */
	return NULL;
}

#define blk_to_word(x)	((x)/32)
#define blk_to_off(x)	((x)%32)
#define wbitmask(x)	(1 << blk_to_off(x))
#define setbit(m, x)	((int *)m)[blk_to_word(x)] |= wbitmask(x)
#define bitset(m, x)	(((int *)m)[blk_to_word(x)] & wbitmask(x))

int
mdb_greater_than(efs_daddr_t smd)
{
	int bottom = 0;
	int top = mdbcount - 1;
	int middle = (top + bottom) / 2;

	if (mdbcount == 0)
		return 0;
	if (mdbcache[top].startbn <= smd)
		return mdbcount;
	if (mdbcache[bottom].startbn > smd)
		return 0;

	/* From now on bottom <= smd < top, so at loops end we return top */
	while (bottom < middle) {
		if (mdbcache[middle].startbn <= smd) {
			bottom = middle;
		} else {
			top = middle;
		}
		middle = (top + bottom) / 2;
	}
	return top;
}

int
mdb_less_than_or_equal(efs_daddr_t smd)
{
	int bottom = 0;
	int top = mdbcount - 1;
	int middle = (top + bottom) / 2;

	if (mdbcount == 0)
		return -1;
	if (mdbcache[top].startbn <= smd)
		return top;
	if (mdbcache[bottom].startbn > smd)
		return -1;

	/* From now on bottom <= smd < top, so at loops end we return top */
	while (bottom < middle) {
		if (mdbcache[middle].startbn <= smd) {
			bottom = middle;
		} else {
			top = middle;
		}
		middle = (top + bottom) / 2;
	}
	return bottom;
}

int
insert_mdbcache(efs_daddr_t smd, efs_daddr_t emd, char *buf)
{
	int ret = 0;
	BCACHE_LOCK();
	if (mdbcount < sizeof(mdbcache)/sizeof(struct mdbcache)) {
		int insert;
		int top;

		insert = mdb_greater_than(smd);
		top = mdbcount++;
		while (insert < top) {
			mdbcache[top] = mdbcache[top - 1];
			top--;
		}
		mdbcache[insert].startbn = smd;
		mdbcache[insert].endbn = emd;
		mdbcache[insert].buffer = buf;
		ret = 1;
		mem_preserve((emd - smd + 1) * BBSIZE);
	}
	BCACHE_UNLOCK();
	return ret;
}

int
bcload(unsigned blk, char *buf)
{
	struct bcache *bc;

	if (mdbits && startmdcache <= blk && blk < endmdcache) {
		setbit(mdbits, blk - startmdcache);
		mdcount++;
		return 1;
	}

#ifdef BCDEBUG
	bcloads++;
#endif
	if (!mem_reserve(sizeof (struct bcache), PERM, 0))
		return 0;
	BCACHE_LOCK();
	bc = (struct bcache *)cachebase;
	cachesize -= sizeof (struct bcache);
	cachebase += sizeof (struct bcache);
	bcopy(buf, bc->buf, BBSIZE);
#ifdef BCDEBUG
	bcgoodloads++;
#endif
	bc->next = chead[blk % NHASHHEADS];
	bc->blk = blk;
	chead[blk % NHASHHEADS] = bc;
	BCACHE_UNLOCK();
	return 1;
}

void
bcmddump(void)
{
	efs_daddr_t blk;
	efs_daddr_t smd = startmdcache;
	efs_daddr_t emd = endmdcache;
	efs_daddr_t fblk = smd;

	static efs_daddr_t pagefreemask;

	if (!pagefreemask) {
		pagefreemask = sysconf(_SC_PAGESIZE);
		if (pagefreemask == -1) {
			exit(1);
		}
		pagefreemask = ~(pagefreemask / BBSIZE - 1);
	}

	if ((mdcount * 2 > emd - smd + 1)
	    && insert_mdbcache(smd, emd - 1, mdcache))
	{
		startmdcache = endmdcache = 0;
		mdcache = 0;
		free(mdbits);
		mdbits = 0;
		return;
	}
	/* so that bcload actually does something */
	startmdcache = endmdcache = 0;

	/* transfer blocks to the page cache */
	for (blk = smd; mdbits && blk < emd; blk++)
		if (bitset(mdbits, blk - smd)
		    && !bcload(blk, &mdcache[(blk - smd) * BBSIZE]))
		{
			efs_daddr_t nfreeblks = blk - fblk;
			nfreeblks &= pagefreemask;
			freetmpmap(&mdcache[(fblk - smd) * BBSIZE],
					nfreeblks * BBSIZE);
			mem_release(nfreeblks * BBSIZE);
			fblk += nfreeblks;
			bcload(blk, &mdcache[(blk - smd) * BBSIZE]);
		}

	/* free remaining resources */
	freetmpmap(&mdcache[(fblk - smd) * BBSIZE], (blk - fblk) * BBSIZE);
	mem_release((blk - fblk) * BBSIZE);
	mdcache = 0;
	free(mdbits);
	mdbits = 0;
}

/* New recovery option for saving files in a named directory. Has to be able
 * to handle indirect extents. We duplicate error messages in a logfile, since
 * there may be many such messages and the user will want a record of which
 * saved files are bad. (This option is intended to be a last-ditch
 * resort for very badly corrupted filesystems)!
 */
FILE *copylog = NULL;

int
copyoff(DINODE *dp, int ino)
{
	char namebuf[100];
	int fd, i, bytes, chunk;
	struct extent *ep;
	struct extent *indirs = NULL;
	int rcode = 0;

	/* we will use the "indirect extent" buffer that phase 1 has now
	 * finished with; clear its len flag to be sure no further reads
	 * hit it.
	 */
	saveextentlen = 0;
	
	if (!copylog)
	{
		bzero(namebuf, 100);
		sprintf(namebuf, "%s/fsck_errlog", savedir);
		if ((copylog = fopen(namebuf, "w")) == NULL)
		{
			iderrexit("Can't create files in %s: %s\n",
				  savedir, strerror(errno));
		}
	}

	if (dp->di_nx <= EFS_DIRECTEXTENTS)
		ep = dp->di_u.di_extents;
	else if ((ep = indirs = getindirs(dp, ino)) == NULL)
	{
		fprintf(copylog,"Can't get disk addressing info to save file # %d\n", ino);
		fprintf(stderr,"Can't get disk addressing info to save file # %d\n", ino);
		return (-1);
	}

	bzero(namebuf, 100);
	sprintf(namebuf, "%s/%d", savedir, ino);
	if ((fd = creat(namebuf, 0666)) < 0)
	{
		iderrexit("Can't create files in %s\n", savedir);
	}

	printf("Saving inode # %d in directory %s\n\n", ino, savedir);
	io_report = 0; /* shut up builtin error prints */

	bytes = dp->di_size;
	for (i = 0; i < dp->di_nx; i++, ep++)
	{
		if ((ep->ex_bn == 0)      || 
#ifndef AFS
		    (ep->ex_magic != 0)	  ||
#endif
		    (ep->ex_length == 0))
		{
			fprintf(copylog,"fsck: bad extent in inum %d, copy truncated.\n", ino);
			fprintf(stderr,"fsck: bad extent in inum %d, copy truncated.\n", ino);
			goto out;
		}
		if (grabextent(ep->ex_bn, ep->ex_length) == NO)
		{
			fprintf(copylog,"fsck: read failure saving inum %d, copy truncated.\n", ino);
			fprintf(stderr,"fsck: read failure saving inum %d, copy truncated.\n", ino);
			rcode = -1;
			goto out;
		}
		chunk = ep->ex_length << BBSHIFT;
		if (chunk > bytes)
			chunk = bytes;

		if (writeextent() == NO)
		{
			iderrexit("Write failure in %s\n", savedir);
		}
		bytes -= chunk;
		if (!bytes) break;
	}

out:
	io_report = 1;
	if (indirs)
		free ((char *)indirs);
	close(fd);
	return (rcode);
}

struct extent *
getindirs(DINODE *dp, int ino)
{
	unsigned numindirs, bufsize, totsize;
	struct extent *ep, *indirs;
	char *dst;

	ep = dp->di_u.di_extents;
	numindirs = ep->ex_offset;
	if ((numindirs > EFS_DIRECTEXTENTS) || (numindirs == 0))
		return (NULL);
	bufsize = (dp->di_nx * sizeof(struct extent)) + BBSIZE;
	if ((indirs = (struct extent *) malloc(bufsize)) == NULL)
		return (NULL);
	bzero((char *)indirs, bufsize);
	totsize = 0;
	dst = (char *)indirs;

	io_report = 0; /* shut up builtin error prints */

	while (numindirs--)
	{
		if ((ep->ex_bn == 0)      || 
#ifndef AFS
		    (ep->ex_magic != 0)	  ||
#endif
		    (ep->ex_length == 0))
		{
			fprintf(copylog,"fsck: bad indirect extent in inum %d, copy truncated.\n", ino);
			fprintf(stderr,"fsck: bad indirect extent in inum %d, copy truncated.\n", ino);
			break;
		}
		totsize += (ep->ex_length << BBSHIFT);
		if (totsize > bufsize)
		{
			fprintf(copylog,"fsck: bad indirect extent in inum %d, copy truncated.\n", ino);
			fprintf(stderr,"fsck: bad indirect extent in inum %d, copy truncated.\n", ino);
			break;
		}
		if (bread(dst, (efs_daddr_t)ep->ex_bn, ep->ex_length) == NO) 
		{
			fprintf(copylog,"fsck: indirect extent read failure saving inum %d, copy truncated.\n", ino);
			fprintf(stderr,"fsck: indirect extent read failure saving inum %d, copy truncated.\n", ino);
			break;
		}
		dst += (ep->ex_length << BBSHIFT);
		ep++;
	}
	io_report = 1;
	return (indirs);
}

/********************DHXX temp debug *************************/

char *
blkis(efs_daddr_t blk, char *buf)
{
	if (isinode(blk))
		return "INODE";
	else if (isdir(buf))
		return "DIR";
	else return "?";
}

int
isinode(efs_daddr_t blk)
{
	int blkincg;

	if (blk < 2)
		return 0;
	if (blk < superblk.fs_firstcg)
		return 0;
	blk -= superblk.fs_firstcg;
	blkincg = blk % superblk.fs_cgfsize;
	if (blkincg >= superblk.fs_cgisize)
		return 0;
	else return 1; 
}

int
isdir(char *buf)
{
	unsigned short *cow;

	cow = (unsigned short *)buf;
	if (*cow == 0xbeef)
		return 1;
	else return 0;
}

/*
 * get_sysmem() returns current system free memory but caps it at 8MB on
 * 	if the real number is smaller and 2GB-1 if it's larger.
 */
int
get_sysmem(void)
{
	int smem;
	struct rminfo rmem_info;

        if (sysmp(MP_SASZ, MPSA_RMINFO) < 0) {
                fprintf(stderr,
			"fsck: Warning - sysinfo interface not supported\n");
		/*
		 * Err on the side of caution by returning the smallest
		 * memory acceptable (8MB).  This should never happen.
		 */
		return 0x800000;
        }

	/* find the system memory size, we will use this later to set limits
	 * to the amount of memory we malloc, to avoid thrashing. Fsck is
	 * normally run in init state 1, so not much else should be using
	 * memory at the time. To allow for the rather stupid possibility of
	 * a maximal fs (8 gig) on a minimal memory system, we set this to
	 * at least 8 meg: in that case it's gonna thrash, but nothing we
	 * can do about that!
	 *
	 * NOTE: This is a change from how this used to work.  We're using
	 * the sysmp interface to find the amount of free memory when we're
	 * started up.  The program used to assume 2 MB for the kernel!  I
	 * won't presume to predict how much memory the kernel will use.
	 * The reason this matters is that we swap running fsck on some machines
	 * after crashes thereby trashing the dump.
	 */
	sysmp(MP_SAGET, MPSA_RMINFO, (char *)&rmem_info, sizeof(rmem_info));

	/*
	 * We have machines with more than 2GB of memory now.  A signed int can
	 * only hold 2GB-1 so we need to catch that case and return a maximum
	 * of 2GB-1. 
	 */
	if (rmem_info.freemem > (0x80000000UL / getpagesize()))
		return (0x7fffffff);

	smem = getpagesize() * rmem_info.freemem;

	/*
	 * See the comment above.  We need at least 8 MB for large filesystems.
	 */
	if (smem < 0x800000) {
	    /* Here's where we're likely to swap.  Warn the user. */
	    fprintf(stderr,
		"fsck: Warning - Low free memory, swapping likely\n");
	    return 0x800000;
	}

	return smem;
}

struct child_admin {
	int	pid;
	int 	musage;
	int	passno;
	int	rofs;
	char	name[MAXPATH];
};


/* fsck_from_fstab(): handles the no-argument case, including any
 * parallelization. 
 * To keep track of memory used by child processes, we maintain a table 
 * of child_admin structures; this avoids multiple scans of the fstab too.
 * (Note: we actually use this table to build the list of filesystems
 * to check even in the non-parallel case).
 */

struct child_admin *cad_table = NULL;
int numfs;

void
fsck_from_fstab(void)
{
	FILE *fp;
	int fstabpass = 0;
	int childpid;
	int logfd;
	int i;
	struct child_admin *ca;
	struct statfs s;

	if ((fp = setmntent(fstab,"r")) == NULL)
		iderrexit("Can't open fstab file: %s\n",fstab);
	numfs = countargs(fp);
	endmntent(fp);

	/* countargs notes the highest pass no (if any) in the fstab;
	 * we'll sanity check this to be on the safe side. */

	if (maxfstabpass > 20)
		maxfstabpass = 0;

	/* If parallel, check space for logs; insist on at least  
	 * 4 blox per fs... */

	if (multflag)
	{
		if ((statfs("/etc", &s, sizeof(s), 0) < 0) ||
			(s.f_bfree < (numfs * 4)))
		{
			printf("\n  fsck: insufficient space in root for logs. Abandoning parallel operation.\n");
			multflag = 0;
		}
	}

nextpass:
	for (i = 0, ca = cad_table; i < numfs; i++, ca++)
	{
		if (maxfstabpass && (ca->passno != fstabpass))
			continue;

		if (sbsetup(ca->name, ca->rofs) != YES)
			continue;
forkfail:
		if (!multflag)
		{
			banner(ca->name);
			check(ca->name);
			continue;
		}

		/* Parallel launching: sbsetup() has left the calculated 
		 * memory requirements in 'memsize'.  If there's not enough 
		 * memory free to start this check, wait until enough is free.
		 */

		while (memsize > (system_memory - child_memory))
			reap();

		/* Create the logfile for this check */

		logfd = makelogfd(ca->name);
		if (logfd < 0)
		{
			while (child_memory)
				reap();
			fprintf(stderr,"Fsck: Can't make logfile for %s. Abandoning parallel mode.\n", ca->name);
			multflag = 0;
			goto forkfail;

		}

		child_memory += memsize;
		childpid = fork();
		if (childpid == -1)
		{
			child_memory -= memsize;
			while (child_memory)
				reap();
			fprintf(stderr,"Fsck: fork failed. Abandoning parallel mode.\n");
			multflag = 0;
			goto forkfail;
		}

		if (childpid) 
		{
		/* Parent. Register the child process */

			banner(ca->name);
			ca->pid = childpid;
			ca->musage = memsize;
			closefiles();
			close(logfd);
			continue;
		}
		else
			child_launch(ca->name, logfd);
	}

	while (child_memory)
		reap();

	if (multflag && maxfstabpass)
	{
		if (fstabpass < maxfstabpass)
		{
			fstabpass++;
			goto nextpass;
		}
	}
	printf("\n");
	exit(0);
}

/*
 * Count number of entries in the fstab to be checked. 
 * Make child_admin entry for each fs we'll be checking.
 * Note max passno as we go.
 */
int
countargs(FILE *fp)
{
	int cnt = 0;
	struct mntent *me;
	char *name;
	struct stat statbuf, rstatbuf;
	struct child_admin *ca;
	char *p = NULL;

	/* we'll allocate 10 child_admin structs initially, expanding
	 * later if necessary */

	int tabsize = 10;

	if ((cad_table = (struct child_admin *)calloc(tabsize, 
	    sizeof(struct child_admin))) == NULL)
	{
		iderrexit("Can't get memory\n");
	}

	while (me = getmntent(fp)) {

		if ((strcmp(MNTTYPE_EFS, me->mnt_type) != 0) ||
		    (hasmntopt(me, MNTOPT_NOFSCK) != NULL))
			continue;

		/* If checking in parallel, we assume it's startup boot
		 * and we should ignore anything with a 'noauto' option.
		 */

		if (multflag && hasmntopt(me, MNTOPT_NOAUTO))
			continue;

		/* Safety precaution: unless specified explicitly
		 * (in which case we don't go thru this loop), we
		 * do NOT want to be checking root!
		 */

		if ((strcmp(me->mnt_dir, "/") == 0)) continue;

		/*
		 * we prefer to check the raw device.
		 */
		if (((name = hasmntopt(me, MNTOPT_RAW)) != NULL) &&
		    ((name = strchr(name, '=')) != NULL)) 
		{
			/*
			 * Ensure that any given raw dev is the
			 * same actual disk as the block name, if not, 
			 * use the block name to avoid grinding up the
			 * wrong disk if someone has an incorrect raw=
			 */

			name++;
			if (stat(me->mnt_fsname, &statbuf) < 0)
				continue;
			if (p = strchr(name, ','))
				*p = '\0';
			if ((stat(name, &rstatbuf) < 0) ||
			    (statbuf.st_rdev != rstatbuf.st_rdev))
				name = me->mnt_fsname;
		} else {
			name = me->mnt_fsname;
		}

		cnt++;
		if (cnt > tabsize)
		{
			tabsize += 10;
			if ((cad_table = (struct child_admin *)realloc((char *)cad_table, tabsize * sizeof(struct child_admin))) == NULL)
			{
				iderrexit("Can't get memory\n");
			}
		}
		ca = &cad_table[cnt - 1];
		ca->passno = me->mnt_passno;
		ca->pid = 0;
		ca->musage = 0;
		ca->rofs = hasmntopt(me, MNTOPT_RO) != NULL;
		bzero(ca->name, MAXPATH);
		strcpy(ca->name, name);
		if (p) {
			*p = ',';
			p = NULL;
		}
		if (me->mnt_passno > maxfstabpass)
			maxfstabpass = me->mnt_passno;
	}
	return(cnt);
}

/* reap(): wait for a child to terminate.
 * Report its status, return its memory.
 */

void
reap(void)
{
	int i, pid, status;
	struct child_admin *ca;

	pid = wait(&status);
	for (i = 0, ca = cad_table; i < numfs; i++, ca++)
	{
		if (ca->pid == pid)
		{
			child_memory -= ca->musage;
			status = (status >> 8) & 0xff;
			printf("\nfsck: %s completed: ",ca->name);
			if (status == 0)
				printf("OK.\n");
			else if (status == FIXDONECODE)
			{
				printf("FIXED: log is in %s/%s.\n",
					multlogdir, dname(ca->name));
			}
			else
			{
				printf("FAILED: log is in %s/%s.\n",
					multlogdir, dname(ca->name));
			}
			break;
		}
	}
}

/* child_launch(): redirect output to logfile and run the check */

void
child_launch(char *name, int logfd)
{
	in_child = 1;
	fflush(stdout);
	fflush(stderr);
	dup2(logfd, 1);
	dup2(logfd, 2);

	check(name);
	if (fixdone)
		exit(FIXDONECODE);
	else
		exit(0);
}

/* dname(): derive last component of device name from full path. Assumes
 * a null-terminated absolute pathname.  Returns a pointer into the given 
 * argument string.
 */

char *
dname(char *name)
{
	char *p;

	p = name;
	while (*p) p++;
	while (*p != '/') p--;
	p++;
	return (p);
}

/* makelogfd(): creat's the parallel log file for the given name, returning
 * the fd.
 */

int
makelogfd(char *name)
{
	int logfd;
	char logname[MAXPATH];
	struct stat statbuf;

	if (stat(multlogdir, &statbuf) < 0) 
	{
		mkdir(multlogdir, 0755);
	}
	bzero(logname, MAXPATH);
	sprintf(logname, "%s/%s", multlogdir, dname(name));
	logfd = creat(logname, 0644);
	return (logfd);
}

/* logclean(): for parallel bootup fsck; puts a note in the logfile that
 * the given fs was clean, to avoid possible confusion with leftover
 * logfiles from earlier boots.
 */

static char cleanmsg[] = "Filesystem was clean on bootup.\n";

void
logclean(char *name)
{
	int logfd;

	if ((logfd = makelogfd(name)) < 0)
		return;
	write(logfd, cleanmsg, sizeof(cleanmsg));
	close(logfd);
}

/* banner: prints the "fsck: checking.. " message */

void
banner(char *name)
{
	printf("\n%c fsck: checking %s", id, name);
	if (nflag) 
		printf(" (NO WRITE).");
	if ((strncmp(superblk.fs_fname, "noname", 6)) ||
	   (strncmp(superblk.fs_fpack, "nopack", 6)))
	{
		printf(" Name: %.6s Volume: %.6s\n",
			superblk.fs_fname, superblk.fs_fpack);
	}
	else
		printf("\n");
}


#ifdef AFS
/*
 *      unrawname - If cp is a raw device file, try to figure out the
 *      corresponding block device name by removing the r if it is where
 *      we think it might be.
 */
char *
unrawname(char *cp)
{
        char *dp = strrchr(cp, '/');
        struct stat stb;

        if (dp == 0)
                return (cp);
        if (stat(cp, &stb) < 0)
                return (cp);
        if ((stb.st_mode&S_IFMT) != S_IFCHR)
                return (cp);

        /*  Check for AT&T Sys V disk naming convention
         *    (i.e. /dev/rdsk/c0d0s3).
         *  If it doesn't fit the Sys V naming convention and
         *    this is a series 200/300, check for the old naming
         *    convention (i.e. /dev/rhd).
         *  If it's neither, just return the name unchanged.
         */
        while ( dp >= cp && *--dp != '/' );
        if ( *(dp+1) == 'r' )
                (void)strcpy(dp+1, dp+2);
        else {
                dp = strrchr(cp, '/');
                if ( *(dp+1) == 'r' )
                        (void)strcpy(dp+1, dp+2);
        }
        return (cp);
}
#endif /* AFS */

struct map_head {
	void *addr;
	int len;
	struct map_head *next;
} *map_list;

void 
freealltmpmap(void)
{
	struct map_head *mp;
	for (mp = map_list; mp; ) {
		struct map_head *mpsv = mp;
		munmap(mp->addr, mp->len);
		mp = mp->next;
		free(mpsv);
	}
	map_list = 0;
}

void
freetmpmap(void *addr, int len)
{
	struct map_head *mp, *mpprev = 0;
	BCACHE_LOCK();

	for (mp = map_list; mp && mp->addr != (void *)addr; mp = mp->next)
		mpprev = mp;
	if (!mp) {
		BCACHE_UNLOCK();
		return;
	}
	if (len < mp->len) {
		mp->len -= len;
		mp->addr = (char *)mp->addr + len;
		munmap(addr, len);
		BCACHE_UNLOCK();
		return;
	}
	if (mpprev)
		mpprev->next = mp->next;
	else
		map_list = mp->next;
	munmap(mp->addr, mp->len);
	BCACHE_UNLOCK();
	free(mp);
}

int zero_fd = -1;

void *
tmpmap(int size)
{
	struct  map_head *new;

	new = malloc(sizeof(*new));
	if (!new)
		return 0;

	if (zero_fd == -1)
		zero_fd = open("/dev/zero", 2);
	new->addr = mmap(0, size, PROT_READ|PROT_WRITE,
					MAP_PRIVATE, zero_fd, 0);
	if (new->addr == (void *)-1) {
		free(new);
		return 0;
	}

	new->len = size;
	BCACHE_LOCK();
	new->next = map_list;
	map_list = new;
	BCACHE_UNLOCK();
	return new->addr;
}

int
choose_nsprocs(void)
{

	/*
	 * Since LV is no longer supported, just return the default of 1 sproc
	 */
	if (nsprocs != 0)
		return nsprocs;
	else
		return 1;
}

void
infanticide(void)
{
	int i;

	sigignore(SIGCLD);

	for (i = 1; i < nsprocs; i++)
		if (pids[i] > 0)
			kill(pids[i], SIGINT);

	if (mp_barrier)
		free_barrier(mp_barrier);
	mp_barrier = 0;
}

/*
 * for large transfers, we reduce the size recursively until the kernel
 * accepts the request
 */
int
eread(int fd, char *buf, int size)
{
	int bytes = size * BBSIZE;
	int nextsize = size / 2;
	int result;

	result = read(fd, buf, bytes);
	if (result == bytes)
		return YES;
	if (result != -1)
		return NO;
	/* the value 192 is not important, but the system should be able
	 * to read in a full extent, shouldn't it? */
	if (size < 192 || errno != ENOMEM)
		return NO;
	if (eread(fd, buf, nextsize) == YES
	    && eread(fd, buf + nextsize*BBSIZE, size - nextsize) == YES)
		return YES;
	return NO;
}

/*
 * for large transfers, we reduce the size recursively until the kernel
 * accepts the request
 */
int
ewrite(int fd, char *buf, int size)
{
	int bytes = size * BBSIZE;
	int nextsize = size / 2;
	int result;

	result = write(fd, buf, bytes);
	if (result == bytes)
		return YES;
	if (result != -1)
		return NO;
	if (size < 192 || errno != ENOMEM)
		return NO;
	/* the value 192 is not important, but the system should be able
	 * to read in a full extent, shouldn't it? */
	if (ewrite(fd, buf, nextsize) == YES
	    && ewrite(fd, buf + nextsize*BBSIZE, size - nextsize) == YES)
		return YES;
	return NO;
}
