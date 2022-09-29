#ident	"lib/libsc/lib/kernname.c:  $Revision: 1.3 $"

/*
 * kernname.c -- parse environment vars to determine kernel filename
 */

#include <libsc.h>
#include <parser.h>
#include <ctype.h>

char *env_get_var(char **, char *);

/*
 * make_kernfile - create a reasonable kernel filename 
 */

char *
make_kernfile(int rewind)
{
    static char kernfile[LINESIZE];
    static char lpbuf[64], lfbuf[64];
    static char *lp, *lf;
    static char savelp[64], savelf[64];
    char *newlp, *newlf;
    char *kp = kernfile;

    /* reset to beginning of environment variable list
     */
    if (rewind) {
	lp = getenv ("OSLoadPartition");
	lf = getenv ("OSLoadFilename");
	*savelp = '\0';
	*savelf = '\0';
    }

    /* get the next path in the varialble
     */
    newlp = env_get_var (&lp, lpbuf);
    newlf = env_get_var (&lf, lfbuf);

    /* if we've hit the end of all variables, return NULL
     */
    if (newlp == NULL && newlf == NULL)
	return NULL;

    /* replace any empty portions of the path with most recent 
     * value.  Save new value.
     */
    if (newlp == NULL)
	newlp = savelp;
    else
	strcpy (savelp, newlp);

    if (newlf == NULL)
	newlf = savelf;
    else
	strcpy (savelf, newlf);
    
    if (newlp) {
	while (*newlp != '\0')
	    *kp++ = *newlp++;
	*kp++ = '/';
    }
    
    if (newlf) {
	while (*newlf != '\0')
	    *kp++ = *newlf++;
	*kp = '\0';
    }

    return kernfile;
}
