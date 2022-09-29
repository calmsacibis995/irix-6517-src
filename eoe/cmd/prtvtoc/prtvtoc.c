
/* Copyright 1995, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)prtvtoc:prtvtoc.c	1.4" */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/prtvtoc/RCS/prtvtoc.c,v 1.31 1996/08/03 03:11:10 sbarr Exp $"

/*
 * prtvtoc.c
 *
 * Print a disk partition map ("VTOC"). 
 */

#include <stdio.h>
#include <mountinfo.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/dkio.h>
#include <sys/sema.h>       /* for semaphores in efs_sb.h */
#include <errno.h>
#include <sys/sysmacros.h>
#include <sys/fs/efs.h>
#include <diskinfo.h>
#include <mntent.h>
#include <stdlib.h>
#include <string.h>

/* internal fstab/mtab info */ 

typedef struct fstab_item_s {
    int source;
    dev_t rdev;
    char fstype;
    char bdevname[PATH_MAX];
    char options[PATH_MAX];
    char mount_point[PATH_MAX];
    int freq;
    int passno;
    struct fstab_item_s *next;
} fstab_item_t;

typedef struct fstab_s {
    int count;
    fstab_item_t *list;
} fstab_t;

/*
 * Macros.
 */
#define	strsize(str)	\
		(sizeof(str) - 1)	/* Length of static character array */
#define iddn(x)		\
		(x / NPARTAB)		/* Drive number */
#define idslice(x)	\
		(x & ~(NPARTAB - 1))	/* partition number */

/*
 * Definitions.
 */
#define	reg	register		/* Convenience */
#define	uint	unsigned int		/* Convenience */
#define	ulong	unsigned long		/* Convenience */
#define	ushort	unsigned short		/* Convenience */
#define	DECIMAL	10			/* Numeric base 10 */
#define	FSTABSZ	1024			/* Fstab size limit (plus one) */
#define	HEX	16			/* Numeric base 16 */

#define TABSOURCE_NONE  0
#define TABSOURCE_MTAB  1
#define TABSOURCE_FSTAB 2
#define TABSOURCE_BOTH  3

/*
 * Disk freespace structure.
 */
typedef struct {
	ulong	fr_start;		/* Start of free space */
	ulong	fr_size;		/* Length of free space */
} Freemap;


/*
 * Internal functions.
 */
static Freemap	*findfree(register struct volume_header *);
static int	prtvtoc(char *);
static void	putfree(register struct volume_header *, register Freemap *);
static void	puttable(register struct volume_header *,
			 register Freemap *, char *, char *);
static char	*syserr(void);
static void	usage(void);
static void	warn(register char *, register char *);
static int	rootlabel(char *, char *);
struct volume_header *readvh(char *);

int          display_enhanced_info(void);
fstab_t      *new_fstab(void);
fstab_item_t *new_fstab_item(void);
char         *fstab_get_rawdev(char *);
fstab_item_t *fstab_lookupbydev(fstab_t *, int, dev_t);
char         *fstab_mountpointbydev(fstab_t *, int, dev_t);
int          fstab_init(fstab_t **);
char         *partition_name(int partition);

/*
 * External variables.
 */
extern int	errno;			/* System error code */
extern char	*sys_errlist[];		/* Error messages */
extern int	sys_nerr;		/* Number of sys_errlist[] entries */
extern int	optind;			/* Argument index */
extern char	*optarg;		/* Option argument */

/*
 * Static variables.
 */
static short	fflag;			/* Print freespace shell assignments */
static short	hflag;			/* Omit headers */
static short	sflag;			/* Omit all but the column header */
static short    eflag;                  /* all vol headers + fsys + overlaps */
static short    aflag;                  /* show all volume headers in system */
static short    mflag;                  /* show only mounted partitions */
static char	*fstab = "/etc/fstab";	/* Fstab pathname */
static char	*myname;		/* Last qualifier of arg0 */
static mnt_check_state_t *mntcheck_state;
static fstab_t           *fstab_state;
static dev_t    vh_dev;
static char     vh_fullname[PATH_MAX];

main(ac, av)
int		ac;
reg char	**av;
{
	char	rl[PATH_MAX];
	reg int	idx;

	myname = av[0];

	if (mnt_check_init(&mntcheck_state) == -1) {
	    fprintf(stderr,
		    "%s: mnt_check_init() failure, exiting.\n", myname);
	    exit(1);
	}

	fstab_init(&fstab_state);

	/* If no arguments, we will default to the root disk. */
	if (ac == 1){
		/*
		 * Call rootlabel() first so that prtvtoc
		 * will print out the root device name (not /dev/root)
		 * if it can find it.  If it can't find it,
		 * it will just use /dev/root.
		 */
		(void) rootlabel("/dev/root", rl);
		printf("Printing label for root disk\n\n");
		idx = prtvtoc(rl);
		exit(idx);
	}

	while ((idx = getopt(ac, av, "aemfhst:")) != EOF)
		switch (idx) {
		case 'a':
			++aflag;
			break;
		case 'e':
			++eflag;
			break;
		case 'm':
			++mflag;
			break;
		case 'f':
			++fflag;
			break;
		case 'h':
			++hflag;
			break;
		case 's':
			++sflag;
			break;
		case 't':
			fstab = optarg;
			break;
		default:
			usage();
		}

	/* enhanced multi-volume reports */
	if (aflag || eflag || mflag)
	        exit(display_enhanced_info());


	if (optind >= ac)
		usage();

	idx = 0;
	while (optind < ac)
		idx |= prtvtoc(av[optind++]);
	exit(idx);
	/* NOTREACHED */
}


/*
 * Qsort() key comparison of partitions by starting sector numbers.
 */
static int
partcmp(const void *one, const void *two)
{
	return ((*(struct partition_table **)one)->pt_firstlbn -
		(*(struct partition_table **)two)->pt_firstlbn);
}


/*
 * findfree()
 *
 * Find free space on a disk. Returns a pointer to the static result.
 */
static Freemap *
findfree(register struct volume_header *vh)
{
	register struct partition_table	*part;
	register struct partition_table	*nextpart;
	register struct partition_table	**list;
	struct partition_table	*sorted[NPARTAB + 1];
	register Freemap		*freeidx;
	static Freemap		freemap[NPARTAB + 1];
	unsigned fullsize;
	long hisofar;

	list = sorted;
	for (part = vh->vh_pt; part < &vh->vh_pt[NPARTAB]; ++part) {
		if(part->pt_type == PTYPE_VOLUME)
			fullsize = part->pt_nblks;
		else if (part->pt_nblks > 0)
			*list++ = part;
	}
	
	*list = 0;
	qsort((char *) sorted, (uint) (list - sorted), sizeof(*sorted),
		partcmp);
	freeidx = freemap;
	freeidx->fr_start = 0;
	freeidx->fr_size = 0;
	hisofar = 0;
	for (list = sorted; (part = *list) && (nextpart = *(list + 1)); ++list) 
	{

	/* We want to determine if there are gaps between partitions, 
	   and record them in the freemap if so. The volume header
	   always starts at 0, so there's no need to look for a gap
	   below the lowest partition. Algorithm: if there's a gap
	   between end of current partition & start of next, AND start of
	   next is higher than highest accounted-for block so far, gap consists
	   of space between end of current (OR highest so-far accounted for
	   if greater), & start of next. 
	   Otherwise, no gap on this pass of the loop.
	   (This had to be changed from what was originally here: that
	   gave NEGATIVE gaps for overlapping partitions!!) */

	    if ((part->pt_firstlbn + part->pt_nblks) > hisofar)
	    	hisofar = part->pt_firstlbn + part->pt_nblks;

	    if (nextpart->pt_firstlbn > hisofar)
	    {
		freeidx->fr_start = hisofar;
		freeidx->fr_size = nextpart->pt_firstlbn - hisofar;
		freeidx++;
	    }
	}

	/* finally: is there a gap after the highest partition? */

	if ((fullsize > (part->pt_firstlbn + part->pt_nblks)) &&
	    (fullsize > hisofar)){
		freeidx->fr_start = part->pt_firstlbn + part->pt_nblks;
		freeidx->fr_size = fullsize - freeidx->fr_start; 
		freeidx++;
		}

	freeidx->fr_start = freeidx->fr_size = 0;
	return (freemap);
}

/*
 * prtvtoc()
 *
 * Read and print a VTOC.
 */
static int
prtvtoc (char *name)
{
reg Freemap	*freemap;
struct volume_header *vh;
char fullname[PATH_MAX];
char devname[PATH_MAX];

/* Name may be given as the full pathname (/dev/rdsk/ips0d0vh etc) or just
   as the last component. We will check to see if the given name exists
   as a file. If not, we will prefix it with /dev/rdsk & try again. */

	if (access(name, 0) == 0){
		strncpy(fullname, name, PATH_MAX);
		if (strncmp(fullname, "/dev/rdsk", 9) &&
		    strncmp(fullname, "/dev/dsk", 8)) {
			(void) rootlabel(fullname, devname);
		} else {
			strcpy(devname, fullname);
		}
	}
	else {
		strcpy(fullname, "/dev/rdsk/");
		strncpy((fullname + 10), name, PATH_MAX - 10);
		strcpy(devname, fullname);
	}

        strcpy(vh_fullname, fullname);

	if ((vh = readvh(fullname)) == 0) {
	    return (1);
	}

	freemap = findfree (vh);
	if (fflag) {
		putfree (vh, freemap);
	}
	else {
		puttable(vh, freemap, fullname, devname);
	}
	return (0);
}

/*
 * putfree()
 *
 * Print shell assignments for disk free space. FREE_START and FREE_SIZE
 * represent the starting block and number of blocks of the first chunk
 * of free space. FREE_PART lists the unassigned partitions.
 */
static void
putfree(register struct volume_header *vh, register Freemap *freemap)
{
	register struct partition_table *part;
	register Freemap	*freeidx;
	register int		idx;

	printf ("FREE_START=%d FREE_SIZE=%d", freemap->fr_start, 
		freemap->fr_size);
	for (freeidx = freemap; freeidx->fr_size; ++freeidx)
		;
	printf (" FREE_COUNT=%d FREE_PART=", (long)(freeidx - freemap));
	for (idx = 0, part = vh->vh_pt; part < &vh->vh_pt[NPARTAB]; 
	    part++, idx++) {
		if (part->pt_nblks <= 0) {
			printf ("%x ", idx);
		}
	}
	printf("\n");
}

/*
 * puttable()
 *
 * Print a human-readable VTOC.
 */
static void
puttable(register struct volume_header *vh,
	 register Freemap *freemap,
	 char *name,
	 char *devname)
{
	register struct partition_table *part;
	register struct device_parameters *dp;
	register unsigned	idx;
	register char 	*pname;
	mnt_partition_table_t *ptbl_walk;

	dp = &vh->vh_dp;
	if (!hflag && !sflag) {
		printf ("* %s", name);
		printf (" (bootfile \"%s\")\n", vh->vh_bootfile);
		/* if zero, say BBSIZE, since old fx didn't fill it in. Avoids alarming
			people */
		printf ("* %7d bytes/sector\n", dp->dp_secbytes?dp->dp_secbytes:BBSIZE);
		if (freemap->fr_size) {
			printf ("* Unallocated space:\n*\tStart\t      Size\n");
			do {
				printf ("*  %10d\t%10d\n", freemap->fr_start,
					freemap->fr_size);
			} while ((++freemap)->fr_size);
			printf ("*\n");
		}
	}

	/* force load of vh */
	mnt_find_mount_conflicts(mntcheck_state, vh_fullname);
	for (ptbl_walk = mntcheck_state->partition_list;
	     ptbl_walk;
	     ptbl_walk = ptbl_walk->next) {

	    if (!ptbl_walk->ptable_loaded)
		continue;
	    if (!ptbl_walk->dev_entry)
		continue;
	    if (!ptbl_walk->dev_entry->valid)
		continue;

	    if (!strncmp(ptbl_walk->dev_entry->pathname,
			 vh_fullname,
		         strlen(ptbl_walk->dev_entry->pathname)-2)) {
		break;
	    }
	}

	if (!hflag)
		printf ("Partition  Type  Fs   Start: sec    Size: sec   Mount Directory\n");
	for (idx = 0, part = vh->vh_pt; part < &vh->vh_pt[NPARTAB]; 
	    part++, idx++) {
		int fsvalid;
		if (part->pt_nblks <= 0)
			continue;

		/* construct the char special file name for this partition */
		pname = partname(devname, idx);

		printf ("%2d\t%7s  ", idx, pttype_to_name(part->pt_type));

		if(part->pt_type == PTYPE_EFS || part->pt_type == PTYPE_XFS) {
			printf("%s",
				(pname && (fsvalid = has_fs(pname, part->pt_nblks))) ?
				"yes" : "   ");
		} else {
			printf("   ");
			fsvalid = 0;
		}

		printf ("%12d ",part->pt_firstlbn);
		printf ("%12d  ",part->pt_nblks);
		if (ptbl_walk) {
		    char *fsys;
		    dev_t dev;
		    int owned = 0;
		    int owner;

		    dev = ptbl_walk->ptable[idx].device;
		    if (ptbl_walk->ptable[idx].devptr->owner_dev) {
			dev = ptbl_walk->ptable[idx].devptr->owner_dev;
			owned = 1;
		    }

		    fsys = fstab_mountpointbydev(fstab_state, 
						 TABSOURCE_MTAB,
						 dev);
		    if (fsys) {
			if (owned)
			    printf(" [%s]", fsys);
			else
			    printf(" %s", fsys);
		    } else {
			owner = ptbl_walk->ptable[idx].devptr->owner;
			if (owner == MNT_OWNER_SWAP) {
			    printf(" swap");
			} else if (owner == MNT_OWNER_RAW) {
			    printf(" rawdata");
			}
		    }
		}
		printf ("\n");
	}
}

/*
 * syserr()
 *
 * Return a pointer to a system error message.
 */
static char *
syserr(void)
{
	return (errno <= 0 ? "No error (?)"
	    : errno < sys_nerr ? sys_errlist[errno]
	    : "Unknown error (!)");
}

/*
 * usage()
 *
 * Print a helpful message and exit.
 * Note that rawdisk can be /dev/blah/anypart (or even just the dks0d# part)
 */
static void
usage(void)
{
	fprintf (stderr,"Usage:	%s [[-efhms] [-t fstab]] [rawdiskname]\n",
		 myname);
	fprintf (stderr,
		 "\noptions:\n"
		 "  -f   show freespace on volume\n"
		 "  -h   don't display headers\n"
		 "  -s   limit informative messages\n"
		 "  -t   use alternate fstab (single header mode only)\n"
		 " (options for displaying all volume headers on a system)\n"
		 "  -a   show all disk volumes\n"
		 "  -e   show all disk volumes (extended format)\n"
		 "  -m   print only partitions from mounted volumes\n\n");
	exit(1);
}

/*
 * warn()
 *
 * Print an error message.
 */
static void
warn(register char *what, register char *why)
{
	fprintf (stderr, "%s: %s: %s\n", myname, what, why);
}


/*
 * rootlabel takes the name of a device that is not in /dev/dsk
 * or /dev/rdsk and finds its equivalent in /dev/rdsk.  The
 * name is given in the parameter "name," and the full pathname
 * of the equivalent entry is returned in the parameter "devname."
 * If the function succeeds, it returns 0 and devname has the
 * value of the equivalent /dev/rdsk/entry.  If the function
 * fails, it returns -1 and name is copied to devname.
 */
int
rootlabel(char *name, char *devname)
{
	DIR		*dirp;
	struct dirent	*direntp;
	struct stat	name_statbuf;
	struct stat	dev_statbuf;
	char		path[PATH_MAX];
	int		ret;

	strcpy(devname, name);
	if (stat(name, &name_statbuf) < 0) {
		return -1;
	}
	if ((dirp = opendir("/dev/rdsk")) == NULL) {
		return -1;
	}
	strcpy(path, "/dev/rdsk/");
	ret = -1;
	while ((direntp = readdir(dirp)) != NULL) {
		strncpy((path + 10), direntp->d_name, 30);
		if (stat(path, &dev_statbuf) < 0) {
			continue;
		}
		if (dev_statbuf.st_rdev == name_statbuf.st_rdev) {
			strcpy(devname, path);
			ret = 0;
			break;
		}
	}
	closedir(dirp);
	return(ret);
}
	 	


/* Readvh obtains the volume header specified by the argument name. It
   returns a pointer to the volume header in static storage. */
struct volume_header *
readvh(char *name)
{
	int fd;
	static struct volume_header vh;
	char *rawname;

	/* Transform to equivalent raw pathname
	   since getdiskheader expects a raw dev. */
		
	rawname = findrawpath(name);
	if (!rawname) {
		warn(name, syserr());
		return (0);
	}

	if ((fd = open(rawname, O_RDONLY)) < 0)
	{
		warn(name, syserr());
		return 0;
	}

	if(getdiskheader(fd, &vh) == -1) 
	{
		warn (name, "Can't get volume header");
		(void) close(fd);
		return (0);
	}

	(void) close(fd);
	return &vh;
}

/**********************************************************************/

/* multi-volume reports */

int display_enhanced_info(void)
{
    fstab_item_t *fstab_walk;
    mnt_partition_table_t *ptbl_walk;
    mnt_device_entry_t *dentry;
    int which_part;
    int count;
    char owner[32];
    char *fsys;
    dev_t dev;
    char fsname[1024];
    int found;
    int pt_count = 0;

    /* this forces all volheaders to be read */
    count = mnt_find_existing_mount_conflicts(mntcheck_state);

    /* print volheaders & owners */

    for (ptbl_walk = mntcheck_state->partition_list;
	 ptbl_walk;
	 ptbl_walk = ptbl_walk->next) {


	if (!ptbl_walk->ptable_loaded)
	    continue;
	if (!ptbl_walk->dev_entry)
	    continue;
	if (!ptbl_walk->dev_entry->valid)
	    continue;

	pt_count++;
 	/* count pt but skip display if we're only doing mounted partitions */
	if (mflag) {
	  continue;
	}

	printf("%s\n", ptbl_walk->dev_entry->pathname);
	if (!hflag)
	    printf(" pt# start    end      #blocks  type    owner\n");

	for (which_part = 0; which_part < NPARTS; which_part++) {
	    dentry = ptbl_walk->ptable[which_part].devptr;

	    if (!dentry)
		continue;
	    if (!dentry->valid)
		continue;

	    if (which_part == 8 || which_part == 10)
		owner[0] = '\0';
	    else
		strcpy(owner, mnt_string_owner(dentry->owner));

	    dev = 0;
	    if (dentry->owner_dev)
		dev = dentry->owner_dev;
	    else if (dentry->owner)
		dev = ptbl_walk->ptable[which_part].device;

	    fsys = "";
	    if (dev) {
		fsys = fstab_mountpointbydev(fstab_state, TABSOURCE_MTAB, dev);
		if (!fsys) {
		    fsys = "";
		    if (dentry->owner == MNT_OWNER_SWAP) {
			fsys = "swap";
		    } else if (dentry->owner == MNT_OWNER_RAW) {
			fsys = "rawdata";
		    }
		} else {
		    if (dentry->owner_dev) {
			sprintf(fsname, "[%s]", fsys);
			fsys = fsname;
		    }
		}
	    }
	    
	    printf(" %s %-8d %-8d %-8d %-7.7s %s\n",
		   partition_name(which_part),
		   ptbl_walk->ptable[which_part].start_lbn,
		   ptbl_walk->ptable[which_part].end_lbn,
		   ptbl_walk->ptable[which_part].end_lbn - 
		   ptbl_walk->ptable[which_part].start_lbn + 1,
		   owner,
		   fsys);
	}
    }

    if (!eflag && !mflag)
	return 0;

    /* nothing to show, then leave */
    if (pt_count == 0) {
        printf("Unable to read volume headers");
	if (geteuid() != 0) {
	    puts(", try running as root.");
	} else {
	    puts(".");
        }
        return 0;
    }

    if (!sflag)
	printf("\n%d local filesystems mounted:\n", fstab_state->count);

    /* print devices for each filesystem */

    for (fstab_walk = fstab_state->list;
	 fstab_walk;
	 fstab_walk = fstab_walk->next) {

	if (fstab_walk->source != TABSOURCE_MTAB)
	    continue;

	printf("\n%s\n", fstab_walk->mount_point);
	if (!hflag)
	    printf(" start    size              device\n");

	for (ptbl_walk = mntcheck_state->partition_list;
	     ptbl_walk;
	     ptbl_walk = ptbl_walk->next) {

	    if (!ptbl_walk->ptable_loaded)
		continue;
	    if (!ptbl_walk->dev_entry)
		continue;
	    if (!ptbl_walk->dev_entry->valid)
		continue;

	    for (which_part = 0; which_part < NPARTS; which_part++) {

		dentry = ptbl_walk->ptable[which_part].devptr;

		if (!dentry)
		    continue;
		if (!dentry->valid)
		    continue;

		found = 0;
		if (dentry->owner_dev) {
		    if (dentry->owner_dev == fstab_walk->rdev)
			found = 1;
		} else {
		    if (ptbl_walk->ptable[which_part].device == fstab_walk->rdev)
			found = 1;
		}
		if (found) {
		    fsys = dentry->pathname;
		    if (!strncmp("/dev/rdsk/", fsys, 10)) {
			fsys = fsys + 10;
		    } else if (!strncmp("/dev/dsk/", fsys, 9)) { /* not likely */
			fsys = fsys + 9;
		    }
		    strcpy(owner, mnt_string_owner(dentry->owner));

		    printf(" %-8d %-8d %-8.8s %s\n",
			   ptbl_walk->ptable[which_part].start_lbn,
			   ptbl_walk->ptable[which_part].end_lbn - 
			   ptbl_walk->ptable[which_part].start_lbn + 1,
			   owner,
			   fsys);
		}
	    }
	}
    }

    if (mflag)
	return 0;

    puts(" ");
    count = mnt_find_unmounted_partitions(mntcheck_state);
    if (count) {
	puts("Unallocated partitions:");
	mnt_plist_show(mntcheck_state, stdout, 0);
    } else {
	if (!sflag)
	    puts("No unallocated partitions detected.");
    }

    puts(" ");
    count = mnt_find_existing_mount_conflicts(mntcheck_state);
    if (count) {
	puts("Conflicts detected:");
	mnt_plist_show(mntcheck_state, stdout, 0);
    } else {
	if (!sflag)
	    puts("No conflicts detected.");
    }

    return 0;
}

/* given a partition number, return the associated char string */

char *partition_name(int partition)
{
    char s[4] = { 0, 0, 0, 0 };
    static char partition_str[5];

    partition_str[0] = '\0';

    if (partition == 8) {
	strcat(partition_str, " vh");
    } else if (partition == 10) {
	strcat(partition_str, "vol");
    } else {

	/* where the heck is sgi's itoa()? */
	if (partition < 10) {
	    s[0] = ' ';
	    s[1] = ' ';
	    s[2] = '0' + partition;
	} else {
	    s[0] = ' ';
	    s[1] = '1';
	    s[2] = '0' + (partition - 10);
	}
	strcpy(partition_str, s);
    }
    return partition_str;
}

/*******************************************************************/
 
fstab_t *new_fstab(void)
{
    fstab_t *new_item;
    if ((new_item = malloc(sizeof(fstab_t))) == NULL) {
	puts("prtvtoc: can't malloc in new_fsinfo()");
	exit(1);
    }
    new_item->count = 0;
    new_item->list = 0;

    return new_item;
}

fstab_item_t *new_fstab_item(void)
{
    fstab_item_t *new_item;
    if ((new_item = malloc(sizeof(fstab_item_t))) == NULL) {
	puts("prtvtoc: can't malloc in new_fsitem()");
	exit(1);
    }
    new_item->source = TABSOURCE_NONE;
    new_item->rdev = 0;
    new_item->fstype = 0;
    new_item->bdevname[0] = '\0';
    new_item->options[0] = '\0';
    new_item->mount_point[0] = '\0';
    new_item->freq = 0;
    new_item->passno = 0;
    new_item->next = 0;

    return new_item;
}

/* parse mount opts looking for raw device string */

char *fstab_get_rawdev(char *s)
{
    static char ret[PATH_MAX];
    char *raw;
    char *rend;

    ret[0] = '\0';
    strncpy(ret, s, PATH_MAX);

    if ((raw = strstr(s, "raw=")) != NULL) {

	if ((rend = strrchr(raw+4, ',')) != NULL) {
	    *rend = '\0';
	}
	return raw+4;
    }

    return 0;
}

/* lookup and return fstab_item based on dev_t */

fstab_item_t *fstab_lookupbydev(fstab_t *tab, int source, dev_t dev)
{
    fstab_item_t *which_item;

    for (which_item = tab->list;
	 which_item;
	 which_item = which_item->next) {

	if (source != TABSOURCE_BOTH)
	    if (which_item->source != source)
		continue;

	if (which_item->rdev == dev) {
	    return which_item;
	}
    }
    return 0;
}

/* lookup via dev_t and return char string name of mountpoint */

char *fstab_mountpointbydev(fstab_t *tab, int source, dev_t dev)
{
    fstab_item_t *item;

    item = fstab_lookupbydev(tab, source, dev);
    if (item)
	return item->mount_point;

    return 0;
}

/* incore fstab/mtab info */

int fstab_init(fstab_t **info)
{
    fstab_item_t *new_entry;
    fstab_item_t *walk;
    int skip_flag;
    /* order should match the defines */
    /* only check mtab */
    char *tabname[] = { "/etc/mtab", 0 };
    int which_tab;
    FILE *fp;
    struct mntent *mntent;
    long tab_size;
    struct stat64 sbuf;
    char *raw, *rend;
    char *devname;
    int fstype;
    char *rawpath;

    *info = new_fstab();

    for (which_tab = 0; tabname[which_tab]; which_tab++) {
	if ((fp = setmntent(tabname[which_tab], "r")) == NULL) {
	    perror(tabname[which_tab]);
	    exit(1);
	}

	if (fstat64(fileno(fp), &sbuf)) {
	    fprintf(stderr, "prtvtoc: ");
	    perror(tabname[which_tab]);
	    exit(1);
	}

	tab_size = sbuf.st_size;

	for (;;) {
	    if ((mntent = getmntent(fp)) == NULL) {
		if (ftell(fp) >= tab_size) {
		    /* it's really EOF */
		    break;
		}
		fprintf(stderr,
			"%s: %s: illegal entry\n", myname, tabname[which_tab]);

		/* ignore this entry, go to next */
		continue;
	    }

	    if (!strcmp(mntent->mnt_type, MNTTYPE_EFS)) {
		fstype = PTYPE_EFS;
	    } else if (!strcmp(mntent->mnt_type, MNTTYPE_XFS)) {
		fstype = PTYPE_XFS;
	    } else {
		/* not local (xfs, efs), then skip */
		continue;
	    }

	    /* scan current list for dups */
	    skip_flag = 0;
	    for (walk = (*info)->list; walk; walk = walk->next) {
		if (!strcmp(mntent->mnt_fsname, walk->bdevname)) {
		    skip_flag = 1;
		    break;
		}
	    }
	    /* if duplicate, skip it */
	    if (skip_flag)
		continue;

	    /* alloc new entry at put at head of list */
	    new_entry = new_fstab_item();
	    new_entry->next = (*info)->list;
	    (*info)->list = new_entry;
	    (*info)->count++;

	    new_entry->source = TABSOURCE_MTAB + which_tab;

	    new_entry->fstype = fstype;
	    new_entry->freq = mntent->mnt_freq;
	    new_entry->passno = mntent->mnt_passno;
	    if ((rawpath = findrawpath(mntent->mnt_fsname)) == NULL) {
		    strncpy(new_entry->bdevname, mntent->mnt_fsname, PATH_MAX);
	    } else {
		    strncpy(new_entry->bdevname, rawpath, PATH_MAX);
	    }
	    strncpy(new_entry->mount_point, mntent->mnt_dir, PATH_MAX);
	    strncpy(new_entry->options, mntent->mnt_opts, PATH_MAX);

	    /* default stat name to block name first */
	    devname = new_entry->bdevname;
	    if ((raw = fstab_get_rawdev(mntent->mnt_opts)) != NULL) {
		devname = raw;
	    }

	    if (stat64(devname, &sbuf) != -1) {
		new_entry->rdev = sbuf.st_rdev;
	    }
	    /* stat failure? */
	}
    }
    return 0;
}
