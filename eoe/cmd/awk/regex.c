/*
 * regex.c -- Support code for the POSIX extended regular expression
 *	handling in awk.  We've completely rewritten portions of the
 *	awk code to simplify regular expression handling; the old,
 *	obsolete code lives in b.c and run.c
 */

#if !defined(OLD_REGEXP)

#include <stdio.h>
#include <regex.h>
#include <malloc.h>
#include <pfmt.h>

#include "awk.h"

#define NFA	20

/* Global variables */
fa	*fatab[NFA];
int	nfatab = 0;
extern void nospace(char *);

/* Forward declarations */
fa *mkdfa(uchar *s, int anchor);
void freefa(fa*);
void nospace(char *);


/*
 * makedfa -- Front-end function for mkdfa.  Maintains a cache of
 *	fa's and attempts to satisfy the request for an fa from the
 *	the cache first.
 */

fa *makedfa(s, anchor)  /* returns dfa for reg expr s */
        uchar *s;
        int anchor;
{
        int i, use, nuse;
        fa *fa;

        if (compile_time) {      /* a constant for sure */
		if ((fa = malloc(sizeof(fa))) == NULL) 
			nospace("makedfa");
                return mkdfa(s, anchor);
	}

        for (i = 0; i < nfatab; i++) {    /* is it there already? */
                if (fatab[i]->anchor == anchor && 
		    strcmp(fatab[i]->restr,s) == 0) {
                        fatab[i]->use++;
                        return fatab[i];
		}
        }

        fa = mkdfa(s, anchor);
        if (nfatab < NFA) {     /* room for another */
                fatab[nfatab] = fa;
                fatab[nfatab]->use = 1;
                nfatab++;
                return fa;
        }
        use = fatab[0]->use;    /* replace least-recently used */
        nuse = 0;
        for (i = 1; i < nfatab; i++)
                if (fatab[i]->use < use) {
                        use = fatab[i]->use;
                        nuse = i;
                }
        freefa(fatab[nuse]);
        fatab[nuse] = fa;
        fa->use = 1;
        return fa;
}


/*
 * mkdfa -- Actually generate the deterministic finite automaton for
 * 	the regular expression parsing.  This actually does all the
 *	work.
 */

fa *mkdfa(s, anchor)
    uchar *s;		/* The regular expression string */
    int anchor;		/* A no-op for backward compatibility ? */
{
	fa *pfa;

	if ((pfa = malloc(sizeof(struct fa))) == NULL)
		nospace("mkdfa");

	if (regcomp(&pfa->regex, s, REG_EXTENDED) != 0) {
		fprintf(stderr,"Regular expression compiler failed\n");
		exit(1);
	}

	pfa->anchor = anchor;
	pfa->restr  = tostring(s);

	return pfa;
}


/*
 * freefa -- Free an fa structure allocated by mkdfa.
 */

void freefa(fa *fa)
{
	xfree(fa->restr);
	xfree(fa);
}


/*
 * match -- Return a 1 if the given regexpr matches something in the
 *	string, 0 otherwise.
 */

int 
match(f, p)
    fa *f;
    char *p;
{
	int result;

	result = regexec(&f->regex, p, 0, NULL, 0);
	if (result == 0)
		return 1;
	else
		return 0;
}


char *patbeg;
char *patend;
int  patlen;


/*
 * pmatch -- If the regular expression matches in the given string,
 *	pmatch sets the patbeg, patend, and patlen variables and returns
 *	a 1.  patbeg points to the first character in the matched substring.
 *	patend points to the first character after the end of the matched
 *	substring, and patlen is the total number of character in the 
 *	matched substring.  It is possible to match an empty substring
 *	(this often occurs when the '*' character is used), so patlen
 *	can be equal to zero.
 *	If no match is found, pmatch returns 0.
 */ 

int 
pmatch(f, p, beginning)
    fa *f;
    char *p;
    int beginning;		/* Indicates that we're at the beginning of
				 * of the string, so '^' should match */
{
	regmatch_t pmatch;
 	int result;

	result = regexec(&f->regex, p, 1, &pmatch, 
			 beginning ? 0 : REG_NOTBOL);
	if (result == 0) {
		patbeg = p + pmatch.rm_so;
		patlen = pmatch.rm_eo - pmatch.rm_so;
		patend = p + pmatch.rm_eo;

		return 1;
	} else {
		patbeg = NULL;
		patend = NULL;
		patlen = -1;

		return 0;
	}
} 


/*
 * nematch -- scans the given string for the first match.  Unlike
 *	pmatch, nematch only succeeds if the number of characters
 *	matched (patlen) is greater than 0.  This makes it useful
 *	for situations like decomposing a line into a set of records
 *	based on a regular expression (see recfldbld in lib.c).
 *  	This routine returns a 1 if a match is found, and 0 if no
 *	match is found.  Since it calls pmatch, it also sets the
 *	global variables patbeg, patlen, and patend.
 */

int
nematch(f, p, beginning)
    fa *f;		/* The regular expression to use in matching */
    char *p;		/* The string to match against */
    int beginning;	/* A flag indicating whether the beginning of the
			 * string is also the beginning of the logical 
			 * line. */	
{
	do {
		int result = pmatch(f, p, beginning);
		if (result == 1) {
			if (patlen > 0) {
				return 1;
			} else {
				p = patbeg + 1;
				beginning = 0;
			}
		} else {
			return 0;
		}
	} while (*p);

	return 0;
}


void
nospace(s)
    char *s;
{
    error(MM_ERROR, ":5:Regular expression too big: out of space in %s", s);
}


#endif /* !defined(OLD_REGEXP) */
