/*
 * getterm.c
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.4 $"


#include <stdio.h>
#include <string.h>
#include <limits.h>

/*
 * make a reasonable guess as to the kind of terminal the user is on.
 * We look in /etc/ttytype for this info (format: each line has two
 * words, first word is a term type, second is a tty name), and default
 * to deftype if we can't find any better.  In the case of dialups we get
 * names like "dialup" which is a lousy guess but tset can
 * take it from there.
 */

/* This value is the maximum string length returned by longname() in curses */
#define MAXTTYTYPE 128  

int
getterm(register char *tname, char *buffer, char *filename, char *deftype)
	{
	register FILE	*fdes;
	char		*type, *t;
	char	*lasttok;
	char		ttline[MAXTTYTYPE + PATH_MAX + 1]; 
	int		retval = -1;

	if (filename == NULL)
		filename = "/etc/ttytype";
	if ((fdes = fopen(filename, "r")) != NULL)
		{
		lasttok = NULL;
		while (fgets(ttline, sizeof(ttline), fdes) != NULL)
			{
			if (((type = strtok_r(ttline, " \t\n\r", &lasttok)) != NULL) &&
			    ((t = strtok_r(NULL, " \t\n\r", &lasttok)) != NULL) &&
			    (strcmp(t, tname) == 0))
				{
				strcpy(buffer, type);
				return fclose(fdes);
				}
			}
		retval = fclose(fdes);
		}
	strcpy(buffer, deftype);
	return retval;
	}
