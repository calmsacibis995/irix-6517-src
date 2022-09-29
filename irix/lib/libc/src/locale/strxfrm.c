/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/strxfrm.c	1.15"
#include "synonyms.h"
#include "shlib.h"
#include "mplib.h"
#include <fcntl.h>
#include <locale.h>
#include "_locale.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "colldata.h"
#include <unistd.h>

#define SZ_COLLATE	256	/* minimum size of collation table */
#define SUBFLG		0x80	/* used to specify high order bit of 
				 * secondary weight; this bit is on if
				 * there is a substitution string starting
				 * with this character*/


/* default collation table for the "C" locale */
static xnd	*colltbl = (xnd *)__colldata;	/* collation table */
static subnd	*subtbl _INITBSS;
static long	sub_cnt _INITBSS;	/* number of entries in substitution table */
static VOID	*database _INITBSS;
#ifdef REGEXP
static long	rflag = 0; /* rflag is 1 if database contains regular expressions */
#endif

static char	*cksub(const char **);


#ifdef REGEXP
#define INIT	register char *sp = instring;
#define GETC()	(*sp++)
#define PEEKC()	(*sp)
#define UNGETC(c)	(--sp)
#define RETURN(c)	return;
#define ERROR(c)	fprintf(stderr,"Error in regular expression (%d)\n", c)
#include <regexp.h>
#endif

size_t
strxfrm(char *s1, const char *s2, size_t n)
{
	int	j, idx;
	long	i;
	size_t	len = 0;
	char	*secstr, *sptr;
	const char	*svs2, *tptr;
	char	tmp;
	LOCKDECL ( l );

	if ( __libc_attr._collate_res._coll_as_cmp )
	{
	    if ( n && s1 )
	    {
		strncpy ( s1, s2, n - 1 );
		s1[n-1] = '\0';
	    }
	    return strlen ( s2 );
	}

	LOCKINIT(l, LOCKLOCALE);

	/* initialize colltbl */
	subtbl = __initcoll(&colltbl);

	/* setup string for secondary weights;
	 * the secondary weights will be input in reverse order starting
	 * at the end of the string, s1, and reversed later */
	secstr = sptr = s1 + n - 1;

	svs2 = NULL;
	for (;;) {
		if (*s2 == '\0') {
			/* check if currently transforming substitution string */
			if (!svs2)
				break;	/* LOOP EXIT */
			/* resume tranformation of original string */
			s2 = svs2;
			svs2 = NULL;
			continue;
		}

		/* check for substitution strings */
		if (!svs2) {
			if ((tptr = cksub(&s2)) != NULL) {
				svs2 = s2;
				s2 = tptr;
				continue;
			}
		}

		/* check for 2-to-1 character transformations;
		   default is 1-to-1 character transformation */
		idx = (unsigned char)*s2;
		if (colltbl[idx].ch != 0) {
			i = colltbl[idx].ch;
			j = colltbl[idx].ns + SZ_COLLATE;
			while (--i >= 0) {
				if ((unsigned char)s2[1] == colltbl[j].ch) {
					idx = j;
					s2++;
					break;
				}
				j++;
			}
		}

		/* write transformed character into s1 (if there's room) */
		if (colltbl[idx].pwt != 0) {
			len++;
			if (len < n)
				*s1++ = colltbl[idx].pwt;
			if ((colltbl[idx].swt & ~SUBFLG) != 0) {
				len++;
				if (len < n)
					*secstr-- = (char)(colltbl[idx].swt & ~SUBFLG);
			}
		}
		s2++;
	}

	/* reverse secondary string */
	if (s1) {
		*secstr = '\0';
		i = sptr - secstr;
		while (i-- >= 0) {
			if (s1 < secstr)
				*s1++ = *sptr--;
			else {
				tmp = *s1;
				*s1++ = *sptr;
				*sptr-- = tmp;
				i--;
			}
		}
	}
	UNLOCKLOCALE(l);
	return(len);
}

/* move out of function scope so we get a global symbol for use with data cording */
static char	saved_locale[LC_NAMELEN] = "C";
static 	subnd	*initst;

subnd *
__initcoll(xnd **colltbl)
{
	register long	i;
	int	infile;
	char	*substrs;
	size_t	s;
	struct 	hd *header;


/* (char *) has different sizes in 32-bit and 64-bit system. So
 *  using the following tmp struct to capture the subtbl data first
 */

	struct tnd {
		int	exp;
		int	explen;
		int	repl;
	} *tmpsubnd; 

	struct stat	ibuf;

	/* use setlocale() to find collation database */
	if (strcmp(_cur_locale[LC_COLLATE], saved_locale) == 0)
		return (initst);

	/* free them if saved_locale != current  */
	if ( database != NULL ) {
		free(database);
		database = NULL;
		}
	if ( initst != NULL) {
		free(initst);
		initst = NULL;
		}



	if ((infile = open(_fullocale(_cur_locale[LC_COLLATE],"LC_COLLATE"),O_RDONLY)) == -1) 
		return ((subnd *) -1L);

	/* get size of file, allocate space for it, and read it in */
	if (fstat(infile, &ibuf) != 0)
		goto err1;
	if ((ibuf.st_size < (off_t)sizeof(struct hd) ||
	    (database = malloc((size_t)ibuf.st_size)) == NULL))
		goto err1;
	if (read(infile, database, (size_t)ibuf.st_size) != ibuf.st_size)
		goto err2;
	close(infile);

	header = (struct hd *)database;
	*colltbl = (xnd *)(VOID *)((char *)database + header->coll_offst);

	if ((sub_cnt = header->sub_cnt) == 0) {
		initst = NULL;
	} else {
		s = sub_cnt * sizeof(struct subnd);
		if ((initst = (struct subnd *) malloc(s)) == NULL )
	   	   goto err1;
		tmpsubnd = (struct tnd *)(VOID *)((char *)database + header->sub_offst);
		substrs = (char *)database + header->str_offst;

	  for (i = 0; i < sub_cnt; i++) {
		initst[i].exp = (char *)( tmpsubnd[i].exp + substrs);
		initst[i].explen = tmpsubnd[i].explen;
		initst[i].repl = (char *)(tmpsubnd[i].repl + substrs);
		} 
		
	}
	
#ifdef REGEXP
	rflag = header->flags;
#endif
	(void) strcpy(saved_locale, _cur_locale[LC_COLLATE]);
	return (initst);
err2:
	free(database);
	database=NULL;
	return ((subnd *) -1L);
err1:
	close(infile);
	return ((subnd *) -1L);
}

static char *
cksub(const char **pstr)
{
	int	i;
	const char	*str;

#ifdef REGEXP
	if (rflag)
	{
		str = *pstr;
		if ( subtbl != (subnd *) -1L)
		for (i = 0; i < sub_cnt; i++) {
			if (advance(str, subtbl[i].exp) != 0) {
				*pstr = loc2;
				return(subtbl[i].repl);
			}
		}
	}
	else
#endif
	{
		if ((colltbl[(unsigned char)**pstr].swt & SUBFLG) == 0)
			return(NULL);
		str = *pstr;
		if ( subtbl != (subnd *) -1L)
		for (i = 0; i < sub_cnt; i++) {
			if (strncmp(str, subtbl[i].exp, (size_t)subtbl[i].explen)==0) {
				*pstr += subtbl[i].explen;
				return(subtbl[i].repl);
			}
		}
	}
	return(NULL);
}

int
strcoll(const char *s1, const char *s2)
{
	int		i, j, idx;
	const char	*svs1, *svs2, *tptr;
	unsigned int	prim1, prim2;
	int		sec1, sec2;
	int		secdiff = 0;
	LOCKDECL ( l );

	if ( __libc_attr._collate_res._coll_as_cmp )
	    return strcmp ( s1, s2 );

	LOCKINIT ( l, LOCKLOCALE );

	/* initialize colltbl */
	subtbl = __initcoll(&colltbl);

	svs1 = svs2 = NULL;
	for (;;) {
		if (*s1 == '\0') {
			/* check if currently transforming substitution string */
			if (!svs1) {
				prim1 = 0;
				goto s2lab;
			}
			/* resume tranformation of original string */
			s1 = svs1;
			svs1 = NULL;
			if (*s1 == '\0') {
				prim1 = 0;
				goto s2lab;
			}
		}

		/* check for substitution strings */
		if (!svs1) {
			if ((tptr = cksub(&s1)) != NULL && *tptr != '\0') {
				svs1 = s1;
				s1 = tptr;
			}
		}

		/* check for 2-to-1 character transformations;
		   default is 1-to-1 character transformation */
		idx = (unsigned char)*s1;
		if (colltbl[idx].ch != 0) {
			i = colltbl[idx].ch;
			j = colltbl[idx].ns + SZ_COLLATE;
			while (--i >= 0) {
				if ((unsigned char)s1[1] == colltbl[j].ch) {
					idx = j;
					s1++;
					break;
				}
				j++;
			}
		}
		s1++;
		if ((prim1 = colltbl[idx].pwt) == 0)
			continue;
		sec1 = colltbl[idx].swt & ~SUBFLG;

s2lab:
		if (*s2 == '\0') {
			/* check if currently transforming substitution string */
			if (!svs2) {
				if (prim1 == 0)
					break;	/* LOOP EXIT */
				UNLOCKLOCALE(l);
				return(1);
			}
			/* resume tranformation of original string */
			s2 = svs2;
			svs2 = NULL;
			if (*s2 == '\0') {
				if (prim1 == 0)
					break;	/* LOOP EXIT */
				UNLOCKLOCALE(l);
				return(1);
			}
		}

		/* check for substitution strings */
		if (!svs2) {
			if ((tptr = cksub(&s2)) != NULL && *tptr != '\0') {
				svs2 = s2;
				s2 = tptr;
			}
		}

		/* check for 2-to-1 character transformations;
		   default is 1-to-1 character transformation */
		idx = (unsigned char)*s2;
		if (colltbl[idx].ch != 0) {
			i = colltbl[idx].ch;
			j = colltbl[idx].ns + SZ_COLLATE;
			while (--i >= 0) {
				if ((unsigned char)s2[1] == colltbl[j].ch) {
					idx = j;
					s2++;
					break;
				}
				j++;
			}
		}
		s2++;
		if ((prim2 = colltbl[idx].pwt) == 0)
			goto s2lab;
		sec2 = colltbl[idx].swt & ~SUBFLG;

		if (prim1 != prim2) {
			UNLOCKLOCALE(l);
			return((int)(prim1 - prim2));
		}
		if (!secdiff)
			secdiff = sec1 - sec2;
	}
	UNLOCKLOCALE(l);
	return(secdiff);
}
