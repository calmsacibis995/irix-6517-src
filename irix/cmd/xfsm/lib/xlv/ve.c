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

#include <stdio.h>
#include <string.h>
#include <bstring.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>
#include <sys/sysmacros.h>
#include <xlv_error.h>
#include <xlv_utils.h>
#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <xlv_lab.h>
#include <sys/debug.h>
#include <sys/xlv_attr.h>
#include <tcl.h>

#include "xlv_make_cmd.h"
#include "xlv_make_int.h"
#include "xlv_admin.h"

extern Tcl_HashTable    xlv_obj_table;

int
xlv_dettach_ve (xlv_mk_context_t *target,
		xlv_mk_context_t *new,
		int  type,
		int  pnum,
		int  vnum,
		char *name)
{
	xlv_mk_context_t        temp_context;
	xlv_tab_vol_elmnt_t	*ve;
	xlv_tab_vol_elmnt_t	*ve_old;
	xlv_tab_subvol_t	*orig_subvol;
	xlv_tab_plex_t		*orig_plex;

#ifdef DEBUG
	printf("DBG: xlv_dettach_ve() name =%s plex=%d type=%d vnum=%d \n",
		name, pnum, type, vnum);
#endif
	/*
	 *	1. basic verification
	 */

	if (target->obj_type == XLV_OBJ_TYPE_VOL){
	        switch (type) {
                case    XLV_SUBVOL_TYPE_LOG:
			orig_subvol = target->vol->log_subvol;
                        break;

                case    XLV_SUBVOL_TYPE_DATA:
			orig_subvol = target->vol->data_subvol;
                        break;

                case    XLV_SUBVOL_TYPE_RT:
			orig_subvol = target->vol->rt_subvol;
                        break;
		default:
			return (INVALID_SUBVOL);
        	}

		if (orig_subvol == NULL) {
			return(SUBVOL_NO_EXIST);
		}
		XLV_OREF_SUBVOL(target)= orig_subvol;

		orig_plex = orig_subvol->plex[pnum];
	}


	if (target->obj_type == XLV_OBJ_TYPE_PLEX){
		XLV_OREF_SUBVOL(target)= NULL;
		orig_plex = target->plex;
		ve_old=&(target->plex->vol_elmnts[vnum]);
	}

	/*
	 *	Verify plex
	 */
	if (orig_plex == NULL) {
		return(PLEX_NO_EXIST);
	}
	XLV_OREF_PLEX(target)= orig_plex;

	/*
	 *	Get and verify ve
	 */
	ve_old =&(orig_plex->vol_elmnts[vnum]);
	if (ve_old == NULL) {
		return(VE_NO_EXIST);
	}

	if (orig_plex->num_vol_elmnts ==1) {
		return (LAST_VE);
	}


	/*
	 *      2. Initialize temp oref structure
	 */
	XLV_OREF_INIT (&temp_context);


        /*
         *      3. Create ve to hold the detached ve
         *              and set basic information to that of original ve.
         */

	temp_context.obj_type = XLV_OBJ_TYPE_VOL_ELMNT;
	ve = (xlv_tab_vol_elmnt_t *) malloc (sizeof (xlv_tab_vol_elmnt_t));
	bcopy (ve_old, ve, sizeof (xlv_tab_vol_elmnt_t));
	ve->state = XLV_VE_STATE_EMPTY;
	strcpy(ve->veu_name, name);
	XLV_OREF_SET_VOL_ELMNT (&temp_context, ve);


	/*
	 *	4. Set to new ve
	 */
        *new = temp_context;

	return (TCL_OK);

} /* end of xlv_dettach_ve() */



/* 
 *	NOTE:  The plex and subvol attributes are modified in the
 *		calling routine.
 */

/*
 * Create a volume element.
 */
int
xlv_mod_ve (xlv_mk_context_t	*xlv_mk_context,
	    xlv_mk_context_t	*new_oref,
	    int			subvol_type,
	    int			pnum,
	    int			mode,
	    int			vnum,
	    char		**msg)
{
	xlv_tab_vol_elmnt_t	*I_ve;
	xlv_tab_subvol_t	*subvol;
        xlv_tab_plex_t     	*owning_plex;
	xlv_mk_context_t	temp_context;
	int			rval;
	char			str[512];

#ifdef DEBUG
	printf("DBG: xlv_mod_ve() mode =%d \n", mode);
#endif

	XLV_OREF_INIT (&temp_context);

	/*
	 * Verify that we have ve to add if mode == add.
	 */
	if (mode == ADD) {
		I_ve = new_oref->vol_elmnt;
		if (I_ve == NULL) {
#ifdef DEBUG
			sprintf(str,gettxt(":31", 
				":62: I_ve is null exit() \n"));
#endif
			return(ADD_NOVE);	/* YYY TAC change to return */
		}
	}

	/*
	 * Modifing volume element to an existing plex.
	 * Make sure the plex exists.
	 */

        if (xlv_mk_context->obj_type == XLV_OBJ_TYPE_VOL_ELMNT){
		return(-1);		/* YYY TAC E_INVALID_OP_ON_TYPE */
	}

        if (xlv_mk_context->obj_type == XLV_OBJ_TYPE_PLEX){
                owning_plex = xlv_mk_context->plex;
		subvol = NULL;
        }
        else {
                if (subvol_type == XLV_SUBVOL_TYPE_LOG) {
                        owning_plex = 
				xlv_mk_context->vol->log_subvol->plex[pnum];
			subvol = xlv_mk_context->vol->log_subvol;
                }
                if (subvol_type == XLV_SUBVOL_TYPE_DATA) {
                        owning_plex = 
				xlv_mk_context->vol->data_subvol->plex[pnum];
			subvol = xlv_mk_context->vol->data_subvol;
                }
                if (subvol_type == XLV_SUBVOL_TYPE_RT) {
                        owning_plex = 
				xlv_mk_context->vol->rt_subvol->plex[pnum];
			subvol = xlv_mk_context->vol->rt_subvol;
                }
	}

	if (owning_plex == NULL) {
		return (XLV_STATUS_PLEX_NOT_DEFINED);
	}

	/*
	 * Make sure that we have not exceeded the max number
	 * of volume elements per plex if we are adding ve's.
	 */
	if ((owning_plex->num_vol_elmnts == XLV_MAX_VE_PER_PLEX) 
		&& (mode == ADD)) {
		return (XLV_STATUS_TOO_MANY_VES);
	}
		
	/* 
	 * The new context should have the same
	 * volume, subvolume, and plex fields as the current context.
	 */
	XLV_OREF_VOL (&temp_context) = XLV_OREF_VOL (xlv_mk_context);
	XLV_OREF_SUBVOL (&temp_context) = subvol;

 	/*
	 * Make sure that containing plex has enough space. If 
	 * not, realloc.
	 */
	if (mode == ADD) 
	{
		unsigned 	indx;

		/* 
		 * adding to end; so set the begin and end size correctly 
		 * for xlv_add_ve_to_plex
		 */
		if (vnum > (int)owning_plex->num_vol_elmnts) {
			return(E_VE_GAP);
		}

		if (vnum == (int)owning_plex->num_vol_elmnts) {
			long long	size;
			long long	last;

			indx = owning_plex->num_vol_elmnts - 1;
			/*
			 * Compute size of the volume element being added
			 * and append this ve to the end of the plex.
			 */
			size = owning_plex->vol_elmnts[indx].end_block_no - 
				owning_plex->vol_elmnts[indx].start_block_no;
			last = owning_plex->vol_elmnts[indx].end_block_no +1;
			I_ve->start_block_no = last;
			I_ve->end_block_no = last + size;
		}
		if ((rval = xlv_add_ve_to_plex(&owning_plex, I_ve, vnum)) != 0)
		{
#ifdef DEBUG
printf("DBG: call to xlv_add_ve_to_plex() failed with %d \n", rval);
#endif
			return (rval);
		}

		/*
		 * If this plex belongs to a subvolume, update
		 * it to point to the newly malloc'ed piece.
		 */
		if (subvol != NULL) {
			subvol->plex[pnum] = owning_plex;
			
			/* update size of subvolume if appropriate */
			indx = owning_plex->num_vol_elmnts -1;
			if (subvol->subvol_size < 
			    owning_plex->vol_elmnts[indx].end_block_no+1) 
			{
				subvol->subvol_size = 
				    owning_plex->vol_elmnts[indx].
					end_block_no +1;
			}
			if ((owning_plex->num_vol_elmnts-1) == vnum) {
				owning_plex->vol_elmnts[vnum].state = 
							XLV_VE_STATE_ACTIVE;
			}
		}

		XLV_OREF_VOL_ELMNT (&temp_context) = &owning_plex->vol_elmnts[vnum];
	}

	if (mode == SUB) {
		/*
		 *	prior to removing the ve, validate that it
		 *	is ok to remove the ve.
		 */
		if (owning_plex->num_vol_elmnts ==1) {
			return (LAST_VE);
		}

		if (subvol != NULL) {
			rval = xlv_chk_ve (subvol, pnum, vnum);
			if (rval != TCL_OK) {
				char	yval[5];
				int	state = -1;

				state = xlv_get_vol_state(xlv_mk_context);
				if (rval == E_VE_MTCH_BAD_STATE) {

					if (state != TCL_OK) {
						return(rval);
					}

					/*
					 * TAC: recommend that the user
					 * run xlv_assemble and then 
					 * repeat operation.  If this
					 * is not possible and they really
					 * want to do this then go ahead
					 * and allow it.
					 */

					sprintf(str,gettxt(":31", 
	":: The ve's in the other plexes that cover the same range of disk\n"));
					add_to_buffer(msg,str);
					sprintf(str,gettxt(":31", 
	":: blocks as this ve are not in the active state. \n"));
					add_to_buffer(msg,str);
					sprintf(str,gettxt(":31",
 	":: You may run xlv_assemble to start a plex revive operation \n"));
					add_to_buffer(msg,str);
					sprintf(str,gettxt(":31",
	":: which will copy the data from the ve you wish to remove/detach.\n"));
					add_to_buffer(msg,str);
					sprintf(str,gettxt(":31",
	":: You can then safely redo this operation after the ve is active.\n"));
					add_to_buffer(msg,str);
					sprintf(str,gettxt(":31",
	":: If you want to force this operation, then answer 'yes' to the\n"));
					add_to_buffer(msg,str);
					sprintf(str,gettxt(":31",
	":: following question. This will destroy this data.\n"));
					add_to_buffer(msg,str);
					sprintf(str,gettxt(":31",
	":: This is not recommended.\n"));
					add_to_buffer(msg,str);
	
					sprintf(str,gettxt(":31",
":: If you really want to do this, you must use the command xlv_admin(1M).\n"));
					add_to_buffer(msg,str);
					return(rval);

				} 
				if (rval == E_VE_SPACE_NOT_CVRD) {
					if (state != TCL_OK) {
						return(rval);
					}

					sprintf(str,gettxt(":31", 
	":: The range of disk blocks covered by this ve is not covered by\n"));
					add_to_buffer(msg,str);
					sprintf(str,gettxt(":31", 
	":: other ves within this subvolume. Although the volume is not online,\n"));
					add_to_buffer(msg,str);
					sprintf(str,gettxt(":31", 
	":: You might want to add space to another plex before removing\n"));
					add_to_buffer(msg,str);
					sprintf(str,gettxt(":31", 
	":: this ve.  This may be done through xlv_make(1m). \n"));
					add_to_buffer(msg,str);
					sprintf(str,gettxt(":31",
":: If you really want to do this, you must use the command xlv_admin(1M).\n"));
					add_to_buffer(msg,str);
					return(rval);
				}

				if (rval == LAST_PLEX) {
					if (state != TCL_OK) {
						return(rval);
					}

					sprintf(str,gettxt(":31", 
	":: Warning - you are removing space from a volume that is not \n"));
					add_to_buffer(msg,str);
					sprintf(str,gettxt(":31", 
	":: plexed.  The volume is not active so this may be a valid \n"));
					add_to_buffer(msg,str);
					sprintf(str,gettxt(":31", 
	":: operation.  If plexing is supported, then it is \n"));
					add_to_buffer(msg,str);
					sprintf(str,gettxt(":31", 
	":: recommended that you add a plex with a ve that covers \n"));
					add_to_buffer(msg,str);
					sprintf(str,gettxt(":31", 
	":: this space.  This may be done with xlv_make and xlv_admin. \n"));
					add_to_buffer(msg,str);
					sprintf(str,gettxt(":31",
":: If you really want to do this, you must use the command xlv_admin(1M).\n"));
					add_to_buffer(msg,str);

					return(rval);
				}
				return(rval);
			}
		}

		/*
		 *	Compacts ve's, decrements num_vol_elmnts, and
		 *	zero's out the last ve.
		 */
		/*FALLTHRU*/
cont_work:
		if ((rval = xlv_rem_ve_from_plex(owning_plex, vnum)) != 0) {
			return (rval);
		}
	}

	XLV_OREF_PLEX (&temp_context) = owning_plex;

	/*
	 * This is the commit point, we are going to modify both
	 * the subvol data structure (if it exists) to point to 
	 * this plex object as well as modify the original 
	 * context.
	 */

	/* TAC XXX */
	if (owning_plex != NULL) {
		XLV_OREF_TYPE (&temp_context) = XLV_OREF_TYPE (xlv_mk_context);
	}

	*xlv_mk_context = temp_context;
	return (TCL_OK);

} /* end of xlv_mod_ve() */


int
xlv_free_ve(xlv_mk_context_t *target) 
{
	int		 rval;
	Tcl_HashEntry    *curr_entry;
	xlv_mk_context_t *oref;
	char             name[32];

	/*
	 *	Ensure that this is a Volume Element object
	 */

	if (target == NULL) {
		return (E_NO_OBJECT);
	}

	if (XLV_OREF_VOL_ELMNT(target) == NULL) {
		return (E_NO_OBJECT);
	}

	if (target->obj_type != XLV_OBJ_TYPE_VOL_ELMNT) {
		return(E_OBJ_NOT_VOL_ELMNT);
	}

	/*
	 *	Remove the structure from hash table
	 */
	
	/*
	 *	See if this is the first object in the hash list
	 *	AND the only entry in the hash list.
	 */
	strcpy(name, target->vol_elmnt->veu_name);
	curr_entry = Tcl_FindHashEntry(&xlv_obj_table, name);
	oref = (xlv_oref_t *) Tcl_GetHashValue(curr_entry);
	if ((oref == target) && (target->next == NULL)) {
		if (( rval = xlv_del_hashentry( target->vol_elmnt->veu_name )) 
			!= TCL_OK) {
			return(rval);
		}
	}
	else 		/* There are more objects in the hash list... */
	{
                int             found =0;
		xlv_mk_context_t *prev;

#ifdef DEBUG
		printf("DBG: name='%s' \n",name);
		if (target->obj_type == XLV_OBJ_TYPE_VOL)
			printf("DBG: Vol oref=%s \n", oref->vol->name);
		if (target->obj_type == XLV_OBJ_TYPE_PLEX)
			printf("DBG: Plex oref=%s \n", oref->plex->name);
#endif
		/* If we are the first entry in the list */
		if (oref == target) {
			Tcl_SetHashValue (curr_entry, target->next);
			printf("verifing reset \n");
			curr_entry = Tcl_FindHashEntry(&xlv_obj_table, name);
			oref = (xlv_oref_t *) Tcl_GetHashValue(curr_entry);
#ifdef DEBUG
		if (oref->obj_type == XLV_OBJ_TYPE_VOL)
			printf("DBG: xlv_free_ve() vol name is %s\n",
				oref->vol->name);
		if (oref->obj_type == XLV_OBJ_TYPE_PLEX)
			printf("DBG: xlv_free_ve() plex name is %s\n",
				oref->plex->name);
#endif
		}
		else {  /* middle entry or last entry */
			prev = oref;
			found = 0;
			do {
				if (oref->next == target) {
#ifdef DEBUG
			printf("DBG: xlv_free_ve() modified pointer\n");
#endif
					prev->next = target->next;
					found =1;
				} else {
					prev = oref;
					oref = oref->next;
				}
			} while ((oref->next != NULL) && (found));
		}
	}

	free(target->vol_elmnt);
	free(target);
	return(TCL_OK);

} /* end of xlv_free_ve() */
