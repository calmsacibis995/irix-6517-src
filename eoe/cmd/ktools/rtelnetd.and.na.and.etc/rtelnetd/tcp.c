/*
 *****************************************************************************
 *
 *        Copyright 1989, 1990, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use. 
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Module Description::
 *
 * 	Annex Reverse Telnet Daemon
 *	modified telnetd to provide host-pty/annex-port association
 *
 * Original Author: Paul Mattes		Created on: an impulse
 *

   Modified by curtis@sgi.com during 6/91
      Added support for inetd-like operation --
	Now takes as arguments --
	    a directory name,
	    a data file of relations between --
		pseudo-tty names,
		annex IP addresses,
		and port numbers;
	create the directory (if required);
	allocate enough pseudo-ttys for all the names in the data file;
	create new ptty slaves in "directory" with names from the data file;
	wait for an open on any one of those pttys;
	fork a child with the appropriate arguments to act as a telnet
	    transparent pipe to the indicated port on the indicated annex box;
	when the child exits, go back to listening for an open on that ptty.
    This program is used to provide access to many annex ports from a
    single machine without having an rtelnet process associated with the
    currently inactive ports.
    All code for non-SGI machines (ie: under #ifdef) has been removed.

 *
 *****************************************************************************
 */

#include "rtelnetd.h"


/*
 *****************************************************************************
 *
 * Most of the rest of the file is a rip-off from the telnetd daemon.
 *
 *****************************************************************************
 */

/*
 * I/O data buffers, pointers, and counters.
 */
extern char	ptyibuf[], *ptyip;
extern char	ptyobuf[], *pfrontp, *pbackp;
extern char	netibuf[], *netip;
extern char	netobuf[], *nfrontp, *nbackp;

extern int	net, pty;		/* fd's for net connection and for pty */
extern int	port_num;		/* port number requested on annex box */

extern struct	sockaddr_in sin;
extern struct	sockaddr_in sin2;

extern int	pcc, ncc;
extern int	saved_errno;


/*
 * Main loop for TCP.  Select from pty and network, and
 * hand data to telnet receiver finite state machine.
 */
do_tcp()
{
	int on = 1;
	int net_errno, pty_errno;

#ifdef SIGTSTP
	signal(SIGTSTP, SIG_IGN);
#endif

	pcc = 0; ncc = 0;

	if (debug > 2)
		fprintf(stderr,"fd: net=%d, pty=%d\n", net, pty);

	for (;;) {
		fd_set ibits, obits, xbits;
		register int c;
		int n_found;

		/*
		 * Never look for input if there's still
		 * stuff in the corresponding output buffer
		 */
		FD_ZERO(&ibits);
		FD_ZERO(&obits);
		FD_ZERO(&xbits);
		FD_SET(net, &xbits);
		FD_SET(pty, &xbits);
		if ((nfrontp - nbackp) || (pcc > 0))
			FD_SET(net, &obits);
		else
			FD_SET(pty, &ibits);

		if (pfrontp - pbackp)
			FD_SET(pty, &obits);
		else
			FD_SET(net, &ibits);

		if (ncc < 0 && pcc < 0)
			break;

		if (debug > 2)
			fprintf(stderr, "selecting: i=%#x, o=%#x, x=%#x\n",
				ibits.fds_bits[0], obits.fds_bits[0],
				xbits.fds_bits[0]);
		n_found = select(FD_SETSIZE, &ibits, &obits, &xbits,
					     (struct timeval *)0);
		if (n_found < 0) {
			if (errno == EINTR) {
				fprintf(stderr,"select interrupted\n");
				continue;
			}
			perror("rtelnetd: select");
			exit(1);
		}
		if (debug > 2)
			fprintf(stderr,"%d found: i=%#x, o=%#x, x=%#x\n",
					   n_found, ibits.fds_bits[0],
					   obits.fds_bits[0],
					   xbits.fds_bits[0]);
		if (n_found == 0)
			continue;

		/*
		 * Something to read from the network...
		 */
		if (FD_ISSET(net, &ibits) || FD_ISSET(net, &xbits)) {
			if (debug > 1)
				fprintf(stderr, "net: ");
			ncc = read(net, netibuf, BUFSIZ);
			if (debug > 1) {
				saved_errno = errno;
				fprintf(stderr, "returns %d errno %d\n",
						ncc, errno);
				errno = saved_errno;
			}
			if (ncc < 0 && errno == EWOULDBLOCK)
				ncc = 0;
			else {
				if (ncc <= 0) {
					net_errno = (ncc < 0) ? errno : 0;
					pty_errno = 0;
					break;
				}
				netip = netibuf;
			}
			if (debug > 4 && ncc > 0)
				datadump(netip, ncc);
		}

		/*
		 * Something to read from the pty...
		 */
		if (FD_ISSET(pty, &ibits) || FD_ISSET(pty, &xbits)) {
			if (debug > 1)
				fprintf(stderr, "pty: pcc was %d ", pcc);
			pcc = read(pty, ptyibuf, BUFSIZ);
			if (debug > 1) {
				saved_errno = errno;
				fprintf(stderr, "returns %d errno %d\n",
						pcc, errno);
				errno = saved_errno;
			}
			if (pcc < 0 && errno == EWOULDBLOCK)
				pcc = 0;
			else {
				if (pcc <= 0) {
					net_errno = 0;
					pty_errno = (pcc < 0) ? errno : 0;
					break;
				}
				ptyip = &ptyibuf[0];
				if (debug > 2) {
					fprintf(stderr, "pkt- %x, cnt- %d\n",
							ptyibuf[0], pcc);
				}
			}
			if (debug > 4 && pcc > 0)
				datadump(ptyip, pcc);
		}

		while (pcc > 0) {
			if ((&netobuf[BUFSIZ] - nfrontp) < 2)
				break;
			c = *ptyip++ & 0377, pcc--;
			*nfrontp++ = c;
		}

		if (FD_ISSET(net, &obits) && (nfrontp - nbackp) > 0)
			netflush();
		if (ncc > 0) {
			while (ncc > 0) {
				if ((&ptyobuf[BUFSIZ] - pfrontp) < 2)
					return;
				c = *netip++ & 0377, ncc--;
				*pfrontp++ = c;
			}
		}
		if ((pfrontp - pbackp) > 0)
			ptyflush();
		if (FD_ISSET(pty, &obits) && (pfrontp - pbackp) > 0)
			ptyflush();
	}

	if (debug > 1) {
		fprintf(stderr,
			"out of loop pcc=%d pty_errno=%d ncc=%d net_errno=%d\n",
			pcc, pty_errno, ncc, net_errno);
	}

	/*
	 * I/O error from pty or it looks like somebody closed the slave device.
	 * Simply exit.
	 */
	if (pty_errno
#ifdef BROKEN_NET
/*
 * some systems don't correctly indicate close of the connection!
 */
	    || !net_errno
#endif
	) {
		int off = 0;

#ifdef USE_FIONBIO
		ioctl(f, FIONBIO, (caddr_t)&off);	/* blocking reads on closing */
#endif
#ifdef RESET_ANNEX_PORT
		if (hangup) {		/* we also need to set_global_passwd */
			(void)reset_line( (struct sockaddr_in *)&sin,
					  (u_short)SERIAL_DEV,
					  (u_short)port_num);
		}
#endif
	}
	if (shutdown(net, 2) == -1)	/* Telnet connection down */
		perror("shutdown");
	(void)close(net);
}
