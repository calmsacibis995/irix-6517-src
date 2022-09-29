/* 
 * Copyright 1993, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ifdef __STDC__
	#pragma weak acl_from_text = _acl_from_text
	#pragma weak acl_to_short_text = _acl_to_short_text
	#pragma weak acl_to_text = _acl_to_text
	#pragma weak acl_size = _acl_size
	#pragma weak acl_copy_ext = _acl_copy_ext
	#pragma weak acl_copy_int = _acl_copy_int
	#pragma weak acl_free = _acl_free
	#pragma weak acl_valid = _acl_valid
	#pragma weak acl_delete_def_file = _acl_delete_def_file
	#pragma weak acl_get_fd = _acl_get_fd
	#pragma weak acl_get_file = _acl_get_file
	#pragma weak acl_set_fd = _acl_set_fd
	#pragma weak acl_set_file = _acl_set_file
	#pragma weak acl_dup = _acl_dup
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <sys/acl.h>
#include <sys/syssgi.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>

#define LONG_FORM	0
#define SHORT_FORM	1

#define TAG		0
#define UID		1
#define PERM		2

#define MAX_ENTRY_SIZE	30

static char *
skip_white(char *s)
{
	char *cp;

	if (*s != ' ')
		return s;
	for (cp = s; *cp == ' '; cp++)
		;
	*s = '\0';
	return cp;
}

static char *
skip_to_white(char *s)
{
	while (*s != '\0' && *s != ' ')
		s++;
	return s;
}

static char *
skip_separator(char *s)
{
	char *cp;

	for (cp = s; *cp == ' '; cp++)
		;

	if (*cp++ != ':')
		return NULL;

	for (; *cp == ' '; cp++)
		;

	*s = '\0';
	return cp;
}

static char *
skip_to_separator(char *s)
{
	while (*s != '\0' && *s != ' ' && *s != ':')
		s++;
	return s;
}

/* 
 * Translate "rwx" into internal representations
 */
static int
get_perm (char *perm, acl_perm_t *p)
{
	*p = (acl_perm_t)0;
	if (perm == NULL)
		return 0;

	while (*perm) {
		switch (*perm++) {
		case '-':
			break;
		case 'r':
			*p |= ACL_READ;
			break;
		case 'w':
			*p |= ACL_WRITE;
			break;
		case 'x':
			*p |= ACL_EXECUTE;
			break;
		default:
			return -1;
		}
	}
	return 0;
}

static void
acl_error (void *b0, void *b1, int e)
{

	if (b0)
		free (b0);
	if (b1)
		free (b1);
	setoserror (e);
}

/* 
 * Converts either long or short text form of ACL into internal representations
 *      Input long text form lines are either
 *                      #.....\n
 *              or 
 *                      []<tag>[]:[]<uid>[]:[]<perm>[][#....]\n
 *      short text form is
 *                      <tag>:<uid>:<perm>
 *      returns a pointer to ACL
 */
struct acl *
acl_from_text (const char *buf_p)
{
	struct passwd *pw;
	struct group *gr;
	struct acl *aclbuf;
	char *bp, *fp;
	char c;

	/* check args for bogosity */
	if (buf_p == NULL || *buf_p == '\0') {
		acl_error (NULL, NULL, EINVAL);
		return (NULL);
	}

	/* allocate copy of acl text */
	if ((bp = strdup(buf_p)) == NULL) {
		acl_error (NULL, NULL, ENOMEM);
		return (NULL);
	}

	/* allocate ourselves an acl */
	if ((aclbuf = (acl_t)malloc(sizeof (*aclbuf))) == NULL) {
		acl_error(bp, NULL, ENOMEM);
		return (NULL);
	}
	aclbuf->acl_cnt = 0;

	/* Clear out comment lines and translate newlines to blanks */
	for (fp = bp, c = '\0'; *fp != '\0'; fp++) {
		if (*fp == '\t' || *fp == ',')
			*fp = ' ';
		else if (*fp == '#' || *fp == '\n')
			c = *fp;
		if (c) {
			*fp = ' ';
			if (c == '\n')
				c = '\0';
		}
	}

	/* while not at the end of the text buffer */
	for (fp = skip_white(bp); fp != NULL && *fp != '\0'; ) {
		acl_entry_t entry;
		char *tag, *qa, *perm;

		if (aclbuf->acl_cnt > ACL_MAX_ENTRIES) {
			acl_error (bp, aclbuf, EINVAL);
			return (NULL);
		}

		/* get tag */
		tag = fp;
		fp = skip_to_separator(tag);

		if (*fp == '\0' && aclbuf->acl_cnt != 0)
			break;

		/* get qualifier */
		if ((qa = skip_separator(fp)) == NULL) {
			acl_error (bp, aclbuf, EINVAL);
			return (NULL);
		}
		if (*qa == ':') {
			/* e.g. u::rwx */
			fp = qa;
			qa = NULL;
		}
		else {
			/* e.g. u:fred:rwx */
			fp = skip_to_separator(qa);
		}

		/* get permissions */
		if ((perm = skip_separator(fp)) == NULL) {
			acl_error (bp, aclbuf, EINVAL);
			return (NULL);
		}
		fp = skip_to_white(perm);
		fp = skip_white(fp);


		entry = &aclbuf->acl_entry[aclbuf->acl_cnt++];
		entry->ae_id = 0;
		entry->ae_tag = -1;

		/* Process "user" tag keyword */
		if (!strcmp(tag, "user") || !strcmp(tag, "u")) {
			if (qa == NULL || *qa == '\0')
				entry->ae_tag = ACL_USER_OBJ;
			else {
				entry->ae_tag = ACL_USER;
				if (pw = getpwnam(qa))
					entry->ae_id = pw->pw_uid;
				else if (isdigit(*qa))
					entry->ae_id = atoi(qa);
				else {
					acl_error (bp, aclbuf, EINVAL);
					return (NULL);
				}
			}
		}

		/* Process "group" tag keyword */
		if (!strcmp(tag, "group") || !strcmp(tag, "g")) {
			if (qa == NULL || *qa == '\0')
				entry->ae_tag = ACL_GROUP_OBJ;
			else {
				entry->ae_tag = ACL_GROUP;
				if (gr = getgrnam (qa))
					entry->ae_id = gr->gr_gid;
				else if (isdigit (*qa))
					entry->ae_id = atoi(qa);
				else {
					acl_error (bp, aclbuf, EINVAL);
					return (NULL);
				}
			}
		}

		/* Process "other" tag keyword */
		if (!strcmp(tag, "other") || !strcmp(tag, "o")) {
			entry->ae_tag = ACL_OTHER_OBJ;
			if (qa != NULL && *qa != '\0') {
				acl_error (bp, aclbuf, EINVAL);
				return (NULL);
			}
		}

		/* Process "mask" tag keyword */
		if (!strcmp(tag, "mask") || !strcmp(tag, "m")) {
			entry->ae_tag = ACL_MASK;
			if (qa != NULL && *qa != '\0') {
				acl_error (bp, aclbuf, EINVAL);
				return (NULL);
			}
		}

		/* Process invalid tag keyword */
		if (entry->ae_tag == -1) {
			acl_error(bp, aclbuf, EINVAL);
			return (NULL);
		}

		if (get_perm (perm, &entry->ae_perm) == -1) {
			acl_error ((void *) bp, (void *) aclbuf, EINVAL);
			return (NULL);
		}
	}
	free (bp);
	return (aclbuf);
}

enum acl_tt {TT_USER, TT_GROUP, TT_OTHER, TT_MASK};

static char *
acl_to_text_internal (struct acl *aclp, ssize_t *len_p, const char *strs[],
		      int isshort)
{
	int i, buflen, s;
	char *buf, *c, delim;
	acl_entry_t entry;

	if (acl_valid(aclp) == -1)
		return (char *) 0;

	buflen = aclp->acl_cnt * MAX_ENTRY_SIZE;
	if (!(c = buf = (char *) malloc (buflen)))
	{
		acl_error ((void *) 0, (void *) 0, ENOMEM);
		return (char *) 0;
	}

	for (i = 0, delim = (isshort ? ',' : '\n'); i < aclp->acl_cnt; i++)
	{
		if (buflen - (c - buf) < MAX_ENTRY_SIZE)
		{
			acl_error ((void *) buf, (void *) 0, ENOMEM);
			return (char *) 0;
		}

		entry = &aclp->acl_entry[i];

		switch (entry->ae_tag)
		{
			case ACL_USER_OBJ:
				s = sprintf (c, "%s:", strs[TT_USER]);
				break;
			case ACL_USER: {
				struct passwd *pw;
				if (pw = getpwuid (entry->ae_id))
					s = sprintf (c, "%s%s:", strs[TT_USER],
						     pw->pw_name);
				else
					s = sprintf (c, "%s%d:", strs[TT_USER],
						     entry->ae_id);
				break;
			}
			case ACL_GROUP_OBJ:
				s = sprintf (c, "%s:", strs[TT_GROUP]);
				break;
			case ACL_GROUP: {
				struct group *gr;
				if (gr = getgrgid (entry->ae_id))
					s = sprintf (c, "%s%s:",
						     strs[TT_GROUP],
						     gr->gr_name);
				else
					s = sprintf (c, "%s%d:",
						     strs[TT_GROUP],
						     entry->ae_id);
				break;
			}
			case ACL_OTHER_OBJ:
				s = sprintf (c, "%s:", strs[TT_OTHER]);
				break;
			case ACL_MASK:
				s = sprintf (c, "%s:", strs[TT_MASK]);
				break;
			default:
				acl_error ((void *) buf, (void *) 0, EINVAL);
				return (char *) 0;
		}
		c += s;
		*c++ = (entry->ae_perm & ACL_READ) ? 'r' : '-';
		*c++ = (entry->ae_perm & ACL_WRITE) ? 'w' : '-';
		*c++ = (entry->ae_perm & ACL_EXECUTE) ? 'x' : '-';
		*c++ = delim;
	}
	if (isshort)
		*--c = '\0';
	else
		*c = '\0';
	if (len_p)
		*len_p = (ssize_t) (c - buf);
	return buf;
}

/* 
 * Translate an ACL to short text form.
 *      Inputs are a pointer to an ACL
 *                 a pointer to the converted text buffer size
 *      Output is the pointer to the text buffer
 */
char *
acl_to_short_text (struct acl *aclp, ssize_t *len_p)
{
	static const char *strs[] = {"u:", "g:", "o:", "m:"};
	return acl_to_text_internal (aclp, len_p, strs, 1);
}

/* 
 * Translate an ACL to long text form.
 *      Inputs are a pointer to an ACL
 *                 a pointer to the converted text buffer size
 *      Output is the pointer to the text buffer
 */
char *
acl_to_text (struct acl *aclp, ssize_t *len_p)
{
	static const char *strs[] = {"user:", "group:", "other:", "mask:"};
	return acl_to_text_internal (aclp, len_p, strs, 0);
}

ssize_t
acl_size (struct acl *aclp)
{
	if (aclp == NULL)
	{
		setoserror (EINVAL);
		return ((ssize_t) (-1));
	}
	return (sizeof (*aclp));
}

/* 
 * For now, the internal and external ACL are the same.
 */
ssize_t
acl_copy_ext (void *buf_p, struct acl *acl, ssize_t size)
{
	if (size <= 0 || !acl || !buf_p)
	{
		acl_error ((void *) 0, (void *) 0, EINVAL);
		return (ssize_t) - 1;
	}

	if (size < sizeof (struct acl))
	{
		acl_error ((void *) 0, (void *) 0, ERANGE);
		return (ssize_t) - 1;
	}

	*(struct acl *) buf_p = *acl;
	return (sizeof (*acl));
}

/* 
 * For now, the internal and external ACL are the same.
 */
struct acl *
acl_copy_int (const void *buf_p)
{
	struct acl *aclp;

	if (buf_p == NULL)
	{
		acl_error ((void *) NULL, (void *) NULL, EINVAL);
		return (NULL);
	}

	aclp = (struct acl *) malloc (sizeof (*aclp));
	if (aclp == NULL)
	{
		acl_error ((void *) NULL, (void *) NULL, ENOMEM);
		return (aclp);
	}

	*aclp = *(struct acl *) buf_p;
	return aclp;
}

int
acl_free (void *obj_p)
{
	if (obj_p)
		free (obj_p);
	return 0;
}

/* 
 * Validate an ACL
 */
int
acl_valid (struct acl *aclp)
{
	struct acl_entry *entry, *e;
	int user = 0, group = 0, other = 0, mask = 0, mask_required = 0;
	int i, j;

	if (aclp == NULL)
		goto acl_invalid;

	if (aclp->acl_cnt > ACL_MAX_ENTRIES)
		goto acl_invalid;

	for (i = 0; i < aclp->acl_cnt; i++)
	{

		entry = &aclp->acl_entry[i];

		switch (entry->ae_tag)
		{
			case ACL_USER_OBJ:
				if (user++)
					goto acl_invalid;
				break;
			case ACL_GROUP_OBJ:
				if (group++)
					goto acl_invalid;
				break;
			case ACL_OTHER_OBJ:
				if (other++)
					goto acl_invalid;
				break;
			case ACL_USER:
			case ACL_GROUP:
				for (j = i + 1; j < aclp->acl_cnt; j++)
				{
					e = &aclp->acl_entry[j];
					if (e->ae_id == entry->ae_id && e->ae_tag == entry->ae_tag)
						goto acl_invalid;
				}
				mask_required++;
				break;
			case ACL_MASK:
				if (mask++)
					goto acl_invalid;
				break;
			default:
				goto acl_invalid;
		}
	}
	if (!user || !group || !other || (mask_required && !mask))
		goto acl_invalid;
	else
		return 0;
acl_invalid:
	setoserror (EINVAL);
	return (-1);
}

/* 
 * Delete a default ACL by filename.
 */
int
acl_delete_def_file (const char *path_p)
{
	struct acl acl;

	acl.acl_cnt = ACL_NOT_PRESENT;
	if (syssgi (SGI_ACL_SET, path_p, -1, 0, &acl) == -1)
		return (-1);
	else
		return 0;
}

/* 
 * Get an ACL by file descriptor.
 */
struct acl *
acl_get_fd (int fd)
{
	struct acl *aclp = (struct acl *) malloc (sizeof (*aclp));

	if (aclp == NULL)
	{
		setoserror (ENOMEM);
		return (aclp);
	}

	if (syssgi (SGI_ACL_GET, 0, fd, aclp, 0) == -1)
	{
		free ((void *) aclp);
		return (NULL);
	}
	else
		return aclp;
}

/* 
 * Get an ACL by filename.
 */
struct acl *
acl_get_file (const char *path_p, acl_type_t type)
{
	struct acl *aclp = (struct acl *) malloc (sizeof (*aclp));
	int acl_get_error;

	if (aclp == NULL)
	{
		setoserror (ENOMEM);
		return (NULL);
	}

	switch (type)
	{
		case ACL_TYPE_ACCESS:
			acl_get_error = (int) syssgi (SGI_ACL_GET, path_p, -1,
						      aclp,
						      (struct acl *) NULL);
			break;
		case ACL_TYPE_DEFAULT:
			acl_get_error = (int) syssgi (SGI_ACL_GET, path_p, -1,
						      (struct acl *) NULL,
						      aclp);
			break;
		default:
			setoserror (EINVAL);
			acl_get_error = -1;
	}

	if (acl_get_error == -1)
	{
		free ((void *) aclp);
		return (NULL);
	}
	else
		return aclp;
}

/* 
 * Set an ACL by file descriptor.
 */
int
acl_set_fd (int fd, struct acl *aclp)
{
	if (acl_valid (aclp) == -1)
	{
		setoserror (EINVAL);
		return (-1);
	}

	if (aclp->acl_cnt > ACL_MAX_ENTRIES)
	{
/* setoserror(EACL2BIG);                until EACL2BIG is defined */
		return (-1);
	}

	if (syssgi (SGI_ACL_SET, 0, fd, aclp, (struct acl *) NULL) == -1)
		return (-1);
	else
		return 0;
}

/* 
 * Set an ACL by filename.
 */
int
acl_set_file (const char *path_p, acl_type_t type, struct acl *aclp)
{
	int acl_set_error;

	if (acl_valid (aclp) == -1)
	{
		setoserror (EINVAL);
		return (-1);
	}

	if (aclp->acl_cnt > ACL_MAX_ENTRIES)
	{
/* setoserror(EACL2BIG);                until EACL2BIG is defined */
		return (-1);
	}

	switch (type)
	{
		case ACL_TYPE_ACCESS:
			acl_set_error = (int) syssgi (SGI_ACL_SET, path_p, -1,
						      aclp,
						      (struct acl *) NULL);
			break;
		case ACL_TYPE_DEFAULT:
			acl_set_error = (int) syssgi (SGI_ACL_SET, path_p, -1,
						      (struct acl *) NULL,
						      aclp);
			break;
		default:
			setoserror (EINVAL);
			return (-1);
	}
	if (acl_set_error == -1)
		return (-1);
	else
		return 0;
}

acl_t
acl_dup (acl_t acl)
{
	acl_t dup = (acl_t) NULL;

	if (acl != (acl_t) NULL)
	{
		dup = (acl_t) malloc (sizeof (*acl));
		if (dup != (acl_t) NULL)
			*dup = *acl;
		else
			setoserror (ENOMEM);
	}
	else
		setoserror (EINVAL);
	return (dup);
}
