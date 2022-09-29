
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.3 $"

/* lvinfo: provides information about current state of logical volumes */

#include <sys/types.h>
#include <sys/dkio.h>
#include <sys/dvh.h>
#include <sys/stat.h>
#include <sys/major.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h> 
#include <sys/lv.h>
#include "lvutils.h"
#include <sys/sysmacros.h>  

#define MAX_LV_DEVS 100

extern int errno;
char *progname;
struct lv *lvp;
int lvbufsize = sizeof(struct lv) + 
		((MAX_LV_DEVS - 1) * sizeof(struct physdev));
struct lvioarg lvarg;

main(argc, argv)
int argc;
char **argv;
{
int i;
	progname = argv[0];

	if ((lvp = (struct lv *)malloc(lvbufsize)) == NULL)
	{
		fprintf(stderr,"%s: can't get memory\n", progname);
		exit(-1);
	}
	lvarg.lvp = lvp;
	lvarg.size = lvbufsize;

	if (argc == 1)
		scan_rdev();
	else
		for (i = 1; i < argc; i++)
			show_vol(argv[i], 1);
	exit(0);
}

/* scan_rdev(): scan through /dev/rdsk and pass the names to show_vol
 * with non-verbose option; this will ignore anything that isn't an
 * initialised volume.
 */

scan_rdev()
{
register DIR *dir;
register struct dirent *de;

	if ((dir = opendir("/dev/rdsk/")) == NULL)
	{
		fprintf(stderr,"%s: can't open /dev/rdsk\n",progname);
		exit(-1);
	}
	while ((de = readdir(dir)) != NULL)
		show_vol(de->d_name, 0);
}

/* show_vol(): expects the argument to be the name of a logical volume 
 * device, either full pathname or last component (lvx)
 * If verbose, it warns of uninitialized volumes and non-lv names, 
 * otherwise silently ignores them.
 */

show_vol(name, verbose)
char *name;
int verbose;
{
int fd;
struct stat sb;
static char path[MAXDEVPATHLEN];
char *fullname;
int i, minor_num;

	if (!strncmp(name, "lv", 2))
	{
		minor_num = atoi(name + 2);
		bzero(path, MAXDEVPATHLEN);
		sprintf(path,"%s%s", RAWPATHPREF, name);
		fullname = path;
	}
	else if (!strncmp(name, RAWPATHPREF, RAWPATHLEN) &&
		 !strncmp((name + RAWPATHLEN), "lv", 2))
	{
		minor_num = atoi(name + RAWPATHLEN + 2);
		fullname = name;
	}
	else 
	{
		if (verbose)
			fprintf(stderr,"%s: %s is not a logical volume device name.\n",
				progname, name);
		return;
	}

	if ((minor_num < 0) || (minor_num > 255))
	{
		if (verbose)
			fprintf(stderr,"%s: %s is not a valid logical volume name.\n",
			progname, name);
		return;
	}

	fd = open(fullname, O_RDONLY);
	if (fd < 0)
	{
		if (verbose)
			fprintf(stderr,"%s: can't open %s.\n", progname,  fullname);
		return;
	}
	if (fstat(fd, &sb) < 0)
	{
		if (verbose)
			fprintf(stderr,"%s: can't stat %s\n", progname,  fullname);
		return;
	}
	if ((sb.st_mode & S_IFMT) != S_IFCHR)
	{
		if (verbose)
			fprintf(stderr,"%s: %s is not a char device.\n", progname,  fullname);
		return;
	}
	if (major(sb.st_rdev) != LV_MAJOR)
	{
		if (verbose)
			fprintf(stderr,"%s: %s is not a logical volume device.\n", progname,  fullname);
		return;
	}
	if (ioctl(fd, DIOCGETLV, &lvarg) < 0)
	{
		if (verbose)
			fprintf(stderr,"%s: %s is not initialized.\n", progname, name);
		return;
	}

	printf("# lv%d size is %d blocks\n", minor_num, lvp->l_size);
	printf("lv%d: :", minor_num);
	printf("stripes=%d:",lvp->l_stripe);
	if (lvp->l_stripe > 1)
		printf("step=%d:",lvp->l_gran);
	printf("devs= ");
	for (i = 0; i < lvp->l_ndevs; i++)
	{
		printpath(lvp->pd[i].dev, 0, 0);
		if (i != (lvp->l_ndevs -1))
			printf(", \\\n\t");
	}
	printf("\n\n");
}

