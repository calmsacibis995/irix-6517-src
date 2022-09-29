/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/setlocale.c	1.12"
/*
* setlocale - set and query function for all or parts of a program's locale.
*/
#include "synonyms.h"
#include "shlib.h"
#include "mplib.h"
#include <locale.h>
#include "_locale.h"	/* internal to libc locale data structures */
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <wchar.h>
#include "iconv_cnv.h"
#include "iconv_int.h"

static char *set_cat(int, const char *);
static char *get_part(int cat,  const char * locale);

#define min(a,b)	(a) < (b) ? (a) : (b)

/* Returns a pointer to the part of a compound locale name
 * (/a/b/c/d/e/f) corresponding to the category.
 *
 * This function still works if the locale arguments points to or
 * anywhere whithin the static variable part.
 * 
 * Returns NULL if locale does not contain enough parts
 */
/* move out of function scope so we get a global symbol for use with data cording */
static char gpart [ LC_NAMELEN ];

char *
get_part ( int cat, const char * locale )
{
    size_t l = 0;

    while ( (cat-- >= 0) && locale && *locale )
	if ( locale = strchr ( locale, '/' ) )
	    locale ++;

    if ( !locale )
	return NULL;

    if ( *locale )
    {
	l = min ( strcspn ( locale, "/" ), LC_NAMELEN - 1 );
	strncpy ( gpart, locale, l );
    }
    *(gpart + l) = '\0';

    return gpart;
}

/* move out of function scope so we get a global symbol for use with data cording */
static int reset = 0;

char *
setlocale(int cat, const char *loc)
{
	char part[LC_NAMELEN];
	char *lp;
	int prev_seen;

	LOCKDECL(l);

	if (cat > LC_ALL)
		return(NULL);
	LOCKINIT(l, LOCKLOCALE);
	if (loc == 0)	/* query */
	{
		char *ansr;

		if (cat != LC_ALL)
			ansr = _cur_locale[cat];
		else
		{
			register char *p, *q;
			register int flag = 0;
			register int i;
#ifdef _LIBC_NONSHARED
			static char ans[LC_ALL * LC_NAMELEN + 1];
#else  /* _LIBC_NONSHARED */
			static char *ans = NULL;

			if (!ans &&
			    (ans = calloc(1, LC_ALL * LC_NAMELEN + 1)) == 0)
				return 0;
#endif /* _LIBC_NONSHARED */

			/*
			* Generate composite locale description.
			*/
			ansr = p = ans;
			for (i = LC_CTYPE; i < LC_ALL; i++)
			{
				*p++ = '/';
				q = _cur_locale[i];
				(void)strcpy(p, q);
				p += strlen(q);
				if (!flag && i > LC_CTYPE)
					flag = strcmp(q, _cur_locale[i - 1]);
			}
			if (!flag)
				/* all locale designators the same */
				strcpy(ans, q);

			/* We have expanded an alias */
			if ( _alias_val && !strcmp ( ans, _alias_val ) )
			    strcpy ( ans, _alias_name );
			    
		}
		UNLOCKLOCALE(l);
		return ansr;
	}

	/*
	* Handle LC_ALL setting specially.
	*/
	if (cat == LC_ALL)
	{
		register const char *p;
		register size_t i;
		register char *sv_loc;
		int search_alias = 0;

		if (!reset)
			sv_loc = setlocale(LC_ALL, NULL);
		cat = LC_CTYPE;

		if ((p = loc)[0] != '/')	/* simple locale */
		{
			loc = strncpy(part, p, LC_NAMELEN - 1);
			part[LC_NAMELEN - 1] = '\0';
			search_alias = 1;
		}
		do	/* for each category other than LC_ALL */
		{
			if (p[0] == '/')	/* piece of composite locale */
			{
				i = strcspn(++p, "/");
				(void)strncpy(part, p, min ( i, LC_NAMELEN - 1 ) );
				part[i] = '\0';
				p += i;
			}
			if (part[0]=='\0')
				lp=_nativeloc ( cat, & search_alias );
			else
				lp = part;

			/* If the requested locale is identical, go to the next category */
			if ( !strcmp ( lp, _cur_locale[cat] ) )
			    continue;

			/* Alias substitution occurs only if the env var used is LANG or LC_ALL
			 * or if setlocale (LC_ALL, "<locale>" is used */
			if ( search_alias && lp[0] != '/' && ( _alias_val = __mbwc_locale_alias ( lp ) ) )
			{
			    strncpy ( _alias_name, lp, LC_NAMELEN - 1);
			    _alias_name[LC_NAMELEN - 1] = '\0';
			    lp = _alias_val;
			}

			/* Both _nativeloc and __mbwc_locale_alias could have returned a compound locale */
			if ( lp[0] == '/' )
			    lp = get_part ( cat, lp );

			prev_seen=0;

			if ( lp && cat > LC_NUMERIC )
				for(i=0;i<cat;i++) {
					if (strcmp(_cur_locale[i],lp)==0) {
						prev_seen=1;
						break;
					}
				}

			if (prev_seen) {
			    /* Resources need to be set for the LC_COLLATE category.
			       If the locale has not been seen yet, those resources
			       will get set in set_cat */
			    if ( cat == LC_COLLATE )
				_set_res_collate( loc );

				if (strcmp(_cur_locale[cat],lp) != 0) {
					(void) strcpy(_cur_locale[cat],lp);
				}
			} else {
				if ( ! lp || ( set_cat(cat, lp) == 0 ) ) {
					reset = 1;
					(void) setlocale(LC_ALL, sv_loc);
					reset = 0;
					UNLOCKLOCALE(l);
					return 0;
				}
			}
		} while (++cat < LC_ALL);
		UNLOCKLOCALE(l);
		return setlocale(LC_ALL, NULL);
	}
	lp = set_cat(cat, loc);
	UNLOCKLOCALE(l);
	return(lp);
}

static char *
set_cat(int cat, const char *loc)
{
	char part[LC_NAMELEN];
	int search_alias = 0;
	/*
	* Set single category's locale.  By default,
	* just note the new name and handle it later.
	* For LC_CTYPE and LC_NUMERIC, fill in their
	* tables now.
	*/
	if (loc[0] == '\0')
		loc = _nativeloc(cat, & search_alias );
	else
	{
	loc = strncpy(part, loc, LC_NAMELEN - 1);
		part[LC_NAMELEN - 1] = '\0';
	}

	/*
	 * If hasn't changed since last time, no need to do anything
	 */
	if (!strcmp(_cur_locale[cat], loc)) {
		return _cur_locale[cat];
	}

	if ( search_alias && loc[0] != '/' && ( _alias_val = __mbwc_locale_alias ( loc ) ) )
	{
	    strncpy ( _alias_name, loc, LC_NAMELEN - 1);
	    _alias_name[LC_NAMELEN - 1] = '\0';
	    loc = _alias_val;
	}
	
	/* _nativeloc and __mbwc_locale_alias could have returned a compound locale */
	if ( loc[0] == '/' )
	    if ( ! ( loc = get_part ( cat, loc ) ) )
		return 0;

	if ( cat == LC_NUMERIC ) {
	    if ( _set_tab_numeric ( loc ) != 0 )
		return 0;
	}
	else if ( cat == LC_CTYPE ) {
	    if ( _set_tab_ctype ( loc ) != 0 )
		return 0;
	}
	else {
		static const char *name[LC_ALL] = 
			{ "LC_CTYPE", 
			  "LC_NUMERIC",
			  "LC_TIME",
			  "LC_COLLATE",
			  "LC_MONETARY",
			  "LC_MESSAGES"
 			};
		if ( access (_fullocale(loc, name[cat]), R_OK) == -1 )
			return 0;

		if ( cat == LC_COLLATE )
		    _set_res_collate( loc );
	}

	return strcpy(_cur_locale[cat], loc);
}
