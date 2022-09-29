#ident	"lib/libsc/lib/bootname.c:  $Revision: 1.7 $"

/*
 * bootname.c -- parse environment vars to determine bootfile name
 */

#include <libsc.h>
#include <parser.h>
#include <ctype.h>

char *env_get_var(char **, char *);
extern int _prom;

/*
 * make_bootfile - create a reasonable bootfile name 
 */

char *
make_bootfile(int rewind)
{
    static char bootfile[LINESIZE];
    static char *sp, *osl;
    static char savesp[128], saveosl[128];
    char spbuf[128], oslbuf[128];
    char *newsp, *newosl;
    char *bp = bootfile;

    /* reset to beginning of environment variable list
     */
    if (rewind) {
	if (_prom) {
		sp = getenv ("SystemPartition");
		osl = getenv ("OSLoader");
	}
	else {
		sp = getenv ("OSLoadPartition");
		osl = getenv ("OSLoadFileName");
	}
	*savesp = '\0';
	*saveosl = '\0';
    }

    /* get the next path in the varialble
     */
    newsp = env_get_var (&sp, spbuf);
    newosl = env_get_var (&osl, oslbuf);

    /* if we've hit the end of all variables, return NULL
     */
    if (newsp == NULL && newosl == NULL)
	return NULL;

    /* replace any empty portions of the path with most recent 
     * value.  Save new value.
     */
    if (newsp == NULL)
	newsp = savesp;
    else
	strcpy (savesp, newsp);

    if (newosl == NULL)
	newosl = saveosl;
    else
	strcpy (saveosl, newosl);
    
    if (newsp) {
	while (*newsp != '\0')
	    *bp++ = *newsp++;
	*bp++ = '/';
    }
    
    if (newosl) {
	while (*newosl != '\0')
	    *bp++ = *newosl++;
	*bp = '\0';
    }

    return bootfile;
}

/*
 * env_get_var -- break line into variables
 */
char *
env_get_var(char **cpp, char *buf)
{
	register char *wp = buf;
	register char c;
	char *cp;

	if (!(cp = *cpp) || *cp == '\0')
		return NULL;

	while (isspace(*cp))
		cp++;

	if (*cp == '\0') {
		*cpp = cp;
		*wp = 0;
		return NULL;
	}

	while ((c = *cp) && !isspace(c)) {

		if (c == ';') {
			cp++;		/* skip over ';' and stop */
			break;
		}
		    
		switch (c) {

		case '"':
		case '\'':
			cp++;	/* skip opening quote */

			while (*cp && *cp != c)
				*wp++ = *cp++;

			if (*cp == c) /* skip closing quote */
				cp++;
			break;

		default:
			*wp++ = *cp++;
			break;
		}
	}

	while (isspace(*cp))
		cp++;

	*cpp = cp;
	*wp = 0;
	return(buf);
}
