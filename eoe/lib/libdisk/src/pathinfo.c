/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.38 $"

#include <stdio.h>
#include <stdlib.h> 
#include <limits.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/dkio.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/major.h>
#include <dirent.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <bstring.h>
#include <unistd.h>
#include <sys/conf.h>
#include <invent.h>
#include <sys/hwgraph.h>
#include <sys/iograph.h>
#include <sys/scsi.h>
#include <sys/attributes.h>
#include <diskinvent.h>


/* static data used by these routines. */
static struct volume_header volhdr;
static struct stat64 sbuf;
static xlv_disk_geometry_t xlv;
static int internal_path_lookup = 0;

/* internal static functions */
static int findcharfd(char *);
static int is_hwgfullpath(char *);

/* findrawpath(): attempts to locate the char device corresponding to the
 * given block device.
 *
 * Returns path if path is already the raw device.
 * Returns NULL if no corresponding char device can be found.
 *
 * Tries the fast way first: if the given path starts with /dev/dsk/
 * it constructs a corresponding /dev/rdsk/ and stats that to see if it's
 * correct. Failing that, it tries the hard way: an exhaustive search
 * through /dev/rdsk and /dev for a corresponding dev.
 *
 * Side effect, used by other routines: stats path into sbuf.
 * Used by internal libdisk routines, it uses an auto buffer, 
 * called externally it mallocs a buffer for the return.
 *
 */


char *
findrawpath(char *path)
{

	#define DSK_PATHLEN MAXDEVNAME
	static char scratchbuf[DSK_PATHLEN];
	char orig_cwd[PATH_MAX];
	char *rawpathbuf;
	int rawpathlen = DSK_PATHLEN;
	struct stat64 rsbuf;
	static char *dirlist[] = {"/dev/rdsk", "/dev", "/dev/rxlv", NULL};
	static char *mrdirlist[] =
		{"/root/dev/rdsk", "/root/dev", "/root/dev/rxlv", NULL};
	char **cdir;
	DIR *dir;
	struct dirent *file;
	char *thefile;
	int trymr = 1;
	char devname[MAXDEVNAME];
	int  devnamelen = MAXDEVNAME;
	int is_xlv = 0;

	if (stat64(path, &sbuf) < 0)
		return (NULL);
	if ((sbuf.st_mode & S_IFMT) == S_IFCHR)
		return (path);
	if ((sbuf.st_mode & S_IFMT) != S_IFBLK)
		return (NULL);

	/* use internal buffer or allocate a new one for the user */
	if (internal_path_lookup) 
		rawpathbuf = scratchbuf;
	else
		if ((rawpathbuf = malloc(DSK_PATHLEN)) == NULL)
			return (NULL);

	/* XXXsbarr: zap the major test when XLV goes HWG */
	if (major(sbuf.st_rdev) == XLV_MAJOR) {
		is_xlv = 1;
	} else {
		/* get the driver name */
		if (dev_to_drivername(sbuf.st_rdev, devname, &devnamelen))
			if (strncmp(devname, "xlv", 3) == 0)
			    is_xlv = 1;
	}


	/* check if the driver is XLV, if so handle the prefixes directly */
	if (is_xlv) {

		bzero(rawpathbuf, DSK_PATHLEN);
		/* see if prefix is /dev/xlv if so convert to /dev/rxlv */
		/* this also handles */
		if (strncmp(path, "/dev/xlv/", 9) == 0) {
			strcpy(rawpathbuf, "/dev/rxlv/");
			strcat(rawpathbuf, path+9);
			return rawpathbuf;
		} else if (strncmp(path, "/root/dev/xlv/", 14) == 0) {
			strcpy(rawpathbuf, "/root/dev/rxlv/");
			strcat(rawpathbuf, path+14);
			return rawpathbuf;
		}
	}

	/* everything in /dev/dsk and /dev/rdsk is in the HWG now */

	/* Check if this is a HWG special file */
	if (filename_to_devname(path, rawpathbuf, &rawpathlen)) {
		char *p;

		/* this test should never fail */
		if ((p = strstr(rawpathbuf, "/" EDGE_LBL_BLOCK)) != NULL) {
			strcpy(p, "/" EDGE_LBL_CHAR);
		}

                return(rawpathbuf);
	}

	/* the rest of this code assumes a non-hwg device since it 
         * relies on the char dev_t and block dev_t being equivalent
         * XXXsbarr: are there any left? 
	 */

	cdir = dirlist;		/* Try regular dirlist first */

	/* preserve our starting directory, if possible */
	if(getcwd(orig_cwd, sizeof(orig_cwd)) == NULL)
		*orig_cwd = NULL;

hardway:
	for(; *cdir; cdir++) 
	{
		if(chdir(*cdir))
			continue;
		if((dir = opendir(*cdir)) == NULL)
			continue;
		while((file = readdir(dir)) != NULL) 
		{
			thefile = file->d_name;
			if((stat64(thefile, &rsbuf) == 0) &&
			   ((rsbuf.st_mode & S_IFMT) == S_IFCHR) &&
			   (rsbuf.st_rdev == sbuf.st_rdev)) 
			{
				bzero(rawpathbuf, DSK_PATHLEN);
				sprintf(rawpathbuf, "%s/%s", *cdir, file->d_name);
				closedir(dir);
				if(*orig_cwd)
					(void)chdir(orig_cwd);
				return (rawpathbuf);
			}
		}
		closedir(dir);
	}

	/*
	 * If called from the miniroot, check the $rbase directories also
	 * since SCSI major #s have changed and we might not find the old ones
	 * in /dev/
	 */
	if (trymr && strncmp(path, "/root/dev/", 10) == 0) {
		trymr = 0;
		cdir = mrdirlist;
		goto hardway;
	}

	if(*orig_cwd)
		(void)chdir(orig_cwd);
	return (NULL);
}


/* findblockpath(): attempts to locate the block device corresponding to the
 * given character device.
 *
 * Returns path if path is already the block device.
 * Returns NULL if no corresponding block device can be found.
 *
 * Tries the fast way first: if the given path starts with /dev/rdsk/
 * it constructs a corresponding /dev/dsk/ and stats that to see if it's
 * correct. Failing that, it tries the hard way: an exhaustive search
 * through /dev/dsk and /dev for a corresponding dev.
 *
 * Side effect, used by other routines: stats path into sbuf.
 * Used by internal libdisk routines, it uses an auto buffer, 
 * called externally it mallocs a buffer for the return.
 *
 */
char *
findblockpath(char *path)
{

	#define DSK_PATHLEN MAXDEVNAME
	static char scratchbuf[DSK_PATHLEN];
	char orig_cwd[PATH_MAX];
	char *blkpathbuf;
	int blkpathlen = DSK_PATHLEN;
	struct stat64 bsbuf;
	static char *dirlist[] = {"/dev/dsk", "/dev", "/dev/xlv", NULL};
	static char *mrdirlist[] =
		{"/root/dev/dsk", "/root/dev", "/root/dev/xlv", NULL};
	char **cdir;
	DIR *dir;
	struct dirent *file;
	char *thefile;
	int trymr = 1;
	char devname[MAXDEVNAME];
	int  devnamelen = MAXDEVNAME;
	int is_xlv = 0;

	if (stat64(path, &sbuf) < 0)
		return (NULL);
	if ((sbuf.st_mode & S_IFMT) == S_IFBLK)
		return (path);
	if ((sbuf.st_mode & S_IFMT) != S_IFCHR)
		return (NULL);

	/* use internal buffer or allocate a new one for the user */
	if (internal_path_lookup) 
		blkpathbuf = scratchbuf;
	else
		if ((blkpathbuf = malloc(DSK_PATHLEN)) == NULL)
			return (NULL);

	/* XXXsbarr: zap the major test when XLV goes HWG */
	if (major(sbuf.st_rdev) == XLV_MAJOR) {
		is_xlv = 1;
	} else {
		/* get the driver name */
		if (dev_to_drivername(sbuf.st_rdev, devname, &devnamelen))
			if (strncmp(devname, "xlv", 3) == 0)
			    is_xlv = 1;
	}


	/* check if the driver is XLV, if so handle the prefixes directly */
	if (is_xlv) {

		bzero(blkpathbuf, DSK_PATHLEN);
		/* see if prefix is /dev/rxlv if so convert to /dev/xlv */
		/* this also handles */
		if (strncmp(path, "/dev/rxlv/", 10) == 0) {
			strcpy(blkpathbuf, "/dev/xlv/");
			strcat(blkpathbuf, path+10);
			return blkpathbuf;
		} else if (strncmp(path, "/root/dev/rxlv/", 15) == 0) {
			strcpy(blkpathbuf, "/root/dev/xlv/");
			strcat(blkpathbuf, path+15);
			return blkpathbuf;
		}
	}

	/* everything in /dev/dsk and /dev/rdsk is in the HWG now */

	/* Check if this is a HWG special file */
	if (filename_to_devname(path, blkpathbuf, &blkpathlen)) {
		char *p;

		/* this test should never fail */
		if ((p = strstr(blkpathbuf, "/" EDGE_LBL_CHAR)) != NULL) {
			strcpy(p, "/" EDGE_LBL_BLOCK);
		}

		return(blkpathbuf);
	}

	/* the rest of this code assumes a non-hwg device since it 
         * relies on the char dev_t and block dev_t being equivalent
         * XXXsbarr: are there any left? 
	 */

	cdir = dirlist;		/* Try regular dirlist first */

	/* preserve our starting directory, if possible */
	if(getcwd(orig_cwd, sizeof(orig_cwd)) == NULL)
		*orig_cwd = NULL;

hardway:
	for(; *cdir; cdir++) 
	{
		if(chdir(*cdir))
			continue;
		if((dir = opendir(*cdir)) == NULL)
			continue;
		while((file = readdir(dir)) != NULL) 
		{
			thefile = file->d_name;
			if((stat64(thefile, &bsbuf) == 0) &&
			   ((bsbuf.st_mode & S_IFMT) == S_IFBLK) &&
			   (bsbuf.st_rdev == sbuf.st_rdev)) 
			{
				bzero(blkpathbuf, DSK_PATHLEN);
				sprintf(blkpathbuf, "%s/%s", *cdir, file->d_name);
				closedir(dir);
				if(*orig_cwd)
					(void)chdir(orig_cwd);
				return (blkpathbuf);
			}
		}
		closedir(dir);
	}

	/*
	 * If called from the miniroot, check the $rbase directories also
	 * since SCSI major #s have changed and we might not find the old ones
	 * in /dev/
	 */
	if (trymr && strncmp(path, "/root/dev/", 10) == 0) {
		trymr = 0;
		cdir = mrdirlist;
		goto hardway;
	}

	if(*orig_cwd)
		(void)chdir(orig_cwd);
	return (NULL);
}


/* findsize(): determine the available size in blocks of the device named
 * by path.
 */

__int64_t
findsize(char *path)
{
	int fd;
	dev_t dev;
	major_t part;
	struct partition_table *pt;
	char devname[MAXDEVNAME];
	int  devnamelen = MAXDEVNAME;
	int ret;
	int is_xlv = 0;

	if ((fd = findcharfd(path)) < 0)
		return (-1LL);

	dev = sbuf.st_rdev;

	/* XXXsbarr: zap the major test when XLV goes HWG */
	if (major(dev) == XLV_MAJOR) {
		is_xlv = 1;
	} else {
		/* get the driver name */
		if (dev_to_drivername(dev, devname, &devnamelen))
			if (strncmp(devname, "xlv", 3) == 0)
			    is_xlv = 1;
	}


	/* The location & format of the returned data depends
	 * on whether the device is a regular partition or a logical volume. 
	 */

	if (is_xlv) {
	
		ret = ioctl(fd, DIOCGETXLV, (caddr_t)&xlv);
		close(fd);
		if (ret < 0)
			return (-1LL);
		return (long long)(xlv.subvol_size);
	}

	/* regular disk */
	ret = ioctl(fd, DIOCGETVH, (caddr_t)&volhdr);
	close(fd);
	if (ret < 0)
		return(-1LL);

	part = dev_to_partnum(dev);
	pt = &volhdr.vh_pt[part];
	return (long long)(pt->pt_nblks);
}


/* is_rootpart()
 *
 * returns 1 if partition looks like a root partition.
 * Look for likely major numbers and then return 1 if the partition number
 *   is 0.
 *
 * XXXsbarr: this is only used by efs/mkfs, we should zap this when efs
 * disappears.
 */

int
is_rootpart(char *path)
{
	char devnm[MAXDEVNAME];
	int devnmlen = sizeof(devnm);
	struct stat64 sb;

	/* check if it's an XLV volume */
	/* XXXsbarr: this needs to be converted when XLV goes HWG */
	if (stat64(path, &sb) != -1) {
		/* if it's XLV major and minor of 0 then it's root */
		if (major(sb.st_rdev) == XLV_MAJOR) {
			if (minor(sb.st_rdev) == 0)
				return 1;
			return 0;
		}
	}
	
		
	/* must be a disk, assume all rootdisks have to be HWG devices */

	if (path_to_rawpath(path, devnm, &devnmlen, NULL) == NULL)
		return 0;

	if (strstr(devnm, "/" EDGE_LBL_DISK "/") == NULL)
		return 0;

	if (path_to_partnum(devnm) == 0)
		return 1;

	return 0;
}

/* findcharfd(): returns a file descriptor open for reading to the char device
 * corresponding to the given path (which may be a block or char device).
 */
static int
findcharfd(char *path)
{
	char *rawpath;
	int fd;

	internal_path_lookup = 1;
	rawpath = findrawpath(path);
	internal_path_lookup = 0;

	if (rawpath == NULL)
		return -1;

	if ((fd = open(rawpath, O_RDONLY)) < 0)
		return (-1);
	return (fd);
}

/*********************************************************************
 * dksc path and alias generation and manipulation
 */

/*
 * These alias routines are a logical extension of the dksc driver.  We
 * convert a full length pathname into an "old style" 'dks1d2s0' name.
 * We're using the same algorithm that the driver is using to create the
 * aliases in the hardware graph.  Ideally, we should have an _ALIAS
 * attribute in the hardware graph itself, but in the argument against
 * kernel bloat we're going to do it here since it's argued that libdisk
 * is technically 'part' of the disk driver or at least can mask it.
 *
 * -sbarr 9/96
 */

static char *
dksc_path_to_alias(
	char *sourcepath,
	char *alias,
	int *len,
	inventory_t *inv,
	int inv_len)
{
#define DEV_BLK_PREFIX "/dev/dsk"
#define DEV_CHR_PREFIX "/dev/rdsk"

	char pstr[4];
	char fullpath[MAXPATHLEN];
	uint64_t fcnode = 0, fcport;
	int partition;
	int target;
	int ctlr;
	int lun;
	char *prefix;
	char *devnm2;
	int ret;
	inventory_t invent[DSK_MAX_INVENTRIES];
	int i;

	if (!len || !alias)
		return NULL;

	strcpy(fullpath, sourcepath);

	/* check if it's block or char */
	if (strstr(fullpath, "/" EDGE_LBL_CHAR))
		prefix = DEV_CHR_PREFIX;
	else if (strstr(fullpath, "/" EDGE_LBL_BLOCK))
		prefix = DEV_BLK_PREFIX;
	else
		goto dksc_error;

	/* extract target (disk) number for disk */
	/* extract partition number for disk */
	/* go to the disk node, get ctlr number */

	/* get partition number */
	if (strstr(fullpath, EDGE_LBL_VOLUME_HEADER "/")) {
		partition = SCSI_DISK_VH_PARTITION;
		strcpy(pstr, "vh");
	} else if (strstr(fullpath, EDGE_LBL_VOLUME "/")) {
		partition = SCSI_DISK_VOL_PARTITION;
		strcpy(pstr, "vol");
	} else {
		devnm2 = strstr(fullpath, EDGE_LBL_PARTITION "/");
		if (devnm2 == NULL)
			goto dksc_error;
		devnm2 += strlen(EDGE_LBL_PARTITION "/");
		partition = atoi(devnm2);
		sprintf(pstr, "s%d", partition);
	}

	/*
	 * Find target or node/port
	 * Start search from scsi_ctlr, since "node" may be in early
	 *   part of path.
	 */
	if ((devnm2 = strstr(fullpath, EDGE_LBL_SCSI_CTLR)) == NULL)
		goto dksc_error;
	if (devnm2 = strstr(devnm2, EDGE_LBL_NODE "/")) {
		devnm2 += strlen(EDGE_LBL_NODE "/");
		fcnode = strtoull(devnm2, NULL, 16);
		if (devnm2 = strstr(fullpath, EDGE_LBL_PORT "/")) {
			devnm2 += strlen(EDGE_LBL_PORT "/");
			fcport = strtoull(devnm2, NULL, 16);
		}
		else
			goto dksc_error;
	}
	else if (devnm2 = strstr(fullpath, EDGE_LBL_TARGET "/")) {
		devnm2 += strlen(EDGE_LBL_TARGET "/");
		target = atoi(devnm2);
	}
	else
		goto dksc_error;

	/* find lun */
	devnm2 = strstr(fullpath, EDGE_LBL_LUN "/");
	if (devnm2 == NULL)
		goto dksc_error;
	devnm2 += strlen(EDGE_LBL_LUN "/");
	lun = atoi(devnm2);

	/* if inventory entry already sent, use it */
	if (inv == NULL || inv_len == 0) {

		/* find controller */
		devnm2 = strstr(fullpath, "/" EDGE_LBL_DISK "/");
		if (devnm2 == NULL)
			goto dksc_error;
		devnm2 += strlen("/" EDGE_LBL_DISK);
		*devnm2 = '\0';

		inv_len = sizeof(invent);
		ret = attr_get(fullpath, _INVENT_ATTR,
			       (char *)&invent[0], &inv_len, 0);
		inv = &invent[0];
		inv_len = (int)(inv_len/sizeof(inventory_t));
		if (ret)
			goto dksc_error;
	}

	ctlr = -1;
	for (i = 0; i < inv_len; i++) {
		if (inv[i].inv_class == INV_DISK) {
			if (inv[i].inv_type == INV_SCSIDRIVE) {
				ctlr = (int)inv[i].inv_controller;
				break;
			}
		}
	}
	if (ctlr == -1)
		goto dksc_error;

	/* we have all of the relevant info, now create the alias */
	if (fcnode != 0)
		sprintf(alias, "%s/%llx/lun%d%s/c%dp%llx",
			prefix, fcnode, lun, pstr, ctlr, fcport);
	else if (lun)
		sprintf(alias, "%s/dks%dd%dl%d%s",
			prefix, ctlr, target, lun, pstr);
	else
		sprintf(alias, "%s/dks%dd%d%s", prefix, ctlr, target, pstr);

	*len = (int)strlen(alias);

	return alias;

dksc_error:
	/* an error occurred, copy path into alias and return NULL */
	strncpy(alias, fullpath, *len);
	alias[(*len)-1] = '\0';
	return NULL;
}

/*
 * Create a disk alias for a known disk device
 *
 * This is the fastpath to path_to_alias.  This routine assumes
 * that the specified device path was generated from the dksc driver
 * and is a full path (not an alias).  If you're unsure what device
 * type you're passing in, use 'devpath_to_alias' instead, since it
 * checks if it's dealing with dksc devices.
 */

char *
diskpath_to_alias(char *fullpath, char *alias, int *len)
{
	return dksc_path_to_alias(fullpath, alias, len, NULL, 0);
}

char *
diskpath_to_alias2(
	char *fullpath,
	char *alias,
	int *len,
	inventory_t *inv,
	int inv_len)
{
	return dksc_path_to_alias(fullpath, alias, len, inv, inv_len);
}

/*
 * Given a generic pathname create the official dskc alias
 *
 * This could also serve as a good generic entry point for all device
 * alias code.
 */

char *
path_to_alias(char *path, char *alias, int *len)
{
	char driver[MAXDEVNAME];
	int  drvlen = sizeof(driver);
	char devpath[MAXDEVNAME];
	int  devlen = sizeof(devpath);

	if (is_hwgfullpath(path) == 0) {
		/* get full blown device pathname */
		if (filename_to_devname(path, devpath, &devlen) == NULL)
			return NULL;
		path = devpath;
	}

	/* Get the driver name so we know what the device should look like.
         *
         * We only support dksc aliases here (we're a logical extension
         * of the driver).  All other non HWG and non dksc devices will fail
         * to generate an alias.  (This alias cruft should really be handled
         * as a first class item in the kernel via an attribute call).
	 */
	if (filename_to_drivername(path, driver, &drvlen) == NULL) {
		strncpy(alias, path, *len);
		/* just in case we actually did run to the end */
		alias[(*len)-1] = '\0';
		return NULL;
	}

	if (strcmp(driver, "dksc") == NULL)
		return dksc_path_to_alias(path, alias, len, NULL, 0);

	return NULL;
}

/*
 * Given a dev_t create the official dksc alias
 */

char *
dev_to_alias(dev_t dev, char *alias, int *len)
{
	char devpath[MAXDEVNAME];
	int  devlen = sizeof(devpath);

	/* get full blown device pathname */
	if (dev_to_devname(dev, devpath, &devlen) == NULL)
		return NULL;

	return path_to_alias(devpath, alias, len);
}

/*********************************************************************
 * dksc partition and controller manipulation
 *
 * Routines to generate and extract info from full dksc pathnames
 */

static int
is_hwgfullpath(char *path)
{
	/* check if we're have a full devpath already */
	if (strstr(path, "/" EDGE_LBL_CHAR))
		return 1;
	if (strstr(path, "/" EDGE_LBL_BLOCK))
		return 1;

	return 0;
}


/* path_to_partnum()
 *
 * return the partition number of the block device
 */

int
path_to_partnum(char *path)
{
	char devnm[MAXDEVNAME];
	int  length = sizeof(devnm);
	char dvrname[MAXDEVNAME];
	int  dvrlen = sizeof(dvrname);
	char *p;

	/* check if we're have a full devpath already */
	if (is_hwgfullpath(path) == 0) {
		if (filename_to_devname(path, devnm, &length) == NULL)
			return -1;
		path = devnm;
	}

	if (strstr(path, "/" EDGE_LBL_VOLUME_HEADER "/"))
		return SCSI_DISK_VH_PARTITION;

	if (strstr(path, "/" EDGE_LBL_VOLUME "/")) 
		return SCSI_DISK_VOL_PARTITION;

	if ((p = strstr(path, "/" EDGE_LBL_PARTITION "/")))
		return atoi(p + strlen("/" EDGE_LBL_PARTITION "/"));

	/* for floppies, there are no partitions. A vh is faked up,
	 * it has the size in the first partition slot. For other
	 * disks, the partition is given by the LS nybble of the
	 * minor number.
	 */
	/* for floppies, always return partition 0 */
	if (filename_to_drivername(devnm, dvrname, &dvrlen)) {
		if (strncmp(dvrname, "smfd", 4) == 0)
			return 0;
	}

	return -1;
}

/* dev_to_partnum()
 *
 * given a dev_t return the number of the associated disk partition
 */

int
dev_to_partnum(dev_t dev)
{
	char devnm[MAXDEVNAME];
	int  length = sizeof(devnm);

	if (dev_to_devname(dev, devnm, &length))
		return path_to_partnum(devnm);

	return -1;
}


/* path_to_ctlrnum()
 *
 * return the controller number for the given device
 */

int
path_to_ctlrnum(char *path)
{
	char devnm[MAXDEVNAME];
	int  length = sizeof(devnm);
	char *p;

	/* check if we're have a full devpath already */
	if (is_hwgfullpath(path) == 0) {
		if (filename_to_devname(path, devnm, &length) == NULL)
			return -1;
		path = devnm;
	}

	/* XXXsbarr: modify for lego, have to get inventory */
	if (p = strstr(path, "/" EDGE_LBL_SCSI_CTLR "/"))
		return atoi(p + strlen("/" EDGE_LBL_SCSI_CTLR "/"));

	return -1;
}

/* dev_to_ctlrnum()
 *
 * given a dev_t return the number of the associated device controller
 */

int
dev_to_ctlrnum(dev_t dev)
{
	char devnm[MAXDEVNAME];
	int  length = sizeof(devnm);

	if (dev_to_devname(dev, devnm, &length))
		return path_to_ctlrnum(devnm);

	return -1;
}


/* dev_to_rawpath()
 *
 * Given a dev_t, convert it into the canonical pathname for the
 * associated raw device.
 */

char *
dev_to_rawpath(dev_t dev, char *destpath, int *destlen, struct stat64 *sbuf)
{
	char path[MAXDEVNAME];
	char tmppath[MAXDEVNAME];
	int len = sizeof(path);
	char orig_cwd[PATH_MAX];
	DIR *dir;
	struct dirent *file;
	struct stat64 isbuf;

	if (major(dev) != XLV_MAJOR) {

		/* get the canonical name for the device */
		if (dev_to_devname(dev, path, &len) == NULL)
			return NULL;

		return path_to_rawpath(path, destpath, destlen, sbuf);

	}

	/* do we need internal buffer? */
	if (sbuf == NULL)
		sbuf = &isbuf;

	/*
         * We're an XLV volume.  XLV is not part of the HWG yet, so we'll
         * have to scan the /dev/rxlv directory explicitly
         */

	/* preserve our starting directory, if possible */
	if (getcwd(orig_cwd, sizeof(orig_cwd)) == NULL)
		orig_cwd[0] = '\0';

	if (chdir("/dev/rxlv"))
		return NULL;

	if ((dir = opendir("/dev/rxlv")) == NULL)
		return NULL;

	while ((file = readdir(dir)) != NULL) {

		if (stat64(file->d_name, sbuf) == -1)
			continue;

		/* for XLV block dev_t == char dev_t */
		if (sbuf->st_rdev == dev) {
			strcpy(tmppath, "/dev/rxlv/");
			strncat(tmppath, file->d_name,
				sizeof(tmppath)-strlen("/dev/rxlv/"));
			strncpy(destpath, tmppath, *destlen);
			/* make sure string is 0 terminated */
			destpath[(*destlen)-1] = '\0';
			*destlen = (int)strlen(destpath);

			closedir(dir);
			if (orig_cwd[0] != '\0')
				(void)chdir(orig_cwd);
			return destpath;
		}
	}
	closedir(dir);
	if (orig_cwd[0] != '\0')
		(void)chdir(orig_cwd);
	return NULL;
}

/* path_to_rawpath()
 *
 * given a device path, convert it into the canonical raw path
 * for that device
 */

char *
path_to_rawpath(char *path, char *destpath, int *destlen, struct stat64 *sbuf)
{
	char *blkchr_str;
	struct stat64 lsbuf;

	/* use internal buff if user doesn't pass one in */
	if (sbuf == NULL)
		sbuf = &lsbuf;

	/* check if we're a fullpath already */
	if (is_hwgfullpath(path) == 0) {
		/* get the canonical name for the device */
		if (filename_to_devname(path, destpath, destlen) == NULL)
			return NULL;
	} else {
		/* already a fullpath, just copy it over */
		strcpy(destpath, path);
		*destlen = (int)strlen(destpath);
	}

	if (stat64(destpath, sbuf) == -1)
		return NULL;

	/* determine if it's a block or char device */
	if ((sbuf->st_mode & S_IFMT) == S_IFCHR)
		return destpath;
	/* if it's not a block dev, then return with an error */
	if ((sbuf->st_mode & S_IFMT) != S_IFBLK)
		return NULL;

	/* find the "/block" string and convert to a "/char" */
	if ((blkchr_str = strstr(destpath, "/" EDGE_LBL_BLOCK)) == NULL)
		return NULL;

	strcpy(blkchr_str, "/" EDGE_LBL_CHAR);
	(*destlen)--;

	/* get the new dev_t */
	if (stat64(destpath, sbuf) == -1)
		return NULL;

	return destpath;
}

/* dev_to_partpath()
 *
 * Take a given dev_t and return the path of the specified partition
 * associated with the dev_t.  Also return the stat struct if requested.
 */

char *
dev_to_partpath(
	dev_t dev,
	int npartnum,
	int devtype_flag,
	char *destpath,
	int *destlen,
	struct stat64 *buf)
{
	char path[MAXDEVNAME];
	int  len = sizeof(path);

	if (dev_to_devname(dev, path, &len) == NULL)
		return NULL;

	return path_to_partpath(path, npartnum,
				devtype_flag, destpath, destlen, buf);
}

/* path_to_partpath()
 *
 * Take a given device and return the same device stem but with
 * a different partition.  Also return the stat struct for the new
 * filename if requested.
 */

char *
path_to_partpath(
	const char *path,
	int npartnum,
	int devtype_flag,
	char *destpath,
	int *destlen,
	struct stat64 *sbuf)
{
	int blkchr_flag;
	int length = *destlen;
	char *disk_str;
	char *dev_str;
	struct stat64 lsbuf;
	const char *disk_keyword = "/" EDGE_LBL_DISK "/";

	destpath[0] = '\0';
	*destlen = 0;

	if (sbuf == NULL)
		sbuf = &lsbuf;

	/* verify that it's a valid partition number */
	if (npartnum < -1)
		return NULL;

	/* get the dev_t for this device (possibly an alias) */
	if (stat64(path, sbuf) == -1)
		return NULL;

	/* determine if it's a block or char device */
	if ((sbuf->st_mode & S_IFMT) == S_IFBLK)
		blkchr_flag = S_IFBLK;
	else if ((sbuf->st_mode & S_IFMT) == S_IFCHR)
		blkchr_flag = S_IFCHR;
	else
		return NULL;

	/* if devtype is set, override the block/char type */
	if (devtype_flag == 0)
		devtype_flag = blkchr_flag;

	if (devtype_flag == S_IFBLK)
		dev_str = EDGE_LBL_BLOCK;
	else if (devtype_flag == S_IFCHR)
		dev_str = EDGE_LBL_CHAR;
		
	/* get the canonical name for the device */
	if (is_hwgfullpath((char *)path) == 0) {
		if (filename_to_devname((char *)path, destpath, &length) == NULL)
			return NULL;
	} else {
		strcpy(destpath, path);
		length = (int)strlen(destpath);
	}

	/* verify it's a disk device */
	if ((disk_str = strstr(destpath, disk_keyword)) == NULL)
		return NULL;

	/* point to 1st char after '/disk/' */
	disk_str += strlen(disk_keyword);

	if (npartnum == -1) {
		if ((npartnum = path_to_partnum(destpath)) == -1)
			return NULL;
	}

	/* replace the partition name with the one we want */
	if (npartnum == SCSI_DISK_VH_PARTITION) {
		strcpy(disk_str, EDGE_LBL_VOLUME_HEADER "/");
		strcat(disk_str, dev_str);
	} else if (npartnum == SCSI_DISK_VOL_PARTITION) {
		strcpy(disk_str, EDGE_LBL_VOLUME "/");
		strcat(disk_str, dev_str);
	} else {
		sprintf(disk_str, EDGE_LBL_PARTITION "/%d/%s",
			npartnum, dev_str);
	}

	*destlen = (int)strlen(destpath);

	/* stat the new filename to confirm that the new partition exists */
	if (stat64(destpath, sbuf) == -1)
		return NULL;

	return destpath;
}

/* path_to_vhpath()
 *
 * Given a disk filename, return the raw path to the disk's volume header
 */

char *
path_to_vhpath(char *path, char *destpath, int *destlen)
{
	return path_to_partpath(path, SCSI_DISK_VH_PARTITION, S_IFCHR,
				destpath, destlen, NULL);
}

/* path_to_vhpath()
 *
 * Given a disk filename, return the raw path to the disk's volume partition
 */

char *
path_to_volpath(char *path, char *destpath, int *destlen)
{
	return path_to_partpath(path, SCSI_DISK_VOL_PARTITION, S_IFCHR,
				destpath, destlen, NULL);
}
