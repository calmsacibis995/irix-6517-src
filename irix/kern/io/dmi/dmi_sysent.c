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
#ident "$Revision: 1.30 $"


#include <sys/types.h>
#include <sys/capability.h>

#include "dmi_private.h"

struct sysdmia {
	sysarg_t cmd;
	sysarg_t arg1, arg2, arg3, arg4, arg5, arg6, arg7;
};

/*
 *  Entry point for DMI syscall.
 */

int
dmi(
	struct sysdmia	*uap,
	rval_t		*rvp)
{
	int		error;
	uint64_t	xtrargs[4];

	if (!_CAP_ABLE(CAP_DEVICE_MGT))
		return(EPERM);

	switch (uap->cmd) {
	case DM_CLEAR_INHERIT:
		error = dm_clear_inherit(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(dm_attrname_t *) uap->arg5);	/* attrnamep */
		break;
	case DM_CREATE_BY_HANDLE:
		error = dm_create_by_handle(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* dirhanp */
				(size_t)	uap->arg3,	/* dirhlen */
				(dm_token_t)	uap->arg4,	/* token */
				(void *)	uap->arg5,	/* hanp */
				(size_t)	uap->arg6,	/* hlen */
				(char *)	uap->arg7);	/* cname */
		break;
	case DM_CREATE_SESSION:
		error = dm_create_session(
				(dm_sessid_t)	uap->arg1,	/* oldsid */
				(char *)	uap->arg2,	/* sessinfop */
				(dm_sessid_t *)	uap->arg3);	/* newsidp */
		break;
	case DM_CREATE_USEREVENT:
		error = dm_create_userevent(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(size_t)	uap->arg2,	/* msglen */
				(void *)	uap->arg3,	/* msgdatap */
				(dm_token_t *)	uap->arg4);	/* tokenp */
		break;
	case DM_DESTROY_SESSION:
		error = dm_destroy_session(
				(dm_sessid_t)	uap->arg1);	/* sid */
		break;
	case DM_DOWNGRADE_RIGHT:
		error = dm_downgrade_right(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4);	/* token */
		break;
	case DM_FD_TO_HANDLE:
		error = dm_fd_to_hdl(
				(int)		uap->arg1,	/* fd */
				(void *)	uap->arg2,	/* hanp */
				(size_t *)	uap->arg3);	/* hlenp */
		break;
	case DM_FIND_EVENTMSG:
		error = dm_find_eventmsg(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(dm_token_t)	uap->arg2,	/* token */
				(size_t)	uap->arg3,	/* buflen */
				(void *)	uap->arg4,	/* bufp */
				(size_t *)	uap->arg5);	/* rlenp */
		break;
	case DM_GET_ALLOCINFO:
		if (copyin((char *) uap->arg7, xtrargs, sizeof(xtrargs)))
			return(EFAULT);
		error = dm_get_allocinfo_rvp(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(dm_off_t *)	uap->arg5,	/* offp */
				(u_int)		uap->arg6,	/* nelem */
				(dm_extent_t *)	xtrargs[0],	/* extentp */
				(u_int *)	xtrargs[1],	/* nelemp */
						rvp);
		break;
	case DM_GET_BULKALL:
		if (copyin((char *) uap->arg7, xtrargs, sizeof(xtrargs)))
			return(EFAULT);
		error = dm_get_bulkall_rvp(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(u_int)		uap->arg5,	/* mask */
				(dm_attrname_t *) uap->arg6,	/* attrnamep */
				(dm_attrloc_t *) xtrargs[0],	/* locp */
				(size_t)	xtrargs[1],	/* buflen */
				(void *)	xtrargs[2],	/* bufp */
				(size_t *)	xtrargs[3],	/* rlenp */
						rvp);
		break;
	case DM_GET_BULKATTR:
		if (copyin((char *) uap->arg7, xtrargs, sizeof(xtrargs)))
			return(EFAULT);
		error = dm_get_bulkattr_rvp(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(u_int)		uap->arg5,	/* mask */
				(dm_attrloc_t *)uap->arg6,	/* locp */
				(size_t)	xtrargs[0],	/* buflen */
				(void *)	xtrargs[1],	/* bufp */
				(size_t *)	xtrargs[2],	/* rlenp */
						rvp);
		break;
	case DM_GET_CONFIG:
		error = dm_get_config(
				(void *)	uap->arg1,	/* hanp */
				(size_t)	uap->arg2,	/* hlen */
				(dm_config_t)	uap->arg3,	/* flagname */
				(dm_size_t *)	uap->arg4);	/* retvalp */
		break;
	case DM_GET_CONFIG_EVENTS:
		error = dm_get_config_events(
				(void *)	uap->arg1,	/* hanp */
				(size_t)	uap->arg2,	/* hlen */
				(u_int)		uap->arg3,	/* nelem */
				(dm_eventset_t *) uap->arg4,	/* eventsetp */
				(u_int *)	uap->arg5);	/* nelemp */
		break;
	case DM_GET_DIOINFO:
		error = dm_get_dioinfo(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(dm_dioinfo_t *)uap->arg5);	/* diop */
		break;
	case DM_GET_DIRATTRS:
		if (copyin((char *) uap->arg7, xtrargs, sizeof(xtrargs)))
			return(EFAULT);
		error = dm_get_dirattrs_rvp(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(u_int)		uap->arg5,	/* mask */
				(dm_attrloc_t *)uap->arg6,	/* locp */
				(size_t)	xtrargs[0],	/* buflen */
				(void *)	xtrargs[1],	/* bufp */
				(size_t *)	xtrargs[2],	/* rlenp */
						rvp);
		break;
	case DM_GET_DMATTR:
		if (copyin((char *) uap->arg7, xtrargs, sizeof(xtrargs)))
			return(EFAULT);
		error = dm_get_dmattr(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(dm_attrname_t *) uap->arg5,	/* attrnamep */
				(size_t)	uap->arg6,	/* buflen */
				(void *)	xtrargs[0],	/* bufp */
				(size_t *)	xtrargs[1]);	/* rlenp */
		break;
	case DM_GET_EVENTLIST:
		error = dm_get_eventlist(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(u_int)		uap->arg5,	/* nelem */
				(dm_eventset_t *) uap->arg6,	/* eventsetp */
				(u_int *)	uap->arg7);	/* nelemp */
		break;
	case DM_GET_EVENTS:
		error = dm_get_events(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(u_int)		uap->arg2,	/* maxmsgs */
				(u_int)		uap->arg3,	/* flags */
				(size_t)	uap->arg4,	/* buflen */
				(void *)	uap->arg5,	/* bufp */
				(size_t *)	uap->arg6);	/* rlenp */
		break;
	case DM_GET_FILEATTR:
		error = dm_get_fileattr(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(u_int)		uap->arg5,	/* mask */
				(dm_stat_t *)	uap->arg6);	/* statp */
		break;
	case DM_GET_MOUNTINFO:
		error = dm_get_mountinfo(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(size_t)	uap->arg5,	/* buflen */
				(void *)	uap->arg6,	/* bufp */
				(size_t *)	uap->arg7);	/* rlenp */
		break;
	case DM_GET_REGION:
		error = dm_get_region(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(u_int)		uap->arg5,	/* nelem */
				(dm_region_t *)	uap->arg6,	/* regbufp */
				(u_int *)	uap->arg7);	/* nelemp */
		break;
	case DM_GETALL_DISP:
		error = dm_getall_disp(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(size_t)	uap->arg2,	/* buflen */
				(void *)	uap->arg3,	/* bufp */
				(size_t *)	uap->arg4);	/* rlenp */
		break;
	case DM_GETALL_DMATTR:
		error = dm_getall_dmattr(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(size_t)	uap->arg5,	/* buflen */
				(void *)	uap->arg6,	/* bufp */
				(size_t *)	uap->arg7);	/* rlenp */
		break;
	case DM_GETALL_INHERIT:
		error = dm_getall_inherit(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(u_int)		uap->arg5,	/* nelem */
				(dm_inherit_t *)uap->arg6,	/* inheritbufp*/
				(u_int *)	uap->arg7);	/* nelemp */
		break;
	case DM_GETALL_SESSIONS:
		error = dm_getall_sessions(
				(u_int)		uap->arg1,	/* nelem */
				(dm_sessid_t *)	uap->arg2,	/* sidbufp */
				(u_int *)	uap->arg3);	/* nelemp */
		break;
	case DM_GETALL_TOKENS:
		error = dm_getall_tokens(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(u_int)		uap->arg2,	/* nelem */
				(dm_token_t *)	uap->arg3,	/* tokenbufp */
				(u_int *)	uap->arg4);	/* nelemp */
		break;
	case DM_INIT_ATTRLOC:
		error = dm_init_attrloc(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(dm_attrloc_t *) uap->arg5);	/* locp */
		break;
	case DM_MKDIR_BY_HANDLE:
		error = dm_mkdir_by_handle(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* dirhanp */
				(size_t)	uap->arg3,	/* dirhlen */
				(dm_token_t)	uap->arg4,	/* token */
				(void *)	uap->arg5,	/* hanp */
				(size_t)	uap->arg6,	/* hlen */
				(char *)	uap->arg7);	/* cname */
		break;
	case DM_MOVE_EVENT:
		error = dm_move_event(
				(dm_sessid_t)	uap->arg1,	/* srcsid */
				(dm_token_t)	uap->arg2,	/* token */
				(dm_sessid_t)	uap->arg3,	/* targetsid */
				(dm_token_t *)	uap->arg4);	/* rtokenp */
		break;
	case DM_OBJ_REF_HOLD:
		error = dm_obj_ref_hold(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(dm_token_t)	uap->arg2,	/* token */
				(void *)	uap->arg3,	/* hanp */
				(size_t)	uap->arg4);	/* hlen */
		break;
	case DM_OBJ_REF_QUERY:
		error = dm_obj_ref_query_rvp(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(dm_token_t)	uap->arg2,	/* token */
				(void *)	uap->arg3,	/* hanp */
				(size_t)	uap->arg4,	/* hlen */
						rvp);
		break;
	case DM_OBJ_REF_RELE:
		error = dm_obj_ref_rele(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(dm_token_t)	uap->arg2,	/* token */
				(void *)	uap->arg3,	/* hanp */
				(size_t)	uap->arg4);	/* hlen */
		break;
	case DM_PATH_TO_FSHANDLE:
		error = dm_path_to_fshdl(
				(char *)	uap->arg1,	/* path */
				(void *)	uap->arg2,	/* hanp */
				(size_t *)	uap->arg3);	/* hlenp */
		break;
	case DM_PATH_TO_HANDLE:
		error = dm_path_to_hdl(
				(char *)	uap->arg1,	/* path */
				(void *)	uap->arg2,	/* hanp */
				(size_t *)	uap->arg3);	/* hlenp */
		break;
	case DM_PENDING:
		error = dm_pending(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(dm_token_t)	uap->arg2,	/* token */
				(dm_timestruct_t *) uap->arg3);	/* delay */
		break;
	case DM_PROBE_HOLE:
		if (copyin((char *) uap->arg5, xtrargs, sizeof(xtrargs)))
			return(EFAULT);
		error = dm_probe_hole(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(dm_off_t)	xtrargs[0],	/* off */
				(dm_size_t)	xtrargs[1],	/* len */
				(dm_off_t *)	uap->arg6,	/* roffp */
				(dm_size_t *)	uap->arg7);	/* rlenp */
		break;
	case DM_PUNCH_HOLE:
		if (copyin((char *)uap->arg5, xtrargs, sizeof(xtrargs)))
			return(EFAULT);
		error = dm_punch_hole(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(dm_off_t)	xtrargs[0],	/* off */
				(dm_size_t)	xtrargs[1]);	/* len */
		break;
	case DM_QUERY_RIGHT:
		error = dm_query_right(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(dm_right_t *)	uap->arg5);	/* rightp */
		break;
	case DM_QUERY_SESSION:
		error = dm_query_session(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(size_t)	uap->arg2,	/* buflen */
				(void *)	uap->arg3,	/* bufp */
				(size_t *)	uap->arg4);	/* rlenp */
		break;
	case DM_READ_INVIS:
		if (copyin((char *) uap->arg5, xtrargs, sizeof(xtrargs)))
			return(EFAULT);
		error = dm_read_invis_rvp(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(dm_off_t)	xtrargs[0],	/* off */
				(dm_size_t)	xtrargs[1],	/* len */
				(void *)	uap->arg6,	/* bufp */
						rvp);
		break;
	case DM_RELEASE_RIGHT:
		error = dm_release_right(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4);	/* token */
		break;
	case DM_REMOVE_DMATTR:
		error = dm_remove_dmattr(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(int)		uap->arg5,	/* setdtime */
				(dm_attrname_t *) uap->arg6);	/* attrnamep */
		break;
	case DM_REQUEST_RIGHT:
		error = dm_request_right(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(u_int)		uap->arg5,	/* flags */
				(dm_right_t)	uap->arg6);	/* right */
		break;
	case DM_RESPOND_EVENT:
		error = dm_respond_event(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(dm_token_t)	uap->arg2,	/* token */
				(dm_response_t)	uap->arg3,	/* response */
				(int)		uap->arg4,	/* reterror */
				(size_t)	uap->arg5,	/* buflen */
				(void *)	uap->arg6);	/* respbufp */
		break;
	case DM_SEND_MSG:
		error = dm_send_msg(
				(dm_sessid_t)	uap->arg1,	/* targetsid */
				(dm_msgtype_t)	uap->arg2,	/* msgtype */
				(size_t)	uap->arg3,	/* buflen */
				(void *)	uap->arg4);	/* bufp */
		break;
	case DM_SET_DISP:
		error = dm_set_disp(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(dm_eventset_t *) uap->arg5,	/* eventsetp */
				(u_int)		uap->arg6);	/* maxevent */
		break;
	case DM_SET_DMATTR:
		if (copyin((char *) uap->arg7, xtrargs, sizeof(xtrargs)))
			return(EFAULT);
		error = dm_set_dmattr(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(dm_attrname_t *) uap->arg5,	/* attrnamep */
				(int)		uap->arg6,	/* setdtime */
				(size_t)	xtrargs[0],	/* buflen */
				(void *)	xtrargs[1]);	/* bufp */
		break;
	case DM_SET_EVENTLIST:
		error = dm_set_eventlist(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(dm_eventset_t *) uap->arg5,	/* eventsetp */
				(u_int)		uap->arg6);	/* maxevent */
		break;
	case DM_SET_FILEATTR:
		error = dm_set_fileattr(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(u_int)		uap->arg5,	/* mask */
				(dm_fileattr_t *)uap->arg6);	/* attrp */
		break;
	case DM_SET_INHERIT:
		error = dm_set_inherit(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(dm_attrname_t *)uap->arg5,	/* attrnamep */
				(mode_t)	uap->arg6);	/* mode */
		break;
	case DM_SET_REGION:
		error = dm_set_region(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(u_int)		uap->arg5,	/* nelem */
				(dm_region_t *)	uap->arg6,	/* regbufp */
				(dm_boolean_t *) uap->arg7);	/* exactflagp */
		break;
	case DM_SET_RETURN_ON_DESTROY:
		error = dm_set_return_on_destroy(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(dm_attrname_t *) uap->arg5,	/* attrnamep */
				(dm_boolean_t)	uap->arg6);	/* enable */
		break;
	case DM_SYMLINK_BY_HANDLE:
		if (copyin((char *) uap->arg7, xtrargs, sizeof(xtrargs)))
			return(EFAULT);
		error = dm_symlink_by_handle(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* dirhanp */
				(size_t)	uap->arg3,	/* dirhlen */
				(dm_token_t)	uap->arg4,	/* token */
				(void *)	uap->arg5,	/* hanp */
				(size_t)	uap->arg6,	/* hlen */
				(char *)	xtrargs[0],	/* cname */
				(char *)	xtrargs[1]);	/* path */
		break;
	case DM_SYNC_BY_HANDLE:
		error = dm_sync_by_handle(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4);	/* token */
		break;
	case DM_UPGRADE_RIGHT:
		error = dm_upgrade_right(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4);	/* token */
		break;
	case DM_WRITE_INVIS:
		if (copyin((char *) uap->arg6, xtrargs, sizeof(xtrargs)))
			return(EFAULT);
		error = dm_write_invis_rvp(
				(dm_sessid_t)	uap->arg1,	/* sid */
				(void *)	uap->arg2,	/* hanp */
				(size_t)	uap->arg3,	/* hlen */
				(dm_token_t)	uap->arg4,	/* token */
				(int)		uap->arg5,	/* flags */
				(dm_off_t)	xtrargs[0],	/* off */
				(dm_size_t)	xtrargs[1],	/* len */
				(void *)	uap->arg7,	/* bufp */
						rvp);
		break;
	default:
		error = ENOSYS;
		break;
	}
	return(error);
}
