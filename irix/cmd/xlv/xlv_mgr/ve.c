
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
#ident "$Revision: 1.15 $"

#include <bstring.h>
#include <stdio.h>
#include <stdlib.h>
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

#define	MOD_VE_ADD	1
#define	MOD_VE_SUB	2
static int _xlv_mod_ve (
	xlv_oref_t	*target,	/* Out: oref reflects modifications */
	xlv_oref_t	*ve_oref,	/* ADD: ve to be added */
	int		svtype,		/* type of owning subvolume */
	int		pnum,		/* plex number */
	int		mode,		/* mod operation: ADD SUB */
	int		vnum);		/* resulting index into plex */

static int _make_detach_ve_oref (
	xlv_oref_t *target,		/* object holding the ve */
	xlv_oref_t *ve_oref,		/* Out: filled in w/ detached ve's */
	int	   svtype,		/* type of the owning subvolume */
	int	   pnum,		/* plex number */
	int	   vnum,		/* ve number */
	char	   *name);		/* name of the new standalone ve */

static int _check_ve_mirrored (
	xlv_tab_subvol_t *svp,		/* subvolume */
	int		 pnum,		/* plex number */
	int		 vnum);		/* ve number */

/*
 ******************************************************************
 *
 * Destroy the volume element. The oref structure itself is not freed.
 *
 * Return XLV_MGR_STATUS_OKAY on success.
 *
 ******************************************************************
 */
int
xlv_delete_ve (
	xlv_oref_t	*oref,
	int		freeit,		/* flag to free the ve structure */
	int		inplace)	/* flag to leave the label on disk */
{
	int	rval = XLV_MGR_STATUS_OKAY;

	/*
	 * Verify that this is a plex.
	 */
	if (oref->obj_type != XLV_OBJ_TYPE_VOL_ELMNT) {
#ifdef DEBUG
fprintf(stderr, "DBG: xlv_delete_ve() called without a ve oref\n");
#endif
		fprintf(stderr, "Internal error: object is not a ve.\n");
		return (INVALID_OPER_ENTITY);
	}

	/*
	 * Remove the volume information from the disk labels if we're not
	 * leaving it in place.
	 */
	if (!inplace &&
	    0 != xlv_lower_rm_ve (xlv_cursor_p, &xlv_vh_list, NULL, oref)) {
		rval = xlv_lower_errno;
#ifdef DEBUG
fprintf(stderr, "DBG: xlv_delete_ve(): failed in xlv_lower_rm_ve\n");
#endif
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
		if (XLV_OREF_VOL_ELMNT (oref) != NULL)
			free (XLV_OREF_VOL_ELMNT (oref));
		XLV_OREF_VOL_ELMNT (oref) = NULL;
	}

	return(XLV_MGR_STATUS_OKAY);

} /* end of xlv_delete_ve() */


/*
 * This function was taken from xlv_admin/plex.c$xlv_chk_ve()
 *
 * xlv_chk_ve() checks that the space covered by the removed
 * plex is covered by other plexes. If the ve is not mirrored
 * then E_NO_PCOVER is returned.
 *
 * Return 0 if the volume element is actively mirrored.
 * Otherwise return:
 *	LAST_PLEX	    - single plex so ve is not mirrored
 * 	E_VE_MTCH_BAD_STATE - ve is mirrored but other mirrors are not active
 *	E_VE_SPACE_NOT_CVRD - ve is nor mirrored
 */
static int
_check_ve_mirrored (xlv_tab_subvol_t *svp, int pnum, int vnum)
{
	xlv_tab_plex_t		*plex;
	xlv_tab_vol_elmnt_t	*ve;
	unsigned		p, i;
	long long		start, end;
	int			status = E_VE_SPACE_NOT_CVRD;
	time_t			timestamp;
	int			state;

	if (svp->num_plexes == 1) {
		/* Cannot be mirrored if there is only one plex. */
		return (LAST_PLEX);
	}

	/*
	 *	Obtain the values that we need to match.
	 */
	plex = svp->plex[pnum];
	state = plex->vol_elmnts[vnum].state;
	start= plex->vol_elmnts[vnum].start_block_no;
	end  = plex->vol_elmnts[vnum].end_block_no;
	timestamp = plex->vol_elmnts[vnum].veu_timestamp;
#ifdef DEBUG
fprintf(stderr,
	"DBG: _check_ve_mirrored() start=%llu end=%llu for plex %d, ve %d\n",
	start, end, pnum, vnum);
#endif
	if (0 == start && 0 == end) {
		/*
		 * The volume element in question is missing.
		 * Removing this volume element should not be
		 * a problem so just say this ve is mirrored.
		 */
		return(0);
	}

	/*
	 *	Ve's across plexes must line up on their boundaries.
	 *	That is the ve.start_no of one plex must be equal to
	 *	the ve start_no of another plex if they are to cover
	 *	the same address space.
	 *
	 *	Hence we only have to check the ve start_no's.
	 */
        for (p = 0; p < svp->num_plexes; p++) {
		if (p == pnum)
			continue;

		plex = svp->plex[p];
		if (plex == NULL)
			continue;		/* plex is missing */

		/*
		 * for each vol elmnt in this plex...
		 *
		 * Ve's are arranged in ascending order within a plex.
		 */
                for (i=0; i < plex->num_vol_elmnts; i++) {
			ve = &plex->vol_elmnts[i];

			if (start > ve->start_block_no) {
				/*
				 * target address is beyond this ve's
				 */
				continue;	/* go on to next ve */

			} else if (start == ve->start_block_no) {
				/*
				 * Start matches, ensure the end matches.
				 */
				if (end != ve->end_block_no) {
					ASSERT(0);	/* How can this be? */
					/* return (E_VE_END_NO_MATCH); */
					break; /* go on to next plex */
				}

				/*
				 * This ve is mirroring the address space.
				 * This ve must also be in the active state.
				 * If it is not active then the ve cannot
				 * have useful data (yet). In this case
				 * the address space is not mirrored.
				 *
				 * Note that the ve could be in a clean state
				 * if we are operating on a shutdown'ed volume.
				 */ 
				if (ve->state != XLV_VE_STATE_ACTIVE
				    && ve->state != XLV_VE_STATE_CLEAN) {
					status = E_VE_MTCH_BAD_STATE;
					break;	/* go on to next plex */
				}
				/*
				 * When the ve being checked is active or
				 * clean, the mirroring ve must have the
				 * same or newer timestamp. Cannot have a
				 * pulled-and-reinserted disk fool us.
				 * See bug 324915.
				 */
				if ((state == XLV_VE_STATE_ACTIVE
					|| state == XLV_VE_STATE_CLEAN)
				    && ve->veu_timestamp < timestamp) {
					status = E_VE_MTCH_BAD_STATE;
					break;	/* go on to next plex */
				}
				return (0);	/* yeah, it's mirrored */

			} else {
				/*
				 * (start < ve->start_block_no)
				 *
				 * This ve spans the address space beyond
				 * the target so stop searching this plex
				 * and move on to the next plex.
			 	 */
				break;
			 }
		} /* end for each vol element in this plex... */
	} /* for each plex */

	return (status);

} /* end of _check_ve_mirrored() */


/* 
 * This function was taken from xlv_admin/ve.c$xlv_mod_ve()
 *
 * Modify (adding/removing) a volume element relative to
 * its owning plex. The <target> oref is changed to reference
 * the resulting object.
 *
 * NOTE: The plex and subvol attributes should have been modified
 * in the calling routine before coming into this routine.
 */
static int
_xlv_mod_ve (
	xlv_oref_t	*target,	/* OUT: oref reflects modifications */
	xlv_oref_t	*ve_oref,	/* IN: ve to be added */
	int		svtype,		/* type of owning subvolume */
	int		pnum,		/* plex number */
	int		mode,		/* mod operation: ADD SUB */
	int		vnum)		/* resulting index into plex */
{
	xlv_tab_vol_elmnt_t	*I_ve;
	xlv_tab_subvol_t	*subvol;
        xlv_tab_plex_t     	*owning_plex;
	xlv_oref_t		temp_oref;
	int			rval;

	ASSERT (mode == MOD_VE_ADD || mode == MOD_VE_SUB);

	XLV_OREF_INIT (&temp_oref);

	/*
	 * Verify that we have ve to add if mode == add.
	 */
	if (mode == MOD_VE_ADD) {
		I_ve = ve_oref->vol_elmnt;
		if (I_ve == NULL) {
#ifdef DEBUG
fprintf(stderr, "DBG: _xlv_mod_ve() I_ve is null\n");
#endif
			return(ADD_NO_VE);
		}
	}

	/*
	 * Modifing volume element to an existing plex.
	 * Confirm that the plex exists.
	 */
	switch (target->obj_type) {
	    case XLV_OBJ_TYPE_VOL:
                if (svtype == XLV_SUBVOL_TYPE_LOG) {
			subvol = target->vol->log_subvol;
                } else if (svtype == XLV_SUBVOL_TYPE_DATA) {
			subvol = target->vol->data_subvol;
                } else if (svtype == XLV_SUBVOL_TYPE_RT) {
			subvol = target->vol->rt_subvol;
                }
		owning_plex = subvol->plex[pnum];
		break;
	    case XLV_OBJ_TYPE_PLEX:
		subvol = NULL;
                owning_plex = target->plex;
		break;
	    default:
		return (INVALID_OPER_ENTITY);
	}

	if (owning_plex == NULL) {
		return (XLV_STATUS_PLEX_NOT_DEFINED);
	}

	if (mode == MOD_VE_ADD) {
		unsigned 	idx;
        	xlv_tab_plex_t	*orig_pp;

		/*
		 * Make sure that we have not exceeded the max number
		 * of volume elements per plex if we are adding ve's.
		 */
		if (owning_plex->num_vol_elmnts == XLV_MAX_VE_PER_PLEX) {
			return (XLV_STATUS_TOO_MANY_VES);
		}

		/*
		 * When the volume element number is not specified
		 * then treat the operation like an append operation
		 * and add it to the end of the plex.
		 */
		if (vnum < 0) {
			vnum = (int)owning_plex->num_vol_elmnts;
		}

		/*
		 * Cannot have holes in the plex structure itself.
		 */
		if (vnum > (int)owning_plex->num_vol_elmnts) {
			return (E_VE_GAP);
		}

		if (vnum == (int)owning_plex->num_vol_elmnts) {
			/* 
			 * Adding to end; so correctly set the begin
			 * and end size for xlv_add_ve_to_plex().
			 */
			long long	size;
			long long	last;

			idx = owning_plex->num_vol_elmnts - 1;
			/*
			 * Compute size of the volume element being added
			 * and append this ve to the end of the plex.
			 */
			size = I_ve->end_block_no - I_ve->start_block_no + 1;
			ASSERT(size > 0);
			ASSERT(owning_plex->vol_elmnts[idx].end_block_no > 0);
			last = owning_plex->vol_elmnts[idx].end_block_no;
			I_ve->start_block_no = last + 1;
			I_ve->end_block_no = last + size;
		}

 		/*
		 * xlv_add_ve_to_plex() checks that the containing plex
		 * has enough space and grows the plex if there is not
		 * enough space for the additional volume element. So
		 * <owning_plex> may be changed.
		 */
		orig_pp = owning_plex;
		rval = xlv_add_ve_to_plex(&owning_plex, I_ve, vnum);
		if (rval != 0) {
#ifdef DEBUG
fprintf(stderr, "DBG: _xlv_mod_ve(): xlv_add_ve_to_plex() failed with %d\n", rval);
#endif
			/*
			 * remap xlv_add_ve_to_plex() errors
			 */
			if (_E_VE_OVERLAP == rval) {
				rval = E_VE_OVERLAP;
			} else if (_E_VE_NO_MEMORY == rval) {
				rval = MALLOC_FAILED;
			} else {
				rval = VE_FAILED;
			}
			return (rval);
		}
		
		if (subvol != NULL) {
			/*
			 * If this plex belongs to a subvolume, update
			 * the plex pointer because it may point to a
			 * newly malloc'ed (grown) plex.
			 */
			subvol->plex[pnum] = owning_plex;
			
			/* update size of subvolume if appropriate */
			idx = owning_plex->num_vol_elmnts - 1;
			if ((__int64_t)subvol->subvol_size < 
			    owning_plex->vol_elmnts[idx].end_block_no+1) {
				subvol->subvol_size = 
				    owning_plex->vol_elmnts[idx].end_block_no+1;
			}
			/*
			 * Should check that the new ve does not overlap
			 * with any other ve in the other plexes.
			 */
			if (check_plex_overlap(subvol, pnum)) {
				xlv_rem_ve_from_plex(owning_plex, vnum);
				rval = E_VE_OVERLAP;
				return (rval);
			}

			/*
			 * The new ve has an initial state of "empty."
			 * xlv_assemble(1m) will make this ve "active"
			 * if it is the only ve spanning this particular
			 * address space. In other words, if this ve is
			 * not a mirror of an existing ve.
			 */
			owning_plex->
				vol_elmnts[vnum].state = XLV_VE_STATE_EMPTY;
		} else if (orig_pp != owning_plex) {
			/*
			 * This is a standalone plex which grew. The hash
			 * table has a dangling reference to the old plex
			 * structure. So remove and readd to reset the
			 * structure.
			 */
#ifdef DEBUG
fprintf(stderr, "DBG: standalone plex structure grew\n");
#endif
			 (void)remove_object_from_table(target, 1);
			 target->plex = owning_plex;	/* point to new */
			 add_object_to_table(target);
		}

		XLV_OREF_VOL_ELMNT(&temp_oref) = &owning_plex->vol_elmnts[vnum];
	} /* mode == MOD_VE_ADD */

	else if (mode == MOD_VE_SUB) {
		/*
		 * Remove volume element from plex.
		 */
		if (owning_plex->num_vol_elmnts == 1) {
			return (LAST_VE);
		}

		if (subvol != NULL) {
			/*
			 * The owning plex is part of a subvolume.
			 */
			int	ask_user = 0;
			int	found;
			int	vol_isfree;

			rval = _check_ve_mirrored (subvol, pnum, vnum);
			if (rval == 0) {
				/*
				 * ve is mirrored so this ve can be removed
				 */
				goto cont_rm_ve;
			}

			/*
			 * Ve is not mirrored. Check if the volume
			 * is active. If the volume is inactive then
			 * the user is free to remove the ve.
			 */
			vol_isfree = xlv_vol_isfree(target, &found);
			if (XLV_MGR_STATUS_OKAY != vol_isfree)
				return(rval);

			/*
			 * The volume is not active so the ve can be
			 * removed even though the ve is not mirrored.
			 */
			if (rval == E_VE_MTCH_BAD_STATE) {
				ask_user = 1;
				printf (
	"The ve's in the other plexes that cover the same range of disk\n"
	"blocks as this ve are not in the active state.\n"
 	"You may run xlv_assemble(1m) to start a plex revive operation\n"
	"which will copy the data from the ve you wish to remove/detach.\n"
	"You can then safely redo this operation after the ve is active.\n"
	"If you want to force this operation, then answer 'yes' to the\n"
	"following question. This will destroy this data.\n"
	"This is not recommended.\n");
			} 
			else if (rval == E_VE_SPACE_NOT_CVRD) {
				ask_user = 1;
				printf (
	"The range of disk blocks covered by this ve is not covered by\n"
	"other ves within this subvolume. Although the volume is not\n"
	"online, You might want to add space to another plex before removing\n"
	"this ve.  This may be done through xlv_make(1m).\n");
			}
			else if (rval == LAST_PLEX) {
				ask_user = 1;
				printf (
	"Warning - you are removing space from a volume that is not\n"
	"plexed. The volume is not active so this may be a valid\n"
	"operation. If plexing is supported, then it is recommended\n"
	"that you add a plex with a ve that covers this space. This\n"
	"may be done with xlv_make(1m) and xlv_mgr(1m).\n");
			}

			if (ask_user) {
				char	yval[5];
				get_val (yval, VAL_DESTROY_DATA);
				if (!strcasecmp("yes", yval)) {
					/* rval is reset later */
					goto cont_rm_ve;
				}
			}
			return (rval);
		}

		/*FALLTHRU*/
cont_rm_ve:
		/*
		 *	Compacts ve's, decrements num_vol_elmnts, and
		 *	zero's out the last ve.
		 */
		rval = xlv_rem_ve_from_plex(owning_plex, vnum);
		if (rval != 0) {
			return (rval);
		}
	} /* mode == MOD_VE_SUB */

	else {
		/* Unknown Modify Ve Operation. */
		return (XLV_MGR_STATUS_FAIL);
	}

	/*
	 * Commit point. The subvol data structure (if it exists)
	 * has been modified to point to the owning plex. The original 
	 * oref must be updated.
	 */
	XLV_OREF_TYPE(&temp_oref) = XLV_OREF_TYPE(target);
	XLV_OREF_VOL(&temp_oref) = XLV_OREF_VOL(target);
	XLV_OREF_SUBVOL(&temp_oref) = subvol;
	XLV_OREF_PLEX(&temp_oref) = owning_plex;
	strncpy(temp_oref.nodename, target->nodename, XLV_NODENAME_LEN);

	XLV_OREF_COPY(&temp_oref, target);

	return (XLV_MGR_STATUS_OKAY);

} /* end of _xlv_mod_ve() */


/*
 * This routine was copied from xlv_admin/ve.c$xlv_dettach_ve()
 *
 * This routine creates a oref to reference the volume element
 * that will be detached. The plex and ve location are checked.
 *
 * On success, <ve_oref> is set and XLV_MGR_STATUS_OKAY is returned.
 * If the ve is missing, <ve_oref> has a NULL vol_elmnt value.
 * Else <ve_oref> is referencing a malloc'ed xlv_tab_vol_elmnt_t struct.
 *
 * Note that the original ve is left in the place -- the original
 * ve is not removed from the parent plex. This is different from
 * the way the corresponding plex routine _make_detach_p_oref() is
 * implemented.
 */
static int
_make_detach_ve_oref (
	xlv_oref_t *target,	/* object holding the ve */
	xlv_oref_t *ve_oref,	/* Out: filled in w/ detach ve's info */
	int  svtype,		/* type of the owning subvolume */
	int  pnum,		/* plex number */
	int  vnum,		/* ve number */
	char *name)		/* name to give the new standalone ve */
{
	xlv_oref_t	        temp_oref;
	xlv_tab_vol_elmnt_t	*ve;
	xlv_tab_vol_elmnt_t	*orig_ve;
	xlv_tab_subvol_t	*orig_subvol;
	xlv_tab_plex_t		*orig_plex = NULL;
	unsigned		st;

#ifdef DEBUG
fprintf(stderr, "DBG: _make_detach_ve_oref() name=%s plex=%d svtype=%d vnum=%d\n",
	name, pnum, svtype, vnum);
#endif
	/*
	 * Verify that the plex is okay.
	 */
	if (target->obj_type == XLV_OBJ_TYPE_VOL) {
		/*
		 * The owning object is a volume.
		 */
		if (svtype == XLV_SUBVOL_TYPE_LOG) {
			orig_subvol = target->vol->log_subvol;
		} else if (svtype == XLV_SUBVOL_TYPE_DATA) {
			orig_subvol = target->vol->data_subvol;
		} else if (svtype == XLV_SUBVOL_TYPE_RT) {
			orig_subvol = target->vol->rt_subvol;
		} else {
			return (INVALID_SUBVOL);
		}

		if (orig_subvol == NULL) {
			return(SUBVOL_NO_EXIST);
		}
		XLV_OREF_SUBVOL(target) = orig_subvol;

		if (0 <= pnum && pnum < XLV_MAX_PLEXES) {
			orig_plex = orig_subvol->plex[pnum];
			XLV_OREF_PLEX(target) = orig_plex;
		} else {
			orig_plex = NULL;
		}
	}
	else if (target->obj_type == XLV_OBJ_TYPE_PLEX) {
		/*
		 * The owning object is a standalone plex.
		 */
		XLV_OREF_SUBVOL(target) = NULL;		/* be safe */
		orig_plex = XLV_OREF_PLEX(target);
	}

	if (orig_plex == NULL) {
		return(PLEX_NO_EXIST);
	}

	/*
	 * Find the specified volume element.
	 * Removing that ve must not destory the plex.
	 */
	if (vnum < 0 || vnum >= orig_plex->num_vol_elmnts) {
		return(VE_NO_EXIST);
	} else if (orig_plex->num_vol_elmnts == 1) {
		return (LAST_VE);
	} else {
		orig_ve = &(orig_plex->vol_elmnts[vnum]);
	}

        /*
         * Create a volume element oref for the ve being detached.
         */
	if (uuid_is_nil(&orig_ve->uuid, &st)) {
		/*
		 * Volume elememnt is missing.
		 */
		ve = NULL;
	} else {
		ve = (xlv_tab_vol_elmnt_t *)malloc(sizeof(xlv_tab_vol_elmnt_t));
		bcopy (orig_ve, ve, sizeof(xlv_tab_vol_elmnt_t));
		ve->state = XLV_VE_STATE_EMPTY;
		strcpy(ve->veu_name, name);
	}

	XLV_OREF_INIT (&temp_oref);
	XLV_OREF_SET_VOL_ELMNT (&temp_oref, ve);
	strncpy(temp_oref.nodename, target->nodename, XLV_NODENAME_LEN);

	/*
	 * Copy the oref back to the caller.
	 */
	XLV_OREF_COPY(&temp_oref, ve_oref);

	return (XLV_MGR_STATUS_OKAY);

} /* end of _make_detach_ve_oref() */


/*
 * Detach a volume element from the given object.
 * If a non-NULL <dst_name> is given, save the detached ve as <dst_name>.
 * It is the caller's responsibility to check that there is no other
 * ve object of that name.
 *
 * Return XLV_MGR_STATUS_OKAY on success.
 */
/*ARGSUSED3*/
int
xlv_detach_ve (
	xlv_mgr_cursor_t *cursor,
	char		 *dst_name,
	int		 delete,
	int		 force)
{
	int		retval = XLV_MGR_STATUS_OKAY;
	xlv_oref_t	ve_oref;
	char		*name;

	name = (NULL == dst_name) ? "_DETACHED_VE_" : dst_name;

	retval = _make_detach_ve_oref (
			cursor->oref, &ve_oref, cursor->sv_type,
			cursor->plex_no, cursor->ve_no, name);
	if (retval == XLV_MGR_STATUS_OKAY) {
		retval = _xlv_mod_ve (
			cursor->oref, NULL, cursor->sv_type,
			cursor->plex_no, MOD_VE_SUB, cursor->ve_no);
	} else {
		free (ve_oref.vol_elmnt);
	}

	if (retval != XLV_MGR_STATUS_OKAY) {
		print_error(retval);
		return(retval);
	}

	if (ve_oref.vol_elmnt == NULL) {
		/*
		 * The volume element is missing so all that is needed
		 * is to remove the path to the volume element.
		 */
		xlv_attr_cursor_t path;
		xlv_tab_subvol_t  *svp = XLV_OREF_SUBVOL(cursor->oref);

		path.vol = -1;			/* vol */
		if (svp) {			/* subvol */
			ASSERT(force);
			path.subvol = minor(svp->dev);
		} else {
			path.subvol = -1;
		}
		path.plex = cursor->plex_no;	/* plex */
		path.ve = cursor->ve_no;	/* ve */

		retval = xlv_lower_rm_path (
			xlv_cursor_p, &xlv_vh_list, cursor->oref, NULL, &path);
		return (retval);
	}

	if (NULL == dst_name) {
		delete = 1;
		retval = xlv_lower_rm_ve (
			xlv_cursor_p, &xlv_vh_list, cursor->oref, &ve_oref);
	} else {
		retval = xlv_lower_detach_ve (
			xlv_cursor_p, &xlv_vh_list, cursor->oref, &ve_oref);
	}
	if (retval != 0) {
		delete = 1;
		retval = xlv_lower_errno;
		if (retval == XLV_LOWER_ESTALECURSOR) { /* XXX try again? */ }
#ifdef DEBUG
fprintf(stderr, "DBG: xlv_detach_ve() failed calling %s().\n",
	(NULL == dst_name) ? "xlv_lower_rm_ve" : "xlv_lower_detach_ve");
#endif
	}

	if (delete) {
		free (ve_oref.vol_elmnt);
	} else {
		/*
		 * The detached ve is now a standalone object
		 * so it must be added to the table.
		 */
		add_object_to_table(&ve_oref);
	}

	return (retval);
} /* end of xlv_detach_ve() */


/*
 * Return XLV_MGR_STATUS_OKAY on success.
 */
int
xlv_attach_ve (
	xlv_mgr_cursor_t *cursor,
	xlv_oref_t	 *ve_oref)
{
	int	retval = XLV_MGR_STATUS_OKAY;

	retval =  _xlv_mod_ve ( cursor->oref, ve_oref, cursor->sv_type,
				cursor->plex_no, MOD_VE_ADD, cursor->ve_no);
	if (retval != XLV_MGR_STATUS_OKAY) {
		print_error(retval);
		return(retval);
	}

	/*
	 * Remove the standalone ve object from the disk labels
	 * and the object hash table.  Leave the label alone, we'll write out 
	 * the new info later.
	 *
	 * Note it is okay to actually free the ve structure because
	 * the ve information should have been copied into the plex
	 * structure.  
	 */
	retval = xlv_delete_ve (ve_oref, 1, 1);
	if (XLV_MGR_STATUS_OKAY != retval) {
		return (retval);
	}

	/*
	 * Make the new volume real by updating the disk labels
	 * and notifying the kernel of the new configuration.
	 *
	 * Note we don't need to add this volume to the hash table
	 * because it should already be in the table.
	 */
	retval = xlv_lower_add_ve (xlv_cursor_p, &xlv_vh_list, cursor->oref);
	if (retval != 0) {
		retval = xlv_lower_errno;
		print_error(retval);
	}

	return (retval);

} /* end of xlv_attach_ve() */


/*
 * Given the xlv manager cursor, set the cursor oref to point to the
 * referenced volume element. 
 *
 * On success, cursor->oref is set to the volume element
 * and XLV_MGR_STATUS_OKAY is returned.
 */
int
xlv_ve_oref_from_cursor (xlv_mgr_cursor_t *cursor)
{
	xlv_tab_vol_elmnt_t	*vep;
	xlv_tab_subvol_t	*svp;
	xlv_tab_plex_t		*plexp;
	xlv_oref_t		*obj_oref;

	int	svtype;		/* type of the owning subvolume */
	int	pnum;		/* plex number */
	int	vnum;		/* ve number */

#ifdef DEBUG
fprintf(stderr, "DBG: xlv_ve_oref_from_cursor() name=%s plex=%d svtype=%d vnum=%d\n",
	cursor->objname, cursor->plex_no, cursor->sv_type, cursor->ve_no);
#endif

	/*
	 * Confirm that the cursor references a volume element.
	 */
	if (XLV_OBJ_TYPE_VOL_ELMNT != cursor->type)
		return (VE_NO_EXIST);

	obj_oref = cursor->oref;
	svtype = cursor->sv_type;
	pnum = cursor->plex_no;
	vnum = cursor->ve_no;

	/*
	 * Verify that the plex is okay.
	 */
	if (obj_oref->obj_type == XLV_OBJ_TYPE_VOL) {
		/*
		 * The owning object is a volume.
		 */
		if (svtype == XLV_SUBVOL_TYPE_LOG) {
			svp = obj_oref->vol->log_subvol;
		} else if (svtype == XLV_SUBVOL_TYPE_DATA) {
			svp = obj_oref->vol->data_subvol;
		} else if (svtype == XLV_SUBVOL_TYPE_RT) {
			svp = obj_oref->vol->rt_subvol;
		} else {
			return (INVALID_SUBVOL);
		}
		if (svp == NULL) {
			return(SUBVOL_NO_EXIST);
		}
		XLV_OREF_SUBVOL(obj_oref) = svp;

		if (0 <= pnum && pnum < XLV_MAX_PLEXES) {
			plexp = svp->plex[pnum];
			XLV_OREF_PLEX(obj_oref) = plexp;
		} else {
			plexp = NULL;
		}
	}
	else if (obj_oref->obj_type == XLV_OBJ_TYPE_PLEX) {
		/*
		 * The owning object is a standalone plex.
		 */
		XLV_OREF_SUBVOL(obj_oref) = NULL;		/* be safe */
		plexp = XLV_OREF_PLEX(obj_oref);
	}

	if (plexp == NULL) {
		return(PLEX_NO_EXIST);
	}

	/*
	 * Find the specified volume element.
	 * Removing that ve must not destory the plex.
	 */
	if (vnum < 0 || vnum >= plexp->num_vol_elmnts) {
		return(VE_NO_EXIST);
	} else {
		vep = &(plexp->vol_elmnts[vnum]);
	}

	XLV_OREF_VOL_ELMNT(obj_oref) = vep;

	return (XLV_MGR_STATUS_OKAY);

} /* end of xlv_ve_oref_from_cursor() */
