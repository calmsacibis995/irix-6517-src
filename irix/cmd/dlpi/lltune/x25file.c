
/******************************************************************
 *
 *  SpiderX25 - Configuration utility
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *  X25_FILE.C
 *
 *  X25 Control Interface: Read Configuration File
 *
 ******************************************************************/

/*
 *	 /usr/projects.old/tcp/PBRAIN/SCCS/pbrainF/rel/src/clib/x25util/0/s.x25file.c
 *	@(#)x25file.c	5.8
 *
 *	Last delta created	10:07:04 5/12/92
 *	This file extracted	15:14:16 5/14/92
 *
 */


#include <stdio.h>
#include <string.h>

extern long strtol();
extern char *prog_name;
extern void conf_exerr(char *);
extern void experr(char *, char *);

typedef unsigned short ushort;
typedef unsigned char  unchar;

/* location of configuration files */
#ifndef TEMPLATE
#define TEMPLATE "/var/opt/snet/template/"
#endif

/* value for access call */
#define	ACCREAD	04


#define BUFSZ   256

static char *cname;
static int cline;
static FILE *cf;
static char inbuf[BUFSZ];

/* Open file for reading */
void
open_conf(file)
char *file;
{
	if (file)
	{
		if (file[0] == '/')
			cname = file;
		else
		{
			static char fullname[100];

			/*
			 *  Check if given configuration file exists in
			 *  template directory then fail.
			 */

			strcpy(fullname, TEMPLATE);
			strcat(fullname, file);
			cname = fullname;
		}

		if (!(cf = fopen(cname, "r")))
			experr("file %s failed to open", cname);
	}
	else
		cf = stdin;

	cline = 0;

}
	
/* Close conf file */
void
close_conf()
{
	if (cname)
	{
		do
		{
			cline++;
			if (!fgets(inbuf, BUFSZ, cf))
			{
				fclose(cf);
				return;
			}
		}
		while (inbuf[0] == '#' || inbuf[0] == '\n');
		conf_exerr("too many lines");
	}
}

/* Return next non-empty line */
char *get_conf_line()
{
	do
	{
		cline++;
		if (!fgets(inbuf, BUFSZ, cf))
			conf_exerr("not enough lines");
		if (!strchr(inbuf, '\n'))
			conf_exerr("line too long");
		*strpbrk(inbuf, "#\n") = '\0';
	}
	while (inbuf[0] == '\0');
	return inbuf;
}

/* Return unsigned short value */
ushort
get_conf_ushort()
{
	register ushort uval;
	long value;
	char *line;

	char *s;

	/* Read value */
	line = get_conf_line();
	uval = (ushort)(value = strtol(line, &s, 10));
	if (*s) conf_exerr("syntax error");
	if ((long)uval != value) conf_exerr("out of range");

	return uval;
}
	
int
hex_val(c)
char c;
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;	/* Not hex! */
}


/* Return short value in up to 'n' hex digits */
ushort
get_conf_hex(n)
{
	ushort uval = 0;
	register char *p = get_conf_line();

	while (*p && n--)
	{
		register digit;

		if ((digit = hex_val(*p++)) < 0)
			conf_exerr("not a hex digit");
		uval = uval << 4 | digit;
	}

	if (*p) conf_exerr("too many hex digits");

	return uval;
}
	
/* Return unsigned char value */
unchar
get_conf_unchar()
{
	register ushort uval = get_conf_ushort();
	if (uval > 255) conf_exerr("out of range");

	return uval;
}
	

/* Report errors */
void
conf_exerr(text)
char *text;
{
	fprintf(stderr, "%s: %s at line %d of ", prog_name, text, cline);
	if (cname)
	{
		fprintf(stderr, "file %s\n", cname);
		fclose(cf);
	}
	else
		fprintf(stderr, "input\n");
	exit(2);
}


void
exerr(format, arg)
char *format, *arg;
{
	fprintf(stderr, "%s: ", prog_name);
	fprintf(stderr, format, arg);
	fprintf(stderr, "\n");
	exit(2);
}

void
experr(format, device)
char *format, *device;
{
	extern errno, sys_nerr;
	extern char *sys_errlist[];

	fprintf(stderr, "%s: ", prog_name);
	fprintf(stderr, format, device);
	if (errno < sys_nerr)
		fprintf(stderr, " (%s)\n", sys_errlist[errno]);
	else
		fprintf(stderr, " (error %d)\n", errno);
	exit(2);
}

char *basename(path)
char	*path;
{
	char	*base = path;

	while (*base)
		if (*base++ == '/')
			path = base;
	
	return path;
}

