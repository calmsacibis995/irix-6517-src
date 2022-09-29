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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/lpsched/lpNet/parent/RCS/main.c,v 1.1 1992/12/14 11:31:24 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/



/*=================================================================*/
/*
*/
#include	"lpNet.h"
#include	"boolean.h"
#include	"errorMgmt.h"
#include	"debug.h"

/*----------------------------------------------------------*/
/*
**	Global variables.
*/
	processInfo	ProcessInfo;

/*=================================================================*/

/*=================================================================*/
/*
*/
int
main (argc, argv)

int	argc;
char	*argv [];
{
	/*----------------------------------------------------------*/
	/*
	*/
		boolean	done;
	static	char	FnName []	= "main";

	/*----------------------------------------------------------*/
	/*
	*/
	lpNetInit (argc, argv);

	done = False;

	while (! done)
		switch (ProcessInfo.processType) {
		case	ParentProcess:
			lpNetParent ();
			break;

		case	ChildProcess:
			lpNetChild ();
			break;

		default:
			TrapError (Fatal, Internal, FnName,
			"Unknown processType.");
		}

	return	0;	/*  Never reached.	*/
}
/*==================================================================*/

/*==================================================================*/
/*
*/
void
Exit (exitCode)

int	exitCode;
{
	if (exitCode == 0)
		WriteLogMsg ("Normal process termination.");
	else
		WriteLogMsg ("Abnormal process termination.");

	exit (exitCode);
}
/*==================================================================*/
