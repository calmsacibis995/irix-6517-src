/* @(#)exportent.c	1.8 88/03/10 D/NFS */
/* @(#)exportent.c 1.8 88/02/08 SMI */

/*
 * Exported file system table manager. Reads/writes "/etc/xtab".
 * Copyright (C) 1986 by Sun Microsystems, Inc.
 */

#ifdef __STDC__
	#pragma weak setexportent = _setexportent
	#pragma weak endexportent = _endexportent
	#pragma weak getexportent = _getexportent
	#pragma weak remexportent = _remexportent
	#pragma weak addexportent = _addexportent
	#pragma weak getexportopt = _getexportopt
#endif
#include "synonyms.h"

#include <stdio.h>
#include <ctype.h>
#include <exportent.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>		/* for prototyping */
#include <sys/stat.h>		/* for prototyping */
#include <fcntl.h>		/* for prototyping */
#include <unistd.h>		/* for prototyping */
#include <stdlib.h>		/* for prototyping */

#define BUFINCRSZ 128

static char TMPFILE[] = "/tmp/xtabXXXXXX";

static char *skipwhite(char *);
static char *skipnonwhite(char *);
static char *sgi_options(char *, char *);

FILE *
setexportent(void)
{
	FILE *f;
	int fd;

	/*
	 * Create the tab file if it does not exist already
	 */ 
	if (access(TABFILE, F_OK) < 0) {
		fd = open(TABFILE, O_CREAT, 0644);
		close(fd);
	}
	if (access(TABFILE, W_OK) == 0) {
		f = fopen(TABFILE, "r+");
	} else {
		f = fopen(TABFILE, "r");
	}
	if (f == NULL) {	
	   	return (NULL);
	}
	if (flock(fileno(f), LOCK_EX) < 0) {
		(void)fclose(f);
		return (NULL);
	}
	return (f);
}


void
endexportent(FILE *f)
{
	(void)fclose(f);
}


/*
 * SGI and old /etc/exports syntax permits:
 *
 *	exportpath	options		importer1 importer2 ...
 *
 * where importer1, importer2, etc. are hostnames or netgroups.
 * The new /etc/exports syntax specifies import access with the
 * "access" options:
 *
 *	exportpath	options,access=importer1:importer2:...
 *
 */
/*ARGSUSED*/
static char *
sgi_options(
	char *line,	/* beginning of malloc'ed line vector */
	char *p)	/* pointer to first option char in line */
{
	static char *buf = NULL;
	static size_t bufsz = 0;
	char *opts;
	char *next;
	char *importer;
	char *lasttok;
	size_t len;
	char *b;

	opts = p;

	len = strlen(p);
	if ( buf == NULL ) {
		buf = (char *)malloc(2*len);
		if ( buf == NULL ) {
			return opts;
		}
		bufsz = 2*len;
	} else if ( bufsz < 2*len ) {
		free(buf);
		buf = (char *)malloc(2*len);
		if ( buf == NULL ) {
			return opts;
		}
		bufsz = 2*len;
	}
	b = buf;

	/* copy options to buf */
	while ( *p && (*p != ' ') && (*p != '\t') ) {
		*b++ = *p++;
	}
	if (*p == 0) {
		return opts;
	}
	*p++ = 0;
	/* advance p to the old-style access list */
	p = skipwhite(p);
	if (*p == 0) {
		return opts;
	}

	next = b;
	lasttok = NULL;
	importer = strtok_r(p, " \t", &lasttok);
	next += sprintf(next, ",%s=%s", ACCESS_OPT, importer);
	while (importer = strtok_r(0, " \t", &lasttok)) {
		next += sprintf(next, ":%s", importer);
	}

	return buf;
}

#define getexportent_buf_append(c, bp, buf, bufsz)			\
	{								\
		if ( bp == (bufsz) ) {					\
			char *buf2;					\
			buf2 = realloc((buf), (bufsz)+BUFINCRSZ);	\
			if ( buf2 == NULL ) {				\
				return NULL;				\
			}						\
			(buf) = buf2;					\
			(bufsz) += BUFINCRSZ;				\
		} 							\
		(buf)[bp++] = (c);					\
	}

struct exportent *
getexportent(FILE *f)
{
	static char *buf = NULL;
	static size_t bufsz = 0;
	size_t bp = 0;
	static struct exportent xent _INITBSSS;
	int done;
	int cc;
	char *p;

	/* initialize buffer if needed */
	if ( buf == NULL ) {
		buf = (char *)malloc((unsigned)BUFINCRSZ);
		if ( buf == NULL ) {
			return NULL;
		}
		bufsz = BUFINCRSZ;
	}

	/* get a line, growing buf as necessary */
	done = 0;
	while ( !done ) {
		cc = getc(f);
		switch ( cc ) {
		case EOF:
			if ( bp == 0 ) {
				return (NULL);
			} else {
				getexportent_buf_append('\0', bp, buf, bufsz);
				done = 1;
			}
			break;
		case '\n':
			getexportent_buf_append('\0', bp, buf, bufsz);
			done = 1;
			break;
		default:
			getexportent_buf_append(cc, bp, buf, bufsz);
			break;
		}
	}

	xent.xent_dirname = buf;
	xent.xent_options = NULL;
	/* skip over the dirname */
	p = skipnonwhite(buf);
	/* was that all? */
	if (*p == 0) {
		return (&xent);
	}
	/* terminate xent_dirname string */
	*p++ = 0;
	/* skip over the intervening whitespace */
	p = skipwhite(p);
	/* was that all? */
	if (*p == 0) {
		return (&xent);
	}
	/* skip leading '-' */
	if (*p == '-') {
		p++;
	}
	/* convert old style options to new */
	xent.xent_options = sgi_options(buf, p);

	return (&xent);
}

#define remexportent_buf_append(c, bp, buf, bufsz)			\
	{								\
		if ( bp == (bufsz) ) {					\
			char *buf2;					\
			buf2 = realloc((buf), (bufsz)+BUFINCRSZ);	\
			if ( buf2 == NULL ) {				\
				(void)fclose(f2);			\
				return (-1);				\
			}						\
			(buf) = buf2;					\
			(bufsz) += BUFINCRSZ;				\
		} 							\
		(buf)[bp++] = (c);					\
	}

int
remexportent(FILE *f, char *dirname)
{
	static char *buf = NULL;
	static size_t bufsz = 0;
	FILE *f2;
	size_t len;
	char fname[sizeof(TMPFILE)];
	int fd;
	long pos;
	long rempos;
	int remlen;
	int res;
	int cc;
	int filedone;
	int linedone;
	int bp;

	pos = ftell(f);
	rempos = 0;
	remlen = 0;
	(void)strcpy(fname, TMPFILE);
 	fd = mkstemp(fname);
	if (fd < 0) {
		return (-1);
	}
	if (unlink(fname) < 0) {
		(void)close(fd);
		return (-1);
	}
	f2 = fdopen(fd, "r+");
	if (f2 == NULL) {
		(void)close(fd);
		return (-1);
	}
	if ( buf == NULL ) {
		buf = (char *)malloc(BUFINCRSZ);
		if ( buf == NULL ) {
			(void)fclose(f2);
			return (-1);
		}
		bufsz = BUFINCRSZ;
	}
	len = strlen(dirname);
	rewind(f);
	filedone = 0;
	while ( !filedone ) {
		linedone = 0;
		bp = 0;
		while ( !linedone ) {
			switch ( cc = getc(f) ) {
			case '\n':
				remexportent_buf_append('\n', bp, buf, bufsz);
				remexportent_buf_append('\0', bp, buf, bufsz);
				linedone = 1;
				break;
			case EOF:
				remexportent_buf_append('\0', bp, buf, bufsz);
				filedone = linedone = 1;
				break;
			default:
				remexportent_buf_append(cc, bp, buf, bufsz);
				break;
			}
		}
		if (strncmp(buf, dirname, len) != 0 || ! isspace(buf[len])) {
			if (fputs(buf, f2) == EOF) {
				(void)fclose(f2);	
				return (-1);
			}
		} else {
			remlen = (int)strlen(buf);
			rempos = ftell(f) - remlen;
		}
	}
	rewind(f);
	if (ftruncate(fileno(f), 0L) < 0) {
		(void)fclose(f2);	
		return (-1);
	}
	rewind(f2);
	while (fgets(buf, (int)bufsz, f2)) {
		if (fputs(buf, f) == EOF) {
			(void)fclose(f2);
			return (-1);
		}
	}
	(void)fclose(f2);
	if (remlen == 0) {
		/* nothing removed */
		(void) fseek(f, pos, L_SET);
		res = -1;
	} else if (pos <= rempos) {
		res = fseek(f, pos, L_SET);
	} else if (pos > rempos + remlen) {
		res = fseek(f, pos - remlen, L_SET);
	} else {
		res = fseek(f, rempos, L_SET);
	}
	return (res < 0 ? -1 : 0); 
}


int
addexportent(FILE *f, char *dirname, char *options)
{
	long pos;	

	pos = ftell(f);
	if (fseek(f, 0L, L_XTND) >= 0) 
	{
	    fprintf(f, "%s", dirname);
	    if (options)
		fprintf(f, " -%s", options);
	    fprintf(f, "\n");
	    if (fseek(f, pos, L_SET) >= 0)
		return (0);
	}
	return (-1);
}
 
char *
getexportopt(struct exportent *xent, char *opt)
{
	static char *tokenbuf = NULL;
	static size_t tokenbufsz = 0;
	char *lp;
	char *tok, *lasttok;
	size_t len;
	char *ntokenbuf;

	if (xent->xent_options == NULL) {
		return (NULL);
	}

	len = strlen(xent->xent_options) + 1;
	if ( tokenbufsz < len ) {
		if (tokenbuf == NULL) {
			tokenbuf = (char *)malloc(len);
			if (tokenbuf == NULL) {
				return NULL;
			}
		} else {
			ntokenbuf = (char *)realloc(tokenbuf, len);
			if (ntokenbuf == NULL) {
				return NULL;
			}
			tokenbuf = ntokenbuf;
		}
	}

	(void)strcpy(tokenbuf, xent->xent_options);
	lp = tokenbuf;
	len = strlen(opt);
	lasttok = NULL;
	while ((tok = strtok_r(lp, ",", &lasttok)) != NULL) {
		lp = NULL;
		if (strncmp(opt, tok, len) == 0) {
			if (tok[len] == '=') {
				return (&tok[len + 1]);
			} else if (tok[len] == 0) {
				return ("");
			}
		}
	}
	return (NULL);
}
 
#define iswhite(c) 	((c) == ' ' || c == '\t')

static char *
skipwhite(char *str)
{
	while (*str && iswhite(*str)) {
		str++;
	}
	return (str);
}

static char *
skipnonwhite(char *str)
{
	while (*str && ! iswhite(*str)) {
		str++;
	}
	return (str);
}
