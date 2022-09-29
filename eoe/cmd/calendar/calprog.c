/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)calendar:calprog.c	1.8.1.3"

/* /usr/lib/calprog produces an egrep -f file
   that will select today's and tomorrow's
   calendar entries, with special weekend provisions

   used by calendar command
*/


#include <fcntl.h>
#include <varargs.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>
#include <locale.h>
#include <fmtmsg.h>
#include <unistd.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>

#define DAY	(3600*24L)

extern  char	*getenv(), *malloc();
extern	int	errno;

static read_tmpl();
static generate();

static char	*file;
static int	old_behavior;
static int	linenum = 1;
static long	t;

char	cmd_label[] = "UX:calprog";

struct calerr {
	int	flag;
	char	*cmsg;		/* catalog */
	char	*dmsg;		/* default */
} calerr[] = {
	{ SGINL_SYSERR,		_SGI_DMMX_CannotOpen,
		"Cannot open %s"			},
	{ SGINL_NOSYSERR,	_SGI_DMMX_outofmem,
		"Out of memory"				},
	{ SGINL_SYSERR,		_SGI_DMMX_cannotstat,
		"cannot stat %s"			},
	{ SGINL_NOSYSERR,	_SGI_DMMX_cal_notregf,
		"file '%s' is not a regular file"	},
	{ SGINL_SYSERR,		_SGI_DMMX_CannotRead,
		"Cannot read from %s"			},
	{ SGINL_NOSYSERR,	_SGI_DMMX_cal_lineerr,
		"file '%s', error on line %d"		},
	{ SGINL_NOSYSERR,	_SGI_DMMX_cal_illfmt,
		"file '%s', format descriptions are missing" },
};

#define	CAL_MAX_ERR	6

/*
 * some msg prints
 */
static void
error(nbr, arg1, arg2)
int nbr;
{
	register struct calerr *ep;

	if(nbr > CAL_MAX_ERR)
	    return;
	ep = calerr + nbr;
	_sgi_nl_error(ep->flag, cmd_label,
	    gettxt(ep->cmsg, ep->dmsg),
	    arg1,
	    arg2);
	exit(1);
}

static
char *month[] = {
	"[Jj]an",
	"[Ff]eb",
	"[Mm]ar",
	"[Aa]pr",
	"[Mm]ay",
	"[Jj]un",
	"[Jj]ul",
	"[Aa]ug",
	"[Ss]ep",
	"[Oo]ct",
	"[Nn]ov",
	"[Dd]ec"
};

static
tprint(t)
long t;
{
	struct tm *tm;
	tm = localtime(&t);
	(void)printf("(^|[ \t(,;])((%s[^ ]* *|0*%d/|\\*/)0*%d)([^0123456789]|$)\n",
		month[tm->tm_mon], tm->tm_mon + 1, tm->tm_mday);
}

main()
{
	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);

	(void)time(&t);
	if (((file = getenv("DATEMSK")) == 0) || file[0] == '\0')
		old_behavior = 1;
	if (old_behavior)
		tprint(t);
	else
		read_tmpl();
	switch(localtime(&t)->tm_wday) {
	case 5:
		t += DAY;
		if (old_behavior)
			tprint(t);
		else
			read_tmpl();
	case 6:
		t += DAY;
		if (old_behavior)
			tprint(t);
		else
			read_tmpl();
	default:
		t += DAY;
		if (old_behavior)
			tprint(t);
		else
			read_tmpl();
	}
	exit(0);
}


static
read_tmpl()
{
	char	*clean_line();
	FILE  *fp;
	char *bp, *start;
	struct stat64 sb;
	int	no_empty = 0;

	if ((start = (char *)malloc(512)) == NULL)
		error(1);
	if ((fp = fopen(file, "r")) == NULL)
		error(0, file);
	if (stat64(file, &sb) < 0)
		error(2, file);
	if ((sb.st_mode & S_IFMT) != S_IFREG)
		error(3, file);
	for(;;) {
	 	bp = start;
		if (!fgets(bp, 512, fp)) {
			if (!feof(fp)) 
				{
				free(start);
				fclose(fp);
				error(4, file);
				}
			break;
		}
		if (*(bp+strlen(bp)-1) != '\n')   /* terminating newline? */
			{
			free(start);
			fclose(fp);
			error(5, file, linenum);
			}
		bp = clean_line(bp);
		if (strlen(bp))  /*  anything left?  */
			{
			no_empty++;
			generate(bp);
			}
	linenum++;
	}
	free(start);
	fclose(fp);
	if (!no_empty)
		error(6, file);
}


char  *
clean_line(s)
char *s;
{
	char  *ns;

	*(s + strlen(s) -1) = (char) 0; /* delete newline */
	if (!strlen(s))
		return(s);
	ns = s + strlen(s) - 1; /* s->start; ns->end */
	while ((ns != s) && (isspace(*ns))) {
		*ns = (char)0;	/* delete terminating spaces */
		--ns;
		}
	while (*s)             /* delete beginning white spaces */
		if (isspace(*s))
			++s;
		else
			break;
	return(s);
}

static
generate(fmt)
char *fmt;
{
	char	timebuf[1024];
	char	outbuf[2 * 1024];
	char	*tb, *ob;
	int	space = 0;

	cftime(timebuf,fmt,&t);
	tb = timebuf;
	ob = outbuf;
	while(*tb)
		if (isspace(*tb))
			{
			++tb;
			space++;
			}
		else
			{
			if (space)
				{
				*ob++ = '[';
				*ob++ = ' ';
				*ob++ = '\t';
				*ob++ = ']';
				*ob++ = '*';
				space = 0;
				continue;
				}
			if(isalpha(*tb))
				{
				*ob++ = '[';
				*ob++ = toupper(*tb);
				*ob++ = tolower(*tb++);
				*ob++ = ']';
				continue;
				}
			else
				*ob++ = *tb++;
				if (*(tb - 1) == '0')
					*ob++ = '*';
			}
	*ob = '\0';
	printf("%s\n",outbuf);
}
