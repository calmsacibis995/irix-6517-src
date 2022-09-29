/*
** Pathname convenience package
*/

#include "path.h"
#include <string.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef TRUE
#define TRUE 1;
#endif
#ifndef FALSE
#define FALSE 0;
#endif

char*
pathbasename(const char* path)
{
    register const char*	c = path;
    const char*		lastbase = 0;
    register int	length = 0;
    char*		retstr;

    if (*c != '/') {		/* first name might be base */
	lastbase = c;
    }

    while (*c) {
	if (*c == '/') {
	    switch (*(c+1)) {	/* look-ahead one char */
	    case '/':		/* c is a useless repetitious slash */
		c++;
		break;
	    case '\0':		/* c is a useless slash at end of path */
		c++;
		break;
	    default:
		c++;		/* c+1 is first char of possible basename */
		lastbase = c;
		c++; length = 1;
		break;
	    }
	} else {		/* char in possible basename */
	    c++; length++;
	}
    }

    if (lastbase == 0) {	/* no basename found */
	retstr = malloc(1);
	retstr[0] = '\0';
	return retstr;
    }

    retstr = malloc(length+1);
    strncpy(retstr, lastbase, length);  /* copy only base sans trailing /'s */
    retstr[length] = '\0';

    return retstr;
}


char*
pathdirname(const char* path)
{
    register const char*	c;
    const char*		lastslash = 0;
    char*		retstr;

    for (c = path; *c; c++) {
	if (*c == '/') {
	    const char* tmplastslash = c;
	    for (c++; *c == '/'; c++) {;}
	    			/* ignore redundant slashes */
	    if (*c) {		/* not superfluous trailing slash */
		lastslash = tmplastslash;
	    } else {		/* was superfluous trailing slash */
		break;
	    }
	}
    }

    if (lastslash == 0) {
	if (path[0] == '/') { 	/* path is "/" or has repeated "/" */
	    retstr = strdup("/");
	    return retstr;
	} else {		/* basename only implies current dir "." */
	    retstr = strdup(".");
	    return retstr;
	}
    }


    retstr = malloc(lastslash-path+1);
    strncpy(retstr, path, lastslash-path);
				/* copy only dir sans trailing /'s */
    retstr[lastslash-path] = '\0';

    return retstr;
}


char*
pathexpandtilde(const char* path)
{
    register const char*	c;
    char		user[255];
    char*		homedir;
    char*		retstr;

    if (path[0] != '~') {
	retstr = strdup(path);
	return retstr;
    }
    
    if (path[1] == '/' || path[1] == '\0') {	/* ~/... or ~ */
	homedir = getenv("HOME");
	c = &(path[1]);
    } else {					/* ~<user>/... */
	struct passwd*	pwent;
	int i;

	c = &(path[1]);
	for (i=0; *c != '/' && *c != '\0'; c++, i++) {
	    user[i] = *c;
	}
	user[i] = '\0';

	pwent = getpwnam(user);
	if (pwent == 0) {			/* no such user */
	    retstr = strdup(path);
	    return retstr;
	}

	homedir = pwent->pw_dir;
    }

    if (homedir == 0) {
	homedir = "";				/* consistency with csh & ksh */
    }
    retstr = malloc(strlen(homedir) + strlen(c) + 1);
    strcpy(retstr, homedir);
    strcat(retstr, c);
    return retstr;
}
