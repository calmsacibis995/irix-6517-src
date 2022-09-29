/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "$Revision: 1.28 $"
/*	Swap administrative interface
 *	Used to add/delete/list swap devices in use by paging.
 */

#include	<stdio.h>
#include	<fcntl.h>
#include        <mountinfo.h>
#include	<limits.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<strings.h>
#include	<sys/types.h>
#include	<dirent.h>
#include	<sys/swap.h>
#include	<sys/stat.h>
#include	<sys/mkdev.h>
#include	<ustat.h>
#include	<getopt.h>
#include	<diskinfo.h>
#include	<mntent.h>
#include	<sys/capability.h>

#define LFLAG 1
#define DFLAG 2
#define AFLAG 3
#define SFLAG 4
#define MFLAG 5
#define UFLAG 6

extern int list(int, int);
extern int olist(void);
extern int addall(void);
extern int delall(void);
extern void getmntdef(xswapres_t *xsr);
extern int getlist(swaptbl_t **stp, int *nsw);
extern void printsz(pgno_t sz, int fw, int inblocks);
extern int doswap(int inblocks);
extern int nopt(struct mntent *mnt, char *opt, __uint64_t *val);
extern void usage(void);
extern int add(xswapres_t *xsr, int lookedup, off_t swapfile_offset);
extern int sdelete(char *path, off_t offset);
int check_overlaps(char *, off_t);

int safe = 1;		/* perform fs safety checks */
int Debug = 0;
char *mntfile = MNTTAB;
int pgsz;

int
main(int argc, char **argv)
{
	off_t low;
	int flag = 0, c, ret;
	char *pathname;
	xswapres_t xsr;
	char *sptr;
	int fulllist = 0;
	int inblocks = 0;
	int newlist = 0;
	off_t swapfile_offset = -1;

	if (argc < 2) {
		usage();
		exit(1);
	}
	pgsz = getpagesize();

	xsr.sr_pri = -1;
	xsr.sr_maxlength = -1;
	xsr.sr_length = -1;
	xsr.sr_vlength = -1;
	xsr.sr_start = -1;
	while ((c = getopt(argc, argv, "nbfM:Duimsldap:v:")) != EOF) {
		switch(c) {
		case 'n':
			newlist = 1;
			break;
		case 'f':
			fulllist = 1;
			break;
		case 'b':
			inblocks = 1;
			break;
		case 'i':
			safe = 0;
			break;
		case 'p':
			xsr.sr_pri = (char)strtol(optarg, &sptr, 0);
			if (sptr == optarg) {
				fprintf(stderr, "swap:Invalid # for priority: %s\n",
					optarg);
				exit(1);
			}
			break;
		case 'v':
			xsr.sr_vlength = strtoll(optarg, &sptr, 0);
			if (sptr == optarg) {
				fprintf(stderr, "swap:Invalid # for vlength: %s\n",
					optarg);
				exit(1);
			}
			break;
		case 'u':	/* delete all in fstab */
			if (flag) {
				usage();
				exit(1);
			}
			flag |= UFLAG;
			break;
		case 'm':	/* add all in fstab */
			if (flag) {
				usage();
				exit(1);
			}
			flag |= MFLAG;
			break;
		case 's':
			if (flag) {
				usage();
				exit(1);
			}
			flag |= SFLAG;
			break;
		case 'l':
			if (flag) {
				usage();
				exit(1);
			}
			flag |= LFLAG;
			break;
		
		case 'd':
			if (getuid() != 0) {
				fprintf(stderr, "Must be superuser to add or delete swap.\n");
				exit(1);
			}
			if (flag) {
				usage();
				exit(1);
			}
			flag |= DFLAG;
			break;
		
		case 'a':
			if (getuid() != 0) {
				fprintf(stderr, "Must be superuser to add or delete swap.\n");
				exit(1);
			}
			if (flag) {
				usage();
				exit(1);
			}
			flag |= AFLAG;
			break;
		case 'D':
			Debug++;
			break;
		case 'M':
			mntfile = optarg;
			break;
		}
	}
	if (flag == LFLAG) {
		if ((argc - optind) != 0) {
			usage();
			exit(1);
		}
		if (newlist)
			ret = list(fulllist, inblocks);
		else
			ret = olist();
	} else if (flag == DFLAG) {
		if ((argc - optind) == 1) {
			pathname = argv[optind];
			low = -1;
		} else if ((argc - optind) == 2) {
			pathname = argv[optind];
			low = strtoll(argv[++optind], &sptr, 0);
			if (sptr == argv[optind]) {
				fprintf(stderr, "swap:Invalid # for start: %s\n",
					argv[optind]);
				exit(1);
			}
		} else {
			usage();
			exit(1);
		}
		ret = sdelete(pathname, low);
	} else if (flag == AFLAG) {
		if ((argc - optind) < 1) {
			usage();
			exit(1);
		}
		xsr.sr_name = argv[optind++];
		if ((argc - optind) >= 1) {
			xsr.sr_start = strtoll(argv[optind], &sptr, 0);
			swapfile_offset = xsr.sr_start;
			if (sptr == argv[optind]) {
				fprintf(stderr, "swap:Invalid # for start: %s\n",
					argv[optind]);
				exit(1);
			}
			optind++;
		}
		if ((argc - optind) >= 1) {
			xsr.sr_length = strtoll(argv[optind], &sptr, 0);
			if (sptr == argv[optind]) {
				fprintf(stderr, "swap:Invalid # for length: %s\n",
					argv[optind]);
				exit(1);
			}
			optind++;
		}
		if ((argc - optind) >= 1) {
			usage();
			exit(1);
		}
		ret = add(&xsr, 0, swapfile_offset);
	} else if (flag == UFLAG) {
		ret = delall();
	} else if (flag == MFLAG) {
		ret = addall();
	} else if (flag == SFLAG) {
		ret = doswap(inblocks);
	} else {
		usage();
		exit(1);
	}
	exit(ret);
	/* NOTREACHED */
}

void
usage(void)
{
	printf("usage:\tswap -l[nfb]\n");
	printf("\tswap -m\n");
	printf("\tswap -u\n");
	printf("\tswap -s[b]\n");
	printf("\tswap -d <file name / lswap#> [<low block>]\n");
	printf("\tswap -a [-i] [-p pri] [-v vlen] <file name> [<low block> [<nbr of blocks>]]\n");
}

int
list(int fulllist, int inblocks)
{
	register int i;
	int error, nswap;
	swaptbl_t *st;
	swapent_t *se;
	mode_t type;
	struct stat statbuf;
	int maxlen = 0;

	if (error = getlist(&st, &nswap))
		return error;
	else if (nswap == 0) {
		printf("No swap devices configured\n");
		return(0);
	}
        se = st->swt_ent;
        for (i = 0; i < nswap; i++, se++) {
		int l;
		if ((l = strlen(se->ste_path)) > maxlen)
			maxlen = l;
	}
	maxlen += 2;

	if (fulllist)
		printf(" # %-*s   dev    pri swaplo    pswap     free  maxswap    vswap\n",
				maxlen, "path");
	else
		printf(" # %-*spri    pswap     free  maxswap    vswap\n",
				maxlen, "path");

        se = st->swt_ent;
        for (i = 0; i < nswap; i++, se++) {
                printf("%2d %-*s", se->ste_lswap, maxlen, se->ste_path);
		if (fulllist) {
			if (stat(se->ste_path, &statbuf) < 0)
				printf("   ?,?    ");
			else {
				type = (statbuf.st_mode & (S_IFBLK | S_IFCHR));
				printf("%4d,%-5d",
					type ? major(statbuf.st_rdev) : major(statbuf.st_dev),
					type ? minor(statbuf.st_rdev) : minor(statbuf.st_dev));
			}
		}
                printf("  %1d", se->ste_pri);
		if (fulllist)
			printf(" %6lld", (off64_t)se->ste_start);
		printsz(se->ste_pages, 8, inblocks);
		printsz(se->ste_free, 8, inblocks);
		printsz(se->ste_maxpages, 8, inblocks);
		printsz(se->ste_vpages, 8, inblocks);
                if (se->ste_flags & ST_INDEL)
                        printf(" INDEL");
                if (se->ste_flags & ST_STALE)
                        printf(" ESTALE");
                if (se->ste_flags & ST_EACCES)
                        printf(" EACCES");
                if (se->ste_flags & ST_IOERR)
                        printf(" IOERR");
		printf("\n");
        }
	return 0;
}

/*
 * For compatability reasons we must continue to ship a old stupid
 * format for list.
 * The -n flag causes a better one to be output..
 */
int
olist(void)
{
	register int i;
	int error, nswap;
	swaptbl_t *st;
	swapent_t *se;
	mode_t type;
	struct stat statbuf;

	if (error = getlist(&st, &nswap))
		return error;
	else if (nswap == 0) {
		printf("No swap devices configured\n");
		return(0);
	}

        printf("lswap %-8s\t   dev    pri swaplo   blocks     free  maxswap    vswap\n", "path");

        se = st->swt_ent;
        for (i = 0; i < nswap; i++, se++) {
                printf("%5d %s\n\t\t", se->ste_lswap, se->ste_path);
                if (stat(se->ste_path, &statbuf) < 0)
                        printf("   ?,?    ");
                else {
                        type = (statbuf.st_mode & (S_IFBLK | S_IFCHR));
                        printf("%4d,%-5d",
                                type ? major(statbuf.st_rdev) : major(statbuf.st_dev),
                                type ? minor(statbuf.st_rdev) : minor(statbuf.st_dev));
		}
                printf("  %1d %6lld", se->ste_pri, (off64_t)se->ste_start);
		printsz(se->ste_pages, 8, 1);
		printsz(se->ste_free, 8, 1);
		printsz(se->ste_maxpages, 8, 1);
		printsz(se->ste_vpages, 8, 1);
                if (se->ste_flags & ST_INDEL)
                        printf(" INDEL");
                if (se->ste_flags & ST_STALE)
                        printf(" ESTALE");
                if (se->ste_flags & ST_EACCES)
                        printf(" EACCES");
                if (se->ste_flags & ST_IOERR)
                        printf(" IOERR");
		printf("\n");
        }
	return 0;
}

int
getlist(swaptbl_t **stp, int *nsw)
{
	char *path;
	register int i;
	int nswap;
	swaptbl_t *st;
	swapent_t *se;

	if ((nswap = swapctl(SC_GETNSWP)) < 0) {
		perror("swap:swapctl(SC_GETNSWAP)");
		return(1);
	} else if (nswap == 0) {
		*nsw = 0;
		return(0);
	}

	st = malloc(sizeof(*st) + ((nswap-1) * sizeof(swapent_t)));
	st->swt_n = nswap;
	path = malloc(nswap * PATH_MAX);

	for (i = 0, se = st->swt_ent; i < nswap; i++, se++) {
		se->ste_path = path;
		path += PATH_MAX;
	}

	if ((nswap = swapctl(SC_LIST, st)) < 0) {
		perror("swap:swapctl(SC_LIST)");
		return(3);
	}
	*nsw = nswap;
	*stp = st;
	return 0;
}

/*
 * Print swap stats - note that since we can allocate up to the SUM
 * of phys mem and swap, that the amount reserved can exceed the
 * size of swap ..
 * We opt to print 'logical' swap - thus these figures include the amount
 * of physical memory and virtual swap available
 * Note that the 'reserved' amount is really reserved minus allocated
 * since ALL blocks that are allocated have already been reserved.
 */
int
doswap(int inblocks)
{
	auto off_t tot, free, resv, lmax;
	off_t nalloc;

#define topg(a) \
		((pgno_t) (((long long)(a) * 512) / pgsz))
	swapctl(SC_GETSWAPTOT, &tot);
	swapctl(SC_GETFREESWAP, &free);
	swapctl(SC_GETRESVSWAP, &resv);
	swapctl(SC_GETLSWAPTOT, &lmax);
        nalloc = tot - free;
	printf("total:");
	printsz(topg(nalloc), 1, inblocks);
	printf(" allocated +");
	printsz(topg(resv - nalloc), 1, inblocks);
	printf(" add'l reserved =");
	printsz(topg(resv), 1, inblocks);
	if (inblocks)
		printf(" blocks used,");
	else
		printf(" bytes used,");
	printsz(topg(lmax - resv), 1, inblocks);
	if (inblocks)
		printf(" blocks available\n");
	else
		printf(" bytes available\n");
	return 0;
}

int
sdelete(char *path, off_t offset)
{
	register swapres_t *si;
	swapres_t swpi;
	unsigned long lswap;
	char *ptr;
	cap_t ocap;
	cap_value_t cap_swap_mgt = CAP_SWAP_MGT;

	lswap = strtoul(path, &ptr, 0);
	if (ptr != path) {
		/* its a number - use it as a logical swap index */
		ocap = cap_acquire(1, &cap_swap_mgt);
		if (swapctl(SC_LREMOVE, lswap) < 0) {
			cap_surrender(ocap);
			fprintf(stderr, "swap:swap delete failed for logical swap # ");
			perror(path);
			return(3);
		}
		cap_surrender(ocap);
	} else {
		if (offset == -1)
			offset = 0;
		si = &swpi;
		si->sr_name = path;
		si->sr_start = offset;

		ocap = cap_acquire(1, &cap_swap_mgt);
		if (swapctl(SC_REMOVE, si) < 0) {
			cap_surrender(ocap);
			fprintf(stderr, "swap:swap delete failed for ");
			perror(path);
			return(3);
		}
		cap_surrender(ocap);
	}
	return(0);
}

int
add(xswapres_t *xsr, int lookedup, off_t swapfile_offset)
{
	char *path = xsr->sr_name;
	char npath[PATH_MAX];
	cap_t ocap;
	cap_value_t cap_swap_mgt = CAP_SWAP_MGT;

	if (*path != '/') {
		/* make into full path */
		if (getcwd(npath, PATH_MAX) &&
		    ((strlen(npath) + strlen(path) + 2) < PATH_MAX)) {
			strcat(npath, "/");
			strcat(npath, path);
			path = npath;
			xsr->sr_name = npath;
		}
	}
	if (safe)
		if (check_overlaps(path, swapfile_offset)) {
			fprintf(stderr, "swap:swapadd ignored for %s\n",
				path);
			return 2;
		}

	if (Debug) {
		printf("Would have added <%s>\n", xsr->sr_name);
		return 0;
	}

	/* pick up any defaults from fstab */
	if (!lookedup)
		getmntdef(xsr);

	/*
	 * provider any base level defaults for things not
	 * specified 
	 */
	if (xsr->sr_maxlength == -1)
		xsr->sr_maxlength = xsr->sr_length;
	if (xsr->sr_vlength == -1)
		xsr->sr_vlength = xsr->sr_maxlength;
	if (xsr->sr_start == -1)
		xsr->sr_start = 0;

	ocap = cap_acquire(1, &cap_swap_mgt);
	if (swapctl(SC_SGIADD, xsr) < 0) {
		cap_surrender(ocap);
		fprintf(stderr, "swap:swapadd failed for ");
		perror(path);
		return(3);
	}
	cap_surrender(ocap);
	return(0);
}

/*
 * Check for overlap with mounted fs 
 * Returns 1 if something overlaps or is mounted.  This includes
 * partitions reserved in /etc/fstab via 'ignore' entries to raw
 * devices.
 */
int
check_overlaps(char *special, off_t swapfile_offset)
{
	struct stat sbuf;
	mnt_check_state_t *check_state;
	int check_return;

	if (stat(special, &sbuf) == -1)
		return(0);

	if (!S_ISBLK(sbuf.st_mode))
		return(0);

	/*
	 * if there's an offset skip the mount check, someone might
         * actually want an fs on the device (miniroot).
	 */
	if (swapfile_offset > 0) {
	        return(0);
	}

	/* call libdisk to check for overlaps, reservations, etc. */
	if (mnt_check_init(&check_state) == -1) {
	        fprintf(stderr, "swap: unable to init mount check routines, "
			"skipping %s\n", special);
		return(0);
	}

	check_return = mnt_find_mount_conflicts(check_state, special);

	if (check_return > 0) {
	        if (mnt_causes_test(check_state, MNT_CAUSE_MOUNTED)) {
		    (void) fprintf(stderr, "swap: %s is already in use.\n",
				   special);
		} else if (mnt_causes_test(check_state, MNT_CAUSE_OVERLAP)) {
		    (void) fprintf(stderr, "swap: %s overlaps a partition already in use.\n", special);
		} else {
		    mnt_causes_show(check_state, stderr, "swap");
		}
		(void) fprintf(stderr, "\n");
		(void) fflush(stderr);
		mnt_plist_show(check_state, stderr, "swap");
		(void) fprintf(stderr, "\n");
	}

	mnt_check_end(check_state);

	if (check_return > 0)
	        return(1);

	/* if -1 then we just ingore and let the devopen fail */
	return(0);
}

/*
 * addall - add all swap devices found in fstab
 * Note that the libdisk uses the mnttab also - so we must
 * save all info and collect them all before adding anything
 */
int
addall(void)
{
	struct stat mnttab_stat;
	off_t mnttab_size;
	int count;
	FILE *mnttab;
	struct mntent *mntp;
	xswapres_t *xsr = NULL;
	xswapres_t *xsrp;
	__uint64_t val;
	int error, rv, i, nent = 0;
	off_t swapfile_offset = -1;

	mnttab = setmntent(mntfile, "r");
	if (mnttab == NULL) {
		(void) fprintf(stderr, "swap: ");
		perror(mntfile);
		exit(1);
	}
	if (fstat(fileno(mnttab), &mnttab_stat) == -1) {
		(void) fprintf(stderr, "swap: ");
		perror(mntfile);
		exit(1);
	}
	mnttab_size = mnttab_stat.st_size;

	for (count = 1; ; count++) {
		if ((mntp = getmntent(mnttab)) == NULL) {
			if (ftell(mnttab) >= mnttab_size)
				break;		/* it's EOF */
			(void) fprintf(stderr,
			    "mount: %s: illegal entry on line %d\n",
			    mntfile, count);
			continue;
		}
		if ((strcmp(mntp->mnt_type, MNTTYPE_SWAP) != 0) ||
		    hasmntopt(mntp, MNTOPT_NOAUTO) ||
		    (strcmp(mntp->mnt_fsname, "/dev/swap") == 0) ) {
			continue;
		}
		nent++;
		xsr = realloc(xsr, nent * sizeof(*xsr));
		xsrp = xsr + (nent - 1);
		xsrp->sr_name = strdup(mntp->mnt_fsname);
		xsrp->sr_pri = -1;
		xsrp->sr_maxlength = -1;
		xsrp->sr_length = -1;
		xsrp->sr_vlength = -1;
		xsrp->sr_start = -1;
		error = 0;
		if ((rv = nopt(mntp, MNTOPT_SWPLO, &val)) > 0)
			xsrp->sr_start = swapfile_offset = val;
		else if (rv < 0)
			error++;
		if ((rv = nopt(mntp, MNTOPT_LENGTH, &val)) > 0)
			xsrp->sr_length = val;
		else if (rv < 0)
			error++;
		if ((rv = nopt(mntp, MNTOPT_MAXLENGTH, &val)) > 0)
			xsrp->sr_maxlength = val;
		else if (rv < 0)
			error++;
		if ((rv = nopt(mntp, MNTOPT_VLENGTH, &val)) > 0)
			xsrp->sr_vlength = val;
		else if (rv < 0)
			error++;
		if ((rv = nopt(mntp, MNTOPT_PRI, &val)) > 0)
			xsrp->sr_pri = val;
		else if (rv < 0)
			error++;
		if (error) {
			fprintf(stderr, "swap: <%s> ignored due to errors.\n",
				xsrp->sr_name);
			nent--;
		}
				
	}
	(void) endmntent(mnttab);

	rv = 0;
	for (i = 0; i < nent; i++)
		rv += add(xsr + i, 1, swapfile_offset);
	return rv;
}

int
delall(void)
{
	register int i;
	int error, nswap;
	swaptbl_t *st;
	swapent_t *se;
	int rv = 0;
	cap_t ocap;
	cap_value_t cap_swap_mgt = CAP_SWAP_MGT;

	if (error = getlist(&st, &nswap))
		return error;
	else if (nswap == 0)
		return(0);

        se = st->swt_ent;
        for (i = 0; i < nswap; i++, se++) {
		if (se->ste_flags & (ST_INDEL|ST_BOOTSWAP|ST_NOTREADY))
			continue;

		ocap = cap_acquire(1, &cap_swap_mgt);
		if (error = swapctl(SC_LREMOVE, se->ste_lswap)) {
			fprintf(stderr, "swap: remove of logical swap %d failed:",
				se->ste_lswap);
			perror("");
			rv++;
		}
		cap_surrender(ocap);
	}
	return rv;
}

void
getmntdef(xswapres_t *xsr)
{
	struct stat mnttab_stat;
	off_t mnttab_size;
	int count;
	FILE *mnttab;
	struct mntent *mntp;
	__uint64_t val;
	int rv, error;

	mnttab = setmntent(mntfile, "r");
	if (mnttab == NULL) {
		(void) fprintf(stderr, "swap: ");
		perror(mntfile);
		exit(1);
	}
	if (fstat(fileno(mnttab), &mnttab_stat) == -1) {
		(void) fprintf(stderr, "swap: ");
		perror(mntfile);
		exit(1);
	}
	mnttab_size = mnttab_stat.st_size;

	for (count = 1; ; count++) {
		if ((mntp = getmntent(mnttab)) == NULL) {
			if (ftell(mnttab) >= mnttab_size)
				break;		/* it's EOF */
			(void) fprintf(stderr,
			    "swap: %s: illegal entry on entry %d\n",
			    mntfile, count);
			continue;
		}
		if ((strcmp(mntp->mnt_type, MNTTYPE_SWAP) != 0) ||
		    (strcmp(mntp->mnt_fsname, xsr->sr_name) != 0)) {
			continue;
		}
		error = 0;
		if (xsr->sr_start == -1) {
			if ((rv = nopt(mntp, MNTOPT_SWPLO, &val)) > 0)
				xsr->sr_start = val;
			else if (rv < 0)
				error++;
		}
		if (xsr->sr_length == -1) {
			if ((rv = nopt(mntp, MNTOPT_LENGTH, &val)) > 0)
				xsr->sr_length = val;
			else if (rv < 0)
				error++;
		}
		if (xsr->sr_maxlength == -1) {
			if ((rv = nopt(mntp, MNTOPT_MAXLENGTH, &val)) > 0)
				xsr->sr_maxlength = val;
			else if (rv < 0)
				error++;
		}
		if (xsr->sr_vlength == -1) {
			if ((rv = nopt(mntp, MNTOPT_VLENGTH, &val)) > 0)
				xsr->sr_vlength = val;
			else if (rv < 0)
				error++;
		}
		if (xsr->sr_pri == -1) {
			if ((rv = nopt(mntp, MNTOPT_PRI, &val)) > 0)
				xsr->sr_pri = val;
			else if (rv < 0)
				error++;
		}
		if (error) {
			fprintf(stderr, "swap: <%s> ignored due to errors.\n",
				xsr->sr_name);
			exit(1);
		}
		break;
	}
	(void) endmntent(mnttab);
	return;
}

/*
 * Return the value of a numeric option of the form foo=x, if
 * If the option is found and is valid, return 1;
 * if no option is found return 0.
 * If an invalid setting is found, return -1
 */
int
nopt(struct mntent *mnt, char *opt, __uint64_t *val)
{
        char *str;
        char *equal;

        str = hasmntopt(mnt, opt);
        if (str == NULL)
                return (0);
        equal = str + strlen(opt);
        if (*equal != '=') {
                (void) fprintf(stderr,
                    "swap: <%s> missing value for option '%s'\n",
						 mnt->mnt_fsname, str);
                return (-1);
        }
        if (sscanf(equal + 1, "%llu", val) != 1) {
                (void) fprintf(stderr,
                    "swap: <%s> illegal value for option '%s'\n", 
						 mnt->mnt_fsname, str);
                return (-1);
        }
        return (1);
}

void
printsz(pgno_t sz, int fw, int inblocks)
{
	off_t csz;
	float fsz;
	char suff;

	if (inblocks) {
		csz = sz * (pgsz / 512);
		printf(" %*lld", fw, csz);
	} else {
		if (sz < (1024*1024 / pgsz)) {
			suff = 'k';
			fsz = (float)sz * pgsz / (1024.);
		} else if (sz < (1024 * 1024 * 1024) / pgsz) {
			suff = 'm';
			fsz = (float)sz * pgsz / (1024. * 1024.);
		} else {
			suff = 'g';
			fsz = (float)sz * pgsz / (1024. * 1024. * 1024.);
		}
		printf(" %*.2f%c", fw - 1, fsz, suff);
	}

}
