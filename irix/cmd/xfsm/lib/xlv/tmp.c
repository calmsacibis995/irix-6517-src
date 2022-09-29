
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


#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bstring.h>
#include <sys/debug.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/syssgi.h>
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
#include "xlv_make_int.h"
#include "xlv_admin.h"
#include <tcl.h>
#include "xlv_make_cmd.h"

static void	xlv_free_vol_space(xlv_tab_vol_entry_t *vol);
int		xlv_free_vol(xlv_mk_context_t *target, char ** msg);
static int	xlv_get_vol_space(xlv_tab_vol_entry_t **vol);

void		prnt_err(int xlv_err, char **msg);

extern xlv_vh_entry_t *xlv_vh_list;


void
prnt_err(int xlv_err, char **msg)
{

	char	str[128];

	switch(xlv_err) {

		case XLV_LOWER_ESYSERRNO:
			sprintf(str,gettxt(":31", 
	":: Failed in xlv_lower with errno =%d\n"),errno);
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: Please exit (choice 99) xlv_admin and restart it. \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: This is a safety measure. \n"));
			add_to_buffer(msg,str);
			break;

		case XLV_LOWER_EINVALID:
			sprintf(str,gettxt(":31", 
	":: Failed in xlv_lower with invalid argument\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: Please exit (choice 99) xlv_admin and restart it. \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: This is a safety measure. \n"));
			add_to_buffer(msg,str);
			break;

		case XLV_LOWER_EIO:
			sprintf(str,gettxt(":31", 
	":: Failed in xlv_lower with I/O error.\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: Please exit (choice 99) xlv_admin and restart it. \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: This is a safety measure. \n"));
			add_to_buffer(msg,str);
			break;

		case XLV_LOWER_EKERNEL:
			sprintf(str,gettxt(":31", 
	":: Failed in xlv_lower with with kernel error - errno =%d\n"),errno);
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: Please exit (choice 99) xlv_admin and restart it. \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: This is a safety measure. \n"));
			add_to_buffer(msg,str);
			break;

		case XLV_LOWER_ENOSUPPORT:
			sprintf(str,gettxt(":31", 
	":: Failed in xlv_lower with unsupported operation.\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: Please exit (choice 99) xlv_admin and restart it. \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: This is a safety measure. \n"));
			add_to_buffer(msg,str);
			break;

		case XLV_LOWER_ENOIMPL:
			sprintf(str,gettxt(":31", 
	":: Failed in xlv_lower with unimplemented operation.\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: Please exit (choice 99) xlv_admin and restart it. \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: This is a safety measure. \n"));
			add_to_buffer(msg,str);
			break;

		case XLV_LOWER_ESTALECURSOR:
			sprintf(str,gettxt(":31", 
	":: Failed in xlv_lower with stale cursor when attempting to change\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: an object. Another user has modified the volumes since you last\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: invoked xlv_admin. \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: Please exit (choice 99) xlv_admin and restart it. \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: This is a safety measure. \n"));
			add_to_buffer(msg,str);
			break;

		case XLV_STATUS_TOO_MANY_PLEXES:
			sprintf(str,gettxt(":31", 
	":44: Error: too many plexes \n"));
			add_to_buffer(msg,str);
			break;

		case RM_SUBVOL_CRTVOL:
			sprintf(str,gettxt(":31", 
	":62: Error RM_SUBVOL_CRTVOL \n"));
			add_to_buffer(msg,str);
			break;

		case RM_SUBVOL_NOVOL:
			sprintf(str,gettxt(":31", 
	":62: Requested subvolume does not exist for this volume. \n"));
			add_to_buffer(msg,str);
			break;

		case RM_SUBVOL_NOSUBVOL:
			sprintf(str,gettxt(":31", 
	":62: RM_SUBVOL_NOSUBVOL \n"));
			add_to_buffer(msg,str);
			break;

		case DEL_PLEX_BAD_TYPE:
			sprintf(str,gettxt(":31", 
	":62: DEL_PLEX_BAD_TYPE \n"));
			add_to_buffer(msg,str);
			break;

		case ADD_NOVOL:
			sprintf(str,gettxt(":31", 
	":62: ADD_NOVOL \n"));
			add_to_buffer(msg,str);
			break;

		case DEL_VOL_BAD_TYPE:
			sprintf(str,gettxt(":31", 
	":62: DEL_VOL_BAD_TYPE \n"));
			add_to_buffer(msg,str);
			break;

		case SUBVOL_NO_EXIST:
			sprintf(str,gettxt(":31", 
	":62: Requested subvolume does not exist.\n"));
			add_to_buffer(msg,str);
			break;

		case MALLOC_FAILED:
			sprintf(str,gettxt(":31", 
	":62: The call to malloc failed. \n"));
			add_to_buffer(msg,str);
			break;

		case INVALID_SUBVOL:
			sprintf(str,gettxt(":31", 
	":62: An unknown type of subvolume was requested. \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: Known types are log, data, and rt. \n"));
			add_to_buffer(msg,str);
			break;

		case INVALID__OPER_ENTITY:
			sprintf(str,gettxt(":31", 
	":62: INVALID__OPER_ENTITY \n"));
			add_to_buffer(msg,str);
			break;

		case PLEX_NO_EXIST:
			sprintf(str,gettxt(":31", 
	":62: Requested plex does not exist for this volume/subvolume. \n"));
			add_to_buffer(msg,str);
			break;

		case LAST_SUBVOL:
			sprintf(str,gettxt(":31", 
	":62: LAST_SUBVOL \n"));
			add_to_buffer(msg,str);
			break;

		case LAST_PLEX:
			sprintf(str,gettxt(":31", 
	":62: \n An attempt was made to modify the last plex within the subvolume.\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: This is not a legal operation.\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: The only valid ways to remove space from the last plex are:\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: 1) delete the entire volume, or\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: 2) add a new plex to the volume, wait for XLV to copy the data to\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	"::    the new plex, and then remove the original plex or ve.\n"));
			add_to_buffer(msg,str);

		case INVALID_PLEX:
			sprintf(str,gettxt(":31", 
	":: An attempt was made to add a plex that contained \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: volume elements which did not match the volume elements \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: of the corresponding plexes within the subvolume. \n"));
			add_to_buffer(msg,str);
			break;

		case INVALID_VE:
			sprintf(str,gettxt(":31", 
	":62: INVALID_VE \n"));
			add_to_buffer(msg,str);
			break;

		case LAST_VE:
			sprintf(str,gettxt(":31", 
	":62: An attempt was made to remove or detach the last \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: volume element of a plex.  This is not a valid operation. \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: One must remove / detach the plex in this instance.\n"));
			add_to_buffer(msg,str);
			break;

		case INVALID_OPER_ENTITY:
			sprintf(str,gettxt(":31", 
	":: An object of inappropriate type was selected for this operation.\n"));
			add_to_buffer(msg,str);
			break;

		case ENO_SUCH_ENT:
			sprintf(str,gettxt(":31", 
	":62: Requested object was not found. \n"));
			add_to_buffer(msg,str);
			break;

		case VE_FAILED:
			sprintf(str,gettxt(":31", 
	":62: VE_FAILED \n"));
			add_to_buffer(msg,str);
			break;

		case E_NO_PCOVER:
			sprintf(str,gettxt(":31", 
	":: The range of disk blocks covered by this plex is either not covered by  \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: volume elements in another plex OR the volume elements \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: in these plexes are not in the active state.   \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: You can only remove or detach a plex if either of these conditions hold.\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: Selection '42' will show the range of disk blocks covered by a\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: plex. It will also show its state.\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: If the range of disk blocks being removed is not covered by another plex,\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: add new plexes or ve's until it is covered and then remove the plex.\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: If the range of disk blocks being removed is covered by another\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: plex but the ve's are not in the active state, then run\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: xlv_assemble(1m) to make them active.\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: If xlv_assemble(1m) has been run, then simply wait for the plexes to \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: move to an active state.  A 'ps -elf | grep xlv' will show \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: the plex revive taking place. Once the volume elements have\n" ));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: all moved to an active state, you may remove or detach \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: the plex.\n\n"));
			add_to_buffer(msg,str);
			break;

		case E_VE_SPACE_NOT_CVRD:
			sprintf(str,gettxt(":31", 
	":: The range of disk blocks covered by this ve is not covered by another ve.\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: You must first cover this range of disk blocks with another plex or ve\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: before removing this ve.  If you want to delete the\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: volume then select 'delete volume' (E_VE_SPACE_NOT_CVRD)\n \n"));
			add_to_buffer(msg,str);
			break;

		case E_VE_END_NO_MATCH:
			sprintf(str,gettxt(":31", 
	":: Ve's within a plex must be of equal size. \n"));
			add_to_buffer(msg,str);
			break;

		case E_VE_GAP:
			sprintf(str,gettxt(":31", 
	":: VE index exceeds number of ve's plus one; this would cause an invalid gap. \n"));
			add_to_buffer(msg,str);
			break;

		case E_VE_MTCH_BAD_STATE:
			sprintf(str,gettxt(":31", 
	":: The corresponding ve's in the other plexes are not in the active\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: state. Please run xlv_assemble.\n"));
			add_to_buffer(msg,str);
			break;

		case VE_NO_EXIST:
			sprintf(str,gettxt(":31", 
	":: An attempt was made to remove or detach a non-existant ve. \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31", 
	":: Select 'Show object' to view the object. \n"));
			add_to_buffer(msg,str);
			break;

		case E_VOL_STATE_MISS_UNIQUE:
			sprintf(str,gettxt(":31",
	":: Volume is missing a unique piece.  The only valid operation \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31",
	":: in this instance is 'Delete Object' \n"));
			add_to_buffer(msg,str);
			break;

		case E_VOL_STATE_MISS_SUBVOL:
			sprintf(str,gettxt(":31",
	":: Volume is missing a required subvolume.  The only valid \n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31",
        ":: operation in this instance is 'Delete Object' \n"));
			add_to_buffer(msg,str);
			break;

		case E_SUBVOL_BUSY:
			sprintf(str,gettxt(":31",
	":: Volume is active; an active volume may not be deleted. \n"));
			add_to_buffer(msg,str);
			break;

		case E_HASH_TBL:
			sprintf(str,gettxt(":31",
	":: Error in tcl hash table. \n"));
			add_to_buffer(msg,str);
			break;
			
		case E_VOL_MISS_PIECE:
			sprintf(str,gettxt(":31",
	":: Volume is missing some pieces. The only valid operation\n"));
			add_to_buffer(msg,str);
			sprintf(str,gettxt(":31",
	":: in this instance is 'Delete Object'\n"));
			add_to_buffer(msg,str);
			break;
			
		default:
			sprintf(str,gettxt(":31", 
			":62:Returned w/ error %d \n"), xlv_err);
			add_to_buffer(msg,str);

	}

} /* end of prnt_err() */




/*
 ******************************************************************
 *
 *	Free the space occupied by a volume and it's oref
 *
 ******************************************************************
 */
int
xlv_free_vol(xlv_mk_context_t * target, char ** msg)
{

	int			rval =-1;
	int			i;
	char			str[128];

	/*
	 *	Verify that none of the subvolumes are open.
	 *	If they are open then do not remove the volume.
	 */
	
	if ((rval = xlv_get_vol_state(target)) != TCL_OK) {
		if ((rval != ENFILE) && (rval != ENOENT)) {
			return(rval);
		}
	}

	/*
	 *	0. Remove the volume information from
	 *		the disk labels.
	 */
	if ( (rval = xlv_lower_rm_vol( &cursor, &xlv_vh_list, 
		target)) != TCL_OK) {
		if (rval < 0) {
			rval = xlv_lower_errno;
		}
		sprintf(str,gettxt(":31", 
			":62: Failed in xlv_lower_rm_vol \n"));
		add_to_buffer(msg,str);
		prnt_err(rval,msg);
		return(rval);
	}

	/*
	 *	1. Verify that this is a volume 
	 */
	if (target->obj_type != XLV_OBJ_TYPE_VOL) {
		return (INVALID_OPER_ENTITY);
	}


	/*
	 *      YYY TAC
	 */
	if (target->next != NULL) {
#ifdef DEBUG
		printf("DBG: reset this link \n");
#endif
	}


	/*
	 *	Remove the entry from the hash table.
	 */
	if ((rval = xlv_del_hashentry( target->vol->name )) != TCL_OK) {
			sprintf(str,gettxt(":31", 
			"::xlv_del_hashentry failed %d \n"),rval);
			add_to_buffer(msg,str);
		return(rval);
	}


	/*
	 *	For each subvolume, free it's plexes, 
	 *		then free the subvolume.
	 */
	if (target->vol->log_subvol != NULL) {
		for (i=0; i< target->vol->log_subvol->num_plexes; i++) {
			if (target->vol->log_subvol->plex[i] != NULL) {
				free (target->vol->log_subvol->plex[i]);
			}
		}
		free (target->vol->log_subvol);
	}

	if (target->vol->data_subvol != NULL) {
		for (i=0; i< target->vol->data_subvol->num_plexes; i++) {
			if (target->vol->data_subvol->plex[i] != NULL) {
				free (target->vol->data_subvol->plex[i]);
			}
		}
		free (target->vol->data_subvol);
	}

	if (target->vol->rt_subvol != NULL) {
		for (i=0; i< target->vol->rt_subvol->num_plexes; i++) {
			if (target->vol->rt_subvol->plex[i] != NULL) {
				free (target->vol->rt_subvol->plex[i]);
			}
		}
		free (target->vol->rt_subvol);
	}


	/*
	 *	Free the volume and the oref
	 */
	free (target->vol);
	free (target);

	return(TCL_OK);

} /* end of xlv_free_vol() */


static int
xlv_get_vol_space(xlv_tab_vol_entry_t  **vol)
{
	*vol = (xlv_tab_vol_entry_t *) malloc(sizeof(xlv_tab_vol_entry_t));

	if (*vol == NULL) {
		return(-1);
	}

	if ((NULL == ((*vol)->log_subvol = get_subvol_space())) ||
	    (NULL == ((*vol)->data_subvol = get_subvol_space())) ||
	    (NULL == ((*vol)->rt_subvol = get_subvol_space()))) {
		fprintf(stderr, "Failed to allocate space for volume entry.\n");
		xlv_free_vol_space(*vol);
		return(-1);
	}

	return (0);
}


static void
xlv_free_vol_space(xlv_tab_vol_entry_t  *vol)
{
	free_subvol_space(vol->log_subvol);
	free_subvol_space(vol->data_subvol);
	free_subvol_space(vol->rt_subvol);
	free(vol);
}


/*
 * 	xlv_get_vol_state(xlv_mk_context_t * target)
 *
 *	Searches through kernel table until
 *		1.) ENOENT is found - kernel table was not found
 *			This is ok, as there maybe valid for reason
 *			for none of the volumes being assembled.
 *			(ex. all volumes are plexed & plexing was disabled)
 *		2.) ENFILE is found - kernel is not aware of the volume.
 *			This is ok, as for some reason the volume may
 *			not have been assembled (ex. plexing was disabled)
 *		3.) Volume was found.
 *
 *	If the volume is found then we see if any of its subvolumes are
 *	busy.  If they are busy we set rval to E_SUBVOL_BUSY
 *	
 *	Return codes:
 *	If ENOENT or ENFILE is encountered we return TCL_OK.  As we
 *	just want to make sure that the volume is not busy.  If the
 *	volume was located and its subvolumes are not busy we return
 *	TCL_OK.  
 *
 *	If any other error was encountered including E_SUBVOL_BUSY 
 *	we return the error code.
 */

int
xlv_get_vol_state(xlv_mk_context_t *target)
{
	int			rval =TCL_OK;
	xlv_attr_req_t 		req;
	xlv_tab_vol_entry_t	*vol;
	extern int		errno;
	int			i;

	/*
	 *	Allocate space for a volume and its possible subvol's.
	 */
	if (0 > xlv_get_vol_space(&vol)) {
		rval = ENOMEM;
		return(-1);
	}
	req.attr = XLV_ATTR_VOL;
	req.ar_vp = vol;

	/*
	 * XXX Hack to clear the xlv cursor.
	 *
	 *	Must clear the xlv cursor so the kernel query
	 *	can start at the beginning.
	 */
	for (i=1; i<5; i++)
	cursor.u_bits[i] = -1;

	/*
	 *	loop through kernel table until volume is found
	 *	or end of table is returned
	 */
	for (;;) {

		if (syssgi(SGI_XLV_ATTR_GET, &cursor, &req) < 0) {

			/*
			 * Can't get information from kernel.
			 */
			rval = oserror();

			if ((rval == ENFILE) || (rval == ENOENT)) {
				rval = TCL_OK;
				goto done;
			}

			/*
			 * if cursor is stale; we will reread it to see
			 * if we may continue.
			 */
			if (rval == ESTALE) {
				if (0 > syssgi(SGI_XLV_ATTR_CURSOR, &cursor)) {
					rval =XLV_LOWER_ESTALECURSOR;
					goto done;
				} 
				rval = TCL_OK; 
				continue;
			}
			else {
				/*
				 * Unexpected error.
				 */
				goto done;
			}
		}

		/*
		 * We've got an entry from the kernel, is it the one
		 * we want?
		 */
		if (strcmp(target->vol->name,vol->name) == 0) {
			break;
		}
	}

	/*
	 *	not end of table then we have found the volume;
	 *	see if subvolumes are busy.
	 */
	if ((vol->log_subvol
		&& XLV_ISOPEN(vol->log_subvol->flags)) ||
	    (vol->data_subvol
		&& XLV_ISOPEN(vol->data_subvol->flags)) ||
   	    (vol->rt_subvol
		&& XLV_ISOPEN(vol->log_subvol->flags))) {
			rval = E_SUBVOL_BUSY;
	}

	/*FALLTHRU*/
done:
	xlv_free_vol_space(vol);
	return(rval);

} /* end of xlv_get_vol_state() */
