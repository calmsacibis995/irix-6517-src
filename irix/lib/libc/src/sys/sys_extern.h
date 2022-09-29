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
#ifndef __SYS_EXTERN_H__
#define __SYS_EXTERN_H__

#include <sys/time.h>
#include <stddef.h>

/* BSD_getime.s */
extern int BSD_getime(struct timeval *);

/* syscall.s */
extern ptrdiff_t syscall(int , ...);

#endif
