/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"$Revision: 1.5 $"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <pwd.h>
#include <stdlib.h>
#include <malloc.h>
#include "libadm.h"

						
#define PROMPT	"Enter the login name of an existing user"
#define MESG	"Please enter the login name of an existing user."
#define ALTMESG	"Please enter one of the following login names:\\n\\t"
#define MALSIZ	64

#define DELIM1 '/'
#define BLANK ' '

int		ckpwdfile(void);

static char *
setmsg(int disp)
{
	struct passwd
		*pwdptr;
	int	count, n, m;
	char	*msg;

	if(disp == 0)
		return(MESG);

	m = MALSIZ;
	n = sizeof(ALTMESG);
	msg = (char *) calloc(m, sizeof(char));
	(void) strcpy(msg, ALTMESG);

	setpwent();
	count = 0;
	while(pwdptr = getpwent()) {
		n += strlen(pwdptr->pw_name) + 2;
		while(n >= m) {
			m += MALSIZ;
			msg = (char *) realloc(msg, m*sizeof(char));
		}
		if(count++)
			(void) strcat(msg, ", ");
		(void) strcat(msg, pwdptr->pw_name);
	}
	endpwent();
	return(msg);
}

int
ckuid_dsp(void)
{
	struct passwd *pwdptr;

	/* if display flag is set, then list out passwd file */
	if (ckpwdfile() == 1)
		return(1);
	setpwent();
	while(pwdptr = getpwent())
		(void) printf("%s\n", pwdptr->pw_name);
	endpwent();
	return(0);
}

int
ckuid_val(char *usrnm)
{
	int	valid;

	setpwent ();
	valid = (getpwnam(usrnm) ? 0 : 1);
	endpwent ();
	return(valid);
}

int
ckpwdfile(void) /* check to see if passwd file there */
{
	setpwent ();
	if (!getpwent()) {
		endpwent ();
		return(1);
	}
	endpwent ();
	return(0);
}

void
ckuid_err(short disp, char *error)
{
	char	*msg;

	msg = setmsg(disp);
	puterror(stdout, msg, error);
	if(disp)
		free(msg);
}

void
ckuid_hlp(int disp, char *help)
{
	char	*msg;

	msg = setmsg(disp);
	puthelp(stdout, msg, help);
	if(disp)
		free(msg);
}

int
ckuid(char *uid, short disp, char *defstr, char *error, char *help, char *prompt)
{
	char	*defmesg,
		input[128];

	defmesg = NULL;
	if(!prompt)
		prompt = PROMPT;

start:
	putprmpt(stderr, prompt, NULL, defstr);
	if(getinput(input)) {
		if(disp && defmesg)
			free(defmesg);
		return(1);
	}

	if(!strlen(input)) {
		if(defstr) {
			if(disp && defmesg)
				free(defmesg);
			(void) strcpy(uid, defstr);
			return(0);
		}
		if(!defmesg)
			defmesg = setmsg(disp);
		puterror(stderr, defmesg, error);
		goto start;
	} else if(!strcmp(input, "?")) {
		if(!defmesg)
			defmesg = setmsg(disp);
		puthelp(stderr, defmesg, help);
		goto start;
	} else if(ckquit && !strcmp(input, "q")) {
		if(disp && defmesg)
			free(defmesg);
		return(3);
	} else if(ckuid_val(input)) {
		if(!defmesg)
			defmesg = setmsg(disp);
		puterror(stderr, defmesg, error);
		goto start;
	}
	(void) strcpy(uid, input);
	if(disp && defmesg)
		free(defmesg);
	return(0);
}
