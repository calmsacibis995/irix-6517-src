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

#ident "$Id: error.c,v 2.4 1997/09/12 01:07:32 markgw Exp $"

#include <stdio.h>
#include <stdlib.h>
#include "pmapi.h"
#include "impl.h"
#include "./pmlc.h"

extern char	*configfile;

void
yywarn(char *s)
{
    extern int	yylineno;

    fprintf(stderr, "Warning [%s, line %d]\n",
	    configfile == NULL ? "<stdin>" : configfile, yylineno);
    if (s != NULL && s[0] != '\0')
	fprintf(stderr, "%s\n", s);
}

void
yyerror(char *s)
{
    extern int	yylineno;
    int		c;

    fprintf(stderr, "Error [%s, line %d]\n",
	    configfile == NULL ? "<stdin>" : configfile, yylineno);
    if (s != NULL && s[0] != '\0')
	fprintf(stderr, "%s\n", s);
    c = lastinput();
    for ( ; ; ) {
	if (c == '\0')
	    break;
	if ((mystate == GLOBAL || (mystate & INSPEC)) && c == '\n')
	    break;
	if ((mystate & INSPECLIST) && c == '}')
	    break;
	c = input();
    }
    mystate = GLOBAL;
}
