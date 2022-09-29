/*
 * FILE: /usr/include/sys/miser_public.h
 *
 * DESCRIPTION:
 *      Miser/cpuset public header file.
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

#ifndef __MISER_PUBLIC_H
#define __MISER_PUBLIC_H

#include <sys/types.h>

#define MISER_QUEUE_ID_MAX		20
#define MISER_REQUEST_BUFFER_SIZE	1024

typedef __uint64_t memory_t;
typedef __uint64_t id_type_t;
typedef __int32_t ncpus_t;
typedef __int32_t bid_t;
typedef __uint32_t miser_time_t;
typedef miser_time_t quanta_t;

#define MISER_EXCEPTION_TERMINATE		0x0001
#define MISER_EXCEPTION_SUSPEND			0x0002
#define MISER_EXCEPTION_WEIGHTLESS		0x0004
#define MISER_EXCEPTION_CALLBACK		0x0008
#define MISER_EXCEPTION_SELF_PAGE		0x0010
#define MISER_EXCEPTION_ADVANCE			0x0020
#define MISER_SEG_DESCR_FLAG_STATIC		0x0040
#define MISER_SEG_DESCR_FLAG_NORMAL		0x0080

#define MISER_CPUSET_CPU_EXCLUSIVE		0x0100
#define MISER_CPUSET_KERN			0x0200
#define MISER_CPUSET_MEMORY_LOCAL		0x0400
#define MISER_CPUSET_MEMORY_EXCLUSIVE		0x0800
#define MISER_CPUSET_MEMORY_KERNEL_AVOID        0x1000

#define MISER_ADMIN_QUEUE_MODIFY_MOVE		1
#define MISER_ADMIN_QUEUE_MODIFY_RESET		2
#define MISER_ADMIN_QUEUE_QUERY_RESOURCES	3
#define MISER_ADMIN_QUEUE_QUERY_JOB		4
#define MISER_ADMIN_QUEUE_QUERY_NAMES		5
#define MISER_ADMIN_JOB_QUERY_SCHEDULE		6
#define MISER_ADMIN_TERMINATE			7
#define MISER_ADMIN_QUEUE_QUERY_RESOURCES1	8

#define MISER_CPUSET_CREATE			20
#define MISER_CPUSET_DESTROY			21
#define MISER_CPUSET_ATTACH			22
#define MISER_CPUSET_QUERY_CPUS			23
#define MISER_CPUSET_QUERY_NAMES		24
#define MISER_CPUSET_QUERY_CURRENT		25
#define MISER_CPUSET_MOVE_PROCS			26
#define MISER_CPUSET_LIST_PROCS			27

#define MISER_KERN_EXIT				40
#define MISER_KERN_TIME_EXPIRE			41
#define MISER_KERN_CALLBACK 			42
#define MISER_KERN_MEMORY_OVERRUN		43

#define MISER_USER_JOB_SUBMIT			50
#define MISER_USER_JOB_KILL			51

typedef enum error{
	MISER_ERR_OK,
	MISER_ERR_POLICY_ID_BAD,
	MISER_ERR_QUEUE_ID_BAD,
	MISER_ERR_RESOURCES_OUT,
	MISER_ERR_RESOURCES_INSUFF,
	MISER_ERR_INTERVAL_BAD,
	MISER_ERR_TIME_QUANTUM_INVALID,
	MISER_ERR_QUEUE_JOBS_ACTIVE,
	MISER_ERR_INVALID_FILE,
	MISER_ERR_INVALID_FNAME,
	MISER_ERR_EFAULT,
	MISER_ERR_PERM,
	MISER_ERR_BAD_TYPE,
	MISER_ERROR,
	MISER_ERR_JOB_ID_NOT_FOUND,
	MISER_ERR_DURATION_TOO_LONG
} error_t;

typedef struct miser_resources {
	ncpus_t			mr_cpus;
	memory_t		mr_memory;	
} miser_resources_t;

typedef struct miser_seg {
	quanta_t		ms_rtime;
	quanta_t		ms_etime;
	int32_t			ms_multiple;
	miser_resources_t	ms_resources;
	uint32_t		ms_flags;
	int			ms_priority;
} miser_seg_t;

typedef struct miser_job {
	id_type_t		mj_queue;
	bid_t			mj_bid;
	miser_time_t		mj_etime;
	int32_t			mj_count;
	miser_seg_t		mj_segments[1];
} miser_job_t;

typedef struct miser_qseg {
	quanta_t		mq_stime;
	quanta_t		mq_etime;
	miser_resources_t       mq_resources;
} miser_qseg_t;

typedef struct miser_move {
	id_type_t		mm_from;	
	id_type_t		mm_to;
	int32_t			mm_count;
	miser_qseg_t		mm_qsegs[1];
} miser_move_t;	

typedef struct miser_bids {
	id_type_t		mb_queue;
	bid_t			mb_first;
	int32_t			mb_count;
	bid_t			mb_buffer[1];
} miser_bids_t;

typedef struct miser_schedule {
	bid_t			ms_job;
	int16_t			ms_first;
	int16_t			ms_count;
	miser_seg_t		ms_buffer[1];
} miser_schedule_t;

typedef struct miser_queue_resources {
	id_type_t		mqr_queue;
	quanta_t		mqr_start;
	int16_t			mqr_count;
	miser_resources_t       mqr_buffer[1];
} miser_queue_resources_t;

typedef struct miser_queue_resources1 {
	id_type_t		mqr_queue;
	quanta_t		mqr_start;
	int32_t			mqr_count;
	miser_resources_t       mqr_buffer[1];
} miser_queue_resources1_t;

typedef struct miser_queue_names {
	uint16_t		mqn_count;
	id_type_t		mqn_queues[1];
} miser_queue_names_t;
	
typedef struct miser_queue_cpuset {
	id_type_t		mqc_queue;
	ncpus_t			mqc_count;
	uint64_t		mqc_flags;
	cpuid_t			mqc_cpuid[1];
} miser_queue_cpuset_t;

typedef struct miser_cpuset_pid {
	id_type_t		mcp_queue;
	int16_t			mcp_count;
	int16_t			mcp_max_count;
	pid_t			mcp_pids[1];
} miser_cpuset_pid_t;

typedef struct miser_data {
	int16_t			md_request_type;
	int16_t			md_error;
	union {
		char			data[MISER_REQUEST_BUFFER_SIZE];
		__uint64_t		_pad0; 
	} u;
} miser_data_t;

#define md_data 	u.data
 
typedef struct miser_request {
	bid_t			mr_bid;
	miser_data_t		mr_req_buffer;
} miser_request_t;

typedef error_t (*miser_policy_func_t)(void*, bid_t,
				     miser_seg_t*,
                                     miser_seg_t*,
				     miser_time_t*);

struct policy_defs {
	char policy_str[sizeof(id_type_t)];
	miser_policy_func_t policy1;
	miser_policy_func_t policy2;
};

struct vm_pool_s;

extern struct policy_defs *miser_policy_begin;
extern struct policy_defs *miser_policy_end;
extern struct vm_pool_s	*miser_pool;	/* VM pool reserved for miser */


#ifdef _KERNEL

struct  job_s;

void	miser_init(void);
int	miser_get_quantum(sysarg_t);
int	miser_get_request(sysarg_t);
int	miser_respond(sysarg_t);
int	miser_reset_job(sysarg_t );
void	miser_send_request(pid_t, int, caddr_t buffer, int length);
int	miser_send_request_scall(sysarg_t, sysarg_t);
void	miser_inform(int16_t flags);
int 	miser_set_resources(sysarg_t, sysarg_t);
int	miser_set_quantum(sysarg_t);
int	miser_check_access(sysarg_t, sysarg_t);
void	miser_time_expire(struct job_s *);
void	miser_exit(struct job_s *);
int 	miser_get_bids(sysarg_t);
int 	miser_get_job(sysarg_t);

void	cpuset_init(void);
int	miser_cpuset_create(miser_queue_cpuset_t *, sysarg_t);
int 	miser_cpuset_destroy(miser_queue_cpuset_t *);
int 	miser_cpuset_attach(miser_queue_cpuset_t *);
int	miser_cpuset_query_current(sysarg_t);
int	miser_cpuset_query_names(sysarg_t);
int	miser_cpuset_query(miser_queue_cpuset_t *, sysarg_t);
int 	miser_cpuset_check_access(void *kt, int cpu);

#endif /* _KERNEL */

#endif /* __MISER_PUBLIC_H */
