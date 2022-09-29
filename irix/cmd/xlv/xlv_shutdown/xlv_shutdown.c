/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1993-1994, Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.27 $"
/*
 * Command to gracefully shutdown XLV volumes on system.
 */
#include <bstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/debug.h>
#include <sys/syssgi.h>
#include <sys/sysmacros.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <xlv_oref.h>
#include <xlv_lab.h>
#include <sys/xlv_attr.h>
#include <xlv_utils.h>
#include <xlv_cap.h>
#include "xlv_shutdown_utils.h"

#define	ME	"xlv_shutdown"
#define VERSION 1			/* first shutdown to use a version */

int verbose = 0;
int aflag = 1;
int force = 0;
int volcount = 0;	/* Count of volumes being checked for shutdown. */

#define	NSELECT_ENTRIES	1023
typedef struct selectentry_s {
	char	*s_volname;	/* volume name */
	int	s_found;	/* named volume is found */
} selectentry_t;

char	*selectlist = NULL;


/*
 * Function Prototypes
 */
static void usage(void);
static int  xlv_sd_shutdown_ve(xlv_oref_t *I_oref, int *cleanp);
static int  xlv_sd_shutdown_rootve(xlv_oref_t *I_oref, int dummy);
static int  xlv_sd_shutdown_vol(xlv_attr_cursor_t *cursor,
				   xlv_tab_vol_entry_t *vol,
				   xlv_vh_entry_t *vh_list);
static void xlv_sd_get_cursor(xlv_attr_cursor_t *cursor);
static int  xlv_sd_do_shutdown(xlv_vh_entry_t *vh_list, xlv_queue_t *vol_list);
static void xlv_sd_vol_list(xlv_vh_entry_t *vh_list, xlv_queue_t *vol_list);

static int xlv_sd_uuid_queue(xlv_queue_t *uuidq, uuid_t *uuidp);
static boolean_t xlv_sd_uuid_found(xlv_queue_t *uuidq, uuid_t *uuidp);
#define xlv_sd_uuid_qinit(uuidq) 	q_init(uuidq, (xlv_queue_cb_t)free)
#define xlv_sd_uuid_qdestroy(uuidq) 	q_destroy(uuidq)

/* internal return values */
#define SD_ERR		-1
#define SD_DONE		0


/*
 * Callback routine to xlv_for_each_ve_in_vol.
 * Just mark the state clean if it is active.
 */
int
xlv_sd_shutdown_ve(
	xlv_oref_t 	*I_oref,
	int		*cleanp)
{
	xlv_tab_vol_elmnt_t	*vep;

	vep = XLV_OREF_VOL_ELMNT (I_oref);
	
	ASSERT (vep != NULL);	

	/*
	 * Clear out the name field in the volume element.
	 * This is a union with the timestamp field.  Since name is only
	 * relevant for standalone volume elements, there's no need to have 
	 * this field set for volumes attached to plexes.  By leaving
	 * it set, we can run into trouble any time the kernel trusts the
	 * timestamp set directly from the label (currently only root).
	 */
	bzero(vep->veu_name, sizeof(xlv_name_t));

	/*
	 * Only active volume element can be "cleanly" shutdown.
	 * Volume element in non-active states are left alone.
	 */
	if (vep->state == XLV_VE_STATE_ACTIVE) {
		vep->state = XLV_VE_STATE_CLEAN;
		*cleanp = *cleanp + 1;
	}

	return(0);		/* continue with next volume element */
}

/*
 * Callback routine to xlv_for_each_ve_in_vol from xlv_sd_update_root.
 *
 * Clear out the name field in the volume element.  This is a union
 * with the timestamp field.  Since name is only relevant for standalone
 * volume elements, there's no need to have this field set for volumes
 * attached to plexes.  By leaving it set, we can run into trouble for
 * plexed roots which gets doesn't have the timestamp properly set by
 * the utilities.  
 */
/*ARGSUSED1 */
int xlv_sd_shutdown_rootve(
	xlv_oref_t	*I_oref, 
	int		dummy)
{
	xlv_tab_vol_elmnt_t	*vep;

	vep = XLV_OREF_VOL_ELMNT (I_oref);
	
	ASSERT (vep != NULL);	

	bzero(vep->veu_name, sizeof(xlv_name_t));

	return(0);		/* continue with next volume element */
}

/*
 * Actually perform the shutdown of a given subvolume. 
 * We simply mark all it's contituent volume elements 'clean', and remove the
 * volume from kernel tables altogether.
 */
int
xlv_sd_shutdown_vol(
	xlv_attr_cursor_t	*cursor,
	xlv_tab_vol_entry_t 	*vol, 
	xlv_vh_entry_t 		*vh_list)
{
	int 			ncleaned = 0;
	xlv_oref_t		oref;
	int			status = 0;
	xlv_attr_req_t		req;
	uint_t			st;
	
	XLV_OREF_INIT(&oref);
	XLV_OREF_SET_VOL(&oref, vol);
	XLV_OREF_SET_UUID(&oref, &vol->uuid);
	
	/*
	 * Iterate thru all ve's and mark them 'clean'. We write these to
	 * disk next.
	 */
	xlv_for_each_ve_in_vol (
		&oref, (XLV_OREF_PF)xlv_sd_shutdown_ve, (void *)&ncleaned );
	
	/*
	 * Notify kernel of new settings.
	 */
	req.attr = XLV_ATTR_VOL;
	req.cmd  = XLV_ATTR_CMD_REMOVE;
	req.ar_vp = vol;
	/*CONSTCOND*/
	for (;;) {
		if (cap_dev_syssgi(SGI_XLV_ATTR_SET, cursor, &req) < 0) {
			int err = oserror();
			if (ESTALE == err) {
				if (verbose) {
					fprintf(stderr,
		"%s: Stale XLV cursor - re-obtaining\n",
						ME);
				}
				xlv_sd_get_cursor(cursor);
				continue;
			}

			if (ENOENT == err) {
				fprintf(stderr,
		"%s: Kernel error - volume \"%s\" not found\n",
					ME, vol->name);
			}
			else {
				fprintf(stderr,
		"%s: Error setting kernel attrs of volume \"%s\": %s\n", 
					ME, vol->name, strerror(err));
			}
			return (SD_ERR);
		}
		break;		/* finished removing the volume */
	}

	/*
	 * Verify if there is more than one plex associated with ANY subvolume.
	 * If so, then update the labels, otherwise the label state of 'clean'
	 * is really no different than 'active', so we can skip the update.
	 */
	if ((vol->data_subvol
		&& !uuid_is_nil(&vol->data_subvol->uuid, &st)
		&& (vol->data_subvol->num_plexes > 1)) ||
	    (vol->log_subvol
		&& !uuid_is_nil(&vol->log_subvol->uuid, &st)
		&& (vol->log_subvol->num_plexes > 1)) ||
	    (vol->rt_subvol
		&& !uuid_is_nil(&vol->rt_subvol->uuid, &st)
		&& (vol->rt_subvol->num_plexes > 1))) {

		/*
		 * Update disk label.
		 */
		xlv_lab2_write_oref (
			&vh_list, &oref, NULL, &status, XLV_LAB_WRITE_PARTIAL);
		
	} 

	if (status) {
		/*
		 * It is not clear if backing out the changes that we
		 * just made in the Kernel Table will give us anything.
		 * (so we just return).
		 */
		fprintf(stderr,
			"%s: Error in writing disk label for volume \"%s\"\n",
			ME, vol->name);
		return (SD_ERR);
	}
	
	return (SD_DONE);

} /* end of xlv_sd_shutdown_vol() */


/*
 * Update the timestamp of the root volume elements.
 */
int
xlv_sd_update_root(
	xlv_tab_vol_entry_t 	*vol, 
	xlv_vh_entry_t 		*vh_list)
{
	xlv_oref_t		oref;
	int			status;
	
	XLV_OREF_INIT(&oref);
	XLV_OREF_SET_VOL(&oref, vol);
	XLV_OREF_SET_UUID(&oref, &vol->uuid);

	/*
	 * Iterate thru all root ve's and clear the name/timestamp union
	 */
	xlv_for_each_ve_in_vol (
		&oref, (XLV_OREF_PF)xlv_sd_shutdown_rootve, NULL);

	/*
	 * No changes, update timestamp in the disk label.
	 */
	xlv_lab2_write_oref (
		&vh_list, &oref, NULL, &status, XLV_LAB_WRITE_PARTIAL);

	if (status) {
		fprintf(stderr,
		"%s: Error in writing disk label for root volume \"%s\"\n",
			ME, vol->name);
		return (SD_ERR);
	}

	return (SD_DONE);

} /* end of xlv_sd_update_root() */


/*
 * go thru the list of volumes and attempt to shut them down, one by one.
 */
int
xlv_sd_do_shutdown(
	xlv_vh_entry_t 		*vh_list,
	xlv_queue_t		*vol_list)
{
	xlv_tab_vol_entry_t 	*vol;
	u_int			nvols = 0;
	xlv_attr_cursor_t	cursor;
	int			retcode;
	char			*update_fmt;

	xlv_sd_get_cursor(&cursor);

	while (! q_is_empty(vol_list)) {
		vol = NULL;
		q_dequeue(vol_list, (void **)&vol);
		ASSERT(vol);

		if ((vol->data_subvol != NULL) &&
		    (minor(vol->data_subvol->dev) == 0)) {
			/*
			 * This is the root volume. Just update the
			 * timestamp but don't shutdown the volume.
			 */
			retcode = xlv_sd_update_root(vol, vh_list);
			update_fmt = "%s: Done updating root volume \"%s\"\n";
		} else {
			/*
			 * Mark all its volume elements clean
			 */
			retcode = xlv_sd_shutdown_vol(&cursor, vol, vh_list);
			update_fmt = "%s: Done shutting down volume \"%s\"\n";
		}

		if (retcode == SD_DONE) {
			nvols++;
			/* 
			 * In an interactive shutdown, we print the final
			 * result depending on whether or not the verbose
			 * flag is set.
			 */
			if (verbose || !aflag) {
				fprintf(stderr, "\n");
				xlv_tab_vol_print(vol, 0);
				fprintf(stderr, update_fmt, ME, vol->name);
			}
		}
		else if (verbose || !aflag) {
			fprintf(stderr,
				"%s: Shutdown failed on volume \"%s\"\n",
				ME, vol->name);
		}
		
		free_vol_space(vol);
	}
	
	return (nvols);
		
} /* end of xlv_sd_do_shutdown() */



/*
 * Just get the XLV cursor to be used in our ATTR_GET/SET syscalls 
 */
void
xlv_sd_get_cursor(
	xlv_attr_cursor_t	 *cursor)
{
	if (0 > syssgi(SGI_XLV_ATTR_CURSOR, cursor)) {
		fprintf(stderr, "%s: Failed to get an XLV cursor\n", ME);
		exit(1);
	}
}


/*
 * Keep track of uuid's we couldn't shutdown, so when we get a STALE
 * cursor from an ATTR_GET, we know not to retry shutting down these 
 * volumes.
 */
static int
xlv_sd_uuid_queue(xlv_queue_t *uuidq, uuid_t *uuidp)
{
	uuid_t *voluuid = malloc(sizeof(uuid_t));

	if (voluuid == NULL)
		return 1;

	memcpy (voluuid, uuidp, sizeof(uuid_t));

	q_enqueue(uuidq, (void *)voluuid);

	return 0;
}

/* 
 * Return true if this volume has already been processed
 */
static boolean_t
xlv_sd_uuid_found(xlv_queue_t *uuidq, uuid_t *uuidp)
{
	xlv_queue_entry_t *qe;
	int i;
	uint_t dummy;
	
	if (!uuidq->nentries)
		return B_FALSE;
		
	qe = uuidq->head;

	for(i = 0; i < uuidq->nentries; i++) {
		if (uuid_equal(qe->value, uuidp, &dummy))
			return B_TRUE;
		qe = qe->next;
	}
	return B_FALSE;
}

/*
 * Iterates over all volumes (in kernel table) and try to shutdown all but the
 * root volume.
 */
void
xlv_sd_vol_list(xlv_vh_entry_t *vh_list, xlv_queue_t *vol_list)
{
	xlv_attr_cursor_t 	ocursor, cursor;
	xlv_attr_req_t		req;
	xlv_tab_vol_entry_t	*vol;
	xlv_queue_t		uuid_queue;
	uint_t			st;

	selectentry_t	select[NSELECT_ENTRIES+1];
	int		nselect = 0, nvols = 0;
	int		i;

	/*
	 * Establish the selected list of volumes.
	 */
	if (!aflag && selectlist) {
		for (nselect = 0; nselect < NSELECT_ENTRIES; nselect++) {
			select[nselect].s_volname = strtok(selectlist, ",");
			selectlist = NULL;	/* */

			select[nselect].s_found = 0;
			if (select[nselect].s_volname == NULL)
				break;
		}
		if (nselect == NSELECT_ENTRIES) {
			 fprintf(stderr,
		"WARNING: only first %d volumes specified will be processed\n",
				NSELECT_ENTRIES);
			/* ensure loop termination later */
			select[nselect].s_volname = NULL;
			select[nselect].s_found = 0;
		}
	}
	
	/*
	 * We need this cursor to do our ATTR_GET/SET operations on volumes 
	 */
	xlv_sd_get_cursor(&cursor);
	ocursor = cursor;

	/* 
	 * initialize the queue of uuids processed
	 */
	xlv_sd_uuid_qinit(&uuid_queue);

	/* allocate space */
	vol = get_vol_space();
	if (NULL == vol) {
		goto cant_create_vol;
	}
	req.attr = XLV_ATTR_VOL;
	req.ar_vp = vol;
	
	/*CONSTCOND*/
	for (;;) {
		/*
		 * If this is not the first time thru this loop, the
		 * volume struct may not be bzero'ed. This should be
		 * okay as long as the kernel clears volume and subvolume
		 * uuid when they don't exist in the kernel.
		 */
		if (syssgi(SGI_XLV_ATTR_GET, &cursor, &req) < 0) {
			int err = oserror();
			if (ENFILE == err || ENOENT == err) {
				free_vol_space(vol);
				/* end of enumeration */
				break;
			}
			if (ESTALE == err) {
				/* 
				 * Only print a message here if the 
				 * STALE isn't the result of shutting down
				 * a volume while we're in the middle of
				 * our ATTR_GET call.
				 */
				xlv_sd_get_cursor(&cursor);
				if(ocursor.generation + 1 == cursor.generation){
					cursor = ocursor;
					cursor.generation++;
				} else if (verbose) {
					fprintf(stderr,
					"%s: Stale cursor - re-obtaining\n",
						ME);
				}
				continue;
			}
			fprintf(stderr,
				"%s: Can't get attributes for volumes: %s\n",
				ME, strerror(err));
			break;
		}
		ocursor = cursor;

		/*
		 * Can only handle valid existing volumes. Why did the
		 * kernel enumeration give us this?
		 */
		if (uuid_is_nil(&vol->uuid, &st)) {
			if (verbose) {
				fprintf(stderr, "%s: Skip null volume\n", ME);
			}
			continue;	/* not bzero'ing struct's */
		}
		
		/*
		 * If we've already seen this uuid before, skip it.  We
		 * got a stale cursor, and had to reprocess the entry
		 */
		if (xlv_sd_uuid_found(&uuid_queue, &vol->uuid))
			continue;

		if ((vol->state == XLV_VOL_STATE_NONE) ||
		    (vol->state == XLV_VOL_STATE_MISS_UNIQUE_PIECE)) {
			if (verbose) {
				fprintf(stderr,
				"%s: Volume %s has state \"%s\" - %s\n",
					ME, vol->name,
					xlv_vol_state_str(vol->state),
					(force) ? "forcing shutdown" : "skipping");
			}
			if (!force) {
				/* remember this uuid so we skip it */
				if (xlv_sd_uuid_queue(&uuid_queue, &vol->uuid))
					goto cant_create_vol;
				continue;	/* not bzero'ing struct's */
			}
		}

		/*
		 * Is this a volume selected for shutdown.
		 */
		if (!aflag && nselect) {
			for (i=0; i < nselect; i++) {
				if (!strncmp(vol->name, select[i].s_volname,
					     XLV_NAME_LEN)) { 
					select[i].s_found = 1;
					break;
				}
			}
			if (i == nselect) {
				/* skip non-selected kernel volume */
				if (xlv_sd_uuid_queue(&uuid_queue, &vol->uuid))
					goto cant_create_vol;
				continue;	/* not bzero'ing struct's */
			}
		}

		volcount++;	/* another volume to check for shutdown */

		/*
		 * Check for the root subvolume.  (minor device 0)
		 */
		if ((vol->data_subvol != NULL) &&
		    (minor(vol->data_subvol->dev) == 0)) {
			if (verbose)
				fprintf(stderr,
	"%s: Volume \"%s\" is the root volume - can only update timestamp\n", 
					ME, vol->name);
			/* add the root volume to the list to avoid 
			 * reprocessing it later
			 */
			if (xlv_sd_uuid_queue(&uuid_queue, &vol->uuid))
				goto cant_create_vol;

			goto add_this;
		}

		/*
		 * XXX Any special treatment for the other reserved subvolume?
		 */

		/*
		 * We don't shutdown the volume if at least one of its subvols 
		 * is still open - just to be safe.
		 *
		 * Note that a valid subvolume has a non null uuid.
		 * The kernel clears the subvolume uuid if the subvolume
		 * does not exist.
		 */
		if ((vol->data_subvol
			&& !uuid_is_nil(&vol->data_subvol->uuid, &st)
			&& XLV_ISOPEN(vol->data_subvol->flags)) ||
		    (vol->log_subvol
			&& !uuid_is_nil(&vol->log_subvol->uuid, &st)
			&& XLV_ISOPEN(vol->log_subvol->flags)) ||
		    (vol->rt_subvol
			&& !uuid_is_nil(&vol->rt_subvol->uuid, &st)
			&& XLV_ISOPEN(vol->rt_subvol->flags))) {

			fprintf(stderr,
				"%s: Volume \"%s\" has open subvolume(s) - ", 
				ME, vol->name);
			if (force) {
				fprintf(stderr, "forcefully shut it down\n");
			} else {
				fprintf(stderr, "cannot shut it down\n");

				/* remember the uuid so we can skip it later */
				if (xlv_sd_uuid_queue(&uuid_queue, &vol->uuid))
					goto cant_create_vol;
				continue;	/* not bzero'ing struct's */
			}
		}

		/*FALLTHROUGH*/
add_this:
		/* attach this volume to the list the volumes to be shutdown */
		q_enqueue(vol_list, (void *)vol);

		/*
		 * Go ahead and shutdown the volume right now to avoid
		 * having lots of memory tied up with huge volume lists
		 */
		nvols += xlv_sd_do_shutdown(vh_list, vol_list);

		if (!aflag && (volcount == nselect)) {
			break;
		}

		/* Allocate space on the heap for the next volume */
		vol = get_vol_space();
		if (NULL == vol)
			goto cant_create_vol;
		req.ar_vp = vol;

	} /* for loop to find all volumes */

	if (verbose) {
		fprintf(stderr,
			"%s: Succeeded on %d of %d volume(s)\n",
			ME, nvols, volcount);
	}

	if (volcount < nselect) {
		/*
		 * Did not find some specified volumes.
		 */
		for (i=0; i < nselect; i++) {
			if (select[i].s_found)
				continue;
			fprintf(stderr, "%s: Cannot find volume \"%s\".\n",
				ME, select[i].s_volname);
		}
	}
	
	xlv_sd_uuid_qdestroy(&uuid_queue);

	return;

cant_create_vol:
	/*
	 * Don't bother to free-up the memory we may have allocated.
	 */
	fprintf(stderr, "Failed to allocate space for volume entry.\n");
	exit(1);

} /* end of xlv_sd_vol_list() */


static void
usage(void)
{
	fprintf(stderr, "usage: %s [-fv] [-n list]\n", ME);
	fprintf(stderr, "-n list  Comma separated list of volumes to shutdown.\n");
	fprintf(stderr, "-v       Display verbose status messages.\n");
	fprintf(stderr, "-f       Force shutdown of open volumes.\n");
	exit(1);
}


/*
 * M a i n
 */
int
main (int argc, char **argv)
{
	int		ch;
	xlv_vh_entry_t	*vh_list;
	int		status;
	xlv_queue_t	vol_list;
	int		retcode = 0;

	if (cap_envl(0, CAP_DEVICE_MGT, 0) == -1) {
		fprintf(stderr, "%s: must be started by super-user\n", argv[0]);
		exit(1);
	}

	vh_list = NULL;

	while ((ch = getopt(argc, argv, "fvVn:")) != EOF)
		switch((char)ch) {
		      case 'f':
			force = 1;
			break;

		      case 'v':
			verbose = 1;
			break;

		      case 'V':
			printf("version %d\n", VERSION);
			exit(0);
			break;

		      case 'n':
			aflag = 0;		/* not doing all volumes */
			selectlist = optarg;
			break;

		      default:
			usage();
		}

	if (argc -= optind)
		usage();

	if (! (aflag || selectlist)) {
		usage();
		exit(0);
	}
	
	if (verbose)
		fprintf(stderr, "%s: Reading disk labels\n", ME);

	xlv_lab2_create_vh_info_from_hinv (&vh_list, NULL, &status);
	
	if (! vh_list) {
		if (verbose)
			fprintf(stderr, "%s: There are no disk labels.\n", ME);
		exit(1);
	}

	/*
	 * Go through the list of volume elements in volumes and mark each
	 * volume element as clean (meaning graceful shutdown).
	 * Also, remove the kernel table entries for these volumes.
	 */
	q_init(&vol_list, (xlv_queue_cb_t)free_vol_space);
	xlv_sd_vol_list(vh_list, &vol_list);

	if (!aflag && volcount == 0)
		retcode = 1;

	ASSERT(q_is_empty(&vol_list));

	return(retcode);
}
