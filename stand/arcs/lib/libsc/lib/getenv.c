#ident	"lib/libsc/lib/getenv.c:  $Revision: 1.23 $"

/* 
 * getenv.c - programming interface to get and set environment variables
 */

#include "stringlist.h"
#include <libsc.h>
#include <ctype.h>
#include <libsc_internal.h>

char **environ;
struct string_list environ_str;
struct string_list argv_str;
static char *nvmatch(const char *, char *);

/*
 * setenv - normal setenv entry point, cannot override readonly variables
 */
int
setenv (char *name, char *value)
{
#if DEBUG_X
	extern int Debug;

	if (Debug) {
	    cpu_errputs("setenv(\"");
	    cpu_errputs(name? name: "<0>");
	    cpu_errputs("\")=");
	    cpu_errputs(value? value: "<0>");
	    cpu_errputs("\n\r");
	}
#endif
    return _setenv (name, value, 0);
}

/*
 * syssetenv - system setenv entry point, can override readonly variables
 */
int
syssetenv (char *name, char *value)
{
    return _setenv (name, value, 1);
}

/*
 *	getenv(name)
 *	returns ptr to value associated with name, if any, else NULL
 */
char *
getenv(const char *name)
{
	register char **p = environ;
	register char *v = NULL;

	if (p)
		while (*p != NULL)
			if ((v = nvmatch(name, *p++)) != NULL)
				break;
#if DEBUG
	{
	    static char lastname[128];
	    if (Debug && strcmp(name,lastname))
	    {
		cpu_errputs("\tgetenv(\"");
		cpu_errputs(name? name: "<0>");
		cpu_errputs("\")=");
		cpu_errputs(v? v: "<0>");
		cpu_errputs(" ");
		strcpy(lastname, name);
	    }
	}
#endif
	return(v);
}

/*
 *	s1 is either name, or name=value
 *	s2 is either name, or name=value
 *	if names match, return value of s2, else NULL
 *	if s1 and s2 are equal, return pointer to empty string
 *	used for environment searching
 */

static char *
nvmatch(const char *s1, char *s2)
{
	while (tolower(*s1) == tolower(*s2++))
		if (*s1 == '=' || *s1++ == '\0')
			return(s2);
	if (*s1 == '\0' && *(s2-1) == '=')
		return(s2);
	return(NULL);
}

/*
 * initenv - copy environment someplace safe so we can
 * 	play with it.
 */
void
initenv(char ***envp)
{
    environ = (char **)_copystrings(*envp, &environ_str);
    *envp = environ;
}


/*
 * initargv - copy argv variables someplace safe
 */
void
initargv(long *ac, char ***av)
{
    init_str(&argv_str);
    while ((*ac)--) {
	if (**av) {
	    new_str1(**av, &argv_str);
	}
	++(*av);
    }
    *ac = argv_str.strcnt;
    *av = argv_str.strptrs;
}

/*
 * getargv - search for value of name in argv stringlist
 */
char *
getargv(char *name)
{
    register int i = argv_str.strcnt;
    register char **p = argv_str.strptrs;
    register char *v;

    if (p)
	while (i--)
	    if ((*p != NULL) && ((v = nvmatch(name, *p++)) != NULL))
		return(v);
    return(NULL);
}
