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

#include	<sys/types.h>
#include	<sys/socket.h>
#include	<netinet/in.h>
#include	<netdb.h>

#include <tcl.h>
#include <tclExtend.h>
#include <tmFuncs.h>

/*
 *	argv[0]	- command name
 *	argv[1]	- parameters
 */
int
xfsGetHostNames(ClientData clientData,
		Tcl_Interp *interp,
		int argc,
		char **argv)
{
	static char	*msg;
	static char	*items;
	int	rval;

	if(argc != 2) {
		Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
			" parameters\"", (char *) NULL);
		return TCL_ERROR;
	}

	msg = NULL;
	items = NULL;

	if((rval = xfsGetHosts(argv[1], &items, &msg)) == 0) {
		Tcl_SetResult(interp, items, TCL_DYNAMIC);
		return TCL_OK;
	}
	else {
		Tcl_AppendResult(interp, msg, (char *) NULL);
		free(msg);
		return TCL_ERROR;
	}
}

int
xfsmGetAliasData(ClientData clientData,
		 Tcl_Interp *interp,
		 int argc,
		 char **argv)
{
	struct hostent	*host_ent;
	Tcl_DString	result;
	int		i;

	if((host_ent = gethostbyname(argv[1])) == NULL) {
		switch(h_errno) {
			case	HOST_NOT_FOUND:
				break;

			case	TRY_AGAIN:
				break;

			case	NO_RECOVERY:
				break;

			case	NO_DATA:
				break;

		}
		return TCL_ERROR;
	}

	Tcl_DStringInit(&result);

	Tcl_DStringAppendElement(&result, host_ent->h_name);

	for(i = 0; host_ent->h_aliases[i] != NULL; ++i) {
		Tcl_DStringAppendElement(&result, host_ent->h_aliases[i]);
	}

	Tcl_SetResult(interp, Tcl_DStringValue(&result), TCL_VOLATILE);
	Tcl_DStringFree(&result);

	return TCL_OK;
}
