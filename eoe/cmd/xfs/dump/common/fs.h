#ifndef FS_H
#define FS_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/fs.h,v 1.4 1995/07/29 03:38:43 cbullis Exp $"

/* fs - utilities for examining and manipulating file systems
 */

/* fs_info - decides if a source name describes a file system, and if
 * so returns useful information about that file system.
 *
 * returns BOOL_FALSE if srcname does not describe a file system.
 */
extern bool_t fs_info( char *fstype,		/* out: fs type (fsid.h) */
		       intgen_t fstypesz,	/* in: buffer size */
		       char *fstypedef,		/* in: default fs type */
		       char *fsdevice,		/* out: char. spec. dev. file */
		       intgen_t fsdevicesz,	/* in: buffer size */
		       char *mntpt,		/* out: where fs mounted */
		       intgen_t mntptsz,	/* in: buffer size */
		       uuid_t *fsid,		/* out: fs uuid */
		       char *srcname );		/* in: how user named the fs */

/* fs_mounted - checks if a file system is mounted at its mount point
 */
extern bool_t fs_mounted( char *fstype,
		          char *fsdevice,
		          char *mntpt,
		          uuid_t *fsid );

/* fs_getid - retrieves the uuid of the file system containing the named
 * file. returns -1 with errno set on error.
 */
extern intgen_t fs_getid( char *fullpathname, uuid_t *fsidp );

/* tells how many inos in use
 */
extern size_t fs_getinocnt( char *mnts );

#endif /* FS_H */
