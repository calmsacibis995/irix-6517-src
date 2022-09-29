/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/lpsched/lpNet/parent/RCS/init.c,v 1.1 1992/12/14 11:31:19 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/



/*=================================================================*/
/*
*/
#include	<sys/utsname.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<signal.h>
#include	<string.h>
#include	<sys/fcntl.h>
#include	"lpNet.h"
#include	"logMgmt.h"
#include	"errorMgmt.h"
#include	"debug.h"


/*---------------------------------------------------------*/
/*
**	Local function prototypes.
*/
#ifdef	__STDC__

static	char	*LogMsgTagFn (void);

#else

static	char	*LogMsgTagFn ();

#endif
/*=================================================================*/

/*=================================================================*/
/*
*/
void
lpNetInit (argc, argv)

int	argc;
char	*argv [];
{
	/*----------------------------------------------------------*/
	/*
	*/
		int		fd;
		char		msgBuffer [128];
		struct utsname	utsName;
	static	char		FnName [] = "lpNetInit";


	/*----------------------------------------------------------*/
	/*
	**	Default process initialization.
	*/
	ProcessInfo.processType	= ParentProcess;
	ProcessInfo.processRank	= ParentRank;
	ProcessInfo.processId	= getpid ();
	ProcessInfo.lpExec	= -1;
	ProcessInfo.listenS5	= -1;
	ProcessInfo.listenBSD	= -1;

	ProcessInfo.remoteSystemInfo_p	   = NULL;
	ProcessInfo.remoteConnectionInfo_p = NULL;

	(void)	uname (&utsName);
	ProcessInfo.systemName_p = strdup (utsName.nodename);


	(void)	dup2 (0, 3);
	(void)	close (0);
	ProcessInfo.lpExec = 3;

	fd = open ("/dev/null", O_RDWR);
	if (fd == -1)
		TrapError (Fatal, Unix, FnName, "open");

	(void)	dup2 (fd, 1);
	(void)	dup2 (fd, 2);


	/*----------------------------------------------------------*/
	/*
	**	Init the logger.  The logger is considered critical
	**	If it cannot open the requested file it opens an
	**	alternate in /tmp.
	*/
	SetLogMsgTagFn (LogMsgTagFn);

	if (! OpenLogFile (FILE_LPNET_LOG))
		TrapError (Fatal, Internal, FnName, "OpenLogFile");


	/*----------------------------------------------------------*/
	/*
	**	Open the networkInfoFile.  This is also considered
	**	critical since lpNet cannot do any work without
	**	remote system info.
	*/
	if (! OpenNetworkInfoFile (FILE_LPNET_DATA))
		TrapError (Fatal, Internal, FnName, "OpenNetworkInfoFile");

	/*----------------------------------------------------------*/
	/*
	**	Change our working directory to the home directory
	**	of the LP system.  All subsequent file accesses
	**	will be relative to here.
	*/
	if (chdir (LP_HOME_DIR) == -1)
		TrapError (Fatal, Unix, FnName, "chdir");


	WriteLogMsg ("Starting.");


	return;
}
/*==================================================================*/

/*==================================================================*/
/*
*/
static char
*LogMsgTagFn ()
{
	static	char	MsgTag [32];

	(void)	sprintf (MsgTag, "%c %5d %s",
		ProcessInfo.processType == ParentProcess ? 'p' : 'c',
		ProcessInfo.processId,
		ProcessInfo.remoteSystemInfo_p == NULL ? "<none>" :
		ProcessInfo.remoteSystemInfo_p->systemName_p);

	return	MsgTag;
}
/*==================================================================*/
