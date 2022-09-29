/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char sccsid[] = "@(#)process.c	5.4 (Berkeley) 3/13/86";
#endif not lint

/*
 * process.c handles the requests, which can be of three types:
 *	ANNOUNCE - announce to a user that a talk is wanted
 *	LEAVE_INVITE - insert the request into the table
 *	LOOK_UP - look up to see if a request is waiting in
 *		  in the table for the local user
 *	DELETE - delete invitation
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <syslog.h>
#include <netdb.h>
#include <netinet/in.h>

#include <protocols/talkd.h>

char	*strcpy();
CTL_MSG *find_request();
CTL_MSG *find_match();

process_request(mp, rp)
	register CTL_MSG *mp;
	register CTL_RESPONSE *rp;
{
	register CTL_MSG *ptr;
	extern int debug;

	rp->vers = TALK_VERSION;
	rp->type = mp->type;
	rp->id_num = htonl(0);
	if (mp->vers != TALK_VERSION) {
		syslog(LOG_WARNING, "Bad protocol version %d", mp->vers);
		rp->answer = BADVERSION;
		return;
	}
	mp->id_num = ntohl(mp->id_num);
	mp->addr.sa_family = ntohs(mp->addr.sa_family);
	if (mp->addr.sa_family != AF_INET) {
		syslog(LOG_WARNING, "Bad address, family %d",
		    mp->addr.sa_family);
		rp->answer = BADADDR;
		return;
	}
	mp->ctl_addr.sa_family = ntohs(mp->ctl_addr.sa_family);
	if (mp->ctl_addr.sa_family != AF_INET) {
		syslog(LOG_WARNING, "Bad control address, family %d",
		    mp->ctl_addr.sa_family);
		rp->answer = BADCTLADDR;
		return;
	}
	mp->pid = ntohl(mp->pid);
	if (debug)
		print_request("process_request", mp);
	switch (mp->type) {

	case ANNOUNCE:
		do_announce(mp, rp);
		break;

	case LEAVE_INVITE:
		ptr = find_request(mp);
		if (ptr != (CTL_MSG *)0) {
			rp->id_num = htonl(ptr->id_num);
			rp->answer = SUCCESS;
		} else
			insert_table(mp, rp);
		break;

	case LOOK_UP:
		ptr = find_match(mp);
		if (ptr != (CTL_MSG *)0) {
			rp->id_num = htonl(ptr->id_num);
			rp->addr = ptr->addr;
			rp->addr.sa_family = htons(ptr->addr.sa_family);
			rp->answer = SUCCESS;
		} else
			rp->answer = NOT_HERE;
		break;

	case DELETE:
		rp->answer = delete_invite(mp->id_num);
		break;

	default:
		rp->answer = UNKNOWN_REQUEST;
		break;
	}
	if (debug)
		print_response("process_request", rp);
}

do_announce(mp, rp)
	register CTL_MSG *mp;
	CTL_RESPONSE *rp;
{
	struct hostent *hp;
	CTL_MSG *ptr;
	int result;

	/* see if the user is logged */
	result = find_user(mp->r_name, mp->r_tty);
	if (result != SUCCESS) {
		rp->answer = result;
		return;
	}
#define	satosin(sa)	((struct sockaddr_in *)(sa))
	hp = gethostbyaddr((char *)&satosin(&mp->ctl_addr)->sin_addr,
		sizeof (struct in_addr), AF_INET);
	if (hp == (struct hostent *)0) {
		rp->answer = MACHINE_UNKNOWN;
		return;
	}
	ptr = find_request(mp);
	if (ptr == (CTL_MSG *) 0) {
		insert_table(mp, rp);
		rp->answer = announce(mp, hp->h_name);
		return;
	}
	if (mp->id_num > ptr->id_num) {
		/*
		 * This is an explicit re-announce, so update the id_num
		 * field to avoid duplicates and re-announce the talk.
		 */
		ptr->id_num = new_id();
		rp->id_num = htonl(ptr->id_num);
		rp->answer = announce(mp, hp->h_name);
	} else {
		/* a duplicated request, so ignore it */
		rp->id_num = htonl(ptr->id_num);
		rp->answer = SUCCESS;
	}
}

#include <utmp.h>

/*
 * Search utmp for the local user
 */
find_user(name, tty)
	char *name, *tty;
{
#ifdef sgi
	struct utmp ubuf,*utmp_ptr;
#else
	struct utmp ubuf;
#endif
	int status;
	FILE *fd;
	struct stat statb;
	char ftty[20];

#ifdef sgi 
#define SCMPN(a, b)	strncmp(a, b, sizeof (a))
#define nonuser(ut) \
	 ((ut).ut_line[0] == 0 || (ut).ut_type != USER_PROCESS \
	     || strncmp((ut).ut_line, "tty", 3) \
	     && strncmp((ut).ut_line, "tport", 5) \
	     && strncmp((ut).ut_line, "console", 7))

	status = NOT_HERE;
	(void) strcpy(ftty, "/dev/");
        setutent();
	while (utmp_ptr = getutent()) {
		ubuf = *utmp_ptr;
		if (nonuser(ubuf))
			continue;
		if (SCMPN(ubuf.ut_name, name) == 0) {
			if (*tty == '\0') {
				status = PERMISSION_DENIED;
				/* no particular tty was requested */
				(void) strcpy(ftty+5, ubuf.ut_line);
				if (stat(ftty,&statb) == 0) {
					if (!(statb.st_mode & 020))
						continue;
					(void) strcpy(tty, ubuf.ut_line);
					status = SUCCESS;
					break;
				}
			}
			if (strcmp(ubuf.ut_line, tty) == 0) {
				status = SUCCESS;
				break;
			}
		}
        }
#else
	if ((fd = fopen("/etc/utmp", "r")) == NULL) {
		perror("Can't open /etc/utmp");
		return (FAILED);
	}
#define SCMPN(a, b)	strncmp(a, b, sizeof (a))
	status = NOT_HERE;
	(void) strcpy(ftty, "/dev/");
	while (fread((char *) &ubuf, sizeof ubuf, 1, fd) == 1)
		if (SCMPN(ubuf.ut_name, name) == 0) {
			if (*tty == '\0') {
				status = PERMISSION_DENIED;
				/* no particular tty was requested */
				(void) strcpy(ftty+5, ubuf.ut_line);
				if (stat(ftty,&statb) == 0) {
					if (!(statb.st_mode & 020))
						continue;
					(void) strcpy(tty, ubuf.ut_line);
					status = SUCCESS;
					break;
				}
			}
			if (strcmp(ubuf.ut_line, tty) == 0) {
				status = SUCCESS;
				break;
			}
		}
	fclose(fd);
#endif
	return (status);
}
