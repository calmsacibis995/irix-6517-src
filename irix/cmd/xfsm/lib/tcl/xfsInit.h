#ifndef	_xfsInit_h
#define	_xfsInit_h

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

#define	XFS_VAR_SUSER	"expert"
#define	XFS_VAR_VERBOSE	"verbose"

/*
 * Command procedures - one for every command in xlv_make.
 *
 * Each of these procedures are of type Tcl_CmdProc.
 */
extern int  xfsObjects(ClientData, Tcl_Interp *, int, char **);
extern int  xfsInfo(ClientData, Tcl_Interp *, int, char **);

extern int  xfsGetPartTable(ClientData, Tcl_Interp *, int, char **);
extern int  xfsSetPartTable(ClientData, Tcl_Interp *, int, char **);
extern int  xfsTransPartTable(ClientData, Tcl_Interp *, int, char **);
extern int  xfsGetPartsInUse(ClientData, Tcl_Interp *, int, char **);
extern int  xfsmGetBBlk(ClientData, Tcl_Interp *, int, char **);
extern int  xfsmChkMounts(ClientData, Tcl_Interp *, int, char **);

extern int  xfsmFsCmd(ClientData, Tcl_Interp *, int, char **);
extern int  xfsmFsInfoCmd(ClientData, Tcl_Interp *, int, char **);
extern int  xfsmXlvCmd(ClientData, Tcl_Interp *, int, char **);

extern int  xfsGetHostNames(ClientData, Tcl_Interp *, int, char **);
extern int  xfsmSort(ClientData, Tcl_Interp *, int, char **);
extern int  xfsmScrollVisible(ClientData, Tcl_Interp *, int, char **);
extern int  xfsmServerAlive(ClientData, Tcl_Interp *, int, char **);

extern int  xfsmGetAliasData(ClientData, Tcl_Interp *, int, char **);

#endif	/* _xfsInit_h */
