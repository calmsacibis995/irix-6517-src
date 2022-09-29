/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"$Revision: 1.4 $"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "libadm.h"


static void
setmsg(char *msg, short base)
{
	if((base == 0) || (base == 10))
		(void) sprintf(msg, "Please enter an integer.");
	else
		(void) sprintf(msg, "Please enter a base %d integer.", base);
}

static void
setprmpt(char *prmpt, short base)
{
	if((base == 0) || (base == 10))
		(void) sprintf(prmpt, "Enter an integer.");
	else
		(void) sprintf(prmpt, "Enter a base %d integer.", base);
}

int
ckint_val(char *value, short base)
{
	char	*ptr;

	(void) strtol(value, &ptr, base);
	if(*ptr == '\0')
		return(0);
	return(1);
}

void
ckint_err(short base, char *error)
{
	char	defmesg[64];

	setmsg(defmesg, base);
	puterror(stdout, defmesg, error);
}

void
ckint_hlp(short base, char *help)
{
	char	defmesg[64];

	setmsg(defmesg, base);
	puthelp(stdout, defmesg, help);
}
	
int
ckint(long *intval, short base, char *defstr, char *error, char *help, char *prompt)
{
	long	value;
	char	*ptr,
		input[128],
		defmesg[64],
		temp[64];

	if(!prompt) {
		setprmpt(temp, base);
		prompt = temp;
	}
	setmsg(defmesg, base);

start:
	putprmpt(stderr, prompt, NULL, defstr);
	if(getinput(input))
		return(1);

	if(strlen(input) == 0) {
		if(defstr) {
			*intval = strtol(defstr, NULL, base);
			return(0);
		}
		puterror(stderr, defmesg, error);
		goto start;
	} else if(!strcmp(input, "?")) {
		puthelp(stderr, defmesg, help);
		goto start;
	} else if(ckquit && !strcmp(input, "q"))
		return(3);

	value = strtol(input, &ptr, base);
	if(*ptr != '\0') {
		puterror(stderr, defmesg, error);
		goto start;
	}
	*intval = value;
	return(0);
}
