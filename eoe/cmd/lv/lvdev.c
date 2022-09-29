
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

#ident "$Revision: 1.5 $"

/* subroutines for initializing, checking, & if necessary creating the
 * logical volume devices.
 */


#include <sys/types.h>
#include <sys/dkio.h>
#include <sys/stat.h>
#include <sys/dvh.h>
#include <sys/major.h>
#include <fcntl.h>
#include <ustat.h>
#include <stdio.h>
#include <sys/lv.h>
#include <sys/sysmacros.h>
#include "lvutils.h"



extern int errno;
extern char *progname;

lvdev_init(vol, lvp, override)
register struct logvol *vol;
struct lv *lvp;
int override;		/* allow/disallow re-initialization */
{
char 	rnamebuf[MAXDEVPATHLEN];
char 	bnamebuf[MAXDEVPATHLEN];
int	lvfd;
struct  lvioarg arg;
struct  ustat ust;
struct  lv testlv;
struct  stat sb;
dev_t	dev;

	bzero(bnamebuf, MAXDEVPATHLEN);
	bzero(rnamebuf, MAXDEVPATHLEN);
	sprintf(rnamebuf,"%s%s", RAWPATHPREF, vol->tabent->devname);
	sprintf(bnamebuf,"%s%s", BLOCKPATHPREF, vol->tabent->devname);
	dev = makedev(LV_MAJOR, vol->minor);
	
	/* first, check if the raw & block dev files for name exist;
	 * if not attempt to create them.
	 */

	if (stat(rnamebuf, &sb) == 0)
	{
		if ((sb.st_mode & S_IFMT) != S_IFCHR)
		{
			fprintf(stderr,"%s: %s is not a char device\n", 
				progname, rnamebuf);
			return (-1);
		}
	}
	else
	{
		if (mknod(rnamebuf, S_IFCHR | S_IRUSR | S_IWUSR , dev) < 0)
		{
			fprintf(stderr,"%s: cannot create %s\n", 
				progname, rnamebuf);
			return (-1);
		}
	}

	if (stat(bnamebuf, &sb) == 0)
	{
		if ((sb.st_mode & S_IFMT) != S_IFBLK)
		{
			fprintf(stderr,"%s: %s is not a block device\n", 
				progname, bnamebuf);
			return (-1);
		}
	}
	else
	{
		if (mknod(bnamebuf, S_IFBLK | S_IRUSR | S_IWUSR , dev) < 0)
		{
			fprintf(stderr,"%s: cannot create %s\n", 
				progname, bnamebuf);
			return (-1);
		}
	}

	if ((lvfd = open(rnamebuf, O_RDWR)) < 0)
	{
		fprintf(stderr,"%s: can't open device for %s\n",
			progname, vol->tabent->devname);
		return (-1);
	}

	/* Now some safety checks. If override's not set but the volume is
	 * already initialized, print warning message and return.
	 */

	arg.lvp = &testlv;
	arg.size = sizeof(testlv);
	if (!override && (ioctl(lvfd, DIOCGETLV, &arg) == 0))
	{
		fprintf(stderr,"%s: %s is already initialized.\n",
			progname, vol->tabent->devname);
		close(lvfd);
		/*
		 * return 1 instead of -1 so this harmless
		 * case can be detected by shell scripts.
		 */
		return (1);
	}

	/* Even if override is set, we will not proceed on a mounted fs! */

	if (ustat(dev, &ust) == 0)
	{
		fprintf(stderr,"%s: %s contains a mounted filesystem: ignored.\n",
			progname, vol->tabent->devname);
		close(lvfd);
		return (-1);
	}

	arg.lvp = lvp;
	arg.size = LVSIZE(vol->ndevs);
	if (ioctl(lvfd, DIOCSETLV, &arg) < 0)
	{
		fprintf(stderr,"%s: configuration of %s failed: %s\n",
			progname, vol->tabent->devname, strerror(errno));
		close(lvfd);
		return (-1);
	}
	close(lvfd);
	return (0);
}
