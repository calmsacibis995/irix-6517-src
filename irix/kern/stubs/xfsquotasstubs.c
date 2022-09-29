/*
 * XFS Disk Quota stubs
 */
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/quota.h>
#include <sys/fs/xfs_types.h>

struct xfs_qm *xfs_Gqm = NULL;
mutex_t	xfs_Gqm_lock;

struct xfs_qm   *xfs_qm_init() {return NULL;}

void 		xfs_qm_destroy() { return; }
int		xfs_qm_dqflush_all() { return nopkg(); }
int		xfs_qm_dqattach() { return nopkg(); }
int		xfs_quotactl() { return nopkg(); }
int		xfs_qm_dqpurge_all() { return nopkg(); }
void		xfs_qm_mount_quotainit() { return; }
void		xfs_qm_unmount_quotadestroy() { return; }
int		xfs_qm_mount_quotas() { return nopkg(); }
int 		xfs_qm_unmount_quotas() { return nopkg(); }
void		xfs_qm_dqdettach_inode() { return; }
int 		xfs_qm_sync() { return nopkg(); }
int		xfs_qm_check_inoquota() { return nopkg(); }
int		xfs_qm_check_inoquota2() { return nopkg(); }
void		xfs_qm_dqrele_all_inodes() { return; }

/*
 * dquot interface
 */
void		xfs_dqlock() { return; }
void		xfs_dqunlock() { return; }
void		xfs_dqunlock_nonotify() { return; }
void		xfs_dqlock2() {return;}
void 		xfs_qm_dqput() { return; }
void 		xfs_qm_dqrele() { return; }
int		xfs_qm_dqid() { return -1; }
int		xfs_qm_dqget() { return nopkg(); }	
int 		xfs_qm_dqcheck() { return nopkg(); }

/*
 * Transactions
 */
void 		xfs_trans_alloc_dqinfo() { return; }
void 		xfs_trans_free_dqinfo() { return; }
void		xfs_trans_dup_dqinfo() { return; }
void		xfs_trans_mod_dquot() { return; }
int		xfs_trans_mod_dquot_byino() { return nopkg(); }
void		xfs_trans_apply_dquot_deltas() { return; }
void		xfs_trans_unreserve_and_mod_dquots() { return; }
int		xfs_trans_reserve_quota_nblks() { return nopkg(); }
int		xfs_trans_reserve_quota_bydquots() { return nopkg(); }
void		xfs_trans_log_dquot() { return; }
void		xfs_trans_dqjoin() { return; }

/* 
 * Vnodeops Utility Functions
 */

struct xfs_dquot *xfs_qm_vop_chown() { return NULL; }
int		xfs_qm_vop_dqalloc() { return nopkg(); }
int		xfs_qm_vop_chown_dqalloc() { return nopkg(); }
int		xfs_qm_vop_chown_reserve() { return nopkg(); }
int		xfs_qm_vop_rename_dqattach() { return nopkg(); }
void		xfs_qm_vop_dqattach_and_dqmod_newinode() { return; }


