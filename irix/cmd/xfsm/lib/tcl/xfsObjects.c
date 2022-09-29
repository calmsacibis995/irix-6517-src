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
#ident "$Revision: 1.1 $"

#include	<tcl.h>
#include	<tclExtend.h>
#include	<tmFuncs.h>

/*
 *	argv[0]	- command name
 *	argv[1] - search data
 */
int
xfsObjects(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	static char	*objlist;
	static char	*msg;
	int		rval;

	if(argc != 2) {
		Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
			"pattern\"", (char *) NULL);
		return TCL_ERROR;
	}

	objlist = NULL;
	msg = NULL;
	if((rval = getObjects(argv[1], &objlist, &msg)) != 0) {
		Tcl_SetResult(interp, msg, TCL_DYNAMIC);
		return TCL_ERROR;
	}

	Tcl_SetResult(interp, objlist, TCL_DYNAMIC);
	return TCL_OK;
}
