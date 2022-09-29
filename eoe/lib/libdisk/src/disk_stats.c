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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/stat.h>
#include <sys/elog.h>
#include <sys/types.h>
#include <sys/attributes.h>
#include <sys/invent.h>
#include <sys/conf.h>
#include <sys/iograph.h>
#include <diskinfo.h>
#include <ftw.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>

#define DKIOTIME_DEBUG 0

struct diosetup * (*dio_alloc)(void);
struct disk_iostat_info * (*disk_iostat_info_alloc)(void);

static char *
valid_direct_disk_name(const char *disk_alias_name)
{
	char	*str;

	/*
	 * check for name of type .../dks...vol
	 */
	if ((str = strrchr(disk_alias_name, '/')) != NULL &&
	    (str = strstr(str, "dks")) != NULL &&
	    strstr(str, "vol") != NULL)
	{
		return str;
	}
	return NULL;
}

static char *
valid_fabric_disk_name(const char *disk_alias_name)
{
	char	*nodename;
	char	*str;

	str = strstr(disk_alias_name, "rdisk/");
	if (str == NULL)
		return NULL;
	nodename = str + strlen("rdisk/");
	str = strchr(nodename, '/');
	if (str == NULL)
		return NULL;
	str++;
	if (strncmp(str, "lun", 3) != 0)
		return NULL;
	if (strstr(str, "vol/c") == NULL)
		return NULL;
	return nodename;
}


/*
 * walk function for scanning each disk volume in the
 * /hw/disk directory
 */

/*ARGSUSED*/
int
disk_volume_scan(const char 		*disk_alias_name,
		 const struct stat 	*stat_p,
		 int			obj_type,
		 struct FTW		*ftw_info) 
{
	char		*str;	
	struct diosetup *ds;	
	
#if DKIOTIME_DEBUG
	printf("disk_volume_scan : disk_alias_name = %s\n",
	       disk_alias_name);
#endif
	/* check if the name is of the form dks#d#vol */
	if (str = valid_direct_disk_name(disk_alias_name))
	{
		/* allocate an entry in the dio setup list */
		ds = dio_alloc();
		/* setup the new dio list entry */
		strcpy(ds->dname, str);
		str = strstr(ds->dname, "vol");
		str[0] = '\0';
#if DKIOTIME_DEBUG
		printf("disk_volume_scan : added %s\n", ds->dname);
#endif
	}
	return 0;
}

/*ARGSUSED*/
int 
neo_disk_scan(	const char		*disk_alias_name,
		const struct stat	*stat_p,
		int			 obj_type,
		struct FTW		*ftw_info) 
{
	struct disk_iostat_info		*dio;
	struct iotime			 dummy_iotime;
	int				 dummy_size = sizeof(dummy_iotime);
	char				*str;
	char				 diskname[128];

#if DKIOTIME_DEBUG
	printf("neo_disk_scan : checking %s\n", disk_alias_name);
#endif
	if (attr_get(disk_alias_name, INFO_LBL_DKIOTIME,
		     (char *) &dummy_iotime, &dummy_size, 0) == 0)
	{
		/*
		 * Allocate statistics structure and set device name
		 */
		dio = disk_iostat_info_alloc();
		dio->devicename = malloc(strlen(disk_alias_name) + 1);
		strcpy(dio->devicename, disk_alias_name);

		/*
		 * Get disk name.
		 */
		if (str = valid_fabric_disk_name(disk_alias_name))
		{
			strcpy(diskname, str);
			str = strstr(diskname, "vol");
			strcpy(str, str+3);
		}
		else if (str = valid_direct_disk_name(disk_alias_name))
		{
			strcpy(diskname, str);
			str = strstr(diskname, "vol");
			*str = '\0';
		}
		else
			strcpy(diskname, disk_alias_name);
		dio->diskname = malloc(strlen(diskname) + 1);
		strcpy(dio->diskname, diskname);
#if DKIOTIME_DEBUG
		printf("neo_disk_scan : added %s\n", diskname);
#endif
	}
	return 0;
}

/* 
 * get the disk info hanging off the vertex corresponding
 * to the given alias
 */
int
dkiotime_get(char *dsname, struct iotime  *iotim) 
{
	int		dkiotimesz = sizeof(struct iotime);	
	char		disk_alias_name[108];
	
	sprintf(disk_alias_name,"/hw/"EDGE_LBL_RDISK"/%svol", dsname);
#if DKIOTIME_DEBUG
	printf("dkiotime_get : disk_alias_name = %s\n", disk_alias_name);
#endif
	/* get the dkiotime time structure for this disk drive */
	if (attr_get((char *)disk_alias_name,INFO_LBL_DKIOTIME,
		     (char *)iotim,&dkiotimesz,0) == -1) {
#if DKIOTIME_DEBUG
		printf("dkiotime_get : attr_get failed on %s\n", disk_alias_name);
#endif
		return -1;
	}
	
	return 0;
}

/*
 * diskname is the disk alias name (/hw/rdisk/...)
 */
int
dkiotime_neoget(char *diskname, struct iotime *iotim) 
{
	int		dkiotimesz = sizeof(struct iotime);	
	
#if DKIOTIME_DEBUG
	printf("dkiotime_neoget : diskname = %s\n", diskname);
#endif
	/* get the dkiotime time structure for this disk drive */
	if (attr_get(diskname, INFO_LBL_DKIOTIME, (char *) iotim, &dkiotimesz, 0) == -1)
	{
#if DKIOTIME_DEBUG
		printf("dkiotime_get : attr_get failed on %s\n", dsname);
#endif
		return -1;
	}
	return 0;
}

/*
 * Build a table of pointers to disk iotime structs
 *
 * The iotime structs in the scsi driver are not in a linear array
 * and furthermore there are different kinds of disk drivers: scsi and
 * maybe PCI RAID (not currently supported). This routine finds all
 * of the disks in each driver and builds one array of pointers to each
 * drive's iotime struct. 
 * It also builds a parallel array of the device names; these will be
 * explicitly embedded in the data records since not all devices 
 * may be equipped and there may be gaps between drive numbers.
 *
 * By the way, don't forget that rpc.rstatd is just like this.
 */
void
sgi_disk_setup(struct diosetup * (alloc_fn)(void))
{
	char			*dir = "/hw/"EDGE_LBL_RDISK;
	struct 	stat		buf;

#if DKIOTIME_DEBUG
	printf("sgi_disk_setup\n");
#endif
	dio_alloc = alloc_fn;
	/* check if there is a /hw/rdisk directory */
	if (stat(dir,&buf) == -1)
		return;

	/* check all the vol entries in the /hw/rdisk directory */
	nftw(dir,disk_volume_scan,1,FTW_PHYS);
}

void
sgi_neo_disk_setup(struct disk_iostat_info * (alloc_fn)(void))
{
	char			*dir = "/hw/"EDGE_LBL_RDISK;
	struct 	stat		buf;

#if DKIOTIME_DEBUG
	printf("sgi_neo_disk_setup\n");
#endif
	disk_iostat_info_alloc = alloc_fn;

	/* check if there is a /hw/rdisk directory */
	if (stat(dir, &buf) == -1)
		return;

	/* check all the vol entries in the /hw/rdisk directory */
	nftw(dir, neo_disk_scan, 3, FTW_PHYS);
}
