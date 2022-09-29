/*
 * $Id: filesys.h,v 1.6 1997/10/07 05:50:11 chatz Exp $
 */

#include <mntent.h>
#include <sys/statfs.h>

typedef struct fsinst_t {		/* Filesystem instance */
    int		    inst;		/* Instance identifier */
    char	    *fsname;
    char	    *mntdir;
    struct statfs   statfs;
    struct {
	unsigned int	mounted : 1;	/* Is filesystem mounted? */
	unsigned int	fetched : 1;	/* Fetched in current request already? */
	unsigned int	used	: 1;	/* Is filesystem in use or gone away */
    } status;
} fsinst_t;

extern fsinst_t *fsilist;		/* Array of filesystems */
extern int	nfsi;			/* # filesystems in array */
extern int	nmfd;			/* Number of mounted filesystems */

