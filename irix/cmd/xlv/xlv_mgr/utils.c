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
#ident "$Revision: 1.24 $"

#include <errno.h>
#include <bstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#include <sys/syssgi.h>
#include <sys/buf.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_error.h>
#include <xlv_oref.h>
#include <xlv_utils.h>
#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <sys/xlv_attr.h>
#include <sys/xlv_stat.h>
#include <tcl.h>
#include <xlv_lab.h>
#include <xlv_lower_admin.h>
#include <xlv_cap.h>
#include "xlv_mgr.h"

extern int xlv_oref_to_xlv_make_cmds(FILE *sink, xlv_oref_t *oref);


/*
 * Translate a number or name to a subvolume index number.
 * Return 1 iff the complete name string is translated to a number,
 * otherwise return 0.  Don't want partial translation.
 */
int
_name_to_subvol(char *name, int *sv_number)
{
	xlv_mgr_cursor_t	cursor;	
	int			retval;
	char			*end;

	/* check if this is a subvolume number */
	*sv_number = strtol(name, &end, 0);
	if (end == name || end[0] != '\0') {
		/*
		 * this must be a named subvolume
		 */
		cursor.oref = NULL;
		retval = parse_name(name, XLV_OBJ_TYPE_SUBVOL, &cursor);
		if (retval != XLV_MGR_STATUS_OKAY || !cursor.oref
		    || !XLV_OREF_SUBVOL(cursor.oref)) {
			return(0);
		}
		*sv_number = minor(XLV_OREF_SUBVOL(cursor.oref)->dev);
		free(cursor.oref);
	}
	return(1);
}


/*
 * Display the error message associated with each xlv error.
 */
void
print_error(int xlv_err)
{
	switch(xlv_err) {

	case XLV_LOWER_ESYSERRNO:
		fprintf (stderr,
	"Failed in xlv_lower with errno (%d): %s\n"
	"Please execute the \"reset\" command as a safety measure.\n",
		errno, strerror(errno));
		break;

	case XLV_LOWER_EINVALID:
		fprintf (stderr,
	"Failed in xlv_lower with invalid argument\n"
	"Please execute the \"reset\" command as a safety measure.\n");
		break;

	case XLV_LOWER_EIO:
		fprintf (stderr,
	"Failed in xlv_lower with I/O error.\n"
	"Please execute the \"reset\" command as a safety measure.\n");
		break;

	case XLV_LOWER_EKERNEL:
		fprintf (stderr,
	"Failed in xlv_lower with kernel error (%d): %s\n"
	"Please execute the \"reset\" command as a safety measure.\n",
		errno, strerror(errno));
		break;

	case XLV_LOWER_ENOSUPPORT:
		fprintf (stderr,
	"Failed in xlv_lower with unsupported operation.\n"
	"Please execute the \"reset\" command as a safety measure.\n");
		break;

	case XLV_LOWER_ENOIMPL:
		fprintf (stderr,
	"Failed in xlv_lower with unimplemented operation.\n"
	"Please execute the \"reset\" command as a safety measure.\n");
		break;

	case XLV_LOWER_ESTALECURSOR:
		fprintf (stderr,
	"Failed in xlv_lower with stale cursor when attempting to change\n"
	"an object. Another user has modified the XLV configuration since\n"
	"this tool was invoked. Please execute the \"reset\" command as a\n"
	"safety measure.\n");
		break;

	case XLV_STATUS_TOO_MANY_PLEXES:
		fprintf (stderr, "Error: too many plexes\n");
		break;

	case RM_SUBVOL_CRTVOL:
		fprintf (stderr, "Error RM_SUBVOL_CRTVOL\n");
		break;

	case RM_SUBVOL_NOVOL:
		fprintf (stderr,
	"Requested subvolume does not exist for this volume.\n");
		break;

	case RM_SUBVOL_NOSUBVOL:
		fprintf (stderr, "RM_SUBVOL_NOSUBVOL\n");
		break;

	case DEL_PLEX_BAD_TYPE:
		fprintf (stderr, "DEL_PLEX_BAD_TYPE\n");
		break;

	case ADD_NOVOL:
		fprintf (stderr, "ADD_NOVOL\n");
		break;

	case DEL_VOL_BAD_TYPE:
		fprintf (stderr, "DEL_VOL_BAD_TYPE\n");
		break;

	case SUBVOL_NO_EXIST:
		fprintf (stderr, "Requested subvolume does not exist.\n");
		break;

	case MALLOC_FAILED:
		fprintf (stderr, "The call to malloc failed.\n");
		break;

	case INVALID_SUBVOL:
		fprintf (stderr,
	"An unknown type of subvolume was requested.\n"
	"Known types are log, data, and rt.\n");
		break;

#if 0
	case INVALID_OPER_ENTITY:
		fprintf (stderr, "INVALID_OPER_ENTITY\n");
		break;
#endif

	case PLEX_NO_EXIST:
		fprintf (stderr,
	"Requested plex does not exist for this volume/subvolume.\n");
		break;

	case LAST_SUBVOL:
		fprintf (stderr, "LAST_SUBVOL\n");
		break;

	case LAST_PLEX:
		fprintf (stderr,
	"An attempt was made to modify the last plex within the subvolume.\n"
	"This is not a legal operation. The only valid ways to remove space\n"
	"from the last plex are:\n"
	"1) delete the entire volume, or\n"
	"2) add a new plex to the volume, wait for XLV to copy the data to\n"
	"   the new plex, and then remove the original plex or ve.\n");
		break;

	case INVALID_PLEX:
		fprintf (stderr,
	"An attempt was made to add a plex that contained\n"
	"volume elements which did not match the volume elements\n"
	"of the corresponding plexes within the subvolume.\n");
		break;

	case INVALID_VE:
		fprintf (stderr, "INVALID_VE\n");
		break;

	case LAST_VE:
		fprintf (stderr,
	"An attempt was made to remove or detach the last\n"
	"volume element of a plex. This is not a valid operation.\n"
	"One must remove / detach the plex in this instance.\n");
		break;

	case INVALID_OPER_ENTITY:
		fprintf (stderr,
	"An object of inappropriate type was selected for this operation.\n");
		break;

	case ENO_SUCH_ENT:
		fprintf (stderr, "Requested object was not found.\n");
		break;

	case VE_FAILED:
		fprintf (stderr, "VE_FAILED\n");
		break;

	case ADD_NO_VE:
		fprintf (stderr, "ADD_NO_VE\n");
		break;

	case E_NO_PCOVER:
		fprintf (stderr,
	"The range of disk blocks covered by this plex is either not\n"
	"covered by volume elements in another plex OR the volume elements\n"
	"in these plexes are not in the active state. You can only remove\n"
	"or detach a plex if either of these conditions hold. The\n"
	"\"show object\" command will show the range of disk blocks\n"
	"covered by a plex. It will also show its state. If the range\n"
	"of disk blocks being removed is not covered by another plex, add\n"
	"new plexes or ve's until it is covered and then remove the plex.\n"
	"If the range of disk blocks being removed is covered by another\n"
	"plex but the ve's are not in the active state, then run\n"
	"xlv_assemble(1m) to make them active. If xlv_assemble(1m) has\n"
	"been run, then simply wait for the plexes to move to an active\n"
	"state.  A \"ps -elf | grep xlv\" will show the plex revive\n"
	"taking place. Once the volume elements have all moved to an\n"
	"active state, you may remove or detach the plex.\n");
		break;

	case E_VE_SPACE_NOT_CVRD:
		fprintf (stderr,
	"The range of disk blocks covered by this ve is not covered by\n"
	"another ve. You must first cover this range of disk blocks with\n"
	"another plex or ve before removing this ve. If you want to delete\n"
	"the volume then execute \"delete object\" command.\n");
		break;

	case E_VE_END_NO_MATCH:
		fprintf (stderr,
	"Ve's within a plex must be of equal size.\n");
		break;

	case E_VE_GAP:
		fprintf (stderr,
	"VE index exceeds number of ve's plus one;"
	" this would cause an invalid gap.\n");
		break;

	case E_VE_MTCH_BAD_STATE:
		fprintf (stderr,
	"The corresponding ve's in the other plexes are not in the active\n"
	"state. Please run xlv_assemble(1m).\n");
		break;

	case VE_NO_EXIST:
		fprintf (stderr,
	"An attempt was made to remove or detach a non-existant ve.\n"
	"Execute the \"show object\" command to view the object.\n");
		break;

	case E_VOL_STATE_MISS_UNIQUE:
		fprintf (stderr,
	"Volume is missing a unique piece. The only valid operation\n"
	"in this instance is \"delete object\"\n");
		break;

	case E_VOL_STATE_MISS_SUBVOL:
		fprintf (stderr,
	"Volume is missing a required subvolume. The only valid\n"
        "operation in this instance is \"delete object\"\n");
		break;

	case E_SUBVOL_BUSY:
		fprintf (stderr,
	"Volume is active; an active volume cannot be deleted.\n");
		break;

	case E_HASH_TBL:
		fprintf (stderr, "Error in tcl hash table.\n");
		break;

	case E_VE_OVERLAP:
		fprintf (stderr, "Ve overlaps with other ves.\n");
		break;

	case E_VOL_MISS_PIECE:
		fprintf (stderr,
	"Volume is missing some pieces. The only valid operation\n"
	"in this instance is \"delete object\"\n");
		break;
	
	default:
		fprintf (stderr, "Returned with error %d\n", xlv_err);

	}

} /* end of print_error() */


#define	KILO	1024
#define MEGA	(1024*KILO)
#define GIGA	(1024*MEGA)

/*
 * Display the software configuration of the kernel XLV driver.
 */
/*ARGSUSED*/
void
print_config(int expert)
{
	xlv_attr_req_t		req;
	xlv_attr_memory_t	xlvmem;
	xlv_attr_cursor_t	cursor;
	float			size;
	int			index;
	char	*scale_strings[] = {"bytes", "KB", "MB", "GB"};

	char	*p_strings[] = {"unknown", "not present", "present"};
	char	*onoff_strings[] = {"???", "off", "on"};

	extern int has_plexing_license;
	extern int has_plexing_support;

	/*
	 * First we need to get a XLV cursor.
	 */
	if (0 > syssgi(SGI_XLV_ATTR_CURSOR, &cursor)) {
		printf("Failed to get a XLV cursor\n");
		return;
	}

	req.attr = XLV_ATTR_LOCKS;
	if (0 > syssgi(SGI_XLV_ATTR_GET, &cursor, &req)) {
		printf("Allocated subvol locks: ???\tlocks in use: ???\n");
	} else {
		printf("Allocated subvol locks: %d\tlocks in use: %d\n",
			req.ar_max_locks, req.ar_num_locks);
	}

	printf("Plexing license: %s\n", p_strings[has_plexing_license+1]);
	printf("Plexing support: %s\n", p_strings[has_plexing_support+1]);

	req.attr = XLV_ATTR_FLAGS;
	if (0 > syssgi(SGI_XLV_ATTR_GET, &cursor, &req)) {
		index = -1;
	} else {
		index = req.ar_flag1 & XLV_FLAG_STAT;
	}
	printf("Statistic Collection: %s\n", onoff_strings[index+1]);

	if (req.ar_flag2 & XLV_FLAG2_FAILSAFE) {
		printf("Failsafe Mode: on\n");
	}

	req.attr = XLV_ATTR_MEMORY;
	req.cmd = XLV_ATTR_CMD_PLEXMEM;
	req.ar_mem = &xlvmem;

	if (has_plexing_support &&
	   (0 > syssgi(SGI_XLV_ATTR_GET, &cursor, &req))) {

		printf("Plex buffer pool statistics: ???\n");

	} else if (has_plexing_support) {
		if (xlvmem.size < KILO) {
			size = xlvmem.size;
			index = 0;
		} else if (xlvmem.size >= KILO && xlvmem.size < MEGA) {
			size = (float)xlvmem.size / KILO;
			index = 1;
		} else if (xlvmem.size >= MEGA && xlvmem.size < GIGA) {
			size = (float)xlvmem.size / MEGA;
			index = 2;
		} else {
			size = (float)xlvmem.size / GIGA;
			index = 3;
		}
		
		printf("Plex buffer pool statistics: \n");
		if (index) {
			printf(
			"\tmaximum concurrent I/O's %d;  buffer size %.2f%s\n",
			       xlvmem.slots, size, scale_strings[index]);
		} else {
			printf(
			"\tmaximum concurrent I/O's %d;  buffer size %d %s\n",
		               xlvmem.slots, xlvmem.size, scale_strings[index]);
		}
		printf("\tpool hits %d; misses %d; waits %d; resized %d times\n"
		       ,xlvmem.hits, xlvmem.misses, xlvmem.waits, 
		       xlvmem.resized);
		printf("\tmemory growth rate %d%%; maximum miss rate %d%%\n", 
			xlvmem.scale, xlvmem.threshold);
	}

	req.attr = XLV_ATTR_MEMORY;
	req.cmd = XLV_ATTR_CMD_VEMEM;
	req.ar_mem = &xlvmem;

	if (0 > syssgi(SGI_XLV_ATTR_GET, &cursor, &req)) {
		printf("Volume Element buffer pool statistics: ???\n");
	} else {
		if (xlvmem.size < KILO) {
			size = xlvmem.size;
			index = 0;
		} else if (xlvmem.size >= KILO && xlvmem.size < MEGA) {
			size = (float)xlvmem.size / KILO;
			index = 1;
		} else if (xlvmem.size >= MEGA && xlvmem.size < GIGA) {
			size = (float)xlvmem.size / MEGA;
			index = 2;
		} else {
			size = (float)xlvmem.size / GIGA;
			index = 3;
		}
		
		printf("Volume Element buffer pool statistics: \n");
		if (index) {
			printf(
			"\tmaximum concurrent I/O's %d;  buffer size %.2f%s\n",
			       xlvmem.slots, size, scale_strings[index]);
		} else {
			printf(
			"\tmaximum concurrent I/O's %d;  buffer size %d %s\n",
		               xlvmem.slots, xlvmem.size, scale_strings[index]);
		}
		printf("\tpool hits %d; misses %d; waits %d; resized %d times\n"
		       ,xlvmem.hits, xlvmem.misses, xlvmem.waits, 
		       xlvmem.resized);
		printf("\tmemory growth rate %d%%; maximum miss rate %d%%\n", 
			xlvmem.scale, xlvmem.threshold);
	}

	req.attr = XLV_ATTR_SV_MAXSIZE;
	if (0 > syssgi(SGI_XLV_ATTR_GET, &cursor, &req)) {
		req.ar_sv_maxsize = 0xFFFFFFFFL;
	} 
	printf("Maximum subvol block number: 0x%llx\n", req.ar_sv_maxsize);

} /* end of print_config() */


/*
 * Display subvolume statistics.
 */
void
_doprint_stat(xlv_stat_t *statp, int verbose)
{
	__uint32_t	stripeops;

	printf("  read ops: %d,  read blocks: %lld\n",
		statp->xs_ops_read, statp->xs_io_rblocks);

	printf("  write ops: %d,  write blocks: %lld\n",
		statp->xs_ops_write, statp->xs_io_wblocks);

	/*
	 * There are no locks for collecting statistics, so the
	 * operation count can be off.  Let make sure the stripe
	 * ops total add up.
	 */

	stripeops = statp->xs_align_less_sw + statp->xs_align_more_sw
		  + statp->xs_align_full_sw + statp->xs_align_part_su
		  + statp->xs_unalign_less_sw + statp->xs_unalign_more_sw
		  + statp->xs_unalign_full_sw + statp->xs_unalign_part_su;

	if (!verbose && stripeops == 0) {
		return;
	}

	printf("    stripe ops: %d,  total units: %d\n",
		stripeops/*statp->xs_stripe_io*/, statp->xs_su_total);

	printf("\tlargest single i/o: %d stripe units,  frequency: %d\n",
		statp->xs_max_su_len, statp->xs_max_su_cnt);

	printf(
	"\taligned     <    stripe width; ends on stripe unit: %d\n"
	"\taligned     >    stripe width; ends on stripe unit: %d\n"
	"\taligned     =    stripe width; ends on stripe unit: %d\n"
	"\taligned   > or < stripe width; doesn't end on stripe unit: %d\n\n",
		statp->xs_align_less_sw, statp->xs_align_more_sw,
		statp->xs_align_full_sw, statp->xs_align_part_su);

	printf(
	"\tunaligned   <    stripe width; ends on stripe unit: %d\n"
	"\tunaligned   >    stripe width; ends on stripe unit: %d\n"
	"\tunaligned   =    stripe width; doesn't end on stripe unit: %d\n"
	"\tunaligned > or < stripe width; doesn't end on stripe unit: %d\n",
	       statp->xs_unalign_less_sw, statp->xs_unalign_more_sw,
	       statp->xs_unalign_full_sw, statp->xs_unalign_part_su);

} /* end of _doprint_stat() */


void
print_stat(char *sv_name, int verbose)
{
	xlv_attr_req_t		req;
	xlv_attr_cursor_t	cursor;
	xlv_stat_t		stat;
	extern xlv_tab_t	*tab_sv;
	int			max_sv, sv;
	char	name[100];

	static char *svtype_str[] = {
		"",     /* 0-based. */
		"log", "data", "rt", "rsvd", "unknown",
	};

	/* First we need to get a XLV cursor. */
	if (0 > syssgi(SGI_XLV_ATTR_CURSOR, &cursor)) {
		printf("Failed to get a XLV cursor\n");
		return;
	}

	/* Determine the maximum subvolume supported in this kernel. */
	req.attr = XLV_ATTR_LOCKS;
	if (0 > syssgi(SGI_XLV_ATTR_GET, &cursor, &req)) {
		max_sv = 0;
	} else {
		max_sv = req.ar_max_locks;
	}

	/* Set up the kernel interface structure. */
	req.attr = XLV_ATTR_STATS;
	req.ar_statp = &stat;

	if (sv_name == NULL) {
		/* get statistics for all subvolumes */
		for (sv = 0; sv < max_sv; sv++) {
			if (!tab_sv || NULL == tab_sv->subvolume[sv].vol_p)
				continue;
			cursor.subvol = sv;
			if (0 > syssgi(SGI_XLV_ATTR_GET, &cursor, &req)) {
				if (oserror() == ENFILE)  /* no more sv's */
					return;
				if (oserror() == ENOENT)
					continue;
				perror("Cannot get statistics");
				return;
			}
			if (!stat.xs_ops_read && !stat.xs_ops_write && !verbose)
				continue;
			/* get official subvolume name and display stat */
			sprintf(name, "%s.%s",
				tab_sv->subvolume[sv].vol_p->name,
				svtype_str[tab_sv->subvolume[sv].subvol_type]);
			printf("\nSubvolume %d (%s):\n", sv, name);
			_doprint_stat(&stat, verbose);
		}
		return;
	}

	/* Want a specific subvolume */
	if (!_name_to_subvol(sv_name, &sv)) {
		printf("Cannot map %s to a subvolume.\n", sv_name);
		return;
	}

	/* get specific subvolume statistics */
	if (sv >= max_sv) {
		printf("Subvolume %d: does not exist.\n", sv);
		return;
	}
	cursor.subvol = sv;
	if (0 > syssgi(SGI_XLV_ATTR_GET, &cursor, &req)) {
		if (oserror() == ENOENT) {
			printf("Subvolume %d (%s): does not exist.\n",
				sv, sv_name);
	} else {
			perror("Cannot get statistics");
		}
		return;
	}
	if (tab_sv && tab_sv->subvolume[sv].vol_p) {
		sprintf(name, "%s.%s", tab_sv->subvolume[sv].vol_p->name,
			svtype_str[tab_sv->subvolume[sv].subvol_type]);
	} else {
		name[0] = '\0';
	}
	printf("Subvolume %d (%s):\n", sv, name);
	_doprint_stat(&stat, verbose);

} /* end of print_stat() */


/*
 * Display all the on disk XLV labels.
 */
int
print_labels(int p_mode, int secondary)
{
	xlv_vh_entry_t	*vh, *vhlist;
	int		status;
	int		disks = 0;

	vh = vhlist = NULL;
	status = -1;

	xlv_lab2_create_vh_info_from_hinv (&vhlist, NULL, &status);

	if (vhlist) {
		for (vh = vhlist; vh != NULL; vh = vh->next) {
			xlv_print_vh (vh, p_mode, secondary);
			disks++;
		}
		xlv_lab2_destroy_vh_info(&vhlist, &status);

	} else {
		printf("XLV labels not found on any disks.\n");
	}

	return (disks);

} /* end of print_labels() */


int
print_one_label(char *device_name, int p_mode, int secondary)
{
	xlv_vh_entry_t  vh_entry, *vh;
	int		status = 0;
	int		retval = XLV_MGR_STATUS_OKAY;
	struct stat	stat_buf;
	char		path[XLV_MAXDEVPATHLEN+10];
	char		errmsg[250];

	/* /dev/rdsk/dks%dd%dl%dvh */

	vh = &vh_entry;
	bzero (vh, sizeof (xlv_vh_entry_t));
	vh->vfd = -1;

	setoserror(0);				/* reset errno */

	if ((device_name[0] != '/') ||
	    (0 != strncmp(device_name, "/dev/rdsk/", 10)) ) {
		 strcat (strcpy(path, "/dev/rdsk/"), device_name);
	} else {
		strcpy(path, device_name);
	}

	if (status = stat (path, &stat_buf)) {
		sprintf(errmsg, "Cannot access device \"%s\"", path);
	} else if (status = xlv_lab1_open (path, NULL, vh)) {
		sprintf(errmsg, "Cannot get label from \"%s\"", path);
	} else {
		 xlv_print_vh (vh, p_mode, secondary);
	}

	if (status) {
		perror (errmsg);
		retval = XLV_MGR_STATUS_FAIL;
	}

	if (vh->vfd != -1)
		close(vh->vfd);

	return (retval);

} /* end of print_one_label() */


/*
 * Display all the subvolumes in the kernel. 
 *
 * If <sv_idx> is a valid subvolume index, display only that subvolume.
 */
/*ARGSUSED1*/
int
print_kernel_tab(int p_mode, int format, int sv_idx)
{
	xlv_tab_subvol_t	*svp;
	xlv_attr_req_t		req;
	xlv_attr_cursor_t	cursor;
	int			status;
	int			count;

	/*
	 * First we need to get a XLV cursor.
	 */
	if (0 > syssgi(SGI_XLV_ATTR_CURSOR, &cursor)) {
		printf("Failed to get a XLV cursor\n");
		return(1);
	}

	if (NULL == (svp = get_subvol_space())) {
		printf("Cannot malloc a subvolume entry.\n");
		return(1);
	}

	req.attr = XLV_ATTR_SUBVOL;
	req.ar_svp = svp;

	status = 0;
	count = 0;
	while (status == 0) {
		status = syssgi(SGI_XLV_ATTR_GET, &cursor, &req);
		if (status < 0) {
			int err = oserror();
			if (ENFILE != err && ENOENT != err) {
				perror("syssgi(SGI_XLV_ATTR_GET) failed.");
			}
			break;		/* end of enumeration */
		} else {
			if (sv_idx > -1) {
				if (sv_idx != minor(svp->dev))
					continue;
				status = 1;	/* found subvol so end loop */
			}
			if (count) printf("\n");
			xlv_tab_subvol_print(svp, p_mode);
			count++;
		}
	}
	
	free_subvol_space(svp);

	if (sv_idx > -1) {
		if (count == 0)
			printf("Subvolume %d does not exist.\n", sv_idx);
		return (0);
	}

	if (count == 0)
		printf("\nThere are no subvolumes.\n");
	else if (count == 1)
		printf("\nThere is 1 subvolume.\n");
	else
		printf("\nThere are %d subvolumes.\n", count);

	return (0);

} /* end of print_kernel_tab() */


static char *
_revive_percent_str(struct xlv_tab_subvol_s *svp)
{
	static char	buf[10];
	__int64_t	start, stop, done;

	start = svp->critical_region.start_blkno;
	stop = svp->critical_region.end_blkno;
	if (stop == 0) {
		strcpy(buf, "~??%");
	} else { 
		done = (start * 100) / svp->subvol_size;
		sprintf(buf, "~%2lld%%", done);
	}

	return(buf);
}

static void
_print_vol(xlv_tab_vol_entry_t *volp) {
	struct xlv_tab_subvol_s *svp;
	int	count = 0;
	char	*pct;
	char	buf[100], *msg = &buf[0];


	printf("Volume:\t%s (%s)", volp->name, xlv_vol_state_str(volp->state));
	if ((svp = volp->data_subvol)
	    && (svp->flags & XLV_SUBVOL_PLEX_CPY_IN_PROGRESS)) {
		pct = _revive_percent_str(svp); 
		sprintf(msg, "data=%s", pct);
		msg = &buf[strlen(msg)];
		count++;
	}
	if ((svp = volp->log_subvol)
	    && (svp->flags & XLV_SUBVOL_PLEX_CPY_IN_PROGRESS)) {
		pct = _revive_percent_str(svp); 
		sprintf(msg, (count) ? ", log=%s" : "log=%s", pct);
		msg = &buf[strlen(msg)];
		count++;
	}
	if ((svp = volp->rt_subvol)
	    && (svp->flags & XLV_SUBVOL_PLEX_CPY_IN_PROGRESS)) {
		pct = _revive_percent_str(svp); 
		sprintf(msg, (count) ? ", rt=%s" : "rt=%s", pct);
		msg = &buf[strlen(msg)];
		count++;
	}
	if (count)
		printf("\trevive[%s]", buf);
	printf("\n");

} /* end of _print_vol() */


/*
 * Display all the volumes in the kernel. 
 *
 * If <volname> is not NULL, display only that volume.
 */
int
print_kernel_tabvol(int p_mode, int format, char *volname)
{
	xlv_tab_vol_entry_t	*volp;
	xlv_attr_req_t		req;
	xlv_attr_cursor_t	cursor;
	int			status;
	int			count;

	/*
	 * First we need to get a XLV cursor.
	 */
	if (0 > syssgi(SGI_XLV_ATTR_CURSOR, &cursor)) {
		printf("Failed to get a XLV cursor\n");
		return(1);
	}

	/*
	 * Allocate space for a full sized volume.
	 */
	volp = get_vol_space();
	if (NULL == volp) {
		printf("Cannot malloc a full volume entry.\n");
		return(1);
	}

	req.attr = XLV_ATTR_VOL;
	req.ar_vp = volp;

	status = 0;
	count = 0;
	while (status == 0) {
		status = syssgi(SGI_XLV_ATTR_GET, &cursor, &req);
		if (status < 0) {
			if (ENFILE == oserror())
				/* end of enumeration */
				break;
			perror("syssgi(SGI_XLV_ATTR_GET) failed.");
		} else {
			if (volname) {
				if (0 != strcmp(volname, volp->name))
					continue;
				status = 1;	/* found vol so end loop */
			}
			if (format == FMT_LONG) {
				if (count) printf("\n");
				xlv_tab_vol_print(volp, p_mode);
			} else {
				/* display using the short format */
				_print_vol(volp);
			}
			count++;
		}
	} /* while loop */
	
	free_vol_space (volp);

	if (volname) {
		if (count == 0)
			printf("Volume \"%s\" does not exist.\n", volname);
		return (0);
	}

	if (count == 0)
		printf("\nThere are no volumes.\n");
	else if (count == 1)
		printf("\nThere is 1 volume.\n");
	else
		printf("\nThere are %d volumes.\n", count);

	return (0);

} /* end of print_kernel_tabvol() */


/*
 * Display all known objects.
 *
 * If format == FMT_LONG then display the whole structure,
 * otherwise only display the name and type.
 *
 * Return 0 on success.
 */
int
print_all_objects (int format, int p_mode)
{
	Tcl_HashEntry	*hash_entry;
	Tcl_HashSearch	search_ptr;
	xlv_oref_t 	*oref;
	int		count = 0;
	int		nvol = 0;
	int		nplex = 0;
	int		nve = 0;
	char		*owner;
	char		*me = xlv_getnodename();

	hash_entry = Tcl_FirstHashEntry (&xlv_obj_table, &search_ptr);

	while (hash_entry) {

		oref = (xlv_oref_t *) Tcl_GetHashValue (hash_entry);
		ASSERT (oref != NULL);

		for ( ; oref; oref = oref->next) {
			count++;

			if (oref->obj_type == XLV_OBJ_TYPE_VOL) {
				nvol++;
			} else if (oref->obj_type == XLV_OBJ_TYPE_PLEX) {
				nplex++;
			} else if (oref->obj_type == XLV_OBJ_TYPE_VOL_ELMNT) {
				nve++;
			}

			if (format == FMT_LONG) {
				if (count > 1)
					printf("\n");
			 	xlv_oref_print (oref, p_mode);
				/* printf("\n"); */
				continue;
			}

			/* display using the short format */

			switch (oref->obj_type) {
			case XLV_OBJ_TYPE_VOL:
				owner = oref->vol->nodename;
				printf("Volume:\t\t%s (%s",
					XLV_OREF_VOL(oref)->name,
					xlv_vol_state_str(oref->vol->state));
				if (strncmp(me, owner, XLV_NODENAME_LEN)) {
					printf("; node=%s)\n", owner);
				} else {
					printf(")\n");
				}
				break;

			case XLV_OBJ_TYPE_PLEX:
				printf("Plex:\t\t%s",
					XLV_OREF_PLEX(oref)->name);
				/*
				 * Display the nodename if not local.
				 */
				owner = oref->nodename;
				if (strncmp(me, owner, XLV_NODENAME_LEN)) {
					printf(" (node=%s)\n", owner);
				} else {
					printf("\n");
				}
				break;

			case XLV_OBJ_TYPE_VOL_ELMNT:
				printf("Volume Element:\t%s",
					XLV_OREF_VOL_ELMNT(oref)->veu_name);
				/*
				 * Display the nodename if not local.
				 */
				owner = oref->nodename;
				if (strncmp(me, owner, XLV_NODENAME_LEN)) {
					printf(" (node=%s)\n", owner);
				} else {
					printf("\n");
				}
				break;
			}
		}

		hash_entry = Tcl_NextHashEntry (&search_ptr);

	} /* end of while (hash_entry) loop */

	printf("\nVol: %d; Standalone Plex: %d; Standalone Ve: %d\n",
		nvol, nplex, nve);

	return(0);

} /* end of print_all_objects() */


/*
 * Display an object with the given name.
 *
 * XXX print_name() should take a "type" field so a preference
 * can be specified.
 *
 * Return 0 on success.
 * Return 1 if there are no object with the given name.
 */
int
print_name(char *name, int p_mode)
{
	xlv_oref_t	*oref;
	int		error = 1;
	char		*nname = NULL;		/* nodename */
	char		*oname = NULL;		/* object name */

	if (name == NULL || *name == '\0') {
		return (error);
	}

	oref = find_object_in_table(name, XLV_OBJ_TYPE_NONE, NULL);

	if ((oref == NULL) && (oname = strstr(name, "."))) {
		/*
		 * Cannot find object with that name.
		 * Check for the nodename.objectname syntax.
		 */
		nname = name;
		oname[0] = '\0';		/* terminate nodename string */
		oname++;
		oref = find_object_in_table(oname, XLV_OBJ_TYPE_NONE, nname);
		if (oref == NULL) {
			oname[-1] = '.';	/* undo edit */
		}
	}

	if (oref) {
		xlv_oref_print (oref, p_mode);
		free (oref);
		error = 0;			/* found object to display */
	}

	return (error);

} /* end of print_name() */


/*
 * Generate xlv_make(1m) script to make all known objects.
 *
 * Return 0 on success and 1 on failure.
 */
int
script_all_objects (FILE *stream)
{
	Tcl_HashEntry	*hash_entry;
	Tcl_HashSearch	search_ptr;
	xlv_oref_t 	*oref;
	char		*oref_name;
	char		*oref_typestr;
	char		*timestr;
	time_t		t;
	int		status;
	int		count = 0;
	int		errors = 0;

	t = time(NULL);
	timestr = asctime(localtime(&t));

	hash_entry = Tcl_FirstHashEntry (&xlv_obj_table, &search_ptr);

	while (hash_entry) {

		oref = (xlv_oref_t *) Tcl_GetHashValue (hash_entry);
		ASSERT (oref != NULL);

		for ( ; oref; oref = oref->next) {

			switch (oref->obj_type) {
			case XLV_OBJ_TYPE_VOL:
				oref_name = XLV_OREF_VOL(oref)->name;
				oref_typestr = "Volume";
				break;

			case XLV_OBJ_TYPE_PLEX:
				oref_name = XLV_OREF_PLEX(oref)->name;
				oref_typestr = "Plex";
				break;

			case XLV_OBJ_TYPE_VOL_ELMNT:
				oref_name = XLV_OREF_VOL_ELMNT(oref)->veu_name;
				oref_typestr = "Volume Element";
				break;
			}

			if (count == 0) {
				fprintf(stream, "#\n# Date: %s", timestr);
			}
			fprintf(stream, "#\n# Create %s %s\n#\n",
				oref_typestr, oref_name);
			status = xlv_oref_to_xlv_make_cmds (stream, oref);
			if (status) {
				fprintf(stderr,
					"Failed to create object %s.\n",
					oref_name);
				errors++;
			} else {
				count++;
			}
		}

		hash_entry = Tcl_NextHashEntry (&search_ptr);

	} /* end of while (hash_entry) loop */

	if (count > 0) {
		fprintf (stream, "exit\n");
	}

	return((errors == 0) ? 0 : 1 );

} /* end of script_all_objects() */


/*
 * Script an object with the given name.
 *
 * Return 0 on success and 1 on failure.
 */
int
script_one(FILE *stream, char *name)
{
	xlv_oref_t	*oref;
	int		status = 1;
	char		*typestr;
	char		*timestr;
	time_t		t;

	if (name == NULL || *name == '\0') {
		return (1);
	}

	oref = find_object_in_table(name, XLV_OBJ_TYPE_NONE, NULL);
	if (oref == NULL) {
		fprintf(stderr, "Object \"%s\" was not found.\n", name);
		return(1);
	}

	switch (oref->obj_type) {
	case XLV_OBJ_TYPE_VOL:
		typestr = "Volume";
		break;

	case XLV_OBJ_TYPE_PLEX:
		typestr = "Plex";
		break;

	case XLV_OBJ_TYPE_VOL_ELMNT:
		typestr = "Volume Element";
		break;
	}

	t = time(NULL);
	timestr = asctime(localtime(&t));

	fprintf(stream, "#\n# Date: %s", timestr);
	fprintf(stream, "#\n# Create %s %s\n#\n", typestr, name);

	status = xlv_oref_to_xlv_make_cmds (stream, oref);

	if (status) {
		fprintf (stderr,
	"Failed to generate xlv_make(1m) script to create object %s.\n",
			name);
	} else {
		fprintf(stream, "exit\n");
	}

	free (oref);

	return (status);

} /* end of script_one() */

/*
 * Change the memory pool parameters
 */
int
change_mem_params(const int cmd, int slots, int grow, int miss)
{
	xlv_attr_req_t		req;
	xlv_attr_memory_t	xlvmem;
	xlv_attr_cursor_t	cursor;

	xlvmem.slots = slots;
	xlvmem.scale = grow;
	xlvmem.threshold = miss;

	/*
	 * First we need to get a XLV cursor.
	 */
	if (0 > syssgi(SGI_XLV_ATTR_CURSOR, &cursor)) {
		printf("Failed to get a XLV cursor\n");
		return 1;
	}

	req.attr = XLV_ATTR_MEMORY;
	req.cmd = cmd;
	req.ar_mem = &xlvmem;

	if (0 > cap_dev_syssgi(SGI_XLV_ATTR_SET, &cursor, &req)) {
		printf("Unable to set memory parameters\n");
		return 1;
	}

	return 0;
}

/*
 * Enable or disable flags ... statistics and failsafe.
 *
 * Values:  0 - unset
 *	    1 - set
 *	   -1 - no-op
 */
int
set_attr_flags(int dostat, int dofailsafe)
{
	xlv_attr_req_t		req;
	xlv_attr_cursor_t	cursor;

	/*
	 * First we need to get a XLV cursor.
	 */
	if (0 > syssgi(SGI_XLV_ATTR_CURSOR, &cursor)) {
		printf("Failed to get a XLV cursor\n");
		return 1;
	}

	req.attr = XLV_ATTR_FLAGS;

	/*
	 * Get the current settings.
	 */
	req.cmd = XLV_ATTR_CMD_QUERY;	/* XXX not really needed for gets*/
	if (0 > syssgi(SGI_XLV_ATTR_GET, &cursor, &req)) {
		perror("Unable to get attributes.\n");
		return 1;
	}

	/*
	 * Make the changes.
	 */
	req.cmd = XLV_ATTR_CMD_SET;
	if (dostat == 0) {
		req.ar_flag1 &= ~XLV_FLAG_STAT;
	} else if (dostat == 1) {
		req.ar_flag1 |= XLV_FLAG_STAT;
	}
	if (dofailsafe == 0) {
		req.ar_flag2 &= ~XLV_FLAG2_FAILSAFE;
	} else if (dofailsafe == 1) {
		req.ar_flag2 |= XLV_FLAG2_FAILSAFE;
	}
	if (0 > cap_dev_syssgi(SGI_XLV_ATTR_SET, &cursor, &req)) {
		perror("Unable to set attributes.\n");
		return 1;
	}

	return 0;
}

/*
 * Scan all the disks and delete the XLV disk label.
 * Set *count to the number of XLV disk labels removed.
 *
 * Return XLV_MGR_STATUS_OKAY upon success.
 */
int
delete_all_labels(int *count)
{
	xlv_vh_entry_t  *vh, *vhlist;
	int             status;
	int		failures = 0;
	int		delete_cnt = 0;
	char		*str;

	vh = vhlist = NULL;
	status = -1;

	xlv_lab2_create_vh_info_from_hinv (&vhlist, NULL, &status);
	if (vhlist) {
		for (vh = vhlist; vh != NULL; vh = vh->next) {
			printf("Delete XLV label on %s ...", vh->vh_pathname);
			status = xlv_delete_xlv_label(vh, vhlist);
			if (0 == status) {
				str = "done";
				delete_cnt++;
			} else {
				str = "failed";
				failures++;
			}
			printf(" %s.\n", str);
		}
		xlv_lab2_destroy_vh_info(&vhlist, &status);

	} else {
		printf("XLV labels not found on any disks.\n");
	}

	*count = delete_cnt;
	return ((0 == failures) ? XLV_MGR_STATUS_OKAY: XLV_MGR_STATUS_FAIL);

} /* end of delete_all_labels() */


/*
 * Delete the XLV disk label from the given device.
 *
 * Return XLV_MGR_STATUS_OKAY upon success.
 *
 * XXXjleong Don't use perror(3c)
 */
int
delete_one_label (char *device_name)
{
	xlv_vh_entry_t  vh_entry, *vh;
	int		status = 0;
	int		retval = XLV_MGR_STATUS_OKAY;
	struct stat	stat_buf;
	char		path[XLV_MAXDEVPATHLEN+10];
	char		errmsg[250];

	/* /dev/rdsk/dks%dd%dl%dvh */

	vh = &vh_entry;
	bzero (vh, sizeof (xlv_vh_entry_t));
	vh->vfd = -1;

	setoserror(0);				/* reset errno */

	if ((device_name[0] != '/') ||
	    (0 != strncmp(device_name, "/dev/rdsk/", 10)) ) {
		 strcat (strcpy(path, "/dev/rdsk/"), device_name);
	} else {
		strcpy(path, device_name);
	}

	if (status = stat (path, &stat_buf)) {
		sprintf(errmsg, "Cannot access device \"%s\"", path);
	} else if (status = xlv_lab1_open (path, NULL, vh)) {
		sprintf(errmsg, "Cannot open \"%s\"", path);
	} else if (status = xlv_lab1_delete_label (vh)) {
		sprintf(errmsg, "Cannot delete the XLV label on \"%s\"", path);
	}

	if (status) {
		perror (errmsg);
		retval = XLV_MGR_STATUS_FAIL;
	}

	if (vh->vfd != -1)
		close(vh->vfd);

	return (retval);

} /* end of delete_one_label() */


/*
 * Clear the statistic counters
 */
void
reset_stat(char *sv_name)
{
	xlv_attr_req_t		req;
	xlv_attr_cursor_t	cursor;
	xlv_stat_t		stat;
	extern xlv_tab_t	*tab_sv;
	int			max_sv;
	int			idx;
	int			sv;

	/*
	 * First we need to get a XLV cursor.
	 */
	if (0 > syssgi(SGI_XLV_ATTR_CURSOR, &cursor)) {
		printf("Failed to get a XLV cursor\n");
		return;
	}

	if (sv_name == NULL) {
		sv = -1;
	} else {
		if (!_name_to_subvol(sv_name, &sv)) {
			printf("Cannot map %s to a subvolume.\n", sv_name);
			return;
		}
	}

	/* Determine the maximum subvolume supported in this kernel. */
	req.attr = XLV_ATTR_LOCKS;
	if (0 > syssgi(SGI_XLV_ATTR_GET, &cursor, &req)) {
		max_sv = 0;
	} else {
		max_sv = req.ar_max_locks;
	}

	/* Set up the kernel interface structure. */
	req.attr = XLV_ATTR_STATS;
	req.ar_statp = &stat;
	bzero(&stat, sizeof(stat));

	if (sv == -1) {
		/* clear counters for all subvolumes */
		for (idx = 0; idx < max_sv; idx++) {
			cursor.subvol = idx;
			if (0 > cap_dev_syssgi(SGI_XLV_ATTR_SET, &cursor, &req)) {
				if (oserror() == ENOENT)  /* sv doesn't exit */
					continue;
				if (oserror() == ENFILE)  /* no more sv's */
					return;
				fprintf(stderr, "Subvolume %d: ", idx);
				perror("Cannot clear statistics");
				continue;
			}
#ifdef DEBUG
			fprintf(stderr, "DBG: Cleared subvolume %d\n", idx);
#endif
		}
		return;
	}

	/* get specific subvolume statistics */
	if (sv >= max_sv) {
		printf("Subvolume %d: does not exist.\n", sv);
		return;
	}
	cursor.subvol = sv;
#ifdef DEBUG
	fprintf(stderr, "DBG: Subvolume %d: ", sv);
#endif
	if (0 > cap_dev_syssgi(SGI_XLV_ATTR_SET, &cursor, &req)) {
		if (oserror() == ENOENT || oserror() == ENFILE) {
			printf("Subvolume %d: does not exist.\n", sv);
		} else {
			perror("Cannot clear statistics");
		}
		return;
	}
#ifdef DEBUG
	fprintf(stderr, "cleared\n");
#endif

} /* end of reset_stat() */
