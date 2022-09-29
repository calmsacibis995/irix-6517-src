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
#include <grp.h>
#include <stdlib.h>
#include <malloc.h>
#include "libadm.h"

#define PROMPT	"Enter the name of an existing group"
#define MESG	"Please enter the name of an existing group."
#define ALTMESG	"Please enter one of the following group names:\\n\\t"
#define MALSIZ	64

#define DELIM1 '/'
#define BLANK ' '

int		ckgrpfile(void);

static char *
setmsg(int disp)
{
	struct group
		*grpptr;
	int	count, n, m;
	char	*msg;

	if(disp == 0)
		return(MESG);

	m = MALSIZ;
	n = sizeof(ALTMESG);
	msg = (char *) calloc(m, sizeof(char));
	(void) strcpy(msg, ALTMESG);

	setgrent();
	count = 0;
	while(grpptr = getgrent()) {
		n += strlen(grpptr->gr_name) + 2;
		while(n >= m) {
			m += MALSIZ;
			msg = (char *) realloc(msg, m*sizeof(char));
		}
		if(count++)
			(void) strcat(msg, ", ");
		(void) strcat(msg, grpptr->gr_name);
	}
	endgrent();
	return(msg);
}

int
ckgid_dsp(void)
{
	struct group *grpptr;

	/* if display flag is set, then list out group file */
	if (ckgrpfile() == 1)
		return(1);
	setgrent();
	while (grpptr = getgrent())
		(void) printf("%s\n", grpptr->gr_name);
	endgrent();
	return(0);
}

int
ckgid_val(char *grpnm)
{
	int	valid;

	setgrent ();
	valid = (getgrnam(grpnm) ? 0 : 1);
	endgrent ();
	return(valid);
}

int
ckgrpfile(void) /* check to see if group file there */
{
	setgrent ();
	if (!getgrent()) {
		endgrent ();
		return(1);
	}
	endgrent ();
	return(0);
}

void
ckgid_err(int disp, char *error)
{
	char	*msg;

	msg = setmsg(disp);
	puterror(stdout, msg, error);
	if(disp)
		free(msg);
}

void
ckgid_hlp(int disp, char *help)
{
	char	*msg;

	msg = setmsg(disp);
	puthelp(stdout, msg, help);
	if(disp)
		free(msg);
}

int
ckgid(char *gid, short disp, char *defstr, char *error, char *help, char *prompt)
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
			(void) strcpy(gid, defstr);
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
	} else if(ckgid_val(input)) {
		if(!defmesg)
			defmesg = setmsg(disp);
		puterror(stderr, defmesg, error);
		goto start;
	}
	(void) strcpy(gid, input);
	if(disp && defmesg)
		free(defmesg);
	return(0);
}
