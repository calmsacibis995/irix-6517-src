/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"$Revision: 1.4 $"

#include <stdio.h>
#include <string.h>
#include "libadm.h"

extern int	ckwidth;

void
putprmpt(FILE *fp, char *prompt, char *choices[], char *defstr)
{
	char buffer[512];
	int i, n;

	(void) fputc('\n', fp);
	(void) strcpy(buffer, prompt);
	if(defstr)
		(void) sprintf(buffer+strlen(buffer), " (default:\\ %s)", 
			defstr);

	n = strlen(prompt);
	if(!n || !strchr(":?", prompt[n-1])) {
		(void) strcat(buffer, "\\ [");
		for(i=0; choices && choices[i]; ++i) {
			(void) strcat(buffer, choices[i]);
			(void) strcat(buffer, ",");
		}
		(void) strcat(buffer, ckquit ? "?,q] " : "?] ");
	} else
		(void) strcat(buffer, " ");
	(void) puttext(fp, buffer, 0, ckwidth);
}
