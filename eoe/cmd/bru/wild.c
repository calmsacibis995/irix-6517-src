/************************************************************************
 *									*
 *			Copyright (c) 1984, Fred Fish			*
 *			     All Rights Reserved			*
 *									*
 *	This software and/or documentation is protected by U.S.		*
 *	Copyright Law (Title 17 United States Code).  Unauthorized	*
 *	reproduction and/or sales may result in imprisonment of up	*
 *	to 1 year and fines of up to $10,000 (17 USC 506).		*
 *	Copyright infringers may also be subject to civil liability.	*
 *									*
 ************************************************************************
 */


/*
 *  FILE
 *
 *	wild.c    routines to do filename generation pattern matching
 *
 *  SCCS
 *
 *	@(#)wild.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Contains routines to do filename generation pattern matching
 *	in the same manner as the standard UNIX shell.
 *
 */

#include "autoconfig.h"

#include <stdio.h>

#if (unix || xenix)
#  include <sys/types.h>
#else
#  include "sys.h"
#endif

#include "typedefs.h"
#include "dbug.h"
#include "manifest.h"
#include "errors.h"

#define ASTERISK '*'		/* The '*' metacharacter */
#define QUESTION '?'		/* The '?' metacharacter */
#define LEFT_BRACKET '['	/* The '[' metacharacter */
#define RIGHT_BRACKET ']'	/* The ']' metacharacter */

#define IS_OCTAL(ch) (ch >= '0' && ch <= '7')

static BOOLEAN do_list ();
static char nextch ();
static VOID list_parse ();
static BOOLEAN iswild ();

extern VOID bru_error ();	/* Report an error to user */
extern VOID done ();


/*
 *  FUNCTION
 *
 *	wild   test string for wildcard match
 *
 *  SYNOPSIS
 *
 *	BOOLEAN wild (string, pattern)
 *	register char *string;
 *	register char *pattern;
 *
 *  DESCRIPTION
 *
 *	Test string for match using pattern.  The pattern may
 *	contain the normal shell metacharacters for pattern
 *	matching.  The '*' character matches any string,
 *	including the null string.  The '?' character matches
 *	any single character.  A list of characters enclosed
 *	in '[' and ']' matches any character in the list.
 *	If the first character following the beginning '['
 *	is a '!' then any character not in the list is matched.
 *
 *	As a special extension to shell pattern matching, a '!'
 *	character at the start of the pattern is taken to mean
 *	reverse the sense of the match (I.E. return true for
 *	all strings NOT matching the pattern).  The internal
 *	routine iswild() is called on the rest of the pattern.
 *	
 */


/*
 *  PSEUDO CODE
 *
 *	Begin wild
 *	End wild
 *
 */

BOOLEAN wild (string, pattern)
register char *string;
register char *pattern;
{
    register BOOLEAN match;

    DBUG_ENTER ("wild");
    DBUG_PRINT ("wild", ("match \"%s\" and \"%s\"", string, pattern));
    if (*pattern == '!') {
	match = !iswild (string, ++pattern);
    } else {
	match = iswild (string, pattern);
    }
    DBUG_PRINT ("wild", ("returns %d", match));
    DBUG_RETURN (match);
}


/*
 *  FUNCTION
 *
 *	iswild   test string for wildcard match
 *
 *  SYNOPSIS
 *
 *	static BOOLEAN iswild (string, pattern)
 *	register char *string;
 *	register char *pattern;
 *
 *  DESCRIPTION
 *
 *	Test string for match using pattern.  The pattern may
 *	contain the normal shell metacharacters for pattern
 *	matching.  The '*' character matches any string,
 *	including the null string.  The '?' character matches
 *	any single character.  A list of characters enclosed
 *	in '[' and ']' matches any character in the list.
 *	If the first character following the beginning '['
 *	is a '!' then any character not in the list is matched.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin iswild
 *	    Switch on type of pattern character
 *		Case ASTERISK:
 *		    Attempt to match asterisk
 *		    Break
 *		Case QUESTION MARK:
 *		    Attempt to match question mark
 *		    Break
 *		Case EOS:
 *		    Match is result of EOS on string test
 *		    Break
 *		Case default:
 *		    If explicit match then
 *			Match is result of submatch
 *		    Else
 *			Match is FALSE
 *		    End if
 *		    Break
 *	    End switch
 *	    Return result of match test
 *	End iswild
 *
 */

static BOOLEAN iswild (string, pattern)
register char *string;
register char *pattern;
{
    register BOOLEAN match;

    DBUG_ENTER ("iswild");
    DBUG_PRINT ("iswild", ("match \"%s\" and \"%s\"", string, pattern));
    match = FALSE;
    switch (*pattern) {
	case ASTERISK:
	    pattern++;
	    do {
		match = iswild (string, pattern);
	    } while (!match && *string++ != EOS);
	    break;
	case QUESTION:
	    if (*string != EOS) {
		match = iswild (++string, ++pattern);
	    }
	    break;
	case EOS:
	    if (*string == EOS) {
		match = TRUE;
	    }
	    break;
	case LEFT_BRACKET:
	    if (*string != EOS) {
		match = do_list (string, pattern);
	    }
	    break;
	default:
	    if (*string++ == *pattern++) {
		match = iswild (string, pattern);
	    } else {
		match = FALSE;
	    }
	    break;
    }
    DBUG_PRINT ("iswild", ("returns %d", match));
    DBUG_RETURN (match);
}


/*
 *  FUNCTION
 *
 *	do_list    process a list and following substring
 *
 *  SYNOPSIS
 *
 *	static BOOLEAN do_list (string, pattern)
 *	register char *string;
 *	register char *pattern;
 *
 *  DESCRIPTION
 *
 *	Called when a list is found in the pattern.  Returns
 *	TRUE if the current character matches the list and
 *	the remaining substring matches the remaining pattern.
 *
 *	Returns FALSE if either the current character fails to
 *	match the list or the list matches but the remaining
 *	substring and subpattern's don't.
 *
 *  RESTRICTIONS
 *
 *	The mechanism used to match characters in an inclusive
 *	pair (I.E. [a-d]) may not be portable to machines
 *	in which the native character set is not ASCII.
 *
 *	The rules implemented here are:
 *
 *		(1)	The backslash character may be
 *			used to quote any special character.
 *			I.E.  "\]" and "\-" anywhere in list,
 *			or "\!" at start of list.
 *
 *		(2)	The sequence \nnn becomes the character
 *			given by nnn (in octal).
 *
 *		(3)	Any non-escaped ']' marks the end of list.
 *
 *		(4)	A list beginning with the special character
 *			'!' matches any character NOT in list.
 *			The '!' character is only special if it
 *			is the first character in the list.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin do_list
 *	    Default result is no match
 *	    Skip over the opening left bracket
 *	    If the next pattern character is a '!' then
 *		List match gives FALSE
 *		Skip over the '!' character
 *	    Else
 *		List match gives TRUE
 *	    End if
 *	    While not at closing bracket or EOS
 *		Get lower and upper bounds
 *		If character in bounds then
 *		    Result is same as sense flag.
 *		    Skip over rest of list
 *		End if
 *	    End while
 *	    If match found then
 *		If not at end of pattern then
 *		    Call iswild with rest of pattern
 *		End if
 *	    End if
 *	    Return match result
 *	End do_list
 *
 */

static BOOLEAN do_list (string, pattern)
register char *string;
char *pattern;
{
    register BOOLEAN match;
    register BOOLEAN if_found;
    register BOOLEAN if_not_found;
    auto char lower;
    auto char upper;

    DBUG_ENTER ("do_list");
    pattern++;
    if (*pattern == '!') {
	if_found = FALSE;
	if_not_found = TRUE;
	DBUG_PRINT ("iswild", ("match sense reversed"));
	pattern++;
    } else {
	if_found = TRUE;
	if_not_found = FALSE;
    }
    match = if_not_found;
    while (*pattern != ']' && *pattern != EOS) {
	list_parse (&pattern, &lower, &upper);
	DBUG_PRINT ("iswild", ("range is %3.3o-%3.3o", lower, upper));
	DBUG_PRINT ("iswild", ("test for match with %3.3o", *string));
	if (*string >= lower && *string <= upper) {
	    match = if_found;
	    while (*pattern != ']' && *pattern != EOS) {pattern++;}
	}
    }
    if (*pattern++ != ']') {
	bru_error (ERR_CCLASS);
	done ();
    } else {
	if (match) {
	    match = iswild (++string, pattern);
	}
    }
    DBUG_RETURN (match);
}


/*
 *  FUNCTION
 *
 *	list_parse    parse part of list into lower and upper bounds
 *
 *  SYNOPSIS
 *
 *	static VOID list_parse (patp, lowp, highp)
 *	char **patp;
 *	char *lowp;
 *	char *highp;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a pattern pointer (patp), pointer to
 *	a place to store lower bound (lowp), and pointer to a
 *	place to store upper bound (highp), parses part of
 *	the list, updating the pattern pointer in the process.
 *
 *	For list characters which are not part of a range,
 *	the lower and upper bounds are set to that character.
 *
 */

static VOID list_parse (patp, lowp, highp)
char **patp;
char *lowp;
char *highp;
{
    DBUG_ENTER ("list_parse");
    DBUG_PRINT ("iswild", ("parse class at %s", *patp));
    *lowp = nextch (patp);
    if (**patp == '-') {
	(*patp)++;
	*highp = nextch (patp);
    } else {
	*highp = *lowp;
    }
    DBUG_VOID_RETURN;
}


/*
 *  FUNCTION
 *
 *	nextch    determine next character in a pattern
 *
 *  SYNOPSIS
 *
 *	static char nextch (patp)
 *	char **patp;
 *
 *  DESCRIPTION
 *
 *	Given pointer to a pointer to a pattern, uses the pattern
 *	pointer to determine the next character in the pattern,
 *	subject to translation of backslash-char and backslash-octal
 *	sequences.
 *
 *	The character pointer is updated to point at the next pattern
 *	character to be processed.
 *
 */

static char nextch (patp)
char **patp;
{
    register char ch;
    register int chsum;
    register int count;

    DBUG_ENTER ("nextch");
    ch = *(*patp)++;
    if (ch == '\\') {
	ch = *(*patp)++;
	if (IS_OCTAL (ch)) {
	    chsum = 0;
	    for (count = 0; count < 3 && IS_OCTAL (ch); count++) {
		chsum <<= 3;
		chsum += ch - '0';
		ch = *(*patp)++;
	    }
	    (*patp)--;
	    ch = (char) chsum;
	}
    }
    DBUG_RETURN (ch);
}
