/******************************************************************************
 *
 *        Copyright 1989, 1990, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale is strictly prohibited.
 *
 * Module Function:
 *
 *	Select SRPC or ERPC interface, make a remote procedure call
 *
 * Original Author: Dave Harris    Created on: April 4, 1988
 *
 * Revision Control Information:
 *
 * $Id: rpc.c,v 1.3 1996/10/04 12:14:18 cwilson Exp $
 *
 * This file created by RCS from: 
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/rpc.c,v $
 *
 * Revision History:
 *
 * $Log: rpc.c,v $
 * Revision 1.3  1996/10/04 12:14:18  cwilson
 * latest rev
 *
 * Revision 1.20  1993/12/30  14:16:11  carlson
 * SPR 2084 -- made compatible with 64 bit machines.
 *
 * Revision 1.19  1993/06/30  16:53:04  bullock
 * spr.1849 - Changed variable name stime to lasttime since stime is
 * a command on NCR 3000.
 *
 * Revision 1.18  92/11/23  14:25:06  carlson
 * If srpc_create fails, then assume that something's wrong with
 * encryption, rather than using an uninitialized variable.
 * 
 * Revision 1.17  92/11/13  14:26:16  carlson
 * SPR 1080 -- added a parameter to erpc_callresp.
 * 
 * Revision 1.16  91/06/21  10:33:51  emond
 * Kludge to get around fgets on Sequent which returns even when it doesn't
 * have any input. We now sit in fgets loop until input per Soctt Griffiths.
 * 
 * Revision 1.15  91/04/09  00:22:41  emond
 * Accommodate for generic API interface
 * 
 * Revision 1.14  90/02/15  15:30:33  loverso
 * Cleanup and fix getname()
 * 
 * Revision 1.13  89/04/11  01:03:41  loverso
 * fix extern declarations, inet_addr is u_long, remove extern htonl et al
 * 
 * Revision 1.12  89/04/05  12:44:33  loverso
 * Changed copyright notice
 * 
 * Revision 1.11  88/11/29  14:50:15  harris
 * Inet_ntoa argument type made kosher for Sun 4.
 * 
 * Revision 1.10  88/07/08  14:04:49  harris
 * Add keepalive feature to SRPC sessions using time stamp.  Beta compatible.
 * 
 * Revision 1.9  88/06/02  16:52:56  harris
 * Always try no password before prompting!  Dont map inet addr to no passwd.
 * 
 * Revision 1.8  88/05/31  17:16:48  parker
 * Added include of termio.h for SYS_V.
 * 
 * Revision 1.7  88/05/25  10:47:40  harris
 * Remove & operator in front of the char null_key[] to eliminate warning.
 * 
 * Revision 1.6  88/05/25  10:09:30  harris
 * Map internet address as a password to the null string (no encryption).
 * 
 * Revision 1.5  88/05/25  09:33:26  parker
 * include file changes
 * 
 * Revision 1.4  88/05/19  18:05:42  harris
 * Cache new 16 byte passwords and their respective crypt tables.
 * 
 * Revision 1.3  88/05/13  17:06:07  harris
 * Made SYS_V interface cleaner.  Use correct ioctl()'s
 * 
 * Revision 1.2  88/05/10  17:28:01  harris
 * Allow reentry of passwords when the wrong password is used.  Try default
 * password first.  Include SLIP, UMAX_V and XENIX compatibility.  Don't
 * echo passwords.  Always try last good password first.
 * 
 * Revision 1.1  88/05/04  23:26:03  harris
 * Initial revision
 * 
 * This file is currently under revision by:
 * $Locker:  $
 *
 ******************************************************************************/

#define RCSID   "$Id: rpc.c,v 1.3 1996/10/04 12:14:18 cwilson Exp $"
#ifndef lint
static char rcsid[] = RCSID;
#endif

/* Include Files */
#include "../inc/config.h"

#include "port/port.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include "../libannex/api_if.h"
#include <netdb.h>
#include <strings.h>

#include <stdio.h>

#ifdef SYS_V
#include <termio.h>
#else
#include <sgtty.h>
#endif

#include "../inc/courier/courier.h"
#include "../inc/erpc/netadmp.h"

#define	INIT

#include "netadm.h"
#include "netadm_err.h"
#include "../libannex/srpc.h"

/* Defines and Macros */

#define INCOMING_COUNT  1
#define NRETRIES	2
#define KEEPALIVE	5

/* Structure Definitions */

typedef	struct	password_cache
	{
		struct	password_cache	*pc_link;
		UINT32			pc_iaddr;
		int			pc_keepalive;
		KEYDATA			*pc_key;

	}	PWCACHE;

/* External Declarations */

extern char		*malloc(), *inet_ntoa();
extern UINT32		inet_addr();
extern time_t		time();

extern int debug;

/* Forward Routine Declarations */

char		*get_password(), *getname();
KEYDATA		*default_password();
PWCACHE		*search_password();

/* Static Declarations */

static	PWCACHE	*cache_head = (PWCACHE *)(0),
		*cache_tail = (PWCACHE *)(&cache_head);

static	KEYDATA	master_table = { "" };

static	char	keyboard[KEYSZ + 4];

static	char	unknown[] = "unknown";

static	char	null_key[KEYSZ] = "";

static UINT32	pep;		/* PEP protocol id number */

/* Keep alive socket information from rpc() call to rpc() call */

static	int	s = 0;			/* socket file descriptor */
static	int	sopen = 0;		/* is socket open? */
static	time_t	lasttime = 0;		/* time last used, if ever */
static	int	keepalive;		/* understand sessions? */
static	struct sockaddr_in sock_inet;	/* copy because port is set */
static	SRPC	srpc;			/* SRPC state structure */

/*
 *  rpc()
 *
 *  Make a remote procedure call for netadm clients.
 *
 *  Initialize a socket.		Abort on error.
 *
 *  Select ERPC or SPRC interface,	The ERPC interface is selected if there
 *					is no password for the Annex Pinet_addr
 *					in the cache.  If the call is rejected
 *					with a CMJ_SRPC, the SRPC interface is
 *					used.  Password will be prompted if
 *					no set_global_password() was called.
 *
 *  Use selected interface(s) to make a remote procedure call:
 *
 *  - Make ERPC procedure call.		Bump pep id, setup iovec's, and make
 *					and ERPC call to COURRPN_NETADM.  The
 *					response is check for validity, aborts
 *					and rejects are mapped to NA codes.  In
 *					particular, NAE_SREJECT falls through
 *					to make a SRPC call w/default password.
 *					Two different reject codes (CMJ_SRPC,
 *					CMJ_SESSION) identify an Annex as Beta
 *					release R4.0.1-3, or Beta R4.0.4 +.
 *					Parameters are converted according to
 *					the type argument, into the *Pdata arg.
 *
 *  - Make SPRC procedure call.		Bump pep id.  Set up SRPC key (xmt)
 *					and call srpc_create requesting prog
 *					COURRPN_NETADM proc RPROC_SRPC_OPEN,
 *					keep SRPC session alive until another
 *					Annex is accessed, or 5 sec's silence.
 *					Note that srpc_create() sets up new
 *					keys, handles, and sequence numbers.
 *					Return NAE_ABORT if any error occurred.
 *					Make SRPC call.  Map reject or abort
 *					errors to NA errors.  If successful,
 *					convert response according to selected
 *					type argument, put data in *Pdata.
 *					Return errors.  Exception is NA_SREJECT
 *					which means the password was incorrect.
 *					Try null password (no encryption) next.
 *					If that doesn't work, prompt for one.
 *
 *  Return.				All exception conditions jump here,
 *					returning the appropriate error code.
 *					A successful invocation also ends here
 *					returning NAE_SUCCESS.
 */

rpc(Pinet_addr, procedure, nvecs, outgoing, Pdata, type)

struct sockaddr_in *Pinet_addr;		/* Inet address of Annex */
int		procedure;		/* remote procedure call nr */
int		nvecs;			/* number of outgoing iovecs */
struct	iovec	outgoing[];		/* iovecs for data to be sent */
char		*Pdata;			/* place to put return value */
u_short		type;			/* type of return value expected */
{
    int		erpc_retcd,		/* bytes returned by erpc_callresp */
		srpc_retcd,		/* return code from SRPC layer */
		length,			/* actual message return length */
		argsize,		/* total length of call arguments */
		retry = 0,		/* SRPC retry with prompted password */
		tried_null = 0,		/* has null password been tried? */
		return_code;		/* final return code for caller */

    time_t	    now;		/* time in seconds since "epoch" */

    KEYDATA	    *key;		/* encryption key table */

    struct iovec    incoming[INCOMING_COUNT + 1];

    char	    args[MESSAGE_LENGTH];
    char	    response[RESPONSE_SIZE];
    char	    *answer;

    /* Check *Pinet_addr address family. */

    if (Pinet_addr->sin_family != AF_INET)
	return NAE_ADDR;

    if (debug)
	printf("rpc.c/rpc:  address %X, port %d.\n",
	    Pinet_addr->sin_addr.s_addr,ntohs(Pinet_addr->sin_port));

    /*
     *  If time-stamp is recent enough, then a session already exists
     *  between this copy of NA and the Annex in question.  SRPC
     *	may continue to use it, ERPC uses a new socket every time.
     *  Beta release R4.0.1 thru R4.0.3 do not understand session,
     *  and should have been detected above as non-keepalive conversations.
     */

    now = time((time_t *)0);

    if(keepalive &&
       (Pinet_addr->sin_addr.s_addr == sock_inet.sin_addr.s_addr) &&
       lasttime && (lasttime >= (now - KEEPALIVE)))
    {
	goto direct_call;
    }

    if(sopen)		/* close the socket if one was open */
	(void)close(s);

    sopen = 0;		/* I am NOT open */

    if((return_code = init_socket(&s)) != NAE_SUCC)
	return return_code;

    sopen = -1;		/* I am open */
	
    /*
     *  In case the password is incorrect, a new session needs to be
     *  established, so a new copy of the destination is used.  This
     *  is required because the Srpc interface modified the sin_port.
     */

again:

    lasttime = (time_t)0;	/* reset keep alive time for new session */

    /* Copy destination so I can modify sin_port at will */

    bcopy((caddr_t)Pinet_addr, (caddr_t)&sock_inet, sizeof(sock_inet));

    /* Decide on SRPC or ERPC interface */

    if(fetch_key(&sock_inet, &key, &keepalive))
    {
	pep++;
	incoming[1].iov_base = response;
	incoming[1].iov_len = sizeof(response);

	/* Call erpc_callresp() to communicate the request to the annex. */

	if (debug)
	    printf("rpc.c/rpc:  Calling erpc_callresp.\n");
	erpc_retcd =
	erpc_callresp(s, &sock_inet, pep, COURRPN_NETADM, NETADM_VERSION,
		      (unsigned short)procedure, nvecs,
		      outgoing, DELAY, TIMEOUT, INCOMING_COUNT, incoming,0);
	if (debug)
	    printf("rpc.c/rpc:  erpc_callresp returns %d.\n",
		erpc_retcd);

	/* Check for timeout from erpc_sendresp(). */

	if(erpc_retcd == -1)
	{
	    return_code = NAE_TIME;
	    goto exit;
	}

	/* Verify that the response is not a rejection or an abortion. */

	return_code = verify_response(response, erpc_retcd);

        if(return_code == NAE_SREJECT ||
	   return_code == NAE_SESSION)	/* means SRPC interface required */
	{
	    if(return_code == NAE_SESSION)
	    {
		keepalive = -1;
	    }
	    else
	    {
		keepalive = 0;
	    }
	    key = default_password(&sock_inet, keepalive);
	    goto use_srpc;
	}
	else if(return_code != NAE_SUCC)  /* return error to caller */
	{
	    goto exit;
	}
	else
	{
	/* decode return arguments according to parameter type */

	    length = erpc_retcd - sizeof(CMRETURN);
	    answer = &response[sizeof(CMRETURN)];
	    return_code = return_param(Pdata, type, answer, length);
	    goto exit;
	}
	/*  only NAE_SREJECT should fall through - use SRPC interface  */
    } else {

use_srpc:

	pep++;				/* bump pep id for every call */

	if (!key || key->password[0] == '\0')
	    tried_null = -1;

	if (debug)
	    printf("rpc.c/rpc:  Calling srpc_create.\n");
	srpc_retcd = 
	srpc_create(&srpc, s, &sock_inet, pep, COURRPN_NETADM,
		    NETADM_VERSION, RPROC_SRPC_OPEN, key);
	if (debug)
	    printf("rpc.c/rpc:  srpc_create returns %d.\n",srpc_retcd);

	if (srpc_retcd != 0) {
	    length = 4;
	    goto chkerr;
	}

	/*
	 *  Now that a session is open, calls can be made.
	 *  A time stamp keeps the socket open for KEEPALIVE
	 *  seconds.  An rpc() for the same Annex within KEEPALIVE
	 *  seconds jumps directly to here, and is in the same
	 *  SRPC session.  Same UDP port and encryption is used.
	 */

direct_call:

	pep++;

	if(nvecs)
	{
	    argsize = copy_iov(&outgoing[1], nvecs, args + sizeof(SHDR),
			       (sizeof(args) - sizeof(SHDR)));

	    argsize += sizeof(SHDR);
	}
	else
	    argsize = sizeof(SHDR);

	if (debug) {
	    printf("rpc.c/rpc:  Calling srpc_callresp.\n");
	    printf("\tProcedure %d.\n",procedure);
	}
	srpc_retcd =
	srpc_callresp(&srpc, s, &sock_inet, pep, COURRPN_NETADM,
		      NETADM_VERSION, (unsigned short)procedure,
		      args, argsize, DELAY, TIMEOUT,
		      response, sizeof(response), &length);
	if (debug)
	    printf("rpc.c/rpc:  srpc_callresp returns %d.\n",
		srpc_retcd);

chkerr:

	/* return decoded parameters, or NA error reason code */

#define	reason length

	switch (srpc_retcd)
	{
	    case S_SUCCESS:

		if(keepalive)
		  lasttime = time((time_t *)0);	  /* keep session alive! */
		answer = &response[sizeof(SHDR)];
		return_code = return_param(Pdata, type, answer, length);
		break;

	    case S_REJECTED:

		if (debug)
		    printf("rpc.c/rpc:  S_REJECTED, reason code %d.\n",
			reason);
		if (reason < 0 || reason > MAX_DETAIL) {
		    printf("\tOut of range 0 to %d.\n",MAX_DETAIL);
		    return_code = NAE_REJ;
		} else
		    return_code =  details[reason];
		break;

	    case S_ABORTED:

		if(reason < 0 || reason > MAX_ERROR)
		 return_code =  NAE_ABT;
		else
		 return_code =  errors[reason];
		break;

	    case S_TIMEDOUT:

		return_code = NAE_TIME;
		break;

	    default:

		return_code = NAE_SABORT;
	}

	if(srpc_retcd != S_SUCCESS)
	    keepalive = 0;		/* all errors require a new session */

	/*
	 *  Password did not work, try another one!
	 *  First, if a null password has not yet been tried, do so!
	 *  Next time through, ask the user (/dev/tty) for one
	 *  Ask the user again, unless NRETRIES exceeded
	 */

	if (return_code == NAE_SREJECT && retry < NRETRIES) {
	    if (debug)
		printf("rpc.c/rpc:  Password didn't work.\n");
	    if (tried_null) {
		if (debug)
		    printf("rpc.c/rpc:  Retrying with password.\n");
		retry++;
		ask_password(&sock_inet);
	    } else {
		if (debug)
		    printf("rpc.c/rpc:  Retrying without password.\n");
		no_password(&sock_inet);	/* unencrypted */
	    }
	    goto again;
	}
    }

exit:

    if (debug)
	printf("rpc.c/rpc:  returning with code %d.\n",return_code);
    return return_code;

}   /* rpc() */


/*
 *  Copy from an IO vector into a finite contiguous memory space
 */

copy_iov(iovector, nvecs, args, maxargs)

struct	iovec	iovector[];	/* array of IO vector entries */
int		nvecs;		/* number of IO vector entries */
char		*args;		/* contiguous destination */
int		maxargs;	/* size of destination, to be respected */
{
	int	totalsize, i, length, space;

	space = maxargs;

	for(i = 0, totalsize = 0 ; i < nvecs && space; i++)
	{
		length = iovector[i].iov_len;
		if(length > space)
		    length = space;	/* can't copy into outer space */

		bcopy(iovector[i].iov_base, args, length);

		args += length;		/* point to next available byte */
		space -= length;	/* reduce amount of space left */
		totalsize += length;	/* increase amount copied */
	}
	return totalsize;	/* length of data actually copied */
}


/*
 *  Initialize encryption tables for the global (default) password
 */

set_global_password(string)

char	*string;			/* string to become global passwd */
{
	(void)make_table(string, &master_table);
	return;
}

/*
 *  Fetch Annex password encryption tables from cache, or return "error"
 *  -1 means no cache entry, client will use erpc first
 */

fetch_key(Pinet, key, keepalive)

struct	sockaddr_in	*Pinet;		/* points to inet address */
KEYDATA			**key;		/* ptr to ptr to crypt data */
int			*keepalive;	/* true if Annex handles sessions */
{
	PWCACHE		*cachentry;

	cachentry = search_password(Pinet);

	if(cachentry == (PWCACHE *)0)
	{
		*key = (KEYDATA *)0;
		*keepalive = 0;
		return -1;
	}
	else
	{
		*key = cachentry->pc_key;
		*keepalive = cachentry->pc_keepalive;
		return 0;
	}
}


/*
 *  Return a pointer to Annex password cache entry
 */

PWCACHE *
search_password(Pinet)

struct	sockaddr_in	*Pinet;		/* points to inet address */
{
	PWCACHE		*link;

	link = cache_head;

	while(link && link->pc_iaddr != Pinet->sin_addr.s_addr)
	    link = link->pc_link;

	return link;
}

/*
 *  Assign the default password encryption tables to a specific Annex
 *  Also keep track of wether or not this Annex understands SRPC sessions
 */

KEYDATA *
default_password(Pinet, keepalive)

struct	sockaddr_in	*Pinet;		/* points to inet address */
int			keepalive;	/* does annex understand sessions? */
{
	/* add the global password to the password cache */

	add_password(Pinet, &master_table, keepalive);

	/* calculate encryption key and return */

	return &master_table;
}


/*
 *  Make a dummy encryption table with no password for an Annex
 *  This function will change an existing password cache entry
 */

no_password(Pinet)

struct	sockaddr_in	*Pinet;		/* points to inet address */
{
	PWCACHE	*pwcent;

	pwcent = search_password(Pinet);

	if(!pwcent)			/* cache entry is KNOWN to exist */
	    return;

	pwcent->pc_key = (KEYDATA *)0;

	return;
}

/*
 *  Make an Annex password encryption table based on password query
 *  This function will change an existing password cache entry
 */

ask_password(Pinet)			/* prompt for annex password */

struct	sockaddr_in	*Pinet;		/* points to inet address */
{
	char	*password;
	PWCACHE	*pwcent;
	KEYDATA	*key;

	password = get_password(&Pinet->sin_addr);
	pwcent = search_password(Pinet);

	if(!pwcent)
	    return;			/* cache entry is KNOWN to exist */

	/*
	 *  If last encryption table in use was the master table, or if
	 *  no encryption (null pointer), allocate a new table
	 */

	if(pwcent->pc_key == &master_table)
	    pwcent->pc_key = (KEYDATA *)0;

	key = make_table(password, pwcent->pc_key);
	pwcent->pc_key = key;

	return;
}


/*
 *  Add a password cache entry for a given Annex with supplied passwd table
 */

add_password(Pinet, keyinfo, keepalive)

struct	sockaddr_in	*Pinet;		/* ptr to address (inet, port) */
KEYDATA			*keyinfo;	/* ptr to encryption tables */
int			keepalive;	/* Annex understands sessions? */
{
	PWCACHE	*new_entry;

	/* allocate a password cache entry, or return if no memory */

	new_entry = (PWCACHE *)malloc(sizeof(PWCACHE));

	if(new_entry == (PWCACHE *)0)
	{
		printf("Warning: password not cached due to ENOMEM\n");
		return;
	}

	/* fill in entry with pertinent details, prompt for password */

	new_entry->pc_iaddr = Pinet->sin_addr.s_addr;
	new_entry->pc_link = (PWCACHE *)0;
	new_entry->pc_keepalive = keepalive;
	new_entry->pc_key = keyinfo;

	/* link entry into list, declare entry as the new tail, return */

	cache_tail->pc_link = new_entry;
	cache_tail = new_entry;

	return;
}


/*
 *  Functions for password prompting, turning echo off and on (/dev/tty)
 */

static	FILE	*rfp;		/* read file desc for /dev/tty */
static	FILE	*wfp;		/* write file desc for /dev/tty */
static	int	isopen = 0;	/* flag set on first call */

char *
get_password(iaddr)		/* prompt for a password from /dev/tty */

struct	in_addr	*iaddr;
{
	char	*p,			/* pointer for parsing response */
		*host,			/* name of host, or unknown */
		*internet;		/* internet address in dot notation */
	char    *kb = keyboard;
	int	len;
	if(!isopen)
	{
	    isopen = -1;
	    rfp = fopen("/dev/tty", "r");
	    wfp = fopen("/dev/tty", "w");
	}

	if(!iaddr)
	{
	    fprintf(wfp, "Password: ");
	}
	else
	{
	    internet = inet_ntoa(*iaddr);
	    host = getname((char *)iaddr);
	    fprintf(wfp, "Password for %s <%s>: ", internet, host);
	}

	(void)fflush(wfp);
	devttynoecho();
	len = 0;
	while (len == 0) {
	    bzero(keyboard, KEYSZ+4);
	    (void)fgets(kb, KEYSZ + 2, rfp);
	    len = strlen(kb);
	}
	devttyecho();
	fprintf(wfp, "\n");
	(void)fflush(wfp);

	keyboard[KEYSZ - 1] = '\0';

	/* strip newline, replaced with a null */

	for(p = keyboard; *p != '\0'; p++)
	    if(*p == '\n')
	    {
		*p = '\0';
		break;
	    }

	return keyboard;
}


devttynoecho()		/* turn echo off */
{
#ifdef	SYS_V
	struct	termio	tio;
#else
	struct	sgttyb	sg;
#endif

	int	fd;

	if(!isopen)
	    return;

	fd = fileno(rfp);

#ifdef	SYS_V
	(void)ioctl(fd, (int)TCGETA, &tio);
	tio.c_lflag &= ~(ECHO);
	(void)ioctl(fd, (int)TCSETA, &tio);
#else
	(void)ioctl(fd, (int)TIOCGETP, (char *)&sg);
	sg.sg_flags &= ~(ECHO);
	(void)ioctl(fd, (int)TIOCSETP, (char *)&sg);
#endif

	return;
}

devttyecho()		/* turn echo on */
{
#ifdef	SYS_V
	struct	termio	tio;
#else
	struct	sgttyb	sg;
#endif

	int	fd;

	if(!isopen)
	    return;

	fd = fileno(rfp);

#ifdef	SYS_V
	(void)ioctl(fd, (int)TCGETA, &tio);
	tio.c_lflag |= ECHO;
	(void)ioctl(fd, (int)TCSETA, &tio);
#else
	(void)ioctl(fd, (int)TIOCGETP, (char *)&sg);
	sg.sg_flags |= ECHO;
	(void)ioctl(fd, (int)TIOCSETP, (char *)&sg);
#endif

	return;
}


/*
 * fetch hostname from /etc/hosts, given inet address
 */
char *
getname(address)
char 	*address;
{
#ifdef need_netdb
	return unknown;
#else
	struct	hostent	*hostdata;	/* host data, name, etc. */

	hostdata = gethostbyaddr(address, sizeof(struct in_addr), AF_INET);

	if(hostdata == (struct hostent *)0)
		return unknown;
	else
		return hostdata->h_name;
#endif
}
