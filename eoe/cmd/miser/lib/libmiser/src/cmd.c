/*
 * FILE: eoe/cmd/miser/lib/libmiser/src/cmd.c
 *
 * DESCRIPTION:
 *	Library functions that implement miser commands.
 */

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1997 Silicon Graphics, Inc.		  *
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
#include <fcntl.h>
#include <paths.h>
#include "libmiser.h"


time_t       time_quantum;
miser_data_t miser_data;


/* Structure to hold miser queue id list */
typedef struct mqlist {
        uint16_t	mq_count;
        id_type_t	*mq_qid;
} mqlist_t;


/* Structure to hold miser job id list */
typedef struct mjlist {
        uint16_t	mj_count;
        bid_t		*mj_jid;
} mjlist_t;


miser_queue_names_t *
miser_get_qnames(void)
/*
 * Returns a pointer to miser_queue_names_t containing a list of
 * configured qids and a count.  Returns NULL in case of error.
 */
{
	id_type_t	*first, *last;
	miser_queue_names_t *mqn = (miser_queue_names_t*)miser_data.md_data;

	/* Point to qname memory location in the structure */
	last  = (id_type_t *) (&miser_data + 1);
	first = (id_type_t *) mqn->mqn_queues;
	last  = first + (last - first);

	/* Build the request */
	mqn->mqn_count = last - mqn->mqn_queues;

	/* Set request type flag for the sysmp call */
	miser_data.md_request_type = MISER_ADMIN_QUEUE_QUERY_NAMES;

	/* Make the sysmp call to get data requested */
	if (sysmp(MP_MISER_SENDREQUEST, &miser_data)) {
		fprintf(stderr, "\nmiser: qinfo failed (%s).\n\n", 
			strerror(errno));
		return NULL;
	}

	/* Print miser error, if set, and return NULL */
	if (miser_data.md_error) {
		fprintf(stderr, "\nmiser: qinfo failed (%s).\n\n", 
		       miser_error(miser_data.md_error));
		return NULL;
	}

	/* Return a pointer to the structure with data requested */
	return mqn;

} /* miser_get_qnames */


miser_bids_t * 
miser_get_jids(id_type_t qid, int start)
/*
 * Returns a pointer to miser_bids_t containing a list of
 * active bids and a count for the specified qid.  Returns NULL
 * in case of error.
 */
{
	bid_t 		*first, *last;
        miser_bids_t	*jq = (miser_bids_t *)miser_data.md_data;

	/* Point to qname memory location in the structure */
	last  = (bid_t *)(&miser_data + 1);
	first = (bid_t *)jq->mb_buffer;
	last  = first + (last - first);

	/* Build the request */
	jq->mb_first = start;
	jq->mb_queue = qid;
	jq->mb_count = last - jq->mb_buffer;

	/* Set request type flag for the sysmp call */
        miser_data.md_request_type = MISER_ADMIN_QUEUE_QUERY_JOB;

	/* Make the sysmp call to get data requested */
	if (sysmp(MP_MISER_SENDREQUEST, &miser_data)) {
		fprintf(stderr, "\nmiser: qinfo failed (%s).\n\n", 
			strerror(errno));
		return NULL;
	}

	/* Print miser error, if set, and return NULL */
	if (miser_data.md_error) {
		fprintf(stderr, "\nmiser: qinfo failed (%s).\n\n", 
			miser_error(miser_data.md_error));
		return NULL;
	}

	/* Return a pointer to the structure with data requested */
	return jq;

} /* miser_get_jids */


miser_schedule_t *
miser_get_jsched(bid_t jid, int start)
/*
 * Returns a pointer to miser_schedule_t containing a schedule for
 * the specified jid.  Returns NULL in case of error.
 */
{
	miser_seg_t *first, *last;
	miser_schedule_t *js = (miser_schedule_t *)miser_data.md_data;

	/* Point to job schedule memory location in the structure */
	last  = (miser_seg_t *)(&miser_data + 1);
	first = (miser_seg_t *)js->ms_buffer;
	last  = first + (last - first);

	/* Build the request */
        js->ms_first = start;
        js->ms_job   = jid;
        js->ms_count = last - js->ms_buffer;

	/* Set request type flag for the sysmp call */
        miser_data.md_request_type = MISER_ADMIN_JOB_QUERY_SCHEDULE;

	/* Make the sysmp call to get data requested */
        if (sysmp(MP_MISER_SENDREQUEST, &miser_data)) {
                fprintf(stderr, "\nmiser: jinfo failed (%s).\n\n", 
			strerror(errno));
                return NULL;
        }

	/* Print miser error, if set, and return NULL */
        if (miser_data.md_error) {

		/* Print all error messages except when jid not found */ 
		if (miser_data.md_error != MISER_ERR_JOB_ID_NOT_FOUND)
			fprintf(stderr, "\nmiser: jinfo failed (%s).\n\n",
				miser_error(miser_data.md_error));

                return NULL;
        }

        return js;

} /* miser_get_jsched */


int
miser_get_prpsinfo(int pid, prpsinfo_t *pinfo)
/*
 * Fills the prpsinfo_t structure with information on the pid requested.
 * Returns 0 in case of error.
 */
{
        int     procfd;
        char    pname[PRCOMSIZ];

        sprintf(pname, "%s%010ld", _PATH_PROCFSPI, pid);

        if ((procfd = open(pname, O_RDONLY)) == -1) {
                return 1;
	}

	if ((ioctl(procfd, PIOCPSINFO, pinfo)) == -1) {
		return 1;
	}

	close(procfd);

        return 0;	/* Success */

} /* miser_get_prpsinfo */


int
miser_submit(char *qname, miser_data_t *req)
/*
 * Submit a job to a miser queue.
 */
{
        miser_job_t *job = (miser_job_t *) req->md_data;

        req->md_request_type = MISER_USER_JOB_SUBMIT;
        job->mj_queue = miser_qid(qname);

        if (setpgid(0, 0) == -1) {

                fprintf(stderr, "\nmiser: submit failed ");

                if (errno == EINVAL) {
                        fprintf(stderr, "(terminal does not support job "
				"control).\n\n");

                } else if (errno == EPERM) {
                        fprintf(stderr, "(process is a session leader).\n\n");

                } else {
                        fprintf(stderr, "(internal miser error).\n\n");
		}

                return 0;	/* error */
        }

        if (sysmp(MP_MISER_SENDREQUEST, req)) {

                fprintf(stderr, "\nmiser: submit failed ");

                if (errno == ESRCH) {
			fprintf(stderr, "(cannot contact miser).\n\n");

                } else if (errno == EFAULT) {
                        fprintf(stderr, "(args could not be processed).\n\n");

                } else if (errno == EBUSY) {
                        fprintf(stderr, "(either process belongs to a "
				"cpuset or is mustrun).\n\n");

                } else if (errno == ENOMEM) {
                        fprintf(stderr, "(not enough memory).\n\n");

                } else {
                        fprintf(stderr, "(%s).\n\n", 
				strerror(errno));
		}

                return 0;	/* error */
        }

        if (req->md_error) {
                fprintf(stderr, "\nmiser: submit failed (%s).\n\n", 
			miser_error(req->md_error));
                return 0;	/* error */
        }

        return 1;	/* success */

} /* miser_submit */


int
miser_move(char *srcq, char *dstq, miser_data_t *req)
/*
 * Move a block of resources from one queue to another.
 */
{
        miser_move_t *move = (miser_move_t *) req->md_data;

        req->md_request_type = MISER_ADMIN_QUEUE_MODIFY_MOVE;

        move->mm_from = miser_qid(srcq);
        move->mm_to   = miser_qid(dstq);

        if (sysmp(MP_MISER_SENDREQUEST, req)) {
                fprintf(stderr, "\nmiser: move failed (%s).\n\n",
			strerror(errno));
                return 1;	/* error */
        }

        if (req->md_error) {
                fprintf(stderr, "\nmiser: move failed (%s).\n\n", 
			miser_error(req->md_error));
                return 1;	/* error */
        }

        return 0;	/* success */

} /* miser_move */


int
miser_kill(int bid)
/*
 * Clear miser resource reservation for a killed miser job.
 */
{
	miser_data.md_request_type = MISER_USER_JOB_KILL;

	memset(miser_data.md_data, 0, MISER_REQUEST_BUFFER_SIZE);
	memcpy(miser_data.md_data, &bid, sizeof(bid));

	/* Must be set before passing it in */
	miser_data.md_error = MISER_ERR_OK;

	if (sysmp(MP_MISER_SENDREQUEST, &miser_data)) {
		fprintf(stderr, "\nmiser: kill failed - cannot contact "
				"miser.\n\n");
		return 1;	/* error */
	}

	if (miser_data.md_error) {
		fprintf(stderr, "\nmiser: kill failed - %s,\n\n",
			miser_error(miser_data.md_error));
		return 1;	/* error */
	}

	return 0;	/* success */

} /* miser_kill */


int
miser_reset(char *fname)
/*
 * The miser_reset command is used to force a running version
 * of miser to use a new configuration file (the format of the
 * configuration file is detailed in miser(4) ). The new
 * configuration will succeed if and only if all currently
 * scheduled jobs can be successfully scheduled against the new
 * configuration. If the attempt at creating a new configuration 
 * fails, then miser(1) retains the old configuration.
 */
{
	miser_data.md_request_type = MISER_ADMIN_QUEUE_MODIFY_RESET;
	memset(miser_data.md_data, 0, MISER_REQUEST_BUFFER_SIZE);
	strcpy(miser_data.md_data, fname);

	if (sysmp(MP_MISER_SENDREQUEST, &miser_data)) {
		fprintf(stderr, "\nmiser: reset failed - cannot not contact "
			"miser (%s).\n\n", strerror(errno));
		return 1;       /* error */
        }

        if (miser_data.md_error) {
                fprintf(stderr, "\nmiser: reset failed (%s).\n\n",
                        miser_error(miser_data.md_error));
                return 1;       /* error */
        } else {
		fprintf(stderr, "\nmiser: reset is successful with config file:"
			"\n\t%s.\n\n", fname);
	}

        return 0;	/* success */

} /* miser_reset */


int
miser_qname()
/*
 * Returns a list of miser queue names currently configured.
 */
{
	int 		i;		/* Loop index */
	char 		*q_name;	/* Pointer to a qname string */
	id_type_t	*qid_ptr;	/* Pointer to queue id */

	miser_queue_names_t *mq;	/* Hold incoming miser qname list */

	/* Get a list of configured qnames */
	mq = miser_get_qnames();

	if ((mq != NULL) && (mq->mqn_count > 0)) {
		fprintf(stdout, "\nMiser Queue(s):\n");
	} else {  
		fprintf(stderr, "\nmiser: qinfo failed (no queue name "
				"returnd).\n\n");
		return 1;	/* error */
	}       

	/* Point to the first qid in the list found */
	qid_ptr = mq->mqn_queues; 

	for (i = 0; i < mq->mqn_count; i++) {

		/* Convert qid to qname, incr qid_ptr to point next */
		q_name = (char *)miser_qstr(*qid_ptr++); 
                        
		fprintf(stdout, "   %s\n", q_name);
	}       

	fprintf(stdout, "\n");

	return 0;	/* success */

} /* miser_qname */


int
miser_qjid(char *q_name, int start)
/*
 * Query and list all miser submitted jobids in the specified queue.
 */
{
	int		i;		/* Loop index */
	bid_t		*jid_ptr;	/* Pointer to job id */ 
	id_type_t	qid;		/* Hold queue id */
	miser_bids_t 	*mj;		/* Hold incoming miser jid list */

	/* Convert qname to qid */
	qid = miser_qid(q_name);

	/* Get a list of submitted jids for the qid specified */
	mj = miser_get_jids(qid, start);

	if ((mj != NULL) && (mj->mb_count > 0)) {
		fprintf(stdout, "\nSubmitted Job(s) in Queue [%s]:\n", q_name);
	} else {
		fprintf(stdout, "\nNo Submitted Job in Queue [%s].\n", q_name);
		return 0;	/* success */
	}

	/* Point to the first jid in the list found */
	jid_ptr = mj->mb_buffer;

	/* Print and incr jid to point to the next */
	for (i = 0; i < mj->mb_count; i++) {
		fprintf(stdout, "   %d\n", *jid_ptr++);
	}

	fprintf(stdout, "\n");

	return 0; 	/* success */

} /* miser_qjid */


int 
miser_qstat(char* queue, int realstart)
/*
 * Returns the free resources associated with qname. A queue, as 
 * described in miser(1), has a resource vector of time and space.  
 * Each element of the vector represents a quantum of time that has 
 * an associated space against which a job can request resources.  
 * The output of this option is a list of all the elements of the 
 * resource vector in tabular form.  At the top of the table is the 
 * name of the queue and the size of the quantum in seconds.  Each 
 * entry of the table consists of a start time, space tuple, and end
 * end time.  
 */
{
        miser_resources_t *buffer, *current;
	int remaining = 0, qsize = 0, nextstart = 0, start = 0, count = 0;
	char timebuf[30];

        miser_queue_resources1_t *qr = 
                (miser_queue_resources1_t *)miser_data.md_data;
        miser_data.md_request_type = MISER_ADMIN_QUEUE_QUERY_RESOURCES1;

	qsize =  (miser_resources_t *)(&miser_data + 1) - 
			(miser_resources_t *)qr->mqr_buffer;

	qr->mqr_queue = miser_qid(queue);
	qr->mqr_start = realstart != 0 ? realstart/time_quantum : 0;
	qr->mqr_count = qsize; 

	do {
		if (sysmp(MP_MISER_SENDREQUEST, &miser_data)) {
			fprintf(stderr, "\nmiser: qinfo failed (%s).\n\n", 
				strerror(errno));
			return 1;	/* error */
		}

		if (miser_data.md_error) {
			fprintf(stderr, "\nmiser: qinfo failed (%s).\n\n",
			       miser_error(miser_data.md_error));
			return 1;	/* error */
		}

		if (qr->mqr_count > qsize)   { 

			if (remaining == 0) { 
				remaining = count = qr->mqr_count;
				start = nextstart = qr->mqr_start;

				qr->mqr_count = qsize;

				buffer = malloc(sizeof(miser_resources_t) * 
					count);

				memset(buffer, 0, count);
				current = &buffer[0];
			}

			if (qsize > remaining) {
				qr->mqr_count = remaining;
				remaining     = 0;	
			} else {
				qr->mqr_count = qsize;
				remaining -= qsize;
				nextstart += qsize;
			}

		} else { /* qr->mqr_count <= qsize */
			count   = qsize;
			buffer  = malloc(sizeof(miser_resources_t) * count);
			current = &buffer[0];
			start   = qr->mqr_start;
		}

		qr->mqr_start = nextstart;
		memcpy(current, qr->mqr_buffer, 
			qsize * sizeof(miser_resources_t));

		current += qr->mqr_count;

	} while (remaining > 0);

	curr_time_str(timebuf);

	fprintf (stdout, "\nQueue: [%s]  Quantum: [%d]  Time: [%s]\n\n", 
		queue, miser_quantum(), timebuf);

	fprintf (stdout, 
"+---------------------+-------+----------+---------------------+\n");  
	fprintf (stdout, 
"|     START TIME      |  CPU  |  MEMORY  |       END TIME      |\n"); 	
	fprintf (stdout, 
"+---------------------+-------+----------+---------------------+\n");

	miser_print_block(count, buffer, current, start);

	fprintf (stdout, 
"+---------------------+-------+----------+---------------------+\n\n");  

	free(buffer);

	return 0;	/* success */

} /* miser_qstat */


int
miser_qstat_all(int start)
/*
 * Returns the free resources associated with all defined miser queues.
 */
{
	int		i;		/* Loop index */
	char		*q_name;		/* Pointer to qname */
	id_type_t	*qid_ptr;	/* Pointer to queue id */
        mqlist_t	mql;		/* Hold local miser qid list */

	int		return_val = 0;	/* 0 - success */

	miser_queue_names_t *mq;	/* Hold incoming miser qname list */

	/* Get a list of configured qnames */
	mq = miser_get_qnames();

	if ((mq == NULL) || (mq->mqn_count <= 0)) {
		fprintf(stderr, "\nmiser: qinfo failed (sysmp error).\n\n");
		return 1;	/* error */
	}

	/* Initialize and allocate local memory for qid list */
	mql.mq_count = mq->mqn_count;
	mql.mq_qid   = malloc(sizeof(id_type_t) * (mql.mq_count));

	if (mql.mq_qid == NULL) {
		fprintf(stderr, "\nmiser: qinfo failed (malloc error).\n\n");
		return 1;       /* error */
	}

	/* Point to the first qid in the list */
	qid_ptr = mql.mq_qid;

	/* Load qids in local memory */
	for (i = 0; i < mql.mq_count; i++) {
		*qid_ptr++ = mq->mqn_queues[i]; 
	}

	/* Point to the first qid in the list again */
	qid_ptr = mql.mq_qid;

	/* Collect job ids for each qid */
	for (i = 0; i < mql.mq_count; i++) {

		/* Convert qid to qname */
		q_name = (char *)miser_qstr(*qid_ptr++);

		if (miser_qstat(q_name, start)) {
			return_val = 1;	/* error */
		}

	} /* for 1 */

	/* Free allocated memory for qid list */
	if (mql.mq_qid != NULL) {
		free(mql.mq_qid);
	}

	return return_val;

} /* miser_qstat_all */


int
miser_qjsched_all(int start)
/*
 * Returns a list of all scheduled jobs in all configured miser queues 
 * ordered by job ID.  The job list produces a brief description of the 
 * job.  See miser_jinfo for a description of the output fields.
 */
{
	int		i, j;		/* Loop index */
	char		*q_name;		/* Pointer to qname */
	bid_t		*jid_ptr;	/* Pointer to job id */
	id_type_t	*qid_ptr;	/* Pointer to queue id */
	miser_seg_t	*seg_ptr;	/* Pointer to seg */
        mqlist_t	mql;		/* Hold local miser qid list */
        mjlist_t	mjl;		/* Hold local miser jid list */

	miser_queue_names_t *mq;	/* Hold incoming miser qname list */
	miser_bids_t	    *mj;	/* Hold incoming miser jid list */
	miser_schedule_t    *ms;	/* Hold incoming miser job sched  */

	/* Get a list of configured qnames */
	mq = miser_get_qnames();

	if ((mq != NULL) && (mq->mqn_count > 0)) {
		fprintf(stdout, "\nSubmitted Job(s) in Miser Queue(s):\n");
		fprintf(stdout, "-----------------------------------\n");
	} else {
		fprintf(stderr, "\nmiser: qinfo failed (sysmp error).\n\n");
		return 1;	/* error */
	}

	/* Initialize and allocate local memory for qid list */
	mql.mq_count = mq->mqn_count;
	mql.mq_qid   = malloc(sizeof(id_type_t) * (mql.mq_count));

	if (mql.mq_qid == NULL) {
		fprintf(stderr, "\nmiser: qinfo failed (malloc error).\n\n");
		return 1;       /* error */
	}

	/* Point to the first qid in the list */
	qid_ptr = mql.mq_qid;

	/* Load qids in local memory */
	for (i = 0; i < mql.mq_count; i++) {
		*qid_ptr++ = mq->mqn_queues[i]; 
	}

	/* Point to the first qid in the list again */
	qid_ptr = mql.mq_qid;

	/* Collect job ids for each qid */
	for (i = 0; i < mql.mq_count; i++) {

		/* Convert qid to qname */
		q_name = (char *)miser_qstr(*qid_ptr);

		fprintf(stdout, "\nQueue [%s]:\n\n", q_name);

		/* 
		 * Get a list of submitted jids for specified qid. 
		 * Incr ptr to point to next qid at next iteration.
		 */
		mj = miser_get_jids(*qid_ptr++, start);

		if ((mj == NULL) || (mj->mb_count == 0)) {
			fprintf(stdout, "   No Submitted Job Found.\n\n");
			continue;
		}

		/* Initialize and allocate local memory for jid list */
		mjl.mj_count = mj->mb_count;
		mjl.mj_jid = malloc(sizeof(bid_t) * mjl.mj_count);

		if (mjl.mj_jid == NULL) {
			fprintf(stderr, "\nmiser: qinfo failed "
				"(malloc error).\n\n");
			return 1;       /* error */
		}

		/* Point to the first jid in the list */
		jid_ptr = mjl.mj_jid;

		/* Load jids in local memory */
		for (j = 0; j < mjl.mj_count; j++) {
			*jid_ptr++ = mj->mb_buffer[j];
		}

		/* Point to the first jid in the list again */
		jid_ptr = mjl.mj_jid;

		/* Collect job schedule for each job id */
		for (j = 0; j < mjl.mj_count; j++) {

			/* 
			 * Get a job sched for specified jid.
			 * Incr ptr to point to next jid. 
			 */
			ms = miser_get_jsched(*jid_ptr++, start);

			if ((ms == NULL) || (ms->ms_count == 0)) {
				continue; /* jid doesn't exist anymore */
			}

			if (j==0) {
				fprintf(stdout, 
" JOBID   CPU  MEM     START TIME         END TIME        USER       COMMAND\n"
"-------- --- ----- ----------------- ----------------- -------- ---------------\n");
			}

			/* Point to the first seg in the list */
			seg_ptr = (miser_seg_t *)ms->ms_buffer;

			fprintf(stdout, "%8d ", ms->ms_job);

			miser_print_job_status(seg_ptr);
			fprintf(stdout, " ");

			miser_print_job_desc(ms->ms_job);
			fprintf(stdout, "\n");

		} /* for 2 */

		/* Free allocated memory for jid list */
		if (mjl.mj_jid != NULL) {
			free(mjl.mj_jid);
		}

	} /* for 1 */

	fprintf(stdout, "\n");

	/* Free allocated memory for qid list */
	if (mql.mq_qid != NULL) {
		free(mql.mq_qid);
	}

	return 0;	/* success */

} /* miser_qjsched_all */
