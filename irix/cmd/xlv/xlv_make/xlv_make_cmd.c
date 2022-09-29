/**************************************************************************
 *                                                                        *
 *              Copyright (C) 1994, Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.45 $"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bstring.h>
#include <tcl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uuid.h>

#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>
#include <sys/sysmacros.h>
#include <xlv_error.h>
#include <xlv_utils.h>
#include <sys/debug.h>
#include <diskinfo.h>
#include <mountinfo.h>
#include "xlv_make_cmd.h"
#include "xlv_make_int.h"


/*
 * TCL commands for xlv_make.
 *
 * We try to do as much error checking as possible here and have
 * the back-end routines (in xlv_make_int) do all the work.
 */

static xlv_mk_context_t	xlv_mk_context;

/*
 * Static string to hold the string representation of the current
 * context.  This will be the object being created.
 *
 * <volume_name>_data.2.256.64.
 * The max length is about 45 characters.
 */
static char		xlv_mk_context_str[50];

#ifdef DEBUG
static int		init_called = 0;
#endif

/*
 * static to hold partition check state pointer
 */

static mnt_check_state_t *mnt_check_state = 0;

/*
 * Returns whether or not the force option is in effect
 */
int
_isforce(Tcl_Interp *interp)
{
	char	*force;
	int	value;

	force = Tcl_GetVar(interp, XLV_MAKE_VAR_FORCE, 0);

	if (force && (TCL_OK == Tcl_GetInt(interp, force, &value))) {
		return (value);
	}
	return 0;
}

/*
 * Returns the level of expertise as determined by the value of
 * the "expert" TCL variable.
 */
int
_isexpert(Tcl_Interp *interp)
{
	char	*expert;
	int	expertise;

	expert = Tcl_GetVar(interp, XLV_MAKE_VAR_SUSER, 0);

	if (expert && (TCL_OK == Tcl_GetInt(interp, expert, &expertise)))
		return (expertise);

	return (0);
}

static int
_isverbose(Tcl_Interp *interp)
{
	char	*verbose;
	int	value;

	verbose = Tcl_GetVar(interp, XLV_MAKE_VAR_VERBOSE, 0);

	if (verbose && (TCL_OK == Tcl_GetInt(interp, verbose, &value)))
		return (value);
	
	return (0);
}

static int
_no_Assemble(Tcl_Interp *interp)
{
	char	*no_Assemble;
	int	value;

	no_Assemble = Tcl_GetVar(interp, XLV_MAKE_VAR_NO_ASSEMBLE, 0);

	if (no_Assemble && (TCL_OK == Tcl_GetInt(interp, no_Assemble, &value)))
		return (value);
	
	return (0);
}

/* xlv_make_init()
 *
 * Init backend and partition checking routines 
 */

void
xlv_make_init (void)
{
	ASSERT (init_called == 0);
#ifdef DEBUG
	init_called = 1;
#endif
	XLV_OREF_INIT (&xlv_mk_context);

	xlv_mk_init ();

	/* init partition checking routines */
	if (mnt_check_init(&mnt_check_state) == -1) {
	        fprintf(stderr, "xlv_make: partition check init failed. "
			"Mounted partition checking disabled.\n");
		mnt_check_state = 0;
	}
}

/* xlv_make_done()
 *
 * Cleanup house before we leave
 */

void
xlv_make_done (void)
{
        /* sighup,sigterm and friends should be caught and directed here
	 * (mnt_check_init might leave extraneous /tmp entries around)
	 */
        if (mnt_check_state)
	        mnt_check_end(mnt_check_state);
}

/*
 * Command procedures - one for every command in xlv_make.
 *
 * Each of these procedures are of type Tcl_CmdProc.
 */


/*
 * Process the vol command:
 *
 *	vol <volume_name> -readonly -noautomount
 */

/*ARGSUSED*/
int
xlv_make_vol (ClientData clientData,
	      Tcl_Interp *interp,
	      int	 argc,
	      char	 *argv[])
{
	char	*volume_name = NULL;
	long	flag;
	int	i;
	int	status;
	int	expertflag;

	ASSERT (init_called);

	/*
	 * By TCL convention:
	 * 	argv[0] = command name.
	 * 	argv[argc] = NULL.
	 */

	if (argc < 2) {
		set_interp_result (interp, XLV_STATUS_VOL_NAME_NEEDED);
		return (TCL_ERROR);
	}

	/*
	 * By default, all volumes are marked automount unless
	 * explicitly overriden by -noautomount.
	 */

	flag = XLV_VOL_AUTOMOUNT;
	for (i=1; i < argc; i++) {
		if (*argv[i] != '-') {
			if (volume_name == NULL)
				volume_name = argv[i];
			else {
				set_interp_result (interp, 
				    XLV_STATUS_UNEXPECTED_SYMBOL, argv[i]);
				return (TCL_ERROR);
			}
		}
		else if (strcmp (argv[i], "-readonly") == 0)
			flag |= XLV_VOL_READONLY;
		else if (strcmp (argv[i], "-noautomount") == 0)
			flag &= ! XLV_VOL_AUTOMOUNT;
		else {
			set_interp_result (interp,
			    XLV_STATUS_UNKNOWN_FLAG, argv[i]);
			return (TCL_ERROR);
		}

	}

	if (volume_name == NULL) {
		set_interp_result (interp, XLV_STATUS_VOL_NAME_NEEDED);
		return (TCL_ERROR);
	}

	/*
	 * Check if the volume object has already been created.
	 */
	expertflag = _isexpert(interp);

	status = xlv_mk_vol (&xlv_mk_context, volume_name, flag, expertflag);
	if (status != XLV_STATUS_OK) {
		XLV_OREF_INIT (&xlv_mk_context);
		set_interp_result (interp, status);
		return (TCL_ERROR);
	}

	xlv_oref_to_string (&xlv_mk_context, xlv_mk_context_str);
	set_interp_result_str (interp, xlv_mk_context_str);

	return TCL_OK;
}

/*ARGSUSED*/
int
xlv_make_subvol_log (ClientData clientData,
		     Tcl_Interp *interp,
		     int	argc,
		     char	*argv[])
{
	int	status;

	ASSERT (init_called);

	if (argc > 1) {
		set_interp_result (interp, XLV_STATUS_UNEXPECTED_SYMBOL, 
			argv[1]);
		return (TCL_ERROR);
	}

	status = xlv_mk_subvol (&xlv_mk_context, XLV_SUBVOL_TYPE_LOG, 0);
	if (status != XLV_STATUS_OK) {
                set_interp_result (interp, status);
                return (TCL_ERROR);
        }

	xlv_oref_to_string (&xlv_mk_context, xlv_mk_context_str);
	set_interp_result_str (interp, xlv_mk_context_str);

	return TCL_OK;
}

/*ARGSUSED*/
int
xlv_make_subvol_data (ClientData clientData,
		      Tcl_Interp *interp,
		      int	 argc,
		      char	 *argv[])
{
	int	status;

        ASSERT (init_called);

        if (argc > 1) {
                set_interp_result (interp, XLV_STATUS_UNEXPECTED_SYMBOL,
                        argv[1]);
                return (TCL_ERROR);
        }

        status = xlv_mk_subvol (&xlv_mk_context, XLV_SUBVOL_TYPE_DATA, 0);
        if (status != XLV_STATUS_OK) {
                set_interp_result (interp, status);
                return (TCL_ERROR);
        }

        xlv_oref_to_string (&xlv_mk_context, xlv_mk_context_str);
        set_interp_result_str (interp, xlv_mk_context_str);

	return TCL_OK;
}

/*
 * Process the rt command:
 *
 *	rt [-no_error_retry]
 */
/*ARGSUSED*/
int
xlv_make_subvol_rt (ClientData clientData,
		    Tcl_Interp *interp,
		    int	       argc,
		    char       *argv[])
{
        int     status;
        int     i;
	long	flags;

        ASSERT (init_called);

	/* Parse command line */
	flags = 0;
	for (i=1; i < argc; i++) {
		if (*argv[i] != '-') {
                	set_interp_result (interp,
				XLV_STATUS_UNEXPECTED_SYMBOL, argv[i]);
			return (TCL_ERROR);
		}
		else if (strcmp (argv[i], "-no_error_retry") == 0) {
			flags |= (long) XLV_SUBVOL_NO_ERR_RETRY;
		}
		else {
			set_interp_result (interp,
				XLV_STATUS_UNKNOWN_FLAG, argv[i]);
			return (TCL_ERROR);
		}
	}

        status = xlv_mk_subvol (&xlv_mk_context, XLV_SUBVOL_TYPE_RT, flags);
        if (status != XLV_STATUS_OK) {
                set_interp_result (interp, status);
                return (TCL_ERROR);
        }

        xlv_oref_to_string (&xlv_mk_context, xlv_mk_context_str);
        set_interp_result_str (interp, xlv_mk_context_str);

	return TCL_OK;
}


/*
 * Example:
 * 	xlv_make> plex -state active <plex_no>
 */
/*ARGSUSED*/
int
xlv_make_plex (ClientData clientData,
	       Tcl_Interp *interp,
	       int	  argc,
	       char	  *argv[])
{
        int     status;
	char	*plex_name;
	int	ve_state;
	int	set_ve_state = 0;
	int	expertflag;
	int	index;
	unsigned num_vol_elmnts;

        ASSERT (init_called);

	/*
	 * Parse any option flags.
	 */
	for (index = 1; (index < argc) && (*argv[index] == '-'); index++) {
		if (strcmp (argv[index], "-state") == 0) {
			if (++index >= argc) { 
				set_interp_result (interp,
					XLV_STATUS_MISSING_VALUE,
					argv[index-1]);
				return TCL_ERROR;
			}
			for (ve_state=0;
			     ve_state <= XLV_VE_STATE_INCOMPLETE;
			     ve_state++) {
				if (!strcmp(argv[index],
				    xlv_ve_state_str((unsigned)ve_state)))
					break;
			}
			if (ve_state > XLV_VE_STATE_INCOMPLETE) {
				set_interp_result(interp, XLV_STATUS_BAD_VALUE);
				return TCL_ERROR;
			}
			set_ve_state++;
		} else {
			set_interp_result (interp, XLV_STATUS_UNKNOWN_FLAG,
				argv[index]);
			return TCL_ERROR;
		}
	}

	/*
	 * We know there are command options when the index is greater
	 * than 1. But the user must be an expect to use command options.
	 */ 
	expertflag = _isexpert(interp);
	if (index != 1 && !expertflag) {
		set_interp_result (interp, XLV_STATUS_NO_PERMISSION,
			argv[index]);
		return TCL_ERROR;
	}

	if (index < argc)
		plex_name = argv[index];
	else
		plex_name = NULL;
	/*
	 * XXX Should there be a check that there is no more agruments?
	 */

        status = xlv_mk_plex (&xlv_mk_context, plex_name, 0, expertflag);
        if (status != XLV_STATUS_OK) {
                set_interp_result (interp, status);
                return (TCL_ERROR);
        }

	/*
	 * Perform plex option
	 */
	if (set_ve_state) {
		xlv_tab_plex_t *plexp = XLV_OREF_PLEX(&xlv_mk_context);

		num_vol_elmnts = plexp->num_vol_elmnts;
		for (index = 0; (long) index < num_vol_elmnts; index++)
			plexp->vol_elmnts[index].state = (uchar_t)ve_state;

		/*
		 * The disk labels need to be updated so complete
		 * this object by setting the volume element pointer
		 * to reference the last ve.
		 */
		XLV_OREF_VOL_ELMNT(&xlv_mk_context) = &plexp->vol_elmnts[index];
		XLV_MK_CTXT_SET_DIRTY(&xlv_mk_context);
	}

        xlv_oref_to_string (&xlv_mk_context, xlv_mk_context_str);
        set_interp_result_str (interp, xlv_mk_context_str);

	return TCL_OK;
} /* end of xlv_make_plex() */


/*
 * Parse volume element command flags.
 *
 *	-start 
 *	-state 
 *	-stripe
 *	-stripe_unit
 *	-concatenate
 * 	-force
 *
 * If they are specified, the values are copied into ve.
 */

int
parse_ve_flags (Tcl_Interp		*interp,
		int			argc,
		char			*argv[], 
		int			*IO_arg_index,
		unsigned		*flags,
		xlv_tab_vol_elmnt_t	*ve)
{
	int	arg_index = *IO_arg_index;
	int	num;


	if (strcmp (argv[arg_index], "-start") == 0) {
		if (++arg_index >= argc) {
			set_interp_result (interp, XLV_STATUS_MISSING_VALUE,
				argv[arg_index-1]);
			return TCL_ERROR;
		}
	
		if (Tcl_GetInt (interp, argv[arg_index], &num)
			!= TCL_OK) {
			return TCL_ERROR;
		}

		if (num < 0) {
			set_interp_result (interp, XLV_STATUS_BAD_VALUE);
			return TCL_ERROR;
		}
	
		ve->start_block_no = (long long)num;
		*flags |= START;
		arg_index++;
	}
	else if (strcmp (argv[arg_index], "-force") == 0) {
		*flags |= FORCE;
		arg_index++;
	}
	else if (strcmp (argv[arg_index], "-state") == 0) {
		if (++arg_index >= argc) { 
			set_interp_result (interp, XLV_STATUS_MISSING_VALUE,
				argv[arg_index-1]);
			return TCL_ERROR;
		}

		if (!_isexpert(interp)) {
			set_interp_result (interp, XLV_STATUS_NO_PERMISSION);
			return TCL_ERROR;
		}

		for (num=0; num <= XLV_VE_STATE_INCOMPLETE; num++) {
			if (! strcmp(argv[arg_index],
			    xlv_ve_state_str((unsigned) num)))
				break;
		}
		if (num > XLV_VE_STATE_INCOMPLETE) {
			set_interp_result (interp, XLV_STATUS_BAD_VALUE);
			return TCL_ERROR;
		}
			
		ve->state = (unsigned char) num;
		arg_index++;
	}
	else if (strcmp (argv[arg_index], "-stripe") == 0) {
		arg_index++;
		ve->type = XLV_VE_TYPE_STRIPE;
	}
	else if (strcmp (argv[arg_index], "-stripe_unit") == 0) {
		if (++arg_index >= argc) { 
			set_interp_result (interp, XLV_STATUS_MISSING_VALUE,
				argv[arg_index-1]);
			return TCL_ERROR;
		}

		if (Tcl_GetInt (interp, argv[arg_index], &num)
			!= TCL_OK) {
                        return TCL_ERROR;
                }
                if (num < 0) {
                        set_interp_result (interp, XLV_STATUS_BAD_VALUE);
                        return TCL_ERROR;
                }

		ve->stripe_unit_size = (ushort_t) num;
		if (num != ve->stripe_unit_size) {
			/*
			 * dropped some bits in the int to short cast
			 */
                        set_interp_result (interp, XLV_STATUS_BAD_VALUE);
                        return TCL_ERROR;
		}
		ve->type = XLV_VE_TYPE_STRIPE;
                arg_index++;
	}
	else if ((strcmp (argv[arg_index], "-concat") == 0) ||
		 (strcmp (argv[arg_index], "-concatenate") == 0)) {
		arg_index++;
		ve->type = XLV_VE_TYPE_CONCAT;
	}
	else {
		set_interp_result (interp, XLV_STATUS_UNKNOWN_FLAG,
			argv[arg_index]);
		return TCL_ERROR;
	}

	*IO_arg_index = arg_index;
	return TCL_OK;
} /* end of parse_ve_flags() */


/*
 * Process a single disk partition.  This routine checks the partition
 * size and the tracksize.  
 *
 * min_partition_size is the size of the smallest partition that makes
 * up this volume element.  It is initialized internally in this routine.
 */

static int
process_disk_partition (Tcl_Interp		*interp,
			xlv_tab_vol_elmnt_t	*ve,
			char			*device_path,
			dev_t			dev,
			long			*O_min_partition_size)
{
	long long	lpart_size;
	long		part_size;


	if (ve->grp_size >= XLV_MAX_DISK_PARTS_PER_VE) {
		set_interp_result (interp, XLV_STATUS_TOO_MANY_PARTITIONS);
		return TCL_ERROR;
	}

	ASSERT (dev != makedev(0,0));
	xlv_fill_dp(&ve->disk_parts[ve->grp_size], dev);

	/*
	 * Make sure the partition is not in use by any other xlv
	 * objects.
	 */
	if (xlv_mk_disk_part_in_use (&xlv_mk_context, device_path, dev)) {
		set_interp_result (interp,
		    XLV_STATUS_DISK_PART_IN_USE, device_path);
		return TCL_ERROR;
	}

	/*
	 * Check the partition size. 
	 */
	lpart_size = findsize (device_path);
	if (lpart_size == -1LL || lpart_size > 0x7fffffffLL) {
		set_interp_result (interp, XLV_STATUS_CANT_FIND_PARTITION_SIZE,
		    device_path);
		return (TCL_ERROR);
	}
	part_size = (long)lpart_size;

	if (ve->type == XLV_VE_TYPE_CONCAT) {
		*O_min_partition_size = part_size;

		/* as with mkfs_efs, default to 128.  track size really has
		 * no valid meaning with scsi, and 128 is a reasonable default
		 * for striping. */
		ve->stripe_unit_size = (ushort_t) 128;

	}
	else {
		if (*O_min_partition_size != part_size) {
			if (*O_min_partition_size >= 0 && ! Tclflag) {
				printf(xlv_error_inq_text(
				       XLV_STATUS_UNEQUAL_PART_SIZE));
				printf("\n");
			}
			set_interp_result (interp,XLV_STATUS_UNEQUAL_PART_SIZE,
					   device_path);
			if (*O_min_partition_size > part_size || 
			    *O_min_partition_size < 0) {
				*O_min_partition_size = part_size;
			}
		}

		if (ve->stripe_unit_size == 0) {
			/* as with mkfs_efs, default to 128.  track size really has
			 * no valid meaning with scsi, and 128 is a reasonable default
			 * for striping. */
			ve->stripe_unit_size = (ushort_t) 128;
		}
	}

#ifdef DEBUG
#ifdef DISK_PARTNAME
	strncpy (ve->disk_parts[ve->grp_size].part_name,
	    device_path, XLV_NAME_LEN);
#endif
#endif

	ve->disk_parts[ve->grp_size].part_size = part_size;
	ve->grp_size++;

	return (TCL_OK);

} /* end of process_disk_partition() */


/*ARGSUSED*/
int
xlv_make_ve (ClientData clientData,
	     Tcl_Interp *interp,
	     int	argc,
	     char	*argv[])
{
	int			status;
	int			i;
	int			j;
	dev_t			dev_num;
	dev_t			dev_list[XLV_MAX_DISK_PARTS_PER_VE];
	xlv_tab_vol_elmnt_t	ve;
	xlv_tab_vol_elmnt_t     *prev_ve_in_plex = NULL;
	xlv_tab_plex_t		*plex;
	char			device_path[XLV_MAXDEVPATHLEN];
	struct stat		stat_buf;
	long			min_partition_size; /* smallest striped part */
	long long		cat_partition_size; /* total concat'ed parts */
	xlv_tab_subvol_t 	*svp;
	ve_table_entry_t 	*rowp;
	static ve_table_entry_t	*table = NULL;	    /* re-use */
	int			rows = 0 ;
	int			dev_count = 0;
	unsigned 		flags = 0;
	int			first_nonflag = 1;

	ASSERT (init_called);

	if (argc <= 1) {
		set_interp_result (interp, XLV_STATUS_DISK_PART_NEEDED);
		return TCL_ERROR;
	}

	min_partition_size = -1;
	cat_partition_size = 0;

	bzero (&ve, sizeof (xlv_tab_vol_elmnt_t));
	ve.type = XLV_VE_TYPE_CONCAT;	/* concatenate disks by default */
	ve.state = XLV_VE_STATE_EMPTY;

	/*
	 * We initialize the starting blockno of this volume element
	 * to be the next block after the previous volume element,
	 * if any.  This may be overridden by the user via the -start
	 * flag.
	 */
	plex = XLV_OREF_PLEX (&xlv_mk_context);
	svp = XLV_OREF_SUBVOL (&xlv_mk_context);
	if (svp && plex == NULL) {
		/*
		 * A subvolume has been defined but a plex has not
		 * been defined. This means that there is no mirroring.
		 * In this case just create the default plex 0.
		 */
		status = xlv_mk_plex (&xlv_mk_context, NULL, 0, 0);
		if (status != XLV_STATUS_OK) {
			set_interp_result (interp, status);
			return (TCL_ERROR);
		}
		plex = XLV_OREF_PLEX (&xlv_mk_context);
	}
	if ((plex != NULL) && (plex->num_vol_elmnts > 0)) {
		prev_ve_in_plex = &(plex->vol_elmnts[plex->num_vol_elmnts-1]);

		ve.start_block_no = prev_ve_in_plex->end_block_no + 1;
	}

	/*
	 * Process the elements of the commandline.
	 */

	i = 1;
	while (i < argc) {
		/*
		 * Must use while loop because parse_ve_flags()
		 * can increment "i".
		 */
		if (*argv[i] == '-') {

			status = parse_ve_flags (interp, argc, argv, &i,
			    &flags, &ve);
			if (status != TCL_OK)
				return (status);
			continue;
		}

		/*
	 	 * It is either a device name or a disk 
	 	 * partition name.
	 	 */
#if 0
		if (Tcl_ExprString (interp, argv[i]) != TCL_OK)
			return (TCL_ERROR);
#endif
		strcpy (interp->result, argv[i]);

		if ((strncmp (interp->result, "/dev/dsk/", 9) != 0) &&
		    (strncmp (interp->result, "/hw/", 4) != 0)) {

			/*
			 * Does not look like a device pathname.
			 * Try prepending /dev/dsk/ & see if it works.
			 */
			strcat (strcpy (device_path, "/dev/dsk/"),
			    interp->result);

		}
		else
			strcpy (device_path, interp->result);

		/*
		 * See if we really have a block device & see if it
		 * is mounted.
		 */

		if ((stat (device_path, &stat_buf) == 0) &&
		    (stat_buf.st_mode & S_IFBLK)) {

		        /* check if partition is already mounted */
		        if (mnt_check_state) {

			    if (mnt_find_mount_conflicts(
				mnt_check_state, device_path) > 0) {

				int result_type = XLV_STATUS_DISK_PART_MOUNTED;

				if (mnt_causes_test(mnt_check_state, MNT_CAUSE_OVERLAP))
				    result_type = XLV_STATUS_DISK_PART_OVERLAP;


				/*
				 * There is a mounted filesystem here.
				 */
				if (! flags&FORCE) {
					set_interp_result (interp,
					    result_type,
					    device_path);
					return TCL_ERROR;
				}
			    }
			}

			/*
			 * Make sure same device is not listed twice
			 */
			dev_num = stat_buf.st_rdev;
			for(j = 0;j < dev_count;j++) {
				if (dev_list[j] == dev_num) {
					set_interp_result ( interp,
						XLV_STATUS_SAME_DISK_PART);
					return TCL_ERROR;
				}
			}
			dev_list[dev_count++] = dev_num;

			/*
			 * Got a real block device node.
			 */
			status = process_disk_partition (interp, &ve, 
			    device_path, stat_buf.st_rdev, &min_partition_size);
			if (status != TCL_OK)
				return (status);

			first_nonflag = 0;
		}
		else if ( first_nonflag && (ve.veu_name[0] == '\0')) {

			/*
			 * If a name has not been set for this volume
			 * element, then let's assume that the user
			 * is creating a standalone volume element and
			 * that this is the volume element name.
			 * e.g., ve my_ve /dev/dsk/dks1d5s2.
			 *
			 * The other components, however, must be
			 * device names.
			 */

			/*
			 * XXX Check that the name is a valid name.
			 * Names should be alphanumeric, with the
			 * addition of underscores ("_").
			 *
			 * Note: Hard to check for something like "dks0d2s8".
			 */

			strncpy (ve.veu_name, interp->result, XLV_NAME_LEN);
		}
		else {
			set_interp_result (interp,
			    XLV_STATUS_UNKNOWN_DEVICE, argv[i]);
			return TCL_ERROR;
		}

		if (ve.type == XLV_VE_TYPE_CONCAT) {
			if (min_partition_size != -1) {
				cat_partition_size += min_partition_size;
			/*
			 * Reset so that the next iteration yields
			 * the partition size.
			 */
				min_partition_size = -1;
			}
		}

		i++;
	} /* finished processing command line */

	if (ve.grp_size < 1) {
		set_interp_result (interp, XLV_STATUS_DISK_PART_NEEDED);
		return TCL_ERROR;
	}

	if (ve.type == XLV_VE_TYPE_CONCAT) {
		/*
		 * Concatenating, so compute the offsets for using
		 * all the partitions.
		 */
		ve.end_block_no = ve.start_block_no + cat_partition_size - 1LL;
	}
	else {	/* ve.type == XLV_VE_TYPE_STRIPE */
		/*
		 * Striping. Disallow if:
		 *
		 * 	striped ve has only one partition
		 *	stripe or smallest partition is zero
		 * 	stripe is larger than the smallest partition
		 */
		if (ve.grp_size == 1) {
			set_interp_result (interp, XLV_STATUS_BAD_STRIPE);
			return TCL_ERROR;
		}
		if (! (ve.stripe_unit_size > 0) ) {
			set_interp_result (interp, XLV_STATUS_BAD_VALUE);
			return TCL_ERROR;
		}
		if (ve.stripe_unit_size > min_partition_size) {
			set_interp_result (interp, XLV_STATUS_STRIPE_TOO_BIG);
			return TCL_ERROR;
		}

		/*
		 * Make sure that the number of blocks is 
		 * an integral number of the stripe unit size.
		 */
		min_partition_size = (min_partition_size / ve.stripe_unit_size)
		    			* ve.stripe_unit_size;

		/*
		 * Compute the offsets for this volume element.
		 *
		 * Note: Must cast "min_partition_size" to a 64 bit int
		 * because (min_partition_size * ve.grp_size) can overflow
		 * a 32 bit int.
		 *
		 * XXX Should min_partition_size be a 64 bit int?
		 */
		ve.end_block_no = ve.start_block_no + 
		    ((long long)min_partition_size * ve.grp_size) - 1LL;
	}

	/*
	 * Check to make sure that the block numbers are increasing.
	 */
	if ((prev_ve_in_plex != NULL) &&
	    (ve.start_block_no <= prev_ve_in_plex->end_block_no) &&
	    (ve.veu_name[0] == '\0')) {
		set_interp_result (interp, XLV_STATUS_NON_INCREASING_BLKNO);
		return TCL_ERROR;
	}

	if ( _isforce(interp)) {
		flags |= FORCE;
	}
	if ( _isexpert(interp)) {
		flags |= EXPERT;
	}

	/*
	 * Check for overlapping entries.
	 */
	if (svp && (svp->num_plexes > 1)) {
		/*
		 * All volume elements in the same row
		 * must be the same size.
		 */
		if (table == NULL) {
			table = malloc(sizeof(*table) * XLV_MAX_VE_DEPTH);
			if (table == NULL) {
				set_interp_result (
					interp, XLV_STATUS_MALLOC_FAILED );
				return TCL_ERROR;
			}
		}

		rows = xlv_fill_subvol_ve_table(svp, table, XLV_MAX_VE_DEPTH);
		for (i = 0; i < rows; i++) {
			rowp = &table[i];
			/*
			 * Inserting - ve comes before this row.
			 */
			if ((ve.start_block_no < rowp->start_blkno) &&
			    (ve.end_block_no < rowp->start_blkno))
				break;	
			/*
			 * Mirroring - ve is in this row.
			 */
			if ((ve.start_block_no == rowp->start_blkno) &&
			    (ve.end_block_no == rowp->end_blkno))
				break;
			/*
			 * Next - ve is in a subsequent row.
			 */
			if ((ve.start_block_no > rowp->start_blkno) &&
			    (ve.start_block_no > rowp->end_blkno))
				continue;
			/*
			 * Error - ve overlaps this row.
			 */
			set_interp_result (interp, XLV_STATUS_OVERLAPPING_VE);
			return TCL_ERROR;
		}
	}

	/*
	 * Make sure that this disk part has not been used for another
	 * disk part in the same object (that may not have been finished
	 * yet.)
	 */
	if (prev_ve_in_plex != NULL) {
		/* XXX */
	}

	status = xlv_mk_ve (&xlv_mk_context, &ve, flags);
	if (status != XLV_STATUS_OK) {
		set_interp_result (interp, status);
		return (TCL_ERROR);
	}

	xlv_oref_to_string (&xlv_mk_context, xlv_mk_context_str);
	set_interp_result_str (interp, xlv_mk_context_str);

	XLV_MK_CTXT_SET_DIRTY(&xlv_mk_context);

	return TCL_OK;

} /* end of xlv_make_ve() */


/*ARGSUSED*/
int
xlv_make_show (ClientData clientData,
	       Tcl_Interp *interp,
	       int	  argc,
	       char	  *argv[])
{
	ASSERT (init_called);
	xlv_mk_print_all (&xlv_mk_context, _isverbose(interp));
	return TCL_OK;
}

/*ARGSUSED*/
int
xlv_make_clear (ClientData clientData,
	      Tcl_Interp *interp,
	      int	 argc,
	      char	 *argv[])
{
	ASSERT (init_called);
	XLV_OREF_INIT (&xlv_mk_context);
	return TCL_OK;
}


/*ARGSUSED*/
int
xlv_make_create (ClientData clientData,
	      Tcl_Interp *interp,
	      int	 argc,
	      char	 *argv[])
{
	int	count, status;
	int	verbose =  _isverbose(interp);
	int	object_inprogress = 0;
	char	*name;

	ASSERT (init_called);

	if (!xlv_oref_is_null(&xlv_mk_context)) {
		object_inprogress = 1;
		name = xlv_mk_context_name(&xlv_mk_context);
		printf("Cannot write incomplete object %s to disk.\n", name);
	}

	if ((status = xlv_mk_update_vh()) != XLV_STATUS_OK) {
		set_interp_result (interp, status);
		return TCL_ERROR;
	}

	status = xlv_mk_write_disk_labels(&count, verbose);
	if (verbose) {
		printf("Updated %d %s to disk.\n",
			count, (count==1)? "object":"objects");
	}
	if (status != XLV_STATUS_OK) {
		set_interp_result (interp, status);
		return TCL_ERROR;
	}

	/*
	 * All new objects have been saved to disk so clear
	 * the indicator so user can quit without being questioned
	 * about work in progress.
	 */
	if (object_inprogress == 0) {
		xlv_mk_new_obj_defined = 0;
	}

	return TCL_OK;
}


/*ARGSUSED*/
int
xlv_make_end (ClientData clientData,
	      Tcl_Interp *interp,
	      int	 argc,
	      char	 *argv[])
{
	int	status;

	ASSERT (init_called);
	status = xlv_mk_finish_curr_obj (&xlv_mk_context);
	set_interp_result (interp, status); 
	return TCL_OK;
}


/*ARGSUSED*/
int
xlv_make_exit (ClientData clientData,
	       Tcl_Interp *interp,
	       int	  argc,
	       char	  *argv[])
{
	int	status;
	int	count;
	int	verbose =  _isverbose(interp);

	ASSERT (init_called);

	if (! xlv_oref_is_null (&xlv_mk_context)) {
		set_interp_result (interp, XLV_STATUS_XLV_MK_QUIT);
		return TCL_OK;
	}
	if (xlv_mk_exit_check() == YES) {
		if ((status = xlv_mk_update_vh()) != XLV_STATUS_OK) {
			set_interp_result (interp, status);
			return TCL_ERROR;
		}
		status = xlv_mk_write_disk_labels(&count, verbose);

		/*
		 * There could have been an error updating labels
		 * of some objects but any new/changed objects should
		 * be immediately reflected in the kernel. So ignore
		 * the return status for now and assemble new geometry
		 * iff there are changes.
		 */
		if (count == 0 || _no_Assemble(interp)) {
			if (!Tclflag) {
				printf("xlv_assemble not invoked\n");
				fflush(stdout);
			}
		} else {
			/*
			 * XXX There should be a way to specify another
			 * path to xlv_assemble(1m).
			 */
			if (!Tclflag) {
				printf("Invoking xlv_assemble\n");
				fflush(stdout);
			}
			if (_isverbose(interp)) {
				system(_PATH_XLV_ASSEMBLE);
			} else {
				system(_PATH_XLV_ASSEMBLE " -q");
			}
		}

		/*
		 * Now look at the return status from writing labels.
		 */
		if (status != XLV_STATUS_OK) {
			set_interp_result (interp, status);
			return TCL_ERROR;
		} else { 
			Tcl_SetResult (interp, "end", TCL_VOLATILE);
			return TCL_OK;
		}
	}
	return TCL_OK;
} /* end of xlv_make_exit() */


/*ARGSUSED*/
int
xlv_make_quit (ClientData clientData,
	       Tcl_Interp *interp,
	       int	  argc,
	       char	  *argv[])
{
        ASSERT (init_called);

        /* 
         * Verify that quit is what is wanted
         */
	if (xlv_mk_quit_check(&xlv_mk_context) == YES)
		Tcl_SetResult (interp, "end", TCL_VOLATILE);
	return TCL_OK;
}

/*ARGSUSED*/
int
xlv_make_shell (ClientData clientData,
	       Tcl_Interp *interp,
	       int	  argc,
	       char	  *argv[])
{
        ASSERT (init_called);

	system("/sbin/sh");
	return TCL_OK;
}


/*
 * XXXjleong display more command help.
 */
const char *STR_VOL =
"vol volume_name    - Start definition of a volume named \"volume_name\".";

const char *STR_DATA =
"data               - Start definition of the data subvolume.";

const char *STR_LOG =
"log                - Start definition of the log subvolume.";

const char *STR_RT =
"rt                 - Start definition of the realtime subvolume.";

const char *STR_PLEX =
"plex [plex_name]   - Start definition a plex.  \"plex_name\" is specified\n"
"                     for a standalone plex.";

const char *STR_VE =
"ve [-start][-stripe][-stripe_unit N][-force][vol_element_name] partition(s)\n"
"                   - Define volume element partition(s).\n"
"                     \"vol_element_name\" is specified for a standalone ve.";

const char *STR_END =
"end                - Finished composing current object.";

const char *STR_CLEAR =
"clear              - Delete partially created object.";

const char *STR_CREATE =
"create             - Create newly defined objects by writing labels to disk.";

const char *STR_SHOW =
"show               - Show all objects.";

const char *STR_EXIT =
"exit               - Write labels and terminate session.";

const char *STR_QUIT =
"quit               - Terminate session without writing labels.";

const char *STR_HELP =
"help or ?          - Display this help message.";

const char *STR_SH =
"sh                 - Fork a shell.";

/*ARGSUSED*/
int
xlv_make_help (ClientData clientData,
	       Tcl_Interp *interp,
	       int	  argc,
	       char	  *argv[])
{
	int	idx;
	int	help_mask = 0;
	char	c, *opt;

	ASSERT (init_called);

#define HELP_VOL	0x0001
#define HELP_DATA	0x0002
#define HELP_LOG	0x0004
#define HELP_RT		0x0008
#define HELP_PLEX	0x0010
#define HELP_VE		0x0020
#define HELP_END	0x0040
#define HELP_CLEAR	0x0080
#define HELP_CREATE	0x0100
#define HELP_SHOW	0x0200
#define HELP_EXIT	0x0400
#define HELP_QUIT	0x0800
#define HELP_HELP	0x1000
#define HELP_SH		0x2000
#define HELP_MASK       0xFFFF

	for (idx=1; idx < argc; idx++) {
		c = argv[idx][0];
		opt = argv[idx];
		if ((c == 'v') && 0 == strcmp(opt, "vol")) {
			help_mask |= HELP_VOL;
		} else if ((c == 'd') && 0 == strcmp(opt, "data")) {
			help_mask |= HELP_DATA;
		} else if ((c == 'l') && 0 == strcmp(opt, "log")) {
			help_mask |= HELP_LOG;
		} else if ((c == 'r') && 0 == strcmp(opt, "rt")) {
			help_mask |= HELP_RT;
		} else if ((c == 'p') && 0 == strcmp(opt, "plex")) {
			help_mask |= HELP_PLEX;
		} else if ((c == 'v') && 0 == strcmp(opt, "ve")) {
			help_mask |= HELP_VE;
		} else if ((c == 'e') && 0 == strcmp(opt, "end")) {
			help_mask |= HELP_END;
		} else if ((c == 'c') && 0 == strcmp(opt, "clear")) {
			help_mask |= HELP_CLEAR;
		} else if ((c == 'c') && 0 == strcmp(opt, "create")) {
			help_mask |= HELP_CREATE;
		} else if ((c == 's') && 0 == strcmp(opt, "show")) {
			help_mask |= HELP_SHOW;
		} else if ((c == 'e') && 0 == strcmp(opt, "exit")) {
			help_mask |= HELP_EXIT;
		} else if ((c == 'q') && 0 == strcmp(opt, "quit")) {
			help_mask |= HELP_QUIT;
		} else if ((c == 'h') && 0 == strcmp(opt, "help")) {
			help_mask |= HELP_HELP;
		} else if ((c == 's') && 0 == strcmp(opt, "sh")) {
			help_mask |= HELP_SH;
		}
	}

	if (help_mask == 0) {
		help_mask = HELP_MASK;
	}

	if (help_mask & HELP_VOL)
		printf("%s\n", STR_VOL);
	if (help_mask & HELP_DATA)
		printf("%s\n", STR_DATA);
	if (help_mask & HELP_LOG)
		printf("%s\n", STR_LOG);
	if (help_mask & HELP_RT)
		printf("%s\n", STR_RT);
	if (help_mask & HELP_PLEX)
		printf("%s\n", STR_PLEX);
	if (help_mask & HELP_VE)
		printf("%s\n", STR_VE);
	if (help_mask & HELP_END)
		printf("%s\n", STR_END);
	if (help_mask & HELP_CLEAR)
		printf("%s\n", STR_CLEAR);
	if (help_mask & HELP_CREATE)
		printf("%s\n", STR_CREATE);
	if (help_mask & HELP_SHOW)
		printf("%s\n", STR_SHOW);
	if (help_mask & HELP_EXIT)
		printf("%s\n", STR_EXIT);
	if (help_mask & HELP_QUIT)
		printf("%s\n", STR_QUIT);
	if (help_mask & HELP_HELP)
		printf("%s\n", STR_HELP);
	if (help_mask & HELP_SH)
		printf("%s\n", STR_SH);

	return TCL_OK;
}

