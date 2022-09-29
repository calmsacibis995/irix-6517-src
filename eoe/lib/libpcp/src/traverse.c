/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: traverse.c,v 2.3 1998/11/15 08:35:24 kenmcd Exp $"

#include <string.h>
#include "pmapi.h"
#include "impl.h"

extern int	errno;

/*
 * generic depth-first recursive descent of the PMNS
 */
int
pmTraversePMNS(char *name, void(*func)(char *name))
{
    int		sts;
    char	**enfants;

    if ((sts = pmGetChildren(name, &enfants)) < 0) {
	return sts;
    }
    else if (sts == 0) {
	/* leaf node, name is full name of a metric */
	(*func)(name);
	sts = 1;
    }
    else if (sts > 0) {
	int	j;
	char	*newname;
	int	n;

	for (j = 0; j < sts; j++) {
	    newname = (char *)malloc(strlen(name) + 1 + strlen(enfants[j]) + 1);
	    if (newname == NULL) {
		sts = -oserror();
		fprintf(stderr, "pmTraversePMNS: malloc: %s\n", strerror(-sts));
		break;
	    }
	    if (*name == '\0')
		strcpy(newname, enfants[j]);
	    else {
		strcpy(newname, name);
		strcat(newname, ".");
		strcat(newname, enfants[j]);
	    }
	    n = pmTraversePMNS(newname, func);
	    free(newname);
	    if (n < 0) {
		sts = n;
		break;
	    }
	    sts += n;
	}
	free(enfants);
    }

    return sts;
}
