/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *	The frontend for the rpc calls to create/delete/edit filesystems.
 */

#ident "xfs_fs_funcs.c: $Revision: 1.6 $"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/debug.h>
#include <sys/stat.h>
#include <mntent.h>
#include "xfs_info_defs.h"
#include "xfs_fs_defs.h"
#include "xfs_utils.h"

#ifdef DEBUG
char debug_str[BUFSIZ];
#endif

/*
 *	create_default_values_str()
 *	Given the options table, this routine checks for the options which
 *	have default values and generates keyword value pairs for them. 
 *	If the default value matches the boolean values corresponding
 *	to the option, the values "true" and "false" are reported with the
 *	keyword.
 */

void
create_default_values_str(struct fs_option options_tab[],int options_tab_len,char** default_values_str)
{
	int 	index;
	char	str[BUFSIZ];

	/* Check the input parameters */
	if ((options_tab == NULL) || (default_values_str == NULL))
	{
		return;
	}

	/* Traverse the options table looking for default values */
	index = 0;
	while (index < options_tab_len)
	{
		/* Does the option have a default value ? */
		if (options_tab[index].default_opts_str!=NULL) 
		{
			/* Is it same as the "true" boolean condition value */ 
			if ((options_tab[index].true_bool_opts_str!= NULL) &&
				(strcmp(options_tab[index].true_bool_opts_str,
				options_tab[index].default_opts_str)==0))
			{
				sprintf(str,"%s:true\n",options_tab[index].str);
				add_to_buffer(default_values_str,str);
			}
			/* Is it same as the "false" boolean condition value */ 
			else if ((options_tab[index].false_bool_opts_str!= NULL)
		 	&& (strcmp(options_tab[index].false_bool_opts_str,
					options_tab[index].default_opts_str)==0))
			{
				sprintf(str,"%s:false\n",options_tab[index].str);
				add_to_buffer(default_values_str,str);
			}
			/* It is a regular value corresponding to the option */
			else
			{
				sprintf(str,"%s:%s\n",options_tab[index].str,
					options_tab[index].default_opts_str);
				add_to_buffer(default_values_str,str);
			}
		}
		index++;
	}
}

/*
 *	xfs_get_default_values()
 *	For the specified filesystem type, the keyword value pairs are
 *	generated for those options whose default values are indicated.
 *	The output is the string containing the default values. 
 *	On success 0 is returned. In case of an error a 1 is returned.
 */

int
xfs_get_default_values(char* fs_type,char** default_values_str)
{
	int returnValue = 0;

	/* Check the parameters */
	if ((fs_type != NULL) && (default_values_str != NULL))
	{
		/* Look for options specific to the filesystem */
		if (strcmp(fs_type,EFS_FS_TYPE)==0)
		{
			create_default_values_str(efs_fs_options,
							EFS_FS_OPTS_TAB_LEN,
							default_values_str);

		}
		else if (strcmp(fs_type,XFS_FS_TYPE)==0)
		{
			create_default_values_str(xfs_fs_options,
							XFS_FS_OPTS_TAB_LEN,
							default_values_str);
		}
		else if (strcmp(fs_type,ISO9660_FS_TYPE)==0)
		{
			create_default_values_str(iso9660_fs_options,
							ISO9660_FS_OPTS_TAB_LEN,
							default_values_str);
		}
		else if (strcmp(fs_type,NFS_FS_TYPE)==0) 
		{
			create_default_values_str(nfs_fs_options,
							NFS_FS_OPTS_TAB_LEN,
							default_values_str);
		}
		else if (strcmp(fs_type,SWAP_FS_TYPE)==0)
		{
			create_default_values_str(swap_fs_options,
							SWAP_FS_OPTS_TAB_LEN,
							default_values_str);
		}
		else if (strcmp(fs_type,CACHE_FS_TYPE)==0)
		{
			create_default_values_str(cache_fs_options,
							CACHE_FS_OPTS_TAB_LEN,
							default_values_str);
		}
	}
	else
	{
		returnValue = 1;
	}

	return(returnValue);
}

/*
 *	create_option_str()
 *	This function takes the keyword, option table and the option
 *	string as input. If the keyword is located in the option table,
 *	the option string is suitably appended with the information 
 *	about this option. All the information is located based on the value
 *	of the keyword from the option table.
 *	If the keyword is foudn in the table, a 0 is returned. Otherwise,
 *	a 1 is returned.
 */

int
create_option_str(char* keyword,char* value,struct fs_option options_tab[],int options_tab_len,char** options_str)
{
	char	str[BUFSIZ];
	int	index;
	int	found=1;

	/* Check the parameters */
	if ((keyword == NULL) || (value == NULL) || (options_tab == NULL) ||
		(options_str == NULL))
	{
		return(1);
	}

	index = 0;
	while (index < options_tab_len)
	{
		if ((options_tab[index].str!=NULL) &&
			(strcmp(keyword, options_tab[index].str)==0))
		{
			found = 0;
			/* option corresponding to "true" boolean condition */
			if ((strcmp(value,"TRUE")==0) || 
				(strcmp(value,"true")==0))
			{
				if (options_tab[index].true_bool_opts_str!=NULL)
				{
					sprintf(str,
					options_tab[index].true_bool_opts_str,
						value);
					strcat(str,",\0");
					add_to_buffer(options_str,str);
				}
				break;
			}
			/* Option corresponding to boolean value, "FALSE" */
			else if ((strcmp(value,"FALSE")==0) ||
					(strcmp(value,"false")==0))
			{
				if (options_tab[index].false_bool_opts_str!=NULL)
				{
					sprintf(str,
					options_tab[index].false_bool_opts_str,
						value);
					strcat(str,",\0");
					add_to_buffer(options_str,str);
				}
				break;
			}
			/* Use Regular options string */
			else 
			{
				if (options_tab[index].opts_str!=NULL)
				{
					sprintf(str,options_tab[index].opts_str,
						value);
					strcat(str,",\0");
					add_to_buffer(options_str,str);
				}
				break;
			}
		}
		index++;
	}
	return(found);
}

/*
 *	create_fs_options_str()
 *	Parses the input string to extract keyword value pairs which
 *	specify the filesystem options for the fileystem.
 *	A string with the export options specified in the format needed to 
 *	add it to /etc/exports is returned. In case of error, a NULL 
 *	is returned.
 */

char*
create_fs_options_str(char* fs_type,char* parameters)
{
	char	*line=NULL;
	char	*key=NULL;
	char	*value=NULL;	
	char	str[BUFSIZ];
	char	*fs_opts_str=NULL;
	char	*chr=NULL;
	int	index;
	char	*dup_parameters=NULL;
	int	found;

	/* Check the parameters */
	if ((fs_type == NULL) || (parameters == NULL))
	{
		return(NULL);
	}

	/* Parse the parameters string */
	dup_parameters = strdup(parameters);
	ASSERT(dup_parameters!=NULL);

	for(line = strtok(dup_parameters, "\n");
	    line != NULL;
	    line = strtok(NULL, "\n")) {

		/* Extract the keyword and value */
		if(xfsmGetKeyValue(line, &key, &value)) {

			/* Look for general options */
			found = create_option_str(key,value,generic_fs_options,
						GENERIC_FS_OPTS_TAB_LEN,
						&fs_opts_str);

			/* Look for options specific to the filesystem */
			if (found != 0)
			{
				if (strcmp(fs_type,EFS_FS_TYPE)==0)
				{
					create_option_str(key,value,
							efs_fs_options,
							EFS_FS_OPTS_TAB_LEN,
							&fs_opts_str);

				}
				else if (strcmp(fs_type,XFS_FS_TYPE)==0)
				{
					create_option_str(key,value,
							xfs_fs_options,
							XFS_FS_OPTS_TAB_LEN,
							&fs_opts_str);
				}
				else if (strcmp(fs_type,ISO9660_FS_TYPE)==0)
				{
					create_option_str(key,value,
							iso9660_fs_options,
							ISO9660_FS_OPTS_TAB_LEN,
							&fs_opts_str);
				}
				else if (strcmp(fs_type,NFS_FS_TYPE)==0) 
				{
					create_option_str(key,value,
							nfs_fs_options,
							NFS_FS_OPTS_TAB_LEN,
							&fs_opts_str);
				}
				else if (strcmp(fs_type,SWAP_FS_TYPE)==0)
				{
					create_option_str(key,value,
							swap_fs_options,
							SWAP_FS_OPTS_TAB_LEN,
							&fs_opts_str);
				}
				else if (strcmp(fs_type,CACHE_FS_TYPE)==0)
				{
					create_option_str(key,value,
							cache_fs_options,
							CACHE_FS_OPTS_TAB_LEN,
							&fs_opts_str);
				}
			}
		}
	}

	/* Remove the last "," in fs options string */
	if (fs_opts_str != NULL)
	{
		if ((chr = strrchr(fs_opts_str,',')) != NULL)
		{
			memset(chr,'\0',1);
		}
	}

	if (dup_parameters != NULL)
	{
		free(dup_parameters);
	}

	return(fs_opts_str);
}

/*
 *	create_export_options_str()
 *	Parses the input string to extract keyword value pairs which
 *	specify the export options for the fileystem.
 *	A string with the export options specified in the format needed to 
 *	add it to /etc/exports is returned. In case of error, a NULL 
 *	is returned.
 */

char*
create_export_options_str(char* parameters)
{
	char	*line=NULL;
	char	*key=NULL;
	char	*value=NULL;	
	char	*export_opts_str=NULL;
	char	*chr=NULL;
	char	*dup_parameters=NULL;

	if (parameters == NULL) {
		return(NULL);
	}

	/* Parse the parameters string */
	dup_parameters = strdup(parameters);
	ASSERT(dup_parameters!=NULL);

	for(line = strtok(dup_parameters, "\n");
	    line != NULL;
	    line = strtok(NULL, "\n")) {

		/* Extract the keyword and value */
		if(xfsmGetKeyValue(line, &key, &value)) {
			/* Look for options which are a really a combination of 
			   two or more options but with the same value */
			if (strcmp(key,XFS_EXPOPTS_RWROOT_STR)==0) {
				create_option_str(XFS_EXPOPTS_RW_STR,value,
						xfs_export_opts_tab,
						XFS_EXPORT_OPTS_TAB_LEN,
						&export_opts_str);

				create_option_str(XFS_EXPOPTS_ROOT_STR,value,
						xfs_export_opts_tab,
						XFS_EXPORT_OPTS_TAB_LEN,
						&export_opts_str);
			}
			else {		
				create_option_str(key,value,
						xfs_export_opts_tab,
						XFS_EXPORT_OPTS_TAB_LEN,
						&export_opts_str);
			}
		}
	}

	/* Add the default option "rw", if export options "ro" and "rw=host"
 	   are not specified */
	if ((export_opts_str != NULL) && 
		(strstr(export_opts_str,"ro,") == NULL) &&
		(strstr(export_opts_str,"rw=") == NULL)) {
		add_to_buffer(&export_opts_str,"rw,");
	}
	/* Add the option "ro", if export option "rw=host" is specified and 
	  "ro" is not already indicated. This is to enable other hosts 
	   read only access */
	else if ((export_opts_str != NULL) && 
		(strstr(export_opts_str,"rw=") != NULL) &&
		(strstr(export_opts_str,"ro,") == NULL)) {
		add_to_buffer(&export_opts_str,"ro,");
	}

	/* Remove the last "," in export options string */
	if (export_opts_str != NULL) {
		if ((chr = strrchr(export_opts_str,',')) != NULL) {
			memset(chr,'\0',1);
		}
	}

	if (dup_parameters != NULL) {
		free(dup_parameters);
	}

	return(export_opts_str);
}

/*
 *	create_device_name()
 *	Given the raw device name, the actual device name is constructed by 
 *	replacing /dev/rdsk in filesystem pathname with /dev/dsk. 
 *	Example: For filesystem name, the raw device name such as
 *	/dev/rdsk/ips0d3s7 is transformed to /dev/dsk/ips0d3s7.
 *	Return Value : Device name on success
 *			NULL on error
 */

char*
create_device_name(char* fsname)
{
	char* device_name=NULL;
	char *dup_fsname=NULL;
	char* token=NULL;
	struct stat stat_buf;


	/* Check if the fsname has a "/" */
	if (fsname == NULL)
	{
		device_name = NULL;
	}
	else if (strchr(fsname, ':') != NULL)
	{
		device_name = strdup(fsname);
	}
	else if (strchr(fsname,'/') == NULL)
	{
		/* Add prefix /dev/dsk to fsname */
		device_name = (char*) malloc(
					strlen("/dev/dsk/")+strlen(fsname)+1);
		ASSERT(device_name != NULL);
		strcpy(device_name,"/dev/dsk/");
		strcat(device_name,fsname);

		if (stat(device_name,&stat_buf)) {
			/* try XFS prefix */
			if (device_name != NULL)
			{
				free(device_name);
			}
			device_name = (char*) malloc(
				strlen("/dev/dsk/xlv/")+strlen(fsname)+1);
			ASSERT(device_name != NULL);
			strcpy(device_name,"/dev/dsk/xlv/");
			strcat(device_name,fsname);

			if (stat(device_name,&stat_buf)) {
				/* Invalid fs name */
				free(device_name);
				device_name = strdup(fsname);
			}
		}
	}
	/* Check if the filesystem path contains xlv/ prefix */
	else if (strstr(fsname,"xlv/")==fsname)
	{
		/* Add prefix /dev/dsk to fsname */
		device_name = (char*) malloc(
					strlen("/dev/dsk/")+strlen(fsname)+1);
		ASSERT(device_name != NULL);
		strcpy(device_name,"/dev/dsk/");
		strcat(device_name,fsname);
	}
	/* Check if the filesystem path contains /dev/rdsk prefix */
	else if (strstr(fsname,"/dev/rdsk/")==fsname)
	{
		dup_fsname = strdup(fsname);
		ASSERT(dup_fsname!=NULL);
	
		device_name = (char*) malloc(strlen(dup_fsname)+1);	
		ASSERT(device_name != NULL);
		memset(device_name,strlen(dup_fsname)+1,'\0');

		strcpy(device_name,"/dev/dsk");
		strtok(dup_fsname,"/");
		strtok(NULL,"/");
		while ((token = strtok(NULL,"/")) != NULL)
		{
			strcat(device_name,"/");
			strcat(device_name,token);
		}

		if (dup_fsname != NULL)
			free(dup_fsname);

		if (stat(device_name,&stat_buf))
		{
			/* Invalid fs name */
			free(device_name);
			device_name = strdup(fsname);
		}

	}
	else 
	{
		device_name = strdup(fsname);
	}

	return(device_name);
}

/*
 *	create_raw_device_name()
 *	The raw device name is constructed by replacing /dev/dsk in
 *	filesystem pathname with /dev/rdsk. 
 *	Example: For filesystem name, /dev/dsk/ips0d3s7 the raw device name 
 *		would be /dev/rdsk/ips0d3s7
 *		For filesystem name, /dev/dsk/xlv/t1 the raw device name 
 *		would be /dev/rdsk/xlv/t1
 *	Return Value : raw device name on success
 *			NULL on error
 */

char*
create_raw_device_name(char* fsname)
{
	char* raw_device_name=NULL;
	char *dup_fsname=NULL;
	char* token=NULL;

	/* Check if the filesystem path contains /dev/dsk prefix */
	if ((fsname!=NULL) && (strstr(fsname,"/dev/dsk/")==fsname))
	{
		dup_fsname = strdup(fsname);
		ASSERT(dup_fsname!=NULL);
	
		raw_device_name = (char*) malloc(strlen(dup_fsname)+2);	
		ASSERT(raw_device_name != NULL);
		memset(raw_device_name,strlen(dup_fsname)+2,'\0');

		strcpy(raw_device_name,"/dev/rdsk");
		strtok(dup_fsname,"/");
		strtok(NULL,"/");
		while ((token = strtok(NULL,"/")) != NULL)
		{
			strcat(raw_device_name,"/");
			strcat(raw_device_name,token);
		}

		if (dup_fsname != NULL)
			free(dup_fsname);
	}

	return(raw_device_name);
}

/*
 *	add_raw_device_name()
 *	This function checks the options string to determine if the raw
 *	device is specified. If the raw device name is not specified, 
 *	it is generated from the filesystem name and added to options string.
 */

void
add_raw_device_name(char* fsname,char** fs_options_str)
{
	char *raw_device_name=NULL;
	char str[BUFSIZ];

	/* Check if the raw device name is specified */
	if ((fs_options_str!=NULL) && (*fs_options_str != NULL) &&
		(strstr(*fs_options_str,"raw=")==NULL))
	{
		if ((fsname!=NULL) &&
			((raw_device_name=create_raw_device_name(fsname))!=NULL))
		{
			sprintf(str,",raw=%s",raw_device_name);
			add_to_buffer(fs_options_str,str);
			free(raw_device_name);
		}
	}
}

/*
 *	create_default_fs_options_str()
 *	A string containing the default options for the filesystem
 *	fstab entry is created. The raw device name is constructed from
 *	the filesystem name.
 */

char*
create_default_fs_options_str(char* fsname)
{
	char *fs_options_str=NULL;
	char *raw_device_name=NULL;
	char str[BUFSIZ];

	if (fsname != NULL)
	{
		/* The default option is rw */
		add_to_buffer(&fs_options_str,"rw");

		if ((raw_device_name=create_raw_device_name(fsname))!=NULL)
		{
			sprintf(str,",raw=%s",raw_device_name);
			add_to_buffer(&fs_options_str,str);
			free(raw_device_name);
		}
	}

	return(fs_options_str);
}

/*
 *	xfsCreateInternal(parameters, msg)
 *	
 *      A new filesystem is created by invoking the underlying
 *      the fs_create() function. The filesystem name and the various
 *	arguments for creating the filesystem are extracted from the 
 *	parameters string.
 *      The format of parameters string is a series of keyword value pairs
 *      with newline as seperator. All the arguments pertinent to creating
 *      the filesystem must be specified in parameters string. For example,
 *      the mount directory of the filesystem is indicated as shown below.
 *              XFS_MNT_DIR:/d2
 *
 *	The warning/error messages in msg string are of the format
 *		{message1}{message2}.....
 *
 *	Return Value:
 *		0	Success
 *		1	Failure
 *		2 	Partial success
 *
 */


int
xfsCreateInternal(const char* parameters, char **msg)
{
        struct mntent fstab_entry= { NULL, NULL, NULL, NULL, 0, 0 };
	int 	flag=0;
	int	returnVal=0;
	char	*exp_opts=NULL;
	char	*fs_opts=NULL;
	char	*line=NULL;
	char	*key=NULL;
	char	*value=NULL;	
	char	str[BUFSIZ];

	char	*efs_opts=NULL;
	char	*recovery_flag_str=NULL;
	char	*align_flag_str=NULL;
	char	*n_inodes_str=NULL;
	char	*blocks_str=NULL;
	char	*sectors_str=NULL;
	char	*cgsize_str=NULL;
	char	*cgalign_str=NULL;
	char	*ialign_str=NULL;
	char	*proto_str=NULL;

	char	*xfs_opts=NULL;
	char	*blk_size_log_str=NULL;
	char	*blk_size_str=NULL;
	char	*data_agcount_str=NULL;
	char	*data_file_type_str=NULL;
	char	*data_size_str=NULL;
	char	*inode_log_str=NULL;
	char	*inode_perblk_str=NULL;
	char	*inode_size_str=NULL;
	int	log_internal_value=0;
	char	*log_size_str=NULL;
	char	*proto_file_str=NULL;
	char	*rt_extsize_str=NULL;
	char	*rt_size_str=NULL;
	char	*xlv_device_str=NULL;
	char	*dup_parameters=NULL;
	char	*fs_type=NULL;
	int	mnt_dir_create_flag=0;
	char	*mnt_dir_owner=NULL;
	char	*mnt_dir_group=NULL;
	char	*mnt_dir_mode=NULL;

	boolean_t	dev_ok;
	char		device_name[PATH_MAX + 1];


	/* Check the input parameters */
	if ((parameters == NULL) || (msg == NULL)) {
		return(1);
	}

	*msg = NULL;

	dup_parameters = strdup(parameters);
	ASSERT(dup_parameters!=NULL);

	/* Initialize the dump frequency and fsck pass number */
	fstab_entry.mnt_passno = fstab_entry.mnt_freq = 0;

	/* Parse the parameters string */
	for(line = strtok((char *)parameters, "\n");
	    line != NULL;
	    line = strtok(NULL, "\n")) {

		/* Extract the keyword and value */
		if(xfsmGetKeyValue(line, &key, &value) &&
			(strcmp(value,"false")!=0) &&
			(strcmp(value,"FALSE")!=0)) {

			if (strcmp(key,XFS_FS_NAME_STR)==0) {
				fstab_entry.mnt_fsname = value;
			}
			else if (strcmp(key,XFS_MNT_DIR_STR)==0) {
				fstab_entry.mnt_dir = value;
			}
			else if (strcmp(key,XFS_FS_TYPE_STR)==0) {
				/* Convert the fs type to lower case */
				fstab_entry.mnt_type = fs_type = 
					to_lower_fs_type_str(value);
			}
			else if (strcmp(key,XFS_FS_DUMP_STR)==0) {
				fstab_entry.mnt_freq=atoi(value);
			}
			else if (strcmp(key,XFS_FS_FSCK_PASS_STR)==0) {
				fstab_entry.mnt_passno=atoi(value);
			}
			else if ((strcmp(key,XFS_MOUNT_FS_STR)==0) &&
				((strcmp(value,"true")==0) ||
				(strcmp(value,"TRUE")==0))) {
				flag |= MOUNT_FS;
			}
			else if ((strcmp(key,XFS_EXPORT_FS_STR)==0) &&
				((strcmp(value,"true")==0) ||
				(strcmp(value,"TRUE")==0))) {
				flag |= EXPORT_FS;
			}
			else if (strcmp(key,EFS_RECOVER_FLAG_STR)==0) {
				recovery_flag_str = value;
			}
			else if (strcmp(key,EFS_ALIGN_FLAG_STR)==0) {
				align_flag_str = value;
			}
			else if (strcmp(key,EFS_NO_INODES_STR)==0) {
				n_inodes_str = value;
			}
			else if (strcmp(key,EFS_NO_BLOCKS_STR)==0) {
				blocks_str = value;
			}
			else if (strcmp(key,EFS_SECTORS_TRACK_STR)==0) {
				sectors_str = value;
			}
			else if (strcmp(key,EFS_CYL_GRP_SIZE_STR)==0) {
				cgsize_str = value;
			}
			else if (strcmp(key,EFS_CGALIGN_STR)==0) {
				cgalign_str = value;
			}
			else if (strcmp(key,EFS_IALIGN_STR)==0) {
				ialign_str = value;
			}
			else if (strcmp(key,EFS_PROTO_STR)==0) {
				proto_str = value;
			}
			else if (strcmp(key,XFS_BLK_SIZE_LOG_STR)==0) {
				blk_size_log_str = value;
			}
			else if (strcmp(key,XFS_BLK_SIZE_STR)==0) {
				blk_size_str = value;
			}
			else if (strcmp(key,XFS_DATA_AGCOUNT_STR)==0) {
				data_agcount_str = value;
			}
			else if (strcmp(key,XFS_DATA_FILE_TYPE_STR) ==0) {
				data_file_type_str = value;
			}
			else if (strcmp(key,XFS_DATA_SIZE_STR)==0) {
				data_size_str = value;
			}
			else if (strcmp(key,XFS_INODE_LOG_STR)==0) {
				inode_log_str = value;
			}
			else if (strcmp(key,XFS_INODE_PERBLK_STR)==0) {
				inode_perblk_str = value;
			}
			else if (strcmp(key,XFS_INODE_SIZE_STR)==0) {
				inode_size_str = value;
			}
			else if ((strcmp(key,XFS_LOG_INTERNAL_STR)==0) &&
				((strcmp(value,"true")==0) ||
				(strcmp(value,"TRUE")==0))) {
				log_internal_value = 1;
			}
			else if (strcmp(key,XFS_LOG_SIZE_STR)==0) {
				log_size_str = value;
			}
			else if (strcmp(key,XFS_PROTO_FILE_STR)==0) {
				proto_file_str = value;
			}
			else if (strcmp(key,XFS_RT_EXTSIZE_STR)==0) {
				rt_extsize_str = value;
			}
			else if (strcmp(key,XFS_RT_SIZE_STR)==0) {
				rt_size_str = value;
			}
			else if (strcmp(key,XFS_XLV_DEVICE_STR)==0) {
				xlv_device_str = value;
			}
			else if ((strcmp(key,XFS_MNT_DIR_CREATE_STR)==0)
				&& ((strcmp(value,"true")==0) ||
				    (strcmp(value,"TRUE")==0))) {
				mnt_dir_create_flag = 1;
			}
			else if (strcmp(key,XFS_MNT_DIR_OWNER_STR)==0) {
				mnt_dir_owner = value;
			}
			else if (strcmp(key,XFS_MNT_DIR_GROUP_STR)==0) {
				mnt_dir_group = value;
			}
			else if (strcmp(key,XFS_MNT_DIR_MODE_STR)==0) {
				mnt_dir_mode = value;
			}
		}
	}

	/* Transform fs_name */
	dev_ok = xfsConvertNameToDevice(fstab_entry.mnt_fsname,
						device_name, B_FALSE);
	fstab_entry.mnt_fsname = device_name;

	/* Construct the string with export options for the filesystem*/
	if((exp_opts = create_export_options_str(dup_parameters)) == NULL) {
		if((exp_opts = get_export_opts(device_name)) == NULL) {
			exp_opts = strdup("ro");
		}
	}

	if(dev_ok == B_FALSE) { 
		sprintf(str, gettxt(":16","File system name not specified.\n"));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else if (fstab_entry.mnt_dir == NULL) {
		sprintf(str,gettxt(":17","%s: No mount directory specified.\n"),
			fstab_entry.mnt_fsname);
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else if (fstab_entry.mnt_type == NULL) {
		sprintf(str,gettxt(":13","%s: No filesystem type specified.\n"),
			fstab_entry.mnt_fsname);
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else if (is_valid_fs_type(fstab_entry.mnt_type)!=0) {
		sprintf(str,gettxt(":24", "%s: Unknown filesystem type.\n"),
			fstab_entry.mnt_type);
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	/* Create the mount directory */
	else if ((mnt_dir_create_flag == 1) &&
		(create_mnt_dir(mnt_dir_owner,mnt_dir_group,
			mnt_dir_mode, fstab_entry.mnt_dir,msg)!=0)) {
		sprintf(str,gettxt(":25", "%s: Unable to create directory.\n"),
			fstab_entry.mnt_dir);
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	/* Check the filesystem options */
	else if (((fs_opts = create_fs_options_str(
		fstab_entry.mnt_type,dup_parameters))==NULL) &&
		((fs_opts=create_default_fs_options_str(
				fstab_entry.mnt_fsname))==NULL)) {
		sprintf(str,gettxt(":14", "%s: Missing filesystem options.\n"),
			fstab_entry.mnt_fsname);
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else if (strcmp(fstab_entry.mnt_type,EFS_FS_TYPE)==0) {
		/* Check for align flag */
		if ((align_flag_str != NULL) && 
		    (strcmp(align_flag_str,"TRUE")==0)) {
			add_to_buffer(&efs_opts,"-a ");
		}

		/* Check for recovery flag */
		if ((recovery_flag_str != NULL) && 
		    (strcmp(recovery_flag_str,"TRUE")==0)) {
			add_to_buffer(&efs_opts,"-r ");
		}

		/* Check for #inodes */
		if (n_inodes_str != NULL) {
			sprintf(str,"-n %s ",n_inodes_str);
			add_to_buffer(&efs_opts,str);
		}

		/* Append the filesystem name */
		add_to_buffer(&efs_opts,fstab_entry.mnt_fsname);

		if (blocks_str != NULL) {
			sprintf(str, " %s ", blocks_str);
			add_to_buffer(&efs_opts, str);

			/* The other arguments, min_inodes, heads,
			   sectors, cfgsize, cgalign & ialign must
			   be specified too */
			if (n_inodes_str == NULL) {
				sprintf(str,gettxt(":19",
				"%s: Number of inodes not specified.\n"),
				fstab_entry.mnt_fsname);
				add_to_buffer(msg,str);
				returnVal = 1;
			}
			if (sectors_str == NULL) {
				sprintf(str,gettxt(":20",
			"%s: Number of sectors per track not specified.\n"),
				fstab_entry.mnt_fsname);
				add_to_buffer(msg,str);
				returnVal = 1;
			}
			if (cgsize_str == NULL) {
				sprintf(str,gettxt(":21",
		"%s: Size of cylinder group in blocks not specified.\n"),
				fstab_entry.mnt_fsname);
				add_to_buffer(msg,str);
				returnVal = 1;
			}
			if (cgalign_str == NULL) {
				sprintf(str,gettxt(":22",
		"%s: Cylinder group alignment boundary not specified.\n"),
				fstab_entry.mnt_fsname);
				add_to_buffer(msg,str);
				returnVal = 1;
			}
			if (ialign_str == NULL) {
				sprintf(str,gettxt(":23",
	"%s: Cylinder group inode list alignment boundary not specified.\n"),
				fstab_entry.mnt_fsname);
				add_to_buffer(msg,str);
				returnVal = 1;
			}
			if(returnVal == 0) {
				sprintf(str, "%s %s %s %s %s %s ",
					n_inodes_str,
					"0",	/* no. of heads unused */
					sectors_str,
					cgsize_str,
					cgalign_str,
					ialign_str);
				add_to_buffer(&efs_opts, str);
			}
		}		

		if (proto_str != NULL) {
			add_to_buffer(&efs_opts,proto_str);
		}	
	}			
	else if (strcmp(fstab_entry.mnt_type,XFS_FS_TYPE)==0) {
		/* Check for blocksize */
		if (blk_size_log_str != NULL) {
			sprintf(str,"-b log=%s ",blk_size_log_str);
			add_to_buffer(&xfs_opts,str);
		}
		else if (blk_size_str != NULL) {
			sprintf(str,"-b size=%s ",blk_size_str);
			add_to_buffer(&xfs_opts,str);
		}
		
		/* Check for data subvol options */
		if (data_agcount_str != NULL) {
			sprintf(str,"-d agcount=%s ", data_agcount_str);
			add_to_buffer(&xfs_opts,str);
		}
		if (data_file_type_str != NULL) {
			if (strcmp(data_file_type_str,
					XFS_DATA_FILE_REGULAR_TYPE)==0) {
				sprintf(str,"-d file=1 ");
				add_to_buffer(&xfs_opts,str);
			}
			else if (strcmp(data_file_type_str,
					XFS_DATA_FILE_SPECIAL_TYPE)==0) {
				sprintf(str,"-d file=0 ");
				add_to_buffer(&xfs_opts,str);
			}
		}
		if (data_size_str != NULL) {
			sprintf(str,"-d size=%s ", data_size_str);
			add_to_buffer(&xfs_opts,str);
		}

		/* Check for Inode options */
		if (inode_log_str != NULL) {
			sprintf(str,"-i log=%s ", inode_log_str);
			add_to_buffer(&xfs_opts,str);
		}
		else if (inode_perblk_str != NULL) {
			sprintf(str,"-i perblock=%s ", inode_perblk_str);
			add_to_buffer(&xfs_opts,str);
		}
		else if (inode_size_str != NULL) {
			sprintf(str,"-i size=%s ",inode_size_str);
			add_to_buffer(&xfs_opts,str);
		}

		/* Check for Log section options */
		if (log_internal_value == 1)  {
			add_to_buffer(&xfs_opts, "-l internal ");
		}
		if (log_size_str != NULL) {
			sprintf(str,"-l size=%s ",log_size_str);
			add_to_buffer(&xfs_opts,str);
		}

		/* Check for prototype file */
		if (proto_file_str != NULL) {
			sprintf(str,"-p %s ",proto_file_str);
			add_to_buffer(&xfs_opts,str);
		}

		/* Check for Realtime section options */
		if (rt_extsize_str != NULL) {
			sprintf(str,"-r extsize=%s ",rt_extsize_str);
			add_to_buffer(&xfs_opts,str);
		}
		if (rt_size_str != NULL) {
			sprintf(str,"-r size=%s ",rt_size_str);
			add_to_buffer(&xfs_opts,str);
		}

		/* Check if the xlv device name is indicated */
		if ((xlv_device_str != NULL) && 
			((strcmp(xlv_device_str,"true")==0) ||
				(strcmp(xlv_device_str,"TRUE")==0))) {
			sprintf(str,"%s ", fstab_entry.mnt_fsname);
			add_to_buffer(&xfs_opts,str);
		}
		else {
			sprintf(str,"-d name=%s ", fstab_entry.mnt_fsname);
			add_to_buffer(&xfs_opts,str);
		}
	}	

	if (returnVal == 0) {
		/* Add the raw device name in case of efs & xfs filesystems*/
		if ((strcmp(fstab_entry.mnt_type,XFS_FS_TYPE)==0) ||
			(strcmp(fstab_entry.mnt_type,EFS_FS_TYPE)==0)) {
			add_raw_device_name(fstab_entry.mnt_fsname,
					&fs_opts);
		}

		fstab_entry.mnt_opts = fs_opts;

#ifdef DEBUG
		if (efs_opts != NULL) {
			sprintf(debug_str,"mkfs_efs %s",efs_opts);
			dump_to_debug_file("mkfs_efs command");
			dump_to_debug_file(debug_str);
			printf("DEBUG:: mkfs_efs %s\n",efs_opts);
		}
		if (xfs_opts != NULL) {
			sprintf(debug_str,"mkfs_xfs %s",xfs_opts);
			dump_to_debug_file("mkfs_xfs command");
			dump_to_debug_file(debug_str);
			printf("DEBUG:: mkfs_xfs %s\n",xfs_opts);
		}
#endif

		/* Check if the efs_opts/xfs_opts are given */
		if (((strcmp(fstab_entry.mnt_type,XFS_FS_TYPE)==0) &&
			(xfs_opts==NULL)) ||
			((strcmp(fstab_entry.mnt_type,EFS_FS_TYPE)==0) &&
			(efs_opts==NULL))) {
			sprintf(str,gettxt(":14",
				"%s: Missing filesystem options.\n"),
				fstab_entry.mnt_fsname);
			add_to_buffer(msg,str);
			returnVal = 1;
		}
		else {
			returnVal = xfs_fs_create(NULL,&fstab_entry,
					efs_opts,xfs_opts, exp_opts,flag,msg);
		}
	}

	if (efs_opts != NULL) {
		free(efs_opts);
	}
	if (xfs_opts != NULL) {
		free(xfs_opts);
	}
	if (exp_opts != NULL) {
		free(exp_opts);
	}
	if (fs_opts != NULL) {
		free(fs_opts);
	}
	if (fs_type != NULL) {
		free(fs_type);
	}
	if (dup_parameters != NULL) {
		free(dup_parameters);
	}

	return(returnVal);
}

/*
 *	xfsEditInternal(parameters, msg)
 *	
 *	An existing filesystem is modified by invoking the underlying
 *	the xfs_fs_edit() function. The arguments are extracted
 *	from the parameters string. The parameters string is a series of
 *	keyword value pairs seperated by newline. The keywords this function
 *	looks for are as follows: XFS_FS_NAME, XFS_MNT_DIR, XFS_FS_TYPE,
 *				  XFS_FS_OPTS, XFS_FS_DUMP, XFS_EXPOPTS.
 *
 *	The warning/error messages in msg string are of the format
 *		{message1}{message2}.....
 *
 *	Return Value:
 *		0	Success
 *		1	Failure
 *		2 	Partial success
 *
 */


int
xfsEditInternal(const char* parameters, char **msg)
{
	struct mntent fstab_entry= { NULL, NULL, NULL, NULL, 0, 0 };
	int	returnVal=0;
	char*	export_options_str=NULL;
	char	*fs_options_str=NULL;
	char	*line=NULL;
	char	*key=NULL;
	char	*value=NULL;	
	char	str[BUFSIZ];
	char	*fs_type=NULL;
	char	*dup_parameters=NULL;

	boolean_t	dev_ok;
	char		device_name[PATH_MAX + 1];

	/* Check the input parameters */
	if ((parameters == NULL) || (msg == NULL))
	{
		returnVal = 1;
	}
	else
	{
		*msg = NULL;

		dup_parameters = strdup(parameters);
		ASSERT(dup_parameters!=NULL);

		/* Initialize the dump frequency and fsck pass number */
		fstab_entry.mnt_passno = fstab_entry.mnt_freq = 0;

		/* Parse the parameters string */
		for(line = strtok((char *)parameters, "\n");
		    line != NULL;
		    line = strtok(NULL, "\n")) {

			/* Extract the keyword and value */
			if(xfsmGetKeyValue(line, &key, &value) &&
				(strcmp(value,"false")!=0) &&
				(strcmp(value,"FALSE")!=0))
			{
				if (strcmp(key,XFS_FS_NAME_STR)==0)
				{
					fstab_entry.mnt_fsname = value;
				}
				else if (strcmp(key,XFS_MNT_DIR_STR)==0)
				{
					fstab_entry.mnt_dir = value;
				}
				else if (strcmp(key,XFS_FS_TYPE_STR)==0)
				{
					/* Convert the fs type to lower case */
					fstab_entry.mnt_type = fs_type = 
						to_lower_fs_type_str(value);
				}
				else if (strcmp(key,XFS_FS_DUMP_STR)==0)
				{
					fstab_entry.mnt_freq = atoi(value);
				}
				else if (strcmp(key,XFS_FS_FSCK_PASS_STR)==0)
				{
					fstab_entry.mnt_passno=atoi(value);
				}
			}
		}

		/* Construct the string with export options for xfs/efs 
		   filesystems */
		export_options_str = create_export_options_str(dup_parameters);

		/* Transform fs_name */
		dev_ok = xfsConvertNameToDevice(fstab_entry.mnt_fsname,
							device_name, B_FALSE);
		fstab_entry.mnt_fsname = device_name;

		if(dev_ok == B_FALSE) {
			sprintf(str,gettxt(":16",
					"File system name not specified\n"));
			add_to_buffer(msg,str);
			returnVal = 1;
		}
		else if (fstab_entry.mnt_dir == NULL) 
		{
			sprintf(str,gettxt(":17",
					"%s: No ount directory specified.\n"),
					fstab_entry.mnt_fsname);
			add_to_buffer(msg,str);
			returnVal = 1;
		}
		else if (fstab_entry.mnt_type == NULL)
		{
			sprintf(str,gettxt(":13",
				"%s: No filesystem type specified.\n"),
				fstab_entry.mnt_fsname);
			add_to_buffer(msg,str);
			returnVal = 1;
		}
		else if (is_valid_fs_type(fstab_entry.mnt_type)!=0)
		{
			sprintf(str,gettxt(":24",
				"%s: Unknown filesystem type.\n"),
				fstab_entry.mnt_type);
			add_to_buffer(msg,str);
			returnVal = 1;
		}
		/* Construct the string with filesystem options */
		else if (((fs_options_str = create_fs_options_str(
			fstab_entry.mnt_type,dup_parameters))==NULL) &&
			((fs_options_str=create_default_fs_options_str(
					fstab_entry.mnt_fsname))==NULL))
		{
			sprintf(str,gettxt(":14",
					"%s: Missing filesystem options.\n"),
					fstab_entry.mnt_fsname);
			add_to_buffer(msg,str);
			returnVal = 1;
		}
		else 
		{
			add_raw_device_name(fstab_entry.mnt_fsname,
						&fs_options_str);
			fstab_entry.mnt_opts = fs_options_str;

			returnVal = xfs_fs_edit(NULL,&fstab_entry,
						export_options_str,msg);
		}
	}

	if (export_options_str != NULL) {
		free(export_options_str);
	}
	if (fs_options_str != NULL) {
		free(fs_options_str);
	}
	if (fs_type != NULL) {
		free(fs_type);
	}
	if (dup_parameters != NULL) {
		free(dup_parameters);
	}

	return(returnVal);
}

/*
 *      xfsExportInternal(parameters, msg)
 *
 *      The specified filesystem is exported. The filesystem name, 
 *	mount directory & the flags are stored in the parameters string 
 *	as keyword value pairs.
 *	The keywords this function looks for are as follows : XFS_FS_NAME,
 *	XFS_MNT_DIR. 
 *
 *      All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 */

int
xfsExportInternal(const char* parameters, char **msg)
{
	char* 	fs_name=NULL;
	char* 	fs_name_str=NULL;
	char* 	mnt_dir=NULL;
	char* 	mnt_pt=NULL;
	int	returnVal=0;
	char	*line=NULL;
	char	*key=NULL;
	char	*value=NULL;	
	char	str[BUFSIZ];
	char	*exp_opts=NULL;
	char	*dup_parameters=NULL;
	boolean_t	update_exports = B_FALSE;

	boolean_t	dev_ok;
	char		device_name[PATH_MAX + 1];

#ifdef DEBUG
	if (parameters != NULL) {
		printf("DEBUG: xfsExportInternal parameters %s\n", parameters);
	}
#endif

	/* Check the input parameters */
	if ((parameters == NULL) || (msg == NULL)) {
		return(1);
	}

	*msg = NULL;

	/* Make a copy of the parameters */
	dup_parameters = strdup(parameters);
	ASSERT(dup_parameters!=NULL);

	/* Parse the parameters string */
	for(line = strtok((char *)parameters, "\n");
	    line != NULL;
	    line = strtok(NULL, "\n")) {

		/* Extract the keyword and value */
		if(xfsmGetKeyValue(line, &key, &value) &&
		    (strcmp(value,"false")!=0) &&
		    (strcmp(value,"FALSE")!=0)) {
			if (strcmp(key,XFS_FS_NAME_STR)==0) {
				fs_name = value;
			}
			else if (strcmp(key,XFS_MNT_DIR_STR)==0) {
				mnt_dir = value;
			}
			else if (strcmp(key,XFS_UPDATE_EXPORTS_STR)==0) {
				update_exports = B_TRUE;
			}
		}
	}

	/* Check if fs_name is a valid directory */
	if ((fs_name != NULL) && (isvalidmntpt(fs_name)==0)) {
		/* Get the export options for the filesystem
		 * First try the input keyword/value pairs
		 */
		if((exp_opts=create_export_options_str(dup_parameters))==NULL) {
		    if((exp_opts = get_export_opts(fs_name)) == NULL) {
			exp_opts = strdup("ro");
		    }
		}

		mnt_dir = fs_name;
		returnVal = xfs_export(NULL, NULL, mnt_dir, update_exports,
					 exp_opts, msg);
	}
	else {
		/* Transform fs_name */
		dev_ok = xfsConvertNameToDevice(fs_name, device_name, B_FALSE);
		fs_name = device_name;

		if(dev_ok == B_TRUE) {
			/* If the mount directory is not supplied,
			 * lookup the file /etc/mtab and /etc/fstab
			 */
			if (mnt_dir == NULL) {
				/* Lookup /etc/mtab first */
				if ((mnt_dir = mnt_pt = 
					get_mnt_dir(MTAB,fs_name)) == NULL) {
					/* Try /etc/fstab */
					mnt_dir = mnt_pt = 
					get_mnt_dir(FSTAB,fs_name);
				}
			}
			/* If the mount directory is given but the filesystem
			 * name is not given, lookup FSTAB using the 
			 * mount directory
			 */
			else if (strcmp(fs_name,INVALID_FS_NAME_STR)==0) {
				fs_name=fs_name_str=get_fs_name(FSTAB,mnt_dir);
			}
		}

		if (fs_name == NULL) {
			sprintf(str,gettxt(":16",
				"File system name not specified.\n"));
			add_to_buffer(msg,str);
			returnVal = 1;
		}
		else if (mnt_dir == NULL) {
			sprintf(str,gettxt(":17",
				"%s: No mount directory specified.\n"),
				fs_name);
			add_to_buffer(msg,str);
			returnVal = 1;
		}
		else {
			/* Get the export options for the filesystem */
			/* First try the input keyword/value pairs */
			if ((exp_opts = create_export_options_str(
						dup_parameters)) == NULL) {
			    if((exp_opts = get_export_opts(mnt_dir)) == NULL) {
				exp_opts = strdup("ro");
			    }
			}

			returnVal = xfs_export(NULL, fs_name, mnt_dir,
				 update_exports, exp_opts, msg);
		}
	}

	if (dup_parameters != NULL) {
		free(dup_parameters);
	}
	if (exp_opts != NULL) {
		free(exp_opts);
	}
	if (mnt_pt != NULL) {
		free(mnt_pt);
	}
	if (fs_name_str != NULL) {
		free(fs_name_str);
	}

	return(returnVal);
}

/*
 *      xfsUnexportInternal(parameters, msg)
 *
 *      The specified filesystem is unexported. 
 *      The filesystem name is stored in the parameters string 
 *	as keyword value pair.
 *
 *      All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 */

int
xfsUnexportInternal(const char* parameters, char **msg)
{
	char* 	fs_name=NULL;
	int	returnVal=0;
	char	*line=NULL;
	char	*key=NULL;
	char	*value=NULL;	
	char	str[BUFSIZ];
	int	delete_flag=0;

	boolean_t	dev_ok;
	char		device_name[PATH_MAX + 1];

	/* Check the input parameters */
	if ((parameters == NULL) || (msg == NULL)) {
		return(1);
	}

	*msg = NULL;

	/* Parse the parameters string */
	for(line = strtok((char *)parameters, "\n");
	    line != NULL;
	    line = strtok(NULL, "\n")) {

		/* Extract the keyword and value */
		if(xfsmGetKeyValue(line, &key, &value)) {
			if ((strcmp(key,XFS_RM_EXPORTS_ENTRY_STR)==0) &&
				 ((strcmp(value,"true")==0) || 
				 (strcmp(value,"TRUE")==0)))
			{
				delete_flag |= DEL_EXPORT_ENTRY_FLAG;
			}
			else if (strcmp(key,XFS_FS_NAME_STR)==0)
			{
				fs_name = value;
			}
			
		}
	}

#ifdef	DEBUG
	printf("xfsUnexportInternal: %s\n", fs_name);
#endif

	/* Check if fs_name is a valid directory */
	if ((fs_name != NULL) && (isvalidmntpt(fs_name)==0)) {
#ifdef	DEBUG
	printf("\tisvalidmntpt == 0\n");
#endif
		/* Get the export options for the filesystem
		 * First try the input keyword/value pairs
		 */
		returnVal = xfs_unexport(NULL,fs_name,delete_flag,msg);
	} else {
		/* Transform fs_name */
		dev_ok = xfsConvertNameToDevice(fs_name, device_name, B_FALSE);
		fs_name = device_name;

		if(dev_ok == B_FALSE) {
			sprintf(str,gettxt(":16",
					"File system name not specified.\n"));
			add_to_buffer(msg,str);
			returnVal = 1;
		}
		else
		{
#ifdef	DEBUG
	printf("\tisvalidmntpt != 0\n");
	printf("\tfs_name = %s\n", fs_name);
#endif
			returnVal = xfs_unexport(NULL,fs_name,delete_flag,msg);
		}
	}

	return(returnVal);
}

/*
 *      xfsMountInternal(parameters, msg)
 *
 *      The specified filesystem is mounted and/or exported depending on
 *      the mount & export flags. The filesystem name, mount directory & 
 *	the flags are stored in the parameters string as keyword value pairs.
 *	The keywords this function
 *	looks for are as follows:XFS_FS_NAME,XFS_MNT_DIR and
 *	XFS_EXPORT_FS.
 *	If the value of the keyword XFS_EXPORT_FS is "TRUE", the filesystem
 *	is exported.
 *
 *      All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 */
int
xfsMountInternal(const char* parameters, char **msg)
{
	char* 	fs_name=NULL;
	char* 	fs_name_str=NULL;
	char* 	mnt_dir=NULL;
	char* 	mnt_pt=NULL;
	char*	fs_type=NULL;
	int	flag=MOUNT_FS;
	int	returnVal=0;
	char	*line=NULL;
	char	*key=NULL;
	char	*value=NULL;	
	char	str[BUFSIZ];
	char	*export_options_str=NULL;
	char	*mnt_options=NULL;
	char	*dup_parameters=NULL;

	int	mnt_dir_create_flag=0;
	char	*mnt_dir_owner=NULL;
	char	*mnt_dir_group=NULL;
	char	*mnt_dir_mode=NULL;

	char	*device_name;
	struct  mntent nfstab_ent;
	struct  mntent ofstab_ent;
	char	*xfs_remote_host_str=NULL;
	char	*nfs_fs_name=NULL;
	char	*xfs_update_fstab_flag=NULL;

	*msg = NULL;

	/* Check the input parameters */
	if ((parameters == NULL) || (msg == NULL)) {
		return(1);
	}

	/* Make a copy of the parameters */
	dup_parameters = strdup(parameters);
	ASSERT(dup_parameters!=NULL);

	/* Parse the parameters string */
	for(line = strtok((char *)parameters, "\n");
	    line != NULL;
	    line = strtok(NULL, "\n")) {

		/* Extract the keyword and value */
		if(xfsmGetKeyValue(line, &key, &value) &&
			(strcmp(value,"false")!=0) &&
			(strcmp(value,"FALSE")!=0)) {

			if (strcmp(key,XFS_FS_NAME_STR)==0) {
				fs_name = value;
			}
			else if (strcmp(key,XFS_MNT_DIR_STR)==0) {
				mnt_dir = value;
			}
			else if (strcmp(key,XFS_FS_TYPE_STR)==0) {
				/* Convert the fs type to lower case */
				fs_type = to_lower_fs_type_str(value);
			}
			else if (strcmp(key,XFS_MNT_DIR_OWNER_STR)==0) {
				mnt_dir_owner = value;
			}
			else if (strcmp(key,XFS_MNT_DIR_GROUP_STR)==0) {
				mnt_dir_group = value;
			}
			else if (strcmp(key,XFS_MNT_DIR_MODE_STR)==0) {
				mnt_dir_mode = value;
			}
			else if (strcmp(key,XFS_REMOTE_HOST_STR)==0) {
				xfs_remote_host_str = value;
			}
			else if (strcmp(key,XFS_UPDATE_FSTAB_STR)==0) {
				xfs_update_fstab_flag = value;
			}
			else if ((strcmp(value,"true")==0) ||
				(strcmp(value,"TRUE")==0)) {
				if (strcmp(key,XFS_MOUNT_FS_STR)==0) {
					flag |= MOUNT_FS;
				}
				else if(strcmp(key, XFS_EXPORT_FS_STR)==0) {
					flag |= EXPORT_FS;
				}
				else if(strcmp(key, XFS_MNT_DIR_CREATE_STR)==0){
					mnt_dir_create_flag = 1;
				}
			}
		}
	}

	/* Retreive the fs name/mount point, if required from/etc/fstab */
	if ((fs_name != NULL) && (fs_type != NULL) &&
	    (strcmp(fs_type, NFS_FS_TYPE) != 0)) {

		/* Transform fs_name */
		device_name = fs_name = create_device_name(fs_name);

		/* If the mount directory is not supplied, lookup
		   the file /etc/fstab */
		if (mnt_dir == NULL) {
			mnt_dir = mnt_pt = get_mnt_dir(FSTAB,fs_name);
		}
		/* If the mount directory is given but the filesystem
		   name is not given, lookup FSTAB using the mount directory */
		else if (strcmp(fs_name,INVALID_FS_NAME_STR)==0) {
			fs_name = fs_name_str=get_fs_name(FSTAB,mnt_dir);
		}
	}

	/* Create NFS fs_name */
	if (strcmp(fs_type, NFS_FS_TYPE)==0) {
		if (xfs_remote_host_str == NULL) {
			/* TODO Add error message */
			returnVal = 1;
		}
		else {
			nfs_fs_name = (char*) malloc(
						strlen(xfs_remote_host_str) +
						1 + strlen(fs_name) + 1);
			ASSERT(nfs_fs_name != NULL);
			strcpy(nfs_fs_name, xfs_remote_host_str);
			strcat(nfs_fs_name, ":");
			strcat(nfs_fs_name, fs_name);

			fs_name = nfs_fs_name;
#ifdef DEBUG
printf("DEBUG: fs_name = %s\n", fs_name);
#endif

		}
	}

	if (fs_name == NULL) {
		sprintf(str, gettxt(":16",
				"File system name not specified.\n"));
		add_to_buffer(msg, str);
		returnVal = 1;
	}
	else if (mnt_dir == NULL) {
		sprintf(str, gettxt(":17",
			"%s: No mount directory specified.\n"), fs_name);
		add_to_buffer(msg, str);
		returnVal = 1;
	}
	else if (fs_type == NULL) {
		sprintf(str, gettxt(":13",
			"%s: No filesystem type specified.\n"), fs_name);
		add_to_buffer(msg, str);
		returnVal = 1;
	}
	else if (is_valid_fs_type(fs_type)!=0) {
		sprintf(str, gettxt(":24",
			"%s: Unknown filesystem type.\n"), fs_type);
		add_to_buffer(msg, str);
		returnVal = 1;
	}
	/* Create the mount directory */
	else if ((mnt_dir_create_flag == 1) &&
		(create_mnt_dir(mnt_dir_owner,mnt_dir_group,
			mnt_dir_mode,mnt_dir,msg)!=0)) {
		sprintf(str, gettxt(":25",
			"%s: Unable to create directory.\n"), mnt_dir);
		add_to_buffer(msg, str);
		returnVal = 1;
	}
	else {
		/* Get the mount options for the filesystem */
		/* First try the input keyword/value pairs */
		if ((mnt_options = create_fs_options_str(
					fs_type,dup_parameters)) == NULL) {
/*
 *	TODO:	Replace with xfs_getmentent()
 */
			/* Lookup fstab file */
			mnt_options = get_mnt_opts(fs_name);
		}

		/* Get the export options for the filesystem */
		/* First try the input keyword/value pairs */
		if ((export_options_str = 
			     create_export_options_str(dup_parameters))==NULL) {
			export_options_str = get_export_opts(mnt_dir);
		}

		returnVal = xfs_mount(NULL, fs_name, mnt_dir,
				mnt_options, export_options_str, flag, msg);

#ifdef DEBUG
	printf("DEBUG: xfs_update_fstab_flag is %s\n",xfs_update_fstab_flag);
#endif
		/* Add/Update the fstab entry */
		if ((returnVal == 0) && (xfs_update_fstab_flag != NULL)) {
#ifdef DEBUG
	printf("DEBUG: xfs_update_fstab_flag is %s\n",xfs_update_fstab_flag);
#endif

			/* Add default mount option if no mount options given */
			if (mnt_options == NULL) {
				if (strcmp(fs_type, NFS_FS_TYPE) == 0) {
				    add_to_buffer(&mnt_options, "rw");
				}
				else {
				    add_to_buffer(&mnt_options, "rw,noquota");
				    add_raw_device_name(fs_name, &mnt_options);
				}
			}
			else if (strcmp(fs_type, NFS_FS_TYPE) != 0) {
				add_raw_device_name(fs_name, &mnt_options);
			}
				
			nfstab_ent.mnt_fsname = fs_name;
			nfstab_ent.mnt_dir = mnt_dir;
			nfstab_ent.mnt_type = fs_type;
			nfstab_ent.mnt_opts = mnt_options;
			nfstab_ent.mnt_freq = 0;
			nfstab_ent.mnt_passno = 0;

			if (xfs_getmntent(FSTAB, fs_name, &ofstab_ent) == 0) {
				if(xfs_replace_mntent(FSTAB, &ofstab_ent,
							&nfstab_ent) != 0) {
					returnVal = 1;
				}
				free(ofstab_ent.mnt_fsname);
				free(ofstab_ent.mnt_dir);
				free(ofstab_ent.mnt_type);
				free(ofstab_ent.mnt_opts);
			} else {
				if(xfs_replace_mntent(FSTAB, NULL,
							&nfstab_ent) != 0) {
					returnVal = 1;
				}
			}
		}
	}

	if (dup_parameters != NULL) {
		free(dup_parameters);
	}
	if (mnt_options != NULL) {
		free(mnt_options);
	}
	if (export_options_str != NULL) {
		free(export_options_str);
	}
	if (fs_type != NULL) {
		free(fs_type);
	}
	if (mnt_pt != NULL) {
		free(mnt_pt);
	}
	if (fs_name_str != NULL) {
		free(fs_name_str);
	}
	if (device_name != NULL) {
		free(device_name);
	}
	if (nfs_fs_name != NULL) {
		free(nfs_fs_name);
	}

	return(returnVal);
}

/*
 *      xfsUnmountInternal(parameters, msg)
 *
 *      The specified filesystem is unmounted and/or unexported depending on
 *      the mount & export flag. The filesystem name, mount directory and 
 *	the flags are stored in the parameters string as keyword value pairs.
 *	The keywords this function
 *	looks for are as follows: XFS_FS_NAME. 
 *
 *      All errors and warnings are reported in the msg string in the format.
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 */

int
xfsUnmountInternal(const char *parameters, char **msg)
{
	char* 	fs_name=NULL;
	int	flag=UNMOUNT_FS;
	int	returnVal=0;
	char	*line=NULL;
	char	*key=NULL;
	char	*value=NULL;	
	char	str[BUFSIZ];
	int	force_umount_flag = XFS_REG_UMOUNT_FS;

	char	*xfs_remote_host_str=NULL;
	char	*nfs_fs_name=NULL;

	boolean_t	dev_ok;
	char		device_name[PATH_MAX + 1];

	/* Check the input parameters */
	if ((parameters == NULL) || (msg == NULL)) {
		return(1);
	}

	*msg = NULL;

	/* Parse the parameters string */
	for(line = strtok((char *)parameters, "\n");
	    line != NULL;
	    line = strtok(NULL, "\n")) {

		/* Extract the keyword and value */
		if(xfsmGetKeyValue(line, &key, &value)) {
			if ((strcmp(key,XFS_UNEXPORT_FS_STR) == 0) &&
			    ((strcmp(value,"true") == 0) ||
			    (strcmp(value,"TRUE") == 0))) {
				flag |= UNEXPORT_FS;
			}
			else if (strcmp(key,XFS_FS_NAME_STR) == 0) {
				fs_name = value;
			}
			else if ((strcmp(key, XFS_FORCE_UMOUNT_STR)==0) &&
				((strcmp(value, "true") == 0) ||
				  strcmp(value,"TRUE") == 0)) {
				force_umount_flag = 
					XFS_FORCE_UMOUNT_FS;
			}
			else if (strcmp(key,XFS_REMOTE_HOST_STR)==0) {
				xfs_remote_host_str = value;
			}
		}
	}

	/* Transform fs_name */
	dev_ok = xfsConvertNameToDevice(fs_name, device_name, B_FALSE);
	fs_name = device_name;

	if((dev_ok == B_TRUE) && (xfs_remote_host_str != NULL)) {
		nfs_fs_name = (char*) malloc(
				strlen(xfs_remote_host_str) +
				1 + strlen(fs_name) + 1);
		ASSERT(nfs_fs_name != NULL);
		strcpy(nfs_fs_name,xfs_remote_host_str);
		strcat(nfs_fs_name,":");
		strcat(nfs_fs_name,fs_name);

		fs_name = nfs_fs_name;
	}

	if(dev_ok == B_FALSE) {
		sprintf(str, gettxt(":16",
				"File system name not specified.\n"));
		add_to_buffer(msg, str);
		returnVal = 1;
	}
	else {
		returnVal = xfs_umount(NULL,fs_name,flag,force_umount_flag,msg);
	}

	if (nfs_fs_name != NULL) {
		free(nfs_fs_name);
	}

	return(returnVal);
}


/*
 *	xfsDeleteInternal(parameters, msg)
 *	
 *	The filesystem is deleted by invoking the underlying
 *	the xfs_fs_delete() function. The arguments are extracted
 *	from the parameters string. The keyword XFS_FS_NAME is searched and its
 *	value is the name of the filesystem to be deleted.
 *
 *	The warning/error messages in msg string are of the format
 *		{message1}{message2}.....
 *
 *	Return Value:
 *		0	Success
 *		1	Failure
 *		2 	Partial success
 *
 */


int
xfsDeleteInternal(const char* parameters, char **msg)
{
	char* 	fs_name=NULL;
	int	returnVal=0;
	char	*line=NULL;
	char	*key=NULL;
	char	*value=NULL;	
	char	str[BUFSIZ];
	int	delete_flag=0;
	int	force_umount_flag = XFS_REG_UMOUNT_FS;

	boolean_t	dev_ok;
	char		device_name[PATH_MAX + 1];

	/* Check the input parameters */
	if ((parameters == NULL) || (msg == NULL)) {
		return(1);
	}

	*msg = NULL;

	/* Parse the parameters string */
	for(line = strtok((char *)parameters, "\n");
	    line != NULL;
	    line = strtok(NULL, "\n")) {

		/* Extract the keyword and value */
		if(xfsmGetKeyValue(line, &key, &value)) {
			if (strcmp(key,XFS_FS_NAME_STR)==0)
			{
				fs_name = value;
			}
			else if ((strcmp(key,
					XFS_RM_FSTAB_ENTRY_STR)==0) &&
				 ((strcmp(value,"true")==0) || 
				 (strcmp(value,"TRUE")==0)))
			{
				delete_flag |= DEL_FSTAB_ENTRY_FLAG;
			}
			else if ((strcmp(key,
					XFS_RM_EXPORTS_ENTRY_STR)==0) &&
				 ((strcmp(value,"true")==0) || 
				 (strcmp(value,"TRUE")==0)))
			{
				delete_flag |= DEL_EXPORT_ENTRY_FLAG;
			}
			else if ((strcmp(key,XFS_FORCE_UMOUNT_STR)==0)&&
				 ((strcmp(value,"true")==0) || 
				 (strcmp(value,"TRUE")==0)))

			{
				force_umount_flag = XFS_FORCE_UMOUNT_FS;
			}
		}
	}

#ifdef DEBUG
	if (fs_name != NULL)
	{
		printf("DEBUG: xfsDeleteInternal fsname %s\n",fs_name);
	}
#endif

	/* Transform fs_name */
	dev_ok = xfsConvertNameToDevice(fs_name, device_name, B_FALSE);

	if(dev_ok == B_FALSE) {
	    /*
	     *	Check to see if someone removed the volume
	     *	out from underneath us.  If so, proceed with
	     *	the deletion so that fstab, xtab, etc. will be
	     *	cleaned up.
	     */
	    if(strncmp(fs_name, "/dev/xlv/", 9) == 0) {
		returnVal = xfs_fs_delete(NULL, fs_name, delete_flag,
						force_umount_flag, msg);
	    }
	    else {
		sprintf(str,gettxt(":16",
				"File system name not specified.\n"));
		add_to_buffer(msg,str);
		returnVal = 1;
	    }
	}
	else {
		returnVal = xfs_fs_delete(NULL,device_name,delete_flag,
						force_umount_flag,msg);
	}

	return(returnVal);
}


/*
 *	xfsGetDefaultsInternal(parameters, msg)
 *	
 *	The default values for the filesystem type indicated is fetched by
 *	invoking xfs_get_default_values(). The filesystem type is given by
 *	the keyword value pair - XFS_FS_TYPE.
 *
 *	The warning/error messages in msg string are of the format
 *		{message1}{message2}.....
 *
 *	Return Value:
 *		0	Success
 *		1	Failure
 *		2 	Partial success
 *
 */
int
xfsGetDefaultsInternal(const char* parameters,char** info,char **msg)
{
	char* 	fs_type=NULL;
	int	returnVal=0;
	char	*line=NULL;
	char	*key=NULL;
	char	*value=NULL;	
	char	str[BUFSIZ];

	/* Check the input parameters */
	if ((parameters == NULL) || (msg == NULL)) {
		return(1);
	}

	*msg = NULL;

	/* Parse the parameters string */
	for(line = strtok((char *)parameters, "\n");
	    line != NULL;
	    line = strtok(NULL, "\n")) {

		/* Extract the keyword and value */
		if(xfsmGetKeyValue(line, &key, &value) &&
		    (strcmp(value,"false") != 0) &&
		    (strcmp(value,"FALSE") != 0)) {
			if (strcmp(key, XFS_FS_TYPE_STR) == 0) {
				fs_type = to_lower_fs_type_str(value);
			}
		}
	}

	if (fs_type == NULL) {
		sprintf(str, gettxt(":13",
			"%s: No file system type specified.\n"), "");
		add_to_buffer(msg, str);
		returnVal = 1;
	}
	else if (is_valid_fs_type(fs_type) != 0) {
		sprintf(str, gettxt(":24",
			"%s: Unknown filesystem type.\n"), fs_type);
		add_to_buffer(msg, str);
		returnVal = 1;
	}
	else {
		returnVal = xfs_get_default_values(fs_type, info);
	}

	if (fs_type != NULL) {
		free(fs_type);
	}

	return(returnVal);
}


/*
 *      xfsGetExportInfoInternal()
 *      This function gets the information of all entries in /etc/exports.
 *
 *
 *      The warning/error messages in msg string are of the format
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 *
 */
int
xfsGetExportInfoInternal(const char* parameters, char **info, char **msg)
{
        int     returnVal = 0;
        char    str[BUFSIZ];
	struct exportent *export_entry;
	FILE	*fp;


        /* Check the input parameters */
        if ((parameters == NULL) || (info == NULL) || (msg == NULL)) {
                sprintf(str,
                "Invalid parameters passed to xfsGetExportInfoInternal()\n");
                add_to_buffer(msg, str);
		return(1);
        }

	*msg = NULL;
	*info = NULL;

	/* Read the exported directories in /etc/exports file */
	if ((fp = setexportent()) != NULL) {
		/* Read the entries in the file, adding them
		   to the fs_info_list */

		while ((export_entry = getexportent(fp)) != NULL) {
#ifdef DEBUG
printf("Export Entry %s %s\n",export_entry->xent_dirname,
				export_entry->xent_options);
#endif

			sprintf(str, "%s:%s\n", XFS_MNT_DIR_STR,
					export_entry->xent_dirname);
			add_to_buffer(info, str);
			dump_export_options(export_entry->xent_options, info);
		}

		/* Close the file */
		endexportent(fp);
        }

        return(returnVal);
}


/*
 *      xfsSimpleFsInfoInternal()
 *      This function gets simple information re: a filesystem
 *
 *      The warning/error messages in msg string are of the format
 *              {message1}{message2}.....
 *
 *      Return Value:
 *              0       Success
 *              1       Failure
 *              2       Partial success
 */
int
xfsSimpleFsInfoInternal(const char *data_in, char **data_out, char **msg)
{
        int     returnVal = 0;
	char	*line = NULL;
	char	*key = NULL;
	char	*value = NULL;	
	char	*mp;
        char    str[BUFSIZ];

	*msg = NULL;
	*data_out = NULL;

        /* Check the input parameters */
        if ((data_in == NULL) || (data_out == NULL) || (msg == NULL)) {
                sprintf(str,
                "Invalid parameters passed to xfsSimpleFsInfoInternal()\n");
                add_to_buffer(msg, str);
		return(1);
        }

	for(line = strtok((char *)data_in, "\n");
	    line != NULL;
	    line = strtok(NULL, "\n")) {

	    /* Extract the keyword and value */
	    if(xfsmGetKeyValue(line, &key, &value)) {

		if (strcmp(key, "IS_MOUNTED") == 0) {
		    break;
		} else
		if (strcmp(key, "IS_EXPORTED") == 0) {
		    break;
		} else
		if (strcmp(key, "MOUNT_POINT") == 0) {
		    if((mp = get_mnt_dir(MTAB, value)) != NULL) {
			*data_out = strdup(mp);
		    } else if((mp = get_mnt_dir(FSTAB, value)) != NULL) {
			*data_out = strdup(mp);
		    } else {
			sprintf(str, gettxt(":169",
				"%s: No /etc/fstab or /etc/mtab entry.\n"),
				value);
			add_to_buffer(msg, str);
			returnVal = 1;
		    }
		    break;
		} else
		if (strcmp(key, "NAME_TO_DEVICE") == 0) {

		    if (xfsConvertNameToDevice(value, str, B_FALSE) == B_TRUE) {
			*data_out = strdup(str);
		    }
		    else {
			sprintf(str, gettxt(":168",
			"%s: Unable to convert to device name.\n"), value);
			add_to_buffer(msg, str);
			returnVal = 1;
		    }
		    break;
		} else {
			returnVal = 1;
		}
	    }
        }

        return(returnVal);
}

