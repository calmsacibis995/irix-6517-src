/**************************************************************************
 *									  *
 * Copyright (C) 1986-1993 Silicon Graphics, Inc.			  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __PRIV_EXTERN_H__
#define __PRIV_EXTERN_H__

/* 
	This header file is to define extern's that are defined in
	libc's that are used internally only, thus being "private"
	to libc.  Other extern's such as "printf" are defined in
	a "public" header (stdio.h in this case) and are therefore
	a "public" extern.

	It is possible that several of these externs are already
	defined in othere "private" headers files that are found
	with in each individual sub-directory of libc/src.
*/

#include <sys/types.h>
#include <netinet/in.h> /* for bindresvport */

/* mp/mp_def.c - also defined in the private header mp/mplib.h */
extern int __multi_thread;

/* yp/yp_bind.c */
extern void _yp_unbind_all(void);

/* net/res_send.c */
extern void  _res_close(void);

/* proc/execv.s - see environ(5) */
extern char **environ;

/* gen/irixerror.c */
extern char *__irixerror(int);
extern char *__svr4error(int);
extern char *__sys_errlisterror(int);
extern int _sys_nerr;

/* yp/yp_bind.c */
extern int yp_get_default_domain(char **);

/* yp/yp_update.c */
extern int yp_update(char *, char *, unsigned , char *, int , char *, int );

/* net/bindresvport.c */
extern int bindresvport(int, struct sockaddr_in *);

/* gen/syslog.c */
extern int _using_syslog;

/* gen/_getcwd.c */
#define	GETCWD	0
#define	GETWD	1
extern char *__getcwd(char *, size_t, int);

/* math/atod.c */
extern double _atod ( char *, int , int );

/* math/ltoa.c */
extern int _ltoa ( long, char * );
extern int _ultoa ( unsigned long, char * );

/* gen/oserror.c */
extern void __initperthread_errno(void);

/* signal/sigprocmask.c */
extern int __sgi_prda_procmask(int);

/* ksigqueue.s */
extern int ksigqueue(pid_t, int, int, void *);

#if (!defined(_COMPILER_VERSION) || (_COMPILER_VERSION<700)) /* !Mongoose */
/* compare.s */
extern int __add_and_fetch (int *, int);
#endif

#endif
