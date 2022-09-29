/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.5 $"

#include "uucp.h"

/*
 * expand file name expansion is based on first characters
 *	/	-> fully qualified pathname. no
 *		   processing necessary
 *	~	-> prepended with login directory
 *	~/	-> prepended with Pubdir
 *	default	-> prepended with current directory
 *	file	-> filename to expand
 * returns: 
 *	0	-> ok
 *      FAIL	-> no Wrkdir name available
 */
expfile(file)
register char *file;
{
	register char *fpart, *up;
	int uid;
	char user[NAMESIZE], save[MAXFULLNAME];

	strcpy(save, file);
	if (*file == '/')
	    ;
	else if (*file ==  '~') {
		/* find / and copy user part */
		for (fpart = save + 1, up = user; *fpart != '\0'
			&& *fpart != '/'; fpart++)
				*up++ = *fpart;
		*up = '\0';
		if ((user[0]=='\0') || (gninfo(user, &uid, file) != 0)){
			(void) strcpy(file, Pubdir);
		}
		(void) strcat(file, fpart);
	}
	else {
		(void) sprintf(file, "%s/%s", Wrkdir, save);
		if (Wrkdir[0] == '\0')
			return(FAIL);
	}

	if (canPath(file) != 0) { /* I don't think this will ever fail */
		(void) strcpy(file, CORRUPTDIR);
		return(FAIL);
	}
	else
		return(0);
}


/*
 * make all necessary directories
 *	name	-> directory to make
 * return: 
 *	0	-> success
 * 	FAIL	-> failure
 */
int
mkdirs(name)
register char *name;
{
	register char *p;
	char dir[MAXFULLNAME];

	strcpy(dir, name);
	if (*LASTCHAR(dir) != '/')
	    	(void) strcat(dir, "/");
	p = dir + 1;
	for (;;) {
	    if ((p = strchr(p, '/')) == NULL)
		return(0);
	    *p = '\0';
	    DEBUG(4, "mkdir - %s\n", dir);
	    if (mkdirs2(dir, 0) == FAIL)
		return (FAIL);
	    *p++ = '/';
	}
}


#ifdef ATTSV
/*
 * Make name a directory if it is not already a directory
 * ATTSV handling of setuid is less than convenient ...
 * return:
 *	0 -> ok
 *	FAIL -> failed
 */

mkdirs2(name, mask)
register char	*name;
register int	mask;
{
	int	status;

	if (DIRECTORY(name))
		return(0);		/* directory exists */

	status = mkdir(name,0770);

	if (status != 0) {
	    errent(Ct_CREATE, name, status, __FILE__, __LINE__);
	    return(FAIL);
	}

	if ( chmod(name, (mask ^ 0777) & 0777) != 0) {
		errent(Ct_CHMOD, name, errno, __FILE__, __LINE__);
		setuid(Euid);
		return(FAIL);
	};
	if (chown(name, UUCPUID, getgid()) != 0) {
		DEBUG(5, "CHOWN FAILED %s  ", name);
		DEBUG(5, "errno %d\n", errno);
		errent(Ct_CHOWN, name, errno, __FILE__, __LINE__);
		setuid(Euid);
		return(FAIL);
	}
	setuid(Euid);

	return(0);
}
#endif /* ATTSV */

#ifdef V7
mkdirs2(name, mask)
register char	*name;
register int	mask;
{
	int	ret;

	if (DIRECTORY(name))
		return(0);		/* directory exists */

	if (vfork() == 0) {
		setuid(Euid);	/* this makes it trivial in V7 */
		umask(mask);
		/* close stdout and stderr to fail quietly */
		close(1);
		close(2);
		execl("/bin/mkdir", "mkdir", name, (char *) 0);
		_exit(1);
	}
	wait(&ret);

	if (ret != 0)		/* why should we abort if some remote tries */
	    return(FAIL);	/* to make a garbage directory */

	return(0);
}
#endif /* V7 */

/*
 * expand file name and check return
 * print error if it failed.
 *	file	-> file name to check
 * returns: 
 *      0	-> ok
 *      FAIL	-> if expfile failed
 */
ckexpf(file)
char *file;
{
	if (expfile(file) == 0)
		return(0);

	fprintf(stderr, "Illegal filename (%s).\n", file);
	return(FAIL);
}


/*
 * make canonical path out of path passed as argument.
 *
 * Eliminate redundant self-references like // or /./
 * (A single terminal / will be preserved, however.)
 * Dispose of references to .. in the path names.
 * In relative path names, this means that .. or a/../..
 * will be treated as an illegal reference.
 * In full paths, .. is always allowed, with /.. treated as /
 *
 * returns:
 *	0	-> path is now in canonical form
 *	FAIL	-> relative path contained illegal .. reference
 */

int
canPath(path)
register char *path;	/* path is modified in place */
{
    register char *to, *fr;

    to = fr = path;
    if (*fr == '/') *to++ = *fr++;
    for (;;) {
	/* skip past references to self and validate references to .. */
	for (;;) {
	    if (*fr == '/') {
		fr++;
		continue;
	    }
	    if ((strncmp(fr, "./", 2) == SAME) || EQUALS(fr, ".")) {
		fr++;
		continue;
	    }
	    if ((strncmp(fr, "../", 3) == SAME) || EQUALS(fr, "..")) {
		fr += 2;
		/*	/.. is /	*/
		if (((to - 1) == path) && (*path == '/')) continue;
		/* error if no previous component */
		if (to <= path) return (FAIL);
		/* back past previous component */
		while ((--to > path) && (to[-1] != '/'));
		continue;
	    }
	    break;
	}
	/*
	 * What follows is a legitimate component,
	 * terminated by a null or a /
	 */
	if (*fr == '\0') break;
	while ((*to++ = *fr) && (*fr++ != '/'));
    }
    /* null path is . */
    if (to == path) *to++ = '.';
    *to = '\0';
    return (0);
}
