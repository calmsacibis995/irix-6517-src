/*
*
* Copyright 1997, Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/message_system/RCS/cmdmsg.c,v 1.1 1997/03/25 22:42:17 bforney Exp $"

#include <nl_types.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>


#define ERRFILE		stderr	/* Default output stream		  */
#define MAXMLN		1024	/* Maximum message length		  */
#define Nulchar		'\0'

static void _docmdmsg();
void _docmdprompt();
extern char *_docmdgetmsg();

FILE	*errfile = ERRFILE;

/************************************************************************/
/*	Name:	_cmdmsg							*/
/*									*/
/*	Description:							*/
/*		IRIX utility command run time message handler.  	*/
/*									*/
/*	Formal parameters:						*/
/*		msgno - message catalog message number			*/
/*		erno - system error number				*/
/*		severity - Pointer to severity string, "" string if no  */ 
/*			   severity 					*/
/************************************************************************/

void
_cmdmsg(int msgno, int erno, const char *severity, const char *cmdname,
	const char *cmdcat, const char *syscat, ...)
{
	va_list	args;

	/* add varargs to tmpbuf */
	va_start(args, syscat);
	(void) _docmdmsg(msgno, erno, severity, cmdname, cmdcat,
			 syscat, args);
	va_end(args);
}


/************************************************************************/
/*	Name:	_docmdmsg						*/
/*									*/
/*	Description:							*/
/*		IRIX utility command run time message handler.  	*/
/*									*/
/*	Formal parameters:						*/
/*		msgno - message catalog message number			*/
/*		erno - system error number				*/
/*		severity - Pointer to severity string, "" string if no  */ 
/*			   severity 					*/
/*		args - argument list					*/
/************************************************************************/

static void
_docmdmsg(int msgno,
	  int erno,
	  const char *severity,
	  const char *cmdname,
	  const char *cmdcat,
	  const char *syscat,
	  va_list args)
{
	char	msgbuf[MAXMLN*2+1];	/* Message buffer		*/
	char	outbuf[MAXMLN*2+1];	/* Formatted output buffer	*/
	char	*mp;			/* Pointer to message buffer	*/

	msgbuf[0] = '\0';

	mp = _docmdgetmsg(msgno, erno, msgbuf, MAXMLN*2, cmdname, cmdcat,
			  syscat, args);

	/* build formatted message */
	(void) cmdmsgfmt(cmdname, msgno? cmdcat: syscat,
		msgno? msgno:erno, severity, msgbuf, outbuf, 
		sizeof(outbuf));

	/* print formatted message */
	(void) write(fileno(errfile), outbuf, strlen(outbuf));

	return;
}





