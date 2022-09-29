#ident	"$Revision: 1.9 $"

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/uuid.h>
#include <sys/vnode.h>
#include <ksys/behavior.h>
#include <sys/buf.h>
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_mount.h"
#include "xfs_alloc_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_ialloc.h"
#include "xfs_alloc.h"
#include "xfs_imap.h"
#include "xfs_btree.h"
#include "xfs_bmap.h"
#include "xfs_attr_sf.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode_item.h"
#include "xfs_buf_item.h"
#include "xfs_da_btree.h"
#include "xfs_inode.h"
#include "xfs_error.h"
#include "xfs_trans_priv.h"
#include "xfs_bit.h"
#include "xfs_rw.h"
#include "xfs_clnt.h"
#include "xfs_quota.h"
#include "xfs_dqblk.h"
#include "xfs_dquot_item.h"
#include "xfs_dquot.h"
#include "xfs_qm.h"
#include "xfs_quota_priv.h"
#include "xfs_itable.h"
#include "xfs_attr_leaf.h"
#include "xfs_dir_leaf.h"
#include "xfs_extfree_item.h"
#include "xfs_log_priv.h"
#include "xfs_log_recover.h"

/* Structure that contains fields that are pointers to key kernel 
 * structures). This forces the type information to be sucked into 
 * kernel the symbol table.
 */
typedef struct xfs_icrash_s {
	xfs_inode_t 			 	*xfs_icrash0;
	xfs_mount_t 		 		*xfs_icrash1;
	xfs_dquot_t 		 		*xfs_icrash2;
	xfs_trans_t 		 		*xfs_icrash3;
	xfs_inode_log_item_t 			*xfs_icrash4;
	xfs_gap_t 		 		*xfs_icrash5;
	xfs_da_state_path_t 			*xfs_icrash6;
	xfs_attr_list_context_t 		*xfs_icrash7;
	xfs_attr_leafblock_t 			*xfs_icrash8;
	xfs_attr_leaf_hdr_t 			*xfs_icrash9;
	xfs_da_blkinfo_t 			*xfs_icrash10;
	xfs_attr_leaf_map_t			*xfs_icrash11;
	xfs_attr_leaf_entry_t			*xfs_icrash12;
	xfs_attr_leaf_name_local_t		*xfs_icrash13;
	xfs_attr_leaf_name_remote_t		*xfs_icrash14;
	xfs_attr_shortform_t			*xfs_icrash15;
	xfs_attr_sf_hdr_t			*xfs_icrash16;
	xfs_attr_sf_entry_t			*xfs_icrash17;
	xfs_da_args_t				*xfs_icrash18;
	xfs_da_intnode_t 			*xfs_icrash19;
	xfs_da_state_blk_t 			*xfs_icrash20;
	xfs_agf_t               		*xfs_icrash21;
	xfs_alloc_arg_t				*xfs_icrash22;
	xfs_buf_log_item_t			*xfs_icrash23;
	xfs_bmalloca_t				*xfs_icrash24;
	xfs_bmbt_rec_32_t			*xfs_icrash25;
	xfs_da_state_t				*xfs_icrash26;
	xfs_dir_leafblock_t			*xfs_icrash27;
	xfs_dir_shortform_t			*xfs_icrash28;
	xfs_bmap_free_t				*xfs_icrash29;
	xfs_extent_t				*xfs_icrash30;
	xfs_efi_log_format_t			*xfs_icrash31;
	xfs_efi_log_item_t			*xfs_icrash32;
	xfs_efd_log_format_t			*xfs_icrash33;
	xfs_efd_log_item_t			*xfs_icrash34;
	xfs_imap_t				*xfs_icrash35;
	xfs_ihash_t				*xfs_icrash36;
	xfs_fid_t				*xfs_icrash37;
	xfs_fid2_t				*xfs_icrash38;
	xfs_bstime_t				*xfs_icrash39;
	xfs_bstat_t				*xfs_icrash40;
	xfs_inogrp_t				*xfs_icrash41;
	xfs_log_iovec_t				*xfs_icrash42;
	xlog_ticket_t				*xfs_icrash43;
	xlog_op_header_t			*xfs_icrash44;
	xlog_rec_header_t			*xfs_icrash45;
	xlog_iclog_fields_t			*xfs_icrash46;
	xlog_in_core_t				*xfs_icrash47;
	xlog_t					*xfs_icrash48;
	xlog_recover_item_t			*xfs_icrash49;
	xlog_recover_t				*xfs_icrash50;
	xfs_mod_sb_t				*xfs_icrash51;
	xfs_qm_t				*xfs_icrash52;
	xfs_quotainfo_t				*xfs_icrash53;
	xfs_dquot_acct_t			*xfs_icrash54;
	xfs_ail_ticket_t			*xfs_icrash55;
	xfs_item_ops_t				*xfs_icrash56;
	xfs_uaccmap_t				*xfs_icrash57;
} xfs_icrash_t;

xfs_icrash_t *xfs_icrash_struct;

/* Dummy function called by icrash_init() to ensure that symbol
 * information from this module gets included in the kernel
 * symbol table.
 */
void 				  
xfs_icrash(void)
{
}
