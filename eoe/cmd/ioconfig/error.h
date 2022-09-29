/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1995 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ifndef _ERROR_H_
#define _ERROR_H_

/* error type */
typedef enum ioc_error_e { 
	FILE_CREATE_ERR		,
	FILE_OPEN_ERR		,
	FILE_WRITE_ERR		,
	INVALID_ARG_ERR		,
	ATTR_GET_ERR		,
	ATTR_SET_ERR		,
	CTLR_NUM_MISMATCH_ERR	,
	CTLR_NUM_ABSENT_ERR	,
	FORK_FAIL_ERR		,
	FTW_ERR			,
	IOCTL_ERR		,
	UNAME_ERR		,
	SCHEME_ERR		,
	GETPWNAM_ERR		,
	CHOWN_ERR		,
	CHMOD_ERR		,
	OPENDIR_ERR		,
	READLINK_ERR		,
	ERR_TAB_END		
} ioc_error_t;

/* error action type */	
typedef enum ioc_err_action_e {
	_PERROR_	,
	_PERROR_EXIT_	,
	_RETURN_	,
	_PANIC_		,
	_PERROR_RETURN_	
} ioc_err_action_t;

/* error table entry  type */
typedef struct ioc_err_tab_entry_s {
	ioc_error_t		err_tab_error;
	ioc_err_action_t	err_tab_action;
	char			*err_tab_format;
} ioc_err_tab_entry_t;


extern void error(ioc_error_t  , ...);

#endif /* #ifndef _ERROR_H_ */
