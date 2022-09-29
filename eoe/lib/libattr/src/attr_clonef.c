 /*************************************************************************
 #									  *
 # 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 #									  *
 #  These coded instructions, statements, and computer programs  contain  *
 #  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 #  are protected by Federal copyright law.  They  may  not be disclosed  *
 #  to  third  parties  or copied or duplicated in any form, in whole or  *
 #  in part, without the prior written consent of Silicon Graphics, Inc.  *
 #									  *
 #************************************************************************/

/*
 * 	attr_clonef -- Extended Attribute library extension routine
 */
#ident	"@(#)libattr/attr_clonef.c	1.1"

#include <sys/types.h>
#include <malloc.h>
#include <bstring.h>
#include <errno.h>
#include <sys/attributes.h>

#define	BUFSIZE		512	/* buffer for list of attr names */

/*
 * Given two file descriptors, copy all of the Extended User Attributes from
 * one to the other.  Do this for both the User Namespace and the Root-Only
 * namespace.  If we are not running as root, then the root-only namespace
 * will (silently) not be copied.
 *
 * The "flags" argument is parallel to the "flags" argument to the attribute
 * system calls.
 *
 * The caller can distinguish between read failures and write failures.
 * Failures while reading an attribute from the source file return 1 and
 * errno is set accordingly by the attr_get(2) or attr_list(2) syscalls.
 * Failures while writing an attribute to the destination file return -1 and
 * errno is set accordingly by the attr_set(2) syscall.
 *
 * A return value of zero means that all attributes were copied successfully
 * (except possibly the root-only attributes, see above).
 */

static int
attr_clone_copyf(int src, int dst, char *list_buf, char *attr_buf, int flags);

/*
 * Set up some working memory for the attribute value, then call a work
 * routine (twice) to do the actual attribute copying.
 *
 * If the User Namspace attribute copy fails, then don't bother trying the
 * root-only namespace.  We are either running without root permissions, or
 * the call would have failed even if we were running with root permissions.
 *
 * Note that we special case EPERM on the root-only namespace copy, if the
 * user namespace copy worked then it really is just a test for our being
 * root or not, so we silently ignore that error.
 */
int
attr_clonef(int src, int dst, int flags)
{
	char lbuf[BUFSIZE], *abuf;
	int error;

	/*
	 * Get ourselves some working storage.
	 */
	abuf = malloc(ATTR_MAX_VALUELEN);
	if (abuf == NULL) {
		return(ENOMEM);
	}

	/*
	 * Copy all the user-space attributes (if any).
	 */
	error = attr_clone_copyf(src, dst, lbuf, abuf, flags);
	if (error) {
		free(abuf);
		return(error);
	}

	/*
	 * Copy all the ROOT-namespace attributes.
	 * If we aren't root, we will get EPERM, and we will ignore it.
	 */
	error = attr_clone_copyf(src, dst, lbuf, abuf, flags|ATTR_ROOT);
	if (error && (errno == EPERM))
		error = 0;		/* ignore non-root access errors */

	free(abuf);
	return(error);
}

/*
 * Copy all the attributes (if we can see any) from one fd to the other.
 *
 * There are more efficient ways to do this (eg: attr_multi(), but not now).
 */
static int
attr_clone_copyf(int src, int dst, char *list_buf, char *attr_buf, int flags)
{
	attrlist_t *alist;
	attrlist_ent_t *attr;
	attrlist_cursor_t cursor;
	int length, i;

	bzero((char *)&cursor, sizeof(cursor));
	do {
		if (attr_listf(src, list_buf, BUFSIZE, flags, &cursor) < 0)
			return(1);

		alist = (attrlist_t *)list_buf;
		for (i = 0; i < alist->al_count; i++) {
			attr = ATTR_ENTRY(list_buf, i);
			length = ATTR_MAX_VALUELEN;
			if (attr_getf(src, attr->a_name, attr_buf, &length,
					   flags) < 0) {
				return(1);
			}
			if (attr_setf(dst, attr->a_name, attr_buf, length,
					   flags) < 0) {
				return(-1);
			}
		}
	} while (alist->al_more);
	return(0);
}
