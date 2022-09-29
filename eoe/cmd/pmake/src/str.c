/*-
 * str.c --
 *	General utilites for handling strings.
 *
 * Copyright (c) 1988, 1989 by the Regents of the University of California
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any non-commercial purpose
 * and without fee is hereby granted, provided that the above copyright
 * notice appears in all copies.  The University of California,
 * Berkeley Softworks and Adam de Boor make no representations about
 * the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * Interface:
 *	Str_Concat	     	Concatenate two strings, placing some sort
 *	    	  	    	of separator between them and freeing
 *	    	  	    	the two strings, all this under the control
 *	    	  	    	of the STR_ flags given as the third arg.
 *
 *	Str_New	  	    	Duplicate a string and return the copy.
 *
 *	Str_FindSubstring   	Find a substring within a string (from
 *	    	  	    	original Sprite libc).
 *
 *	Str_Match   	    	Pattern match two strings.
 */
#if !defined(lint) && defined(keep_rcsid)
static char     *rcsid = "Id: str.c,v 1.22 89/11/14 13:44:07 adam Exp $ SPRITE (Berkeley)";
#endif lint

#include    "make.h"

/*-
 *-----------------------------------------------------------------------
 * Str_Concat  --
 *	Str_Concatenate and the two strings, inserting a space between them
 *	and/or freeing them if requested
 *
 * Results:
 *	the resulting string
 *
 * Side Effects:
 *	The strings s1 and s2 are free'd
 *-----------------------------------------------------------------------
 */
char *
Str_Concat (s1, s2, flags)
    char           *s1;		/* first string */
    char           *s2;		/* second string */
    int             flags;	/* flags governing Str_Concatenation */
{
    int             len;	/* total length */
    register char  *cp1,	/* pointer into s1 */
                   *cp2,	/* pointer into s2 */
                   *cp;		/* pointer into result */
    char           *result;	/* result string */

    /*
     * get the length of both strings 
     */
    for (cp1 = s1; *cp1; cp1++) {
	 /* void */ ;
    }
    for (cp2 = s2; *cp2; cp2++) {
	 /* void */ ;
    }

    len = (cp1 - s1) +
	(cp2 - s2) +
	    (flags & (STR_ADDSPACE | STR_ADDSLASH) ? 1 : 0) +
		1;

    result = malloc (len);

    for (cp1 = s1, cp = result; *cp1 != '\0'; cp++, cp1++) {
	*cp = *cp1;
    }

    if (flags & STR_ADDSPACE) {
	*cp++ = ' ';
    } else if (flags & STR_ADDSLASH) {
	*cp++ = '/';
    }

    for (cp2 = s2; *cp2 != '\0'; cp++, cp2++) {
	*cp = *cp2;
    }

    *cp = '\0';

    if (flags & STR_DOFREE) {
	free (s1);
	free (s2);
    }
    return (result);
}

/*-
 *-----------------------------------------------------------------------
 * Str_New  --
 *	Create a new unique copy of the given string
 *
 * Results:
 *	A pointer to the new copy of it
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
#ifndef sgi	/* same as strdup(3) */
char *
Str_New (str)
    char           *str;	/* string to duplicate */
{
    register char  *cp;		/* new space */

    cp = malloc (strlen (str) + 1);
    (void) strcpy (cp, str);
    return (cp);
}
#endif

static char
DoBackslash (c)
    char c;
{
    switch (c) {
	case 'n': return ('\n');
	case 't': return ('\t');
	case 'b': return ('\b');
	case 'r': return ('\r');
	case 'f': return ('\f');
	default:  return (c);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Str_BreakString --
 *	Fracture a string into an array of words, taking quotation marks
 *	into account. The string should have its leading 'breaks'
 *	characters removed.
 *
 * Results:
 *	Pointer to the array of pointers to the words. This array must
 *	be freed by the caller. To make life easier, the first word is
 *	always the value of the .PMAKE variable.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
char **
Str_BreakString (str, breaks, end, argcPtr)
    register char 	*str;	    	/* String to fracture */
    register char 	*breaks;    	/* Word delimiters */
    register char 	*end;	    	/* Characters to end on */
    int	    	  	*argcPtr;   	/* OUT: Place to stuff number of
					 * words */
{
    char            	*defargv[256]; 	/* Temporary argument vector.
					 * Big enough for most purposes. */
    char    	    	**argv;	    	/* Argv being built */
    int	    	    	maxargc;    	/* Length of argv */
    register int    	argc;	    	/* Count of words */
    char            	**av;	    	/* Returned vector */
    register char   	*tstr;	    	/* Pointer into tstring */
    char            	*tstring;	/* Temporary storage for the
					 * current word */
    
    argc = 1;
    argv = defargv;
    maxargc = sizeof(defargv)/sizeof(defargv[0]);
    argv[0] = Var_Value (".PMAKE", VAR_GLOBAL);

    tstr = tstring = malloc(strlen(str) + 2);
    while ((*str != '\0') && (index (end, *str) == (char *)NULL)) {
	if (index (breaks, *str) != (char *)NULL) {
	    *tstr++ = '\0';
	    argv[argc++] = Str_New(tstring);
	    while ((*str != '\0') &&
		   (index (breaks, *str) != (char *)NULL) &&
		   (index (end, *str) == (char *)NULL)) {
		       str++;
		   }
	    tstr = tstring;
	    /*
	     * Enlarge the argument vector, if necessary
	     */
	    if (argc == maxargc) {
		maxargc *= 2;
		if (argv == defargv) {
		    argv = (char **)malloc(maxargc*sizeof(char *));
		    bcopy(defargv, argv, sizeof(defargv));
		} else {
		    argv = (char **)realloc(argv,
					    maxargc*sizeof(char *));
		}
	    }
	} else if (*str == '"') {
	    str += 1;
	    while ((*str != '"') &&
		   (index (end, *str) == (char *)NULL)) {
		       if (*str == '\\') {
			   str += 1;
			   *tstr = DoBackslash(*str);
		       } else {
			   *tstr = *str;
		       }
		       str += 1;
		       tstr += 1;
		   }
		   
	    if (*str == '"') {
		str+=1;
	    }
	} else if (*str == '\'') {
	    str += 1;
	    while ((*str != '\'') &&
		   (index (end, *str) == (char *)NULL)) {
		       if (*str == '\\') {
			   str += 1;
			   *tstr = DoBackslash(*str);
		       } else {
			   *tstr = *str;
		       }
		       str += 1;
		       tstr += 1;
		   }
		   
	    if (*str == '\'') {
		str+=1;
	    }
	} else if (*str == '\\') {
	    str += 1;
	    *tstr = DoBackslash(*str);
	    str += 1;
	    tstr += 1;
	} else {
	    *tstr = *str;
	    tstr += 1;
	    str += 1;
	}
    }
    if (tstr != tstring) {
	/*
	 * If any word is left over, add it to the vector
	 */
	*tstr = '\0';
	argv[argc++] = Str_New(tstring);
    }
    argv[argc] = (char *) 0;
    *argcPtr = argc;
    if (argv == defargv) {
	av = (char **) malloc ((argc+1) * sizeof(char *));
	bcopy ((char *)argv, (char *)av, (argc + 1) * sizeof(char *));
    } else {
	/*
	 * Shrink vector to match actual number of args.
	 */
	av = (char **)realloc(argv, (argc+1) * sizeof(char *));
    }
    free(tstring);
    
    return av;
}

/*-
 *-----------------------------------------------------------------------
 * Str_FreeVec --
 *	Free a string vector returned by Str_BreakString. Frees all the
 *	strings in the vector and then frees the vector itself.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The blocks addressed by the vector are freed.
 *
 *-----------------------------------------------------------------------
 */
void
Str_FreeVec (count, vecPtr)
    register int  count;
    register char **vecPtr;
{
    for (count -= 1; count > 0; count -= 1) {
	free (vecPtr[count]);
    }
    free (vecPtr);
}
#ifndef Sprite

/*
 *----------------------------------------------------------------------
 * Str_FindSubstring --
 *	See if a string contains a particular substring.
 *
 * Results:
 *	If string contains substring, the return value is the
 *	location of the first matching instance of substring
 *	in string.  If string doesn't contain substring, the
 *	return value is NULL.  Matching is done on an exact
 *	character-for-character basis with no wildcards or special
 *	characters.
 *
 * Side effects:
 *	None.
 *----------------------------------------------------------------------
 */
char *
Str_FindSubstring(string, substring)
    register char *string;	/* String to search. */
    char *substring;		/* Substring to try to find in string. */
{
    register char *a, *b;

    /*
     * First scan quickly through the two strings looking for a
     * single-character match.  When it's found, then compare the
     * rest of the substring.
     */
    
    b = substring;
    for ( ; *string != 0; string += 1) {
	if (*string != *b) {
	    continue;
	}
	a = string;
	while (TRUE) {
	    if (*b == 0) {
		return string;
	    }
	    if (*a++ != *b++) {
		break;
	    }
	}
	b = substring;
    }
    return (char *) NULL;
}

#endif /* !Sprite */

/*
 *----------------------------------------------------------------------
 *
 * Str_Match --
 *
 *      See if a particular string matches a particular pattern.
 *
 * Results:
 *      Non-zero is returned if string matches pattern, 0 otherwise.
 *      The matching operation permits the following special characters
 *      in the pattern: *?\[] (see the man page for details on what
 *      these mean).
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Str_Match(string, pattern)
    register char *string;              /* String. */
    register char *pattern;             /* Pattern, which may contain
                                         * special characters.
                                         */
{
    char c2;

    while (1) {
        /* See if we're at the end of both the pattern and the string.
         * If, we succeeded.  If we're at the end of the pattern but
         * not at the end of the string, we failed.
         */
        
        if (*pattern == 0) {
            if (*string == 0) {
                return 1;
            } else {
                return 0;
            }
        }
        if ((*string == 0) && (*pattern != '*')) {
            return 0;
        }

        /* Check for a "*" as the next pattern character.  It matches
         * any substring.  We handle this by calling ourselves
         * recursively for each postfix of string, until either we
         * match or we reach the end of the string.
         */
        
        if (*pattern == '*') {
            pattern += 1;
            if (*pattern == 0) {
                return 1;
            }
            while (*string != 0) {
                if (Str_Match(string, pattern)) {
                    return 1;
                }
                string += 1;
            }
            return 0;
        }
    
        /* Check for a "?" as the next pattern character.  It matches
         * any single character.
         */

        if (*pattern == '?') {
            goto thisCharOK;
        }

        /* Check for a "[" as the next pattern character.  It is followed
         * by a list of characters that are acceptable, or by a range
         * (two characters separated by "-").
         */
        
        if (*pattern == '[') {
            pattern += 1;
            while (1) {
                if ((*pattern == ']') || (*pattern == 0)) {
                    return 0;
                }
                if (*pattern == *string) {
                    break;
                }
                if (pattern[1] == '-') {
                    c2 = pattern[2];
                    if (c2 == 0) {
                        return 0;
                    }
                    if ((*pattern <= *string) && (c2 >= *string)) {
                        break;
                    }
                    if ((*pattern >= *string) && (c2 <= *string)) {
                        break;
                    }
                    pattern += 2;
                }
                pattern += 1;
            }
            while ((*pattern != ']') && (*pattern != 0)) {
                pattern += 1;
            }
            goto thisCharOK;
        }
    
        /* If the next pattern character is '/', just strip off the '/'
         * so we do exact matching on the character that follows.
         */
        
        if (*pattern == '\\') {
            pattern += 1;
            if (*pattern == 0) {
                return 0;
            }
        }

        /* There's no special character.  Just make sure that the next
         * characters of each string match.
         */
        
        if (*pattern != *string) {
            return 0;
        }

        thisCharOK: pattern += 1;
        string += 1;
    }
}
