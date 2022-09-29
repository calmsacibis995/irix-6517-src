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

#ifndef __BATCH_H__
#define __BATCH_H__

#define batch_get_bid() (curuthread->ut_job->j_un.ju_batch.b_bid)

int	batch_create(void);
int	batch_set_resources(miser_request_t *req);
void 	batch_init_cpus(int);
extern  batchq_t batch_queue;
void 	batch_push_queue_locked(job_t*);
#endif /* __BATCH_H__ */
