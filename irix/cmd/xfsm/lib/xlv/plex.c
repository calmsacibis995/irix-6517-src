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
#ident "$Revision: 1.3 $"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <bstring.h>
#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>
#include <xlv_utils.h>
#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <xlv_lab.h>
#include <xlv_error.h>
#include "xlv_make_int.h"
#include <sys/debug.h>
#include <sys/xlv_attr.h>
#include <sys/syssgi.h>
#include "xlv_admin.h"
#include <tcl.h>


extern Tcl_HashTable    xlv_obj_table;

static int xlv_chk_plex (xlv_tab_subvol_t *sv, int pnum);
static int xlv_chk_add_plex (xlv_tab_subvol_t *sv, int del_plex);

static int
xlv_subvol_verify (xlv_mk_context_t *target, int sub_volume_type)
{
	if (XLV_OREF_VOL(target) == NULL) {
		return (RM_SUBVOL_NOVOL);
	} 

	switch (sub_volume_type) {
		case    XLV_SUBVOL_TYPE_LOG:
			if (target->vol->log_subvol == NULL) {
				return(SUBVOL_NO_EXIST);
			}
			if (target->vol->rt_subvol == NULL && 
				target->vol->data_subvol == NULL) {
				return(LAST_SUBVOL);
			}
			break;
		case	XLV_SUBVOL_TYPE_DATA:
			if (target->vol->data_subvol == NULL) {
				return(SUBVOL_NO_EXIST);
			}
			if (target->vol->rt_subvol == NULL && 
				target->vol->log_subvol == NULL) {
				return(LAST_SUBVOL);
			}
			break;
		case	XLV_SUBVOL_TYPE_RT:
			if (target->vol->rt_subvol == NULL) {
				return(SUBVOL_NO_EXIST);
			}
			if (target->vol->data_subvol == NULL && 
				target->vol->log_subvol == NULL) {
				return(LAST_SUBVOL);
			}
			break;
		default:
			return (INVALID_SUBVOL);
	}

	return (TCL_OK);

} /* end of xlv_subvol_verify() */


int
xlv_dettach_plex (xlv_oref_t *orig_oref,
		  xlv_oref_t *new_oref,
		  char *name,
		  int  sub_volume_type,
		  int  plex_num,
		  char **msg) 
{
	xlv_tab_subvol_t	*orig_subvol;
	xlv_tab_plex_t		*orig_plex;
	xlv_mk_context_t	temp_context;
	int			rval =TCL_OK;
	int			i;
	char			str[512];

#ifdef DEBUG
printf("DBG: enter xlv_dettach_plex() subvol:%d plex:%d \n",
	sub_volume_type, plex_num);
#endif

	/*
	 *	1. Basic verification 
	 */

	if (orig_oref->obj_type == XLV_OBJ_TYPE_PLEX) {
		/* can't detach a plex from plex */
#if DEBUG
		printf("DBG: tried to detach plex from self \n");
#endif
		return(INVALID_OPER_ENTITY);
	}

	/* 
	 *	1a. find the plex to dettach and verify it's existance.
	 */
	switch (sub_volume_type) {
		case    XLV_SUBVOL_TYPE_LOG:
			orig_subvol = orig_oref->vol->log_subvol;
			break;

		case    XLV_SUBVOL_TYPE_DATA:
			orig_subvol = orig_oref->vol->data_subvol; 
			break;

		case    XLV_SUBVOL_TYPE_RT:
			orig_subvol = orig_oref->vol->rt_subvol; 
			break;
	}
	orig_plex = orig_subvol->plex[plex_num];

	if (orig_plex == NULL) {
		return(PLEX_NO_EXIST);
	}

	XLV_OREF_SUBVOL(orig_oref)= orig_subvol;

#ifdef DEBUG
	if (orig_oref->plex == orig_plex) {
		printf("DBG: xlv_dettach_plex() orig oref == orig_plex \n");
	}
#endif

	/*
	 * No matter what, cannot detach the only plex of a subvolume.
	 */
	if ( (rval = xlv_subvol_verify(orig_oref,sub_volume_type)) != TCL_OK){
		if (rval == LAST_SUBVOL){
			if (orig_subvol->num_plexes == 1) {
				return(LAST_PLEX);
			} else {
				rval =0;	/* reset as it is ok */
			}
		} else
			return (rval);
	}

	/*	
	 *	1c. Verify that the remaining plexes cover the
	 *		address space that is about to be removed.
	 */
	if ((rval = xlv_chk_plex(orig_subvol,plex_num)) != TCL_OK) {
		if (rval == E_NO_PCOVER) {
			char yval[10];

			if (xlv_get_vol_state(orig_oref) != TCL_OK) {
				return (rval);
			}
			sprintf(str,gettxt(":1",
":: The range of disk blocks covered by this plex is not covered by another\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":1",
":: plex. If you remove/detach this plex then the data on this plex\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":1",
":: will be destroyed.  You should cover this range of disk blocks\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":1",
":: with another plex if you want to preserve the data on this plex.\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":1",
":: You can create a new plex using xlv_make(1m) and then attach it using xlv_admin. \n\n"));
			add_to_buffer(msg,str);

			sprintf(str,gettxt(":1",
":: If you really want to do this, you must use the command xlv_admin(1M).\n"));
			add_to_buffer(msg,str);

		}
		return(rval);
	}

	
	/*
	 *	2. Initialize temp oref structure
	 */
	XLV_OREF_INIT (&temp_context);	/* initialize the oref struct */

	/*
	 *	3. Set dettached plex values
	 */
	temp_context.obj_type = XLV_OBJ_TYPE_PLEX;
	temp_context.plex = orig_plex;
	strcpy(orig_plex->name,name);

	/*
	 *	4. Move plexes in original volume as required.
	 *	That is, if we are detaching the second plex
	 *	then we need to move the third plex to the second plex
	 *	and the fourth plex to the third plex.
	 *
	 *	Decrement count of plexes in original volume.
	 */
	for (i=plex_num; i<4; i++) {
		if (orig_subvol->num_plexes > i+1) {
			orig_subvol->plex[i] = orig_subvol->plex[i+1];
		} else {
			orig_subvol->plex[i] = NULL;
		}
	}
	orig_subvol->num_plexes--;

	/*
	 *	6. Move temp to the new_oref .
	 */
	*new_oref = temp_context;

	/* 
	 *	7. Write out labels
	 */
	if (0 > syssgi(SGI_XLV_ATTR_CURSOR, &cursor)) {
		return(oserror());
        }

	return(rval);

} /* end of xlv_dettach_plex() */


int
xlv_rm_plex (xlv_mk_context_t *orig_oref,
	     xlv_mk_context_t *new_oref,
	     int	      type, 
	     int	      pnum,
	     char	      **msg)
{
	char 	name[33];
	int	rval = TCL_OK;

	/* 
	 *  Set up a temporary name for the plex that is being removed.
	 */

	strcpy(name,"dettachplex");

	/*
	 *	dettach & later free memory for the plex
	 */

	rval = xlv_dettach_plex(orig_oref, new_oref, name, type, pnum, msg);
	
	/*
	 *	IMPORTANT
	 *	The memory for the removed plex is freed in the calling
	 *	function AFTER everything has been written to the disk.
	 */
	
	return(rval);
}


int 
xlv_add_plex (xlv_mk_context_t *target,
	      xlv_mk_context_t *new_oref,
	      int	sub_volume_type,
	      int 	num)
{
	xlv_tab_subvol_t	*subvol;
	int			rval =TCL_OK;

	/*
	 *	1. Basic verification 
	 */

	if (target->obj_type == XLV_OBJ_TYPE_PLEX) {	
		/* cannot dettach a plex from plex  */
#if DEBUG
		printf("DBG: xlv_add_plex() tried to add plex to self \n"); 
#endif
		return(INVALID_OPER_ENTITY);
	}

	rval = xlv_subvol_verify(target, sub_volume_type);
	if ( (rval == LAST_SUBVOL) || (rval == LAST_PLEX) ){
		rval = TCL_OK;
	}

	if (rval != TCL_OK) {
#ifdef DEBUG
		printf("DBG: xlv_add_plex() died doing subvolume verify \n");
#endif
		return (rval);
	}

	if (new_oref->plex == NULL) {
#ifdef DEBUG
		printf("DBG: xlv_add_plex() ... died add plex is null \n");
#endif
		return(PLEX_NO_EXIST);
	}

	/*
	 *	1a. verify that max_plex has not been exceeded for subvol
	 */
	switch (sub_volume_type) {
		case    XLV_SUBVOL_TYPE_LOG:
			subvol = target->vol->log_subvol;
			break;
		case    XLV_SUBVOL_TYPE_DATA:
			subvol = target->vol->data_subvol;
			break;
		case    XLV_SUBVOL_TYPE_RT:
			subvol = target->vol->rt_subvol;
			break;
	}
	if (subvol->num_plexes == XLV_MAX_PLEXES) {
		return(XLV_STATUS_TOO_MANY_PLEXES);
	}

	/*
	 *	3. Add the plex to the subvolume
	 */
	subvol->plex[subvol->num_plexes] = new_oref->plex;
	subvol->num_plexes++;

	/*
	 *	Verify that this plex ve's match the
	 *	ve's of the other plexes that it will be
	 *	overlapping.  If it does not then we remove the
	 *	two changes made in step 3 and return with the
	 *	failure
	 */

	rval = xlv_chk_add_plex(subvol, subvol->num_plexes -1);
	if (rval != TCL_OK) {
		subvol->num_plexes--;
		subvol->plex[subvol->num_plexes] = NULL;
		return (rval);
	}

	target->subvol = subvol;
	target->plex = new_oref->plex;

	/*
	 *	NOTE: the new_oref->plex is handled in the free_plex
	 *	routine.  If it were to be set to NULL here we would
	 *	not be able to remove it from the hash table in the
	 *	free_plex routine.
	 */
	
	return (rval);

} /* end of xlv_add_plex() */


int
xlv_free_plex(xlv_mk_context_t *target, int mode) 
{
	Tcl_HashEntry    *curr_entry;
	xlv_mk_context_t *oref;
	char             name[100];

	/*
	 *	Ensure that this is a plex object
	 */

	if (target == NULL) {
		return (E_NO_OBJECT);
	}

	if (XLV_OREF_PLEX(target) == NULL) {
		return (E_NO_OBJECT);
	}

	if (target->obj_type != XLV_OBJ_TYPE_PLEX) {
		return(E_OBJ_NOT_PLEX);
	}

	/*
	 *	Remove the structure from hash table
	 */

        /*
         *      See if this is the first object in the hash list
         *      AND the only entry in the hash list.
         */
        strncpy(name, target->plex->name, sizeof(xlv_name_t));
        curr_entry = Tcl_FindHashEntry(&xlv_obj_table, name);
        oref = (xlv_oref_t *) Tcl_GetHashValue(curr_entry);
        if ((oref == target) && (target->next == NULL)) {
                if (xlv_del_hashentry(target->plex->name) != TCL_OK) {
                        return(E_HASH_TBL);
                }
        }
	else          /* There are more objects in the hash list... */
        {
                int              found =0;
                xlv_mk_context_t *prev;

#ifdef DEBUG
                printf("DBG: name='%s' \n", name);
		if (target->obj_type == XLV_OBJ_TYPE_VOL)
                        printf("DBG: Vol oref=%s\n", oref->vol->name);
                if (target->obj_type == XLV_OBJ_TYPE_VOL_ELMNT)
                        printf("DBG: Plex oref=%s\n", oref->vol_elmnt->veu_name);
#endif
                /* If we are the first entry in the list */
                if (oref == target) {
#ifdef DEBUG
                        printf("DBG: setting up first - going to try to reset.\n");
#endif
                        Tcl_SetHashValue (curr_entry, target->next);
                        curr_entry = Tcl_FindHashEntry(&xlv_obj_table, name);
                        oref = (xlv_oref_t *) Tcl_GetHashValue(curr_entry);
#ifdef DEBUG
                        if (oref->obj_type == XLV_OBJ_TYPE_VOL)
                                printf("DBG: name is %s \n", oref->vol->name);
                        else if (oref->obj_type == XLV_OBJ_TYPE_VOL_ELMNT)
                                printf("DBG: name is %s \n",
					oref->vol_elmnt->veu_name);
#endif
                }
                else {  /* middle entry or last entry */
                        prev = oref;
                        found =0;
#ifdef DEBUG
                        printf("DBG: setting up middle or last \n");
#endif
                        do {
                                if( oref->next == target) {
                                        prev->next = target->next;
                                        found =1;
                                } else {
	                                prev = oref;
       		                        oref = oref->next;
				}
                        }while ((oref->next != NULL) && (found));
                }
        }

	/* In the case of add, the plex still exists */
	if (mode == SUB) {
		free(target->plex);
	}

	free(target);
	return(TCL_OK);

} /* end of xlv_free_plex() */


typedef struct {
	long long	start_blkno;
	long long	end_blkno;
	unsigned	read_plex_vector;
	unsigned	write_plex_vector;
} ve_entry_t;

#define CAN_READ_WRITE_VE(ve) (((ve)->state != XLV_VE_STATE_OFFLINE) && \
			     ((ve)->state != XLV_VE_STATE_INCOMPLETE))

/*
 * Verify that the space covered by the plex being removed is
 * covered by other plexes.  If not then return E_NO_PCOVER.
 */
static int 
xlv_chk_plex (xlv_tab_subvol_t *sv, int pnum)
{
	xlv_tab_plex_t		*plex;		/* this is being removed */
	xlv_tab_vol_elmnt_t	*ve;
	ve_entry_t		*ve_array;
	unsigned		ve_array_len, p, i;
	unsigned		ve_index, ve_array_index;
	long long		ve_start_blkno, ve_array_start_blkno;
	size_t 			max_len;
	int			j;
	int			num_remaining;
	long long		start;

        /*
         * We only check if there is more than one plex.
         */

	if (sv->num_plexes == 1){
		return (LAST_PLEX);
	}

	/*
	 * Set up simple array to contain plexes that may cover the
	 * space of the removed plex.  Because the address spaces
	 * of each of these plexes may be disjoint, we need to allocate
	 * the max possible.
	 */
	num_remaining=sv->num_plexes-1;

	/*
	 *	Malloc memory for array and initialize
	 */

	max_len = num_remaining * XLV_MAX_VE_PER_PLEX;
	ve_array = (ve_entry_t *) malloc (max_len * sizeof(ve_entry_t));
	bzero (ve_array, (max_len * sizeof(ve_entry_t)));

	/*
	 * Go through all the plexes.  Insert an entry for every
	 * volume address range that's not already in ve_array.
	 */

	ve_array_len = 0;
        for (p = 0; p < sv->num_plexes; p++) {

		if (p == pnum) {	/* don't put the plex to be removed */
			continue;	/* in the array */
		}

		plex = sv->plex[p];

		if (plex == NULL) {
			continue;		/* plex is missing */
		}

		/*
		 * for each vol elmnt in this plex...
		 */
                for (ve_index=0; ve_index < plex->num_vol_elmnts; ve_index++) 
		{
			/*
			 * XXX Instead of set ve_array_index = 0,
			 * ve_array_index should mark the spot where
			 * the previous check of this plex ended.
			 */
			ve_array_index = 0;

			ve = &plex->vol_elmnts[ve_index];
			if (ve == NULL) {
				continue;
			}

			/*
			 * Skip this volume element if it's not active 
			 */
			if (ve->state != XLV_VE_STATE_ACTIVE ) {
#ifdef DEBUG
				printf("DBG: ---- state not active \n");
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
				 * then add this new block range to the
				 * end of ve_array.
				 */
				if (ve_array_index > ve_array_len) {
					ve_array[ve_array_len].start_blkno =
						ve->start_block_no;
					ve_array[ve_array_len].end_blkno =
						ve->end_block_no;
					ve_array_len++;
					goto next_iteration;
				}
			}

			if (ve_start_blkno < ve_array_start_blkno) {

				/*
				 * We've gone beyond this volume
				 * element's block range. This means
				 * that this block range did not exist
				 * in ve_array.
				 * We need to insert the block range.
				 */
				for (i = ve_array_len;
				     i > ve_array_index; i--) {
					ve_array[i] = ve_array[i-1];
				}

				ve_array[ve_array_index].start_blkno =
					ve_start_blkno;
				ve_array[ve_array_index].end_blkno =
					ve->end_block_no;
				ve_array_len++;

			} else {
				/*
				 * ve_start_blkno == ve_array_start_blkno
				 *
				 * An existing entry spans the same
				 * address space as this ve so continue.
				 */
			}

			/*FALLTHRU*/
next_iteration:
			ve_array_index++;
		}	/* end for each vol elmnt in this plex... */
	}

	/*
	 * Now go through and make sure that we've covered the entire
	 * address range for the plex to be removed.
	 *
	 */
	ASSERT (ve_array_len <= (max_len+1));

#ifdef DEBUG
	printf("DBG: xlv_chk_plex() ve_array_len = %d\n", ve_array_len);
	for (i=0; i<ve_array_len; i++)
		printf("DBG: i=%d start=%lld\n", i, ve_array[i].start_blkno);
	printf("DBG: remove plex[%d] num_vol_elmnts = %d\n",
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

		start = plex->vol_elmnts[i].start_block_no;

		for (j=0; j< ve_array_len; j++) {
			if (ve_array[j].start_blkno == start) {
				/*
				 * match found so this mirror'ed 
				 */
				break;
			}
		}

		if (j == ve_array_len) {
			/*
			 * No match was found for this ve so
			 * this ve is "unique."
			 */
			free (ve_array);
			return (E_NO_PCOVER);
		}
	}

	/*
	 *	Free memory for array 
	 */
	free (ve_array);

	return(TCL_OK);

} /* end of xlv_chk_plex() */


/*
 * Verify that the space covered by the removed plex is covered by
 * other plexes.  If not then return E_NO_PCOVER.
 */
int 
xlv_chk_ve (xlv_tab_subvol_t *sv, int pnum, int vnum)
{
	xlv_tab_plex_t		*plex;
	xlv_tab_vol_elmnt_t	*ve;
	unsigned		p, i;
	long long		start, end;

        /*
         * We only check if there is more than one plex.
         */
	if (sv->num_plexes == 1) {
		return (LAST_PLEX);
	}

	/*
	 *	Obtain the values that we need to match.
	 */
	plex = sv->plex[pnum];
	start= sv->plex[pnum]->vol_elmnts[vnum].start_block_no;
	end= sv->plex[pnum]->vol_elmnts[vnum].end_block_no;

#ifdef DEBUG
	printf("DBG: xlv_chk_ve() start=%llu end=%llu for plex %d, ve %d \n",
		start, end, pnum, vnum);
#endif

	/*
	 *	Ve's across plexes must line up on their boundaries.
	 *	That is the ve.start_no of one plex must be equal to
	 *	the ve start_no of another plex if they are to cover
	 *	the same address space.
	 *
	 *	Hence we only have to check the ve start_no's.
	 */
        for (p = 0; p < sv->num_plexes; p++) {
		if (p == pnum) {
			continue;
		}

		plex = sv->plex[p];
		if (plex == NULL)
			continue;		/* plex is missing */

		/*
		 * for each vol elmnt in this plex...
		 */
                for (i=0; i < plex->num_vol_elmnts; i++) {
			ve = &plex->vol_elmnts[i];

			/*
			 * Skip this volume element if it's offline or
			 * incomplete.
			 */

			if (start > ve->start_block_no) {
				continue;	/* to next ve */

			} else if (start == ve->start_block_no) {
				/*
				 * The start matches, now we have to ensure
				 * that the end also matches
				 */
				if (end != ve->end_block_no) {
					return (E_VE_END_NO_MATCH);
				}

				/*
				 *	Now check to ensure that 
				 *	the state is active.  If it
				 *	empty or stale, then the user
				 * 	will destroy data.  If they
				 *	really want to destroy.
				 */ 
				if (ve->state == XLV_VE_STATE_ACTIVE) {
#ifdef DEBUG
				printf("DBG: ---- state is active\n");
#endif
					return (TCL_OK);
				} else {
#ifdef DEBUG
				printf("DBG: ---- state is not active\n");
#endif
					return (E_VE_MTCH_BAD_STATE);
				}

			} else {
				/*
				 * start < ve->start_block_no
				 *
				 * This ve spans the address space beyond
				 * the target so stop searching this plex
				 * and move on to the next plex.
				 */
				break;		/* to next plex */
			}
		}	/* end for each vol elmnt in this plex... */

	}	/* for each plex */
	return(E_VE_SPACE_NOT_CVRD);

} /* end of xlv_chk_ve() */


/*
 *	In this case, all we want to ensure is that corresponding
 *	volume elements in the other plexes have the same start
 *	and end points.
 */
static int 
xlv_chk_add_plex (xlv_tab_subvol_t *sv, int del_plex)
{
	xlv_tab_plex_t		*plex;
	xlv_tab_vol_elmnt_t	*ve;
	ve_entry_t		*ve_array;
	unsigned		ve_array_len, p, i;
	unsigned		ve_index, ve_array_index;
	long long		ve_start_blkno, ve_array_start_blkno;
	long long		start, end;
	int 			j;
	int			num_remaining;
	int			rval = TCL_OK;
	size_t			max_len;
	int			num_ve;

	/*
	 *	Malloc memory for array and initialize
	 */

	num_remaining=sv->num_plexes-1;		/* == del_plex */
	max_len = num_remaining * XLV_MAX_VE_PER_PLEX;
	ve_array = (ve_entry_t *) malloc (max_len * sizeof(ve_entry_t));
	bzero (ve_array, (max_len * sizeof(ve_entry_t)));

	/*
	 * Put all of the volume elements for each plex in 
	 * in the array except for the last one.  The last
	 * plex is the one which we just added.
	 */

	ve_array_len = 0;
        for (p = 0; p < num_remaining; p++) {

		plex = sv->plex[p];

		if (plex == NULL)
			continue;		/* plex is missing */

		/*
		 * for each vol elmnt in this plex...
		 */
                for (ve_index=0; ve_index < plex->num_vol_elmnts; ve_index++) {

			/*
			 * We initialize this to zero for now.
			 * XXX Make it remember where it left off.
			 */
			ve_array_index = 0;
			ve = &plex->vol_elmnts[ve_index];
			if (ve == NULL) {
				continue;
			}

			/*
			 * Handle the special-case where this is the very
			 * first valid vol element that we see.
			 */
			if (ve_array_len == 0) {
				ve_array[0].start_blkno = ve->start_block_no;
				ve_array[0].end_blkno = ve->end_block_no;
				ve_array_len++;
			}

			ve_start_blkno = ve->start_block_no;
			ve_array_start_blkno =
				ve_array[ve_array_index].start_blkno;

			/*
                         * Find the ve_array entry within which
                         * this volume element falls.
                         */

			while (ve_start_blkno > ve_array_start_blkno) {
				ve_array_index++;
				ve_array_start_blkno = 
					ve_array[ve_array_index].start_blkno;

				/*
				 * If we've run past the end of ve_array,
				 * then add this new block range to the
				 * end of ve_array.
				 */
				if (ve_array_index > ve_array_len) {
					ve_array[ve_array_len].start_blkno =
						ve->start_block_no;
					ve_array[ve_array_len].end_blkno =
						ve->end_block_no;

					ve_array_len++;
					goto next_iteration;
				}
			}

			if (ve_start_blkno < ve_array_start_blkno) {
				/*
				 * We've gone beyond this volume
				 * element's block range. This means
				 * that this block range did not exist
				 * in ve_array.
				 * We need to insert the block range.
				 */
				for (i = ve_array_len;
				     i > ve_array_index; i--) {
					ve_array[i] = ve_array[i-1];
				}

				ve_array[ve_array_index].start_blkno =
					ve_start_blkno;
				ve_array[ve_array_index].end_blkno =
					ve->end_block_no;
				ve_array_len++;
			}

			/*FALLTHRU*/
next_iteration:
			ve_array_index++;
		}	/* end for each vol elmnt in this plex... */
	}

#ifdef DEBUG
	printf("DBG: ve_array =%d \n",ve_array_len);
	for (j=0; j<ve_array_len; j++) {
		printf("DBG: a_start = %llu, a_end=%llu \n",
			ve_array[j].start_blkno,ve_array[j].end_blkno);
	}
#endif

	/*
	 *	part II
	 *
	 * now we step through the ve's in the plex which was
	 * just added (sv->num_plexes-1) and verify that they
	 * have the same start and end points as corresponding
	 * ve's in peer plexes.  If one does not, it is an error.
	 */

	plex = sv->plex[num_remaining];
	num_ve = plex->num_vol_elmnts;

	for ( i=0; i<num_ve; i++) {
		/*
		 *	Check if we are adding a ve in the space
		 *	before the first one in this plex.
		 */

		rval = INVALID_PLEX;
		start = plex->vol_elmnts[i].start_block_no;
		end = plex->vol_elmnts[i].end_block_no;

		for (j=0; j<ve_array_len; j++) {

			/*
			 * Check if this is a new ve entry. It must
			 * not overlap with an existing entry.
			 */
			if ((start == ve_array[j].start_blkno) &&
			    (end == ve_array[j].end_blkno)){
				/*
				 * Found new entry.
				 */
				rval = TCL_OK;
				break;
			}
		} /* end of for j */

		/*
		 * if we fall out of the "j" loop with
		 * rval not being set to TCL_OK, then the
		 * ve did not match a ve in a corresponding
		 * plex.  This is an error, hence we break
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

} /* end of xlv_chk_add_plex() */

