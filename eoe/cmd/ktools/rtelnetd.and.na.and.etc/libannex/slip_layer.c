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
 *	SL/IP layer of Xenix UDP SL/IP
 *
 * Original Author:  Paul Mattes		Created on: 01/04/88
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/slip_layer.c,v 1.3 1996/10/04 12:09:05 cwilson Exp $
 *
 * This file created by RCS from:
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/slip_layer.c,v $
 *
 * Revision History:
 *
 * $Log: slip_layer.c,v $
 * Revision 1.3  1996/10/04 12:09:05  cwilson
 * latest rev
 *
 * Revision 1.7  1994/08/04  10:58:46  sasson
 * SPR 3211: "debug" variable defined in multiple places.
 *
 * Revision 1.6  1993/12/30  13:14:13  carlson
 * SPR 2084 -- made compatible with 64 bit machines.
 *
 * Revision 1.5  1989/04/11  00:59:28  loverso
 * fix extern declarations, inet_addr is u_long, remove extern htonl et al
 *
 * Revision 1.4  89/04/05  12:40:21  loverso
 * Changed copyright notice
 * 
 * Revision 1.3  88/05/31  17:12:25  parker
 * fixes to get XENIX/SLIP to build again
 * 
 * Revision 1.2  88/05/24  18:35:57  parker
 * Changes for new install-annex script
 * 
 * Revision 1.1  88/04/15  11:58:47  mattes
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************/

#define RCSDATE $Date: 1996/10/04 12:09:05 $
#define RCSREV	$Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/slip_layer.c,v 1.3 1996/10/04 12:09:05 cwilson Exp $"

#ifndef lint
static char rcsid[] = RCSID;
#endif

/*****************************************************************************
 *									     *
 * Include files							     *
 *									     *
 *****************************************************************************/

#include <sys/types.h>
#include "../inc/config.h"
#include "port/port.h"
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <errno.h>
#include "../inc/slip/slip_user.h"
#include "../inc/slip/slip_system.h"
#include "../inc/slip/BSDslip.h"


/*****************************************************************************
 *									     *
 * Local defines and macros						     *
 *									     *
 *****************************************************************************/

#undef read
#undef write
#undef close


/*****************************************************************************
 *									     *
 * Structure and union definitions					     *
 *									     *
 *****************************************************************************/

/*****************************************************************************
 *									     *
 * External data							     *
 *									     *
 *****************************************************************************/

extern int debug;
extern errno_t errno;

extern UINT32 inet_addr();


/*****************************************************************************
 *									     *
 * Global data								     *
 *									     *
 *****************************************************************************/

UINT32 _my_inet_address;


/*****************************************************************************
 *									     *
 * Static data								     *
 *									     *
 *****************************************************************************/

static int ports_in_use = 0;
static int portlock = -1;


/*****************************************************************************
 *									     *
 * Forward definitions							     *
 *									     *
 *****************************************************************************/

int _net_init(mtu, buffersize)
int *mtu;
int *buffersize;
{
    FILE *f;
    int n_scanned;
    char addr[32], tty[32];

    /* look for the configuration file and pick it apart */

    f = fopen(CFGFILE, "r");
    if(!f) {
	perror(CFGFILE);
	return(errno);
	}

    /* we want "addr tty [stty ...] */

    n_scanned = fscanf(f, "%s %s", addr, tty);
    if(n_scanned == EOF || n_scanned < 2) {
	fprintf(stderr, "Illegal configuration file\n");
	return(errno);
	}

    /* parse the inet address */

    _my_inet_address = inet_addr(addr);
    if(!_my_inet_address) {
	fprintf(stderr, "Illegal internet address in %s: %s\n",
		CFGFILE, addr);
	}
    if(debug)
	printf("My IP address is %lx\n", _my_inet_address);

    /* grab the port lock file */

    if(portlock == -1) {
	portlock = open(PORTLOCK, O_RDWR);
	if(portlock == -1) {
	    perror(PORTLOCK);
	    return(errno);
	    }
	}

    /* return the MTU and go home */

    *mtu = SLMTU;
    *buffersize = SLMTU + sizeof(u_short);
    return(0);
    }

int _try_to_reserve(port)
int port;
{
    char pn[32];
    int fd;
    struct flock lock;

    /* try for the lock */

    lock.l_type = F_WRLCK;
    lock.l_whence = 0;
    lock.l_start = ntohs(port);
    lock.l_len = 1;
    if(fcntl(portlock, F_SETLK, &lock) == -1) {
	if(errno == EAGAIN)
	    return(EADDRINUSE);
	else
	    return(errno);
	}

    /* create a named pipe to hold the incoming data */

    sprintf(pn, SLIPDATA, ntohs(port));
    if(mknod(pn, S_IFIFO | 0666, 0) == -1) {
	if(errno == EEXIST)
	    errno = 0;
	else {
	    perror(pn);
	    exit(1);
	    }
	}
    else
	(void)chmod(pn, 0666);

    ++ports_in_use;
    return(0);
    }

u_short _pick_a_random_port()
{
    int i;

    for(i = 7000; ; ++i)
	if(!_try_to_reserve(htons(i)))
	    return(htons(i));
    }

int _close_port(port, last_close)
int port;
int last_close;
{
    char pn[32];
    struct flock lock;

    /* Give up the port lock, if we're really supposed to */

    if(last_close) {
	sprintf(pn, SLIPDATA, ntohs(port));
	unlink(pn);
	lock.l_type = F_UNLCK;
	lock.l_whence = 0;
	lock.l_start = ntohs(port);
	lock.l_len = 1;
	(void)fcntl(portlock, F_SETLK, &lock);
	}

    --ports_in_use;
    return(0);
    }

int _sl_input(iobuf, cc, buf, port)
struct sockiobuf *iobuf;
int *cc;
char **buf;
u_short port;
{
    char fn[32];
    FILE *f;
    int nbytes;
    int fd;
    int pkt_len;

    /* Read fresh data, if necessary */

restart:

    if(!iobuf->sb_len) {

	sprintf(fn, SLIPDATA, ntohs(port));
	fd = open(fn, O_RDONLY);
	if(fd == -1) {
	    if(errno == EINTR)
		return(errno);
	    perror("_sl_input: open");
	    exit(1);
	    }

	iobuf->sb_curr = iobuf->sb_base;
	nbytes = read(fd, iobuf->sb_curr, SLMTU + sizeof(u_short));

	switch(nbytes) {
	case 0:		/* EOF after other side closed */
	    if(debug)
		fprintf(stderr, "_sl_read: EOF\n");
	    close(fd);
	    goto restart;
	case -1:		/* An error? */
	    if(errno == EINTR) {
		close(fd);
		return(EINTR);
		}
	    if(debug)
		perror("_sl_input: read");
	    exit(1);
	default:
	    close(fd);
	    iobuf->sb_len = nbytes;
	    break;
	    }

	}

    if(iobuf->sb_len < sizeof(u_short)) {
	iobuf->sb_len = 0;
	goto restart;
	}

    pkt_len = *(u_short *)iobuf->sb_curr;
    iobuf->sb_len -= 2;
    iobuf->sb_curr += 2;
    if(iobuf->sb_len < pkt_len) {
	iobuf->sb_len = 0;
	goto restart;
	}

    *cc = pkt_len;
    *buf = iobuf->sb_curr;
    iobuf->sb_curr += pkt_len;
    iobuf->sb_curr -= pkt_len;
    return(0);
    }


int _sl_output(iobuf)
struct sockiobuf *iobuf;
{
    int fd;

    if(iobuf->sb_len > SLMTU)
	return(EMSGSIZE);
	
    /* open the output FIFO */

    fd = open(OUTPIPE, O_WRONLY);
    if(fd < 0) {
	perror(OUTPIPE);
	return(errno);
	}

    /* glue on the length and do some output */

    iobuf->sb_curr -= sizeof(u_short);
    *(u_short *)(iobuf->sb_curr) = iobuf->sb_len;
    iobuf->sb_len += sizeof(u_short);

    (void)write(fd, iobuf->sb_curr, iobuf->sb_len);
    (void)close(fd);

    return(0);
    }


int _private_fileno(s)
int s;
{
    return(s == portlock);
    }
