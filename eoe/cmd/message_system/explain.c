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

#ident  "$Header: /proj/irix6.5.7m/isms/eoe/cmd/message_system/RCS/explain.c,v 1.6 1997/06/23 20:31:17 bforney Exp $"

/*
 * explain: IRIX message explanation display tool
 *
 *     explain msgid
 *
 * Explain gets one message identifier from the command line then breaks the id
 * into group code and message number.  It opens the message catalog for the
 * indicated group and retrieves the numbered message from the explanation set.
 * If the output device is a tty, "more" (or a user specified PAGER) is used to
 * page the output.
 *
 * Tabs are set up to be read with tab spacing = 3
 */

#include <malloc.h>
#define __NLS_INTERNALS 1
#include <nl_types.h>
#undef __NLS_INTERNALS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "errmsg.h"
#include <errno.h>
#include <dirent.h>
#include <sys/param.h>

#define MORE "more -s"		/* default paging filter */
#define CMDCAT "msgsys.cat"
#define SYSCAT ""
#define EXPLAIN_CAT_EXT ".exp"
#define MESSAGE_CAT_EXT ".cat"

void catopenErrMsg(int, const char *);

void
main(int argc, char **argv)
{

	char *cmdname;		 /* pointer to command invocation name */
	char *gptr;		 /* pointer to group code name */
	char *nptr;    	      	 /* pointer to message number string */
	char *mptr;     	 /* pointer to explanation in message buffer */
	char *junk;	         /* miscellaneous pointer */
	char *pager;		 /* pointer to pager string */
	char nbuff[MAXPATHLEN];  /* character manipulation buffer */

	FILE *pfd;		 /* pipe file descriptor */

	int bufsize = 2000;	 /* explanation buffer size */
	int msgno;		 /* message number value */
	int catopenErr;


	nl_catd catd = (nl_catd)-1;	 /* catalog descriptor */

	char path[NL_MAXPATHLEN];
	char exp_filepath[NL_MAXPATHLEN];

	/* Get command and validate number of arguments */

	if (cmdname = strrchr(argv[0], '/'))
		++cmdname;
	else
		cmdname = argv[0];
	if (argc < 2 || argc > 3) {
		_cmdmsg(ERRMSG_EXPLAINUSAGE, 0, "ERROR", cmdname, CMDCAT,
			SYSCAT);
		exit(1);
	}

	/* Get group code and message number from arguments */

	if (argc == 2) {
		gptr = argv[1];
		if (nptr = strchr(argv[1], '-'))
			*nptr++ = '\0';
		else {
			nptr = argv[1] + strcspn(argv[1], "0123456789");
			junk = nbuff;
			while (*nptr != '\0') {
				*junk++ = *nptr;
				*nptr++ = '\0';
			}
			*junk = '\0';
			nptr = nbuff;
		}
	}
	else {
		gptr = argv[1];
		nptr = argv[2];
	}
	msgno = strtol(nptr, &junk, 10);
	if (!junk || junk == nptr || strlen(gptr) == 0) {
		_cmdmsg(ERRMSG_MSGIDENT, 0, "ERROR", cmdname, CMDCAT,
			SYSCAT, gptr, nptr);
		exit(1);
	}

	if ((mptr = _cat_name(gptr, path, 0, NL_MAXPATHLEN )) != 0) {
	  char *temp;

	  /* check for a MESSAGE_CAT_EXT string at the end of the file
	     path. If it exists, don't copy it. */
	  if ((temp = strstr(mptr, MESSAGE_CAT_EXT)) != NULL) {
	    strncpy(exp_filepath, mptr, temp-mptr);
	    exp_filepath[temp-mptr] = '\0';
	  } else {
	    strcpy(exp_filepath, mptr);
	  }
	  strcat(exp_filepath, EXPLAIN_CAT_EXT);
	  catd = catopen(exp_filepath, 0);
	}

	if (catd == (nl_catd)-1) {
	  catopenErr= __catopen_error_code();
	  if (catopenErr < 0) {
	    catopenErrMsg(catopenErr, cmdname);
	  } else {
	    _cmdmsg(ERRMSG_EXPCATOPEN, catopenErr, "ERROR", cmdname, CMDCAT,
		    SYSCAT, gptr);
	  }
	  exit(1);
	}

	/* Read explanation from catalog and print it.  More than one read may
	 * be required to insure that the entire message comes in */

	mptr = catgets(catd, NL_EXPSET, msgno, NULL);
	if (mptr == NULL) {
	  _cmdmsg(ERRMSG_NOEXPMSG, 0, "ERROR", cmdname,
		  CMDCAT, SYSCAT, gptr, nptr);
	  exit(1);
	}

	/* If stdout is a tty, pipe the output thru the pager.  Otherwise, write
	 * directly to stdout.
	 */

	if (isatty(fileno(stdout))) {
		if ((pager = getenv("PAGER")) == NULL)
			pager = MORE;
		if ((pfd = popen(pager, "w")) == NULL) {
			_cmdmsg(CMD_POPEN, 0, "ERROR", cmdname, CMDCAT,
				SYSCAT, pager);
			exit(1);
		}
		fputs (mptr, pfd);
		(void) pclose (pfd);
	}
	else
		fputs (mptr, stdout);

	catclose(catd);
	exit(0);
}

void 
catopenErrMsg(int CatOpenErr, const char *cmdname)
{
	switch(CatOpenErr) {
		case NL_ERR_MALLOC:
		  _cmdmsg(ERRMSG_NL_MALLOC, 0, "ERROR", cmdname,
			  CMDCAT, SYSCAT);
		  break;
	        case NL_ERR_MAXOPEN:
		  _cmdmsg(ERRMSG_NL_MAXOPEN, 0, "ERROR", cmdname,
			  CMDCAT, SYSCAT);
		  break;
	        case NL_ERR_MAP:
		  _cmdmsg(ERRMSG_NL_MAP, 0, "ERROR", cmdname,
			  CMDCAT, SYSCAT);
	        case NL_ERR_HEADER:
		  _cmdmsg(ERRMSG_NL_HEADER, 0, "ERROR", cmdname,
			  CMDCAT, SYSCAT);
		  break;
	}
} 


