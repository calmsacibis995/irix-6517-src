/*
 * FILE: eoe/cmd/miser/queue.C
 *
 * DESCRIPTION:
 *	Miser queue resource management functions.
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


#include <sys/sysmp.h>
#include <errno.h>
#include "queue.h"
#include "debug.h"
#include "algo_ext.h"
#include "libmiser.h"


#define MAX_DURATION	259200	/* Maximum value for system queue length */

extern error_t G_error;         /* global miser error */
quanta_t now;

void 
miser_queue::space_reset(int interval)
{
	space::iterator i = space_available.begin()
				+ ( interval ? duration : 0);
	space tspace(duration);

	convert_to_space_vector(definition->records.begin(),
				definition->records.end(),
				tspace.begin(), tspace.end());

	convert_to_localtime(i, i + duration, tspace.begin(), tspace.end());

} /* space_reset */


struct system_queue_updater 
{
	space tmp;

	space::iterator begin;

	system_queue_updater(quanta_t length, quanta_t offset) 
		: tmp(length), begin(system_pool->space_available.begin()
		+ (length == duration ? offset : 0)) { 
		SYMBOL_PRINT(system_pool->space_available); 
	}

	void operator() (miser_queue& q) {
		space tspace(duration); 

		convert_to_space_vector(q.definition->records.begin(),
					q.definition->records.end(),
					tspace.begin(), 
					tspace.begin() + duration);

		convert_to_localtime(tmp.begin(), tmp.begin() + duration,
				     tspace.begin(), 
				     tspace.begin() + duration);

		copy(tmp.begin(), tmp.begin() + duration,
			tmp.begin() + duration, tmp.end());

		apply(begin, begin + tmp.size(), tmp.begin(), sub());
	}

}; /* system_queue_updater */


void
setnow()
{
	now = getmisertime() / time_quantum + 1;

} /* setnow */


void
queue_update(miser_queue* queue)
{
	int old_period = queue->period;

	queue->last_accessed_time = now;
	queue->period = now / duration;

	STRING_PRINT(miser_qstr(queue->definition->queue));
	SYMBOL_PRINT(system_pool);

	quanta_t length;

	switch (queue->period - old_period) {

	case 0:
		return;

	case 1:
		length = duration;
		queue->space_reset(old_period & 1);

		break;

	default:
		length = 2 * duration;
		queue->space_reset(0);
		queue->space_reset(1);

		break;
	}

	if (queue != system_pool) {
		return;
	}

	for_each(active_queues->begin(), active_queues->end(),
		system_queue_updater(length, duration * (old_period & 1)));

} /* queue_update */


void
queue_allocate_blocks(miser_queue* q, miser_seg_t* bseg, miser_seg_t* eseg)
{
	for_each(bseg, eseg, queue_resources<miser_mod<sub> >(*q));

} /* queue_allocate_blocks */


inline quanta_t
qitt(miser_queue& q, cyclic_iterator<space> i)
{
	quanta_t access_offset = q.last_accessed_time % (2 * duration);
	quanta_t iterator_offset = get_offset(i);

	return q.last_accessed_time
		+ iterator_offset - access_offset
		+ (access_offset > iterator_offset 
		? 2 * duration : 0);

} /* qitt */


struct dominates : binary_function<miser_space, miser_space, bool> 
{
	bool operator () (const miser_space& greater, 
		const miser_space& lesser) const {
		
		return lesser.mr_memory <= greater.mr_memory 
			&& lesser.mr_cpus <= greater.mr_cpus;
	}	

}; /* dominates */


long
queue_find_block(miser_queue* q, quanta_t start, quanta_t end, 
	miser_seg_t& seg)
{
        if (end == MISER_MAX_AVAILABLE_TIME) {
                end = duration + q->last_accessed_time;
	}

	int length = 1; 

	if (seg.ms_rtime != 0) {
        	length = (seg.ms_rtime - 1)/seg.ms_resources.mr_cpus + 1;
	}

#ifdef DEBUG

	int n = 0;

	distance(qtti(*q, start), qtti(*q,end), n);

#endif

	cyclic_iterator<space> b = qtti(*q, start);
	cyclic_iterator<space> block = find_seq(qtti(*q, start), 
						qtti(*q, end),
						bind2nd(dominates(), 
						seg.ms_resources), 
						length);

	if (block == qtti(*q, end)) {
                 return -1;
	}

	seg.ms_rtime = length * seg.ms_resources.mr_cpus;

	return qitt(*q, block);

} /* queue_find_block */


struct check 
{
	miser_queue	*q;
	static error_t	error;

	check(miser_queue* queue) : q(queue) {}

	bool operator () (miser_qseg_t& c)
	{
		SYMBOL_PRINT(c.mq_etime);
		SYMBOL_PRINT(q->last_accessed_time);
		SYMBOL_PRINT(c.mq_stime);

		if (c.mq_etime > q->last_accessed_time + duration
				|| c.mq_stime < q->last_accessed_time) {
			error = MISER_ERR_INTERVAL_BAD;
			return true;
		}

		cyclic_iterator<space> r = 
			find_if(qtti(*q, c.mq_stime), qtti(*q, c.mq_etime),
				not1(bind2nd(dominates(), c.mq_resources)));

		if (r == qtti(*q, c.mq_etime)) {
			return false;
		}

                error = MISER_ERR_RESOURCES_INSUFF;

		return true;
	}

}; /* check */


error_t check::error;


void
fix_block(miser_qseg_t &rb)
{
	rb.mq_stime /= time_quantum;
	rb.mq_etime /= time_quantum;
	rb.mq_stime += now;	
	rb.mq_etime += now;

} /* fix_block */	


void
unfix_block(miser_qseg_t &rb)
{
	rb.mq_stime *= time_quantum;
	rb.mq_etime *= time_quantum;

} /* unfix_block */


error_t
queueid_move_block(id_type_t from, id_type_t to, miser_qseg_t* rb, 
			miser_qseg_t* erb )
{
	if ((void*)erb < start_request || (void*)erb > end_request) {
		return MISER_ERR_EFAULT;
	}

        miser_queue* from_queue = find_queue(from, W_OK);

        if (!from_queue) {
		return G_error;
	}
        
        miser_queue* to_queue = find_queue(to, W_OK);

        if (!to_queue) {
		return G_error;
	}

        if (from_queue == to_queue) {
		return MISER_ERR_QUEUE_ID_BAD;
	}

        queue_update(to_queue);
        queue_update(from_queue);

	for_each(rb, erb, fix_block);

	if (find_if(rb, erb, check(from_queue)) != erb) {
                 return check::error;
	}

        for_each(rb, erb, queue_resources<miser_mod<sub> >(*from_queue));
	for_each(rb, erb, queue_resources<miser_mod<add> >(*to_queue));

	for_each(rb, erb, unfix_block);

	return MISER_ERR_OK;

} /* queueid_move_block */


int
convert_dst(space::iterator dbegin, space::iterator dend,
		space::iterator sbegin, space::iterator send)
{
	tzset();

	time_t starttime = now;
	starttime *= time_quantum; 
	time_t endtime = starttime  + duration * time_quantum;
	int quanta_to_hour = 3600 / time_quantum;
	int fit = 3600 % time_quantum; 
	struct tm starttm, endtm;

	localtime_r(&starttime, &starttm);	
	localtime_r(&endtime, &endtm);

	if (endtm.tm_isdst < 0 || starttm.tm_isdst < 0) {
		merror("\nmiser: information on daylight savings is not "
			"available, therefore, only using timezone "
			"difference.\n");
		return 1;	/* error */
	}

	if (endtm.tm_isdst == starttm.tm_isdst) { 
		return 1;	/* error */
	}

	space::iterator b = dbegin + (starttime / time_quantum) % duration;
	space::iterator s = sbegin + (starttime / time_quantum) % duration;

	int found =  0;

	for (int i = 0; i < duration; i++) {
		time_t curtime = starttime + i * time_quantum;
		struct tm curtm, futtm;

		localtime_r(&curtime, &curtm);
		curtime += 3600;
		localtime_r(&curtime, &futtm);

		if (((futtm.tm_isdst != curtm.tm_isdst) 
			|| (curtm.tm_isdst != starttm.tm_isdst)) && !found) {

			if (endtm.tm_isdst)  {
				if (b - quanta_to_hour < dbegin) {
					int left = quanta_to_hour - (b-dbegin);
					s = dend - left;
					i -= quanta_to_hour;
				} else {
					s += quanta_to_hour;
					i -= quanta_to_hour;
				}	

			} else {
				if (b + quanta_to_hour >= dend) {
					int left = b + quanta_to_hour - dend;

					fill(b, dend, miser_space());
					fill(dbegin, dbegin + left, 	
							miser_space()); 
					b = dbegin + left; 
					i += quanta_to_hour;

				} else {
					fill(b, b+quanta_to_hour, 
						miser_space(0, 0));
					b += quanta_to_hour;
					i += quanta_to_hour;
				}
			}

			if (fit) {
				*b = miser_space(0,0);
				i++;
				continue;
			}

			found = 1;
		}	

		*b = *s;
		b++; s++;

		if (s >= send) {
			s = sbegin;
		}
			
		if (b >= dend) {
			b = dbegin;
		}
	}	

	return 0;	/* success */

} /* convert_dst */	


void
convert_to_localtime(space::iterator dbegin, space::iterator dend,
			space::iterator sbegin, space::iterator send)
/*
 * called when the start and end times are both whithin daylight savings.
 */
{
	tzset();
	int time_zone = timezone;
	time_t starttime = now ;
	starttime *= time_quantum; 
	time_t endtime = starttime  + duration * time_quantum;
	space::iterator s = sbegin, b = dbegin;

	/* First find real time 0 */
	struct tm starttm, endtm;
	localtime_r(&starttime, &starttm);
	localtime_r(&endtime,&endtm);

	if (starttm.tm_isdst) {
		time_zone = altzone;	
	}

	int timezero = (time_zone / time_quantum) % (long) duration;

	if (!timezero) { 
		copy(sbegin, send, dbegin, dend); 
		return;
	}

	if (b + timezero >= dend) {
		int k = b + timezero - dend;
		b = dbegin + k; 

	} else if (b + timezero <= dbegin) {
		b = dend + timezero;

	} else {
		b += timezero;
	}

	for (int i = 0; i < duration; i++) {
		*b = *s;
		b++;
		s++;

		if (b >= dend) {
			b = dbegin;
		}

		if (s >= send) {
			s = sbegin;
		}
	}

	if (!convert_dst(sbegin, send, dbegin, dend)) {
		copy(sbegin, send, dbegin, dend);
	}
	
} /* convert_to_localtime */


void
convert_to_space_vector(miser_qseg_t* rb, miser_qseg_t* erb,
			space::iterator begin, space::iterator end)
{
	miser_qseg_t* rbp;

	for (rbp = rb; rbp != erb; rbp++) {
		fill(begin + rbp->mq_stime, begin + rbp->mq_etime,
			miser_space(rbp->mq_resources));
	}

	quanta_t max_time = 0;

	for (rbp = rb; rbp != erb; rbp++) {
		if (max_time < rbp->mq_etime) {
			max_time = rbp->mq_etime;
		}
	}
	
	copy(begin, end, begin + max_time, end);

} /* convert_to_space_vector */


inline quanta_t 
max_end(const quanta_t& a1, const miser_qseg_t& a2)
{
	return max(a1, a2.mq_etime);

} /* max_end */


int
verify_allocation(miser_space& s)
{
	if (s.mr_memory > G_maxmemory) {
		return 1;	/* error */
	}

	if (s.mr_cpus > G_maxcpus) {
		return 1;	/* error */
	}

	return 0;	/* success */

} /* verify_allocation */


int 
is_valid(miser_qseg_t &rdp) 
{
	return rdp.mq_stime > duration;

} /* is_valid */

	
void
is_over(miser_qseg_t &rdp)
{
	if (rdp.mq_etime > duration) {
		rdp.mq_etime = duration;
	}

} /* is_over */


void
fix_qdef(queue_definition *spec) 
{
	resources rtemp;

	insert_iterator<resources> rins(rtemp, rtemp.begin());
		
	remove_copy_if(spec->records.begin(), spec->records.end(),
			rins, is_valid);

	for_each(rtemp.begin(), rtemp.end(), is_over);

	for(int i = 0; i < rtemp.size(); i++) {
		spec->records[i] = rtemp[i];
	}

	spec->records.erase(spec->records.begin() + i, spec->records.end());

} /* fix_qdef */


error_t
queue_create(queue_definition* spec, queuelist *newlist)
{
	PRINT_VERBOSE(("queue_create: %s", miser_qstr(spec->queue)));

	if ((system_pool == 0) != (spec->queue == system_id)) {
		return MISER_ERR_QUEUE_ID_BAD;
	}

	if (spec->queue == system_id) {
		STRING_PRINT("[queue_create] system queue");

		duration = accumulate(spec->records.begin(),
				spec->records.end(), (quanta_t)0, max_end);

		if (duration > MAX_DURATION) {
			return MISER_ERR_DURATION_TOO_LONG;

		} else if (duration > (MAX_DURATION/2)) {
			merror("\n\nWARNING: Miser system queue duration is "
				"long - Miser performance may suffer.\n"
				"\t To reduce duration, use lower value for "
				"the END attribute in the\n"
				"\t queue config files.\n\n");
		}
	}

	fix_qdef(spec);
        space allocation(duration * 2);
	space tspace(duration * 2);

        convert_to_space_vector(spec->records.begin(), spec->records.end(),
				 tspace.begin(), tspace.begin() + duration);

	if (find_if(tspace.begin(), tspace.begin() + duration,
			verify_allocation) != (tspace.begin() + duration)) {
		return MISER_ERR_RESOURCES_OUT;
	}

	STRING_PRINT("[queue_create] Resources Verified OK");	

	copy(tspace.begin(), tspace.begin()+duration, tspace.begin()+duration);

	if (spec->queue == system_id) {
		system_pool = new miser_queue(spec);
		return MISER_ERR_OK;
	}

	space::iterator i;

	convert_to_localtime(allocation.begin(), allocation.begin() + duration,
				tspace.begin(), tspace.begin() + duration);

	convert_to_localtime(allocation.begin() + duration, allocation.end(),
				tspace.begin() + duration, tspace.end());

	i = mismatch(system_pool->space_available.begin(),
                   system_pool->space_available.end(),
                   allocation.begin(), dominates()).first;

	if (i != system_pool->space_available.end()) {
		SYMBOL_PRINT(system_pool->space_available.end() - i);
		SYMBOL_PRINT(system_pool->space_available);

		return MISER_ERR_RESOURCES_OUT;
	}

	STRING_PRINT("[queue_create] Found Allocation");

	miser_policy* policy = find_policy_manager(spec->policy);

	if (!policy) {
		return MISER_ERR_POLICY_ID_BAD;
	}

        newlist->push_front(miser_queue());
        (*newlist->begin()).init(spec);

	apply(system_pool->space_available.begin(),
	      system_pool->space_available.end(), allocation.begin(), sub());

        return MISER_ERR_OK;

} /* queue_create */


error_t
queue_query_resources(id_type_t id, miser_queue_resources_t *mqr)
{
        miser_resources_t *end = mqr->mqr_buffer + mqr->mqr_count;
        miser_resources_t *begin = mqr->mqr_buffer;

        quanta_t start = mqr->mqr_start;

	STRING_PRINT("[queue_query_resources]");

        if ((void*)end < start_request || (void*)end > end_request) {
                return MISER_ERR_EFAULT;
	}

        miser_queue* queue = find_queue(id, R_OK);

        if (!queue) { 
		return G_error;
	}

        queue_update(queue);

        if (start < queue->last_accessed_time) {
                start = queue->last_accessed_time;
	}

        if (end-begin < duration) {
                mqr->mqr_count = duration;
	}

        end = copy(qtti(*queue, start), 
		qtti(*queue, queue->last_accessed_time + duration), begin, end);

        mqr->mqr_start = start;

        return MISER_ERR_OK;

} /* queue_query_resources */


error_t
queue_query_resources1(id_type_t id, miser_queue_resources1_t *mqr)
{
	miser_resources_t *end = mqr->mqr_buffer + mqr->mqr_count;
	miser_resources_t *begin = mqr->mqr_buffer;

	quanta_t start = mqr->mqr_start;

	STRING_PRINT("[queue_query_resources1]");

	if ((void*)end < start_request || (void*)end > end_request) {
		return MISER_ERR_EFAULT;
	}

	miser_queue* queue = find_queue(id, R_OK);

	if (!queue) {
		return G_error;
	}

	queue_update(queue);

	if (start < queue->last_accessed_time) {
		start = queue->last_accessed_time;
	}

	if (end-begin < duration) {
		mqr->mqr_count = duration;
	}

	end = copy(qtti(*queue, start),
		qtti(*queue, queue->last_accessed_time + duration), 
		begin, end);

	mqr->mqr_start = start;

	return MISER_ERR_OK;

} /* queue_query_resources1 */


error_t
queue_query_names(id_type_t *begin, id_type_t *& end)
{
	STRING_PRINT("[queue_query_names]");

	end = copy(active_queues->begin(), active_queues->end(), begin, end);

	return MISER_ERR_OK;

} /* queue_query_names */
	

struct job_query_filter 
{
	id_type_t queue;
	bid_t bid;

	job_query_filter(id_type_t Queue, bid_t Bid)
		: queue(Queue), bid(Bid) {}

	bool operator()(const miser_job_schedule &job) {
		return !(job.queue->definition->queue == queue
			 && job.batch_id > bid);
	}

}; /* job_query_filter */


bid_t*
queue_query_jobs(id_type_t queue, bid_t bid, bid_t* begin, bid_t* end)
{ 
	vector<bid_t> bids;

	remove_copy_if(job_schedule_queue.begin(), job_schedule_queue.end(),
			back_inserter(bids), job_query_filter(queue, bid));

	return partial_sort_copy(bids.begin(), bids.end(), begin, end);

} /* queue_query_jobs */


static miser_queue* miser_queue_save;

void
restore(miser_queue*& target)
{
	STRING_PRINT("[restore]");

	delete target;
	target = miser_queue_save;

} /* restore */


void
save(miser_queue* source)
{
	miser_queue_save = source;

} /* save */


error_t
queue_rebuild_from_queues(queuelist* new_queues)
{
	queuelist* queuesave = active_queues;
	active_queues = new_queues;

	if (find_if(job_schedule_queue.begin(), job_schedule_queue.end(),
			job_reschedule) != job_schedule_queue.end()) {
		restore(system_pool);
		delete new_queues;
		active_queues = queuesave;
		return MISER_ERR_RESOURCES_INSUFF;
	}

	for_each(job_schedule_queue.begin(), job_schedule_queue.end(),
		 job_retarget);

	delete queuesave;

        return MISER_ERR_OK;

} /* queue_rebuild_from_queues */


static error_t    error;
static queuelist* qdef_nqueues;

void *
_qdef_alloc(miser_qhdr_t *qhdr)
{
	queue_definition* spec;

	if (time_quantum == 0) {
		time_quantum = qhdr->quantum;

		if (time_quantum <= 0) {
			merror("\nmiser: invalid time quantum.\n");
			exit(1);	/* error */
		}

		if (sysmp(MP_MISER_SETRESOURCE, MPTS_MISER_QUANTUM,
				&time_quantum)) {
			merror("\nmiser: failed to set quantum, %s.\n", 
				strerror(errno));
			exit(1);	/* error */
		}
	}

	if (qhdr->quantum != time_quantum) {
		merror("\nmiser: invalid time quantum.\n");
		return 0;	/* error */
	}

	spec = new queue_definition(qhdr->name, qhdr->policy, qhdr->nseg);

	if (spec) {
		spec->file = qhdr->file;
	}

	return spec;

} /* _qdef_alloc */


int
_qdef_callb(void *qdef)
{
	queue_definition* qdef_spec = (queue_definition *) qdef;
	error_t	error;

	error = queue_create(qdef_spec, qdef_nqueues);

	if (error != MISER_ERR_OK) {
		merror("\nmiser: failed to create queue \"%s\": %s.\n", 
			(char *) &qdef_spec->queue, miser_error(error));
		exit(1);	/* error */
	}

	return 1;	/* success */

} /* _qdef_callb */


miser_qseg_t *
_qdef_elems(void *qdef, uint64_t i)
{
	queue_definition* qdef_spec = (queue_definition *) qdef;

	return &qdef_spec->records[i];

} /* _qdef_elems */


miser_queue *
find_queue(id_type_t id) 
{
	if (id == system_id) {
		return 0;	/* errror */
	}

	queuelist::iterator i = find(active_queues->begin(), 
					active_queues->end(), id);

	if (i == active_queues->end()) {
		return 0;	/* error */
	}

	return &*i;

} /* find_queue */
	

void 
schedule_job(miser_job_t* mj)
{
	int i;
	miser_queue *q = find_queue(mj->mj_queue);

	STRING_PRINT("[schedule_job]");

	for (i = 0; i < mj->mj_count; i++) {
		if (queue_find_block(q,get_start_time(mj->mj_segments[i]),
				mj->mj_segments[i].ms_etime,
				mj->mj_segments[i]) == -1) {
                        merror("\nmiser: could not schedule job %d.\n", 
				mj->mj_bid);
			exit(1);	/* error */
                }

                queue_allocate_blocks(q, mj->mj_segments,
                                        mj->mj_segments + mj->mj_count);
        }

} /* schedule_job */


void
reschedule_job(bid_t& bid)
{
	miser_request_t	mgr;
        miser_job_t	*mj;

	STRING_PRINT("[reschedule_job]");

        mj = (miser_job_t *) &mgr.mr_req_buffer.md_data;

	mj->mj_bid   = bid;
	mj->mj_queue = -1;

	if (sysmp(MP_MISER_GETRESOURCE, MPTS_MISER_JOB, &mgr) == -1) {
		merror("\nmiser: could not get job %d: %s\n", 
			bid, strerror(errno));
                exit(1);	/* error */
        }

        mj = (miser_job_t *) &mgr.mr_req_buffer.md_data;

	STRING_PRINT("[reschedule_job] Rescheduling a job");
	SYMBOL_PRINT(bid);
	SYMBOL_PRINT(mj->mj_count);

	if (mj->mj_count < 0) {
		return;
	}

        quanta_t total_time = 0;

        for (int i = 0; i < mj->mj_count; i++) {
                total_time += mj->mj_segments[i].ms_etime;
	}

	SYMBOL_PRINT(mj->mj_count);

	if (mj->mj_etime < now * time_quantum) {
		return;
	}

	for_each(mj->mj_segments, mj->mj_segments + mj->mj_count,
                        unfix_ctime(mj->mj_etime-total_time));

        miser_queue *q = find_queue(mj->mj_queue);

        if (!q) {
                merror("\nmiser: could not reschedule job %d "
			"because queue %s could not be found. Exiting.\n", 
			bid, (char*) &mj->mj_queue);
                exit(1);	/* error */
        }

        job_schedule_queue.push_front(miser_job_schedule());
        miser_job_schedule& j = *job_schedule_queue.begin();

        j.init(bid, mj->mj_segments, mj->mj_segments + mj->mj_count);
        j.queue = q;

	schedule_job(mj);

} /* reschedule_job */


error_t 
repack_reschedule_job(jobschedule::iterator& job)
{
	miser_request_t mgr;
        miser_job_t*    mj;

	error_t ret_val = MISER_ERR_OK;
	vector<miser_seg_t> backup_schedule;

	STRING_PRINT("[repack_reschedule_job]");

        mj = (miser_job_t *) &mgr.mr_req_buffer.md_data;
	mj->mj_bid = bid_t(*job);
	mj->mj_queue = job->queue->definition->queue;
	mj->mj_etime = job->schedule[0].ms_etime;
	mj->mj_count = 1;

	copy(job->schedule.begin(), job->schedule.end(), mj->mj_segments,
		mj->mj_segments + mj->mj_count);

	copy(job->schedule.begin(), job->schedule.end(),
		back_inserter(backup_schedule));

	/* Now erase the job freeing up its resources */
	job_schedule_queue.erase(job);	
	SYMBOL_PRINT(mj->mj_bid);

	assert(mj->mj_etime > now);

	miser_queue *q = find_queue(mj->mj_queue);
	assert(q != NULL);	

	job_schedule_queue.push_front(miser_job_schedule());
        miser_job_schedule& j = *job_schedule_queue.begin();

        j.init(mj->mj_bid, mj->mj_segments, mj->mj_segments + mj->mj_count);
	j.queue = q;
	
	assert(mj->mj_segments[0].ms_resources.mr_cpus > 0);

	ret_val = q->policy->do_policy(q, mj->mj_bid, mj->mj_segments, 
			mj->mj_segments + mj->mj_count, &(mj->mj_etime));

	/* Must have been an error */
	assert(ret_val == MISER_ERR_OK);

	if (ret_val != MISER_ERR_OK) {

		copy(backup_schedule.begin(),backup_schedule.end(),
			mj->mj_segments, mj->mj_segments + mj->mj_count);

		schedule_job(mj);
		return MISER_ERROR;
	}

	assert(mj->mj_segments[0].ms_resources.mr_cpus > 0);

	copy(mj->mj_segments, mj->mj_segments + mj->mj_count,
		j.schedule.begin(), j.schedule.end());

	mj->mj_etime *= time_quantum;

	for_each(mj->mj_segments, mj->mj_segments + mj->mj_count, fix_ctime());

	SYMBOL_PRINT(mj->mj_segments[0]);

	if (sysmp(MP_MISER_SETRESOURCE, MPTS_MISER_JOB, &mgr) != 0) {

		copy(backup_schedule.begin(),backup_schedule.end(),
			mj->mj_segments, mj->mj_segments + mj->mj_count);

		mj->mj_etime /= time_quantum;
		schedule_job(mj);

		ret_val = MISER_ERROR;
	}

	return ret_val;	

} /* repack_reschedule_job */


error_t 
miser_rebuild()
{
	miser_request_t mgr;
	miser_bids_t*   mb;

	STRING_PRINT("[miser_rebuild]");

	memset(&mgr, 0, sizeof(mgr));

	if (sysmp(MP_MISER_GETRESOURCE, MPTS_MISER_BIDS, &mgr) == -1) {
		merror ("\nmiser: getting bids failed.\n");
		exit(1);	/* error */
	}

	mb = (miser_bids_t*) &mgr.mr_req_buffer.md_data;

	vector<bid_t> bids(mb->mb_count);

	copy (mb->mb_buffer, mb->mb_buffer + mb->mb_count, bids.begin(), 
		bids.end());

	SYMBOL_PRINT(mb->mb_count);
	
	for_each(bids.begin(), bids.end(), reschedule_job);

	return MISER_ERR_OK;

} /* miser_rebuild */
	

error_t
miser_reconfig(char* fname, queuelist*& new_queues)
{
	extern void*		(*qdef_alloc)(miser_qhdr_t *);
	extern int		(*qdef_callb)(void *);
	extern miser_qseg_t*	(*qdef_elems)(void *, uint64_t);

	STRING_PRINT("[miser_reconfig]");

	qdef_alloc = &_qdef_alloc;
	qdef_callb = &_qdef_callb;
	qdef_elems = &_qdef_elems;

	error = MISER_ERR_OK;

	if (system_pool 
		&& !miser_checkaccess(system_pool->definition->file, W_OK)) {
		return MISER_ERR_PERM;
	}
		
        new_queues   = new queuelist;
	qdef_nqueues = new_queues;

	save(system_pool);
	system_pool  = 0;

	if (!parse(PARSE_QDEF, fname)) {
		error = MISER_ERR_INVALID_FILE;
	}

	if (error != MISER_ERR_OK) {
		SYMBOL_PRINT(error);
		STRING_PRINT("[miser_reset] Reset failed");

		miser_error(error);
		restore(system_pool);
		delete new_queues;

		return error;
	}

	STRING_PRINT("Reset succeeded");

        return error;

} /* miser_reconfig */
