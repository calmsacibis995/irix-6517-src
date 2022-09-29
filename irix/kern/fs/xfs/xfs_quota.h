#ifndef __XFS_QUOTA_H__
#define __XFS_QUOTA_H__
#ident "$Revision: 1.17 $"
/*
 * External Interface to the XFS disk quota subsystem.
 */
struct	bhv_desc;
struct  vfs;
struct  xfs_disk_dquot;
struct  xfs_dqhash;
struct  xfs_dquot;
struct  xfs_inode;
struct  xfs_mount;
struct  xfs_trans;

/* 
 * We use only 16-bit prid's in the inode, not the 64-bit version in the proc.
 * uid_t is hard-coded to 32 bits in the inode. Hence, an 'id' in a dquot is
 * 32 bits..
 */
typedef __int32_t	xfs_dqid_t;
/*
 * Eventhough users may not have quota limits occupying all 64-bits, 
 * they may need 64-bit accounting. Hence, 64-bit quota-counters,
 * and quota-limits. This is a waste in the common case, but heh ...
 */
typedef __uint64_t	xfs_qcnt_t;
typedef __uint16_t      xfs_qwarncnt_t;

/* 
 * Disk quotas status in m_qflags, and also sb_qflags. 16 bits.
 */
#define XFS_UQUOTA_ACCT	0x0001  /* user quota accounting ON */
#define XFS_UQUOTA_ENFD	0x0002  /* user quota limits enforced */
#define XFS_UQUOTA_CHKD	0x0004  /* quotacheck run on usr quotas */
#define XFS_PQUOTA_ACCT	0x0008  /* project quota accounting ON */
#define XFS_PQUOTA_ENFD	0x0010  /* proj quota limits enforced */
#define XFS_PQUOTA_CHKD	0x0020  /* quotacheck run on prj quotas */

/* 
 * Incore only flags for quotaoff - these bits get cleared when quota(s)
 * are in the process of getting turned off. These flags are in m_qflags but
 * never in sb_qflags.
 */
#define XFS_UQUOTA_ACTIVE	0x0040  /* uquotas are being turned off */
#define XFS_PQUOTA_ACTIVE	0x0080  /* pquotas are being turned off */

/*
 * Typically, we turn quotas off if we weren't explicitly asked to 
 * mount quotas. This is the mount option not to do that.
 * This option is handy in the miniroot, when trying to mount /root.
 * We can't really know what's in /etc/fstab until /root is already mounted!
 * This stops quotas getting turned off in the root filesystem everytime
 * the system boots up a miniroot.
 */
#define XFS_QUOTA_MAYBE		0x0100 /* Turn quotas on if SB has quotas on */

/*
 * Checking XFS_IS_*QUOTA_ON() while holding any inode lock guarantees
 * quota will be not be switched off as long as that inode lock is held.
 */
#define XFS_IS_QUOTA_ON(mp)  	((mp)->m_qflags & (XFS_UQUOTA_ACTIVE | \
						   XFS_PQUOTA_ACTIVE))
#define XFS_IS_UQUOTA_ON(mp)	((mp)->m_qflags & XFS_UQUOTA_ACTIVE)
#define XFS_IS_PQUOTA_ON(mp) 	((mp)->m_qflags & XFS_PQUOTA_ACTIVE)

/*
 * Flags to tell various functions what to do. Not all of these are meaningful
 * to a single function. None of these XFS_QMOPT_* flags are meant to have
 * persistent values (ie. their values can and will change between versions)
 */
#define XFS_QMOPT_DQLOCK	0x0000001 /* dqlock */
#define XFS_QMOPT_DQALLOC	0x0000002 /* alloc dquot ondisk if needed */
#define XFS_QMOPT_UQUOTA	0x0000004 /* user dquot requested */
#define XFS_QMOPT_PQUOTA	0x0000008 /* proj dquot requested */
#define XFS_QMOPT_FORCE_RES	0x0000010 /* ignore quota limits */
#define XFS_QMOPT_DQSUSER	0x0000020 /* don't cache super users dquot */
#define XFS_QMOPT_SBVERSION	0x0000040 /* change superblock version num */
#define XFS_QMOPT_QUOTAOFF	0x0000080 /* quotas are being turned off */
#define XFS_QMOPT_UMOUNTING	0x0000100 /* filesys is being unmounted */
#define XFS_QMOPT_DOLOG		0x0000200 /* log buf changes (in quotacheck) */
#define XFS_QMOPT_DOWARN        0x0000400 /* increase warning cnt if necessary */
#define XFS_QMOPT_ILOCKED	0x0000800 /* inode is already locked (excl) */
#define XFS_QMOPT_DQREPAIR	0x0001000 /* repair dquot, if damaged. */

/* 
 * flags to xfs_trans_mod_dquot to indicate which field needs to be
 * modified.
 */
#define XFS_QMOPT_RES_REGBLKS 	0x0010000
#define XFS_QMOPT_RES_RTBLKS	0x0020000
#define XFS_QMOPT_BCOUNT	0x0040000
#define XFS_QMOPT_ICOUNT	0x0080000
#define XFS_QMOPT_RTBCOUNT	0x0100000
#define XFS_QMOPT_DELBCOUNT	0x0200000
#define XFS_QMOPT_DELRTBCOUNT	0x0400000
#define XFS_QMOPT_RES_INOS	0x0800000

/*
 * flags for dqflush and dqflush_all.
 */
#define XFS_QMOPT_SYNC		0x1000000
#define XFS_QMOPT_ASYNC		0x2000000
#define XFS_QMOPT_DELWRI	0x4000000

/* 
 * flags to xfs_trans_mod_dquot.
 */
#define XFS_TRANS_DQ_RES_BLKS	XFS_QMOPT_RES_REGBLKS
#define XFS_TRANS_DQ_RES_RTBLKS	XFS_QMOPT_RES_RTBLKS
#define XFS_TRANS_DQ_RES_INOS	XFS_QMOPT_RES_INOS
#define XFS_TRANS_DQ_BCOUNT	XFS_QMOPT_BCOUNT
#define XFS_TRANS_DQ_DELBCOUNT	XFS_QMOPT_DELBCOUNT
#define XFS_TRANS_DQ_ICOUNT	XFS_QMOPT_ICOUNT
#define XFS_TRANS_DQ_RTBCOUNT	XFS_QMOPT_RTBCOUNT
#define XFS_TRANS_DQ_DELRTBCOUNT XFS_QMOPT_DELRTBCOUNT


#define XFS_QMOPT_QUOTALL	(XFS_QMOPT_UQUOTA|XFS_QMOPT_PQUOTA)
#define XFS_QMOPT_RESBLK_MASK	(XFS_QMOPT_RES_REGBLKS | XFS_QMOPT_RES_RTBLKS)

/*
 * This check is done typically without holding the inode lock;
 * that may seem racey, but it is harmless in the context that it is used.
 * The inode cannot go inactive as long a reference is kept, and 
 * therefore if dquot(s) were attached, they'll stay consistent.
 * If, for example, the ownership of the inode changes while
 * we didnt have the inode locked, the appropriate dquot(s) will be
 * attached atomically.
 */
#define XFS_NOT_DQATTACHED(mp, ip) ((XFS_IS_UQUOTA_ON(mp) &&\
				     (ip)->i_udquot == NULL) || \
				    (XFS_IS_PQUOTA_ON(mp) && \
				     (ip)->i_pdquot == NULL))

#define XFS_QM_NEED_QUOTACHECK(mp) ((XFS_IS_UQUOTA_ON(mp) && \
				     (mp->m_sb.sb_qflags & \
				      XFS_UQUOTA_CHKD) == 0) || \
				    (XFS_IS_PQUOTA_ON(mp) && \
				     (mp->m_sb.sb_qflags & \
				      XFS_PQUOTA_CHKD) == 0))

#define XFS_MOUNT_QUOTA_ALL	(XFS_UQUOTA_ACCT|XFS_UQUOTA_ENFD|\
				 XFS_UQUOTA_CHKD|XFS_PQUOTA_ACCT|\
				 XFS_PQUOTA_ENFD|XFS_PQUOTA_CHKD)
#define XFS_MOUNT_QUOTA_MASK	(XFS_MOUNT_QUOTA_ALL | XFS_UQUOTA_ACTIVE | \
				 XFS_PQUOTA_ACTIVE)

#define XFS_IS_REALTIME_INODE(ip) ((ip)->i_d.di_flags & XFS_DIFLAG_REALTIME)

/*
 * Quota Manager Interface.
 */
extern struct xfs_qm   *xfs_qm_init(void);
extern void 		xfs_qm_destroy(struct xfs_qm *xqm);
extern int		xfs_qm_dqflush_all(struct xfs_mount *, int);
extern int		xfs_qm_dqattach(struct xfs_inode *, uint);
extern int		xfs_quotactl(struct bhv_desc *, int, int, caddr_t);
extern int		xfs_qm_dqpurge_all(struct xfs_mount *, uint);
extern void		xfs_qm_mount_quotainit(struct xfs_mount *, uint);
extern void		xfs_qm_unmount_quotadestroy(struct xfs_mount *);
extern int		xfs_qm_mount_quotas(struct xfs_mount *);
extern int 		xfs_qm_unmount_quotas(struct xfs_mount *);
extern void		xfs_qm_dqdettach_inode(struct xfs_inode *ip);
extern int 		xfs_qm_sync(struct xfs_mount *mp, short flags);

/*
 * dquot interface.
 */
extern void		xfs_dqlock(struct xfs_dquot *dqp);
extern void		xfs_dqunlock(struct xfs_dquot *dqp);
extern void		xfs_dqunlock_nonotify(struct xfs_dquot *dqp);
extern void		xfs_dqlock2(struct xfs_dquot *d1,
				    struct xfs_dquot *d2);
extern void 		xfs_qm_dqput(struct xfs_dquot *dqp);
extern void 		xfs_qm_dqrele(struct xfs_dquot *dqp);
extern xfs_dqid_t	xfs_qm_dqid(struct xfs_dquot *dqp);
extern int		xfs_qm_dqget(struct xfs_mount *mp, 
				     struct xfs_inode *ip,
				      xfs_dqid_t id,
				      uint type, uint doalloc, 
				      struct xfs_dquot **O_dqpp);
extern int 		xfs_qm_dqcheck(struct xfs_disk_dquot *, 
				       xfs_dqid_t, uint, uint, char *);

/*
 * Vnodeops specific code that should actually be _in_ xfs_vnodeops.c, but
 * is here because it's nicer to keep vnodeops (therefore, XFS) lean 
 * and clean.
 */
extern struct xfs_dquot *	xfs_qm_vop_chown(struct xfs_trans *tp, 
						 struct xfs_inode *ip, 
						 struct xfs_dquot **olddq,
						 struct xfs_dquot *newdq);
extern int		xfs_qm_vop_dqalloc(struct xfs_mount	*mp,
					   struct xfs_inode	*dp,
					   uid_t	uid,
					   xfs_prid_t	prid,
					   uint		flags,
					   struct xfs_dquot	**udqpp,
					   struct xfs_dquot	**pdqpp);

extern int		xfs_qm_vop_chown_dqalloc(struct xfs_mount	*mp,
						 struct xfs_inode	*ip,
						 int			mask,
						 uid_t			uid,
						 xfs_prid_t		prid,
						 struct xfs_dquot	**udqpp,
						 struct xfs_dquot	**pdqpp);

extern int		xfs_qm_vop_chown_reserve(struct xfs_trans	*tp,
						 struct xfs_inode	*ip,
						 struct xfs_dquot	*udqp,
						 struct xfs_dquot	*pdqp,
						 uint		privileged);

extern int		xfs_qm_vop_rename_dqattach(struct xfs_inode	**i_tab);
extern void		xfs_qm_vop_dqattach_and_dqmod_newinode(
						struct xfs_trans *tp,
						struct xfs_inode *ip,
						struct xfs_dquot *udqp,	
						struct xfs_dquot *pdqp);


/*
 * Dquot Transaction interface
 */
extern void 		xfs_trans_alloc_dqinfo(struct xfs_trans *tp);
extern void 		xfs_trans_free_dqinfo(struct xfs_trans *tp);
extern void		xfs_trans_dup_dqinfo(struct xfs_trans *otp, 
					     struct xfs_trans *ntp);
extern void		xfs_trans_mod_dquot(struct xfs_trans *tp, 
					    struct xfs_dquot *dqp,
					    uint field, long delta);
extern int		xfs_trans_mod_dquot_byino(struct xfs_trans *tp, 
						  struct xfs_inode *ip,
						  uint field, long delta);
extern void		xfs_trans_apply_dquot_deltas(struct xfs_trans *tp);
extern void		xfs_trans_unreserve_and_mod_dquots(struct xfs_trans *tp);

extern int		xfs_trans_reserve_quota_nblks(struct xfs_trans *tp,
						      struct xfs_inode *ip,
						      long nblks, long ninos,
						      uint type);


extern int		xfs_trans_reserve_quota_bydquots(struct xfs_trans *tp,
							 struct xfs_dquot *udqp,
							 struct xfs_dquot *pdqp,
							 long nblks,
							 long ninos,
							 uint flags);
extern void		xfs_trans_log_dquot(struct xfs_trans *tp, 
					    struct xfs_dquot *dqp);
extern void		xfs_trans_dqjoin(struct xfs_trans *tp, 
					 struct xfs_dquot *dqp);
extern void		xfs_qm_dqrele_all_inodes(struct xfs_mount *, uint);

/* 
 * Regular disk block quota reservations 
 */
#define 	xfs_trans_reserve_blkquota(tp, ip, nblks) \
xfs_trans_reserve_quota_nblks(tp, ip, nblks, 0, XFS_QMOPT_RES_REGBLKS)
						  
#define 	xfs_trans_unreserve_blkquota(tp, ip, nblks) \
xfs_trans_reserve_quota_nblks(tp, ip, -(nblks), 0, XFS_QMOPT_RES_REGBLKS)

#define 	xfs_trans_reserve_quota(tp, udq, pdq, nb, ni, f) \
xfs_trans_reserve_quota_bydquots(tp, udq, pdq, nb, ni, f|XFS_QMOPT_RES_REGBLKS) 

#define 	xfs_trans_unreserve_quota(tp, ud, pd, b, i, f) \
xfs_trans_reserve_quota_bydquots(tp, ud, pd, -(b), -(i), f|XFS_QMOPT_RES_REGBLKS)

/*
 * Realtime disk block quota reservations 
 */
#define 	xfs_trans_reserve_rtblkquota(mp, tp, ip, nblks) \
xfs_trans_reserve_quota_nblks(tp, ip, nblks, 0, XFS_QMOPT_RES_RTBLKS)
						  
#define 	xfs_trans_unreserve_rtblkquota(tp, ip, nblks) \
xfs_trans_reserve_quota_nblks(tp, ip, -(nblks), 0, XFS_QMOPT_RES_RTBLKS)

#define 	xfs_trans_reserve_rtquota(mp, tp, uq, pq, blks, f) \
xfs_trans_reserve_quota_bydquots(mp, tp, uq, pq, blks, 0, f|XFS_QMOPT_RES_RTBLKS) 

#define 	xfs_trans_unreserve_rtquota(tp, uq, pq, blks) \
xfs_trans_reserve_quota_bydquots(tp, uq, pq, -(blks), XFS_QMOPT_RES_RTBLKS)

#endif	/* !__XFS_QUOTA_H__ */
