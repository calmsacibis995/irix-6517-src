
/* Copyright 1996, Silicon Graphics, Inc.
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

#include <diskinvent.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/iograph.h>
#include <sys/conf.h>
#include <sys/attributes.h>

/* dsk_walk_drives()
 *
 * This routine walks /hw/rdisk looking for volume header entries.
 * When a vh is found, the canonical name is passed back to the
 * user along with the stat & inventory structs.
 *
 * The user registers a callback function and an optional user-data pointer.
 * The callback is executed for each volume header device found.
 *
 * CREATED: 7/30/96 sbarr
 */
int (*dsk_walk_drives_func)(char *, struct stat64 *, inventory_t *, int, void *);
void *dsk_walk_drives_arg;

/*ARGSUSED*/
int
dsk_walk_drive(const char *devpath, const struct stat64 *sbuf,
	       int type, struct FTW* ftwinfo)
{
	char		 newpath[MAXDEVNAME];
	char		 diskpath[MAXDEVNAME];
	char		*diskp;
	inventory_t	 invent[DSK_MAX_INVENTRIES];
	int		 length;
	int		 error;

	if (strstr(devpath, "vh") == NULL)
		return 0;
	if ((sbuf->st_mode & S_IFMT) != S_IFCHR)
		return 0;
	length = sizeof(newpath);
	if (filename_to_devname((char *) devpath, newpath, &length) == 0)
		return 0;
	
	strcpy(diskpath, newpath);
	diskp = strstr(diskpath, "/" EDGE_LBL_DISK "/");
	if (diskp == NULL)
		return 0;
	diskp += strlen("/" EDGE_LBL_DISK );
	*diskp = '\0';
	length = sizeof(invent);

	error = attr_get(diskpath, _INVENT_ATTR, (char *) invent, &length, ATTR_ROOT);
	if (error)
		length = 0;
	else
		length /= sizeof(inventory_t);
	
	(*dsk_walk_drives_func)(newpath, (struct stat64 *) sbuf, invent,
				length, dsk_walk_drives_arg);
	return 0;
}

int
dsk_walk_drives(
	int (*userfunc)(char *, struct stat64 *, inventory_t *, int, void *),
	void *usrdat)
{
	dsk_walk_drives_func = userfunc;
	dsk_walk_drives_arg = usrdat;
	return nftw64("/hw/" EDGE_LBL_RDISK, dsk_walk_drive, 3, FTW_PHYS);
}

/* dsk_walk_partitions()
 *
 * This routine walks /hw/rdisk returning all disk partitions found
 * excluding root,swap,usr and volume_header entries.
 *
 * When a partition is found, the canonical name is passed back to the
 * user along with the stat & inventory structs.
 *
 * The user registers a callback function and an optional user-data pointer.
 * The callback is executed for each volume header device found.
 *
 * CREATED: 9/9/96 sbarr
 */

int
dsk_walk_partitions(
	int (*userfunc)(char *, struct stat64 *, inventory_t *, int, void *),
	void *usrdat)
{
	char *rdisk_path = "/hw/" EDGE_LBL_RDISK;
	char devpath[MAXDEVNAME];
	char newpath[MAXDEVNAME];
	char diskpath[MAXDEVNAME];
	char *diskp;
	int  newlen;
	DIR *dp;
	struct stat64 sbuf;
	struct dirent64 *direntp;
	char *dname;
	int error;
	inventory_t invent[DSK_MAX_INVENTRIES];
	int inv_length;
	int uret;
	char space[sizeof(struct dirent64)+NAME_MAX+1];
	struct dirent64 *dirent = (struct dirent64 *)space;

	if ((dp = opendir(rdisk_path)) == NULL)
		return 0;

	while (readdir64_r(dp, dirent, &direntp) == 0) {
		/* check if end of dir */
		if (direntp == NULL)
			break;

		dname = direntp->d_name;

		/* check for '.' and '..' */
		if (dname[0] == '.') {
			if (dname[1] == '\0')
				continue;
			if ((dname[1] == '.') && (dname[2] == '\0'))
				continue;
		}

		/* skip 'root', 'swap', 'usr', 'volume_header' */
		if (strcmp(dname, "root") == 0)
			continue;
		if (strcmp(dname, "swap") == 0)
                        continue;
		if (strcmp(dname, "usr") == 0)
                        continue;
		if (strcmp(dname, "volume_header") == 0)
                        continue;

		strcpy(devpath, rdisk_path);
		strcat(devpath, "/");
		strcat(devpath, dname);
		
		if (stat64(devpath, &sbuf) == -1)
			continue;

		dname = devpath;

		if ((sbuf.st_mode & S_IFMT) != S_IFCHR)
			continue;

		newlen = sizeof(newpath);
		if (filename_to_devname(devpath, newpath, &newlen) != 0)
			dname = newpath;

		strcpy(diskpath, dname);
		if (diskp = strstr(diskpath, "/" EDGE_LBL_DISK "/")) {

			/* trim off path after "disk" */
			diskp = diskp + strlen("/" EDGE_LBL_DISK );
			*diskp = '\0';

			inv_length = sizeof(invent);
			error = attr_get(diskpath, _INVENT_ATTR,
					 (char *)&invent[0],
					 &inv_length, ATTR_ROOT);

			inv_length = (int)(inv_length/sizeof(inventory_t));
			if (error)
				inv_length = 0;
		}

		/* call the user func */
		uret = (*userfunc)(dname, &sbuf, &invent[0], inv_length, usrdat);
		if (uret != 0)
			break;
	}

	closedir(dp);
	return 0;
}
