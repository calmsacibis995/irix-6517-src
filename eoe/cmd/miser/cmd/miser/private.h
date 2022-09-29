/*
 * FILE: eoe/cmd/miser/cmd/miser/private.h
 *
 * DESCRIPTION:
 *	Miser private declaration and function prototypes.
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


#ifndef __MISER_PRIVATE_H
#define __MISER_PRIVATE_H

#include <sys/miser_public.h>
#include <list.h>
#include <vector.h>
#include <algo.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <syslog.h>
#include "libmiser.h"

#define MISER_MAX_AVAILABLE_TIME 0
#define PRINT_VERBOSE(a)	 if (G_verbose && !G_syslog) merror a

typedef void* miser_acl_t;

struct miser_queue;

extern void*		end_request;
extern void*		start_request;
extern quanta_t		duration;
extern id_type_t	system_id;
extern ncpus_t		G_maxcpus;
extern memory_t		G_maxmemory;
extern bool		G_verbose;
extern time_t		time_quantum;
extern miser_queue*	system_pool;


void queue_update(miser_queue* queue);

struct 
miser_space : miser_resources_t 
{
        miser_space() {
		mr_memory = 0;
		mr_cpus   = 0;
	}

        miser_space(memory_t mem, ncpus_t nc) {
		mr_memory = mem;
		mr_cpus   = nc;
	}

	miser_space(miser_resources_t res) : miser_resources_t(res) {}

}; /* miser_space */


typedef vector<miser_space>	space;
typedef vector<miser_qseg_t>	resources;


class miser_policy; 

struct 
miser_policy 
{
        id_type_t		id;
        miser_policy_func_t	do_policy;
        miser_policy_func_t	redo_policy;

        miser_policy() : id(0), do_policy(0), redo_policy(0) {}

        miser_policy(const policy_defs& p)
			: do_policy(p.policy1), redo_policy(p.policy2) {
		char buf[sizeof(id)];
		strncpy(buf, p.policy_str, sizeof(buf));
		transform(buf, buf + sizeof(buf), (char *)&id, tolower);
	}

	operator id_type_t() { return id ;}

}; /* miser_policy */

   
typedef	list<miser_policy>	policy_list;
extern	policy_list		policies;


inline miser_policy*
find_policy_manager(const id_type_t& id)
{
        policy_list::iterator i = find(policies.begin(), policies.end(), id);

        if (i == policies.end()) {
                return 0;
	}

	return &*i;

} /* find_policy_manager */


struct 
queue_definition 
{
	id_type_t	queue;
	id_type_t	policy;
	resources	records;
	FILE*		file;

	queue_definition(id_type_t Queue, id_type_t Policy, int nsegs)
		: queue(Queue), policy(Policy), records(nsegs), file(0) {}

	~queue_definition() {
		if (file) {
			fclose(file);
		}
	}

}; /* queue_definition */


void convert_to_space_vector(resources::iterator, resources::iterator,
				space::iterator, space::iterator);

void convert_to_localtime(space::iterator, space::iterator,
				space::iterator, space::iterator);

struct 
miser_queue 
{
        quanta_t		last_accessed_time;
	quanta_t		period;
        miser_acl_t*		acl;
	miser_policy*		policy;
	queue_definition*	definition;
        space			space_available;

	void init() {
		if (space_available.size() == 0) {
			space_available.insert(space_available.begin(),
						duration * 2,
						miser_space());
		}

		period = 0;
		queue_update(this);
		acl = 0;
		policy = definition 
			? find_policy_manager(definition->policy) : 0;
	}

	void init(queue_definition* spec) {
		definition = spec;
		init();
		space_reset(0);
		space_reset(1);
	}

	miser_queue() : definition(0) {};

	miser_queue(queue_definition* Def)
			: space_available(duration*2), definition(Def) {
		init();
		space_reset(0);
		space_reset(1);
	}

	void space_reset(int interval);

#ifdef DEBUG
	void verify_queue() {}
#endif

	operator id_type_t() { return definition->queue; }
	~miser_queue()       { if (definition) delete definition; }

}; /* miser_queue */


typedef list <miser_queue> queuelist;


struct 
miser_job_schedule 
{
        bid_t			batch_id;
        vector<miser_seg_t>	schedule;
	miser_queue*		queue;
        miser_time_t 		now;
	
	miser_job_schedule() : batch_id(0), queue(0) {}

	void init(bid_t Bid, miser_seg_t* Begin, miser_seg_t* End) {
		batch_id = Bid;
		copy(Begin, End, back_inserter(schedule));
	}

	~miser_job_schedule();
	operator bid_t()	{ return batch_id; }

}; /* miser_job_schedule */


typedef list<miser_job_schedule> jobschedule;

error_t	job_schedule(id_type_t, bid_t, miser_seg_t*, miser_seg_t*, 
		quanta_t*, int ca = 1);
bool	job_reschedule(miser_job_schedule&);
void	job_retarget(miser_job_schedule&);
error_t	job_deschedule(bid_t);
void	jobs_move(miser_queue *from, miser_queue *to);

error_t	queueid_move_block(id_type_t from, id_type_t to,
		miser_qseg_t* rb, miser_qseg_t* erb);
error_t	queue_create(queue_definition* spec, queuelist *);
error_t	queue_query_resources(id_type_t id, miser_queue_resources_t *q);
error_t	queue_query_resources1(id_type_t id, miser_queue_resources1_t *q);
error_t	job_query_schedule(bid_t, int, miser_seg_t*, miser_seg_t*&);
bid_t*	queue_query_jobs(id_type_t, bid_t, bid_t*, bid_t*);
error_t	queue_query_names(id_type_t *, id_type_t *&);
error_t	queue_rebuild_from_jobs (miser_queue*, jobschedule::iterator,
		jobschedule::iterator);
error_t	queue_rebuild_from_queues(queuelist*);

error_t	miser_reconfig(char* filename, queuelist*&);
error_t	miser_rebuild(void);

bool	operator== (miser_job_schedule&, miser_job_schedule& job);
bool	operator< (miser_job_schedule&, miser_job_schedule& job);
bool	operator> (miser_job_schedule&, miser_job_schedule& job);
bool	operator== (miser_queue&, miser_queue& job);

char*	print_miser_error(error_t error);
void	setnow();


#endif /* __MISER_PRIVATE_H */
