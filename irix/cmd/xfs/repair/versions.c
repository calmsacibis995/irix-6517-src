#ident "$Revision: 1.12 $"

#include "vsn.h"
#define _KERNEL 1
#include <sys/param.h>
#include <sys/buf.h>
#undef _KERNEL
#include <sys/uuid.h>
#if VERS >= V_64
#include <ksys/behavior.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_quota.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_dir.h>
#include <sys/fs/xfs_mount.h>

#define EXTERN
#include "versions.h"
#undef EXTERN
#include "err_protos.h"
#include "globals.h"

void
update_sb_version(xfs_mount_t *mp)
{
	xfs_sb_t	*sb;
	__uint16_t	vn;

	sb = &mp->m_sb;

	if (fs_attributes)  {
		if (!XFS_SB_VERSION_HASATTR(sb))  {
			assert(fs_attributes_allowed);

			XFS_SB_VERSION_ADDATTR(sb);
		}
	}

	if (fs_inode_nlink)  {
		if (!XFS_SB_VERSION_HASNLINK(sb))  {
			assert(fs_inode_nlink_allowed);

			XFS_SB_VERSION_ADDNLINK(sb);
		}
	}

	/*
	 * fix up the superblock version number and feature bits,
	 * turn off quota bits and flags if the filesystem doesn't
	 * have quotas.
	 */
	if (fs_quotas)  {
		if (!XFS_SB_VERSION_HASQUOTA(sb))  {
			assert(fs_quotas_allowed);

			XFS_SB_VERSION_ADDQUOTA(sb);
		}

		/*
		 * protect against stray bits in the quota flag field
		 */
		if (sb->sb_qflags & ~(XFS_UQUOTA_ACCT|XFS_UQUOTA_ENFD|
				XFS_UQUOTA_CHKD|XFS_PQUOTA_ACCT|
				XFS_PQUOTA_ENFD|XFS_PQUOTA_CHKD))  {
			/*
			 * update the incore superblock, if we're in
			 * no_modify mode, it'll never get flushed out
			 * so this is ok.
			 */
			do_warn("bogus quota flags 0x%x set in superblock",
				sb->sb_qflags & ~(XFS_UQUOTA_ACCT|
				XFS_UQUOTA_ENFD|
				XFS_UQUOTA_CHKD|XFS_PQUOTA_ACCT|
				XFS_PQUOTA_ENFD|XFS_PQUOTA_CHKD));

			sb->sb_qflags &= (XFS_UQUOTA_ACCT|
				XFS_UQUOTA_ENFD|
				XFS_UQUOTA_CHKD|XFS_PQUOTA_ACCT|
				XFS_PQUOTA_ENFD|XFS_PQUOTA_CHKD);

			if (!no_modify)
				do_warn(", bogus flags will be cleared\n");
			else
				do_warn(", bogus flags would be cleared\n");
		}
	} else  {
		sb->sb_qflags = 0;

		if (XFS_SB_VERSION_HASQUOTA(sb))  {
			lost_quotas = 1;
			vn = sb->sb_versionnum;
			vn &= ~XFS_SB_VERSION_QUOTABIT;

			if (!(vn & XFS_SB_VERSION_ALLFBITS))
				vn = XFS_SB_VERSION_TOOLD(vn);
			
			assert(vn != 0);
			sb->sb_versionnum = vn;
		}
	}

	if (!fs_aligned_inodes)  {
		if (XFS_SB_VERSION_HASALIGN(sb))  {
			if (XFS_SB_VERSION_NUM(sb) == XFS_SB_VERSION_4)
				XFS_SB_VERSION_SUBALIGN(sb);
		}
	}

	return;
}

/*
 * returns 0 if things are fine, 1 if we don't understand
 * this superblock version.  Sets superblock geometry-dependent
 * global variables.
 */
int
parse_sb_version(xfs_sb_t *sb)
{
	int issue_warning;

	fs_attributes = 0;
	fs_inode_nlink = 0;
	fs_quotas = 0;
	fs_aligned_inodes = 0;
	fs_sb_feature_bits = 0;
	fs_ino_alignment = 0;
	fs_has_extflgbit = 0;
	have_uquotino = 0;
	have_pquotino = 0;
	issue_warning = 0;

	/*
	 * ok, check to make sure that the sb isn't newer
	 * than we are
	 */
	if (XFS_SB_VERSION_HASEXTFLGBIT(sb))  {
		fs_has_extflgbit = 1;
		if (!fs_has_extflgbit_allowed)  {
			issue_warning = 1;
			do_warn(
			   "This filesystem has uninitialized extent flags.\n");
		}
	}

	if (XFS_SB_VERSION_HASSHARED(sb))  {
		fs_shared = 1;
		if (!fs_shared_allowed)  {
			issue_warning = 1;
			do_warn("This filesystem is marked shared.\n");
		}
	}

	if (issue_warning)  {
		do_warn(
"This filesystem uses 6.5 feature(s) not yet supported in this release.\n\
Please run a 6.5 version of xfs_repair.\n");
		return(1);
	}

	if (!XFS_SB_GOOD_VERSION(sb))  {
		do_warn(
	"WARNING:  unknown superblock version %d\n", XFS_SB_VERSION_NUM(sb));
		do_warn(
	"This filesystem contains features not understood by this program.\n");
		return(1);
	}

	if (XFS_SB_VERSION_NUM(sb) == XFS_SB_VERSION_4)  {
		if (!fs_sb_feature_bits_allowed)  {
			do_warn(
	"WARNING:  you have disallowed superblock feature bits disallowed\n");
			do_warn(
	"\tbut this superblock has feature bits.  The superblock\n");

			if (!no_modify)  {
				do_warn(
	"\twill be downgraded.  This may cause loss of filesystem meta-data\n");
			} else   {
				do_warn(
	"\twould be downgraded.  This might cause loss of filesystem\n");
				do_warn(
	"\tmeta-data.\n");
			}
		} else   {
			fs_sb_feature_bits = 1;
		}
	}

	if (XFS_SB_VERSION_HASATTR(sb))  {
		if (!fs_attributes_allowed)  {
			do_warn(
	"WARNING:  you have disallowed attributes but this filesystem\n");
			if (!no_modify)  {
				do_warn(
	"\thas attributes.  The filesystem will be downgraded and\n");
				do_warn(
	"\tall attributes will be removed.\n");
			} else  {
				do_warn(
	"\thas attributes.  The filesystem would be downgraded and\n");
				do_warn(
	"\tall attributes would be removed.\n");
			}
		} else   {
			fs_attributes = 1;
		}
	}

	if (XFS_SB_VERSION_HASNLINK(sb))  {
		if (!fs_inode_nlink_allowed)  {
			do_warn(
	"WARNING:  you have disallowed version 2 inodes but this filesystem\n");
			if (!no_modify)  {
				do_warn(
	"\thas version 2 inodes.  The filesystem will be downgraded and\n");
				do_warn(
	"\tall version 2 inodes will be converted to version 1 inodes.\n");
				do_warn(
	"\tThis may cause some hard links to files to be destroyed\n");
			} else  {
				do_warn(
	"\thas version 2 inodes.  The filesystem would be downgraded and\n");
				do_warn(
	"\tall version 2 inodes would be converted to version 1 inodes.\n");
				do_warn(
	"\tThis might cause some hard links to files to be destroyed\n");
			}
		} else   {
			fs_inode_nlink = 1;
		}
	}

	if (XFS_SB_VERSION_HASQUOTA(sb))  {
		if (!fs_quotas_allowed)  {
			do_warn(
	"WARNING:  you have disallowed quotas but this filesystem\n");
			if (!no_modify)  {
				do_warn(
	"\thas quotas.  The filesystem will be downgraded and\n");
				do_warn(
	"\tall quota information will be removed.\n");
			} else  {
				do_warn(
	"\thas quotas.  The filesystem would be downgraded and\n");
				do_warn(
	"\tall quota information would be removed.\n");
			}
		} else   {
			fs_quotas = 1;

			if (sb->sb_uquotino != 0 &&
					sb->sb_uquotino != NULLFSINO)
				have_uquotino = 1;

			if (sb->sb_pquotino != 0 &&
					sb->sb_pquotino != NULLFSINO)
				have_pquotino = 1;
		}
	}

	if (XFS_SB_VERSION_HASALIGN(sb))  {
		if (fs_aligned_inodes_allowed)  {
			fs_aligned_inodes = 1;
			fs_ino_alignment = sb->sb_inoalignmt;
		} else   {
			do_warn(
	"WARNING:  you have disallowed aligned inodes but this filesystem\n");
			if (!no_modify)  {
				do_warn(
	"\thas aligned inodes.  The filesystem will be downgraded.\n");
				do_warn(
"\tThis will permanently degrade the performance of this filesystem.\n");
			} else  {
				do_warn(
	"\thas aligned inodes.  The filesystem would be downgraded.\n");
				do_warn(
"\tThis would permanently degrade the performance of this filesystem.\n");
			}
		}
	}

	/*
	 * calculate maximum file offset for this geometry
	 */
	fs_max_file_offset = 0x7fffffffffffffffLL >> sb->sb_blocklog;

	return(0);
}
