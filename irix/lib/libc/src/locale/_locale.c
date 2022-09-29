/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/_locale.c	1.5"
#include "synonyms.h"
#include <locale.h>
#include "_locale.h"
#include <string.h>
#include <stdlib.h>
#include "mplib.h"

#include <wchar.h>
#include "iconv_cnv.h"
#include "iconv_int.h"

#define min(a,b)	(a) < (b) ? (a) : (b)

char *	_alias_val = 0;
char	_alias_name [ LC_NAMELEN ];

/* return value for category with "" locale.
 * search_alias will be set to 1 if the environment variable
 * used requires the alias list to be searched. Alias are
 * allowed for LC_ALL and LANG environment variables
 */
/* move out of function scope so we get a global symbol for use with data cording */
static char ansn[LC_NAMELEN];

char *
_nativeloc(int cat, int * search_alias )
{
#define	lang "LANG"
#define NMC	4
	static const char _loc_envs[LC_ALL][NMC][12] =	/* env. vars for "" */
	{
		{"LC_CTYPE",	lang, 	"CHRCLASS",	""},
		{"LC_NUMERIC",	lang,	"",		""},
		{"LC_TIME",	lang,	"LANGUAGE",	""},
		{"LC_COLLATE",	lang,	"",		""},
		{"LC_MONETARY",	lang,	"",		""},
		{"LC_MESSAGES",	lang,	"",		""},
	};
	register char *s;
	register const char *p;
	register int index;
	int maxlen = LC_NAMELEN-1;

	assert(ISLOCKLOCALE);
	if ((s = getenv("LC_ALL")) == 0 || s[0] == '\0') {
		for (index = 0; index < NMC; ++index) {
			p = _loc_envs[cat][index];
			if ((s = getenv(p)) != 0 && s[0] != '\0') {

			        /* if we used LANG, then we need to search for aliases */
			        *search_alias = ! strcmp ( p, lang );
				goto found;
			}
		}
		*search_alias = 0;
		return "C";
	}
	*search_alias = 1;
found:
	(void)strncpy(ansn, s, (size_t)maxlen);
	ansn[maxlen] = '\0';
	return ansn;
}

char *
_fullocale(const char *loc, const char *file)	/* "/usr/lib/locale/<loc>/<file>" */
{
	register char *p;
#ifdef _LIBC_NONSHARED
	static char ansf[18 + 2 * LC_NAMELEN] = "/usr/lib/locale/";
#else  /* _LIBC_NONSHARED */
	static char *ansf = NULL;

	if ((ansf == NULL) && (ansf = calloc(1, 18 + 2 * LC_NAMELEN)) == 0)
		return 0;

	(void)strcpy(ansf, "/usr/lib/locale/");
#endif /* _LIBC_NONSHARED */

	assert(ISLOCKLOCALE);
	p = ansf + 16;
	(void)strcpy(p, loc);
	p += strlen(loc);
	p[0] = '/';
	(void)strcpy(p + 1, file);
	return ansf;
}
