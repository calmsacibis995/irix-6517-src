#ident "$Revision: 1.1 $"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parse.h"

/*
 * Skip over a field e.g. if p == "aaa:bbb", return "bbb".
 */
char *
skipfield (char *p, int separator)
{
    while (*p != separator && *p != '\n' && *p != '\0')
	++p;
    if (*p == '\n')
	*p = '\0';
    else if (*p != '\0')
	*p++ = '\0';
    return (p);
}

void
parse_free (char **words)
{
    char **tmp;

    if (words == (char **) NULL)
	return;

    tmp = words;
    while (*tmp != (char *) NULL)
	free (*tmp++);
    free (words);
}

static void
copy_down (char *a, int interval)
{
    char *b = a + interval;

    while (*a++ = *b++)
	/* empty loop */;
}

/*
 * Break up a line into words separated by one or more delimiter characters.
 * A character is a delimiter if isdelim returns TRUE. If isdelim is NULL,
 * no character is treated as a delimiter character. Adjacent delimiter
 * characters are treated as a single delimiter character.
 *
 * Words may be grouped into quoted strings. A character is a quote
 * character if isquote returns TRUE. If isquote is NULL, no character
 * is treated as a quote character. Adjacent quoted strings are combined.
 *
 * Returns a list of substrings of `line' terminated by a NULL pointer.
 * When finished with that list, parse_free() it.
 *
 * Returns a NULL pointer when an error is detected.
 */
char **
parse_line (const char *line, pl_quote_func isquote, pl_delim_func isdelim,
	    unsigned int *countp)
{
    unsigned int i, nwords;
    char quotechar, *begin, *end, *copy, **words = NULL;
    enum {S_EATDELIM, S_STRING, S_QUOTE, S_COPY, S_DONE, S_ERROR} state;

    /*
     * check for erroneous input
     */
    if (line == (char *) NULL)
	return (words);

    /*
     * make working copy of input
     */
    copy = malloc (strlen (line) + 1);
    if (copy == (char *) NULL)
	return (words);
    strcpy (copy, line);

    /*
     * allocate and initialize array of possible words
     */
    nwords = (strlen (line) + 1) / 2 + 1;
    words = (char **) malloc (nwords * sizeof (*words));
    if (words == (char **) NULL)
    {
	free (copy);
	return words;
    }
    for (i = 0; i < nwords; i++)
	words[i] = (char *) NULL;
    nwords = 0;

    /*
     * enter the machine
     */
    end = copy;
    state = S_EATDELIM;
    do
    {
	switch (state)
	{
	    case S_EATDELIM:
		while (isdelim && (*isdelim) (*end))
		    end++;
		begin = end;
		state = (*end == '\0' ? S_DONE : S_STRING);
		break;
	    case S_STRING:
		if (isquote && (*isquote) (*end))
		{
		    quotechar = *end;
		    copy_down (end, 1);
		    state = S_QUOTE;
		}
		else if (*end == '\0')
		{
		    state = S_COPY;
		    break;
		}
		else
		{
		    if (isdelim && (*isdelim) (*end))
		    {
			*end++ = '\0';
			state = S_COPY;
		    }
		    else
			end++;
		}
		break;
	    case S_QUOTE:
		while (*end != '\0' && *end != quotechar)
		    end++;
		if (*end == '\0')
		    state = S_ERROR;
		else
		{
		    copy_down (end, 1);
		    state = S_STRING;
		}
		break;
	    case S_COPY:
		words[nwords] = malloc (strlen (begin) + 1);
		if (words[nwords] == (char *) NULL)
		    state = S_ERROR;
		else
		{
		    strcpy(words[nwords++], begin);
		    state = S_EATDELIM;
		}
		break;
	    case S_ERROR:
		parse_free (words);
		words = (char **) NULL;
		state = S_DONE;
		break;
	}
    } while (state != S_DONE);

    if (countp)
	    *countp = nwords;
    free (copy);
    return (words);
}
