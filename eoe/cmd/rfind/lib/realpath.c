/*
 * See the following SGI PV Incidents to understand why
 * copies of the realpath and getcwd libc code have been
 * copied here.  The only real change was the addition of
 * the few lines in getcwd.c involving rootstat/root_dev/root_ino.
 *    558311 - rfind broken on loopback file systems
 *    558312 - getcwd/getwd broken on loopback file systems
 *    558313 - pwd broken on loopback file systems
 *    558314 - ptools p_tupdate very slow on kudzu loopback file systems
 *    558315 - libc __getcwd() broken on loopback file systems
 *
 * Paul Jackson
 * 2 Jan 98
 * Silicon Graphics
 */

/*
 * A few days later ... Resolving bug 558311 requires more
 * work.  The realpath() routine (while it calls getwd() to
 * convert input relative paths to absolute) mostly works
 * from root down (unlike getwd(), which works from "." up).
 * Therefore changes to getwd() to avoid prepending "lofs"
 * prefixes don't help realpath() in the case that realpath()
 * is given an absolute path with a "lofs" prefix.
 *
 * So added a trim_lofs() method, that trims off any autofs
 * prefix (e.g. "/hosts/bonnie.engr") from the result of
 * my_realpath(), just before returning it.
 *
 * Paul Jackson
 * 6 Jan 98
 * Silicon Graphics
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


/*
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */

#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/param.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <mntent.h>
#include "externs.h"

static int pathcanon(const char *, char *, int);

/* LINTLIBRARY */

/*
 * Input name in raw, canonicalized pathname output to canon.  If dosymlinks
 * is nonzero, resolves all symbolic links encountered during canonicalization
 * into an equivalent symlink-free form.  Returns 0 on success, -1 on failure.
 * The routine fails if the current working directory can't be obtained or if
 * either of the arguments is NULL.
 *
 * Sets errno on failure.
 */
static int
pathcanon(const char *raw, char *canon, int dosymlinks)
{
    register char	*s,
			*d;
    register char	*limit = canon + MAXPATHLEN;
    char		*modcanon;
    int			nlink = 0;

    /*
     * Do a bit of sanity checking.
     */
    if (raw == NULL || canon == NULL) {
#if _ABIAPI
	errno = EINVAL;
#else
	setoserror(EINVAL);
#endif
	return (-1);
    }

    if (*raw == 0) {
#if _ABIAPI
	errno = ENOENT;
#else
	setoserror(ENOENT);
#endif
	return (-1);
    }
    if (strlen(raw) > MAXPATHLEN) {
#if _ABIAPI
	errno = ENAMETOOLONG;
#else
	setoserror(ENAMETOOLONG);
#endif
	return (-1);
    }

    /*
     * If the path in raw is not already absolute, convert it to that form.
     * In any case, initialize canon with the absolute form of raw.  Make
     * sure that none of the operations overflow the corresponding buffers.
     * The code below does the copy operations by hand so that it can easily
     * keep track of whether overflow is about to occur.
     */
    s = (char *)raw;
    d = canon;
    if (*s != '/') {
	/* Relative; prepend the working directory. */
	if (getcwd(d, MAXPATHLEN) == NULL) {
	    /* Use whatever errno value getcwd may have left around. */
	    return (-1);
	}
	d += strlen(d);
	/* Add slash to separate working directory from relative part. */
	if (d < limit)
	    *d++ = '/';
	modcanon = d;
    } else
	modcanon = canon;
    while (d < limit && *s)
	*d++ = *s++;

    /* Add a trailing slash to simplify the code below. */
    s = "/";
    while (d < limit && (*d++ = *s++))
	continue;
	

    /*
     * Canonicalize the path.  The strategy is to update in place, with
     * d pointing to the end of the canonicalized portion and s to the
     * current spot from which we're copying.  This works because
     * canonicalization doesn't increase path length, except as discussed
     * below.  Note also that the path has had a slash added at its end.
     * This greatly simplifies the treatment of boundary conditions.
     */
    d = s = modcanon;
    while (d < limit && *s) {
	if ((*d++ = *s++) == '/' && d > canon + 1) {
	    register char  *t = d - 2;

	    switch (*t) {
	    case '/':
		/* Found // in the name. */
		d--;
		continue;
	    case '.': 
		switch (*--t) {
		case '/':
		    /* Found /./ in the name. */
		    d -= 2;
		    continue;
		case '.': 
		    if (*--t == '/') {
			/* Found /../ in the name. */
			while (t > canon && *--t != '/')
			    continue;
			d = t + 1;
		    }
		    continue;
		default:
		    break;
		}
		break;
	    default:
		break;
	    }
	    /*
	     * We're at the end of a component.  If dosymlinks is set
	     * see whether the component is a symbolic link.  If so,
	     * replace it by its contents.
	     */
	    if (dosymlinks) {
		char		link[MAXPATHLEN + 1];
		register int	llen;

		/*
		 * See whether it's a symlink by trying to read it.
		 *
		 * Start by isolating it.
		 */
		*(d - 1) = '\0';
		if ((llen = readlink(canon, link, sizeof link)) >= 0) {
		    /* Make sure that there are no circular links. */
		    nlink++;
		    if (nlink > MAXSYMLINKS) {
#if _ABIAPI
			errno = ELOOP;
#else
			setoserror(ELOOP);
#endif
			return (-1);
		    }
		    /*
		     * The component is a symlink.  Since its value can be
		     * of arbitrary size, we can't continue copying in place.
		     * Instead, form the new path suffix in the link buffer
		     * and then copy it back to its proper spot in canon.
		     */
		    t = link + llen;
		    *t++ = '/';
		    /*
		     * Copy the remaining unresolved portion to the end
		     * of the symlink. If the sum of the unresolved part and
		     * the readlink exceeds MAXPATHLEN, the extra bytes
		     * will be dropped off. Too bad!
		     */
		    (void) strncpy(t, s, sizeof(link) - (size_t)llen - 1);
		    link[sizeof link - 1] = '\0';
		    /*
		     * If the link's contents are absolute, copy it back
		     * to the start of canon, otherwise to the beginning of
		     * the link's position in the path.
		     */
		    if (link[0] == '/') {
			/* Absolute. */
			(void) strcpy(canon, link);
			d = s = canon;
		    }
		    else {
			/*
			 * Relative: find beginning of component and copy.
			 */
			--d;
			while (d > canon && *--d != '/')
			    continue;
			s = ++d;
			/*
			 * If the sum of the resolved part, the readlink
			 * and the remaining unresolved part exceeds
			 * MAXPATHLEN, the extra bytes will be dropped off.
			*/
			if (strlen(link) >= (size_t)(limit - s)) {
				(void) strncpy(s, link, (size_t)(limit - s));
				*(limit - 1) = '\0';
			} else {
				(void) strcpy(s, link);
			}
		    }
		    continue;
		} else {
		   /*
		    * readlink call failed. It can be because it was
		    * not a link (i.e. a file, dir etc.) or because the
		    * the call actually failed.
		    */
		    if (errno != EINVAL)
			return (-1);
		    *(d - 1) = '/';	/* Restore it */
		}
	    } /* if (dosymlinks) */
	}
    } /* while */

    /* Remove the trailing slash that was added above. */
    if (*(d - 1) == '/' && d > canon + 1)
	    d--;
    *d = '\0';
    return (0);
}

/*
 * Pathnames on loop-back file systems (autofs mounts of
 * local file systems) have an additional prefix such as
 * "/hosts/<hostname>".  These prefixes equate to "/",
 * typically, and need to be trimmed off for this rfind
 * code, which deals internally with paths as formed by
 * the initial EFS/XFS mounts, rather than with the longer
 * loop-back variants.  The prefixes to trim, and what
 * they are equivalent to, can be discovered by scanning
 * the mount table for "lofs" entries.
 *
 * For example, on bonnie.engr as I write this now,
 * the mount table has the entries:
 *  / on /hosts/bonnie.engr type lofs (rw,lofsid=1,dev=3000000)
 *  / on /hosts/bonnie.engr.sgi.com type lofs (rw,lofsid=32,dev=3000000)
 *  / on /hosts/bonnie type lofs (rw,lofsid=184,dev=3000000)
 *
 * Given such a mount table, the following routine would
 * trim any of the prefixes "/hosts/bonnie.engr",
 * "/hosts/bonnie.engr.sgi.com" or "/hosts/bonnie"
 * from the input path.
 *
 * trim_lofs() always returns the same pointer as was
 * passed in, after rewriting the buffer as needed to
 * trim off a "lofs" prefix (actually, to convert such
 * a prefix to its equivalent -- but since that is "/",
 * it just looks like a simple prefix trim).
 */

/*
 * isancestor (char *par, char *child, ulong_t parlen)
 *
 * Returns 1 if par syntactically appears to be a pathname
 * that is an ancestor of child, else returns 0.
 */

int isancestor (char *par, char *child, ulong_t parlen) {
        if (parlen == 1L)
                return (par[0] == '/');
        if (strncmp (par, child, parlen) == 0) {
                char c = child[parlen];
                return (c == '/' || c == '\0');
        }
        return 0;
}

char *
trim_lofs (char *inpath)
{
    char copypath[MAXPATHLEN];      /* copy inpath during prefix conversion */
    FILE *mtabp;                    /* /etc/mtab */
    struct mntent *mntp;            /* current mtab entry */
    int newlen;			    /* new, trimmed path length */

    if (strlen (inpath) + 1 > MAXPATHLEN)
	return inpath;

    if ((mtabp = setmntent("/etc/mtab", "r")) == NULL)
	return inpath;

    while ((mntp = getmntent(mtabp)) != NULL) {

	/* look for "lofs" mounts */
	if (strncmp(mntp->mnt_type, MNTTYPE_LOFS, strlen(MNTTYPE_LOFS)) != 0)
	    continue;

	/* look for ancestors of the input path */
	if ( ! isancestor (mntp->mnt_dir, inpath, strlen(mntp->mnt_dir)) )
	    continue;

	/* skip conversions that would be too long */
	newlen = 0;
	newlen += strlen (mntp->mnt_fsname);
	newlen += 1;	/* "/" */
	newlen += strlen (inpath) - strlen (mntp->mnt_dir);
	newlen += 1;	/* nul */
	if (newlen > MAXPATHLEN)
	    continue;

	strcpy (copypath, inpath);
	strcpy (inpath, mntp->mnt_fsname);
	strcat (inpath, "/");
	strcat (inpath, copypath + strlen(mntp->mnt_dir));
	pathcomp (inpath);
    }

    endmntent(mtabp);
    return inpath;
}

/*
 * Canonicalize the path given in raw, resolving away all symbolic link
 * components.  Store the result into the buffer named by canon, which
 * must be long enough (MAXPATHLEN bytes will suffice).  Returns NULL
 * on failure and canon on success.
 *
 * The routine indirectly invokes the readlink() system call and getcwd()
 * so it inherits the possibility of hanging due to inaccessible file 
 * system resources.
 */
char *
my_realpath(const char *raw, char *canon)
{
    return (pathcanon(raw, canon, 1) < 0 ? NULL : trim_lofs (canon) );
}

