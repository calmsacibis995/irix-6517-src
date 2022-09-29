/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.7 $"


#include <stdlib.h>
#include <bstring.h>
#include <sys/errno.h>
#include <sys/syssgi.h>
#include <sys/handle.h>


extern	int	errno;

/* just pick a value we know is more than big enough */
#define	MAXHANSIZ	64

/*
 *  The actual content of a handle is supposed to be opaque here.
 *  But, to do handle_to_fshandle, we need to know what it is.  Sigh.
 *  However we can get by with knowing only that the first 8 bytes of
 *  a file handle are the file system ID, and that a file system handle
 *  consists of only those 8 bytes.
 */

#define	FSIDSIZE	8

typedef union {
	int	fd;
	char	*path;
} comarg_t;

int	obj_to_handle (
		int		opcode,
		comarg_t	obj,
		void		**hanp,
		size_t		*hlen);


int
path_to_handle (
	char		*path,		/* input,  path to convert */
	void		**hanp,		/* output, pointer to data */
	size_t		*hlen)		/* output, size of returned data */
{
	comarg_t	obj;

	obj.path = path;
	return obj_to_handle (SGI_PATH_TO_HANDLE, obj, hanp, hlen);
}

int
path_to_fshandle (
	char		*path,		/* input,  path to convert */
	void		**hanp,		/* output, pointer to data */
	size_t		*hlen)		/* output, size of returned data */
{
	comarg_t	obj;

	obj.path = path;
	return obj_to_handle (SGI_PATH_TO_FSHANDLE, obj, hanp, hlen);
}

int
fd_to_handle (
	int		fd,		/* input,  file descriptor */
	void		**hanp,		/* output, pointer to data */
	size_t		*hlen)		/* output, size of returned data */
{
	comarg_t	obj;

	obj.fd = fd;
	return obj_to_handle (SGI_FD_TO_HANDLE, obj, hanp, hlen);
}

int
handle_to_fshandle (
	void		*hanp,
	size_t		hlen,
	void		**fshanp,
	size_t		*fshlen)
{
	if (hlen < FSIDSIZE)
		return EINVAL;
	*fshanp = malloc (FSIDSIZE);
	if (*fshanp == NULL)
		return ENOMEM;
	*fshlen = FSIDSIZE;
	bcopy (hanp, *fshanp, FSIDSIZE);
	return 0;
}

int
obj_to_handle (
	int		opcode,
	comarg_t	obj,
	void		**hanp,
	size_t		*hlen)
{
	char		hbuf [MAXHANSIZ];
	int		ret;

	if (opcode == SGI_FD_TO_HANDLE)
		ret = (int) syssgi (opcode, obj.fd, hbuf, hlen);
	else
		ret = (int) syssgi (opcode, obj.path, hbuf, hlen);
	if (ret)
		return ret;
	*hanp = malloc (*hlen);	
	if (*hanp == NULL) {
		errno = ENOMEM;
		return -1;
	}
	bcopy (hbuf, *hanp, (int) *hlen);
	return 0;
}

int
open_by_handle (
	void		*hanp,
	size_t		hlen,
	int		rw)
{
	return (int) syssgi (SGI_OPEN_BY_HANDLE, hanp, hlen, rw);
}

int
readlink_by_handle (
	void		*hanp,
	size_t		hlen,
	void		*buf,
	size_t		bufsiz)
{
	return (int) syssgi (SGI_READLINK_BY_HANDLE, hanp, hlen, buf, bufsiz);
}

int
attr_multi_by_handle(
	void		*hanp,
	size_t		hlen,
	void		*buf,
	int		rtrvcnt,
	int		flags)
{
	return (int) syssgi (SGI_ATTR_MULTI_BY_HANDLE, hanp, hlen, buf, rtrvcnt, 
									flags);
}

int
attr_list_by_handle(
	void		*hanp,
	size_t		hlen,
	char		*buf,
	size_t		bufsiz,
	int		flags,
	struct attrlist_cursor *cursor)
{
	return (int) syssgi(SGI_ATTR_LIST_BY_HANDLE, hanp, hlen, buf, bufsiz, 
								flags, cursor);
}

int
fssetdm_by_handle(
	void            *hanp,
	size_t          hlen,
	struct	fsdmidata *fssetdm)
{
	return (int) syssgi(SGI_FSSETDM_BY_HANDLE, hanp, hlen, fssetdm);
}
	

/*ARGSUSED*/
void
free_handle (
	void		*hanp,
	size_t		hlen)
{
	free (hanp);
}
