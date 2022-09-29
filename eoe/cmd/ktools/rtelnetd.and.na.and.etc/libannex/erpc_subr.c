/*
 *****************************************************************************
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
 *	ERPC routines
 *
 * Original Author: Jonathan Taylor	Created on: 84/06/22
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/erpc_subr.c,v 1.3 1996/10/04 12:08:10 cwilson Exp $
 *
 * This file created by RCS from:
 *
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/erpc_subr.c,v $
 *
 * Revision History:
 *
 * $Log: erpc_subr.c,v $
 * Revision 1.3  1996/10/04 12:08:10  cwilson
 * latest rev
 *
 * Revision 1.15  1994/08/04  10:55:05  sasson
 * SPR 3211: SGI compiler requires arguments in select() call to
 * be of type "fd_set".
 *
 * Revision 1.14  1993/12/30  13:12:31  carlson
 * SPR 2084 -- made compatible with 64 bit machines.
 *
 * Revision 1.13  1992/11/13  14:36:05  carlson
 * SPR 1080 -- added a parameter to erpc_callresp to skip the first
 * transmission of data.  This is used by srpc_callresp to ignore out-
 * of-sequence packets.
 *
 * Revision 1.12  92/01/24  14:30:08  carlson
 * SPR 512 -- fix for Bull DPX/360 -- use correct size for messages.
 * 
 * Revision 1.11  91/05/19  16:36:12  emond
 * Assure erpc_callresp() returns -a if recvmsg fails, otherwise we end up with
 * hung erpcd's. (that's suppose to be a -1 not a -a).
 * 
 * Revision 1.10  91/04/24  19:18:03  emond
 * Pass debug flat to api_sendmsg() don't pass TRUE otherwise it will always
 * print debug mesgs.
 * 
 * Revision 1.9  91/04/08  14:24:58  emond
 * Modified to work with TLI as well as Socket interface.
 * 
 * Revision 1.8  89/10/16  17:29:17  loverso
 * Generalize for all machines missing select (sigh)
 * 
 * Revision 1.7  89/04/11  01:06:58  loverso
 * workaround for missing select with CMC, remove htonl externs
 * 
 * Revision 1.6  89/04/05  12:40:12  loverso
 * Changed copyright notice
 * 
 * Revision 1.5  88/06/10  10:11:40  mattes
 * erpc_callresp: also fill in the responding host's address
 * 
 * Revision 1.4  88/05/25  12:09:06  harris
 * Merger differences between r1.1 and r1.2 BACK into r1.3 to produce r1.4?!
 * 
 * Revision 1.3  88/05/24  18:35:29  parker
 * Changes for new install-annex script
 * 
 * Revision 1.2  88/05/04  17:38:27  harris
 * Erpc_callresp now updates (to)->sin_port based on from.sin_port.
 * 
 * Revision 1.1  88/04/15  11:56:30  mattes
 * Initial revision
 * 
 * Revision 2.10  88/02/24  11:26:59  harris
 * For MACH.  Since SUN and MACH have ntohl macros, I made code universal.
 * 
 * Revision 2.9  87/12/04  12:26:42  harris
 * Use special SENDMSG RECVMSG compatibility code.  See sendrecv.c.
 * 
 * Revision 2.8  87/09/22  11:38:35  parker
 * Fixed some bugs uncovered in build on SUN.
 * 
 * Revision 2.7  87/07/28  11:56:25  parker
 * Fixed calculation of sdelay in erpc_call() for xenix code.
 * 
 * Revision 2.6  87/06/10  18:12:34  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 2.5  86/12/10  15:49:12  parker
 * Added {get,set}_unspec_l() functions.
 * 
 * Revision 2.4  86/06/11  10:38:05  harris
 * Checked in changes form Release_1 1.2.1.6 version.
 * 
 * Revision 1.2.1.6  86/06/10  18:57:02  harris
 * Use constants to determine size of structs when not zero mod 4.
 * 
 * Revision 1.2.1.5  86/06/10  14:23:51  harris
 * Made more portable by using special functions to convert host/network formats
 * which work even when longs are not on a long boundary (get_long(), setlong()).
 * Change SYS_V version to look for a "../inc/port/SYS_V.h" include file.
 * Change EXOS version to look for a "../inc/port/EXOS.h" include file.
 * 
 * Revision 1.2.1.4  86/05/29  16:26:54  harris
 * Added definitions of <errno.h> and _errno (helps compile!).
 * 
 * Revision 1.2.1.3  86/05/29  14:57:49  harris
 * Merged Excelan (Rel.1) and gould and other release 2 changes for rel. 1.
 * Moved include file to ../inc/exos/EXOS.h, make SUN use SENDMSG (not sendmsg).
 * 
 * Revision 1.12.1.2  86/05/23  18:32:24  harris
 * Support for Excelan.  #ifdefs to handle include file differences in 4.1.
 * 
 * Revision 2.0  86/02/20  14:40:39  parker
 * First development revision for Release 2
 * 
 * Revision 1.2  86/02/14  11:00:34  goodmon
 * Removed non-relative pathnames of include files.
 * 
 * Revision 1.1  85/11/18  13:45:17  brennan
 * Initial revision
 * 
 * Revision 1.10  85/10/15  14:20:16  goodmon
 * Cleaned out some lint.
 * 
 * Revision 1.9  85/08/12  11:46:00  taylor
 * Removed include of xns.h and pep.h
 * 
 * Revision 1.8  85/07/22  10:19:12  goodmon
 * Added debugging code to go with the -D switch.
 * 
 * Revision 1.7  85/07/08  15:44:34  taylor
 * Define rcsid[].
 * 
 * Revision 1.6  85/06/19  16:50:17  taylor
 * Include common protocol definition files.
 * 
 * Revision 1.5  85/06/10  18:37:12  goodmon
 * fixed a bug in erpc_sendresp() that caused the courier header to be
 * stripped off, which it shouldn't have been.
 * 
 * Revision 1.4  85/06/04  14:45:09  taylor
 * Modify erpc_callresp to return -1 on timeout.
 * 
 * Revision 1.3  85/06/04  14:19:25  taylor
 * Modify erpc_callresp to check, then strip PEP header in return message.
 * 
 * Revision 1.2  85/05/30  20:44:57  taylor
 * Added routine erpc_sendresp()
 * 
 * Revision 1.1  85/05/01  21:39:30  taylor
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************
 */

#define RCSDATE $Date: 1996/10/04 12:08:10 $
#define RCSREV	$Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/erpc_subr.c,v 1.3 1996/10/04 12:08:10 cwilson Exp $"
#ifndef lint
static char rcsid[] = RCSID;
#endif

/*
 *	Include Files
 */
#include "../inc/config.h"

#include "port/port.h"
#include <sys/types.h>
#include "api_if.h"
#include <sys/uio.h>
#if !defined(EXOS) && !defined(SLIP) && !defined(need_select)
#include <sys/time.h>
#endif
#include <netinet/in.h>

#include <errno.h>
#include "../inc/courier/courier.h"
#include "../inc/erpc/erpc.h"
#include <stdio.h>

#if TLIPOLL
#include <fcntl.h>
#include <poll.h>
#endif

#ifdef need_select
#include <signal.h>
#endif

/*
 *	External Data Declarations
 */

extern int errno;

UINT32 get_unspec_long(), get_long();

/*
 *	Defines and Macros
 */

#define NPOLL	1	/* number of file descriptors to poll (TLI only) */

#ifdef	FD_ZERO
#define	FDSETNULLPTR	(fd_set *)0
#else
#define	FDSETNULLPTR	(int *)0
#endif

/*
 *	Structure Definitions
 */


/*
 *	Forward Routine Declarations
 */

static	void	send_call();		/*  common call message sender	*/

/*
 *	Global Data Declarations
 */

#ifdef need_select
static int alarm_flag = 0;
#endif

extern	int debug;		/* 1 iff -d switch seen by erpcd; 0 otherwise */

/*
 * Send an ERPC Call message without waiting for a response.
 *
 *      "s"         is a descriptor for a UDP socket
 *      "to"        if non-zero, names recipient of message 
 *                  if zero, indicates socket is connect()ed to peer
 *      "pid"       is PEP ID from call message being responded to
 *      "rpnum"
 *      "rpver"
 *      "rproc"
 *      "iov"       is an array of iovecs
 *      "iovlen"    is the number of iovecs in the array
 */
erpc_call (s, to, pid, rpnum, rpver, rproc, iovlen, iov)
struct       sockaddr_in *to;		/* optional */
UINT32       pid, rpnum;
u_short      rpver, rproc;
struct       iovec *iov;	/* optional */
{

	/* call common send routine to send the CALL message 
	 */

	send_call(s, to, pid, rpnum, rpver, rproc, iovlen, iov);
	return;
}

#ifdef need_select
/*
 * Signal handler for alarm that replaces select
 *
 */
void alrm_hndlr()
{
        /* set flag indicating that there ihas been an alarm */
        alarm_flag = 1;
        /* reset signal handleing for an alarm */
        signal(SIGALRM, SIG_DFL);
}
#endif

/*
 * Send an ERPC CALL message and return what comes back.
 *
 * Sends message and awaits response on UDP socket "s".  If "s" is
 * not bound, "to" must name the recipient of the message. "pid" is
 * the PEP ID assigned to the outgoing message and matched to the
 * incoming message.  It should be chosen in such a way that it is
 * unlikely to be reused in the lifetime of a message. "rpnum", "rpver",
 * and "rproc" are the remote program, remote program version, and remote
 * procedure desired, respectively.  "iov" is an array of iovecs.  The
 * first element of "iov" should not be used.  "iovlen" is the
 * number of iovecs in the array that are actually used.  "delay" is the
 * expected turnaround time (in seconds) of the message at the remote
 * end.  "timeout" is the total amount of time the caller wishes to wait
 * for a response.
 *
 * "riov" is an array of iovecs that describe the buffer(s) into which
 * the result is to be placed.  The first element of "riov" should not be used.
 * "riovlen" is the number of iovecs in "riov" that are actually used.
 *
 * erpc_callresp returns as a function value the number of bytes received
 * in the response or -1 if a timeout occurred.
 */
int erpc_callresp (s, to, pid, rpnum, rpver, rproc, iovlen, iov, delay,
		   timeout, riovlen, riov, skipsend)
int 	        s, iovlen, timeout, delay, riovlen,skipsend;
struct          sockaddr_in *to;    /* optional */
UINT32 		pid,
		rpnum;
u_short 	rpver,
		rproc;
struct iovec 	*iov,		    /* optional */
		*riov;
{

#if defined(EXOS) || defined(need_select)
	INT32	readfds;
	INT32	sdelay;
#else
#ifndef SLIP
#ifdef	FD_ZERO
	fd_set	readfds;
#else
	int	readfds;
#endif
        struct timeval timeval;
#endif
#endif
        int 	nfound, cc;
        struct msghdr msg;
	struct sockaddr_in from;
        struct pephdr pephdr;
        register struct pephdr *ph = &pephdr;
	static char *app_nam="erpc_callresp";
#if TLIPOLL
#define	NPOLL 1
	struct pollfd pollfds[NPOLL];
#endif

	while (timeout > 0) {

		/* call common send routine to send the CALL message */

		if (skipsend)
			skipsend = 0;
		else
			send_call(s, to, pid, rpnum, rpver, rproc, iovlen, iov);

		/* await a return message or timeout */
		for (;;) {
#ifdef SLIP
			/* post a read timeout */
			if(so_schedule_timeout(s, delay)) {
				perror("erpc_callresp: so_schedule_timeout");
				return(-1);
			}
#else
#ifdef need_select
			/* use alarm for timeout on recv */
			signal(SIGALRM, alrm_hndlr);
			alarm(delay);
			alarm_flag = 0; /* clear alarm signal flag */
#else
			/* use select to wait for data */
#if TLIPOLL
			pollfds[0].fd=s;
			pollfds[0].events=POLLIN;
			/* delay is in sec and poll wants it in msec */
			if(poll(pollfds,NPOLL,(delay*1000)) < 0) {
				if(errno != EINTR) { /* is this wrong ?? */
					perror("erpc_sendresp: poll");
					return(-1);
				}
				if(debug)
					printf("erpc_subr: poll returned -1\n");
			}
			if ((pollfds[0].revents ==  0) ||
		 	   ((pollfds[0].revents !=  POLLIN) && 
			   (errno == EINTR))) {
				timeout -= delay;
				break;
			}
#else

#ifdef FD_ZERO
			FD_ZERO(&readfds);
			FD_SET(s, &readfds);
#else
			readfds = (1 << s);
#endif

#ifdef EXOS
			/* use old-style (4.1) select */
			sdelay = delay;
			sdelay = sdelay << 10;
			nfound = select(32, &readfds, (INT32 *)0, sdelay);
#else
			/* use real select */
			timeval.tv_sec = delay;
			timeval.tv_usec = 0;
			nfound = select(s + 1, &readfds, FDSETNULLPTR,
					FDSETNULLPTR, &timeval);
#endif /*EXOS*/
			/* error? */

			if (nfound < 0 && errno != EINTR) {
				perror("erpc_sendresp: select");
				return(-1);
			}

			/* timeout? */
			if (nfound == 0 || (nfound <0 && errno == EINTR)) {
				timeout -= delay;
				break;
			}

#endif /*TLIPOLL*/
#endif /*need_select*/
#endif /*SLIP*/

			/*
			 * Read PEP header into local buffer, rest of message
			 * into caller's buffer.
			 */
			riov->iov_base = (caddr_t)ph;
			riov->iov_len = PEPSIZE;
			msg.msg_name = (caddr_t)&from;
			msg.msg_namelen = sizeof(from);
			msg.msg_accrights = (caddr_t)0;
			msg.msg_accrightslen = 0;
			msg.msg_iov = riov;
			msg.msg_iovlen = riovlen + 1;
			cc = api_recvmsg(s,&msg,&timeout,delay,app_nam,debug);
			if (cc == -1)
			    return(-1);
			if (cc == -2)	     /* EINTR on SLIP or need_select */
			    break;
			if (cc < PEPSIZE)
				continue;
			if(get_long(ph->ph_id) != pid)
				continue;
			if (ntohs(ph->ph_client) != PET_ERPR)
				continue;
			if (to) {
				to->sin_port = from.sin_port;
				to->sin_addr.s_addr = from.sin_addr.s_addr;
			}
			return(cc - PEPSIZE);
		}
	}

	/* timeout */

	return(-1);
}

/*
 * Send an ERPC reject message.
 * ('to' doesn't look optional to me!)
 */
erpc_reject(s, to, pid, det, verlo, verhi)
	struct sockaddr_in *to;		/* optional */
	UINT32 pid;
	u_short det, verlo, verhi;
{

	TLI_PTR(struct t_unitdata,tlireqsnd,NULL)
	char *app_nam="erpc_reject";
	struct {
		struct jhdr jhdr;
		u_short vers[2];
	} jbuff;
	register struct jhdr *jh = &jbuff.jhdr;
	int nb = PEPSIZE + CMJSIZE;

	if (debug)
	    printf("erpc_reject: %d %d %d\n", det, verlo, verhi);

	/*
	 * Format reject message.
	 */
	set_long(jh->jh_id, pid);
	jh->jh_client = ntohs(PET_ERPR);
	jh->jh_type = ntohs(C_REJECT);
	jh->jh_tid = 0;
	jh->jh_det = ntohs(det);
	if (det == CMJ_NOVERS) {
		jbuff.vers[0] = ntohs(verlo);
		jbuff.vers[1] = ntohs(verhi);
		nb += 2*sizeof(u_short);
	}
	TLI_ALLOC(tlireqsnd,t_unitdata,s,T_UNITDATA,T_ADDR|T_UDATA,
		  app_nam,exit(1));

	(void)api_sndud(s,
		((to==NULL)?0:sizeof(struct sockaddr_in)),
		to, tlireqsnd, (u_char *)jh, nb, app_nam, TRUE);
	return;
}

/*
 * Send an ERPC Return message.
 * This is a normal case path; a return message usually implies that the 
 * call message was understood and acted on.
 *
 *      "s"         is a descriptor for a UDP socket
 *      "to"        if non-zero, names recipient of message 
 *                  if zero, indicates socket is connect()ed to peer
 *      "pid"       is PEP ID from call message being responded to
 *      "iov"       is an array of iovecs
 *      "iovlen"    is the number of iovecs in the array
 */
erpc_return (s, to, pid, iovlen, iov)
struct    sockaddr_in *to;	/* optional */
UINT32    pid;
struct    iovec *iov;		/* optional */
{
	static char *app_nam="erpc_return";
        struct rhdr rhdr;
        register struct rhdr *rh = &rhdr;
	struct msghdr msg;
	struct iovec liov[1], *piov;

#ifndef lint
	int bufsize = 0;                  /* for debug */
	static u_char buffer[1600];       /* for debug, holds max size buffer*/
	int i;                            /* for debug */
#endif

	set_long (&rh->rh_id[0], pid);
	if (debug)
	{
	    printf ("erpc_return: iovlen=%d\n", iovlen);
	    printf ("erpc_return: rh_id[0]=%x  rh_id[1]=%x\n", rh->rh_id[0], rh->rh_id[1]);
	}

	rh->rh_client = ntohs(PET_ERPR);
	rh->rh_type = ntohs(C_RETURN);
	rh->rh_tid = 0;

	piov = iovlen ? iov : liov;
	piov->iov_base = (caddr_t)rh;
	piov->iov_len = RHDRSIZE;
	msg.msg_name = (caddr_t)to;
	msg.msg_namelen =
		(to == NULL) ? 0 : sizeof(struct sockaddr_in);
	msg.msg_accrights = (caddr_t)0;
	msg.msg_accrightslen = 0;
	msg.msg_iov = piov;
	msg.msg_iovlen = iovlen + 1;
	/*
	 * if you want to print the message being returned here
	 * SENDMSG() in ../erpc/sendrecv.c has some code that can help
	 */
	(void)api_sendmsg(s, &msg, app_nam, debug);
	return;
}

/*
 * Send an ERPC Abort message.
 *
 * "err"     error code
 * ""        all other arguments same as erpc_return()
 */
erpc_abort (s, to, pid, err, iovlen, iov)
struct    sockaddr_in *to;	/* optional */
UINT32    pid;
u_short   err;
int iovlen;
struct    iovec *iov;		/* optional */
{
	struct ahdr ahdr;
	struct msghdr msg;
	struct iovec liov[1], *piov;
	char *app_nam="erpc_abort";

	if (debug)
	    printf("erpc_abort: %d %d\n", err, iovlen);

	set_long(ahdr.ah_id, pid);
	ahdr.ah_client = ntohs(PET_ERPR);
	ahdr.ah_type = ntohs(C_ABORT);
	ahdr.ah_tid = 0;
	ahdr.ah_err = ntohs(err);

	piov = iovlen ? iov : liov;
	piov->iov_base = (caddr_t)&ahdr;
	piov->iov_len = sizeof(ahdr);
	msg.msg_name = (caddr_t)to;
	msg.msg_namelen =
		(to == NULL) ? 0 : sizeof(struct sockaddr_in);
	msg.msg_accrights = (caddr_t)0;
	msg.msg_accrightslen = 0;
	msg.msg_iov = piov;
	msg.msg_iovlen = iovlen + 1;
	(void)api_sendmsg(s, &msg, app_nam, debug);
	return;
}

/*
 * Common Call message sender shared by erpc_call and erpc_callresp.
 */
static void send_call (s, to, pid, rpnum, rpver, rproc, iovlen, iov)
struct     sockaddr_in *to;	/* optional */
UINT32     pid, rpnum;
u_short    rpver, rproc;
struct     iovec *iov;		/* optional */
{
	char *app_nam="send_call";
	struct chdr chdr;
	register struct chdr *ch = &chdr;
	struct msghdr msg;
	struct iovec liov, *piov;

	if (debug)
	    printf("send_call %d\n", iovlen);

	set_long(ch->ch_id, pid);
	ch->ch_client = ntohs(PET_ERPC);
	ch->ch_type = ntohs(C_CALL);
	ch->ch_tid = 0;
	set_long(ch->ch_rpnum, rpnum);
	ch->ch_rpver = ntohs(rpver);
	ch->ch_rproc = ntohs(rproc);
	piov = iovlen ? iov : &liov;
	piov->iov_base = (caddr_t)ch;
	piov->iov_len = CHDRSIZE;
	msg.msg_name = (caddr_t)to;
	msg.msg_namelen =
		(to == NULL) ? 0 : sizeof(struct sockaddr_in);
	msg.msg_accrights = (caddr_t)0;
	msg.msg_accrightslen = 0;
	msg.msg_iov = piov;
	msg.msg_iovlen = iovlen + 1;
	(void)api_sendmsg(s, &msg, app_nam, debug);
	return;
}

/*
 *	get_long: get a 32-bit value from an array of 2 u_short's, first
 *		 convert it from network to host byte ordering;  this is
 *		 to ensure correct return even if not aligned at a long!
 */

UINT32 get_long(shorts)

u_short *shorts;
{
	union split
	{
		UINT32  sp_long;
		u_short sp_shorts[2];
	} SP;

#define Sp_long    SP.sp_long
#define Sp_shorts  SP.sp_shorts

	Sp_shorts[0] = shorts[0];
	Sp_shorts[1] = shorts[1];

	return ntohl(Sp_long);
}


/*
 *	set_long: convert 32-bit value from host to network ordering, and
 *		 place result in a location which might not be properly
 *		 aligned (not on a 32-bit boundary)
 */

set_long(two_shorts, a_long)

u_short *two_shorts;
UINT32  a_long;
{
	union split
	{
	   UINT32  sp_long;
		u_short sp_shorts[2];
	} SP;

#define Sp_long    SP.sp_long
#define Sp_shorts  SP.sp_shorts

	Sp_long = htonl(a_long);
	two_shorts[0] = Sp_shorts[0];
	two_shorts[1] = Sp_shorts[1];

	return;
}

/*
 *	get_unspec_long: get a 32-bit value from an array of 2 u_short's
 */

UINT32 get_unspec_long(shorts)

u_short *shorts;
{
	union split
	{
		UINT32  sp_long;
		u_short sp_shorts[2];
	} SP;

#define Sp_long    SP.sp_long
#define Sp_shorts  SP.sp_shorts

	Sp_shorts[0] = shorts[0];
	Sp_shorts[1] = shorts[1];

	return(Sp_long);
}
/*
 *	set_unspec_long: place long in a location which might not be properly
 *		 aligned (not on a 32-bit boundary)
 */

set_unspec_long(two_shorts, a_long)

u_short *two_shorts;
UINT32  a_long;
{
	union split
	{
		UINT32  sp_long;
		u_short sp_shorts[2];
	} SP;

#define Sp_long    SP.sp_long
#define Sp_shorts  SP.sp_shorts

	Sp_long = a_long;
	two_shorts[0] = Sp_shorts[0];
	two_shorts[1] = Sp_shorts[1];

	return;
}

/* prints messages
 * assumes contigous string of bytes
 * prints in hex
 * used by bfs.c, generically handy
 */
display(buffer, buflen)
        char *buffer;
        int buflen;
{
        int i;
	unsigned char *buf = (unsigned char *)buffer;
        char line[17];

        line[16] = 0;
        printf("received message length = %d\n",buflen);
        for (i=0; i < buflen; i++)
            {
            printf("%x%x ", buf[i] >> 4, buf[i] & 0x0f);
            line[i % 16] = (buf[i] >= 32 && buf[i] <= 127) ? buf[i] : '.';
            if (15 == (i % 16))
                printf("  %s\n", line);
            }
        printf("\n");
}       /* display */
