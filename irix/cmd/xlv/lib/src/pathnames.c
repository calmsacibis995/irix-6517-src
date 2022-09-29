
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

#ident "$Revision: 1.23 $"

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
#include <diskinvent.h>
#include <sys/dkio.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/sysmacros.h>
#include <sys/major.h>
#include <sys/scsi.h>
#include <string.h>
#include <bstring.h>
#include <pathnames.h>
#include <invent.h>

#define MAXCTLRNAMELEN        20

extern int errno;
extern char *progname;
extern int verbose;

extern dev_t name_to_dev (char *pathname);

/*
 * Convert hardware graph pathname to an old style name.
 * Note that the return string is static and is changed by the call.
 */
char *
pathtoalias(char *p)
{
	static char	alias[MAXDEVNAME];
	int		len = MAXDEVNAME;
	
	diskpath_to_alias(p, alias, &len);

	return(alias);
}


/* pathtopart: returns the partition implied by the given name. */
/* XXXsbarr unused? */

int
pathtopart(register char *p)
{
	return path_to_partnum(p);
}

/* Pathtovhpath: takes a pathname of a block device & deduces the pathname
 * of the relevent volume header. Returns a pointer to this in static
 * storage. Note that we use the raw dev for the volume header: so that
 * if writing, it's synchronous & we get reliable notification of errors.
 */

char * 
pathtovhpath(register char *p)
{
	static char pathbuf[MAXDEVPATHLEN];
	int length = sizeof(pathbuf);

	return path_to_vhpath(p, pathbuf, &length);
}

/* devtopath(): given a dev, return the pathname for it. The dev is expected
 * to be a block disk device. 
 *
 * This has even more horribly arbitrary & nonportable assumptions about
 * external major numbers & SGI device naming conventions built into it!
 */ 

char *
devtopath(dev_t dev, int alias)
{
	static char pathbuf[MAXDEVPATHLEN];
	int length = sizeof(pathbuf);

	bzero(pathbuf, MAXDEVPATHLEN);

	if (alias) {
		return dev_to_alias(dev, pathbuf, &length);
	} else {
		return dev_to_devname(dev, pathbuf, &length);
	}
}

/* devtorawpath(): 	
 * given a dev, return the pathname for it. 
 * The dev is expected to be a raw disk device. 
 *
 * The returned static string is the device's fully qualified pathname.
 * ie. pathname is of the form /dev/rdsk/...
 */ 

char *
devtorawpath(register dev_t dev)
{
	static char pathbuf[MAXDEVPATHLEN];
	int length = sizeof(pathbuf);

	bzero(pathbuf, MAXDEVPATHLEN);

	return dev_to_rawpath(dev, pathbuf, &length, NULL);
}

/* XXXsbarr */
/* no longer called from xlv_equiv_classes.c  xlv_add_device()
 * this should be phased out... do disk inventory walk via ftw
 */

dev_t
disk_devno(const major_t ctlr, const minor_t target, const int lun)
{
    char	  path[MAXDEVPATHLEN];

    /* assumption: the vh partition always exists, so stat it */
    if (lun) {
	sprintf(path, "/dev/rdsk/dks%dd%dl%dvh", ctlr, target, lun);
    }
    else {
	sprintf(path, "/dev/rdsk/dks%dd%dvh", ctlr, target);
    }

    return CTLR_UNITOF(name_to_dev(path));
}

void
scsi_devname(char *dev_name, const major_t ctlr, const minor_t target,
	     const int lun)
{
    sprintf(dev_name, "/dev/scsi/sc%dd%dl%d", ctlr, target, lun);
}


/*
 * printpath(): given a dev, prints the deduced pathname.
 */

void
printpath(register dev_t dev, int alias)
{
	char *path;

	if (path = devtopath(dev, alias)) {
		printf("%s", path);

	} else if (alias && (path = devtopath(dev, 0))) { 
		/*
		 * When an alias is request and the request fails,
		 * try full blown device pathname
		 */
		printf("%s", path);

	} else {
		printf("Unnamed device %#X", dev);
	}
}
