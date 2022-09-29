/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ksh:include/sh_config.h	1.6.4.5"


#ifndef _sh_config_
#define _sh_config_     1
/*
 * This has been generated from install/config
 * The information is based on the compile time environment.
 * It may be necessary to change values of some parameters for cross
 *  development environments.
 */

#include        <sys/types.h>

#define _sys_acct_      1
#define _dirent_ 1
#define _sys_dirent_    1
#define _fcntl_ 1
#define _sys_fcntl_     1
#define _sys_file_      1
#define _sys_ioctl_     1
#define _locale_ 1
#define _poll_ 1
#define _sys_stream_    1
#define _sgtty_ 1
#define _sys_times_     1
#define _termio_ 1
#define _sys_termio_    1
#define _termios_   1
#define _sys_wait_   1
#define _unistd_ 1
#define _sys_utsname_   1
#define _bin_grep_      1
#define _usr_bin_lp_    1
#define _usr_bin_vi_    1
#define _bin_newgrp_    1
#define _sys_resource_  1
#ifdef __STDC__
#define VOID    void
#define PROTO 1
#else
#ifdef PROTO
#undef PROTO
#endif
#define VOID	char
#endif
#define SIG_NORESTART   1
#undef _sys_file_
#define signal  sigset
#define sigrelease(s)   sigrelse(s)
#define sig_begin()
#define NFILE   128
#define sh_rand() rand()
#define	SETJMP	setjmp
#define	LONGJMP	longjmp
#define PIPE_ERR        29
#define SHELLMAGIC      1
#define PATH_MAX        1024
#define HZ      100             /* 100 ticks/second of the clock */
#define	FIONREAD        _IOR('f', 127, int)	/* get # bytes to read */
#define LSTAT   1
#define SYSCALL 1
#define DEVFD   1
#define JOBS    1
#define WINSIZE 1
#define _SELECT5_ 1
#define ESH     1
#define MULTIBYTE       1
#define NEWTEST 1
#define OLDTEST 1
#define VSH     1
#define SOCKET	1
#define ESHPLUS	1
#define POSIX	1
#define MULTIGROUPS	1
#define SETREUID 	1
#define BRACEPAT	1
#define SUID_EXEC	1
#define YELLOWP	1
#endif

/*
**      USL change:
**              P_UID changed to PUID because of conflict with sys/procset.h
**		PUID set for security
*/
#define	PUID	1

/*
**      USL change:
**		ECHO_N set for BSD compat.
*/
#define	ECHO_N	1
