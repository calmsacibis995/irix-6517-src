/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef _SYS_KTYPES_H
#define _SYS_KTYPES_H

#include <sys/types.h>

typedef	app32_long_t		irix5_uid_t;
typedef	app64_int_t		irix5_64_uid_t;

typedef	app32_long_t		irix5_gid_t;
typedef	app64_int_t		irix5_64_gid_t;

typedef	app32_long_t		irix5_pid_t;
typedef	app64_int_t		irix5_64_pid_t;

typedef	app32_long_t		irix5_clock_t;
typedef	app64_int_t		irix5_64_clock_t;

typedef	app32_long_t		irix5_off_t;
typedef	app32_long_t		irix5_size_t;
typedef	app32_long_long_t	irix5_n32_off_t;
typedef	app64_long_t		irix5_64_off_t;

typedef	__uint32_t		irix5_fpreg_t;
typedef	__uint64_t		irix5_64_fpreg_t;

#endif	/* _SYS_KTYPES_H */
