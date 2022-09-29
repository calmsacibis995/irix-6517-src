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
#ifndef __PROC_EXTERN_H__
#define __PROC_EXTERN_H__

#include <unistd.h>
#include <wait.h>	/* for waitsys parameter defines */

/* execvp.c */
extern const char *__execat(const char *, const char *, char *);

/* pagelock.s */
extern int pagelock(void *, size_t , int);

/* waitsys.s */
extern int _waitsys(idtype_t, id_t, siginfo_t *, int, struct rusage *);

/* _wstat.c */
extern int _wstat(int, int);

#endif
