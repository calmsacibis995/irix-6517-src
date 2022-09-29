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
#ifndef lint
static char sccsid[] = "based upon @(#)telnetd.c 4.26 (Berkeley) 83/08/06";
static char rcsid[] = " ";
#endif

#define RESET_ANNEX_PORT
#define BROKEN_NET

#include "../inc/config.h"
#ifdef RESET_ANNEX_PORT
#include "../inc/erpc/netadmp.h"
#endif

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <sys/termio.h>


#define PORT_BASE_TELNET	5000	/* base TELNET port number (add to line #) */
#define PORT_BASE_TCP		7000	/* base TCP port number (add to line #) */

struct	portdb_struct {		/* info on each port to connect to */
	struct portdb_struct *forw;	/* next port in list (NULL if last) */
	struct portdb_struct *back;	/* prior port in list (NULL if first) */
	char	*slave_name;		/* name of slave pty (from DB file) */
	char	*annex_addr;		/* IP addr of annex (from DB file) */
	int	port_number;		/* port number to use (from DB file) */
	int	flags;			/* OR'ed together flags for this port */
	char	*fullname;		/* full pathname of slave pty */
	dev_t	device;			/* major/minor number of slave side */
	int	master_fd;		/* fd for open pty (master side) */
	int	child_pid;		/* process id of child telnet session */
	int	state;			/* state of that port */
};

	/* portdb_info.flags */
#define	PORTDB_CLEANUP	0x1		/* cleanup and remove on child exit */
#define	PORTDB_TCP	0x1000		/* use TCP instead of TELNET */
#define	PORTDB_TCP_STR	"tcp"		/* flag string in database file */

	/* portdb_info.state */
#define	PORTDB_INACTIVE	1	/* port is waiting for slave to open */
#define	PORTDB_ACTIVE	2	/* slave open, child proc supporting */
#define	PORTDB_EXITED	3	/* slave open, but child exited */

#define MAXLINEWIDTH	1024	/* max width of input line in data file */
#define MAXNAME		255	/* max width of input line in data file */

extern	char	*portdb_file;		/* name of file with name/port info */
extern	int	portdb_count;		/* count of active port structures */
extern	struct portdb_struct *portdb_info;	/* head of linked list */

#define	DEFAULT_DIRNAME	"/dev/annex"	/* default dev_dirname */
extern	char	*dev_dirname;		/* name of dir to put pty slaves in */
extern	int	slave_major;		/* major() number for slave ptys */
extern	int	reread_database;	/* if set, must reread the DB (SIGHUP) */
extern	int	reap_a_child;		/* if set, clean up exited children */

extern	void	catch_sigint();		/* catch ^C, cleanup and exit */
extern	void	catch_sighup();		/* catch -HUP, set flag to reread DB */
extern	void	catch_sigcld();		/* track exiting children in main */

extern	char	*progname;		/* name this program was invoked as */

extern	int	so_debug;		/* do socket level debugging */
extern	int	options;		/* options for the socket debugging */
extern	int	debug;			/* print some local debugging info */
extern	int	binary;			/* try for "binary" telnet session */
#ifdef RESET_ANNEX_PORT
extern	int	hangup;			/* drop DTR on the port when done */
#endif

void	datadump();		/* dump telnet info when debugging */

extern	int errno, saved_errno;
extern	struct hostent *gethostbyname();
extern	int h_errno;
extern	char *malloc();
