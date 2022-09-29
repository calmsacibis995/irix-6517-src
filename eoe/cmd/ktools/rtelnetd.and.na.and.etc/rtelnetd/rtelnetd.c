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
		port numbers,
		and comma separated flag names;
	create the directory (if required);
	for each name in the database file --
	    allocate a pseudo-tty;
	    create slave in "directory" with the name from the data file;
	wait for an open on any one of those pttys;
	fork a child with the appropriate arguments to act as a telnet or TCP
	    transparent pipe to the indicated port on the indicated annex box;
	when the child exits --
	    go back to listening for an open on that ptty;
	when a SIGHUP signal comes in --
	    remove and forget all *inactive* pttys;
	    reopen and reparse the datafile --
		allocate a pseudo-tty;
		create slave in "directory" with the name from the data file;
	    go back to waiting for incoming connections.

    This program is used to provide access to many annex ports from a
    single machine without having an rtelnet process associated with the
    currently inactive ports.
    All code for non-SGI machines (ie: under #ifdef) has been removed.

 *
 *****************************************************************************
 */

#include "rtelnetd.h"
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <ctype.h>
#include <rpcsvc/ypclnt.h>

/*
 * Allocate space for vars defined in rtelnetd.h
 */
char	*portdb_file;			/* name of file with name/port info */
int	portdb_count;			/* count of active port structures */
struct portdb_struct *portdb_info;	/* array of port info structs */

char 	*ypdomain = NULL;
char 	default_domain_name[64];
#define TRUE 1
#define FALSE 0
#define YPMAP "ktools"
#define _SGI_REENTRANT_FUNCTIONS
#define DELAY 300
#define KGENERATE "/usr/annex/kgenerate"

char	*dev_dirname;		/* name of dir to put pty slaves in */
int	slave_major;		/* major() number for slave ptys */
int	reread_database;	/* if set, must reread the DB (SIGHUP) */
int	reap_a_child;		/* if set, clean up exited children */

char	*progname;		/* name this program was invoked as */

int	so_debug;		/* do socket level debugging */
int	options;		/* options for the socket debugging */
int	debug;			/* print some local debugging info */
int	binary;			/* try for "binary" telnet session */
#ifdef RESET_ANNEX_PORT
int	hangup;			/* hangup the port when done (ie: drop DTR) */
#endif


#ifdef RESET_ANNEX_PORT
char	*optstring = "bdhmrD";
#else
char	*optstring = "bdmrD";
#endif

extern	char *optarg;		/* arg string on command line (ie: -f <name>) */
extern	int optind;		/* argv[] index of next arg on command line */
extern	int opterr;


usage()
{
	fprintf(stderr,
	    "usage: %s [-%s] <portsfile_pathname> [<dev_dirname>]\n",
	    progname, optstring);
	exit(1);
}


main(argc, argv)
	int argc;
	char *argv[];
{
	struct portdb_struct	*info;
	struct stat		statb;
	char			*bank, *unit;
	int			ch;

	progname = argv[0];
	while ((ch = getopt(argc, argv, optstring)) != -1) {
	    switch(ch) {
		case 'b': binary++;	break; /* try binary mode */
		case 'd': so_debug++;	break; /* turn on socket debugging */
#ifdef RESET_ANNEX_PORT
		case 'h': hangup++;	break; /* reset annex port on close */
#endif
		case 'D': debug++;	break; /* verbose debugging output */
		default:
		    fprintf(stderr, "%s: unknown flag '%c'\n", progname, ch);
		    usage();
	    }
	}


	/*
	 * setup NIS
	 */
	if (!getdomainname(default_domain_name, 64)) {
		ypdomain = default_domain_name;
	} else {
		fprintf(stderr, "%s: invalid domainname.\n", progname);
		exit(1);
	}
	if (strlen(ypdomain) == 0) {
		fprintf(stderr, "%s: invalid domainname.\n", progname);
		exit(1);
	}

	portdb_file = argv[optind++];

	if (optind < argc)
		dev_dirname = argv[optind++];
	else
		dev_dirname = DEFAULT_DIRNAME;
	if (stat(dev_dirname, &statb) < 0) {
		fprintf(stderr, "%s: %s: ", progname, dev_dirname);
		perror("");
		usage();
	}
	if ((statb.st_mode & S_IFMT) != S_IFDIR) {
		fprintf(stderr, "%s: %s: must be a directory\n",
				progname, dev_dirname);
		usage();
	}
	(void)umask((mode_t)0);			/* don't inherit the umask */

	sigset(SIGINT,  catch_sigint);
	sigset(SIGCLD,  catch_sigcld);
	sigset(SIGHUP,  catch_sighup);	/* HUP means to reread config file */

	parse_dbfile();

	if (!debug) {
		int fd;

		if (fork())
			exit(0);
		(void) close(0);
		(void) close(1);
		(void) close(2);

		fd = open("/dev/console", O_RDWR);
		if (fd < 0)
			fd = open("/dev/tty", O_RDWR);
		if (fd < 0)
			fd = open("/dev/null", O_RDWR);
		(void)dup2 (0, 1);
		(void)dup2 (0, 2);
		(void)ioctl(fd, TIOCNOTTY, (caddr_t)0);
	}
	setpgrp();

	if (so_debug)
		options |= SO_DEBUG;

#undef USE_SELECT
#ifdef USE_SELECT
	while (1) {
		fd_set read_bits, exc_bits;
		struct timeval giveup;
		int n_found;

		FD_ZERO(&read_bits);
		FD_ZERO(&exc_bits);
		for (info = portdb_info; info != NULL; info = info->forw) {
			if (info->state == PORTDB_INACTIVE) {
				FD_SET(info->master_fd, &read_bits);
				FD_SET(info->master_fd, &exc_bits);
			}
		}
		giveup.tv_sec = DELAY;
		giveup.tv_usec = 0;
		n_found = select(portdb_count+5, &read_bits, 0, &exc_bits,
						 &giveup);
		if ((n_found < 0) && (errno != EINTR)) {
			perror("select");
			exit(1);
		}
		if (debug > 2) {
			fprintf(stderr,
				"select: %d found, read=0x%x, exc=0x%x %s\n",
				n_found, read_bits.fds_bits[0],
				exc_bits.fds_bits[0],
				(n_found < 0) ? "(interrupted)" : "");
		}
		if (reread_database || (n_found == 0)) {
			parse_dbfile();
		}
		if (reap_a_child) {
			reap_children();
		}
		if (n_found <= 0)
			continue;
		for (info = portdb_info; info != NULL; info = info->forw) {
			if ((info->state == PORTDB_INACTIVE) &&
			    (FD_ISSET(info->master_fd, &read_bits))) {
				start_connection(info);
			}
			else if (FD_ISSET(info->master_fd, &exc_bits)) {
				(void)close(info->master_fd);
				open_slave(info);
			}
		}
	}
#else /* USE_POLL */
	{
#include <poll.h>
	struct pollfd *pollfds, *pfd;
	int n_found;

	pollfds = (struct pollfd *)calloc(portdb_count, sizeof(*pfd));
	if (pollfds == NULL) {
		fprintf(stderr,
			"%s: Couldn't malloc space for poll structs\n",
			progname);
		perror("malloc(pollfds struct)");
		exit(1);
	}

	while (1) {
		for (info = portdb_info, pfd = pollfds; info != NULL;
			  info = info->forw, pfd++) {
			if (info->state == PORTDB_INACTIVE) {
				pfd->fd = info->master_fd;
				pfd->events = POLLIN | POLLPRI;
			}
		}
		n_found = poll(pollfds, portdb_count, DELAY*1000);
		if ((n_found < 0) && (errno != EINTR)) {
			perror("poll");
			exit(1);
		}
		if (debug > 2) {
			fprintf(stderr, "poll: %d found, %s\n", n_found, 
					(n_found < 0) ? "(interrupted)" : "");
		}
		if (reread_database || (n_found == 0)) {
			parse_dbfile();
		}
		if (reap_a_child) {
			reap_children();
		}
		if (n_found <= 0)
			continue;
		for (info = portdb_info, pfd = pollfds; info != NULL;
			  info = info->forw, pfd++) {
			if ((info->state == PORTDB_INACTIVE) &&
			    ((pfd->revents & (POLLIN|POLLPRI)) != 0)) {
				start_connection(info);
			}
			else if ((pfd->revents & (POLLERR|POLLHUP)) != 0) {
				close(info->master_fd);
				open_slave(info);
			}
		}
	}
	}
#endif /* SELECT or POLL */
}

int 
reread_dbase(int doit)
{
	static long portdb_timestamp = 0; /* time in secs of last update */
	int scratch_timestamp, len, err;
	char *val;

	if (debug)
		fprintf(stderr, "reread_dbase()\n");

	/* 
	 * determine if we need to re-read the NIS map.
	 * naturally,  if portdb_timestamp is zero, we have to.
	 */
	err = ypmatch("timestamp", &val, &len);
	if (err) {
		fprintf(stderr,
			"%s: unable to locate timestamp in ktools NIS map.\n",
			progname);
		exit(1);
	}
	if ((sscanf(val, "%d", &scratch_timestamp)) != 1) {
		fprintf(stderr,
			"%s: invalid timestamp in ktools NIS map.\n",
			progname);
		exit(1);
	}
	if (scratch_timestamp <= portdb_timestamp) {
		return(0);
	}
	if (doit)
		portdb_timestamp = scratch_timestamp;
	return(1);
}

parse_dbfile()
{
	struct portdb_struct *info;

	if (reread_dbase(1)) {
		reap_old_ports();
		parse_rtelnetd("con");
		parse_rtelnetd("dbg");
		if (debug)
			fprintf(stderr, "system(KGENERATE)\n");
		system(KGENERATE);
		for (info = portdb_info; info != NULL; info = info->forw)
			if (!(info->flags & PORTDB_CLEANUP))
				open_slave(info);
	}
}

parse_rtelnetd(char *type)
{
	struct portdb_struct *info;
	int port[9], portcount, len, line, nargs, i, j, err;
	char *val, *scratch, *strtok_lasts = NULL;
	char strings[4][64], temp_string1[30];

	info = NULL;

	strcpy(strings[0], "rtelnetd_");
	strcat(strings[0], type);

	/*
	 * get the console connections
	 */
	if ((err = ypmatch(strings[0], &val, &len))) {
		fprintf(stderr,
			"%s: unable to find %s key in ktools map, %d.\n",
			strings[0], progname, err);
		return(1);
	}
	val[len] = NULL;				/* nuke that newline */

	if ((scratch = strtok_r(val, "`", &strtok_lasts)) == NULL) {
		fprintf(stderr, "Hmm, nothing in %s key....\n", strings[0]);
		exit(1);
	}

	while (scratch) {
		if ((sscanf(scratch, "%[-.A-Za-z0-9]:%s", strings[0],
				     temp_string1)) != 2) {
			fprintf(stderr,
				"problems parsing host/console combo:\n\t%s\n",
				scratch);
			exit(1);
		}

		/*
		 * strings[0] now holds host, temp_string1 holds all port #s
		 */
		portcount = sscanf(temp_string1, "%d,%d,%d,%d,%d,%d,%d,%d",
						 &port[0], &port[1], &port[2],
						 &port[3], &port[4], &port[5],
						 &port[6], &port[7], &port[8]);
		if (!portcount) {
			fprintf(stderr, "problems parsing list:\n\t%s\n",
					temp_string1);
			exit(1);
		}

		/*
		 * get annex IP addr
		 */
		if ((err = find_annex_addr(strings[0], strings[1]))) {
			fprintf(stderr,
				"%s: couldn't find the annex for %s, %s.\n",
				progname, strings[0], strings[1]);
			goto nextloop;
		}
		len = strlen(strings[1]);

		/*
		 * loop through and create all console entries.
		 */
		for (i = 0; i < portcount; i++) {
			/*
			 * If we don't have one already, allocate a port
			 * structure
			 */
			if (info == NULL) {
				info = (struct portdb_struct *)malloc((unsigned)sizeof(struct portdb_struct));
				if (info == NULL) {
					fprintf(stderr,
						"%s: Couldn't malloc space for a port struct\n",
						progname);
					perror("malloc(port struct)");
					exit(1);
				}
				bzero(info, sizeof(struct portdb_struct));
			}

			info->annex_addr = malloc((unsigned)len+2);
			if (info->annex_addr == NULL) {
				fprintf(stderr,
					"%s: Couldn't malloc space for Annex name\n",
					progname);
				perror("malloc(annex addr)");
				exit(1);
			}
			strcpy(info->annex_addr, strings[1]);

			/* 
			 * Create our port name
			 */
			sprintf(strings[2], "%s.%s.%d", strings[0], type, i);

			/*
			 * Check the name of the slave pty
			 */
			len = strlen(strings[2]);
			for (j = 0; j != len; j++) {
				if (!isgraph(strings[2][j])) {
					fprintf(stderr,
						"%s: %s ignored: slave pty name has bad char in it\n",
						progname, strings[0]);
					break;
				}
			};
			if (j != len)
				break;

			/*
			 * Store the name of the slave pty
			 */
			info->slave_name = malloc((unsigned)len+2);
			if (info->slave_name == NULL) {
				fprintf(stderr,
					"%s: Couldn't malloc space for slave pty name\n",
					progname);
				perror("malloc(slave pty name)");
				exit(1);
			}
			strcpy(info->slave_name, strings[2]);

			/*
			 * Build the full pathname of the slave pty
			 */
			len = strlen(dev_dirname) + 1 + strlen(info->slave_name);
			if (len > PATH_MAX) {
				fprintf(stderr,
					"%s: %s ignored: slave pty name too long\n",
					progname, info->slave_name);
				break;
			}
			
			info->fullname = malloc((unsigned)len+2);
			if (info->fullname == NULL) {
				fprintf(stderr,
					"%s: Couldn't malloc space for full pty name\n",
					progname);
				perror("malloc(full slave pty name)");
				exit(1);
			}
			strcpy(info->fullname, dev_dirname);
			strcat(info->fullname, "/");
			strcat(info->fullname, info->slave_name);

			/*
			 * Pick up and check the port number to use on
			 * the Annex box
			 */
			info->port_number = port[i];
			if ((info->port_number < 0) ||
			    (info->port_number > 64)) {
				fprintf(stderr,
					"%s: configuration error: %s ignored: out of range port number (%d)\n",
					progname, info->slave_name,
					info->port_number);
				break;
			}

			/*
			 * We made it this far, so it must all be filled in
			 * and OK.  Link it onto the head of the list.
			 */
			info->forw = portdb_info;
			info->back = NULL;
			if (info->forw)
				info->forw->back = info;
			portdb_info = info;
			portdb_count++;

			info = NULL;	/* allocate a new one */

			/* print out portdb_info struct */
			if (debug) {
				fprintf(stderr, "slave_name:\t%s\n",
						portdb_info->slave_name);
				fprintf(stderr, "annex_addr:\t%s\n",
						portdb_info->annex_addr);
				fprintf(stderr, "port_number:\t%d\n",
						portdb_info->port_number);
				fprintf(stderr, "fullname:\t%s\n",
						portdb_info->fullname);
			}
		}
nextloop:
		scratch = strtok_r(NULL, "`", &strtok_lasts);
	}
	if (info)
		free(info);
}


int
find_annex_addr(char *host, char *return_hostname)
{
	int len, i, found, err;
	char *val, *scratch;

	if ((err = ypmatch(host, &val, &len))) {
		fprintf(stderr,
			"%s: Unable to find entry for %s in ktools NIS map.\n",
			progname, host);
		return(1);
	}

	/*
	 * step through info till we get what we want
	 */
	i = 0;
	found = 0;
	while ( i < len && found != 15 ) {
		if (val[i++] == '`')
			found++;
	} 
	if ((sscanf(&val[i], "%[-.A-Za-z0-9]", return_hostname)) != 1) {
		fprintf(stderr, "%s: Unable to determine annex for %s:\n\t%s\n",
				progname, host, &val[i]);
		return(1);
	}

	/*
	 * Pick up and check the Annex box IP address
	 */
	len = strlen(return_hostname);
	if (len > MAXHOSTNAMELEN) {
		fprintf(stderr,
			"%s: %s ignored: annex IP address string too long\n",
			progname, return_hostname);
		return(1);
	}
	if ((gethostbyname(return_hostname) == NULL) &&
	    (h_errno == HOST_NOT_FOUND)) {
		fprintf(stderr,
			"%s: %s ignored: annex IP address not resolvable\n",
			progname, return_hostname);
		return(1);
	}
	return(0);
}

  

ypmatch(char *key, char **val, int *len)
{
	int err, error = FALSE;

	*val = NULL;
	*len = 0;
	err = yp_match(ypdomain, YPMAP, key, strlen(key), val, len);

	if (err == YPERR_KEY) {
		err = yp_match(ypdomain, YPMAP, key, (strlen(key) + 1),
					 val, len);
	}

	if (err)
		error = TRUE;
	return (error);
}

scan_line(buffer, line, strings, num_strings)
	char *buffer;
	int line;
	char *strings[];
	int num_strings;
{
	int in_string, which, i;
	char *ch;

	for (i = 0; i < num_strings; i++)
		strings[i] = NULL;

	in_string = which = 0;
	for (ch = buffer, i = 0; (*ch) && (i < MAXLINEWIDTH); ch++, i++) {
		if ((*ch == ' ') || (*ch == '\t') || (*ch == '\n')) {
			if (in_string) {
				*ch = '\0';
				if (++which == num_strings)
					break;	/* got all strings, leave */
				in_string = 0;
			}
		} else if (*ch == '#') {
			break;			/* comment: ignore the rest */
		} else if (!in_string) {
			strings[which] = ch;
			in_string = 1;		/* starting new string */
		}
	}
	return(which);
}

/*
 * Free up a port structure.
 */
free_port(info)
	struct portdb_struct *info;
{
	if (portdb_info == info)
		portdb_info = info->forw;
	if (info->forw)
		info->forw->back = info->back;
	if (info->back)
		info->back->forw = info->forw;
	free(info->slave_name);
	free(info->fullname);
	free(info->annex_addr);
	free(info);
	portdb_count--;
}

/*
 * open the slave side of the pty for a port,
 * getting a new pty minor() number and
 * removing whatever is there already.
 */
open_slave(info)
	struct portdb_struct *info;
{
	struct termio term;
	struct stat statb;
	int on = 1;

	/*
	 * Get the major number for slave pseudo-ttys (only the first time)
	 */
	if (!slave_major) {
		if (stat("/dev/ttyq1", &statb) < 0) {
			fprintf(stderr, "%s: Couldn't stat slave pty #1\n",
					progname);
			perror("stat(slave pty #1)");
			exit(1);
		}
		slave_major = major(statb.st_rdev);
	}

	/*
	 * Remove the slave if it currently exists
	 */
	if (remove_slave(info) < 0)
		return;

	/*
	 * SGI-style ptys are a single bank of many units with a weird master.
	 * master="ptc", slave="ttyqU" where U=[0-9][0-9].
	 * Openning the (single) master pty allocates a new master/slave pair.
	 * The minor(st_rdev) number returned by a stat() on the master is the
	 * unit number that we have been assigned.  It is up to us to open or
	 * create the slave side using the unit number returned by stat().
	 */
	if ((info->master_fd = open("/dev/ptc", O_RDWR|O_NDELAY)) < 0) {
		fprintf(stderr, "%s: Couldn't open master pty\n", progname);
		perror("open(master pty)");
		exit(1);
	}
	if (fstat(info->master_fd, &statb) < 0) {
		fprintf(stderr, "%s: Couldn't stat master pty\n", progname);
		perror("stat(master pty)");
		exit(1);
	}

	/*
	 * Create the slave pty node with the right unit number
	 */
	info->state = PORTDB_INACTIVE;
	info->device = makedev(slave_major, minor(statb.st_rdev));
	if (mknod(info->fullname, (mode_t)(S_IFCHR|0666), info->device) < 0) {
		fprintf(stderr, "%s: Couldn't mknod() slave pty: %s\n",
				progname, info->fullname);
		perror("mknod(slave pty)");
		exit(1);
	}
	if (debug)
		fprintf(stderr,
			"slave created: unit=%d, port=%d, path=%s, fd=%d\n",
			minor(statb.st_rdev), info->port_number,
			info->fullname, info->master_fd);
	/*
	 * Do some "SVR4 Compatibility Magic" crap.
	 */
	if (ioctl(info->master_fd, UNLKPT, NULL) < 0) {
		perror("ioctl(UNLKPT on master pty)");
		exit(1);
	}

	/*
	 * Set the line control bits for the newly openned pty
	 */
	if (ioctl(info->master_fd, TCGETA, (caddr_t)&term) < 0) {
		perror("ioctl(TCGETA on master pty)");
		exit(1);
	}
	term.c_oflag = (term.c_oflag & ~TABDLY) | TAB3 | ONLCR;
	term.c_cflag &= ~PARENB;
	if (ioctl(info->master_fd, TCSETA, (caddr_t)&term) < 0) {
		perror("ioctl(TCSETA on master pty)");
		exit(1);
	}
#ifdef USE_FIONBIO
	if (ioctl(info->master_fd, FIONBIO, (caddr_t)&on) < 0) {
		perror("ioctl(FIONBIO on master pty)");
		exit(1);
	}
#endif
}

/*
 * Remove the slave if it currently exists
 */
remove_slave(info)
	struct portdb_struct *info;
{
	struct stat statb;

	if (stat(info->fullname, &statb) >= 0) {
		if (((statb.st_mode & S_IFMT) != S_IFCHR) ||
		    (major(statb.st_rdev) != slave_major)) {
			fprintf(stderr, "%s: File %s not a slave pty, not removed\n",
					progname, info->fullname);
			return(-1);
		}
		if (unlink(info->fullname) < 0) {
			fprintf(stderr, "%s: Unlink failed: %s \n",
					progname, info->fullname);
			perror("unlink(slave pty)");
			return(-1);
		}
		if (debug)
			fprintf(stderr, "slave pty removed: unit=%d, port=%d, path=%s\n",
					minor(statb.st_rdev), info->port_number,
					info->fullname);
	}
	return(0);
}

/*
 * Free up all slave ptys and port structures that aren't in active use.
 */
reap_children()
{
	struct portdb_struct *info;

	reap_a_child = 0;
restart:
	/*
	 * After changing the portdb_info linked list, loop pointer isn't
	 * valid anymore, so we have to start the search all over again.
	 */
	for (info = portdb_info; info != NULL; info = info->forw) {
		if (info->state == PORTDB_EXITED) {
			if (info->flags & PORTDB_CLEANUP) {
				close(info->master_fd);
				free_port(info);
				goto restart;
			} else {
				close(info->master_fd);
				open_slave(info);
			}
		}
	}
}

/*
 * Free up all slave ptys and port structures that aren't in active use.
 */
reap_old_ports()
{
	struct portdb_struct *info;

	reread_database = 0;
restart:
	/*
	 * After changing the portdb_info linked list, loop pointer isn't
	 * valid anymore, so we have to start the search all over again.
	 */
	for (info = portdb_info; info != NULL; info = info->forw) {
		switch(info->state) {
		default:
			fprintf(stderr, "%s: port %d (%s) in unknown state\n",
					progname, info->port_number,
					info->fullname);
			/*FALLTHROUGH*/
		case PORTDB_INACTIVE:
		case PORTDB_EXITED:
			close(info->master_fd);
			remove_slave(info);
			free_port(info);
			goto restart;
			break;

		case PORTDB_ACTIVE:
			remove_slave(info);
			break;

		}
	}
}



/*
 * Signal handling functions
 */
void
catch_sigcld(arg)
int arg;
{
	struct portdb_struct *info;
	int pid, status;

	if ((pid = wait(&status)) < 0) {
		perror("wait(exiting child process)");
		exit(1);
	}
	if (debug)
		fprintf(stderr, "child exited: %d\n", pid);

	if (WIFEXITED(status) || WIFSIGNALED(status)) {
		for (info = portdb_info; info != NULL; info = info->forw) {
			if (pid == info->child_pid) {
				info->state = PORTDB_EXITED;
				info->child_pid = 0;
				reap_a_child++;
				break;
			}
		}
		if (info == NULL) {
			fprintf(stderr, "%s: signal caught for unknown child\n",
					progname);
		}
	}
	sigset(SIGCLD, catch_sigcld);
}

void
catch_sighup(int arg)
{
	struct portdb_struct *info;

	if (debug)
		fprintf(stderr,"catch_sighup()\n");
	if (reread_dbase(0)) {
		for (info = portdb_info; info != NULL; info = info->forw) {
			if (info->state == PORTDB_ACTIVE)
				info->flags |= PORTDB_CLEANUP;
		}
		reread_database++;
	}
	sigset(SIGHUP, catch_sighup);
}


void
catch_sigint()
{
	if (debug)
		fprintf(stderr,"catch_sigint()\n");
	exit(1);
}
