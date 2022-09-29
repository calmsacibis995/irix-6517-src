
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

#ident "$Revision: 1.13 $"

/* Subroutines doing various pathname conversions for the logical
 * volume utilities.
 * These are inevitably dependent on the SGI device naming conventions!
 *
 * Therefore:
 * SEVERE UGLINESS WARNING!! SEVERE UGLINESS WARNING!
 * Lovers of portable code leave the room now!!
 *
 * We make the assumption that while new controller names (such as
 * 'ips', 'xyl', 'dks') may appear, the #d#s# format following will
 * NOT change, likewise that the /dev/dsk and /dev/rdsk locations of
 * block and character files for disk devices will not change.
 *
 * General convention note: apart from functions returning a pointer, 
 * all others return 0 for OK and -1 for error.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/dvh.h>
#include <sys/dkio.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/sysmacros.h>
#include <sys/lv.h>
#include <sys/major.h>
#include <sys/scsi.h>
#include "lvutils.h"

extern int errno;
extern char *progname;
extern int verbose;

/* Workhorse function for decomposing pathnames into controller # etc. */

static struct pathanalysis *
decompose_path(p)
char *p;
{
static struct pathanalysis pa;
int i;

	if (!strncmp(p, BLOCKPATHPREF, BLOCKPATHLEN)) 
		p += BLOCKPATHLEN;
	else if (!strncmp(p, RAWPATHPREF, RAWPATHLEN)) 
		p += RAWPATHLEN;
	else
		return (NULL);

	bzero (&pa, sizeof(pa));

	/* p is now positioned at the start of the controller name.
	 * We shall assume that controller names will never contain
	 * numeric characters, & will always be < MAXCTLRNAMELEN
	 * chars long...
	 */

	i = 0;
	while ((i < MAXCTLRNAMELEN) && !isdigit(*p))
	{
		pa.ctlrname[i++] = *p++;
	}
	if (i == MAXCTLRNAMELEN) return (NULL);

	/* Now controller number. We assume this will not be more than
	 * 3 digits.
	 */

	i = 0;
	while ((i <= 3) && isdigit(*p))
	{
		pa.ctlr *= 10;
		pa.ctlr += (*p++ - '0');
		i++;
	}
	if (i == 4) return (NULL);

	/* Now drive number. We assume this will not be more than
	 * 3 digits.
	 */

	if (*p++ != 'd') return (NULL);
	i = 0;
	while ((i <= 3) && isdigit(*p))
	{
		pa.drive *= 10;
		pa.drive += (*p++ - '0');
		i++;
	}
	if (i == 4) return (NULL);

	/* Now there can be either a partition or a lun number.
	 */
	if (*p == 'l') {
		/* Now lun number. We assume this will not be more than
		 * 2 digits.
		 */
		p++;
		i = 0;
		while ((i <= 2) && isdigit(*p))
		{
			pa.lun *= 10;
			pa.lun += (*p++ - '0');
			i++;
		}
		
		/* the lun number MUST be non 0.
		 * otherwise there would be two names for devices with a lun
		 * of 0:
		 *	dks#d#l0s#     and
		 *	dks#d#s#
		 *
		 * Irix only allows the second form
		 */
		if ((i == 3) || (pa.lun == 0) ) return (NULL);
	} else {
		pa.lun = 0;
	}

	/* Now partition. This will not be more than 2 digits, and may
	 * not exceed 15.
	 */

	if (*p++ != 's') return (NULL);
	i = 0;
	while ((i <= 2) && isdigit(*p))
	{
		pa.partition *= 10;
		pa.partition += (*p++ - '0');
		i++;
	}
	if (i == 3) return (NULL);
	if (pa.partition > 15) return (NULL);
	return (&pa);
}

/* checkformat(): tests if the given string is a plausible block device
 * pathname in /dev/dsk
 */

checkformat(p)
register char *p;
{
int part;

	if (strncmp(p, BLOCKPATHPREF, BLOCKPATHLEN)) return (-1);
	if (decompose_path(p) == NULL) return (-1);
	return (0);
}

/* pathtopart: returns the partition implied by the given name. */

pathtopart(p)
register char *p;
{
struct pathanalysis *pa;
	
	if ((pa = decompose_path(p)) == NULL) return (-1);
	return (pa->partition);
}

/* Pathtovhpath: takes a pathname of a block device & deduces the pathname
 * of the relevent volume header. Returns a pointer to this in static
 * storage. Note that we use the raw dev for the volume header: so that
 * if writing, it's synchronous & we get reliable notification of errors.
 */

char * 
pathtovhpath(p)
register char *p;
{
static char vhnbuf[MAXDEVPATHLEN];
struct pathanalysis *pa;
	
	if ((pa = decompose_path(p)) == NULL) return (NULL);

	bzero(vhnbuf, MAXDEVPATHLEN);
	if (pa->lun) {
		sprintf(vhnbuf,"/dev/rdsk/%s%dd%dl%dvh", 
			pa->ctlrname, pa->ctlr, pa->drive, pa->lun);
	} else {
		sprintf(vhnbuf,"/dev/rdsk/%s%dd%dvh", 
			pa->ctlrname, pa->ctlr, pa->drive);
	}

	return (vhnbuf);
}

/* pathtolabname: constructs the name of the logical volume label file
 * for the partition implied by the given pathname. Returns a pointer
 * to this in static storage.
 */

char * 
pathtolabname(p)
register char *p;
{
static char labnbuf[MAXDEVPATHLEN];

	bzero(labnbuf, MAXDEVPATHLEN);
	sprintf(labnbuf,"lvlab%d", pathtopart(p));
	return (labnbuf);
}

/* devtopath(): given a dev, return the pathname for it. The dev is expected
 * to be a block disk device. 
 *
 * This has even more horribly arbitrary & nonportable assumptions about
 * external major numbers & SGI device naming conventions built into it!
 *
 * If confirm is set, having guessed the pathname, we stat it to confirm 
 * the guess...
 */ 

char *
devtopath(dev, confirm)
register dev_t dev;
int confirm;
{
struct stat buf;
static char pathbuf[MAXDEVPATHLEN];
int maj;

	bzero(pathbuf, MAXDEVPATHLEN);
	maj = major(dev);

	switch (maj)
	{
	case DKSC_MAJOR:
		if ( SCSI_LUNOF(dev) ) {
			sprintf(pathbuf,"/dev/dsk/dks%dd%dl%ds%d",
				SCSI_CTLROF(dev), SCSI_UNITOF(dev), 
				SCSI_LUNOF(dev), PARTOF(dev));
		} else {
			sprintf(pathbuf,"/dev/dsk/dks%dd%ds%d",
				SCSI_CTLROF(dev), SCSI_UNITOF(dev), 
				PARTOF(dev));
		}
		break;

	case USRAID_MAJOR:	/* same structure as DKSC, diff prefix */
		sprintf(pathbuf,"/dev/dsk/rad%dd%ds%d",
			SCSI_CTLROF(dev), SCSI_UNITOF(dev), PARTOF(dev));
		break;

	case OLDDKSC_MAJOR :
		sprintf(pathbuf,"/dev/dsk/dks0d%ds%d",
			OSCSI_UNITOF(dev), PARTOF(dev));
		break;

	case OLDDKSC1_MAJOR :
		sprintf(pathbuf,"/dev/dsk/dks1d%ds%d",
			OSCSI_UNITOF(dev), PARTOF(dev));
		break;

	case OLDDKSC2_MAJOR :
		sprintf(pathbuf,"/dev/dsk/dks2d%ds%d",
			OSCSI_UNITOF(dev), PARTOF(dev));
		break;

	case OLDDKSC3_MAJOR :
		sprintf(pathbuf,"/dev/dsk/dks3d%ds%d",
			OSCSI_UNITOF(dev), PARTOF(dev));
		break;

	case JAG0_MAJOR :
	case JAG1_MAJOR :
	case JAG2_MAJOR :
	case JAG3_MAJOR :
	case JAG4_MAJOR :
	case JAG5_MAJOR :
	case JAG6_MAJOR :
	case JAG7_MAJOR :
		sprintf(pathbuf,"/dev/dsk/jag%dd%ds%d",
			JAG_CTLROF(dev), JAG_UNITOF(dev), PARTOF(dev));
		break;

	default : 
		sprintf(pathbuf,"Unknown device: maj %d min %d",
				major(dev), (unsigned) dev & L_MAXMIN);
		return (pathbuf);
	};

	if (confirm)
	{
		if ((stat(pathbuf, &buf) < 0) ||
		    (buf.st_rdev != dev)) 
		{
			bzero(pathbuf, MAXDEVPATHLEN);
			sprintf(pathbuf,"Unknown device: maj %d min %d",
				major(dev), (unsigned) dev & L_MAXMIN);

		}
	}

	return (pathbuf);
}

/* printpath(): given a dev, prints the deduced pathname.
 * The error flag controls whether print is on stdout or stderr.
 * The confirm flag controls whether the textual guess should be confirmed
 * by actually 'stat'ing  the generated guess.
 */

printpath(dev, error, confirm)
register dev_t dev;
int error, confirm;
{
char *path;

	path = devtopath(dev, confirm);
	if (!error)
		printf("%s", path);
	else
		fprintf(stderr,"%s", path);
}
