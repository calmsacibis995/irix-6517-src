#include <string.h>
#include <helpapi/HelpBroker.h>

/* -------------- interface to SGIHelp ---------------- */

long HELPdisplay (char *filename)
    /* this calls the corresponding topic through SGIHelp */
{
char tmpstr[100], *s, *helpid;

strcpy(tmpstr, strrchr(filename, '/'));
if (tmpstr == NULL)
	strcpy(helpid, filename);
else  {
	helpid = tmpstr;
	helpid++;   /* to skip the initial '/' */
}

/* substitute occurences of . in helpid by _ to avoid problems with SGIHelp */
for (s=helpid; *s != '\0'; s++)
	if ((*s) == '.')
		*s = '_';

/* if helpid ends in _helpfile, make it end in _help */
if (strcmp (helpid + strlen(helpid)-4, "file") == 0)
	*(helpid + strlen(helpid)-4) = '\0';

printf("helpid = %s\n", helpid); 
SGIHelpMsg (helpid, "*", NULL);

return 1;
}


void HELPposition(long x1, long x2, long y1, long y2)
{
}

void HELPclose()
{
}
