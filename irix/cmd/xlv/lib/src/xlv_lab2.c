/**************************************************************************
 *                                                                        *
 *           Copyright (C) 1993-1994, Silicon Graphics, Inc.              *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.97 $"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <sys/param.h>
#include <diskinvent.h>
#include <sys/iograph.h>
#include <syslog.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bstring.h>
#include <invent.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <pathnames.h>
#include <sys/xlv_vh.h>
#include <xlv_oref.h>
#include <xlv_lab.h>
#include <xlv_oref.h>
#include <xlv_utils.h>
#include <xlv_cap.h>


/*
 * By default attempt to make labels consistent if we find
 * they aren't
 */
boolean_t	xlv_label_recovery	= B_TRUE;

/*
 * Given a list of volume headers and a pathname, determines whether
 * the volume header for the named device is in the list.
 * If so, return its address.
 */
static xlv_vh_entry_t *xlv__lab2_vh_incore (
	xlv_vh_entry_t	*vh_list,
	char *vhpathname);

typedef struct {
        xlv_vh_entry_t  **vh_list_p;
        int             status;
	int		mode;
} xlv_lab2_read_arg_t;

static int xlv__lab2_commit_labels (
	xlv_vh_entry_t	*vh_list,
	int		freelist,
	int		partial_ok);

static int xlv__lab2_update_labels (xlv_vh_entry_t *vh_list);

static int read_vol_elmnt (
	xlv_oref_t	    *I_oref,
	xlv_lab2_read_arg_t *read_arg);

static int xlv__lab2_purge_labels (xlv_vh_entry_t *vh_list);

static int xlv__lab2_update_labels_nodename (
	xlv_vh_entry_t	*vh_list,
	xlv_name_t	nodename);

static void xlv__lab2_write_lab (
	xlv_vh_disk_label_entry_t *lab,
	xlv_oref_t		  *oref,
	time_t		  	  *timestamp);


#if 0
static void xlv__lab2_lock (void);
static void xlv__lab2_unlock (void);
#endif

static void xlv__lab2_new_trans_id (unsigned long long *trans_id);
static int disk_walk(char *, struct stat64 *, inventory_t *, int, void *);
#ifdef NO_LONGER_USED
static int controller_walk(char *, struct stat64 *, inventory_t *, int, void *);
#endif



/*
 * Log the message to syslog.
 */
void
slogit(int priority, char *fmt, ...)
{
	va_list args;
	char	str[1024];

	va_start(args, fmt);
	vsprintf(str, fmt, args);
	va_end(args);

#ifdef DEBUG
	fprintf(stderr, "DBG: %s\n", str);
#endif
	xlv_openlog("xlv", LOG_PID | LOG_NOWAIT | LOG_CONS | LOG_PERROR, 
		    LOG_USER);
	syslog(LOG_USER | priority, str);
	xlv_closelog();
} /* slogit() */


/*
 * We attempt to keep open file descriptors for all volume headers of
 * interest, to avoid opening and closing. However, since there may be
 * many disk parts in a logical volume and we process multiple logical
 * volumes at a time, we may run out.  To avoid gobbling up all the
 * available fd's for this process, we will start closing file descriptors
 * once we reach a preset limit. We must also verify that the fd is
 * open prior to calling an xlv_lab1 routine.
 */

static unsigned int _maxfds = XLV_MINFDS;

int
xlv_set_maxfds(int max)
{
	struct rlimit	rl;
	unsigned long	total;		/* max opened vh + fluff */
	unsigned long	delta;

	if (max <= _maxfds) {		/* no shrinkage allowed */
		/* NOP */
		goto ret_maxfds;
	}

	if (0 != getrlimit(RLIMIT_NOFILE, &rl)) {
		/*
		 * Cannot set new limit if we cannot get the current
		 * limit or if we can exceed the hard limit.
		 */
		goto ret_maxfds;
	}

	if (max > XLV_MAXFDS) {
		max = XLV_MAXFDS;
	} else if (max < XLV_MINFDS) {
		max = XLV_MINFDS;
	}

	total = max + XLV_FDS_FOR_APPL;

	if (total > rl.rlim_cur) {
		if (total > rl.rlim_max) {
			/*
			 * Cannot grow as much as the caller wants,
			 * but grow as much a possible.
			 */
			delta = total - (ulong_t)rl.rlim_max;
			total -= delta;
			max -= delta;	/* grow as much as we can */
			if (max <= _maxfds) {
				/* don't allow shrinkage */
				goto ret_maxfds;
			}
		}
		rl.rlim_cur = total;
		if (0 != setrlimit(RLIMIT_NOFILE, &rl)) {
			goto ret_maxfds;
		}
	}

	_maxfds = max;		/* New maximum fds */

ret_maxfds:
	return(_maxfds);
}

/*
 * See comments below in xlv__lab2_purge_fds().
 */
static int xlv_lab2_nfds = 0;

static void
xlv__lab2_purge_fds (xlv_vh_entry_t     *vh_list)
{
        xlv_vh_entry_t                  *vh;

        for (vh = vh_list; vh; vh = vh->next) {
		if (vh->vfd >= 0) {
#ifdef DEBUG_NFDS
printf("DBG: Reclaiming fd=%d (%s)", vh->vfd, vh->vh_pathname);
#endif
			xlv_lab1_reclaim_fd (vh);
        		if (xlv_lab2_nfds > 0) {
				xlv_lab2_nfds--;
#ifdef DEBUG_NFDS
printf(" ==> done, --xlv_lab2_nfds=%d\n", xlv_lab2_nfds);
#endif
			} else {
				/* going negative? */
				ASSERT(0);
			}
		}
        }

	/*
	 * This one counter is used to track file descriptors
	 * from multiple vh lists. If there is only one outstanding
	 * vh list then this code is correct. Otherwise setting it
	 * to zero would cause us to lose track of the other file
	 * descriptors in the other lists. This is okay for now
	 * because our current open file descriptor limit is pretty
	 * low compared to the process limit. 
	 *
	 * XXX Open file descriptor counter should be part of list context.
         *
	 * xlv_lab2_nfds = 0;
	 */
}



/*
 * This is called after the volume header has been opened.
 */
static void
xlv__lab2_inc_open_fds (xlv_vh_entry_t	*vh_list)
{
	xlv_lab2_nfds++;

	if (xlv_lab2_nfds >= _maxfds)
		xlv__lab2_purge_fds (vh_list);
}


/*
 * Returns 1 if the fd for the vh_entry is ok to use.
 */
static int
xlv__lab2_check_fd (xlv_vh_entry_t      *vh_entry,
                    xlv_vh_entry_t      *vh_list)
{
        if (vh_entry->vfd >= 0)
                return 1;

        if (xlv_lab2_nfds > _maxfds)
                xlv__lab2_purge_fds (vh_list);

        if (xlv_lab1_restore_fd (vh_entry))
                return 0;
	xlv_lab2_nfds++;

        return 1;
}


/*
 * Look at an xlv_tab_vol_entry and write
 * the labels that describe the geometry out to all the disk
 * parts that make up the volume.
 * Because of the hierarchical structure of XLV volumes, it
 * is always possible to find the 'highest' object that is
 * changed.  Often this will be the entire volume.  However,
 * it can be a subvolume, plex, or even just a disk part.
 * To minimize the amount of disk labels that we touch, we
 * pass in the object handle of the 'highest' node that's
 * affected.
 *
 * This routine assumes that xlv_tab and xlv_tab_vol are
 * consistent.
 *
 * This routine is called from mkxlv (user level command),
 * and also from the xlv_admin process.
 * 
 * This routine will also generate a new sequence number.
 *
 * Since new volume labels may be read in, *vh_list will point
 * to the new list head.  Note that the contents of vh_list are 
 * not freed on either success or failure.
 */
void
xlv_lab2_write (xlv_vh_entry_t	    **vh_list,
		xlv_tab_vol_entry_t *tab_vol_entry,
		uuid_t		    *obj_uuid,
		int 		    *status)
{
	xlv_oref_t		oref;
	xlv_lab2_read_arg_t	read_arg;

	XLV_OREF_INIT (&oref);
	XLV_OREF_SET_UUID (&oref, obj_uuid);

	if (xlv_oref_resolve (&oref, tab_vol_entry) != 0)
		ASSERT (0);	/* Inconsistent entry in xlv_tab_vol */
	
	/*
	 * For each device, open the label if it's not already 
	 * incore.  If the disk is an xlv disk part, read its label in.
	 * Otherwise, initialize the xlv label on the disk.  
	 * Note that the inner-loop routine will link each vh header
	 * that is read into the vh_list.
	 *
	 * After reading the label in, the object reference is
	 * linked onto the label so that we can merge the new
	 * geometry information into the disk label in 
	 * xlv__lab2_update_labels.
	 */

	read_arg.vh_list_p = vh_list;
	read_arg.status = 0;
	read_arg.mode = XLV_LAB_WRITE_FULL;
	xlv_for_each_ve_in_obj (&oref, (XLV_OREF_PF) read_vol_elmnt,
				(void *) &read_arg);
	if (*status = read_arg.status) {
		return;
	}

	/* 
	 * Update the incore volume headers (and the on-disk labels)
	 * with the updated volume geometry. 
	 * This routine steps through all the incore volume headers,
	 * and examines all the [new] object references attached to 
	 * the each vh entry (done in read_vol_elmnt).
	 */
	if (*status = xlv__lab2_update_labels (*vh_list)) {
		return;
	}

	/*
	 * Write the master block to make the new labels active.
	 */
	*status = xlv__lab2_commit_labels (*vh_list, YES, XLV_LAB_WRITE_FULL);

} /* end of xlv_lab2_write() */


/*
 * Takes 2 xlv_oref_t and write the geometry that describes
 * the objects to all the disk parts that make them up.
 *
 * We take 2 objects because when we do dynamic change, we
 * may be moving a piece from one object to another.  (Since
 * each move operation is atomic, we can never have a operation
 * that involves more than 3 objects.)
 *
 * This routine is called from xlv_make and xlv_admin commands.
 *
 * This routine will also generate a new sequence number.
 */
void
xlv_lab2_write_oref (
	xlv_vh_entry_t		**vh_list,
        xlv_oref_t              *oref1,
        xlv_oref_t              *oref2,
        int                     *status,
	int			mode)
{
	xlv_lab2_read_arg_t     read_arg;

	/*
	 * Go through every disk part that make up the objects.  For
	 * each disk partition, read in the volume header.  If the
	 * disk partition is already part of an XLV object, read its
	 * XLV label in.  Otherwise, initialize a new XLV label.
	 *
	 * Note that the inner-loop routine will link each vh header
	 * that is read into vh_list.
	 */

	if (oref1 != NULL) {
		read_arg.vh_list_p = vh_list;
		read_arg.status = 0;
		read_arg.mode = mode;
		xlv_for_each_ve_in_obj (oref1, (XLV_OREF_PF) read_vol_elmnt,
					(void *) &read_arg);
		if (*status = read_arg.status) {
			return;
		}
	}

	if (oref2 != NULL) {
		read_arg.vh_list_p = vh_list;
                read_arg.status = 0;
		read_arg.mode = mode;
		xlv_for_each_ve_in_obj (oref2, (XLV_OREF_PF) read_vol_elmnt,
					(void *) &read_arg);
                if (*status = read_arg.status) {
                        return;
                }
	}


        /*
         * Update the incore volume headers (and the on-disk labels)
         * with the updated volume geometry.
         * This routine steps through all the incore volume headers,
         * and examines all the [new] object references attached to
         * the each vh entry (done in read_vol_elmnt).
         */
        if (*status = xlv__lab2_update_labels (*vh_list)) {
                return;
        }

	/*
	 * Write the master block to make the new labels active.
	 */
   	*status = xlv__lab2_commit_labels (*vh_list, YES, mode);

} /* end of xlv_lab2_write_oref() */


/*
 * Update the node name in the XLV disk label header on all the
 * disks that are associated with the list of objects.
 */
void
xlv_lab2_write_nodename_oref (
	xlv_vh_entry_t		**vh_list,
        xlv_oref_t              *oref_list,
	xlv_name_t		nodename,
        int                     *status)
{
	xlv_lab2_read_arg_t     read_arg;
	xlv_oref_t		*oref;
	int			retval;

	if (oref_list == NULL) {
		*status = 0;
		return;
	}

	/*
	 * Parse the oref list and determine which disks
	 * need to be updated.
	 */
	for (oref = oref_list; oref != NULL; oref = oref->next) {
		/*
		 * Generate the list of disks which
		 * are assoicated with this object.
		 */
		read_arg.vh_list_p = vh_list;
		read_arg.status = 0;
		read_arg.mode = XLV_LAB_WRITE_FULL;
		xlv_for_each_ve_in_obj (
			oref, (XLV_OREF_PF)read_vol_elmnt, (void *)&read_arg );
		
		/*
		 * Stop at the slightest hint of problems.
		 *
		 * XXXjleong Need to do some cleanup on the "xlv_tab_list"
		 * field in the vh_list. orefs where malloc'ed.
		 */
		if (retval = read_arg.status) {
			goto done_nodename;
		}
	}

	/*
	 * Update the XLV labels on each of these disks.
	 */
	retval = xlv__lab2_update_labels_nodename (*vh_list, nodename);

	if (0 == retval) {
   		retval = xlv__lab2_commit_labels (
				*vh_list, YES, XLV_LAB_WRITE_FULL);
	}

	/*FALLTHRU*/

done_nodename:
	*status = retval;

} /* end of xlv_lab2_write_nodename_oref() */


/*
 * Takes the passed-in component, which is associated with the
 * xlv_oref_t, and writes its values to all the disk parts that
 * make up that component.
 *
 * This routine is called from xlv_admin commands to change
 * component (subvol, plex, ve) specific values.
 *
 * This routine will also generate a new sequence number.
 */
void
xlv_lab2_write_oref_component (
	xlv_vh_entry_t		**vh_list,
        xlv_oref_t              *oref1,
	xlv_obj_type_t		ocomponent1,
        xlv_oref_t              *oref2,
	xlv_obj_type_t		ocomponent2,
        int                     *status,
        int                     mode)
{
        xlv_oref_t              *oref;
	xlv_obj_type_t		otype;
	xlv_lab2_read_arg_t     read_arg;

	if (oref1 == NULL) {
		*status = 0;
		return;
	}

	oref = oref1;
	otype = ocomponent1;

process_oref:

	/*
	 * Go through every disk part that make up the object component.
	 * For each disk partition, read in the volume header.  If the
	 * disk partition is already part of an XLV object, read its
	 * XLV label in.  Otherwise, initialize a new XLV label.
	 *
	 * Note that read_vol_elmnt() will link each vh header
	 * that is read into vh_list.
	 */

	read_arg.vh_list_p = vh_list;
	read_arg.status = 0;
	read_arg.mode = mode;

	switch (otype) {
	case XLV_OBJ_TYPE_VOL:
		xlv_for_each_ve_in_vol (
			oref, (XLV_OREF_PF)read_vol_elmnt, (void *)&read_arg);
		break;

	case XLV_OBJ_TYPE_SUBVOL:
	case XLV_OBJ_TYPE_LOG_SUBVOL:
	case XLV_OBJ_TYPE_DATA_SUBVOL:
	case XLV_OBJ_TYPE_RT_SUBVOL:
		(void) xlv_for_each_ve_in_subvol (
			oref, (XLV_OREF_PF)read_vol_elmnt, (void *)&read_arg);
		break;

	case XLV_OBJ_TYPE_PLEX:
		(void) xlv_for_each_ve_in_plex (
			oref, (XLV_OREF_PF)read_vol_elmnt, (void *)&read_arg);
		break;

	case XLV_OBJ_TYPE_VOL_ELMNT:
		if (XLV_OREF_VOL_ELMNT(oref) == NULL)
			ASSERT(0);
		else
			(void) read_vol_elmnt (oref, &read_arg);
		break;

	default:
		*status = 1;		/* don't handle this case */
	} /* switch */

	if (*status = read_arg.status) {
		return;
	}

	if (oref2) {
		oref = oref2;
		otype = ocomponent2;
		oref2 = NULL;		/* terminate loop */
		goto process_oref;
	}

        /*
         * Update the incore volume headers (and the on-disk labels)
         * with the updated volume geometry.
         * This routine steps through all the incore volume headers,
         * and examines all the [new] object references attached to
         * the each vh entry (done in read_vol_elmnt).
         */
        if (*status = xlv__lab2_update_labels (*vh_list)) {
                return;
        }

	/*
	 * Write the master block to make the new labels active.
	 */
   	*status = xlv__lab2_commit_labels (*vh_list, YES, mode);

} /* xlv_lab2_write_oref_component() */


/*
 * Purge all disk labels associated with the given component.
 *
 * This routine is called from xlv_admin commands to change
 * component (subvol, plex, ve) specific values.
 *
 * This routine will also generate a new sequence number.
 */
void
xlv_lab2_remove_oref_component (
	xlv_vh_entry_t		**vh_list,
        xlv_oref_t              *oref,
	xlv_obj_type_t		ocomponent,
        int                     *status,
        int                     mode)
{
	xlv_lab2_read_arg_t     read_arg;

	if (oref == NULL) {
		*status = 0;
		return;
	}

	read_arg.vh_list_p = vh_list;
	read_arg.status = 0;
	read_arg.mode = mode;

	/*
	 * Go through every disk part that make up the object component.
	 * For each disk partition, read in the volume header.
	 *
	 * Note that read_vol_elmnt() will link each vh header
	 * that is read into vh_list.
	 */

	switch (ocomponent) {
	case XLV_OBJ_TYPE_VOL:
		xlv_for_each_ve_in_vol (
			oref, (XLV_OREF_PF)read_vol_elmnt, (void *)&read_arg);
		break;

	case XLV_OBJ_TYPE_SUBVOL:
	case XLV_OBJ_TYPE_LOG_SUBVOL:
	case XLV_OBJ_TYPE_DATA_SUBVOL:
	case XLV_OBJ_TYPE_RT_SUBVOL:
		(void) xlv_for_each_ve_in_subvol (
			oref, (XLV_OREF_PF)read_vol_elmnt, (void *)&read_arg);
		break;

	case XLV_OBJ_TYPE_PLEX:
		(void) xlv_for_each_ve_in_plex (
			oref, (XLV_OREF_PF)read_vol_elmnt, (void *)&read_arg);
		break;

	case XLV_OBJ_TYPE_VOL_ELMNT:
		if (XLV_OREF_VOL_ELMNT(oref) == NULL)
			ASSERT(0);
		else
			(void) read_vol_elmnt (oref, &read_arg);
		break;

	default:
		*status = 1;		/* don't handle this case */
	} /* switch */

	if (*status = read_arg.status) {
		return;
	}

        /*
         * Purge object from the on-disk labels. This routine
	 * steps through all the incore volume headers, and
	 * examines all the object references attached to
         * the each vh entry (done in read_vol_elmnt).
         */
        if (*status = xlv__lab2_purge_labels (*vh_list)) {
                return;
        }

	/*
	 * Write the master block to make the new labels active.
	 */
   	*status = xlv__lab2_commit_labels (*vh_list, YES, mode);

} /* xlv_lab2_remove_oref_component() */


static int
read_vol_elmnt (xlv_oref_t		*I_oref,
		xlv_lab2_read_arg_t	*read_arg)
{
	xlv_oref_t        	*oref_p;
	xlv_tab_vol_elmnt_t    	*ve;
	int                    	d;
	xlv_tab_disk_part_t	*dp;
	xlv_vh_entry_t		*vh_entry, **vh_list_p;
	char			*vh_pathname;
	int			status;
	int			mode;

	char	fmt_no_init[] = "Cannot initialize label on %s\n";
#ifdef DEBUG
	char	fmt_no_open[] = "Cannot open %s\n";
#endif

	vh_list_p = read_arg->vh_list_p;
	mode = read_arg->mode;

	ve = XLV_OREF_VOL_ELMNT (I_oref);
	ASSERT (ve != NULL);

	status = -1;
	for (d = 0, dp = ve->disk_parts; d < ve->grp_size; d++, dp++) {

		if (0 == DP_DEV(*dp)) {
			/*
			 * The device does not exist. Must be an
			 * incomplete object.
			 */
			continue;
		}

		vh_pathname = pathtovhpath (devtopath (DP_DEV(*dp), B_FALSE));

		if (vh_pathname == NULL) {
			/*
			 * The volume header pathname could not be generated.
			 * We don't know how to map dev_t to a pathname.
			 */
			continue;
		}

		/*
		 * Check if the volume header is already in core.  If so,
		 * get the volume header from the incore table.
		 */

		vh_entry = xlv__lab2_vh_incore(*vh_list_p, vh_pathname);

		/*
		 * If the label is not incore, read it in and link it
		 * into vh_list.  Under normal operation, the label should
		 * already be incore.  However such a situation can occur
		 * if the label is deleted from the disk vh after the volume
		 * is assembled into the kernel.  Xlv_shutdown(1m) can
		 * be caught in this situation.
		 */
		if (vh_entry == NULL) {
#ifdef DEBUG
			fprintf(stderr, "DBG: read_vol_elmnt() add vh %s\n",
				vh_pathname);
#endif
			vh_entry = (xlv_vh_entry_t *) malloc (
				sizeof (xlv_vh_entry_t));
			bzero (vh_entry, sizeof (xlv_vh_entry_t));
			vh_entry->vfd = -1;

			/* 
			 * Read the label in and link it into vh_list.
			 */
			if (xlv_lab1_open(vh_pathname, dp, vh_entry)) {
#ifdef DEBUG
				xlv_error(fmt_no_open,
					  pathtoalias(vh_pathname));
#endif
				free(vh_entry);
				if (mode == XLV_LAB_WRITE_PARTIAL) {
					continue;
				} else {
                                	goto done;
				}
			}
			xlv__lab2_inc_open_fds(*vh_list_p);

			/*
                         * If the disk does not already have an XLV label,
                         * write one.
                         */
                        if (vh_entry->xlv_label == NULL) {
				if (xlv_lab1_init(vh_entry)) {
                                        xlv_error(fmt_no_init,
						  pathtoalias(vh_pathname));
					free(vh_entry);
					if (mode == XLV_LAB_WRITE_PARTIAL) {
						continue;
					} else {
                                		goto done;
					}
                                }
			}

		        /*
                         * Add the incore volume header to the head of the
                         * vh list.
                         */
			vh_entry->next = *vh_list_p;
			*vh_list_p = vh_entry;
		}
		else {
			/*
			 * The label is in core. There are several cases:
			 *   1) there is no XLV label.  In this case,
			 *      write one.
			 *   2) there is an XLV label and it was read in 
			 *      from the xlv_lab file.  (The file only
			 *      stores XLV labels.)  In this case, reread
			 *      the label from the volume header.
			 *   3) the XLV label was read in from the volume
			 *      header.  In this case, do nothing.
			 */

			/*
			 * If the disk does not already have an XLV label,
			 * write one.
			 */
			if (vh_entry->xlv_label == NULL) {
				/*
				 * Case 1 above.
				 */
				if (xlv_lab1_init(vh_entry)) {
					xlv_error(fmt_no_init,
						  pathtoalias(vh_pathname));
					goto done;
				}
			}
#ifdef VER1_LABELS
			else {
				xlv_vh_disk_label_t     *label;
				void			*ptr;

				/*
				 * A label that's read in from the xlv_lab
				 * file has only a primary copy, no secondary.
				 */
				label = XLV__LAB1_SECONDARY(vh_entry);
				if (label == NULL) {
					/*
					 * Case 2 above.
					 *
					 * 9/7/95 The xlv_lab file does not
					 * exist anymore so this case should
					 * never happen.
					 */
					ASSERT(0);

					/*
					 * Save a pointer to the current
					 * label because xlv_lab1_open will
					 * create a new one and overwrite
					 * "xlv_label" pointer. Don't free
					 * the current label until a new is
					 * created.
					 *
					 * Note: xlv_label & label0_p were
					 * allocated out of the same malloc
					 * chunk.
					 */
					ptr = vh_entry->xlv_label;

					/*
					 * Read label into the vh_entry that
					 * is already in the vh_list.
					 */
					if (xlv_lab1_open(vh_pathname, dp,
							  vh_entry)) {
#ifdef DEBUG
						xlv_error(fmt_no_open,
							  pathtoalias(vh_pathname));
#endif
						goto done;
					}
					xlv__lab2_inc_open_fds(*vh_list_p);
					free (ptr);
				}
				else {
					/*
					 * Confirm vol header has been read.
					 */
					ASSERT(vh_entry->dev_bsize > 0);
				}
			}
#endif
		}

		/*
		 * Link this object reference into the vh_entry so that we
		 * can locate the relevant information from xlv_tab later.
		 */
		oref_p = (xlv_oref_t *) malloc (sizeof(xlv_oref_t));
		XLV_OREF_COPY (I_oref, oref_p);
		XLV_OREF_SET_DISK_PART (oref_p, dp);
		oref_p->next = vh_entry->xlv_tab_list;
		vh_entry->xlv_tab_list = oref_p;

	}
	status = 0;

done:
	read_arg->status = status;
	return (0);

} /* end of read_vol_elmnt() */


/*
 * Routines to get and set the node name of the local system.
 * The node name identifies the ownership of a XLV label.
 */
static int	_nodename_inited = 0;		/* know local node name? */
static char	_nodename[_SYS_NMLN];		/* local system node name */

char *
xlv_getnodename(void)
{
	static struct utsname	system_name;

	if (! _nodename_inited) {
		if (0 > uname (&system_name)) {
			ASSERT(0);
		}
		strcpy(_nodename, system_name.nodename);
		_nodename_inited++;
	}

	return _nodename;
}

int
xlv_setnodename(char *name)
{
	char	*np;

	strncpy(_nodename, name, _SYS_NMLN);

	/*
	 * Don't include domainname.
	 */
	np = strchr(_nodename, '.');
	if (np)
		*np = '\0';

	_nodename_inited++;

	return(_nodename_inited);
}


/*
 * Go through the list of volume headers.  Process the object
 * references attached to each and remove their corresponding
 * label.
 *
 * Note: This function is very much like xlv__lab2_update_labels()
 * except this function is removing a label as oppose to writing
 * a new label. See xlv__lab2_update_labels() for useful algorithm
 * comments.
 */
static int
xlv__lab2_purge_labels (xlv_vh_entry_t *vh_list)
{
	unsigned long long	trans_id;
	xlv_vh_entry_t		*vh;
	xlv_oref_t		*oref;
	xlv_vh_disk_label_t	*primary_label, *label;
	xlv_vh_disk_label_entry_t *lab;
	xlv_tab_disk_part_t	*dp;
	int			i;
#ifdef VER1_LABELS    
	int			last;
#endif

	/* 
	 * Generate a new transaction identifier to identify all the
	 * disk labels that participated in this operation.
	 */
	xlv__lab2_new_trans_id (&trans_id);

	for (vh = vh_list; vh != NULL; vh = vh->next) {
		/*
		 * Only purge labels with obj ref's hanging off them.
		 */
		if (vh->xlv_tab_list == NULL)
			continue;

		/*
		 * Copy the primary to the secondary label and use it.
		 */
		primary_label = XLV__LAB1_PRIMARY (vh);
		label = XLV__LAB1_SECONDARY (vh);

#ifndef VER1_LABELS
		/*
		 * The xlv_lab filesystem file is not supported so we
		 * must have read this label from the disk header.
		 * The secondary label must exist.
		 */
		ASSERT(label);
#else
		/*
		 * The secondary label is null if it was read in
		 * from the xlv_lab file instead of the disk header.
		 */
		if (label == NULL) {
			char	*chunk;

			/* skip master block and point to the first label */
			chunk = (char *)(vh->xlv_label) +
				roundblk(sizeof(xlv_vh_master_t), DEV_BSIZE);

			if (primary_label == vh->label0_p) {
				/* Bump up to the next label slot. */
				chunk += roundblk(sizeof(xlv_vh_disk_label_t),
						  DEV_BSIZE);
				label = (xlv_vh_disk_label_t *)chunk;
				vh->label1_p = label;
			} else {
				/* This should not be the case! */
				ASSERT(primary_label == vh->label0_p);
				label = (xlv_vh_disk_label_t *)chunk;
				vh->label0_p = label;
			}
		}
#endif

		bcopy (primary_label, label, sizeof(xlv_vh_disk_label_t));

		/*
		 * Now the primary and secondary labels are the same. 
		 * Make all updates in-place using the secondary label.
		 */
		label->header.transaction_id = trans_id;

#if 0
		/*
		 * The primary must have the nodename set because
		 * an existing entry is being removed. When the entry
		 * was added the nodename should have been set.
		 */
		ASSERT(strlen(label->header.nodename));
#else
		if (0 == strlen(label->header.nodename)) {
			/*
			 * The primary didn't have the nodename set so
			 * this is probably the first write. Set the
			 * nodename here.
			 */
			strncpy (label->header.nodename, xlv_getnodename(), 
			         XLV_NODENAME_LEN);
		}
#endif
		
		/*
		 * Go thru the list of disk parts that are to be removed.
		 */
		for (oref = (xlv_oref_t *) vh->xlv_tab_list;
		     oref != NULL; oref = oref->next) {

			ASSERT(XLV_OREF_RESOLVED(oref, XLV_OBJ_TYPE_DISK_PART));
			dp = XLV_OREF_DISK_PART (oref);

			/*
			 * Find the existing partition's label.
			 */
			for (i = 0, lab = label->label_entry; 
			     i < XLV_MAX_DISK_PARTS;
			     i++, lab++) {
#if 0
			    	if (!uuid_equal(
				    &lab->disk_part.uuid, &dp->uuid)) {
					continue;
				}
#else
				/* XXX This should really be something else. */
			    	if (lab->disk_part.dev != DP_DEV(*dp))
					continue;
#endif
				ASSERT(label->header.version != 1);
				bzero(lab,sizeof(*lab));
				label->header.disk_parts_on_disk--;
				break;	/* finished this one */
			}
			if (i >= XLV_MAX_DISK_PARTS) {
				fprintf(stderr,
				"Warning: No label update for devno %#x\n",
					DP_DEV(*dp));
				ASSERT(0);
			}

			/*
			 * Do some cleanup when there are no more disk
			 * label entries. Remove the nodename so a future
			 * disk label addition can set the nodename. The
			 * nodename can be changed at that time.
			 */
			if (0 == label->header.disk_parts_on_disk) {
				label->header.nodename[0] = '\0';
			}

		} /* for vh->xlv_tab_list */

	} /* for vh_list */

	return (0);

} /* end of xlv__lab2_purge_labels() */


/*
 * xlv__lab2_update_labels_nodename()
 * Change the nodename which is found in the label header.
 * Upon completion the primary and the secondary labels are
 * particularly the same. The only difference is the nodename.
 * 
 * Note that the timestamp of the disk part is not changed.
 */
static int
xlv__lab2_update_labels_nodename (
	xlv_vh_entry_t	*vh_list,
	xlv_name_t	nodename)
{
	unsigned long long	trans_id;
	xlv_vh_entry_t		*vh;
	xlv_vh_disk_label_t	*primary_label, *label;

	xlv__lab2_new_trans_id (&trans_id);	/* identify all the players */ 

	for (vh = vh_list; vh != NULL; vh = vh->next) {
		/*
		 * Only update labels with obj ref's hanging off them.
		 *
		 * Note that we don't care how many oref's are hanging
		 * off of "xlv_tab_list" because we just want to update
		 * the nodename which only appears in the label header.
		 */
		if (vh->xlv_tab_list == NULL)
			continue;

		primary_label = XLV__LAB1_PRIMARY (vh);
		label = XLV__LAB1_SECONDARY (vh);

		/*
		 * Copy the primary label to the secondary label.
		 * Both labels should be the same. The only difference
		 * is the nodename in the header.
		 */
		bcopy (primary_label, label, sizeof(xlv_vh_disk_label_t));

		label->header.transaction_id = trans_id;

		/*
		 * Change the node name -- this is all we really
		 * want to do.
		 */
		strncpy (label->header.nodename, nodename, XLV_NODENAME_LEN);
	}

	return (0);

} /* end of xlv__lab2_update_labels_nodename() */


/*
 * Go through the list of volume headers.  Process the object
 * references attached to each and update them with the new
 * labels.
 */
static int
xlv__lab2_update_labels (xlv_vh_entry_t *vh_list)
{
	unsigned long long	trans_id;
	xlv_vh_entry_t		*vh;
	xlv_oref_t		*oref;
	xlv_vh_disk_label_t	*primary_label, *label;
	xlv_vh_disk_label_entry_t *lab;
	xlv_tab_disk_part_t	*dp;
	time_t			timestamp;
	uint_t			status;

	/* Generate a new transaction identifier for this update. 
	 * This will identify all the disk labels that participated
	 * in this operation.
	 */

	xlv__lab2_new_trans_id (&trans_id);

	timestamp = time(NULL);

	for (vh = vh_list; vh != NULL; vh = vh->next) {
		/*
		 * Only update labels with obj ref's hanging off them.
		 */
		if (vh->xlv_tab_list == NULL)
			continue;

		/*
		 * We want to write to the backup/secondary copy of
		 * label.  First, we copy the primary to the secondary.
		 */
		primary_label = XLV__LAB1_PRIMARY (vh);
		label = XLV__LAB1_SECONDARY (vh);

		/*
		 * The secondary label could be null if it was read in
		 * from the xlv_lab file instead of the disk header.
		 * In this case, we "malloc" a secondary label. We do it
		 * at this point instead of at the point the label was
		 * read in from the xlv_lab file since most of those
		 * labels will not be modified.
		 *
		 * NOTE:
		 *	The xlv_lab file allocated a big enough chunk
		 * for everything so don't need to really do a malloc.
		 * Just put the secondary label, after the primary label.
		 * 	xlv_lab2_free_vh_list() uses &vh->xlv_label
		 * to free the whole chunk (master and both labels).
		 */
		if (label == NULL) {
			/*
			 * xlv_labfile_read_vh_list() always put the
			 * read-in label in label0_p.
			 */
			char	*chunk;

			/* skip master block and point to the first label */
			chunk = (char *)(vh->xlv_label) +
				roundblk(sizeof(xlv_vh_master_t), DEV_BSIZE);

			if (primary_label == vh->label0_p) {
				/*
				 * Bump up to the next label slot.
				 */
				chunk += roundblk(sizeof(xlv_vh_disk_label_t),
						  DEV_BSIZE);
				label = (xlv_vh_disk_label_t *)chunk;
				vh->label1_p = label;
			} else {
				/*
				 * This should not be the case!
				 */
				ASSERT(primary_label == vh->label0_p);
				label = (xlv_vh_disk_label_t *)chunk;
				vh->label0_p = label;
			}
		}

		bcopy (primary_label, label, sizeof(xlv_vh_disk_label_t));

		/* At this point, the secondary label contains everything
		 * that was in the primary and we can make all updates 
		 * in-place using the secondary label.
		 */
		label->header.transaction_id = trans_id;

		/*
		 * Don't blindly copy the local nodename name into the
		 * the label. Fill in the nodename iff it isn't already set.
		 * This way a disk configurated on another system won't
		 * have its name removed.
		 *
		 * Note: This can change after we develop some protocol
		 * to take ownership of this disk.
		 */
		if (0 == strlen(label->header.nodename)) {
			/*
			 * This is probably the initial label entry write.
			 */
			strncpy (label->header.nodename, xlv_getnodename(), 
			         XLV_NODENAME_LEN);
		}
		

		/* Go through the list of disk parts that are modified
		 * as part of this update and write them to the incore
		 * volume header entry.
		 */
		for (oref = (xlv_oref_t *) vh->xlv_tab_list;
		     oref != NULL; oref = oref->next) {

			ASSERT(XLV_OREF_RESOLVED(oref, XLV_OBJ_TYPE_DISK_PART));

			/*
			 * See if we are rewriting an existing
			 * partition's label. If this is a new
			 * disk part, increase the count.
			 */

			dp = XLV_OREF_DISK_PART (oref);
			/* XXXsbarr - this dev_t to partnum conversion is
                           a real performance hit - we should find a way to
                           eliminate it */
			lab = &(label->label_entry[dev_to_partnum(DP_DEV(*dp))]);
			if (uuid_is_nil(&lab->disk_part.uuid, &status)) {
				/*
				 * Make sure all labels get upgraded
				 * to latest version.
				 */
				label->header.version = XLV_VH_VERS;
		   		label->header.magic = XLV_VH_MAGIC_NUM;
		   		label->header.disk_parts_on_disk++;
			}
			/*
			 * XXX If the uuid is not nil should there be
			 * a check that the uuid is equal?
			 */
			   
			/* Copy the information from xlv_tab into the label. */
			xlv__lab2_write_lab (lab, oref, &timestamp);
		}

	}

	return (0);
} /* end of xlv__lab2_update_labels() */


static void
xlv__lab2_write_lab (xlv_vh_disk_label_entry_t	*lab,
		     xlv_oref_t			*oref,
		     time_t			*timestamp)
{
	xlv_tab_vol_entry_t	*v = XLV_OREF_VOL(oref);
	xlv_tab_subvol_t	*sv = XLV_OREF_SUBVOL(oref);
	xlv_tab_plex_t		*p = XLV_OREF_PLEX(oref);
	xlv_tab_vol_elmnt_t	*ve = XLV_OREF_VOL_ELMNT(oref);
	xlv_tab_disk_part_t	*dp = XLV_OREF_DISK_PART(oref);
	int			i;
	unsigned		st;

	if (v) {
		/*
		 * If a volume is defined, then all of the pieces that
		 * make it up should be also.
		 */
		ASSERT (sv && p && ve && dp);

		lab->vol.flag = v->flags;
		COPY_UUID (v->uuid, lab->vol.volume_uuid);
		if (v->log_subvol != NULL) {
			COPY_UUID (v->log_subvol->uuid,
				   lab->vol.log_subvol_uuid);
		} else {
			uuid_create_nil (&lab->vol.log_subvol_uuid, &st);
		}
		if (v->data_subvol != NULL) {
			COPY_UUID (v->data_subvol->uuid,
				   lab->vol.data_subvol_uuid);
		} else {
			uuid_create_nil (&lab->vol.data_subvol_uuid, &st);
		}
		if (v->rt_subvol != NULL) {
			COPY_UUID (v->rt_subvol->uuid, lab->vol.rt_subvol_uuid);
		} else {
			uuid_create_nil (&lab->vol.rt_subvol_uuid, &st);
		}
		COPY_NAME (v->name, lab->vol.volume_name);

		lab->vol.subvol_flags = XLV_SUBVOL_FLAGS_SAVE(sv->flags);
		lab->vol.subvol_padding = 0;
		lab->vol.subvol_timestamp = *timestamp;

		/*
		 * Note that we do not support stand-alone subvolumes.
		 */

		lab->vol.sector_size = v->sector_size;
		lab->vol.subvolume_type = sv->subvol_type;

		/*
		 * XXX 10/4/94	Is sv->subvol_depth valid? Should use
		 * it instead of recalculating the depth every time.
		 * Recalculating here is safe but slow.
		 *
		 * Note: Can change if we can be sure xlv_make(1m) and
		 * xlv_admin(1m) set/updates the depth. Also need to
		 * be sure the disk labels have the depth value set.
		 * The depth wouldn't be set if it is the old style
		 * labels -- as is the case with Beta 1.
		 */
		lab->vol.subvol_depth = xlv_tab_subvol_depth(sv);
		lab->vol.reserved = 0;
	}
	else {
		/*
		 * We are just defining a piece of a volume.
		 * It's either a plex or volume element.
		 * Note that we do not support stand-alone subvolumes.
		 */
		bzero (&(lab->vol), sizeof(xlv_vh_vol_t));
	}


	if (p) {
		/*
		 * If we have a plex, then the volume elements and
		 * the disk parts that make it up must be defined
		 * also.
		 */
		ASSERT (ve && dp);

		COPY_UUID (p->uuid, lab->plex.uuid);
		COPY_NAME (p->name, lab->plex.name);

		if (sv) {
			lab->plex.num_plexes = sv->num_plexes;
			for (i=0; i < sv->num_plexes; i++) {
				if (sv->plex[i] == p) {
					lab->plex.seq_num = i;
					break;
				}
			}
		}
		else {
			lab->plex.num_plexes = 1;
			lab->plex.seq_num = 0;
		}
 
		lab->plex.num_vol_elmnts = p->num_vol_elmnts;
		lab->plex.flags = p->flags;
	}
	else {
		/*
		 * Must be a standalone volume element or disk part.
		 */
		bzero (&(lab->plex), sizeof(xlv_vh_plex_t));
	}

	if (ve) {
		ASSERT (dp);

		COPY_UUID (ve->uuid, lab->vol_elmnt.uuid);
		COPY_NAME (ve->veu_name, lab->vol_elmnt.name);
		lab->vol_elmnt.type = ve->type;
		lab->vol_elmnt.state = ve->state;
		lab->vol_elmnt.grp_size = ve->grp_size;

		if (p) {
			for (i=0; i < p->num_vol_elmnts; i++) {
				if (&p->vol_elmnts[i] == ve) {
					lab->vol_elmnt.seq_num_in_plex = i;
					break;
				}
			}
		}
		else
			lab->vol_elmnt.seq_num_in_plex = 0;

		lab->vol_elmnt.stripe_unit_size = ve->stripe_unit_size;
		lab->vol_elmnt.start_block_no = ve->start_block_no;
		lab->vol_elmnt.end_block_no = ve->end_block_no;
	}
	else {
		bzero (&(lab->vol_elmnt), sizeof(xlv_vh_vol_elmnt_t));
	}

	/*
	 * A disk part must always be defined.
	 */
	ASSERT (dp);

	COPY_UUID (dp->uuid, lab->disk_part.uuid);

	/* XXXsbarr: ignore this dev for now, it will get clobbered in lab1 */
	/* 11/27/96	xlv_lab1_write() ignores this label entry dev_t
	 * and inserts the partition number into the label entry.  This
	 * makes it okay to migrate the disk back to a non-hardware graph
	 * system.
	 */
	lab->disk_part.dev = DP_DEV(*dp);

	lab->disk_part.size = dp->part_size;
	for (i=0; i < ve->grp_size; i++) {
		if (&ve->disk_parts[i] == dp) {
		    	lab->disk_part.seq_in_grp = i;
		    	break;
		}
	}
	lab->disk_part.state = XLV_VH_DISK_PART_STATE_GOOD;	/* gotta be */

} /* end of xlv__lab2_write_lab() */


/*
 * Commit the label changes by bumping the master block sequence number. 
 */
static int
xlv__lab2_commit_labels (
	xlv_vh_entry_t	*vh_list,
	int		freeoref,	/* free oref hanging off vh */
	int		mode)		/* okay to commit some but not all */
{
	xlv_vh_entry_t		*vh, *vh_back;
	boolean_t		backout_error;
	xlv_oref_t		*oref_p;

	/*
	 * Now that all the incore labels have been constructed, we
	 * write them out to disk.
	 */
	for (vh = vh_list; vh != NULL; vh = vh->next) {

		/*
		 * Only write labels that were modified.
		 * These are labels with obj ref's hanging off them.
		 */
		if (vh->xlv_tab_list == NULL)
			continue;

	    	/*
		 * We want to write to the secondary (backup) copy of the
		 * label.  After all the writes have completed, we will
		 * commit the changes by making the secondary copies the
		 * primary.
		 */
	    	if (xlv__lab2_check_fd (vh, vh_list) && xlv_lab1_write (vh)) {
	        	/*
			 * We got an error.  But it's ok since we have
	        	 * not updated the master block yet.
			 *
			 * Note:  This means that some of the backup label
			 * on disk may be bad. But this should be okay cuz
			 * the backup labels are not used.
			 */
			if (mode == XLV_LAB_WRITE_FULL) {
		    		xlv_error ("Unable to write label on %s: %s\n",
					   pathtoalias(vh->vh_pathname),
					   strerror(errno));
	        		return (-1);
			}

			/*
			 * Partial update is acceptable but the oref
			 * hanging off the vh should be free so we
			 * won't bother with this vh again.
			 */ 
		    	xlv_error ("Unable to write label on %s: %s; "
				   "continue with next vh\n",
				   pathtoalias(vh->vh_pathname),
				   strerror(errno));
			while (vh->xlv_tab_list != NULL) {
				oref_p = vh->xlv_tab_list;
				vh->xlv_tab_list = oref_p->next;
				free(oref_p);
			}
	    	}
	}

	for (vh = vh_list; vh != NULL; vh = vh->next) {

		/*
                 * Only write labels that were modified.
                 * These are labels with obj ref's hanging off them.
                 */
                if (vh->xlv_tab_list == NULL)
                        continue;

		/*
		 * Bump the sequence number in the master block to
		 * make the current label the primary.
		 */
		vh->xlv_label->vh_seq_number++;
		if (xlv__lab2_check_fd (vh, vh_list) &&
		    xlv_lab1_write_master_blk (vh) ) {
		    	xlv_error ("Unable to complete label on %s\n",
				   pathtoalias(vh->vh_pathname));

			ASSERT (mode == XLV_LAB_WRITE_FULL);
		    	/*
			 * Here, we have a choice, either try to back out the
		         * changes already made, or go on.  We choose to 
			 * backout the changes.
			 */
		    	backout_error = B_FALSE;
		    	for (vh_back = vh_list; vh_back != vh; 
		             vh_back = vh_back->next) {
				if (vh_back->xlv_tab_list != NULL) {
					vh_back->xlv_label->vh_seq_number--;
					if (xlv__lab2_check_fd (vh, vh_list) &&
					    xlv_lab1_write_master_blk (vh) ) 
						backout_error = B_TRUE;
				}
		    	}
		    	if (backout_error) {
				xlv_error (
			    "Unable to restore previous state of volume\n");
		    	}
		    	else {
				xlv_error (
			    "Volume successfully restored to previous state\n");
		    	}

		    	return (-1);
		} /* finished bumping seq num in master block */

		/*
		 * If the system crashes before all master sectors are
		 * updated, xlv_lab2_make_consistent() will do the
		 * recovery.
		 */
	}

	if (freeoref != YES)
		return (0);

	for (vh = vh_list; vh != NULL; vh = vh->next) {
		/*
                 * The labels with obj ref's hanging off them
		 * have been committed so clear the list by
		 * freeing the disk part object which were
		 * created (in read_vol_elmnt).
		 */
		while (vh->xlv_tab_list != NULL) {
			oref_p = vh->xlv_tab_list;
			vh->xlv_tab_list = oref_p->next;
			free(oref_p);
		}
	}

	return (0);

} /* end of xlv__lab2_commit_labels() */


#include <sys/time.h>

static void
xlv__lab2_new_trans_id (
	unsigned long long	*trans_id)
{
	static unsigned long long	internal_trans_id = 0;
	struct timeval	tp;
	struct timezone	tz;

	if (internal_trans_id == 0) {
		if (gettimeofday (&tp, &tz)) {
		    	ASSERT (0);
		}
		internal_trans_id = tp.tv_sec;
		internal_trans_id = (internal_trans_id << 32) | tp.tv_usec;
	}
	*trans_id = ++internal_trans_id;
}



/*
 * Given a device pathname, looks in vh_list to see if the device
 * volume header has already been read-in. If the device is in the
 * volume header list, return the corresponding vh_entry.
 * Otherwise return NULL and the caller must call xlv_lab1_open() to
 * get the device on the volume header list.
 *
 * Device vh pathnames looks like:
 *   /hw/module/1/slot/io1/baseio/pci/0/scsi_ctlr/0/target/5/lun/0
 *       /disk/volume_header/char
 */
static xlv_vh_entry_t *
xlv__lab2_vh_incore (xlv_vh_entry_t	*vh_list,
		     char		*vhpathname)
{
	xlv_vh_entry_t	*vp;

	for (vp = vh_list; vp != NULL; vp = vp->next) {

		/* XXXsbarr: we're guaranteed only to have 1 path via the new
                   kernel based failover code (io/failover.c).  We should
                   eventually remove the extra 3 dev_t's in the dp struct
                   if XLV1 sticks around */

		/*
		 * Compare volheader device name
		 */
		if (!strcmp(vp->vh_pathname, vhpathname)) {
			goto end_vp_loop;
		}
	}

end_vp_loop:
	return (vp);
}


/*
 * Free all the vh_entries in IO_vh_list.
 */
void
xlv_lab2_free_vh_list (xlv_vh_entry_t	**IO_vh_list)
{
	xlv_vh_entry_t	*vh_entry, *next;

	xlv__lab2_purge_fds (*IO_vh_list);

	for (vh_entry = *IO_vh_list; vh_entry != NULL; vh_entry = next) {
		next = vh_entry->next;
		if (vh_entry->xlv_label != NULL) {
			free (vh_entry->xlv_label);
		}
		free (vh_entry);
	}
	*IO_vh_list = NULL;
}


#ifdef CONVERT_OLD_XLVLAB
int
xlv_convert_old_labels(xlv_vh_entry_t *vh_entry, int flags)
{
	int		i;
	unsigned	st;
	dev_t		partition;
	int		partition_type;
	dev_t		lbl_devnum;
	int		subvol_type;

	xlv_vh_entry_t		  tmp_vh_entry;
	xlv_vh_disk_label_t	  *label;
	xlv_vh_disk_label_entry_t *lep;
	xlv_vh_disk_label_t	  dummy_label;
	xlv_oref_t		  dummy_oref[XLV_MAX_DISK_PARTS];
	xlv_tab_disk_part_t	  dummy_disk_part;
	xlv_ptype_update_list_t	  ptype_update_list[2];

	char	*Bad;
	char	BadLog[]="Log ve should have partition type xlv";
	char	BadData[]="Data ve should have partition type xlv";
	char	BadRt[]="Rt ve should have partition type xlv";
	char	BadStandalone[]="Standalone object should have partition type xlv";
	char	BadVh[]="Unable to write to volume header";
	char	warn_template[]="Warning: Partition %s%s. %s.%s";
	char	warn_template2[]="Warning: Partition %s%s. %s.\nUpdating volume header.%s";
	char	err_template[]="Rejecting partition %s%s. %s.%s";

	/*
	 * If old style label, update to new style now
	 */
	bcopy(vh_entry,&tmp_vh_entry,sizeof(xlv_vh_entry_t));
	label = XLV__LAB1_PRIMARY(vh_entry);
	tmp_vh_entry.label0_p = &dummy_label;
	tmp_vh_entry.label1_p = &dummy_label;
	bzero(&dummy_label,sizeof(dummy_label));
	bcopy(label,&dummy_label,sizeof(xlv_vh_header_t));
	tmp_vh_entry.next = NULL;
	tmp_vh_entry.xlv_tab_list = (void *)dummy_oref;
	for (i = 0;i < XLV_MAX_DISK_PARTS;i++) {
		lep = &(label->label_entry[i]);
		if (uuid_is_nil(&(lep->disk_part.uuid),&st)) {
			break;
		}
		/*
		 * Decide if standalone or not
		 */
		if (uuid_is_nil (&(lep->vol.volume_uuid),&st)) {
			subvol_type = XLV_SUBVOL_TYPE_UNKNOWN;
		} else {
			subvol_type = lep->vol.subvolume_type;
		}

		lbl_devnum = lep->disk_part.dev;
		partition = dev_to_partnum(lbl_devnum);
		partition_type = vh_entry->vh.vh_pt[partition].pt_type;

		/*
		 * Match partition type against subvolume type
		 */
		if (xlv_check_partition_type(subvol_type,partition_type,NO)
		    == -1) {
			switch (subvol_type) {
			case XLV_SUBVOL_TYPE_LOG:
				Bad = BadLog;
				break;
			case XLV_SUBVOL_TYPE_DATA:
				Bad = BadData;
				break;
			case XLV_SUBVOL_TYPE_RT:
				Bad = BadRt;
				break;
			case XLV_SUBVOL_TYPE_UNKNOWN:
				Bad = BadStandalone;
				break;
			}

			if (flags&XLV_READ_NO_CONVERT) {
				xlv_print_type_check_msg(warn_template,
					vh_entry->vh_pathname,Bad,lbl_devnum,
					partition,NULL);
				continue;
			} else {
				xlv_print_type_check_msg(warn_template2,
					vh_entry->vh_pathname,Bad,lbl_devnum,
					partition,NULL);
			}
			/*
			 * Update the volume header now 
			 */
			strcpy(ptype_update_list[0].vh_pathname,
			       vh_entry->vh_pathname);
			ptype_update_list[0].num_partitions = 1;
			ptype_update_list[1].num_partitions = 0;
			ptype_update_list[0].partition[0] = partition;
			/*
			 * All subvolumes have partition type "xlv".
			 */
			ptype_update_list[0].partition_type[0] = PTYPE_XLV;

			if (xlv_lab1_update_ptype(&ptype_update_list[0]) == -1){
				xlv_print_type_check_msg(err_template,
					vh_entry->vh_pathname, BadVh,
					lbl_devnum,partition,NULL);
				return -1;
			}
		}

		dummy_oref[i].next = &dummy_oref[i+1];
		xlv_fill_dp(&dummy_disk_part,
			    label->label_entry[i].disk_part.dev);
		dummy_oref[i].disk_part = &dummy_disk_part;
		dummy_label.label_entry[dev_to_partnum(DP_DEV(*dummy_oref[i].disk_part))] =
			label->label_entry[i];
	}
	if (!(flags&XLV_READ_NO_CONVERT)) {
		dummy_oref[i].next = NULL;
		dummy_label.header.version = XLV_VH_VERS;
		if (xlv__lab2_commit_labels (
				&tmp_vh_entry, NO, XLV_LAB_WRITE_FULL) == -1) {
			return -1;
		}
	}
	return 0;

} /* end of xlv_convert_old_labels() */
#endif	/* CONVERT_OLD_XLVLAB */


/*****************************************************************/
/*
 * Routines below are for reading labels during system startup.
 */
/*****************************************************************/
/*
 * Locates all the disks attached to the system, read their
 * labels, filters out partial updates, and create xlv_vol_tab
 * and xlv_tab tables that describes the geometry of all the 
 * logical volumes on this system.
 *
 * Error messages will be printed on the console if orphaned
 * disks are discovered.
 */
void
xlv_lab2_read (
	xlv_vh_entry_t		**vh_list,
	xlv_tab_vol_t		*xlv_tab_vol,
	xlv_tab_t		*xlv_tab,
	int			flags,
	int			*status)
{
	xlv_vh_entry_t		*vh_list_ptr;
	xlv_vh_disk_label_t	*label;
	xlv_vh_entry_t		*orphaned_list;
	xlv_vh_entry_t		*tmp_vh_list;
	boolean_t		labels_converted = B_FALSE;

	tmp_vh_list = NULL;
	xlv_lab2_create_vh_info_from_hinv (&tmp_vh_list, NULL, status);
	for (vh_list_ptr = tmp_vh_list;vh_list_ptr != NULL; vh_list_ptr = vh_list_ptr->next) {
		label = XLV__LAB1_PRIMARY(vh_list_ptr);
		if (label->header.version == 1) {
#ifdef CONVERT_OLD_XLVLAB
			xlv_convert_old_labels(vh_list_ptr, flags);
			labels_converted = B_TRUE;
#else
			ASSERT(0);
#endif
		}
	}

	/*
	 * xlv_convert_old_labels modifies vh_list.  So if we call it,
	 * read in a new vh_list.
	 */
	if (labels_converted) {
		xlv_lab2_destroy_vh_info(&tmp_vh_list, status);
		*vh_list = NULL;
		xlv_lab2_create_vh_info_from_hinv (vh_list, NULL, status);
	}
	else
		*vh_list = tmp_vh_list;

	if (*vh_list != NULL) {
		/*
		 * Check that the labels and disk parts are
		 * consistent. Convert from on disk labels to in-core
		 * data structures. In the process of converting,
		 * check for volumes from other systems. Also
		 * create XLV device nodes for each of the
		 * subvolume, and a directory entry for each
		 * volume.
		 */
		xlv_vh_to_xlv_tab (*vh_list, xlv_tab_vol,  xlv_tab,
				   &orphaned_list, flags, status);
	}
#ifdef DEBUG
	else
		fprintf (stderr, "DBG: No XLV volumes on system\n");
#endif
}


/*
 * Sanity check the populated xlv_tab_vol and xlv_tab.
 * Equivalent of fsck.
 */
/* ARGSUSED0 */
void
xlv_lab2_check (
	xlv_vh_entry_t *vh_list,
        xlv_tab_vol_t *xlv_tab_vol,
        xlv_tab_t *xlv_tab,    
        int *status)
{
    	*status = 0;

	/* XXXjleong:	xlv_lab2_check() is a stub. */
}


#if 0
/*
 * Routines internal to xlv_lab2_read/write.
 */
/*
 * We need to make sure that only one label operation is in
 * progress at any given moment.
 */
static void
xlv__lab2_lock (void)
{
	/* XXX */
}

static void
xlv__lab2_unlock (void)
{
	/* XXX */
}
#endif

#ifdef NOT_HWGRAPH
/* This entire feature no longer works courtesy of the hwgraph */
/* XXXsbarr: the call to this function has been commented out */

/*
 * Scan the list of volume headers and determine if any disk migrated.
 * The disk migrated when the controller and driver/unit number in dev_t
 * stored in the label entry does not match that of the volume header
 * from which the label was read.
 *
 * If a disk has migrated, try to do recovery. Generate the correct
 * dev_t by taking the volume header's controller and unit numbers
 * and concatenating them with the partition number of the dev_t in
 * label entry. The resulting dev_t is checked and created if it does
 * not exist.
 *
 * Note:
 *	If recovery is not possible, set the disk part's state to
 * indicate that there is no dev_t so the label won't be used.
 */

void
xlv_dev_make_consistent (
	xlv_vh_entry_t	*vh_list,
	int		*status)
{
	xlv_vh_entry_t		  *vh;
	xlv_vh_disk_label_t	  *label;
	xlv_vh_disk_label_entry_t *lep;			/* label entry ptr */
	char			  pn[MAXDEVPATHLEN];	/* pathname */
	dev_t			  disk_dev;	/* vol header's ctlr & unit */
	dev_t			  part_dev;	/* label entry's ctlr & unit */
	int			  i;
	struct stat		  buf;
	char			  msg[1024];
	unsigned		  st;


	for (vh = vh_list; vh != NULL; vh = vh->next) {
		label = XLV__LAB1_PRIMARY (vh);

		if (label->header.magic != XLV_VH_MAGIC_NUM) {
			/* XXXjleong Should this disk go on a orphan list? */
			continue;
		}

		/*
		 * Get the controller and drive/unit numbers for the disk
		 * by getting the volume header's dev_t and masking out
		 * the partition number.
		 */
		disk_dev = CTLR_UNITOF(name_to_dev(vh->vh_pathname));

		for (i=0; i < XLV_MAX_DISK_PARTS; i++) {
			lep = &label->label_entry[i];
			if (uuid_is_nil (&(lep->vol_elmnt.uuid), &st)) {
				/*
				 * entry does not reference a volume element
				 * so don't bother with the check
				 */
				continue;
			}

			/*
			 * Check to see if we have the right dev.
			 */
			part_dev = CTLR_UNITOF(lep->disk_part.dev);
			if (disk_dev == part_dev)
				continue;
			
			/*
			 * The disk migrated so generate the correct dev_t.
			 */
			part_dev = disk_dev | (lep->disk_part.dev & PART_MASK);
			strcpy (pn, devtopath(part_dev, 0));

			/* hwgraph dev_t's change, so the fixed ordering
			 * assumption doesn't work here
			 */
			sprintf(msg,
				"Disk part %s (%d,%d) migrated to %s (%d,%d)", 
				devtopath(lep->disk_part.dev, 0),
				major(lep->disk_part.dev),
				(unsigned)lep->disk_part.dev & L_MAXMIN,
				pn, major(part_dev),
				(unsigned)part_dev & L_MAXMIN);
			slogit(LOG_INFO, "%s", msg);
			if (stat(pn, &buf) < 0) {
				if (cap_mknod(pn, S_IFBLK|S_IREAD|S_IWRITE,
					      part_dev) < 0) {
					sprintf(msg, "Cannot create %s", pn);
					perror(msg);
					lep->disk_part.state =
						XLV_VH_DISK_PART_STATE_NODEV; 
					continue;
				};
				sprintf(msg, "Created device: %s (%d,%d)",
					pn, major(part_dev),
					(unsigned)part_dev & L_MAXMIN);
				slogit(LOG_INFO, "%s", msg);
			}
			else if (buf.st_rdev != part_dev) {
				sprintf(msg, "Ignore %s: In use by (%d,%d)",
					pn, major(buf.st_rdev),
					(unsigned)buf.st_rdev & L_MAXMIN);
				slogit(LOG_INFO, "%s", msg);
				lep->disk_part.state =
					XLV_VH_DISK_PART_STATE_NODEV; 
				continue;
			}
			/*
			 * Save the correct dev_t and pathname.
			 */
			lep->disk_part.dev = part_dev;
		}
	}

	*status = 0;

} /* end of xlv_dev_make_consistent() */
#endif	/* NOT_HWGRAPH */


typedef struct xlv_host_s {
	unsigned long long	max_id;		/* high transaction id */
	char			*name;		/* system name */
} xlv_host_t;

static int _label_recovery = 0;			/* label made consistent */

/*
 * Scan the disk labels in disk_info.  Figures out what the
 * last transaction was, complete it if it was incomplete.
 *
 * Algorithm:
 *   1. get max transaction_seq_number.
 *      NOTE: IF a disk that contains the max trans seq number
 *      is offline & then comes back. !!!!
 *	Tricky to generate new seq_numbers; we'll use a timestamp.
 *   2. go through disk_info.  If transaction_seq_number !=
 *      max_transaction_seq_number, read in transaction
 *      sequence number from the secondary label.  If seq
 *      number in secondary label == max_transaction_seq_number,
 *      read that label in.
 *      Also, write the master blk to point to the secondary 
 *      label.
 *
 * This relies on there being at most 1 outstanding transaction
 * at a time.
 *
 * The output is a disk_info list (that's consistent).
 *
 * Note:
 *	Skip volume headers with bad primary and secondary labels.
 */ 
void
xlv_lab2_make_consistent (
	xlv_vh_entry_t *vh_list,
	int	       *status)
{
#define	MAX_HOSTS		5
#define	TRANS_ID(labelp)	((labelp)->header.transaction_id)

	xlv_vh_entry_t		*vh;
	xlv_vh_disk_label_t	*prime, *back;
	xlv_host_t		*hosts;
	int			h_len, h_cnt, h_max;
	int			idx;
	char			*apn;	/* alias for vh pathname */

	h_cnt = 0;
	h_max = MAX_HOSTS;
	h_len = sizeof(xlv_host_t) * MAX_HOSTS;
	hosts = (xlv_host_t *)malloc(h_len);
	bzero(hosts, h_len);

	/*
	 * Make first pass through the volume header list to
	 * find the latest transaction identifier for each host.
	 */
	for (vh = vh_list; vh != NULL; vh = vh->next) {
		prime = XLV__LAB1_PRIMARY (vh);
		back = XLV__LAB1_SECONDARY (vh);

		/*
		 * Check for a valid XLV label.
		 */
		if (prime->header.magic != XLV_VH_MAGIC_NUM) {
			/*
			 * Primary label is bad, Look to secondary for help.
			 * If the secondary label is good, use the nodename.
			 * Set the trans id to zero so algorithm can work.
			 */
			if (back->header.magic == XLV_VH_MAGIC_NUM) {
				strcpy(prime->header.nodename,
					back->header.nodename);
				prime->header.transaction_id = 0; 
				/* XXX But the 2nd pass ignores this label */
			} else {
				xlv_error("Ignore version %d label on %s for label check.\n",
					  prime->header.magic,
					  pathtoalias(vh->vh_pathname));
				continue;	/* skip this label */
			}
		}

		/*
		 * Find the associate table entry for the host.
		 */
		for (idx = 0; idx < h_cnt; idx++) {
			if (!strcmp(prime->header.nodename, hosts[idx].name))
				break;
		}
		if (idx == h_cnt) {
			/*
			 * Label is not in table, so add it.
			 */
			if (h_cnt >= h_max) {
				/* host table needs to grow */
				xlv_host_t	*h_tmp;
				int		oldlen;

				h_max = h_max + MAX_HOSTS; 
				oldlen = h_len;
				h_len = sizeof(xlv_host_t) * h_max;
				h_tmp = (xlv_host_t *)malloc(h_len);
				bcopy((char *)hosts, (char *)h_tmp, oldlen);
				bzero((char *)(&h_tmp[h_cnt]), h_len-oldlen);
				free(hosts);
				hosts = h_tmp;
			}
			h_cnt++;
			hosts[idx].max_id = prime->header.transaction_id;
			hosts[idx].name = prime->header.nodename;
			continue;
		}

		if (TRANS_ID(prime) > hosts[idx].max_id)
			hosts[idx].max_id = TRANS_ID(prime);
	} /* end of first pass */

	/*
	 * Make a second pass through the volume header list to 
	 * search for backup labels with the latest transaction id.
	 * These entries represents volume headers whose master sector
	 * have not been updated.
	 */
	for (vh = vh_list; vh != NULL; vh = vh->next) {
		prime = XLV__LAB1_PRIMARY (vh);
		back = XLV__LAB1_SECONDARY (vh);

		if (back->header.magic != XLV_VH_MAGIC_NUM)
			continue;		/* ignore bad labels */

		for (idx = 0; idx < h_cnt; idx++) {
			if (!strcmp(prime->header.nodename, hosts[idx].name))
				break;
		}

		if ((TRANS_ID(prime) != hosts[idx].max_id) &&
		    (TRANS_ID(back)  == hosts[idx].max_id)) {
			/* If we don't want to automatically recover labels
			 * in a state of flux, set EAGAIN, and have the 
			 * caller re-read the labels
			 */
			if (!xlv_label_recovery) {
				*status = EAGAIN;
				return;
			}

			/*
			 * Use the backup/secondary label.
			 */
			vh->xlv_label->vh_seq_number++;
			apn = pathtoalias(vh->vh_pathname);
			if (xlv__lab2_check_fd (vh, vh_list) &&
			    xlv_lab1_write_master_blk (vh) ) {
				xlv_error("Unable to update label on %s\n",apn);
				/*
				 * XXX Continue but can this be a problem?
				 */
			}
			slogit(LOG_INFO, "Recovered xlv label on %s", apn);
			_label_recovery++;	/* another one completed */
		}
	} /* end of second pass */

	free(hosts);

	*status = 0;

#undef	TRANS_ID
} /* end of xlv_lab2_make_consistent() */

typedef struct walk_args_s {
	xlv_vh_entry_t	**xlv_vh_list_p;
	xlv_vh_entry_t	**non_xlv_vh_list_p;
} walk_args_t;

#ifdef NO_LONGER_USED
/* ARGSUSED */
static int
controller_walk(
	char *devpath,
	struct stat64 *sbuf,
	inventory_t *inv,
	int inv_count,
	void *usrdat)
{
	int i;

	if (inv_count == 0)
		return 0;

	for (i = 0; i < inv_count; i++) {

#ifdef DEBUG
printf("Looking for class %d type %d, got class %d type %d\n",
       INV_SCSI, INV_RAIDCTLR, inv->inv_class, inv->inv_type);
#endif

		/* Search for RAID controllers */
		if (inv->inv_class == INV_SCSI
		    && inv->inv_type == INV_RAIDCTLR) {

#ifdef DEBUG
			printf("xlvlab: Found a RAID SCSI controller: %s\n", devpath);
#endif
                        xlv_add_device(inv, devpath, sbuf->st_rdev);
		}

		inv++;
	}

	return 0;
}
#endif

/* ARGSUSED */
static int
disk_walk(
	char          *vh_pathname,
	struct stat64 *sbuf,
	inventory_t   *inv,
	int           inv_count,
	void          *usrdat)
{
	walk_args_t		*arg = (walk_args_t *)usrdat;
	xlv_tab_disk_part_t	dp;
#ifdef DEBUG
	char			msg[512];
#endif
	xlv_vh_entry_t		*vh_entry = NULL;
	int			i;

	/* if it's not a scsi disk, skip it */
	i = 0;
	while (i < inv_count) {
		if (inv[i].inv_class == INV_DISK && inv[i].inv_type == INV_SCSIDRIVE)
			break;
		i++;
	}
	if (i == inv_count)
		return 0;

	/* create a dummy disk part with one device */
	dp.dev[0] = sbuf->st_rdev;
	dp.active_path = dp.retry_path = 0;
	dp.n_paths = 1;

#ifdef DEBUG
	fprintf(stderr, "DBG: disk_walk() vh %s(%d)\n", vh_pathname, dp.dev[0]);
#endif

	/*
	 * Open each disk. The disk header would be linked into
	 * the internal vh_list.
	 */
	if (arg->xlv_vh_list_p)
		vh_entry = xlv__lab2_vh_incore (*(arg->xlv_vh_list_p), vh_pathname);

	if ((! vh_entry) && arg->non_xlv_vh_list_p)
		vh_entry =
			xlv__lab2_vh_incore (*(arg->non_xlv_vh_list_p), vh_pathname);

	if (! vh_entry) {
		vh_entry = (xlv_vh_entry_t *) malloc (sizeof(xlv_vh_entry_t));
		bzero (vh_entry, sizeof (xlv_vh_entry_t));
		vh_entry->vfd = -1;

		setoserror(0);			/* reset errno */
		/*
		 * The disk will be picked up when the working
		 * path is enumerated.  XXX KK: what if no
		 * path is good, or the working path doesn't
		 * look like a disk yet? */
		if (xlv_lab1_open(vh_pathname, &dp, vh_entry)) {
			/*
			 * Do nothing in this case, continue scanning.
			 */
			free (vh_entry);
#ifdef DEBUG
			sprintf(msg, "Ignoring device %s",
				pathtoalias(vh_pathname));
			if (oserror())
				perror (msg);
			else
				fprintf (stderr, "%s: non-XLV device\n", msg);
#endif
			return 0;
		} else if (sbuf->st_rdev != vh_entry->dev) {
			/*
			 * Different dev_t so must have successfully
			 * switched paths.
			 */
#ifdef DEBUG
			fprintf(stderr, "Switch from %s(%d) to %s(%d)\n",
				pathtoalias(vh_pathname), sbuf->st_rdev,
				pathtoalias(vh_entry->vh_pathname),
				vh_entry->dev);
#endif
			if ((arg->xlv_vh_list_p &&
	(xlv__lab2_vh_incore(*(arg->xlv_vh_list_p), vh_entry->vh_pathname)))
			    || (arg->non_xlv_vh_list_p &&
	(xlv__lab2_vh_incore (*(arg->non_xlv_vh_list_p), vh_entry->vh_pathname)))) {
#ifdef DEBUG
				fprintf(stderr,
					"DBG:    Skip seen vh %s(%d)\n",
					pathtoalias(vh_entry->vh_pathname),
					vh_entry->dev);
#endif
				/* seen this path before */
				xlv_lab1_close(&vh_entry);    /* entry is freed */
				return(0);
			}
		}

		/*
		 * Link the disk into the appropriate list, depending
		 * upon whether this is part of an XLV volume and also
		 * on what the caller wanted.
		 */
		if (arg->xlv_vh_list_p && vh_entry->xlv_label) {
#ifdef DEBUG
			fprintf(stderr, "DBG:    Add %s(%d)\n",
				pathtoalias(vh_entry->vh_pathname),
				vh_entry->dev);
#endif
			vh_entry->next = *(arg->xlv_vh_list_p);
			*(arg->xlv_vh_list_p) = vh_entry;
			xlv__lab2_inc_open_fds(*(arg->xlv_vh_list_p));
		} else if (arg->non_xlv_vh_list_p && (!vh_entry->xlv_label)) {
			/*
			 * This disk is not part of an XLV volume
			 * so close the vh's fd. We keep track of
			 * the number of open fd's to disks that
			 * are part of a volume.
			 */
#ifdef DEBUG
			fprintf(stderr, "DBG:    Add non-XLV %s(%d)\n",
				pathtoalias(vh_entry->vh_pathname),
				vh_entry->dev);
#endif
			vh_entry->next = *(arg->non_xlv_vh_list_p);
			*(arg->non_xlv_vh_list_p) = vh_entry;
			if (vh_entry->vfd) {
				xlv_lab1_reclaim_fd(vh_entry);
			}
		} else {
			/* No where to o add so close fd and free entry */
#ifdef DEBUG
			fprintf(stderr, "DBG:    Skip non-XLV %s(%d)\n",
				pathtoalias(vh_entry->vh_pathname),
				vh_entry->dev);
#endif
			xlv_lab1_close (&vh_entry);
		}

	} else {
#ifdef DEBUG
		fprintf(stderr, "DBG:    Skip seen vh %s(%d)\n",
			pathtoalias(vh_pathname), dp.dev[0]);
#endif
	}

	return 0;

} /* end of disk_walk() */


/*
 * Return linked lists of volume headers for those disks that 
 * belong to XLV volumes and those that don't.
 *
 * The caller should set non_xlv_vh_list_p to NULL if only the
 * xlv vh list is needed (and vice versa.)
 * 
 * May print errors on the console.
 */ 
void
xlv_lab2_create_vh_info_from_hinv (
	xlv_vh_entry_t	**xlv_vh_list_p,
	xlv_vh_entry_t	**non_xlv_vh_list_p,
	int		*status)
{
	walk_args_t     walk_args;
	cap_t		ocap;
	cap_value_t	cap_device_mgt = CAP_DEVICE_MGT;

	*status = 0;			/* not used */

	if (_maxfds < XLV_MAXFDS) {
		/*
		 * Want the maximum number of open volume headers.
		 */
		(void)xlv_set_maxfds(XLV_MAXFDS);
	}

	walk_args.xlv_vh_list_p = xlv_vh_list_p;
	walk_args.non_xlv_vh_list_p = non_xlv_vh_list_p;

	ocap = cap_acquire (1, &cap_device_mgt);
#ifdef NO_LONGER_USED
	dsk_walk_drives(controller_walk, NULL);
#endif
	dsk_walk_drives(disk_walk, &walk_args);
	cap_surrender (ocap);

	/*
	 * At this point, *xlv_vh_list_p contains volume headers (and XLV
	 * labels) for all the XLV volumes on this machine.
	 * Note that some of these XLV labels may have just been
	 * initialized and hence may contain no disk parts.
	 */

} /* end of xlv_lab2_create_vh_info_from_hinv() */


/*
 * Frees the linked lists of volume headers generated by
 * xlv_lab2_create_vh_info_from_hinv(). All open volume header
 * file descriptors are closed before the freeing of the volume
 * header entry.
 */
void
xlv_lab2_destroy_vh_info(
	xlv_vh_entry_t	**xlv_vh_list_p,
	int		*status)
{
	xlv_vh_entry_t *vh, *next; xlv_vh_entry_t *vh_list;

	if (vh_list = *xlv_vh_list_p) {
		xlv__lab2_purge_fds (vh_list);

		vh = vh_list;
		while (vh) {
			next = vh->next;
			free(vh);
			vh = next;
		}

		*xlv_vh_list_p = NULL;
	}

	*status = 0;

} /* end of xlv_lab2_destroy_vh_info() */


/*
 * Delete the XLV label from the given volume header. Must check that
 * the handle to the volume header is still open -- the file decriptor
 * may have been closed since last access.
 */
int
xlv_delete_xlv_label(xlv_vh_entry_t *vh, xlv_vh_entry_t *vh_list)
{
	int code;

	if (xlv__lab2_check_fd (vh, vh_list)) {
		code = xlv_lab1_delete_label(vh);
	} else {
		code = -1;
	}

	return (code);
}
	

/*
 * Print the xlv label.
 */
void
xlv_print_xlv_label (xlv_vh_disk_label_t *lab, int p_mode)
{
	int				d;
	unsigned			st;
	xlv_vh_disk_label_entry_t	*lentry;
	char				sv_type;
	char				*time_str;
	time_t				clock;
	int				count;
	char				*uuid_str;
	char				*path;
	char				partstr[] = "partition 0xXXX";

	printf("magic: 0x%lx, version: %d, trans id: 0x%llx\n",
	       lab->header.magic, lab->header.version,
	       lab->header.transaction_id);
	printf("nodename: %s", lab->header.nodename);
	if (p_mode > 0) {
		printf("  reserved: 0x%x, timestamp: 0x%llx",
		       lab->header.reserved, lab->header.timestamp);
	}
	printf("\n");

	printf ("disk parts: %d\n", lab->header.disk_parts_on_disk);
	for (d=0,count=0; d < XLV_MAX_DISK_PARTS; d++) {
		lentry = &(lab->label_entry[d]);
		if (uuid_is_nil(&lentry->disk_part.uuid,&st)) {
			continue;
		}
		count++;
		printf("(%d:%d)", count, d);	/* count and entry slot */

		/*
		 * vol & subvol info.
		 */
		/* lentry->vol.flag */
		if (p_mode == XLV_PRINT_ALL) {
			printf("\tvol: %s\t(flags=0x%0x)\n",
				lentry->vol.volume_name,
				lentry->vol.flag);
		}
		if (p_mode & XLV_PRINT_UUID) {
			if (!uuid_is_nil(&lentry->vol.volume_uuid, &st)) {
				uuid_to_string (&(lentry->vol.volume_uuid),
						&uuid_str, &st);
				printf ("\tvol uuid:\t%s\n", uuid_str);
				free (uuid_str);
			}
			if (!uuid_is_nil(&lentry->vol.log_subvol_uuid, &st)) {
				uuid_to_string (&(lentry->vol.log_subvol_uuid),
						&uuid_str, &st);
				printf ("\tlog uuid:\t%s\n", uuid_str);
				free (uuid_str);
			}
			if (!uuid_is_nil(&lentry->vol.data_subvol_uuid, &st)) {
				uuid_to_string (&(lentry->vol.data_subvol_uuid),
						&uuid_str, &st);
				printf ("\tdata uuid:\t%s\n", uuid_str);
				free (uuid_str);
			}
			/* lentry->vol.rt_subvol_uuid */
			if (!uuid_is_nil(&lentry->vol.rt_subvol_uuid, &st)) {
				uuid_to_string (&(lentry->vol.rt_subvol_uuid),
						&uuid_str, &st);
				printf ("\trt uuid:\t%s\n", uuid_str);
				free (uuid_str);
			}
			if (lentry->plex.num_plexes > 0) {
				uuid_to_string (&(lentry->plex.uuid),
						&uuid_str, &st);
				printf ("\tplex uuid:\t%s\n", uuid_str);
				free (uuid_str);
			}
			uuid_to_string (&(lentry->vol_elmnt.uuid),
					&uuid_str, &st);
			printf ("\tve uuid:\t%s\n", uuid_str);
			free (uuid_str);
			uuid_to_string (&(lentry->disk_part.uuid),
					&uuid_str, &st);
			printf ("\tpart uuid:\t%s\n", uuid_str);
			free (uuid_str);
		}
		printf ("\tvol: %s ", lentry->vol.volume_name);
		sv_type = lentry->vol.subvolume_type;
		switch( sv_type ) {
		    case XLV_SUBVOL_TYPE_LOG:
			printf ("[log ]"); break;
		    case XLV_SUBVOL_TYPE_DATA:
			printf ("[data]"); break;
		    case XLV_SUBVOL_TYPE_RT:
			printf ("[rt  ]"); break;
		    default:
			printf ("[NULL]");
		}
		/* lentry->vol.subvol_flags */
		/* lentry->vol.subvol_padding */
		/* lentry->vol.sector_size */
		clock = (time_t)lentry->vol.subvol_timestamp;
		time_str = asctime(localtime(&clock));
		printf ("\tdepth: %d  time: %s",
			lentry->vol.subvol_depth,
			time_str);		/* "\n" embbeded in time_str */

		/*
		 * plex info
		 */
		if (0 == lentry->plex.num_plexes) {
			/*
			 * No plex so it must be a stand alone volume element.
			 */
			printf ("\tplex %d (NULL)", lentry->plex.seq_num);
		} else {
			printf ("\tplex %d (%d of %d)",
				lentry->plex.seq_num,
				lentry->plex.seq_num+1,
				lentry->plex.num_plexes);
			if (lentry->plex.name[0] != '\0')
				printf (" %s ", lentry->plex.name);
			printf ("\tflags: 0x%x", lentry->plex.flags);
		}
		printf ("\n");

		/*
		 * ve info
		 */
		printf ("\tve %d (%d of %d)",
			(int) lentry->vol_elmnt.seq_num_in_plex,
			(int) lentry->vol_elmnt.seq_num_in_plex+1,
			lentry->plex.num_vol_elmnts);
		if (lentry->vol_elmnt.name[0] != '\0')
			printf (" %s ", lentry->vol_elmnt.name);
		printf ("\tstate: %s",
			xlv_ve_state_str(lentry->vol_elmnt.state));
		/* lentry->vol_elmnt.reserved[0] */
		/* lentry->vol_elmnt.reserved[1] */
		printf ("  start=%llu  end=%llu",
			lentry->vol_elmnt.start_block_no,
			lentry->vol_elmnt.end_block_no);
		if (lentry->vol_elmnt.type == XLV_VE_TYPE_STRIPE)
			printf ("  step=%d",
				lentry->vol_elmnt.stripe_unit_size);
		printf ("\n");

		/*
		 * disk parts
		 */
		path = devtopath(lentry->disk_part.dev, 1 /* use alias */);
		if (NULL == path) {
			/*
			 * Cannot map to a pathname so this is probably
			 * a partition number. 
			 */
			sprintf(partstr, "dev_t %#x", lentry->disk_part.dev);
			path = partstr;
		}
		printf ("\tdisk (%s)part %d (%d of %d)  size=%d  path=%s\n",
			(lentry->vol_elmnt.type == XLV_VE_TYPE_CONCAT) ?
				"cat" : "stripe",
			lentry->disk_part.seq_in_grp,
			lentry->disk_part.seq_in_grp+1,
			lentry->vol_elmnt.grp_size,
			lentry->disk_part.size,
			path);
		/* lentry->disk_part.state */
	}
} /* end of xlv_print_xlv_label() */



/*
 * Print an xlv_vh_entry.
 */
void
xlv_print_vh (xlv_vh_entry_t	*vh,
	      int		p_mode,
	      int		secondary)	/* show secondary label */
{
	xlv_vh_disk_label_t	*lab;
	xlv_oref_t		*oref;

	if (vh->xlv_label == NULL) {
		/* No label on this volume header */
		return;
	}

	printf ("\n%s\n", vh->vh_pathname);

	printf ("%s (%d, %d)  seq_num: %d\n", pathtoalias(vh->vh_pathname), 
		major(vh->dev), minor(vh->dev), 
		(int) vh->xlv_label->vh_seq_number);

	lab = XLV__LAB1_PRIMARY (vh);
	printf ("Primary label\n");
	xlv_print_xlv_label (lab, p_mode);

	lab = XLV__LAB1_SECONDARY(vh);
	if (secondary && lab) {
		printf ("Secondary label\n");
		xlv_print_xlv_label (lab, p_mode);
	}

	for (oref = (xlv_oref_t *) vh->xlv_tab_list; 
	     oref != NULL; oref = oref->next) {
		printf ("oref %x [type=%d]\n", oref, XLV_OREF_TYPE(oref));
	}

}
