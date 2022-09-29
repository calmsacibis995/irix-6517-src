/*
 * FILE: eoe/cmd/miser/lib/libmiser/src/print.c
 *
 * DESCRIPTION:
 *      Library functions that print miser output.
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


#include <fcntl.h>
#include <pwd.h>
#include "libmiser.h"


void
miser_print_job_desc(bid_t jid)
/*
 * Prints miser job description (owner, command) for the suplied jobid.
 */
{
	prpsinfo_t      pinfo;	/* Process info structure */
	struct passwd   *pwptr;	/* Password info structure */

	/* Get process information from prpsinfo structure */
	if (!miser_get_prpsinfo(jid, &pinfo)) {

		/* Lookup user information from password file using userid */
		pwptr = getpwuid(pinfo.pr_uid);

		if (pwptr != NULL) {
			/* Print username and process command name */
                	fprintf(stdout, "%-8s %-15s", 
				pwptr->pw_name, pinfo.pr_fname);
		}
        }

} /* miser_print_job_desc */


void
miser_print_job_status(miser_seg_t *seg)
/*
 * Prints miser job status for the job.
 */
{
	char	buf1[20];	/* Hold start time string */
	char	buf2[20];	/* Hold start time string */
	time_t	stime = 0;	/* Initialize start time */

	/* Compute job start time based on end time and duration */
	stime = seg->ms_etime - (seg->ms_rtime / seg->ms_resources.mr_cpus);

	/* Print start and end time, num cpus, and memory */
        fprintf(stdout, "%3d %5.0b %17s %17s",
		seg->ms_resources.mr_cpus,
		(float) seg->ms_resources.mr_memory,
		fmt_time(buf1, 20, stime),
		fmt_time(buf2, 20, seg->ms_etime));

} /* miser_print_job_status */


void
miser_print_job_sched(miser_seg_t *seg)
/*
 * Prints the miser job schedule.
 */
{
        char   buf1[30];	/* Hold start time string */
        char   buf2[30];	/* Hold end time string */
        time_t stime = 0;	/* Initialize start time */

        /* Compute job start time based on end time and duration */
        stime = seg->ms_etime - (seg->ms_rtime / seg->ms_resources.mr_cpus);

	fprintf(stdout, "%3d %5.0b %4d:%.2d.%.2d %17s %17s %3d %3d %1s%1s\n",
		seg->ms_resources.mr_cpus,
		(float) seg->ms_resources.mr_memory,
		seg->ms_rtime / 3600,
		(seg->ms_rtime % 3600) / 60,
		(seg->ms_rtime % 3600) % 60,
		fmt_time(buf1, 30, stime),
		fmt_time(buf2, 30, seg->ms_etime),
		seg->ms_multiple, 
		seg->ms_priority,
		(seg->ms_flags & MISER_SEG_DESCR_FLAG_STATIC ? "S" : ""),
		(seg->ms_flags & MISER_EXCEPTION_TERMINATE ? "K" :
			seg->ms_flags & MISER_EXCEPTION_WEIGHTLESS ? "W" : ""));

} /* miser_print_job_sched */


void
print_move(FILE *fo, miser_data_t *req)
/*
 * Prints confirmation of miser_move command.  
 */
{
	miser_move_t *move = (miser_move_t *) req->md_data;
	miser_qseg_t *qseg;

	int  i;
	char buf1[30], buf2[30];
	
	fprintf(fo, "from: %s\n", miser_qstr(move->mm_from));
	fprintf(fo, "to:   %s\n", miser_qstr(move->mm_to));
	fprintf(fo, "%13s %19s %13s %5s\n", "start", "end", "cpus", "mem");
	fprintf(fo, "    %s      %s %6s %5s\n", "---------------",
		"---------------", "----", "---");

	for (i = 0; i < move->mm_count; i++) {
		qseg = &move->mm_qsegs[i];

		fprintf(fo, "%20s %20s %5d %5.0b\n", 
			fmt_time(buf1, 30, qseg->mq_stime),
			fmt_time(buf2, 30, qseg->mq_etime), 
			qseg->mq_resources.mr_cpus,
			(float)qseg->mq_resources.mr_memory);
	} /* for */

} /* miser_print_move */


miser_resources_t*
strip_empty(miser_resources_t *first, miser_resources_t *last)
/*
 * Initialize miser_resources_t buffer and return pointer to the last
 * segment.
 */
{
        miser_resources_t *current = first;

        while (current != last)  {
                if (current->mr_cpus == 0 && current->mr_memory == 0) {
                        current++;
                        continue;
                }

                return current;
        }

        return last;

} /* strip_empty */


void
miser_print_block(int count, miser_resources_t *first, 
	miser_resources_t *last, int start)
/*
 * Prints time segments and available resource during that period for
 * a specified miser queue.
 */
{
        miser_resources_t *current;
        struct tm         *tmptr;

        time_t ctime	= 0;
	int    i	= 0;

        char   buf[1024];

        last    = first + count;
        current = strip_empty(first, last);

        if (last == current) {
                current = first;
	}

        i     = current - first;
        first = current;

        while (first != last) {

                ctime = (start + i++) * miser_quantum();
                tmptr = localtime(&ctime);

                strftime(buf, 1024, "%m/%d/%Y %T", tmptr);

		if (i == 1) {	/* very first pass */
			fprintf(stdout, "| %s | %5d | %7.0b  |", buf,
				first->mr_cpus, (float) first->mr_memory);
		} else {
			if (current->mr_cpus == first->mr_cpus &&
				current->mr_memory == first->mr_memory) {

				current = first;
				first++;

				if (first == last) {
					fprintf(stdout, " %s |\n", buf);
				}

				continue;
			}

			if (first->mr_cpus != 0 && first->mr_memory != 0) {
				fprintf(stdout, " %s |\n| %s | %5d | %7.0b  |",
					buf, buf, first->mr_cpus, 
					(float) first->mr_memory);
			}
		}

                current = first;
                first++;

        } /* while */

} /* miser_print_block */
