/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char RCSid[] = 
"$Id: lookup.c,v 1.3 1995/09/22 07:37:10 ack Exp $";

static char sccsid[] = "@(#)lookup.c	5.1 (Berkeley) 6/6/85";

static char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#include "defs.h"

	/* symbol types */
#define VAR	1
#define CONST	2

struct syment {
	int	s_type;
	char	*s_name;
	struct	namelist *s_value;
	struct	syment *s_next;
};

static struct syment *hashtab[HASHSIZE];

/*
 * Define a variable from a command line argument.
 */
#ifdef sgi
void
#endif
define(name)
	char *name;
{
	register char *cp, *s;
	register struct namelist *nl;
	struct namelist *value;

	debugmsg(DM_CALL, "define(%s)", name);

	cp = strchr(name, '=');
	if (cp == NULL)
		value = NULL;
	else if (cp[1] == '\0') {
		*cp = '\0';
		value = NULL;
	} else if (cp[1] != '(') {
		*cp++ = '\0';
		value = makenl(cp);
	} else {
		value = NULL;
		nl = NULL;
		*cp++ = '\0';
		do
			cp++;
		while (*cp == ' ' || *cp == '\t');
		for (s = cp; ; s++) {
			switch (*s) {
			case ')':
				*s = '\0';
			case '\0':
				break;
			case ' ':
			case '\t':
				*s++ = '\0';
				while (*s == ' ' || *s == '\t')
					s++;
				if (*s == ')')
					*s = '\0';
				break;
			default:
				continue;
			}
			if (nl == NULL)
				value = nl = makenl(cp);
			else {
				nl->n_next = makenl(cp);
				nl = nl->n_next;
			}
			if (*s == '\0')
				break;
			cp = s;
		}
	}
	(void) lookup(name, REPLACE, value);
}

/*
 * Lookup name in the table and return a pointer to it.
 * LOOKUP - just do lookup, return NULL if not found.
 * INSERT - insert name with value, error if already defined.
 * REPLACE - insert or replace name with value.
 */

struct namelist *
lookup(name, action, value)	/* %% in name.  Ignore quotas in name */
	char *name;
	int action;
	struct namelist *value;
{
	register unsigned n;
	register char *cp;
	register struct syment *s;
	char ebuf[BUFSIZ];

	debugmsg(DM_CALL, "lookup(%s, %d, %x)", name, action, value);

	n = 0;
	for (cp = name; *cp; )
		n += *cp++;
	n %= HASHSIZE;

	for (s = hashtab[n]; s != NULL; s = s->s_next) {
		if (strcmp(name, s->s_name))
			continue;
		if (action != LOOKUP) {
			if (action != INSERT || s->s_type != CONST) {
				(void) sprintf(ebuf, "%s redefined", name);
				yyerror(ebuf);
			}
		}
		return(s->s_value);
	}

	if (action == LOOKUP) {
		(void) sprintf(ebuf, "%s undefined", name);
		yyerror(ebuf);
		return(NULL);
	}

	s = ALLOC(syment);
	s->s_next = hashtab[n];
	hashtab[n] = s;
	s->s_type = action == INSERT ? VAR : CONST;
	s->s_name = name;
	s->s_value = value;

	return(value);
}
