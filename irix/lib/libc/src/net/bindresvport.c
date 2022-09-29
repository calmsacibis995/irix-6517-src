#ifdef __STDC__
	#pragma weak bindresvport = _bindresvport
#endif
#include "synonyms.h"

#if !defined(lint) && defined(SCCSIDS)
static  char sccsid[] = "@(#)bindresvport.c	1.1 88/03/22 4.0NFSSRC; from 1.8 88/02/08 SMI"; /* from UCB 4.2 83/06/27 */
#endif

/*
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <bstring.h>		/* prototype for bzero() */
#include <unistd.h>		/* prototype for getpid() */
#include <sys/capability.h>
#include <pthread.h>
#include <mplib.h>

/* move out of function scope so we get a global symbol for use with data cording */
static short port _INITBSS;

/*
 * Bind a socket to a privileged IP port
 */
int
bindresvport(sd, sin)
	int sd;
	struct sockaddr_in *sin;
{
	struct sockaddr_in myaddr;
	int i, res;
	cap_t ocap;
	cap_value_t capv = CAP_PRIV_PORT;
	extern pthread_mutex_t portnum_lock;

	/* VARIABLES PROTECTED BY portnum_lock: port */

#define STARTPORT 600
#define ENDPORT (IPPORT_RESERVED - 1)
#define NPORTS	(ENDPORT - STARTPORT + 1)

	if (sin == (struct sockaddr_in *)0) {
		sin = &myaddr;
		bzero(sin, sizeof (*sin));
		sin->sin_family = AF_INET;
	} else if (sin->sin_family != AF_INET) {
		setoserror(EPFNOSUPPORT);
		return (-1);
	}
	MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_LOCK, &portnum_lock) );
	if (port == 0) {
		port = (short)((getpid() % NPORTS) + STARTPORT);
	}
	i = NPORTS;

	ocap = cap_acquire (1, &capv);
	do {
		sin->sin_port = (unsigned short) htons(port);
		if (port == ENDPORT) {
			port = STARTPORT;
		} else {
			port++;
		}
		res = bind(sd, sin, sizeof(struct sockaddr_in));
	} while (--i > 0 && res < 0 && errno == EADDRINUSE);
	cap_surrender (ocap);

	MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &portnum_lock) );
	return (res);
}
