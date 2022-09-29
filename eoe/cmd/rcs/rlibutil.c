/* RCS library APIs (helpers/protocol) */

/* Copyright 1996 Silicon Graphics, Inc.

This file is part of RCS.

RCS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

RCS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RCS; see the file COPYING.
If not, write to the Free Software Foundation,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

Report problems and direct all questions to:

    rcs-bugs@cs.purdue.edu

*/

/*
 * $Log: rlibutil.c,v $
 * Revision 1.1  1996/06/29 00:38:35  msf
 * Initial revision
 *
 */


#include "rcsbase.h"

#define DEVNULL "/dev/null"

/* Exported variables */
int Ttimeflag;		/* -T set for session (set RCS file time) */
sigjmp_buf rcsApiEnv;	/* environment at entry to RCS library */
char rcsParamName[MAXPATHLEN]; /* rcspathname last passed as parameter */

static RCSCONN processconn = (RCSCONN)-1;	
					/* session running in this process */


/* Structs and tables to track files, connections, etc */
#define MAXCONN 1
			/* table of options/state for each connection/sess */
static struct {
#ifdef has_signal
    signal_type (*interrupt)(int sig); /* application interrupt handler */
#endif
    bool Ttimeflag;		/* does -T flag apply (affect ,v timestamp) */
    bool noquietflag;		/* should we turn off quiet (-q) flag? */
    const char *auth;		/* -w: default is login */
    const char *suffixes;       /* -x: default is ,v/ */
    FILE  *debugout;		/* output file for debug info */
} connection[MAXCONN];


int rcsOpenConnection(host, opts, conn)
    const char *host;		/* server machine; ignored on server */
    struct rcsConnOpts *opts;	/* options (service opt ignored on server) */
    RCSCONN *conn;		/* returned connection object */
{
    /* largely a no-op on the server.  Saves connection information. */
    int conn_num = 0;

    /* Find an available connection.
     * We only support one simultaneous connection per process.
     */
    if (processconn != (RCSCONN)-1) return ERR_TOOMANY_CONNECTIONS;
    processconn = (RCSCONN)conn_num;

    /* Remember connection/session information */
    connection[conn_num].interrupt = opts->interrupt;
    connection[conn_num].Ttimeflag = opts->Ttimeflag;
    connection[conn_num].noquietflag = opts->noquietflag;
    connection[conn_num].auth = /* opts->auth ?
				str_save(opts->auth) : */
				getcaller();
    connection[conn_num].suffixes = opts->suffixes ?
				    str_save(opts->suffixes) :
				    X_DEFAULT;
#ifdef has_signal
    connection[conn_num].interrupt = opts->interrupt;
    rcsusrhdlr = opts->interrupt;
#endif

    /* Redirect diagnostic/error output, as requested (but never to stderr) */
    if (opts->debugout) {
	if (!(connection[conn_num].debugout = fopen(opts->debugout, "w")))
	    return ERR_BAD_FCREATE;
    }
    else connection[conn_num].debugout = fopen(DEVNULL, "w");

    /* Copy per-connection values into global variables
     * (FIX if we allow multiple connections within a process)
     */
    suffixes = connection[conn_num].suffixes;
    interactiveflag = false;		/* library not "interactive" -I */
    cmdid = "rcslib";			/* generic "command" for err msgs */
    quietflag = connection[conn_num].noquietflag &&
	        connection[conn_num].debugout ?
		    false : true;	/* only print info if place to write */

    author = connection[conn_num].auth;
    finform = connection[conn_num].debugout;

    /* Initialize other global variables, as needed */
    rcsParamName[0] = '\0';	/* "cached" RCS file */

    /* Return connection number, and success (0) */
    *conn = (RCSCONN)conn_num;
    return 0;
}


	int
rcsCloseConnection(conn)
	RCSCONN conn;
{
	/* Method for destroying a connection/session.
	 * Within the library (standalone), all that is required
	 * is that the heap associated with the session be freed.
	 */

	/* Map connection to internal representation */
	if ((int)conn >= MAXCONN) return ERR_APPL_ARGS;

	tempunlink();
#ifdef has_signal
	rcsusrhdlr = NULL;
#endif
	processconn = (RCSCONN)-1;
	if (finform != stderr) fclose(finform);

	/* For multiple sessions in a single process, free per-session
	 * memory here.
	 */
	return 0;
}

/* Needed for DOS and OS/2 ?? */
/* By calling exiterr(), routines may return to the library interface,
 * indicate a system error, and not exit the program.
 */
	void
exiterr()
{
	/* Shortcircuit memory frees, stdio usage, since this may
	 * be called in an interrupt, where most state info is suspect.
	 * Just clean up files, and exit.
	 */
	ORCSerror();		/* normally done for file assoc'd w/session */
	dirtempunlink();	/* ditto */
	if ((int)processconn >= 0)
	    rcsCloseConnection(processconn);
	siglongjmp(rcsApiEnv, ERR_FATAL);  /* return to caller w/ fatal error */
	_exit(EXIT_FAILURE);	/* just in case */
}
	
	int
rcsOpenArch(openforwrite, mustexist, rcspath)
	bool openforwrite;
	bool mustexist;
	const char *rcspath;
{
	/* Open an RCS file (via pairnames(), and initializing the
	 * lexer (all the way up through calling getadmin()).
	 *
	 * Returns: 0 on success (if archive file exists)
	 *          1 on success (if archive file does not exist, but not
	 *			  required to exist)
	 *          ERR_OPEN_ARCH < 0 (on generic error, such as EPERM, etc.,
	 *			       including archive not existing, if
	 *				mustexist is set)
	 *	    ERR_TOOMANY_ARCH_OPEN (above limit, now 1)
	 *	    ERR_NETWORK_DOWN (transmission cut)
	 *
	 * Additional bookkeeping: whether the revision tree has
	 * been parsed yet, and associating the open file with
	 * the session, so that when the session is destroyed, all
	 * open files are also closed.
	 */
	
	int i, j;
	int rc;
	static const char *pairpath = rcsParamName;

	/* Copy string to rcsParamName, because pairnames may set workname
	 * or RCSname to the string, so it must be static.  Also, rcsParamName
	 * is used to recognize that a subsequent API call is operating
	 * on the same archive.
	 */
	strcpy(rcsParamName, rcspath);
	i = pairnames(1, &pairpath,
		     (openforwrite ? rcswriteopen : rcsreadopen),
			mustexist, quietflag); 

	if (i) { /* File has been initialized (and opened if it exists) */

		if (i > 0) {	/* archive file exists */
		    rc = 0;
		}
		else {		/* archive file does not exist (okay) */
		    rc = 1;
		}
	}
	else    rc = ERR_OPEN_ARCH;

	fcopy = NULL;
	frewrite = NULL;

	return rc;
}

	int
rcsCloseArch()
{
	/* Write portions of RCS files to disk as needed 
	 * Unlink per-file temp files, lock file.
	 * Reclaim per-file memory.
	 */

	/* if(file-needs-to-be-written-out && no-errors)
	 * {
	 *    putadmin();
	 *    if (Head) puttree(Head, frewrite);
	 *    putdesc(X, textfile);  NB: X is true for rcs -t only;
	 *			 	 change desc even if not new file
	 *    cmd-specific-cleanup
	 *    donerewrite(has-changed?, Tflag?RCSstat.st_mtime : -1);
	 * }
	 */

	Izclose(&finptr);	/* tests for NULL, and sets to NULL */
	Ozclose(&fcopy);
	ORCSclose();		/* close frewrite; close lockfile */
	

	/* For a number of reasons, a session can only handle
	 * a single open file at a time.  One of them is that
	 * makedirtemp/dirtempunlink use statics which are assumed
	 * to be reusable for the next file.
	 *
	 * We extend that reliance here, and invoke dirtempunlink
	 * to do some per-file cleanup.
	 */
	 dirtempunlink();	/* unlinks lockfile for RCS file */

	ffree();		/* free all per-file memory */
	rcsParamName[0] = '\0';	/* no file "cached" */

	return 0;
}

