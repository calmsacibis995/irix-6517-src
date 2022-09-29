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
 * Simple wait barrier implementation for pthread applications,
 * using pthread mutexes and condition variables.
 */

#include <pthread.h>
#include "bar.h"

int
bar_init(bar_t *barrier, int users)
{
	if (pthread_mutex_init(&barrier->bar_mutex, NULL))
		return -1;

	if (pthread_cond_init(&barrier->bar_wait, NULL))
		return -1;

	barrier->bar_users = users;
	barrier->bar_current_users = 0;

	return 0;
}

int
bar_destroy(bar_t *barrier)
{
	if (pthread_mutex_destroy(&barrier->bar_mutex))
		return -1;

	if (pthread_cond_destroy(&barrier->bar_wait))
		return -1;

	return 0;
}

int
bar_wait(bar_t *barrier)
{
	int ret = 0;

	if (pthread_mutex_lock(&barrier->bar_mutex))
		return -1;

	barrier->bar_current_users++;

	if (barrier->bar_current_users < barrier->bar_users) {
		if (pthread_cond_wait(&barrier->bar_wait, &barrier->bar_mutex)) {
			(void) pthread_mutex_unlock(&barrier->bar_mutex);
			return -1;
		}

		barrier->bar_current_users--;

		if (pthread_mutex_unlock(&barrier->bar_mutex))
			return -1;
	} else {
		ret = pthread_cond_broadcast(&barrier->bar_wait);
		if (pthread_mutex_unlock(&barrier->bar_mutex))
			return -1;
	}

	return ret;
}
