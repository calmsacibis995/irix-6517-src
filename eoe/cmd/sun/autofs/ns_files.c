/*
 *	ns_files.c
 *
 *	Copyright (c) 1988-1993 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#ident	"@(#)ns_files.c	1.9	94/01/24 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
/*
#include <sys/types.h>
#include <sys/sema.h>
*/
#include <sys/stat.h>
#include <sys/param.h>
#include <pwd.h>				
#include "autofsd.h"

#define	NETMASKS_FILE	"/etc/netmasks"
#define	MASK_SIZE	1024
#define	STACKSIZ	30

static FILE *file_open(char *, char *, char **, char ***);

extern char *user_to_execute_maps_as;
void
init_files(char **stack, char ***stkptr)
{
	if (stack == NULL && stkptr == NULL)
		return;
	(void) stack_op(INIT, NULL, stack, stkptr);
}

getmapent_files(key, mapname, ml, stack, stkptr)
	char *key;
	char *mapname;
	struct mapline *ml;
	char **stack, ***stkptr;
{
	int nserr;
	FILE *fp = NULL;
	char word[MAXFILENAMELEN+1], wordq[MAXFILENAMELEN+1];
	char linebuf[LINESZ], lineqbuf[LINESZ];
	char *lp, *lq;
	struct stat stbuf;
	char fname[MAXFILENAMELEN]; /* /etc prepended to mapname if reqd */
	int syntaxok = 1;

	if ((fp = file_open(mapname, fname, stack, stkptr)) == NULL) {
		nserr = 1;
		goto done;
	}

	if (stat(fname, &stbuf) < 0) {
		nserr = 1;
		goto done;
	}

	/*
	 * If the file has its execute bit on then
	 * assume it's an executable map.
	 * Execute it and pass the key as an argument.
	 * Expect to get a map entry on the stdout.
	 */
	if (stbuf.st_mode & S_IXUSR && user_to_execute_maps_as) {
		int rc;

		/*
		 * For security, do not use popen().
		 * Instead, explicitly fork+exec with a pipe.
		 * Also, only execute root-owned files not globally writable.
		 */
		if (stbuf.st_uid != 0 || stbuf.st_mode & S_IWOTH) {
			nserr = 1;
			goto done;
		}

		if (trace > 1) {
			(void) fprintf(stderr,
				"  Executable map: map=%s key=%s\n",
				fname, key);
		}

		rc = read_execout(key, &lp, fname, ml->linebuf, LINESZ);

		if (rc != 0) {
			nserr = 1;
			goto done;
		}

		if (lp == NULL || strlen(ml->linebuf) == 0) {
			nserr = 1;
			goto done;
		}

		unquote(ml->linebuf, ml->lineqbuf);
		nserr = 0;
		goto done;
	}


	/*
	 * It's just a normal map file.
	 * Search for the entry with the required key.
	 */
	for (;;) {
		lp = get_line(fp, fname, linebuf, sizeof (linebuf));
		if (lp == NULL) {
			nserr = 1;
			goto done;
		}
		if (verbose && syntaxok && isspace(*(u_char *)lp)) {
			syntaxok = 0;
			syslog(LOG_ERR,
				"leading space in map entry \"%s\" in %s",
				lp, mapname);
		}
		lq = lineqbuf;
		unquote(lp, lq);
		getword(word, wordq, &lp, &lq, ' ', sizeof(word));
		if (strcmp(word, key) == 0)
			break;
		if (word[0] == '*' && word[1] == '\0')
			break;
		if (word[0] == '+') {
			nserr = getmapent(key, word+1, ml, stack, stkptr);
			if (nserr == 0)
				goto done;
			continue;
		}

		/*
		 * sanity check each map entry key against
		 * the lookup key as the map is searched.
		 */
		if (verbose && syntaxok) { /* sanity check entry */
			if (*key == '/') {
				if (*word != '/') {
					syntaxok = 0;
					syslog(LOG_ERR,
					"bad key \"%s\" in direct map %s\n",
					word, mapname);
				}
			} else {
				if (strchr(word, '/')) {
					syntaxok = 0;
					syslog(LOG_ERR,
					"bad key \"%s\" in indirect map %s\n",
					word, mapname);
				}
			}
		}
	}

	(void) strcpy(ml->linebuf, lp);
	(void) strcpy(ml->lineqbuf, lq);
	nserr = 0;
done:
	if (fp) {
		(void) stack_op(POP, (char *) NULL, stack, stkptr);
		(void) fclose(fp);
	}
	return (nserr);
}

getnetmask_files(netname, mask)
	char *netname, **mask;
{
	FILE *f;
	char line[MASK_SIZE];
	int nsresult = 1;
	char *out, *lasts;

	f = fopen(NETMASKS_FILE, "r");
	if (f == NULL)
		return (1);

	while (fgets(line, MASK_SIZE -  1, f)) {
		out = strtok_r(line, " \t\n", &lasts);
		if (strcmp(out, netname) == 0) {
			*mask = strdup(strtok_r(NULL, " \t\n", &lasts));
			nsresult = 0;
			break;
		}
	}

	fclose(f);
	return (nsresult);
}

loadmaster_files(mastermap, defopts, stack, stkptr)
	char *mastermap;
	char *defopts;
	char **stack, ***stkptr;
{
	FILE *fp;
	int done = 0;
	char *line, *dir, *map, *opts;
	char linebuf[1024];
	char lineq[1024];
	char fname[MAXFILENAMELEN]; /* /etc prepended to mapname if reqd */


	if ((fp = file_open(mastermap, fname, stack, stkptr)) == NULL)
		return (1);

	while ((line = get_line(fp, fname, linebuf,
				sizeof (linebuf))) != NULL) {
		unquote(line, lineq);
		if (macro_expand("", line, lineq, sizeof (linebuf)) == -1) {
			pr_msg("Warning: ignore overflowing entry in %s.\n",
				fname);
			continue;
		}
		dir = line;
		while (*dir && isspace(*dir))
			dir++;
		if (*dir == '\0')
			continue;
		map = dir;

		while (*map && !isspace(*map)) map++;
		if (*map)
			*map++ = '\0';

		if (*dir == '+') {
			opts = map;
			while (*opts && isspace(*opts))
				opts++;
			if (*opts != '-')
				opts = defopts;
			else
				opts++;
			/*
			 * Check for no embedded blanks.
			 */
			if (strcspn(opts, " 	") == strlen(opts)) {
				dir++;
				(void) loadmaster_map(dir, opts, stack, stkptr);
			} else {
pr_msg("Warning: invalid entry for %s in %s ignored.\n", dir, fname);
				continue;
			}

		} else {
			while (*map && isspace(*map))
				map++;
			if (*map == '\0')
				continue;
			opts = map;
			while (*opts && !isspace(*opts))
				opts++;
			if (*opts) {
				*opts++ = '\0';
				while (*opts && isspace(*opts))
					opts++;
			}
			if (*opts != '-')
				opts = defopts;
			else
				opts++;
			/*
			 * Check for no embedded blanks.
			 */
			if (strcspn(opts, " 	") == strlen(opts)) {
				dirinit(dir, map, opts, 0, stack, stkptr);
			} else {
pr_msg("Warning: invalid entry for %s in %s ignored.\n", dir, fname);
				continue;
			}
		}
		done++;
	}

	stack_op(POP, (char *) NULL, stack, stkptr);
	(void) fclose(fp);

	return (!done);
}

loaddirect_files(map, local_map, opts, stack, stkptr)
	char *map, *local_map, *opts;
	char **stack, ***stkptr;
{
	FILE *fp;
	int done = 0;
	char *line, *p1, *p2;
	char linebuf[1024];
	char fname[MAXFILENAMELEN]; /* /etc prepended to mapname if reqd */

	if ((fp = file_open(map, fname, stack, stkptr)) == NULL)
		return (1);

	while ((line = get_line(fp, fname, linebuf,
				sizeof (linebuf))) != NULL) {
		p1 = line;
		while (*p1 && isspace(*p1))
			p1++;
		if (*p1 == '\0')
			continue;
		p2 = p1;
		while (*p2 && !isspace(*p2))
			p2++;
		*p2 = '\0';
		if (*p1 == '+') {
			p1++;
			(void) loaddirect_map(p1, local_map, opts, stack, stkptr);
		} else {
			dirinit(p1, local_map, opts, 1, stack, stkptr);
		}
		done++;
	}

	(void) stack_op(POP, (char *) NULL, stack, stkptr);
	(void) fclose(fp);

	return (!done);
}

/*
 * This procedure opens the file and pushes it onto the
 * the stack. Only if a file is opened successfully, is
 * it pushed onto the stack
 */
static FILE *
file_open(map, fname, stack, stkptr)
	char *map, *fname;
	char **stack, ***stkptr;
{
	FILE *fp;

	if (*map != '/') {
		/* prepend an "/etc" */
		(void) strcpy(fname, "/etc/");
		(void) strcat(fname, map);
	} else
		(void) strcpy(fname, map);

	if (!stack_op(PUSH, fname, stack, stkptr))
		return (NULL);

	fp = fopen(fname, "r");
	if (fp == NULL)
		(void) stack_op(POP, (char *) NULL, stack, stkptr);

	return (fp);
}

/*
 * reimplemnted to be MT-HOT.
 */
int
stack_op(op, name, stack, stkptr)
	int op;
	char *name;
	char **stack, ***stkptr;
{
	char **ptr = NULL;
	char **stk_top = &stack[STACKSIZ - 1];

	/*
	 * the stackptr points to the next empty slot
	 * for PUSH: put the element and increment stkptr
	 * for POP: decrement stkptr and free
	 */

	switch (op) {
	case INIT:
		for (ptr = stack; ptr != stk_top; ptr++)
			*ptr = (char *) NULL;
		*stkptr = stack;
		return (1);
	case ERASE:
		for (ptr = stack; ptr != stk_top; ptr++)
			if (*ptr) {
				if (trace > 1)
					fprintf(stderr, "  ERASE %s\n", *ptr);
				free (*ptr);
				*ptr = (char *) NULL;
			}
		*stkptr = stack;
		return (1);
	case PUSH:
		if (*stkptr == stk_top)
			return (0);
		for (ptr = stack; ptr != *stkptr; ptr++)
			if (*ptr && (strcmp(*ptr, name) == 0)) {
				return (0);
			}
		if (trace > 1)
			fprintf(stderr, "  PUSH %s\n", name);
		if ((**stkptr = strdup(name)) == NULL) {
			fprintf(stderr, "  FATAL ERROR IN STACK PUSH\n");
			return (0);
		}
		(*stkptr)++;
		return (1);
	case POP:
		if (*stkptr != stack)
			(*stkptr)--;
		else
			fprintf(stderr, "  FATAL ERROR IN STACK POP\n");

		if (*stkptr && **stkptr) {
			if (trace > 1)
				fprintf(stderr, "  POP %s\n", **stkptr);
			free (**stkptr);
			**stkptr = (char *) NULL;
		}
		return (1);
	default:
		return (0);
	}
}

/*
 * Setup the uid and gid for the exec'ed process.
 */
static int 
set_user(char *user)
{
   struct passwd *pw=NULL;

   if (user == NULL)
	   return -1;

   if (!(pw=getpwnam(user)))     /* get password file entry. */
	   return -1;

   if (setgid(pw->pw_gid))       /* set group-id first. */
	   return -1;

   if (setuid(pw->pw_uid))       /* set user-id last. */
	   return -1;

   return 0;
}

#define	READ_EXECOUT_ARGS 3

/*
 * read_execout(char *key, char **lp, char *fname, char *line, int linesz)
 * A simpler, multithreaded implementation of popen().  Used for security.
 * Returns 0 on OK or -1 on error.
 */
static int
read_execout(char *key, char **lp, char *fname, char *line, int linesz)
{
	int p[2];
	int status = 0;
	int child_pid;
	char *args[READ_EXECOUT_ARGS];
	struct sigaction nact, oact;
	int sigfail;

	if (pipe(p) < 0) {
		fprintf(stderr, "read_execout: Cannot create pipe");
		return (-1);
	}

	/* setup args for execv */
	if (((args[0] = strdup(fname)) == NULL) ||
		((args[1] = strdup(key)) == NULL)) {
		fprintf(stderr, "read_execout: Memory allocation failed");
		if (args[0])
			free(args[0]);
		return (-1);
	}
	args[2] = NULL;

	/* Make sure SNOWAIT is not set so our waitpid() works. */
	nact.sa_flags = 0;
	nact.sa_handler = SIG_DFL;
	sigemptyset(&nact.sa_mask);
	if ((sigfail = sigaction(SIGCLD, &nact, &oact)) == -1)
		syslog(LOG_ERR, "sigaction failed: %m");

	if (trace > 3)
		fprintf(stderr, "  read_execout: forking .....\n");

	switch ((child_pid = fork())) {
	case -1:
		fprintf(stderr, "read_execout: Cannot fork");
		/* free args */
		if (args[0])
			free(args[0]);
		if (args[1])
			free(args[1]);
		if (!sigfail)
			sigaction(SIGCLD, &oact, NULL);
		return (-1);
	case 0: 
		{
			/*
			 * Child
			 */
			close(p[0]);
			close(1);
			if (fcntl(p[1], F_DUPFD, 1) != 1) {
				fprintf(stderr,
					"read_execout: dup of stdout failed");
				exit(-1);
			}
			close(p[1]);
			if (set_user(user_to_execute_maps_as) == 0) {
				execv(fname, &args[0]);
			}
			exit(-1);
		}
	default:
		/*
		 * Parent
		 */
		close(p[1]);

		/*
		 * wait for child to complete. Note we read after the
		 * child exits to guarantee a full pipe.
		 */
		while (waitpid(child_pid, &status, 0) < 0) {
			/* if waitpid fails with EINTR, restart */
			if (errno != EINTR) {
				status = -1;
				break;
			}
		}
		if (!sigfail)
			sigaction(SIGCLD, &oact, NULL);

		if (status != -1)
			*lp = get_line(fdopen(p[0], "r"), fname, line, linesz);
		close(p[0]);

		/* free args */
		if (args[0])
			free(args[0]);
		if (args[1])
			free(args[1]);

		if (trace > 3)
			fprintf(stderr, "  read_execout: map=%s key=%s line=%s\n",
			fname, key, line);

		return (status);
	}
}
