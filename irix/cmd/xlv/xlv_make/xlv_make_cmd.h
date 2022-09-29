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
#ident "$Revision: 1.11 $"

#define	XLV_MAKE_VAR_SUSER	"expert"
#define	XLV_MAKE_VAR_VERBOSE	"verbose"
#define	XLV_MAKE_VAR_FORCE	"force"
#define	XLV_MAKE_VAR_NO_ASSEMBLE	"no_xlv_assemble"

#define	_PATH_XLV_ASSEMBLE	"/sbin/xlv_assemble"

/*
 * Command procedures - one for every command in xlv_make.
 *
 * Each of these procedures are of type Tcl_CmdProc.
 */
extern int  xlv_make_vol (ClientData clientData, Tcl_Interp *interp, 
	int argc, char *argv[]);
extern int  xlv_make_subvol_log (ClientData clientData, Tcl_Interp *interp, 
	int argc, char *argv[]);
extern int  xlv_make_subvol_data (ClientData clientData, Tcl_Interp *interp, 
	int argc, char *argv[]);
extern int  xlv_make_subvol_rt (ClientData clientData, Tcl_Interp *interp, 
	int argc, char *argv[]);
extern int  xlv_make_plex (ClientData clientData, Tcl_Interp *interp, 
	int argc, char *argv[]);
extern int  xlv_make_ve (ClientData clientData, Tcl_Interp *interp, 
	int argc, char *argv[]);
extern int  xlv_make_show (ClientData clientData, Tcl_Interp *interp, 
	int argc, char *argv[]);
extern int  xlv_make_end (ClientData clientData, Tcl_Interp *interp, 
	int argc, char *argv[]);
extern int  xlv_make_exit (ClientData clientData, Tcl_Interp *interp, 
	int argc, char *argv[]);
extern int  xlv_make_quit (ClientData clientData, Tcl_Interp *interp, 
	int argc, char *argv[]);
extern int  xlv_make_help (ClientData clientData, Tcl_Interp *interp, 
	int argc, char *argv[]);
extern int  xlv_make_clear (ClientData clientData, Tcl_Interp *interp, 
	int argc, char *argv[]);
extern int  xlv_make_create (ClientData clientData, Tcl_Interp *interp, 
	int argc, char *argv[]);
extern int  xlv_make_shell (ClientData clientData, Tcl_Interp *interp, 
	int argc, char *argv[]);

extern void xlv_make_init (void);
extern void xlv_make_done (void);


/*
 * Back-end routines called by the command procedures.
 */

