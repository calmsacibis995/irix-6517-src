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
#ident "$Revision: 1.5 $"

#include	<tcl.h>
#include	<unistd.h>
#include	<locale.h>
#include	"xfsInit.h"

/*
 * Command table.
 */
typedef struct {
	char		*name;
	Tcl_CmdProc	*proc;
} cmd_entry_type;

static cmd_entry_type		xfs_cmds [] = {
	{ "xfsObjects",		xfsObjects },
	{ "xfsInfo",		xfsInfo },

	{ "xfsGetPartTable",	xfsGetPartTable },
	{ "xfsSetPartTable",	xfsSetPartTable },
	{ "xfsTransPartTable",	xfsTransPartTable },
	{ "xfsGetPartsInUse",	xfsGetPartsInUse },
	{ "xfsmGetBBlk",	xfsmGetBBlk },
	{ "xfsmChkMounts",	xfsmChkMounts },

	{ "fsCmd",	xfsmFsCmd },
	{ "fsInfoCmd",	xfsmFsInfoCmd },
	{ "xlvCmd",	xfsmXlvCmd },

	{ "xfsGetHostNames",	xfsGetHostNames },
	{ "getAliasData",	xfsmGetAliasData },

	{ "xfsmSort",		xfsmSort },
	{ "xfsmServerAlive",	xfsmServerAlive },
	{ "scrollVisible",	xfsmScrollVisible },

	{ NULL,		NULL }
};

/*
 * Register all the XFS make commands so that they can be invoked
 * by the Tcl interpreter.
 */
int
xfsInit(Tcl_Interp *interp)
{
	cmd_entry_type	*cmd_p;

	setlocale(LC_ALL, "C");
	setcat("xlv_admin");

	for(cmd_p = xfs_cmds; cmd_p->name != NULL; cmd_p++) {
		Tcl_CreateCommand(interp, cmd_p->name, cmd_p->proc, NULL, NULL);
	}

	return TCL_OK;
}
