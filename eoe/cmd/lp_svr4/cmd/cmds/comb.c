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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/cmds/RCS/comb.c,v 1.1 1992/12/14 11:28:19 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#include <assert.h>
#include <signal.h>
#include <string.h>

#include "lp.h"
#include "msgs.h"

#define	WHO_AM_I	I_AM_COMB
#include "oam.h"

#define NULL 0

char message[MSGMAX],
     reply[MSGMAX];

void reject(), accept(), enable(), disable();

void
main(argc, argv)
int argc;
char *argv[];
{
    char *p;

    if (p = strrchr(argv[0], '/'))
	p++;
    else
	p = argv[0];
    
    strncpy(who_am_i+3, p, 14);

    if (STREQU(p, "reject")) reject(argc, argv);
    if (STREQU(p, "accept")) accept(argc, argv);
    if (STREQU(p, "enable")) enable(argc, argv);
    if (STREQU(p, "disable")) disable(argc, argv);
}

void startup(), cleanup(), err_exit();

#if	defined(__STDC__)
void catch();
#else
int catch();
#endif

void
reject(argc, argv)
int argc;
char *argv[];
{
    int i, c, dests = 0, rc = 0;
    char *dest, *reason = "unknown reason";
    int type, size;
    short status;
    extern char *optarg;
    extern int optind, opterr, optopt;

    if(argc == 1) {
        LP_OUTMSG(ERROR, E_CMB_USREJECT);
	cleanup();	
	exit(argc == 1);
    }

    if(STREQU(argv[1], "-?")) {
usage:
        LP_OUTMSG(INFO, E_CMB_USREJECT);
        cleanup();
        exit(argc == 1);
    }

    startup();

    opterr = 0; /* disable printing of errors by getopt */
    for(i = 1; i < argc; i++) {
	dest = argv[i];
	if(*(dest) == '-') {
	    optind = i;
	    c = getopt(argc, argv, "r:");
	    switch (c) {
	    case 'r':
		if (isprinter(optarg) || isclass(optarg))
		    optind--;
		else
		{
		    reason = optarg;
		    if ((size_t) strlen(reason) > 1024)
			reason[1024] = '\0';
		}
		break;

	    case '?':
		if (optopt == '?')
		    goto usage;
		LP_ERRMSG1 (ERROR, E_LP_OPTION, argv[optind-1]);
		err_exit();
	    }
	    i = optind-1;
    	} else {
    	    dests++;
    	    /* reject(dest, reason) */
    	    size = putmessage(message, S_REJECT_DEST, dest, reason);
	    assert(size != -1);
    	    if (msend(message)) {
		LP_ERRMSG(ERROR, E_LP_MSEND);
		err_exit();
	    }
    	    if ((type = mrecv(reply, sizeof(reply))) == -1) {
		LP_ERRMSG(ERROR, E_LP_MRECV);
		err_exit();
	    }
    	    if (type != R_REJECT_DEST
		 || getmessage(reply, type, &status) == -1) {
		LP_ERRMSG1(ERROR, E_LP_BADREPLY, type);
		err_exit();
	    }
	    switch (status) {
	    case MOK:
                LP_OUTMSG1(INFO, E_CMB_NOREQ, dest);
		continue;
	    case MERRDEST:
		LP_ERRMSG1(WARNING, E_CMB_SECREJ, dest);
		break;
	    case MNODEST:
		LP_ERRMSG1(ERROR, E_LP_DSTUNK, dest);
		rc = 1;
		break;
	    case MNOPERM:
		LP_ERRMSG (ERROR, E_LP_NOTADM);
		rc = 1;
		break;
	    default:
		LP_ERRMSG1 (ERROR, E_LP_BADSTATUS, status);
		rc = 1;
	    }
	}
    }

    cleanup();

    if(dests == 0) {
	LP_ERRMSG(ERROR, E_LP_NODEST);
	exit(1);
    }
    exit(rc);
}

void
accept(argc, argv)
int argc;
char *argv[];
{
    int size, type, i, rc = 0;
    short status;
    char *dest, *strcpy();

    if(argc == 1) {
        LP_OUTMSG(ERROR, E_CMB_USACCEPT);
	cleanup();
	exit(argc == 1);
    }

    if(STREQU(argv[1], "-?")) {
usage:
        LP_OUTMSG(INFO, E_CMB_USACCEPT);
        cleanup();
        exit(argc == 1);
    }

    startup();

    for(i = 1; i < argc; i++) {
	dest = argv[i];
	if (STREQU(dest, "-?"))
	    goto usage;
	size = putmessage(message, S_ACCEPT_DEST, dest);
	assert(size != -1);
	if (msend(message)) {
	    LP_ERRMSG(ERROR, E_LP_MSEND);
	    err_exit();
	}
	if ((type = mrecv(reply, sizeof(reply))) == -1) {
	    LP_ERRMSG(ERROR, E_LP_MRECV);
	    err_exit();
	}
	if (type != R_ACCEPT_DEST || getmessage(reply, type, &status) == -1) {
	    LP_ERRMSG1 (ERROR, E_LP_BADREPLY, type);
	    err_exit();
	}

	switch (status) {
	case MOK:
            LP_OUTMSG1(INFO, E_CMB_ACCEPTING, dest);
	    continue;
	case MERRDEST:
	    LP_ERRMSG1(WARNING, E_CMB_SECACC, dest);
	    rc = 1;
	    break;
	case MNODEST:
	    LP_ERRMSG1(ERROR, E_LP_DSTUNK, dest);
	    rc = 1;
	    break;
	case MNOPERM:
	    LP_ERRMSG (ERROR, E_LP_NOTADM);
	    rc = 1;
	    break;
	default:
	    LP_ERRMSG1 (ERROR, E_LP_BADSTATUS, status);
	    rc = 1;
	}
    }

    cleanup();
    exit(rc);
}

void
enable(argc, argv)
int argc;
char *argv[];
{
    int i, type, size, rc = 0;
    short status;
    char *dest, *strcpy();

    if(argc == 1) {
        LP_OUTMSG(ERROR, E_CMB_USENABLE);
        cleanup();
        exit(argc == 1);
    } 

    if(STREQU(argv[1], "-?")) {
usage:
        LP_OUTMSG(INFO, E_CMB_USENABLE);
        cleanup();
        exit(argc == 1);
    }

    startup();

    for(i = 1; i < argc; i++) {
	dest = argv[i];
	if (STREQU(dest, "-?"))
	    goto usage;
	if (isclass(dest)) {
		LP_ERRMSG1 (ERROR, E_CMB_ENACLASS, dest);
		continue;	/* MR bl88-02715 */
	}
	size = putmessage(message, S_ENABLE_DEST, dest);
	assert(size != -1);
	if (msend(message)) {
	    LP_ERRMSG(ERROR, E_LP_MSEND);
	    err_exit();
	}
	if ((type = mrecv(reply, sizeof(reply))) == -1) {
	    LP_ERRMSG(ERROR, E_LP_MRECV);
	    err_exit();
	}
	if (type != R_ENABLE_DEST || getmessage(reply, type, &status) == -1) {
	    LP_ERRMSG1 (ERROR, E_LP_BADREPLY, type);
	    err_exit();
	}

	switch (status) {
	case MOK:
            LP_OUTMSG1(INFO, E_CMB_ENABLED, dest);
	    continue;
	case MERRDEST:
	    LP_ERRMSG1(WARNING, E_CMB_SECENA, dest);
	    rc = 1;
	    break;
	case MNODEST:
	    LP_ERRMSG1(ERROR, E_LP_DSTUNK, dest);
	    rc = 1;
	    break;
	case MNOPERM:
	    LP_ERRMSG (ERROR, E_LP_NOTADM);
	    rc = 1;
	    break;
	default:
	    LP_ERRMSG1 (ERROR, E_LP_BADSTATUS, status);
	}
    }

    cleanup();

    exit(rc);
}

#define TRUE	1
#define FALSE	0

void
disable(argc, argv)
int argc;
char **argv;
{
    int rc = 0, cancel = FALSE, Wait = FALSE,
	dests = 0, type, size, c;
    short status, when;
    char *reason = "unknown reason", *dest, *req_id;
    extern char *optarg;
    extern int optind, opterr, optopt;

    if(argc == 1) {
        LP_OUTMSG(ERROR, E_CMB_USDISABLE);
        cleanup();
        exit(argc == 1);
    } 

    if(STREQU(argv[1], "-?")) {
usage:
        LP_OUTMSG(INFO, E_CMB_USDISABLE);
        cleanup();
        exit(argc == 1);
    }

    opterr = 0; /* disable printing of errors by getopt */
    while ((c = getopt(argc, argv, "cWr:")) != -1)
	switch(c) {
	case 'c':
	    if (cancel)
		LP_ERRMSG1 (WARNING, E_LP_2MANY, 'c');
	    cancel = TRUE;
	    break;
	case 'W':
	    if (Wait)
		LP_ERRMSG1 (WARNING, E_LP_2MANY, 'W');
	    Wait = TRUE;
	    break;
	case 'r':
	    if (isprinter(optarg) || isclass(optarg))
		optind--;
	    else
	    {
		reason = optarg;
		if ((size_t) strlen(reason) > 1024)
		    reason[1024] = '\0';
	    }
	    break;

	case '?':
	    if (optopt == '?')
		goto usage;
	    LP_ERRMSG1 (ERROR, E_LP_OPTION, argv[optind-1]);
	    exit(1);
	}

    if (Wait && cancel) {
	LP_ERRMSG(ERROR, E_LP_OPTCOMB);
	exit(1);
    }

    startup();

    for ( ;optind < argc; optind++) {
	dest = argv[optind];
	if(*(dest) == '-') {
	    c = getopt(argc, argv, "r:");
	    switch (c) {
	    case 'r':
		if (isprinter(optarg) || isclass(optarg))
		    optind--;
		else
		    reason = optarg;
		break;
	    case '?':
		LP_ERRMSG1 (ERROR, E_LP_OPTION, argv[optind-1]);
		err_exit();
	    }
	    optind--;
	} else {
	    if (isclass(dest)) {
		LP_ERRMSG1 (ERROR, E_CMB_DISCLASS, dest);
		continue;		/* MR bl88-02715 */
	    }
	    dests++;
	    /* disable(dest, reason, cancel, Wait); */
	    when = (Wait) ? 1 : ((cancel) ? 2 : 0);
    	    size = putmessage(message, S_DISABLE_DEST, dest, reason, when);
	    assert(size != -1);
    	    if (msend(message)) {
		LP_ERRMSG(ERROR, E_LP_MSEND);
		err_exit();
	    }
    	    if ((type = mrecv(reply, sizeof(reply))) == -1) {
		LP_ERRMSG(ERROR, E_LP_MRECV);
		err_exit();
	    }
    	    if (type != R_DISABLE_DEST
		 || getmessage(reply, type, &status, &req_id) == -1) {
		LP_ERRMSG1 (ERROR, E_LP_BADREPLY, type);
		err_exit();
	    }
	    switch (status) {
	    case MOK:
		if (req_id && *req_id)
                    LP_OUTMSG1(INFO, E_CMB_CANCELLED, req_id);
                LP_OUTMSG1(INFO, E_CMB_DISABLED, dest);
		break;
	    case MERRDEST:
		LP_ERRMSG1(WARNING, E_CMB_SECDIS, dest);
		break;
	    case MNODEST:
		LP_ERRMSG1(ERROR, E_LP_DSTUNK, dest);
		rc = 1;
		break;
	    case MNOPERM:
		LP_ERRMSG (ERROR, E_LP_NOTADM);
		rc = 1;
		break;
	    default:
		LP_ERRMSG1 (ERROR, E_LP_BADSTATUS, status);
		rc = 1;
	    }
	}
    }

    cleanup();

    if(dests == 0) {
	LP_ERRMSG(ERROR, E_LP_NODEST);
	exit(1);
    }

    exit(rc);
}

void
startup()
{
#if	defined(__STDC__)
    void	catch();
#endif
    
    if (mopen()) {LP_ERRMSG(ERROR, E_LP_MOPEN); exit(1);}

    if(signal(SIGHUP, SIG_IGN) != SIG_IGN)
	signal(SIGHUP, catch);
    if(signal(SIGINT, SIG_IGN) != SIG_IGN)
	signal(SIGINT, catch);
    if(signal(SIGQUIT, SIG_IGN) != SIG_IGN)
	signal(SIGQUIT, catch);
    if(signal(SIGTERM, SIG_IGN) != SIG_IGN)
	signal(SIGTERM, catch);
}

/* catch -- catch signals */

#if	defined(__STDC__)
void	catch()
#else
int	catch()
#endif
{

    signal(SIGHUP, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    err_exit();
}

void
cleanup()
{
    (void)mclose ();
}

void
err_exit()
{
    cleanup();
    exit(1);
}
