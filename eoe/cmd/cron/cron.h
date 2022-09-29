/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)cron:cron.h	1.4" */
#ident	"$Revision: 1.12 $"

#define FALSE		0
#define TRUE		1
#define CR_MINUTE	60L
#define CR_HOUR		60L*60L
#define CR_DAY		24L*60L*60L
#define	NQUEUE		27		/* number of queues available */
#define	ATEVENT		0		/* not used */
#define BATCHEVENT	1		/* only used in "batch" command */
#define CRONEVENT	26

#define ADD		'a'
#define DELETE		'd'
#define	AT		'a'
#define TRIX_AT		'A'
#define CRON		'c'
#define TRIX_CRON	'C'

#define	QUE(x)		('a'+(x))

#define UNAMESIZE	20	/* max chars in a user name */
#define	FLEN		255
#define	LLEN		UNAMESIZE

/* structure used for passing messages from the
   at and crontab commands to the cron			*/

struct	message {
	char	etype;
	char	action;
	char	fname[FLEN];
	char	logname[LLEN];
	char	label[FLEN];
};

/* anything below here can be changed */

#define CRONDIR		"/var/spool/cron/crontabs"
#define MACCRONDIR	"/var/spool/cron/crontabs/:mac"
#define ATDIR		"/var/spool/cron/atjobs"
#define MACATDIR	"/var/spool/cron/atjobs/:mac"
#define ACCTFILE	"/var/cron/log"
#define CRONALLOW	"/etc/cron.d/cron.allow"
#define CRONDENY	"/etc/cron.d/cron.deny"
#define ATALLOW		"/etc/cron.d/at.allow"
#define ATDENY		"/etc/cron.d/at.deny"
#define PROTO		"/etc/cron.d/.proto"
#define	QUEDEFS		"/etc/cron.d/queuedefs"
#define	FIFO		"/etc/cron.d/FIFO"
#define FIFOLBL		"msenequal/mintequal"

#define SHELL		_PATH_BSHELL	/* shell to execute */

#define CTLINESIZE	1000	/* max chars in a crontab line */
