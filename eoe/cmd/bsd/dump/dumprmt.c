#ident "$Revision: 1.9 $"

/*
 *  Modified to use send()/recv() instead of write()/read(),
 *  to work around an IRIX 3.* bug which prevents sending more
 *  than 2 Gbytes of data over a single TCP connection.
 *
 *  Also modified for significantly better error handling.
 *  This includes new routine rmtreset() called in dumptape.c
 *  This is so that if remote tape daemon dies, then current process will
 *  exit X_REWRITE, and the previous process will still have 'rmtape'
 *  indicating the dead connection.  When it forks again, the new child
 *  will get SIGPIPE errors, and won't be able to recover, so it will
 *  exit X_REWRITE, and we get nowhere.  By having the X_REWRITE case
 *  call rmtreset(), the problem can be immediately cleaned up.
 *	-Mike Muuss, BRL, 8-March-1991.
 */

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 * static char sccsid[] = "@(#)dumprmt.c	5.5 (Berkeley) 10/22/87";
 */

#if _DUMP_
#include "dump.h"
#else
#include "restore.h"
#endif
#include <sys/mtio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pwd.h>
#include "dumprmt.h"

#define	TS_CLOSED	0
#define	TS_OPEN		1

static int	doingvers;
static int	rmtape = -1;
static char	*rmtpeer;
static char	*rmtpeer_orig;
static int	rmtstate = TS_CLOSED;
static int	server_version = -1;

static int	okname(char *);
static int	rmtcall(const char *, char *);
static void	rmtconnaborted();
static void	rmterror(char *);
static void	rmtgetconn(void);
static int	rmtgetb(void);
static int	rmtgets(char *, int);
static int	rmtreply(const char *);

int
rmthost(char **host)
{
	rmtpeer = *host;
	rmtpeer_orig = strdup(rmtpeer);
	signal(SIGPIPE, rmtconnaborted);
	rmtgetconn();
	*host = rmtpeer;
	if (rmtape < 0)
		return (0);
	return (1);
}

void
rmtreset(void)
{
	if (rmtape != -1)
		(void)close(rmtape);
	rmtape = -1;
	rmtpeer = rmtpeer_orig;
	rmtpeer_orig = strdup(rmtpeer);	/* rmtpeer clobbered by rmtgetconn */
	rmtgetconn();
	if( rmtape < 0 )  {
		msg("Unable to reset connection to %s\n", rmtpeer);
		exit(X_ABORT);
	}
}

static void
rmtconnaborted()
{
	fprintf(stderr, "dump: Lost connection to remote host (SIGPIPE).\n");
	if (rmtape != -1)
		(void) close(rmtape);
	rmtape = -1;
	exit(X_REWRITE);
}

/*
 *  Simply log an error message, and close link to remote machine.
 *  Propagating the error condition upwards is the duty of the caller.
 */
static void
rmterror(char *str)
{
	msg("ERROR %s, disconnecting from %s.\n", str, rmtpeer);
	if (rmtape != -1)
		(void)close(rmtape);
	rmtape = -1;
	errno = 0;
}

static void
rmtgetconn(void)
{
	static struct servent *sp = 0;
	static struct passwd *pw = 0;
	char *name;
	char *tmphost;
	int size;

	if (sp == 0) {
		sp = getservbyname("shell", "tcp");
		if (sp == 0) {
			fprintf(stderr, "rdump: shell/tcp: unknown service\n");
			exit(1);
		}
	}
	if (pw == 0) {
		pw = getpwuid(getuid());
		if (pw == 0) {
			fprintf(stderr, "rdump: who are you?\n");
			exit(1);
		}
	}
	/*
	 * The format now is user@host:device (not host.user:device)
	 */
	tmphost = rindex(rmtpeer, '@');
	if (tmphost) {
		*tmphost++ = 0;
		name = rmtpeer;
		rmtpeer = tmphost;
		if (!okname(name))
			exit(1);
	} else {
		name = pw->pw_name;
	}
	rmtape = rcmd(&rmtpeer, sp->s_port, pw->pw_name, name, "/etc/rmt", 0);
	if (rmtape < 0)
		return;
	size = ntrec * TP_BSIZE;
	while (size > TP_BSIZE &&
	    setsockopt(rmtape, SOL_SOCKET, SO_SNDBUF, &size, sizeof (size)) < 0)
		size -= TP_BSIZE;
}

static int
okname(char *cp0)
{
	char *cp;
	int c;

	for (cp = cp0; *cp; cp++) {
		c = *cp;
		if (!isascii(c) || !(isalnum(c) || c == '_' || c == '-')) {
			fprintf(stderr, "rdump: invalid user name %s\n", cp0);
			return (0);
		}
	}
	return (1);
}

int
rmtopen(const char *tape, int mode, ...)
{
	char buf[256];
	int rfd, rc, failedv = 0;

	doingvers = 0;	/* be paranoid */
	if (rmtape < 0)  {
		rmtpeer = rmtpeer_orig;
		rmtpeer_orig = strdup(rmtpeer);
		rmtgetconn();
		if( rmtape < 0 )  return (-1);
	}

reopen:
	(void)sprintf(buf, "O%s\n%d\n", tape, mode);
	rmtstate = TS_OPEN;
	rfd = rmtcall(tape, buf);
	if(rfd == -1)
		return rfd;
	if(failedv == 0) {
		/* determine which version for mtiocget; have to close
		 * and reopen, because most older rmt servers would exit
		 * on unrecognized commands. */
		strcpy(buf, "V2\n");
		doingvers++;
		rc = rmtcall("ioctl", buf);
		doingvers--;
		if(rc == -1) {
			failedv = 1;
			rmtreset();
			sleep(10);	/* allow time for cleanup. hack, hack... */
			goto reopen;
		}
		if(rc > 0)
			server_version = rc;
	}

	return rfd;
}

/* ARGSUSED */
int
rmtclose(int fd)
{
	int status;

	if (rmtstate != TS_OPEN)  {
		return -1;
	}
	if (rmtape < 0 ) {
		/* Special handling for dead remote machines:
		 * pretend it closed OK.
		 */
		rmtstate = TS_CLOSED;
		return 0;
	}
	status = rmtcall("close", "C\n");
	rmtstate = TS_CLOSED;
	return status;
}

int
rmtread(char *buf, int count)
{
	char line[30];
	int n, i, cc;

	if (rmtape < 0 ) return -1;

	(void)sprintf(line, "R%d\n", count);
	n = rmtcall("read", line);
	if (n < 0) {
		errno = n;
		return (-1);
	}
	for (i = 0; i < n; i += cc) {
		cc = recv(rmtape, buf+i, n - i, 0);
		if (cc <= 0) {
			rmterror("rmtread <= 0");
			return (i);
		}
	}
	return (n);
}

int
rmtwrite(char *buf, int count)
{
	char line[30];
	int	len;

	if (rmtape < 0 ) return -1;

	(void)sprintf(line, "W%d\n", count);
	len = strlen(line);
	if (send(rmtape, line, len, 0) != len ||
	    send(rmtape, buf, count, 0) != count) {
		perror("send");
		return -1;
	}
	return (rmtreply("write"));
}

int
rmtseek(int offset, int pos)
{
	char line[80];

	(void)sprintf(line, "L%d\n%d\n", offset, pos);
	return (rmtcall("seek", line));
}

int
rmtioctl(int cmd, void *arg)
{
	char buf[256];
	char c;
	int rc, cnt, ssize;
	char *p, *omtget;
	short mt_type;

	/* MTIOCOP is the easy one. nothing is transfered in binary */
	if (cmd == MTIOCTOP) {
		sprintf(buf, "I%d\n%d\n", ((struct mtop *) arg)->mt_op,
			((struct mtop *) arg)->mt_count);
		return (rmtcall("ioctl", buf));
	}
	else if(cmd == MTIOCGET) {
		/*
		 *	grab the status and read it directly into the structure
		 *  Since the data is binary data, and the other machine might
		 *  The original code could overwrite the space it malloced;
		 *  must be careful to not do that!
		 *  NOTE: the original /etc/rmt did NOT support a newline after
		 *  the S command, and Sun still does not.  Neither does the
		 *  current bsd source, all the way through the tahoe release.
		 *  So do NOT add the \n to this!  The sgi rmt command will
		 *  work either way.  Olson, 4/91
		 */
		strcpy(buf, "S");
		if((rc=rmtcall("ioctl", buf)) == -1)
			return rc;
		
		ssize = rc;

		/*
		 * if server is of the old version, then the mtget struct
		 * that it returns is not the same as the one that we knows
		 * so lets convert it into something that we can use
		 * (note this assumption may no longer be true either, since
		 * sizeof old_mtget is 16, but some Sun machines return
		 * 20 bytes of data, at least one running 4.0.3 does...
		 */
		if (server_version == -1) {
			p = omtget = (char *)malloc(sizeof(struct old_mtget));
			if (p == (char *)0) {
				errno = ENOMEM;
				return(-1);
			}
			if(sizeof(struct old_mtget) < ssize)
				ssize = sizeof(struct old_mtget);
		} else {
			if(sizeof(struct mtget) < ssize)
				ssize = sizeof(struct mtget);
			p = arg;
		}
		

		rc -= ssize;
		for (; ssize > 0; ssize--)
		{
			cnt = rmtgetb();
			if (cnt < 0)
			{
abortit:
				errno = EIO;
				return(-1);
			}
			*p++ = cnt;
		}

		/* handle any bytes we didn't know what to do with */
		while(rc-- > 0)
			if(rmtgetb() == -1)
				goto abortit;

		/*
		 *	now we check for byte position. mt_type is a small
		 *	integer field (normally) so we will check its
		 *	magnitude. if it is larger than
		 *	256, we will assume that the bytes are swapped and
		 *	go through and reverse all the bytes
		 */
		if (server_version == -1)  
			p = omtget;
		else
			p = arg;

		mt_type = ((struct mtget *) p)->mt_type;

	
		if (mt_type >= 256) {
			/* assume that we need to swap byte */
			for (cnt = 0; cnt < rc; cnt += 2)
			{
				c = p[cnt];
				p[cnt] = p[cnt+1];
				p[cnt+1] = c;
			}
		}

		/* 
		 * now mtgetp has the correct (byte-swapped if needed)
		 * data, if server is of old version then lets convert
		 * the data into something that we can use
		 * else all done
		 */
		if (server_version == -1) {
			struct mtget *newp = (struct mtget *)arg;
			struct old_mtget *oldp = (struct old_mtget *)omtget;

			newp->mt_type = oldp->mt_type;
			newp->mt_dsreg = oldp->mt_dsreg;
			newp->mt_erreg = oldp->mt_erreg;
			newp->mt_resid = oldp->mt_resid;
			newp->mt_fileno = oldp->mt_fileno;
			newp->mt_blkno = oldp->mt_blkno;

			/* 
			 * dsreg has the HW specific bits set and it is
			 * different bet. tape driver, that is why
			 * dposn is invented in the newer version so that 
			 * the code that deciphers dposn can be generic
			 * old version doesn't know about dposn, so just
			 * set it to 0 to get consistent result
			 */
			newp->mt_dposn = 0;
		}

		return(0);
	}
	else {
		errno = EINVAL;
		return(-1);
	}
}

static int
rmtcall(const char *cmd, char *buf)
{
	int	len = strlen(buf);

	if (rmtape < 0 ) return -1;

	if (send(rmtape, buf, len, 0) != len)  {
		rmterror("rmtcall send()");
		return (-1);
	}
	return (rmtreply(cmd));
}

static int
rmtreply(const char *cmd)
{
	char code[30], emsg[BUFSIZ];

	if( rmtgets(code, sizeof (code)) < 0 )  return (-1);
	if (*code == 'E' || *code == 'F') {
		if( rmtgets(emsg, sizeof (emsg)) < 0 )  return (-1);
		msg("%s: %s %s\n", cmd, emsg, code + 1);
		if (*code == 'F') {
			rmtstate = TS_CLOSED;
			return (-1);
		}
		return (-1);
	}
	if (*code != 'A') {
		msg("Protocol to remote tape server botched (code='%s'?).\n",
		    code);
		rmterror("rmtreply protocol");
		return (-1);
	}
	return (atoi(code + 1));
}

static int
rmtgetb(void)
{
	unsigned char c;
	int n;

	if ((n = recv(rmtape, &c, 1, 0)) != 1)  {
		if(!doingvers) { 	/* do not want complaint during this */
			if (n == 0) 
				msg("Couldn't read from remote tape\n");
			else
				perror("recv");
			rmterror("rmtgetb recv");
		}
		return (-1);
	}
	return (c);
}

static int
rmtgets(char *cp, int len)
{
	int	c;

	while (len > 1) {
		if( (c = rmtgetb()) < 0 )  return (-1);
		*cp = c;
		if ( c == '\n') {
			cp[1] = 0;
			return (0);
		}
		cp++;
		len--;
	}
	rmterror("rmtgets: reply unterminated, or too long");
	return (-1);
}
