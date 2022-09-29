/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)glob.h	5.1 (Berkeley) 6/6/85
 */

/*
 * Extern declarations for globally accessable variables.
 */

#include "def.h"

extern int	msgCount;		/* Count of messages read in */
extern int	mypid;			/* Current process id */
extern int	rcvmode;		/* True if receiving mail */
extern int	sawcom;			/* Set after first command */
extern int	hflag;			/* Sequence number for network -h */
extern char	*rflag;			/* -r address for network */
extern char	*Tflag;			/* -T temp file for netnews */
extern char	nosrc;			/* Don't source /usr/lib/Mail.rc */
extern char	nolock;			/* Don't lock mailfile */
extern char	noheader;		/* Suprress initial header listing */
extern int	selfsent;		/* User sent self something */
extern int	senderr;		/* An error while checking */
extern int	mailrcerr;		/* An error in mailrc checking */
extern int	edit;			/* Indicates editing a file */
extern int	readonly;		/* Will be unable to rewrite file */
extern int	noreset;		/* String resets suspended */
extern int	sourcing;		/* Currently reading variant file */
extern int	loading;		/* Loading user definitions */
extern int	shudann;		/* Print headers when possible */
extern int	cond;			/* Current state of conditional exc. */
extern FILE	*itf;			/* Input temp file buffer */
extern FILE	*otf;			/* Output temp file buffer */
extern FILE	*pipef;			/* Pipe file we have opened */
extern int	image;			/* File descriptor for image of msg */
extern FILE	*input;			/* Current command input file */
extern char	*editfile;		/* Name of file being edited */
extern char	*sflag;			/* Subject given from non tty */
extern int	outtty;			/* True if standard output a tty */
extern int	intty;			/* True if standard input a tty */
extern int	baud;			/* Output baud rate */
extern char	mbox[PATHSIZE];		/* Name of mailbox file */
extern char	mailname[PATHSIZE];	/* Name of system mailbox */
extern int	uid;			/* The invoker's user id */
extern char	mailrc[PATHSIZE];	/* Name of startup file */
extern char	deadletter[PATHSIZE];	/* Name of #/dead.letter */
extern char	homedir[PATHSIZE];	/* Path name of home directory */
extern char	myname[PATHSIZE];	/* My login id */
extern off_t	mailsize;		/* Size of system mailbox */
extern int	lexnumber;		/* Number of TNUMBER from scan() */
extern char	lexstring[STRINGLEN];	/* String from TSTRING, scan() */
extern int	regretp;		/* Pointer to TOS of regret tokens */
extern int	regretstack[REGDEP];	/* Stack of regretted tokens */
extern char	*stringstack[REGDEP];	/* Stack of regretted strings */
extern int	numberstack[REGDEP];	/* Stack of regretted numbers */
extern struct message	*dot;		/* Pointer to current message */
extern struct message	*message;	/* The actual message structure */
extern struct var *variables[HSHSIZE];/* Pointer to active var list */
extern struct grouphead *groups[HSHSIZE];/* Pointer to active groups */
extern struct ignore *ignore[HSHSIZE];/* Pointer to ignored fields */
extern struct ignore *retain[HSHSIZE];/* Pointer to retained fields */
extern int	nretained;		/* Number of retained fields */
extern char	**altnames;		/* List of alternate names for user */
extern char	**localnames;			/* List of aliases for our local host */
extern int	debug;			/* Debug flag set */
extern int	rmail;			/* Being called as rmail */
extern int	inithdr;		/* Printing initial headers */
extern int	savedegid;		/* Saved effective grp ID from start */
extern int	dasheflg;		/* -e option to see if mail exists */
extern int	dashHflg;		/* -H option print just mail headers */
extern int	ismailx;		/* prog called? 0=='Mail' 1=='mailx' */
extern int	sawnextcom;		/* Set after "next" command */

#include <setjmp.h>

extern jmp_buf	srbuf;

