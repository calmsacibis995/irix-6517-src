
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
#ident "$Revision: 1.70 $"


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
#include "xlv_make_int.h"
#include "xlv_admin.h"
#include <tcl.h>

extern void xlv_tab_block_map_print(xlv_block_map_t *block_map); 

static void	xlv_free_vol_space(xlv_tab_vol_entry_t *vol);
static int	xlv_free_vol(xlv_mk_context_t *target);
static int	xlv_get_vol_space(xlv_tab_vol_entry_t **vol);

static xlv_mk_context_t *get_name(
	char	*object,
	char	*name,
	int	choice,
	int	*type,
	int	*pnum,
	int	*vnum,
	int	*rval);

static void prnt_err(int xlv_err);

extern xlv_vh_entry_t *xlv_vh_list;


/*
 * This function prompts the user for his wishes
 * and reads in his response.
 *
 * Note that blank lines and comments (lines with leading "#")
 * are ignored.
 */
int
get_val(char *buffer, int choice)
{
	char *prompt;
	char buf[500], *ch;

	/*
	 * Ask for the user's desire..
	 */
	switch (choice) {
		case VAL_COMMAND:			/* choice */
			pfmt(stderr, MM_NOSTD, 
			":33:Please select choice... \n");
			prompt = "xlv_admin> ";
			break;

		case VAL_OBJECT_NAME:
			pfmt(stdout, MM_NOSTD, 
			":34:Please enter name of object to be operated on.\n");
			prompt = "  object name> ";
			break;
 		case VAL_SUBVOL_NUMBER:
			pfmt(stdout, MM_NOSTD, 
	":35:Please select subvol type... log(1) data(2) rt(3).\n");
			prompt = "  subvol(1/2/3)> ";
                        break;

		case VAL_NEW_OBJECT_NAME:
			pfmt(stdout, MM_NOSTD, 
			":36:Please enter name of new object. \n");
			prompt = "  object name> ";
			break;

		case VAL_PLEX_NUMBER:
			pfmt(stdout, MM_NOSTD, 
			":37:Please select plex number (0-3). \n");
			prompt = "  plex(0-3)> ";
			break;

		case VAL_CREATED_OBJECT_NAME:
			pfmt(stdout, MM_NOSTD, 
			":36:Please enter name of created object.\n");
			prompt = "  object name> ";
			break;

		case VAL_PART_NAME:
			pfmt(stdout, MM_NOSTD, 
	":38:Please enter name of partition you wish to use.\n");
			prompt = "partition name> ";
			break;

		case VAL_VE_NUMBER:
			pfmt(stdout, MM_NOSTD, 
			":39:Please enter ve number. \n");
			prompt = "  ve number> ";
			break;

		case VAL_ADD_OBJECT:
			pfmt(stdout, MM_NOSTD, 
	":40:Please enter the object you wish to add to the target. \n");
			prompt = "  object name> ";
			break;

		case VAL_DESTROY_DATA:
			pfmt(stdout, MM_NOSTD,
	"::Do you want to forcibly destroy this data. (yes / no) \n");
			prompt = "  confirm(yes/no)> ";
			break;

		case VAL_DELETE_LABELS:
			pfmt(stdout, MM_NOSTD,
	"::Deleting the disk labels will cause all volumes to be lost!! \n");
			pfmt(stdout, MM_NOSTD,
	"::Are you sure you want to do this? (yes / no) \n");
			prompt = "  confirm(yes/no)> ";
			break;

		case VAL_NODENAME:
			pfmt(stdout, MM_NOSTD,
			"::Please enter the node name.\n");
			prompt = "  node name> ";
			break;
	}

	/*
	 * Get the user's response.
	 * Ingore white space, blank lines and comments.
	 */
	do {
		printf(prompt);
		buf[0] = '\0';
		gets(buf);		/* XXX should check buf's size */

		/* skip all leading white space */
		for (ch = buf; *ch != '\0'; ch++) {
			if ( ! isspace((unsigned) *ch) )
				break;		/* found a real character */
		}

	} while (*ch == '\0' || *ch == '#' );

	strcpy(buffer, ch);

	return(0);

} /* end of getval() */


#if 0
xlv_add(int choice, char * name)
{

	xlv_mk_context_t	* target;
	xlv_mk_context_t	new_oref;
	xlv_tab_plex_t		sv_plex;
	xlv_part_list_t		partitions;
	char	buf[3];
	int	rval =0;
	int	type, pnum, vnum = -1;
	char	*volp, *svolp, *plexp, *vep;
	char	vol[32];		/* XXX TAC use define here */

	printf("NOT supported at this time \n");
	return(0);

	XLV_OREF_INIT (&new_oref);


	/*
	 *	Determine if the name is a quad or simply the volume name.
	 *	A quad is the real item which is represented by 
	 *	<vol>.<subvol>.<plex>.ve
	 */

	if ((volp=strtok (name, ".")) != NULL) {
		strcpy(vol,volp);
		if ((svolp=strtok (NULL, ".")) != NULL) {
			if (strcmp(svolp,"data")) {
				type =2;
			} else {
				 if (strcmp(svolp,"log")) {
				type =1;
				} else {
					if (strcmp(svolp,"rt")) {
						type =3;
					} else {
						printf("TAC error in subvolume type \n");
					}
				}
			}
		
			if ((plexp=strtok (NULL, ".")) != NULL) {
				pnum = atoi (plexp);
				if ((vep=strtok (NULL, ".")) != NULL) {
					vnum = atoi (vep);
				}	/* ve */
			}	/* plex */
		}	/* subvolume */
		
	}

	switch(choice){
		case 2:
			target = xlv_find_vol( vol );
			break;
		case 3:
			target = xlv_find_gen( vol );
			break;
	}

	if (target == NULL) {
		pfmt(stderr, MM_NOSTD, 
		":41: Requested object '%s' was not found. \n", name);
		return(0);
	}


#ifdef DEBUG
	printf("DBG: Target is ... \n");
		xlv_oref_print (target, XLV_PRINT_ALL);
#endif

	if (type == -1) {
		if (target->vol != NULL) {
			get_val (buf, VAL_SUBVOL_NUMBER);
       			type = atoi(buf);
		} else {
			if (target->plex != NULL) {
				pnum =0;
			} else {
				pfmt(stderr, MM_NOSTD, 
				":42:May not add to a standalone ve \n");
			}
		}
	}
		

	
	switch(choice){
		case 2:
			rval =create_plex(2,&new_oref, type, pnum);
			if (rval != TCL_OK) {
				prnt_err(rval);
				return(rval);
			}

			/* Currently, plex is always added to end */
			if ( (rval = xlv_add_plex(target, &new_oref, type, 
				pnum)) != TCL_OK) {
				prnt_err(rval);
				return(rval);
			}

			if ((rval = xlv_lower_add_plex (&cursor, &xlv_vh_list, 
				target)) != TCL_OK) {

				if (rval < 0) {
					rval = xlv_lower_errno;
				}

				prnt_err(rval);
				return(rval);
			}
			break;

		case 3:
			create_ve(&new_oref);
			if (pnum == -1) {
				get_val (buf, VAL_PLEX_NUMBER);
		        	pnum = atoi(buf);
			}
			if (vnum == -1) {
				get_val (buf, VAL_VE_NUMBER);
       		 		vnum = atoi(buf);
			}

			if ( (rval = xlv_mod_ve(target, &new_oref, type,pnum,1,
				vnum)) != TCL_OK) {
				prnt_err(rval);
				return(rval);
			}

			if ( (rval = xlv_lower_add_ve( &cursor, &xlv_vh_list, 
				target)) != TCL_OK) {
				if (rval < 0) {
					rval = xlv_lower_errno;
				}
				prnt_err(rval);
				return(rval);
			}
#ifdef DEBUG
			printf("DBG: rval from xlv_lower_add_ve=%d \n",rval);
#endif
			break;

		default:
			pfmt(stderr, MM_NOSTD, ":32:Invalid choice \n");
			return(-1);
	}
	errno =0;

	if (rval = xlv_tab_revive_subvolume(target->subvol) != 0) {
		pfmt(stderr, MM_NOSTD,"::TAC error in revive FIX THIS\n");
	}

#ifdef DEBUG
	printf("DBG: ** errno =%d rval=%d \n", errno, rval);

	/*
	 *	Show the resulting object.
	 */
	xlv_oref_print (target, XLV_PRINT_BASIC);
#endif

	return (TCL_OK);
} /* end of xlv_add() */
#endif /* 0 */


int
xlv_prnt_blockmap(char *name)
{
	xlv_mk_context_t	*target;
	xlv_tab_subvol_t        *subvol;
	xlv_block_map_t		*bmap;

	target = xlv_find_gen( name );
	if (target == NULL) {
		pfmt(stderr, MM_NOSTD, 
		":41: Requested object '%s' was not found. \n", name);
		return(-1);
	}

	if ( target->obj_type != XLV_OBJ_TYPE_VOL) {
		pfmt(stderr, MM_NOSTD, ":: Object must be a volume \n");
		return(-1);
	}

	xlv_oref_print (target, XLV_PRINT_BASIC);

	if (target->vol->log_subvol != NULL) {
		subvol = target->vol->log_subvol;
		if (subvol->num_plexes > 1) {
			bmap = xlv_tab_create_block_map(subvol);
			xlv_tab_block_map_print(bmap);
		} else {
			pfmt(stderr, MM_NOSTD,
			":: Subvolume, log, has only one plex. \n");
		}
	}

	if (target->vol->data_subvol != NULL) {
		subvol = target->vol->data_subvol;
		if (subvol->num_plexes > 1) {
			bmap = xlv_tab_create_block_map(subvol);
			xlv_tab_block_map_print(bmap);
		} else {
			pfmt(stderr, MM_NOSTD,
			":: Subvolume, data, has only one plex. \n");
		}
	}

	if (target->vol->rt_subvol != NULL) {
		subvol = target->vol->rt_subvol;
		if (subvol->num_plexes > 1) {
			bmap =xlv_tab_create_block_map(subvol);
			xlv_tab_block_map_print(bmap);
		} else {
			pfmt(stderr, MM_NOSTD,
			":: Subvolume, rt, has only one plex. \n");
		}
	}
	return(TCL_OK);

} /* end of xlv_prnt_blockmap() */


int
xlv_remove(int choice, char *object)
{

	xlv_mk_context_t	*target;
	xlv_mk_context_t	new_oref;
	int	type = -1;
	int	pnum = -1;
	int	vnum = -1;
	char	name2[32];
	char	name[32];
	int	rval =TCL_OK;
#if 0
	int	try=0;
#endif

	target = get_name(object, name, choice, &type, &pnum, &vnum, &rval);
	if (rval != TCL_OK) {
		prnt_err(rval);
		return(rval);
	} else if (target == NULL) {
		return(_E_NOT_FOUND);
	}


#ifdef DEBUG
	printf("DBG: Removing object ...\n");
	xlv_oref_print (target, XLV_PRINT_BASIC);
#endif

	/*
	 *      Ask the user which subvolume and plex they would like to
	 *      modify. If the entity is a volume.
	if (target->vol != NULL) {
		get_val (buf, VAL_SUBVOL_NUMBER);
	        type = atoi(buf);
	}
	 */
	
	switch(choice) {
		case CHOICE_PLEX:
			if (target->vol == NULL) {
				pfmt(stderr, MM_NOSTD, 
				":44:May not remove plex from a standalone plex \n");
				return (-1);
			}
			
			if ( (rval = xlv_rm_plex(target, &new_oref, type, 
				pnum)) != TCL_OK) {
				prnt_err(rval);
				return(rval);
			}

			if ( (rval = xlv_lower_rm_plex (&cursor, 
				&xlv_vh_list, target, &new_oref)) != TCL_OK) {
				if (rval < 0) {
					rval = xlv_lower_errno;
#if 0
					if ((rval == XLV_LOWER_ESTALECURSOR)&&
						(try != 1)){
						if ((rval = xlv_try_recover
						     (target)) == TCL_OK){
							try =1;
							goto one_more_time;
						}
					} else {
						prnt_err(rval);
						return(rval);
					}
#endif
				}
#ifdef DEBUG
printf("DBG: xlv_remove() could not recover from error in xlv_lower\n");
#endif
				prnt_err(rval);
				return(rval);
			}
			
			/*
			 *	XXX TAC free plex & new_oref
			free (new_oref->plex);
			 */

			break;
		case CHOICE_VE:
#ifdef DEBUG
printf("DBG: xlv_remove() subvol=%d plex=%d ve=%d \n",type, pnum, vnum);
#endif
			strcpy(name2,"removeVe");
			if ( (rval = xlv_dettach_ve(target, &new_oref, type, 
				pnum, vnum, name2)) != TCL_OK) {
				prnt_err(rval);
				return(rval);
			}
			if ( (rval = xlv_mod_ve(target, &new_oref, type, pnum, 
				2, vnum)) != TCL_OK) {
				prnt_err(rval);
				return(rval);
			}
			if ( (rval = xlv_lower_rm_ve( &cursor, &xlv_vh_list, 
				target,&new_oref)) != TCL_OK) {
				if (rval < 0) {
					rval = xlv_lower_errno;
#if 0
					if ((rval == XLV_LOWER_ESTALECURSOR)&&
						(try != 1)){
						if ((rval = xlv_try_recover
						     (target)) == TCL_OK){
							try =1;
							goto once_more;
						}
					} else {
						prnt_err(rval);
						return(rval);
					}
#endif
				}
#ifdef DEBUG
printf("DBG: xlv_remove() could not recover from error in xlv_lower\n");
#endif
				prnt_err(rval);
				return(rval);
			}
			/*
			 *	XXX TAC free plex & new_oref
			free (new_oref->vol_elmnt);
			 */
			break;

		default:
			pfmt(stderr, MM_NOSTD, ":32:Invalid choice \n");
			return(-1);
	}

#ifdef DEBUG
	printf("DBG: xlv_remove() ...\n");
	xlv_oref_print (target, XLV_PRINT_BASIC);
#endif
	return(rval);

} /* end of xlv_remove() */


int
xlv_dettach(int choice, char *object)
{
	xlv_mk_context_t	*target;
	xlv_mk_context_t	new_oref;
	xlv_mk_context_t	*test_oref;
	int	type =-1;
	int	pnum =-1;
	int	vnum =-1;
	char	name2[32];
	char	name[32];
	int 	rval =TCL_OK;
#if 0
	int	try=0;
#endif

	/*
	 *	Find the requested entity.
	 */

	target = get_name(object, name, choice, &type, &pnum, &vnum, &rval);
	if (rval != TCL_OK) {
		prnt_err(rval);
		return(rval);
	} else if (target == NULL) {
		return(_E_NOT_FOUND);
	}


#ifdef DEBUG
	printf("DBG: xlv_dettach() detach item from ...\n");
	xlv_oref_print (target, XLV_PRINT_BASIC);
#endif

	/*
	 *	Ask the user what they would like to call the
	 *	detached item.
	 */
	get_val (name2, VAL_NEW_OBJECT_NAME);

	switch(choice) {
		case CHOICE_SUBVOL:
			pfmt(stderr, MM_NOSTD, 
	":30: Subvolume detach is not supported in this version. \n");
			break;


		case CHOICE_PLEX:
			if ((test_oref = xlv_find_gen( name2 )) != NULL) {
				pfmt(stderr, MM_NOSTD, 
	":: XLV object '%s' already exists; select another name. \n", name2);
				return(-1);
			}

			if ( (rval = xlv_dettach_plex(target, &new_oref, name2, 
				type, pnum)) != TCL_OK) {
				prnt_err(rval);
				return(rval);
			}

			if ( (rval = xlv_lower_detach_plex (&cursor, 
				&xlv_vh_list, target, &new_oref)) != TCL_OK) {
				if (rval < 0) {
					rval = xlv_lower_errno;
#if 0
					if ((rval == XLV_LOWER_ESTALECURSOR)&&
						(try != 1)){
						if ((rval = xlv_try_recover
						     (target)) == TCL_OK){
							try =1;
							goto one_more_time;
						}
					} else {
						prnt_err(rval);
						return(rval);
					}
#endif
				}
#ifdef DEBUG
printf("DBG: xlv_dettach() could not recover from error in xlv_lower\n");
#endif
				prnt_err(rval);
				return(rval);
			}
			break;


		case CHOICE_VE:
			if ((test_oref = xlv_find_gen( name2 )) != NULL){
				pfmt(stderr, MM_NOSTD, 
	":: XLV object '%s' already exists; select another name. \n",name2);
				return(-1);
			}
			if (( rval = xlv_dettach_ve(target, &new_oref, type, 
				pnum, vnum, name2)) != TCL_OK) {
				prnt_err(rval);
				return(rval);
			}

			if (( rval = xlv_mod_ve(target, &new_oref, type, 
				pnum, 2, vnum)) != TCL_OK) {
				prnt_err(rval);
				return(rval);
			}

			if (( rval = xlv_lower_detach_ve(&cursor, 
				&xlv_vh_list, target, &new_oref)) != TCL_OK) {
				if (rval < 0) {
					rval = xlv_lower_errno;
#if 0
					if ((rval == XLV_LOWER_ESTALECURSOR)&&
						(try != 1)){
						if ((rval = xlv_try_recover
						     (target)) == TCL_OK){
							try =1;
							goto once_more;
						}
					} else {
						prnt_err(rval);
						return(rval);
					}
#endif
				}
#ifdef DEBUG
printf("DBG: xlv_dettach() could not recover from error in xlv_lower\n");
#endif
				prnt_err(rval);
				return(rval);
			}
			break;

		default:
			pfmt(stderr, MM_NOSTD, ":32:Invalid choice \n");
			return(-1);
	}

#ifdef DEBUG
	printf("DBG: ============= Modified Object =================\n");
	xlv_oref_print (target, XLV_PRINT_ALL);
	printf("DBG: ============= New Object ======================\n");
	xlv_oref_print (&new_oref, XLV_PRINT_ALL);
	printf("DBG: ============= Updating Hash Table =============\n");
#endif
	/*
	 *	Write to table 
	 */

	xlv_mk_add_completed_obj(&new_oref, NO);

	return(rval);

} /* end of xlv_dettach() */


static void
prnt_err(int	xlv_err)
{
	switch(xlv_err) {

		case XLV_LOWER_ESYSERRNO:
			pfmt(stderr, MM_NOSTD, 
	":: Failed in xlv_lower with errno (%d): %s\n", errno, strerror(errno));
			pfmt(stderr, MM_NOSTD, 
	":: Please exit (choice 99) xlv_admin and restart it. \n");
			pfmt(stderr, MM_NOSTD, 
	":: This is a safety measure. \n");
			break;

		case XLV_LOWER_EINVALID:
			pfmt(stderr, MM_NOSTD, 
	":: Failed in xlv_lower with invalid argument\n");
			pfmt(stderr, MM_NOSTD, 
	":: Please exit (choice 99) xlv_admin and restart it. \n");
			pfmt(stderr, MM_NOSTD, 
	":: This is a safety measure. \n");
			break;

		case XLV_LOWER_EIO:
			pfmt(stderr, MM_NOSTD, 
	":: Failed in xlv_lower with I/O error.\n");
			pfmt(stderr, MM_NOSTD, 
	":: Please exit (choice 99) xlv_admin and restart it. \n");
			pfmt(stderr, MM_NOSTD, 
	":: This is a safety measure. \n");
			break;

		case XLV_LOWER_EKERNEL:
			pfmt(stderr, MM_NOSTD, 
	":: Failed in xlv_lower with kernel error (%d): %s\n",
		errno, strerror(errno));
			pfmt(stderr, MM_NOSTD, 
	":: Please exit (choice 99) xlv_admin and restart it. \n");
			pfmt(stderr, MM_NOSTD, 
	":: This is a safety measure. \n");
			break;

		case XLV_LOWER_ENOSUPPORT:
			pfmt(stderr, MM_NOSTD, 
	":: Failed in xlv_lower with unsupported operation.\n");
			pfmt(stderr, MM_NOSTD, 
	":: Please exit (choice 99) xlv_admin and restart it. \n");
			pfmt(stderr, MM_NOSTD, 
	":: This is a safety measure. \n");
			break;

		case XLV_LOWER_ENOIMPL:
			pfmt(stderr, MM_NOSTD, 
	":: Failed in xlv_lower with unimplemented operation.\n");
			pfmt(stderr, MM_NOSTD, 
	":: Please exit (choice 99) xlv_admin and restart it. \n");
			pfmt(stderr, MM_NOSTD, 
	":: This is a safety measure. \n");
			break;

		case XLV_LOWER_ESTALECURSOR:
			pfmt(stderr, MM_NOSTD, 
	":: Failed in xlv_lower with stale cursor when attempting to change\n");
			pfmt(stderr, MM_NOSTD, 
	":: an object. Another user has modified the volumes since you last\n");
			pfmt(stderr, MM_NOSTD, 
	":: invoked xlv_admin. \n");
			pfmt(stderr, MM_NOSTD, 
	":: Please exit (choice 99) xlv_admin and restart it. \n");
			pfmt(stderr, MM_NOSTD, 
	":: This is a safety measure. \n");
			break;

		case XLV_STATUS_TOO_MANY_PLEXES:
			pfmt(stderr, MM_NOSTD, 
	":44: Error: too many plexes \n");
			break;

		case RM_SUBVOL_CRTVOL:
			pfmt(stderr, MM_NOSTD, 
	":62: Error RM_SUBVOL_CRTVOL \n");
			break;

		case RM_SUBVOL_NOVOL:
			pfmt(stderr, MM_NOSTD, 
	":62: Requested subvolume does not exist for this volume. \n");
			break;

		case RM_SUBVOL_NOSUBVOL:
			pfmt(stderr, MM_NOSTD, 
	":62: RM_SUBVOL_NOSUBVOL \n");
			break;

		case DEL_PLEX_BAD_TYPE:
			pfmt(stderr, MM_NOSTD, 
	":62: DEL_PLEX_BAD_TYPE \n");
			break;

		case ADD_NOVOL:
			pfmt(stderr, MM_NOSTD, 
	":62: ADD_NOVOL \n");
			break;

		case DEL_VOL_BAD_TYPE:
			pfmt(stderr, MM_NOSTD, 
	":62: DEL_VOL_BAD_TYPE \n");
			break;

		case SUBVOL_NO_EXIST:
			pfmt(stderr, MM_NOSTD, 
	":62: Requested subvolume does not exist.\n");
			break;

		case MALLOC_FAILED:
			pfmt(stderr, MM_NOSTD, 
	":62: The call to malloc failed. \n");
			break;

		case INVALID_SUBVOL:
			pfmt(stderr, MM_NOSTD, 
	":62: An unknown type of subvolume was requested. \n");
			pfmt(stderr, MM_NOSTD, 
	":: Known types are log, data, and rt. \n");
			break;

		case INVALID__OPER_ENTITY:
			pfmt(stderr, MM_NOSTD, 
	":62: INVALID__OPER_ENTITY \n");
			break;

		case PLEX_NO_EXIST:
			pfmt(stderr, MM_NOSTD, 
	":62: Requested plex does not exist for this volume/subvolume. \n");
			break;

		case LAST_SUBVOL:
			pfmt(stderr, MM_NOSTD, 
	":62: LAST_SUBVOL \n");
			break;

		case LAST_PLEX:
			pfmt(stderr, MM_NOSTD, 
	":62: \n An attempt was made to modify the last plex within the subvolume.\n");
			pfmt(stderr, MM_NOSTD, 
	":: This is not a legal operation.\n");
			pfmt(stderr, MM_NOSTD, 
	":: The only valid ways to remove space from the last plex are:\n");
			pfmt(stderr, MM_NOSTD, 
	":: 1) delete the entire volume, or\n");
			pfmt(stderr, MM_NOSTD, 
	":: 2) add a new plex to the volume, wait for XLV to copy the data to\n");
			pfmt(stderr, MM_NOSTD, 
	"::    the new plex, and then remove the original plex or ve.\n");
			break;

		case INVALID_PLEX:
			pfmt(stderr, MM_NOSTD, 
	":: An attempt was made to add a plex that contained \n");
			pfmt(stderr, MM_NOSTD, 
	":: volume elements which did not match the volume elements \n");
			pfmt(stderr, MM_NOSTD, 
	":: of the corresponding plexes within the subvolume. \n");
			break;

		case INVALID_VE:
			pfmt(stderr, MM_NOSTD, 
	":62: INVALID_VE \n");
			break;

		case LAST_VE:
			pfmt(stderr, MM_NOSTD, 
	":62: An attempt was made to remove or detach the last \n");
			pfmt(stderr, MM_NOSTD, 
	":: volume element of a plex.  This is not a valid operation. \n");
			pfmt(stderr, MM_NOSTD, 
	":: One must remove / detach the plex in this instance.\n");
			break;

		case INVALID_OPER_ENTITY:
			pfmt(stderr, MM_NOSTD, 
	":: An object of inappropriate type was selected for this operation.\n");
			break;

		case ENO_SUCH_ENT:
			pfmt(stderr, MM_NOSTD, 
	":62: Requested object was not found. \n");
			break;

		case VE_FAILED:
			pfmt(stderr, MM_NOSTD, 
	":62: VE_FAILED \n");
			break;

		case E_NO_PCOVER:
			pfmt(stderr, MM_NOSTD, 
	":: The range of disk blocks covered by this plex is either not covered by  \n");
			pfmt(stderr, MM_NOSTD, 
	":: volume elements in another plex OR the volume elements \n");
			pfmt(stderr, MM_NOSTD, 
	":: in these plexes are not in the active state.   \n");
			pfmt(stderr, MM_NOSTD, 
	":: You can only remove or detach a plex if either of these conditions hold.\n");
			pfmt(stderr, MM_NOSTD, 
	":: Selection '42' will show the range of disk blocks covered by a\n");
			pfmt(stderr, MM_NOSTD, 
	":: plex. It will also show its state.\n");
			pfmt(stderr, MM_NOSTD, 
	":: If the range of disk blocks being removed is not covered by another plex,\n");
			pfmt(stderr, MM_NOSTD, 
	":: add new plexes or ve's until it is covered and then remove the plex.\n");
			pfmt(stderr, MM_NOSTD, 
	":: If the range of disk blocks being removed is covered by another\n");
			pfmt(stderr, MM_NOSTD, 
	":: plex but the ve's are not in the active state, then run\n");
			pfmt(stderr, MM_NOSTD, 
	":: xlv_assemble(1m) to make them active.\n");
			pfmt(stderr, MM_NOSTD, 
	":: If xlv_assemble(1m) has been run, then simply wait for the plexes to \n");
			pfmt(stderr, MM_NOSTD, 
	":: move to an active state.  A 'ps -elf | grep xlv' will show \n");
			pfmt(stderr, MM_NOSTD, 
	":: the plex revive taking place. Once the volume elements have\n" );
			pfmt(stderr, MM_NOSTD, 
	":: all moved to an active state, you may remove or detach \n");
			pfmt(stderr, MM_NOSTD, 
	":: the plex.\n\n");
			break;

		case E_VE_SPACE_NOT_CVRD:
			pfmt(stderr, MM_NOSTD, 
	":: The range of disk blocks covered by this ve is not covered by another ve.\n");
			pfmt(stderr, MM_NOSTD, 
	":: You must first cover this range of disk blocks with another plex or ve\n");
			pfmt(stderr, MM_NOSTD, 
	":: before removing this ve.  If you want to delete the\n");
			pfmt(stderr, MM_NOSTD, 
	":: volume then select 'delete volume' (E_VE_SPACE_NOT_CVRD)\n \n");
			break;

		case E_VE_END_NO_MATCH:
			pfmt(stderr, MM_NOSTD, 
	":: Ve's within a plex must be of equal size. \n");
			break;

		case E_VE_GAP:
			pfmt(stderr, MM_NOSTD, 
	":: VE index exceeds number of ve's plus one; this would cause an invalid gap. \n");
			break;

		case E_VE_MTCH_BAD_STATE:
			pfmt(stderr, MM_NOSTD, 
	":: The corresponding ve's in the other plexes are not in the active\n");
			pfmt(stderr, MM_NOSTD, 
	":: state. Please run xlv_assemble.\n");
			break;

		case VE_NO_EXIST:
			pfmt(stderr, MM_NOSTD, 
	":: An attempt was made to remove or detach a non-existant ve. \n");
			pfmt(stderr, MM_NOSTD, 
	":: Select 'Show object' to view the object. \n");
			break;

		case E_VOL_STATE_MISS_UNIQUE:
			pfmt(stderr, MM_NOSTD,
	":: Volume is missing a unique piece.  The only valid operation \n");
			pfmt(stderr, MM_NOSTD,
	":: in this instance is 'Delete Object' \n");
			break;

		case E_VOL_STATE_MISS_SUBVOL:
			pfmt(stderr, MM_NOSTD,
	":: Volume is missing a required subvolume.  The only valid \n");
			pfmt(stderr, MM_NOSTD,
        ":: operation in this instance is 'Delete Object' \n");
			break;

		case E_SUBVOL_BUSY:
			pfmt(stderr, MM_NOSTD,
	":: Volume is active; an active volume may not be deleted. \n");
			break;

		case E_HASH_TBL:
			pfmt(stderr, MM_NOSTD,
	":: Error in tcl hash table. \n");
			break;
			
		case E_VOL_MISS_PIECE:
			pfmt(stderr, MM_NOSTD,
	":: Volume is missing some pieces. The only valid operation\n"
	" in this instance is 'Delete Object'\n");
			break;

		default:
			pfmt(stderr, MM_NOSTD, 
				":62:Returned w/ error %d \n", xlv_err);

	}

} /* end of prnt_err() */


int
xlv_delete(char *name)
{
	xlv_mk_context_t	*target;
	int			rval;
	
	target = xlv_find_gen( name );
	if (target == NULL) {
		pfmt(stderr, MM_NOSTD,
			":: Requested object '%s' was not found \n",name);
		return(0);

	}

	if (target->obj_type == XLV_OBJ_TYPE_VOL) {
			if ((rval = xlv_free_vol(target)) != TCL_OK) {
				prnt_err(rval);
				return(rval);
			}
	}

	if ( target->obj_type == XLV_OBJ_TYPE_PLEX) {
			if ( (rval = xlv_lower_rm_plex (&cursor, 
				&xlv_vh_list, NULL, target)) != TCL_OK) {
				if (rval < 0) {
					rval = xlv_lower_errno;
				}
				pfmt(stderr, MM_NOSTD, 
				"::xlv_lower_rm_plex failed change \n");
				prnt_err(rval);
				return(rval);
			}
			if ((rval = xlv_free_plex(target,SUB)) != TCL_OK) {
				prnt_err(rval);
				return(rval);
			}
	}

	if ( target->obj_type == XLV_OBJ_TYPE_VOL_ELMNT) {

			if ( (rval = xlv_lower_rm_ve( &cursor, &xlv_vh_list, 
				NULL,target)) != TCL_OK) {
				if (rval < 0) {
					rval = xlv_lower_errno;
				}
				prnt_err(rval);
				return(rval);
			}

			if ((rval = xlv_free_ve(target)) != TCL_OK) {
				prnt_err(rval);
				return(rval);
			}
	}

	return (TCL_OK);

} /* end of xlv_delete() */


int
xlv_edit(int choice, char *name)	/* XXX Not implemented */
{
	pfmt(stderr, MM_NOSTD, ":31: Not in this version \n");
	return (0);
}



int
xlv_e_add(int choice, char *object)
{
	xlv_mk_context_t	*target;
	xlv_mk_context_t	*new_oref;
	int	type = -1;
	int	pnum = -1;
	int	vnum = -1;
	char	name2[32];
	char	name[32];
	int	rval = TCL_OK;

	extern	int xlv_tab_revive_subvolume(xlv_tab_subvol_t *xlv_p);

	if (choice == CHOICE_PLEX) {
		pnum = 0;

	} else if (choice == CHOICE_VE)  {
		vnum = -2;
	}

	target = get_name(object, name, choice, &type, &pnum, &vnum, &rval);
	if (rval != TCL_OK) {
		prnt_err(rval);
		return(rval);
	} else if (target == NULL) {
		return(_E_NOT_FOUND);
	}

	if (choice == CHOICE_VE_PLEX_END)  {
		/*
		 * adds ve to end; after setting the vnum,
		 * this choice as just CHOICE_VE
		 */
		choice = CHOICE_VE;
	}

	/*
	 * Check that the standalone object being added is good.
	 * The object is good if it exists as configured, ie is not
	 * missing any pieces.
	 */
	get_val (name2, VAL_ADD_OBJECT);
	new_oref = xlv_find_gen( name2 );
	if (new_oref == NULL) {
		pfmt(stderr, MM_NOSTD,
		     ":41: Requested object '%s' was not found. \n", name2);
		return(0);
	}
	if (xlv_is_incomplete_oref(new_oref)) {
		pfmt(stderr, MM_NOSTD,
		     ":: Requested object '%s' is missing pieces.\n", name2);
		return(0);
	}

	switch(choice) {
		case CHOICE_PLEX:
			/* Currently, plex is always added to end */
			if ( (rval = xlv_add_plex(target, new_oref, type, 
				pnum)) != TCL_OK) {
				prnt_err(rval);
				return(rval);
			}

			/*	
			 * 	Remove the existing plex from the
			 *	disk labels.
			 */

			if (target == NULL) {
				pfmt(stderr, MM_NOSTD, ":62: object == NULL\n");
			}

			if ((rval = xlv_free_plex(new_oref,ADD)) != TCL_OK) {
				prnt_err(rval);
				return(rval);
			}

			strcpy(target->plex->name,"");
			/* XXXjleong target->plex->name[0] = '\0'; */
			if ((rval = xlv_lower_add_plex (&cursor, &xlv_vh_list, 
				target)) != TCL_OK) {
				if (rval < 0) {
					rval = xlv_lower_errno;
				}
				prnt_err(rval);
				return(rval);
			}

			break;

		case CHOICE_VE:
			if ( (rval = xlv_mod_ve(target, new_oref, type,pnum,1,
				vnum)) != TCL_OK) {
				if (rval == -2) {
					pfmt(stderr,MM_NOSTD,
"::Error: tried to insert a ve where one currently exists. \n");
					pfmt(stderr,MM_NOSTD,
"::Double-check the plex and volume element number, \n");
					pfmt(stderr,MM_NOSTD,
"::remember that zero is the first volume element.\n");
					return(rval);
				} else {
					prnt_err(rval);
					return(rval);
				}
			}

			if ( (rval = xlv_lower_rm_ve (&cursor, 
				&xlv_vh_list, NULL, new_oref)) != TCL_OK) {

				if (rval < 0) {
					rval = xlv_lower_errno;
				}
				pfmt(stderr, MM_NOSTD,
":62: xlv_lower_rm_ve backout failed change %d \n",rval);
				/* YYY TAC provide function to try to
				 *	recover from this 
				 */
				prnt_err(rval);
				return(rval);
			}

			if ((rval = xlv_free_ve(new_oref)) != TCL_OK) {
				prnt_err(rval);
				return(rval);
			}

			if (target->vol_elmnt != NULL){
				strcpy(target->vol_elmnt->veu_name,"");
			}

			if (0> syssgi(SGI_XLV_ATTR_CURSOR, &cursor)) {
				printf("XXX get cursor failed \n");
			}

			if ( (rval =xlv_lower_add_ve( &cursor, & xlv_vh_list, 
				target)) != TCL_OK) {
				if (rval < 0) {
					rval = xlv_lower_errno;
				}
				prnt_err(rval);
				return(rval);
			}

			break;

		default:
			pfmt(stderr, MM_NOSTD, ":32:Invalid choice \n");
			return(-1);
	}

	/*
	 *	Show the resulting object.
	 */
#ifdef DEBUG
	printf("DBG: xlv_e_add() ...\n");
	xlv_oref_print (target, XLV_PRINT_BASIC);
#endif

	if ((target->subvol) &&
	    (rval = xlv_tab_revive_subvolume(target->subvol) != 0)) {
		pfmt(stderr, MM_NOSTD,
		     "::cannot revive subvolume. Please check xlv_plexd.\n");
	}

	return (TCL_OK);

} /* end of xlv_e_add() */


/*
 ******************************************************************
 *
 *	Free the space occupied by a volume and it's oref
 *
 ******************************************************************
 */
static int
xlv_free_vol(xlv_mk_context_t * target)
{

	int			rval =-1;
	int			i;

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
		pfmt(stderr, MM_NOSTD, 
			":62:Failed in xlv_lower_rm_vol \n");
		prnt_err(rval);
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
			pfmt(stderr, MM_NOSTD, 
			"::xlv_del_hashentry failed %d \n",rval);
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


/*
 * XXXjleong	get_name() should handle nodename being specified.
 */
static xlv_mk_context_t *
get_name (char	*object,
	  char	*name,
	  int	choice,
	  int	*type,
	  int	*pnum,
	  int	*vnum,
	  int	*rval )
{
	char			*volp, *svolp, *plexp, *vep;
	char			buf[3];
	xlv_tab_subvol_t        *subvol;
	xlv_mk_context_t	*target;
	int			add_ve_op = 0;

	*rval = TCL_OK;

	if (*vnum== -2) {	/* this tells we have an add ve oper. */
	 	*vnum=-1;
		add_ve_op=1;
	}

	/*
	 * see if we have a quad, if so pull out each peice
	 */
	if ((volp=strtok (object, ".")) != NULL) {
		strcpy(name,volp);

		/*
		 * Find correct subvolume.
		 */
		if ((svolp=strtok (NULL, ".")) != NULL) {
			if (0 == strcmp(svolp, "data")) {
				*type = XLV_SUBVOL_TYPE_DATA;
			} else if (0 == strcmp(svolp, "log")) {
				*type = XLV_SUBVOL_TYPE_LOG;
			} else if (0 == strcmp(svolp, "rt")) {
				*type = XLV_SUBVOL_TYPE_RT;
			} else {
				pfmt(stderr, MM_NOSTD, ":32:Invalid choice \n");
			}
			
			if ((plexp=strtok (NULL, ".")) != NULL) {
				*pnum = atoi (plexp);
				if ((vep=strtok (NULL, ".")) != NULL) {
					*vnum = atoi (vep);
				}	/* ve */
			}	/* plex */
		}	/* subvolume */
	}

	/*
	 *	find the volume
	 */
	target = xlv_find_gen( name );

	if (target == NULL) {
		pfmt(stderr, MM_NOSTD, 
			":41: Requested object '%s' was not found. \n", name);
		return(NULL);
	}

	/* 
	 * we didn't get a subvolume so we have to ask the user. 
	 */
	if ((*type == -1) && (target->obj_type == XLV_OBJ_TYPE_VOL)) {

		if (target->vol->state == XLV_VOL_STATE_MISS_UNIQUE_PIECE) {
			*rval = E_VOL_STATE_MISS_UNIQUE;
			return (NULL);
		}

		if (target->vol->state == XLV_VOL_STATE_MISS_COMMON_PIECE) {
			*rval = E_VOL_MISS_PIECE;
			return (NULL);
		}

		if (target->vol->state == XLV_VOL_STATE_MISS_REQUIRED_SUBVOLUME)
		{
			*rval = E_VOL_STATE_MISS_SUBVOL;
			return (NULL);
		}

		/*
		 * if there is only one then simply set the value and
		 * don't ask the user
		 */
		if ((target->vol->log_subvol != NULL) &&
			(target->vol->data_subvol == NULL) &&
			(target->vol->rt_subvol == NULL)) {
				*type = XLV_SUBVOL_TYPE_LOG;
			}
			
		if ((target->vol->log_subvol == NULL) &&
			(target->vol->data_subvol != NULL) &&
			(target->vol->rt_subvol == NULL)) {
				*type = XLV_SUBVOL_TYPE_DATA;
			}
			

		if ((target->vol->log_subvol == NULL) &&
			(target->vol->data_subvol == NULL) &&
			(target->vol->rt_subvol != NULL)) {
				*type = XLV_SUBVOL_TYPE_RT;
			}
			
		if (*type == -1) {
			get_val (buf, VAL_SUBVOL_NUMBER);
       			*type = atoi(buf);
		}
	} 


	if (target->obj_type == XLV_OBJ_TYPE_VOL_ELMNT){
		pfmt(stderr, MM_NOSTD, 
			":42:May not operate on a standalone ve \n");
		return(NULL);
	}

	if (target->obj_type == XLV_OBJ_TYPE_VOL) {
		switch (*type) {
			case 1:
				subvol=target->vol->log_subvol;
				break;
			case 2:
				subvol=target->vol->data_subvol;
				break;
			case 3:
				subvol=target->vol->rt_subvol;
				break;
			default:
				pfmt(stderr, MM_NOSTD,
					"::Invalid subvolume. \n");
				return(NULL);
		}

		if (subvol == NULL) {
			pfmt(stderr, MM_NOSTD, 
				" ::Subvolume does not exist.\n");
			return(NULL);
		}
		
		/*
		 *	we didn't get a plex so we have to ask the user.	
		 */
		if (*pnum == -1) {
			if (subvol->num_plexes == 1) {
				*pnum = 0;
			} else {
				get_val (buf, VAL_PLEX_NUMBER);
				*pnum = atoi(buf);
			}
		}

		if (*pnum > subvol->num_plexes - 1) {
			pfmt(stderr, MM_NOSTD, 
			" ::Requested plex exceeds number of plexes.\n");
			return(NULL);
		}

		/*
		 *	we didn't get a ve so we have to ask the user.
		 */
		if (choice == CHOICE_VE_PLEX_END) {		/* ADD TO END */
			if (subvol->plex[*pnum] == NULL) {
				pfmt(stderr, MM_NOSTD, 
					" ::Plex does not exist.\n");
				return(NULL);
			}
			*vnum = (int)subvol->plex[*pnum]->num_vol_elmnts;

		} else if (CHOICE_VE == 3) {
			if (add_ve_op == 1) {  /* MUST ask for ve */
				get_val (buf, VAL_VE_NUMBER);
				*vnum = atoi(buf);

			} else if (*vnum == -1) {
				if (subvol->plex[*pnum] == NULL) {
					pfmt(stderr, MM_NOSTD, 
						" ::Plex does not exist.\n");
					return(NULL);
				}

				if (subvol->plex[*pnum]->num_vol_elmnts == 1) {
					*vnum =0;
				} else {
					get_val (buf, VAL_VE_NUMBER);
					*vnum = atoi(buf);
				}
			}

			if (*vnum > (int)subvol->plex[*pnum]->num_vol_elmnts) {
				pfmt(stderr, MM_NOSTD, 
				  " ::Requested ve exceeds number of ve's.\n");
				return(NULL);
			}
		}
	}

	if (target->obj_type == XLV_OBJ_TYPE_PLEX) {
		*pnum = 0;

		/*
		 *	we didn't get a ve so we have to ask the user.
		 */
		if (choice == CHOICE_VE_PLEX_END) {		/* ADD TO END */
			if (target->plex == NULL) {
				pfmt(stderr, MM_NOSTD, 
					" ::Plex does not exist.\n");
				return(NULL);
			}
			*vnum = (int)target->plex->num_vol_elmnts;

		} else if (choice == CHOICE_VE) {
			if (add_ve_op == 1) {  /* MUST ask for ve */
				get_val (buf, VAL_VE_NUMBER);
				*vnum = atoi(buf);

			} else if (*vnum == -1) {
				if (target->plex == NULL) {
					pfmt(stderr, MM_NOSTD, 
						" ::Plex does not exist.\n");
					return(NULL);
				}

				if (target->plex->num_vol_elmnts == 1) {
					*vnum =0;
				} else {
					get_val (buf, VAL_VE_NUMBER);
					*vnum = atoi(buf);
				}
			}
	
			if (*vnum > (int)target->plex->num_vol_elmnts) {
				pfmt(stderr, MM_NOSTD, 
				" ::Requested ve exceeds number of ve's.\n");
				return(NULL);
			}

		} else if (choice == CHOICE_PLEX) {
			pfmt(stderr, MM_NOSTD,
				"::May not perform requested operation on a stand-a-lone plex.\n");
			return(NULL);
		}
	}

	return(target);

} /* end of get_name() */


static int
xlv_get_vol_space(xlv_tab_vol_entry_t  **vol)
{
	*vol = (xlv_tab_vol_entry_t *) malloc(sizeof(xlv_tab_vol_entry_t));

	if (*vol == NULL) {
		fprintf(stderr, "Failed to allocate space for volume.\n");
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
	unsigned		st;
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
		&& !(uuid_is_nil (&(vol->log_subvol->uuid), &st))
		&& XLV_ISOPEN(vol->log_subvol->flags)) ||
	    (vol->data_subvol
		&& !(uuid_is_nil (&(vol->data_subvol->uuid), &st))
		&& XLV_ISOPEN(vol->data_subvol->flags)) ||
   	    (vol->rt_subvol
		&& !(uuid_is_nil (&(vol->rt_subvol->uuid), &st))
		&& XLV_ISOPEN(vol->log_subvol->flags))) {
			rval = E_SUBVOL_BUSY;
	}

	/*FALLTHRU*/
done:
	xlv_free_vol_space(vol);
	return(rval);

} /* end of xlv_get_vol_state() */


/*
 * Set the nodename in the disk label header for every disk
 * that makes up this object.
 *
 * "object_list" is a comma separated list of object names.
 */
int
set_nodename(char *object_list)
{
	xlv_mk_context_t	*target;	/* context for one object */
	xlv_mk_context_t	oref;		/* local copy of context */
	xlv_name_t		nodename;	/* new name to be assigned */
	char			buffer[600];	/* unfiltered node name */
	char			*object;
	int			status;

	get_val(buffer, VAL_NODENAME);
	/*
	 * XXXjleong Check that the nodename is valid.
	 */
	bcopy(buffer, nodename, sizeof(xlv_name_t));	/* only copy enough */

	object = strtok(object_list, ",");
	for ( ; object; object = strtok(NULL, ",")) {
#ifdef DEBUG
		printf("DBG: set node name for object \"%s\"\n", object);
#endif
		target = xlv_find_gen( object );
		if (target == NULL) {
			pfmt(stderr, MM_NOSTD,
			":41: Requested object '%s' was not found.\n", object);
			continue;
		}
	
		XLV_OREF_COPY(target, &oref);
		oref.next = NULL;
	
		status = xlv_lower_set_nodename(
				&cursor, &xlv_vh_list, &oref, nodename);
	}

	return(status);
}

/*
 * Set the state of the given volume element.
 */
int
set_ve_state(char *name)	/* XXXjleong set_ve_state */
{
	return(-1);
}
