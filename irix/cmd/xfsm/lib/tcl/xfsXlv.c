/**************************************************************************
 *                                                                        *
 *              Copyright (C) 1994, Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.2 $"

#include <tcl.h>
#include <tclExtend.h>
#include <tmFuncs.h>

extern boolean_t	xfsmHasRootPermission(Tcl_Interp *);


/*
 *	argv[0]	- command name
 *	argv[1] - object
 *	argv[2] - params
 */
int
xfsmXlvCmd(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	static char	*msg;
	char		c;
	int		rval;

	msg = NULL;
	c = argv[1][0];

	if (c == 'a' && !strcmp("attach", argv[1])) {
		if(! xfsmHasRootPermission(interp)) {
			return TCL_ERROR;
		}
		if(argc != 4) {
		    sprintf(interp->result,
			"wrong # args: should be \"%.50s %s object params\"",
			argv[0], argv[1]);
		    return TCL_ERROR;
		}
		if((rval = attachXlvObject(argv[2], argv[3], &msg)) != 0) {
			Tcl_SetResult(interp, msg, TCL_DYNAMIC);
			return TCL_ERROR;
		}
	} else
	if (c == 'c' && !strcmp("create", argv[1])) {
		if(! xfsmHasRootPermission(interp)) {
			return TCL_ERROR;
		}
		if(argc != 4) {
		    sprintf(interp->result,
			"wrong # args: should be \"%.50s %s object params\"",
			argv[0], argv[1]);
		    return TCL_ERROR;
		}
		if((rval = createXlvObject(argv[2], argv[3], &msg)) != 0) {
			Tcl_SetResult(interp, msg, TCL_DYNAMIC);
			return TCL_ERROR;
		}
	} else
	if (c == 'd' && !strcmp("delete", argv[1])) {
		if(! xfsmHasRootPermission(interp)) {
			return TCL_ERROR;
		}
		if(argc != 3) {
		    sprintf(interp->result,
			"wrong # args: should be \"%.50s %s object\"",
			argv[0], argv[1]);
		    return TCL_ERROR;
		}
		if((rval = deleteXlvObject(argv[2], &msg)) != 0) {
			Tcl_SetResult(interp, msg, TCL_DYNAMIC);
			return TCL_ERROR;
		}
	} else
	if (c == 'd' && !strcmp("detach", argv[1])) {
		if(! xfsmHasRootPermission(interp)) {
			return TCL_ERROR;
		}
		if(argc != 4) {
		    sprintf(interp->result,
			"wrong # args: should be \"%.50s %s object params\"",
			argv[0], argv[1]);
		    return TCL_ERROR;
		}
		if((rval = detachXlvObject(argv[2], argv[3], &msg)) != 0) {
			Tcl_SetResult(interp, msg, TCL_DYNAMIC);
			return TCL_ERROR;
		}
	} else
	if (c == 'r' && !strcmp("remove", argv[1])) {
		if(! xfsmHasRootPermission(interp)) {
			return TCL_ERROR;
		}
		if(argc != 4) {
		    sprintf(interp->result,
			"wrong # args: should be \"%.50s %s object params\"",
			argv[0], argv[1]);
		    return TCL_ERROR;
		}
		if((rval = removeXlvObject(argv[2], argv[3], &msg)) != 0) {
			Tcl_SetResult(interp, msg, TCL_DYNAMIC);
			return TCL_ERROR;
		}
	} else
	if (c == 's' && !strcmp("synopsis", argv[1])) {
		if(argc != 3) {
		    sprintf(interp->result,
			"wrong # args: should be \"%.50s %s object\"",
			argv[0], argv[1]);
		    return TCL_ERROR;
		}
		if((rval = getXlvSynopsis(argv[2], &msg)) != 0) {
			Tcl_SetResult(interp, msg, TCL_DYNAMIC);
			return TCL_ERROR;
		} else {
			Tcl_SetResult(interp, msg, TCL_DYNAMIC);
		}
	} else
	if (c == 'o' && !strcmp("objInfo", argv[1])) {
		if(argc != 3) {
		    sprintf(interp->result,
			"wrong # args: should be \"%.50s %s object\"",
			argv[0], argv[1]);
		    return TCL_ERROR;
		}
		if((rval = infoXlvObj(argv[2], &msg)) != 0) {
			Tcl_SetResult(interp, msg, TCL_DYNAMIC);
			return TCL_ERROR;
		} else {
			Tcl_SetResult(interp, msg, TCL_DYNAMIC);
		}
	} else {
		sprintf(interp->result, "%.50s: unknown method \"%s\"",
			argv[0], argv[1]);
		return TCL_ERROR;
	}

	return TCL_OK;
}
