/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * File: error_info.h
 *	SN specific error header file.
 */

#ifndef __SYS_SN_ERROR_INFO_H__
#define __SYS_SN_ERROR_INFO_H__

/*
 * WARNING! Changes to the size of this structures should be done with
 *	    care. The prom is aware of these structures and allocates 
 *	    the structures. Any change will probably mean a new prom.
 */

typedef struct klerr_info_s {
	__uint64_t	ei_time;	/* The RTC of some error event */
} klerr_info_t;

typedef struct hubni_errcnt_s {
	__uint64_t	niretry_count;
	__uint64_t	nicb_count;
	__uint64_t	nisn_count;
	char		niretry_pegged;
	char		nicb_pegged;
	char		nisn_pegged;
} hubni_errcnt_t;

typedef struct hubii_errcnt_s {
	__uint64_t	iicb_count;
	__uint64_t	iisn_count;
	char		iicb_pegged;
	char		iisn_pegged;
} hubii_errcnt_t;

typedef struct hub_errcnt_s {
	int	hec_enable;
	hubii_errcnt_t 	hec_iierr_cnt;
	hubni_errcnt_t 	hec_nierr_cnt;
} hub_errcnt_t;

#define HUB_ERRCNT_TIMEOUT HZ

#define	SN_ERRSTATE_NODES	16
#define	SN_ERRSTATE_BUFSZ_CUBE	_PAGESZ
#define	ROUND_CUBE(n)		\
		(((n) / SN_ERRSTATE_NODES) + (n & (SN_ERRSTATE_NODES - 1) ? 1 : 0))

extern char	*sneb_buf;
extern int	sneb_size;
extern int	snerrste_pages_per_cube;

#endif /* __SYS_SN_ERROR_INFO_H__ */
