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
 * Module Description::
 *
 * 	Compatibility routines for those lacking systems:
 *	sendto sendmsg recvfrom recvmsg
 *
 * Original Author: Dave Harris		Created on: June 10, 1986
 *
 * Module Reviewers:
 *
 *	harris lint
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/sendrecv.c,v 1.3 1996/10/04 12:08:53 cwilson Exp $
 *
 * This file created by RCS from:
 *
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/sendrecv.c,v $
 *
 * Revision History:
 *
 * $Log: sendrecv.c,v $
 * Revision 1.3  1996/10/04 12:08:53  cwilson
 * latest rev
 *
 * Revision 1.10  1991/08/06  09:07:32  emond
 * Added Scott's change to allow NEED_0_SENDTO.
 *
 * Revision 1.9  91/05/01  22:41:47  emond
 * Changed name of replacement functions. i.e. _sendto to xylo_sendto.
 * This is to prevent the conflict with lib on HP's.
 * 
 * Revision 1.8  90/02/15  15:37:48  loverso
 * clobber #else/#endif TAG
 * 
 * Revision 1.7  89/04/27  13:41:04  loverso
 * Add missing { }
 * 
 * Revision 1.6  89/04/12  15:07:16  loverso
 * Add debug messages
 * 
 * Revision 1.5  89/04/11  01:07:49  loverso
 * reflect differences from EXOS socket calls and real socket calls
 * 
 * Revision 1.4  89/04/05  12:40:19  loverso
 * Changed copyright notice
 * 
 * Revision 1.3  88/05/24  18:35:52  parker
 * Changes for new install-annex script
 * 
 * Revision 1.1  88/04/15  11:58:20  mattes
 * Initial revision
 * 
 * Revision 2.3  87/08/27  10:37:39  mattes
 * #include changes for UMAX V
 * 
 * Revision 2.2  87/06/10  18:12:42  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 2.0  86/06/13  12:57:22  harris
 * Branch for development 2.0.
 * 
 * Revision 1.1.1.1  86/06/10  16:38:53  harris
 * Branch created for "Release_1".
 * 
 * Revision 1.1  86/06/10  16:33:57  harris
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************
 */

#define RCSDATE $Date: 1996/10/04 12:08:53 $
#define RCSREV	$Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/sendrecv.c,v 1.3 1996/10/04 12:08:53 cwilson Exp $"
#ifndef lint
static char rcsid[] = RCSID;
#endif


/*	Include Files		*/
#include "../inc/config.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>

/*	Defines			*/

#define	MSGBUFSIZE	2048

/*	Forward Function Declarations		*/

extern char *bcopy(); 

extern int debug;

#ifndef SLIP
/*
 *	recvfrom, recvmsg: quick and dirty interface routines
 *	for the Excelan 4.1c environment.
 *	Some socket addresses, flags,and some other arguments are ignored,
 *	but probably should be checked.
 */

xylo_recvfrom(s,msg,len,flags,from,fromlen)
int s,len,flags;
int *fromlen;
char *msg;
struct sockaddr_in *from;
{
	int ret;

	if (debug)
		fprintf(stderr, "libannex recvfrom\n");
	if (from) {
		*fromlen = sizeof(struct sockaddr_in);
#ifdef EXOS
		ret =  receive(s, from, msg, len);
#else
		/* XXX */
		if (debug)
			fprintf(stderr, "NOT IMPLEMENTED!\n");
		ret =  recv(s, msg, len, flags);
#endif
	}
	else
		ret = read(s, msg, len);		/* XXX ignores flags */

	if (debug > 1) {
		int i;
		fprintf(stderr, "recv'd %d bytes:", ret);
		for (i = 0; i < ret; i++) {
			if (i%16 == 0)
				fprintf(stderr, "\n%3d: ", i);
			fprintf(stderr, "%2x ", msg[i]);
		}
		fputs("\n", stderr);
	}
	return ret;
}
#endif

/*
 *	recvmsg() is scatter/gather version of recvfrom()
 */

xylo_recvmsg(s,msg,flags)
int s,flags;
struct msghdr *msg;
{
	static char buffer[MSGBUFSIZE];
	struct sockaddr_in *from;
	int bufsize = 0;
	int nread;
	int i;

	/*
	 * NOTE: this assumes that the entire message was
	 * passed atomically to socket, the data is "padded"
	 * to each vector length. It can then copy data to vectors
	 * in msg from buffer. 
	 */

	if (debug)
		fprintf(stderr, "libannex recvmsg\n");
	from = (struct sockaddr_in *) msg->msg_name;
	if (from) {
#ifdef EXOS
		nread = receive(s, from, buffer, MSGBUFSIZE);
#else
		int fromsize = sizeof(*from);
		nread = recvfrom(s, buffer, MSGBUFSIZE, 0, from, &fromsize);
#endif
	}
	else {
#ifdef EXOS
		struct sockaddr_in who;

		nread = receive(s, &who, buffer, MSGBUFSIZE);
#else
		nread = read(s, buffer, MSGBUFSIZE);
#endif
	}
	if (nread == -1)
		return(nread);
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
	return (nread);
}

#ifndef SLIP
/*
 *	sendto, sendmsg: interface routines for the Excelan environment.  
 *      Some socket addresses, flags, and some other arguments are ignored, 
 *      but probably should be checked.
 *
 *      "s"         is a descriptor for a UDP socket
 *      "msg"       pointer to message
 *      "len"       size of message
 *      "to"        if non-zero, names recipient of message 
 *                  if zero, indicates socket is connect()ed to peer
 *      "tolen"     length of recipients name
 */

xylo_sendto (s,msg,len,flags,to,tolen)
int     s,len,flags,tolen;
char    *msg;
struct  sockaddr_in *to;
{
	int    nwritten = 0, nremain = len;
	char   *residmsg = msg;

	if (debug)
		fprintf(stderr, "libannex sendto\n");

	/*  If address is passed, assume we have not been "connect()"ed
	 *  so, use send().  Otherwise, assume we have been "connect()"ed
	 *  and we can use write().
	 */

	do {
		if (to) {
#ifdef EXOS
			nwritten = send(s, to, residmsg, nremain);
#else
#ifdef NEED_0_SENDTO 
#undef sendto
			nwritten = sendto(s, msg, len, flags, to, tolen);
#define sendto xylo_sendto
#else
			/* XXX to who? */
			if (debug)
				fprintf(stderr, "NOT IMPLEMENTED!\n");
			nwritten = send(s, residmsg, nremain, flags);
#endif
#endif
		} else
			nwritten = write(s, residmsg, nremain);
		if (nwritten < 0) {
			perror("sendto: send|write");
			return(nwritten);
		}
		nremain -= nwritten;
		residmsg += nwritten;
	} while (nremain > 0);
	return(len);
}
#endif

/*
 *	sendmsg() is scatter/gather version of sendto()
 */

xylo_sendmsg (s,msg,flags)
int     s, flags;
struct  msghdr *msg;
{
	int     bufsize = 0;
	char    *residmsg;
	static char buffer[MSGBUFSIZE];
	int     i;

	if (debug)
		fprintf(stderr, "libannex sendmsg\n");

	/*
	 * loop, concatenating vector messages into one large message
	 * NOTE: this assumes that the entire message must be
	 * passed atomically to socket, hence the malloc of buffer.
	 * Otherwise, loop could do writes for each element of vector.
	 * Sigh.
	 */

	for (i = 0; i < msg->msg_iovlen; i++) {
		bcopy(msg->msg_iov[i].iov_base,
			buffer+bufsize,
			msg->msg_iov[i].iov_len);
		bufsize += msg->msg_iov[i].iov_len;
	}
	return sendto(s, buffer, bufsize, flags,
			msg->msg_name, msg->msg_namelen);
}
