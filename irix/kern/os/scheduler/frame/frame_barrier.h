/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __SYS_BARRIER_H__
#define __SYS_BARRIER_H__

typedef struct syncbar
{
	int nclients;
	int registered;
	volatile int in_counter;
	struct syncbar *next;
} syncbar_t;

#define SYNCBAR_CUTOFF 4

extern syncbar_t* syncbar_create(int nclients);
extern syncbar_t* syncbar_register(syncbar_t* syncbar);
extern void syncbar_unregister(syncbar_t* syncbar);
extern void syncbar_wait(syncbar_t* syncbar);
extern void syncbar_kick(syncbar_t* syncbar);
extern void syncbar_destroy(syncbar_t* syncbar);
extern void syncbar_print(syncbar_t* syncbar);

#endif /* __SYS_BARRIER_H__ */
