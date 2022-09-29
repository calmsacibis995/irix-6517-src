/*
 * df(1) - report disk free block counts and other statistics
 */

#ident "$Revision: 1.68 $"

#include <bstring.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <mntent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/attributes.h>
#include <sys/param.h>
#include <sys/fsid.h>
#include <sys/fstyp.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/buf.h>
#include <sys/syssgi.h>
#include <sys/uuid.h>
#include <sys/fs/xfs_fsops.h>
#include <sys/mount.h>

#include <stdarg.h>
#include <stdlib.h>
#include <diskinfo.h>
#include <invent.h>
#include <locale.h>
#include <fmtmsg.h>
#include <unistd.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>

#define	KSHIFT		10
#define	BBTOK(bb)	((bb) >> (KSHIFT - BBSHIFT))

#ifdef DEBUG
char	MTAB[] = "./mtab";
#else
char	MTAB[] = MOUNTED;
#endif

typedef long long df_blkcnt_t;
typedef long long df_filcnt_t;

char	do_free_scan;		/* force scan of freelist */
char	do_inode_stats;		/* print used and free file counts */
char	do_FStype;		/* print the VFS type */
char	Vflag;			/* print formatted command lines */
char	eflag;			/* print free file count */
char	rtflag;                 /* print xfs realtime free information */
int 	Pflag = 0;		/* xpg4: formatted header */
unsigned long mtptwidth = 19ul;	/* width of mount point field */

char	*fs_type;		/* File system type to search for */
enum	units { halfk, kb, mb, human } units = halfk;
int	local_fs_only;		/* list only local filesystems */
int	explicit_args;		/* true if arguments were given */
int 	errors = 0;

int main(int argc, char **argv);
int same_mount(char *devpath, struct mntent *mntp);
struct mntent *getmntbyid(mountid_t *midp);
char *devnm(char *file, mountid_t *midp);
char *searchdir(DIR *dirp, dev_t dev);
void put_head(void);
void put_line(struct mntent *mntp);
int stat_entry(struct mntent *mntp, struct statfs *sfbp);
int ping_nfs_server(struct mntent *mntp);
int scan_freelist(struct mntent *mntp, struct statfs *sfbp);
int bread(int fd, daddr_t bno, char *bp, int cnt);
void xfs_getrt(char *name, struct statfs *sfbp);
int usage(void);
int is_block_or_char(char *path);
int my_getmountid(const char *name, mountid_t *mid);
int my_getmntany(FILE *mtabp, struct mntent *mntent, char *mntpnt);
static void chknsnip(char *, unsigned int);


/*
 * The options specifier string argument to getopt.
 */
char	opts[] = "befhikmPlnqrtw:VF:";

char	cmd_label[16] = "UX:df";

/*
 * error messages
 */
struct dferr {
	int     flag;
	char    *cmsg;		/* catalog */
	char    *dmsg;		/* default */
} dferr[] = {
	{ SGINL_SYSERR,		_SGI_DMMX_cannotstat,
		"cannot stat %s"			},
	{ SGINL_NOSYSERR,	_SGI_DMMX_df_notfnd,
		"%s not found"				},
	{ SGINL_NOSYSERR,	_SGI_DMMX_df_NFSservnotresp,
		"%s: NFS server %s not responding"	},
	{ SGINL_NOSYSERR,	_SGI_DMMX_df_supblk,
		"unrecognizable superblock on %s"	},
	{ SGINL_NOSYSERR,	_SGI_DMMX_df_freelist,
		"cannot scan freelist for filesystem %s" },
	{ SGINL_NOSYSERR,	_SGI_DMMX_df_badfreecnt,
		"bad free count for block %ld on %s"	},
	{ SGINL_NOSYSERR,	_SGI_DMMX_df_badfreeblk,
		"bad free block %ld on %s"		},
	{ SGINL_NOSYSERR,	_SGI_DMMX_df_rdfreeblk,
		"cannot read free block %ld on %s"	},
	{ SGINL_SYSERR,		_SGI_DMMX_CannotOpen,
		"Cannot open %s"			},
	{ SGINL_NOSYSERR,	_SGI_DMMX_df_cantgetsts,
		"Cannot get status from %s"		},
	{ SGINL_NOSYSERR,	_SGI_DMMX_df_readsb,
		"Cannot read superblock on %s"		},
};

#define	ERR_STAT		0
#define	ERR_notfnd		1
#define	ERR_NFSservnotresp	2
#define	ERR_supblk		3
#define	ERR_freelist		4
#define	ERR_badfreecnt		5
#define	ERR_badfreeblk		6
#define	ERR_rdfreeblk		7
#define	ERR_open		8
#define ERR_status		9
#define	ERR_readsb		10

#define	DF_MAX_ERR		10

int
usage(void)
{
	_sgi_nl_usage(SGINL_USAGE, "UX:df",
	    gettxt(_SGI_DMMX_df_usage, "df [-%s] [-F type] [-w width] [filesystem ...]"),
	    "befhikmPlnqrtV");
	_sgi_nl_usage(SGINL_USAGE, "UX:devnm",
	    gettxt(_SGI_DMMX_devnm_usage, "devnm name"));
	return(-1);
}

/*
 * some msg prints
 */
static int
error(int nbr, char *arg1, char *arg2)
{
	register struct dferr *ep;
	register int oerrno = errno;

        /* Won't pass anything >= MAXPATHLEN */
        chknsnip(arg1, MAXPATHLEN);
        chknsnip(arg2, MAXPATHLEN);

	if(nbr > DF_MAX_ERR)
	    return(oerrno);
	ep = dferr + nbr;
	_sgi_nl_error(ep->flag, cmd_label,
	    gettxt(ep->cmsg, ep->dmsg),
	    arg1,
	    arg2);
	return(oerrno);
}

int
main(int argc, char **argv)
{
	char *cp, *basename;
	register int c, i, j;
	unsigned width;
	register struct mntent *mntp;
	register FILE *mtabp;
	mountid_t mid;
	struct mntent mntent;

	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)strncpy(cmd_label + 3, argv[0], 10);
	(void)setlabel(cmd_label);

	/*
	 * Check whether we're invoked as devnm(1M).
	 */
	basename = argv[0];
	while ((cp = strrchr(basename, '/')) != NULL) {
		if (cp[1] != '\0') {
			basename = cp + 1;
			break;
		}
		*cp = '\0';
	}
	if (strcmp(basename, "devnm") == 0) {
		while (--argc > 0) {
			if (my_getmountid(*++argv, &mid) < 0) {
				error(ERR_STAT, *argv, 0);
				errors++;
				continue;
			}
			cp = devnm(*argv, &mid);
			if (cp != NULL)
				printf("%s %s\n", cp, *argv);
			else {
				error(ERR_notfnd, *argv, 0);
				errors++;
			}
		}
		return errors;
	}

	if (getenv("HUMAN_BLOCKS") && !getenv("POSIXLY_CORRECT")) {
		units = human;
	}

	/*
	 * Invoked as df(1): process options and arguments.
	 */
	while ((c = getopt(argc, argv, opts)) != EOF) {
		switch (c) {
		  case 'k':
			units = kb;
			break;

		  case 'm':
			units = mb;
			break;

		  case 'h':
			units = human;
			break;

		  case 'P':
			Pflag = 1;
			break;

		  case 'b':
			units = halfk;
			break;

		  case 'f':
			do_free_scan = 1;
			break;

		  case 'q':
			break;

		  case 'i':
			do_inode_stats = 1;
			break;

		  case 'l':
			local_fs_only = 1;
			break;

		  case 't':	/* XXX SVID compatibility */
			break;

		  case 'F':	/* SVID */
			fs_type = optarg;
			break;

		  case 'n':	/* SVID */
			do_FStype = 1;
			break;

		  case 'e':	/* SVID */
			eflag = 1;
			break;

		  case 'V':	/* SVID */
			Vflag = 1;
			break;

		  case 'r':
			rtflag = 1;
			break;

		  case 'w':
			width = strtoul(optarg, NULL, 0);
			/* don't bother to complain if wrong */
			if(width)
				mtptwidth = width;
			break;

		  default:
			return usage();
		}
	}

	explicit_args = (optind < argc);
	if ((mtabp = setmntent(MTAB, "r")) == NULL)
		return(error(ERR_open, MTAB, 0));

	put_head();
	if (!explicit_args) {
		while ((mntp = getmntent(mtabp)) != NULL) {
			if (strcmp(mntp->mnt_type, MNTTYPE_IGNORE) == 0 ||
			   (mntp->mnt_opts != NULL && strstr(mntp->mnt_opts, "ignore")))
				continue;
			put_line(mntp);
		}
	} else {
		local_fs_only = 0;
		/*
		 * Check for argv/mtab overlap.
		 */
		for (i = optind; i < argc; i++) {
			/*
			 * If an exact match is found in the mtab file and it's
			 * type is not LOFS we have what we need. If not we must
			 * use the mountid to find a match in the mtab file.
			 */
			if ((my_getmntany(mtabp, &mntent, argv[i]) == 0) &&
			    (strcmp( mntent.mnt_type, MNTTYPE_LOFS) != 0)) {
				mntp = &mntent;
			} else if (is_block_or_char(argv[i]) ||
				(my_getmountid(argv[i], &mid) == -1) ||
				!(mntp = getmntbyid(&mid)) ||
				(is_nohide(argv[i], mntp))) {
					bzero(&mntent, sizeof(mntent));
		                        chknsnip(argv[i], MAXPATHLEN);
					mntent.mnt_fsname = argv[i];
					mntent.mnt_dir = mntent.mnt_type = NULL;
					mntp = &mntent;
			}
			put_line(mntp);
		}
	}
	endmntent(mtabp);

	return errors;
}

int
is_block_or_char(char *path)
{
	struct stat sb;

	if (stat(path, &sb) == 0) {
		return(S_ISCHR(sb.st_mode) || S_ISBLK(sb.st_mode));
	}
	return(0);
}

int 
is_nohide(char *path, struct mntent *mntp)
{
	struct stat sb1, sb2;

	if ((stat(path, &sb1) == 0) && (stat(mntp->mnt_dir, &sb2) == 0)) {
		if (sb1.st_dev != sb2.st_dev) {
			return (1);
		}
	}
	return (0);
}

struct mntent *
getmntbyid(mountid_t *midp)
{
	register FILE *mtabp;
	register struct mntent *mntp;
	mountid_t mnt_mid;

	if ((mtabp = setmntent(MTAB, "r")) == NULL)
		return NULL;
	while ((mntp = getmntent(mtabp)) != NULL) {
		if (strcmp(mntp->mnt_type, MNTTYPE_IGNORE) == 0)
			continue;
		if (my_getmountid(mntp->mnt_dir, &mnt_mid) < 0)
			continue;
		if (bcmp(midp, &mnt_mid, sizeof(mountid_t)) == 0)
			return mntp;
	}
	endmntent(mtabp);
	return NULL;
}

char *
old_device_name(char *file)
{
	register int i;
	char fsid[FSTYPSZ];
	char *name;
	struct stat stb;

	static struct devs {
		char	*dirname;
		DIR	*dirp;
	} devs[] = {		/* in order of desired search */
		/*
		 * /dev/dsk before /dev, so in the miniroot, devnm /
		 * will give /dev/dsk/dks0d1s1 not /dev/root.
		 */
		"/dev/dsk",	NULL,
		"/dev",		NULL,
		"/dev/xlv",	NULL,
	};
	static char devname[MAXPATHLEN];

	/*
	 * Assume a disk-based file system.
	 */
	if (stat(file, &stb) == -1) {
		return(NULL);
	}
	if (devs[0].dirp == NULL) {
		for (i = 0; i < sizeof(devs) / sizeof(devs[0]); i++)
			devs[i].dirp = opendir(devs[i].dirname);
	}

	for (i = 0; i < sizeof(devs) / sizeof(devs[0]); i++) {
		if (devs[i].dirp != NULL
		    && chdir(devs[i].dirname) == 0
		    && (name = searchdir(devs[i].dirp, stb.st_dev)) != NULL) {
                        /* Can't put more than sizeof(devname) in devname */
                        if((strlen(devs[i].dirname)+strlen(name))
                                                         >= sizeof(devname)){
                                return NULL;

                        }
			sprintf(devname, "%s/%s", devs[i].dirname, name);
			return devname;
		}
	}

	/* 
	 * If all else fails, see if the library routine whose job it is
	 * to convert from dev_t's to names can do it.  :->  We had to
	 * do all the earlier work for backwards compatibility, and we
	 * only try the fast and easy way as a last resort.
	 */
	{
		int length = MAXPATHLEN;

		if (dev_to_devname(stb.st_dev, devname, &length))
			return (devname);
	}

	return NULL;
}

char *
searchdir(DIR *dirp, dev_t dev)
{
	register struct dirent *dep;
	struct stat stb;

	rewinddir(dirp);
	while ((dep = readdir(dirp)) != NULL) {
		if (dep->d_ino == 0 || strcmp(dep->d_name, ".") == 0 ||
		    strcmp(dep->d_name, "..") == 0)
			continue;
		if (stat(dep->d_name, &stb) < 0)
			continue;
		if (dev != stb.st_rdev || !S_ISBLK(stb.st_mode))
			continue;
		return dep->d_name;
	}
	return NULL;
}

char *
devnm(char *file, mountid_t *midp)
{
	char *name = NULL;
	register struct mntent *mntp;

	if (mntp = getmntbyid(midp)) {
		if ((strcmp(mntp->mnt_type, MNTTYPE_XFS) == 0) ||
			(strcmp(mntp->mnt_type, MNTTYPE_EFS) == 0)) {
				name = old_device_name(mntp->mnt_dir);
				if (!name) {
					name = mntp->mnt_fsname;
				}
		} else {
			name = mntp->mnt_fsname;
		}
	} else {
		name = old_device_name(file);
	}
	return name;
}

char *unitname(enum units u, int pflag)
{
	switch (u) {
	    case halfk: return (pflag ? "512-blocks" : "blocks");
	    case kb: return (pflag ? "1024-blocks" : "kbytes");
	    case mb: return ("MB-blocks");
	    case human: return ("Size");
	}
	/* NOTREACHED */
}

/* we no longer worry quite so much if it fits in 80 columns, and
 * we no longer truncate mount points at all, and certainly not
 * shorter if -i given.  Since we never truncate at all, we allow
 * less by default, which works for most people.  With -i, on
 * 80 columns, the mount points are always wrapped, which doesn't
 * look tooLengthened the numeric fields because large filesystems
 * are now relatively common, anddoing so keeps the columns lined up
 * more often.
*/
void
put_head(void)
{
	if(do_FStype || eflag || Vflag)
		return;
	if (Pflag)  {
		printf("%-*s %s   Used   Available Capacity", 
			mtptwidth, "Filesystem", unitname(units, 1));
	} else if (units == human) {
		printf("%-*s    Type  %s   use  avail  %%use ",
		     mtptwidth, "Filesystem", unitname(units, 0));
	} else if (units == mb) {
		printf("%-*s    Type  %s    use   avail  %%use",
		     mtptwidth, "Filesystem", unitname(units, 0));
	} else {
		printf("%-*s    Type  %s     use     avail  %%use",
		     mtptwidth, "Filesystem", unitname(units, 0));
	}
	if (do_inode_stats)
		printf("   iuse  ifree %%iuse  Mounted\n");
	else
		printf(" Mounted on\n");
}

/* Return a number formated in human format.  Stolen from top. */
char	*
number(df_blkcnt_t kbytes)
{
#define	NSTR	4
	static	char	bufs[NSTR][16];
	static	int	index = 0;
	char		*p;
	char		tag;
	double		amt;
	df_blkcnt_t	mega = 1024;		/* MB = 1024 KB */
	df_blkcnt_t	giga = (1024*mega);	/* GB = 1024 MB */
	df_blkcnt_t	tera = (1024*giga);	/* TB = 1024 GB */

	p =  bufs[index];
	index = (index + 1) % NSTR;

	if (kbytes == 0) {
		strcpy(p, "0");
		return (p);
	}

	amt = (double)kbytes;

	if (amt >= tera) {
		amt /= tera;
		tag = 'T';
	} else if (amt >= giga) {
		amt /= giga;
		tag = 'G';
	} else if (amt >= mega) {
		amt /= mega;
		tag = 'M';
	} else {
		tag = 'K';
	}
	if (amt >= 10) {
		sprintf(p, "%4.0f%c", amt, tag);
	} else {
		sprintf(p, "%4.1f%c", amt, tag);
	}
	return (p);
}

#define CEIL(val) \
	( (val == (int) val) ? (int)val : ((int)val)+1)
double 
ceil_f(double val) { 
	return ( (val == (int) val) ? val : val+1);
}

#define PERCENT(amount, total) \
	ceil_f(((double)((amount) * 100.)) / ((double)(total)))
#define	ZERO_PERCENT(amt, tot) \
	((amt) = 0, (tot) = 1)

void
put_line(struct mntent *mntp)
{
	struct statfs sfsb;
	register df_blkcnt_t total;
	register df_blkcnt_t used;
	register long factor = 1;

	if (local_fs_only && strcmp(mntp->mnt_type, MNTTYPE_EFS) != 0 &&
	    strcmp(mntp->mnt_type, MNTTYPE_XFS) != 0)
		return;
	if (!explicit_args && strcmp(mntp->mnt_type, MNTTYPE_DBG) == 0)
		return;
	if (!explicit_args && strcmp(mntp->mnt_type, MNTTYPE_FD) == 0)
		return;
	if (!explicit_args && strcmp(mntp->mnt_type, MNTTYPE_HWGFS) == 0)
		return;
	if (fs_type && mntp->mnt_type && strcmp(mntp->mnt_type, fs_type) != 0)
		return;

	/*
	 * Get filesystem statistics.  If do_free_scan, get free counts
	 * the hard way.
	 */
	if (stat_entry(mntp, &sfsb) != 0)
		return;
	if (do_free_scan && scan_freelist(mntp, &sfsb) != 0)
		return;

	/*
	 * Compute total size and scale used and avail counts.
	 * Convert counts to 512-byte units if not already so.
	 * statfs block counts are in terms of fragment size, if
	 * fragment size is non-zero.
	 */
	if (sfsb.f_frsize >= BBSIZE)
		factor = (sfsb.f_frsize >> BBSHIFT);
	else if (sfsb.f_bsize >= BBSIZE)
		factor = (sfsb.f_bsize >> BBSHIFT);
	else
		factor = 1;		/* safety -XXX */
	sfsb.f_blocks *= factor;
	sfsb.f_bfree *= factor;

	total = sfsb.f_blocks;
	switch (units) {
	    case halfk:	break;
	    case human:
		/* Fall through */
	    case kb:
		sfsb.f_bfree = BBTOK(sfsb.f_bfree);
		sfsb.f_blocks = total = BBTOK(total);
		break;
	    case mb:
		sfsb.f_bfree = BBTOK(sfsb.f_bfree) >> 10;
		sfsb.f_blocks = total = BBTOK(total) >> 10;
		break;
	}

	/*
	 * handle the 'V' option for svr4
	 */

	if (Vflag) {
		printf("df -F %s %s\n", mntp->mnt_type,
			mntp->mnt_dir ? mntp->mnt_dir : mntp->mnt_fsname);
		return;
	}
	/*
	 * Print the new line, calculating percentages.
	 */
	if (sfsb.f_blocks > 0)
		used = sfsb.f_blocks - sfsb.f_bfree;
	else
		ZERO_PERCENT(used, sfsb.f_blocks);
	/* don't truncate any longer.  See comments at put_head() */

	printf("%-*s ", mtptwidth, mntp->mnt_fsname);

	if (eflag) {
		printf("%7lld\n", (df_filcnt_t)sfsb.f_ffree);
		return;
	}
	if (do_FStype)
		printf("%7s", mntp->mnt_type);
	else if (Pflag) {
		if (units == human) {
			printf("%4s %4s  %5s  %3d%%    ",
			    number(total), number(used), 
			    number(sfsb.f_bfree),
			    (int)PERCENT(used, sfsb.f_blocks));
		} else {
			printf("%10lld %8lld %7lld  %3d%%    ",
			    total, used,
			    (df_blkcnt_t)sfsb.f_bfree,
			    (int)PERCENT(used, sfsb.f_blocks));
		}
	} else {
		if (units == human) {
			printf("%7s %4s %4s  %5s  %3d%%",
			    mntp->mnt_type,
			    number(total), number(used), 
			    number(sfsb.f_bfree),
			    (int)PERCENT(used, sfsb.f_blocks));
		} else if (units == mb) {
			printf("%7s %8lld %8lld %7lld %4d",
			    mntp->mnt_type, total, used,
			    (df_blkcnt_t)sfsb.f_bfree,
			    (int)PERCENT(used, sfsb.f_blocks));
		} else {
			printf("%7s %8lld %8lld %8lld %3d",
			    mntp->mnt_type, total, used,
			    (df_blkcnt_t)sfsb.f_bfree,
			    (int)PERCENT(used, sfsb.f_blocks));
		}
	}

	if (do_inode_stats) {
		register df_filcnt_t iused;

		if (sfsb.f_files > 0)
			iused = sfsb.f_files - sfsb.f_ffree;
		else
			ZERO_PERCENT(iused, sfsb.f_files);
		if (units == human) {
			printf("  %5s %7s  %3d ",
			    number(iused/1000), 
			    number((df_filcnt_t)sfsb.f_ffree / 1000),
			    (int)PERCENT(iused, sfsb.f_files));
		} else {
			printf(" %7lld %7lld %3d ",
			    iused, (df_filcnt_t)sfsb.f_ffree,
			    (int)PERCENT(iused, sfsb.f_files));
		}
	}
	if (!do_FStype) {
		if (mntp->mnt_dir != NULL)
			printf("  %s", mntp->mnt_dir);
		else
			printf("  %-.6s", sfsb.f_fname);
	}
	putchar('\n');
}

int
stat_entry(struct mntent *mntp, struct statfs *sfbp)
{
	register char *name;
	register short fstyp;
	static char typename[FSTYPSZ];
	struct mntent *mnt2;
	mountid_t mid;

	/*
	 * Check the filesystem's root directory, or if we weren't given
	 * the root, the filesystem device.
	 */
	/*
	 * If its type is NFS, ping the server's null procedure before
	 * attempting a statfs(2) call, which might hang.
	 */
	if (!ping_nfs_server(mntp))
		return ETIMEDOUT;
	name = mntp->mnt_dir;
	if (name != NULL) {
		/*
		 * We have a mounted filesystem.  If its type is NFS, ping
		 * the server's null procedure before attempting a statfs(2)
		 * call, which might hang.
		 */
		fstyp = 0;
		if (statfs(name, sfbp, sizeof *sfbp, fstyp) == 0) {
			fstyp = sfbp->f_fstyp;
		} else {
			errors++;
			return(error(ERR_status, name, 0));
		}
		if (rtflag)
			xfs_getrt(name, sfbp);
	} else {
		/*
		 * If no directory is given, assume mntp->mnt_fsname is
		 * an unmounted filesystem device or a file contained in
		 * the filesystem of interest, and loop over type indices
		 * trying to statfs it.
		 */
		register short nfstyp;

		name = mntp->mnt_fsname;
		nfstyp = sysfs(GETNFSTYP);
		for (fstyp = 1; fstyp < nfstyp; fstyp++) {
			if (statfs(name, sfbp, sizeof *sfbp, fstyp) == 0)
				break;
		}
		if (fstyp == nfstyp) {
			/*
			 * Not an unmounted filesystem device.  Check for
			 * a file in some filesystem.
			 */
			fstyp = 0;
			if (statfs(name, sfbp, sizeof *sfbp, fstyp) == 0) {
				fstyp = sfbp->f_fstyp;
				if ((my_getmountid(name, &mid) == 0) &&
				    (mnt2 = getmntbyid(&mid))) {
					*mntp = *mnt2;
					mntp->mnt_fsname = name;
				}
			} else {
				errors++;
				return(error(ERR_status, name, 0));
			}
			if (rtflag)
				xfs_getrt(name, sfbp);
		}
	}
#ifdef DEBUG
	pstatfs(mntp->mnt_fsname, sfbp);
#endif

	/*
	 * Now get the filesystem type's name and set it in mntp if it's
	 * not already set.
	 */
	if (mntp->mnt_type == NULL) {
		if (fstyp == 0
		    || fstyp != sfbp->f_fstyp
		    || sysfs(GETFSTYP, fstyp, typename) < 0) {
			errors++;
			return(error(ERR_status, name, 0));
		}
		mntp->mnt_type = typename;
	}

	return 0;
}

/*
 * NFS-specific code.
 */
#include <netdb.h>
#include <rpc/rpc.h>
#include <sys/fs/nfs.h>
#include <sys/socket.h>

#define	WAIT	1	/* wait one second before first retransmission */
#define	TOTAL	3	/* wait no more than three seconds (two tries) */

int
ping_nfs_server(struct mntent *mntp)
{
	char *fsname, *cp;
	int len, sock;
	static int hostlen;
	static char host[MAXHOSTNAMELEN+1];
	struct hostent *hp;
	struct sockaddr_in sin;
	struct timeval tv;
	CLIENT *client;
	static enum clnt_stat clnt_stat;

	if (!mntp->mnt_type || ((strcmp(mntp->mnt_type, MNTTYPE_NFS) != 0) &&
		(strcmp(mntp->mnt_type, MNTTYPE_NFS2) != 0) &&
		(strcmp(mntp->mnt_type, MNTTYPE_NFS3) != 0) &&
		(strcmp(mntp->mnt_type, MNTTYPE_CACHEFS) != 0)))
			return 1;

	fsname = mntp->mnt_fsname;
	cp = strchr(fsname, ':');
	if (cp == NULL)
		return 1;
	len = cp - fsname;
	if (len >= sizeof host)
		return 1;
	if (len == hostlen && strncmp(fsname, host, len) == 0)
		return clnt_stat != RPC_TIMEDOUT;
	hostlen = len;
	(void) strncpy(host, fsname, len);
	host[len] = '\0';

	hp = gethostbyname(host);
	if (hp == NULL || hp->h_addrtype != AF_INET)
		return 1;
	sin.sin_family = AF_INET;
	sin.sin_port = NFS_PORT;
	bcopy(hp->h_addr, &sin.sin_addr, sizeof sin.sin_addr);
	bzero(sin.sin_zero, sizeof sin.sin_zero);

	tv.tv_sec = WAIT;
	tv.tv_usec = 0;
	sock = RPC_ANYSOCK;
	client = clntudp_create(&sin, NFS_PROGRAM, NFS_VERSION, tv, &sock);
	if (client == NULL)
		return 1;

	tv.tv_sec = TOTAL;
	clnt_stat = clnt_call(client, RFS_NULL, xdr_void, 0, xdr_void, 0, tv);
	clnt_destroy(client);
	if (clnt_stat == RPC_TIMEDOUT) {
		errors++;
		error(ERR_NFSservnotresp, mntp->mnt_dir, host);
		return 0;
	}
	return 1;
}

/*
 * EFS-dependent code.
 */
#include <values.h>
#include <sys/fs/efs_fs.h>
#include <sys/fs/efs_ino.h>
#include <sys/fs/efs_sb.h>

daddr_t efs_freescan(int fd, struct efs *sp);
daddr_t efs_freescancg(int fd, struct efs *sp, int cg);

int
scan_freelist(struct mntent *mntp, struct statfs *sfbp)
{
	register int fd;
	register daddr_t bfree;
	union {
		char		block[BBSIZE];
#ifdef Fs_MAGIC
		struct filsys	bell;
#endif /* Fs_MAGIC */
		struct efs	efs;
	} super;
#ifdef Fs_MAGIC
	daddr_t bell_freescan();
#endif /* Fs_MAGIC */
	char *rawpath;

	if (strcmp(mntp->mnt_type, MNTTYPE_EFS) != 0)
		return 0;

	rawpath = findrawpath(mntp->mnt_fsname);
	if ((fd = open(rawpath ? rawpath : mntp->mnt_fsname, 0)) < 0) {
		errors++;
		return(error(ERR_open, mntp->mnt_fsname, 0));
	}
	if (bread(fd, EFS_SUPERBB, (char *) &super, sizeof super)) {
		close(fd);
		errors++;
		return(error(ERR_readsb, mntp->mnt_fsname, 0));
	}
	if (IS_EFS_MAGIC(super.efs.fs_magic)) {
		bfree = efs_freescan(fd, &super.efs);
#ifdef FsMAGIC
	} else if (super.bell.s_magic == FsMAGIC) {
		bfree = bell_freescan(fd, &super.bell, mntp->mnt_fsname);
#endif /* FsMAGIC */
	} else {
		errors++;
		error(ERR_supblk, mntp->mnt_fsname, 0);
		close(fd);
		return -1;
	}
	close(fd);
	if (bfree < 0) {
		errors++;
		error(ERR_freelist, mntp->mnt_fsname, 0);
		return -1;
	}
#ifdef DEBUG
	fprintf(stderr, "%s freescan %ld, bfree %lld\n",
		mntp->mnt_fsname, bfree, (df_blkcnt_t)sfbp->f_bfree);
#endif
	sfbp->f_bfree = (df_blkcnt_t)bfree;
	return 0;
}

#ifdef FsMAGIC
daddr_t
bell_freescan(fd, sp, filesys)
	int fd;
	struct filsys *sp;
	char *filesys;
{
	register daddr_t freeblocks;
	register int i;
	register daddr_t b;
	struct fblk fbuf;

	freeblocks = 0;

	/*
	 * copy superblock free info to private fblk buf.
	 */
	fbuf.df_nfree = sp->s_nfree;
	bcopy((char *)sp->s_free, (char *)fbuf.df_free, sizeof sp->s_free);
	b = 0;

	for (;;) {
		if (fbuf.df_nfree == 0)
			break;

		if (fbuf.df_nfree < 0 || fbuf.df_nfree > NICFREE) {
			error(ERR_badfreecnt, (char *)b, filesys);
			return -1;
		}

		freeblocks += fbuf.df_nfree;

		b = fbuf.df_free[0];
		if (b == 0) {
			freeblocks--;
			break;
		}

		if (b < sp->s_isize || b >= sp->s_fsize) {
			error(ERR_badfreeblk, (char *)b, filesys);
			return -1;
		}

		if (bread(fd, b, (char *) &fbuf, sizeof fbuf)) {
			error(ERR_rdfreeblk, (char *)b, filesys);
			return -1;
		}
	}

	return freeblocks;
}
#endif /* FsMAGIC */

int
bread(int fd, daddr_t bno, char *bp, int cnt)
{
	register int n;
	register int nb;

	nb = (int)BTOBB(cnt);
	if ((n = readb(fd, bp, bno, nb)) != nb)
		return (n < 0) ? errno : EINVAL;
	return 0;
}

daddr_t
efs_freescan(int fd, struct efs *sp)
{
	int cg, nf, nfree = 0;

	for (cg = 0; cg < sp->fs_ncg; cg++)
		if ((nf = efs_freescancg(fd, sp, cg)) < 0)
			return -1;
		else
			nfree += nf;
	return nfree;
}

/* bn to byte offset of bm block */
#define	EFS_BMBOFF(bn)	(((bn) & 07777) >> 3)

/* bn to block offset in bitmap */
#define	EFS_BMBB(bn)	((bn) >> 12)

/* bn to bit "offset" in bitmap byte */
#define	EFS_BMBIT(bn)	((bn) & 07)

#define	EFS_BMBASE(fs)	((fs)->fs_bmblock ? (fs)->fs_bmblock : EFS_BITMAPBB)

daddr_t
efs_freescancg(int fd, struct efs *sp, int cg)
{
	int ib, nfree = 0;
	daddr_t nb, bn = EFS_CGIMIN(sp, cg) + sp->fs_cgisize;
	daddr_t lastbn = EFS_CGIMIN(sp, cg + 1) - 1;
	daddr_t bmbn = EFS_BMBASE(sp) + EFS_BMBB(bn);
	daddr_t lastbmbn = EFS_BMBASE(sp) + EFS_BMBB(lastbn);
	char b, *cp, *bp = (char *)malloc((nb = lastbmbn - bmbn + 1) * BBSIZE);
	if (bp == NULL)
		return -1;

	if (readb(fd, bp, bmbn, nb) != nb)
		return -1;

	cp = bp + EFS_BMBOFF(bn);
	b = *cp;
	b >>= EFS_BMBIT(bn);
	ib = BITSPERBYTE - EFS_BMBIT(bn);
	for (; bn <= lastbn; cp++, ib=BITSPERBYTE, b = *cp)	/* each byte */
		for (; bn <= lastbn && ib--; bn++) {		/* each bit */
			if (b & 01)
				nfree++;
			b >>= 1;
		}
	free(bp);
	return nfree;
}

/*
 * Get xfs realtime space information and add it to sfbp.
 * Don't complain if it doesn't work.
 * Don't worry if it doesn't fit in 32 bits...
 */
void
xfs_getrt(char *name, struct statfs *sfbp)
{
	int bsize;
	xfs_fsop_counts_t cnt;
	int factor;
	int fd;
	xfs_fsop_geom_t geo;

	fd = open(name, O_RDONLY);
	if (fd < 0)
		return;
	if (syssgi(SGI_XFS_FSOPERATIONS, fd, XFS_FS_GEOMETRY, (void *)0, &geo) < 0 ||
	    syssgi(SGI_XFS_FSOPERATIONS, fd, XFS_FS_COUNTS, (void *)0, &cnt) < 0) {
		close(fd);
		return;
	}
	close(fd);
	if (!geo.rtblocks)
		return;
	bsize = sfbp->f_frsize ? sfbp->f_frsize : sfbp->f_bsize;
	factor = geo.blocksize / bsize;         /* currently this is == 1 */
	sfbp->f_blocks += geo.rtblocks * factor;
	sfbp->f_bfree += (cnt.freertx * geo.rtextsize) * factor;
}

/* truncate arbitarily long arguments to a defined max value */

static void chknsnip(s, max)
char *s;
unsigned int max;
{
        char *p;

        if(!s)
                return;

        if(strlen(s) >= max) {
                p=s;
                p+=(max-1);
                *p='\0';
        }
}


#ifdef DEBUG
pstatfs(char *name, struct statfs *sfbp)
{
	fprintf(stderr, "\
statfs(%s) = {\n\
	fstyp %d,\n\
	bsize %ld,\n\
	frsize %ld,\n\
	blocks %lld,\n\
	bfree %lld,\n\
	files %lld,\n\
	ffree %lld,\n\
	fname %.6s,\n\
	fpack %.6s\n\
}\n",
	    name,
	    sfbp->f_fstyp,
	    sfbp->f_bsize,
	    sfbp->f_frsize,
	    (df_blkcnt_t)sfbp->f_blocks,
	    (df_blkcnt_t)sfbp->f_bfree,
	    (df_filcnt_t)sfbp->f_files,
	    (df_filcnt_t)sfbp->f_ffree,
	    sfbp->f_fname,
	    sfbp->f_fpack);
}
#endif

int
my_getmountid(const char *name, mountid_t *mid)
{
	struct stat s;
	int i;
	int len;
	char *p;

	if (lstat(name, &s) < 0 || !S_ISLNK(s.st_mode) || !(len = s.st_size))
		p = (char *)name;
	else if (stat(name, &s) < 0)
		p = (char *)name;
	else if (S_ISDIR(s.st_mode)) {
		p = malloc(strlen(name) + 3);
		sprintf(p, "%s/.", name);
	} else {
		p = malloc(len + 1);
		if (readlink(name, p, len) != len) {
			free(p);
			p = (char *)name;
		} else {
			p[len] = '\0';
			i = my_getmountid(p, mid);
			free(p);
			return i;
		}
	}
	i = getmountid(p, mid);
	if (p != name)
		free(p);
	return i;
}

/*
 * Look for an exact match in the mtab file based on
 * mnt_fsname or mnt_dir.
 */
int
my_getmntany(FILE *mtabp, struct mntent *mntent, char *mntpnt)
{
	struct mntent mntpref;

	bzero(&mntpref, sizeof(mntpref));
	mntpref.mnt_fsname = mntpnt;
	rewind(mtabp);
	if (getmntany(mtabp, mntent, &mntpref) == 0)
		return 0;

	/* now try the match on mnt_dir */
	mntpref.mnt_fsname = NULL;
	mntpref.mnt_dir = mntpnt;
	rewind(mtabp);
	if (getmntany(mtabp, mntent, &mntpref) == 0)
		return 0;

	return -1;
}
