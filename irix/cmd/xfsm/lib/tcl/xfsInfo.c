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
#ident "$Revision: 1.4 $"

#include <tcl.h>
#include <tclExtend.h>
#include <tmFuncs.h>

/*
 *	argv[0]	- command name
 *	argv[1] - object data
 */
int
xfsInfo(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	static char	*objinfo;
	static char	*msg;
	int		rval;

	if(argc != 2) {
		Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
			" object_data\"", (char *) NULL);
		return TCL_ERROR;
	}

	if((rval = getInfo(argv[1], &objinfo, &msg)) != 0) {
		Tcl_SetResult(interp, msg, TCL_DYNAMIC);
		return TCL_ERROR;
	}

	Tcl_SetResult(interp, objinfo, TCL_DYNAMIC);
	return TCL_OK;
}

/*
 *	argv[0]	- command name
 *	argv[1] - object data
 */
int
xfsmGetBBlk(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	static char	*bblks;
	static char	*msg;
	int		rval;

	if(argc != 2) {
		Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
			" object_data\"", (char *) NULL);
		return TCL_ERROR;
	}

	if((rval = xfsGetBBlk(argv[1], &bblks, &msg)) != 0) {
		Tcl_SetResult(interp, msg, TCL_DYNAMIC);
		return TCL_ERROR;
	}

	Tcl_SetResult(interp, bblks, TCL_DYNAMIC);
	return TCL_OK;
}

/*
 *	argv[0]	- command name
 *	argv[1] - object data
 */
int
xfsmChkMounts(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	static char	*info;
	static char	*msg;
	int		rval;

	if(argc != 2) {
		Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
			" object_data\"", (char *) NULL);
		return TCL_ERROR;
	}

	if((rval = xfsChkMounts(argv[1], &info, &msg)) != 0) {
		Tcl_SetResult(interp, msg, TCL_DYNAMIC);
		return TCL_ERROR;
	}

	Tcl_SetResult(interp, info, TCL_DYNAMIC);
	return TCL_OK;
}
