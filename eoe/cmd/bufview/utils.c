/*
 *  Buffer cache viewing program based on top.
 *
 *  This program may be freely redistributed,
 *  but this entire comment MUST remain intact.
 *
 *  Copyright (c) 1984, 1989, William LeFebvre, Rice University
 *  Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
 */

/*
 *  This file contains various handy utilities used by top.
 */

#include "bv.h"
#include "os.h"

int
atoiwi(char *str)
{
	register int len;

	len = strlen(str);
	if (len != 0)
	{
		if (strncmp(str, "infinity", len) == 0 ||
		    strncmp(str, "all",      len) == 0 ||
		    strncmp(str, "maximum",  len) == 0)
		{
			return(Infinity);
		}
		else
		if (str[0] == '-') {
			return(Invalid);
		}
		else
		{
			return(atoi(str));
		}
	}
	return(0);
}

/*
 *  itoa - convert integer (decimal) to ascii string for positive numbers
 *  	   only (we don't bother with negative numbers since we know we
 *	   don't use them).
 */

#define ITOARRAY	24
char *
itoa(uint64_t val)
{
	static char array[ITOARRAY][8];
	static int aindex = 0;
	char *ptr;

	ptr = array[aindex];
	aindex = (aindex + 1) % 8;

	ptr += ITOARRAY;
	*--ptr = '\0';
	if (val == 0)
	{
		*--ptr = '0';
	}
	else while (val != 0)
	{
		*--ptr = (val % 10) + '0';
		val /= 10;
	}
	return(ptr);
}

/*
 *  strecpy(to, from) - copy string "from" into "to" and return a pointer
 *	to the END of the string "to".
 */

char *
strecpy(char *to, char *from)
{
	while ((*to++ = *from++) != '\0')
		;
	return(--to);
}

/*
 * string_index(string, array) - find string in array and return index
 * Ariel: made it more friendly allowing to compare string prefixes
 */

int
string_index(char *string, char **array)
{
	int	i = 0;
	int	len = strlen(string);

	while (*array != NULL)
	{
		if (strncmp(string, *array, len) == 0)
		{
			return i;
		}
		array++;
		i++;
	}
	return -1;
}

/*
 * argparse(line, cntp) - parse arguments in string "line", separating them
 *	out into an argv-like array, and setting *cntp to the number of
 *	arguments encountered.  This is a simple parser that doesn't understand
 *	squat about quotes.
 */

char **
argparse(char *line, int *cntp)
{
    register char *from;
    register char *to;
    register int cnt;
    register int ch;
    int length;
    int lastch;
    register char **argv;
    char **argarray;
    char *args;

    /* unfortunately, the only real way to do this is to go thru the
       input string twice. */

    /* step thru the string counting the white space sections */
    from = line;
    lastch = cnt = length = 0;
    while ((ch = *from++) != '\0')
    {
	length++;
	if (ch == ' ' && lastch != ' ')
	{
	    cnt++;
	}
	lastch = ch;
    }

    /* add three to the count:  one for the initial "dummy" argument,
       one for the last argument and one for NULL */
    cnt += 3;

    /* allocate a char * array to hold the pointers */
    argarray = (char **)malloc(cnt * sizeof(char *));

    /* allocate another array to hold the strings themselves */
    args = (char *)malloc(length+2);

    /* initialization for main loop */
    from = line;
    to = args;
    argv = argarray;
    lastch = '\0';

    /* create a dummy argument to keep getopt happy */
    *argv++ = to;
    *to++ = '\0';
    cnt = 2;

    /* now build argv while copying characters */
    *argv++ = to;
    while ((ch = *from++) != '\0')
    {
	if (ch != ' ')
	{
	    if (lastch == ' ')
	    {
		*to++ = '\0';
		*argv++ = to;
		cnt++;
	    }
	    *to++ = ch;
	}
	lastch = ch;
    }
    *to++ = '\0';

    /* set cntp and return the allocated array */
    *cntp = cnt;
    return(argarray);
}

# define	MAXKBUF		16
# define	kilo		1024
# define	mega		(kilo*kilo)
# define	giga		(kilo*kilo*kilo)

char *
konvert(uint64_t val)
{
	static char retarray[MAXKBUF][8];
	static int index = 0;
	char *p;

	p = retarray[index];
	index = (index + 1) % 8;

	if (val >= giga)
		sprintf(p, "%5.1fG", ((float)val) / giga);
	else if (val >= mega)
		sprintf(p, "%5.1fM", ((float)val) / mega);
	else if (val >= kilo)
		sprintf(p, "%5.1fK", ((float)val) / kilo);
	else
		sprintf(p, "%6lld", val);
	return(p);
}
