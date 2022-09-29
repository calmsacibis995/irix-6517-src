/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.25 $"

#ifndef _SYS_DMAPI_H
#define _SYS_DMAPI_H

#ifdef	__cplusplus
extern	"C" {
#endif

#include <sys/types.h>


/**************************************************************************
 *									  *
 * The SGI implementation of DMAPI is based upon the X/Open document	  *
 * 	Systems Management: Data Storage Managment (XDSM) API		  *
 * dated February 1997.  Not all DMAPI functions and structure fields	  *
 * have been implemented.  Most importantly, the DMAPI functions	  *
 * dm_request_right, dm_release_right, dm_query_right, dm_upgrade_right	  *
 * and dm_downgrade_right do not work as described in the specification.  *
 * 									  *
 * The XFS filesystem currently does not allow its locking mechanisms to  *
 * be externally accessed from user space.  While the above-mentioned	  *
 * dm_xxx_right functions exist and can be called by applications, they	  *
 * always return successfully without actually obtaining any locks	  *
 * within the filesystem.						  *
 * 									  *
 * Applications which do not need full rights support and which only	  *
 * make dm_xxx_right calls in order to satisfy the input requirements of  *
 * other DMAPI calls should be able to use these routines to avoid	  *
 * having to implement special-case code for SGI platforms.  Applications *
 * which truely need the capabilities of a full implementation of rights  *
 * will unfortunately have to come up with alternate software solutions	  *
 * until such time as rights can be completely implemented.		  *
 * 									  *
 * Functions and structure fields defined within this file which are not  *
 * supported in the SGI implementation of DMAPI are indicated by comments *
 * following their definitions such as "not supported", or "not		  *
 * completely supported".  Any function or field not so marked may be	  *
 * assumed to work exactly according to the spec.			  *
 *									  *
 **************************************************************************/



/* The first portion of this file contains defines and typedefs that are
   DMAPI implementation-dependent, and could be different on other platforms.
*/

typedef int64_t		dm_attrloc_t;
typedef	u_int		dm_boolean_t;
typedef	uint64_t	dm_eventset_t;
typedef	uint64_t	dm_fsid_t;
typedef	uint64_t	dm_ino_t;
typedef uint32_t	dm_igen_t;
typedef int64_t		dm_off_t;
typedef	u_int		dm_sequence_t;
typedef int		dm_sessid_t;
typedef uint64_t	dm_size_t;
typedef int64_t		dm_ssize_t;
typedef int		dm_token_t;

#define	DM_REGION_NOEVENT	0x0
#define	DM_REGION_READ		0x1
#define DM_REGION_WRITE		0x2
#define	DM_REGION_TRUNCATE	0x4

/* Values for the mask argument used with dm_get_fileattr, dm_get_bulkattr,
   dm_get_dirattrs, and dm_set_fileattr.
*/

#define	DM_AT_MODE	0x0001
#define	DM_AT_UID	0x0002
#define	DM_AT_GID	0x0004
#define	DM_AT_ATIME	0x0008
#define	DM_AT_MTIME	0x0010
#define	DM_AT_CTIME	0x0020
#define	DM_AT_SIZE	0x0040
#define	DM_AT_DTIME	0x0080
#define	DM_AT_HANDLE	0x0100
#define	DM_AT_EMASK	0x0200
#define	DM_AT_PMANR	0x0400
#define	DM_AT_PATTR	0x0800
#define	DM_AT_STAT	0x1000
#define	DM_AT_CFLAG	0x2000

#define	DM_EV_WAIT	0x1		/* used in dm_get_events() */

#define	DM_MOUNT_RDONLY	0x1		/* me_mode field in dm_mount_event_t */

#define	DM_RR_WAIT	0x1

#define	DM_UNMOUNT_FORCE 0x1		/* ne_mode field in dm_namesp_event_t */

#define	DM_WRITE_SYNC	0x1		/* used in dm_write_invis() */

#define DM_SESSION_INFO_LEN	256
#define	DM_NO_SESSION		0
#define DM_TRUE			1
#define DM_FALSE		0
#define	DM_INVALID_TOKEN	0
#define	DM_NO_TOKEN		(-1)
#define	DM_INVALID_HANP		NULL
#define	DM_INVALID_HLEN		0
#define	DM_GLOBAL_HANP		((void *)(1LL))
#define	DM_GLOBAL_HLEN		((size_t)(1))
#define	DM_VER_STR_CONTENTS	"SGI DMAPI (XDSM) API, Release 1.0."


#define	DMEV_SET(event_type, event_list) \
	((event_list) |= (1 << (event_type)))
#define	DMEV_CLR(event_type, event_list) \
	((event_list) &= ~(1 << (event_type)))
#define	DMEV_ISSET(event_type, event_list) \
	(int)(((event_list) & (1 << (event_type))) != 0)
#define	DMEV_ZERO(event_list) \
	(event_list) = 0


typedef	struct {
	int	vd_offset;	/* offset from start of containing struct */
	u_int	vd_length;	/* length of data starting at vd_offset */
} dm_vardata_t;

#define DM_GET_VALUE(p, field, type) \
	((type) ((char *)(p) + (p)->field.vd_offset))

#define DM_GET_LEN(p, field) \
	((p)->field.vd_length)

#define DM_STEP_TO_NEXT(p, type) \
	((type) ((p)->_link ? (char *)(p) + (p)->_link : NULL))




/* The remainder of this include file contains defines, typedefs, and
   structures which are strictly defined by the DMAPI 2.3 specification.

   (The _link field which appears in several structures is an
   implementation-specific way to implement DM_STEP_TO_NEXT, and
   should not be referenced directly by application code.)
*/


#define	DM_ATTR_NAME_SIZE	8


struct dm_attrname {
	u_char	an_chars[DM_ATTR_NAME_SIZE];
};
typedef	struct dm_attrname	dm_attrname_t;


struct dm_attrlist {
	int		_link;
	dm_attrname_t	al_name;
	dm_vardata_t	al_data;
};
typedef struct dm_attrlist	dm_attrlist_t;


typedef enum {
	DM_CONFIG_INVALID,
	DM_CONFIG_BULKALL,
	DM_CONFIG_CREATE_BY_HANDLE,
	DM_CONFIG_DTIME_OVERLOAD,
	DM_CONFIG_LEGACY,
	DM_CONFIG_LOCK_UPGRADE,
	DM_CONFIG_MAX_ATTR_ON_DESTROY,
	DM_CONFIG_MAX_ATTRIBUTE_SIZE,
	DM_CONFIG_MAX_HANDLE_SIZE,
	DM_CONFIG_MAX_MANAGED_REGIONS,
	DM_CONFIG_MAX_MESSAGE_DATA,
	DM_CONFIG_OBJ_REF,
	DM_CONFIG_PENDING,
	DM_CONFIG_PERS_ATTRIBUTES,
	DM_CONFIG_PERS_EVENTS,
	DM_CONFIG_PERS_INHERIT_ATTRIBS,
	DM_CONFIG_PERS_MANAGED_REGIONS,
	DM_CONFIG_PUNCH_HOLE,
	DM_CONFIG_TOTAL_ATTRIBUTE_SPACE,
	DM_CONFIG_WILL_RETRY
} dm_config_t;


struct	dm_dioinfo {			/* non-standard SGI addition */
	u_int		d_mem;
	u_int		d_miniosz;
	u_int		d_maxiosz;
	dm_boolean_t	d_dio_only;
};
typedef	struct dm_dioinfo	dm_dioinfo_t;


struct dm_dispinfo {
	int		_link;
	u_int		di_pad1;		/* reserved; do not reference */
	dm_vardata_t	di_fshandle;
	dm_eventset_t	di_eventset;
};
typedef	struct dm_dispinfo	dm_dispinfo_t;


typedef enum {
	DM_EVENT_INVALID	= -1,
	DM_EVENT_CANCEL		= 0,		/* not supported */
	DM_EVENT_MOUNT		= 1,
	DM_EVENT_PREUNMOUNT	= 2,
	DM_EVENT_UNMOUNT	= 3,
	DM_EVENT_DEBUT		= 4,		/* not supported */
	DM_EVENT_CREATE		= 5,
	DM_EVENT_CLOSE		= 6,		/* not supported */
	DM_EVENT_POSTCREATE	= 7,
	DM_EVENT_REMOVE		= 8,
	DM_EVENT_POSTREMOVE	= 9,
	DM_EVENT_RENAME		= 10,
	DM_EVENT_POSTRENAME	= 11,
	DM_EVENT_LINK		= 12,
	DM_EVENT_POSTLINK	= 13,
	DM_EVENT_SYMLINK	= 14,
	DM_EVENT_POSTSYMLINK	= 15,
	DM_EVENT_READ		= 16,
	DM_EVENT_WRITE		= 17,
	DM_EVENT_TRUNCATE	= 18,
	DM_EVENT_ATTRIBUTE	= 19,
	DM_EVENT_DESTROY	= 20,
	DM_EVENT_NOSPACE	= 21,
	DM_EVENT_USER		= 22,
	DM_EVENT_MAX		= 23
} dm_eventtype_t;


struct dm_eventmsg {
	int		_link;
	dm_eventtype_t	ev_type;
	dm_token_t	ev_token;
	dm_sequence_t	ev_sequence;
	dm_vardata_t	ev_data;
};
typedef	struct dm_eventmsg	dm_eventmsg_t;


struct dm_cancel_event {			/* not supported */
	dm_sequence_t	ce_sequence;
	dm_token_t	ce_token;
};
typedef	struct dm_cancel_event	dm_cancel_event_t;


struct dm_data_event {
	dm_vardata_t	de_handle;
	dm_off_t	de_offset;
	dm_size_t	de_length;
};
typedef struct dm_data_event dm_data_event_t;


struct dm_destroy_event {
	dm_vardata_t		ds_handle;
	dm_attrname_t		ds_attrname;
	dm_vardata_t		ds_attrcopy;
};
typedef	struct dm_destroy_event dm_destroy_event_t;


struct dm_mount_event {
	mode_t		me_mode;
	dm_vardata_t	me_handle1;
	dm_vardata_t	me_handle2;
	dm_vardata_t	me_name1;
	dm_vardata_t	me_name2;
	dm_vardata_t	me_roothandle;
};
typedef	struct dm_mount_event dm_mount_event_t;


struct dm_namesp_event {
	mode_t		ne_mode;
	dm_vardata_t	ne_handle1;
	dm_vardata_t	ne_handle2;
	dm_vardata_t	ne_name1;
	dm_vardata_t	ne_name2;
	int		ne_retcode;
};
typedef	struct dm_namesp_event dm_namesp_event_t;


typedef enum {
	DM_EXTENT_INVALID,
	DM_EXTENT_RES,
	DM_EXTENT_HOLE
} dm_extenttype_t;


struct dm_extent {
	dm_extenttype_t	ex_type;
	u_int		ex_pad1;		/* reserved; do not reference */
	dm_off_t	ex_offset;
	dm_size_t	ex_length;
};
typedef struct dm_extent dm_extent_t;


struct dm_fileattr {
	mode_t		fa_mode;
	uid_t		fa_uid;
	gid_t		fa_gid;
	time_t		fa_atime;
	time_t		fa_mtime;
	time_t		fa_ctime;
	time_t		fa_dtime;
	u_int		fa_pad1;		/* reserved; do not reference */
	dm_off_t	fa_size;
};
typedef struct dm_fileattr dm_fileattr_t;


struct dm_inherit {				/* not supported */
	dm_attrname_t	ih_name;
	mode_t		ih_filetype;
};
typedef struct dm_inherit dm_inherit_t;


typedef enum {
	DM_MSGTYPE_INVALID,
	DM_MSGTYPE_SYNC,
	DM_MSGTYPE_ASYNC
} dm_msgtype_t;


struct dm_region {
	dm_off_t	rg_offset;
	dm_size_t	rg_size;
	u_int		rg_flags;
	u_int		rg_pad1;		/* reserved; do not reference */
};
typedef struct dm_region dm_region_t;


typedef enum {
	DM_RESP_INVALID,
	DM_RESP_CONTINUE,
	DM_RESP_ABORT,
	DM_RESP_DONTCARE
} dm_response_t;


typedef enum {
	DM_RIGHT_NULL,
	DM_RIGHT_SHARED,
	DM_RIGHT_EXCL
} dm_right_t;


struct dm_stat {
	int		_link;
	dm_vardata_t	dt_handle;
	dm_vardata_t	dt_compname;
	int		dt_nevents;
	dm_eventset_t	dt_emask;
	int		dt_pers;		/* field not supported */
	int		dt_pmanreg;
	time_t		dt_dtime;
	u_int		dt_change;		/* field not supported */
	u_int		dt_pad1;		/* reserved; do not reference */

	dev_t		dt_dev;
	dm_ino_t	dt_ino;
	mode_t		dt_mode;
	nlink_t		dt_nlink;
	uid_t		dt_uid;
	gid_t		dt_gid;
	dev_t		dt_rdev;
	u_int		dt_pad2;		/* reserved; do not reference */
	dm_off_t	dt_size;
	time_t		dt_atime;
	time_t		dt_mtime;
	time_t		dt_ctime;
	u_int		dt_blksize;
	dm_size_t	dt_blocks;

	/* Non-standard filesystem-specific fields.  Currently XFS is the only
	   supported filesystem type.
	*/

	uint64_t	dt_pad3;	/* reserved; do not reference */
	int		dt_fstype;	/* filesystem index; see sysfs(2) */
	union	{
		struct	{
			dm_igen_t	igen;
			u_int		xflags;
			u_int		extsize;
			u_int		extents;
			u_short		aextents;
			u_short		dmstate;
		} sgi_xfs;
	} fsys_dep;
};
typedef	struct dm_stat	dm_stat_t;

#define	dt_xfs_igen	fsys_dep.sgi_xfs.igen
#define	dt_xfs_xflags	fsys_dep.sgi_xfs.xflags
#define	dt_xfs_extsize	fsys_dep.sgi_xfs.extsize
#define	dt_xfs_extents	fsys_dep.sgi_xfs.extents
#define	dt_xfs_aextents	fsys_dep.sgi_xfs.aextents
#define	dt_xfs_dmstate	fsys_dep.sgi_xfs.dmstate

/* Flags for the non-standard dt_xfs_xflags field. */

#define	DM_XFLAG_REALTIME	0x1
#define	DM_XFLAG_PREALLOC	0x2
#define	DM_XFLAG_HASATTR	0x80000000


struct	dm_timestruct {
	time_t		dm_tv_sec;
	int		dm_tv_nsec;
};
typedef	struct dm_timestruct dm_timestruct_t;


struct	dm_xstat {				/* not supported */
	dm_stat_t	dx_statinfo;
	dm_vardata_t	dx_attrdata;
};
typedef	struct dm_xstat	dm_xstat_t;




/* The following list provides the prototypes for all functions defined in
   the DMAPI interface.
*/

extern int
dm_clear_inherit(				/* not supported */
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep);

extern int
dm_create_by_handle(				/* not supported */
	dm_sessid_t	sid,
	void		*dirhanp,
	size_t		dirhlen,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen,
	char		*cname);

extern int
dm_create_session(
	dm_sessid_t	oldsid,
	char		*sessinfop,
	dm_sessid_t	*newsidp);

extern int
dm_create_userevent(
	dm_sessid_t	sid,
	size_t		msglen,
	void		*msgdatap,
	dm_token_t	*tokenp);

extern int
dm_destroy_session(
	dm_sessid_t	sid);

extern int
dm_downgrade_right(		/* not completely supported; see caveat above */
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token);

extern int
dm_fd_to_handle(
	int		fd,
	void		**hanpp,
	size_t		*hlenp);

extern int
dm_find_eventmsg(
	dm_sessid_t	sid,
	dm_token_t	token,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp);

extern int
dm_get_allocinfo(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_off_t	*offp,
	u_int		nelem,
	dm_extent_t	*extentp,
	u_int		*nelemp);

extern int
dm_get_bulkall(					/* not supported */
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		mask,
	dm_attrname_t	*attrnamep,
	dm_attrloc_t	*locp,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp);

extern int
dm_get_bulkattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		mask,
	dm_attrloc_t	*locp,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp);

extern int
dm_get_config(
	void		*hanp,
	size_t		hlen,
	dm_config_t	flagname,
	dm_size_t	*retvalp);

extern int
dm_get_config_events(
	void		*hanp,
	size_t		hlen,
	u_int		nelem,
	dm_eventset_t	*eventsetp,
	u_int		*nelemp);

extern int
dm_get_dirattrs(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		mask,
	dm_attrloc_t	*locp,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp);

extern int
dm_get_dmattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp);

extern int
dm_get_eventlist(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		nelem,
	dm_eventset_t	*eventsetp,
	u_int		*nelemp);

extern int
dm_get_events(
	dm_sessid_t	sid,
	u_int		maxmsgs,
	u_int		flags,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp);

extern int
dm_get_fileattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		mask,
	dm_stat_t	*statp);

extern int
dm_get_mountinfo(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp);

extern int
dm_get_region(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		nelem,
	dm_region_t	*regbufp,
	u_int		*nelemp);

extern int
dm_getall_disp(
	dm_sessid_t	sid,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp);

extern int
dm_getall_dmattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp);

extern int
dm_getall_inherit(				/* not supported */
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		nelem,
	dm_inherit_t	*inheritbufp,
	u_int		*nelemp);

extern int
dm_getall_sessions(
	u_int		nelem,
	dm_sessid_t	*sidbufp,
	u_int		*nelemp);

extern int
dm_getall_tokens(
	dm_sessid_t	sid,
	u_int		nelem,
	dm_token_t	*tokenbufp,
	u_int		*nelemp);

extern int
dm_handle_cmp(
	void		*hanp1,
	size_t		hlen1,
	void		*hanp2,
	size_t		hlen2);

extern void
dm_handle_free(
	void		*hanp,
	size_t		hlen);

extern u_int
dm_handle_hash(
	void		*hanp,
	size_t		hlen);

extern dm_boolean_t
dm_handle_is_valid(
	void		*hanp,
	size_t		hlen);

extern int
dm_handle_to_fshandle(
	void		*hanp,
	size_t		hlen,
	void		**fshanpp,
	size_t		*fshlenp);

extern int
dm_handle_to_fsid(
	void		*hanp,
	size_t		hlen,
	dm_fsid_t	*fsidp);

extern int
dm_handle_to_igen(
	void		*hanp,
	size_t		hlen,
	dm_igen_t	*igenp);

extern int
dm_handle_to_ino(
	void		*hanp,
	size_t		hlen,
	dm_ino_t	*inop);

extern int
dm_handle_to_path(
	void		*dirhanp,
	size_t		dirhlen,
	void		*targhanp,
	size_t		targhlen,
	size_t		buflen,
	char		*pathbufp,
	size_t		*rlenp);

extern int
dm_init_attrloc(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrloc_t	*locp);

extern int
dm_init_service(
	char		**versionstrpp);

extern int
dm_make_handle(
	dm_fsid_t	*fsidp,
	dm_ino_t	*inop,
	dm_igen_t	*igenp,
	void		**hanpp,
	size_t		*hlenp);

extern int
dm_make_fshandle(
	dm_fsid_t	*fsidp,
	void		**hanpp,
	size_t		*hlenp);

extern int
dm_mkdir_by_handle(				/* not supported */
	dm_sessid_t	sid,
	void		*dirhanp,
	size_t		dirhlen,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen,
	char		*cname);

extern int
dm_move_event(
	dm_sessid_t	srcsid,
	dm_token_t	token,
	dm_sessid_t	targetsid,
	dm_token_t	*rtokenp);

extern int
dm_obj_ref_hold(
	dm_sessid_t	sid,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen);

extern int
dm_obj_ref_query(
	dm_sessid_t	sid,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen);

extern int
dm_obj_ref_rele(
	dm_sessid_t	sid,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen);

extern int
dm_path_to_fshandle(
	char		*path,
	void		**hanpp,
	size_t		*hlenp);

extern int
dm_path_to_handle(
	char		*path,
	void		**hanpp,
	size_t		*hlenp);

extern int
dm_pending(
	dm_sessid_t	sid,
	dm_token_t	token,
	dm_timestruct_t	*delay);

extern int
dm_probe_hole(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_off_t	off,
	dm_size_t	len,
	dm_off_t	*roffp,
	dm_size_t	*rlenp);

extern int
dm_punch_hole(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_off_t	off,
	dm_size_t	len);

extern int
dm_query_right(			/* not completely supported; see caveat above */
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_right_t	*rightp);

extern int
dm_query_session(
	dm_sessid_t	sid,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp);

extern dm_ssize_t
dm_read_invis(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_off_t	off,
	dm_size_t	len,
	void		*bufp);

extern int
dm_release_right(		/* not completely supported; see caveat above */
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token);

extern int
dm_remove_dmattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	int		setdtime,
	dm_attrname_t	*attrnamep);

extern int
dm_request_right(		/* not completely supported; see caveat above */
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		flags,
	dm_right_t	right);

extern int
dm_respond_event(
	dm_sessid_t	sid,
	dm_token_t	token,
	dm_response_t	response,
	int		reterror,
	size_t		buflen,
	void		*respbufp);

extern int
dm_send_msg(
	dm_sessid_t	targetsid,
	dm_msgtype_t	msgtype,
	size_t		buflen,
	void		*bufp);

extern int
dm_set_disp(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_eventset_t	*eventsetp,
	u_int		maxevent);

extern int
dm_set_dmattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep,
	int		setdtime,
	size_t		buflen,
	void		*bufp);

extern int
dm_set_eventlist(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_eventset_t	*eventsetp,
	u_int		maxevent);

extern int
dm_set_fileattr(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		mask,
	dm_fileattr_t	*attrp);

extern int
dm_set_inherit(					/* not supported */
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep,
	mode_t		mode);

extern int
dm_set_region(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		nelem,
	dm_region_t	*regbufp,
	dm_boolean_t	*exactflagp);

extern int
dm_set_return_on_destroy(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep,
	dm_boolean_t	enable);

extern int
dm_symlink_by_handle(				/* not supported */
	dm_sessid_t	sid,
	void		*dirhanp,
	size_t		dirhlen,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen,
	char		*cname,
	char		*path);

extern int
dm_sync_by_handle(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token);

extern int
dm_upgrade_right(		/* not completely supported; see caveat above */
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token);

extern dm_ssize_t
dm_write_invis(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	int		flags,
	dm_off_t	off,
	dm_size_t	len,
	void		*bufp);


/* Non-standard SGI additions to the DMAPI interface. */

extern int
dm_get_dioinfo(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_dioinfo_t	*diop);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DMAPI_H */
