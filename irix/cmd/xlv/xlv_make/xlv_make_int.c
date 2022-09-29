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
#ident "$Revision: 1.48 $"

#include <stdlib.h>
#include <string.h>
#include <bstring.h>
#include <ctype.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <diskinvent.h>
#include <xlv_oref.h>
#include <xlv_utils.h>
#include <pathnames.h>
#include <sys/conf.h>
#include <sys/xlv_vh.h>
#include <xlv_lab.h>
#include <xlv_error.h>
#include "xlv_make_int.h"
#include <sys/debug.h>


int	xlv_mk_new_obj_defined=0;	/* xlv_mk_add_completed_obj() called */

/*
 * Back-end routine for xlv_make.  These routines do not
 * deal with any TCL user interface data structures and only 
 * return status codes.  This way, they can be used with other
 * front-ends.
 * Note that we do use the TCL supplied hash table routines.
 * But these can be used with other user interfaces also.
 */

/*
 * Key algorithm:
 *
 * At any given time, the current object that we are working
 * on is defined by the contents of the xlv_mk_context_t.
 *
 * xlv_mk_context_t contains pointers to the volume, subvolume, 
 * plex, volume element. Since we build top-down, all the higher
 * level objects must be defined before we can work on an
 * object.  The exception would be when the new object is a
 * higher level object itself.
 *
 * When we add a new object of the same type, we first back up 
 * to the parent and then record the new state.  So for example,
 * if we are adding a second plex after defining the first plex,
 * we would create a new context consisting of the same volume
 * and subvolume as the current context, and then plug the new
 * plex information as the plex object.
 *
 * xlv_mk_context_t contains only pointers to objects (usually
 * allocated from heap.  When we build a new object, we must
 * allocate the data structure from heap, set up any cross 
 * links with higher level objects, and update xlv_mk_context.
 *
 * Note that these routines are pretty sloppy about freeing
 * up memory since we expect to be run within the context of
 * commands which will terminate.
 */

/*
 * The routines that actually do the work of putting together
 * the pieces.
 */

static int xlv_mk_complete (xlv_mk_context_t *xlv_mk_context, int check_vol);

static int xlv_mk_ask_yes_or_no(char *query, int yes_or_no);

void xlv_mk_add_completed_obj (xlv_mk_context_t *xlv_mk_context, int initflag);

static int xlv_mk_check_partition_type (
	int			subvol_type,
	xlv_tab_vol_elmnt_t	*ve,
	xlv_tab_plex_t		*plex,
	unsigned		flags);

static int xlv_find_obj_in_table (
	xlv_mk_context_t	*xlv_mk_context,
	xlv_name_t		name);


/*
 * Assumptions:
 * 	volume_name is non-null, of the right length.
 *	if volume_name already in-use, caller has already warned user.
 */

int
xlv_mk_vol (xlv_mk_context_t	*xlv_mk_context,
	    xlv_name_t		volume_name,
	    long		volume_flags,
	    int			expert)
{
	xlv_tab_vol_entry_t	*vol;
	xlv_mk_context_t	temp_context;
	unsigned		ignore_status;

	ASSERT (volume_name != NULL);

	/* 
	 * If we were constructing another object, finish it up.
	 */
	if (xlv_mk_context->obj_type != XLV_OBJ_TYPE_NONE) {

		/* Verify that the previous object is complete. */
		if (! xlv_mk_complete(xlv_mk_context, 1)) {
			return (XLV_STATUS_INCOMPLETE);
		}

		/* Add the previous object to the hash list. */
		xlv_mk_add_completed_obj (xlv_mk_context, NO);
	}

	if (xlv_find_obj_in_table (
	    expert ? xlv_mk_context : &temp_context, volume_name)) {
		return (expert ? XLV_STATUS_OK : XLV_STATUS_VOL_DEFINED);
	} 

	/*
	 * Start a new context. 
	 */
	XLV_OREF_INIT (xlv_mk_context);

	/* Create a volume record. */
	vol = (xlv_tab_vol_entry_t *) malloc(sizeof (xlv_tab_vol_entry_t));
	bzero (vol, sizeof (xlv_tab_vol_entry_t));

	/* This also initializes the object type to be vol. */
	XLV_OREF_SET_VOL (xlv_mk_context, vol);

	uuid_create (&vol->uuid, &ignore_status);
	if (ignore_status != 0) {
		free (vol);
		xlv_mk_context->obj_type = XLV_OBJ_TYPE_NONE;
		return ((int) ignore_status);
	}

	strncpy (vol->name, volume_name, XLV_NAME_LEN);
	vol->flags = volume_flags;
	vol->log_subvol = NULL;
	vol->data_subvol = NULL;
	vol->rt_subvol = NULL;

	return (XLV_STATUS_OK);

} /* end of xlv_mk_vol() */


/*
 * Make a subvolume.
 * Note that a subvolume cannot be a top-level object.
 */

int
xlv_mk_subvol (xlv_mk_context_t	*xlv_mk_context,
	       short		sub_volume_type,
	       long		sub_volume_flags)
{
	xlv_tab_subvol_t 	*subvol;
	xlv_tab_subvol_t 	**subvol_p;
	xlv_tab_vol_entry_t     *vol;
	xlv_mk_context_t	temp_context;
	unsigned		ignore_status;

        /*
         * We are defining a subvolume within an existing volume.
         * Make sure that a volume has been defined.
         */
        if (XLV_OREF_VOL (xlv_mk_context) == NULL)
                return (XLV_STATUS_VOL_NOT_DEFINED);

	/*
	 * If we were defining another subvolume for this volume,
	 * make sure it's complete.
	 */
	if (XLV_OREF_SUBVOL (xlv_mk_context) != NULL) {
		if (! xlv_mk_complete(xlv_mk_context, 0)) {
			return (XLV_STATUS_INCOMPLETE);
		}
	}

	/*
	 * This subvolume must have the same volume as the
	 * current context.  Note that we use OREF_SET_VOL to
	 * set the type of the object to be a volume.
	 * Note that we work with a temporary context and only
	 * modify the original after we are sure we won't fail.
	 */

	XLV_OREF_INIT (&temp_context);
	XLV_OREF_SET_VOL (&temp_context, XLV_OREF_VOL (xlv_mk_context));

	vol = XLV_OREF_VOL(&temp_context);
	switch (sub_volume_type) {
		case	XLV_SUBVOL_TYPE_LOG:
			subvol_p = &vol->log_subvol;
			break;

		case	XLV_SUBVOL_TYPE_DATA:
			subvol_p = &vol->data_subvol;
			break;

		case	XLV_SUBVOL_TYPE_RT:
			subvol_p = &vol->rt_subvol;
			break;

		default:
			ASSERT (0);
	}

	if (*subvol_p == 0) {
		subvol = (xlv_tab_subvol_t *) malloc(sizeof(xlv_tab_subvol_t));
		bzero (subvol, sizeof(xlv_tab_subvol_t));

		subvol->vol_p = vol;
		subvol->subvol_type = sub_volume_type;
		uuid_create (&subvol->uuid, &ignore_status);

		*subvol_p = subvol;
	}
	else {
		/*
		 * The subvolume already exists so don't bother
		 * creating it.
		 */
		subvol = *subvol_p;
	}

	/*
	 * Note: Set the subvolume flag -- possibly changing the
	 * flag value of an existing subvolume.
	 */
	subvol->flags = sub_volume_flags;

	XLV_OREF_SUBVOL (&temp_context) = *subvol_p;

	*xlv_mk_context = temp_context;
        return (XLV_STATUS_OK);

} /* end of xlv_mk_subvol() */



/*
 * Create a plex.
 */

int
xlv_mk_plex (xlv_mk_context_t    *xlv_mk_context,
             xlv_name_t          plex_name,
             long                plex_flags,
              int                expert)
{
	xlv_tab_plex_t     	*plex;
	xlv_tab_subvol_t	*subvol;
	unsigned	        ignore_status;
	xlv_mk_context_t	temp_context;
	int 			plex_index = -1;
	int			standalone_plex = 0;
	int			existing_plex = 0;


	XLV_OREF_INIT (&temp_context);

	/*
	 * If a name was supplied and that name did not correspond
	 * to a valid plex number, then we are building a new,
	 * standalone plex. This means that a standalone plex cannot
	 * be named "0", "1", "2", "3"
	 */

	if (plex_name != 0) {
		if ((1 == strlen(plex_name)) && isdigit(plex_name[0]))
			plex_index = atoi(plex_name);

		if ((plex_index < 0) || (plex_index >= XLV_MAX_PLEXES))
			standalone_plex++;
		else
			existing_plex++;
	}



	if (standalone_plex) {

		/* 
		 * If we were constructing some other object, make
		 * sure that it's done.
		 */

		if (xlv_mk_context->obj_type != XLV_OBJ_TYPE_NONE) {

			/* Verify that the previous object is complete. */
			if (! xlv_mk_complete(xlv_mk_context, 1)) {
				fprintf(stderr,
					"Cannot make standalone plex \"%s\"\n",
					plex_name);
				return (XLV_STATUS_INCOMPLETE);
			}

			/* Add the previous object to the hash list. */
			xlv_mk_add_completed_obj (xlv_mk_context,NO);
		}

		/*
		 * Check if the plex has already been created.
		 */
		if (xlv_find_obj_in_table (
		    expert ? xlv_mk_context : &temp_context, plex_name)) {
			return(expert ? XLV_STATUS_OK:XLV_STATUS_PLEX_DEFINED);
		}

		XLV_OREF_INIT (&temp_context);


		/*
		 * The plex does not have a containing subvol.
		 */

		subvol = NULL;
	}
	else {

		/*
		 * This plex is to be added to an existing subvolume.
		 * In this case, the subvolume must already have been
		 * built. 
		 */

		subvol = XLV_OREF_SUBVOL (xlv_mk_context);
		if (subvol == NULL) {
			return (XLV_STATUS_SUBVOL_NOT_DEFINED);
		}

		/*
		 * If we have defined another plex for this subvolume,
		 * make sure that it's complete.
		 */

		if (XLV_OREF_PLEX (xlv_mk_context) != NULL) {
			if (! xlv_mk_complete(xlv_mk_context, 0)) {
				return (XLV_STATUS_INCOMPLETE);
			}
		}

		if (existing_plex) {
			/*
			 * Make sure that the plex exists.
			 */
			if (! (plex_index < subvol->num_plexes)) {
				return (XLV_STATUS_PLEX_NOT_DEFINED);
			}
		} else {
			/*
			 * Make sure that we have not exceeded the max number
			 * of plexes per subvolume.
			 */
			if (subvol->num_plexes == XLV_MAX_PLEXES) {
				return (XLV_STATUS_TOO_MANY_PLEXES);
			}
		}

		/* 
		 * Create a new context.  It should have the same
		 * volume and subvolume fields as the current context.
		 * (Since we are adding a new plex.)
		 */

		XLV_OREF_TYPE(&temp_context) = XLV_OREF_TYPE(xlv_mk_context);
		XLV_OREF_VOL (&temp_context) = XLV_OREF_VOL (xlv_mk_context);
		XLV_OREF_SUBVOL (&temp_context) =
			XLV_OREF_SUBVOL (xlv_mk_context);
	}

	if (existing_plex) {
		plex = subvol->plex[plex_index];

	} else {

		/*
		 * Create a plex record.
		 */
		plex = (xlv_tab_plex_t *) malloc(sizeof (xlv_tab_plex_t));
		bzero (plex, sizeof (xlv_tab_plex_t));

		uuid_create (&plex->uuid, &ignore_status);
		if (ignore_status != 0) {
			free (plex);
			return ((int)ignore_status);
		}

		plex->max_vol_elmnts = 1;
	}

	plex->flags = plex_flags;

	/*
	 * This is the commit point, we are going to modify both
	 * the subvol data structure (if it exists) to point to 
	 * this plex object as well as modify the original 
	 * context.
	 */

	if (subvol != NULL) {
		/*
		 * We just set the plex field of the oref but leave
		 * the object type unchanged; the object as a whole
		 * is not a plex, it's either a subvol or volume.
		 */
		XLV_OREF_TYPE (&temp_context) = XLV_OREF_TYPE (xlv_mk_context);
		XLV_OREF_PLEX (&temp_context) = plex;
		if (!existing_plex) {
			subvol->plex [subvol->num_plexes] = plex;
			subvol->num_plexes++;
		}
	}
	else {
		/*
		 * If there is no owning subvolume, then we
		 * must be defining a top-level plex.  Set the
		 * type of the object to be plex and copy the
		 * name of the plex.
		 */
		XLV_OREF_SET_PLEX (&temp_context, plex);

		ASSERT (plex_name != NULL);
		strncpy (plex->name, plex_name, XLV_NAME_LEN);
	}

	*xlv_mk_context = temp_context;

	return (XLV_STATUS_OK);
} /* end of xlv_mk_plex() */


xlv_vh_entry_t	*xlv_vh_list = NULL;
static xlv_vh_entry_t	*non_xlv_vh_list = NULL;

/*
 * Create a volume element.
 */
int
xlv_mk_ve (xlv_mk_context_t	*xlv_mk_context,
	   xlv_tab_vol_elmnt_t	*I_ve,
	   unsigned		flags)
{
	xlv_tab_vol_elmnt_t	*ve;
	xlv_tab_subvol_t	*subvol;
	xlv_tab_plex_t     	*owning_plex;
	xlv_tab_plex_t		*old_plex;
	unsigned		ignore_status;
	xlv_mk_context_t	temp_context;
	int			p;
	int			d;
	int			subvol_type;
	int			expert;
	xlv_tab_plex_t		*current_plex = NULL;

	XLV_OREF_INIT (&temp_context);

	/*
	 * If a name was supplied, then we are building a new, standalone
	 * volume element.
	 */

	if (I_ve->veu_name[0] != '\0') {
		/* 
		 * If we were constructing some other object, make
		 * sure that previous object is done.
		 */

		if (xlv_mk_context->obj_type != XLV_OBJ_TYPE_NONE) {

			/* Verify that the previous object is complete. */
			if (! xlv_mk_complete(xlv_mk_context, 1)) {
				fprintf(stderr,
					"Cannot make standalone ve \"%s\"\n",
					I_ve->veu_name);
				return (XLV_STATUS_INCOMPLETE);
			}

			/* Add the previous object to the hash list. */
			xlv_mk_add_completed_obj (xlv_mk_context,NO);
		}

		/*
		 * Check if the ve has already been created.
		 */
		expert = flags & EXPERT;
		if (xlv_find_obj_in_table (
		    expert ? xlv_mk_context : &temp_context, I_ve->veu_name)) {
			return (expert ? XLV_STATUS_OK : XLV_STATUS_VE_DEFINED);
		}

		/*
		 * Check if we really meant start block to not be 0
		 */
		 if (!(flags&START)) {
			I_ve->end_block_no = I_ve->end_block_no -
					     I_ve->start_block_no;
			I_ve->start_block_no = 0;
		}

		/*
		 * Check partition type against intended usage
		 */
		if (-1 == xlv_mk_check_partition_type (
				XLV_SUBVOL_TYPE_UNKNOWN,
				I_ve, current_plex, flags)) {
			return (XLV_STATUS_BAD_PARTITION_TYPE);
		}

		XLV_OREF_INIT (&temp_context);

		/*
		 * The volume element does not have a containing plex.
		 */

		owning_plex = NULL;

		/*
		 * Since this is a standalone volume element, we can
		 * just malloc it without setting up any cross-links
		 * from plexes.
		 */
		ve = (xlv_tab_vol_elmnt_t *) malloc (sizeof(*ve));

	}
	else {
		/*
		 * This volume element is to be added to an existing plex.
		 * Make sure the plex exists.
		 */
		owning_plex = XLV_OREF_PLEX (xlv_mk_context);
		if (owning_plex == NULL) {
			return (XLV_STATUS_PLEX_NOT_DEFINED);
		}

		/*
		 * Make sure that we have not exceeded the max number
		 * of volume elements per plex.
		 */
		if (owning_plex->num_vol_elmnts == XLV_MAX_VE_PER_PLEX) {
			return (XLV_STATUS_TOO_MANY_VES);
		}

		/*
		 * Check partition type against intended usage
		 */
		if (xlv_mk_context->subvol != NULL) {
			subvol_type = (int) xlv_mk_context->subvol->subvol_type;
		} else {
			subvol_type = XLV_SUBVOL_TYPE_UNKNOWN;
			current_plex = owning_plex;
		}
		if (-1 == xlv_mk_check_partition_type (
				subvol_type, I_ve,current_plex, flags)) {
			return (XLV_STATUS_BAD_PARTITION_TYPE);
		}

		/* 
		 * The new context should have the same
		 * volume, subvolume, and plex fields as the current context.
		 */
		XLV_OREF_VOL (&temp_context) = XLV_OREF_VOL (xlv_mk_context);
		XLV_OREF_SUBVOL (&temp_context) = 
		    XLV_OREF_SUBVOL (xlv_mk_context);

		/*
		 * Make sure that containing plex has enough space. If 
		 * not, realloc.
		 */

		if (owning_plex->num_vol_elmnts >= 
		    owning_plex->max_vol_elmnts) {

			/* 
			 * Need to extend. Since xlv_tab_plex_t already
			 * has a xlv_tab_vol_elmnt_t, we only need to
			 * extend by max_vol_elmnts to grow it by one
			 * volume element.
			 */

			old_plex = owning_plex;
			owning_plex = realloc (owning_plex,
			    sizeof (xlv_tab_plex_t) + 
			    (sizeof (xlv_tab_vol_elmnt_t) * 
			    owning_plex->max_vol_elmnts) );
			owning_plex->max_vol_elmnts++;

			/*
			 * If this plex belongs to a subvolume, update
			 * it to point to the newly malloc'ed piece.
			 */
			if ((subvol = XLV_OREF_SUBVOL(&temp_context)) != NULL) {
				for (p = 0; p < subvol->num_plexes; p++) {
					if (subvol->plex[p] == old_plex)
						subvol->plex[p] = owning_plex;
				}
			}

		}

		/*
		 * Pointer to next available ve entry in plex.
		 */

		ve = &(owning_plex->vol_elmnts[owning_plex->num_vol_elmnts]);
	}

	bcopy (I_ve, ve, sizeof (xlv_tab_vol_elmnt_t));

	uuid_create (&ve->uuid, &ignore_status);
	if (ignore_status != 0) {
		if (owning_plex == NULL)
			free (ve);
		return ((int)ignore_status);
	}

	/* XXX Should not create a new UUID if this disk part already
	   exists on the disk. */

	for (d=0; d < ve->grp_size; d++) {
		uuid_create (&(ve->disk_parts[d].uuid), &ignore_status);
	}

	XLV_OREF_PLEX (&temp_context) = owning_plex;

	/*
	 * This is the commit point, we are going to modify both
	 * the subvol data structure (if it exists) to point to 
	 * this plex object as well as modify the original 
	 * context.
	 */

	if (owning_plex != NULL) {
		owning_plex->num_vol_elmnts++;
		XLV_OREF_TYPE (&temp_context) = XLV_OREF_TYPE (xlv_mk_context);
		XLV_OREF_VOL_ELMNT (&temp_context) = ve;
	}
	else
		XLV_OREF_SET_VOL_ELMNT (&temp_context, ve);

	*xlv_mk_context = temp_context;

	return (XLV_STATUS_OK);

} /* end of xlv_mk_ve() */


/*
 * Finish up the current object.
 */
int
xlv_mk_finish_curr_obj (xlv_mk_context_t *xlv_mk_context)
{
	if (xlv_mk_complete (xlv_mk_context, 1)) {

		/*
		 * Add the object to our internal hash table.
		 */
		xlv_mk_add_completed_obj (xlv_mk_context,NO);

		XLV_OREF_INIT (xlv_mk_context);
		return XLV_STATUS_OK;
	}
	else {
		/* XXX Should identify the incomplete object */
		return XLV_STATUS_INCOMPLETE;
	}
}


/*
 * Determines whether the current object is complete.
 * Return 1 if the object is complete, otherwise return 0.
 */
static int 
xlv_mk_complete (xlv_mk_context_t *xlv_mk_context, int check_vol)
{
	xlv_tab_vol_entry_t	*vp;
	char			name[250];

	/*
	 * A volume, or piece thereof, is complete if we have
	 * defined it down to the volume element.  Without a volume
	 * element, there is no place to store the configuration. 
	 */
	if (XLV_OREF_VOL_ELMNT(xlv_mk_context) == NULL) {
		xlv_oref_to_string(xlv_mk_context, name);
		fprintf(stderr, "Not finished defining %s\n", name);
		return(0);
	}

	/*
	 * A valid volume must have a data subvolume.
	 */
	if (check_vol && (vp = XLV_OREF_VOL(xlv_mk_context))
	    && (vp->data_subvol == NULL)) {
		/*
		 * Give warning about missing data subvolume.
		 * Really shouldn't be putting the message here
		 * cuz it breaks the model of how xlv_make(1m)
		 * display messages (via tcl) but this is the
		 * simplest place.
		 */
		fprintf(stderr,
			"Volume \"%s\" does not have a data subvol\n",
			vp->name);
		return(0);
	}

	return(1);

	/* XXXjleong Can we just check the dirty flag? */
}



/*
 * As we create an object, we add it to a hash table.
 * 
 * The hash table contains entries of type xlv_oref_t and entries
 * are hashed by their string names.
 *
 * We use the next field of xlv_oref_t to link elements in the 
 * same hash bucket.
 */

#include <tcl.h>

/*
 * We maintain a single table for all objects so that we can
 * search in one place should the user give us a name.
 */

Tcl_HashTable	xlv_obj_table;

/*
 * We also maintain an incore list of all the volume headers that we read
 * in.  As we modify the on-disk labels, this incore data structure is
 * updated. We will use it to generate the list of XLV objects during
 * init and also to write out the new list at the end of xlv_make.
 */

void 
xlv_mk_init (void)
{
	xlv_oref_t		*oref;
	xlv_oref_t		*oref_list;
	int			status;
	xlv_vh_entry_t		*vh_list_ptr;
	xlv_vh_disk_label_t	*label;
	boolean_t		label_converted = B_FALSE;

	Tcl_InitHashTable (&xlv_obj_table, TCL_STRING_KEYS);

	if (xlv_vh_list == NULL) {
		/*
		 * The disk label cache file does not exist.
		 * Try reading label from the disk volume header.
		 */
		xlv_lab2_create_vh_info_from_hinv (&xlv_vh_list,
			&non_xlv_vh_list, &status);
		for (vh_list_ptr = xlv_vh_list; vh_list_ptr != NULL;
		     vh_list_ptr = vh_list_ptr->next) {
			label = XLV__LAB1_PRIMARY(vh_list_ptr);
			if (label->header.version == 1) {
#ifdef CONVERT_OLD_XLVLAB
				xlv_convert_old_labels(vh_list_ptr, 0);
				label_converted = B_TRUE;
#else
				ASSERT(0);
#endif
			}
		}

		/*
		 * xlv_convert_old_labels has the side effect of modifying
		 * xlv_vh_list. So if it ran, read in the xlv_vh_list
		 * again.
		 */
		if (label_converted) {
			xlv_vh_list = NULL;
			xlv_lab2_create_vh_info_from_hinv (&xlv_vh_list,
				NULL, &status);
		}
	}

	/*
	 * Generate the oref list and filter out incomplete volumes.
	 */
	xlv_vh_list_to_oref (
		xlv_vh_list, &oref_list, NULL, NULL, 1, XLV_READ_ALL);

	/*
	 * Add all the existing, complete objects to the hash table.
	 */
	for (oref = oref_list; oref != NULL; oref = oref->next) {
		oref->flag = 0;		/* Initialize flag -- not dirty */
		xlv_mk_add_completed_obj (oref,YES);
	}

} /* end of xlv_mk_init() */


char *
xlv_mk_context_name (xlv_mk_context_t	*ctxt)
{
	char *name;

	if (XLV_OREF_VOL(ctxt) != NULL) {
		name = XLV_OREF_VOL(ctxt)->name;
	} else if (XLV_OREF_PLEX(ctxt) != NULL) {
		name = XLV_OREF_PLEX(ctxt)->name;
	} else if (XLV_OREF_VOL_ELMNT(ctxt) != NULL) {
		name = XLV_OREF_VOL_ELMNT(ctxt)->veu_name;
	} else {
		name = NULL;
	}
	return(name);
} /* end of xlv_mk_context_name() */


void
xlv_mk_add_completed_obj (
	xlv_mk_context_t	*xlv_mk_context,
	int			initflag)
{
	xlv_oref_t	*hash_item;	/* new object we want to add. */
	int		found = 0;
	char		*name;

	/*
	 * The hash key must be null-terminated.  So we allocate
	 * an extra byte for the NULL in case the top-level
	 * object name takes up XLV_NAME_LEN characters.
	 */

	char		hash_key [XLV_NAME_LEN+1];
	Tcl_HashTable	*hash_table;
	int		new_entry_created;

	Tcl_HashEntry	*curr_entry;	/* current entry hashed to 
					   same location. */

	/*
	 * Set global so quit knows that work was done
	 */
	if (!initflag)
		xlv_mk_new_obj_defined = 1;

	hash_item = (xlv_oref_t *) malloc (sizeof (xlv_oref_t));
	*hash_item = *xlv_mk_context;

	name = xlv_mk_context_name(xlv_mk_context); 
	if (name == NULL) {
		ASSERT (0);	/* This object is not complete */
	}
	strncpy (hash_key, name, XLV_NAME_LEN);

	hash_table = &xlv_obj_table;
	hash_key[XLV_NAME_LEN] = '\0';

	if (XLV_OREF_SUBVOL (xlv_mk_context) != NULL) {
		/*
		 * Set the subvolume depth here. This information is
		 * needed by disk-label-reading function to determine
		 * if a "unique" or "common" piece is missing when
		 * there are missing parts.
		 */
		XLV_OREF_SUBVOL(xlv_mk_context)->subvol_depth =
			(short) xlv_tab_subvol_depth(XLV_OREF_SUBVOL(xlv_mk_context));
	}

	/*
	 * Add this item to the hash table. 
	 * 
	 * We only want to add one entry under each hash key. Since
	 * there may be multiple objects with the same name, we 
	 * check for an existing entry & add the new one to the
	 * end if there is an entry already.
	 */
	curr_entry = Tcl_CreateHashEntry (
                        hash_table, hash_key, &new_entry_created);

	if (new_entry_created) {
		hash_item->next = NULL;
	}
	else {
		/*
		 * This key already exists in the hash table.
		 * See if it is the same object.
		 */
		xlv_oref_t *oref;

		oref = (xlv_oref_t *)Tcl_GetHashValue(curr_entry);
		while (oref) {
			if (XLV_OREF_VOL (hash_item)) {
				found = (XLV_OREF_VOL(oref) ==
					 XLV_OREF_VOL(hash_item));
			}
			else if (XLV_OREF_PLEX (hash_item)) {
				found = (XLV_OREF_PLEX(oref) ==
					 XLV_OREF_PLEX(hash_item));
			}
			else if (XLV_OREF_VOL_ELMNT (hash_item)) {
				found = (XLV_OREF_VOL_ELMNT(oref) ==
					 XLV_OREF_VOL_ELMNT(hash_item));
			}
			else {
				ASSERT (0);	/* cannot be -- see above */
			}

			if (found) {
				/*
				 * Same object, so just copy new
				 * flag value.
				 */
				oref->flag = xlv_mk_context->flag; 
				free(hash_item);
				return;
			}

			oref = oref->next;
		}

		/*
		 * XXX 11/12/94	   But xlv_find_obj_in_table() returns
		 * the first object with this name. Maybe we shouldn't
		 * bother linking the collision hash items.
		 *
		 * So there will more than one object with this name.
		 * In this case, we link the new object in.
		 */
		hash_item->next = Tcl_GetHashValue (curr_entry);
	}

	Tcl_SetHashValue (curr_entry, hash_item);

} /* end of xlv_mk_add_completed_obj() */


/* 
 * Prompt the user for yes or no answer
 */
static int
xlv_mk_ask_yes_or_no(char *query, int yes_or_no)
{
	int firstletter;
	char yes[]="(yes)", no[]="(no)", *defaultstr;

	if (yes_or_no == YES)
		defaultstr = yes;
	else
		defaultstr = no;
	printf("%s%s  ",query,defaultstr);
	firstletter=getchar();
	fflush(stdin);
	switch(firstletter){
	case 'y':
		return YES;
	case 'n':
		return NO;
	case EOF:
		return EOF;
	default:
		if (defaultstr == yes)
			return YES;
		else
			return NO;
	}
}
	

/*
 * Verify that user wishes to write any newly created
 * objects to disk
 */

int
xlv_mk_exit_check(void)
{
	int  rc;
	char query[] = "Newly created objects will be written to disk.\n"
		       "Is this what you want?";

	if (Tclflag)
		return YES;

	if (xlv_mk_new_obj_defined) {
		rc = xlv_mk_ask_yes_or_no(query,YES);
		switch(rc) {
		case YES:
		case EOF:
			return YES;
		case NO:
			return NO;
		default:
			printf("xlv_mk_exit_check(): Bad return code %d\n",rc);
			return YES;
		}
	}
	return YES;
} /* end of xlv_mk_exit_check() */
		
/*
 * Check to see if any objects have been created
 * or partially created
 */
int
xlv_mk_quit_check(xlv_mk_context_t *current_context)
{
	int  rc;
	char template[] =
	    "There are %s created objects that will be lost if you quit now.\n"
	    "Do you still wish to quit?";
	char query[250];
	char badrc_format[] = "xlv_mk_quit_check(): Bad return code %d\n";

	if (Tclflag)
		return YES;

	/*
	 * Check for newly created objects
	 */
	if (xlv_mk_new_obj_defined) {
		sprintf(query, template, "newly");
		rc = xlv_mk_ask_yes_or_no(query,NO);
		switch(rc) {
		case EOF:
			printf("\nQuitting without updating\n");
			/* FALLTHROUGH */
		case YES:
			return YES;
		case NO:
			return NO;
		default:
			printf(badrc_format, rc);
			return YES;
		}
	}
	/*
	 * Check for partially created objects
	 */
	if (! xlv_oref_is_null(current_context)) {
		sprintf(query, template, "partially");
		rc = xlv_mk_ask_yes_or_no(query,YES);
		switch(rc) {
		case YES:
		case EOF:
			return YES;
		case NO:
			return NO;
		default:
			printf(badrc_format, rc);
			return YES;
		}
	}
	return YES;
} /* end of xlv_mk_quit_check() */


/*
 * Compare two context object and determine if they refer to
 * the same thing.
 */
static int
xlv_mk_same_oref(xlv_mk_context_t *ctxt1, xlv_mk_context_t *ctxt2)
{
	int same = 0;

	if ( XLV_OREF_VOL(ctxt1) ) {
		if ( XLV_OREF_VOL(ctxt2) )
			same = !strncmp(XLV_OREF_VOL(ctxt1)->name,
					XLV_OREF_VOL(ctxt2)->name,
					XLV_NAME_LEN);
	}
	else if ( XLV_OREF_PLEX(ctxt1) ) {
		if ( XLV_OREF_PLEX(ctxt2) ) 
			same = !strncmp(XLV_OREF_PLEX(ctxt1)->name,
					XLV_OREF_PLEX(ctxt2)->name,
					XLV_NAME_LEN);
	}
	else if ( XLV_OREF_VOL_ELMNT(ctxt1)) {
		if ( XLV_OREF_VOL_ELMNT(ctxt2) )
			same = !strncmp(XLV_OREF_VOL_ELMNT(ctxt1)->veu_name,
					XLV_OREF_VOL_ELMNT(ctxt2)->veu_name,
					XLV_NAME_LEN);
	}

	return (same);
}
		

/* 
 * Print all the objects that have been created.
 */

void
xlv_mk_print_all (xlv_mk_context_t *current_context, int p_mode)
{
	Tcl_HashEntry	*hash_entry;
	Tcl_HashSearch	search_ptr;
	xlv_oref_t	*oref;
	int		count = 0;

	/*
	 * XXX Add way to set print mode.
	 */

	/*
	 * Print out current, uncommitted object.
	 */
	if (! xlv_oref_is_null (current_context)) {
		printf ("\tCurrent Working Object\n");
		xlv_oref_print (current_context, p_mode);
	}

	/*
	 * Print out all the committed objects.
	 */
	hash_entry = Tcl_FirstHashEntry (&xlv_obj_table, &search_ptr);

	if (hash_entry != NULL) {
		printf("\n\tCompleted Objects\n");
	}
	while (hash_entry != NULL) {

		oref = (xlv_oref_t *)  Tcl_GetHashValue (hash_entry);
		ASSERT (oref != NULL);

		for ( ; oref != NULL; oref = oref->next) {
			/*
			 * Don't display the current context again.
			 */
			if (xlv_mk_same_oref(oref, current_context))
				continue;

			if (count)
				printf("\n");
			printf("(%d)  ", ++count);

			xlv_oref_print (oref, p_mode);
		}

		hash_entry = Tcl_NextHashEntry (&search_ptr);
	}

} /* end of xlv_mk_print_all() */


/*
 * Argument passed to construct_ptype_list.
 *   partition_type indicates the partition type to write in the disk 
 *      header for each disk part in this volume element.
 *   ptype_update_list is an output argument. It is set to the array
 *      of volume header information that needs to be updated.
 */
typedef struct {
	unsigned		partition_type;		/* input */
	xlv_ptype_update_list_t	*ptype_update_list;	/* output */
} xlv_mk_construct_t;


/*
 * This routine is called by xlv_for_each_ve() to accumulate a list
 * of vh's for newly created objects. The input specifies the
 * partition type to put in the partition header for the affected
 * disk parts. The output is the head of the vh list.
 *
 * This routine returns 0 if we should continue processing.
 * It returns 1 if we should terminate the iteration.
 */
static int
construct_ptype_list(xlv_oref_t		*oref, 
		     xlv_mk_construct_t	*mk_const)
{
	int	i;
	int	j;
	int	num_partitions;
	int	grp_size;
	char	vh_name[MAXDEVNAME];
	char	vh_temp[MAXDEVNAME];
	int     len;
	dev_t	partnum;
	dev_t	devnum;
	struct  stat64 sbuf;

	/*
	 * variables holding state information
	 */
	static	dev_t	disk_list[XLV_MAX_DISKS];
	static	xlv_ptype_update_list_t ptype_update_list[XLV_MAX_DISKS];
	static	int	num_disks = 0;

	/*
	 * Initialize the "ptype_update_list" array.
	 * "num_partitions == 0" indicates an emtpy slot.
	 */
	if (num_disks == 0) {
		for (i = 0; i < XLV_MAX_DISKS; i++)
			ptype_update_list[i].num_partitions = 0;
	}

	ASSERT (mk_const->partition_type == PTYPE_XLV);

	grp_size = (int) oref->vol_elmnt->grp_size;

	/*
	 * For each partition in the ve
	 */
	for (i = 0; i < grp_size; i++) {

		devnum = DP_DEV(oref->vol_elmnt->disk_parts[i]);
		partnum = dev_to_partnum(devnum);

		/* convert dev to hwg pathname */
		len = sizeof(vh_temp);
		if (dev_to_devname(devnum, vh_temp, &len) == NULL)
			goto error;

		/* switch to volheader pathname */
		len = sizeof(vh_name);
		if (path_to_vhpath(vh_temp, vh_name, &len) == NULL) {
			fprintf(stderr,
				"Cannot generate vh pathname for device 0x%x\n",
				devnum);
                        goto error;
		}

		/* use rdev of the vh partition to match against */
		if (stat64(vh_name, &sbuf) == -1)
			goto error;

		/*
		 * See if we are already modifying some other parition
		 * on this disk. If so, update the ptype_update_list entry
		 * for that disk.
		 */
		for (j = 0;j < num_disks;j++) {
			if (disk_list[j] == sbuf.st_rdev)
				goto found;
		}

		/*
		 * This is a new disk; add it to disk_list[].
		 */
		num_disks++;
		if (num_disks > XLV_MAX_DISKS) {
			fprintf(stderr, "Exceeding the maximum of %d disks.\n",
				XLV_MAX_DISKS);
                        goto error;
		}

		strcpy(ptype_update_list[j].vh_pathname, vh_name);
		/* use the vh devno as the matching criteria */
		disk_list[j] = sbuf.st_rdev;
		ptype_update_list[j].num_partitions = 0;

		/* FALLTHROUGH */
found:
		/*
		 * Add this partition to the list of partitions on this
		 * disk that needs to be modified.
		 * For each partition, we record the partition number and
		 * the partition type.
		 */
		num_partitions = ptype_update_list[j].num_partitions++;
		ptype_update_list[j].partition[num_partitions] = partnum;
		ptype_update_list[j].partition_type[num_partitions] =
			mk_const->partition_type;

	}
	mk_const->ptype_update_list = ptype_update_list;
	return 0;	/* continue scan */
error:
	mk_const->ptype_update_list = NULL;
	return 1;	/* error, discontinue scan */

} /* end of construct_ptype_list() */


/*
 * Change vh type from PTYPE_EFS or PTYPE_XFS to PTYPE_XLV
 */
int
xlv_mk_update_vh(void)
{
	Tcl_HashEntry   	*hash_entry;
	Tcl_HashSearch  	search_ptr;
	xlv_oref_t		*oref;
	xlv_oref_t		tmp_oref;
	xlv_oref_t		*oref_ptr;
	xlv_mk_construct_t	mk_const;
	xlv_tab_subvol_t        *subvol;
	boolean_t		need_to_update_vh = B_FALSE;

	mk_const.ptype_update_list = NULL;
	mk_const.partition_type = PTYPE_XLV;

	hash_entry = Tcl_FirstHashEntry (&xlv_obj_table, &search_ptr);

	/*
	 * For each object in the hash table
	 */
	for (; hash_entry != NULL;
	     hash_entry = Tcl_NextHashEntry (&search_ptr)) {
		oref = (xlv_oref_t *)  Tcl_GetHashValue (hash_entry);
		for (oref_ptr = oref; oref_ptr != NULL;
		     oref_ptr = oref_ptr->next) {

			/*
			 * Old objects
			 * This object was already created when xlv_make
			 * ran. It was not created during this session.
			 *
			 * XXX Need to find out why vol_elmnt == NULL.
			 */
			if (oref_ptr->vol_elmnt == NULL) {
				continue;
			}

			/*
			 * At this point, the object is one that was newly
			 * created during this invocation of xlv_make.
			 */
			if (oref_ptr->subvol == NULL) {

				/*
				 * A standalone plex or volume element.
				 * Standalone objects are always marked
				 * with partition type PTYPE_XLV.
				 */
				xlv_for_each_ve_in_obj (oref_ptr,
					(XLV_OREF_PF)construct_ptype_list,
					&mk_const);
				if (mk_const.ptype_update_list == NULL) 
					return (XLV_STATUS_CANT_WRITE_VH);
				
				need_to_update_vh = B_TRUE;

			} else {
				/*
				 * We have a volume.
				 */

				XLV_OREF_COPY(oref_ptr, &tmp_oref);

				subvol = XLV_OREF_VOL(oref_ptr)->log_subvol;
				if (subvol) {
					XLV_OREF_SUBVOL(&tmp_oref) = subvol;

					xlv_for_each_ve_in_subvol(&tmp_oref,
					  (XLV_OREF_PF)construct_ptype_list,
					  &mk_const);
					if (mk_const.ptype_update_list==NULL) 
					    return (XLV_STATUS_CANT_WRITE_VH);

					need_to_update_vh = B_TRUE;
				}

				subvol = XLV_OREF_VOL(oref_ptr)->rt_subvol;
				if (subvol) {
					XLV_OREF_SUBVOL(&tmp_oref) = subvol;

					xlv_for_each_ve_in_subvol(&tmp_oref,
					  (XLV_OREF_PF)construct_ptype_list,
					  &mk_const);
					if (mk_const.ptype_update_list==NULL) 
					    return (XLV_STATUS_CANT_WRITE_VH);

					need_to_update_vh = B_TRUE;
				}

				subvol = XLV_OREF_VOL(oref_ptr)->data_subvol;
				if (subvol) {
					XLV_OREF_SUBVOL(&tmp_oref) = subvol;

					xlv_for_each_ve_in_subvol(&tmp_oref,
					  (XLV_OREF_PF)construct_ptype_list,
					  &mk_const);
					if (mk_const.ptype_update_list==NULL) 
					    return (XLV_STATUS_CANT_WRITE_VH);

					need_to_update_vh = B_TRUE;
				}
			}
		}
	}

	if (need_to_update_vh) {
		ASSERT(mk_const.ptype_update_list);
		if (xlv_lab1_update_ptype (mk_const.ptype_update_list) == -1) {
			return (XLV_STATUS_CANT_WRITE_VH);
		}
	}
	return (XLV_STATUS_OK);

} /* end of xlv_mk_update_vh() */


/*
 * Write all the objects out to the disk labels.  Only committed
 * objects are written out.
 *
 * XXX Need to tag objects that have not been changed so we don't
 * write them again.
 *
 * XXX Need some way to pass more information error messages back
 * in case of failure.
 */

int
xlv_mk_write_disk_labels (int *write_cnt, int verbose)
{
	Tcl_HashEntry   *hash_entry;
	Tcl_HashSearch  search_ptr;
	xlv_oref_t      *oref;
	int		status;
	int		error = 0;		
	int		count = 0;
	char		*name;

	hash_entry = Tcl_FirstHashEntry (&xlv_obj_table, &search_ptr);

	while (hash_entry != NULL) {

		oref = (xlv_oref_t *) Tcl_GetHashValue (hash_entry);
		ASSERT (oref != NULL);

		for ( ; oref != NULL; oref = oref->next) {
			if (! XLV_MK_CTXT_DIRTY(oref))
				continue;

			if (verbose) {
				name = xlv_mk_context_name(oref);
				printf("Writing disk labels for %s\n", name);
			}

			xlv_lab2_write_oref (&xlv_vh_list, oref, NULL,
					     &status, XLV_LAB_WRITE_FULL);
			if (status != 0) {
				error++;
				/*
				 * Failed to update this object's label
				 * but continue on with other objects.
				 */
			} else {
				count++;
				XLV_MK_CTXT_UNSET_DIRTY(oref);
			}
		}

		hash_entry = Tcl_NextHashEntry (&search_ptr);
	}

	*write_cnt = count;

	return ((error) ? XLV_STATUS_CANT_WRITE_LABEL: XLV_STATUS_OK);
}




/*
 * The following typedef's and internal routines are to support
 * xlv_mk_disk_part_in_use().
 */
typedef struct {
	dev_t	dev;
	int	found;
} cmp_dev_t;
 

static int
cmp_dev	(xlv_oref_t	*oref,
	 cmp_dev_t	*arg_blk)
{
	xlv_tab_vol_elmnt_t     *ve = XLV_OREF_VOL_ELMNT(oref);
	int			i;

	for (i = 0; i < ve->grp_size; i++) {
		if (DP_DEV(ve->disk_parts[i]) == arg_blk->dev) 
			arg_blk->found = 1;
	}

	return (0);	/* continue iteration */
}

static int
disk_part_in_obj (xlv_oref_t	*oref,
		  dev_t		dev)
{
	cmp_dev_t	arg_blk;

	arg_blk.found = 0;
	arg_blk.dev = dev;
	xlv_for_each_ve_in_obj (oref, (XLV_OREF_PF)cmp_dev, &arg_blk);

	return (arg_blk.found);
}


/*
 * Returns 1 if the disk partition is part of an existing XLV
 * object.  This routine first checks the current uncommitted
 * object as described by oref.  It then iterates through all
 * the committed objects in the object table.
 */
/*ARGSUSED1*/
int
xlv_mk_disk_part_in_use (xlv_oref_t	*current_oref,
			 char		*device_path, 
			 dev_t		dev)
{
        Tcl_HashEntry   *hash_entry;
        Tcl_HashSearch  search_ptr;
        xlv_oref_t      *oref;

	if ((! xlv_oref_is_null(current_oref)) &&
	    disk_part_in_obj (current_oref, dev)) {
		return (1);
	}

        hash_entry = Tcl_FirstHashEntry (&xlv_obj_table, &search_ptr);

        while (hash_entry != NULL) {

                oref = (xlv_oref_t *)  Tcl_GetHashValue (hash_entry);
                ASSERT (oref != NULL);

                do {
                        if (disk_part_in_obj (oref, dev)) 
				return (1);
			
                        oref = oref->next;
                } while (oref != NULL);

                hash_entry = Tcl_NextHashEntry (&search_ptr);
        }
	return (0);
}


/*
 * Find the object in the hash table with the given name.
 * Return 1 if found otherwise return 0.
 */
static int
xlv_find_obj_in_table(xlv_mk_context_t	*xlv_mk_context,
		      xlv_name_t	name)
{
	boolean_t	found;
	xlv_oref_t	*oref;
	Tcl_HashEntry	*entry;

	if (!name)
		return (0);
	entry = Tcl_FindHashEntry(&xlv_obj_table, name);

	if (!entry)
		return (0);

	oref = (xlv_oref_t *)Tcl_GetHashValue(entry);
	while (oref) {
		if ( XLV_OREF_VOL(oref) &&
		    !strcmp(XLV_OREF_VOL(oref)->name, name) ) 
			found = B_TRUE;
		else if ( XLV_OREF_PLEX(oref) &&
		    !strcmp(XLV_OREF_PLEX(oref)->name, name) ) 
			found = B_TRUE;
		else if ( XLV_OREF_VOL_ELMNT(oref) &&
		    !strcmp(XLV_OREF_VOL_ELMNT(oref)->veu_name, name) ) 
			found = B_TRUE;
		else
			found = B_FALSE;

		if (found) {
			XLV_OREF_COPY(oref, xlv_mk_context);
			return(1);
		}
		oref = oref->next;
	}

	return (0);
}

/*
 * Text strings for xlv_mk_check_partition_type
 */
static	char	template[] = "Rejecting ve containing %s%s\n%s%s";
static	char	NotFound[] = "Partition not found in volume header list";
static	char	BadDataType[] = "Subvolume type data must have partition type xfs,efs,or xlv";
static	char	BadRtType[] = "Subvolume type rt must have partition type xfs,efs,or xlv";
static	char	BadLogType[] = "Subvolume type log must have partition type xfslog";
static	char	BadVeType[] = "Standalone ve must have partition type xfs,efs,xlv,or xfslog";
static	char	BadPlexType[] = "Standalone plex must have partition type xfs,efs,xlv,or xfslog";
static	char	BadStandaloneVe[] = "Mixing partition type xfslog with data types not allowed";
static	char	BadStandalonePlex[] = "Partition type must be consistent with other ve's in plex";
static	char	BadLvolType[] = "Partition could already belong to lv. Check /etc/lvtab";
static	char	IllegalType[] = "Illegal partition type";
static	char	ReallyBad[] = "Subvolume type does not match any known";
static	char	Force[] = "\nUse \"ve -force\" to override or invoke with \"-f\" flag";

/*
 * The following routine checks the partition type in the disk parts that
 * make up a ve.
 * If the ve is part of a volume, then we check it against the type of the
 * subvolume to which the ve belongs.
 * If the ve is standalone, then we check to make sure that either all
 * the partitions are xlv or xlvlog.
 *
 * On input, subvol_type is set to XLV_SUBVOL_TYPE_UNKNOWN if this is a
 * standalone plex or ve.
 */
static int
xlv_mk_check_partition_type (int			subvol_type,
			     xlv_tab_vol_elmnt_t	*ve,
			     xlv_tab_plex_t		*plex,
			     unsigned			flags)
{
	int	i;
	int	j;
	int	k;
	int	grp_size;
	int	ve_type;
	int	plex_type;
	int	partition_type;
	dev_t	ve_devnum;
	dev_t	ve_partition;
	unsigned	forceflag;
	xlv_vh_entry_t	*vh_ptr;
	xlv_vh_entry_t	*vh_lists[2];
	int	badflag = 0;
	int	old_ve_type = -1;
	int	old_plex_type = -1;
	int	num_passes = 1;
	dev_t	old_ve_devnum = -1;
	dev_t	old_ve_partition = -1;
	char    devname[MAXDEVNAME];
	char    vhdevname[MAXDEVNAME];
	int     len;

	vh_lists[0] = xlv_vh_list;
	vh_lists[1] = non_xlv_vh_list;
	forceflag = flags&FORCE;

	/*
	 * If this volume element is part of a plex and the plex already
	 * has another volume element, then we need to do 2 passes 
	 * through the following for-loop to make sure that the 
	 * partition types in all the volume elements are the same.
	 *
	 * This only needs to be done for standalone plexes since it's 
	 * the only case where we don't know the partition type ahead
	 * of time; it can be either xlv or xfslog.  (If the ve is part
	 * of a log subvolume, for example, we know that it should be xfslog.)
	 */
	num_passes = 1;
	if (plex && (plex->num_vol_elmnts > 0))
		num_passes = 2;

	/*
	 * Check every ve in the (standalone) plex
	 * 
	 * We go through this for loop either once or twice.
	 * In the first pass, we make sure that all the disk parts across
	 * a ve are the same.
	 * The second pass is run only for standlone plexes. Here, we check
	 * that all the ve's in a plex have the same partition type (either
	 * xlv or xfslog.)
	 */
	for (i = 0; i < num_passes; i++) {

		/*
		 * Check every partition in the ve.
		 * If we are in pass 2 where we are checking that ve's
		 * in the same plex have the same partition type, then
		 * we only need to check the first disk part in each
		 * ve (since we've already established in pass 1 that 
		 * all the disk parts in a ve have the same ptype). 
		 */
		grp_size = (int) ve->grp_size;
		for (j=0;j<grp_size;j++) {

			ve_devnum = DP_DEV(ve->disk_parts[j]);

			len = sizeof(devname);
			if (dev_to_devname(ve_devnum, devname, &len) == NULL) {
				fprintf(stderr,
			"xlv_make: unable to convert dev %d to devname\n",
					DP_DEV(ve->disk_parts[j]));
				continue;
			}

			len = sizeof(vhdevname);
			if (path_to_vhpath(devname, vhdevname, &len) == NULL) {
				fprintf(stderr,
			"xlv_make: unable to convert path to vhpath: %s\n",
					devname);
				continue;
			}

			/*
			 * If not in xlv_vh_list, try non_xlv_vh_list
			 * (disks without an xlv_label)
			 */
			for (k=0;k<2;k++) {
				/*
				 * Find correct vh entry
				 */
				for (vh_ptr=vh_lists[k]; vh_ptr != NULL;
				     vh_ptr=vh_ptr->next) {

					/* check if vol headers match,
					 * if so, we're the same device
					 */
					if (strcmp(vh_ptr->vh_pathname,
						   vhdevname) == 0) {
						ve_partition = path_to_partnum(devname);
						if (ve_partition == -1) {
							fprintf(stderr,
	        "xlv_make: unable to get partition number from pathname: %s\n",
								devname);
							continue;
						}
						goto found;
					}
				}
			}
			xlv_print_type_check_msg (template,NULL,NotFound,
				ve_devnum,ve_partition,NULL);
			return -1;

found:
			partition_type = vh_ptr->vh.vh_pt[ve_partition].pt_type;

			/*
			 * Reject, under all circumstances, partitions of
			 * the following types
			 */
			switch(partition_type) {
				case PTYPE_VOLHDR:
				case PTYPE_VOLUME:
					xlv_print_type_check_msg(template,
	vh_ptr->vh_pathname,IllegalType,ve_devnum,ve_partition,NULL);
				return -1;
			}
			/*
			 * Check ve type against partition type
			 */
			ve_type = xlv_check_partition_type (
					subvol_type, partition_type, YES);
			if (ve_type == -1 && !forceflag) {
				const char *Bad;

				if (partition_type == PTYPE_LVOL ||
				    partition_type == PTYPE_RLVOL) {
					Bad = BadLvolType;
				} else {
					switch (subvol_type) {
					case XLV_SUBVOL_TYPE_LOG:
						Bad = BadLogType;
						break;
					case XLV_SUBVOL_TYPE_DATA:
						Bad = BadDataType;
						break;
					case XLV_SUBVOL_TYPE_RT:
						Bad = BadRtType;
						break;
					case XLV_SUBVOL_TYPE_UNKNOWN:
						Bad = (plex) ?
							BadPlexType : BadVeType;
						break;
					default:
						Bad = ReallyBad;
					}
				}
				xlv_print_type_check_msg (
					template, vh_ptr->vh_pathname, Bad,
					ve_devnum, ve_partition, Force);
				badflag++;
			}

			/*
			 * If we are in pass 2 where we are checking
			 * that all the ve's in this [standlone] plex
			 * have the same ptype, we only need to look at
			 * the first partition. We've already established
			 * in pass 1 that all the partitions in the ve
			 * have the same ptype.
			 */
			if (i > 0) {
				break;
			}

			/*
			 * If we reach this part, we are still in the 
			 * first pass.
			 *
			 * If not standalone, check all partitions in the ve
			 */
			if (subvol_type != XLV_SUBVOL_TYPE_UNKNOWN) {
				continue;
			}

			/*
			 * If we reach here, we are in pass 1 and we have
			 * a standalone plex or volume element. Since it's
			 * a standalone plex or ve, they can have either
			 * ptype xfslog or xlv.
			 * Regardless of which it is, make sure that they
			 * are the same ptype in all the disk parts that
			 * make up this ve.
			 */
			if (ve_type != old_ve_type && old_ve_type != -1 &&
			    !forceflag) {
				if (!badflag++) {
					xlv_print_type_check_msg(template,
vh_ptr->vh_pathname,BadStandaloneVe,ve_devnum,ve_partition,Force);
				}
				goto bad;
			} else {
				old_ve_type = ve_type;
			}
		}
		/*
		 * Type to check against one ve in the standalone plex
		 */
		plex_type = ve_type;

		/*
		 * Check that log and data types are not mixed in ve's
		 * across a standalone plex
		 */
		if (plex_type != old_plex_type && old_plex_type != -1 &&
		    !forceflag) {
			if (!badflag++) {
				xlv_print_type_check_msg(template,
vh_ptr->vh_pathname,BadStandalonePlex,old_ve_devnum,old_ve_partition,Force);
			}
			goto bad;
		} else {
			old_plex_type = plex_type;
		}
		old_ve_type = -1;	/* XXX This can be eliminated. */
		old_ve_devnum = ve_devnum;
		old_ve_partition = ve_partition;
		/*
		 * Get partition_type for first ve in standalone plex 
		 * to check against current ve
		 */
		ve = &(plex->vol_elmnts[0]);
	}

	/* FALLTHROUGH */
bad:
	return (badflag ? -1 : 0);

} /* end of xlv_mk_check_partition_type() */

