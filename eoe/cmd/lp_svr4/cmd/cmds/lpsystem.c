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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/cmds/RCS/lpsystem.c,v 1.1 1992/12/14 11:28:40 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/



/*==================================================================*/
/*
*/
#include	<sys/utsname.h>
#include	<stdio.h>
#include	<tiuser.h>
#include	<netconfig.h>
#include	<netdir.h>

#include	"lp.h"
#include	"systems.h"
#include	"msgs.h"
#include	"boolean.h"
#include	"debug.h"

#define WHO_AM_I	I_AM_LPSYSTEM
#include "oam.h"

#define	DEFAULT_TIMEOUT	-1
#define	DEFAULT_RETRY	10

static	int	Timeout;
static	int	Retry;
static	char	*Sysnamep;
static	char	*Protocolp;
static	char	*Timeoutp;
static	char	*Retryp;
static	char	*Commentp;

#ifdef	__STDC__
static	int	NotifyLpsched (int, char *);
static	void	SecurityCheck (void);
static	void	TcpIpAddress (void);
static	void	ListSystems (char * []);
static	void	RemoveSystems (char * []);
static	void	AddModifySystems (char * []);
static	void	formatsys (SYSTEM *);
#else
static	int	NotifyLpsched ();
static	void	SecurityCheck ();
static	void	TcpIpAddress ();
static	void	ListSystems ();
static	void	RemoveSystems ();
static	void	AddModifySystems ();
static	void	formatsys ();
#endif

/*==================================================================*/

/*==================================================================*/
/*
*/
#ifdef	__STDC__
int
main (int argc, char * argv [])
#else
int
main (argc, argv)

int	argc;
char	*argv [];
#endif
{
		int	c;
		boolean	lflag = False,
			rflag = False,
			Aflag = False,
			badOptions = False;
	extern	int	opterr,
			optind;
	extern	char	*optarg;
 

	while ((c = getopt(argc, argv, "t:T:R:y:lrA?")) != EOF)
	switch (c & 0xFF)
	{
	case 't':
		if (Protocolp)
		{
                    LP_ERRMSG1(ERROR, E_SYS_MANYOPT, "t");
                    return 1;
		}
		Protocolp = optarg;
		if (! STREQU(NAME_S5PROTO, Protocolp) &&
		    ! STREQU(NAME_BSDPROTO, Protocolp))
		{
                        LP_ERRMSG2(ERROR, E_SYS_SUPPROT,
			   	NAME_S5PROTO, NAME_BSDPROTO);
			return	1;
		}
		break;
		
	case 'T':
		if (Timeoutp)
		{
                        LP_ERRMSG1(ERROR, E_SYS_MANYOPT, "T");
			return	1;
		}
		Timeoutp = optarg;
		if (*Timeoutp == 'n')
			Timeout = -1;
		else
		if (sscanf (Timeoutp, "%d", &Timeout) != 1 || Timeout < 0)
		{
                        LP_ERRMSG1(ERROR, E_SYS_BADTIMEOUT, Timeoutp);
			return	1;
		}
		break;
		
	case 'R':
		if (Retryp)
		{
                        LP_ERRMSG1(ERROR, E_SYS_MANYOPT, "R");
			return	1;
		}
		Retryp = optarg;
		if (*Retryp == 'n')
			Retry = -1;
		else 
		if (sscanf (Retryp, "%d", &Retry) != 1 || Retry < 0)
		{
                        LP_ERRMSG1(ERROR, E_SYS_BADRETRY, Retryp);
			return	1;
		}
		break;
		
	case 'y':
		if (Commentp)
		{
                        LP_ERRMSG1(ERROR, E_SYS_MANYOPT, "y");
			return	1;
			}
		Commentp = optarg;
		break;

	case 'l':
		if (lflag)
		{
                        LP_ERRMSG1(ERROR, E_SYS_MANYOPT, "l");
			return	1;
		}
		lflag++;
		break;
		
	case 'r':
		if (rflag)
		{
                        LP_ERRMSG1(ERROR, E_SYS_MANYOPT, "r");
			return	1;
		}
		rflag++;
		break;

	case 'A':
		if (Aflag)
		{
                        LP_ERRMSG1(ERROR, E_SYS_MANYOPT, "A");
			return	1;
		}
		Aflag++;
		break;

	default:
                LP_ERRMSG1(ERROR, E_SYS_OPTION, c & 0xFF);
		return	1;
		
	case '?':
                LP_OUTMSG(INFO, E_SYS_USAGE);
		return	0;
	}

	/*
	**  Check for valid option combinations.
	**
	**  The '-A' option is mutually exclusive.
	**  The '-l' option is mutually exclusive.
	**  The '-r' option is mutually exclusive.
	*/
	if (Aflag && (Protocolp || Timeoutp || Retryp || Commentp))
		badOptions = True;

	if (lflag &&
	   (Protocolp || Timeoutp || Retryp || Commentp || rflag || Aflag))
		badOptions = True;

	if (rflag && (Protocolp || Timeoutp || Retryp || Commentp || Aflag))
		badOptions = True;

	if (badOptions)
	{
                LP_ERRMSG(ERROR, E_SYS_IMPRUS);
		return	1;
	}

	/*
	**	Lets do some processing.
	**	We'll start with the flags.
	*/
	if (Aflag)
	{
		TcpIpAddress ();
		/*NOTREACHED*/
	}
	if (lflag)
	{
		ListSystems (&argv [optind]);
		/*NOTREACHED*/
	}
	if (rflag)
	{
		RemoveSystems (&argv [optind]);
		/*NOTREACHED*/
	}

	AddModifySystems (&argv [optind]);

	return	0;
}
/*==================================================================*/

/*==================================================================*/
/*
*/
#ifdef	__STDC__
static	void
SecurityCheck (void)
#else
static	void
SecurityCheck ()
#endif
{
	if (geteuid () != 0)
	{
                LP_ERRMSG(ERROR, E_SYS_BEROOT);
		(void)	exit (1);
	}
}
/*==================================================================*/

/*==================================================================*/
/*
*/
#ifdef	__STDC__
static	void
TcpIpAddress (void)
#else
static	void
TcpIpAddress ()
#endif
{
	int	i;
        extern int errno;
	struct	utsname		utsbuf;
	struct	netconfig	*configp;
	struct	nd_hostserv	hostserv;
	struct	nd_addrlist	*addrsp;

	struct	netconfig	*getnetconfigent ();

	(void)	uname (&utsbuf);
	configp = getnetconfigent ("tcp");
	if (! configp)
	{
		LP_ERRMSG (ERROR, E_SYS_NOTCPIP);
		(void)	exit (2);
	}
	hostserv.h_host = utsbuf.nodename;
	hostserv.h_serv = "printer";
	if (netdir_getbyname (configp, &hostserv, &addrsp))
	{
                i = errno;
		(void)	pfmt(stderr, ERROR, NULL);
                errno = i;
		(void)	perror ("netdir_getbyname");
		(void)	exit (2);
	}
	for (i=0; i < addrsp->n_addrs->len; i++)
		(void)	printf ("%02x", addrsp->n_addrs->buf [i]);
	(void)	printf ("\n");
	(void)	exit (0);
}
/*==================================================================*/

/*==================================================================*/
/*
*/
#ifdef	__STDC__
static	void
ListSystems (char *syslistp [])
#else
static	void
ListSystems (syslistp)

char *syslistp [];
#endif
{
	char	*sysnamep;
	SYSTEM	*systemp;

	if (! *syslistp)
	{
		while ((systemp = getsystem (NAME_ALL)) != NULL)
			formatsys (systemp);
	}
	else
	for (sysnamep = *syslistp; sysnamep; sysnamep = *++syslistp)
	{
		if (STREQU(NAME_ANY, sysnamep)  ||
		    STREQU(NAME_NONE, sysnamep) ||
		    STREQU(NAME_ALL, sysnamep))
		{
                        LP_ERRMSG1(WARNING, E_SYS_RESWORD, sysnamep);
			continue;
		}
		if ((systemp = getsystem (sysnamep)) == NULL)
		{
                        LP_ERRMSG1(WARNING, E_SYS_NOTFOUND, sysnamep);
			continue;
		}
		formatsys (systemp);
	}
	(void)	exit (0);
}
/*==================================================================*/

/*==================================================================*/
/*
*/
#ifdef	__STDC__
static	void
RemoveSystems (char *syslistp [])
#else
static	void
RemoveSystems (syslistp)

char	*syslistp [];
#endif
{
	char	*sysnamep;
	SYSTEM	*systemp;


	SecurityCheck ();

	if (! syslistp || ! *syslistp)
	{
                LP_ERRMSG(ERROR, E_SYS_IMPRUS);
		(void)	exit (1);
	}
	for (sysnamep = *syslistp; sysnamep; sysnamep = *++syslistp)
	{
		if (STREQU(NAME_ANY, sysnamep)  ||
		    STREQU(NAME_NONE, sysnamep) ||
		    STREQU(NAME_ALL, sysnamep))
		{
                        LP_ERRMSG1(WARNING, E_SYS_RESWORD, sysnamep);
			continue;
		}
		if (! (systemp = getsystem (sysnamep)))
		{
                        LP_ERRMSG1(WARNING, E_SYS_NOTFOUND, sysnamep);
			continue;
		}
		if (NotifyLpsched (S_UNLOAD_SYSTEM, sysnamep) != MOK ||
		    delsystem (sysnamep))
		{
                        LP_ERRMSG1(ERROR, E_SYS_NOTREM, sysnamep);
			(void)	exit (2);
		}
		else
                        LP_ERRMSG1(ERROR, E_SYS_REMOVED, sysnamep);
	}
	(void)	exit (0);
}
/*==================================================================*/

/*==================================================================*/
/*
*/
#ifdef	__STDC__
static	void
AddModifySystems (char *syslistp [])
#else
static	void
AddModifySystems (syslistp)

char	*syslistp [];
#endif
{
		char	*sysnamep;
		SYSTEM	*systemp,
			sysbuf;
		boolean	modifiedFlag;

	static	SYSTEM	DefaultSystem =
			{
				NULL, NULL, NULL, S5_PROTO, NULL,
				DEFAULT_TIMEOUT, DEFAULT_RETRY,
				NULL, NULL, NULL
			}; 


	SecurityCheck ();

	for (sysnamep = *syslistp; sysnamep; sysnamep = *++syslistp)
	{
		modifiedFlag = False;
		if (systemp = getsystem (sysnamep))
		{
			sysbuf = *systemp;
			modifiedFlag = True;
		}
		else
		{
			sysbuf = DefaultSystem;
			sysbuf.name = sysnamep;
		}
		if (Protocolp)
		{
			sysbuf.protocol = 
				(! strcmp(Protocolp,NAME_BSDPROTO)
				 	? BSD_PROTO : S5_PROTO);
		}
		if (Timeoutp)
		{
			sysbuf.timeout = Timeout;
		}
		if (Retryp)
		{
			sysbuf.retry = Retry;
		}
		if (Commentp)
		{
			sysbuf.comment = Commentp;
		}
		if (putsystem (sysnamep, &sysbuf))
		{
                        LP_ERRMSG2(ERROR, E_SYS_COULDNT,
				modifiedFlag ? "modify" : "add", sysnamep);
			(void)	exit (2);
		}
		if (NotifyLpsched (S_LOAD_SYSTEM, sysnamep) != MOK)
		{
			/*
			**  Try to put the old system back.
			**
			*/
			if (systemp)
			{
				(void)	putsystem (sysnamep, systemp);
			}
                        LP_ERRMSG2(ERROR, E_SYS_COULDNT,
				modifiedFlag ? "modify" : "add", sysnamep);
			(void)	exit (2);
		}
		if (modifiedFlag)
		{
                        LP_OUTMSG1(INFO, E_SYS_MODIFIED, sysnamep);
		}
		else
		{
                        LP_OUTMSG1(INFO, E_SYS_ADDED, sysnamep);
		}
	}
	(void)	exit (0);
}
/*==================================================================*/

/*==================================================================*/
/*
*/
#ifdef	__STDC__
static	int
NotifyLpsched (int msgType, char *sysnamep)
#else
static	int
NotifyLpsched (msgType, sysnamep)

int	msgType;
char	*sysnamep;
#endif
{
	/*----------------------------------------------------------*/
	/*
	*/
		char	msgbuf [MSGMAX];
		ushort	status;

	static	boolean	OpenedChannel	= False;
	static	char	FnName []	= "NotifyLpsched";

        int i;
        extern int errno;


	/*----------------------------------------------------------*/
	/*
	*/
	if (! OpenedChannel)
	{
		if (mopen () < 0)
		{
			/*
			**  Assume scheduler is down and we can
			**  do what we want.
			*/
#ifdef	DEBUG
                        i = errno;
			(void)	pfmt(stderr, ERROR, NULL);
                        errno = i;
			(void)	perror ("mopen");
#endif
			return	MOK;
		}
		OpenedChannel = True;
	}
	if (putmessage (msgbuf, msgType, sysnamep) < 0)
	{
                i = errno;
		(void)	pfmt(stderr, ERROR, NULL);
                errno = i;
		(void)	perror ("putmessage");
		(void)	exit (2);
	}
	if (msend (msgbuf) < 0)
	{
                i = errno;
		(void)	pfmt(stderr, ERROR, NULL);
                errno = i;
		(void)	perror ("putmessage");
		(void)	exit (2);
	}
	if (mrecv (msgbuf, sizeof (msgbuf)) < 0)
	{
                i = errno;
		(void)	pfmt(stderr, ERROR, NULL);
                errno = i;
		(void)	perror ("mrecv");
		(void)	exit (2);
	}
	if (getmessage (msgbuf, mtype (msgbuf), &status) < 0)
	{
                i = errno;
		(void)	pfmt(stderr, ERROR, NULL);
                errno = i;
		(void)	perror ("mrecv");
		(void)	exit (2);
	}
	return	(int)	status;
}
/*==================================================================*/

/*==================================================================*/
/*
*/
#ifdef	__STDC__
static	void
formatsys (SYSTEM * sys)
#else
static	void
formatsys (sys)
SYSTEM	*sys;
#endif
{
        LP_OUTMSG2(INFO|MM_NOSTD, E_SYS_SYSTYPE, sys->name,
		(sys->protocol == S5_PROTO ? NAME_S5PROTO : NAME_BSDPROTO));
	if (sys->timeout == -1)
                LP_OUTMSG(INFO|MM_NOSTD, E_SYS_NEVER);
	else
                LP_OUTMSG1(INFO|MM_NOSTD, E_SYS_MINUTES, sys->timeout);
	if (sys->retry == -1)
                LP_OUTMSG(INFO|MM_NOSTD, E_SYS_NORETRY);
	else
                LP_OUTMSG1(INFO|MM_NOSTD, E_SYS_MINRETRY, sys->retry);
        LP_OUTMSG1(INFO|MM_NOSTD, E_SYS_COMMENT,
		(sys->comment == NULL ? "none" : sys->comment));
}
/*==================================================================*/
