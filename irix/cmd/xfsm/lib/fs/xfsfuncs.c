/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"xfsfuncs.c: $Revision: 1.4 $"

/*
 * xfsfuncs.c -- library of xfs functions to mount/unmount/create/delete
 * file systems.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pfmt.h>
#include <pwd.h>
#include <grp.h>
#include <mntent.h>
#include <exportent.h>
#include <sys/debug.h>
#include "xfs_fs_defs.h"
#include "xfs_utils.h"

#include <netinet/in.h>
#include <netdb.h>


extern int errno;
	
/*
 *	create_mnt_dir()
 *	Creates the mount directory, if it does not exist already. The
 *	user, group and mode are set for newly created directory. 
 *	The mode is an octal value which is represnted as a string.
 *	It is converted internally to the octal value it represents.
 *	If the creation is successful it returns a 0. 
 *	On error, 1 is returned.
 */

int
create_mnt_dir(const char* username, const char* groupname, const char* 
mode_str, const char* dirname,char** msg)
{
	int		returnValue = 0;
	struct passwd	*passwd_entry = NULL;
	struct group	*group_entry = NULL;
	mode_t		mode = S_IFDIR | 0775;
	uid_t		owner = -1;
	gid_t		group = -1;
	char		str[BUFSIZ];
	struct stat	stat_buf;

	/* Check the parameters */
	if(dirname == NULL) {
		returnValue = 1;
	}
	else if(stat(dirname, &stat_buf) == 0) {
	    if ((stat_buf.st_mode & S_IFMT) != S_IFDIR) {
		/*	TODO:	Add to catalog	*/
		sprintf(str, "%s exists but is not a directory.\n", dirname);
		add_to_buffer(msg, str);
		returnValue = 1;
	    }
	}
	else {
	    umask(0);

	    /* Convert the mode string to octal value */
	    if (mode_str != NULL) {
		mode = (mode_t) strtol(mode_str, (char **)NULL, 8);
	    }

	    /* Get the user id from the username */
	    if (username != NULL) {
		if((passwd_entry = getpwnam(username)) == NULL) {
		    sprintf(str,
			gettxt(":70", "User %s not found.\n"), username);
			add_to_buffer(msg, str);
			returnValue = 1;
		    } else {
			owner = passwd_entry->pw_uid;
		}
	    }

	    /* Get the group id */
	    if (groupname != NULL) {
		if ((group_entry = getgrnam(groupname)) == NULL) {
		    sprintf(str,
			gettxt(":71", "Group %s not found.\n"), groupname);
			add_to_buffer(msg, str);
			returnValue = 1;
		    } else {
			group = group_entry->gr_gid;
		}
	    }

	    /* Create the directory */
	    if (returnValue == 0) {
		if (mkdir(dirname, mode) != 0) {
		    returnValue = 1;
		}
		else if ((owner != -1) || (group != -1)) {
		    if (chown(dirname, owner, group) != 0) {
			if (owner != -1 && group != -1) {
			sprintf(str, gettxt(":164",
			"%s: Unable to change owner to %s and group to %s.\n%s\n"),
					dirname, username, groupname,
					strerror(errno));
			} else if (owner != -1) {
				sprintf(str, gettxt(":165",
				"%s: Unable to change owner to %s.\n%s\n"),
					dirname, username, strerror(errno));
			}
			else {
				sprintf(str, gettxt(":166",
				"%s: Unable to change group to %s.\n%s\n"),
					dirname, groupname, strerror(errno));
			}
			add_to_buffer(msg, str);
			returnValue = 1;
		    }
		}
	    }
	}

	return(returnValue);
}
				

/*
 *	is_valid_fs_type()
 *	Checks if the filesystem type belongs to the list of known types.
 *	If the filesystem is of unknown type, a 1 is returned. For
 *	a known type 0 is returned.
 */

int
is_valid_fs_type(const char* fs_type)
{
	int index=0;

	if (fs_type != NULL)
	{
		while (xfs_fs_types[index] != NULL) 
		{
			if (strcmp(fs_type,xfs_fs_types[index])==0)
			{
				return(0);
			}
			index++;
		}
	}
	return(1);
}


/*
 *	to_lower_fs_type_str()
 *	Converts the filesystem type string to the lower case. This
 *	is done to enable the filesystem type string specified in the GUI
 *	to be upper or lower case. The filesystem, however recognizes only 
 *	the lower case type string.
 *	Return Value : 	The filesystem type string in lower case.
 *			NULL in case of unknown filesystem type.
 *
 */

char*
to_lower_fs_type_str(const char* fs_type_str)
{
	char*	lc_fs_type_str=NULL;

	if (fs_type_str == NULL)
	{
		lc_fs_type_str=NULL;
	}
	else if (is_valid_fs_type(fs_type_str)==0)
	{
		lc_fs_type_str = strdup(fs_type_str);
		ASSERT(lc_fs_type_str!=NULL);
	}
	else if (strcmp(fs_type_str,"EFS")==0)
	{
		lc_fs_type_str = strdup("efs");
		ASSERT(lc_fs_type_str!=NULL);
	}
	else if (strcmp(fs_type_str,"XFS")==0)
	{
		lc_fs_type_str = strdup("xfs");
		ASSERT(lc_fs_type_str!=NULL);
	}
	else if (strcmp(fs_type_str,"NFS")==0)
	{
		lc_fs_type_str = strdup("nfs");
		ASSERT(lc_fs_type_str!=NULL);
	}
	else if (strcmp(fs_type_str,"PROC")==0)
	{
		lc_fs_type_str = strdup("proc");
		ASSERT(lc_fs_type_str!=NULL);
	}
	else if (strcmp(fs_type_str,"FD")==0)
	{
		lc_fs_type_str = strdup("fd");
		ASSERT(lc_fs_type_str!=NULL);
	}
	else if (strcmp(fs_type_str,"CDFS")==0)
	{
		lc_fs_type_str = strdup("cdfs");
		ASSERT(lc_fs_type_str!=NULL);
	}
	else if (strcmp(fs_type_str,"ISO9660")==0)
	{
		lc_fs_type_str = strdup("iso9660");
		ASSERT(lc_fs_type_str!=NULL);
	}
	else if (strcmp(fs_type_str,"DOS")==0)
	{
		lc_fs_type_str = strdup("dos");
		ASSERT(lc_fs_type_str!=NULL);
	}
	else if (strcmp(fs_type_str,"HFS")==0)
	{
		lc_fs_type_str = strdup("hfs");
		ASSERT(lc_fs_type_str!=NULL);
	}
	else if (strcmp(fs_type_str,"SWAP")==0)
	{
		lc_fs_type_str = strdup("swap");
		ASSERT(lc_fs_type_str!=NULL);
	}
	else if (strcmp(fs_type_str,"CACHEFS")==0)
	{
		lc_fs_type_str = strdup("cachefs");
		ASSERT(lc_fs_type_str!=NULL);
	}
	else 
	{
		lc_fs_type_str = strdup(fs_type_str);
		ASSERT(lc_fs_type_str!=NULL);
	}

	return(lc_fs_type_str);
}

/*
 *	get_fs_name(tabfile,mnt_dir)
 *	This functions looks up the system file(tabfile) for the entry 
 *	containing the mount directory. If found, it returns the 
 *	filesystem name corresponding to the entry. Otherwise, a NULL
 *	is returned.
 *	The tabfile is typically /etc/fstab or /etc/mtab.
 */

char*
get_fs_name(char* tabfile,char* mnt_dir)
{
	FILE *fp;
	struct mntent *fstab_entry;
	char* fs_name=NULL;
	
	/* Check the parameters, tabfile and filename */
	if ((tabfile == NULL) || (mnt_dir == NULL))
	{
		fs_name = NULL;
	}
	else if ((fp = setmntent(tabfile,"r")) != NULL)
	{
		/* Read the entries in the file, comparing 
		   to the filename */
	
		while ((fstab_entry = getmntent(fp)) != NULL)
		{
			if ((fstab_entry->mnt_dir!=NULL) &&
				(strcmp(fstab_entry->mnt_dir,mnt_dir)==0)) 
			{
				fs_name = strdup(fstab_entry->mnt_fsname);
				ASSERT(fs_name);
				break;
			}
		}

		/* Close the file */
		endmntent(fp);
	}

	return(fs_name);
}

/*
 *	get_mnt_dir(filename)
 *	This function checks the type of the filename. If it is 
 *	not a character/block spl file the filename is the mount point itself.
 *	Otherwise, the filesystem has an entry in /etc/mtab (if it is 
 *	currently mounted) or in /etc/fstab (if the file system is created
 *	earlier & exists). Depending on the tabfile indicated one of them
 *	is looked up. If the filesystem entry is found the mount directory
 *	is returned. Otherwise a NULL is returned.
 */
char *
get_mnt_dir(char *tabfile, char *filename)
{
	struct mntent	mtab_ent;
	struct stat	stat_buf;

	/* Check the parameters */
	if((tabfile == NULL) || (filename == NULL)) {
		return(NULL);
	}

	if ((stat(filename, &stat_buf) == 0) &&
	  (((stat_buf.st_mode & S_IFMT) == S_IFREG) ||
	   ((stat_buf.st_mode & S_IFMT) == S_IFDIR))) {
		return(strdup(filename));
	}

	if(xfs_getmntent(tabfile, filename, &mtab_ent) == 0) {
		free(mtab_ent.mnt_fsname);
		free(mtab_ent.mnt_type);
		free(mtab_ent.mnt_opts);
		return(mtab_ent.mnt_dir);
	}

	return(NULL);
}

/*
 *	get_export_opts()
 *	Checks the file /etc/exports to determine if the file system is
 *	currently exported. The export options are stored in the
 *	export_opts string, since the information returned by getexportent()
 *	is static. The returnValue is NULL, if the file system does 
 *	not have an entry in /etc/exports.
 */

char*
get_export_opts(char *fs_mnt_dir)
{
	struct exportent	ent;

	/* Check the parameter fs_mnt_dir */
	if (fs_mnt_dir == NULL) {
		return(NULL);
	}

	if(xfs_get_exportent(EXTAB, fs_mnt_dir, &ent) == 0) {
		free(ent.xent_dirname);
		return(ent.xent_options);
	}

	return(NULL);
}


/*
 *	get_mnt_opts()
 *	Checks if the file system exists in the /etc/fstab file and
 *	returns the filesystem mount options. 
 *	Returns NULL if filesystem is not found in /etc/fstab.
 */

char* 
get_mnt_opts(char* fs_name)
{
	FILE *fp;
	struct mntent *fstab_entry;
	char *mnt_opts=NULL;

	/* Check the parameter fs_name */
	if (fs_name == NULL)
	{
		mnt_opts = NULL;
	}
	/* Open the file system description file */
	else if ((fp = setmntent(FSTAB,"r")) != NULL)
	{
		while ((fstab_entry = getmntent(fp)) != NULL)
		{
			if ((fstab_entry->mnt_fsname!=NULL) &&
				(strcmp(fstab_entry->mnt_fsname,fs_name)==0))
			{
				if (fstab_entry->mnt_opts != NULL)
				{
					mnt_opts=strdup(fstab_entry->mnt_opts);
					ASSERT(mnt_opts!=NULL);
				}
				break;
			}
		}

		endmntent(fp);	
	}

	return(mnt_opts);
}

/*
 *	is_exported()
 *	Checks the file /etc/xtab to determine if the file system is
 *	currently exported.
 *	Returns 0 if the file system is exported, otherwise it returns 1.
 */

int
is_exported(char *fs_name, char *in_mntdir)
{
	struct exportent	exp_ent;
	struct mntent		mtab_ent;
	FILE			*fp;
	char			*mntdir = NULL;
	int			returnValue = 1;
	int			got_mntent = 1;
	
	/* Check the parameter, fs_name */
	if (fs_name == NULL) {
		return(1);
	}

	if(in_mntdir == NULL) {
		if((got_mntent = xfs_getmntent(MTAB, fs_name, &mtab_ent))
					!= 0 || mtab_ent.mnt_dir == NULL) {
			return(1);
		}
		mntdir = mtab_ent.mnt_dir;
	} else {
		mntdir = in_mntdir;
	}

	if(xfs_get_exportent(NULL, mntdir, &exp_ent) == 0 &&
	   exp_ent.xent_dirname != NULL) {
		free(exp_ent.xent_dirname);
		free(exp_ent.xent_options);
		returnValue = 0;
	}

	if(got_mntent == 0) {
		free(mtab_ent.mnt_fsname);
		free(mtab_ent.mnt_dir);
		free(mtab_ent.mnt_type);
		free(mtab_ent.mnt_opts);
	}

	return(returnValue);
}

/*
 *	add_fstab_entry()
 *	Adds the file system to /etc/fstab if the entry does not 
 *	already exist.
 *	Returns 0 if successful, otherwise it returns 1.
 */

int
add_fstab_entry(struct mntent *new_fs_entry,char** msg)
{
	struct 	mntent *fstab_entry;
	struct 	stat sb;
	FILE 	*fp;
	int 	returnValue = 0;
	short 	found = 0;
	char	str[BUFSIZ];

	/* Check the parameters */
	if ((new_fs_entry==NULL) || (msg==NULL) || 
		(new_fs_entry->mnt_type==NULL) || 
		(new_fs_entry->mnt_dir==NULL) || 
		(new_fs_entry->mnt_opts==NULL) || 
		(new_fs_entry->mnt_fsname==NULL))
	{
#ifdef DEBUG
printf ("add_vfstab error\n");
#endif
		returnValue = 1;
	}
	/* First check if the file system name is valid */
	else if ((strcmp(new_fs_entry->mnt_type,NFS_FS_TYPE)!=0) &&
		(access(new_fs_entry->mnt_fsname,0) == -1))
	{
		sprintf(str,gettxt(":26","%s: File is inaccessible.\n%s\n"),
				new_fs_entry->mnt_fsname, strerror(errno)); 
		add_to_buffer(msg,str);
		returnValue = 1;
	}
	else if ((strcmp(new_fs_entry->mnt_type,NFS_FS_TYPE)!=0) &&
		(stat(new_fs_entry->mnt_fsname,&sb) == -1))
	{
		sprintf(str,gettxt(":27","%s: Invalid file.\n%s\n"),
				new_fs_entry->mnt_fsname, strerror(errno)); 
		add_to_buffer(msg,str);
		returnValue = 1;
	}
	/* It must be a block special file in case of xfs/efs filesystems */
	else if (((strcmp(new_fs_entry->mnt_type,EFS_FS_TYPE)==0) ||
		(strcmp(new_fs_entry->mnt_type,XFS_FS_TYPE)==0)) &&
		((sb.st_mode & S_IFMT) != S_IFBLK))
	{
		sprintf(str,gettxt(":28",
				"%s: must be block special file.\n"),
				new_fs_entry->mnt_fsname); 
		add_to_buffer(msg,str);
		returnValue = 1;
	}
	/* Open the file system description file */
	else if ((fp = setmntent(FSTAB,"r+")) == NULL)
	{
		sprintf(str,gettxt(":29",
				"%s: Unable to open file.\n%s\n"),
				EXTAB);
		add_to_buffer(msg,str);
		returnValue = 1;
	}
	else
	{
		/* Read the entries in the file, comparing 
		   to the new_fs_entry file system name */

		while ((fstab_entry = getmntent(fp)) != NULL)
		{
			if ((fstab_entry->mnt_fsname!=NULL) &&
				(fstab_entry->mnt_dir!=NULL) &&
				(strcmp(fstab_entry->mnt_fsname,
				new_fs_entry->mnt_fsname)==0) &&
			   (strcmp(fstab_entry->mnt_dir,
				new_fs_entry->mnt_dir)==0))
			{
				found = 1;
				break;
			}
		}

		if (found == 0)
		{
			if (addmntent(fp,new_fs_entry) != 0)
			{
				sprintf(str,gettxt(":30",
					"%s: Unable to add to %s.\n"),
					new_fs_entry->mnt_fsname, FSTAB);
				add_to_buffer(msg,str);
				returnValue = 1;
			}
		}

		/* Close the file */
		endmntent(fp);
	}
	return(returnValue);
}

/*
 *	mount_fs()
 *	Mounts the file system, if it is not already mounted.
 *	Returns 0 if successful, otherwise it returns 1.
 */

int
mount_fs(char* fsname,char* mnt_dir,char* mnt_options,char** msg)
{
	char cmdbuf[POPEN_CMD_BUF_LEN];
	int returnValue = 0;
	FILE *pfd;
	char str[BUFSIZ];
	char errbuf[BUFSIZ];
	char *output = NULL;
	struct stat mnt_dir_stat;

	/* Check the parameters */
	if ((fsname == NULL) || (mnt_dir == NULL) || (msg == NULL))
	{
		sprintf(str,gettxt(":56",
			"Invalid parameters passed to mount_fs()\n"));
		add_to_buffer(msg,str);
		returnValue = 1;
	}
	else if (isvalidmntpt(mnt_dir) == 1)
	{
		sprintf(str,gettxt(":57",
			"%s: Invalid mount point for filesystem %s.\n"),
			mnt_dir,fsname);
		add_to_buffer(msg,str);
		returnValue = 1;
	}
	/* Save the permissions of the mount directory */
	else if (stat(mnt_dir,&mnt_dir_stat) == -1)
	{
		sprintf(str,gettxt(":73",
			"%s: Unable to get status of mount point.\n%s\n"),
				mnt_dir, strerror(errno));
		add_to_buffer(msg,str);
		returnValue = 1;
	}
	else
	{
		/* Check if mount options indicated and generate appropriate
		   mount command */
		if (mnt_options != NULL)
		{
			sprintf(cmdbuf,"/etc/mount -o %s %s %s 2>&1", 
					mnt_options, fsname, mnt_dir);
		}
		else
		{
			sprintf(cmdbuf,"/etc/mount %s %s 2>&1", fsname,mnt_dir);
		}

#ifdef DEBUG
		printf("DEBUG: mount command %s\n",cmdbuf);
#endif

		pfd = xfs_popen(cmdbuf, "r", 0);
		while(fgets(errbuf, BUFSIZ, pfd) != NULL) {
			add_to_buffer(&output, errbuf);
		}

		if (returnValue=xfs_pclose(pfd)) 
		{
			sprintf(str, gettxt(":31",
				"%s: Error executing /etc/mount:\n%s\n"),
					fsname, output);
			add_to_buffer(msg,str);
			returnValue = 1;
		}
		else if (chmod(mnt_dir,mnt_dir_stat.st_mode)!= 0)
		{
			/* Restore the permissions of the mount directory */
			sprintf(str,gettxt(":74",
			"%s: Unable to change permissions of mount point.\n"),
					mnt_dir);
			add_to_buffer(msg,str);
			/* TODO: this is done on purpose - not fatal */
			returnValue = 0;
		}
		if(output != NULL) {
			free(output);
		}
	}

	return(returnValue);
}


/*
 *	unmount_fs()
 *	Unmounts the file system. if the force flag is XFS_FORCE_UMOUNT_FS,
 *	the filesystem is forcefully unmounted killing processes that are
 *	accessing the device.
 *	Returns 0 if successful, otherwise it returns 1.
 */

int
unmount_fs(char* mnt_dir,int force_flag,char** msg)
{
	char cmdbuf[POPEN_CMD_BUF_LEN];
	int returnValue = 0;
	FILE *pfd;
	char str[BUFSIZ];
	char errbuf[BUFSIZ];
	char *output = NULL;
	struct stat mnt_dir_stat;

#ifdef DEBUG
	printf("DEBUG: unmount_fs: %s, %d\n", mnt_dir, force_flag);
#endif

	if ((mnt_dir == NULL) || (msg == NULL))
	{
		sprintf(str,gettxt(":58",
			"Invalid parameters passed to unmount_fs()\n"));
		add_to_buffer(msg,str);
		return(1);
	}

	if (isvalidmntpt(mnt_dir) == 1)
	{
		sprintf(str,gettxt(":36",
			"%s is an invalid mount point.\n"),mnt_dir);
		add_to_buffer(msg,str);
		return(1);
	}

	/* Save the permissions of the mount directory */
	if (stat(mnt_dir,&mnt_dir_stat) == -1)
	{
		sprintf(str,gettxt(":73",
			"%s: Unable to get status of mount point.\n%s\n"),
				mnt_dir, strerror(errno));
		add_to_buffer(msg,str);
		return(1);
	}

	/* Check if the filesystem is to be forcefully unmounted */
	if (force_flag == XFS_FORCE_UMOUNT_FS) {
		sprintf(cmdbuf,"/etc/umount -k %s 2>&1", mnt_dir);
	}
	else {
		sprintf(cmdbuf,"/etc/umount %s 2>&1", mnt_dir);
	}

#ifdef DEBUG
	printf("DEBUG: mount command %s\n",cmdbuf);
#endif

	pfd = xfs_popen(cmdbuf, "r", 0);
	while(fgets(errbuf, BUFSIZ, pfd) != NULL) {
		add_to_buffer(&output, errbuf);
	}

	if (returnValue = xfs_pclose(pfd)) {
		sprintf(str, gettxt(":32",
				"%s: Error executing /etc/umount:\n%s\n"),
				mnt_dir, output);
		add_to_buffer(msg, str);
		returnValue = 1;
	}

	if(output != NULL) {
		free(output);
	}

	/* Restore the permissions of the mount directory */
	if (chmod(mnt_dir, mnt_dir_stat.st_mode) != 0) {
		sprintf(str,gettxt(":74",
			"%s: Unable to change permissions of mount point.\n"),
			mnt_dir);
		add_to_buffer(msg, str);
		returnValue = 1;
	}

	return(returnValue);
}

/*
 *	add_export_entry()
 *	Adds the file system to /etc/exports if the entry does not 
 *	already exist. This enables the file system to be accessed
 *	from other hosts.
 *	Returns 0 if successful, otherwise it returns 1.
 */

int
add_export_entry(struct exportent *new_export_entry, char** msg)
{
	FILE *fp;
	struct exportent *export_entry;
	int returnValue = 0;
	short found = 0;
	char str[BUFSIZ];

	/* Check the parameters */
	if ((new_export_entry == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":59",
			"Invalid parameters passed to add_export_entry()\n"));
		add_to_buffer(msg, str);
		return(1);
	}

	/* Open the export information file */
	if ((fp = fopen(EXTAB, "r+")) == NULL) {
		sprintf(str,gettxt(":29", 
				"%s: Unable to open file.\n%s\n"), EXTAB);
		add_to_buffer(msg, str);
		return(1);
	}

	/* Read the entries in the file, comparing 
	   to the new_export_entry file system name */
	
	while ((export_entry = getexportent(fp)) != NULL) {
		if ((export_entry->xent_dirname != NULL) &&
		    (new_export_entry->xent_dirname != NULL) &&
		    (strcmp(export_entry->xent_dirname,
			    new_export_entry->xent_dirname) == 0)) {
			found = 1;
			break;
		}
	}

	if (found == 0) {
		if (addexportent(fp,new_export_entry->xent_dirname,
				new_export_entry->xent_options) != 0) {
			sprintf(str, gettxt(":30",
				"%s: Unable to add to %s.\n"),
				new_export_entry->xent_dirname, EXTAB);
			add_to_buffer(msg, str);
			returnValue = 1;
		}
	}

	/* Close the file */
	endexportent(fp);

	return(returnValue);
}

		
/*
 *	export_fs()
 *	The export options for the filesystem are set in the file /etc/exports.
 *	If the flag is 1, it is exported. 
 *	Returns 0 if successful, otherwise it returns 1.
 */
int
export_fs(char *fsname, boolean_t update_exports, char *options,char **msg)
{
	char	cmdbuf[POPEN_CMD_BUF_LEN];
	char	str[BUFSIZ];
	char	errbuf[BUFSIZ];
	char	*output = NULL;
	int	returnValue = 0;
	FILE	*pfd;

	/* Check the parameters */
	if(fsname == NULL || msg == NULL) {
		sprintf(str, gettxt(":61",
			"Invalid parameters passed to export_fs()\n"));
		add_to_buffer(msg, str);
		returnValue = 1;
	}
	else {
		/* Construct the command checking for export options */
		if (options != NULL) {
			sprintf(cmdbuf, "/usr/etc/exportfs -i -o %s %s 2>&1",
							options, fsname);
		}
		else {
			sprintf(cmdbuf, "/usr/etc/exportfs %s 2>&1", fsname);
		}

		pfd = xfs_popen(cmdbuf, "r", 0);
		while(fgets(errbuf, BUFSIZ, pfd) != NULL) {
			add_to_buffer(&output, errbuf);
		}

		if(returnValue = xfs_pclose(pfd)) {
			sprintf(str, gettxt(":41",
				"%s: Error executing /etc/exportfs:\n%s\n"),
				fsname, output);
			add_to_buffer(msg, str);
			returnValue = 1;
		}

		if(output != NULL) {
			free(output);
		}
	}
		
	return(returnValue);
}

/*
 *	unexport_fs()
 *	If the file system is not mounted, then it is an error to
 *	perform unexport_fs().
 *	Returns 0 if successful, otherwise it returns 1.
 */

int
unexport_fs(char *fsname, char **msg)
{
	char	cmdbuf[POPEN_CMD_BUF_LEN];
	char	errbuf[BUFSIZ];
	char	*output = NULL;
	char	str[BUFSIZ];
	int	returnValue = 0;
	FILE	*pfd;

	if(fsname == NULL || msg == NULL) {
		sprintf(str, gettxt(":62",
			"Invalid parameters passed to unexport_fs()\n"));
		add_to_buffer(msg, str);
		returnValue = 1;
	}
	else {
		sprintf(cmdbuf, "/usr/etc/exportfs -u %s 2>&1", fsname);
		pfd = xfs_popen(cmdbuf, "r", 0);
		while(fgets(errbuf, BUFSIZ, pfd) != NULL) {
			add_to_buffer(&output, errbuf);
		}

		if(returnValue = xfs_pclose(pfd)) {
			sprintf(str, gettxt(":41",
				"%s: Error executing /etc/exportfs:\n%s\n"),
				fsname, output);
			add_to_buffer(msg, str);
			returnValue = 1;
		}
		if(output != NULL) {
			free(output);
		}
	}

	return(returnValue);
}


/*
 *	xfs_export()
 *	Exports the specified file system. The filesystem must be mounted.
 *	Return Value is 0 on success, 1 otherwise.
 */

int
xfs_export(char *host,
	   char *fsname,
	   char *mnt_dir,
	   boolean_t update_exports,
	   char *export_options,
	   char **msg)
{
	struct exportent	new_ent;
	struct exportent	old_ent;
	int			returnValue = 0;
	char			str[BUFSIZ];

	/* Check the parameters */
	if ((mnt_dir == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":75",
			"Invalid parameters passed to xfs_export()\n"));
		add_to_buffer(msg, str);
		return(1);
	}

	/* Check if the mnt_dir is a valid mount point */
	if (isvalidmntpt(mnt_dir) != 0) {
		sprintf(str, gettxt(":36",
			"%s is an invalid mount point.\n"), mnt_dir);
		add_to_buffer(msg, str);
		return(1);
	}

	/* Check if the export options are set in
	   /etc/exports is they are not indicated */
	if(export_options == NULL &&
	   xfs_get_exportent(EXTAB, mnt_dir, NULL) != 0) {
		sprintf(str,gettxt(":40",
			"Export options not set for %s.\n"), mnt_dir);
		add_to_buffer(msg,str);
		return(1);
	}

	if (export_options != NULL && update_exports == B_TRUE) {

		old_ent.xent_dirname = mnt_dir;
		xfs_replace_exportent(&old_ent, NULL);

		new_ent.xent_dirname = mnt_dir; 
		new_ent.xent_options = export_options;
		returnValue = xfs_replace_exportent(NULL, &new_ent);
	}

	if (returnValue == 0) {
		returnValue = export_fs(mnt_dir, update_exports,
				export_options, msg);
	}

	return(returnValue);
}

/*
 *	xfs_unexport(host,fs,delete_flag)
 *	Unexports the file system. The entry for the filesystem in /etc/exports
 *	is removed, if the delete flag is set. 
 *	Return Value is 0 on success, 1 otherwise.
 */

int
xfs_unexport(char *host, char *fsname, int delete_flag, char **msg)
{
	struct exportent	exp_ent;
	int			returnValue = 0;
	char			str[BUFSIZ];
	char			*mnt_dir;

	/* Check the parameters */
	if ((fsname == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":76",
			"Invalid parameters passed to xfs_unexport()\n"));
		add_to_buffer(msg, str);
		return(1);
	}

	if((mnt_dir = get_mnt_dir(MTAB, fsname)) == NULL) {
		if((mnt_dir = get_mnt_dir(FSTAB, fsname)) == NULL) {
			sprintf(str, gettxt(":51",
				"%s: Unable to determine mount point for unexporting this filesystem.\n"), fsname);
			add_to_buffer(msg, str);
			returnValue = 2;
		}
	}
	if(is_exported(mnt_dir, mnt_dir) != 0) {
		sprintf(str,gettxt(":50",
			"%s: Filesystem is not exported.\n"), fsname);
		add_to_buffer(msg, str);
		returnValue = 2;
	}
	else {
		returnValue = unexport_fs(mnt_dir, msg);
	}

	if(IS_DEL_EXPORT_FLAG(delete_flag)) {
		/* Delete the filesystem entry in /etc/exports */
		if(xfs_get_exportent(EXTAB, mnt_dir, &exp_ent) == 0 &&
		   exp_ent.xent_dirname != NULL) {
			if(xfs_replace_exportent(&exp_ent, NULL) != 0) {
				/*
				 *	TODO: error msg
				 */
				returnValue = 1;
			}
			free(exp_ent.xent_dirname);
			free(exp_ent.xent_options);
		}
	}

	free(mnt_dir);

	return(returnValue);
}


/*
 *	xfs_mount()
 *	Mounts the file system. If the export flag, EXPORT_FS is set 
 *	the file system is exported for access by other hosts.
 *	Return Value is 0 on success, 1 otherwise.
 */

int
xfs_mount(char *host,
	  char *fsname,
	  char *mnt_dir,
	  char *mnt_options,
	  char *export_options,
	  int flag,
	  char **msg)
{
	struct exportent	nexp_ent;
	struct exportent	oexp_ent;
	char			str[BUFSIZ];
	int			returnValue = 0;
	int			rv;

	/* Check the parameters */
	if ((fsname == NULL) || (mnt_dir == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":63",
			"Invalid parameters passed to xfs_mount()\n"));
		add_to_buffer(msg, str);
		return(1);
	}

	/* Check if the mnt_dir is a valid mount point */
	if (isvalidmntpt(mnt_dir) != 0) {
		sprintf(str, gettxt(":36",
			"%s is an invalid mount point.\n"), mnt_dir);
		add_to_buffer(msg, str);
		return(1);
	}

	/* Check if filesystem is already mounted */
	if (xfs_getmntent(MTAB, fsname, NULL) == 0) {
		sprintf(str, gettxt(":52",
			"%s: Filesystem is currently mounted.\n"), fsname);
		add_to_buffer(msg, str);
		returnValue = 1;
	}
	else {
		returnValue = mount_fs(fsname, mnt_dir, mnt_options, msg);
	}

	/* Export the filesystem if the flag is set */
	if ((returnValue == 0) && (IS_EXPORT_FLAG(flag))) {

		/* Check if the export options are set in
		   /etc/exports if they are not indicated */
		if ((export_options == NULL) &&
			 (xfs_get_exportent(EXTAB, mnt_dir, NULL) != 0)) {
			sprintf(str, gettxt(":40",
				"Export options not set for %s.\n"),
						fsname);
			add_to_buffer(msg, str);
			returnValue = 1;
		}
		else {
			 /* The export options need to be 
			    updated in /etc/exports to take effect */
			if (export_options != NULL) {

				oexp_ent.xent_dirname = mnt_dir;
				xfs_replace_exportent(&oexp_ent, NULL);

				nexp_ent.xent_dirname = mnt_dir; 
				nexp_ent.xent_options = export_options;
				if(xfs_replace_exportent(NULL, &nexp_ent) != 0){
					returnValue = 1;
				}
			}

			if (returnValue == 0) {
				returnValue = export_fs(mnt_dir, B_TRUE,
						export_options, msg);
			}
		}
	}

	return(returnValue);
}

/*
 *	xfs_umount(host,fs,flag,force_flag)
 *	Unmounts the file system depending on the flag value. If the
 *	filesystem is exported, the file system is unexported.
 *	The force_flag value XFS_FORCE_UMOUNT_FS causes the filesystem to 
 *	be forcefully unmounted. 
 *	Return Value is 0 on success, 1 otherwise.
 */

int
xfs_umount(char *host, char *fsname, int flag, int force_flag, char **msg)
{
	struct mntent	mtab_ent;
	int		returnValue = 0;
	char		str[BUFSIZ];

	/* Check the parameters */
	if ((fsname == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":64",
			"Invalid parameters passed to xfs_umount()\n"));
		add_to_buffer(msg, str);
		return(1);
	}

	/* This is a local filesystem */
	/* Get the mount directory from /etc/mtab */
	if(xfs_getmntent(MTAB, fsname, &mtab_ent) != 0) {
		sprintf(str, gettxt(":51",
			"%s: Filesystem is not mounted.\n"), fsname);
		add_to_buffer(msg, str);
		return(1);
	}

	if(is_exported(mtab_ent.mnt_fsname, mtab_ent.mnt_dir) == 0) {
		returnValue = unexport_fs(mtab_ent.mnt_dir, msg);
	}

	if(returnValue == 0) {
		returnValue = unmount_fs(mtab_ent.mnt_dir, force_flag, msg);
	}

	free(mtab_ent.mnt_fsname);
	free(mtab_ent.mnt_dir);
	free(mtab_ent.mnt_type);
	free(mtab_ent.mnt_opts);

	return(returnValue);
}

/*
 *	xfs_fs_create()
 *	Creates a new file system from the fstab_entry supplied. The
 *	file system is mounted & exported depending on the flag value.
 *	Return Value is 0 on success, 1 otherwise.
 */  

int
xfs_fs_create(char *host,
	      struct mntent *fstab_entry,
	      char *efs_options,
	      char *xfs_options,
	      char *export_options,
	      int flag,
	      char **msg) 
{
	struct exportent	oexp_ent;
	struct exportent	nexp_ent;
	struct mntent		old_fstab_entry;
	int			has_fstab_entry,
				has_export_entry;
	char			cmdbuf[POPEN_CMD_BUF_LEN];
	FILE			*pfd;
	char			str[BUFSIZ + 1024];
	char			errbuf[BUFSIZ];
	char			*output = NULL;
	int			delete_flag = 0;
	int			returnValue = 0;

	/* Check the parameters */
	if(fstab_entry == NULL || msg == NULL ||
	   fstab_entry->mnt_fsname == NULL ||
	   fstab_entry->mnt_type == NULL ||
	   *(fstab_entry->mnt_fsname) == '\0' ||
	   *(fstab_entry->mnt_type) == '\0') {
		sprintf(str, gettxt(":64",
			"Invalid parameters passed to xfs_fs_create()\n"));
		add_to_buffer(msg, str);
		return(1);
	}

	/* Check for efs/xfs filesystem */
	if (strcmp(fstab_entry->mnt_type, EFS_FS_TYPE) == 0) {
		sprintf(cmdbuf, "%s %s 2>&1", XFS_MKFS_EFS_CMD,
					efs_options);
	}
	else if (strcmp(fstab_entry->mnt_type, XFS_FS_TYPE) == 0) {
		sprintf(cmdbuf, "%s %s 2>&1", XFS_MKFS_XFS_CMD,
					xfs_options);
	}
	else {
		sprintf(str, gettxt(":24",
			"%s: Unknown filesystem type.\n"),
			fstab_entry->mnt_type);
		add_to_buffer(msg, str);
		return(1);
	}

	/* Does the filesystem already exist ? */
	if(xfs_getmntent(MTAB, fstab_entry->mnt_fsname, NULL) == 0) {
		sprintf(str, gettxt(":54",
			"%s: Already part of a mounted filesystem.\n"),
			fstab_entry->mnt_fsname);
		add_to_buffer(msg, str);
		return(1);
	}

	/* If fstab entry already exists, save it */
	if((has_fstab_entry = xfs_getmntent(FSTAB, fstab_entry->mnt_fsname,
					&old_fstab_entry)) == 0) {

		/* Replace the old fstab entry with the new one */
		if(xfs_replace_mntent(FSTAB, &old_fstab_entry,
						fstab_entry) != 0) {
			returnValue = 1;
		}
	} else {
		/* Add the new fstab entry */
		if(xfs_replace_mntent(FSTAB, NULL, fstab_entry) != 0) {
			returnValue = 1;
		}
	}

	if (returnValue == 0) {
		delete_flag |= DEL_FSTAB_ENTRY_FLAG;
	}
	else {
		sprintf(str, gettxt(":30",
				"%s: Unable to add to %s.\n"),
				fstab_entry->mnt_fsname, FSTAB);
		add_to_buffer(msg, str);
		return(1);
	}

	/* Create the export entry in /etc/exports, if they are indicated */
	if (export_options != NULL) {
		nexp_ent.xent_dirname = fstab_entry->mnt_dir;
		nexp_ent.xent_options = export_options;

		if((has_export_entry = xfs_get_exportent(EXTAB,
				fstab_entry->mnt_dir, &oexp_ent)) == 0) {

			/* Replace the old exports entry with the new one */
			if(xfs_replace_exportent(&oexp_ent, &nexp_ent) != 0) {
				returnValue = 1;
			}
		}
		else {
			/* Add the new exports entry */
			if(xfs_replace_exportent(NULL, &nexp_ent) != 0) {
				returnValue = 1;
			}
		}

		if(returnValue == 0) {
			delete_flag |= DEL_EXPORT_ENTRY_FLAG;
		}
	}

	if (returnValue == 0) {
		pfd = xfs_popen(cmdbuf, "r", 0);
		while(fgets(errbuf, BUFSIZ, pfd) != NULL) {
			add_to_buffer(&output, errbuf);
		}
		if (returnValue = xfs_pclose(pfd)) {
			sprintf(str, gettxt(":34",
				"%s: Error executing %s:\n%s\n"),
					fstab_entry->mnt_fsname,
					XFS_MKFS_XFS_CMD,
					output);
			add_to_buffer(msg, str);
			returnValue = 1;
		}
		else {
			/* A silent fsck operation is performed due to
			 * inconsistentcy in the inode reference count
			 * computed by mkfs and fsck in efs. This 
			 * is a temporary fix
			 */
			if (strcmp(fstab_entry->mnt_type, EFS_FS_TYPE) == 0) {
				sprintf(cmdbuf, "/etc/fsck -q %s 2>&1",
						fstab_entry->mnt_fsname);
				if(output != NULL) {
					free(output);
					output = NULL;
				}
				pfd = xfs_popen(cmdbuf, "r", 0);
				while(fgets(errbuf, BUFSIZ, pfd) != NULL) {
					add_to_buffer(&output, errbuf);
				}
				if (returnValue = xfs_pclose(pfd)) {
					sprintf(str, gettxt(":35",
					"%s: Error executing /etc/fsck:\n%s\n"),
						fstab_entry->mnt_fsname,output);
					add_to_buffer(msg, str);
					returnValue = 1;
				}
			}

			if ((returnValue == 0) && (flag != 0)) {
				returnValue = xfs_mount(NULL,
						fstab_entry->mnt_fsname,
						fstab_entry->mnt_dir,
						fstab_entry->mnt_opts,
						export_options,
						flag, msg);
			}
		}
		if(output != NULL) {
			free(output);
		}
	}

	if(returnValue != 0) {
		/*
		 *	if exported unexport it
		 */
		if(IS_EXPORT_FLAG(flag) &&
		   (is_exported(fstab_entry->mnt_fsname,
				fstab_entry->mnt_dir) == 0)) {
			unexport_fs(fstab_entry->mnt_dir, msg);
		}

		/*
		 *	if mounted unmount it
		 */
		if(IS_MOUNT_FLAG(flag) &&
		   (xfs_getmntent(MTAB, fstab_entry->mnt_fsname, NULL) == 0)) {
			unmount_fs(fstab_entry->mnt_dir, 
						XFS_FORCE_UMOUNT_FS, msg);
		}

		if(IS_DEL_FSTAB_FLAG(delete_flag)) {
			/* Cleanup by removing the fstab entry */
			if(has_fstab_entry == 0) {
				if(xfs_replace_mntent(FSTAB, fstab_entry,
						&old_fstab_entry) != 0) {
				}
			} else {
				if(xfs_replace_mntent(FSTAB, fstab_entry, NULL)
								!= 0) {
				}
			}
		}

		if(IS_DEL_EXPORT_FLAG(delete_flag)) {
			/* Cleanup by removing the exports entry */
			if(has_export_entry == 0) {
				if(xfs_replace_exportent(&nexp_ent,
						&oexp_ent) != 0) {
				}
			} else {
				if(xfs_replace_exportent(&nexp_ent,
						NULL) != 0) {
				}
			}
		}
	}

	if(has_fstab_entry == 0) {
		free(old_fstab_entry.mnt_fsname);
		free(old_fstab_entry.mnt_dir);
		free(old_fstab_entry.mnt_type);
		free(old_fstab_entry.mnt_opts);
	}
	if(has_export_entry == 0) {
		free(oexp_ent.xent_dirname);
		free(oexp_ent.xent_options);
	}

	return(returnValue);
}


/*
 *	xfs_fs_delete()
 *	Removes the file system. The filesystem is unexported and unmounted
 *	depending on its state. Based on the delete_flag value, the entries
 *	in /etc/fstab and /etc/exports are removed.
 *	The possible value for delete_flag are DEL_FSTAB_ENTRY_FLAG and
 *	DEL_EXPORTS_ENTRY_FLAG.
 *	The force_flag value XFS_FORCE_UMOUNT_FS causes the filesystem to 
 *	be forcefully unmounted. 
 *	Return Value is 0 on success, 1 otherwise.
 */  

int
xfs_fs_delete(char *host,
	      char *fsname,
	      int delete_flag,
	      int force_flag,
	      char **msg)
{
	int			returnValue = 0;
	char			str[BUFSIZ];
	char			*mnt_dir = NULL;
	struct	mntent		mtab_ent,
				fstab_ent;
	struct exportent	xtab_ent,
				exp_ent;
	int			has_mtab_ent = 1,
				has_fstab_ent = 1,
				has_xtab_ent = 1,
				has_exp_ent = 1;
	boolean_t		warning = B_FALSE;

	if(fsname == NULL || msg == NULL) {
		sprintf(str, gettxt(":65",
			"Invalid parameters passed to xfs_fs_delete()\n"));
		add_to_buffer(msg, str);
		return(1);
	}

	if((has_mtab_ent = xfs_getmntent(MTAB, fsname, &mtab_ent)) == 0) {
		mnt_dir = mtab_ent.mnt_dir;
	}
	if((has_fstab_ent = xfs_getmntent(FSTAB, fsname, &fstab_ent)) == 0) {
		if(has_mtab_ent != 0) {
			mnt_dir = fstab_ent.mnt_dir;
		}
	}

	if(has_mtab_ent != 0 && has_fstab_ent != 0) {
		sprintf(str,gettxt(":169",
				"%s: No /etc/fstab or /etc/mtab entry.\n"),
				fsname);
		add_to_buffer(msg, str);
		return(1);
	}

	/*
	 *	If there's an entry in /etc/xtab, unexport it
	 */
	if((has_xtab_ent = xfs_get_exportent(NULL, mnt_dir, &xtab_ent)) == 0) {
		if(unexport_fs(mnt_dir, msg) != 0) {
			warning = B_TRUE;
		}
	}

	/*
	 *	If there's an entry in /etc/mtab, unmount the file system
	 */
	if(has_mtab_ent == 0) {
		returnValue = unmount_fs(mtab_ent.mnt_dir, force_flag, msg);
	}

	/*
	 *	If there's an entry in /etc/fstab, remove it
	 */
	if(returnValue == 0 &&
	   has_fstab_ent == 0 &&
	   IS_DEL_FSTAB_FLAG(delete_flag)) {
		if(xfs_replace_mntent(FSTAB, &fstab_ent, NULL) != 0) {
			warning = B_TRUE;
			sprintf(str, gettxt(":37",
					"%s: Unable to remove from %s.\n"),
					fstab_ent.mnt_fsname, FSTAB);
			add_to_buffer(msg, str);
		}
	}

	/*
	 *	If there's an entry in /etc/exports, remove it
	 */
	if (returnValue == 0 && IS_DEL_EXPORT_FLAG(delete_flag)) {
		if(xfs_get_exportent(EXTAB, mnt_dir, &exp_ent) == 0) {
			if(xfs_replace_exportent(&exp_ent, NULL) != 0) {
				warning = B_TRUE;
				sprintf(str, gettxt(":37",
					"%s: Unable to remove from %s.\n"),
					exp_ent.xent_dirname, EXTAB);
				add_to_buffer(msg, str);
			}
			free(exp_ent.xent_dirname);
			free(exp_ent.xent_options);
		}
	}

	if(has_mtab_ent == 0) {
		free(mtab_ent.mnt_fsname);
		free(mtab_ent.mnt_dir);
		free(mtab_ent.mnt_type);
		free(mtab_ent.mnt_opts);
	}
	if(has_fstab_ent == 0) {
		free(fstab_ent.mnt_fsname);
		free(fstab_ent.mnt_dir);
		free(fstab_ent.mnt_type);
		free(fstab_ent.mnt_opts);
	}
	if(has_xtab_ent == 0) {
		free(xtab_ent.xent_dirname);
		free(xtab_ent.xent_options);
	}

	if(returnValue == 0 && warning == B_TRUE) {
		returnValue = 2;
	}
	return(returnValue);
}		

/*
 *	add_mount_info_to_fs_list()
 *	Checks the list to find the file system entry. The mount 
 *	information is stored and the mount flag is set.
 */

void
add_mount_info_to_fs_list(fs_info_entry_t **list,
			  struct mntent *mount_entry,
			  int type)
{
	fs_info_entry_t *elm = NULL;
	fs_info_entry_t *new_elm = NULL;
	struct mntent *mnt_info = NULL;
	int found = 0;

	/* Check the parameters */
	if ((list != NULL) && (mount_entry != NULL))
	{
		elm = *list;
		while (elm != NULL)
		{
			if ((elm->fs_entry != NULL) &&
				(elm->fs_entry->mnt_fsname != NULL) &&
				(mount_entry->mnt_fsname != NULL) &&
				(elm->fs_entry->mnt_dir != NULL) &&
				(mount_entry->mnt_dir != NULL) &&
				(strcmp(elm->fs_entry->mnt_fsname,
					 mount_entry->mnt_fsname)==0)) 
			{
				found = 1;
				break;
			}
			elm = elm->next;
		}

		/* Create the mount info structure */

		mnt_info= (struct mntent *)malloc(sizeof(struct mntent));
		ASSERT(mnt_info!=NULL);

		if (mount_entry->mnt_fsname != NULL)
		{
			mnt_info->mnt_fsname =
				(char*)strdup(mount_entry->mnt_fsname);
			ASSERT(mnt_info->mnt_fsname!=NULL);
		}
		else
		{
			mnt_info->mnt_fsname = NULL;
		}

		if (mount_entry->mnt_dir != NULL)
		{
			mnt_info->mnt_dir =
				(char*)strdup(mount_entry->mnt_dir);
			ASSERT(mnt_info->mnt_dir!=NULL);
		}
		else
		{
			mnt_info->mnt_dir = NULL;
		}

		if (mount_entry->mnt_type != NULL)
		{
			mnt_info->mnt_type =
				(char*)strdup(mount_entry->mnt_type);
			ASSERT(mnt_info->mnt_type!=NULL);
		}
		else
		{
			mnt_info->mnt_type = NULL;
		}

		if (mount_entry->mnt_opts != NULL)
		{
			mnt_info->mnt_opts =
				(char*)strdup(mount_entry->mnt_opts);
			ASSERT(mnt_info->mnt_opts!=NULL);
		}
		else
		{
			mnt_info->mnt_opts = NULL;
		}	

		mnt_info->mnt_freq = mount_entry->mnt_freq;
		mnt_info->mnt_passno = mount_entry->mnt_passno;


		if ((found == 1) && (elm->mount_entry == NULL))
		{
			elm->mount_entry = mnt_info;
			elm->info |= type;
		}
		else
		{
			/* Create a new entry for this mounted filesystem */	
			new_elm = (fs_info_entry_t*) malloc(
						sizeof(fs_info_entry_t));
			ASSERT(new_elm!=NULL);

			new_elm->fs_entry = NULL;
			new_elm->export_entry = NULL;
			new_elm->mount_entry = mnt_info;
			new_elm->info = type;
			new_elm->next = *list;
			*list = new_elm;
		}
	}
}	


/*
 *	add_export_info_to_fs_list()
 *	Checks the list to find the file system entry. The export 
 *	information is stored and the export flag is set.
 */

void
add_export_info_to_fs_list(fs_info_entry_t **list,
			   struct exportent *export_entry,
			   int type)
{
	fs_info_entry_t *elm = NULL;
	int found = 0;

	if ((list != NULL) && (export_entry != NULL))
	{
		/* Traverse the list to locate the filesystem */
		elm = *list;
		while (elm != NULL)
		{
			if ((elm->fs_entry != NULL) && 
				(strcmp(elm->fs_entry->mnt_dir,
				export_entry->xent_dirname)==0))
			{
				found = 1;
				break;
			}
			elm = elm->next;
		}

		if (found==1)
		{
			elm->info |= type;

			elm->export_entry = (struct exportent *)
					malloc(sizeof(struct exportent));
			ASSERT(elm->export_entry!=NULL);

			if (export_entry->xent_dirname != NULL)
			{
				elm->export_entry->xent_dirname =
				(char*)strdup(export_entry->xent_dirname);
				ASSERT(elm->export_entry->xent_dirname != NULL);
			}
			else
			{
				elm->export_entry->xent_dirname = NULL;
			}

			if (export_entry->xent_options != NULL)
			{
				elm->export_entry->xent_options =
				(char*)strdup(export_entry->xent_options);
				ASSERT(elm->export_entry->xent_options != NULL);
			}
			else
			{
				elm->export_entry->xent_options = NULL;
			}
		}
	}

}



/*
 *	add_to_fs_info_list()
 *	Checks the list to see if an entry exists to describe the file system.
 *	If it exists then the info field is updated or else a new entry is
 *	created in the list.
 */

void
add_to_fs_info_list(fs_info_entry_t **list,struct mntent *entry,int type)
{
	fs_info_entry_t *elm = NULL;
	int found = 0;

	if ((list != NULL) && (entry !=NULL))
	{
		/* Traverse the list to locate the filesystem */
		elm = *list;
		found = 0;
		while (elm != NULL)
		{
			if ((elm->fs_entry != NULL) &&
				(elm->fs_entry->mnt_fsname != NULL) &&
				(entry->mnt_fsname != NULL) &&
		    (strcmp(elm->fs_entry->mnt_fsname,entry->mnt_fsname)==0))
			{
				found = 1;
				break;
			}
			elm = elm->next;
		}

		if (found==1)
		{
			elm->info |= type;
		}
		else
		{
			elm = (fs_info_entry_t*)malloc(sizeof(fs_info_entry_t));
			ASSERT(elm!=NULL);

			elm->fs_entry = (struct mntent *)malloc(
							sizeof(struct mntent));
			ASSERT(elm->fs_entry!=NULL);

			if (entry->mnt_fsname != NULL)
			{
				elm->fs_entry->mnt_fsname =
					(char*)strdup(entry->mnt_fsname);
				ASSERT(elm->fs_entry->mnt_fsname!=NULL);
			}
			else
			{
				elm->fs_entry->mnt_fsname = NULL;
			}	

			if (entry->mnt_dir != NULL)
			{
				elm->fs_entry->mnt_dir =
					(char*)strdup(entry->mnt_dir);
				ASSERT(elm->fs_entry->mnt_dir!=NULL);
			}
			else
			{
				elm->fs_entry->mnt_dir = NULL;
			}	

			if (entry->mnt_type != NULL)
			{
				elm->fs_entry->mnt_type =
					(char*)strdup(entry->mnt_type);
				ASSERT(elm->fs_entry->mnt_type!=NULL);
			}
			else
			{
				elm->fs_entry->mnt_type = NULL;
			}	

			if (entry->mnt_opts != NULL)
			{
				elm->fs_entry->mnt_opts =
					(char*)strdup(entry->mnt_opts);
				ASSERT(elm->fs_entry->mnt_opts!=NULL);
			}
			else
			{
				elm->fs_entry->mnt_opts = NULL;
			}	

			elm->fs_entry->mnt_freq = entry->mnt_freq;
			elm->fs_entry->mnt_passno = entry->mnt_passno;

			elm->mount_entry = NULL;
			elm->export_entry = NULL;
			elm->info = type;
			elm->next = *list;
			*list = elm;
		}
	}
}

/*
 *	xfs_info()
 *	Gathers information about the state of diiferent file systems by
 *	reading files /etc/fstab and /etc/mtab. A list of fs_info_entry_t
 *	structures is returned.
 */

fs_info_entry_t*
xfs_info(char* host,char** msg)
{
	FILE *fp;
	struct mntent *fstab_entry;
	struct mntent *mtab_entry;
	struct exportent *export_entry;
	fs_info_entry_t *fs_info_list=NULL;

	if (msg != NULL)
	{
		fstab_entry = mtab_entry = NULL;

		/* Read the file system description file */
		if ((fp = setmntent(FSTAB,"r")) != NULL)
		{
			/* Read the entries in the file, adding them
		   	to the fs_info_list */
	
			while ((fstab_entry = getmntent(fp)) != NULL)
			{
				add_to_fs_info_list(&fs_info_list,
							fstab_entry,FS_FLAG);
			}

			/* Close the file */
			endmntent(fp);
		}

		/* Read the mounted filesystems in /etc/mtab file */
		if ((fp = setmntent(MTAB,"r")) != NULL)
		{
			/* Read the entries in the file, adding them
		   	to the fs_info_list */
	
			while ((mtab_entry = getmntent(fp)) != NULL)
			{
				add_mount_info_to_fs_list(&fs_info_list,
						mtab_entry,MNT_FLAG);
			}

			/* Close the file */
			endmntent(fp);
		}

		/* Read the exported filesystems in /etc/xtab file */
		if ((fp = fopen(XTAB,"r")) != NULL)
		{
			/* Read the entries in the file, adding them
		   	to the fs_info_list */
	
			while ((export_entry = getexportent(fp)) != NULL)
			{
				add_export_info_to_fs_list(&fs_info_list,
						export_entry,EXP_FLAG);
			}

			/* Close the file */
			endexportent(fp);
		}
	}

	return(fs_info_list);
}


/*
 *	xfs_fs_edit()
 *	Updates the file system entry in /etc/fstab. The export options
 *	for the filesystem are also updated, if they are specified. 
 *	These changes will take effect only when the file system is
 *	mounted in future. 
 *	Returns 0 if successful, otherwise it returns 1.
 */

int
xfs_fs_edit(char *host,
	    struct mntent *new_fstab_entry,
	    char *export_opts,
	    char **msg)
{ 
	struct exportent	nexp_ent;
	struct exportent	oexp_ent;
	struct mntent		ent;
	int			returnValue = 0;
	char			*mnt_dir;
	char			str[BUFSIZ];

	if ((new_fstab_entry == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":68",
			"Invalid parameters passed to xfs_fs_edit()\n"));
		add_to_buffer(msg, str);
		return(1);
	}

	if ((new_fstab_entry != NULL) && 
	    (xfs_getmntent(FSTAB, new_fstab_entry->mnt_fsname, &ent) != 0)) {
		sprintf(str, gettxt(":43",
				"%s: Filesystem does not exist.\n"),
				new_fstab_entry->mnt_fsname);
		add_to_buffer(msg, str);
		return(1);
	}

	if (isvalidmntpt(new_fstab_entry->mnt_dir) != 0) {
		sprintf(str, gettxt(":36",
				"%s is an invalid mount point.\n"),
				new_fstab_entry->mnt_dir);
		add_to_buffer(msg, str);
		returnValue = 1;
	}
	else {
		if (export_opts != NULL) {
			/* Get the mount point */
			if ((mnt_dir = get_mnt_dir(FSTAB,
					new_fstab_entry->mnt_fsname)) == NULL) {
				sprintf(str, gettxt(":33",
				    "%s: Unable to get the mount directory.\n"),
				    new_fstab_entry->mnt_fsname);
				add_to_buffer(msg, str);
				returnValue = 1;
			}
			else {
				oexp_ent.xent_dirname = mnt_dir;
				xfs_replace_exportent(&oexp_ent, NULL);

				nexp_ent.xent_dirname = new_fstab_entry->mnt_dir; 
				nexp_ent.xent_options = export_opts;
				if(xfs_replace_exportent(NULL, &nexp_ent) != 0){
					returnValue = 1;
				}

				free(mnt_dir);
			}
		}

		if(xfs_replace_mntent(FSTAB, &ent, new_fstab_entry) != 0) {
			returnValue = 1;
		}
	
	}
	
	free(ent.mnt_fsname);
	free(ent.mnt_dir);
	free(ent.mnt_type);
	free(ent.mnt_opts);

	return(returnValue);
}

/*
 *	delete_xfs_info_entry()
 *	Releases the memory allocated to store the structure 
 *	returned by the call to xfs_info().
 */

void
delete_xfs_info_entry(fs_info_entry_t* fs_info_entry)
{
	if (fs_info_entry != NULL)
	{
		if (fs_info_entry->fs_entry != NULL)
		{
			if (fs_info_entry->fs_entry->mnt_fsname != NULL)
				free(fs_info_entry->fs_entry->mnt_fsname);
			if (fs_info_entry->fs_entry->mnt_dir != NULL)
				free(fs_info_entry->fs_entry->mnt_dir);
			if (fs_info_entry->fs_entry->mnt_type != NULL)
				free(fs_info_entry->fs_entry->mnt_type);
			if (fs_info_entry->fs_entry->mnt_opts != NULL)
				free(fs_info_entry->fs_entry->mnt_opts);
			free(fs_info_entry->fs_entry);
		}

		if (fs_info_entry->mount_entry != NULL)
		{
			if (fs_info_entry->mount_entry->mnt_fsname != NULL)
				free(fs_info_entry->mount_entry->mnt_fsname);
			if (fs_info_entry->mount_entry->mnt_dir != NULL)
				free(fs_info_entry->mount_entry->mnt_dir);
			if (fs_info_entry->mount_entry->mnt_type != NULL)
				free(fs_info_entry->mount_entry->mnt_type);
			if (fs_info_entry->mount_entry->mnt_opts != NULL)
				free(fs_info_entry->mount_entry->mnt_opts);
			free(fs_info_entry->mount_entry);
		}

		if (fs_info_entry->export_entry != NULL)
		{
			if (fs_info_entry->export_entry->xent_dirname != NULL)
				free(fs_info_entry->export_entry->xent_dirname);
			if (fs_info_entry->export_entry->xent_options != NULL)
				free(fs_info_entry->export_entry->xent_options);
			free(fs_info_entry->export_entry);
		}

		free(fs_info_entry);
	}
}

