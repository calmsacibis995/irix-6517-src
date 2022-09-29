/*****************************************************************************
 *
 *        Copyright 1989, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Module Function:
 *
 *	User-level interface to Xenix UDP SL/IP
 *	Simulates much of the BSD4.2 socket system call set
 *
 * Original Author:  Paul Mattes		Created on: 01/04/88
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/socket_layer.c,v 1.3 1996/10/04 12:09:15 cwilson Exp $
 *
 * This file created by RCS from:
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/socket_layer.c,v $
 *
 * Revision History:
 *
 * $Log: socket_layer.c,v $
 * Revision 1.3  1996/10/04 12:09:15  cwilson
 * latest rev
 *
 * Revision 1.8  1994/08/04  10:58:59  sasson
 * SPR 3211: "debug" variable defined in multiple places.
 *
 * Revision 1.7  1989/04/11  01:08:13  loverso
 * removed bzero - now in bcopy.c
 *
 * Revision 1.6  89/04/05  12:40:22  loverso
 * Changed copyright notice
 * 
 * Revision 1.5  88/10/25  15:54:35  mattes
 * I'm too embarassed to say.
 * 
 * Revision 1.4  88/08/31  12:21:44  parker
 * changed _exit to x_exit because XENIX already has a _exit().
 * 
 * Revision 1.3  88/05/31  17:12:38  parker
 * fixes to get XENIX/SLIP to build again
 * 
 * Revision 1.2  88/05/24  18:36:01  parker
 * Changes for new install-annex script
 * 
 * Revision 1.1  88/04/15  11:59:07  mattes
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************/

#define RCSDATE $Date: 1996/10/04 12:09:15 $
#define RCSREV	$Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/socket_layer.c,v 1.3 1996/10/04 12:09:15 cwilson Exp $"

#ifndef lint
static char rcsid[] = RCSID;
#endif

/*****************************************************************************
 *									     *
 * Include files							     *
 *									     *
 *****************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include "../inc/config.h"
#include "../inc/slip/slip_user.h"


/*****************************************************************************
 *									     *
 * Local defines and macros						     *
 *									     *
 *****************************************************************************/

#define N_SOCKS		10		/* How many simultaneous sockets */
#define SOCKET_BASE	200		/* Lowest symbolic socket number */

#define issocket(s)	((s) >= SOCKET_BASE && (s) < SOCKET_BASE + N_SOCKS)

#define MIN(a, b) ((a) > (b) ? (b) : (a))

#define TRUE		1
#define FALSE		0


/*****************************************************************************
 *									     *
 * Structure and union definitions					     *
 *									     *
 *****************************************************************************/

static struct sockinf {
    int si_number;
    int si_flags;
#define SI_OPEN		0x0001
#define SI_CONNECTED	0x0002
#define SI_TICKING	0x0004
    int si_lport;
    struct sockaddr si_peer;
    int si_pid;
    struct sockiobuf iobuf[2];
    } _socket_desc[N_SOCKS];

#define INBUF 0
#define OUTBUF 1

#define ibuf	iobuf[INBUF]
#define ibase	iobuf[INBUF].sb_base
#define icurr	iobuf[INBUF].sb_curr
#define ilen	iobuf[INBUF].sb_len

#define obuf	iobuf[OUTBUF]
#define obase	iobuf[OUTBUF].sb_base
#define ocurr	iobuf[OUTBUF].sb_curr
#define olen	iobuf[OUTBUF].sb_len


/*****************************************************************************
 *									     *
 * External data							     *
 *									     *
 *****************************************************************************/

extern int errno;
extern char *malloc();
extern u_short _pick_a_random_port();
extern int debug;
extern int getpid();


/*****************************************************************************
 *									     *
 * Global data								     *
 *									     *
 *****************************************************************************/

int _network_mtu = 0;
int _io_buffersize = 0;


/*****************************************************************************
 *									     *
 * Static data								     *
 *									     *
 *****************************************************************************/

static int sockets_in_use;


/*****************************************************************************
 *									     *
 * Forward definitions							     *
 *									     *
 *****************************************************************************/

static struct sockinf *sockinf_bynumber();
static struct sockinf *sockinf_byport();


struct protoent *GETPROTOBYNAME(name)
char *name;
{
    static struct protoent p;

    if(!strcmp(name, "udp"))
	return((struct protoent *)0);

    p.p_name = "udp";
    p.p_aliases = (char **)0;
    p.p_proto = IPPROTO_UDP;
    return(&p);
    }

int _socket(family, type, protocol)
int family;		/* AF_INET only */
int type;		/* SOCK_DGRAM only */
int protocol;		/* IPPROTO_UDP only */
{
    struct sockinf *s;
    errno_t error;
    int i;

    /* Check arguments */

    if(family != AF_INET) {
	errno = EAFNOSUPPORT;
	goto socket_abort;
	}
    if(type != SOCK_DGRAM) {
	errno = ESOCKTNOSUPPORT;
	goto socket_abort;
	}

    if(!_network_mtu) {
	if(error = _net_init(&_network_mtu, &_io_buffersize)) {
	    errno = error;
	    goto socket_abort;
	    }
	}

    /* Allocate and initialize the state structure */

    for(i = 0; i < N_SOCKS; ++i)
	if(!(_socket_desc[i].si_flags & SI_OPEN))
	    break;

    if(i >= N_SOCKS) {
	errno = ENOMEM;
	goto socket_abort;
	}

    s = &_socket_desc[i];

    s->ibase = malloc(_io_buffersize);
    if(!s->ibase) {
	errno = ENOMEM;
	goto socket_abort;
	}
    s->obase = malloc(_io_buffersize);
    if(!s->obase) {
	free(s->ibase);
	errno = ENOMEM;
	goto socket_abort;
	}

    s->icurr = s->ibase;
    s->ocurr = s->obase;
    s->ilen = s->olen = 0;
    s->si_number = SOCKET_BASE + i;
    s->si_flags = SI_OPEN;
    s->si_lport = 0;
    s->si_pid = getpid();
    ++sockets_in_use;

    return(s->si_number);

socket_abort:
    return(-1);
    }
    

/* Associate a local address with a socket */

int _bind(s, name, namelen)
int s;			/* returned from socket() */
struct sockaddr *name;	/* limited to Inet type */
int namelen;
{
    struct sockinf *t, *u;
    int port;
    errno_t error;

    if(!(t = sockinf_bynumber(s, FALSE))) {
	errno = EBADF;
	return(-1);
	}

    if(t->si_lport || (namelen != sizeof(*name))) {
	errno = EINVAL;
	return(-1);
	}

    port = ((struct sockaddr_in *)name)->sin_port;
    if(!port)
	port = _pick_a_random_port();
    else {
	if(sockinf_byport(port, TRUE)) {
	    errno = EADDRINUSE;
	    return(-1);
	    }
        if(error = _try_to_reserve(port)) {
	    errno = error;
	    return(-1);
	    }
	}

    t->si_lport = port;
    return(0);
    }


/* Associate a remote address with a socket */

int _connect(s, name, namelen)
int s;			/* returned from socket() */
struct sockaddr *name;	/* limited to Inet type */
int namelen;
{
    struct sockinf *t, *u;
    int port;
    errno_t error;

    if(!(t = sockinf_bynumber(s, FALSE))) {
	errno = EBADF;
	return(-1);
	}

    if(namelen != sizeof(*name)) {
	errno = EINVAL;
	return(-1);
	}

    bcopy((caddr_t)name, (caddr_t)&t->si_peer, sizeof(struct sockaddr));
    t->si_flags |= SI_CONNECTED;
    return(0);
    }


/* receive from anyone; return peer's name */

int _recvfrom(s, buf, len, flags, from, fromlen)
int s;			/* from socket() */
char *buf;		/* buffer */
int flags;		/* must be zero! */
struct sockaddr *from;	/* whom from it come */
int *fromlen;		/* value-result */
{
    struct sockinf *t;
    struct sockaddr dummy;
    char *rsp;
    int error, cc;

    /* Does the socket exist? */

    if(!(t = sockinf_bynumber(s, FALSE))) {
	errno = EBADF;
	goto recvfrom_abort;
	}

    /* Do they want to know where it will come from? */

    if(!from) {

	/* If not connected, must supply 'from' */

	if(!(t->si_flags & SI_CONNECTED)) {
	    from = &dummy;
	    bzero((caddr_t)from, sizeof(*from));
	    }

	/* If connected, require it be from peer (does this mean anything?) */

	else {
	    bcopy((caddr_t)&t->si_peer, (caddr_t)&dummy, sizeof(dummy));
	    from = &dummy;
	    }
	}
    else {
	if(*fromlen != sizeof(*from)) {
	    errno = EINVAL;
	    goto recvfrom_abort;
	    }
	bzero((caddr_t)from, *fromlen);
	}

    /* How can you receive on an unbound socket? */

    if(!t->si_lport) {
	if(debug)
	    printf("recvfrom: unbound socket\n");
	errno = ENOTCONN;
	goto recvfrom_abort;
	}

    error = _udp_input(&t->ibuf, &cc, &rsp, t->si_lport, (struct sockaddr_in *)from);
    if(error) {
	errno = error;
	goto recvfrom_abort;
	}

    bcopy(rsp, buf, MIN(cc, len));	/* Truncate, if need be */
    trivial_alarm_handler(0);
    return(cc);

recvfrom_abort:
    trivial_alarm_handler(0);
    return(-1);
    }


/* receive from your "peer" */

int _recv(s, buf, len, flags)
int s;			/* from socket() */
char *buf;		/* buffer */
int flags;		/* must be zero! */
{
    return(recvfrom(s, buf, len, flags, (struct sockaddr *)0, 0));
    }


/* send to a particular destination */

int _sendto(s, msg, len, flags, to, tolen)
char *msg;		/* data outgoing */
int len;		/* how big she is */
int flags;		/* must be zero! */
struct sockaddr *to;	/* where it is going */
int tolen;		/* how big where is */
{
    struct sockinf *t;
    char *bgb, *bgb2;
    int error;

    if(debug)
	fprintf(stderr, "SL/IP SENDTO %d bytes\n", len);

    if(!(t = sockinf_bynumber(s, FALSE))) {
	errno = EBADF;
	return(-1);
	}

    if(!to) {
	if(!(t->si_flags & SI_CONNECTED)) {
	    errno = ENOTCONN;
	    return(-1);
	    }
	to = &t->si_peer;
	tolen = sizeof(t->si_peer);
	}
    else if(tolen != sizeof(*to)) {
	errno = EINVAL;
	return(-1);
	}

    if(!((struct sockaddr_in *)to)->sin_port) {
	errno = EDESTADDRREQ;
	return(-1);
	}

    if(len >= _network_mtu) {
	if(debug)
	    fprintf(stderr, "sendto: msg too long: %d, mtu is %d\n",
		len, _network_mtu);
	errno = EMSGSIZE;
	return(-1);
	}

    if(!(t->si_lport))
	t->si_lport = _pick_a_random_port();

    t->ocurr = t->obase + (_io_buffersize - len);
    bcopy(msg, t->ocurr, len);
    t->olen = len;
    error = _udp_output(&t->obuf, t->si_lport, (struct sockaddr_in *)to);
    if(error) {
	errno = error;
	return(-1);
	}
    return(0);
    }


/* Send to your "peer" */

int _send(s, msg, len, flags)
char *msg;		/* data outgoing */
int len;		/* how big she is */
int flags;		/* must be zero! */
{
    struct sockinf *t;
    int error;

    if(!(t = sockinf_bynumber(s, FALSE))) {
	errno = EBADF;
	return(-1);
	}

    if(!(t->si_flags & SI_CONNECTED)) {
	errno = EDESTADDRREQ;
	return(-1);
	}

    return(sendto(s, msg, len, flags, &t->si_peer, sizeof(t->si_peer)));
    }


int _setsockopt(s, level, optname, optval, optlen)
int s, level, optname;
char *optval;
int optlen;
{
    return(0);
    }

static int so_close(s)		/* NONSTANDARD CALL */
int s;				/* returned from socket() */
{
    struct sockinf *t;

    if(!(t = sockinf_bynumber(s, TRUE))) {
	errno = EBADF;
	return(-1);
	}

    _close_port(t->si_lport, getpid() == t->si_pid);

    t->si_flags = 0;
    free(t->ibase);
    t->ibase = t->icurr = (char *)0;
    free(t->obase);
    t->obase = t->ocurr = (char *)0;
    t->ilen = t->olen = 0;

    if(!--sockets_in_use) {
	_network_mtu = 0;
	}
    return(0);
    }


static struct sockinf *sockinf_bynumber(n, any_pid)
int n;
int any_pid;
{
    struct sockinf *t;

    if(!issocket(n))
	return((struct sockinf *)0);

    t = &_socket_desc[n - SOCKET_BASE];

    if(!(t->si_flags & SI_OPEN))
	return((struct sockinf *)0);

    if(!any_pid && (t->si_pid != getpid()))
	return((struct sockinf *)0);

    return(t);
    }


static struct sockinf *sockinf_byport(n, any_pid)
int n;
int any_pid;
{
    int i;
    struct sockinf *t;

    for(i = 0; i < N_SOCKS; ++i) {
	t = &_socket_desc[i];
	if((t->si_flags & SI_OPEN) &&
	   (t->si_lport == n) &&
	   (any_pid || (t->si_pid == getpid())))
	    return(&_socket_desc[i]);
	}

    return((struct sockinf *)0);
    }

/*
 * Standard Unix system and library calls we want to be in the way of
 */

#undef perror

_perror(s)
char *s;
{
    static char *slip_ermsg[] = {
/* EDESTADDRREQ		120 */	"Destination address required",
/* EMSGSIZE		121 */	"Message too long",
/* EPROTOTYPE		122 */	"Protocol wrong type for socket",
/* ENOPROTOOPT		123 */	"Protocol not available",
/* EPROTONOSUPPORT	124 */	"Protocol not supported",
/* EOPNOTSUPP		125 */	"Operation not supported on socket",
/* EPFNOSUPPORT		126 */	"Protocol family not supported",
/* EAFNOSUPPORT		127 */	"Address family not supported by protocol family",
/* EADDRINUSE		128 */	"Address already in use",
/* EADDRNOTAVAIL	129 */	"Can't assign requested address",
/* ECONNABORTED		130 */	"Software caused connection abort",
/* ECONNRESET		131 */	"Connection reset by peer",
/* ENOBUFS		132 */	"No buffer space available",
/* EISCONN		133 */	"Socket is already connected",
/* ENOTCONN		134 */	"Socket is not connected",
/* ETIMEDOUT		135 */	"Connection timed out",
/* ECONNREFUSED		136 */	"Connection refused",
				"Unknown error 137",
				"Unknown error 138",
				"Unknown error 139",
				"Unknown error 140",
				"Unknown error 141",
				"Unknown error 142",
				"Unknown error 143",
/* ESOCKTNOSUPPORT	144 */	"Socket type not supported"
	};

    if(errno < MIN_SLIP_ERR || errno > MAX_SLIP_ERR) {
	perror(s);
	return;
	}

    fprintf(stderr, "%s: %s\n", s, slip_ermsg[errno - MIN_SLIP_ERR]);
    }

#undef read

int _read(d, buf, nbytes)
int d;
char *buf;
int nbytes;
{
    if(!issocket(d))
	return(read(d, buf, nbytes));

    return(recv(d, buf, nbytes, 0));
    }


#undef write

int _write(d, buf, nbytes)
int d;
char *buf;
int nbytes;
{
    if(!issocket(d))
	return(write(d, buf, nbytes));

    return(send(d, buf, nbytes, 0));
    }


#undef close

int _close(d)
{
    if(!issocket(d))
	return(close(d));

    return(so_close(d));
    }

#undef exit

int x_exit(status)
int status;
{
    int i;

    for(i = 0; i < N_SOCKS; ++i)
	if(_socket_desc[i].si_flags & SI_OPEN)
	    (void)so_close(i + SOCKET_BASE);

    exit(status);
    }

/* Called after fork(), from child process */

so_closeall()
{
    int s;

    for(s = 3; s < 10; ++s)
	if(!_private_fileno(s))
	    (void)close(s);

    for(s = 0; s < N_SOCKS; ++s)
	if(_socket_desc[s].si_flags & SI_OPEN)
	    (void)so_close(s + SOCKET_BASE);
    }


/* Substitute for "select" call: schedule an
   alarm to rip us out of a single socket read */

static int (*old_handler)() = SIG_DFL;
static int old_alarmtime = 0;

static int trivial_alarm_handler(s)
int s;
{
    int i;

    signal(SIGALRM, old_handler);
    old_handler = SIG_DFL;
    if(!s || old_alarmtime) {
	alarm(old_alarmtime);
	}
    old_alarmtime = 0;

    for(i = 0; i < N_SOCKS; ++i)
	_socket_desc[i].si_flags &= ~SI_TICKING;
    }

int so_schedule_timeout(s, timeout)
int s;
int timeout;	/* in seconds */
{
    struct sockinf *t;

    if(!(t = sockinf_bynumber(s))) {
	errno = EBADF;
	return(-1);
	}

    t->si_flags |= SI_TICKING;
    old_handler = signal(SIGALRM, trivial_alarm_handler);
    old_alarmtime = alarm(timeout * 5);
    return(0);
    }
