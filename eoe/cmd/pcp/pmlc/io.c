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

#ident "$Id: io.c,v 2.1 1997/09/12 01:07:52 markgw Exp $"

#include "pmapi.h"
#include "impl.h"
#include "./pmlc.h"

static char	lastc;
static char	peekc = '\0';

extern int	mystate;
extern int	yylineno;

extern int	eflag;
extern int	iflag;

char input(void)
{
    int		get;
    static int	first = 1;

    if (peekc) {
	lastc = peekc;
	peekc = '\0';
	return lastc;
    }
	
    if (lastc == '\n' || first) {
	if (iflag) {
	    if (mystate == GLOBAL)
		printf("pmlc> ");
	    else
		printf("? ");
	    fflush(stdout);
	}
	if (first)
	    first = 0;
	else
	    yylineno++;
    }
    else if (lastc == '\0')
	return lastc;

    get = getchar();
    if (get == EOF)
	lastc = '\0';
    else {
	lastc = get;
	if (eflag) {
	    putchar(lastc);
	    fflush(stdout);
	}
    }

    return lastc;
}

void unput(char c)
{
    peekc = c;
}

int yywrap(void)
{
    return lastc == '\0';
}

char lastinput(void)
{
    return lastc;
}
