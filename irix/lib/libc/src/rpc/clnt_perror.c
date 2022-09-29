#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = 	"@(#)clnt_perror.c	1.4 90/07/19 4.1NFSSRC Copyr 1990 Sun Micro";
#endif

/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 *  1.16 88/02/08
 */

/*
 * clnt_perror.c
 */
#ifndef _KERNEL
#ifdef __STDC__
	#pragma weak clnt_sperror = _clnt_sperror
	#pragma weak clnt_perror = _clnt_perror
	#pragma weak clnt_syslog = _clnt_syslog
	#pragma weak clnt_sperrno = _clnt_sperrno
	#pragma weak clnt_perrno = _clnt_perrno
	#pragma weak clnt_spcreateerror = _clnt_spcreateerror
	#pragma weak clnt_pcreateerror = _clnt_pcreateerror
#endif
#include "synonyms.h"
#include <stdio.h>
#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#else
#include "types.h"
#include "auth.h"
#include "clnt.h"
#endif


#ifndef _KERNEL

static char *auth_errmsg(enum auth_stat);
static char *buf _INITBSS;
static char *_buf(void);

static char *
_buf(void)
{

	if (buf == NULL)
		buf = (char *)malloc(256);
	return (buf);
}

/*
 * Return string reply error info. For use after clnt_call()
 */
char *
clnt_sperror(CLIENT *rpch, const char *s)
{
	struct rpc_err e;
	char *err;
	char *str = _buf();
	char *strstart = str;

	if (str == NULL)
		return (NULL);
	CLNT_GETERR(rpch, &e);

	(void) sprintf(str, "%s: ", s);  
	str += strlen(str);

	(void) strcpy(str, clnt_sperrno(e.re_status));  
	str += strlen(str);

	switch (e.re_status) {
	case RPC_SUCCESS:
	case RPC_CANTENCODEARGS:
	case RPC_CANTDECODERES:
	case RPC_TIMEDOUT:     
	case RPC_PROGUNAVAIL:
	case RPC_PROCUNAVAIL:
	case RPC_CANTDECODEARGS:
	case RPC_SYSTEMERROR:
	case RPC_UNKNOWNHOST:
	case RPC_UNKNOWNPROTO:
	case RPC_PMAPFAILURE:
	case RPC_PROGNOTREGISTERED:
	case RPC_FAILED:
	case RPC_INTR:
		break;

	case RPC_CANTSEND:
	case RPC_CANTRECV:
		(void) sprintf(str, "; errno = %s",
		    sys_errlist[e.re_errno]); 
		str += strlen(str);
		break;

	case RPC_VERSMISMATCH:
		(void) sprintf(str,
			"; low version = %lu, high version = %lu", 
			e.re_vers.low, e.re_vers.high);
		str += strlen(str);
		break;

	case RPC_AUTHERROR:
		err = auth_errmsg(e.re_why);
		(void) sprintf(str,"; why = ");
		str += strlen(str);
		if (err != NULL) {
			(void) sprintf(str, "%s",err);
		} else {
			(void) sprintf(str,
				"(unknown authentication error - %d)",
				(int) e.re_why);
		}
		str += strlen(str);
		break;

	case RPC_PROGVERSMISMATCH:
		(void) sprintf(str, 
			"; low version = %lu, high version = %lu", 
			e.re_vers.low, e.re_vers.high);
		str += strlen(str);
		break;

	default:	/* unknown */
		(void) sprintf(str, 
			"; s1 = %lu, s2 = %lu", 
			e.re_lb.s1, e.re_lb.s2);
		str += strlen(str);
		break;
	}
	return(strstart) ;
}

void
clnt_perror(CLIENT *rpch, const char *s)
{
	(void) fprintf(stderr,"%s\n",clnt_sperror(rpch,s));
}

void
clnt_syslog(CLIENT *rpch, const char *s)
{
	(void) syslog(LOG_ERR,"%s",clnt_sperror(rpch,s));
}

#endif /* ! _KERNEL */

#ifdef _KERNEL
struct rpc_errtab {
	enum clnt_stat status;
	char *message;
};

static struct rpc_errtab  rpc_errlist[] = {
#else
struct rpc_errtab {
	enum clnt_stat status;
	char message[32];
};

static const struct rpc_errtab  rpc_errlist[] = {
#endif
	{ RPC_SUCCESS, 
		"Success" }, 
	{ RPC_CANTENCODEARGS, 
		"Can't encode arguments" },
	{ RPC_CANTDECODERES, 
		"Can't decode result" },
	{ RPC_CANTSEND, 
		"Unable to send" },
	{ RPC_CANTRECV, 
		"Unable to receive" },
	{ RPC_TIMEDOUT, 
		"Timed out" },
	{ RPC_INTR, 
		"Interrupted"},
	{ RPC_VERSMISMATCH, 
		"Incompatible versions of RPC" },
	{ RPC_AUTHERROR, 
		"Authentication error" },
	{ RPC_PROGUNAVAIL, 
		"Program unavailable" },
	{ RPC_PROGVERSMISMATCH, 
		"Program/version mismatch" },
	{ RPC_PROCUNAVAIL, 
		"Procedure unavailable" },
	{ RPC_CANTDECODEARGS, 
		"Server can't decode arguments" },
	{ RPC_SYSTEMERROR, 
		"Remote system error" },
	{ RPC_UNKNOWNHOST, 
		"Unknown host" },
	{ RPC_UNKNOWNPROTO,
		"Unknown protocol" },
	{ RPC_PMAPFAILURE, 
		"Port mapper failure" },
	{ RPC_PROGNOTREGISTERED, 
		"Program not registered"},
	{ RPC_FAILED, 
		"Failed (unspecified error)"},
};


/*
 * This interface for use by callrpc() and clnt_broadcast()
 */
char *
clnt_sperrno(enum clnt_stat stat)
{
	unsigned int i;

	for (i = 0; i < sizeof(rpc_errlist)/sizeof(struct rpc_errtab); i++) {
		if (rpc_errlist[i].status == stat) {
			return ((char *)rpc_errlist[i].message);
		}
	}
	return ("(unknown error code)");
}

#ifndef _KERNEL
void
clnt_perrno(enum clnt_stat num)
{
	(void) fprintf(stderr,"%s",clnt_sperrno(num));
}

/*
 * Return a string with the reason for
 * why a client handle could not be created
 */
char *
clnt_spcreateerror(const char *s)
{
	char *str = _buf();

	if (str == NULL)
		return (NULL);
	(void) sprintf(str, "%s: ", s);
	(void) strcat(str, clnt_sperrno(rpc_createerr.cf_stat));
	switch (rpc_createerr.cf_stat) {
	case RPC_PMAPFAILURE:
		(void) strcat(str, " - ");
		(void) strcat(str,
		    clnt_sperrno(rpc_createerr.cf_error.re_status));
		break;

	case RPC_SYSTEMERROR:
		(void) strcat(str, " - ");
		if (rpc_createerr.cf_error.re_errno > 0
		    && rpc_createerr.cf_error.re_errno < sys_nerr)
			(void) strcat(str,
			    sys_errlist[rpc_createerr.cf_error.re_errno]);
		else
			(void) sprintf(&str[strlen(str)], "Error %d",
			    rpc_createerr.cf_error.re_errno);
		break;
	}
	(void) strcat(str, "\n");
	return (str);
}

void
clnt_pcreateerror(const char *s)
{
	(void) fprintf(stderr,"%s",clnt_spcreateerror(s));
}
#endif

#ifndef _KERNEL
#define MAXAUTHNAMSIZ	32
struct auth_errtab {
	enum auth_stat status;	
	char message[MAXAUTHNAMSIZ - sizeof(enum auth_stat)];
};

static const struct auth_errtab auth_errlist[] = {
	{ AUTH_OK,
		"Authentication OK" },
	{ AUTH_BADCRED,
		"Invalid client credential" },
	{ AUTH_REJECTEDCRED,
		"Server rejected credential" },
	{ AUTH_BADVERF,
		"Invalid client verifier" },
	{ AUTH_REJECTEDVERF,
		"Server rejected verifier" },
	{ AUTH_TOOWEAK,
		"Client credential too weak" },
	{ AUTH_INVALIDRESP,
		"Invalid server verifier" },
	{ AUTH_FAILED,
		"Failed (unspecified error)" },
};

static char *
auth_errmsg(enum auth_stat stat)
{
	unsigned int i;

	for (i = 0; i < sizeof(auth_errlist)/sizeof(struct auth_errtab); i++) {
		if (auth_errlist[i].status == stat) {
			return((char *)auth_errlist[i].message);
		}
	}
	return(NULL);
}
#endif /* ! _KERNEL */
