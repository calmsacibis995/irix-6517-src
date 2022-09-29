#ifndef _XFS_HANDLE_H
#define _XFS_HANDLE_H

#ident	"$Revision: 1.9 $"

/*
 *  Ok.  This file contains stuff that defines a general handle
 *  mechanism, except that it's only used by xfs at the moment.
 *  Imagine that it's in some generic place.  The same goes for
 *  xfs_handle.c; it's would-be generic but xfs-only right now.
 */

#ifdef _KERNEL

union rval;
struct vnode;

typedef	struct handle {
	union {
		long long align;	/* force alignment of ha_fid */
		fsid_t	ha_fsid;	/* unique file system identifier */
	} ha_u;
	fid_t	ha_fid;			/* file system specific file ID */
} handle_t;

#define	ha_fsid	ha_u.ha_fsid

/*
 *  Kernel only prototypes.
 */

int	path_to_handle		(char *path, void *hbuf, size_t *hlen);
int	path_to_fshandle	(char *path, void *hbuf, size_t *hlen);
int	fd_to_handle		(int	 fd, void *hbuf, size_t *hlen);
int	open_by_handle		(void *hbuf, size_t hlen, int rw, union rval *rvp);
int	readlink_by_handle	(void *hanp, size_t hlen, void *buf, size_t bs,
				 union rval *rvp);
int	gethandle		(void *hanp, size_t hlen, handle_t *handlep);

struct vnode *	handle_to_vp (handle_t *handlep);
int		vp_to_handle (struct vnode *vp, handle_t *handlep);

struct vfs *	altgetvfs (fsid_t *fsid);
#define HANDLE_TO_VFS(h) 	altgetvfs(&((h)->ha_fsid))
/*
 *  Kernel #defines.
 */

#define	HSIZE(handle)	(((char *) &(handle).ha_fid.fid_data - \
			  (char *) &(handle)) + (handle).ha_fid.fid_len)
#define	HANDLE_CMP(h1, h2)	bcmp(h1, h2, sizeof (handle_t))

#define	FSHSIZE		sizeof (fsid_t)
#define	ISFSHANDLE	((handle)->ha_fid.fid_len == 0)

#endif	/* _KERNEL */

#ifndef _KERNEL

/*
 *  Prototypes.
 */

int	path_to_handle		(char *path, void **hanp, size_t *hlen);
int	path_to_fshandle	(char *path, void **hanp, size_t *hlen);
int	fd_to_handle		(int	 fd, void **hanp, size_t *hlen);
int	handle_to_fshandle	(void *hanp, size_t hlen, void **fshanp,
				 size_t *fshlen);
void	free_handle		(void *hanp, size_t hlen);
int	open_by_handle		(void *hanp, size_t hlen, int rw);
int	readlink_by_handle	(void *hanp, size_t hlen, void *buf, size_t bs);

#endif	/* notdef _KERNEL */

#endif	/* _XFS_HANDLE_H */
