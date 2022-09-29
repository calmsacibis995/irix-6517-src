/*
 *  STREAMS system calls macros for portability
 *
 *  Copyright (c) 1991 Spider Systems Limited
 *
 *  This Source Code is furnished under Licence, and may not be
 *  copied or distributed without express written agreement.
 *
 *  All rights reserved.
 *
 *  Written by Mark Valentine <mark@spider.co.uk>
 *
 *  Made in Scotland.
 *
 *  These macros are intended for use by all STREAMS applications,
 *  so that STREAMS system calls can be trapped in emulation libraries,
 *  etc.
 */

/*@(#)streamio.h	1.7 (Spider) 11/13/92*/

#define S_OPEN		open
#define S_CLOSE		close
#define S_GETMSG	getmsg
#define S_PUTMSG	putmsg
#define S_READ		read
#define S_WRITE		write
#define S_IOCTL		ioctl
#define S_FCNTL		fcntl
#define S_POLL		poll
#define S_FORK		fork
#define S_EXIT		exit
