#ifndef _FS_XFS_DMAPI_H
#define _FS_XFS_DMAPI_H

#ident  "$Revision: 1.3 $"

/*	Values used to define the on-disk version of dm_attrname_t. All
 *	on-disk attribute names start with the 8-byte string "SGI_DMI_".
 *
 *      In the on-disk inode, DMAPI attribute names consist of the user-provided
 *      name with the DMATTR_PREFIXSTRING pre-pended.  This string must NEVER be
 *      changed.
 */

#define DMATTR_PREFIXLEN	8
#define DMATTR_PREFIXSTRING	"SGI_DMI_"

#ifdef	_KERNEL
/* Defines for determining if an event message should be sent. */
#define	DM_EVENT_ENABLED(vfsp, ip, event) ( \
	((vfsp)->vfs_flag & VFS_DMI) && \
		( ((ip)->i_d.di_dmevmask & (1 << event)) || \
		  ((ip)->i_mount->m_dmevmask & (1 << event)) ) \
	)

/*
 *	Macros to turn caller specified delay/block flags into
 *	dm_send_xxxx_event flag DM_FLAGS_NDELAY.
 */

#define	UIO_DELAY_FLAG(uiop) ((uiop->uio_fmode&(FNDELAY|FNONBLOCK)) ? \
			DM_FLAGS_NDELAY : 0)
#define AT_DELAY_FLAG(f) ((f&ATTR_NONBLOCK) ? DM_FLAGS_NDELAY : 0)


extern int
xfs_dm_send_data_event(
	dm_eventtype_t	event, 
	bhv_desc_t	*bdp,
	off_t		offset,
	size_t		length, 
	int		flags,
	vrwlock_t	*locktype);

extern int
xfs_dm_send_create_event(
	bhv_desc_t	*dir_bdp,
	char		*name,
	mode_t		new_mode,
	int		*good_event_sent);

extern int
xfs_dm_fcntl(
	bhv_desc_t	*bdp,
	void		*arg,
	int		flags,
	off_t		offset,
	cred_t		*credp,
	union rval	*rvalp);

extern int
xfs_dm_map(
	bhv_desc_t	*bdp,
        off_t           offset,
        size_t          length,
        dm_eventtype_t  max_event);

/*
 *	Function defined in xfs_vnodeops.c used by DMAPI as well as by xfs_vnodeops.c
 */
extern int
xfs_set_dmattrs(
        bhv_desc_t      *bdp,
        u_int           evmask,
        u_int16_t       state,
        cred_t          *credp);

#endif	/* _KERNEL */

#endif  /* _FS_XFS_DMAPI_H */
