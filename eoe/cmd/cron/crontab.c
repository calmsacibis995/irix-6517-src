/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.14 $"
/**************************************************************************
 ***			C r o n t a b . c				***
 **************************************************************************

	date:	7/2/82
	description:	This program implements crontab (see cron(1)).
			This program should be set-uid to root.
	files:
		/usr/lib/cron		 drwxr-xr-x root sys
		/usr/lib/cron/cron.allow -rw-r--r-- root sys
		/usr/lib/cron/cron.deny	 -rw-r--r-- root sys

 **************************************************************************/


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/mac.h>
#include <stdlib.h>
#include <pwd.h>
#include "cron.h"
#include "funcs.h"
#include "permit.h"

/* file creation defines */

#define EDTMP	"/tmp/crontabXXXXXX"		/* editor tmpfile prefix */
#define CRTMP	"_cron"				/* crontab tmpfile prefix */
#define CRMODE	(S_IRUSR | S_IRGRP | S_IROTH)	/* crontab tmpfile mode */
#define EDMODE	(S_IRUSR | S_IWUSR)		/* editor tmpfile mode */
#define CRFLAGS	(O_RDWR | O_CREAT | O_TRUNC)

/* error message defines */

#define BADCRCREATE	"can't create '%s' in the crontab directory: %s\n"
#define BADEDCREATE	"can't create '%s' in the tmp directory: %s\n"
#define BADCROPEN	"can't open crontab file: %s: %s\n"
#define BADDEL		"can't unlink '%s': %s\n"
#define BADSTAT		"can't stat '%s': %s\n"
#define BADWRITE	"write error on temporary file\n"
#define BADSHELL	"because your login shell isn't /bin/sh, you can't use cron\n"
#define BADUSAGE	"proper usage is one of:\n" \
			"\tcrontab [file]\n" \
			"\tcrontab -l [username]\n" \
			"\tcrontab -r [username]\n" \
			"\tcrontab -e [username]\n" \
			"(Only root can access other people's crontabs)\n"
#define INVALIDUSER	"%s not a valid user (no entry in /etc/passwd)\n"
#define NOTALLOWED	"you are not authorized to use cron. Sorry.\n"
#define BADPRIV		"only root is authorized to access other people's crontabs. Sorry.\n"
#define EOLN		"unexpected end of line"
#define UNEXPECT	"unexpected character found in line"
#define OUTOFBOUND	"number out of bounds"
#define SYNTAX		"the crontab file had syntax errors in it; no change was made\n"
#define NOLINES		"no lines in crontab file - ignoring. use crontab -r to remove a crontab\n"
#define BADREAD		"error reading your crontab file %s: %s\n"
#define SYSFAIL		"%s failed\n"

/* tmpfile deletion defines */

#define U_CRONTMP	0x01	/* do/don't delete cron tmpfile */
#define U_EDTMP		0x02	/* do/don't delete editor tmpfile */

static int  mac_enabled, dounlink;
static char *crontmp, edtmp[sizeof(EDTMP)];

static void cerror(const char *, const char *);
static void crabort(const char *, ...);
static void verify_crontab(FILE *);
static void copyfile(FILE *, FILE *, const char *);
static char *cf_create(const char *, const char *);
static int  sendmsg(char, const char *);
static void handle_signals(void);

int
main(int argc, char **argv)
{
	int c, ct_fd, rflag = 0, lflag = 0, eflag = 0, errflag = 0;
	uid_t ruid;
	char login[UNAMESIZE], *loginp;
	char *cronfile, *cronfile_base = NULL;
	char *editor, edbuf[BUFSIZ];
	FILE *cf_fp, *ct_fp;

	mac_enabled = (sysconf(_SC_MAC) > 0);

	/* parse command-line arguments */
	while((c = getopt(argc, argv, "elr")) != EOF)
	{
		switch(c)
		{
			case 'e':
				argc--;
				if (lflag || rflag)
					errflag++;
				else
					eflag++;
				break;
			case 'l':
				argc--;
				if (eflag || rflag)
					errflag++;
				else
					lflag++;
				break;
			case 'r':
				argc--;
				if (eflag || lflag)
					errflag++;
				else
					rflag++;
				break;
			case '?':
				errflag++;
				break;
		}
	}

	/*
	 * after processing -[l|e|r] we have either:
	 *	argc == 2 (a file name was given)
	 *	argc == 1 (no arguments after command name)
	 * In the argc == 1 case we either read stdin OR
	 * the default user crontab (if -[l|e|r] was given)
	 */
	if (errflag || argc > 2)
		crabort(BADUSAGE);

	/* get user name */
	if ((loginp = getuser(ruid = getuid())) == NULL)
	{
		if (per_errno == 2)
			crabort(BADSHELL);
		else
			crabort(INVALIDUSER, "you are");
	}

	/* check if this user is permitted to use cron */
	strcpy(login, loginp);
	if (!allowed(login, CRONALLOW, CRONDENY))
		crabort(NOTALLOWED);

	/* argument given: username or filename */
	if (argc == 2 && (eflag || lflag || rflag))
	{
		argv++;

		/* check for invalid username */
		if (getpwnam(argv[1]) == NULL)
			crabort(INVALIDUSER, argv[1]);

		/* valid username: are we allowed? */
		if (ruid == 0 || strcmp(argv[1], login) == 0)
			cronfile_base = argv[1];
		else
			crabort(BADPRIV);
	}

	/* target crontab file */
	if (cronfile_base == NULL)
		cronfile_base = login;

	cronfile = cf_create(cronfile_base, "");

	/* 'r'emove crontab file */
	if (rflag)
	{
		if (unlink(cronfile) == -1)
			crabort(BADDEL, cronfile, strerror(errno));
		free(cronfile);
		return(sendmsg(DELETE, cronfile_base) != 0);
	}

	/* 'l'ist crontab file */
	if (lflag)
	{
		if ((cf_fp = fopen(cronfile, "r")) == NULL)
			crabort(BADCROPEN, cronfile, strerror(errno));
		copyfile(stdout, cf_fp, cronfile);
		fclose(cf_fp);
		free(cronfile);
		return(0);
	}

	/* signals need to be dealt with here */
	handle_signals();

	/* prevent bad things from happening because of bogus umask */
	(void) umask((mode_t) 0);

	/* create cron temporary file */
	sprintf(edbuf, "%-5d", (int) getpid());
	crontmp = cf_create(CRTMP, edbuf);
	if ((ct_fd = open(crontmp, CRFLAGS, CRMODE)) == -1)
		crabort(BADCRCREATE, crontmp, strerror(errno));
	dounlink |= U_CRONTMP;
	if (set_cloexec(ct_fd) == -1 || (ct_fp = fdopen(ct_fd, "w+")) == NULL)
		crabort(BADCRCREATE, crontmp, strerror(errno));

	/* 'e'dit crontab file */
	if (eflag)
	{
		pid_t pid;
		int wstat;

		/* open user's crontab */
		if ((cf_fp = fopen(cronfile, "r")) == NULL && errno != ENOENT)
			crabort(BADCROPEN, cronfile, strerror(errno));

		/*
		 * fork a child with the user's uid
		 * to edit the crontab file
		 */
		if ((pid = fork()) == (pid_t) -1)
			crabort(SYSFAIL, "fork");

		/* child process */
		if (pid == 0)
		{
			time_t omodtime;
			struct stat stbuf;
			int et_fd;
			FILE *et_fp;

			/* child can't delete crontmp */
			dounlink &= ~U_CRONTMP;

			/* give up suid */
			if (setuid(ruid) == -1)
				crabort(SYSFAIL, "setuid");

			/* open editor temporary file */
			strcpy(edtmp, EDTMP);
			mktemp(edtmp);
			if ((et_fd = open(edtmp, CRFLAGS, EDMODE)) == -1)
				crabort(BADEDCREATE, edtmp, strerror(errno));
			dounlink |= U_EDTMP;
			if (set_cloexec(et_fd) == -1)
				crabort(BADEDCREATE, edtmp, strerror(errno));
			if ((et_fp = fdopen(et_fd, "w+")) == NULL)
				crabort(BADEDCREATE, edtmp, strerror(errno));

			/* copy user's crontab to editor temporary file */
			if (cf_fp != NULL)
			{
				copyfile(et_fp, cf_fp, cronfile);
				fclose(cf_fp);
				rewind(et_fp);
			}

			/* get modtime of editor temporary file */
			if (fstat(et_fd, &stbuf) == -1)
				crabort(BADSTAT, edtmp, strerror(errno));
			omodtime = stbuf.st_mtime;

			/* determine which editor to use */
			editor = getenv("VISUAL");
			if (editor == NULL)
				editor = getenv("EDITOR");
			if (editor == NULL)
				editor = "vi";

			/* protect against buffer overrun */
			if (strlen(editor) + strlen(edtmp) + 2 > sizeof(edbuf))
				crabort("editor environment too long\n");

			/* create editor command string */
			sprintf(edbuf, "%s %s", editor, edtmp);

			/* invoke the editor */
			if (system(edbuf) == -1)
				crabort("can't execute '%s'\n", editor);
			
			/* sanity checks */
			if (fstat(et_fd, &stbuf) == -1)
				crabort(BADSTAT, edtmp, strerror(errno));
			if (stbuf.st_size == 0)
				crabort("temporary file empty\n", edtmp);
			if (omodtime == stbuf.st_mtime)
				crabort("crontab file not changed\n");

			/* copy editor temporary to cron temporary */
			copyfile(ct_fp, et_fp, edtmp);

			/* shut down */
			fclose(et_fp);
			fclose(ct_fp);
			dounlink &= ~U_EDTMP;

			/* remove temporary file */
			if (unlink(edtmp) == -1)
				crabort(BADDEL, edtmp, strerror(errno));

			exit(0);
		}

		if (cf_fp != NULL)
			fclose(cf_fp);

		/* parent process: wait for child to exit */
		while (waitpid(pid, &wstat, 0) == -1 && errno == EINTR)
			/* empty loop */;

		/* if our child didn't exit normally, don't continue */
		if (!WIFEXITED(wstat) || WEXITSTATUS(wstat) != 0)
			crabort((char *) NULL);
	}
	else
	{
		if (argc == 1)
		{
			/* no argument: read crontab from standard input */
			copyfile(ct_fp, stdin, "standard input");
		}
		else
		{
			/*
			 * Copy crontab from a file. We can't use access +
			 * open, because that introduces a security flaw
			 * (an exploitable race between access and open).
			 *
			 * Instead, open the named file with the user's
			 * uid, and copy that file into crontmp.
			 */
			uid_t euid = geteuid();

			/* give up suid */
			if (setreuid((uid_t) -1, ruid) == -1)
				crabort(SYSFAIL, "setreuid");

			/* open the named file */
			cf_fp = fopen(argv[1], "r");

			/* restore suid */
			if (setreuid((uid_t) -1, euid) == -1)
				crabort(SYSFAIL, "setreuid");

			/* check for error on open */
			if (cf_fp == NULL)
				crabort(BADCROPEN, argv[1], strerror(errno));

			/* copy named file to cron temporary file */
			copyfile(ct_fp, cf_fp, argv[1]);
			fclose(cf_fp);
		}
	}

	/* copy to user's crontab */
	verify_crontab(ct_fp);
	fclose(ct_fp);
	if (rename(crontmp, cronfile) == -1)
		crabort(BADCRCREATE, cronfile, strerror(errno));
	dounlink &= ~U_CRONTMP;
	free(cronfile);
	free(crontmp);

	/* notify cron of the new crontab */
	return(sendmsg(ADD, cronfile_base) != 0);
}

static int
sendmsg(char action, const char *fname)
{
	int msgfd;
	struct message msg;

	if ((msgfd = open(FIFO, O_WRONLY | O_NDELAY)) == -1)
	{
		if(errno == ENXIO || errno == ENOENT)
			fprintf(stderr, "cron may not be running - call your system administrator\n");
		else
			fprintf(stderr, "crontab: error in message queue open\n");
		return(1);
	}

	msg.action = action;
	strncpy(msg.fname, fname, sizeof(msg.fname));
	msg.fname[sizeof(msg.fname) - 1] = '\0';

	if (mac_enabled)
	{
		mac_t lbl;
		char *lblstr;
		size_t lblstrlen;

		msg.etype = TRIX_CRON;

		/* get our mac label */
		if ((lbl = mac_get_proc()) == NULL)
		{
			fprintf(stderr, "crontab: error in mac processing\n");
			close(msgfd);
			return(1);
		}

		/* convert mac label to human-readable form */
		lblstr = mac_to_text(lbl, &lblstrlen);
		mac_free(lbl);
		if (lblstr == NULL)
		{
			fprintf(stderr, "crontab: error in mac processing\n");
			close(msgfd);
			return(1);
		}

		/* copy to message buffer */
		strncpy(msg.label, lblstr, sizeof(msg.label));
		msg.label[sizeof(msg.label) - 1] = '\0';
		mac_free(lblstr);
	}
	else
	{
		msg.etype = CRON;
	}

	if (write(msgfd, &msg, sizeof(msg)) != sizeof(msg))
	{
		fprintf(stderr,"crontab: error in message send\n");
		close(msgfd);
		return(1);
	}
	close(msgfd);
	return(0);
}

static void
copyfile(FILE *dst, FILE *src, const char *name)
{
	char line[CTLINESIZE];

	while(fgets(line, sizeof(line), src) != NULL)
		if (fputs(line, dst) == EOF && ferror(dst))
			crabort(BADWRITE);

	if (ferror(src))
		crabort(BADREAD, name, strerror(errno));

	if (fflush(dst) == EOF)
		crabort(BADWRITE);
}

static void
delete_tmpfiles(void)
{
	/* unlink files */
	if (dounlink & U_EDTMP)
		(void) unlink(edtmp);
	if (dounlink & U_CRONTMP)
		(void) unlink(crontmp);
}

/* ARGSUSED */
static void
catch(int sig)
{
	delete_tmpfiles();
	exit(1);
}

static void
handle_signals(void)
{
	int i, sigs[] = {SIGINT, SIGHUP, SIGQUIT, SIGTERM};
	struct sigaction act, oact, *snil = NULL;

	act.sa_flags = 0;
	act.sa_handler = catch;
	if (sigemptyset(&act.sa_mask) == -1)
		crabort(SYSFAIL, "sigemptyset");

	/*
	 * Install signal handler(s), but only if we are not
	 * currently ignoring the signal in question.
	 */
	for (i = 0; i < (sizeof(sigs) / sizeof(sigs[0])); i++)
	{
		if (sigaction(sigs[i], snil, &oact) == -1)
			crabort(SYSFAIL, "sigaction");

		if (oact.sa_handler == SIG_IGN)
			continue;

		if (sigaction(sigs[i], &act, snil) == -1)
			crabort(SYSFAIL, "sigaction");
	}
}

static int
next_field(const char *line, char **cursor, int lower, int upper)
{
	int num, num2;

	/* skip leading whitespace */
	while (isspace(**cursor))
		++*cursor;

	/* check for missing field */
	if (**cursor == '\0')
	{
		cerror(EOLN, line);
		return(1);
	}

	/* check for wildcard */
	if (**cursor == '*')
	{
		/* wildcards form a field by themselves */
		++*cursor;
		if (!isspace(**cursor))
		{
			cerror(UNEXPECT, line);
			return(1);
		}
		return(0);
	}
	for(;;)
	{
		/*
		 * Check the first field of a range. A number by itself is
		 * the degenerate case of a range whose start equals its end.
		 * If it:
		 *	is not a numeric quantity
		 *	is too large e.g. causes overflow
		 *	is an out of bounds value
		 * then it is invalid.
		 */
		if (!isdigit(**cursor))
		{
			cerror(UNEXPECT, line);
			return(1);
		}
		errno = 0;
		num = (int) strtol(*cursor, cursor, 10);
		if (errno == ERANGE || num < lower || num > upper)
		{
			cerror(OUTOFBOUND, line);
			return(1);
		}

		/* We have found a numeric range. Check it */
		if (**cursor == '-')
		{
			/*
			 * Check the second field of a range.
			 * If it:
			 *	is not a numeric quantity
			 *	is too large e.g. causes overflow
			 *	is an out of bounds value
			 *	is smaller than the first field
			 * then it is invalid.
			 */
			++*cursor;
			if (!isdigit(**cursor))
			{
				cerror(UNEXPECT, line);
				return(1);
			}
			errno = 0;
			num2 = (int) strtol(*cursor, cursor, 10);
			if (errno == ERANGE || num2 < lower ||
			    num2 > upper || num2 < num)
			{
				cerror(OUTOFBOUND, line);
				return(1); 
			}
		}

		/* whitespace ends a field */
		if (isspace(**cursor))
			break;

		/* check for premature end of line */
		if (**cursor == '\0')
		{
			cerror(EOLN, line);
			return(1); 
		}

		/*
		 * If we are not at whitespace then this field has another
		 * range following this one. Ranges are separated from one
		 * another by commas.
		 */
		if (*((*cursor)++) != ',')
		{
			cerror(UNEXPECT, line);
			return(1); 
		}
	}
	return(0);
}

static void
verify_crontab(FILE *fp)
{
	char line[CTLINESIZE], *cursor;
	int newlines = 0, error = 0;

	/* parse temp file for errors */
	rewind(fp);
	while (fgets(cursor = line, sizeof(line), fp) != NULL)
	{
		/* skip leading whitespace */
		while (isspace(*cursor))
			++cursor;

		/* if this line is a comment or is empty, goto next line */
		if (*cursor == '#' || *cursor == '\0')
			goto cont;

		/* parse numeric fields */
		if (next_field(line, &cursor, 0, 59) ||
		    next_field(line, &cursor, 0, 23) ||
		    next_field(line, &cursor, 1, 31) ||
		    next_field(line, &cursor, 1, 12) ||
		    next_field(line, &cursor, 0, 06))
		{
			error = 1;
			continue;
		}

		/* check for premature end of line */
		if (*++cursor == '\0')
		{
			cerror(EOLN, line);
			error = 1;
			continue; 
		}
cont:
		newlines++;
	}
	if (ferror(fp))
		error = 1;

	if (!error)
	{
		if (newlines == 0)
			crabort(NOLINES); 
	}
	else
		crabort(SYNTAX);
}

static void
crabort(const char *msg, ...)
{
	/* print message */
	if (msg != NULL)
	{
		va_list ap;

		fprintf(stderr, "crontab: ");
		va_start(ap, msg);
		vfprintf(stderr, msg, ap);
		va_end(ap);
	}

	delete_tmpfiles();
	exit(1);
}

static void
cerror(const char *msg, const char *line)
{
	fprintf(stderr, "%scrontab: error on previous line; %s\n", line, msg);
}

static char *
cf_create(const char *name, const char *suffix)
{
	char *root = (mac_enabled ? MACCRONDIR : CRONDIR);
	char *cp = xmalloc(strlen(root) + strlen(name) + strlen(suffix) + 2);

	sprintf(cp, "%s/%s%s", root, name, suffix);
	return (cp);
}
