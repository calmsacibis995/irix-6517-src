/*
 * Copyright (c) 1983 Regents of the University of California.
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

#ifndef lint
static char sccsid[] = "@(#)recvjob.c	5.7 (Berkeley) 6/30/88";
#endif /* not lint */

/*
 * Receive printer jobs from the network, queue them and
 * start the printer daemon.
 */

#include "lp.h"
#ifndef sgi
#include <sys/fs.h>
#else
#include <sys/statfs.h>
#include <sys/fsid.h>
#include <sys/fstyp.h>
#include <sys/sysmacros.h>
#endif

char	*sp = "";
#define ack()	(void) write(1, sp, 1);

char    tfname[40];		/* tmp copy of cf before linking */
char    dfname[40];		/* data files */
int	minfree;		/* keep at least minfree blocks available */
char	*ddev;			/* disk device (for checking free space) */
int	dfd;			/* file system device descriptor */


char	*find_dev();

recvjob()
{
	struct stat stb;
	char *bp = pbuf;
#ifdef sgi
	int status;
	void rcleanup();
#else
	int status, rcleanup();
#endif

	/*
	 * Perform lookup for printer name or abbreviation
	 */
	if ((status = pgetent(line, printer)) < 0)
		frecverr("cannot open printer description file");
	else if (status == 0)
		frecverr("unknown printer %s", printer);
	if ((LF = pgetstr("lf", &bp)) == NULL)
		LF = DEFLOGF;
	if ((SD = pgetstr("sd", &bp)) == NULL)
		SD = DEFSPOOL;
	if ((LO = pgetstr("lo", &bp)) == NULL)
		LO = DEFLOCK;

	(void) close(2);			/* set up log file */
	if (open(LF, O_WRONLY|O_APPEND, 0664) < 0) {
		syslog(LOG_ERR, "%s: %m", LF);
		(void) open("/dev/null", O_WRONLY);
	}

	if (chdir(SD) < 0)
		frecverr("%s: %s: %m", printer, SD);
	if (stat(LO, &stb) == 0) {
		if (stb.st_mode & 010) {
			/* queue is disabled */
			putchar('\1');		/* return error code */
			exit(1);
		}
	} else if (stat(SD, &stb) < 0)
		frecverr("%s: %s: %m", printer, SD);
	minfree = read_number("minfree");
#ifndef sgi
	ddev = find_dev(stb.st_dev, S_IFBLK);
	if ((dfd = open(ddev, O_RDONLY)) < 0)
		syslog(LOG_WARNING, "%s: %s: %m", printer, ddev);
#endif
	signal(SIGTERM, rcleanup);
	signal(SIGPIPE, rcleanup);

	if (readjob())
		printjob();
}

/* Note: sgi statfs works on any file contained in the system, we can use
 * SD directly so the following rigmarole is unnecessary! 
 */

#ifndef sgi

char *
find_dev(dev, type)
	register dev_t dev;
	register int type;
{
	register DIR *dfd = opendir("/dev");
	struct direct *dir;
	struct stat stb;
	char devname[MAXNAMLEN+6];
	char *dp;

	strcpy(devname, "/dev/");
	while ((dir = readdir(dfd))) {
		strcpy(devname + 5, dir->d_name);
		if (stat(devname, &stb))
			continue;
		if ((stb.st_mode & S_IFMT) != type)
			continue;
		if (dev == stb.st_rdev) {
			closedir(dfd);
			dp = (char *)malloc(strlen(devname)+1);
			strcpy(dp, devname);
			return(dp);
		}
	}
	closedir(dfd);
	frecverr("cannot find device %d, %d", major(dev), minor(dev));
	/*NOTREACHED*/
}
#endif /* !sgi */

/*
 * Read printer jobs sent by lpd and copy them to the spooling directory.
 * Return the number of jobs successfully transfered.
 */
readjob()
{
	register int size, nfiles;
	register char *cp;
#ifdef sgi
	void rcleanup();
#endif

	ack();
	nfiles = 0;
	for (;;) {
		/*
		 * Read a command to tell us what to do
		 */
		cp = line;
		do {
			if ((size = read(1, cp, 1)) != 1) {
				if (size < 0)
					frecverr("%s: Lost connection",printer);
				return(nfiles);
			}
		} while (*cp++ != '\n');
		*--cp = '\0';
		cp = line;
		switch (*cp++) {
		case '\1':	/* cleanup because data sent was bad */
			rcleanup();
			continue;

		case '\2':	/* read cf file */
			size = 0;
			while (*cp >= '0' && *cp <= '9')
				size = size * 10 + (*cp++ - '0');
			if (*cp++ != ' ')
				break;
			/*
			 * host name has been authenticated, we use our
			 * view of the host name since we may be passed
			 * something different than what gethostbyaddr()
			 * returns
			 */
			strcpy(cp + 6, from);
			strcpy(tfname, cp);
			tfname[0] = 't';
			if (!chksize(size)) {
				(void) write(1, "\2", 1);
				continue;
			}
			if (!readfile(tfname, size)) {
				rcleanup();
				continue;
			}
			if (link(tfname, cp) < 0)
				frecverr("%s: %m", tfname);
			(void) unlink(tfname);
			tfname[0] = '\0';
			nfiles++;
			continue;

		case '\3':	/* read df file */
			size = 0;
			while (*cp >= '0' && *cp <= '9')
				size = size * 10 + (*cp++ - '0');
			if (*cp++ != ' ')
				break;
			if (!chksize(size)) {
				(void) write(1, "\2", 1);
				continue;
			}
#ifdef sgi
/* 
 * Patch for security hole: a nonprivileged program can tell lpd to write to
 * any file because lpd runs as root.  You tell lpd to create a new spool 
 * file with any name you choose (say, /etc/passwd), and then you give it 
 * the bytes that go into that file...
 *
 * Also, to avoid spoofing the control files, it is illegal to create
 * a datafile whose name qualifies it as a either a control or temporary
 * control file.
 *
 * Fix: don't allow spool filenames that aren't in the current directory.
 *      Originally, this patch called frecverr(), which in turn erased
 *	the data file.  This behavior has been modified.
 */
			(void) strcpy(dfname, cp);
			if (index(dfname, '/')) {
				syslog(LOG_ERR, "illegal path name");
				putchar('\1');
				exit(1);
			}

			if ((dfname[0] == 'c' || dfname[0] == 't') && 
			    dfname[1] == 'f') {
				syslog(LOG_ERR, "illegal data file name");
				putchar('\1');
				exit(1);
			}
				
			(void) readfile(dfname, size);
#else
			strcpy(dfname, cp);
			(void) readfile(dfname, size);
#endif
			continue;
		}
		frecverr("protocol screwup");
	}
}

/*
 * Read files send by lpd and copy them to the spooling directory.
 */
readfile(file, size)
	char *file;
	int size;
{
	register char *cp;
	char buf[BUFSIZ];
	register int i, j, amt;
	int fd, err;

#ifdef sgi
	/* Security patch: To avoid serious problems with remote printing
	 *  requests, we force the system to fail if file already exists 
	 */
	fd = open(file, O_WRONLY|O_CREAT|O_EXCL, FILMOD);
#else
	fd = open(file, O_WRONLY|O_CREAT, FILMOD);
#endif

	if (fd < 0)
		frecverr("%s: %m", file);
	ack();
	err = 0;
	for (i = 0; i < size; i += BUFSIZ) {
		amt = BUFSIZ;
		cp = buf;
		if (i + amt > size)
			amt = size - i;
		do {
			j = read(1, cp, amt);
			if (j <= 0)
				frecverr("Lost connection");
			amt -= j;
			cp += j;
		} while (amt > 0);
		amt = BUFSIZ;
		if (i + amt > size)
			amt = size - i;
		if (write(fd, buf, amt) != amt) {
			err++;
			break;
		}
	}
	(void) close(fd);
	if (err)
		frecverr("%s: write error", file);
	if (noresponse()) {		/* file sent had bad data in it */
		(void) unlink(file);
		return(0);
	}
	ack();
	return(1);
}

noresponse()
{
	char resp;

	if (read(1, &resp, 1) != 1)
		frecverr("Lost connection");
	if (resp == '\0')
		return(0);
	return(1);
}

/*
 * Check to see if there is enough space on the disk for size bytes.
 * 1 == OK, 0 == Not OK.
 * sgi version is different & simpler by use of statfs!
 * We copy the somewhat dubious practice in the BSD version of returning "OK" 
 * if anything fails!!!
 */

chksize(size)
	int size;
{
#ifdef sgi
	int spacefree;
	struct statfs s;

	if (statfs(SD, &s, sizeof(s), 0) < 0) return (1);
	spacefree = s.f_bfree / 2;
#else
	struct stat stb;
	register char *ddev;
	int spacefree;
	struct fs fs;

	if (dfd < 0 || lseek(dfd, (long)(SBOFF), 0) < 0)
		return(1);
	if (read(dfd, (char *)&fs, sizeof fs) != sizeof fs)
		return(1);
	spacefree = freespace(&fs, fs.fs_minfree) * fs.fs_fsize / 1024;
#endif
	size = (size + 1023) / 1024;
	if (minfree + size > spacefree)
		return(0);
	return(1);
}

read_number(fn)
	char *fn;
{
	char lin[80];
	register FILE *fp;

	if ((fp = fopen(fn, "r")) == NULL)
		return (0);
	if (fgets(lin, 80, fp) == NULL) {
		fclose(fp);
		return (0);
	}
	fclose(fp);
	return (atoi(lin));
}

/*
 * Remove all the files associated with the current job being transfered.
 */
#ifdef sgi
void
#endif
rcleanup()
{

	if (tfname[0])
		(void) unlink(tfname);
	if (dfname[0])
		do {
			do
				(void) unlink(dfname);
			while (dfname[2]-- != 'A');
			dfname[2] = 'z';
		} while (dfname[0]-- != 'd');
	dfname[0] = '\0';
}

frecverr(msg, a1, a2)
	char *msg;
{
	rcleanup();
	syslog(LOG_ERR, msg, a1, a2);
	putchar('\1');		/* return error code */
	exit(1);
}
