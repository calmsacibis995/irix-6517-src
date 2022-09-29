/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)kbdload:scrutiny.c	1.2"			*/

#ident  "$Revision: 1.1 $"

/*
 * scrutinize	Use "fullpath" to scrutinize a pathname.  It returns
 *		a pointer to the "reduced" pathname, given the input
 *		name.  It uses the current working directory as the
 *		starting point.  Not relying on the environment, it
 *		uses the "getcwd()" library routine to get the current
 *		working directory name.
 */

#include <stdio.h>
#include <pfmt.h>

static explode(), dotpack(), implode();

char *
scrutinize(arg, uid, euid)

	register char *arg;
	register int uid, euid;
{
	static char out[2048];	/* output */
	char cwd[1024];
	char str[1024];
	char *fullpath();
	register char *tmp;

	if (getcwd(cwd, 1024) == NULL) { /* Don't rely on env "PWD" */
		pfmt(stderr, MM_ERROR, ":127:Working dir name too long.\n");
		return(0);
	}
	if ((strlen(cwd) + strlen(arg)) > 2048) {
		pfmt(stderr, MM_ERROR, ":128:Pathname too long.\n");
		return(0);
	}
	strcpy(str, arg);
	(void) fullpath(cwd, str, out);
	return(out);
}

/*
 * fullpath	Take current working dir, a pathname (in), and a place
 *		to put results.  Return the full pathname of the
 *		file in "out".  This packs out "." and ".." references,
 *		returning the REAL absolute path name of the file.
 *		Example:  /usr/lib/trash/../../junk reduces to
 *			  /usr/junk
 */

char *
fullpath(cwd, in, out)

	char *cwd, *in, *out;
{
	char *carg[256];
	char *iarg[256];
	register int i, c, x;

	*out = '\0';
	explode(cwd, &carg[0]);	/* split up path components */
	explode(in, &iarg[0]);

	/*
	 * Remove leading redundant "." and "/.." components from the
	 * input name.  However, it makes the result begin with a "/",
	 * so re-insert the slash in following component.
	 */
	i = c = 0;
	while ((strcmp("/..", iarg[i]) == 0) ||
	       (strcmp("/.", iarg[i]) == 0)) {
		++i;
		if (iarg[i]) {
			iarg[i] -= 1; *iarg[i] = '/';
		}
		else {	/* oops, only "/.." & "/." components */
			iarg[i] = "/\0\0\0\0";
		}
	}
	/*
	 * Now, reduce out any "." components left in the name, as they
	 * have no effect.
	 */
	while (iarg[i] && (strcmp(iarg[i], ".") == 0))
		++i;
	x = i;
	while (iarg[x]) {
		if (strcmp(iarg[x], ".") == 0) {
			register int y;

			y = x;
			do {
				iarg[y] = iarg[y+1];
				++y;
			} while (iarg[y+1]);
			iarg[y] = NULL;
		}
		++x;
	}
	/*
	 * Does the thing still begin with a "/"?  If so, pack out ".."
	 * references, slap it back together and return.
	 */
	if (*iarg[i] == '/') {
		dotpack(&iarg[i]);
		implode(&iarg[i], out);
	}
	else {
		/*
		 * Otherwise, find end of current working dir.  We'll be
		 * appending components onto it.  Append all components
		 * of "iarg" onto the cwd, then pack out ".." refs.
		 */
		while (carg[c])
			++c;
		/*
		 * Now work through the input string, concatenating
		 * components onto current working directory.  "i" is
		 * where we begin in the input, "c" is where we are in
		 * the cwd.
		 */
		while (iarg[i]) {
			carg[c] = iarg[i];
			++c;
			carg[c] = NULL;	/* keep null termination */
			++i;
		}
		dotpack(&carg[0]);
		implode(&carg[0], out);
	}
	return(out);
}

/*
 * explode	Take a path name and array of char pointers.  Set
 *		the char pointers to point at path components.
 *		Warning: mucks with the original string.  If the
 *		pathname begins with a "/", then the slash is
 *		left in the first component.  All other components
 *		have their slashes removed.  This allows us to
 *		detect an absolute pathname.
 */

static
explode(path, ptr)

	char *path;
	char **ptr;
{
	if (*path == '/') {	/* takes care of multiple leading "/" */
		while (*path == '/')
			++path;
		*ptr++ = --path;	/* start at last slash */
		++path;		/* then go on after it */
	}
	else
		*ptr++ = path;
	while (*path) {
		if (*path == '/') {
			*path++ = '\0';
			*ptr++ = path;
		}
		else {
			while (*path && (*path != '/'))
				++path;
		}
	}
	*ptr = NULL;
}

/*
 * dotpack	Find and remove all ".." references.  As we find each
 *		one, we remove it and the previous reference, as they
 *		cancel out.  We're guaranteed not to get a pathname
 *		that begins with "/..".
 */

static
dotpack(p)

	char *p[];	/* pointer to array of pointers */
{
	register int change, i, j;

	do {
		i = 0;
		change = 0;
		while (p[i]) {
			if (p[i+1] && (strcmp(p[i+1], "..") == 0)) {
				change = 1;
				j = i+2;
				do {
					p[i++] = p[j++];
				} while (p[i]);
				p[i] = p[j] = p[j+1] = NULL;
				i = (-1);
			}
			++i;
		}
	} while (change);
	i = 0;
	while (strcmp(p[i], "..") == 0) {	/* overran slash */
		p[0] = "";
	}
}

/*
 * implode	Pack the array of strings to a single string.
 *		We may get an array that doesn't have a "/" as the
 *		first character of the first string.
 */

static
implode(p, s)

	char *p[];
	char *s;
{
	register int i;
	register char *tmp;

	i = 0;
	if (p[0] && (*p[0] != '/'))
		*s++ = '/';
	while (p[i]) {
		tmp = p[i];
		if (*tmp) {
			while (*tmp)
				*s++ = *tmp++;
			if (p[i+1])
				*s++ = '/';
		}
		++i;
	}
	*s++ = '\0';
}
