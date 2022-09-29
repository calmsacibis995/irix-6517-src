/*
 * FILE: eoe/cmd/miser/cmd/miser/queue.h
 *
 * DESCRIPTION:
 *	Miser queue definitions, declarations, and prototypes.
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


#ifndef __MISER_QUEUE_H
#define __MISER_QUEUE_H


#include <algo.h>
#include "iterator_ext.h"
#include "debug.h"
#include "globals.h"


extern error_t	repack_reschedule_job(jobschedule::iterator&);
extern void	reschedule_job(bid_t&);
extern error_t	G_error;			/* global miser error */


/* private functions */

space::iterator	qmtti(miser_queue&, quanta_t);
long queue_find_block(miser_queue*, quanta_t, quanta_t, miser_seg_t& seg);


inline int 
getmisertime()
{
	struct timeval t;

	gettimeofday(&t, 0);

	/* Return number of seconds till absolute start time */
	return t.tv_sec;

} /* getmisertime */


inline quanta_t
get_start_time(const miser_seg_t&  seg)
{
        quanta_t ret_val = seg.ms_etime - seg.ms_rtime/seg.ms_resources.mr_cpus;

	/* Return job start time based on end time and duration */
        return ret_val;

} /* get_start_time */


inline cyclic_iterator<space>
qtti(miser_queue& q, quanta_t t)
{
	return cycler(q.space_available,
			q.space_available.begin() + t % (2 * duration));

} /* qtti */


struct 
add 
{
	void operator ()(miser_resources_t& jt, const miser_resources_t& js) {
		jt.mr_memory += js.mr_memory;
		jt.mr_cpus   += js.mr_cpus;
	} 

}; /* add */


struct 
sub 
{
	void operator ()(miser_resources_t& jt, const miser_resources_t& js) {
		jt.mr_memory -= js.mr_memory;
		jt.mr_cpus   -= js.mr_cpus;
		assert(jt.mr_cpus >= 0);
	}

}; /* sub */


template<class T>
struct 
miser_mod : T
{
	miser_resources_t bnd;
	miser_mod() {}
	miser_mod(const miser_resources_t& Bnd) : bnd(Bnd) {}

	void operator ()(miser_space& jt, const miser_space& js) {
		T::operator()(jt, js);
	}

	void operator() (miser_space& jt) {
		T::operator()(jt, bnd);
	}

}; /* miser_mod */


template <class T>
struct 
queue_resources
{
        miser_queue& q;
        queue_resources(miser_queue& queue) : q(queue) {}

        int operator () (const miser_seg_t& s) {
		if (q.last_accessed_time >= s.ms_etime) {
			return 1; 
		}

		int stime = get_start_time(s);

		if (q.last_accessed_time > stime) {	
			stime = q.last_accessed_time; 
		}

		assert(s.ms_etime >= stime);
		cyclic_iterator<space> first = qtti(q, stime);
		cyclic_iterator<space> last = qtti(q, s.ms_etime);

		for_each(first, last, T(s.ms_resources));

                return 1;
        }

        int operator () (const miser_qseg_t& rb) {
                for_each(qtti(q, rb.mq_stime), qtti(q, rb.mq_etime),
			 T(rb.mq_resources));

                return 1;

        }

}; /* queue_resources */


struct 
test_access 
{
	int type;

	test_access(int Type) : type(Type) {}

	bool operator()(queuelist::reference q) const { 
		return miser_checkaccess(q.definition->file, type);
	}

}; /* test_access */


inline miser_queue*
find_queue(id_type_t id, int type, int check_access = 1)
{
	/* system queue? retrun system pool, else access error */
	if (id == system_id) {

		if (miser_checkaccess(system_pool->definition->file, type)) {
			return system_pool;

		} else {
			G_error = MISER_ERR_PERM;
			return 0;
		}
	}

	queuelist::iterator i;

	/* find queue id in the list */
	if (id != 0 || !check_access) {
		i = find(active_queues->begin(), active_queues->end(), id);

	} else { 
		i = find_if(active_queues->begin(), active_queues->end(),
				test_access(type));
	}

	/* queue id not in list - set error to bad queue id */
	if (i == active_queues->end()) {
		G_error = MISER_ERR_QUEUE_ID_BAD;
		return 0;
	}

	/* queue configfile access problem - set error to bad permission */
	if (check_access && !miser_checkaccess((*i).definition->file, type)) {
		G_error = MISER_ERR_PERM;
		return 0;
	}

	return &*i;	/* return pointer to miser_queue for the queue id */

} /* find_queue */


struct 
unfix_ctime : public binary_function <quanta_t, miser_seg_t, int>
{
        quanta_t start_time;

        unfix_ctime(quanta_t st) : start_time(st) {;}

        int operator() (miser_seg_t &seg) {
                SYMBOL_PRINT(start_time);
                seg.ms_etime += start_time;
		seg.ms_etime /= time_quantum;
		seg.ms_rtime /= time_quantum;
                SYMBOL_PRINT(seg.ms_etime);
                SYMBOL_PRINT(seg.ms_rtime);

                return 0;
        }

}; /* unfix_ctime */


struct 
fix_ctime : public binary_function <quanta_t, miser_seg_t, int>
{
	quanta_t start_time;

	fix_ctime() {
		start_time = getmisertime();
	}	

	fix_ctime(quanta_t st) : start_time(st) {}

	int operator() (miser_seg_t &seg) {
		seg.ms_etime = seg.ms_etime * time_quantum - start_time;
		seg.ms_rtime *= time_quantum;
		start_time = seg.ms_etime;

		return 1;
	}

}; /* fix_ctime */
	

/* Public interface */
void queue_allocate_blocks(miser_queue*, miser_seg_t* bseg, miser_seg_t* eseg);

error_t	queue_move_blocks(miser_queue*, miser_queue*, miser_qseg_t*,
		miser_qseg_t*);


#endif /* __MISER_QUEUE_H */
