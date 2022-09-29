/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "xfsmSort.c: $Revision: 1.2 $"

/*
 * Filename:	xfsmSort.c
 *
 * Synopsis:	Tcl/Tm script/C custom sorting interface function.
 *
 * Description:	This function acts as an intermediary between the Tcl/Tm
 *		scripts and whatever custom sorting routines there are.
 *		It first checks to make sure that it was passed the proper
 *		number of arguments and then calls qsort with the appropriate
 *		arguments.
 *
 *		This file contains the following functions:
 *			xfsmSort	Tcl/Tm command for custom sorting.
 *
 *		The "glue" that connects these functions to the Tcl/Tm scripts
 *		is located in the xfsInit.c file.
 */
#include <tcl.h>
#include <tclExtend.h>
#include <tmFuncs.h>
#include "xfsmSortUtils.h"

Tcl_Interp	*sortInterp;


/*
 * Usage:	int xfsmSort(ClientData clientData,
 *			     Tcl_Interp *interp,
 *			     int argc,
 *			     char **argv)
 *
 * Synopsis:	Tcl function that calls qsort(3C) on the data passed in.
 *
 * Arguments:	clientData	Unused.
 *		interp		Interpreter in which the command was executed.
 *		argc		The total number of words in the TCL command.
 *		argv		An array of pointers to the values of the words.
 *				argv[0] - command name
 *				argv[1] - a keyword indicating the sorting
 *					  function to be passed to qsort(3C).
 *				argv[2] - list of data to sort
 *
 * Returns:	TCL_OK for success.
 *		TCL_ERROR for error.
 *
 * Description:	This function is called from a Tcl script to perform a sort
 *		on a list of strings.  The data is first put into a form that
 *		qsort(3C) can use (via Tcl_SplitList()) and then qsort(3C) is
 *		called to sort the data.  The data is then put back into list
 *		format (via Tcl_Merge()) so that it can be returned to the
 *		script.
 */
int
xfsmSort(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	int     rval;
	char    c = argv[1][0];
	int	itemc;
	char	**itemv;

	if(argc != 3) {
		Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
			" command keyword/values\"", (char *) NULL);
		return TCL_ERROR;
	}

	if((c == 'd') && (strcmp(argv[1], "diskObjs") == 0)) {
		/*
		 *	Sort disk "object" strings
		 */
		if (Tcl_SplitList(interp, argv[2], &itemc, &itemv) != TCL_OK) {
			return TCL_ERROR;
		}
		if(itemc != 0) {
			sortInterp = interp;
			qsort(itemv, itemc, sizeof(char *), xfsmSortDiskObjs);
			
		}
		Tcl_SetResult(interp, Tcl_Merge(itemc, itemv), TCL_DYNAMIC);
		free(itemv);
	} else
	if((c == 'p') && (strcmp(argv[1], "partNames") == 0)) {
		/*
		 *	Sort partition names
		 */
		if (Tcl_SplitList(interp, argv[2], &itemc, &itemv) != TCL_OK) {
			return TCL_ERROR;
		}
		if(itemc != 0) {
			qsort(itemv, itemc, sizeof(char *), xfsmSortPartNames);
			
		}
		Tcl_SetResult(interp, Tcl_Merge(itemc, itemv), TCL_DYNAMIC);
		free(itemv);
	} else {
		Tcl_AppendResult(interp, "%s: unknown method \"%s\"",
				argv[0], argv[1]);
		return TCL_ERROR;
	}

	return TCL_OK;
}
