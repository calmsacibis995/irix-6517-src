
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
#ident "$Revision: 1.10 $"

#include <stdio.h>
#include <stdlib.h>
#include <bstring.h>
#include <pfmt.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/xlv_attr.h>
#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <xlv_error.h>
#include <xlv_oref.h>
#include <xlv_utils.h>
#include <xlv_lab.h>
#include <xlv_lower_admin.h>
#include "xlv_mgr.h"

extern xlv_vh_entry_t   *xlv_vh_list;

static int _make_detach_p_oref (
	xlv_oref_t *orig_oref,		/* */
	xlv_oref_t *p_oref,		/* OUT: detached plex */
	char *name,			/* new standalone plex */
	int  svtype,			/* type of the owning subvolume*/
	int  pnum,	 		/* */
	int  force);	 		/* do it even if plex is missing */

static int _xlv_subvol_verify (xlv_oref_t *v_oref, int svtype);

static int _check_plex_mirrored (
	xlv_tab_subvol_t *sv,		/* subvolume */
	int		 pnum);		/* index of plex being checked */

static int _append_plex_to_oref (
	xlv_oref_t *v_oref,		/* IN/OUT: Updated data structures */
	xlv_oref_t *p_oref,		/* IN: plex to be added */
	int	   svtype);		/* IN: subvolume type */

typedef struct {
	long long	start_blkno;
	long long	end_blkno;
	time_t		timestamp;
} my_ve_entry_t;
#define	ve_entry_t	my_ve_entry_t	/* HACK!!! */

#define	PLEX_OVERLAP	0	/* check the plex conforms w/ existing ve's */
#define	PLEX_MIRRORED	1	/* check the plex is mirrored */

static int _make_ve_array (
	xlv_tab_subvol_t *sv,
	int		 pnum,		/* plex number to check */
	size_t		 max_len,	/* length of array */
	ve_entry_t	 *ve_array,	/* OUT: table to fill in */
	int		 mode);		/* MIRRORED or OVERLAP */

/*
 ******************************************************************
 *
 * Destroy the plex. Remove the plex from the disk labels and
 * remove its oref from the hash table. The passed in oref structure
 * which references the plex is not freed.
 *
 * Return XLV_MGR_STATUS_OKAY on success.
 *
 ******************************************************************
 */
int
xlv_delete_plex (
	xlv_oref_t	*oref,
	int		freeit,		/* flag to free the plex structure */
	int		inplace)	/* flag to leave the label on disk */
{
	int rval = XLV_MGR_STATUS_OKAY;

	/*
	 * Verify that this is a plex.
	 */
	if (oref->obj_type != XLV_OBJ_TYPE_PLEX) {
#ifdef DEBUG
fprintf (stderr, "DBG: xlv_delete_plex() called without a plex oref.\n");
#endif
		fprintf(stderr, "Internal error: object is not a plex.\n");
		return (INVALID_OPER_ENTITY);
	}

	/*
	 * Remove the volume information from the disk labels if we're not
	 * leaving it in place.
	 */
	if (!inplace &&
	    0 != xlv_lower_rm_plex (xlv_cursor_p, &xlv_vh_list, NULL, oref)) {
#ifdef DEBUG
fprintf (stderr, "DBG: xlv_delete_plex(): failed in xlv_lower_rm_plex\n");
#endif
		rval = xlv_lower_errno;
		print_error(rval);
		return(rval);
	}

	/*
	 * Remove the entry from the hash table.
	 */
	if (0 != remove_object_from_table(oref, 1)) {
		return (XLV_MGR_STATUS_FAIL);
	}

	if (freeit) {
		if (XLV_OREF_PLEX (oref) != NULL)
			free (XLV_OREF_PLEX (oref));
		XLV_OREF_PLEX (oref) = NULL;
	}

	return(XLV_MGR_STATUS_OKAY);

} /* end of xlv_delete_plex() */


/*
 * Go through the plexes and insert a ve_array entry for
 * every volume address range.
 *
 * Return the number of entries filled in the array.
 */
/*ARGSUSED2*/
static int
_make_ve_array (
	xlv_tab_subvol_t *sv,
	int		 pnum,			/* plex number to check */
	size_t		 max_len,		/* length of array */
	ve_entry_t	 *ve_array,		/* OUT: table to fill in */
	int		 mode)			/* MIRRORED or OVERLAP */
{
	xlv_tab_plex_t	    *plex;
	xlv_tab_vol_elmnt_t *ve;

	unsigned	ve_array_len, p;
	unsigned	ve_index, ve_array_index;
	long long	ve_start_blkno, ve_array_start_blkno;
	int		i;

	/*
	 * Go through all the plexes.  Insert an entry for every
	 * volume address range that's not already in ve_array.
	 */
	ve_array_len = 0;
        for (p = 0; p < sv->num_plexes; p++) {

		if ((p == pnum) || 			/* plex being checked */
		    (NULL == (plex = sv->plex[p])))	/* plex is missing */
			continue;

		/*
		 * for each vol elmnt in this plex...
		 */
                for (ve_index=0; ve_index < plex->num_vol_elmnts; ve_index++) {
			/*
			 * Instead of setting ve_array_index = 0,
			 * ve_array_index should mark the spot where
			 * the previous check of this plex ended.
			 * ... Wait cannot do this cuz the timestamp 
			 * may need to be updated!
			 */
			ve_array_index = 0;

			ve = &plex->vol_elmnts[ve_index];

			if (mode == PLEX_MIRRORED &&
			    (ve->state != XLV_VE_STATE_ACTIVE
				&& ve->state != XLV_VE_STATE_CLEAN)) {
				/*
				 * Skip this volume element if it's not active
				 */
#ifdef DEBUG
printf("DBG: plex[%d] ve[%d] is not active nor clean\n", p, ve_index);
#endif
				continue;
			}

			/*
			 * Handle the special-case where this is the very
			 * first valid vol element that we see.
			 */
			if (ve_array_len == 0) {
				ve_array[0].start_blkno = ve->start_block_no;
				ve_array[0].end_blkno = ve->end_block_no;
				ve_array[0].timestamp = ve->veu_timestamp;
				ve_array_len++; 
			}

			ve_start_blkno = ve->start_block_no;

			/*
                         * Find the ve_array entry within which
                         * this volume element falls.
                         */
			ve_array_start_blkno =
				ve_array[ve_array_index].start_blkno;

			while (ve_start_blkno > ve_array_start_blkno) {
				ve_array_index++;
				ve_array_start_blkno = 
					ve_array[ve_array_index].start_blkno;

				/*
				 * If we've run past the end of ve_array,
				 * then APPEND this new block range to the
				 * end of ve_array.
				 */
				if (ve_array_index > ve_array_len) {
					ASSERT (ve_array_len != max_len);
					ve_array[ve_array_len].start_blkno =
						ve->start_block_no;
					ve_array[ve_array_len].end_blkno =
						ve->end_block_no;
					ve_array[ve_array_len].timestamp =
						ve->veu_timestamp;
					ve_array_len++;
					goto next_iteration;
				}
			}

			if (ve_start_blkno < ve_array_start_blkno) {

				/*
				 * The ve_array entry is beyond this volume
				 * element's block range. This means
				 * that this block range did not exist
				 * in ve_array.
				 * We need to INSERT the block range.
				 */
				for (i = ve_array_len;
				     i > ve_array_index; i--) {
					ve_array[i] = ve_array[i-1];
				}

				ASSERT (ve_array_len != max_len);
				ve_array[ve_array_index].start_blkno =
					ve_start_blkno;
				ve_array[ve_array_index].end_blkno =
					ve->end_block_no;
				ve_array[ve_array_index].timestamp =
					ve->veu_timestamp;
				ve_array_len++;

			} else {
				/*
				 * ve_start_blkno == ve_array_start_blkno
				 *
				 * An existing entry spans the same
				 * address space as this ve so CHECK
				 * for the latest timestamp and continue.
				 */
				if (ve_array[ve_array_index].
				    timestamp < ve->veu_timestamp) {
					ve_array[ve_array_index].timestamp =
						ve->veu_timestamp;
				}
				    
			}

			/*FALLTHRU*/
next_iteration:
			ve_array_index++;
		}	/* end for each vol elmnt in this plex... */
	}

	return (ve_array_len);

} /* end of _make_ve_array() */


/*
 * This routine was copied from xlv_admin/plex.c$xlv_chk_plex()
 *
 * Verify that the space covered by the plex identified by <pnum>
 * is covered by other plexes. If not then return E_NO_PCOVER.
 */
static int
_check_plex_mirrored (
	xlv_tab_subvol_t *sv,		/* subvolume */
	int		 pnum)		/* index of plex being checked */
{
	xlv_tab_plex_t	*plex;		/* this is being removed */
	ve_entry_t	*ve_array;
	unsigned	ve_array_len, i, j;
	size_t 		max_len;
	long long	start;
	int		state;
	time_t		timestamp;

	if (sv->num_plexes == 1) {
        	/*
		 * We only check if there is more than one plex.
		 */
		return (LAST_PLEX);
	}

	/*
	 * Malloc memory for array and initialize.
	 * The address spaces of each of these plexes
	 * can be disjoint so allocate the max possible
	 * depth.
	 */
	max_len = (sv->num_plexes - 1) * XLV_MAX_VE_PER_PLEX;
	ve_array = (ve_entry_t *) malloc (max_len * sizeof(ve_entry_t));
	bzero (ve_array, (max_len * sizeof(ve_entry_t)));

	ve_array_len =
		_make_ve_array (sv, pnum, max_len, ve_array, PLEX_MIRRORED);
	ASSERT (ve_array_len <= max_len);

	/*
	 * Go through and check that the given plex is mirrored
	 * by the other plexes.
	 */

#ifdef DEBUG
	printf("DBG: _check_plex_mirrored() ve_array_len=%d\n", ve_array_len);
	for (i=0; i<ve_array_len; i++)
		printf("DBG: i=%d start=%lld\n", i, ve_array[i].start_blkno);
	printf("DBG: checked plex[%d] num_vol_elmnts=%d\n",
		pnum, sv->plex[pnum]->num_vol_elmnts);

#endif
	plex = sv->plex[pnum];	/* plex being checked for mirror'ed ve's */

	/*
	 *	If the number of ve's in the plex being checked
	 *	is larger than ve_array_len then the address
	 *	space is definitely not covered
	 */
	if (plex->num_vol_elmnts > ve_array_len) {
		free (ve_array);
		return (E_NO_PCOVER);
	}

	/*
	 *	Check that each ve in the plex is mirrored.
	 */
	for (i=0; i < plex->num_vol_elmnts; i++) {
		state = plex->vol_elmnts[i].state;
		start = plex->vol_elmnts[i].start_block_no;
		timestamp = plex->vol_elmnts[i].veu_timestamp;
		for (j = 0; j < ve_array_len; j++) {
			if (ve_array[j].start_blkno == start) {
				/*
				 * Address space is mirrored. If the ve is
				 * active/clean, the the mirror ve must
				 * have the same or new timestamp.
				 */
				if ((state == XLV_VE_STATE_ACTIVE
					|| state == XLV_VE_STATE_CLEAN)
				    && ve_array[j].timestamp < timestamp) {
					/*
					 * See bug 324915. The mirror is
					 * not up-to-date so we cannot say
					 * this plex is fully mirrored.
					 */
					j = ve_array_len;   /* say no match */
				}
				break;
			}
		}
		if (j == ve_array_len) {
			/*
			 * No match found so this ve is "unique".
			 */
			free (ve_array);
			return (E_NO_PCOVER);
		}
	}

	/*
	 *	Free memory for array 
	 */
	free (ve_array);

	return(XLV_MGR_STATUS_OKAY);

} /* end of _check_plex_mirrored() */


/*
 * This routine was copied from xlv_admin/plex.c$xlv_subvol_verify()
 */
static int
_xlv_subvol_verify (xlv_oref_t *v_oref, int svtype)
{
	xlv_tab_vol_entry_t     *vp;
	int			retval = XLV_MGR_STATUS_OKAY;

	vp = XLV_OREF_VOL(v_oref);
	if (vp  == NULL) {
		return (RM_SUBVOL_NOVOL);
	} 

	switch (svtype) {
	case XLV_SUBVOL_TYPE_LOG:
		if (vp->log_subvol == NULL) {
			retval = SUBVOL_NO_EXIST;
		} else if (vp->rt_subvol == NULL && vp->data_subvol == NULL) {
			retval = LAST_SUBVOL;
		}
		break;

	case XLV_SUBVOL_TYPE_DATA:
		if (vp->data_subvol == NULL) {
			retval = SUBVOL_NO_EXIST;
		} else if (vp->rt_subvol == NULL && vp->log_subvol == NULL) {
			retval = LAST_SUBVOL;
		}
		break;

	case XLV_SUBVOL_TYPE_RT:
		if (vp->rt_subvol == NULL) {
			retval = SUBVOL_NO_EXIST;
		} else if (vp->data_subvol == NULL && vp->log_subvol == NULL) {
			retval = LAST_SUBVOL;
		}
		break;

	default:
		ASSERT(0);
		retval = INVALID_SUBVOL;
	}

	return (retval);

} /* end of _xlv_subvol_verify() */


/*
 * This routine was copied from xlv_admin/plex.c$xlv_dettach_plex()
 *
 * This routine creates a oref to reference the plex that will be detached.
 * The plex and subvolume location are checked.
 * After all checks are successful, the plex itself is detached. 
 *
 * On success, <p_oref> is set and XLV_MGR_STATUS_OKAY is returned.
 * If the detached plex is missing, <p_oref> has a NULL plex value. 
 */
static int
_make_detach_p_oref (
	xlv_oref_t *orig_oref,		/* object holding the plex */
	xlv_oref_t *p_oref,		/* OUT: detached plex */
	char *name,			/* new standalone plex */
	int  svtype,			/* type of the owning subvolume*/
	int  pnum,	 		/* plex number */
	int  force)	 		/* ignore missing target plex */
{
	xlv_tab_subvol_t	*orig_subvol;
	xlv_tab_plex_t		*orig_plex;
	xlv_oref_t		temp_oref;
	int			rval = XLV_MGR_STATUS_OKAY;
	int			i;

#ifdef DEBUG
printf("DBG: _make_detach_p_oref() svtype=%d plex=%d\n", svtype, pnum);
#endif

	/*
	 *	1. Basic verification 
	 */

	if (orig_oref->obj_type == XLV_OBJ_TYPE_PLEX) {
		/*
		 * cannot detach a plex from plex
		 */
#ifdef DEBUG
printf("DBG: _make_detach_p_oref() tried to detach plex from self\n");
#endif
		return(INVALID_OPER_ENTITY);
	}

	/* 
	 *	1a. find the plex to detach and verify it's existance.
	 */
	if (svtype == XLV_SUBVOL_TYPE_LOG) {
		orig_subvol = orig_oref->vol->log_subvol;
	} else if (svtype == XLV_SUBVOL_TYPE_DATA) {
		orig_subvol = orig_oref->vol->data_subvol;
	} else if (svtype == XLV_SUBVOL_TYPE_RT) {
		orig_subvol = orig_oref->vol->rt_subvol;
	} else {
		return (INVALID_SUBVOL);
	}
	if (orig_subvol == NULL) {
		return(SUBVOL_NO_EXIST);
	}
	orig_plex = orig_subvol->plex[pnum];

	if (orig_plex == NULL) {
		if (force) {
			printf("Warning: Detaching missing plex number %d\n",
				pnum);
			goto cont_rm_plex;	/* skip unnecessary checks */
		} else {
			return(PLEX_NO_EXIST);
		}
	}

	XLV_OREF_SUBVOL(orig_oref)= orig_subvol;

#ifdef DEBUG
	if (orig_oref->plex == orig_plex) {
		printf("DBG: _make_detach_p_oref() orig oref == orig_plex\n");
	}
#endif

	/*
	 * No matter what, cannot detach the only plex of a subvolume.
	 */
	if (orig_subvol->num_plexes == 1) {
		return(LAST_PLEX);
	}

	/*	
	 *	1c. Verify that the remaining plexes cover the
	 *		address space that is about to be removed.
	 */
	rval = _check_plex_mirrored(orig_subvol, pnum);
	if (rval != XLV_MGR_STATUS_OKAY) {
		if (rval == E_NO_PCOVER) {
			char yval[10];
			int  found;
			int  vol_isfree;

			vol_isfree = xlv_vol_isfree(orig_oref, &found);
			if (XLV_MGR_STATUS_OKAY != vol_isfree) {
				return (rval);
			}
			pfmt(stderr, MM_NOSTD, "::"
"The range of disk blocks covered by this plex is not covered by another\n"
"plex. If you remove/detach this plex then the data on this plex\n"
"will be destroyed.  You should cover this range of disk blocks\n"
"with another plex if you want to preserve the data on this plex.\n"
"You can create a new plex using xlv_make(1m) and then attach it using\n"
"xlv_mgr(1m).\n\n");

			get_val (yval, VAL_DESTROY_DATA);
			if (!strcasecmp("yes", yval)) {
				rval = XLV_MGR_STATUS_OKAY;
				goto cont_rm_plex;
			}
		}
		return(rval);
	}

	/*FALLTHRU*/
cont_rm_plex:

	/*
	 *	2. Initialize temp oref structure
	 */
	XLV_OREF_INIT (&temp_oref);	/* initialize the oref struct */

	/*
	 *	3. Set detached plex values
	 */
	strncpy(temp_oref.nodename, orig_oref->nodename, XLV_NODENAME_LEN);
	temp_oref.obj_type = XLV_OBJ_TYPE_PLEX;
	temp_oref.plex = orig_plex;
	if (orig_plex)
		strcpy(orig_plex->name, name);

	/*
	 *	4. Compact the plex array in the orginal subvolume.
	 *	Move plexes in original volume as required. That is,
	 *	if we are detaching the second plex then move the
	 *	third plex to the second plex and the fourth plex
	 *	to the third plex.
	 *
	 *	Decrement count of plexes in original subvolume.
	 */
	for (i = pnum; i < orig_subvol->num_plexes; i++) {
		if (i+1 < orig_subvol->num_plexes) {
			orig_subvol->plex[i] = orig_subvol->plex[i+1];
		} else {
			orig_subvol->plex[i] = NULL;
		}
	}
	orig_subvol->num_plexes--;

	/*
	 *	6. Move temp to the p_oref .
	 */
	*p_oref = temp_oref;

	return(rval);

} /* end of _make_detach_p_oref() */


/*
 * Detach a plex from the given volume object.
 * If a non-NULL <dst_name> is given, save the detached plex as <dst_name>.
 * It is the caller's responsibility to check that there is no other
 * plex object of that name.
 *
 * Return XLV_MGR_STATUS_OKAY on success.
 */
int
xlv_detach_plex (
	xlv_mgr_cursor_t *cursor,
	char		 *dst_name,	/* Name of new standalone plex */
	int		 delete,
	int		 force)
{
	int		retval = XLV_MGR_STATUS_OKAY;
	xlv_oref_t	p_oref;
	char		*name;

	name = (NULL == dst_name) ? "_DETACHED_PLEX_" : dst_name;
	
	/*
	 * When the plex being detached is missing, p_oref.plex is
	 * set to NULL. 
	 */
	retval = _make_detach_p_oref (
		cursor->oref, &p_oref, name,
		cursor->sv_type, cursor->plex_no, force);
	if (retval != XLV_MGR_STATUS_OKAY) {
		print_error(retval);
		return(retval);
	}

	if (p_oref.plex == NULL) {
		/*
		 * The plex is missing so all that is needed is to
		 * remove the path to the plex.
		 */
		xlv_attr_cursor_t path;
		xlv_tab_subvol_t  *svp = XLV_OREF_SUBVOL(cursor->oref);

		ASSERT(force);
		/* path.generation */
		path.vol = -1;			/* vol */
		path.subvol = minor(svp->dev);	/* subvol */
		path.plex = cursor->plex_no;	/* plex */
		path.ve = -1;			/* ve */

		retval = xlv_lower_rm_path (
			xlv_cursor_p, &xlv_vh_list, cursor->oref, NULL, &path);
		return (retval);
	}

	if (NULL == dst_name) {
		delete = 1;
		retval = xlv_lower_rm_plex (
			xlv_cursor_p, &xlv_vh_list, cursor->oref, &p_oref);
	} else {
		retval = xlv_lower_detach_plex (
			xlv_cursor_p, &xlv_vh_list, cursor->oref, &p_oref);
	}
	if (retval != 0) {
		delete = 1;
		retval = xlv_lower_errno;
		if (retval == XLV_LOWER_ESTALECURSOR) { /* XXX try again? */ }
#ifdef DEBUG
printf ("DBG: xlv_detach_plex() failed calling %s()\n",
	(NULL == dst_name) ? "xlv_lower_rm_plex" : "xlv_lower_detach_plex");
#endif
		/*
		 * Failed to detach the plex from the volume object.
		 * XXX Should re_attach the plex to the volume.
		 */
	}

	if (delete) {
		free (p_oref.plex);
	} else {
		/*
		 * The detached plex is now a standalone object
		 * so it must be added to the table.
		 */
		add_object_to_table(&p_oref);
	}

	return (retval);
} /* end of xlv_detach_plex() */


/*
 * This routine was copied from xlv_admin/plex.c$xlv_chk_add_plex()
 *
 * _check_plex_overlap() checks that the specified plex does not
 * have any volume elements that overlap with any volume elements
 * in the other plexes. Ensure is that corresponding volume elements
 * in the other plexes have the same start and end points.
 */
int
check_plex_overlap (
	xlv_tab_subvol_t *sv,
	int		 pnum)
{
	xlv_tab_plex_t	*plex;
	ve_entry_t	*ve_array;
	unsigned	ve_array_len, i, j;
	long long	start, end;
	int		rval = XLV_MGR_STATUS_OKAY;
	size_t		max_len;
	int		num_ve;

	if (sv->num_plexes < 2)
		return (XLV_MGR_STATUS_OKAY);		/* single plex */
	/*
	 * Malloc memory for array and initialize
	 */
	max_len = (sv->num_plexes - 1) * XLV_MAX_VE_PER_PLEX;
	ve_array = (ve_entry_t *) malloc (max_len * sizeof(ve_entry_t));
	bzero (ve_array, (max_len * sizeof(ve_entry_t)));

	ve_array_len =
		_make_ve_array (sv, pnum, max_len, ve_array, PLEX_OVERLAP);
	ASSERT (0 < ve_array_len && ve_array_len <= max_len);

#ifdef DEBUG
	printf("DBG: ve_array =%d \n",ve_array_len);
	for (j=0; j<ve_array_len; j++) {
		printf("DBG: a_start = %llu, a_end=%llu \n",
			ve_array[j].start_blkno,ve_array[j].end_blkno);
	}
#endif

	/*
	 * now we step through the ve's in the given plex
	 * and verify that they have the same start and end
	 * points as corresponding ve's in peer plexes.
	 */
	plex = sv->plex[pnum];
	num_ve = plex->num_vol_elmnts;

	for (i = 0; i < num_ve; i++) {
		/*
		 * XXX Check if we are adding a ve in the space
		 * before the first one in this plex.
		 */

		rval = INVALID_PLEX;
		start = plex->vol_elmnts[i].start_block_no;
		end = plex->vol_elmnts[i].end_block_no;

		/*
		 * XXXjleong An optimization is start the search
		 * from where "j" last stop the search for the
		 * previous ve. The plex just added must have its
		 * ve arranged in an increasing order.
		 */
		for (j = 0; j < ve_array_len; j++) {
			if (start == ve_array[j].start_blkno) {
				/*
				 * The start block number matches,
				 * and so must the end block number.
				 * Otherwise, this ve in the new
				 * plex is invalid.
				 */
				if (end == ve_array[j].end_blkno) {
					/*
					 * Found a match with an
					 * existing entry.
					 */
					rval = XLV_MGR_STATUS_OKAY;
				}
				break;	/* done with this ve */
			}

			/*
			 * Check if this is a new ve entry. It must
			 * not overlap with an existing entry.
			 */
			if ((start > ve_array[j].end_blkno) &&
			    ((j+1 == ve_array_len) ||
				(end < ve_array[j+1].start_blkno))) {
				/*
				 * Found new entry.
				 */
				rval = XLV_MGR_STATUS_OKAY;
				break;
			}
		} /* end of for j */

		/*
		 * if we fall out of the "j" loop with rval
		 * not being set to XLV_MGR_STATUS_OKAY, then
		 * the ve did not match a ve in a corresponding
		 * plex. This is an error, hence we break
		 * out of the i loop and exit.
		 */
		if (rval == INVALID_PLEX) {
			break;
		}
	} /* end of for i */

	/*
	 *	Free memory for array 
	 */
	free (ve_array);
	return(rval);

} /* end of check_plex_overlap() */

/*
 * This routine was copied from xlv_admin/plex.c$xlv_add_plex()
 *
 * On success, the volume oref has its subvolume and plex pointer
 * updated and XLV_MGR_STATUS_OKAY is returned to the caller.
 */
static int 
_append_plex_to_oref (
	xlv_oref_t *v_oref,		/* IN/OUT: Updated data structures */
	xlv_oref_t *p_oref,		/* IN: plex to be added */
	int	   svtype)		/* IN: subvolume type */
{
	xlv_tab_subvol_t *subvol;
	xlv_tab_plex_t	 *plexp;
	int		 rval = XLV_MGR_STATUS_OKAY;
	int		 ve;

	/*
	 * Basic verification 
	 */
	if (v_oref->obj_type == XLV_OBJ_TYPE_PLEX) {	
#ifdef DEBUG
		printf("DBG: _append_plex_to_oref() add plex to itself\n"); 
#endif
		return(INVALID_OPER_ENTITY);
	}

	rval = _xlv_subvol_verify(v_oref, svtype);
	if (rval != XLV_MGR_STATUS_OKAY &&
	    rval != LAST_SUBVOL) {		/* okay to add to last subvol */
#ifdef DEBUG
		printf("DBG: _append_plex_to_oref() failed subvol verify\n");
#endif
		return (rval);
	}

	if (p_oref->plex == NULL) {
#ifdef DEBUG
		printf("DBG: _append_plex_to_oref() plex to add is null\n");
#endif
		return(PLEX_NO_EXIST);
	}
	plexp = p_oref->plex;

	/*
	 * Verify that the subvolume max_plex has not been exceeded.
	 */
	if (svtype == XLV_SUBVOL_TYPE_LOG) {
		subvol = v_oref->vol->log_subvol;
	} else if (svtype == XLV_SUBVOL_TYPE_DATA) {
		subvol = v_oref->vol->data_subvol;
	} else if (svtype == XLV_SUBVOL_TYPE_RT) {
		subvol = v_oref->vol->rt_subvol;
	} else {
		ASSERT(0);
	}
	if (subvol->num_plexes == XLV_MAX_PLEXES) {
		return(XLV_STATUS_TOO_MANY_PLEXES);
	}

	/*
	 * Append the plex to the subvolume.
	 *
	 * Note: Depends on the table of plexes being compacted.
	 */
	subvol->plex[subvol->num_plexes] = plexp;
	subvol->num_plexes++;

	/*
	 * Verify that this plex ve's does not overlap the ve's of
	 * the other plexes. If there is an overlap, undo the previous
	 * changes and return failure.
	 */
	rval = check_plex_overlap(subvol, (subvol->num_plexes - 1));
	if (rval != XLV_MGR_STATUS_OKAY) {
		subvol->num_plexes--;
		subvol->plex[subvol->num_plexes] = NULL;
		return (rval);
	}

	/*
	 * All the ve's in this new plex should be empty.
	 * The plex should not have any ve in the offline state.
	 */
	for (ve = 0; ve < plexp->num_vol_elmnts; ve++) {
		ASSERT(XLV_VE_STATE_INCOMPLETE != plexp->vol_elmnts[ve].state);
		if (XLV_VE_STATE_INCOMPLETE != plexp->vol_elmnts[ve].state)
			plexp->vol_elmnts[ve].state = XLV_VE_STATE_EMPTY;
	}

	v_oref->subvol = subvol;
	v_oref->plex = p_oref->plex;

	/*
	 *	NOTE: the p_oref->plex is handled in the free_plex
	 *	routine.  If it were to be set to NULL here we would
	 *	not be able to remove it from the hash table in the
	 *	free_plex (xlv_delete_plex) routine.
	 */
	
	return (rval);

} /* end of _append_plex_to_oref() */


/*
 * Add a plex to a subvolume.
 *
 * Return XLV_MGR_STATUS_OKAY on success.
 *
 * Note: Currently a plex is always added to end of the plex table.
 */
int xlv_attach_plex (
	xlv_mgr_cursor_t *cursor,
	xlv_oref_t	 *p_oref)
{
	int	retval = XLV_MGR_STATUS_OKAY;

#ifdef DEBUG
	printf("DBG: xlv_attach_plex() called\n");
#endif

	retval = _append_plex_to_oref (cursor->oref, p_oref, cursor->sv_type);
	if (retval != XLV_MGR_STATUS_OKAY) {
		print_error(retval);
		return(retval);
	}

	/*
	 * Remove the standalone plex object from the disk labels
	 * and the object hash table but don't delete the plex data
	 * structure cuz its been added to the subvolume structure.
	 * Leave the label alone, we'll write out the new info later.
	 */
	retval = xlv_delete_plex (p_oref, 0, 1);
	if (XLV_MGR_STATUS_OKAY != retval) {
		return (retval);
	}

	/*
	 * The original standalone plex object has been purged from
	 * the disk labels so we are free to zero out the plex's name.
	 */
	cursor->oref->plex->name[0] = '\0';

	/*
	 * Make the new volume real by updating the disk labels
	 * and notifying the kernel of the new configuration.
	 *
	 * Note we don't need to add this volume to the hash table
	 * because it should already be in the table.
	 */
	retval = xlv_lower_add_plex (xlv_cursor_p, &xlv_vh_list, cursor->oref);
	if (retval != 0) {
		retval = xlv_lower_errno;
		print_error(retval);
	}

	return (retval);

} /* end of xlv_attach_plex() */
