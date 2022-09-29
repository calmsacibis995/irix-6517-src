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

#ident	"libdisk/diskname.c $Revision: 1.18 $ of $Date: 1998/12/04 03:22:13 $"

/**	First version created by Dave Olson @ SGI, 10/88 **/

/* setdiskname(), getdiskname(), endiskname() */


#include <sys/types.h>
#include <sys/invent.h>
#include <sys/smfd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <diskinfo.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <bstring.h>

static struct disknames dknames;
static char *dev_dir;
static char fullpath[256];
static DIR *dir;

static void findnick(struct stat *);

/*	given a struct invent *, return the name(s) of the
	disk in the file system, derived by using the
	standard prefixes, and by looking for the 'nickname' 
	device directly in devdir.  fullname returned is
	relative to "devdir"/rdsk, nickname is relative to "devdir".

	Looks in rdsk, not in dsk, because floppies (and possibly others
	like cdroms later, exist only as char devices.

	Returns NULL if it can't find a matching type, class is not
	disk, type is a controller instead of a disk, or bad value
	for 'type'.
	If doing a 'stat' on the resulting fullname fails, sets
	nodexists field to 0, else 1.
*/


int
setdiskname(char *devdir)
{
	if(dev_dir)	/* set does implied end */
		enddiskname();

	if(strlen(devdir) > sizeof(fullpath) - (sizeof(dknames.fullname) + 7)) {
		/* name too long */
		errno = ENAMETOOLONG;
		return -1;
	}
	if((dir = opendir(devdir)) == NULL)
		return -1;
	dev_dir = devdir;
	return 0;
}

void
enddiskname(void)
{
	dev_dir = NULL;
	closedir(dir);
}

/* convert controller/unit/lun number to ascii, overkill in expected length. */
void
itostrcat(int num, char *s)
{
	char buf[10];
	int i;

	i = 8;
	buf[9] = '\0';
	do {
		buf[i--] = (num % 10) + '0';
		num /= 10;
	} while(num && (i >= 0));

	(void)strcat(s,&buf[i+1]);
}

/* returns static information.  Must be saved if getdiskname is called
 * multiple times.  Same for callers of get nextdisk(), which calls
 * this routine.  In particular, we set the diskinv member here, because
 * most people use this routine via getnextdisk(), and therefore would
 * have no access to the inventory information unless filled in here or
 * in getnextdisk(); do it here because otherwise could have stale info
 * if this routine called directly from some user code.
 */
struct disknames *
getdiskname(inventory_t *inv, unsigned partnum)
{
	struct stat sb;
	char *fullname, *s, *dtype;
	int unit, type, lun = 0;

	if(!dev_dir || !inv || partnum > NPARTS || (inv->inv_class != INV_DISK &&
		(inv->inv_class != INV_SCSI || (inv->inv_type != INV_WORM &&
		inv->inv_type != INV_CDROM && inv->inv_type != INV_OPTICAL)))) {
		errno = EINVAL;
		return NULL;
	}
	if(inv->inv_class == INV_SCSI)	/* treat M-O, CD, and WORM as scsi disks */
		type = INV_SCSIDRIVE;
	else
		type = inv->inv_type;

	switch (type)
	{
	case INV_SCSIDRIVE: 	dtype = "dks";
				lun   = inv->inv_state & 0xff;
				break;

	case INV_SCSIFLOPPY: 	dtype = "fds";
				break;

	default:	errno = EINVAL;
			return (NULL);
	}

	bzero( fullpath, 256 );
	strcpy(fullpath, dev_dir);
	strcat(fullpath, "/rdsk/");
	fullname = fullpath + strlen(fullpath);

	/* don't use sprintf, to avoid dragging in whole stdio package if
		caller doesn't use it. */
	strcpy(fullname, dtype);
	itostrcat((int)inv->inv_controller,fullname);
	s = fullname + strlen(fullname);
	*s++ = 'd';
	*s = '\0';

	if ((unit = (int)inv->inv_unit) >= MAX_DRIVE)
	{
		errno = EINVAL;
		return (NULL);
	}

	itostrcat( unit, fullname);
	s = fullname + strlen(fullname);

	/*
	 * support for non-zero lun numbers on SCSI drives.
	 */
	if ( lun ) {
		*s++ = 'l';
		*s = '\0';
		itostrcat( lun, fullname);
		s = fullname + strlen(fullname);
	}

	if(inv->inv_class == INV_DISK && type == INV_SCSIFLOPPY) {
		char *suffix;
		switch(partnum) {
		case FD_FLOP:
			suffix = ".48";
			break;
		case FD_FLOP_DB:
			suffix = ".96";
			break;
		case FD_FLOP_AT:
			suffix = ".96hi";
			break;
		case FD_FLOP_35LO:
			suffix = ".3.5";
			break;
		case FD_FLOP_35:
			suffix = ".3.5hi";
			break;
		default:
			errno = EINVAL;
			return NULL;
		}
		strcat(fullname, suffix);
	}
	else
		strcpy(fullpath, partname(fullpath, partnum));
	strcpy(dknames.fullname, fullname);	/* after all error checking */

	/* don't want left over from previous call */
	*dknames.rnickname = *dknames.bnickname = '\0';

	if(stat(fullpath, &sb) == -1)
		dknames.nodeok = 0;
	else 	/* we can look for nicknames, if the node exists.  */
		findnick(&sb);
	dknames.diskinv = inv;
	return &dknames;
}


/*	find the nickname for the raw device.
	If the block device exists for the full name, we want to look for
	block nicknames also.
*/
static void
findnick(struct stat *sb)
{
	struct stat nick;
	struct stat bsb;
	char name[256], *sname;
	struct dirent *dp;
	int bmatch=0, cmatch=0;
	dev_t bdev, cdev;
	mode_t bmode, cmode;

	dknames.nodeok = 1;

	strcpy(name, dev_dir);
	strcat(name, "/dsk/");
	strcat(name, dknames.fullname);
	if(stat(name, &bsb) == -1) {
		bmatch++;	/* don't look for block match */
		*dknames.bnickname = '\0';
	}
	else  {
		bdev = bsb.st_rdev;
		bmode = bsb.st_mode & S_IFMT;
	}
	cdev = sb->st_rdev;
	cmode = sb->st_mode & S_IFMT;

	strcpy(name, dev_dir);
	strcat(name, "/");
	sname = &name[strlen(name)];
	(void)rewinddir(dir);
	while(dp=readdir(dir)) {
		strcpy(sname, dp->d_name);	/* create full name */

		/* work with symlinks, links, and separate inodes */
		if(stat(name, &nick) == 0) {
			nick.st_mode &= S_IFMT;
			if(!bmatch && bmode == nick.st_mode &&
				bdev == nick.st_rdev) {
				bmatch++;
				strncpy(dknames.bnickname, dp->d_name,
					sizeof(dknames.bnickname));
				if(cmatch)
					break;
			}
			else if(!cmatch && cmode == nick.st_mode &&
				cdev == nick.st_rdev) {
				cmatch++;
				strncpy(dknames.rnickname, dp->d_name,
					sizeof(dknames.rnickname));
				if(bmatch)
					break;
			}

		}
	}
}

