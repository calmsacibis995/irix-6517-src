/*
 * FILE: eoe/cmd/miser/cmd/miser/globals.h
 *
 * DESCRIPTION:
 *      Miser globals declaration.
 */

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#ifndef __MISER_GLOBALS_H
#define __MISER_GLOBALS_H

#include "private.h"


extern quanta_t		duration;
extern time_t		time_quantum;
extern queuelist*	active_queues;
extern jobschedule	job_schedule_queue;
extern miser_queue*	system_pool;


#endif /* __MISER_GLOBALS_H */
