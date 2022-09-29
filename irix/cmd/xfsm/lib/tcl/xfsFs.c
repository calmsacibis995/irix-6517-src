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
 *	argv[1]	- option
 *	argv[2] - object
 *	argv[3] - params
 */
int
xfsmFsCmd(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	static char	*msg;
	char		c;
	int		rval;

	if(argc != 4) {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
			argv[0], " option object params\"", (char *) NULL);
		return TCL_ERROR;
	}

	if(! xfsmHasRootPermission(interp)) {
		return TCL_ERROR;
	}

	msg = NULL;
	c = argv[1][0];

	if (c == 'c' && !strcmp("create", argv[1])) {
		rval = xfsCreate(argv[2], argv[3], &msg);
	} else
	if (c == 'd' && !strcmp("delete", argv[1])) {
		rval = xfsDelete(argv[2], argv[3], &msg);
	} else
	if (c == 'e' && !strcmp("export", argv[1])) {
		rval = xfsExport(argv[2], argv[3], &msg);
	} else
	if (c == 'm' && !strcmp("modify", argv[1])) {
		rval = xfsEdit(argv[2], argv[3], &msg);
	} else
	if (c == 'm' && !strcmp("mount", argv[1])) {
		rval = xfsMount(argv[2], argv[3], &msg);
	} else
	if (c == 'u' && !strcmp("unexport", argv[1])) {
		rval = xfsUnexport(argv[2], argv[3], &msg);
	} else
	if (c == 'u' && !strcmp("unmount", argv[1])) {
		rval = xfsUnmount(argv[2], argv[3], &msg);
	} else {
		sprintf(interp->result, "%s: unknown method \"%s\"",
			argv[0], argv[1]);
		return TCL_ERROR;
	}

	switch(rval) {
		case	0:
			return TCL_OK;
			break;

		case	2:
			Tcl_SetResult(interp, msg, TCL_DYNAMIC);
			Tcl_SetErrorCode(interp,
					"SERVER",
					"WARNING",
					(char *)NULL);
			return TCL_ERROR;
			break;
			
		case	1:
		default:
			Tcl_SetResult(interp, msg, TCL_DYNAMIC);
			return TCL_ERROR;
			break;
	}
}

/*
 *	argv[0]	- command name
 *	argv[1]	- option
 *	argv[2] - params
 */
int
xfsmFsInfoCmd(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	static char	*msg;
	static char	*info;
	char		c;
	int		rval;

	msg = NULL;
	info = NULL;
	c = argv[1][0];

	if (c == 'e' && !strcmp("export", argv[1])) {
		if(argc != 3) {
			Tcl_AppendResult(interp, "wrong # args: should be \"",
				argv[0], " option params\"", (char *) NULL);
			return TCL_ERROR;
		}
		if((rval = xfsGetExportInfo(argv[2], &info, &msg)) != 0) {
			Tcl_SetResult(interp, msg, TCL_DYNAMIC);
			return TCL_ERROR;
		}
	} else
	if (c == 's' && !strcmp("simple", argv[1])) {
		if(argc != 4) {
			Tcl_AppendResult(interp, "wrong # args: should be \"",
				argv[0], " option object params\"",
				(char *) NULL);
			return TCL_ERROR;
		}
		if((rval = xfsSimpleFsInfo(argv[2], argv[3], &info, &msg))
								!= 0) {
			Tcl_SetResult(interp, msg, TCL_DYNAMIC);
			return TCL_ERROR;
		}
	} else {
		sprintf(interp->result, "%s: unknown method \"%s\"",
			argv[0], argv[1]);
		return TCL_ERROR;
	}

	Tcl_SetResult(interp, info, TCL_DYNAMIC);
	return TCL_OK;
}
