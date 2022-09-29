/*
 *	gfs.c
 *
 *	Description:
 *		Generic file system support
 *
 *		The reason this file is here is that for a while it was thought
 *		that mount_iso9660 should be merged with mount_dos.  Currently,
 *		(2/26/91) it doesn't look like that is going to happen, but I'm
 *		leaving this stuff the way it is in case anybody changes their
 *		mind, and also to show developers of new file system support
 *		which pieces here are iso9660 specific and which are generic.
 *
 *	History:
 *      rogerc      01/23/91    Created
 */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <exportent.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <rpc/svc.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <rpcsvc/ypclnt.h>
#include "iso.h"
#include "util.h"

/*
 * File system dependent initialization functions
 */
static int (*addfs_list[])( char *, u_int, u_int, gfs_t *) = {
	iso_addfs
};

/*
 * File system dependent termination functions
 */
static void (*removefs_list[])( gfs_t *gfs ) = {
	iso_removefs
};

static gfs_t *gfs_list = 0;

/*
 *  int
 *  gfs_addfs( enum fstype type, char *dev, char *mntpnt, u_int flags,
 *   u_int partition, gfs_t **gfsp )
 *
 *  Description:
 *		Add a generic file system to the list.  This function is responsible
 *		for setting gfs_next, gfs_type, gfs_pathname, and gfs_mntpnt.
 *
 *		The file system dependent initialization function is responsible
 *		for setting gfs_dev, gfs_fh, and gfs_fs.
 *
 *  Parameters:
 *      type		Type of file system
 *      dev			Device being mounted
 *      mntpnt		Location of mount
 *      flags		file system dependent
 *      partition	file system dependent
 *      gfsp		Gets pointer to gfs
 *
 *  Returns:
 *		0 if successful, error code otherwise.
 */

int
gfs_addfs( enum fstype type, char *dev, char *mntpnt, u_int flags,
 u_int partition, gfs_t **gfsp )
{
	gfs_t	*gfs;
	int		error;

	gfs = safe_malloc( sizeof (*gfs) );
	bzero( gfs, sizeof (*gfs) );
	gfs->gfs_type = type;

	error = (*addfs_list[type])( dev, flags, partition, gfs );

	if (error)
		return (error);

	gfs->gfs_pathname = safe_malloc( strlen( dev ) + 1 );
	strcpy( gfs->gfs_pathname, dev );

	gfs->gfs_mntpnt = safe_malloc( strlen( mntpnt ) + 1 );
	strcpy( gfs->gfs_mntpnt, mntpnt );
	
	gfs->gfs_next = gfs_list;
	gfs_list = gfs;
	*gfsp = gfs;
	return (0);
}

/*
 *  void
 *  gfs_removefs( gfs_t *gfs )
 *
 *  Description:
 *		Remove gfs from this list
 *
 *		The file system specific termination function is called to perform
 *		any clean up necessary
 *
 *  Parameters:
 *      gfs		gfs to be removed.
 */

void
gfs_removefs( gfs_t *gfs )
{
	gfs_t **gfsp;

	for (gfsp = &gfs_list; *gfsp != NULL; gfsp = &(*gfsp)->gfs_next)
		if (*gfsp == gfs) {
			*gfsp = gfs->gfs_next;
			(*removefs_list[gfs->gfs_type])( gfs );
			return;
		}

	panic( "gfs_removefs: fs %s not found\n", gfs->gfs_pathname );
}

/*
 *  gfs_t *
 *  gfs_getfs( dev_t dev, enum fstype type )
 *
 *  Description:
 *		Return a pointer to the gfs structure for this device and type
 *
 *  Parameters:
 *      dev		device number
 *      type	file system type
 *
 *  Returns:
 *		pointer to gfs_t if successful, NULL otherwise
 */

gfs_t *
gfs_getfs( dev_t dev, enum fstype type )
{
	gfs_t *gfs;

	for (gfs = gfs_list; gfs != NULL; gfs = gfs->gfs_next)
		if (gfs->gfs_dev == dev && gfs->gfs_type ==  type)
			return (gfs);
	return (NULL);
}

/*
 *  int
 *  gfs_getfsbypath( char *path, gfs_t **gfsp, struct svc_req *rq )
 *
 *  Description:
 *		Get a pointer to a gfs_t given its mount point.  Also, if rq is
 *		non-NULL, perform check to see if path should be exported to the
 *		requestor represented by rq.
 *
 *  Parameters:
 *      path	Mount point in which we're interested
 *      gfsp	received pointer to gfs_t
 *      rq		NFS service request
 *
 *  Returns:
 *		0 if successful, error code otherwise
 */

int
gfs_getfsbypath( char *path, gfs_t **gfsp, struct svc_req *rq )
{
	FILE				*fp;
	struct exportent	*xent;
	gfs_t				*gfs;
	int					error;
	struct sockaddr_in	*sin;
	struct hostent		*hp;
	char				*access, **aliases, *host;

	for (gfs = gfs_list; gfs != NULL; gfs = gfs->gfs_next)
		if (strcmp( path, gfs->gfs_mntpnt ) == 0)
			break;
	if (!gfs)
		return (ENOENT);

	if (!rq) {
		*gfsp = gfs;
		return (0);
	}

	/*
	 * Check to see if letting the requestor see this file system is
	 * allowed (someone must run exportfs(1M).
	 *
	 * Note: this code is derived from code found in cmd/sun/rpc.mountd.c
	 */
	fp = setexportent( );

	if (!fp)
		return (EACCES);

	error = EACCES;
	while (error && (xent = getexportent(fp))) {
		if (strcmp( path, xent->xent_dirname ) == 0) {
			sin = svc_getcaller( rq->rq_xprt );
			hp = gethostbyaddr( &sin->sin_addr, sizeof (sin->sin_addr),
			 AF_INET );
			/*
			 *	Derived from cmd/sun/rpc.mountd.c
			 */
			access = getexportopt( xent, ACCESS_OPT );
			if (!access)
				error = 0;
			else
				while ((host = strtok( access, ":")) != NULL) {
					access = NULL;
					if (strcasecmp( host, hp->h_name ) == 0
					 || innetgr( host, hp->h_name, NULL, _yp_domain))
						error = 0;
					else
						for (aliases = hp->h_aliases; *aliases && error;
						 aliases++)
							if (strcasecmp( host, *aliases) == 0
							 || innetgr( access, *aliases, NULL, _yp_domain ))
								error = 0;
				}

			if (error)
				access = getexportopt( xent, ROOT_OPT );
				if (access)
					while ((host = strtok( access, ":")) != NULL) {
						access = NULL;
						if (strcasecmp( host, hp->h_name ) == 0)
							error = 0;
					}
					
			if (error)
				access = getexportopt( xent, RW_OPT );
				if (access)
					while ((host = strtok( access, ":" )) != NULL) {
						access = NULL;
						if (strcasecmp( host, hp->h_name ) == 0)
							error = 0;
					}
		}
	}
	endexportent( fp );

	if (!error)
		*gfsp = gfs;

	return (error);
}
