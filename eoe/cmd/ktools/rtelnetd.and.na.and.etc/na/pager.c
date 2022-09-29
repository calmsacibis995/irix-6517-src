/*
 *****************************************************************************
 *
 *        Copyright 1993 Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Module Function:
 * 	Replaceable pager module.
 *
 * Original Author: James Carlson	Created on: 22FEB93
 *
 * Revision Control Information:
 *
 * $Id: pager.c,v 1.2 1996/10/04 12:10:45 cwilson Exp $
 *
 * This file created by RCS from:
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/na/RCS/pager.c,v $
 *
 * Revision History:
 *
 * $Log: pager.c,v $
 * Revision 1.2  1996/10/04 12:10:45  cwilson
 * latest rev
 *
 * Revision 1.4  1993/07/30  10:21:31  carlson
 * SPR 1923 -- rewrote pager to use file descriptor instead of file pointer.
 *
 * Revision 1.3  1993/07/15  10:52:59  carlson
 * Removed bogus externs which reduced portability.
 *
 * Revision 1.2  93/04/22  09:38:25  carlson
 * Made it work with a missing PAGER variable -- check for null
 * pointers.  Tested in various cases.
 * 
 * Revision 1.1  93/02/24  13:08:50  carlson
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************
 */

#define RCSDATE $Date: 1996/10/04 12:10:45 $
#define RCSREV	$Revision: 1.2 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/na/RCS/pager.c,v 1.2 1996/10/04 12:10:45 cwilson Exp $"
#ifndef lint
static char rcsid[] = RCSID;
#endif

/*
 *	Include Files
 */
#include "../inc/config.h"

#include <stdio.h>
#include <strings.h>
#include <ctype.h>
#include <signal.h>

/*
 *	External Definitions
 */

extern void interrupt();

/*
 *	Defines and Macros
 */

/*
 *	Local Data
 */

static char *pagecmd;
static int saved_out,pagerrunning;
#ifdef HAVE_POPEN
static FILE *outp = NULL;
#else
static char *pagearr[256];
static int pager_proc = 0;
#endif

/*
 *	Forward Routine Definitions
 */

void close_pager();

/*
 *	Global Data Declarations
 */


static void
page_close(arg)
int arg;
{
#ifdef USE_PAGER
	int i;

	i = pagerrunning;
	pagerrunning = 0;
#ifdef SYS_V
	signal(SIGPIPE,page_close);
#endif
	close_pager();
	if (i)
		kill(0,SIGINT);
#endif
}

/*
 * initialize_pager -
 *	Read in environment data and configure appropriate pager.
 */

void
initialize_pager()
{
#ifdef USE_PAGER
	saved_out = dup(1);
	if ((pagecmd = (char *)getenv("PAGER")) == NULL)
		return;
#ifndef HAVE_POPEN
	{
		char chr,*cp,**cpp = pagearr;

		cp = (char *)malloc(strlen(pagecmd)+1);
		strcpy(cp,pagecmd);
		chr = *cp;
		for (;;) {
			while (isspace(chr))
				chr = *++cp;
			if (chr == '\0')
				break;
			*cpp++ = cp;
			while (chr != '\0' && !isspace(chr))
				chr = *++cp;
			*cp = '\0';
 		}
		*cpp = NULL;
	}
#endif
	pagerrunning = 0;
	(void)signal(SIGPIPE,page_close);
#endif
}

/*
 * open_pager -
 */

void
open_pager()
{
#ifdef USE_PAGER
	if (pagecmd == NULL || saved_out < 0)
		return;
#ifdef HAVE_POPEN
	if ((outp = popen(pagecmd, "w")) != NULL) {
		if (dup2(fileno(outp),1) < 0) {
			pclose(outp);
			outp = NULL;
			(void)dup2(saved_out,1);
		} else
			pagerrunning = 1;
	}
#else
	{
		int pip[2];
		int retv;

		if (pipe(pip) < 0)
			return;
		pager_proc = fork();
		if (pager_proc < 0) {		/* Fork failure */
			(void)close(pip[0]);
			(void)close(pip[1]);
			return;
		}
		if (pager_proc == 0) {		/* Child process */
			if (dup2(pip[0],0) < 0)
				exit(1);
			(void)close(pip[1]);
			execvp(pagearr[0],pagearr);
			exit(1);
		}
		(void)close(pip[0]);
		retv = dup2(pip[1],1);
		(void)close(pip[1]);
		if (retv < 0)
			(void)dup2(saved_out,1);
		else
			pagerrunning = 1;
	}
#endif
#endif
}

void
close_pager()
{
#ifdef USE_PAGER
	if (saved_out >= 0)
		(void)dup2(saved_out,1);
#ifdef HAVE_POPEN
	if (outp != NULL) {
		pclose(outp);
		outp = NULL;
	}
#else
	if (pager_proc != 0) {
		(void)wait(NULL);
		pager_proc = 0;
	}
#endif
#endif
}

/*
 * stop_pager -
 *	Kill off the pager process.
 */

void
stop_pager()
{
#ifdef USE_PAGER
	pagerrunning = 0;
#ifdef HAVE_POPEN
	close_pager();
#else
	/* Killing the child will cause a SIGPIPE to come in. */
	if (pager_proc > 0)
		kill(pager_proc,SIGTERM);
#endif
#endif
}
