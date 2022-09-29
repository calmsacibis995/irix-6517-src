/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.		     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _FS_SPECFS_OPS_H	/* wrapper symbol for kernel use */
#define _FS_SPECFS_OPS_H	/* subject to change without notice */

#ident	"$Revision: 1.23 $"

#ifdef	CELL_IRIX

# include <ksys/cell/handle.h>
# include <ksys/cell/relocation.h>

typedef struct specfs_handle {
	obj_handle_t	sh_obj;
	dev_t		sh_dev;
	enum vtype	sh_type;
} spec_handle_t;

# define SPEC_HANDLE_MAKE(handle, service, objid, dev, type)	\
	{							\
		(handle).sh_obj.h_service = service;		\
		(handle).sh_obj.h_objid   = objid;		\
		(handle).sh_dev           = dev;		\
		(handle).sh_type          = type;		\
	}

# define SPEC_HANDLE_TO_SERVICE(handle)	((handle).sh_obj.h_service)
# define SPEC_HANDLE_TO_OBJID(handle)	((handle).sh_obj.h_objid)

/*
 * CELL_IRIX specific prototypes.
 */
extern struct vnode *spec_vnimport(struct vnode *vp, struct cred *, int opencnt);
extern int spec_open(bhv_desc_t *bdp, struct vnode **vpp,
				      mode_t flag, struct cred *credp);


# define SPECFS_IS_LOCAL_DEVICE(Vp)	(		\
							\
	   ((Vp)->v_type == VCHR)			\
	&& (   (emajor((Vp)->v_rdev) == MM_MAJOR)	\
	    || (emajor((Vp)->v_rdev) == PRF_MAJOR)	\
	    || (emajor((Vp)->v_rdev) == ZERO_MAJOR) )	\
					)

#else	/* ! CELL_IRIX */

# ifndef _SPEC_HANDLE_T
  typedef	struct {
  	void	*objid;
  } spec_handle_t;
# define _SPEC_HANDLE_T
# endif	/* ! _SPEC_HANDLE_T */

# define SPEC_HANDLE_TO_OBJID(hndl)	((hndl).objid)

#endif	/* ! CELL_IRIX */


extern int		spec_fstype;
extern dev_t		spec_dev;
extern struct vfs	*spec_vfsp;
extern mutex_t		spec_sync_lock;
extern struct csnode	**spec_csnode_table;
extern struct lsnode	**spec_lsnode_table;


#ifdef	DEBUG
# undef STATIC
# define STATIC
#else	/* ! DEBUG */
# define STATIC static
#endif	/* ! DEBUG */


/*
 * An "ops" vector of "cs/common" functions that can be called out
 * of the "ls/local" side of the world.
 */
struct bmapval;
struct cred;
struct flid;
struct stdata;
struct stdata;
struct uio;
struct __vhandl_s;
struct vopbd;
typedef struct {
	void	(*spec_ops_cs_attach)(dev_t, vtype_t, spec_handle_t *,
				     spec_handle_t *, long *, daddr_t *,
				     struct stdata **);
	int	(*spec_ops_cs_cmp_gen)(spec_handle_t *, long);
	int	(*spec_ops_cs_get_gen)(spec_handle_t *);
	int	(*spec_ops_cs_get_opencnt)(spec_handle_t *);
	daddr_t	(*spec_ops_cs_get_size)(spec_handle_t *);
	int	(*spec_ops_cs_ismounted)(spec_handle_t *);
	void	(*spec_ops_cs_mountedflag)(spec_handle_t *, int);
	void	(*spec_ops_cs_clone)(spec_handle_t *, struct stdata *,
					spec_handle_t *, int, struct stdata **);
	int	(*spec_ops_bmap)(spec_handle_t *,
					off_t, ssize_t, int, struct cred *,
					struct bmapval *, int *);
	int	(*spec_ops_open)(spec_handle_t *, int,
					dev_t *, daddr_t *,
					struct stdata **, mode_t,
					struct cred *);
	int	(*spec_ops_read)(spec_handle_t *, int, vnode_t *,
					struct uio *, int, struct cred *,
					struct flid *);
	int	(*spec_ops_write)(spec_handle_t *, int, vnode_t *,
					struct uio *, int, struct cred *,
					struct flid *);
	int	(*spec_ops_ioctl)(spec_handle_t *, int,
					int, void *, int, struct cred *,
					int *, struct vopbd *);
	int	(*spec_ops_strgetmsg)(spec_handle_t *,
					struct strbuf *, struct strbuf *,
					unsigned char *, int *, int,
					union rval *);
	int	(*spec_ops_strputmsg)(spec_handle_t *,
					struct strbuf *, struct strbuf *,
					unsigned char, int, int);
	int	(*spec_ops_poll)(spec_handle_t *, long,
					short, int, short *,
					struct pollhead **, unsigned int *);
	void	(*spec_ops_strategy)(spec_handle_t *, struct buf *);
	int	(*spec_ops_addmap)(spec_handle_t *,
					vaddmap_t, struct __vhandl_s *,
					off_t, size_t, mprot_t, struct cred *,
					pgno_t *);
	int	(*spec_ops_delmap)(spec_handle_t *,
					struct __vhandl_s *, size_t,
					struct cred *);
	int	(*spec_ops_close)(spec_handle_t *,
					int, lastclose_t, struct cred *);
	void	(*spec_ops_teardown)(spec_handle_t *);
} spec_ls_ops_t;


#ifdef	SPECFS_STATS_DEBUG
struct	spec_stat_info	{
#ifdef	CELL_IRIX
	int	I_spec_dc_strategy;
	int	I_spec_ds_addmap;
	int	I_spec_ds_attach;
	int	I_spec_ds_bmap;
	int	I_spec_ds_clone;
	int	I_spec_ds_close;
	int	I_spec_ds_cmp_gen;
	int	I_spec_ds_delmap;
	int	I_spec_ds_get_gen;
	int	I_spec_ds_get_opencnt;
	int	I_spec_ds_get_size;
	int	I_spec_ds_ioctl;
	int	I_spec_ds_ismounted;
	int	I_spec_ds_list_read;
	int	I_spec_ds_map;
	int	I_spec_ds_mountedflag;
	int	I_spec_ds_open;
	int	I_spec_ds_poll;
	int	I_spec_ds_read;
	int	I_spec_ds_strategy;
	int	I_spec_ds_strgetmsg;
	int	I_spec_ds_strputmsg;
	int	I_spec_ds_teardown;
	int	I_spec_ds_write;
#endif	/* CELL_IRIX */
	int	make_specvp;
	int	make_specvp_locked;
	int	spec_access;
	int	spec_addmap;
	int	spec_allocstore;
	int	spec_at_free;
	int	spec_at_inactive;
	int	spec_at_insert_bhv;
	int	spec_at_getattr;
	int	spec_at_lock;
	int	spec_at_setattr;
	int	spec_at_unlock;
	int	spec_attach;
	int	spec_attr_func;
	int	spec_attr_get;
	int	spec_attr_list;
	int	spec_attr_remove;
	int	spec_attr_set;
	int	spec_bmap;
	int	spec_canonical_name_get;
	int	spec_check_vp;
	int	spec_close;
	int	spec_cmp;
	int	spec_cover;
	int	spec_create;
	int	spec_cs_addmap_subr;
	int	spec_cs_attach;
	int	spec_cs_attr_func;
	int	spec_cs_attr_get_vop;
	int	spec_cs_attr_list_vop;
	int	spec_cs_attr_remove_vop;
	int	spec_cs_attr_set_vop;
	int	spec_cs_bmap_subr;
	int	spec_cs_bmap_vop;
	int	spec_cs_canonical_name_get;
	int	spec_cs_clone;
	int	spec_cs_close_hndl;
	int	spec_cs_close_vop;
	int	spec_cs_cmp_gen;
	int	spec_cs_delete;
	int	spec_cs_delmap_subr;
	int	spec_cs_device_close;
	int	spec_cs_device_hold;
	int	spec_cs_device_rele;
	int	spec_cs_dflt_poll;
	int	spec_cs_free;
	int	spec_cs_fsync_vop;
	int	spec_cs_get;
	int	spec_cs_get_gen;
	int	spec_cs_get_opencnt;
	int	spec_cs_get_size;
	int	spec_cs_getattr_vop;
	int	spec_cs_inactive_vop;
	int	spec_cs_ioctl_hndl;
	int	spec_cs_ioctl_vop;
	int	spec_cs_ismounted;
	int	spec_cs_kill_poll;
	int	spec_cs_lock;
	int	spec_cs_mac_label_get;
	int	spec_cs_mac_pointer_get;
	int	spec_cs_makesnode;
	int	spec_cs_mark;
	int	spec_cs_mountedflag;
	int	spec_cs_open_hndl;
	int	spec_cs_open_vop;
	int	spec_cs_poll_hndl;
	int	spec_cs_poll_vop;
	int	spec_cs_read_rsync;
	int	spec_cs_read_hndl;
	int	spec_cs_read_hndl_exit;
	int	spec_cs_read_vop;
	int	spec_cs_rwlock;
	int	spec_cs_rwlock_lcl;
	int	spec_cs_rwlock_vop;
	int	spec_cs_rwunlock;
	int	spec_cs_rwunlock_lcl;
	int	spec_cs_rwunlock_vop;
	int	spec_cs_setattr_vop;
	int	spec_cs_strategy_subr;
	int	spec_cs_strgetmsg_hndl;
	int	spec_cs_strgetmsg_vop;
	int	spec_cs_strputmsg_hndl;
	int	spec_cs_strputmsg_vop;
	int	spec_cs_teardown_subr;
	int	spec_cs_unlock;
	int	spec_cs_write_hndl;
	int	spec_cs_write_hndl_exit;
	int	spec_cs_write_vop;
#ifdef	CELL_IRIX
	int	spec_dc_addmap_subr;
	int	spec_dc_attach;
	int	spec_dc_bmap_subr;
	int	spec_dc_clone;
	int	spec_dc_close_hndl;
	int	spec_dc_cmp_gen;
	int	spec_dc_delmap_subr;
	int	spec_dc_get_gen;
	int	spec_dc_get_opencnt;
	int	spec_dc_get_size;
	int	spec_dc_ioctl_hndl;
	int	spec_dc_ismounted;
	int	spec_dc_mountedflag;
	int	spec_dc_open_hndl;
	int	spec_dc_poll_hndl;
	int	spec_dc_read_hndl;
	int	spec_dc_strategy_subr;
	int	spec_dc_strgetmsg_hndl;
	int	spec_dc_strputmsg_hndl;
	int	spec_dc_teardown_subr;
	int	spec_dc_write_hndl;
#endif	/* CELL_IRIX */
	int	spec_delete;
	int	spec_delmap;
	int	spec_devsize;
	int	spec_dflt_poll;
	int	spec_dump;
	int	spec_fcntl;
	int	spec_fid2;
	int	spec_fid;
	int	spec_find;
	int	spec_fixgen;
	int	spec_flush;
	int	spec_free;
	int	spec_frlock;
	int	spec_fsync;
	int	spec_get;
	int	spec_getattr;
	int	spec_inactive;
	int	spec_insert;
	int	spec_dev_is_inuse;
	int	spec_ioctl;
	int	spec_ismounted;
	int	spec_kill_poll;
	int	spec_link;
	int	spec_link_removed;
	int	spec_lock;
	int	spec_lookup;
	int	spec_mac_label_get;
	int	spec_mac_pointer_get;
	int	spec_make_clone;
	int	spec_makesnode;
	int	spec_map;
	int	spec_mark;
	int	spec_mkdir;
	int	spec_mountedflag;
	int	spec_open;
	int	spec_pathconf;
	int	spec_poll_cell;
	int	spec_poll_noncell;
	int	spec_read;
	int	spec_read_rsync;
	int	spec_readdir;
	int	spec_readlink;
	int	spec_realvp;
	int	spec_reclaim;
	int	spec_remove;
	int	spec_rename;
	int	spec_rmdir;
	int	spec_rwlock;
	int	spec_rwunlock;
#ifdef	CELL_IRIX
	int	spec_obj_source_prepare;
	int	spec_obj_target_prepare;
	int	spec_obj_source_bag;
	int	spec_obj_target_unbag;
	int	spec_obj_source_end;
	int	spec_obj_source_abort;
	int	spec_obj_target_abort;
#endif	/* CELL_IRIX */
	int	spec_seek;
	int	spec_setattr;
	int	spec_setfl;
	int	spec_strategy;
	int	spec_strgetmsg;
	int	spec_strputmsg;
	int	spec_symlink;
	int	spec_teardown;
	int	spec_teardown_fsync;
	int	spec_unlock;
#ifdef	CELL_IRIX
	int	spec_vnimport;
#endif	/* CELL_IRIX */
	int	spec_vnode_change;
	int	spec_vp;
#ifdef	JTK_DEBUG
	int	spec_vp_is_common;
#endif	/* JTK_DEBUG */
	int	spec_write;
};

#define	SPEC_STATS(xx)						\
	{							\
		extern	struct spec_stat_info	spec_stats;	\
		spec_stats. ## xx += 1;				\
	}

#else	/* ! SPECFS_STATS_DEBUG */

#define	SPEC_STATS(xx)

#endif	/* ! SPECFS_STATS_DEBUG */

#endif	/* _FS_SPECFS_OPS_H */
