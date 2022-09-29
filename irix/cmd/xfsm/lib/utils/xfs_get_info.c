/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *	Provides information on any disk, XLV object or file system
 *	on current host to the GUI.
 */

#ident  "xfs_get_info.c: $Revision: 1.7 $"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <bstring.h>
#include <wctype.h>
#include <wsregexp.h>
#include <widec.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/uuid.h>
#include <fcntl.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>
#include <xlv_utils.h>
#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <sys/xlv_attr.h>
#include <xlv_lab.h>
#include <xlv_error.h>

#include <sys/dkio.h>
#include <sys/scsi.h>
#include <sys/termio.h>
#include <sys/buf.h>
#include <sys/iobuf.h>
#include <sys/elog.h>
#include <sys/dksc.h>

#include "xfs_fs_defs.h"
#include "xfs_query.h"
#include "xfs_xlv_query.h"
#include "xfs_disk_query_defs.h"
#include "xfs_fs_query_defs.h"
#include "xfs_info_defs.h"
#include "xfs_utils.h"
#include "xfs_get_info.h"

/*
 *	get_oref_info()
 *	Dumps the information about the xlv object given its oref.
 *	Return Value : 	0 on success
 *			1 if oref type is unknown  
 */

int
get_oref_info(xlv_oref_t *oref, char **info, char **msg)
{
	char	str[GET_OBJ_BUF_SIZE];
	int	returnVal=0;

	if (oref->obj_type == XLV_OBJ_TYPE_VOL) {
		sprintf(str, "%s:%s\n%s:%ld\n%s:%d\n",
		    XFS_INFO_VOL_NAME,  XLV_OREF_VOL(oref)->name,
		    XFS_INFO_VOL_FLAGS, XLV_OREF_VOL(oref)->flags,
		    XFS_INFO_VOL_STATE, XLV_OREF_VOL(oref)->state);
		add_to_buffer(info,str);
	}
	else if (oref->obj_type == XLV_OBJ_TYPE_SUBVOL) {
		sprintf(str,"%s:%ld\n%s:%hd\n%s:%ld\n%s:%hu\n%s:%ld\n",
		    XFS_INFO_SUBVOL_FLAGS,   XLV_OREF_SUBVOL(oref)->flags,
		    XFS_INFO_SUBVOL_TYPE,    XLV_OREF_SUBVOL(oref)->subvol_type,
		    XFS_INFO_SUBVOL_SIZE,    XLV_OREF_SUBVOL(oref)->subvol_size,
		    XFS_INFO_SUBVOL_NO_PLEX, XLV_OREF_SUBVOL(oref)->num_plexes,
		    XFS_INFO_SUBVOL_INL_RD_SLICE,
				XLV_OREF_SUBVOL(oref)->initial_read_slice);
		add_to_buffer(info,str);
	}
	else if (oref->obj_type == XLV_OBJ_TYPE_PLEX) {
		sprintf(str,"%s:%s\n%s:%ld%s:%ld%s:%ld",
		    XFS_INFO_PLEX_NAME,   XLV_OREF_PLEX(oref)->name,
		    XFS_INFO_PLEX_FLAGS,  XLV_OREF_PLEX(oref)->flags,
		    XFS_INFO_PLEX_NO_VE,  XLV_OREF_PLEX(oref)->num_vol_elmnts,
		    XFS_INFO_PLEX_MAX_VE, XLV_OREF_PLEX(oref)->max_vol_elmnts);
		add_to_buffer(info,str);
	}
	else if (oref->obj_type == XLV_OBJ_TYPE_VOL_ELMNT) {
	    sprintf(str, "%s:%s\n%s:%d\n%s:%d\n%s:%d\n%s:%hd\n%s:%ld\n%s:%ld\n",
		XFS_INFO_VE_NAME, XLV_OREF_VOL_ELMNT(oref)->veu_name,
		XFS_INFO_VE_TYPE, XLV_OREF_VOL_ELMNT(oref)->type,
		XFS_INFO_VE_STATE, XLV_OREF_VOL_ELMNT(oref)->state,
		XFS_INFO_VE_GRP_SIZE, XLV_OREF_VOL_ELMNT(oref)->grp_size,
		XFS_INFO_VE_STRIPE_UNIT_SIZE,
				XLV_OREF_VOL_ELMNT(oref)->stripe_unit_size,
		XFS_INFO_VE_START_BLK_NO,
				XLV_OREF_VOL_ELMNT(oref)->start_block_no,
		XFS_INFO_VE_END_BLK_NO,
				XLV_OREF_VOL_ELMNT(oref)->end_block_no);
	    add_to_buffer(info,str);
	}	
	else {
		sprintf(str,gettxt(":142",
		"xlv object reference type %d unknown\n"), oref->obj_type);
		add_to_buffer(msg,str);
		returnVal = 1;
	}

	return(returnVal);			
}

/*
 *	get_vol_info()
 *	Dumps the information about the specified xlv object to the buffer.
 *	Return Value : 	0 on success
 *			1 on failure
 *			2 on partial failure
 */

int
get_vol_info(char *xlv_obj_name, char **info, char **msg)
{
	query_set_t	*xlv_set = NULL;
	query_set_t	*xlv_obj = NULL;
	char		str[GET_OBJ_BUF_SIZE];
	int		returnVal = 1;

	if (xlv_obj_name != NULL)
	{
		create_xlv_in_set(&xlv_set);
		xlv_obj = xlv_set;
		while (xlv_obj != NULL)
		{
			if (xlv_obj->type == XLV_TYPE_QUERY)
			{
				set_xlv_obj_name(&xlv_obj);
				if (strcmp(xlv_obj->value.xlv_obj_ref->name,
						xlv_obj_name) == 0)
				{
					returnVal = get_oref_info(
					    xlv_obj->value.xlv_obj_ref->oref,
							info, msg);
				}
			}
			xlv_obj = xlv_obj->next;
		}
		delete_in_set(&xlv_set);

		if (returnVal == 1)
		{
			sprintf(str,gettxt(":143","XLV object %s not found\n"),
					xlv_obj_name);
			add_to_buffer(msg,str);
		}

	}

	return(returnVal);
}

/*
 *	btos()
 *	Conversion routine - bytes to sector 
 */

uint
btos(struct volume_header* vh,uint n)
{
        return (n + vh->vh_dp.dp_secbytes - 1) / vh->vh_dp.dp_secbytes;
}

/*
 *	cylof()
 *	Determines cylinder number from address.
 */

uint
cylof(struct volume_header* vh,daddr_t bn)
{
		return bn; /*OLSON: totally bogus, but tcl code uses */
}



/*
 *	blockof()
 *	Determines the block given the cylinder, head & sector information.
 */

uint
blockof(struct volume_header* vh,uint cyl, uint hd, uint sec)
{
        return cyl*hd*sec;	/* OLSON: semi-bogus, but tcl code uses */
}



/*
 *	gread()
 * 	read from disk at block bn, with retries
 *
 * 	For non ESDI/SMD, don't retry on multiple block reads or report errors
 * 	or add them to the error log if we are exercising the
 * 	disk AND this is a multiple block read.  The exercisor
 * 	will retry each block separately when it gets errors
 * 	on multiple block reads.  Otherwise we confuse the
 * 	user by reporting errors as the first block of the read, with
 * 	some drives and firmware revs.
 * 	For ESDI & SMD, though, we do NOT do that, because they only spare
 * 	by mapping whole tracks.
 */

int
gread(char* disk_name,struct volume_header* vh,daddr_t bn,void *buf, uint count)
{
        int i, cnt, syscnt, doreadb;
        daddr_t blk;
        int secsz = (vh->vh_dp).dp_secbytes;
	int ioretries=3;
	int fd;
	uint drivernum=SCSI_NUMBER; /* Initialize just for now !!! */

	if ((fd = open(disk_name,2)) == -1)
	{
		return(-1);
	}

        /* have to avoid overflow on count, so handle sector
         * sizes that are not a multiple of BBSIZE by doing
         * old style reads, but only if within range.
         * This is primarily to handle things like floppies, and
         * perhaps some raid disks, which might have (individual) disks
         * with a logical block size < BBSIZE.
         */
        if(secsz%BBSIZE) {
            if((0x80000000u/secsz) <= bn) {
                errno = 0; /* prevent bogus error message */
                return -1;
            }
            syscnt = count * secsz;
            blk = bn * secsz;
            doreadb = 0;
        }
        else  {
            blk = bn * (secsz/BBSIZE);
            syscnt = count;
            doreadb = 1;
        }

        for(i = 0; i <= ioretries; i++) {
            if(doreadb)
                cnt = readb(fd, buf, blk, syscnt);
            else {
                if(lseek(fd,(long)blk,0) == -1) {
                        /* should have some way to suppress trying to add
                         * a bad block for this one, but with the check above
                         * for out of range, this should be quite rare... */
                        return -1;
                }
                cnt = read(fd, buf, syscnt);
            }
            if(cnt == syscnt)
                return 0;

            if(cnt == -1) {
                if(count != 1)
					break;  /* scanchunk retries sector by sector, and
                                 * reports error */
            }
            else {
                const char *s =
                        "Tried to read %d blocks, only read %d at %s\n";
            }
        }

	close(fd);
        return -1;
}


/*	sc_bytes_sh()
 *	Used to convert all the two byte sequences from scsi modesense, etc.
 * 	to shorts for printing, etc. 
 */

static ushort
sc_bytes_sh(u_char *a)
{
        return (((ushort)a[0])<<8) + (ushort)a[1];
}

/* 	sc_3bytes_uint()
 *	Used to convert all the 3 byte sequences from scsi modesense, etc.
 * 	to uints for printing, etc. 
 */

static uint
sc_3bytes_uint(u_char *a)
{
        return (((uint)a[0])<<16) + (((uint)a[1])<<8) +  (uint)a[2];
}

/* 	sc_bytes_uint()
 *	Used to convert all the 4 byte sequences from scsi modesense, etc.
 * 	to uints for printing, etc. 
 */

static uint
sc_bytes_uint(u_char *a)
{
        return (((uint)a[0])<<24) + (((uint)a[1])<<16) + (((uint)a[2])<<8)
                + (uint)a[3];
}

/*
 *	gioctl()
 * 	ioctl on the disk, with error reporting
 */

int
gioctl(char* disk_name,int cmd, void *ptr)
{
	int fd;
	int retValue;

	if ((fd = open(disk_name,2)) == -1)
	{
		return(-1);
	}

        errno = 0;      /* for scerrwarn and err_fmt calls from caller */
	retValue = ioctl(fd, cmd, ptr);
	close(fd);

        return(retValue); 
}

/*
 *	get_scsidefects()
 * 	Get the current defect list.
 * 	Type is the type of defect list that should be retrieved.
 */

static
get_scsidefects(struct defect_list *addr, uint len, u_char def_type,char* disk_name)
{
        struct dk_ioctl_data getdefects_data;

        getdefects_data.i_addr = (caddr_t)addr;
        getdefects_data.i_len = len;
        getdefects_data.i_page = def_type;

        if(gioctl(disk_name,DIOCRDEFECTS, &getdefects_data) < 0) {
                /* the 3.3 fx is being used on 3.2, so don't report */
                return -1;
        }
        return 0;
}

/* 
 *	get_scsi_badblks()
	if log is non-zero, defects are shown as logical, not physical.
	Unfortunately, some mfg (such as Toshiba) return the phys
	format, even if you request log (which is ok by std), BUT
	the returned fmt type is log, NOT phys (which is NOT ok by std).
	Given this situation, for now we will always display in 
	phys (cyl/head/sec) format, even though we only allow user to
	spare by logical blkno.  This should be ok, as long as someone
	doesn't do the math and file a bug report about the two not 
	matching (which they usually won't, since the logical blkno
	skips reserved tracks and spared sectors, etc.)
	See showbb_func() in bb.c

	To print just grown, we try that first, and if it fails, we do
	it by getting all, then mfg, the eliminate mfg from entire
	list.  This is needed because many mfg's don't return just
	grown list.
*/

char*
get_scsi_badblks(int logical, int justmfg,char* disk_name)
{
	struct defect_list def_list;
	uint defect_cnt;
	int rtype, type, cntdiv;
	int index;
	char	*bad_blk_list_str=NULL;
	char	str[BUFSIZ];
	int	found;
	int	iter;

	/*** NOTE: logical won't work right for Toshiba, and maybe others ***/
	if(logical == 0)
		type = 5;
	else if(logical == 1)
		type = 0;
	else if(logical == -1)
		type = 4;
	else {
		type = 5;	/* physical supported by the most mfg */
	}


	if(justmfg)
		type |= 2<<3;
	else
		type |= 3<<3;
	if (get_scsidefects(&def_list, sizeof(def_list), type, disk_name))
		return(NULL);

	/* 	Check return type because drives (at least CDC Wren,
		and HItachi 514C) return their default format with
		a recovered error if given a type they don't support. */
	rtype = def_list.defect_hdr.format_bits;
	if(rtype != type) {
		printf("drive doesn't support requested badblock format\n");
		if(!(rtype&0x4))
			logical = 1;	/* logical block */
		else if((rtype&7) == 5)
			logical = 0;	/* physical: cyl/head/sec */
		else if((rtype&7) == 4)
			logical = -1;	/* byte from index: cyl/head/bytes */
		else {
			printf("returned type is a 'reserved' type, can't handle it\n");
			return(NULL);
		}
	}
	cntdiv = logical==1 ? sizeof(daddr_t) : sizeof(struct defect_entry);

	defect_cnt = sc_bytes_sh(def_list.defect_hdr.defect_listlen);
	defect_cnt /= cntdiv;
	if((defect_cnt*cntdiv) > sizeof(def_list)) {
#ifdef DEBUG
		printf("claimed size of list (%d) is greater than requested, "
		       "probably not valid\n", defect_cnt);
#endif
		defect_cnt = sizeof(def_list) / cntdiv;
	}

	for (index=0;index < defect_cnt; index++)
	{
		/* Check for duplicate bad block entries */
		found = 1;
		iter = 0;
		while (iter < index)
		{
			if ((sc_3bytes_uint(def_list.defect[iter].def_cyl) ==
			    sc_3bytes_uint(def_list.defect[index].def_cyl)) &&
			   (def_list.defect[iter].def_head ==
			    def_list.defect[index].def_head) &&
			   (sc_bytes_uint(def_list.defect[iter].def_sector) ==
			    sc_bytes_uint(def_list.defect[index].def_sector)))
			{
				found = 0;
			}

			iter++;
		}

		if (found == 1)
		{
			sprintf(str,"%d/%d/%d ",
				sc_3bytes_uint(def_list.defect[index].def_cyl),
				def_list.defect[index].def_head,
			sc_bytes_uint(def_list.defect[index].def_sector));
			add_to_buffer(&bad_blk_list_str,str);
		}
	}

	return(bad_blk_list_str);

}


/*
 *	get_disk_info()
 *	Dumps the information about the specified disk to the buffer.
 *	Return Value : 	0 on success
 *			1 on failure
 *			2 on partial failure
 */

int
get_disk_info(char *disk_name, char **info, char **msg)
{
	int			returnVal = 0;
	char			str[BUFSIZ];
	char			*value = NULL;
	char			*dk_str = NULL;
	xlv_vh_entry_t		*vh_entry;
	xlv_tab_disk_part_t	*dp = NULL;
	int			secbytes;
	long long		mbsize;

	if(disk_name == NULL) {
		sprintf(str, gettxt(":144",
			"get_disk_info: Bad value: disk_name == NULL.\n"));
		add_to_buffer(msg, str);
		return 1;
	}

	vh_entry = (xlv_vh_entry_t*) malloc(sizeof(xlv_vh_entry_t));
	assert(vh_entry!=NULL);
	bzero(vh_entry, sizeof(xlv_vh_entry_t));
		
	if (xlv_lab1_open(disk_name, dp, vh_entry))
	{
		/* Unable to read disk header */
		sprintf(str, gettxt(":144",
			"%s: Error reading disk header.\n"), disk_name);
		add_to_buffer(msg, str);
		return 1;
	}

	/* Skip over the path to retrieve the disk name */
	if((dk_str = strrchr(disk_name, '/')) == NULL) {
		dk_str = disk_name;
	}
	else {
		dk_str++;
	}

	/* Infer the type of the disk by comparing the 
	 * disk_name with known disk types.
	 */
	if (dk_str != NULL) {
		if(strncmp(dk_str, XFS_INFO_SCSIDRIVE, 3) == 0) {
			value = XFS_INFO_DISK_SCSI;
		}
		else if(strncmp(dk_str, XFS_INFO_VSCSIDRIVE, 3) == 0) {
			value = XFS_INFO_DISK_VSCSI;
		}
		else if(strncmp(dk_str, XFS_INFO_SCSIRAIDDRIVE, 3) == 0) {
			value = XFS_INFO_DISK_SCSIRAID;
		}

		if(value != NULL) {
			sprintf(str, "%s:%s\n", XFS_INFO_DISK_TYPE, value);
			add_to_buffer(info, str);
		}
	}

	/*
	 *	This "if" was stolen from libdisk
	 */
	if((secbytes = vh_entry->vh.vh_dp.dp_secbytes) == 0) {
		secbytes = NBPSCTR;
	}
	mbsize = ((long long) vh_entry->vh.vh_dp.dp_drivecap *
		  (long long) secbytes) /
		  (long long) (1<<20);

	sprintf(str,
		"%s:%s\n%s:%d\n%s:%hu\n%s:%hu\n%s:%hu\n%s:%u\n%s:%d\n%s:%u\n",
		XFS_INFO_DISK_NAME,	    disk_name,
		XFS_INFO_DISK_SPARES_CYL,   0 /*OLSON */,
		XFS_INFO_DISK_CYLS,	    0 /*OLSON */,
		XFS_INFO_DISK_TRKS_PER_CYL, 0 /*OLSON */,
		XFS_INFO_DISK_SECS_PER_TRK, 0 /*OLSON */,
		XFS_INFO_DISK_DRIVECAP,	    vh_entry->vh.vh_dp.dp_drivecap,
		XFS_INFO_DISK_SEC_LEN,	    secbytes,
		XFS_INFO_DISK_MBSIZE,	    (uint) mbsize);
	add_to_buffer(info, str);

	/*OLSON: this is bogus; this is for dp_flags!!!!!
	case DP_CTQ_EN :
		value = XFS_INFO_DISK_INTERLV_DP_CTQ_EN;
		break;

	if(value != NULL) {
		sprintf(str, "%s:%s\n", XFS_INFO_DISK_INTERLV_FLAG, value);
		add_to_buffer(info, str);
	}
		OLSON*/

	xlv_lab1_close(&vh_entry);

	return(returnVal);
}

/*
 *	lookup_option_str()
 *	This function looks up the option string in the option table and 
 *	generates the appropriate keyword value pair. First the option
 *	string is matched against the default values for the different
 *	options in the option table. If no match is found, it is matched
 *	with the alternate option values and finally the regular options
 *	string. This process is exactly the reverse of generating the 
 *	filesystem option string from the keyword value pairs.
 *	The generated keyword value pair is added to the buffer.
 *	If the option string is found in the table, a 0 is returned. Otherwise,
 *	a 1 is returned.
 */

int
lookup_option_str(char* option_str,
		  struct fs_option options_tab[],
		  int options_tab_len,
		  char** buffer)
{
	char	str[BUFSIZ];
	int	index;
	char	*value;
	int	len;

	if(option_str == NULL || option_str[0] == '\0') {
		return(1);
	}

	for(index = 0; index < options_tab_len; ++index) {
	    /* Is it a value corresponding to "true" boolean condition ? */
	    if ((options_tab[index].true_bool_opts_str != NULL) &&
		(strcmp(option_str, options_tab[index].true_bool_opts_str)==0))
	    {
			sprintf(str,"%s:TRUE\n", options_tab[index].str);
			add_to_buffer(buffer,str);
			return(0);
	    }

	    /* Is it a value corresponding to "false" boolean condition ? */
	    else if ((options_tab[index].false_bool_opts_str != NULL) &&
		 (strcmp(option_str,options_tab[index].false_bool_opts_str)==0))
	    {
			sprintf(str,"%s:FALSE\n", options_tab[index].str);
			add_to_buffer(buffer,str);
			return(0);
	    }

	    /* Is it a regular option with a possible value? */
	    else if (options_tab[index].opts_str != NULL) {
		/* Find the length of regular options string
		 * to compare. By default it is the entire 
		 * length of the options string
		 *
		 * Example to compare "raw=%s" and "raw=/dev/a"
		 * only the first three characters are sufficient
		 */

		len = strcspn(options_tab[index].opts_str,"=");

		if (strncmp(option_str,options_tab[index].opts_str,len) == 0) {
		    if (len == strlen(options_tab[index].opts_str)) {
			sprintf(str,"%s:TRUE\n", options_tab[index].str);
			add_to_buffer(buffer, str);
			return(0);
		    }
		    else {
			/* Extract the value part */
			strtok(option_str,"=");
			value=strtok(NULL,"=");
			if (value == NULL) {
			    sprintf(str,"%s:%s\n", options_tab[index].str,
						"NULL");
			}
			else {
			    sprintf(str,"%s:%s\n", options_tab[index].str,
					value);
			}
			add_to_buffer(buffer,str);
			return(0);
		    }
		}
	    }
	}

	return(1);
}

/*
 *	dump_export_options()
 *	Splits the export options string into auboptions and dumps
 *	them as individual keyword, value pairs.
 */

void
dump_export_options(char* export_opts_str,char** info)
{
	char	*dup_export_opts_str=NULL;
	char	*token;

#ifdef	DEBUG
	printf("options_str: -%s-\n", export_opts_str);
#endif

	if (export_opts_str != NULL)
	{
		/* Make a copy of the export options string */
		if ((dup_export_opts_str = strdup(export_opts_str)) != NULL)
		{
			for(token = strtok(dup_export_opts_str,",");
			    token != NULL;
			    token = strtok(NULL, ",")) {
				/* Look for export options */
				lookup_option_str(token,
						xfs_export_opts_tab,
						XFS_EXPORT_OPTS_TAB_LEN,
						info);
			}

			free(dup_export_opts_str);
		}
	}
}

/*
 *	dump_fs_options()
 *	Splits the filesystem options string into auboptions and dumps
 *	them as individual keyword, value pairs.
 */

void
dump_fs_options(char* fs_type,char* fs_options_str,char** info)
{
	char 	*dup_fs_options_str;
	char 	*token;
	int	found;

	if (fs_options_str != NULL)
	{
		/* Make a copy of the filesystem options string */
		if ((dup_fs_options_str = strdup(fs_options_str)) != NULL)
		{
			for(token = strtok(dup_fs_options_str, ",");
			    token != NULL;
			    token = strtok(NULL, ",")) {

				/* Look for general options */
				found = lookup_option_str(token,
						generic_fs_options,
						GENERIC_FS_OPTS_TAB_LEN,
						info);

				/* Look for options specific to the filesystem*/
				if (found != 0)
				{
					if (strcmp(fs_type,EFS_FS_TYPE)==0)
					{
						lookup_option_str(token,
							efs_fs_options,
							EFS_FS_OPTS_TAB_LEN,
							info);

					}
					else if (strcmp(fs_type,XFS_FS_TYPE)==0)
					{
						lookup_option_str(token,
							xfs_fs_options,
							XFS_FS_OPTS_TAB_LEN,
							info);
					}
					else if (strcmp(fs_type,ISO9660_FS_TYPE)==0)
					{
						lookup_option_str(token,
							iso9660_fs_options,
							ISO9660_FS_OPTS_TAB_LEN,
							info);
					}
					else if (strcmp(fs_type,NFS_FS_TYPE)==0)
					{
						lookup_option_str(token,
							nfs_fs_options,
							NFS_FS_OPTS_TAB_LEN,
							info);
					}
					else if (strcmp(fs_type,SWAP_FS_TYPE)==0)
					{
						lookup_option_str(token,
							swap_fs_options,
							SWAP_FS_OPTS_TAB_LEN,
							info);
					}
					else if (strcmp(fs_type,CACHE_FS_TYPE)==0)
					{
						lookup_option_str(token,
							cache_fs_options,
							CACHE_FS_OPTS_TAB_LEN,
							info);
					}
				}
			}

			free(dup_fs_options_str);
		}
	}
}
		

/*
 *	get_fs_info()
 *	Dumps the information about the specified file system to the
 *	buffer.
 *	Return Value : 	0 on success
 *			1 on failure
 *			2 on partial failure
 */

int
get_fs_info(char *fs_name, char **info, char **msg)
{
	fs_info_entry_t*	fs_list=NULL;
	fs_info_entry_t*	fsentry=NULL;
	int 			returnVal = 1;
	char			device_name[PATH_MAX + 1];
	char			str[GET_OBJ_BUF_SIZE];

	/* Transform fs_name */
	if(xfsConvertNameToDevice(fs_name, device_name, B_FALSE) == B_FALSE) {
		strcpy(device_name, fs_name);
	}

	fs_list = xfs_info((char*)NULL, msg);

	fsentry = fs_list;
	while (fsentry != NULL) {
	    if((fsentry->fs_entry != NULL) &&
	       (fsentry->fs_entry->mnt_fsname != NULL) &&
		strcmp(device_name, fsentry->fs_entry->mnt_fsname) == 0) {

		returnVal = 0;
		sprintf(str, "%s:%s\n%s:%s\n%s:%s\n%s:%s\n%s:%d\n%s:%d\n",
			XFS_INFO_FS_NAME,        fsentry->fs_entry->mnt_fsname,
			XFS_INFO_FS_PATH_PREFIX, fsentry->fs_entry->mnt_dir,
			XFS_INFO_FS_TYPE,        fsentry->fs_entry->mnt_type,
			XFS_INFO_FS_OPTIONS,     fsentry->fs_entry->mnt_opts,
			XFS_INFO_FS_DUMP_FREQ,   fsentry->fs_entry->mnt_freq,
			XFS_INFO_FS_PASS_NO,     fsentry->fs_entry->mnt_passno);
		add_to_buffer(info, str);

		dump_fs_options(fsentry->fs_entry->mnt_type,
				fsentry->fs_entry->mnt_opts,
				info);

		/* Dump the mount information */
		if (fsentry->mount_entry!=NULL) {
		    sprintf(str, "%s:%s\n%s:%s\n%s:%s\n%s:%s\n%s:%d\n%s:%d\n",
			XFS_INFO_MNT_FS_NAME, fsentry->mount_entry->mnt_fsname,
			XFS_INFO_MNT_DIR,     fsentry->mount_entry->mnt_dir,
			XFS_INFO_MNT_TYPE,    fsentry->mount_entry->mnt_type,
			XFS_INFO_MNT_OPTIONS, fsentry->mount_entry->mnt_opts,
			XFS_INFO_MNT_FREQ,    fsentry->mount_entry->mnt_freq,
			XFS_INFO_MNT_PASSNO,  fsentry->mount_entry->mnt_passno);
		    add_to_buffer(info,str);

		    dump_fs_options(fsentry->mount_entry->mnt_type,
				    fsentry->mount_entry->mnt_opts,
				    info);
		}

		/* Dump the export information */
		if (fsentry->export_entry != NULL) {
		    sprintf(str, "%s:%s\n%s:%s\n",
			XFS_INFO_EXP_DIR, fsentry->export_entry->xent_dirname,
			XFS_INFO_EXP_OPTIONS,
			fsentry->export_entry->xent_options != NULL ?
				fsentry->export_entry->xent_options : "");
		    add_to_buffer(info,str);
		    if(fsentry->export_entry->xent_options) {
		        dump_export_options(
				fsentry->export_entry->xent_options,
				info);
		    }
		}
		break;
	    }
	    else if ((fsentry->mount_entry!=NULL) &&
		     (fsentry->mount_entry->mnt_fsname!=NULL) &&
	             (strcmp(device_name,fsentry->mount_entry->mnt_fsname)==0))
	    {
		returnVal = 0;

		sprintf(str, "%s:%s\n%s:%s\n%s:%s\n%s:%d\n%s:%d\n",
			XFS_INFO_MNT_FS_NAME, fsentry->mount_entry->mnt_fsname,
			XFS_INFO_MNT_DIR,     fsentry->mount_entry->mnt_dir,
			XFS_INFO_MNT_TYPE,    fsentry->mount_entry->mnt_type,
			XFS_INFO_MNT_FREQ,    fsentry->mount_entry->mnt_freq,
			XFS_INFO_MNT_PASSNO,  fsentry->mount_entry->mnt_passno);
		add_to_buffer(info,str);

#ifdef MOUNT_INFO
		sprintf(str,"%s:%s\n",XFS_INFO_MNT_OPTIONS,
			fsentry->mount_entry->mnt_opts);
		add_to_buffer(info,str);
#endif

		dump_fs_options(
			fsentry->mount_entry->mnt_type,
			fsentry->mount_entry->mnt_opts,
			info);

		break;
	    }

	    fsentry = fsentry->next;
	}

	/* Release the memory allocated for fs_list */
	fsentry = fs_list;
	while (fs_list != NULL)
	{
		fsentry = fs_list->next;
		delete_xfs_info_entry(fs_list);
		fs_list = fsentry;
	}

	if (returnVal == 1) {
		sprintf(str,gettxt(":145",
				"%s: File system not found.\n"),device_name);
		add_to_buffer(msg,str);
	}

	return(returnVal);
} 



/*
 *	getInfoInternal(objectId, info, msg)
 *
 *	This routine returns information on the XFS object identified by
 *	objectId. The object may be a disk, filesystem or XLV object.
 *	Any warnings/error messages are stored in msg.
 *	
 *	The format of objectId is shown below.
 *		{hostname obj_type obj_name subtype}
 *
 *	The information about the object is stored as a series of keyword 
 *	value pairs with newline as seperator. The keyword is the attribute
 *	id and its corresponding immediately follows it.
 *		DISK_NAME:/dev/rdsk/dev012s0p1
 *
 *	The warning/error messages in msg string are of the format
 *		{message1}{message2}.....
 *
 *	Return Value:
 *		0	success
 *		1	failure
 *		2	partial success
 *
 */

int
getInfoInternal(const char *objectId, char **info, char **msg)
{
	int 	returnVal = 0;
	char	*hostname;
	char	*obj_name;
	char	*obj_type;
	char	*obj_sub_type;

	char str[BUFSIZ];

	/* Check the input parameters */
	if ((objectId == NULL) || (info == NULL) || (msg == NULL))
	{
		sprintf(str,gettxt(":146",
				"Invalid parameters for getInfoInternal()\n"));
		add_to_buffer(msg,str);
		returnVal = 1;
	}
	else
	{
		/* Initialize the objs and msg buffers */
		*info = NULL;
		*msg = NULL;

		/* Parse the objectId to extract the hostname,
		   obj_name, obj_type and obj_sub_type */
		hostname = strtok((char*)objectId,"{ ");
		obj_type = strtok(NULL,"{ ");
		obj_name = strtok(NULL,"{ ");
		obj_sub_type = strtok(NULL,"{ ");
		

		/* Check if the object type and name are known */
		if (obj_type == NULL) 
		{
			sprintf(str,gettxt(":147",
				"Object Type incorrectly specified\n"));
			add_to_buffer(msg,str);
			returnVal = 1;
		}
		else if (obj_name == NULL)
		{
			sprintf(str,gettxt(":148",
				"Object Name incorrectly specified\n"));
			add_to_buffer(msg,str);
			returnVal = 1;
		}
		else if (obj_sub_type == NULL)
		{
			sprintf(str,gettxt(":149",
				"Object Subtype incorrectly specified\n"));
			add_to_buffer(msg,str);
			returnVal = 1;
		}
		else
		{
			/* Get the information based on the obj_type */
			if (strcmp(obj_type,FS_TYPE)==0)
			{
				returnVal = get_fs_info(obj_name, info, msg);
			}
			else if (strcmp(obj_type,DISK_TYPE)==0)
			{
				returnVal = get_disk_info(obj_name, info, msg);
			}
			else if (strcmp(obj_type,VOL_TYPE)==0)
			{
				returnVal = get_vol_info(obj_name, info, msg);
			}
			else
			{
				sprintf(str,gettxt(":150",
					"Unknown Object Type %s\n"),obj_type);
				add_to_buffer(msg,str);
				returnVal = 1;
			}
		}
	}

	return(returnVal);
}

int
xfsGetBBlkInternal(const char *objectId, char **info, char **msg)
{
	int			returnVal = 0;
	char			str[BUFSIZ];
	char			*dk_str = NULL;
	char			*bblks = NULL;
	xlv_vh_entry_t		*vh_entry;
	xlv_tab_disk_part_t	*dp = NULL;

	char			o_name [256];
	char			o_class[32];

	if ((objectId == NULL) || (info == NULL) || (msg == NULL)) {
		sprintf(str, gettxt(":146",
			"Invalid parameters for xfsGetBBlkInternal()\n"));
		add_to_buffer(msg, str);
		return(1);
	}

	if(xfsParseObjectSignature(DISK_TYPE, objectId, NULL, o_name,
					o_class, NULL, msg) == B_FALSE) {
		return(1);
	}

	vh_entry = (xlv_vh_entry_t*) malloc(sizeof(xlv_vh_entry_t));
	assert(vh_entry!=NULL);
	bzero(vh_entry, sizeof(xlv_vh_entry_t));

	if(xlv_lab1_open(o_name, dp, vh_entry))
	{
		/* Unable to read disk header */
		sprintf(str, gettxt(":144",
			"%s: Error reading disk header.\n"), o_name);
		add_to_buffer(msg, str);
		return 1;
	}

	/* Skip over the path to retrieve the disk name */
	if((dk_str = strrchr(o_name, '/')) == NULL) {
		dk_str = o_name;
	}
	else {
		dk_str++;
	}

	/* Infer the type of the disk by comparing the 
	 * disk_name with known disk types.
	 */
	if(strncmp(dk_str, XFS_INFO_SCSIDRIVE, 3) == 0) {
		bblks = get_scsi_badblks(0, 0, o_name);
	}
	else if(strncmp(dk_str, XFS_INFO_VSCSIDRIVE, 3) == 0) {
		/* HUH?  This should be the same as the above... */
	}
	else if(strncmp(dk_str, XFS_INFO_SCSIRAIDDRIVE, 3) == 0) {
	}

	if(bblks != NULL) {
		sprintf(str, "%s:", XFS_INFO_DISK_BBLK);
		add_to_buffer(info, str);
		add_to_buffer(info, bblks);
		add_to_buffer(info, "\n");
		free(bblks);
	}

	return 0;
}
