/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1997, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Simple wait barrier definitions for pthread applications,
 * using pthread mutexes and condition variables.
 */

#include <pthread.h>

typedef struct bar {
	pthread_mutex_t	 bar_mutex;
	pthread_cond_t	 bar_wait;
	int		 bar_users;
	int		 bar_current_users;
} bar_t;

extern int bar_init(bar_t *barrier, int users);
extern int bar_destroy(bar_t *barrier);
extern int bar_wait(bar_t *barrier);
