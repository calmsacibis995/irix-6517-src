/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libnsl:cr1/getkey.c	1.1.1.1"

#ifdef __STDC__
#ifndef _LIBNSL_ABI
        #pragma weak getkey = _getkey
#endif /* _LIBNSL_ABI */
#endif

#include <libc_synonyms.h>
#include <libnsl_synonyms.h>

/*  libnsl routine to obtain shared key from the (cr1) keymaster daemon  */

#include <cr1.h>
#include "cr1.h"
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static int send_msg(char * scheme, Kmessage *kmsg);

char *
getkey(char *scheme, Principal A, Principal B)
{
	static Kmessage kmsg;		/* message to be passed to keymaster */

	if ( !scheme || !B )		/* need scheme and remote principal */
		return(NULL);

	if ( !A ) {			/* if no local principal, use euid */
		struct passwd *identity;
		if ((identity = getpwuid(geteuid())) == NULL)
			return(NULL);
		else
			A = identity->pw_name;
	}

	/* fill in message structure */

	kmsg.type = GET_KEY;
	kmsg.xtype = DES;
	strncpy(kmsg.principal1, A, PRINLEN);
	strncpy(kmsg.principal2, B, PRINLEN);

	/* send message to keymaster */

	if (send_msg(scheme, &kmsg) != 0)
		return(NULL);
	else
		return(kmsg.key1);
}

/*  Send a message to the keymaster and wait for a reply */

static int
send_msg(char * scheme, Kmessage *kmsg)
{
	int fd;			/*  fd of connection to keymaster  */
	XDR xdrs;		/*  XDR stream for messages to/from keymaster */
	char message[MLEN];	/*  message to/from keymaster  */
	char pipe_name[128];	/*  name of well known keymaster pipe */
	int pos;		/*  position in message buffer  */
	int nchar;		/*  #characters read or written  */
	int request;		/*  remember what we asked for  */
	Kmessage reply;

	request = kmsg->type;

	/*  Open keymaster's well-known STREAMS pipe  */

	sprintf(pipe_name, "%s/%s/%s", DEF_KEYDIR, scheme, DEF_KMPIP);
	if ((fd = open(pipe_name, O_RDWR)) == -1) {
		return(CR_PIPE);
	}

	/*  Create XDR stream for message to keymaster  */

	xdrmem_create(&xdrs, message, sizeof(message), XDR_ENCODE);

	if (!xdr_kmessage(&xdrs, kmsg)) {
		return(CR_MSGOUT);
	}

	pos = xdr_getpos(&xdrs);

	/*  Send message to keymaster  */

	if ((nchar = write(fd, message, (u_int) pos)) != pos) {
		return(CR_MSGOUT);
	}

	/*  Destroy XDR stream for message to keymaster	*/

	xdr_destroy(&xdrs);
	
	/*  Get response from keymaster  */

	nchar = read(fd, message, sizeof(message));
	(void) close(fd);

	if (nchar < 0)
		return(CR_MSGIN);
	else if (nchar == 0) {
		if (request == STOP)
			return(0);
		else
			return(CR_MSGIN);
	}

	/*  Create XDR stream for message from keymaster  */

	xdrmem_create(&xdrs, message, sizeof(message), XDR_DECODE);

	/*  Decode message from keymaster  */

	if (!xdr_kmessage(&xdrs, &reply)) {
		return(CR_MSGIN);
	}
	pos = xdr_getpos(&xdrs);

	/*  Report result from keymaster  */

	switch(reply.type) {

	case SEND_KEY:
		if (request != GET_KEY)
			return(CR_BADREPLY);
		(void)strncpy(kmsg->key1, reply.key1, KEYLEN);
		break;
	case CONFIRM:
		break;
	case REJECT:
		return(CR_REJECT);
		/* NOTREACHED */
		break;
	default:
		return(CR_BADREPLY);
		/* NOTREACHED */
		break;
	}

	/*  Terminate  */

	return(0);
}

/*  These XDR routines support the imposer and responder message formatting  */

static bool_t
xdr_principal(XDR *xdrs, Principal A)
{
	if (xdr_string(xdrs, &A, (u_int) PRINLEN))
		return (TRUE);
	return (FALSE);
}

static bool_t
xdr_key(XDR *xdrs, Key key)
{
	u_int isz = KEYLEN;

	if (xdr_bytes(xdrs, &key, &isz, KEYLEN))
		return (TRUE);
	return (FALSE);
}

static bool_t
xdr_kmessage(XDR *xdrs, Kmessage *kmsg)
{

	/* all messages have a type field */

	if (!xdr_enum(xdrs, (enum_t *)&kmsg->type))
		return(FALSE);

	/* get the encryption algorithm type */

	if (!xdr_enum(xdrs, (enum_t *)&kmsg->xtype))
		return(FALSE);

	/* take care of messages which have two principal fields */

	switch(kmsg->type) {

	case ADD_KEY:
	case DELETE_KEY:
	case CHANGE_KEY:
	case SEND_KEY:
	case GET_KEY:
		if (!xdr_principal(xdrs, kmsg->principal1) ||
		    !xdr_principal(xdrs, kmsg->principal2) )
			return(FALSE);
		break;

	default:
		break;

	}

	/* take care of messages which require an OLD key field */

	switch(kmsg->type) {

	case MASTER_KEY:
	case CHANGE_KEY:
	case DELETE_KEY:
	case SEND_KEY:
		if (!xdr_key(xdrs, kmsg->key1))
			return(FALSE);
		break;

	default:
		break;

	}

	/* take care of messages which require a NEW key field */

	switch(kmsg->type) {

	case ADD_KEY:
	case MASTER_KEY:
	case CHANGE_KEY:
		if (!xdr_key(xdrs, kmsg->key2))
			return(FALSE);
		break;

	default:
		break;

	}

	/* if we got here without failure, return success */

	return(TRUE);

}
