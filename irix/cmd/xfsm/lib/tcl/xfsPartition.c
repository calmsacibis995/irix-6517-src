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
#include "xfsUtil.h"

static char *partinfo;
static char *msg;

extern int		tr_normalize(tuple_t [], int, int, double);
extern int		modify_ranges(tuple_t [], int, int, double);
extern boolean_t	xfsmHasRootPermission(Tcl_Interp *);

/*
 *	argv[0]	- command name
 *	argv[1] - object data
 */
int
xfsGetPartsInUse(ClientData clientData,
		Tcl_Interp *interp,
		int argc,
		char **argv)
{
	int	rval;

	if(argc != 2) {
		Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
			" objdata\"", (char *) NULL);
		return TCL_ERROR;
	}

	partinfo = NULL;
	msg = NULL;
	if((rval = getPartsInUse(argv[1], &partinfo)) != 0) {
		Tcl_SetResult(interp, partinfo, TCL_DYNAMIC);
		return TCL_ERROR;
	}

	Tcl_SetResult(interp, partinfo, TCL_DYNAMIC);
	return TCL_OK;
}

/*
 *	argv[0]	- command name
 *	argv[1] - object data
 */
int
xfsGetPartTable(ClientData clientData,
		Tcl_Interp *interp,
		int argc,
		char **argv)
{
	int	rval;

	if(argc != 2) {
		Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
			" objdata\"", (char *) NULL);
		return TCL_ERROR;
	}

	partinfo = NULL;
	msg = NULL;
	if((rval = xfsGetPartitionTable(argv[1], &partinfo, &msg)) != 0) {
		Tcl_SetResult(interp, msg, TCL_DYNAMIC);
		return TCL_ERROR;
	}

	Tcl_SetResult(interp, partinfo, TCL_DYNAMIC);
	return TCL_OK;
}

int
xfsSetPartTable(ClientData clientData,
		Tcl_Interp *interp,
		int argc,
		char **argv)
{
	int	rval;

	if(! xfsmHasRootPermission(interp)) {
		return TCL_ERROR;
	}

	if(argc != 3) {
		Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
			" objdata data\"", (char *) NULL);
		return TCL_ERROR;
	}

	msg = NULL;
	if((rval = xfsSetPartitionTable(argv[1], argv[2], &msg)) != 0) {
		Tcl_SetResult(interp, msg, TCL_DYNAMIC);
		return TCL_ERROR;
	}

	return TCL_OK;
}

int
xfsTransPartTable(ClientData clientData,
		Tcl_Interp *interp,
		int argc,
		char **argv)
{
	int	i,
		id;
	double	d1,
		d2;
	char	buf[128];

	tuple_t	tuple[64];
	int	normal;
	double	minpct,
		maximum;

	int	l_argc,
		s_argc;
	char	**l_argv,
		**s_argv;

	if(argc != 5) {
		Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
			" normal minpct maximum list\"", (char *) NULL);
		return TCL_ERROR;
	}

	if(Tcl_GetInt(interp, argv[1], &normal) != TCL_OK ||
	   Tcl_GetDouble(interp, argv[2], &minpct) != TCL_OK ||
	   Tcl_GetDouble(interp, argv[3], &maximum) != TCL_OK) {
		return TCL_ERROR;
	}
	if(minpct <= 0 || normal <= 0) {
		Tcl_AppendResult(interp, argv[0],
			"bad args: minpct, or normal", (char *) NULL);
		return TCL_ERROR;
	}

	if(Tcl_SplitList(interp, argv[4], &l_argc, &l_argv) != TCL_OK) {
		return TCL_ERROR;
	}
	if(l_argc <= 0) {
		Tcl_AppendResult(interp, argv[0],
			": empty list specified.", (char *) NULL);
		return TCL_ERROR;
	}
	if(l_argc > 64) {
		Tcl_AppendResult(interp, argv[0],
			": list exceeds maximum number of items (64).",
			(char *) NULL);
		return TCL_ERROR;
	}

	for(i = 0; i < l_argc; ++i) {
		if(Tcl_SplitList(interp, l_argv[i], &s_argc, &s_argv)!=TCL_OK) {
			return TCL_ERROR;
		}
		if(s_argc != 3) {
			return TCL_ERROR;
		}

		if(Tcl_GetInt(interp, s_argv[0], &id) != TCL_OK ||
		   Tcl_GetDouble(interp, s_argv[1], &d1) != TCL_OK ||
		   Tcl_GetDouble(interp, s_argv[2], &d2) != TCL_OK) {
			return TCL_ERROR;
		}
		tuple[i].id = id;
		tuple[i].lvalue = d1;
		tuple[i].rvalue = d2;

		free((char *)s_argv);
	}
	free((char *)l_argv);

	if(tr_normalize(tuple, l_argc, normal, maximum) != 0) {
		return TCL_ERROR;
	}

	if(modify_ranges(tuple, l_argc, normal, minpct) != 0) {
		return TCL_ERROR;
	}

/*
	for(i = 0; i < l_argc; ++i) {
		printf("xfsTransPartTable %d: {%d %d %d}\n",
				i, tuple[i].id, tuple[i].lvalue, tuple[i].rvalue);
	}
*/

	for(i = 0; i < l_argc; ++i) {
		sprintf(buf, "%d %d %d",
				tuple[i].id,
				(int) (tuple[i].lvalue + .5),
				(int) (tuple[i].rvalue + .5));

		Tcl_AppendElement(interp, buf);
	}

	return TCL_OK;
}
