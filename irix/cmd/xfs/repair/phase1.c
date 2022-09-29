#ident "$Revision: 1.15 $"

#include "vsn.h"
#define _KERNEL 1
#include <sys/param.h>
#include <sys/buf.h>
#undef _KERNEL

#include <sys/uuid.h>
#include <stdio.h>
#include <stdlib.h>
#include <bstring.h>
#if VERS >= V_64
#include <ksys/behavior.h>
#endif
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_attr.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_dir_sf.h>
#include <sys/fs/xfs_dir.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir2.h>
#include <sys/fs/xfs_dir2_sf.h>
#endif
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_inode.h>
#include <sys/fs/xfs_mount.h>

#include "globals.h"
#include "agheader.h"
#include "protos.h"
#include "err_protos.h"

/* get us a superblock */

void
no_sb(void)
{
	do_warn("Sorry, could not find valid secondary superblock\n");
	do_warn("Exiting now.\n");
	exit(1);
}

char *
alloc_ag_buf(int size)
{
	char 	*bp;

	if ((bp = memalign(MEM_ALIGN, size)) == NULL)  {
		do_error(
	"could not allocate ag header buffer (size = %d bytes)\n",
			size);
	}

	return(bp);
}

/*
 * this has got to be big enough to hold 4 sectors
 */
#define MAX_SECTSIZE		(512 * 1024)

/* ARGSUSED */
void
phase1(xfs_mount_t *mp)
{
	xfs_sb_t		*sb;
	char			*ag_bp;
	int			rval;
	extern int		is_efs_sb(char *, int);

	io_init();

	do_log("Phase 1 - find and verify superblock...\n");

	primary_sb_modified = 0;
	need_root_inode = 0;
	need_root_dotdot = 0;
	need_rbmino = 0;
	need_rsumino = 0;
	lost_quotas = 0;
	old_orphanage_ino = (xfs_ino_t) 0;

	/*
	 * get AG 0 into ag header buf
	 */
	ag_bp = alloc_ag_buf(MAX_SECTSIZE);
	sb = (xfs_sb_t *) ag_bp;

	if (get_sb(sb, 0LL, MAX_SECTSIZE, 0) == XR_EOF)  {
		do_error("error reading primary superblock\n");
	}

	/*
	 * is this really an sb, verify internal consistency
	 */
	if ((rval = verify_sb(sb, 1)) != XR_OK)  {
		do_warn("bad primary superblock - %s !!!\n",
			err_string(rval));
		if (is_efs_sb((char *) sb, BBSIZE))  {
			if (!assume_xfs)  {
				do_error(
	"found an efs filesystem.  To override, use \"-o assume_xfs\"\n");
			} else  {
				do_warn("ignoring efs superblock\n");
			}
		}
		if (!find_secondary_sb(sb))
			no_sb();
		primary_sb_modified = 1;
	} else if ((rval = verify_set_primary_sb(sb, 0,
					&primary_sb_modified)) != XR_OK)  {
		do_warn("couldn't verify primary superblock - %s !!!\n",
			err_string(rval));
		if (!find_secondary_sb(sb))
			no_sb();
		primary_sb_modified = 1;
	}
	
	if (primary_sb_modified)  {
		if (!no_modify)  {
			do_warn("writing modified primary superblock\n");
			write_primary_sb(sb, sb->sb_sectsize);
		} else  {
			do_warn("would write modified primary superblock\n");
		}
	}

	/*
	 * misc. global var initialization
	 */
	sb_ifree = sb_icount = sb_fdblocks = sb_frextents = 0;

	free(sb);

	return;
}
