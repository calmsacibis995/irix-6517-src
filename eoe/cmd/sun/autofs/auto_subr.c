/*
 *	auto_subr.c
 *
 *	Copyright (c) 1988-1993 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#ident	"@(#)auto_subr.c	1.12	94/02/17 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <mntent.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/systeminfo.h>
#include <sys/utsname.h>
#include <sys/fs/autofs.h>
#include <sys/capability.h>
#include "autofsd.h"

struct mntlist *current_mounts;

static char *check_hier(char *);

void
dirinit(mntpnt, map, opts, direct, stack, stkptr)
	char *mntpnt, *map, *opts;
	int direct;
	char **stack, ***stkptr;
{
	extern struct autodir *dir_head;
	extern struct autodir *dir_tail;
	struct autodir *dir;
	char *p;

	if (strcmp(map, "-null") == 0)
		goto enter;

	p = mntpnt + (strlen(mntpnt) - 1);
	if (*p == '/')
		*p = '\0';	/* trim trailing / */
	if (*mntpnt != '/') {
		pr_msg("dir %s must start with '/'", mntpnt);
		return;
	}
	if (p = check_hier(mntpnt)) {
		pr_msg("hierarchical mountpoint: %s and %s",
			p, mntpnt);
		return;
	}

	/*
	 * If it's a direct map then call dirinit
	 * for every map entry.
	 */
	if (strcmp(mntpnt, "/-") == 0) {
		(void) loaddirect_map(map, map, opts, stack, stkptr);
		return;
	}

enter:
	dir = (struct autodir *)malloc(sizeof *dir);
	if (dir == NULL)
		goto alloc_failed;
	memset((void *) dir, '\0', sizeof (*dir));
	dir->dir_name = strdup(mntpnt);
	if (dir->dir_name == NULL) {
		free((char *)dir);
		goto alloc_failed;
	}
	dir->dir_map = strdup(map);
	if (dir->dir_map == NULL) {
		free((char *)dir->dir_name);
		free((char *)dir);
		goto alloc_failed;
	}
	dir->dir_opts = strdup(opts);
	if (dir->dir_opts == NULL) {
		free((char *)dir->dir_name);
		free((char *)dir->dir_map);
		free((char *)dir);
		goto alloc_failed;
	}
	dir->dir_direct = direct;
	dir->dir_remount = 0;
	dir->dir_next = NULL;

	/*
	 * Append to dir chain
	 */
	if (dir_head == NULL)
		dir_head = dir;
	else
		dir_tail->dir_next = dir;

	dir->dir_prev = dir_tail;
	dir_tail = dir;

	return;

alloc_failed:
	pr_msg("dirinit: memory allocation failed");
}

/*
 *  Check whether the mount point is a
 *  subdirectory or a parent directory
 *  of any previously mounted autofs
 *  mount point.
 */
static char *
check_hier(mntpnt)
	char *mntpnt;
{
	extern struct autodir *dir_head;
	register struct autodir *dir;
	register char *p, *q;

	for (dir = dir_head; dir; dir = dir->dir_next) {
		p = dir->dir_name;
		q = mntpnt;
		for (; *p == *q; p++, q++)
			if (*p == '\0')
				break;
		if (*p == '/' && *q == '\0')
			return (dir->dir_name);
		if (*p == '\0' && *q == '/')
			return (dir->dir_name);
		if (*p == '\0' && *q == '\0')
			return (NULL);
	}
	return (NULL);	/* it's not a subdir or parent */
}

mkdir_r(dir)
	char *dir;
{
	int err;
	char *slash;

	if (mkdir(dir, 0555) == 0 || errno == EEXIST)
		return (0);
	if (errno != ENOENT)
		return (-1);
	slash = strrchr(dir, '/');
	if (slash == NULL)
		return (-1);
	*slash = '\0';
	err = mkdir_r(dir);
	*slash++ = '/';
	if (err || !*slash)
		return (err);
	return (mkdir(dir, 0555));
}

rmdir_r(dir, depth)
	char *dir;
	int depth;
{
	int is_spec = 0;
	int err;
	char mydir[MAXPATHLEN], *slash;

	if (dir[strlen(dir) -1] == ' ')
		is_spec = 1;

	/* make the common case fast */
	err = rmdir(dir);
	if (depth == 1)
		return (err);

	/* uncommon case */
	depth--;
	strcpy(mydir, dir);
	err = 0;
	do {
		depth--;
		slash = strrchr(mydir, '/');
		if (slash == NULL) {
			err = -1;
			break;
		}
		if (is_spec) {
			*slash = ' ';
			*(slash+1) = '\0';
		} else
			*slash = '\0';
		err = rmdir(mydir);
	} while (depth && !err);
	return (err);
}

static int
cap_mkdir(const char *dir, mode_t mode)
{
	int r;
	cap_t ocap;
	const cap_value_t mkdir_caps[] = {CAP_MAC_READ, CAP_MAC_WRITE};

	ocap = cap_acquire(_CAP_NUM(mkdir_caps), mkdir_caps);
	r = mkdir(dir, mode);
	cap_surrender(ocap);
	return(r);
}

autofs_mkdir_r(dir, depth)
	char *dir;
	int *depth;
{
	int err;
	char *slash;
	char spec_dir[MAXPATHLEN]; /* XXX not a good idea in recursive func */

	(void) sprintf(spec_dir, "%s%s", dir, " ");

	err = cap_mkdir(spec_dir, 0555);
	if (err == 0) {
		(*depth)++;
		return (0);
	} else if (errno == EEXIST)
		return (0);
	else if (errno != ENOENT)
		return (-1);

	slash = strrchr(dir, '/');
	if (slash == NULL)
		return (-1);
	*slash = '\0';
	err = autofs_mkdir_r(dir, depth);
	*slash++ = '/';
	if (err || !*slash)
		return (err);
	(*depth)++;
	return (cap_mkdir(spec_dir, 0555));
}

/*
 * Gets the next token from the string "p" and copies
 * it into "w".  Both "wq" and "w" are quote vectors
 * for "w" and "p".  Delim is the character to be used
 * as a delimiter for the scan.  A space means "whitespace".
 *
 * If the token is greater than the buffer size, continue
 * parsing string "p" until the deliminator is found, but
 * return null strings for 'w' and 'wq'.
 */
void
getword(w, wq, p, pq, delim, bufsize)
	char *w, *wq, **p, **pq, delim;
	int bufsize;
{
	int copied = 0;
	char *orig_w = w;

	while ((delim == ' ' ? isspace(**p) : **p == delim) && **pq == ' ')
		(*p)++, (*pq)++;

	while (**p &&
		!((delim == ' ' ? isspace(**p) : **p == delim) &&
			**pq == ' ')) {
		if (++copied < bufsize) {
			*w++  = *(*p)++;
			*wq++ = *(*pq)++;
		} else {
			(*p)++;
			(*pq)++;
		}
	}
	/*
	 * if token larger than buffer, return a null (ie: w[0]="\0") buf
	 */
	if (copied >= bufsize) {
		w = orig_w;
	}

	*w  = '\0';
	*wq = '\0';
}

char *
get_line(fp, map, line, linesz)
	FILE *fp;
	char *map;
	char *line;
	int linesz;
{
	register char *p = line;
	register int len;
	int excess = 0;
	int i;

	*p = '\0';

	for (;;) {
		if (fgets(p, linesz - (p-line), fp) == NULL) {
			return (*line ? line : NULL);	/* EOF */
		}

		len = strlen(line);
		if (len <= 0) {
			p = line;
			continue;
		}
		p = &line[len - 1];

		/*
		 * Is input line too long?
		 */
		if (*p != '\n') {
			excess = 1;
			/*
			 * Perhaps last char read was '\'. Reinsert it
			 * into the stream to ease the parsing when we
			 * read the rest of the line to discard.
			 */
			(void) ungetc(*p, fp);
			break;
		}
trim:
		/* trim trailing white space */
		while (p >= line && isspace(*(u_char *)p))
			*p-- = '\0';
		if (p < line) {			/* empty line */
			p = line;
			continue;
		}

		if (*p == '\\') {		/* continuation */
			*p = '\0';
			continue;
		}

		/* ignore # and beyond */
		if (p = strchr(line, '#')) {
			*p-- = '\0';
			goto trim;
		}
		break;
	}
	if (excess) {
		/*
		 * Discard rest of the line.
		 */
		while (*p = i = getc(fp), i != EOF) {
			if (*p == '\n')		/* end of the long line */
				break;
			else if (*p == '\\') {		/* continuation */
				if (getc(fp) == EOF)	/* ignore next char */
					break;
			}
		}
		syslog(LOG_ERR,
"Ignoring line which reaches maximum of %d characters in map %s.",
			linesz-1, map);
		*line = '\0';
	}

	return (line);
}

/*
 * Performs text expansions in the string "pline".
 * "plineq" is the quote vector for "pline".
 * An identifier prefixed by "$" is replaced by the
 * corresponding environment variable string.  A "&"
 * is replaced by the key string for the map entry.
 * Prevent overflow by dynamically allocating adjacent
 * p and q buffs.
 */
int
macro_expand(key, pline, plineq, psize)
	char *key, *pline, *plineq;
	int psize;
{
	register char *p,  *q;
	register char *bp, *bq;
	register char *s;
	char *buffp, *buffq, *bpend;
	char namebuf[64], *pn;
	char procbuf[256];
	int expand = 0;
	int overflow = 0;
	struct utsname name;
	char *getenv();

	if ((buffp = (char *)malloc(2*psize)) == NULL) {
		return -1;
	}
	bpend = buffq = buffp + psize;
	p = pline;  q = plineq;
	bp = buffp; bq = buffq;

	while (*p && !overflow) {
		if (*p == '&' && *q == ' ') {	/* insert key */
			for (s = key; *s; s++) {
				if (bp >= bpend) {
					overflow++;
					break;
				}
				*bp++ = *s;
				*bq++ = ' ';
			}
			expand++;
			p++; q++;
			continue;
		}

		if (*p == '$' && *q == ' ') {	/* insert env var */
			p++; q++;
			pn = namebuf;
			if (*p == '{') {
				p++; q++;
				while (*p && *p != '}') {
					*pn++ = *p++;
					q++;
				}
				if (*p) {
					p++; q++;
				}
			} else {
				while (*p && isalnum(*p)) {
					*pn++ = *p++;
					q++;
				}
			}
			*pn = '\0';

			s = getenv(namebuf);
			if (s) {
				/* found in env */
			} else if (strcmp(namebuf, "HOST") == 0) {
				(void) uname(&name);
				s = name.nodename;
			} else if (strcmp(namebuf, "OSREL") == 0) {
				(void) uname(&name);
				s = name.release;
			} else if (strcmp(namebuf, "OSNAME") == 0) {
				(void) uname(&name);
				s = name.sysname;
			} else if (strcmp(namebuf, "OSVERS") == 0) {
				(void) uname(&name);
				s = name.version;
			} else if (strcmp(namebuf, "ARCH") == 0) {
				(void) uname(&name);
				s = name.machine;
			} else if (strcmp(namebuf, "CPU") == 0) {
				if (sysinfo(SI_ARCHITECTURE, procbuf, 
					sizeof(procbuf)) > 0) {
					s = procbuf;
				}
			}
			if (s) {
				while (*s) {
					if (bp >= bpend) {
						overflow++;
						break;
					}
					*bp++ = *s++;
					*bq++ = ' ';
				}
			}
			expand++;
			continue;
		}
		if (bp >= bpend) {
			overflow++;
			break;
		}
		*bp++ = *p++;
		*bq++ = *q++;

	}
	if (overflow) {
		free(buffp);
		return -1;
	}
	if (!expand) {
		free(buffp);
		return 0;
	}
	*bp = '\0';
	*bq = '\0';
	(void) strcpy(pline, buffp);
	(void) strcpy(plineq, buffq);
	free(buffp);
	return 0;
}

/*
 * Removes quotes from the string "str" and returns
 * the quoting information in "qbuf". e.g.
 * original str: 'the "quick brown" f\ox'
 * unquoted str: 'the quick brown fox'
 * and the qbuf: '    ^^^^^^^^^^^  ^ '
 */
void
unquote(str, qbuf)
	char *str, *qbuf;
{
	register int escaped, inquote, quoted;
	register char *ip, *bp, *qp;
	char buf[2048];

	escaped = inquote = quoted = 0;

	for (ip = str, bp = buf, qp = qbuf; *ip; ip++) {
		if (!escaped) {
			if (*ip == '\\') {
				escaped = 1;
				quoted++;
				continue;
			} else
			if (*ip == '"') {
				inquote = !inquote;
				quoted++;
				continue;
			}
		}

		*bp++ = *ip;
		*qp++ = (inquote || escaped) ? '^' : ' ';
		escaped = 0;
	}
	*bp = '\0';
	*qp = '\0';
	if (quoted)
		(void) strcpy(str, buf);
}

/*
 * Removes trailing spaces from string "s".
 */
void
trim(s)
	char *s;
{
	char *p = &s[strlen(s) - 1];

	while (p >= s && isspace(*(u_char *)p))
		*p-- = '\0';
}
