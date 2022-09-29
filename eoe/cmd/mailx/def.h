/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)def.h	5.1 (Berkeley) 6/6/85
 */

#include <sys/param.h>		/* includes <sys/types.h> */
#if defined(SVR3) || defined(_SVR4_SOURCE)
#include <sys/types.h>		/* but not in SVR3 */
#endif
#include <signal.h>
#include <stdio.h>
#ifndef	USG
#include <sgtty.h>
#else
#include <termio.h>
#endif
#include "local.h"
#include <limits.h>
#include <string.h>

#undef isalpha
#undef isdigit

/*
 * Mail -- a mail program
 *
 * Commands are:
 *	t <message list>		print out these messages
 *	r <message list>		reply to messages
 *	m <user list>			mail to users (analogous to send)
 *	e <message list>		edit messages
 *	c [directory]			chdir to dir or home if none
 *	x				exit quickly
 *	w <message list> file		save messages in file
 *	q				quit, save remaining stuff in mbox
 *	d <message list>		delete messages
 *	u <message list>		undelete messages
 *	h				print message headers
 *
 * Author: Kurt Shoens (UCB) March 25, 1978
 */


#define	ESCAPE		'~'		/* Default escape for sending */
#define	NMLSIZE		20		/* max names in a message list */
#define	PATHSIZE	PATH_MAX	/* Size of pathnames throughout */
#define	NAMESIZE	NAME_MAX	/* Max size of user name */
#define	HSHSIZE		19		/* Hash size for aliases and vars */
#define	HDRFIELDS	3		/* Number of header fields */
#define	LINESIZE	BUFSIZ		/* max readable line width */
#define	STRINGSIZE	((unsigned) 128)/* Dynamic allocation units */
#define	MAXARGC		1024		/* Maximum list of raw strings */
#define	NOSTR		((char *) 0)	/* Null string pointer */
#define	MAXEXP		25		/* Maximum expansion of aliases */
#define DEFAULTCOLS	72		/* Default screen width for output */
#define DEFAULTROWS	24		/* Default screen depth for output */
#define	equal(a, b)	(strcmp(a,b)==0)/* A nice function to string compare */

struct message {
	short	m_flag;			/* flags, see below */
	short	m_offset;		/* offset in block of message */
	u_int	m_block;		/* block number of this message */
	u_long	m_size;			/* Bytes in the message */
	u_int	m_lines;		/* Lines in the message */
	u_int	m_dlines;		/* Displayable lines in the message
					   (m_lines - ignored headers) */
};

/*
 * flag bits.
 */

#define	MUSED		(1<<0)		/* entry is used, but this bit isn't */
#define	MDELETED	(1<<1)		/* entry has been deleted */
#define	MSAVED		(1<<2)		/* entry has been saved */
#define	MTOUCH		(1<<3)		/* entry has been noticed */
#define	MPRESERVE	(1<<4)		/* keep entry in sys mailbox */
#define	MMARK		(1<<5)		/* message is marked! */
#define	MODIFY		(1<<6)		/* message has been modified */
#define	MNEW		(1<<7)		/* message has never been seen */
#define	MREAD		(1<<8)		/* message has been read sometime. */
#define	MSTATUS		(1<<9)		/* message status has changed */
#define	MBOX		(1<<10)		/* Send this to mbox, regardless */

/*
 * Format of the command description table.
 * The actual table is declared and initialized
 * in cmdtab.c
 */

struct cmd {
	char	*c_name;		/* Name of command */
	int	(*c_func)();		/* Implementor of the command */
	short	c_argtype;		/* Type of arglist (see below) */
	short	c_msgflag;		/* Required flags of messages */
	short	c_msgmask;		/* Relevant flags of messages */
};

/*
 * Format of the tilde escape command description table.
 * The actual table is declared and initialized
 * in cmdtab.c
 */

struct tcmd {
	char	*tc_name;		/* Name of command */
	int	(*tc_func)();		/* Implementor of the command */
	short	tc_argtype;		/* Type of arglist (see below) */
};

/*
 *  XPG4: Certain commands are invalid within a mailx start-up file.
 */
struct bcmd {
	char	*bc_cmdname;		/* Name of command */
};

/* Yechh, can't initialize unions */

#define	c_minargs c_msgflag		/* Minimum argcount for RAWLIST */
#define	c_maxargs c_msgmask		/* Max argcount for RAWLIST */

/*
 * cmdtab argument types.
 */

#define	MSGLIST		0	/* Message list type */
#define	STRLIST		1	/* A pure string */
#define	RAWLIST		2	/* Shell string list */
#define	NOLIST		3	/* Just plain 0 */
#define	NDMLIST		4	/* Message list, no defaults */
#define	ALIASLIST	5	/* A string list of addresses */

#define	P	040		/* Autoprint dot after command */
#define	I	0100		/* Interactive command bit */
#define	M	0200		/* Legal from send mode bit */
#define	W	0400		/* Illegal when read only bit */
#define	F	01000		/* Is a conditional command */
#define	T	02000		/* Is a transparent command */
#define	R	04000		/* Cannot be called from collect */

/*
 * tcmdtab argument types.
 */

#define TNOARG	0		/* No arguments */
#define TSTR	1		/* String only */
#define THDR	2		/* Header pointer only */
#define TBUF	3		/* IO buffers only */
#define THDRSTR 4		/* Header pointer and string */
#define TBUFSTR 5		/* IO buffers and string */
#define TBUFHDR 6		/* IO buffers and header pointer */

/*
 * Suppression flag values (for send_msg() routine).
 */

#define NO_SUPPRESS	0	/* Don't suppress headers */
#define SUPPRESS_IGN	1	/* Suppress ignored headers */
#define SUPPRESS_ALL	2	/* Suppress all headers */

/*
 * S[ave]/s[ave] and C[opy]/c[opy] flag values (for send_msg() routine).
 */
#define	ISCAPSAVE	1
#define	NOTCAPSAVE	0
#define	ISCAPCOPY	1
#define	NOTCAPCOPY	0

/*
 * Values for readonly flag.
 */

#define NOT_READONLY	0	/* mailfile is read and writeable */
#define NO_ACCESS	1	/* we only have read access to mailfile */
#define RO_LOCKED	2	/* mailfile is in use, .rolock prevents
				   our having write access */

/*
 * Oft-used mask values
 */

#define	MMNORM		(MDELETED|MSAVED)/* Look at both save and delete bits */
#define	MMNDEL		MDELETED	/* Look only at deleted bit */

/*
 * Structure used to return a break down of a head
 * line (hats off to Bill Joy!)
 */

struct headline {
	char	*l_from;	/* The name of the sender */
	char	*l_tty;		/* His tty string (if any) */
	char	*l_date;	/* The entire date string */
};

#define	GTO		(1<<0)		/* Grab To: line */
#define	GSUBJECT	(1<<1)		/* Likewise, Subject: line */
#define	GCC		(1<<2)		/* And the Cc: line */
#define	GBCC		(1<<3)		/* And also the Bcc: line */
#define	GNL		(1<<4)		/* Print blank line after */
#define	GDEL		(1<<5)		/* Entity removed from list */
#define	GCOMMA		(1<<6)		/* detract puts in commas */
#define GRT		(1<<7)		/* Grab Reply-to: line */
#define GRR		(1<<8)		/* Grab Return-receipt-to: line */
#define GIRT		(1<<9)		/* Grab In-Reply-To line */
#define GREF		(1<<10)		/* Grab References line */
#define GKEY		(1<<11)		/* Grab Keywords line */
#define GCOM		(1<<12)		/* Grab Comments line */
#define GEN		(1<<13)		/* Grab Encrypted line */
#define GEX		(1<<14)		/* Grab extended headers */
#define GSEP		(1<<15)		/* Print separator string after */
#define	GTOOLATE	(1<<16)		/* Too late to fix too-long strings */

#define	GREGHDR	(GTO|GSUBJECT|GCC|GBCC|GRT|GRR|GIRT|GREF|GKEY|GCOM|GEN)
#define GALLHDR (GREGHDR|GEX)
#define GMASK	(GALLHDR)
				/* Mask of places from whence */
/*
 * Structure used to pass about the current
 * state of the user-typed message header.
 */

#define MAX_EX_HDRS 256

struct header {
	char	*h_to;			/* Dynamic "To:" string */
	char	*h_subject;		/* Subject string */
	char	*h_cc;			/* Carbon copies string */
	char	*h_bcc;			/* Blind carbon copies */
	char	*h_replyto;		/* Reply-to: */
	char	*h_rtnrcpt;		/* Return-receipt-to: */
	char    *h_inreplyto;		/* In-Reply-To: */
	char	*h_keywords;		/* Keywords: */
	char	*h_references;		/* References: */
	char	*h_comments;		/* Comments: */
	char	*h_encrypt;		/* Encrypt: */
	int	 h_seq;			/* Sequence for optimization */
	int	 h_optusedmask;		/* Mask of non-NULL opt headers */
	char	*h_ex_hdrs[MAX_EX_HDRS];/* Extended headers array */
};

/*
 * Structure of namelist nodes used in processing
 * the recipients of mail and aliases and all that
 * kind of stuff.
 */

struct name {
	struct	name *n_flink;		/* Forward link in list. */
	struct	name *n_blink;		/* Backward list link */
	int	n_type;			/* From which list it came */
	char	*n_name;		/* This fella's name */
};

/*
 * Structure of a variable node.  All variables are
 * kept on a singly-linked list of these, rooted by
 * "variables"
 */

struct var {
	struct	var *v_link;		/* Forward link to next variable */
	char	*v_name;		/* The variable's name */
	char	*v_value;		/* And it's current value */
};

struct group {
	struct	group *ge_link;		/* Next person in this group */
	char	*ge_name;		/* This person's user name */
};

struct grouphead {
	struct	grouphead *g_link;	/* Next grouphead in list */
	char	*g_name;		/* Name of this group */
	struct	group *g_list;		/* Users in group. */
};

#define	NIL	((struct name *) 0)	/* The nil pointer for namelists */
#define	NONE	((struct cmd *) 0)	/* The nil pointer to command tab */
#define TNONE	((struct tcmd *) 0)	/* The nil pointer to tcommand tab */
#define	NOVAR	((struct var *) 0)	/* The nil pointer to variables */
#define	NOGRP	((struct grouphead *) 0)/* The nil grouphead pointer */
#define	NOGE	((struct group *) 0)	/* The nil group pointer */

/*
 * Structure of the hash table of ignored header fields
 */
struct ignore {
	struct ignore	*i_link;	/* Next ignored field in bucket */
	char		*i_field;	/* This ignored field */
};

/*
 * Token values returned by the scanner used for argument lists.
 * Also, sizes of scanner-related things.
 */

#define	TEOL	0			/* End of the command line */
#define	TNUMBER	1			/* A message number */
#define	TDASH	2			/* A simple dash */
#define	TSTRING	3			/* A string (possibly containing -) */
#define	TDOT	4			/* A "." */
#define	TUP	5			/* An "^" */
#define	TDOLLAR	6			/* A "$" */
#define	TSTAR	7			/* A "*" */
#define	TOPEN	8			/* An '(' */
#define	TCLOSE	9			/* A ')' */
#define TPLUS	10			/* A '+' */
#define TERROR	11			/* Error detected in message list */

#define	REGDEP	2			/* Maximum regret depth. */
#define	STRINGLEN	64		/* Maximum length of string token */

/*
 * Constants for conditional commands.  These describe whether
 * we should be executing stuff or not.
 */

#define	CANY		0		/* Execute in send or receive mode */
#define	CRCV		1		/* Execute in receive mode only */
#define	CSEND		2		/* Execute in send mode only */

/*
 * Kludges to handle the change from setexit / reset to setjmp / longjmp
 */

#define	setexit()	setjmp(srbuf)
#define	reset(x)	longjmp(srbuf, x)

/*
 * VM/UNIX has a vfork system call which is faster than forking.  If we
 * don't have it, fork(2) will do . . .
 */

#ifndef VMUNIX
#define	vfork()	fork()
#endif
#ifndef	SIGRETRO
#define	sigchild()
#endif

/*
 * 4.2bsd signal interface help...
 */
#ifdef VMUNIX
#define	sigset(s, a)	signal(s, a)
#define	sigsys(s, a)	signal(s, a)
#endif
#if defined(SVR3) || defined(_SVR4_SOURCE)
#define	sigsys(s, a)	signal(s, a)
#endif

/*
 * Truncate a file to the last character written. This is
 * useful just before closing an old file that was opened
 * for read/write.
 */
#define trunc(stream)	ftruncate(fileno(stream), (long) ftell(stream))

/*
 * Headers/body sentinal string for ~vh and ~eh commands.
 */
#define END_HDRS "--------------------\n"
/*
 * Forward declarations of routine types to keep lint and cc happy.
 */

FILE	*Fdopen();
FILE	*collect();
FILE	*infix();
FILE	*mesedit();
FILE	*mespipe();
FILE	*setinput();
char	**unpack();
char	*addto();
char	*arpafix();
char	*calloc();
char	*copy();
char	*copyin();
char	*detract();
char	*expand();
char	*expandbang();
char	*hfield();
char	*index();
char	*name1();
char	*nameof();
char	*nextword();
char	*getenv();
char	*getfilename();
char	*hcontents();
char	*netmap();
char	*netname();
char	*readtty();
char	*reedit();
char	*renamepath();
char	*revarpa();
char	*rindex();
char	*rpair();
char	*salloc();
char	*savestr();
char	*skin();
char	*snarf();
char	*value();
char	*vcopy();
char	*yankword();
off_t	fsize();
#ifndef _SVR4_SOURCE
#ifndef SVR3
#ifndef VMUNIX
int	(*sigset())();
#endif
#endif
#endif
struct	cmd	*lex();
struct  tcmd    *tlex();
struct	grouphead	*findgroup();
struct	name	*cat();
struct	name	*delname();
struct	name	*elide();
struct	name	*extract();
struct	name	*gexpand();
struct	name	*map();
struct	name	*outof();
struct	name	*put();
struct	name	*usermap();
struct	name	*verify();
struct	var	*lookup();
long	transmit();
int	icequal();
int	cmpdomain();
char *getaddr();
int extracthead();
FILE *mktmpfil();
FILE *mkntmpfil();
