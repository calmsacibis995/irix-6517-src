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
#ident "$Revision: 1.60 $"

#include <errno.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <diskinvent.h>
#include <syslog.h>
#include <bstring.h>
#include <sys/debug.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/xlv_vh.h>
#include <xlv_oref.h>
#include <xlv_lab.h>
#include <xlv_utils.h>
#include <pathnames.h>

/*
 * The default hostname. Always assume that the default name means the
 * local host.
 */
#define	_STANDALONE_NODENAME	"IRIS"

/*
 * Note: For the following routines, "nodename == NULL" implies local host.
 */

static int xlv__vh_to_tab_vol (
        xlv_vh_disk_label_entry_t *label,
        xlv_tab_vol_t		  *tab_vol,
        xlv_tab_t		  *xlv_tab,
        xlv_vh_entry_t		  **orphaned_vh_list,
        char			  *nodename,
	int			  mk_vol_device,
	int			  *status);

static xlv_tab_subvol_t *xlv__vh_alloc_mem (
        xlv_vh_disk_label_entry_t *label,
        xlv_tab_vol_t		  *tab_vol,
        xlv_tab_t		  *xlv_tab,
        char			  *nodename,
	int			  mk_vol_device,
	int			  *status);

static void xlv__vh_to_tab_vol_copy_fields (
	xlv_vh_disk_label_entry_t *lab,	/* vol record in label */
	xlv_tab_subvol_t	  *sv);

static void xlv__vh_lab_to_oref (
	char			  *owner,
        xlv_vh_disk_label_entry_t *lab,
        xlv_oref_t		  **oref_pieces_p,
	int			  *status);

/*
 * Check the partition types in the volume header to make sure that
 * they match the volume configuration in the XLV label.
 */
static int xlv__check_label_entry (
	xlv_vh_disk_label_entry_t	*lep,
	xlv_vh_entry_t			*vh_list);


/*
 * Take the volume geometry described in the in-core volume header/
 * xlv label list (vh_list) and create the in-core xlv_tab and
 * xlv_tab_vol structures.
 *
 * Volume headers for standalone pieces (i.e., plexes and volume
 * elements that are not part of a volume) are ignored.
 *
 * If a piece of an incomplete volume is found, then it is threaded
 * onto the orphaned_vh_list.
 *
 * This routine assumes that the labels are all in-core and
 * consistent and that sufficient space has been allocated in
 * tab_vol and xlv_tab.
 */

void
xlv_vh_to_xlv_tab (
        xlv_vh_entry_t	*vh_list,
        xlv_tab_vol_t	*tab_vol,
        xlv_tab_t	*xlv_tab,
        xlv_vh_entry_t	**orphaned_vh_list,
	int		flags,	
        int		*status)
{
	xlv_vh_entry_t		  *vh_entry;
	xlv_vh_disk_label_t 	  *label;
	xlv_vh_disk_label_entry_t *lep;		/* label entry pointer */
	int			  i, s;
	unsigned		  st;
	char			  *me;
	char			  *owner;	/* disk owner, NULL if local */
	__int64_t		  sv_size;

	/* XXXjleong	Create an ignored_vh_list? */
	*orphaned_vh_list = NULL;

	tab_vol->num_vols = 0;
	xlv_tab->num_subvols = 0;

	me = xlv_getnodename();

	/*
	 * Make sure that the labels and disk parts are consistent.
	 */
	xlv_lab2_make_consistent (vh_list, status);

	/* if couldn't make the labels consistent, let the caller retry */
	if (*status == EAGAIN)
		return;

#ifdef NOT_HWGRAPH
	/*
	 * Update the incore disk part entries to reflect the current
	 * devices to which the disks are attached.
	 */
	xlv_dev_make_consistent (vh_list, status);
#endif

	/*
	 * The labels and disk parts are consistent.
	 *
	 * For each disk part on every xlv label, copy the information
	 * from the disk label to the xlv_tab and tab_vol structs.
	 */
	for (vh_entry = vh_list; vh_entry != NULL; vh_entry = vh_entry->next) {

		label = XLV__LAB1_PRIMARY (vh_entry);
		owner = label->header.nodename;

		if (!strncmp(owner, me, XLV_NODENAME_LEN) ||
		    !strncmp(owner, _STANDALONE_NODENAME, XLV_NODENAME_LEN)) {
			/*
			 * Disk configurated on this system
			 */
			if (! (flags & XLV_READ_DOMESTIC) )
				continue;
			owner = NULL;		/* NULL means local system */
		} else {
			/*
			 * Disk configurated on another system
			 */
			if (! (flags & XLV_READ_FOREIGN) )
				continue;
		}

		for (i=0; i < XLV_MAX_DISK_PARTS; i++) {
			lep = &label->label_entry[i];

			if (uuid_is_nil (&(lep->vol.volume_uuid),&st)) {
				continue;
			}

			/*
			 * Ignore entries that don't match the
			 * volume header info.
			 */
			if (xlv__check_label_entry (lep, vh_entry) != 0) {
				continue;
			}
			/*
			 * Watch out for exceeding the size of
			 * the caller's passed-in table.
			 */
			if (xlv__vh_to_tab_vol (
					lep, tab_vol, xlv_tab,
					orphaned_vh_list, owner,
					(flags & XLV_READ_CREATE_DEVICE),
					status) ) {
				/*
				 * Failed to copy to table.
				 * XXX What is the right thing to do now?
				 */
				if (*status == E2BIG || *status == ENOMEM) {
					/*
					 * The kernel cannot handle this
					 * many volumes so it is best to
					 * stop here.
					 */
					return;
				} else {
					/*
					 * Instead of return()'ing, let's just
					 * continue on with building the table.
					 * Maybe this subvolume can just be
					 * filtered out later.
					 */
					continue;
				}
			}
		}
	}

	/*
	 * We don't yet handle missing pieces of volumes.
	 */
	ASSERT (! *orphaned_vh_list);

	/*
	 * Handle partial volumes
	 */
	xlv_check_xlv_tab(tab_vol, xlv_tab);

	/*
	 * Now compute the derived fields of the xlv_tab.
	 * For now, we just compute the subvolume sizes.
	 *
	 * We search the whole table because it is sparse
	 * (indexed by available minor device numbers.)
	 */
	for (s = 0; s < xlv_tab->max_subvols; s++) {

		/*
		 * We assume that the subvol entry is valid if there is
		 * a non-NULL vol_p pointer.
		 */
		if (XLV_SUBVOL_EXISTS(&xlv_tab->subvolume[s])) {
			/*
			 * Check that subvol_size field is big enough
			 * to hold the size of the subvolume. If there
			 * is an overflow set subvol_size to zero.
			 *
			 * This can be a problem when the volume is created
			 * on a 64 bits system and the volume is moved to
			 * a 32 bits system.
			 */
			sv_size = xlv_tab_subvol_size (&xlv_tab->subvolume[s]);
			xlv_tab->subvolume[s].subvol_size = sv_size;
#if 0
			/* XXXjleong Need to do subvolume size check here? */
			if (sv_size != xlv_tab->subvolume[s].subvol_size)
				xlv_tab->subvolume[s].subvol_size = 0;
#endif
		}
	}

	*status = 0;

} /* end of xlv_vh_to_xlv_tab() */


/*
 * Return true if the volume is local to this node
 */
boolean_t
xlv_local_volume(xlv_tab_vol_entry_t *vol_p)
{
	char *me, *owner;

	me = xlv_getnodename();
	owner = vol_p->nodename;

	if (owner == NULL ||
	    !strncmp(owner, me, XLV_NODENAME_LEN) ||
	    !strncmp(owner, _STANDALONE_NODENAME, XLV_NODENAME_LEN))
		return B_TRUE;

	return B_FALSE;

} /* end of xlv_local_volume() */

/*
 * Create the standalone plex and volume element oref's from the
 * disk labels in vh_list.  
 * This routine will skip over vh_entries that belong to
 * volumes [and subvolumes].
 */
void
xlv_vh_to_xlv_pieces (
        xlv_vh_entry_t  *vh_list,
        xlv_oref_t      **oref_list_p,
        int             *status,
	int		filter)		/* remove incomplete pieces */
{
	xlv_vh_entry_t		  *vh;
	xlv_oref_t		  *oref;
	xlv_oref_t		  *oref_pieces;	/* list of standalone pieces */
	xlv_vh_disk_label_t	  *label;
	xlv_vh_disk_label_entry_t *lep;		/* label entry pointer */
	unsigned		  i;
	unsigned		  st;
	char			  *owner;

	oref_pieces = NULL;

	for (vh = vh_list; vh; vh = vh->next) {

		label = XLV__LAB1_PRIMARY (vh);
		owner = label->header.nodename;

		for (i=0; i < XLV_MAX_DISK_PARTS; i++) {
			lep = &label->label_entry[i];

			if ( uuid_is_nil (&(lep->vol.volume_uuid), &st) &&
			     !uuid_is_nil (&(lep->disk_part.uuid), &st) ) {
				/*
				 * Ignore entries that don't match
				 * the volume header info.
				 */

				if (xlv__check_label_entry (lep, vh) != 0) {
					continue;
				}
				/*
				 * This vh entry does not belong to
				 * a volume. Add entry to the xlv
				 * pieces list.
				 */
				xlv__vh_lab_to_oref (
					owner, lep, &oref_pieces, status);
			}
		}
	}


	if (oref_pieces) {
		/*
		 * Make sure all standalone object have all their pieces.
		 * Depending on the <filter> parameter, object missing
		 * pieces can be removed from the list.
		 */
		 xlv_check_oref_pieces(&oref_pieces, filter);
	}

	if (oref_pieces) {
		/*
		 * Add the orefs for the standalone pieces to
		 * the beginning of the list.
		 */
		oref = oref_pieces;
		while (oref->next)
			oref = oref->next;
		oref->next = *oref_list_p;
		*oref_list_p = oref_pieces;
	}

} /* end of xlv_vh_to_xlv_pieces() */



/*
 * Create the volume information.
 *
 * Information for complete volumes (and subvolumes) are returned in
 * tab_vol and xlv_tab.
 */
/* ARGSUSED */
static int
xlv__vh_to_tab_vol (
	xlv_vh_disk_label_entry_t	*label,
	xlv_tab_vol_t 			*tab_vol,
        xlv_tab_t			*xlv_tab,
	xlv_vh_entry_t			**orphaned_vh_list,
	char				*nodename,	/* NULL if local */
	int				mk_vol_device,
	int				*status)
{
	xlv_tab_subvol_t	*sv;
#ifdef DEBUG
	unsigned		st;
#endif

	/*
	 * This routine assumes that the label is part of a 
	 * complete volume.
	 */
	ASSERT (! uuid_is_nil (&label->vol.volume_uuid, &st));

	/*
	 * Put only good disk part in the tables.
	 */
	if (label->disk_part.state != XLV_VH_DISK_PART_STATE_GOOD)
		return (0);		/* XXX Okay to continue */ 

	/*
	 * Allocate the portion of xlv_tab and xlv_tab_vol that is
	 * required to copy off the information from this disk part.
	 * Because we may have seen other disk parts for this volume,
	 * we may not need to do the allocation this time.
	 */
	sv = xlv__vh_alloc_mem (label, tab_vol, xlv_tab, nodename,
				mk_vol_device, status);
	if (NULL == sv) {
		if (0 == *status) {
			/*
			 * Failed for some reason but the status
			 * was not set.
			 */
			ASSERT(0);
			*status = ENOMEM;	/* Could not create subvolume */
		}
		return (1);
	}

	/*
	 * Copy the fields
	 */
	xlv__vh_to_tab_vol_copy_fields (label, sv);

	return (0);

} /* end of xlv__vh_to_tab_vol() */



/*
 * Allocate the dynamic memory required to store the volume,
 * all the subvolumes that belong to the volume, the plex
 * and volume element to which the current disk part belongs.
 * Note that we do not allocate memory for the other plexes
 * as we won't know their sizes until we actually encounter
 * disk parts that belong to them.
 *
 * This routine returns a pointer to the xlv_tab_vol entry
 * that has been allocated for this volume.
 *
 * Return NULL and (*status == E2BIG) when the passed in table
 * is not big enough for the volume and subvolume.
 */
static xlv_tab_subvol_t *
xlv__vh_alloc_mem (
        xlv_vh_disk_label_entry_t	*label,
        xlv_tab_vol_t			*tab_vol,
        xlv_tab_t			*xlv_tab,
	char				*nodename,	/* NULL if local */ 
	int				mk_vol_device,
	int				*status)
{
	xlv_tab_vol_entry_t	*v;
	xlv_tab_subvol_t	*sv;
	xlv_tab_plex_t		*p;
	int			i, sv_index;
	size_t			plex_size;
	uint_t			st;

	/* See if this volume is already in the xlv_tab_vol */
	v = NULL;
	for (i=0; i < tab_vol->num_vols; i++) {
		if (uuid_equal (&label->vol.volume_uuid,
			        &tab_vol->vol[i].uuid, &st)) {
		    v = &tab_vol->vol[i];
		    break;
		}
	}

	if (v == NULL) {
	    	/*
		 * Allocate a volume entry and set up cross links to the
		 * subvolumes. Generate XLV device number for each of the
		 * subvolumes.
		 *
		 * NOTE that subvolumes are indexed within xlv_tab by
		 * their minor device numbers.  Thus xlv_tab is sparse.
		 * xlv_tab_vol is dense.
		 */
		dev_t	dev;

		if (tab_vol->num_vols >= tab_vol->max_vols) {
			/*
			 * There are more volumes than the table can hold.
			 */
			/* ASSERT(0); */
			*status = E2BIG;
			return (NULL);
		}
	    	v = &(tab_vol->vol[tab_vol->num_vols]);
		v->version = XLV_TAB_VOL_ENTRY_VERS;
		v->nodename = strdup(nodename ? nodename : xlv_getnodename());
	    	tab_vol->num_vols++;

	    	if (! uuid_is_nil (&label->vol.log_subvol_uuid, &st)) {
			dev = xlv_uuid_to_dev(&label->vol.log_subvol_uuid);
		    	sv_index = minor(dev);
			if (! (sv_index < xlv_tab->max_subvols)) {
				/* ASSERT(0); */
				*status = E2BIG;
				return (NULL);
			}
		    	sv = &(xlv_tab->subvolume[sv_index]);
			sv->version = XLV_TAB_SUBVOL_VERS;
		    	xlv_tab->num_subvols++;
		    	v->log_subvol = sv;
		    	sv->vol_p = v;
	    	}
	    	if (! uuid_is_nil (&label->vol.data_subvol_uuid, &st)) {
		    	dev = xlv_uuid_to_dev(&label->vol.data_subvol_uuid);
		    	sv_index = minor(dev);
			if (! (sv_index < xlv_tab->max_subvols)) {
				/* ASSERT(0); */
				*status = E2BIG;
				return (NULL);
			}
		    	sv = &(xlv_tab->subvolume[sv_index]);
			sv->version = XLV_TAB_SUBVOL_VERS;
	        	xlv_tab->num_subvols++;
	        	v->data_subvol = sv;
	        	sv->vol_p = v;

			if (mk_vol_device) {
				if (0 >
				    xlv_create_node_pair(label->vol.volume_name,
							 nodename,
							 dev, xlv_tab)) {
					/*EMPTY*/
					/*
					 * XXX Cannot create the device nodes
					 */
				}
			}
	    	}
	    	if (! uuid_is_nil (&label->vol.rt_subvol_uuid, &st)) {
		    	dev = xlv_uuid_to_dev(&label->vol.rt_subvol_uuid);
		    	sv_index = minor(dev);
			if (! (sv_index < xlv_tab->max_subvols)) {
				/* ASSERT(0); */
				*status = E2BIG;
				return (NULL);
			}
		    	sv = &(xlv_tab->subvolume[sv_index]);
			sv->version = XLV_TAB_SUBVOL_VERS;
	        	xlv_tab->num_subvols++;
	        	v->rt_subvol = sv;
	        	sv->vol_p = v;
		}
	}

	/*
	 * Now see if we need to allocate the memory for the plex,
	 * vol elment, and disk part.
	 */
	if (label->vol.subvolume_type == XLV_SUBVOL_TYPE_LOG)
		sv = v->log_subvol;
	else if (label->vol.subvolume_type == XLV_SUBVOL_TYPE_DATA)
		sv = v->data_subvol;
	else
		sv = v->rt_subvol;

	if (sv == NULL) {
		ASSERT(0);
		fprintf(stderr, "");
		/*
		 * Bad subvolume type! vol.subvolume_type don't match
		 * the subvolume pointer. Someone overwrote the type
		 * field.
		 */
		*status = ENXIO;
		return(0);
	}

/* REM DEBUG, the malloc below is apparently not enough.  Someone is
   trashing the plex memory. */

	if (sv->plex[label->plex.seq_num] == NULL) {
		plex_size = sizeof(xlv_tab_plex_t) +
		    ( (label->plex.num_vol_elmnts-1) *
		      sizeof(xlv_tab_vol_elmnt_t) );
	    	p = (xlv_tab_plex_t *) malloc (plex_size);
		bzero (p, (int)plex_size);
	    	sv->plex[label->plex.seq_num] = p;
		p->max_vol_elmnts = label->plex.num_vol_elmnts;
		p->num_vol_elmnts = 0;
	}

	return (sv);
} /* end of xlv__vh_alloc_mem() */


static char *sv_type_text[] = { "unknown0", "log", "data", "rt" };

/*
 * Copy geometry information from the disk label to the in-core
 * xlv_tab and xlv_tab_vol structures.  This routine assumes that
 * the required memory for the in-core tables have already been
 * allocated.
 */
static void
xlv__vh_to_tab_vol_copy_fields (
	xlv_vh_disk_label_entry_t	*lab,	/* vol record in label */
	xlv_tab_subvol_t		*sv)
{
	xlv_tab_vol_entry_t *v;
	xlv_tab_plex_t      *p;
	xlv_tab_vol_elmnt_t *ve;
	xlv_tab_disk_part_t *dp;

	uint_t		st;
	volatile int	degraded_ve = 0;	/* for debugging */

	extern void slogit(int priority, char *fmt, ...);

	v = sv->vol_p;
	p = sv->plex[lab->plex.seq_num];
	ve = &(p->vol_elmnts[lab->vol_elmnt.seq_num_in_plex]);
	dp = &(ve->disk_parts[lab->disk_part.seq_in_grp]);

	if (ve->veu_timestamp != 0) {
		char *sv_type_str;
		char lab_pn[100];

		sv_type_str = (lab->vol.subvolume_type > XLV_SUBVOL_TYPE_RT) ?
			"unknown" : sv_type_text[lab->vol.subvolume_type];
		sprintf(lab_pn, "%s.%s.%d.%d.%d",
			lab->vol.volume_name, sv_type_str, lab->plex.seq_num,
			lab->vol_elmnt.seq_num_in_plex,
			lab->disk_part.seq_in_grp);

		/*
		 * Label entry should match the existing volume element.
		 */
		if (0 != uuid_compare(&ve->uuid, &lab->vol_elmnt.uuid, &st)) {
			slogit(LOG_INFO, "Mismatched label entry: %s", lab_pn);
			return;
		}
		/*
		 * This is a volume element which we have seen.
		 * Make sure that the ve state is consistent by
		 * comparing timestamps. If they are different then
		 * there may have been configuration changes.
		 */
		if (ve->veu_timestamp == lab->vol.subvol_timestamp) {
		 	/*
			 * Go and fill in the disk information.
			 */
			degraded_ve = 0;
			goto cont_disk;

		} else if (ve->veu_timestamp > lab->vol.subvol_timestamp) {
			/*
			 * The existing ve entry is the lastest so just
			 * fill in the disk part information.
			 *
			 * Beware of configuration inconsistencies.
			 */
			slogit(LOG_INFO,
				"Label entry %s has older timestamp", lab_pn);
			degraded_ve = 1;
			goto cont_disk;

		} else /* ve->veu_timestamp < lab->vol.subvol_timestamp */{
			/*
			 * The label entry has the later timestamp
			 * so it must have more recent configuration
			 * information.
			 */
			slogit(LOG_INFO,
				"Label entry %s has newer timestamp", lab_pn);
			degraded_ve = 1;
			if (lab->plex.num_vol_elmnts > p->max_vol_elmnts) {
				/* 
				 * More space required for configuration
				 */
				int		new_size;
				int		old_size;
				xlv_tab_plex_t  *newp;

				slogit(LOG_INFO, "Growing in-core plex");
				new_size = sizeof(xlv_tab_plex_t) +
					((lab->plex.num_vol_elmnts-1) *
						sizeof(xlv_tab_vol_elmnt_t));
				old_size = sizeof(xlv_tab_plex_t) +
					((p->max_vol_elmnts-1) *
						sizeof(xlv_tab_vol_elmnt_t));
				newp = (xlv_tab_plex_t *)malloc(new_size);
				bzero(newp, (int)new_size);
				bcopy(p, newp, (int)old_size);
				newp->max_vol_elmnts =
					lab->plex.num_vol_elmnts;
				free(p);

				/*
				 * Reset pointers
				 */
				sv->plex[lab->plex.seq_num] = newp;
				p = newp;
				ve = &(p->vol_elmnts[lab->
					vol_elmnt.seq_num_in_plex]);
				dp = &(ve->disk_parts[lab->
					disk_part.seq_in_grp]);
			}
		}
	}

	/*
	 * Copy fields from label to table.
	 */
	v->flags = lab->vol.flag;
	COPY_UUID (lab->vol.volume_uuid, v->uuid);
	if (v->log_subvol != NULL) {
		COPY_UUID (lab->vol.log_subvol_uuid, v->log_subvol->uuid);
		v->log_subvol->dev = xlv_uuid_to_dev (&v->log_subvol->uuid);
	}
	if (v->data_subvol != NULL) {
	    	COPY_UUID (lab->vol.data_subvol_uuid, v->data_subvol->uuid);
		v->data_subvol->dev = xlv_uuid_to_dev (&v->data_subvol->uuid);
	}
	if (v->rt_subvol != NULL) {
	    	COPY_UUID (lab->vol.rt_subvol_uuid, v->rt_subvol->uuid);
		v->rt_subvol->dev = xlv_uuid_to_dev (&v->rt_subvol->uuid);
	}
	COPY_NAME (lab->vol.volume_name, v->name);
	v->sector_size = lab->vol.sector_size;

	sv->flags = XLV_SUBVOL_FLAGS_SAVE(lab->vol.subvol_flags);
	sv->subvol_type = lab->vol.subvolume_type;
	sv->subvol_depth = lab->vol.subvol_depth;
	sv->num_plexes = lab->plex.num_plexes;

	COPY_UUID (lab->plex.uuid, p->uuid);
	COPY_NAME (lab->plex.name, p->name);

	/*
	 * Better to leave this 0 and have a final pass that checks to
	 * make sure that we actually have all the elements.
	 * Or, just initialize it when we malloc the data.
	 */
	p->num_vol_elmnts = lab->plex.num_vol_elmnts;
	p->flags = lab->plex.flags;

	COPY_UUID (lab->vol_elmnt.uuid, ve->uuid);

	/*
	 * Since this volume element is part of a volume,
	 * the ve name field should be empty so don't bother
	 * copying the ve name from the label.
	 *
	 *	COPY_NAME (lab->vol_elmnt.name, ve->veu_name);
	 *
	 * Do keep the timestamp so we can do the ve state transition
	 * (e.q. clean -> active) for plex support.
	 */
	ve->veu_uname[0] = 0;
	ve->veu_timestamp = (time_t)lab->vol.subvol_timestamp;

	ve->type = lab->vol_elmnt.type;
	ve->state = lab->vol_elmnt.state;
	ve->grp_size = lab->vol_elmnt.grp_size;
	ve->stripe_unit_size = lab->vol_elmnt.stripe_unit_size;
	ve->start_block_no = lab->vol_elmnt.start_block_no;
	ve->end_block_no = lab->vol_elmnt.end_block_no;

cont_disk:
	COPY_UUID (lab->disk_part.uuid, dp->uuid);
	dp->part_size = lab->disk_part.size;
	/* XXX KK */
	xlv_fill_dp(dp, lab->disk_part.dev);

#if 0
	if (degraded_ve) {
		/*
		 * XXX Anything special for this entry?
		 */
	}
#endif

} /* end of xlv__vh_to_tab_vol_copy_fields() */


/*
 * Copy XLV object information from a disk label into an oref.
 * This routine should only be called to fill in XLV objects
 * that do not belong to volumes. 
 */
static void
xlv__vh_lab_to_oref (
	char				*owner,
	xlv_vh_disk_label_entry_t	*lab,
	xlv_oref_t			**oref_pieces_p,
	int				*status)
{
	xlv_oref_t	    *oref;
	xlv_oref_t	    *oref_pieces;
	xlv_tab_plex_t      *p = NULL;
	xlv_tab_vol_elmnt_t *ve = NULL;
	xlv_tab_disk_part_t *dp = NULL;
	size_t		    plex_size;
	unsigned	    st;
	int		    oref_created = 0;

	ASSERT (uuid_is_nil (&lab->vol.volume_uuid, &st));

	oref_pieces = *oref_pieces_p;

	if (! uuid_is_nil (&lab->plex.uuid, &st)) {
		/*
		 * See if this plex is already on the list.
		 */
		for (oref = oref_pieces; oref; oref = oref->next) {
			if ( !(p = XLV_OREF_PLEX(oref)) )
				continue;
			if ( uuid_equal(&p->uuid, &lab->plex.uuid, &st) )
				break;
		}

		if (!oref) {
			/*
			 * Create the oref for new standalone plex.
			 */ 
			oref = (xlv_oref_t *)malloc(sizeof(xlv_oref_t));
			XLV_OREF_INIT (oref);
			oref_created++;

			plex_size = sizeof(xlv_tab_plex_t) +
	                    ( (lab->plex.num_vol_elmnts-1) *
	                      sizeof(xlv_tab_vol_elmnt_t) );
	                p = (xlv_tab_plex_t *) malloc (plex_size);
	                bzero (p, (int)plex_size);
	                p->max_vol_elmnts = lab->plex.num_vol_elmnts;
	                p->num_vol_elmnts = lab->plex.num_vol_elmnts;
	
			COPY_UUID (lab->plex.uuid, p->uuid);
			COPY_NAME (lab->plex.name, p->name);
			p->flags = lab->plex.flags;
	
			XLV_OREF_SET_PLEX (oref, p);
		}
	}

	/*
	 * The smallest object we can create is a volume element.
	 * There must always be one.
	 */
	if (p) {
		ve = &(p->vol_elmnts[lab->vol_elmnt.seq_num_in_plex]);
	}
	else {
		/*
		 * See if this volume element is already on the list.
		 */
		for (oref = oref_pieces; oref; oref = oref->next) {
			if ( !(ve = XLV_OREF_VOL_ELMNT(oref)) )
				continue;
			if ( uuid_equal(&ve->uuid, &lab->vol_elmnt.uuid, &st) )
				break;
		}

		if (!oref) {
			/*
			 * Create the oref for new standalone volume element.
			 * Note: But standalone ve may not be supported.
			 */ 
			oref = (xlv_oref_t *)malloc(sizeof(xlv_oref_t));
			XLV_OREF_INIT (oref);
			oref_created++;
			ve = (xlv_tab_vol_elmnt_t *) malloc (
				sizeof (xlv_tab_vol_elmnt_t));
			bzero (ve, (int)sizeof(xlv_tab_vol_elmnt_t));
		}
	}

	if (! uuid_is_nil (&lab->vol_elmnt.uuid, &st)) {

		COPY_UUID (lab->vol_elmnt.uuid, ve->uuid);
		COPY_NAME (lab->vol_elmnt.name, ve->veu_name);
		ve->type = lab->vol_elmnt.type;
		ve->state = lab->vol_elmnt.state;
		ve->grp_size = lab->vol_elmnt.grp_size;
		ve->stripe_unit_size = lab->vol_elmnt.stripe_unit_size;
		ve->start_block_no = lab->vol_elmnt.start_block_no;
		ve->end_block_no = lab->vol_elmnt.end_block_no;

		if (! XLV_OREF_PLEX (oref)) {
			XLV_OREF_SET_VOL_ELMNT (oref, ve);
		}
		else {
			XLV_OREF_VOL_ELMNT (oref) = ve;
		}
	}

	ASSERT (! uuid_is_nil (&lab->disk_part.uuid, &st));

	dp = &(ve->disk_parts[lab->disk_part.seq_in_grp]);

	COPY_UUID (lab->disk_part.uuid, dp->uuid);
	dp->part_size = lab->disk_part.size;
	/* XXX KK */
	xlv_fill_dp(dp, lab->disk_part.dev);

	XLV_OREF_DISK_PART (oref) = dp;

	/*
	 * Add the new standaline component to the oref pieces list.
	 */
	if (oref_created) {
		/* remember the owner of this standalone component */
		strncpy(oref->nodename, owner, XLV_NODENAME_LEN);
		oref->next = *oref_pieces_p;
		*oref_pieces_p = oref;
	}
	*status = 0;
} /* end of xlv__vh_lab_to_oref() */



/*
 * Error reasons.
 */
static char	BadLog[]="Log ve should have partition type xfslog";
static char	BadData[]="Data ve should have partition type xlv";
static char	BadRt[]="Rt ve should have partition type xlv";
static char	BadStandalone[]="Standalone object should have partition type xlv or xfslog";
static char	Baduuid[]="No uuid bits";
static char	Badsize[]="Size mismatch";
#if 0
static char	BadVh[]="Unable to write to volume header";
#endif
static char	Badnopart[]="The partition does not exist";
/*
 * Error message templates..
 */
#if 0
static char	warn_template2[]="Warning: Partition %s%s. %s.\nUpdating volume header.%s";
#endif
static char	err_template[]="Rejecting partition %s%s. %s.%s";
#if 0
static char	Force[] = "\nUse \"ve -force\" to override or invoke with \"-f\" flag\n";
#endif

/*
 * Determine whether or not the label entry accurately reflects the
 * current disk configuration. The disk could have changed out from
 * under XLV. 
 *
 * Returns -1 if not good, 0 if good.
 */
static int
xlv__check_label_entry (
	xlv_vh_disk_label_entry_t	*lep,
	xlv_vh_entry_t			*vh_entry)
{
	int	partition;
	int	partition_type;
	int	subvol_type;
	char	*Bad;
	uint	st;

	xlv_vh_disk_label_entry_t *head;
	
	/*
	 * Do some address arthmetic to get the label entry number
	 * which corresponds to the partition number.
	 *
	 * Note: Could have passed in the label entry number!
	 */
	head = &(XLV__LAB1_PRIMARY(vh_entry))->label_entry[0];
	partition = lep - head;

	/*
	 * See if we have a valid disk part.
	 */
	if (uuid_is_nil (&(lep->disk_part.uuid),&st)) {
		Bad = Baduuid;
		goto bad;
	}

	/*
	 * Check for the existence of the partition and that the
	 * partition has not changed size.
	 */
	if (vh_entry->vh.vh_pt[partition].pt_nblks == 0) {
		Bad = Badnopart;
		goto bad;
	}
	if (lep->disk_part.size != vh_entry->vh.vh_pt[partition].pt_nblks) {
		Bad = Badsize;
		goto bad;
	}

	/*
	 * Decide if standalone or not
	 */
	if (uuid_is_nil (&(lep->vol.volume_uuid),&st)) {
		subvol_type = XLV_SUBVOL_TYPE_UNKNOWN;
	} else {
		subvol_type = lep->vol.subvolume_type;
	}

	/*
	 * Match partition type against subvolume type
	 */
	partition_type = vh_entry->vh.vh_pt[partition].pt_type;
	if (xlv_check_partition_type(subvol_type,partition_type,NO) == -1) {
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
		goto bad;
	}

	return 0;

bad:
	xlv_print_type_check_msg (
		err_template, vh_entry->vh_pathname, Bad,
		0 /* not used */, partition, NULL);
	return (-1);

} /* end of xlv__check_label_entry() */
