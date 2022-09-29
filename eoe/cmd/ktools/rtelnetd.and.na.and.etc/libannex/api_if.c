/*
 *****************************************************************************
 *
 *        Copyright 1991, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use. 
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Module Description::
 *
 * 	API I/F - Application Program Interface Interface.
 *		Supplies I/F from application layer to different API systems;
 *		either sockets, TLI, or EXOS.
 *
 * Original Author: D Emond	Created on: New Year's Eve
 *
 * Module Reviewers:
 *	%$(reviewers)$%
 *
 * Revision Control Information:
 * $Id: api_if.c,v 1.2 1996/10/04 12:07:21 cwilson Exp $
 *
 * This file created by RCS from
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/api_if.c,v $
 *
 * Revision History:
 * $Log: api_if.c,v $
 * Revision 1.2  1996/10/04 12:07:21  cwilson
 * latest rev
 *
 * Revision 1.21  1994/08/26  16:43:46  carlson
 * SPR 2940 -- modifications to support Linux Slackware 1.2.
 *
 * Revision 1.20  1993/07/15  11:18:30  carlson
 * SPR 1344 -- wildly bad error handling in api_recvmsg in TLI section.
 *
 * Revision 1.19  93/05/04  13:06:55  carlson
 * Update to last check-in -- EINTR needs to be checked before doing
 * perror, not after.
 * 
 * Revision 1.18  93/04/20  13:05:24  carlson
 * SPR 1386 -- Bug in api_recv (not handling EINTR code) was causing
 * aprint to prematurely exit on Unisys hosts.
 * 
 * Revision 1.17  92/10/12  16:23:45  carlson
 * More initialization added to api_connect for NCR.
 * 
 * Revision 1.16  92/10/07  09:18:01  bullock
 * spr.1172 - for t_connect, need to initialize whole tlicall structure
 * beforehand so NCR doesn't die.  
 * 
 * Revision 1.15  92/09/11  13:58:37  carlson
 * SPR 1058 - need to have our own endpoint address cache for UDP
 * connections, since t_connect doesn't work on all TLI systems.
 * (Reviewed with M Bullock and tested on Sequent and NCR.)
 * 
 * Revision 1.14  92/08/26  15:57:46  carlson
 * Fixed fprintf argument errors and pedantic ANSI compiler warning.
 * 
 * Revision 1.13  92/08/21  14:36:28  carlson
 * Removed all references to t_alloc and t_free, fixed many storage
 * corruptions and losses, fixed some of the debug messages, added
 * some calls to t_look as needed.  The function interfaces should
 * now be reviewed and modified.
 * 
 * Revision 1.12  92/07/01  12:56:37  bullock
 * spr.850 - do a t_free after the t_alloc to return the memory
 * 
 * Revision 1.11  92/04/02  18:01:27  emond
 * Per Alan Barnett for Bull - assure msg->namelen = 0 if *name == 0.
 * 
 * Revision 1.10  92/03/22  22:25:53  emond
 * Older compilers cannot handle "*="; need space in there "= *".
 * 
 * Revision 1.9  91/11/21  18:34:25  carlson
 * SPR 434 -- missing cast in sendto statement.
 * 
 * Revision 1.8  91/09/16  19:22:23  emond
 * Had to comment argument following #endif; illegal on ansii compilers.
 * 
 * Revision 1.7  91/07/29  12:57:34  raison
 * fix ICL SVR4 Sparc-based DRS6000 compiler error
 * 
 * Revision 1.6  91/06/27  14:47:28  emond
 * Move fcntl.h include from api_if.h to api_if.c
 * 
 * Revision 1.5  91/06/24  16:01:52  barnes
 * in api_open, change the path type from (char *) to (int) since
 * the socket call core dumps on XENIX if the socket type is (char *)
 * 
 * Revision 1.4  91/06/21  11:59:22  emond
 * Accommodate for EXOS systems.
 * Fix so t_bind doesn't dereference a zero pointer and crash na.
 * 
 * Revision 1.3  91/05/19  16:33:47  emond
 * Assure api_recvmsg() returns a -2 if SLIP configuration fails recvmsg().
 * 
 * Revision 1.2  91/04/24  19:16:31  emond
 * Clean-up error from ICMP msgs being recv'd in t_rcvudata(). TLI specific.
 * 
 * Revision 1.1  91/04/07  23:45:30  emond
 * Initial revision
 * 
 * This file is currently under revision by: $Locker:  $
 *
 *****************************************************************************
 */

/*
 *	Include Files
 */

#include "../inc/config.h"

#include <sys/types.h>
#include <fcntl.h>
#include "api_if.h"
#include <sys/uio.h>
#include <netinet/in.h>

#include <errno.h>
#include "../inc/courier/courier.h"
#include "../inc/erpc/erpc.h"
#include <stdio.h>
#include <sys/ioctl.h>


/*
 *	External Definitions
 */

extern int so;				/* token for socket number */
extern int t_errno;
extern int errno;

/*
 *	Defines and Macros
 */

#ifndef MSGBUFSIZE
#define	MSGBUFSIZE 2048
#endif

#ifndef ECONNREFUSED
#define ECONNREFUSED 61
#endif

/*
 *	Structure and Typedef Definitions
 */


/*
 *	Forward Function Definitions
 */


/*
 *	Static Definitions
 */

#define	TLI_TCP	"/dev/tcp"
#define	TLI_UDP	"/dev/udp"


/*
 *	Global Data Declarations
 */

/* Map generic Transport options and flags to specific ones */
/*  (either TLI or Sockets) */
#ifdef TLI
#ifdef NOTDEF
	int	tp_opt[MAX_TO_OPTS] = {TP_KEEPALIVE,0};
#endif
	int	tp_flags[MAX_FLGS] = {0,0,0,T_EXPEDITED,};
#else /* Sockets */
	int	tp_opt[MAX_TO_OPTS] = {SO_KEEPALIVE,SO_DEBUG};
#ifndef EXOS
#ifdef LINUX
	int	tp_flags[MAX_FLGS] = {0,MSG_PEEK,0,MSG_OOB};
#else /* LINUX */
	int	tp_flags[MAX_FLGS] = {0,MSG_PEEK,MSG_DONTROUTE,MSG_OOB};
#endif /* LINUX */
#endif /*EXOS*/
#endif /* TLI */


/*
 *	Static Data Declarations
 */


#ifdef TLI
/*
 * Special TLI support routines -- TLI doesn't always allow connecting
 * of a UDP socket, so we implement this function ourselves.
 */

struct rmt_addr {
	struct rmt_addr *next;
	int fd;
	struct sockaddr_in sin;
};

static struct rmt_addr *rmt_head = NULL;

struct rmt_addr *
get_remote_addr(fd)
int fd;
{
	struct rmt_addr *np;

	for (np = rmt_head;np != NULL; np = np->next)
		if (np->fd == fd)
			break;
	return np;
}
#endif

/*****************************************************************************
 *
 * NAME:
 *	api_open()
 *
 * DESCRIPTION:
 *	Performs open down to the TLI API layer
 *
 * ARGUMENTS:
 *	pd		Protocol descriptor
 *	app_nam		ptr to application name
 *	sin		ptr to socket struct
 *	sho_err		flag; TRUE-show error, FALSE-don't show error
 *	
 * RETURN VALUE:
 *	int		File descriptor # of transport endpoint
 *			if == -1 then open failed
 *
 */
int api_open(pd, sin, app_nam, sho_err)
    int		pd;		/* Protocol descriptor */
    char	*app_nam;	/*ptr to application name */
    struct sockaddr_in *sin;	/* ptr to socket struct */
    int		sho_err;

#ifdef TLI

{
	char	err_msg[80];
	int fd;	        /* file descriptor of transport endpoint */
	char *path;		/* pathname to transport endpoint */

	if (pd == IPPROTO_TCP)
	    path = (char *)TLI_TCP;
	else				/* Assumed UDP */
	    path = (char *)TLI_UDP;

	fd = t_open(path,O_RDWR,(struct t_info *)NULL);
	if (fd < 0 && sho_err) {
		sprintf (err_msg, "%s:t_open failed for listen",app_nam);
		t_error(err_msg);
	}
	return fd;
}

#else /* not TLI */

{
	char	err_msg[80];
	int fd;	        /* file descriptor of transport endpoint */
	int path;		/* socket type */

	if (pd == IPPROTO_TCP)
	    path = SOCK_STREAM;
	else				/* Assumed UDP */
	    path = SOCK_DGRAM;

#ifdef EXOS /* 4.1-style sockets, sigh */

	fd = socket(path, (struct sockproto *)0, sin, 0);
#else
	fd = socket(AF_INET, path, 0);
#endif
	if ((fd < 0) && (sho_err)) {
		sprintf (err_msg, "%s: could not get socket",app_nam);
		perror(err_msg);
	}
	return(fd);
}
#endif /*TLI*/


/*****************************************************************************
 *
 * NAME:
 *	api_bind()
 *
 * DESCRIPTION:
 *	Bind given address to transport endpoint.
 *
 * ARGUMENTS:
 *	fd		file (transport endpoint) descriptor
 *	tlibind_p	ptr to ptr to t_bind struct
 *			if == 0 then don't alloc any memory for it.
 *			DEPRECATED -- should be removed!
 *	sin_p		ptr to the source address
 *	app_nam		ptr to name of application
 *	sho_err		flag; TRUE-show error, FALSE-don't show error
 *
 * RETURN VALUE:
 *	0		No error
 *	1		TLI bind failure
 *	2		Socket bind failure
 *
 * SIDE EFFECTS:
 *	None
 *
 * EXCEPTIONS:
 *	None
 *
 * ASSUMPTIONS:
 *	None
 */

int api_bind(fd, tlibind_p, sin_p, app_nam, sho_err)

    int			fd;
    struct sockaddr_in	*sin_p;
    char		*app_nam;
    int	      		sho_err;
#ifdef TLI
    struct t_bind	**tlibind_p;

{
	char	err_msg[80];
	struct t_bind tbind,*temp;

	/* Use tlibind struct ? */
	if (sin_p != (struct sockaddr_in *)NULL) {
	    tbind.addr.buf = (char *)sin_p;
	    tbind.addr.len = sizeof(struct sockaddr_in);
	    temp = &tbind;
	} else
	    temp = (struct t_bind *)NULL;

	if (t_bind(fd,temp,(struct t_bind *)NULL) < 0) {
	    if (sho_err) {
		sprintf (err_msg, "%s:t_bind failed",app_nam);
		t_error(err_msg);
	    }
	    return(1);
	}
	return(0);
}

#else

    int **tlibind_p;

{
	char	err_msg[80];


#ifndef EXOS
	if (bind(fd,(struct sockaddr *)sin_p,sizeof(struct sockaddr_in)) < 0) {
	    if (sho_err) {
		sprintf(err_msg,"%s: bind", app_nam);
		perror(err_msg);
	    }
	    return(2);
 	}
#endif /*!EXOS*/

	return(0);
}
#endif /*TLI*/




/*****************************************************************************
 *
 * NAME:
 *	api_connect()
 *
 * DESCRIPTION:
 *	API I/F layer connect.
 *
 *	Note that a connectionless application can do a Socket API connect
 *	just to store the specified servaddr so that the system know where
 *	to send any future data the applications writes to this socket
 *	descriptor.
 *
 * ARGUMENTS:
 *	fd			transport endpoint file descriptor
 *	pd			protocol descriptor (UDP or TCP)
 *				Indicates whether actual connect should occur
 *	sin			ptr to sockaddr_in struct
 *	app_nam			ptr to name of application that called us.
 *	sho_err			flag; TRUE-show error, FALSE-don't show error
 *
 * RETURN VALUE:
 *	0		No error
 *	1		TLI bind failure
 *	2		Socket bind failure
 *
 * SIDE EFFECTS:
 *	None
 *
 * EXCEPTIONS:
 *	None
 *
 * ASSUMPTIONS:
 *	None
 */

int api_connect(fd, sin, pd, app_nam, sho_err)
    int		fd;		/* transport endpoint file descriptor */
    int		pd;		/* protocol descriptor, UDP or TCP */
    struct sockaddr_in *sin;
    char	*app_nam;
    int		sho_err;	/*flag that indicates whether errors be shown*/


#ifdef TLI
{
	char	err_msg[80];
	int i,j,retval = 0;

	if (pd == IPPROTO_TCP) {
	    struct t_call tlicall;

	    tlicall.addr.buf = (char *)sin;
	    tlicall.addr.len = sizeof(struct sockaddr_in);
	    tlicall.opt.buf = tlicall.udata.buf = (char *)NULL;
	    tlicall.opt.len = tlicall.udata.len = 0;
	    tlicall.sequence = 0;

	    for (j=0;j < 10;j++,sleep(1)) {
		t_errno = errno = 0;
	        if (t_connect(fd,&tlicall,(struct t_call *)NULL) >= 0)
		    break;
		if (t_errno == TLOOK) {
		    i = t_look(fd);
		    if (i == T_DISCONNECT)
			continue;
		    if (sho_err)
			fprintf(stderr,"%s: strange t_look value %d\n",
			    app_nam);
		    retval = 1;
		    break;
		}
		if (sho_err) {
		    sprintf(err_msg,"%s: t_connect failed",app_nam);
		    t_error(err_msg);
		}
		retval = 1;
		break;
	    }
	} else if (sin) {
	    if (get_remote_addr(fd)) {
		if (sho_err)
		    fprintf(stderr,"%s: api_connect:  already connected.\n",
			app_nam);
		retval = 1;
	    } else {
		struct rmt_addr *np;

		np = (struct rmt_addr *)malloc(sizeof(struct rmt_addr));
		if (np == (struct rmt_addr *)NULL) {
		    if (sho_err)
			fprintf(stderr,"%s: api_connect:  out of memory.\n",
			    app_nam);
		    retval = 1;
		} else {
		    np->fd = fd;
		    bcopy(sin,&np->sin,sizeof(np->sin));
		    np->next = rmt_head;
		    rmt_head = np;
		}
	    }
	}
	return retval;
}

#else	/* Sockets */

{

	char	err_msg[80];

#ifdef EXOS
	if (connect(fd, (struct sockaddr *)sin) < 0) {
	    if (sho_err) {
		sprintf(err_msg,"%s: connect", app_nam);
		perror(err_msg);
	    }
	    return(2);
	}

#else /* BSD */

	if(connect(fd,(struct sockaddr *)sin,sizeof(struct sockaddr_in)) < 0) {
	    if (sho_err) {
		sprintf(err_msg,"%s: connect", app_nam);
		perror(err_msg);
	    }
	    return(2);
	}

#endif /* EXOS */
	return(0);

}

#endif /* TLI */



/*****************************************************************************
 *
 * NAME:
 *	api_recv()
 *
 * DESCRIPTION:
 *	Read normal or expedited data from network.
 *
 * ARGUMENTS:
 *	fd		transport endpoint file descriptor		
 *	cbuff		ptr to charc buffer
 *	len		charc buffer length
 *	flags		Transport flags
 *	app_name	name of application requesting service
 *	sho_error	TRUE-show errors; FALSE-don't show errors
 *
 * RETURN VALUE:
 *     -1		Failed read
 *	else		equal to number of charcs read
 *
 * SIDE EFFECTS:
 *	None
 *
 * EXCEPTIONS:
 *	None
 *
 * ASSUMPTIONS:
 *	None
 */

int api_recv(fd, cbuff, len, flags, sho_err, app_nam)
	int	fd;		/*  file descriptor of transport endpoint */
	char	*cbuff;
	char	*app_nam;
	int	sho_err;	/* TRUE-show error, FALSE-don't show error */
	int	flags,
		len;

{
	int	cc;
	char	err_msg[80];

#ifdef TLI
	int tflags,i;

	for (;;) {
	    tflags = 0;
	    if ((cc = t_rcv(fd, cbuff, len, &tflags)) >= 0)
		break;
	    switch (t_errno) {
	    case TLOOK:
		i = t_look(fd);
		if (i == T_DISCONNECT || i == T_ORDREL || i < 0) {
		    if (sho_err)
			fprintf(stderr,"%s: t_rcv closed.\n",app_nam);
		    return -1;
		}
		if (i != T_ERROR)
		    return 0;
		if (sho_err)
		    fprintf(stderr,"%s: t_rcv/t_look T_ERROR\n",
			app_nam);
		return -1;
	    case TNODATA:
		return 0;
	    case TSYSERR:
		if (errno == EINTR)
		    continue;
		if (sho_err) {
		    i = errno;
		    sprintf(err_msg,"%s: t_rcv TSYSERR", app_nam);
		    errno = i;
		    perror(err_msg);
		}
		return -1;
	    default:
		if (sho_err) {
		    sprintf(err_msg,"%s: t_rcv failed", app_nam);
		    t_error(err_msg);
		}
		return -1;
	    }
	}
	return cc;

#else

    for (;;) {
#ifdef EXOS
	cc = read(fd, cbuff, len);
#else
	cc = recv(fd, cbuff, len, tp_flags[flags]);
#endif /*EXOS*/
	if (cc >= 0)
	    return cc;
	if (errno == EINTR)
	    continue;
	if (sho_err) {
	    sprintf(err_msg,"%s: recv/read system error:", app_nam);
	    perror(err_msg);
	}
	return(-1);
    }
#endif

}


/*****************************************************************************
 *
 * NAME:
 *	api_recvmsg()
 *
 * DESCRIPTION:
 *	Socket recvmsg() simulated for TLI
 *	recvmsg() is scatter/gather version of recvfrom()
 *
 * ARGUMENTS:
 *	fd		transport endpoint file descriptor
 *	msg		
 *	cc		Max character count
 *	timeout_p	Ptr to expected turnaround timeout timer.
 *	delay		The expected turnaround time (in seconds) of the
 *			 message at the remote end.
 *	app_nam		Name of application who called us
 *	debug		debug level
 *
 * RETURN VALUE:
 *	-2		error in SLIP or need_select mode
 *	-1		error not in SLIP or need_select mode
 *	else	 	number of characters recv'd
 *
 * SIDE EFFECTS:
 *
 *	None
 *
 * EXCEPTIONS:
 *
 *	None
 *
 * ASSUMPTIONS:
 *
 *	None
 */

int api_recvmsg(fd, msg, timeout_p, delay, app_nam, debug)

    int fd, delay, debug, *timeout_p;
    struct msghdr *msg;
    char *app_nam;

#ifdef TLI

{
	static char buffer[MSGBUFSIZE];
	char err_msg[80];
	struct sockaddr_in *from;
	int bufsize = 0;
	int nread;
	int i;
	int flags=0;

	/*
	 * NOTE: this assumes that the entire message was
	 * passed atomically to this layer, the data are "padded"
	 * to each vector length.  It can then copy data to vectors
	 * in msg from buffer. 
	 */

	if (debug)
		fprintf(stderr,"%s api_recvmsg\n",app_nam);
	from = (struct sockaddr_in *) msg->msg_name;
	if (from != NULL) {
	    struct t_unitdata req;
	    struct sockaddr addr;

	    if (debug)
		fprintf(stderr,"api_recvmsg:from=0x%x\n",from);

	    req.udata.buf = buffer;
	    req.udata.maxlen = sizeof(buffer);
	    req.opt.buf = NULL;
	    req.opt.maxlen = 0;
	    req.addr.buf = (char *)&addr;
	    req.addr.maxlen = sizeof(addr);

	    if (t_rcvudata(fd,&req,&flags) < 0)
		switch (t_errno) {
		case TLOOK:
		    i = t_look(fd);
		    if (i == T_DISCONNECT || i == T_ORDREL || i < 0) {
			if (debug)
			    fprintf(stderr,"%s: t_rcvudata closed.\n",
				app_nam);
			return -1;
		    }
		    if (i != T_ERROR) {
			*timeout_p -= delay;
			return -2;
		    }
		    if (debug)
			fprintf(stderr,
			    "%s: t_rcvudata/t_look T_ERROR\n",
			    app_nam);
		    return -1;
		case TNODATA:
		    nread = 0;
		    break;
		case TSYSERR:
		    if (errno == EINTR) {
			*timeout_p -= delay;
			return -2;
		    }
		    if (debug) {
			i = errno;
			sprintf(err_msg,"%s: t_rcvudata TSYSERR",
			    app_nam);
			errno = i;
			perror(err_msg);
		    }
		    return -1;
		default:
		    if (debug) {
			sprintf(err_msg,"%s: t_rcvudata failed",
			    app_nam);
			t_error(err_msg);
		    }
		    return -1;
		}
	    else {
		bcopy(req.addr.buf,from,req.addr.len);
		nread = req.udata.len;
	    }
	} else { /* from == NULL */
	    nread = read(fd, buffer, MSGBUFSIZE);

	    if (nread < 0) {
		if (errno == EINTR) {
		    *timeout_p -= delay;
		    return -2;
		}
		if (debug) {
		    sprintf(err_msg,"%s:api_recvmsg read failed",
			app_nam);
		    perror(err_msg);
		}
		return -1;
	    }
	}

	if (debug > 1) {
		fprintf(stderr, "recv'd %d bytes:", nread);
		for (i = 0; i < nread; i++) {
			if (i%16 == 0)
				fprintf(stderr, "\n%3d: ", i);
			fprintf(stderr, "%2x ", buffer[i]);
		}
		fputs("\n", stderr);
	}
	for (i = 0; i < msg->msg_iovlen; i++) {
		if ((bufsize > MSGBUFSIZE) || (bufsize > nread)) {
			return(-1);
		}
		bcopy(buffer+bufsize,
			msg->msg_iov[i].iov_base,
			msg->msg_iov[i].iov_len);
		bufsize += msg->msg_iov[i].iov_len;
	}
	return nread;
}

#else /* Sockets */

{
	char err_msg[80];
	int nread;

	nread = recvmsg(fd,msg,0);

	if (nread < 0) {

#if defined(SLIP) || defined(need_select)
		if(errno == EINTR
#ifdef need_select
		   || alarm_flag
#endif
		   ) {
			if (debug) {
				sprintf(err_msg,"%s: recvmsg", app_nam);
				perror(err_msg);
			}
			*timeout_p -= delay;
			return(-2);
		}
#endif
		if (debug) {
		    sprintf(err_msg,"%s: recvmsg", app_nam);
		    perror(err_msg);
		}
		return(-1);
	}
	return (nread);
}

#endif /* TLI */


/*****************************************************************************
 *
 * NAME:
 *	api_sendmsg()
 *
 * DESCRIPTION:
 *	Socket sendmsg() simulated for TLI
 *	api_sendmsg() is scatter/gather
 *
 * ARGUMENTS:
 *	fd		File/Socket descriptor
 *	debug		TRUE - then show debug error messages
 *	msg		Ptr to msghdr struct containing data to txmit
 *	app_nam		Ptr to name of calling application
 *
 * RETURN VALUE:
 *	-1		error
 *	else		number of character tx'd
 *
 * SIDE EFFECTS:
 *
 *	None
 *
 * EXCEPTIONS:
 *
 *	None
 *
 * ASSUMPTIONS:
 *
 *	None
 */


int api_sendmsg (fd, msg, app_nam, debug)

#ifdef TLI

    int	    fd, debug;
    struct msghdr *msg;
    char *app_nam;

{
	int     bufsize = 0;
	static char buffer[MSGBUFSIZE];
	int     i, tolen, nremain;
	struct sockaddr_in *to;
	char err_msg[80];
	int    nwritten;
	char   *residmsg = buffer;

	if (debug)
 	    fprintf(stderr,"%s: api_sendmsg: fd %d, debug %d\n",
		app_nam,fd,debug);

	/*
	 * loop, concatenating vector messages into one large message
	 * NOTE: this assumes that the entire message must be
	 * passed atomically to socket, hence the malloc of buffer.
	 * Otherwise, loop could do writes for each element of vector.
	 * Sigh.
	 */

	for (i = 0; i < msg->msg_iovlen; i++) {
	    if (bufsize + msg->msg_iov[i].iov_len > sizeof(buffer)) {
		if (debug)
		    fprintf(stderr,"%s: api_sendmsg -- too long.\n",
			app_nam);
		return -1;
	    }
	    bcopy(msg->msg_iov[i].iov_base,buffer+bufsize,
		msg->msg_iov[i].iov_len);
	    bufsize += msg->msg_iov[i].iov_len;
	}

	to = (struct sockaddr_in *)msg->msg_name;
	tolen = msg->msg_namelen;
	if (to == NULL) {
	    struct rmt_addr *np;

	    np = get_remote_addr(fd);
	    if (np != NULL) {
		to = &np->sin;
		tolen = sizeof(np->sin);
	    }
	}
/*
 * If address is passed, assume we have not been "connect()"ed, and so
 * use t_sndudata().  Otherwise, assume we have been "connect()"ed and
 * we can use write().
 */
	if (to) {
	    struct t_unitdata tlireq;

	    if (to == NULL)
		tolen = 0;
	    tlireq.addr.buf = (char *)to;
	    tlireq.addr.len = tolen;
	    tlireq.udata.buf = buffer;
	    tlireq.udata.len = bufsize;
	    tlireq.opt.buf = NULL;
	    tlireq.opt.len = 0;
	    if (t_sndudata(fd,&tlireq) < 0) {
		if (t_errno == TFLOW)	/* Equivalent to EAGAIN */
		    bufsize = 0;
		else {
		    sprintf(err_msg,"%s: api_sendmsg t_sndudata failed",
			app_nam);
		    t_error(err_msg);
		    bufsize = -1;
		}
	    }
	    return bufsize;
	}

	while (nremain > 0) {
	    nwritten = write(fd, residmsg, nremain);
	    if (nwritten < 0) {
		sprintf(err_msg,"%s: api_sendmsg send|write",app_nam);
		perror(err_msg);
		return nwritten;
	    }
	    nremain -= nwritten;
	    residmsg += nwritten;
	}
	return bufsize;
}


#else /* Sockets */

    int	    fd;
    struct msghdr *msg;
    char *app_nam;

{
	char err_msg[80];
	int cc;

 	/* fix for BULL DPX/2 300 - Alan Barnett, Pregnana */
 	if(msg->msg_name == (caddr_t)0) msg->msg_namelen = 0;
 
	cc = sendmsg(fd, msg, 0);
	if (cc < 0) {
		sprintf (err_msg, "%s:sendmsg failed", app_nam);
		perror (err_msg);
		return(-1);
	}
	return(cc);
}

#endif


/*****************************************************************************
 *
 * NAME:
 *	api_send()
 *
 * DESCRIPTION:
 *	Send normal or expedited data over network.
 *
 * ARGUMENTS:
 *	fd		transport endpoint file descriptor		
 *	cbuff		ptr to charc buffer
 *	len		number of charcs to write
 *	flags		Transport flags
 *	app_name	name of application requesting service
 *	sho_error	TRUE-show errors; FALSE-don't show errors
 *
 * RETURN VALUE:
 *	-1		Send failed with errno
 *	else		equal to number of charcs sent
 *
 * SIDE EFFECTS:
 *	None
 *
 * EXCEPTIONS:
 *	None
 *
 * ASSUMPTIONS:
 *	None
 */

int api_send(fd, cbuff, len, flags, app_nam, sho_err)
	int	fd;		/*  file descriptor of transport endpoint */
	char	*cbuff;
	char	*app_nam;
	int	sho_err;	/* TRUE-show error, FALSE-don't show error */
	int	flags,
		len;
{

	char	err_msg[80];
	int	cc;

#ifdef TLI
	cc = t_snd(fd, cbuff, len, tp_flags[flags]);
	if (cc < 0) {
	    if (t_errno == TFLOW)
		return 0;
	    if (sho_err) {
		sprintf(err_msg,"%s: t_snd failed", app_nam);
		t_error(err_msg);
	    }
	    return(-1);
	}
	return(cc);
#else

#ifdef EXOS
	cc = write(fd, cbuff, len);
#else
	cc = send(fd, cbuff, len, tp_flags[flags]);
#endif
	if (cc < 0) {
	    if (sho_err) {
		sprintf(err_msg,"%s: write failed", app_nam);
		perror(err_msg);
	    }
	    return(-1);
	}
	return(cc);
#endif

}


/*****************************************************************************
 *
 * NAME:
 *	api_rcvud()
 *
 * DESCRIPTION:
 *	Read user datagram from network via connectionless protocol
 *
 * ARGUMENTS:
 *	cc_p		ptr to charc count. Upon entry this points to size of
 *			rcv buffer (max rcv size).
 *	al_p		ptr to length of address
 *	fd		transport endpoint file descriptor		
 *	tlireq		ptr to tlireq struct (deprecated)
 *	cbuff		ptr to charc buffer
 *	app_name	name of application requesting service
 *	from		ptr to address of desired source of mesg
 *			if == 0, source of mesg already specified via connect
 *
 * RETURN VALUE:
 *	0		No errors
 *	1		failed TCP read
 *	2		TLI t_rcvudata error, not TSYSERR and EINTR
 *	3		TLI t_rcvudata error, TSYSERR and EINTR
 *
 * SIDE EFFECTS:
 *	None
 *
 * EXCEPTIONS:
 *	None
 *
 * ASSUMPTIONS:
 *	None
 */

int api_rcvud(cc_p, al_p, fd, tlireqp, cbuff, app_nam, sho_err, from)
	int	*cc_p, *al_p;   /* charc count and addr len ptrs */
	int	fd;		/*  file descriptor of transport endpoint */
	char	*cbuff;
	char	*app_nam;
	int	sho_err;	/* TRUE-show error, FALSE-don't show error */
	struct sockaddr_in
		*from;	/* ptr to address of originator of mesg */
#ifdef TLI
	struct t_unitdata *tlireqp;

{
	int	flags,i,retval;
	char	err_msg[80]; 
	struct t_unitdata tlireq;
	struct t_uderr uderr;
	struct sockaddr addr;

	tlireq.udata.buf = cbuff;
	tlireq.udata.maxlen = *cc_p;
	tlireq.addr.maxlen = sizeof(addr);
	tlireq.addr.buf = (char *)&addr;
	tlireq.opt.maxlen = 0;
	tlireq.opt.buf = NULL;

	retval = 0;
	while (retval == 0) {
	    flags = 0;
	    if (t_rcvudata(fd,&tlireq,&flags) >= 0)
		break;
	    switch (t_errno) {
	    case TNODATA:
		retval = 4;
		break;
	    case TSYSERR:
		if (errno == EINTR)
		    retval = 3;
		else
		    retval = 2;
		break;
	    case TLOOK:
		i = t_look(fd);
		if (i != T_UDERR) {
		    if (sho_err)
			fprintf(stderr,
			    "%s: api_rcvud/t_look returned %d\n",
			    app_nam,i);
		    retval = 2;
		    break;
		}
		uderr.addr.buf = (char *)&addr;
		uderr.addr.maxlen = sizeof(addr);
		uderr.opt.buf = NULL;
		uderr.opt.maxlen = 0;
		if (t_rcvuderr(fd,&uderr) < 0) {
		    if (sho_err) {
			sprintf(err_msg,"%s: api_rcvud/t_rcvuderr",
			    app_nam);
			t_error(err_msg);
		    }
		    retval = 2;
		    break;
		}
		break;
	    default:
		if (sho_err) {
		    sprintf(err_msg,"%s: api_rcvud/t_rcvudata failed.",
			app_nam);
		    t_error(err_msg);
		}
		retval = 2;
		break;
	    }
	}
	if (retval == 0) {
	    *cc_p = tlireq.udata.len;
	    if (from != 0) {
		*al_p = tlireq.addr.len;
		bcopy(tlireq.addr.buf,from,*al_p);
	    }
	} else {
	    *cc_p = 0;
	    if (from != 0)
		*al_p = 0;
	}
	return retval;
}

#else

    int *tlireqp;

{
	char	err_msg[80];

	/* If we already ran connect() just do a read else readfrom */
	if (from == 0) {
	    if((*cc_p = read(fd, cbuff, *cc_p)) <= 0) {
		if((errno != ECONNREFUSED) && (sho_err)) {
		    sprintf (err_msg, "%s:read failed. errno %d",
							app_nam, errno);
		    perror(err_msg);
		}
		return(1);
	    }
	    return(0);
	}
	else {
	    *cc_p = recvfrom(fd, cbuff, *cc_p, 0,
			     (struct sockaddr *)from, al_p);
	    if (*cc_p < 0) {
		if (errno != EINTR) {
		    sprintf(err_msg,"%s: rcvfrom", app_nam);
		    perror(err_msg);
		}
		return(3);
	    }
	return(0);
	}

}

#endif


/*****************************************************************************
 *
 * NAME:
 *	api_sndud()
 *
 * DESCRIPTION:
 *	Send user datagram vai API
 *
 * ARGUMENTS:
 *	fd		transpoint endpoint file descriptor
 *	alen		length of destination address
 *	to		destination address struct
 *	tlireqsnd	TLI specific send struct
 *	databuf		ptr to buffer to send
 *	dlen		length of buffer to send
 *	sho_err		TRUE-show errors; FALSE-don't show errors
 *
 * RETURN VALUE:
 *	0		no errors
 *	1		TLI send failed
 *	2		Socket sendto Failed
 *
 * SIDE EFFECTS:
 *
 *	None
 *
 * EXCEPTIONS:
 *
 *	None
 *
 * ASSUMPTIONS:
 *	tlireqsnd struct has tx data buffer allocated; DATA from cbuff will
 *	be moved to tlireqsnd-udata.buf, NOT just the ptr to cbuff.
 *	
 */

int api_sndud(fd, alen, to, tlireqsndp, cbuff, dlen, app_nam, sho_err)
    int		fd,		/* transport endpoint file descriptor */
		alen,		/* length of address */
		sho_err,	/* flag; !=0 Show errors */
		dlen;		/* length of data to transmit */
    unsigned char *cbuff; 	/* ptr to data to txmit */
    struct sockaddr_in *to;	/* ptr to socket struct */
    char	*app_nam;

#ifdef TLI

    struct t_unitdata *tlireqsndp;

{
	char	err_msg[80];
	struct t_unitdata tlireqsnd;

	tlireqsnd.addr.buf = (char *)to;
	tlireqsnd.addr.len = alen;
	tlireqsnd.udata.buf = (char *)cbuff;
	tlireqsnd.udata.len = dlen;
	tlireqsnd.opt.buf = NULL;
	tlireqsnd.opt.len = 0;

	if (t_sndudata(fd,&tlireqsnd) < 0) {
	    if (sho_err) {
		sprintf(err_msg,"%s: api_sndud/t_sndudata failed",
		    app_nam);
		t_error(err_msg);
	    }
	    return 1;
	}
	return 0;
}

#else

    int	*tlireqsndp;

{

	char	err_msg[80];

	if (sendto(fd, cbuff, dlen, 0, (struct sockaddr *)to, alen) < 0) {
	    if (sho_err) {
		sprintf (err_msg, "%s:sendto failed", app_nam);
		perror(err_msg);
	    }
	    return(2);
	}
	return(0);
}
#endif

#ifdef NOTDEF


/*****************************************************************************
 *
 * NAME:
 *	api_release()
 *
 * DESCRIPTION:
 *	Orderly release of transport connection.
 *
 * ARGUMENTS:
 *	fd		File descriptor of transport endpoint
 *	howto		How to release file. Passed to shutdown()
 *			 (Socket API only)
 *	app_nam		Ptr to name of application
 *
 * RETURN VALUE:
 *	0		Success
 *	-1		Release failed
 *
 */

int api_release(fd, howto, app_nam)
    int	 fd,
	 howto;
    char *app_nam;

#ifdef TLI
{

	char	err_msg[80];

	if (t_sndrel(fd) < 0) {
	    sprintf(err_msg,"%s: t_sndrel failed", app_nam);
	    t_error(err_msg);
	    return(-1);
	}
	return(0);
}

#else

{
	char	err_msg[80];

	if (shutdown(fd, howto) == -1) {   /* Telnet connection down */
	    sprintf(err_msg,"%s: shutdown failed", app_nam);
	    perror(err_msg);
	    return(-1);
	}
	return(0);
}
#endif

#endif /*NOTDEF*/

/*****************************************************************************
 *
 * NAME:
 *	api_close()
 *
 * DESCRIPTION:
 *	Close given transport endpoint
 *
 * ARGUMENTS:
 *	fd		File descriptor of transport endpoint
 *
 * RETURN VALUE:
 *	0		Success
 *	-1		Close failed
 *
 * SIDE EFFECTS:
 *	File closed
 *
 */

int api_close(fd)
    int	 fd;

{

#ifdef TLI
	struct rmt_addr *np,*npp;

	npp = NULL;
	np = rmt_head;
	for (; np != NULL; npp = np,np = np->next)
	    if (np->fd == fd) {
		if (npp == NULL)
		    rmt_head = np->next;
		else
		    npp = np->next;
		free(np);
		break;
	    }
	(void)t_close(fd);
#else
	(void)close(fd);
#endif
}

#ifdef NOTDEF

/*****************************************************************************
 *
 * NAME:
 *	api_nblk
 *
 * DESCRIPTION:
 *	Set file control non-blocking on or off
 *
 * ARGUMENTS:
 *	fd		transport endpoint file descriptor
 *	nonblk		FALSE - turn off "nonblocking"
 *			TRUE  - turn on "nonblocking"
 *
 * RETURN VALUE:
 *	None
 *
 *	None
 */

void api_nblk(fd, nonblk)
    int	fd,
	nonblk;		/* non-blocking flag */

{
#ifdef TLI 
	(void)fcntl(fd,F_SETFL,nonblk?O_NDELAY:0);
#else
	int flag;

	flag = nonblk ? 1 : 0;
	(void)ioctl(fd, FIONBIO, (caddr_t)&flag);
#endif
}

#endif /* NOTDEF */


/*****************************************************************************
 *
 * NAME:
 *	api_opt()
 *
 * DESCRIPTION:
 *	Set/clear given transport layer option
 *
 * ARGUMENTS:
 *	fd		file descriptor for transport endpoint
 *	option		option to set or clear
 *	flag		TRUE-set option; FALSE-clear option.
 *	app_name	ptr to name of application
 *
 * RETURN VALUE:
 *	0		success
 *	-1		error occurred
 *
 * SIDE EFFECTS:
 *
 *	None
 *
 * EXCEPTIONS:
 *
 *	None
 *
 * ASSUMPTIONS:
 *
 *	None
 */

int api_opt(fd,option,flag,app_nam)
    int	fd,
	option,
	flag;
    char *app_nam;

#ifdef TLI
{

#ifdef NOTDEF	/* No TLI options used at this point */

	char	err_msg[80];
	struct strioctl ioc;
	struct tcp_options tcpoptbuf;

	if (option > MAX_TO_OPTS) {
		sprintf(err_msg,"%s: Transport option (%d) not available",
			 app_nam, option);
		t_error(err_msg);
		return(-1);
	}	    
	if (tp_opt[option] == 0)		/* Not supported by TLI I/F */
		return(0);

	if (flag)
	    ioc.ic_cmd = TCP_SETOPT;
	else
	    ioc.ic_cmd = TCP_CLROPT;

	tcpoptbuf.pr_options = tp_opt[option];
	tcpoptbuf.ltime = 0;
	tcpoptbuf.rcv_buf = 0;
	tcpoptbuf.snd_buf = 0;
	ioc.ic_timout = 0;
	ioc.ic_len = sizeof(struct tcp_options);
	ioc.ic_dp = (char *) &tcpoptbuf;
	ioctl(fd, I_STR, &ioc);
	return(0);

#endif /* NOTDEF */

}

#else /* Sockets */

{
	char	err_msg[80];
	int	on=1,
		off=0,
		*opt_cmd;

	if (option > MAX_TO_OPTS) {
		sprintf(err_msg,"%s: Transport option (%d) not available",
			 app_nam, option);
		perror(err_msg);
		return(-1);
	}	    
	if (tp_opt[option] == 0)	/* Not supported by Socket I/F */
		return(0);

	if (flag)
	    opt_cmd = &on;
	else
	    opt_cmd = &off;

#if !defined(EXOS) && !defined(SLIP)	/* no setsockopt() in SLIP library */

	if (setsockopt(fd,SOL_SOCKET,tp_opt[option],opt_cmd,sizeof(int)) < 0) {
	    sprintf(err_msg,"%s: setsockopt (%d)", app_nam,tp_opt[option]);
	    perror(err_msg);
	    return(-1);
	}
#endif /*SLIP*/

	return(0);
}
#endif


#ifdef NOTDEF
/* Print unitdata struct */
#ifdef TLI
print_unitdata(x)
struct t_unitdata *x;
{
	int i;

	printf("***** sending to tcp *********\n");
	printf("tlireq->addr.maxlen = %d\n", x->addr.maxlen);
	printf("tlireq->addr.len    = %d\n", x->addr.len);
	printf("tlireq->addr.buf    = ");
	for (i=0; i < x->addr.len && i < 50; i++)
		printf("%02x ", *(x->addr.buf+i));
	printf("\n");

	printf("tlireq->udata.maxlen = %d\n", x->addr.maxlen);
	printf("tlireq->udata.len    = %d\n", x->addr.len);
	printf("tlireq->udata.buf    = ");
	for (i=0; i < x->udata.len && i < 50; i++)
		printf("%02x ", *(x->udata.buf+i));
	printf("\n");
}
#endif
#endif
