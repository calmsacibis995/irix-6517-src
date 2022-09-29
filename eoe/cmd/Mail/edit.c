/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char *sccsid = "@(#)edit.c	5.2 (Berkeley) 6/21/85";
#endif /* !lint */

#include "glob.h"
#include <stdio.h>
#include <sys/stat.h>
#include <limits.h>

/*
 * Mail -- a mail program
 *
 * Perform message editing functions.
 */

/*
 * Edit a message list.
 */

editor(msgvec)
	int *msgvec;
{
	char *edname;

	if ((edname = value("EDITOR")) == NOSTR)
		edname = EDITOR;
	return(edit1(msgvec, edname));
}

/*
 * Invoke the visual editor on a message list.
 */

visual(msgvec)
	int *msgvec;
{
	char *edname;

	if ((edname = value("VISUAL")) == NOSTR)
		edname = VISUAL;
	return(edit1(msgvec, edname));
}

/*
 * Edit a message by writing the message into a funnily-named file
 * (which should not exist) and forking an editor on it.
 * We get the editor from the stuff above.
 */

edit1(msgvec, ed)
	int *msgvec;
	char *ed;
{
	char buf[BUFSIZ];
	char *savedenv;
	register int c, inhead;
	int *ip, pid, mesg, lcnt;
	u_int lines, dlines;
	int edFd;
	u_long ms;
#if defined(SVR3) || defined(_SVR4_SOURCE)
	void (*sigint)(), (*sigquit)();
#else
	int (*sigint)(), (*sigquit)();
#endif
	FILE *ibuf, *obuf;
	char edFName[PATH_MAX];
	struct message *mp;
	extern char tempRoot[];
	extern char tempMesg[];
	off_t fsize(), size;
	struct stat statb;
	long modtime, ccnt;

	/*
	 * Set signals; locate editor.
	 */

	sigint = sigset(SIGINT, SIG_IGN);
	sigquit = sigset(SIGQUIT, SIG_IGN);

	/*
	 * Deal with each message to be edited . . .
	 */

	for (ip = msgvec; *ip && ip-msgvec < msgCount; ip++) {
		mesg = *ip;
		mp = &message[mesg-1];
		mp->m_flag |= MODIFY;

		/*
		 * Make up a name for the edit file of the
		 * form "<TMPDIR>/Message%d_XXXXXX".
		 */

		snprintf(edFName, PATH_MAX,
			"%s/Message%d_XXXXXX", tempRoot, mesg);
		if ((edFd = mkstemp(edFName)) == -1) {
			perror(edFName);
			goto out;
		}

		/*
		 * Copy the message into the edit file.
		 */

		if ((obuf = fdopen(edFd, "w")) == NULL) {
			perror(edFName);
			goto out;
		}
		ibuf = setinput(mp);
		if (fgets(buf, BUFSIZ, ibuf) == NULL)
			savedenv = NOSTR;
		else
			savedenv = savestr(buf);
		
		if (send_msg(mp, obuf, NO_SUPPRESS, &lcnt, &ccnt) < 0) {
			perror(edFName);
			fclose(obuf);
			m_remove(edFName);
			goto out;
		}
		fflush(obuf);
		if (ferror(obuf)) {
			m_remove(edFName);
			fclose(obuf);
			goto out;
		}
		fclose(obuf);

		/*
		 * If we are in read only mode, make the
		 * temporary message file readonly as well.
		 */

		if (readonly)
			chmod(edFName, 0400);

		/*
		 * Fork/execl the editor on the edit file.
		 */

		if (stat(edFName, &statb) < 0)
			modtime = 0;
		modtime = statb.st_mtime;
		pid = vfork();
		if (pid == -1) {
			perror("fork");
			m_remove(edFName);
			goto out;
		}
		if (pid == 0) {
			sigchild();
			if (sigint != SIG_IGN)
				sigsys(SIGINT, SIG_DFL);
			if (sigquit != SIG_IGN)
				sigsys(SIGQUIT, SIG_DFL);
			execl(ed, ed, edFName, 0);
			perror(ed);
			_exit(1);
		}
		while (wait(&mesg) != pid)
			;

		/*
		 * If in read only mode, just remove the editor
		 * temporary and return.
		 */

		if (readonly) {
			m_remove(edFName);
			continue;
		}

		/*
		 * Now copy the message to the end of the
		 * temp file.
		 */

		if (stat(edFName, &statb) < 0) {
			perror(edFName);
			goto out;
		}
		if (modtime == statb.st_mtime) {
			m_remove(edFName);
			goto out;
		}
		if ((ibuf = fopen(edFName, "r")) == NULL) {
			perror(edFName);
			m_remove(edFName);
			goto out;
		}
		m_remove(edFName);
		fseek(otf, (long) 0, 2);
		size = fsize(otf);
		mp->m_block = local_blockof(size);
		mp->m_offset = local_offsetof(size);
		if (fgets(buf, BUFSIZ, ibuf) == NULL) {
			perror(edFName);
			goto out;
		}
		ms = 0L;
		lines = 0;
		dlines = 0;
		while (buf[strlen(buf)-1] == '\n')
			buf[strlen(buf)-1] = '\0';
		if (!ishead(buf)) {
			strncpy(buf, savedenv, BUFSIZ);
			buf[BUFSIZ - 1] = '\0';
			rewind(ibuf);
		}
		else
			strcat(buf, "\n");
		if (strlen(buf)) {
			fprintf(otf, "%s", buf);
			ms = strlen(buf);
			lines = 1;
		}
		inhead = 1;
		while (fgets(buf, BUFSIZ, ibuf) != NULL) {
			if (strcmp(buf, "\n") == 0)
				inhead = 0;
			if (inhead && isign(buf))
				dlines--;
			fprintf(otf, "%s", buf);
			if (ferror(otf))
				break;
			lines++;
			dlines++;
			ms += strlen(buf);
		}
		mp->m_size = ms;
		mp->m_lines = lines;
		mp->m_dlines = dlines;
		if (ferror(otf))
			perror(tempMesg);
		fclose(ibuf);
	}

	/*
	 * Restore signals and return.
	 */

out:
	sigset(SIGINT, sigint);
	sigset(SIGQUIT, sigquit);
}
