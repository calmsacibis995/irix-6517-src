/*
 * xlv_mgr_cmd.c
 *
 *      Module implementing the front end of xlv_mgr(1m).
 *      The tcl commands are implemented here.
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

#ident "$Revision: 1.28 $"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <bstring.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#include <ustat.h>

#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/xlv_attr.h>
#include <xlv_oref.h>
#include <sys/sysmacros.h>
#include <xlv_error.h>
#include <xlv_utils.h>
#include <sys/debug.h>

#include "xlv_mgr.h"

#define	XLV_MGR_INVAL_FLAG	"invalid command flag: "
#define	XLV_MGR_INVAL_OPTION	"invalid command option: "
#define	XLV_MGR_WRONG_ARG_COUNT	"wrong # args: should be: "

extern int has_plexing_license;
extern int has_plexing_support;

#if 0	/* fixed in recent tcl 7.6 sources */
/*
 * TCL procedures are prototyped in <tcl.h> but there is a bug
 * in the header which prevents this module from compiling cleanly.
 * So do the prototype here as a work-around.
 */
extern void Tcl_SetResult (
	Tcl_Interp *interp, char *string, Tcl_FreeProc *freeProc);

/*
 * Tcl_AppendResult() is not correctly prototyped in tcl.h.
 * This is basically a wrapper function around Tcl_AppendResult()
 * so we don't get lots of warning messages.
 */
void
append_interp_result (Tcl_Interp *interp, ...)
{
	va_list arg_ptr;
	void	*str;

	va_start (arg_ptr, interp);

	str = va_arg(arg_ptr, void *);
	while (str) {
		Tcl_AppendResult (interp, str, 0);
		str = va_arg(arg_ptr, void *);
	}

	va_end (arg_ptr);
}
#else
#define	append_interp_result	Tcl_AppendResult
#endif

#ifdef DEBUG
static int		init_called = 0;
#endif


/*
 * Returns the level of expertise as determined by the value of
 * the "expert" TCL variable.
 */
static int
_isexpert(Tcl_Interp *interp)
{
	char	*expert;
	int	expertise;

	expert = Tcl_GetVar(interp, XLV_MGR_VAR_SUSER, 0);

	if (expert && (TCL_OK == Tcl_GetInt(interp, expert, &expertise)))
		return (expertise);

	return (0);
}

static int
_isverbose(Tcl_Interp *interp)
{
	char	*verbose;
	int	value;

	verbose = Tcl_GetVar(interp, XLV_MGR_VAR_VERBOSE, 0);

	if (verbose && (TCL_OK == Tcl_GetInt(interp, verbose, &value)))
		return (value);
	
	return (0);
}


void
tclcmd_init (int readlabels)
{
#ifdef DEBUG
	ASSERT (init_called == 0);
	init_called = 1;
#endif
	if (readlabels)
		xlv_mgr_init ();
}



/*
 * **********  C O M M A N D S  **********
 *
 * Command procedures - one for every command in xlv_mgr.
 *
 * Each of these procedures are of type Tcl_CmdProc.
 *
 * We try to do as much error checking as possible here and have
 * the back-end routines (in xlv_mgr_int) do all the work.
 *
 * ***************************************
 */


/*
 * Process the vol command:
 *
 *	xlv_mgr> attach ve src_object dst_plex.N
 *	xlv_mgr> attach plex src_object dst_vol.{data,log,rt}
 *
 * 2.      Add a ve at the END of an existing plex. 
 * 3.      Add a plex to an existing volume.
 */
/*ARGSUSED*/
int
tclcmd_attach (
	ClientData	clientData,
	Tcl_Interp	*interp,
	int		argc,
	char		*argv[])
{
	char	usage_attach[] = "attach {plex,ve} object_name dst";
	char	usage_attach_ve[] = "attach ve object_name dst";
	char	*src_name, *dst_name;
	char	c;
	int	idx;
	int	status;
	int	type;

	/*
	 * By TCL convention:
	 * 	argv[0] = command name.
	 * 	argv[argc] = NULL.
	 */

	ASSERT (init_called);

	/*
	 * Parse flags.
	 */
	for (idx=1; (idx < argc) && (argv[idx][0] == '-'); idx++) {
		if (0 == strcmp(argv[idx], "-")) {
			/* XXX Any attach options? */
		} else {
			append_interp_result (
				interp, XLV_MGR_INVAL_FLAG,
				argv[idx], (char *)NULL);
			return (TCL_ERROR);
		}
	}

	/*
	 * Check number of arguments.
	 */
	if (argc != idx+3) {
		append_interp_result (
			interp, XLV_MGR_WRONG_ARG_COUNT,
			(has_plexing_license) ? usage_attach : usage_attach_ve,
			(char *)NULL);
		return (TCL_ERROR);
	}

	c = argv[idx][0];
	if (c == 'p' && !strcmp(argv[idx], "plex") && has_plexing_license) {
		type = XLV_OBJ_TYPE_PLEX;
	} else if (c == 'v' && !strcmp(argv[idx], "ve")) {
		type = XLV_OBJ_TYPE_VOL_ELMNT;
	} else {
		append_interp_result (
			interp, XLV_MGR_INVAL_OPTION, argv[idx], (char *)NULL);
		return (TCL_ERROR);
	}

	src_name = argv[idx+1];
	dst_name = argv[idx+2];

	status = xlv_mgr_attach (type, src_name, dst_name,
				 ATTACH_MODE_APPEND, _isexpert(interp));

	if (status != XLV_MGR_STATUS_OKAY) {
		append_interp_result (
			interp, "Failed to attach ",
			src_name, " to ", dst_name, (char *)NULL);
		return (TCL_ERROR);
	}

	return (TCL_OK);

} /* end of tclcmd_attach() */


/*
 * Process the vol command:
 *
 *	xlv_mgr> insert ve src_object dst_vol.{data,log,rt}.N.N
 *
 * 1.      Add a ve to an existing plex. 
 */
/*ARGSUSED*/
int
tclcmd_insert (
	ClientData	clientData,
	Tcl_Interp	*interp,
	int		argc,
	char		*argv[])
{
	char	usage_insert[] = "insert ve ve_object dst_object.nnn";
	char	*src_name, *dst_name;
	char	c;
	int	idx;
	int	status;
	int	type;

	ASSERT (init_called);

	for (idx=1; (idx < argc) && (argv[idx][0] == '-'); idx++) {
		if (0 == strcmp(argv[idx], "-")) {
			/* XXX Any insert options? */
		} else {
			append_interp_result (
				interp, XLV_MGR_INVAL_FLAG,
				argv[idx], (char *)NULL);
			return (TCL_ERROR);
		}
	}

	/*
	 * Check number of arguments.
	 */
	if (argc != idx+3) {
		append_interp_result (
			interp, XLV_MGR_WRONG_ARG_COUNT,
			usage_insert, (char *)NULL);
		return (TCL_ERROR);
	}

	c = argv[idx][0];
	/*
	 * XXX Inserting a plex into a subvolume is not supported yet.
	 */
	if (c == 'v' && !strcmp(argv[idx], "ve")) {
		type = XLV_OBJ_TYPE_VOL_ELMNT;
	} else {
		append_interp_result (
			interp, XLV_MGR_INVAL_OPTION, argv[idx], (char *)NULL);
		return (TCL_ERROR);
	}

	src_name = argv[idx+1];
	dst_name = argv[idx+2];

	status = xlv_mgr_attach (type, src_name, dst_name,
				 ATTACH_MODE_INSERT, _isexpert(interp));

	if (status != XLV_MGR_STATUS_OKAY) {
		append_interp_result (
			interp, "Failed to attach ",
			src_name, " to ", dst_name, (char *)NULL);
		return (TCL_ERROR);
	}

	return (TCL_OK);

} /* end of tclcmd_insert() */


/*
 *	xlv_mgr> detach ve vol.data.p_no.ve_no dst_object
 *	xlv_mgr> detach ve plex_name.ve_no dst_object
 *	xlv_mgr> detach plex vol.data.p_no dst_object
 *
 * 11.     Detach a ve from an existing plex.
 * 12.     Detach a plex from an existing volume.
 */
/*ARGSUSED*/
int
tclcmd_detach (
	ClientData	clientData,
	Tcl_Interp	*interp,
	int		argc,
	char		*argv[])
{
	char	usage_detach[] = "detach {plex,ve} part_name dst_object";
	char	usage_detach_ve[] = "detach ve part_name dst_object";
	char	*src_name, *dst_name;
	char	c;
	int	idx;
	int	status;
	int	type;
	int	force = 0;

	ASSERT (init_called);

	/*
	 * Parse flags.
	 */
	for (idx=1; (idx < argc) && (argv[idx][0] == '-'); idx++) {
		if (0 == strcmp(argv[idx], "-force")) {
			force = 1;
		} else {
			append_interp_result (
				interp, XLV_MGR_INVAL_FLAG,
				argv[idx], (char *)NULL);
			return (TCL_ERROR);
		}
	}

	/*
	 * Check number of arguments.
	 */
	if (argc != idx+3) {
		append_interp_result (
			interp, XLV_MGR_WRONG_ARG_COUNT,
			(has_plexing_license) ? usage_detach : usage_detach_ve,
			(char *)NULL);
		return (TCL_ERROR);
	}

	c = argv[idx][0];
	if (c == 'p' && !strcmp(argv[idx], "plex")) {
		/*
		 * Don't need to check if there is a plexing license
		 * because you must be able to go from a mirrored to
		 * a single plex volume.
		 */
		type = XLV_OBJ_TYPE_PLEX;
	} else if (c == 'v' && !strcmp(argv[idx], "ve")) {
		type = XLV_OBJ_TYPE_VOL_ELMNT;
	} else {
		append_interp_result (
			interp, XLV_MGR_INVAL_OPTION, argv[idx], (char *)NULL);
		return (TCL_ERROR);
	}

	src_name = argv[idx+1];
	dst_name = argv[idx+2];

	status = xlv_mgr_detach (
			type, src_name, dst_name, 0, _isexpert(interp), force);

	if (status != XLV_MGR_STATUS_OKAY) {
		append_interp_result (
			interp, "Failed to detach and rename ",
			src_name, " to ", dst_name, (char *)NULL);
		return (TCL_ERROR);
	}

	return (TCL_OK);

} /* end of tclcmd_detach() */


#ifdef REMOVE_COMMAND
/*
 *	xlv_mgr> remove ve vol.data.p_no.ve_no
 *	xlv_mgr> remove ve plex_name.ve_no
 *	xlv_mgr> remove plex vol.data.p_no
 *
 * 21.  Remove a ve from an existing plex.
 * 22.  Remove a plex from an existing volume. 
 */
int
tclcmd_remove (
	ClientData	clientData,
	Tcl_Interp	*interp,
	int		argc,
	char		*argv[])
{
	char	usage_remove[] = "remove {plex,ve} name";
	char	usage_remove_ve[] = "remove ve name";
	char	*src_name;
	char	c;
	int	idx;
	int	status;
	int	type;

	ASSERT (init_called);

	/*
	 * Parse flags.
	 */
	for (idx=1; (idx < argc) && (argv[idx][0] == '-'); idx++) {
		if (0 == strcmp(argv[idx], "-")) {
			/* XXX Any remove options? */
		} else {
			append_interp_result (
				interp, XLV_MGR_INVAL_FLAG,
				argv[idx], (char *)NULL);
			return (TCL_ERROR);
		}
	}

	/*
	 * Check number of arguments.
	 */
	if (argc != idx+2) {
		append_interp_result (
			interp, XLV_MGR_WRONG_ARG_COUNT,
			(has_plexing_license) ? usage_remove : usage_remove_ve,
			(char *)NULL);
		return (TCL_ERROR);
	}

	c = argv[idx][0];
	if (c == 'p' && !strcmp(argv[idx], "plex") && has_plexing_license) {
		type = XLV_OBJ_TYPE_PLEX;
	} else if (c == 'v' && !strcmp(argv[idx], "ve")) {
		type = XLV_OBJ_TYPE_VOL_ELMNT;
	} else {
		append_interp_result (
			interp, XLV_MGR_INVAL_OPTION, argv[idx], (char *)NULL);
		return (TCL_ERROR);
	}

	src_name = argv[idx+1];

	status = xlv_mgr_detach (
			type, src_name, NULL, 1, _isexpert(interp), 0);

	if (status != XLV_MGR_STATUS_OKAY) {
		append_interp_result (
			interp, "Failed to remove ", src_name, (char *)NULL);
		return (TCL_ERROR);
	}

	return (TCL_OK);

} /* end of tclcmd_remove() */
#endif /* REMOVE_COMMAND */


/*
 *	xlv_mgr> delete object <name>
 *	xlv_mgr> delete all_labels
 *	xlv_mgr> delete label dks?d?vh
 *
 * 31.     Delete an object.
 * 32.     Delete all XLV disk labels. 
 */
/*ARGSUSED*/
int
tclcmd_delete (
	ClientData	clientData,
	Tcl_Interp	*interp,
	int		argc,
	char		*argv[])
{
	char	usage_delete[] = "delete object ?name?";
	char	usage_expert_delete[] = "delete all_labels, object ?name?, label dks?d?vh";
	char	*opt_usage = NULL;
	char 	c, *opt;
	char	*targetname;
	int	idx;
	int	count;
	int	retval = TCL_OK;
	int	status = XLV_MGR_STATUS_OKAY;
	int	expert = _isexpert(interp);

	ASSERT (init_called);

	for (idx=1; (idx < argc) && (argv[idx][0] == '-'); idx++) {
		if (0 == strcmp(argv[idx], "-")) {
			/* XXX Any delete options? */
		} else {
			append_interp_result (
				interp, XLV_MGR_INVAL_FLAG,
				argv[idx], (char *)NULL);
			return (TCL_ERROR);
		}
	}

	if (argc <= idx)
		goto arg_error;

	c = argv[idx][0];
	opt = argv[idx];

	targetname = argv[idx+1];	/* may not be valid */

	if ((c == 'o') && (0 == strcmp(opt, "object"))) {
		/*
		 * Delete a specific object.
		 */
		if (argc != idx+2) {
			opt_usage = "should be \"object <object_name>\"";
			goto opt_arg_error;
		}

		if (strlen(targetname) > XLV_NODENAME_LEN) {
			append_interp_result ( interp,
				"\"", targetname, "\" is too long. ",
				(char *)NULL);
			status = XLV_MGR_STATUS_FAIL;
		} else {
			status = xlv_mgr_delete_object(targetname);
		}

	} else if (expert && (c == 'a') && (0 == strcmp(opt, "all_labels"))) {
		/*
		 * Delete all XLV disk labels.
		 */
		status = delete_all_labels(&count);

	} else if (expert && (c == 'l') && (0 == strcmp(opt, "label"))) {
		/*
		 * Delete the XLV disk label on a specific disk.
		 */
		if (argc != idx+2) {
			opt_usage = "should be \"label <disk_name>\"";
			goto opt_arg_error;
		}
		status = delete_one_label(targetname);

	} else {
		append_interp_result (
			interp, "invalid option \"", opt, "\": should be: ",
			(expert) ? usage_expert_delete : usage_delete,
			(char *)NULL);
		retval = TCL_ERROR;
	}

	if (XLV_MGR_STATUS_OKAY != status) {
		append_interp_result (interp, "Delete failed.", (char *)NULL);
		retval = TCL_ERROR;
	}

	return (retval);

arg_error:
	append_interp_result (
		interp, XLV_MGR_WRONG_ARG_COUNT, usage_delete, (char *)NULL);
	return (TCL_ERROR);

opt_arg_error:
	append_interp_result (
		interp, "missing option argument: ", opt_usage, (char *)NULL);
	return (TCL_ERROR);

} /* end of tclcmd_delete() */


/*
 *	xlv_mgr> change nodename <name> <object> [object]*
 *	xlv_mgr> change offline <vol.subvol.plex_no.ve_no>
 *	xlv_mgr> change online <vol.subvol.plex_no.ve_no>
 *	xlv_mgr> change ve_state <state> <object> [object]*
 *	xlv_mgr> change ve_start <start addr> <object>
 *	xlv_mgr> change objectname <old> <new>
 *	xlv_mgr> change name <old> <new>
 *	xlv_mgr> change stat on|off
 *	xlv_mgr> change type [ve|plex|vol] object
 *	xlv_mgr> change plexmem <max pool i/o> <growth %> <miss %>
 *	xlv_mgr> change vemem <max pool i/o> <growth %> <miss %>
 *	xlv_mgr> change readonly on|off <vol>
 *	xlv_mgr> change failsafe on|off
 */
/*ARGSUSED*/
int
tclcmd_change (
	ClientData	clientData,	/* Not used */
	Tcl_Interp	*interp,
	int		argc,
	char		*argv[])
{
	char	usage_change[] ="change online ?ve?, "
				"offline ?ve?, "
				"name ?oldname? ?newname?, "
				"nodename ?name? ?object_list?, "
				"objectname ?oldname? ?newname?, "
				"type ?newtype? ?object?, "
				"ve_start ?address? ?object?, "
				"readonly on|off ?vol?";
	char	usage_plexmem[] =", plexmem ?max_pool_i/os? ?growth%? ?miss%?";
	char	usage_vemem[] =", vemem ?max_pool_i/os? ?growth%? ?miss%?";
	char	usage_do_stat[] =", stat on|off";
	char	usage_do_failsafe[] =", failsafe on|off";
	char	c, *opt;
	int	expert = _isexpert(interp);
	int	idx;
	int	retval = TCL_OK;

        ASSERT (init_called);

	if (argc < 2)
		goto arg_error;

	idx = 1;
	c = argv[idx][0];
	opt = argv[idx];

	if ((c == 'n') && (0 == strcmp(opt, "nodename"))) {
		char	*nodename;

		if (argc < 4)
			goto arg_error;

		nodename = argv[++idx];

		if (strlen(nodename) > XLV_NODENAME_LEN) {
			append_interp_result (
				interp, "invalid option agrument \"",
				nodename, "\"", (char *)NULL);
			retval = TCL_ERROR;
		}
		else {
			char *status_str;
			char *obj;

			for (idx++ ; idx < argc; idx++) {
				obj = argv[idx];
				if (xlv_mgr_set_nodename(nodename, obj)) {
					retval = TCL_ERROR;
					status_str = "failed";
				} else {
					status_str = "done";
				}
				append_interp_result (
					interp, "set node name \"",
					nodename, "\" for object \"", obj,
					"\" ", status_str, "\n", (char *)NULL);
			}
		}

	} else if (((c == 'n') && !strcmp(opt, "name"))
		|| ((c == 'o') && !strcmp(opt, "objectname"))) {
			char *old;
			char *new;

		if (argc != 4)
			goto arg_error;

		old = argv[++idx];
		new = argv[++idx];

		if (xlv_mgr_rename(old, new)) {
			retval = TCL_ERROR;
		}

	} else if ((c == 't') && (0 == strcmp(opt, "type"))) {
		int	typeval;
		char	*typename;
		char	*objname;

		if (argc != 4)
			goto arg_error;

		/* change type {ve,plex,vol} object */
		typename = argv[++idx];
		objname = argv[++idx];
		if (typename[0] == 'p' && !strcmp(typename, "plex")) {
			typeval = XLV_OBJ_TYPE_PLEX;
		} else if (typename[0] == 'v' && !strcmp(typename, "ve")) {
			typeval = XLV_OBJ_TYPE_VOL_ELMNT;
		} else if (typename[0] == 'v' && !strcmp(typename, "vol")) {
			typeval = XLV_OBJ_TYPE_VOL;
		} else {
			append_interp_result (
				interp, XLV_MGR_INVAL_OPTION, typename,
				(char *)NULL);
			retval = TCL_ERROR;
		}

		if (retval != TCL_OK || xlv_mgr_retype(typeval, objname)) {
			retval = TCL_ERROR;
		}

	} else if ((c == 'o') && (0 == strcmp(opt, "online"))) {
		if (argc != 3)
			goto arg_error;

		if (xlv_mgr_online_ve(argv[++idx])) {
			retval = TCL_ERROR;
		}

	} else if ((c == 'o') && (0 == strcmp(opt, "offline"))) {
		if (argc != 3)
			goto arg_error;

		if (xlv_mgr_offline_ve(argv[++idx])) {
			retval = TCL_ERROR;
		}

        } else if  ((c == 'r') && (0 == strcmp(opt, "readonly"))) {
		int readonly;

		if (argc != 4)
			goto arg_error;

		if (!strcmp(argv[1+idx], "on")) {
			readonly = 1;
		} else if (!strcmp(argv[1+idx], "off")) {
			readonly = 0;
		} else {
			append_interp_result (
				interp, "wrong args: should be: readonly on|off <vol>",
				(char *)NULL);
			return (retval = TCL_ERROR);
		}
		if (xlv_mgr_readonly_vol( argv[idx+2], readonly)) {
			retval = TCL_ERROR;
		}

	} else if (expert && (c == 's') &&
			(!strcmp(opt, "stat") || !strcmp(opt, "stats"))) {
		int dostat;

		if (argc != 3)
			goto arg_error;

		if (!strcmp(argv[1+idx], "on")) {
			dostat = 1;
		} else if (!strcmp(argv[1+idx], "off")) {
			dostat = 0;
		} else {
			append_interp_result (
				interp, "wrong args: should be: stat on|off",
				(char *)NULL);
			return (retval = TCL_ERROR);
		}
		if (set_attr_flags(dostat, -1)) {
			retval = TCL_ERROR;
		}

	} else if (expert && (c == 'f') && (0 == strcmp(opt, "failsafe"))) {
		int dofailsafe;

		if (argc != 3)
			goto arg_error;

		if (!strcmp(argv[1+idx], "on")) {
			dofailsafe = 1;
		} else if (!strcmp(argv[1+idx], "off")) {
			dofailsafe = 0;
		} else {
			append_interp_result (
				interp,
				"wrong args: should be: failsafe on|off",
				(char *)NULL);
			return (retval = TCL_ERROR);
		}
		if (set_attr_flags(-1, dofailsafe)) {
			retval = TCL_ERROR;
		}

	} else if ((c == 'v') && (0 == strcmp(opt, "ve_state"))) {
		/* XXX determine ve state value */
		append_interp_result (
			interp, "unimplemented option \"", opt, "\"",
			(char *)NULL);
		retval = TCL_ERROR;

	} else if ((c == 'v') && (0 == strcmp(opt, "ve_start"))) {
		__int64_t start_blkno;
		char *name;

		if (argc != 4) 
			goto arg_error;

		start_blkno = strtoll(argv[++idx], NULL, 0);
		if (start_blkno < 0LL) {
			append_interp_result (
				interp, "negative start blocks aren't allowed",
				(char *)NULL);
			retval = TCL_ERROR;
		} else {
			name = argv[++idx];

			if (xlv_mgr_start_ve(start_blkno, name)) {
				retval = TCL_ERROR;
			}
		}

	} else if (expert && (c == 'v') && (0 == strcmp(opt, "vemem"))) {
		int slots, grow, miss;

		if (argc != 5) 
			goto arg_error;

		slots = strtol(argv[++idx], NULL, 0);
		grow = strtol(argv[++idx], NULL, 0);
		miss = strtol(argv[++idx], NULL, 0);

		if (change_mem_params(XLV_ATTR_CMD_VEMEM, slots, grow, miss))
			retval = TCL_ERROR;

	} else if (expert && has_plexing_support && 
		  (c == 'p') && (0 == strcmp(opt, "plexmem"))) {
		int slots, grow, miss;

		if (argc != 5) 
			goto arg_error;

		slots = strtol(argv[++idx], NULL, 0);
		grow = strtol(argv[++idx], NULL, 0);
		miss = strtol(argv[++idx], NULL, 0);

		if (change_mem_params(XLV_ATTR_CMD_PLEXMEM, slots, grow, miss))
			retval = TCL_ERROR;

	} else {
		append_interp_result (
			interp, "invalid option \"", opt, "\": should be: ",
			usage_change,
			(expert && has_plexing_support) ? usage_plexmem : "",
			(expert ? usage_vemem : ""), 
			(expert ? usage_do_stat : ""),
			(expert ? usage_do_failsafe : ""),
			(char *)NULL);

		retval = TCL_ERROR;
	}

	return (retval);

arg_error:
	append_interp_result (interp, XLV_MGR_WRONG_ARG_COUNT,
		usage_change, 
		(expert && has_plexing_support) ? usage_plexmem : "", 
		(expert ? usage_vemem : ""),
		(expert ? usage_do_stat : ""),
		(expert ? usage_do_failsafe : ""),
		(char *)NULL);
	return (TCL_ERROR);

} /* end of tclcmd_change() */


/*
 *	xlv_mgr> script [-write <filename>] all[_objects]
 *	xlv_mgr> script [-write <filename>] object <name>
 */
/*ARGSUSED*/
int
tclcmd_script (
	ClientData	clientData,		/* not used */
	Tcl_Interp	*interp,
	int		argc,
	char		*argv[])
{
	char	usage_script[] = "script all, object ?name?"; 
	char	c, *opt;
	int	idx;
	int	retval;
	FILE	*stream = stdout;
	char	*filename = NULL;
	char	msg[100];

        ASSERT (init_called);

	for (idx=1; (idx < argc) && (argv[idx][0] == '-'); idx++) {
		if (0 == strcmp(argv[idx], "-write")) {
			filename = argv[++idx];
			if (argc <= idx) {
				goto arg_error;
			}

			stream = fopen(filename, "wb");
			if (NULL == stream) {
				sprintf(msg, "Cannot write to %s", filename);
				perror(msg);
				return (TCL_ERROR);
			}

		} else {
			append_interp_result (
				interp, XLV_MGR_INVAL_FLAG,
				argv[idx], (char *)NULL);
			return (TCL_ERROR);
		}
	}

	if (argc <= idx)
		goto arg_error;

	c = argv[idx][0];
	opt = argv[idx];

	if ((c == 'a') && 
		   ((!strcmp(opt, "all")) || (!strcmp(opt, "all_objects")))) {
		if (argc != idx+1)
			goto arg_error;
		retval = script_all_objects(stream);

	} else if ((c == 'o') && (0 == strcmp(opt, "object"))) {
		if (argc > idx+2) {
			goto arg_error;
		} else if (argc < idx+2) {
			append_interp_result (
				interp, "missing option argument: ",
				"should be \"object <name>\"", (char *)NULL);
			return (TCL_ERROR);
		}
		retval = script_one(stream, argv[idx+1]);

	} else {				/* unknown option */
		append_interp_result (
			interp, "invalid option \"", opt, "\": should be: ",
			usage_script, (char *)NULL);
		return (TCL_ERROR);
	}

	if (filename) {
		fclose(stream);
	}

	return ((retval == XLV_MGR_STATUS_FAIL) ? TCL_ERROR : TCL_OK);

arg_error:
	append_interp_result (
		interp, XLV_MGR_WRONG_ARG_COUNT, usage_script, (char *)NULL);
	return (TCL_ERROR);

} /* end of tclcmd_script() */


/*
 *	xlv_mgr> show [-verbose][-short] kernel [name|idx]
 *	xlv_mgr> show [-verbose] label [vh]*
 *	xlv_mgr> show [-verbose] config
 *	xlv_mgr> show [-long][-short] [-verbose] all[_objects]
 *	xlv_mgr> show [-verbose] [object] <name>
 *	xlv_mgr> show [-verbose] stat [subvolume]
 */
/*ARGSUSED*/
int
tclcmd_show (
	ClientData	clientData,		/* not used */
	Tcl_Interp	*interp,
	int		argc,
	char		*argv[])
{
	char	usage_show[] =
"show [-verbose][-long][-short][-time] all_objects, config, kernel, labels, object ?name_list?, ?objectname?, stat [?subvolume?]"; 

	char	c, *opt;
	int	verbose, expert;
	int	idx;
	int	p_format = 0;
	int	p_mode = XLV_PRINT_BASIC;
	int	do_subvol = 0;

        ASSERT (init_called);

	verbose = _isverbose(interp);
	expert = _isexpert(interp);

	for (idx=1; (idx < argc) && (argv[idx][0] == '-'); idx++) {
		if (0 == strcmp(argv[idx], "-long")) {
			p_format = FMT_LONG;
		} else if (0 == strcmp(argv[idx], "-short")) {
			p_format = FMT_SHORT;
		} else if (0 == strcmp(argv[idx], "-verbose")) {
			verbose = 1;
		} else if (0 == strcmp(argv[idx], "-time")) {
			p_mode = p_mode | XLV_PRINT_TIME;
		} else if (0 == strncmp(argv[idx], "-subvolume", 5)) {
			do_subvol = 1;
		} else {
			append_interp_result (
				interp, XLV_MGR_INVAL_FLAG,
				argv[idx], (char *)NULL);
			return (TCL_ERROR);
		}
	}
	if (verbose)
		p_mode = XLV_PRINT_ALL;

	if (argc <= idx)
		goto arg_error;

	c = argv[idx][0];
	opt = argv[idx];

	if ((c == 'k') && (0 == strcmp(opt, "kernel"))) {
		/*
		 * show [-short] kernel [name]
		 */
		char	*name;

		if (p_format == 0)
			p_format = FMT_LONG;
		name = (argc == idx+1) ? NULL : argv[++idx];
		if (do_subvol) {
			print_kernel_tab (
				p_mode, p_format, name ? atoi(name) : -1);
		} else {
			print_kernel_tabvol (p_mode, p_format, name);
		}

	} else if ((c == 'l') &&
		(!strcmp(opt, "label") || !strcmp(opt, "labels"))) {
		/*
		 * show [-long] label[s]
		 */
		int	second = (p_format == FMT_LONG) ? 1 : 0;

		if (argc != idx+1) {
			while (++idx < argc) {
				print_one_label (argv[idx], p_mode, second);
			}
		} else {
			print_labels (p_mode, second);
		}

	} else if ((c == 'a') && 
		   (!strcmp(opt, "all") || !strcmp(opt, "all_objects"))) {
		/*
		 * show [-long][-verbose] all
		 */
		if (argc != idx+1)
			goto arg_error;
		if (verbose) {
			p_format = FMT_LONG;
		} else if (p_format == 0) {
			p_format = FMT_SHORT;
		}
		print_all_objects (p_format, p_mode);

	} else if ((c == 'o') &&
		(!strcmp(opt, "object") || !strcmp(opt, "objects"))) {
		/*
		 * show [-verbose] object <name>
		 */
		if (argc < idx+2) {
			append_interp_result (
				interp, "missing option argument: ",
				"should be \"object <name>\"", (char *)NULL);
			return (TCL_ERROR);
		} else {
			int count = 0;
			while (++idx < argc) {
				if (count++) printf("\n");
				print_name(argv[idx], p_mode);
			}
		}

	} else if ((c == 'c') && (0 == strcmp(opt, "config"))) {
		/*
		 * show config
		 */
		if (argc != idx+1)
			goto arg_error;
		print_config(expert);

	} else if ((c == 's') &&
		(!strcmp(opt, "stat") || !strcmp(opt, "stats"))) {
		/*
		 * show stat
		 */
		if (argc == idx+1) {
			/* show statistics for all subvolumes */
			print_stat(NULL, verbose);
		} else {
			/* show specific */
			while (++idx < argc) {
				print_stat(argv[idx], verbose);
			}
		}

	} else {				/* unknown option */
		/*
		 * Can this be an object name? Try displaying it.
		 */
		if ((argc == idx+1) && (0 == print_name(argv[idx], p_mode))) {
			/* must have been a "show objectname" command */
			return (TCL_OK);
		}
		append_interp_result (
			interp, "invalid option \"", opt, "\": should be: ",
			usage_show, (char *)NULL);
		return (TCL_ERROR);
	}

	return (TCL_OK);

arg_error:
	append_interp_result (
		interp, XLV_MGR_WRONG_ARG_COUNT, usage_show, (char *)NULL);
	return (TCL_ERROR);

} /* end of tclcmd_show() */


/*
 *	xlv_mgr> quit
 */
/*ARGSUSED0*/
int
tclcmd_quit (
	ClientData	clientData,		/* not used */
	Tcl_Interp	*interp,
	int		argc,			/* not used */
	char		*argv[])		/* not used */
{
        ASSERT (init_called);

	Tcl_SetResult (interp, "end", TCL_VOLATILE);

	return (TCL_OK);
}


/*
 *	xlv_mgr> sh
 */
/*ARGSUSED0*/
int
tclcmd_shell (
	ClientData	clientData,		/* not used */
	Tcl_Interp	*interp,		/* not used */
	int		argc,			/* not used */
	char		*argv[])		/* not used */
{
        ASSERT (init_called);
	system("/sbin/sh");
	return (TCL_OK);
}


/*
 *	xlv_mgr> reset
 *	xlv_mgr> reset stat [subvolume]
 */
/*ARGSUSED*/
int
tclcmd_reset (
	ClientData	clientData,		/* not used */
	Tcl_Interp	*interp,
	int		argc,
	char		*argv[])
{
	char	usage_reset[] = "reset";
	char	usage_stat[] = " [stat [?subvolume?]]";
	char	c, *opt;
	int	idx;
	int	expert = _isexpert(interp);

#ifdef DEBUG
	init_called = 1;
#endif
	for (idx=1; (idx < argc) && (argv[idx][0] == '-'); idx++) {
		if (0 == strcmp(argv[idx], "-")) {
			/* XXX Any reset options? */
		} else {
			append_interp_result (
				interp, XLV_MGR_INVAL_FLAG,
				argv[idx], (char *)NULL);
			return (TCL_ERROR);
		}
	}

	if (argc == 1) {
		/* simple reset command */
		xlv_mgr_init();
		return(TCL_OK);
	}

	c = argv[idx][0];
	opt = argv[idx];

	if (expert && (c == 's') &&
		(!strcmp(opt, "stat") || !strcmp(opt, "stats"))) {
		/*
		 * reset counters
		 */
		if (argc == idx+1) {
			/* reset statistics for all subvolumes */
			reset_stat(NULL);
		} else {
			/* specific subvolume */
			while (++idx < argc) {
				reset_stat(argv[idx]);
			}
		}

	} else {
		goto arg_error;
	}
	return (TCL_OK);

arg_error:
	append_interp_result (
		interp, XLV_MGR_WRONG_ARG_COUNT,
		usage_reset, (expert) ? usage_stat : "",
		(char *)NULL);
	return (TCL_ERROR);

} /* end of tclcmd_reset */



/*
 * HELP strings
 */
const char *STR_SHOW_ALL =
"show [-long][-verbose] all  - Display all known objects.";

const char *STR_SHOW_KERNEL =
"show [-verbose] kernel [?name?] - Display the kernel volume table.";

const char *STR_SHOW_LABEL =
"show [-long] [-verbose] labels [dks?d?vh]\n"
"                            - Display XLV disk labels. The long option\n"
"                              display the secondary label.";

const char *STR_SHOW_CONFIG =
"show config                 - Display XLV software configuration.";

const char *STR_SHOW_OBJECT =
"show [-verbose] object ?name? - Display named object.";

const char *STR_SHOW_STAT =
"show stat [?subvol?]          - Display subvolume(s) statistics.";

const char *STR_ATTACH_VE =
"attach ve ?src? ?dst_plex?  - Append ve object \"src\" to \"dst_plex\".";

const char *STR_ATTACH_PLEX =
"attach plex ?src? ?dst_sv?  - Append plex object \"src\" to \"dst_sv\".";

const char *STR_INSERT_VE =
"insert ve ?src? ?dst.N?     - Insert ve object \"src\" into \"dst\" object.";

const char *STR_DETACH_VE =
"detach [-force] ve ?plex.N? ?name?\n"
"                            - Remove specified ve from its parent object\n"
"                              and save it as \"name\".";

const char *STR_DETACH_PLEX =
"detach [-force] plex ?subvol.N? ?name?\n"
"                            - Remove specified plex from its parent object\n"
"                              and save it as \"name\".";

const char *STR_DELETE_ALL_LABELS =
"delete all_labels           - Delete all XLV disk labels.";

const char *STR_DELETE_LABEL =
"delete label dks?d?vh       - Delete XLV disk label from the volume header.";

const char *STR_DELETE_OBJECT =
"delete object ?name?        - Delete the named object.";

const char *STR_CHANGE_NAME =
"change name ?oldname? ?newname?  - Rename object \"oldname\" to \"newname\".";

const char *STR_CHANGE_NODENAME =
"change nodename ?name? ?object?  - Change the nodename associated with object.";

const char *STR_CHANGE_TYPE =
"change type ve|plex|vol ?object? - Change the type of the object.";

const char *STR_CHANGE_VE_STATE =
"change online|offline ?ve?  - Take the specified ve online|offline.";

const char *STR_CHANGE_STAT =
"change stat on|off          - Enable|Disable statistics gathering.";

const char *STR_CHANGE_VE_START =
"change ve_start ?start_blk? ?ve? - Change the standalone ve start block.";

const char *STR_CHANGE_PLEXMEM = 
"change plexmem ?max pool i/os? ?growth %? ?maximum miss %?\n"
"\t\t\t\t - Change the plex buffer pool parameters.";

const char *STR_CHANGE_VEMEM =
"change vemem ?max pool i/os? ?growth %? ?maximum miss %?\n"
"\t\t\t\t - Change the volume element buffer pool parameters.";

const char *STR_RESET =
"reset                       - Reset data; reread all disk labels.";

const char *STR_RESET_STAT =
"reset stat [?subvol?]       - Clear the counters for the subvolume(s).";

const char *STR_SCRIPT =
"script [-write ?filename?] object ?name?\n"
"script [-write ?filename?] all\n"
"                            - Generate the required xlv_make(1m) commands to\n"
"                              create the named object or all objects.";

const char *STR_SH =
"sh                          - Fork a shell.";

const char *STR_HELP =
"help or ?                   - Display this help message.";

const char *STR_QUIT =
"quit                        - Terminate session.";


/*
 * 	xlv_mgr> help
 */
/*ARGSUSED0*/
int
tclcmd_help (
	ClientData	clientData,		/* not used */
	Tcl_Interp	*interp,		/* not used */
	int		argc,			/* not used */
	char		*argv[])		/* not used */
{
	char	c, *opt;
	int	idx;
	int	expert = _isexpert(interp);
	int	help_mask = 0;

#define	HELP_ATTACH	0x0001
#define	HELP_CHANGE	0x0002
#define	HELP_DELETE	0x0004
#define	HELP_DETACH	0x0008
#define	HELP_HELP	0x0010
#define	HELP_INSERT	0x0020
#define	HELP_QUIT	0x0040
#define	HELP_RESET	0x0080
#define	HELP_SCRIPT	0x0100
#define	HELP_SHOW	0x0200
#define	HELP_MISC	0x8000
#define	HELP_MASK	0xFFFF

	ASSERT (init_called);

	if (0 != mypid) {
		/*
		 * Not super user so functionality is limited.
		 */
		printf("%s\n", STR_SHOW_KERNEL);
		printf("%s\n", STR_SHOW_CONFIG);
		printf("%s\n", STR_SH);
		printf("%s\n", STR_HELP);
		printf("%s\n", STR_QUIT);
		return (TCL_OK);
	}


	for (idx=1; idx < argc; idx++) {
		c = argv[idx][0];
		opt = argv[idx];
		if ((c == 'a') && 0 == strcmp(opt, "attach")) {
			help_mask |= HELP_ATTACH;
		} else if ((c == 'c') && 0 == strcmp(opt, "change")) {
			help_mask |= HELP_CHANGE;
		} else if ((c == 'd') && 0 == strcmp(opt, "delete")) {
			help_mask |= HELP_DELETE;
		} else if ((c == 'd') && 0 == strcmp(opt, "detach")) {
			help_mask |= HELP_DETACH;
		} else if ((c == 'h') && 0 == strcmp(opt, "help")) {
			help_mask |= HELP_HELP;
		} else if ((c == 'i') && 0 == strcmp(opt, "insert")) {
			help_mask |= HELP_INSERT;
		} else if ((c == 'q') && 0 == strcmp(opt, "quit")) {
			help_mask |= HELP_QUIT;
		} else if ((c == 'r') && 0 == strcmp(opt, "reset")) {
			help_mask |= HELP_RESET;
		} else if ((c == 's') && 0 == strcmp(opt, "script")) {
			help_mask |= HELP_SCRIPT;
		} else if ((c == 's') && 0 == strcmp(opt, "show")) {
			help_mask |= HELP_SHOW;
		}
	}

	if (help_mask == 0) {
		help_mask = HELP_MASK;
	}

	/* show command
	 */
	if (help_mask & HELP_SHOW) {
		printf("%s\n", STR_SHOW_ALL);
		printf("%s\n", STR_SHOW_KERNEL);
		printf("%s\n", STR_SHOW_LABEL);
		printf("%s\n", STR_SHOW_CONFIG);
		printf("%s\n", STR_SHOW_OBJECT);
		printf("%s\n", STR_SHOW_STAT);
	}

	/* attach command
	 */
	if (help_mask & HELP_ATTACH) {
		printf("%s\n", STR_ATTACH_VE);
		if (has_plexing_license) {
			printf("%s\n", STR_ATTACH_PLEX);
		}
	}

	/* insert command
	 */
	if (help_mask & HELP_INSERT) {
		printf("%s\n", STR_INSERT_VE);
	}

	/* detach command
	 */
	if (help_mask & HELP_DETACH) {
		printf("%s\n", STR_DETACH_VE);
		printf("%s\n", STR_DETACH_PLEX);
	}

	/* delete command
	 */
	if (help_mask & HELP_DELETE) {
		if (expert) {
			printf("%s\n", STR_DELETE_ALL_LABELS);
			printf("%s\n", STR_DELETE_LABEL);
		}
		printf("%s\n", STR_DELETE_OBJECT);
	}

	/* change command
	 */
	if (help_mask & HELP_CHANGE) {
		printf("%s\n", STR_CHANGE_NAME);
		printf("%s\n", STR_CHANGE_NODENAME);
		printf("%s\n", STR_CHANGE_VE_STATE);
		printf("%s\n", STR_CHANGE_TYPE);
		printf("%s\n", STR_CHANGE_VE_START);
		if (expert) {
			printf("%s\n", STR_CHANGE_STAT);
			if (has_plexing_support)
				printf("%s\n", STR_CHANGE_PLEXMEM);
			printf("%s\n", STR_CHANGE_VEMEM);

		}
	}

#ifdef REMOVE_COMMAND
#define	STR_REMOVE_VE_PLEX	"remove {ve,plex} ?name?     - Remove named part of an object."
#define	STR_REMOVE_VE		"remove ve ?name?            - Remove named volume element of an object."
	/*
	 * remove command
	 */
	if (has_plexing_license) {
		printf("%s\n", STR_REMOVE_VE_PLEX);
	} else {
		printf("%s\n", STR_REMOVE_VE);
	}
#endif

	/* miscellaneous commands
	 */
	if (help_mask & HELP_RESET) {
		printf("%s\n", STR_RESET);
		if (expert) {
			printf("%s\n", STR_RESET_STAT);
		}
	}
	if (help_mask & HELP_SCRIPT) {
		printf("%s\n", STR_SCRIPT);
	}
	if (help_mask & HELP_MISC) {
		printf("%s\n", STR_SH);
	}
	if (help_mask & HELP_HELP) {
		printf("%s\n", STR_HELP);
	}
	if (help_mask & HELP_QUIT) {
		printf("%s\n", STR_QUIT);
	}

	return (TCL_OK);
} /* end of tclcmd_help() */

