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
#ident "$Revision: 1.3 $"

#include <tcl.h>
#include <tclExtend.h>
#include <tmFuncs.h>

#include <Xm/ScrolledW.h>

/*
 *	argv[0]	- command name
 *	argv[1] - scrolled window widget
 *	argv[2] - child
 *	argv[3] - left_right_margin
 *	argv[4] - top_bottom_margin
 */
int
xfsmScrollVisible(ClientData clientData,
		  Tcl_Interp *interp,
		  int argc,
		  char **argv)
{
    Tm_Widget	*tmw;
    Widget	sw,
		child;
    int		lr_margin,
		tb_margin;
    Widget	cw;
    Dimension	cw_dim,
		child_dim;

    if(argc != 5) {
	sprintf(interp->result,
	"wrong # args: should be \"%.50s scrolled_widget child_widget left_right_margin top_bottom_margin\"", argv[0]);
	return TCL_ERROR;
    }

    if((tmw = Tm_WidgetInfoFromPath(interp, argv[1])) == NULL) {
	 sprintf(interp->result,
		"%.50s %s: bad \"scrolled_widget\" value", argv[0], argv[1]);
	 return TCL_ERROR;
    }
    sw = tmw->widget;

    if((tmw = Tm_WidgetInfoFromPath(interp, argv[2])) == NULL) {
	 sprintf(interp->result,
		"%.50s %s: bad \"child_widget\" value", argv[0], argv[2]);
	 return TCL_ERROR;
    }
    child = tmw->widget;

    if(strcmp(argv[3], "center") == 0) {
	XtVaGetValues(sw, XmNclipWindow, &cw, NULL);
	XtVaGetValues(cw, XmNwidth, &cw_dim, NULL);
	XtVaGetValues(child, XmNwidth, &child_dim, NULL);
	lr_margin = (cw_dim - child_dim) / 2;
    } else if(Tcl_GetInt(interp, argv[3], &lr_margin) == TCL_ERROR) {
	return TCL_ERROR;
    }
    if(strcmp(argv[4], "center") == 0) {
	XtVaGetValues(sw, XmNclipWindow, &cw, NULL);
	XtVaGetValues(cw, XmNheight, &cw_dim, NULL);
	XtVaGetValues(child, XmNheight, &child_dim, NULL);
	tb_margin = (cw_dim - child_dim) / 2;
    } else if(Tcl_GetInt(interp, argv[4], &tb_margin) == TCL_ERROR) {
	return TCL_ERROR;
    }

    XmScrollVisible(sw, child, lr_margin, tb_margin);
    return TCL_OK;
}

int
xfsmServerAlive(ClientData clientData,
		Tcl_Interp *interp,
		int argc,
		char **argv)
{
	char	cmd[512];

	if(argc != 2) {
		sprintf(interp->result,
			"wrong # args: should be \"%.50s hostname\"", argv[0]);
		return TCL_ERROR;
	}
	sprintf(cmd, "exec /usr/etc/rpcinfo -t %s 391016", argv[1]);

	return(Tcl_Eval(interp, cmd));
}

boolean_t
xfsmHasRootPermission(Tcl_Interp *interp)
{
	if(geteuid() != 0) {
		Tcl_AppendResult(interp,
				"You must have super-user permissions\n",
				"to perform this action.",
				(char *) NULL);
		return B_FALSE;
	}
	return B_TRUE;
}
