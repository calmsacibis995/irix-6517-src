#ifndef __XLV_MGR_H__
#define __XLV_MGR_H__

/*
 * xlv_mgr.h
 *
 *      Common header file for xlv_mgr(1m)
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.22 $"

#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>

#include <tcl.h>

#define	XLV_MGR_VAR_SUSER	"expert"
#define	XLV_MGR_VAR_VERBOSE	"verbose"

/*
 * XXXjleong Move error codes.
 */
#define	XLV_MGR_STATUS_OKAY	0
#define	XLV_MGR_STATUS_FAIL	1

/*
 * Error codes for the administration functions.
 */
#define		RM_SUBVOL_CRTVOL	100
#define		RM_SUBVOL_NOVOL		101
#define		RM_SUBVOL_NOSUBVOL	102
#define		ADD_NOVOL		103
#define		DEL_VOL_BAD_TYPE	104
#define 	DEL_PLEX_BAD_TYPE	105
#define		SUBVOL_NO_EXIST		106
#define		MALLOC_FAILED		107
#define		INVALID_SUBVOL		108
#if 0
#define		INVALID_OPER_ENTITY	109
#endif
#define		PLEX_NO_EXIST		110
#define		LAST_SUBVOL		111
#define		LAST_PLEX		112
#define		INVALID_PLEX		113
#define		INVALID_VE		114
#define		LAST_VE			115
#define		INVALID_OPER_ENTITY	116
#define		ENO_SUCH_ENT		117
#define		VE_FAILED		118
#define		ADD_NO_VE		119
#define		E_NO_PCOVER		120
#define		E_OBJ_NOT_PLEX		121
#define		E_NO_OBJECT		122
#define		E_OBJ_NOT_VOL_ELMNT	123
#define		E_VE_SPACE_NOT_CVRD	124
#define		E_VE_END_NO_MATCH	125
#define		E_VE_GAP		126
#define 	E_VE_MTCH_BAD_STATE	127
#define		VE_NO_EXIST		128
#define		E_VOL_STATE_MISS_UNIQUE	129
#define		E_VOL_STATE_MISS_SUBVOL	130
#define		E_C_K_VE_NO_MATCH	131
#define		E_VE_NULL		132
#define		E_PLX_NULL		133
#define		E_C_K_PLX_NO_MATCH	134
#define		E_SUBVOL_BUSY		135
#define		E_HASH_TBL		136

#define		E_VE_OVERLAP		137
#define		E_VOL_MISS_PIECE	138

/*
 * Command procedures - one for every command in xlv_mgr.
 *
 * Each of these procedures are of type Tcl_CmdProc.
 */

extern int tclcmd_attach (
	ClientData clientData, Tcl_Interp *interp, int argc, char *argv[] );

extern int tclcmd_detach (
	ClientData clientData, Tcl_Interp *interp, int argc, char *argv[] );

extern int tclcmd_delete (
	ClientData clientData, Tcl_Interp *interp, int argc, char *argv[] );

extern int tclcmd_change (
	ClientData clientData, Tcl_Interp *interp, int argc, char *argv[] );

extern int tclcmd_insert (
	ClientData clientData, Tcl_Interp *interp, int argc, char *argv[] );

extern int tclcmd_remove (
	ClientData clientData, Tcl_Interp *interp, int argc, char *argv[] );

extern int tclcmd_script (
	ClientData clientData, Tcl_Interp *interp, int argc, char *argv[] );

extern int tclcmd_show (
	ClientData clientData, Tcl_Interp *interp, int argc, char *argv[] );

extern int tclcmd_quit (
	ClientData clientData, Tcl_Interp *interp, int argc, char *argv[] );

extern int tclcmd_help (
	ClientData clientData, Tcl_Interp *interp, int argc, char *argv[] );

extern int tclcmd_shell (
	ClientData clientData, Tcl_Interp *interp, int argc, char *argv[] );

extern int tclcmd_reset (
	ClientData clientData, Tcl_Interp *interp, int argc, char *argv[] );

extern void tclcmd_init (int readlabels);


/*
 **************************************************************************
 *                                                                        *
 **************************************************************************
 */

#ifndef YES
#define YES 1
#endif
#ifndef NO
#define NO 0
#endif
#define FORCE	0x1
#define EXPERT	0x2
#define START	0x4

extern int	Tclflag;
extern uid_t	mypid;

extern Tcl_HashTable	xlv_obj_table;

/*
 * The cursor required for querying the kernel.
 */
struct xlv_attr_cursor;
extern struct xlv_attr_cursor *xlv_cursor_p;


/*
 * Initialization procedure
 */
extern void xlv_mgr_init (void);


/*
 * The cursor used by xlv_mgr to specify parts of an object.
 */
typedef struct xlv_mgr_cursor_s {
	xlv_oref_t	*oref;
	char		*nodename;
	char		*objname;
	int		type;		/* XLV_OBJ_TYPE_* */
	int		sv_type;	/* XLV_SUBVOL_TYPE_{LOG,DATA,RT} */
	int		plex_no;	/* iff type is ve or higher */
	int		ve_no;		/* iff type is ve */
} xlv_mgr_cursor_t;


/*
 * Command procedures for xlv_mgr
 *
 * Returns XLV_MGR_STATUS_OKAY upon success
 */

#define	ATTACH_MODE_APPEND	1
#define	ATTACH_MODE_INSERT	2
extern int xlv_mgr_attach (
	int type, char *src_name, char *dst_name, int mode, int expert);

extern int xlv_mgr_detach (
	int type, char *part_name, char *dst_name, int delete,
	int expert, int force);

extern int xlv_mgr_delete_object (char *object_name);

extern int xlv_mgr_set_nodename (char *nodename, char *object);

extern int xlv_mgr_online_ve (char *vename);
extern int xlv_mgr_offline_ve (char *vename);
extern int xlv_mgr_start_ve (__int64_t start, char *vename);

extern int xlv_mgr_rename(char *old, char *new);
extern int xlv_mgr_retype(int type, char *object);
extern int xlv_mgr_readonly_vol (char *name, int flag );


/*
 **************************************************************************
 * Utility procedures
 **************************************************************************
 */

/*
 * Scan all the disks and delete the XLV disk label.
 * Set *count to the number of XLV disk labels removed.
 *
 * Return XLV_MGR_STATUS_OKAY upon success. 
 */
extern int delete_all_labels (int *count);

extern int delete_one_label (char *device_name);

/*
 * Type of values for which get_val() prompts.
 */
#define VAL_COMMAND		1
#define VAL_OBJECT_NAME		2
#define VAL_SUBVOL_NUMBER	3
#define VAL_NEW_OBJECT_NAME	4
#define VAL_PLEX_NUMBER		5
#define VAL_CREATED_OBJECT_NAME	6
#define VAL_PART_NAME		7
#define VAL_VE_NUMBER		8
#define VAL_ADD_OBJECT		9
#define VAL_DESTROY_DATA	10
#define VAL_DELETE_LABELS	11
#define VAL_NODENAME		12
extern int get_val (char *buffer, int choice);

/*
 * display subvolume table
 */
extern int print_kernel_tab (int p_mode, int format, int sv_idx);

/*
 * display volume table
 */
extern int print_kernel_tabvol (int p_mode, int format, char *volname);

extern int print_labels (int p_mode, int secondary);
extern int print_one_label (char *device_name, int p_mode, int secondary);
extern int print_name (char *name, int p_mode);

extern void print_config (int expert);
extern void print_stat (char *sv_name, int verbose);
extern void print_error (int xlv_err);

extern void reset_stat (char *sv_name);
extern int parse_name (char *name, int type, xlv_mgr_cursor_t *cursor);

/* change mem pool parameters */
extern int change_mem_params(const int cmd, int slots, int grow, int miss);

/* enable|disable flags for: statistics gathering, failsafe mode */
int set_attr_flags(int dostat, int dofailsafe);

#define	FMT_LONG	1
#define	FMT_SHORT	2
extern int print_all_objects (int format, int p_mode);

extern int script_one (FILE *stream, char *name);
extern int script_all_objects (FILE *stream);

extern int xlv_vol_isfree (xlv_oref_t *oref, int *found);

extern int xlv_delete_vol (xlv_oref_t *oref);
extern int xlv_delete_plex (xlv_oref_t *oref, int freeit, int inplace);
extern int xlv_delete_ve (xlv_oref_t *oref, int freeit, int inplace);

extern int xlv_attach_plex (xlv_mgr_cursor_t *cursor, xlv_oref_t *p_oref);
extern int xlv_attach_ve (xlv_mgr_cursor_t *cursor, xlv_oref_t *ve_oref);

extern int xlv_detach_plex (
	xlv_mgr_cursor_t *cursor, char *dst_name, int delete, int force);
extern int xlv_detach_ve (
	xlv_mgr_cursor_t *cursor, char *dst_name, int delete, int force);


/*
 * check_plex_overlap() checks that the specified plex does not
 * have any volume elements that overlap with any volume elements
 * in the other plexes. Ensure is that corresponding volume elements
 * in the other plexes have the same start and end points.
 */
extern int check_plex_overlap (
	xlv_tab_subvol_t *svp,		/* subvolume */
	int		 pnum);		/* index of plex being checked */


/*
 * Given the xlv manager cursor, set the cursor oref to point to the
 * referenced volume element.
 *
 * On success, cursor->oref is set to the volume element
 * and XLV_MGR_STATUS_OKAY is returned.
 */
extern int xlv_ve_oref_from_cursor (xlv_mgr_cursor_t *cursor);


/*
 **************************************************************************
 * Routines to administer the object hash table.
 **************************************************************************
 */

/*
 * Add the given object to the object hash table.
 *
 * INPUT: This function does not consume the oref structure but the
 * referenced xlv object is consumed so the caller must not free the
 * actual object.
 */
extern void		add_object_to_table (xlv_oref_t *object);

/*
 * Find the object in the hash table with the given name.
 * If a specific type is given, search for the object of that type.
 * If the type is XLV_OBJ_TYPE_NONE then any object with that
 * name is okay.
 *
 * OUTPUT: Upon success a pointer to an oref identifying the object
 * is returned. The caller is responsible for freeing this oref structure.
 */
extern xlv_oref_t *find_object_in_table (char *name, int type, char *nodename);

/*
 * Remove the XLV object from the hash table.
 *
 * Return 0 on success and 1 on failure.
 *
 * Note: The oref removed from the hash table is free only if it is
 * not the same oref as the passed in <object>
 *
 * <logerror> - indicates whether or not errors are logged stderr.
 */
extern int	remove_object_from_table (xlv_oref_t *object, int logerror);

#endif /* __XLV_MGR_H__ */
