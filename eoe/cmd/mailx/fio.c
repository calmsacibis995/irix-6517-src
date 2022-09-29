/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char *sccsid = "@(#)fio.c	5.3 (Berkeley) 9/5/85";
#endif /* !lint */

#include "glob.h"
#include <sys/stat.h>
#include <errno.h>

/*
 * Mail -- a mail program
 *
 * File I/O.
 */

/*
 * Set up the input pointers while copying the mail file into
 * temp file.
 */

setptr(ibuf)
	FILE *ibuf;
{
	register int c;
	register char *cp, *cp2;
	register int len, lines, dlines;
	u_long size;
	off_t offset;
	char linebuf[LINESIZE];
	char wbuf[LINESIZE];
	int maybe, mestmp, flag, inhead, isfrom;
	struct message this;
	extern char tempSet[];
	extern char tempMesg[];

	if ((mestmp = opentemp(tempSet)) < 0)
		safe_exit(1);
	msgCount = 0;
	offset = 0;
	size = 0L;
	lines = 0;
	dlines = 0;
	isfrom = 0;
	maybe = 1;
	flag = MUSED|MNEW;
	for (;;) {
		if (fgets(linebuf, LINESIZE, ibuf) == NULL) {
			this.m_flag = flag;
			flag = MUSED|MNEW;
			this.m_offset = local_offsetof(offset);
			this.m_block = local_blockof(offset);
			this.m_size = size;
			this.m_lines = lines;
			this.m_dlines = dlines;
			if (append(&this, mestmp)) {
				perror(tempSet);
				safe_exit(1);
			}
			fclose(ibuf);
			makemessage(mestmp);
			close(mestmp);
			return;
		}
		len = strlen(linebuf);
		fputs(linebuf, otf);
		cp = linebuf + (len - 1);
		if (*cp == '\n')
			*cp = 0;
		if (ferror(otf)) {
			perror(tempMesg);
			safe_exit(1);
		}
		if (maybe && linebuf[0] == 'F' && (isfrom = ishead(linebuf))) {
			msgCount++;
			this.m_flag = flag;
			flag = MUSED|MNEW;
			inhead = 1;
			this.m_block = local_blockof(offset);
			this.m_offset = local_offsetof(offset);
			this.m_size = size;
			this.m_lines = lines;
			this.m_dlines = dlines;
			size = 0L;
			lines = 0;
			dlines = 0;
			if (append(&this, mestmp)) {
				perror(tempSet);
				safe_exit(1);
			}
		}
		if (linebuf[0] == 0)
			inhead = 0;
		if (inhead && (isfrom || isign(linebuf)))
			dlines--;
		isfrom = 0;
		if (inhead && (cp = index(linebuf, ':'))) {
			*cp = 0;
			if (icequal(linebuf, "status")) {
				++cp;
				if (index(cp, 'R'))
					flag |= MREAD;
				if (index(cp, 'O'))
					flag &= ~MNEW;
				/* XXXrs nasty assumption here! */
				inhead = 0;
			}
		}
		offset += len;
		size += (long) len;
		lines++;
		dlines++;
		maybe = 0;
		if (linebuf[0] == 0)
			maybe = 1;
	}
}

/*
 * Drop the passed line onto the passed output buffer.
 * If a write error occurs, return -1, else the count of
 * characters written, including the newline.
 */

putline(obuf, linebuf)
	FILE *obuf;
	char *linebuf;
{
	register int c;

	c = strlen(linebuf);
	fputs(linebuf, obuf);
	putc('\n', obuf);
	if (ferror(obuf))
		return(-1);
	return(c+1);
}

/*
 * Read up a line from the specified input into the line
 * buffer.  Return the number of characters read.  Do not
 * include the newline at the end.
 */

readline(ibuf, linebuf)
	FILE *ibuf;
	char *linebuf;
{
	register int n;

	clearerr(ibuf);
	if (fgets(linebuf, LINESIZE, ibuf) == NULL)
		return(0);
	n = strlen(linebuf);
	if (n >= 1 && linebuf[n-1] == '\n')
		linebuf[n-1] = '\0';
	return(n);
}

/*
 * Return a file buffer all ready to read up the
 * passed message pointer.
 */

FILE *
setinput(mp)
	register struct message *mp;
{
	off_t off;
	extern char tempMesg[];

	fflush(otf);
	if (ferror(otf))
		perror(tempMesg);
	off = mp->m_block;
	off <<= 12;
	off += mp->m_offset;
	if (fseek(itf, off, 0) < 0) {
		perror("fseek");
		panic("temporary file seek");
	}
	return(itf);
}

/*
 * Take the data out of the passed ghost file and toss it into
 * a dynamically allocated message structure.
 */

makemessage(f)
{
	register struct message *m;
	register char *mp;
	register count;

	mp = calloc((unsigned) (msgCount + 1), sizeof *m);
	if (mp == NOSTR) {
		printf("Insufficient memory for %d messages\n", msgCount);
		safe_exit(1);
	}
	if (message != (struct message *) 0)
		cfree((char *) message);
	message = (struct message *) mp;
	dot = message;
	lseek(f, 0L, 0);
	while (count = read(f, mp, BUFSIZ))
		mp += count;
	for (m = &message[0]; m < &message[msgCount]; m++) {
		m->m_size = (m+1)->m_size;
		m->m_lines = (m+1)->m_lines;
		m->m_dlines = (m+1)->m_dlines;
		m->m_flag = (m+1)->m_flag;
	}
	message[msgCount].m_size = 0L;
	message[msgCount].m_lines = 0;
	message[msgCount].m_dlines = 0;
}

/*
 * Append the passed message descriptor onto the temp file.
 * If the write fails, return 1, else 0
 */

append(mp, f)
	struct message *mp;
{
	if (write(f, (char *) mp, sizeof *mp) != sizeof *mp)
		return(1);
	return(0);
}

/*
 * Delete a file, but only if the file is a plain file.
 * const char * because stdio.h now declares the posix remove()
 * but we want to check for a directory, so don't use that one...
 */

m_remove(const char *name)
{
	struct stat statb;
	extern int errno;

	if (stat(name, &statb) < 0) {
		if (debug)
			printf("m_remove: cannot stat %s (%s)\n",
			       name, strerror(errno));
		return(-1);
	}
	if ((statb.st_mode & S_IFMT) != S_IFREG) {
		errno = EISDIR;
		if (debug)
			printf("m_remove: cannot delete %s (%s)\n",
			       name, strerror(errno));
		return(-1);
	}
	return(unlink(name));
}

/*
 * Terminate an editing session by attempting to write out the user's
 * file from the temporary.  Save any new stuff appended to the file.
 */
edstop()
{
	int lcnt;
	long ccnt;
	register int gotcha, c;
	register struct message *mp;
	FILE *obuf, *ibuf, *readstat;
	struct stat statb;
	char tempname[30], *id;
	extern char tempRoot[];
	void (*sigs[3])();

	if (readonly)
		return;
	holdsigs();
	if (Tflag != NOSTR) {
		if ((readstat = fopen(Tflag, "w")) == NULL)
			Tflag = NOSTR;
	}
	for (mp = &message[0], gotcha = 0; mp < &message[msgCount]; mp++) {
		if (mp->m_flag & MNEW) {
			mp->m_flag &= ~MNEW;
			mp->m_flag |= MSTATUS;
		}
		if (mp->m_flag & (MODIFY|MDELETED|MSTATUS))
			gotcha++;
		if (Tflag != NOSTR && (mp->m_flag & (MREAD|MDELETED)) != 0) {
			if ((id = hfield("article-id", mp)) != NOSTR)
				fprintf(readstat, "%s\n", id);
		}
	}
	if (Tflag != NOSTR)
		fclose(readstat);
	if (!gotcha || Tflag != NOSTR)
		goto done;
	ibuf = NULL;
	if (stat(editfile, &statb) >= 0 && statb.st_size > mailsize) {
		strncpy(tempname, tempRoot, 30);
		tempname[29] = '\0';
		if((strlen(tempname) + 11) <= 29)
			strcat(tempname, "/mboxXXXXXX");
		mktemp(tempname);
		if ((obuf = fopen(tempname, "w")) == NULL) {
			perror(tempname);
			relsesigs();
			reset(0);
		}
		if ((ibuf = fopen(editfile, "r")) == NULL) {
			perror(editfile);
			fclose(obuf);
			m_remove(tempname);
			relsesigs();
			reset(0);
		}
		fseek(ibuf, mailsize, 0);
		while ((c = getc(ibuf)) != EOF)
			putc(c, obuf);
		fclose(ibuf);
		fclose(obuf);
		if ((ibuf = fopen(tempname, "r")) == NULL) {
			perror(tempname);
			m_remove(tempname);
			relsesigs();
			reset(0);
		}
		m_remove(tempname);
	}
	printf("\"%s\" ", editfile);
	fflush(stdout);
	if ((obuf = fopen(editfile, "r+")) == NULL) {
		perror(editfile);
		relsesigs();
		reset(0);
	}
	fflush(obuf);
	trunc(obuf);
	c = 0;
	for (mp = &message[0]; mp < &message[msgCount]; mp++) {
		if ((mp->m_flag & MDELETED) != 0)
			continue;
		c++;
		if (send_msg(mp, obuf, NO_SUPPRESS, &lcnt, &ccnt, 0) < 0) {
			perror(editfile);
			relsesigs();
			reset(0);
		}
	}
	gotcha = (c == 0 && ibuf == NULL);
	if (ibuf != NULL) {
		while ((c = getc(ibuf)) != EOF)
			putc(c, obuf);
		fclose(ibuf);
	}
	fflush(obuf);
	if (ferror(obuf)) {
		perror(editfile);
		relsesigs();
		reset(0);
	}
	fclose(obuf);
	if (gotcha && value("keep") == NOSTR) {
		m_remove(editfile);
		printf("removed\n");
	}
	else
		printf("complete\n");
	fflush(stdout);

done:
	relsesigs();
}

static int sigdepth = 0;		/* depth of holdsigs() */
static int omask = 0;
#ifndef	VMUNIX
#if defined(SVR3) || defined(_SVR4_SOURCE)
static void (*washup)();		/* what was SIGHUP  setting */
static void (*wasint)();		/* what was SIGINT  setting */
static void (*wasquit)();		/* what was SIGQUIT setting */
#else
static int (*washup)();			/* what was SIGHUP  setting */
static int (*wasint)();			/* what was SIGINT  setting */
static int (*wasquit)();		/* what was SIGQUIT setting */
#endif
#endif /* !VMUNIX */

/*
 * Hold signals SIGHUP - SIGQUIT.
 */
holdsigs()
{
	register int i;

	if (sigdepth++ == 0) {
#ifdef	VMUNIX
		omask = sigblock(sigmask(SIGHUP)|sigmask(SIGINT)|sigmask(SIGQUIT));
#else
		washup = signal(SIGHUP, SIG_IGN);
		wasint = signal(SIGINT, SIG_IGN);
		wasquit= signal(SIGQUIT,SIG_IGN);
#endif /* VMUNIX */
	}
}

/*
 * Release signals SIGHUP - SIGQUIT
 */
relsesigs()
{
	register int i;

	if (--sigdepth == 0) {
#ifdef	VMUNIX
		sigsetmask(omask);
#else
		signal(SIGHUP,washup);
		signal(SIGINT,wasint);
		signal(SIGQUIT,wasquit);
#endif
	}
}

/*
 * Open a temp file by creating, closing, unlinking, and
 * reopening.  Return the open file descriptor.
 */

opentemp(file)
	char file[];
{
	register int f;

	if ((f = creat(file, 0600)) < 0) {
		perror(file);
		return(-1);
	}
	close(f);
	if ((f = open(file, 2)) < 0) {
		perror(file);
		m_remove(file);
		return(-1);
	}
	m_remove(file);
	return(f);
}

/*
 * Determine the size of the file possessed by
 * the passed buffer.
 */

off_t
fsize(iob)
	FILE *iob;
{
	register int f;
	struct stat sbuf;

	f = fileno(iob);
	if (fstat(f, &sbuf) < 0)
		return(0);
	return(sbuf.st_size);
}

/*
 * Take a file name, possibly with shell meta characters
 * in it and expand it by using "sh -c echo filename"
 * Return the file name as a dynamic string.
 */

char *
expand(name)
	char name[];
{
	char xname[BUFSIZ];
	char cmdbuf[BUFSIZ];
	register int pid, l, rc;
	register char *cp, *Shell;
	int s, pivec[2];
	void (*sigint)();
	struct stat sbuf;

	if ((name[0] == '+') && (value("folder") != NOSTR)) {
		cp = expand(name + 1);
		if (*cp != '/' && getfold(cmdbuf, BUFSIZ) >= 0) {
			snprintf(xname, (ssize_t)BUFSIZ,
				"%s/%s", cmdbuf, cp);
			cp = savestr(xname);
		}
		return(cp);
	}
	if (!anyof(name, "~{[*?$`'\"\\ \t"))
		return(name);
	if (pipe(pivec) < 0) {
		perror("pipe");
		return(name);
	}
	if (strlen(name) > PATH_MAX) {
		errno = ENAMETOOLONG;
		perror(name);
		goto err;
	}
	if (anyof(name, "\\"))
		snprintf(cmdbuf, BUFSIZ, "echo '%s'", name);
	else
		snprintf(cmdbuf, BUFSIZ, "echo %s", name);
	if ((pid = vfork()) == 0) {
		sigchild();
		Shell = value("SHELL");
		if (Shell == NOSTR)
			if (ismailx)
				Shell = SHELL_XPG4;
			else
				Shell = SHELL;
		close(pivec[0]);
		close(1);
		dup(pivec[1]);
		close(pivec[1]);
		close(2);
		dup(1);
		execl(Shell, Shell, "-c", cmdbuf, 0);
		_exit(1);
	}
	if (pid == -1) {
		perror("fork");
		close(pivec[0]);
		close(pivec[1]);
		return(NOSTR);
	}
	close(pivec[1]);
	l = read(pivec[0], xname, BUFSIZ);
	close(pivec[0]);
	while (wait(&s) != pid);
		;
	s &= 0377;
	if (s != 0 && s != SIGPIPE) {
		fprintf(stderr, "\"Echo\" failed\n");
		goto err;
	}
	if (l < 0) {
		perror("read");
		goto err;
	}
	if (l == 0) {
		fprintf(stderr, "\"%s\": No match\n", name);
		goto err;
	}
	if (l == BUFSIZ) {
		fprintf(stderr, "Buffer overflow expanding \"%s\"\n", name);
		goto err;
	}
	xname[l] = 0;
	for (cp = &xname[l-1]; *cp == '\n' && cp > xname; cp--)
		;
	*++cp = '\0';
	if (!ismailx) {
		if (any(' ', xname) && stat(xname, &sbuf) < 0) {
			fprintf(stderr, "\"%s\": Ambiguous\n", name);
			goto err;
		}
	}
	return(savestr(xname));

err:
	return(NOSTR);
}

/*
 * XXX - This code will need to be changed to handle the general case
 *       of any output generated by the !command.  The below code will just
 *       pass back the first BUFSIZ from the 'stdout' pipe generated by the
 *       forked process of "!command".
 *
 * Execute the "!command" by using "sh -c command"
 * Return a read of BUFSIZ from the 'stdout' pipe generated by the results
 * of an execution of the passed "!command".
 */

char *
expandbang(name)
	char name[];
{
	char xname[BUFSIZ];
	char cmdbuf[BUFSIZ];
	register int pid, l, rc;
	register char *cp, *Shell;
	int s, pivec[2];
	void (*sigint)();
	struct stat sbuf;

	if ((*name == '!') && (*(name + 1) == '\0'))
		goto err;
	if (pipe(pivec) < 0) {
		perror("pipe");
		return(name);
	}
	snprintf(cmdbuf, BUFSIZ, "%s", (name + 1));
	if ((pid = vfork()) == 0) {
		sigchild();
		Shell = value("SHELL");
		if (Shell == NOSTR)
			if (ismailx)
				Shell = SHELL_XPG4;
			else
				Shell = SHELL;
		close(pivec[0]);
		close(1);
		dup(pivec[1]);
		close(pivec[1]);
		close(2);
		dup(1);
		execl(Shell, Shell, "-c", cmdbuf, 0);
		_exit(1);
	}
	if (pid == -1) {
		perror("fork");
		close(pivec[0]);
		close(pivec[1]);
		return(NOSTR);
	}
	close(pivec[1]);
	l = read(pivec[0], xname, BUFSIZ);
	close(pivec[0]);
	while (wait(&s) != pid);
		;
	s &= 0377;
	if (s != 0 && s != SIGPIPE) {
		fprintf(stderr, "\"%s\" failed\n", name);
		goto err;
	}
	if (l < 0) {
		perror("read");
		goto err;
	}
	if (l == 0) {
		fprintf(stderr, "\"%s\": No match\n", name);
		goto err;
	}
	if (l == BUFSIZ) {
		fprintf(stderr, "Buffer overflow expanding \"%s\"\n", name);
		goto err;
	}
	xname[l] = 0;
	for (cp = &xname[l-1]; *cp == '\n' && cp > xname; cp--)
		;
	*++cp = '\0';
	return(savestr(xname));

err:
	return(NOSTR);
}

/*
 * Determine the current folder directory name.
 * Note: arg 'size' MUST be max size of arg 'name'.
 */
getfold(name, size)
	char *name;
	int size;
{
	char *folder;

	if ((folder = value("folder")) == NOSTR)
		return(-1);
	if ((folder = expand(folder)) == NOSTR)
		return(-1);
	if (*folder == '/') {
		strncpy(name, folder, size);
		name[size - 1]='\0';
	} else
		snprintf(name, size, "%s/%s", homedir, folder);
	return(0);
}

/*
 * A nicer version of Fdopen, which allows us to fclose
 * without losing the open file.
 */

FILE *
Fdopen(fildes, mode)
	char *mode;
{
	register int f;

	f = dup(fildes);
	if (f < 0) {
		perror("dup");
		return(NULL);
	}
	return(fdopen(f, mode));
}
