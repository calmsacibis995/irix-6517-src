/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994, Silicon Graphics, Inc.	          *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef _DMI_PRIVATE_H
#define _DMI_PRIVATE_H

#include <sys/types.h>
#include <ksys/behavior.h>
#include <sys/dmi_kern.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/fsid.h>
#include <sys/fstyp.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/vfs.h>			/* define fsid_t for handle_t */
#include <sys/vnode.h>
#include <sys/handle.h>			/* define handle_t */ 

#include <stddef.h>

#ifdef	DMAPI_ON_KUDZU

#define	DM_GET_CRED	(get_current_cred())
#define	DM_GET_ABI	(get_current_abi())

#else

#define	DM_GET_CRED	(curprocp->p_cred)
#define	DM_GET_ABI	(curprocp->p_abi)

#endif



typedef struct dm_tokdata {
	struct dm_tokdata *td_next;
	struct dm_tokevent *td_tevp;	/* pointer to owning tevp */
	int		td_app_ref;	/* # app threads currently active */
	dm_right_t	td_orig_right;	/* original right held when created */
	dm_right_t	td_right;	/* current right held for this handle */
	short		td_flags;
	short		td_type;	/* object type */
	bhv_desc_t	*td_bdp;	/* behavior descriptor */
	int		td_vcount;	/* # of current application VN_HOLDs */
	handle_t	td_handle;	/* handle for vp or vfsp */
} dm_tokdata_t;

/* values for td_type */

#define	DM_TDT_NONE	0x00		/* td_handle is empty */
#define	DM_TDT_VFS	0x01		/* td_handle points to a vfs */
#define	DM_TDT_REG	0x02		/* td_handle points to a file */
#define	DM_TDT_DIR	0x04		/* td_handle points to a directory */
#define	DM_TDT_LNK	0x08		/* td_handle points to a symlink */
#define	DM_TDT_OTH	0x10		/* some other object eg. pipe, socket */

#define	DM_TDT_VNO	(DM_TDT_REG|DM_TDT_DIR|DM_TDT_LNK|DM_TDT_OTH)
#define	DM_TDT_ANY	(DM_TDT_VFS|DM_TDT_REG|DM_TDT_DIR|DM_TDT_LNK|DM_TDT_OTH)

/* values for td_flags */

#define	DM_TDF_ORIG	0x0001		/* part of the original event */
#define	DM_TDF_EVTREF	0x0002		/* event thread holds vnode reference */
#define	DM_TDF_STHREAD	0x0004		/* only one app can use this handle */
#define	DM_TDF_RIGHT	0x0008		/* vcount bumped for dm_request_right */
#define	DM_TDF_HOLD	0x0010		/* vcount bumped for dm_obj_ref_hold */


/* Because some events contain uint64_t fields, we force te_msg and te_event
   to always be 8-byte aligned.  In order to send more than one message in
   a single dm_get_events() call, we also ensure that each message is an
   8-byte multiple.
*/

typedef struct dm_tokevent {
	struct dm_tokevent  *te_next;
	lock_t		te_lock;	/* lock for all fields but te_next */
	sv_t		te_evt_queue;	/* queue waiting for dm_respond_event */
	sv_t		te_app_queue;	/* queue waiting for handle access */
	int		te_evt_ref;	/* number of event procs using token */
	int		te_app_ref;	/* number of app procs using token */
	int		te_app_slp;	/* number of app procs sleeping */
	int		te_reply;	/* return errno for sync messages */
	short		te_flags;
	short		te_allocsize;	/* alloc'ed size of this structure */
	dm_tokdata_t	*te_tdp;	/* list of handle/right pairs */
	union {
		uint64_t	align;	/* force alignment of te_msg */
		dm_eventmsg_t	te_msg;		/* user visible part */
	} te_u;
	uint64_t	te_event;	/* start of dm_xxx_event_t message */
} dm_tokevent_t;

#define	te_msg	te_u.te_msg

/* values for te_flags */

#define	DM_TEF_LOCKED	0x0001		/* event "locked" by dm_get_events() */
#define	DM_TEF_INTERMED	0x0002		/* a dm_pending reply was received */
#define	DM_TEF_FINAL	0x0004		/* dm_respond_event has been received */


typedef	struct dm_eventq {
	dm_tokevent_t	*eq_head;
	dm_tokevent_t	*eq_tail;
	int		eq_count;	/* size of queue */
} dm_eventq_t;


typedef	struct dm_session {
	struct dm_session	*sn_next;	/* sessions linkage */
	dm_sessid_t	sn_sessid;	/* user-visible session number */
	u_int		sn_flags;
	lock_t		sn_qlock;	/* lock for newq/delq related fields */
	sv_t		sn_readerq;	/* waiting for message on sn_newq */
	u_int		sn_readercnt;	/* count of waiting readers */
	sv_t		sn_writerq;	/* waiting for room on sn_newq */
	u_int		sn_writercnt;	/* count of waiting readers */
	dm_eventq_t	sn_newq;	/* undelivered event queue */
	dm_eventq_t	sn_delq;	/* delivered event queue */
	char		sn_info[DM_SESSION_INFO_LEN];	/* user-supplied info */
} dm_session_t;

/* values for sn_flags */

#define	DM_SN_WANTMOUNT	0x0001		/* session wants to get mount events */


typedef	enum {
	DM_STATE_MOUNTING,
	DM_STATE_MOUNTED,
	DM_STATE_UNMOUNTING,
	DM_STATE_UNMOUNTED
} dm_fsstate_t;


typedef struct dm_fsreg {
	struct dm_fsreg	*fr_next;
	vfs_t		*fr_vfsp;	/* filesystem pointer */
	dm_tokevent_t	*fr_tevp;
	fsid_t		fr_fsid;	/* filesystem ID */
	void		*fr_msg;	/* dm_mount_event_t for filesystem */
	int		fr_msgsize;	/* size of dm_mount_event_t */
	dm_fsstate_t	fr_state;
	sv_t		fr_dispq;
	int		fr_dispcnt;
	sv_t		fr_queue;	/* queue for hdlcnt/vfscnt/unmount */
	lock_t		fr_lock;
	int		fr_hdlcnt;	/* threads blocked during unmount */
	int		fr_vfscnt;	/* threads in VFS_VGET or VFS_ROOT */
	int		fr_unmount;	/* if non-zero, umount is sleeping */
	dm_attrname_t	fr_rattr;	/* dm_set_return_on_destroy attribute */
	dm_session_t	*fr_sessp [DM_EVENT_MAX];
} dm_fsreg_t;




/* events valid in dm_set_disp() when called with a filesystem handle. */

#define	DM_VALID_DISP_EVENTS		( \
	(1 << DM_EVENT_PREUNMOUNT)	| \
	(1 << DM_EVENT_UNMOUNT)		| \
	(1 << DM_EVENT_NOSPACE)		| \
	(1 << DM_EVENT_DEBUT)		| \
	(1 << DM_EVENT_CREATE)		| \
	(1 << DM_EVENT_POSTCREATE)	| \
	(1 << DM_EVENT_REMOVE)		| \
	(1 << DM_EVENT_POSTREMOVE)	| \
	(1 << DM_EVENT_RENAME)		| \
	(1 << DM_EVENT_POSTRENAME)	| \
	(1 << DM_EVENT_LINK)		| \
	(1 << DM_EVENT_POSTLINK)	| \
	(1 << DM_EVENT_SYMLINK)		| \
	(1 << DM_EVENT_POSTSYMLINK)	| \
	(1 << DM_EVENT_READ)		| \
	(1 << DM_EVENT_WRITE)		| \
	(1 << DM_EVENT_TRUNCATE)	| \
	(1 << DM_EVENT_ATTRIBUTE)	| \
	(1 << DM_EVENT_DESTROY)		)


/*
 *  Global handle hack isolation.
 */

#define	DM_GLOBALHAN(hanp, hlen)	(((hanp) == DM_GLOBAL_HANP) && \
					 ((hlen) == DM_GLOBAL_HLEN))


#define	DM_MAX_MSG_DATA		3960



/* Supported filesystem function vector functions. */


typedef	struct {
	int				code_level;
	char				fsys_name[FSTYPSZ];
	dm_fsys_clear_inherit_t		clear_inherit;
	dm_fsys_create_by_handle_t	create_by_handle;
	dm_fsys_downgrade_right_t	downgrade_right;
	dm_fsys_get_allocinfo_rvp_t	get_allocinfo_rvp;
	dm_fsys_get_bulkall_rvp_t	get_bulkall_rvp;
	dm_fsys_get_bulkattr_rvp_t	get_bulkattr_rvp;
	dm_fsys_get_config_t		get_config;
	dm_fsys_get_config_events_t	get_config_events;
	dm_fsys_get_destroy_dmattr_t	get_destroy_dmattr;
	dm_fsys_get_dioinfo_t		get_dioinfo;
	dm_fsys_get_dirattrs_rvp_t	get_dirattrs_rvp;
	dm_fsys_get_dmattr_t		get_dmattr;
	dm_fsys_get_eventlist_t		get_eventlist;
	dm_fsys_get_fileattr_t		get_fileattr;
	dm_fsys_get_region_t		get_region;
	dm_fsys_getall_dmattr_t		getall_dmattr;
	dm_fsys_getall_inherit_t	getall_inherit;
	dm_fsys_init_attrloc_t		init_attrloc;
	dm_fsys_mkdir_by_handle_t	mkdir_by_handle;
	dm_fsys_probe_hole_t		probe_hole;
	dm_fsys_punch_hole_t		punch_hole;
	dm_fsys_read_invis_rvp_t	read_invis_rvp;
	dm_fsys_release_right_t		release_right;
	dm_fsys_remove_dmattr_t		remove_dmattr;
	dm_fsys_request_right_t		request_right;
	dm_fsys_set_dmattr_t		set_dmattr;
	dm_fsys_set_eventlist_t		set_eventlist;
	dm_fsys_set_fileattr_t		set_fileattr;
	dm_fsys_set_inherit_t		set_inherit;
	dm_fsys_set_region_t		set_region;
	dm_fsys_symlink_by_handle_t	symlink_by_handle;
	dm_fsys_sync_by_handle_t	sync_by_handle;
	dm_fsys_upgrade_right_t		upgrade_right;
	dm_fsys_write_invis_rvp_t	write_invis_rvp;
} dm_fsys_vector_t;



extern 	dm_session_t	*dm_sessions;		/* head of session list */
extern	dm_fsreg_t	*dm_registers;
extern	lock_t		dm_reg_lock;		/* lock for registration list */


/*
 *  Kernel only prototypes.
 */

int		dm_find_session_and_lock(
			dm_sessid_t	sid,
			dm_session_t	**sessionpp,
			int		*lcp);

int		dm_find_msg_and_lock(
			dm_sessid_t	sid,
			dm_token_t	token,
			dm_tokevent_t	**tevpp,
			int		*lcp);

dm_tokevent_t *	dm_evt_create_tevp(
			dm_eventtype_t	event,
			int		variable_size,
			void		**msgpp);

int		dm_app_get_tdp(
			dm_sessid_t	sid,
			void		*hanp,
			size_t		hlen,
			dm_token_t	token,
			short		types,
			dm_right_t	right,
			dm_tokdata_t	**tdpp);

int		dm_get_config_tdp(
			void		*hanp,
			size_t		hlen,
			dm_tokdata_t	**tdpp);

void		dm_app_put_tdp(
			dm_tokdata_t	*tdp);

void		dm_put_tevp(
			dm_tokevent_t	*tevp,
			dm_tokdata_t	*tdp);

void		dm_evt_rele_tevp(
			dm_tokevent_t	*tevp,
			int		droprights);

int		dm_enqueue_normal_event(
			vfs_t		*vfsp,
			dm_tokevent_t	*tevp,
			int		flags);

int		dm_enqueue_mount_event(
			vfs_t		*vfsp,
			dm_tokevent_t	*tevp);

int		dm_enqueue_sendmsg_event(
			dm_sessid_t	targetsid,
			dm_tokevent_t	*tevp,
			int		synch);

int		dm_enqueue_user_event(
			dm_sessid_t	sid,
			dm_tokevent_t	*tevp,
			dm_token_t	*tokenp);


int		dm_obj_ref_query_rvp(
			dm_sessid_t	sid,
			dm_token_t	token,
			void		*hanp,
			size_t		hlen,
			union rval	*rvp);

int		dm_read_invis_rvp(
			dm_sessid_t	sid,
			void		*hanp,
			size_t		hlen,
			dm_token_t	token,
			dm_off_t	off,
			dm_size_t	len,
			void		*bufp,
			union rval	*rvp);

int		dm_write_invis_rvp(
			dm_sessid_t	sid,
			void		*hanp,
			size_t		hlen,
			dm_token_t	token,
			int		flags,
			dm_off_t	off,
			dm_size_t	len,
			void		*bufp,
			union rval	*rvp);

int		dm_get_bulkattr_rvp(
			dm_sessid_t	sid,
			void		*hanp,
			size_t		hlen,
			dm_token_t	token,
			u_int		mask,
			dm_attrloc_t	*locp,
			size_t		buflen,
			void		*bufp,
			size_t		*rlenp,
			union rval	*rvp);

int		dm_get_bulkall_rvp(
			dm_sessid_t	sid,
			void		*hanp,
			size_t		hlen,
			dm_token_t	token,
			u_int		mask,
			dm_attrname_t	*attrnamep,
			dm_attrloc_t	*locp,
			size_t		buflen,
			void		*bufp,
			size_t		*rlenp,
			union rval	*rvp);

int		dm_get_dirattrs_rvp(
			dm_sessid_t	sid,
			void		*hanp,
			size_t		hlen,
			dm_token_t	token,
			u_int		mask,
			dm_attrloc_t	*locp,
			size_t		buflen,
			void		*bufp,
			size_t		*rlenp,
			union rval	*rvp);

int		dm_get_allocinfo_rvp(
			dm_sessid_t	sid,
			void		*hanp,
			size_t		hlen,
			dm_token_t	token,
			dm_off_t	*offp,
			u_int		nelem,
			dm_extent_t	*extentp,
			u_int		*nelemp,
			union rval	*rvp);

dm_session_t *	dm_get_session(
			vfs_t		*vfsp,
			dm_eventtype_t	event);

int		dm_waitfor_destroy_attrname(
			vfs_t		*vfsp,
			dm_attrname_t	*attrnamep);

void		dm_clear_fsreg(
			dm_session_t	*s);

int		dm_add_fsys_entry(
			vfs_t		*vfsp,
			dm_tokevent_t	*tevp);

void		dm_change_fsys_entry(
			vfs_t		*vfsp,
			dm_fsstate_t	newstate);

void		dm_remove_fsys_entry(
			vfs_t		*vfsp);

dm_fsys_vector_t *dm_fsys_vector(
			bhv_desc_t	*bdp);

int		dm_cpoutsizet(
			void		*userptr,
			size_t          value);

int		dm_waitfor_disp_session(
			vfs_t		*vfsp,
			dm_tokevent_t	*tevp,
			dm_session_t	**sessionpp,
			int		*lcp);

vnode_t *	dm_handle_to_vp (
			handle_t	*handlep,
			short		*typep);

int		dm_check_dmapi_vp(
			vnode_t		*vp);

dm_tokevent_t *	dm_find_mount_tevp_and_lock(
			fsid_t		*fsidp,
			int		*lcp);

int		dm_path_to_hdl(
			char		*path,
			void		*hanp,
			size_t		*hlenp);

int		dm_path_to_fshdl(
			char		*path,
			void		*hanp,
			size_t		*hlenp);

int		dm_fd_to_hdl(
			int		fd,
			void		*hanp,
			size_t		*hlenp);

#endif	/* _DMI_PRIVATE_H */
