/*
 * Copyright 1988-1996, Silicon Graphics, Inc.
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
#include <stdio.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <unistd.h>
#include <sys/attributes.h>
#include <sys/invent.h>
#include <fcntl.h>
#include <ftw.h>
#include <pwd.h>
#include <grp.h>
#ifndef IOCONFIG_TEST
#include <sys/serialio.h>
#endif
#include <sys/errno.h>
#include <sys/mtio.h>
#include <sys/dkio.h>
#include <sys/scsi.h>
#include <sys/dsreq.h>
#include <sys/ei.h>
#include <sys/plp.h>
#include <string.h>
#include <sys/times.h>
#include <sys/param.h>
#include <sys/iograph.h>
#include <sys/tpsc.h>
#include <sys/syssgi.h>
#include "error.h"
#include "ioconfig.h"

void parse_args(int ,char **);
char start_dir_name[MAXPATHLEN];
char *basename(char*);
int debug = 0;
int run_number = 1;	
int num_devices = MAX_BASE_DEVICES;	/* No. device_[args,process]_table
					 * entries
					 */

/* opens the volume header partition  */

FILE	*config_fp;

static void	process_ioc_scsicontrol_vertex(const char *,inventory_t *);

/* device_args_table:
 * 	contains the arguments to be passed to process_ioc_device_vertex
 * 	depending on the type of device.
 * format:	
 *	<suffix> <pattern> <start_num> <ioctl>
 *
 *		<suffix> = some devices like tty require that
 *			   we assign the controller number to the
 *			   vertex at "/hw/.../tty"  and use 
 *			   "/hw/.../tty/d"  to open & call the ioctl
 *			   to create aliases.
 *		
 *			   In general this is ignored.(IGNORE = "-1")
 *
 *		<pattern> = to uniquely identify this kind of device
 *		<start_num> = min. logical number for this device class
 *		<ioctl_num> = type of ioctl to call once the device has
 *				      been opened.
 */

/* #if 1 ...  
 * For testing the /var/sysgen/ioconfig device
 * policy specification
 */

#if 1
static device_args_t device_args_table[MAX_DEVICES] = 
{
	{EDGE_LBL_BUS, EDGE_LBL_SCSI_CTLR,SCSI_CTLR_START,SOP_MAKE_CTL_ALIAS},
	{"d","tty",TTY_START,SIOC_MKHWG},
	{"",IGNORE_STR,NETWORK_START,IGNORE},	
	{"",IGNORE_STR,ETHER_EF_START,IGNORE},	
	{"","rad_subsys",RAD_START,'mhwg'},
	{"",IGNORE_STR,PCKM_START,IGNORE},	
	{"",IGNORE_STR,PCKM_START,IGNORE},	
	{"",IGNORE_STR,EINT_START,EIMKHWG},
	{"",IGNORE_STR,PLP_START,PLPMKHWG}

};
#else
static device_args_t device_args_table[MAX_DEVICES] =
{
        {EDGE_LBL_BUS, EDGE_LBL_SCSI_CTLR,SCSI_CTLR_START,SOP_MAKE_CTL_ALIAS}
};

#endif
/*
 * device_process_table
 *	determines the ioconfig policy for each device.
 *
 * format:	 
 *	<class> <type> <state> <generic> <info>
 *
 *		<class> = inventory class
 *		<type>  = inventory type
 *		<state> = inventory state
 *		<generic> == 1
 *			<info> points to an entry in device_args_table
 *
 *			process_ioc_device_vertex() is called with these args
 *			which basically
 *				1.assigns the next availble controller number.
 *				2.opens the device
 *				3.does an ioctl
 *
 *		<generic> == 0
 *			<info> points to a device specific special
 *		                         implementation of
 *			process_xxx_vertex(const char *,inventory_t *)
 *
 *	This table is searched based on <class,type,state>.
 *
 *	Successful match occurs if
 *
 *		- class matches & type = state = IGNORE (-1)		OR   
 *
 *		- class matches & type matches & state = IGNORE (-1)	OR
 *
 *		- class matches & type matches & state matches
 *
 */

/* #if 1 ...
 * For testing the /var/sysgen/ioconfig device
 * policy specification
 */

#if 1
static device_process_t device_process_table[MAX_DEVICES] = {
	{INV_DISK,INV_SCSICONTROL,IGNORE,SPECIAL_CASE,(long)process_ioc_scsicontrol_vertex},
	{INV_SERIAL,INV_IOC3_DMA,IGNORE,GENERIC,(long)&device_args_table[IOC_TTY]},
	{INV_NETWORK,IGNORE,IGNORE,GENERIC,(long)&device_args_table[IOC_NETWORK]},
	{INV_NETWORK,INV_ETHER_EF,IGNORE,GENERIC,(long)&device_args_table[IOC_ETHER_EF]},
	{INV_AUDIO,INV_AUDIO_RAD,IGNORE,GENERIC,(long)&device_args_table[IOC_RAD]},
	{INV_MISC,INV_MISC_PCKM,1,GENERIC,(long)&device_args_table[IOC_PCK]},
	{INV_MISC,INV_MISC_PCKM,0,GENERIC,(long)&device_args_table[IOC_PCM]},
	{INV_MISC,INV_MISC_IOC3_EINT,IGNORE,GENERIC,(long)&device_args_table[IOC_EINT]},
	{INV_PARALLEL,INV_EPP_ECP_PLP,IGNORE,GENERIC,(long)&device_args_table[IOC_PLP]},

};
#else
static device_process_t device_process_table[MAX_DEVICES] = {
        {INV_DISK,INV_SCSICONTROL,IGNORE,SPECIAL_CASE,(long)process_ioc_scsicontrol_vertex}
};

#endif
/*
 * Search for the device process table entry for a device of particular
 * inventory class & inventory type
 */
device_process_t *dpt_search(int inv_class,int inv_type,int inv_state)
{
	int			i = num_devices - 1;
	device_process_t	*dpt;

	while ( i >= 0) {
		/* Get the current entry in the table */
		dpt = &device_process_table[i];
		/* Check if the entry's ckass is the
		 * same as the required class
		 */
		if (dpt->inv_class == inv_class) {

			/* If the entry's inventory type
			 * is IGNORE (-1) we already have
			 * a match. Otherwise check whether
			 * the entry's inventory type
			 * matches the required type.
			 */
			if (dpt->inv_type != IGNORE) {
				if (dpt->inv_type == inv_type) {
					/* If the entry's inventory state
					 * is IGNORE (-1) we already have
					 * a match. Otherwise check whether
					 * the entry's inventory state
					 * matches the required state.
					 */

					if (dpt->inv_state != IGNORE) {
						if (dpt->inv_state == inv_state)
							/* Class match;
							 * type match;
							 * state match
							 */
							return dpt;
					} else
						/* Class match 	; 
						 * type match 	; 
						 * state ignore
						 */
						return dpt;	
				}
			} else
				/* class match ;
				 * type ignore ;
				 */
				return dpt;
		}
		i--;
	}

	return 0;
}
/*
 * Copy any ISV specific device arguments from the /etc/iodev
 * file into the device_args_table.
 */
void
device_tables_update(void)
{
	FILE 			*dev_fp;
	int			vtype;
	device_args_t		*dat = &device_args_table[MAX_BASE_DEVICES];
	device_process_t	*dpt = &device_process_table[MAX_BASE_DEVICES];
	char			line[MAX_LINE_LEN];
	int			i;
	int			line_num = 0;

	char			dir_name[MAXPATHLEN];
	char			*start_dir_name = "/var/sysgen/ioconfig";
	DIR			*dirp;
	struct 	direct		*dp;
	int			link_len;

	/* Initialize the entries of the device_process_table 
	 * from index MAX_BASE_DEVICES to MAX_DEVICES
	 */
	for(i = MAX_BASE_DEVICES;i < MAX_DEVICES;i++)
		device_process_table[i].inv_class = IGNORE;


	/* Open the /var/sysgen/ioconfig directory to check for
	 * any device specific ioconfig info
	 */
	if ((dirp = opendir(start_dir_name)) == NULL) {
		if (debug)
			printf("device_tables_update: %s not present\n",
			       start_dir_name);
		return;
	}
	/* Go through each file in the /var/sysgen/ioconfig 
	 * directory
	 */
	while ((dp = readdir(dirp)) != NULL) {
		char	dev_file_name[MAXPATHLEN];
		struct	stat buf;

		/* skip over the current directory &
		 * parent directory entries in the directory
		 */
		if (strcmp(dp->d_name,".") == 0 ||
		    strcmp(dp->d_name,"..") == 0)
			continue;
		sprintf(dev_file_name,"%s/%s",start_dir_name,dp->d_name);
		if (debug)
			printf("Found %s device file\n",dev_file_name);

		/* Open the iodevice file */
		if ((dev_fp = fopen(dev_file_name,"r")) == NULL) {
			if (debug)
				printf("device_tables_update : File %s not there\n",
				       dev_file_name);
			return;
		}
	
		while (fgets(line,MAX_LINE_LEN,dev_fp)) {
			line_num++;
			/* check if we got a comment or a blank line */
			if (line[0] == POUND_CHAR)
				continue;
			/* Check for a blank line 
			 * This is done for the purpose of
			 * illegal file format detection.
			 */
			i = 0;
			while (isspace(line[i]) || line[i] == '\n')
				i++;
			if (strlen(line) == i)
				continue;
			/* Format of the /etc/iodev file:
			 * <class> <type> <state> <suffix> <pattern> <start_num> 
			 * <ioctl>
			 */
			dpt->generic = GENERIC;	/* Always the case is generic for an
						 * ISV.
						 */
			if (7 != sscanf(line,"%d %d %d %s %s %d %x",
					&(dpt->inv_class),
					&(dpt->inv_type),
					&(dpt->inv_state),
					dat->suffix,
					dat->pattern,
					&(dat->start_num),
					&(dat->ioctl_num))) {
				printf("Line %d doesnot seem to have the format:"
				       "<class> <type> <state> <case> <suffix> "
				       "<pattern> <start_num> <ioctl>\n",line_num);
				continue;
			}
			if (debug) {
				printf("dpt:class=%d,type=%d,state=%d,case=%d\t:\t",
				       dpt->inv_class,dpt->inv_type,dpt->inv_state,
				       dpt->generic);
				printf("dat:suffix=%s,pattern=%s,start_num=%d,ioctl=0x%x\n",
				       dat->suffix,dat->pattern,dat->start_num,
				       dat->ioctl_num);
			}
			dpt->info  	= (long)dat;
			num_devices++;
			if (num_devices == MAX_DEVICES) {
				if (debug)
					printf("Reached the pre-defined max# of devices");
				return;
			}
			dat++;dpt++;
		
		}
	}
}
/*
 * Generic process_vertex function.
 * The format of all the process_ioc_xxx_vertex routines below
 * is 
 *		get the next available controller number
 *		open the device
 *		do the ioctl to create the aliases
 * This routine does precisely the above steps but in a device
 * independent manner.
 */
void
process_ioc_device_vertex(const char 	*canonical_name,
			  inventory_t	*invp,
			  const char	*suffix,
			  char		*pattern,
			  int		start_num,
			  int		ioctl_num)
{
	int	fd;
	char	device[MAXDEVNAME];

	if (debug) {
		printf("process_ioc_device_vertex: class=%d,type=%d,state=%d,"
		       "suffix=%s,pattern=%s,start_num=%d,ioctl=0x%x\n",
		       invp->inv_class,invp->inv_type,invp->inv_state,
		       suffix,pattern,start_num,ioctl_num);
		if ((invp->inv_class == INV_DISK)
		    && (invp->inv_type == INV_SCSICONTROL))
			printf("FOUND SCSI CONTROLLER AT %s\n",
			       canonical_name);
		
		if ((invp->inv_class == INV_SERIAL) &&
		    ((invp->inv_type == INV_IOC3_DMA) || 
		     (invp->inv_type == INV_IOC3_PIO)))
			printf("FOUND A SERIAL CONTROL AT %s\n",
			       canonical_name);

		if (invp->inv_class == INV_NETWORK)
			printf("FOUND NETWORK AT %s\n",
			       canonical_name);

		if ((invp->inv_class == INV_AUDIO) &&
		    (invp->inv_type == INV_AUDIO_RAD))
			printf("FOUND AUDIO AT %s\n",canonical_name);

		if ((invp->inv_class == INV_MISC) &&
		    (invp->inv_type == INV_MISC_PCKM))
			printf("FOUND %s AT %s\n",
			       ((invp->inv_state & 0x1) == 1) ? "pckb" : "pcms",
			       canonical_name);

		if ((invp->inv_class == INV_MISC) &&
		    (invp->inv_type == INV_MISC_IOC3_EINT))
			printf("FOUND AN EI DEVICE AT %s\n",
			       canonical_name);

		if ((invp->inv_class == INV_PARALLEL) &&
		    (invp->inv_type == INV_EPP_ECP_PLP))
			printf("FOUND AN ECPP DEVICE AT %s\n",
			       canonical_name);

	}
	/* Assign the next available controller number */
	ioc_control_num_update(canonical_name,
			       pattern,
			       start_num,
			       invp);
	/* device name = canonical_name + suffix */
	if (strcasecmp(suffix,IGNORE_STR))
		sprintf(device,"%s/%s",canonical_name,suffix);
	else
		sprintf(device,"%s",canonical_name);

	/* Open the device */
	if ((fd = open(device,O_RDONLY)) == -1) {
		ioc_error(FILE_OPEN_ERR, "process_ioc_device_vertex", 
			  device);
		return;
	}
	/* call ioctl to build the aliases */
	if ((ioctl_num != IGNORE))
		(void) ioctl(fd, ioctl_num);

	close(fd);
	
}


int
ioc_control_num_update(const char *canonical_name,
		       char *pattern,
		       int min,
		       inventory_t *invp ) 
{
	int	hwgraph_ctlr_num;	/* logical controller number in the kernel
					 * stored in the hardware graph 
					 */
	int	persistent_ctlr_num;	/* controller number stored in the scsi 
					 * config file 
					 */
	int	highest_ctlr_num;	/* highest controller number used in the 
					 * scsi config file 
					 */

	highest_ctlr_num = min;


	/* check if there is already an entry for the controller 
	 * corresponding to the canonical path name in the 
	 * hardware graph 
	 */
	persistent_ctlr_num = ioconfig_find(canonical_name,&highest_ctlr_num,pattern);
	
	if ((hwgraph_ctlr_num = ioc_ctlr_num_get(invp)) == -1) {
		if (debug)
			printf("persistent_ctlr_num is %d\n",persistent_ctlr_num);
		/* logical controller number hasnot been assigned 
		 * to this controller yet in the hardware graph 
		 */
		if (persistent_ctlr_num == -1)	{/* not found in the file */
			
			/* add the logical controller number  -- canonical path name
			 * mapping to the scsi config file 
			 */
			ioconfig_add(highest_ctlr_num,canonical_name);
			
			/* tell  the kernel logical controller number corresponding
			 * to this scsi controller
			 */
			ioc_ctlr_num_put(highest_ctlr_num,canonical_name);
			return highest_ctlr_num;
		} else {
			/* tell  the kernel logical controller number corresponding
			 * to this scsi controller
			 */
			
			ioc_ctlr_num_put(persistent_ctlr_num,canonical_name);
			return hwgraph_ctlr_num;
		}
	} else {
		if ((persistent_ctlr_num != hwgraph_ctlr_num) &&
		    (persistent_ctlr_num != -1)) {
			/* logical controller number found in the kernel
			 * is different from the one in the scsi config file !!!
			 */

			/* Before printing out the warning make sure that
			 * this is not due to the module renumbering or
			 * moving the console basio
			 */
			if (hwgraph_ctlr_num >= min)
				ioc_error(CTLR_NUM_MISMATCH_ERR,"walk_fn",
					  persistent_ctlr_num,hwgraph_ctlr_num,canonical_name);
			else if (debug)
				printf("(IGNORE)Persistent(%d) --- HWG(%d)"
				       " controller num mismatch for %s\n",
				       persistent_ctlr_num,
				       hwgraph_ctlr_num,
				       canonical_name);
		}	

	}
	return hwgraph_ctlr_num;
}
void
process_ioc_scsicontrol_vertex(const char *canonical_name, inventory_t *invp)
{
	char	path[MAXDEVNAME];
	int 	fd;
	void	spawn_scsi_ctlr_process(const char *);

	ioc_control_num_update(canonical_name,EDGE_LBL_SCSI_CTLR, 
			       SCSI_CTLR_START, invp);
	sprintf(path,"%s/"EDGE_LBL_BUS,canonical_name);
	if (strstr(path,EDGE_LBL_MASTER))
		return;
	if((fd =  open(path, O_RDONLY)) == -1) {
		ioc_error(FILE_OPEN_ERR,"process_ioc_scsicontrol_vertex",
			  path);
		return;
	}
	/* Add an alias for the controller */
	if (ioctl(fd, SOP_MAKE_CTL_ALIAS ) == -1) {
		ioc_error(IOCTL_ERR,"process_ioc_scsicontrol_vertex",
			  path);
		close(fd);
		return;
	}

	close(fd);
	spawn_scsi_ctlr_process(canonical_name);
}
/*
 * Check for a raid controller. This is needed to 
 * prevent ioconfig spewing out error messages on trying
 * to open the raid luns because raid_agent has kept them
 * open after the first run of ioconfig.
 */
int
check_raid_controller(const char *canonical_name)
{
	char   		path[MAXDEVNAME];
	inventory_t	inv[10];
	int		inv_size = 10 * sizeof(inventory_t);
	char		*str = 0;
	char 		*pattern = "/"EDGE_LBL_SCSI"/";

	/* Make sure that the path is of the form /hw/.../lun/#/scsi */
	strcpy(path,canonical_name);
	strcat(path,"/");
	str =  strstr(path,pattern);
	if (!str)
		return 0;
	*(str) = '\0';
	if (debug)
		printf("check_raid_controller:path = %s\n",path);
	
	if (attr_get(path,_INVENT_ATTR,(char *)inv,&inv_size,0) == -1)
		return 0;
	if (inv->inv_type == INV_RAIDCTLR)	/* Raid lun */		
		return 1;
	return 0;
}
/*
 * Check if we are currently looking at a removable media
 * device like CDROM / ZIP disk.
 */
int
check_removable_media(const char *canonical_name) {
	char		path[MAXDEVNAME];
	inventory_t	inv[10];
	int		inv_size = 10 * sizeof(inventory_t);
	char		*str;
	char 		*pattern = "/"EDGE_LBL_DISK"/";
	
	strcpy(path,canonical_name);
	str =  strstr(path,pattern);
	*(str + strlen(pattern)) = '\0';
	
	if (attr_get(path,_INVENT_ATTR,(char *)inv,&inv_size,0) == -1)
		return 0;
	if (inv->inv_state & INV_REMOVE)		/* CDROM */
		return 1;

	if (inv->inv_type == INV_SCSIFLOPPY)		/* Removable 
							 * disk like ZIP 
							 */
		return 1;
	return 0;
	
}

int
scsi_ctlr_walk_fn(const char 		*canonical_name,
		  const struct stat 	*stat_p,
		  int			obj_type,
		  struct 	FTW	*ftw_info) 
{
	char	*str;
	char	cmd[MAXDEVNAME + 30];
	int	fd, len1, len2;
	char 	path[MAXPATHLEN];
	char *pattern2 = "/"EDGE_LBL_SCSI;
	char *pattern3 = EDGE_LBL_VOLUME"/"EDGE_LBL_CHAR;
	char *pattern4 = TPSC_DEFAULT"/"EDGE_LBL_CHAR;
	char *pattern5 = SMFD_DEFAULT"/"EDGE_LBL_CHAR;
	char *pattern6 = EDGE_LBL_VOLUME_HEADER"/"EDGE_LBL_CHAR;
	struct stat buf;

	/* Check for user scsi device */
	len1 = strlen(pattern2);
	len2 = strlen(canonical_name);
	str =  (char *)canonical_name + (len2 - len1);

	if (run_number != 2 || !check_raid_controller(canonical_name)) {
		if (strcasecmp(pattern2, str) == 0) {
			if (debug)
				printf("Found a scsi device at %s\n",
				       canonical_name);
			/* 
			 *Hey ... if the canonical_name is a directory skip it,
			 *a ioctl will not work very well on a directory.
			 */
			if (!stat(canonical_name, &buf) && 
			    S_ISDIR(buf.st_mode))
				return 0;

			if((fd = open(canonical_name, O_RDONLY)) < 0) {
				/* Donot print error messages for raid
				 * devices because raid agent is
				 * started before ioconfig due which
				 * we get resource busy messages
				 */
				if (!check_raid_controller(canonical_name))
					ioc_error(FILE_OPEN_ERR, 
						  "scsi_ctlr_walk_fn",
						  canonical_name);
				return 0;
			}

			if (ioctl(fd, DS_MKALIAS) < 0)
				ioc_error(IOCTL_ERR,
					  "scsi_ctlr_walk_fn", canonical_name);

			close(fd);	

		}
	}


	/* Check for tape device */
	len1 = strlen(pattern4);
	len2 = strlen(canonical_name);
	str =  (char *)canonical_name + (len2 - len1);
	
	if (strcasecmp(pattern4, str) == 0) {
		if (debug)
			printf("Found a tape device at %s\n",canonical_name);

		/* 
		 *	Hey ... if the canonical_name is a directory skip it,
	  	 *	a ioctl will not work very well on a directory.
		 */
		if (!stat(canonical_name, &buf) && S_ISDIR(buf.st_mode))
			return 0;
		if((fd = open(canonical_name, O_RDONLY)) < 0) {
			ioc_error(FILE_OPEN_ERR, "scsi_ctlr_walk_fn",
					canonical_name);
			return 0;
		}

		if (ioctl(fd, MTALIAS) < 0)
			ioc_error(IOCTL_ERR,"scsi_ctlr_walk_fn", 
				  canonical_name);

		close(fd);	

	}

	/* Check for floppy device */
	len1 = strlen(pattern5);
	len2 = strlen(canonical_name);
	str =  (char *)canonical_name + (len2 - len1);
	
	if (strcasecmp(pattern5, str) == 0) {
		if (debug)
			printf("Found a scsi device at %s\n",canonical_name);

		/* 
		 *	Hey ... if the canonical_name is a directory skip it,
	  	 *	a ioctl will not work very well on a directory.
		 */
		if (!stat(canonical_name, &buf) && S_ISDIR(buf.st_mode))
			return 0;

		if((fd = open(canonical_name, O_RDONLY)) < 0) {
			ioc_error(FILE_OPEN_ERR, "scsi_ctlr_walk_fn",
					canonical_name);
			return 0;
		}

		if (ioctl(fd, DIOCMKALIAS) < 0)
			ioc_error(IOCTL_ERR,"scsi_ctlr_walk_fn",
				  canonical_name);

		close(fd);	

	}


	/* check if we have ......./volume/char as the canonical name */
	if ((str = strstr(canonical_name, pattern3)))  {
		if (strstr(str+strlen(pattern3),SLASH)) 
			return 0;
	} else
		return 0;

	/* If we got non-removable disk basically hard disk
	 * we have already opened it in the first run of 
	 * ioconfig no need to open it now
	 */
	if (run_number == 2 && 
	    !check_removable_media(canonical_name)) {
		return 0;
	}
	if (debug)
		printf("P%d OPENING DISK AT %s\n",getpid(),canonical_name);
	if ((fd = open(canonical_name,O_RDONLY)) == -1) {
		if (check_removable_media(canonical_name))
			return 0;
		else {
			ioc_error(FILE_OPEN_ERR,
				  "scsi_ctlr_walk_fn",canonical_name);
			return 0;
		}
	}
	sprintf(path,"%s",canonical_name);
	str = strstr(path, pattern3);
	sprintf(str,"%s", EDGE_LBL_PARTITION);

	/* we can do a check before opening the volume
	 * header to see if the partition vertices exist
	 * already due to some program like xlv_plex opening
	 * disks .if this check succeeds then we can do 
	 * the following ioctl. 
	 *
	 * OR
	 *
	 *  we can eliminate the check and always do the ioctl . 
	 *
	 * latter alternative has been  chosen for simplicity and not 
	 * much effect on performance
	 */
	if (ioctl(fd,DIOCMKALIAS) == -1) {
		ioc_error(IOCTL_ERR,
			  "scsi_walk_fn",canonical_name);
		close(fd);
	}

	close(fd);

	return 0;
}

/* fork off a procees to open the volume header for 
 * each disk found for the controller  corresponding
 * to the canonical name
*/
void
spawn_scsi_ctlr_process(const char *canonical_name)
{

	int		parent;
	if ((parent = fork()) == -1)
		ioc_error(FORK_FAIL_ERR,"spawn_scsi_ctlr_process");

	if (parent) 
		return;
	if (debug)
		printf("Spawned a scsi ctlr process for %s\n",canonical_name);

	/* child process & following links */
	if (nftw(canonical_name,scsi_ctlr_walk_fn,MAX_HWG_DEPTH,FTW_PHYS))
		ioc_error(FTW_ERR);
	exit(0); 
}

/* 
 * traversal routine for the file tree walker
 * it informs the kernel about the logical controller numbers
 * of the existing controllers
 * it then tries to  open all the volume headers of the disks
 * under the existing controller.
 */
int
walk_fn(const char 		*canonical_name,
	const struct stat 	*stat_p,
	int			obj_type,
	struct 	FTW		*ftw_info) 
{

    inventory_t     	inv[10];
    int			num, i;
    int			inv_size = 10 * sizeof(inventory_t);
    char		*namep = (char *)canonical_name;
    device_process_t	*dpt;

    /* no need to do processing for this directory */
    if (strstr(canonical_name,"/hw/.invent"))
	return 0;

    if (strstr(canonical_name,"/hw/hw"))
	return 0;
    
    /* get the inventory info associated with this hardware graph vertex */
	
    if (attr_get((char *)canonical_name, _INVENT_ATTR,
		 (char *)inv, &inv_size, 0) == -1) {
	extern int errno;
	ioc_error(ATTR_GET_ERR,"walk_fn",canonical_name,_INVENT_ATTR);
	return 0;
    }

    canonical_name = namep;
    num = inv_size / sizeof(inventory_t);
    for (i = 0; i < num; i++) {
	if (debug)
	    printf("%s: class %d type %d controller %d unit %d state %d\n",
		   canonical_name,
		   inv[i].inv_class, inv[i].inv_type,
		   inv[i].inv_controller, inv[i].inv_unit,
		   inv[i].inv_state);

	/* For networking devices, treat the controller as the type */
	if (inv[i].inv_class == INV_NETWORK)
		inv[i].inv_type = inv[i].inv_controller;
	
	/* Search for the required entry in
	 * the device_process_table for
	 * the given inventory class , type & state
	 */
	dpt = dpt_search(inv[i].inv_class,
			 inv[i].inv_type,
			 inv[i].inv_state);
	if (!dpt)
		continue;
	
	if (dpt->generic) {

		/* Generic case is device_args_table driven */
		device_args_t		*dat;

		dat = (device_args_t *)dpt->info;
		if (!dat)
			continue;
		process_ioc_device_vertex(canonical_name,
					  &inv[i],
					  dat->suffix,
					  dat->pattern,
					  dat->start_num,
					  dat->ioctl_num);
	} else {
		/* Special case is device_process_table driven */
		process_fn_t		process_fn;

		process_fn = (process_fn_t)dpt->info;
		process_fn(canonical_name,&inv[i]);
	}
    }

    return 0;
}
/* 
 * set ownership and permissions for the io devices
 */
void
ioc_owner_perm_set(void)
{
	FILE 	*perm_fp;
	int	rv;
	char	dev_name[MAXPATHLEN];
	char	owner[MAX_NAME_LEN];
	char	group[MAX_NAME_LEN];
	char	line[MAX_LINE_LEN];
	char	cmd[MAX_CMD_LEN];
	int	perm;

	/* open the file which is the persistent repository of
	 * ownership and permissions for devices.
	 */
	if ((perm_fp = fopen(IOPERMFILE,"r")) == NULL) {
		if (debug)
			printf("ioc_owner_perm_set : File %s not there\n",
			       IOPERMFILE);
		return;
	}
	/* 
	 * permission file format:
	 * 		comment lines  start with a #
	 * 		Other lines are of the following format
	 *	 		<device_name> <owner> <group> <nnn>
	 *		where device name can have wild card character(*)
	 */
	while (fgets(line,MAX_LINE_LEN,perm_fp)) {
		
		/* check if we got a comment line */
		if (line[0] == POUND_CHAR) {
			continue;
		}
		rv = sscanf(line,"%s %o %s %s",
			    dev_name,&perm,owner,group);
		if (rv != 4 ) 
			continue;
		if (debug)
			printf("dev_name=%s owner=%s "
			       "group=%s perms=%o\n",
			       dev_name,owner,group,perm);
		
		/* check if there is a wild card */
		if (strstr(line,ASTERISK_STR)) {
			/* set the appropriate owner & group for the list of devices */
			sprintf(cmd,"echo %s | xargs chown %s.%s",dev_name,owner,group);
			system(cmd);
			/* set the permissions for the list of devices */
			sprintf(cmd,"echo %s | xargs chmod %o",dev_name,perm);
			system(cmd);
		} else {
			struct passwd	*pwd_p;
			struct group	*grp_p;	
			uid_t		uid;
			gid_t		gid;

			/* get the owner and group id for the owner */
			if ((pwd_p = getpwnam(owner)) == NULL) {
				ioc_error(GETPWNAM_ERR,"ioc_owner_perm_set",
					  owner);
				continue;
			}
			uid = pwd_p->pw_uid;
			/* get the group id  */
			if ((grp_p = getgrnam(group)) == NULL) {
				ioc_error(GETPWNAM_ERR,
					  "ioc_owner_perm_set : invalid group",
					  group);
				continue;

			}
			gid = grp_p->gr_gid;
			if (debug)
				printf("dev_name = %s uid = %d gid = %d\n",
				       dev_name, uid,gid);

			/* change the owner & group id for this device */
			if (chown(dev_name,uid,gid) == -1) {
			}
			/* change the permissions for this device */
			if (chmod(dev_name,perm) == -1) {
			}
		}
	}
		
	fclose(perm_fp);	
	


}
/*
 * Do the special purpose optimized walk in 
 * /hw/scsi_ctlr directory
 */
int
do_optimal_scsi_ctlr_walk(void)
{
	char		dir_name[MAXPATHLEN];
	char		*start_dir_name = "/hw/"EDGE_LBL_SCSI_CTLR;
	DIR		*dirp;
	struct 	direct	*dp;
	int		link_len;

	/* Open the /hw/scsi_ctlr directory */
	if ((dirp = opendir(start_dir_name)) == NULL) {
		ioc_error(OPENDIR_ERR,"do_optimal_scsi_ctlr_walk",dir_name);
		return -1;
	}
	/* Go through each symbolic link in the /hw/scsi_cltr
	 * directory
	 */
	while ((dp = readdir(dirp)) != NULL) {
		char	link_name[MAXPATHLEN];
		struct	stat buf;
		/* skip over the current directory &
		 * parent directory entries in the directory
		 */
		if (strcmp(dp->d_name,".") == 0 ||
		    strcmp(dp->d_name,"..") == 0)
			continue;
		sprintf(link_name,"%s/%s",start_dir_name,dp->d_name);
		if (debug)
			printf("do_optimal_scsi_ctlr_walk: looking at  %s\n",link_name);
		/* Check if /hw/scsi_ctlr/# is a symbolic link (for new
		 * platforms like IP27 , IP30)
		 * or a proper directory (for old platforms like 
		 * IP19,IP21,IP22,..)
		 */
		if (!lstat(link_name, &buf) && S_ISLNK(buf.st_mode)) {
			if ((link_len = readlink(link_name,
						 dir_name,
						 MAXPATHLEN)) != -1) {
				dir_name[link_len] = '\0';
				if (debug)
					printf("do_optimal_scsi_ctlr_walk: spawning process "
					       "for dir_name[LINK] = %s\n",
					       dir_name);
				/* Spawn off a process for walking 
				 * down the name space of this
				 * scsi controller
				 */
				spawn_scsi_ctlr_process(dir_name);
			} else {
				/* Readlink failed */
				ioc_error(READLINK_ERR,"do_optimal_scsi_ctlr_walk",link_name);
				/* Try to process the other entries in the 
				 * directory 
				 */
				continue;
			}
		} else {
			/* Spawn off a process for walking 
			 * down the name space of this
			 * scsi controller
			 */
			if (debug)
				printf("do_optimal_scsi_ctlr_walk:"
				       "spawning process for dir_name = %s\n",
				       link_name);
			spawn_scsi_ctlr_process(link_name);
		}
	
			
	}
	return 0;
}
/*
 * Sync the controller number file (/etc/ioconfig.conf)
 */
void
ioc_control_num_file_sync(void)
{
	int	fd;

	/* Open the ioconfig controller number map file */
	if ((fd = open(IOCONFIG_CTLR_NUM_FILE,O_RDWR)) < 0) {
		ioc_error(FILE_OPEN_ERR,"ioc_control_num_file_sync",
			  IOCONFIG_CTLR_NUM_FILE);
		return;
	}
	/* Make sure that all the data gets onto secondary storage */
	(void)fsync(fd);
	(void)close(fd);
	
}
int
main(int argc,char **argv)
{
	extern int	errno;

	parse_args(argc, argv);

	/* open the scsi config file which contains the 
	 * logical controller number to canonical name mapping
	 */

	/* if the file exists try to open it for reading and 
	 * writing
	 */
	/* try creating the file */
	if ((config_fp = fopen(IOCONFIG_CTLR_NUM_FILE,"a+")) == NULL) {
		ioc_error(FILE_OPEN_ERR,"main",IOCONFIG_CTLR_NUM_FILE);
		exit(1);
	} 
	device_tables_update();
	/*
	 * Run number tells which run of ioconfig is being done
	 * Basically the idea is to optimise the second run of
	 * ioconfig when the system comes up. 
	 * Right now the idea is to prune down the hwgraph walk 
	 * to just the sub-trees of the scsi controllers.
	 * For a particular scsi controller there is also no
	 * point in opening the hard disk since it was already
	 * done in the first run
	 */
	if (run_number == 1) {
		if (nftw(start_dir_name, walk_fn, MAX_HWG_DEPTH, FTW_PHYS))
			ioc_error(FTW_ERR);
	} else if (run_number == 2) {
		if (do_optimal_scsi_ctlr_walk())
			ioc_error(FTW_ERR);
	}
	fclose(config_fp);

	/* wait for all the controller processes to terminate */
	while ((wait(NULL) != -1) || (errno != ECHILD));

	/* set the ownership & permissions for the devices */
	ioc_owner_perm_set();

	/*
	 * Set up primary swap in the case that the user
	 * specified a path name that was not available
	 * until after ioconfig had created it.
	 */
	syssgi(SGI_EARLY_ADD_SWAP);
	/* Make sure that controller number mapping file gets written
	 * to the disk .
	 */
	ioc_control_num_file_sync();
	return 0;
}

#define USAGE "ioconfig [-d]  -f starting_directory\n"

void
parse_args(int argc,char **argv) 
{
	extern char 	*optarg; 
	int 		c;
	int 		dir_entered = 0;
	int 		link_len;	
	char		link_name[MAXPATHLEN];

	while (( c = getopt(argc, argv, "df:2")) != EOF) 
		switch(c) {
		case 'd':
			debug = 1;
			break;
		case 'f':
			strncpy(start_dir_name, optarg,MAXPATHLEN-1);
			/* This is the only place where ioconfig can 
			 * traverse a link (i.e the starting directory
			 * for the hwgraph walk can be a symbolic link)
			 */
			if ((link_len = readlink(start_dir_name,
						 link_name,
						 MAXPATHLEN)) == -1) {
				if (debug)
					printf("parse_args : %s not a link\n",
						  start_dir_name);
			} else {
				link_name[link_len] = '\0';
				strcpy(start_dir_name,link_name);
			}
			/* Now starting directory name points to a
			 * real directory since if it were a symbolic
			 * link it has been traversed above to get to
			 * the real directory
			 */
			if (debug)
				printf("start dir name = %s\n",start_dir_name);
			dir_entered = 1;
			break;
		case '2':
			run_number = 2;
			break;
		case '?':
		default :
			fprintf(stderr, "usage: %s", USAGE);
			exit(0);
		}
	if (!dir_entered ) {
		fprintf(stderr, "usage: %s", USAGE);
		exit(0);
	}
}


/*
 * Private copy of basename() so we don't have
 * to link with libgen .
 */
char *
basename(char *s)
{
	register char	*p;

	if( !s  ||  !*s )			/* zero or empty argument */
		return  ".";

	p = s + strlen( s );
	while( p != s  &&  *--p == '/' )	/* skip trailing /s */
		*p = '\0';
	
	if ( p == s && *p == '\0' )		/* all slashes */
		return "/";

	while( p != s )
		if( *--p == '/' )
			return  ++p;

	return  p;
}
