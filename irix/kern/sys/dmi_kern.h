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

#ifndef _SYS_DMI_KERN_H
#define _SYS_DMI_KERN_H

#include <sys/dmi.h>

#ifdef	_KERNEL

union rval;
struct vnode;
struct bhv_desc;
struct vfs;
struct handle;

/* The first group of definitions and prototypes define the filesystem's
   interface into the DMAPI code.
*/


/* Definitions used for the flags field on dm_send_data_event(),
   dm_send_unmount_event(), and dm_send_namesp_event() calls.
*/

#define	DM_FLAGS_NDELAY		0x001	/* return EAGAIN after dm_pending() */
#define	DM_FLAGS_UNWANTED	0x002	/* event not in fsys dm_eventset_t */

/* Possible code levels reported by dm_code_level(). */

#define	DM_CLVL_INIT	0	/* DMAPI prior to X/Open compliance */
#define	DM_CLVL_XOPEN	1	/* X/Open compliant DMAPI */


/* Prototypes used outside of the DMI module/directory. */

int		dm_send_data_event(
			dm_eventtype_t	event,
			struct bhv_desc	*bdp,
			dm_right_t	vp_right,
			off_t		off,
			size_t		len,
			int		flags);

int		dm_send_destroy_event(
			struct bhv_desc	*bdp,
			dm_right_t	vp_right);

int		dm_send_mount_event(
			struct vfs	*vfsp,
			dm_right_t	vfsp_right,
			struct bhv_desc *bdp,
			dm_right_t	vp_right,
			struct bhv_desc *rootbdp,
			dm_right_t	rootvp_right,
			char		*name1,
			char		*name2);

int		dm_send_namesp_event(
			dm_eventtype_t	event,
			struct bhv_desc	*bdp1,
			dm_right_t	vp1_right,
			struct bhv_desc	*bdp2,
			dm_right_t	vp2_right,
			char		*name1,
			char		*name2,
			mode_t		mode,
			int		retcode,
			int		flags);

void		dm_send_unmount_event(
			struct vfs	*vfsp,
			struct vnode	*vp,
			dm_right_t	vfsp_right,
			mode_t		mode,
			int		retcode,
			int		flags);

int		dm_code_level(void);

int		dm_vp_to_handle (
			struct vnode	*vp,
			struct handle	*handlep);

/* The following prototypes and definitions are used by DMAPI as its
   interface into the filesystem code.  Communication between DMAPI and the
   filesystem are established as follows:
   1. DMAPI uses the F_DMAPI command in VOP_FCNTL() to ask for the addresses
      of all the functions within the filesystem that it may need to call.
   2. The filesystem returns an array of function name/address pairs which
      DMAPI builds into a function vector.
   The VOP_FCNTL() call is only made one time for a particular filesystem
   type.  From then on, DMAPI uses its function vector to call the filesystem
   functions directly.  Functions in the array which DMAPI doesn't recognize
   are ignored.  A dummy function which returns ENOSYS is used for any function
   that DMAPI needs but which was not provided by the filesystem.  If XFS
   doesn't recognize the F_DMAPI fcntl, DMAPI assumes that it doesn't have the
   X/Open support code; in this case DMAPI uses the XFS-code originally bundled
   within DMAPI.

   The goal of this interface is allow incremental changes to be made to
   both the filesystem and to DMAPI while minimizing inter-patch dependencies,
   and to eventually allow DMAPI to support multiple filesystem types at the
   same time should that become necessary.
*/

typedef enum {
	DM_FSYS_CLEAR_INHERIT		=  0,
	DM_FSYS_CREATE_BY_HANDLE	=  1,
	DM_FSYS_DOWNGRADE_RIGHT		=  2,
	DM_FSYS_GET_ALLOCINFO_RVP	=  3,
	DM_FSYS_GET_BULKALL_RVP		=  4,
	DM_FSYS_GET_BULKATTR_RVP	=  5,
	DM_FSYS_GET_CONFIG		=  6,
	DM_FSYS_GET_CONFIG_EVENTS	=  7,
	DM_FSYS_GET_DESTROY_DMATTR	=  8,
	DM_FSYS_GET_DIOINFO		=  9,
	DM_FSYS_GET_DIRATTRS_RVP	= 10,
	DM_FSYS_GET_DMATTR		= 11,
	DM_FSYS_GET_EVENTLIST		= 12,
	DM_FSYS_GET_FILEATTR		= 13,
	DM_FSYS_GET_REGION		= 14,
	DM_FSYS_GETALL_DMATTR		= 15,
	DM_FSYS_GETALL_INHERIT		= 16,
	DM_FSYS_INIT_ATTRLOC		= 17,
	DM_FSYS_MKDIR_BY_HANDLE		= 18,
	DM_FSYS_PROBE_HOLE		= 19,
	DM_FSYS_PUNCH_HOLE		= 20,
	DM_FSYS_READ_INVIS_RVP		= 21,
	DM_FSYS_RELEASE_RIGHT		= 22,
	DM_FSYS_REMOVE_DMATTR		= 23,
	DM_FSYS_REQUEST_RIGHT		= 24,
	DM_FSYS_SET_DMATTR		= 25,
	DM_FSYS_SET_EVENTLIST		= 26,
	DM_FSYS_SET_FILEATTR		= 27,
	DM_FSYS_SET_INHERIT		= 28,
	DM_FSYS_SET_REGION		= 29,
	DM_FSYS_SYMLINK_BY_HANDLE	= 30,
	DM_FSYS_SYNC_BY_HANDLE		= 31,
	DM_FSYS_UPGRADE_RIGHT		= 32,
	DM_FSYS_WRITE_INVIS_RVP		= 33,
	DM_FSYS_MAX			= 34
} dm_fsys_switch_t;


#define	DM_FSYS_OBJ	0x1		/* object refers to a fsys handle */


/*
 *  Prototypes for filesystem-specific functions.
 */

typedef	int	(*dm_fsys_clear_inherit_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			dm_attrname_t	*attrnamep);

typedef	int	(*dm_fsys_create_by_handle_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			void		*hanp,
			size_t		hlen,
			char		*cname);

typedef	int	(*dm_fsys_downgrade_right_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			u_int		type);	/* DM_FSYS_OBJ or zero */

typedef	int	(*dm_fsys_get_allocinfo_rvp_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			dm_off_t	*offp,
			u_int		nelem,
			dm_extent_t	*extentp,
			u_int		*nelemp,
			union rval	*rvalp);

typedef	int 	(*dm_fsys_get_bulkall_rvp_t)(
			struct bhv_desc	*bdp,		/* root vnode */
			dm_right_t	right,
			u_int		mask,
			dm_attrname_t	*attrnamep,
			dm_attrloc_t	*locp,
			size_t		buflen,
			void		*bufp,
			size_t		*rlenp,
			union rval	*rvalp);

typedef	int 	(*dm_fsys_get_bulkattr_rvp_t)(
			struct bhv_desc	*bdp,		/* root vnode */
			dm_right_t	right,
			u_int		mask,
			dm_attrloc_t	*locp,
			size_t		buflen,
			void		*bufp,
			size_t		*rlenp,
			union rval	*rvalp);

typedef	int	(*dm_fsys_get_config_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			dm_config_t	flagname,
			dm_size_t	*retvalp);

typedef	int	(*dm_fsys_get_config_events_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			u_int		nelem,
			dm_eventset_t	*eventsetp,
			u_int		*nelemp);

typedef	int	(*dm_fsys_get_destroy_dmattr_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			dm_attrname_t	*attrnamep,
			char		**valuepp,
			int		*vlenp);

typedef	int	(*dm_fsys_get_dioinfo_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			dm_dioinfo_t	*diop);

typedef	int	(*dm_fsys_get_dirattrs_rvp_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			u_int		mask,
			dm_attrloc_t	*locp,
			size_t		buflen,
			void		*bufp,
			size_t		*rlenp,
			union rval	*rvalp);

typedef	int	(*dm_fsys_get_dmattr_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			dm_attrname_t	*attrnamep,
			size_t		buflen,
			void		*bufp,
			size_t		*rlenp);

typedef	int	(*dm_fsys_get_eventlist_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			u_int		type,
			u_int		nelem,
			dm_eventset_t	*eventsetp,	/* in kernel space! */
			u_int		*nelemp);	/* in kernel space! */

typedef	int	(*dm_fsys_get_fileattr_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			u_int		mask,
			dm_stat_t	*statp);

typedef	int	(*dm_fsys_get_region_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			u_int		nelem,
			dm_region_t	*regbufp,
			u_int		*nelemp);

typedef	int	(*dm_fsys_getall_dmattr_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			size_t		buflen,
			void		*bufp,
			size_t		*rlenp);

typedef	int	(*dm_fsys_getall_inherit_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			u_int		nelem,
			dm_inherit_t	*inheritbufp,
			u_int		*nelemp);

typedef	int	(*dm_fsys_init_attrloc_t)(
			struct bhv_desc	*bdp,	/* sometimes root vnode */
			dm_right_t	right,
			dm_attrloc_t	*locp);

typedef	int	(*dm_fsys_mkdir_by_handle_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			void		*hanp,
			size_t		hlen,
			char		*cname);

typedef	int	(*dm_fsys_probe_hole_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			dm_off_t	off,
			dm_size_t	len,
			dm_off_t	*roffp,
			dm_size_t	*rlenp);

typedef	int	(*dm_fsys_punch_hole_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			dm_off_t	off,
			dm_size_t	len);

typedef	int	(*dm_fsys_read_invis_rvp_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			dm_off_t	off,
			dm_size_t	len,
			void		*bufp,
			union rval	*rvp);

typedef	int	(*dm_fsys_release_right_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			u_int		type);	/* DM_FSYS_OBJ or zero */

typedef	int	(*dm_fsys_remove_dmattr_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			int		setdtime,
			dm_attrname_t	*attrnamep);

typedef	int	(*dm_fsys_request_right_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			u_int		type,	/* DM_FSYS_OBJ or zero */
			u_int		flags,
			dm_right_t	newright);

typedef	int	(*dm_fsys_set_dmattr_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			dm_attrname_t	*attrnamep,
			int		setdtime,
			size_t		buflen,
			void		*bufp);

typedef	int	(*dm_fsys_set_eventlist_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			u_int		type,
			dm_eventset_t	*eventsetp,	/* in kernel space! */
			u_int		maxevent);

typedef	int	(*dm_fsys_set_fileattr_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			u_int		mask,
			dm_fileattr_t	*attrp);

typedef	int	(*dm_fsys_set_inherit_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			dm_attrname_t	*attrnamep,
			mode_t		mode);

typedef	int	(*dm_fsys_set_region_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			u_int		nelem,
			dm_region_t	*regbufp,
			dm_boolean_t	*exactflagp);

typedef	int	(*dm_fsys_symlink_by_handle_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			void		*hanp,
			size_t		hlen,
			char		*cname,
			char		*path);

typedef	int	(*dm_fsys_sync_by_handle_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right);

typedef	int	(*dm_fsys_upgrade_right_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			u_int		type);	/* DM_FSYS_OBJ or zero */

typedef	int	(*dm_fsys_write_invis_rvp_t)(
			struct bhv_desc	*bdp,
			dm_right_t	right,
			int		flags,
			dm_off_t	off,
			dm_size_t	len,
			void		*bufp,
			union rval	*rvp);


/* Structure definitions used by the F_DMAPI fcntl() call. */

typedef	struct {
	dm_fsys_switch_t  func_no;	/* function number */
	union {
		dm_fsys_clear_inherit_t clear_inherit;
		dm_fsys_create_by_handle_t create_by_handle;
		dm_fsys_downgrade_right_t downgrade_right;
		dm_fsys_get_allocinfo_rvp_t get_allocinfo_rvp;
		dm_fsys_get_bulkall_rvp_t get_bulkall_rvp;
		dm_fsys_get_bulkattr_rvp_t get_bulkattr_rvp;
		dm_fsys_get_config_t get_config;
		dm_fsys_get_config_events_t get_config_events;
		dm_fsys_get_destroy_dmattr_t get_destroy_dmattr;
		dm_fsys_get_dioinfo_t get_dioinfo;
		dm_fsys_get_dirattrs_rvp_t get_dirattrs_rvp;
		dm_fsys_get_dmattr_t get_dmattr;
		dm_fsys_get_eventlist_t get_eventlist;
		dm_fsys_get_fileattr_t get_fileattr;
		dm_fsys_get_region_t get_region;
		dm_fsys_getall_dmattr_t getall_dmattr;
		dm_fsys_getall_inherit_t getall_inherit;
		dm_fsys_init_attrloc_t init_attrloc;
		dm_fsys_mkdir_by_handle_t mkdir_by_handle;
		dm_fsys_probe_hole_t probe_hole;
		dm_fsys_punch_hole_t punch_hole;
		dm_fsys_read_invis_rvp_t read_invis_rvp;
		dm_fsys_release_right_t release_right;
		dm_fsys_remove_dmattr_t remove_dmattr;
		dm_fsys_request_right_t request_right;
		dm_fsys_set_dmattr_t set_dmattr;
		dm_fsys_set_eventlist_t set_eventlist;
		dm_fsys_set_fileattr_t set_fileattr;
		dm_fsys_set_inherit_t set_inherit;
		dm_fsys_set_region_t set_region;
		dm_fsys_symlink_by_handle_t symlink_by_handle;
		dm_fsys_sync_by_handle_t sync_by_handle;
		dm_fsys_upgrade_right_t upgrade_right;
		dm_fsys_write_invis_rvp_t write_invis_rvp;
	} u_fc;
} fsys_function_vector_t;

typedef	struct	{
	int	code_level;
	int	count;		/* Number of functions in the vector */
	fsys_function_vector_t *vecp;
} dm_fcntl_vector_t;

typedef	struct	{
	size_t	length;			/* length of transfer */
	dm_eventtype_t	max_event;	/* Maximum (WRITE or READ)  event */
	int	error;			/* returned error code */
} dm_fcntl_mapevent_t;

typedef	struct	{
	size_t	length;			/* length of transfer */
	dm_eventtype_t	max_event;	/* Maximum (WRITE or READ)  event */
	dm_eventtype_t	issue_event;	/* Event needed to be issued */
	int	error;			/* returned error code */
} dm_fcntl_testevent_t;

typedef	struct	{
	__int32_t	fsd_dmevmask;	/* di_devmask */
	unsigned short  fsd_padding;
	unsigned short  fsd_dmstate;	/* di_dmstate */
} dm_fcntl_fssetdm_t;
                                                                                

typedef	struct	dm_fcntl {
	int	dmfc_subfunc;
	union	{
		dm_fcntl_vector_t	vecrq;
		dm_fcntl_mapevent_t	maprq;
		dm_fcntl_testevent_t	testrq;
		dm_fcntl_fssetdm_t	setdmrq;
	} u_fcntl;
} dm_fcntl_t;

#define	DM_FCNTL_FSYSVECTOR	1
#define	DM_FCNTL_MAPEVENT	2
#define DM_FCNTL_TESTEVENT	3
#define DM_FCNTL_FSSETDM	4


#endif	/* _KERNEL */


/* The following definitions are needed both by the kernel and by the
   library routines.
*/

#define	DM_MAX_HANDLE_SIZE	56	/* maximum size for a file handle */


/*
 *  Opcodes for dmi syscall.
 */

#define	DM_CLEAR_INHERIT	1
#define	DM_CREATE_BY_HANDLE	2
#define	DM_CREATE_SESSION	3
#define	DM_CREATE_USEREVENT	4
#define	DM_DESTROY_SESSION	5
#define	DM_DOWNGRADE_RIGHT	6
#define	DM_FD_TO_HANDLE		7
#define	DM_FIND_EVENTMSG	8
#define	DM_GET_ALLOCINFO	9
#define	DM_GET_BULKALL		10
#define	DM_GET_BULKATTR		11
#define	DM_GET_CONFIG		12
#define	DM_GET_CONFIG_EVENTS	13
#define	DM_GET_DIOINFO		14
#define	DM_GET_DIRATTRS		15
#define	DM_GET_DMATTR		16
#define	DM_GET_EVENTLIST	17
#define	DM_GET_EVENTS		18
#define	DM_GET_FILEATTR		19
#define	DM_GET_MOUNTINFO	20
#define	DM_GET_REGION		21
#define	DM_GETALL_DISP		22
#define	DM_GETALL_DMATTR	23
#define	DM_GETALL_INHERIT	24
#define	DM_GETALL_SESSIONS	25
#define	DM_GETALL_TOKENS	26
#define	DM_INIT_ATTRLOC		27
#define	DM_MKDIR_BY_HANDLE	28
#define	DM_MOVE_EVENT		29
#define	DM_OBJ_REF_HOLD		30
#define	DM_OBJ_REF_QUERY	31
#define	DM_OBJ_REF_RELE		32
#define	DM_PATH_TO_FSHANDLE	33
#define	DM_PATH_TO_HANDLE	34
#define	DM_PENDING		35
#define	DM_PROBE_HOLE		36
#define	DM_PUNCH_HOLE		37
#define	DM_QUERY_RIGHT		38
#define	DM_QUERY_SESSION	39
#define	DM_READ_INVIS		40
#define	DM_RELEASE_RIGHT	41
#define	DM_REMOVE_DMATTR	42
#define	DM_REQUEST_RIGHT	43
#define	DM_RESPOND_EVENT	44
#define	DM_SEND_MSG		45
#define	DM_SET_DISP		46
#define	DM_SET_DMATTR		47
#define	DM_SET_EVENTLIST	48
#define	DM_SET_FILEATTR		49
#define	DM_SET_INHERIT		50
#define	DM_SET_REGION		51
#define	DM_SET_RETURN_ON_DESTROY 52
#define	DM_SYMLINK_BY_HANDLE	53
#define	DM_SYNC_BY_HANDLE	54
#define	DM_UPGRADE_RIGHT	55
#define	DM_WRITE_INVIS		56


#ifndef _KERNEL

/* Prototype used in the library. */

int		dmi (int opcode, ...);

#endif	/* notdef _KERNEL */

#endif	/* _SYS_DMI_KERN_H */
