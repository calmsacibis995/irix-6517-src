/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"$Revision: 1.4 $"

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <pkgstrct.h>
#include <stdlib.h>

char	*errstr = NULL;
int	attrpreset = 0;

static int	getstr(FILE *, char *, int, char *),
		getnum(FILE *, int, long *, long),
		getend(FILE *),
		eatwhite(FILE *);
static short	quoted = 0; /* used for quoted pathnames(special characters) */

static char	mypath[PATH_MAX];
static char	mylocal[PATH_MAX];

int
gpkgmap(struct cfent *ept, FILE *fp, int proto)
{
	int	c;

	errstr = NULL;
	ept->volno = 0;
	ept->ftype = BADFTYPE;
	(void) strcpy(ept->class, BADCLASS);
	ept->path = NULL;
	ept->ainfo.local = NULL;
	if(!attrpreset) {
		/* default attributes were supplied, so don't reset */
		ept->ainfo.mode = -1;
		(void) strcpy(ept->ainfo.owner, "NONE");
		(void) strcpy(ept->ainfo.group, "NONE");
		ept->ainfo.major = BADMAJOR;
		ept->ainfo.minor = BADMINOR;
	}
	ept->cinfo.cksum = ept->cinfo.modtime = ept->cinfo.size = (-1L);

	ept->npkgs = 0;

	if(!fp)
		return(-1);
readline:
	switch(c = eatwhite(fp)) {
	  case EOF:
		return(0);

	  case '0':
	  case '1':
	  case '2':
	  case '3':
	  case '4':
	  case '5':
	  case '6':
	  case '7':
	  case '8':
	  case '9':
		if(ept->volno) {
			errstr = "bad volume number";
			goto error;
		}
		do {
			ept->volno = (ept->volno*10)+c-'0';
			c = getc(fp);
		} while(isdigit(c));
		goto readline;

	  case ':':
	  case '#':
		(void) getend(fp);
		/* fall through */
	  case '\n':
		goto readline;

	  case 'i':
		ept->ftype = (char) c;
		c = eatwhite(fp);
		/* fall through */
	  case '.':
	  case '/':
	  case '\'':
		(void) ungetc(c, fp);

		if(getstr(fp, "=", PATH_MAX, mypath)) {
			errstr = "unable to read pathname field";
			goto error;
		}
		ept->path = mypath;
		if(quoted)
			ept->quoted = 1;
		else
			ept->quoted = 0;
		quoted = 0;

		c = getc(fp);
		if(c == '=') {
			if(getstr(fp, NULL, PATH_MAX, mylocal)) {
				errstr = "unable to read local pathname";
				goto error;
			}
			ept->ainfo.local = mylocal;
			if(quoted)
				ept->quoted = 1;
			quoted = 0;

		} else
			(void) ungetc(c, fp);

		if(ept->ftype == 'i') {
			/* content info might exist */
			if(!getnum(fp, 10, (long *)&ept->cinfo.size, BADCONT) &&
			(getnum(fp, 10, (long *)&ept->cinfo.cksum, BADCONT) ||
			getnum(fp, 10, (long *)&ept->cinfo.modtime, BADCONT))) {
				errstr = "unable to read content info";
				goto error;
			}
		}

		if(getend(fp)) {
			errstr = "extra tokens on input line";
			return(-1);
		}
		return(1);

	  case '?':
	  case 'f':
	  case 'v':
	  case 'e':
	  case 'l':
	  case 's':
	  case 'p':
	  case 'c':
	  case 'b':
	  case 'd':
	  case 'x':
		ept->ftype = (char) c;
		if(getstr(fp, NULL, CLSSIZ, ept->class)) {
			errstr = "unable to read class token";
			goto error;
		}
		if(getstr(fp, "=", PATH_MAX, mypath)) {
			errstr = "unable to read pathname field";
			goto error;
		}
		ept->path = mypath;
		if(quoted)
			ept->quoted = 1;
		else
			ept->quoted = 0;
		quoted = 0;

		c = getc(fp);
		if(c == '=') {
			/* local path */
			if(getstr(fp, NULL, PATH_MAX, mylocal)) {
				errstr = (strchr("sl", ept->ftype) ?
					"unable to read link specification" :
					"unable to read local pathname");
				goto error;
			}
			ept->ainfo.local = mylocal;
			if(quoted)
				ept->quoted = 1;
			quoted = 0;
		} else if(strchr("sl", ept->ftype)) {
			if((c != EOF) && (c != '\n'))
				(void) getend(fp);
			errstr = "missing or invalid link specification";
			return(-1);
		} else
			(void) ungetc(c, fp);
		break;

	  default:
		errstr = "unknown ftype";
error:
		(void) getend(fp);
		return(-1);
	}

	if(strchr("sl", ept->ftype) && (ept->ainfo.local == NULL)) {
		errstr = "no link source specified";
		goto error;
	}

	if(strchr("cb", ept->ftype)) {
		ept->ainfo.major = BADMAJOR;
		ept->ainfo.minor = BADMINOR;
		if(getnum(fp, 10, (long *)&ept->ainfo.major, BADMAJOR) ||
		   getnum(fp, 10, (long *)&ept->ainfo.minor, BADMINOR)) {
			errstr = "unable to read major/minor device numbers";
			goto error;
		}
	}

	if(strchr("cbdxpfve", ept->ftype)) {
		/* links and information files don't 
		 * have attributes associated with them */
		if(getnum(fp, 8, (long *)&ept->ainfo.mode, BADMODE))
			goto end;

		/* mode, owner, group should be here */
		if(getstr(fp, NULL, ATRSIZ, ept->ainfo.owner) ||
		   getstr(fp, NULL, ATRSIZ, ept->ainfo.group)) {
			errstr = "unable to read mode/owner/group";
			goto error;
		}

	/* The following code is commented out since not sure of
	 * the purpose for it and it causes problems since 02000 on 
	 * a directory is legal as of 4.0
	 *	comment - don't have to check for (mode < 0) since '-'
	 *	   is not a legal digit 
	 *	if((ept->ainfo.mode != BADMODE) && ((ept->ainfo.mode > 07777) ||
	 *	(strchr("cbdxp", ept->ftype) && (ept->ainfo.mode > 02000)))) {
	 *		errstr = "illegal value for mode";
	 *		goto error;
	 *	}
	 */
	}
			
	if(strchr("ifve", ept->ftype)) {
		/* if what is being read is not a prototype file, 
		 * then(pkgmap) read the contents info first
		 */
		if(!proto) {
			/* look for content description */
			if(!getnum(fp, 10, (long *)&ept->cinfo.size, BADCONT) &&
			(getnum(fp, 10, (long *)&ept->cinfo.cksum, BADCONT) ||
			getnum(fp, 10, (long *)&ept->cinfo.modtime, BADCONT))) {
				errstr = "unable to read content info";
				goto error;
			}
		}
	}

	if(ept->ftype == 'i')
		goto end;

end:
	if(getend(fp) && ept->pinfo) {
		errstr = "extra token on input line";
		return(-1);
	}

	return(1);
}

static int
getnum(FILE *fp, int base, long *d, long bad)
{
	int c;

	/* leading white space ignored */
	c = eatwhite(fp);
	if(c == '?') {
		*d = bad;
		return(0);
	}

	if(c == '-') 
		return(0);

	if((c == EOF) || (c == '\n') || !isdigit(c)) {
		(void) ungetc(c, fp);
		return(1);
	}

	*d = 0;
	while(isdigit(c)) {
		*d = (*d * base) + (c & 017);
		c = getc(fp);
	}
	(void) ungetc(c, fp);
	return(0);
}

static int
getstr(FILE *fp, char *sep, int n, char *str)
{
	int c;
	/* leading white space ignored */
	c = eatwhite(fp);
	if((c == EOF) || (c == '\n')) {
		(void) ungetc(c, fp);
		return(1); /* nothing there */
	}

	if(c == '-')
		return(0);

	if(c == '\'') {
		/* quoted pathname - contains special characters */
		quoted = 1;
		/* fill up string until another single quote */
		c = getc(fp);
		do {
			if(n-- < 1) {
				*str = '\0';
				return(-1); /* too long */
			}
			if((c == EOF) || (c == '\n'))	
				return(1);
			*str++ = (char) c;
		} while((c = getc(fp)) != '\'');
		*str = '\0';
		return(0);
	}

	/* fill up string until space, tab, or separator */
	while(!strchr(" \t", c) && (!sep || !strchr(sep, c))) {
		if(n-- < 1) {
			*str = '\0';
			return(-1); /* too long */
		}
		*str++ = (char) c;
		c = getc(fp);
		if((c == EOF) || (c == '\n'))
			break; /* no more on this line */
	}
	*str = '\0';
	(void) ungetc(c, fp);
	return(0);
}

static int
getend(FILE *fp)
{
	int c;
	int n;

	n = 0;
	do {
		if((c = getc(fp)) == EOF)
			return(n);
		if(!isspace(c))
			n++;
	} while(c != '\n');
	return(n);
}
	
static int
eatwhite(FILE *fp)
{
	int c;

	/* this test works around a side effect of getc() */
	if(feof(fp))
		return(EOF);
	do
		c = getc(fp);
	while((c == ' ') || (c == '\t'));
	return(c);
}
