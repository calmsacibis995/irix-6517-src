/*
 *****************************************************************************
 *
 *        Copyright 1989, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale is strictly prohibited.
 *
 * Module Description::
 *
 * 	Compatibility include file for standalone XENIX SL/IP
 *
 * Original Author: Many		Created on: May 26, 1986
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/port/RCS/SLIP.h,v 1.2 1996/10/04 12:05:35 cwilson Exp $
 *
 * This file created by RCS from:
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/port/RCS/SLIP.h,v $
 *
 * Revision History:
 *
 * $Log: SLIP.h,v $
 * Revision 1.2  1996/10/04 12:05:35  cwilson
 * latest rev
 *
 * Revision 1.6  1989/04/11  01:05:25  loverso
 * remove code handled by install-annex/libannex.h
 *
 * Revision 1.5  89/04/05  14:42:38  root
 * Changed copyright notice
 * 
 * Revision 1.4  88/08/31  12:20:04  parker
 * changed _exit to x_exit because XENIX already has a _exit().
 * 
 * Revision 1.3  88/05/31  17:06:41  parker
 * changes for new install-annex script.
 * 
 * Revision 1.2  88/05/24  18:26:47  parker
 * Changes for new install-annex script
 * 
 * Revision 1.1  88/04/15  12:14:46  mattes
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 * $Locker:  $
 *
 *****************************************************************************
 */

/*
 * NOTE: This header file includes parts of several 4.3BSD-tahoe header files.
 *
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * Kludge header for timeval structure as defined by 4.2BSD
 * <sys/time.h>
 */

struct timeval {
	long	tv_sec;
	long	tv_usec;
};

/* types.h (assumes "short" is 16 bits, "long" is 32 bits,
	    and "char" is 8 bits signed by default) */

typedef unsigned short u_short;
typedef unsigned long u_long;
typedef u_long n_long;
typedef unsigned int u_int;
typedef unsigned char u_char;
/* typedef char *caddr_t; */
typedef int errno_t;

#define SO_DEBUG 0x01

/*
 *  Here is the simulated socket library for SL/IP
 */
#define socket		_socket
#define bind		_bind
#define connect		_connect
#define send		_send
#define recv		_recv
#define getprotobyname	_getprotobyname

/*
 * Here are the system calls we want to get in the way of
 */
#define read		_read
#define write		_write
#define perror		_perror
#define exit		x_exit
#define close		_close

/* getprotobyname() */

struct protoent {
    char *p_name;
    char **p_aliases;
    long p_proto;
    };

/* socket.h */

#define	SOCK_DGRAM	2
#define	AF_INET		2

struct sockaddr {
    u_short sa_family;
    char sa_data[14];
    };

#define	PF_INET		AF_INET

struct in_addr {	/* An internet address */
    u_long s_addr;
    };

/* in.h */

struct sockaddr_in {
    short sin_family;
    u_short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
    };

#define IPPROTO_UDP	17
#define INADDR_ANY	0L

/* errno.h, offset by 100 from 4.2's values */

#define	EDESTADDRREQ	120	/* Destination address required */
#define	EMSGSIZE	121	/* Message too long */
#define	EPROTOTYPE	122	/* Protocol wrong type for socket */
#define	ENOPROTOOPT	123	/* Protocol not available */
#define	EPROTONOSUPPORT	124	/* Protocol not supported */
#define	EOPNOTSUPP	125	/* Operation not supported on socket */
#define	EPFNOSUPPORT	126	/* Protocol family not supported */
#define	EAFNOSUPPORT	127	/* Address family not supported by protocol family */
#define	EADDRINUSE	128	/* Address already in use */
#define	EADDRNOTAVAIL	129	/* Can't assign requested address */

#define	ECONNABORTED	130	/* Software caused connection abort */
#define	ECONNRESET	131	/* Connection reset by peer */
#define	ENOBUFS		132	/* No buffer space available */
#define	EISCONN		133	/* Socket is already connected */
#define	ENOTCONN	134	/* Socket is not connected */

#define	ETIMEDOUT	135	/* Connection timed out */
#define	ECONNREFUSED	136	/* Connection refused */

#define ESOCKTNOSUPPORT	144	/* Socket type not supported */

#define MIN_SLIP_ERR	EDESTADDRREQ
#define MAX_SLIP_ERR	ESOCKTNOSUPPORT
