/*
 *	gfs.h
 *
 *	Description:
 *		Stuff for generic file system support
 *
 *	History:
 *      rogerc      01/23/91    Created
 */
#include <rpc/types.h>

typedef struct fhandle fhandle_t;

#include <sys/fs/nfs_clnt.h>

enum fstype { ISO9660 };

#define	FHPADSIZE	(NFS_FHSIZE - sizeof(struct loopfid) - 4)

#ifndef NFSMNT_LOOPBACK
#define	NFSMNT_LOOPBACK	0x4000	/* local mount via loopback interface */
struct loopfid {
        u_short lfid_len;       /* bytes in fid, excluding lfid_len */
        u_short lfid_pad;       /* explicit struct padding */
        u_long  lfid_fno;       /* file number, from nodeid attribute */
};
#endif

struct fhandle {
	dev_t	fh_dev;				/* device number */
	u_short	fh_align;			/* alignment padding, must be zero */
	struct loopfid fh_fid;
	char fh_pad[FHPADSIZE];
    };

#define fh_fno fh_fid.lfid_fno
#define fh_len fh_fid.lfid_len

typedef struct gfs {
	/*
	 *	gfs_addfs sets these fields
	 */
	struct gfs	*gfs_next;		/* next in list */
	enum fstype	gfs_type;		/* type of file system */
	char		*gfs_pathname;	/* device pathname */
	char		*gfs_mntpnt;	/* Mount point */
	/*
	 *	fs dependent addfs must set gfs_dev, and has gfs_fs to do
	 *	with as it pleases.  gfs_dev must be unique among all file systems
	 *	of the applicable type.
	 */
	dev_t		gfs_dev;		/* filesystem's device */
	fhandle_t	gfs_fh;			/* Root file handle */
	void		*gfs_fs;		/* pointer to file system dependent info */
} gfs_t;

int
gfs_addfs( enum fstype type, char *dev, char *mntpnt, u_int flags,
 u_int partition, gfs_t **gfsp );

void
gfs_removefs( gfs_t *gfs );

gfs_t *
gfs_getfs( dev_t dev, enum fstype type );

int
gfs_getfsbypath( char *path, gfs_t **gfsp, struct svc_req *rq );
