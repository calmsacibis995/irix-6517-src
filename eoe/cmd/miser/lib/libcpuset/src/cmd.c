/*
 * FILE: eoe/cmd/miser/lib/libcpuset/src/cmd.c
 *
 * DESCRIPTION:
 * 	This file implements the cpuset user commands to perform create, 
 *	destroy, list, query, attach process, move processes, etc. on 
 *	cpusets.
 */   

/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/


#include <sys/sysmp.h>
#include <errno.h>
#include <stdio.h>
#include "libcpuset.h"


#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))


miser_data_t		req;	
miser_queue_cpuset_t*	cs = (miser_queue_cpuset_t*) req.md_data;


id_type_t
cpuset_qid(const char *str)
/*
 * Convert queue name string to numeric queue id value.
 */
{
        id_type_t val = 0;
        int       size = 0;
        char*     tmp = (char *) &val;

        size = MIN(strlen(str), sizeof(id_type_t));

        while (size--) {
                *(tmp++) = tolower(*(str++));
	}

        return val;

} /* cpuset_qid */


int 
cpuset_create (char* qname, char* fname)
/*
 * Creates a cpuset with the configuration file specified by the -f 
 * parameter and the name specified by the -q parameter. If the cpuset
 * name already exists, a CPU specified in the cpuset configuration file 
 * is already a member of a cpuset, or the user does not have the
 * requisite permissions, the operation fails.
 */

{
	FILE*	fp;		/* Hold config file pointer */
	char	buffer[1024];	/* Hold a line in the config file */
	char*	bufptr;		/* Point to the line buffer */
	int	count      = 0;	/* Index counter for mqc_cpuid */
	int	line       = 0;	/* Line number in config file */
	int	cpu0_exist = 0; /* CPU0 flag - initialized to false */

	/* Get total configured cpus in the system */
	int maxcpus = sysmp(MP_NPROCS);

	/* Convert qname string to qid */
	cs->mqc_queue = cpuset_qid(qname);

	/* Initialize mqc_flags flag to 0 */
	cs->mqc_flags = 0;
        
	/* Open cpuset config file readonly */
	if (!(fp = fopen(fname, "r"))) {
		fprintf(stderr, "\ncpuset: Failed to open file %s: %s.\n\n",
			fname, strerror(errno)); 
		return 1;	/* error */
        }

	/* Parse config file and load values to request buffer */
	while(fgets(buffer, sizeof(buffer), fp) != NULL) {

		int	value;		/* Hold CPU numbers in config file */
		char	key[1024];	/* String buffer to hold token */

		bufptr = &buffer[0];	/* Point to begining of the buffer */
		line++;			/* Increment config file line number */

		if (sscanf(buffer, " %s ", key) == 1) {

			if (key[0] == '#') {
				continue;	/* Ignore comment line */	

			/* Check and set flag for EXCLUSIVE cpuset */ 
			} else if (!strcmp(key, "EXCLUSIVE")) { 
				cs->mqc_flags |= MISER_CPUSET_CPU_EXCLUSIVE;

			} else if (!strcmp(key, "MEMORY_LOCAL")) {
				cs->mqc_flags |= MISER_CPUSET_MEMORY_LOCAL;

			} else if (!strcmp(key, "MEMORY_EXCLUSIVE")) {
				cs->mqc_flags |= MISER_CPUSET_MEMORY_EXCLUSIVE;

                        } else if (!strcmp(key, "MEMORY_KERNEL_AVOID")) {
                                cs->mqc_flags |= MISER_CPUSET_MEMORY_KERNEL_AVOID;

			} else if (!strcmp(key, "CPU")) {

				/* Check and set value for a CPU entry */
				bufptr += strlen("CPU");

				/* No CPU ID specified */
				if (sscanf(bufptr, " %d ", &value) != 1) {
					fprintf(stderr, "\ncpuset: File %s, "
						"Line[%d]: No CPU ID.\n\n",
						fname, line);
					return 1;	/* error */
				}

				/* CPU ID specified does not exist */
				if (value >= maxcpus) {
					fprintf(stderr, "\ncpuset: File %s, "
						"Line[%d]: CPU ID > "
						"maxcpus.\n\n", fname, line);
					return 1;	/* error */
				}

				/* CPU 0 is specified - set flag */
				if (value == 0) {
					cpu0_exist = 1;
				}

				/* Assign CPU ID in the array */ 
				cs->mqc_cpuid[count] = value;

				count++;

			/* Invalid token */
			} else { 
				fprintf(stderr, "\ncpuset: File %s, "
					"Line[%d]: '%s' invalid token.\n\n", 
					fname, line, key);
				return 1;	/* error */
			}	

		} /* if */

	} /* while */

	/* CPU 0 is requested to be part of an EXCLUSIVE cpuset - exit */
	if (cs->mqc_flags & MISER_CPUSET_CPU_EXCLUSIVE && cpu0_exist) {
		fprintf(stderr, "\ncpuset: File %s, Line[%d]: CPU 0 cannot "
			"be part of an EXCLUSIVE cpuset.\n\n", fname, line);
		return 1;	/* error */
	}

	/* Assign number of CPUs to the request buffer */
	cs->mqc_count = count;

	/* Set cpuset create flag to the request */ 
        req.md_request_type = MISER_CPUSET_CREATE;

	/* Send miser cpuset create request to the kernel */
        if (sysmp(MP_MISER_SENDREQUEST, &req, fileno(fp)) == -1) {
		fprintf(stderr, "\ncpuset: Failed to create cpuset %s: %s.\n\n",
			qname, strerror(errno));
		return 1;	/* error */
        }

	return 0;	/* success */

} /* cpuset_create */


int 
cpuset_attach(char* qname, char* cmdname, char** cmdargs)
/*
 * Runs the command on the cpuset identified by the -q parameter. If the 
 * user does not have access permissions or the cpuset does not exist, an 
 * error is returned.
 */
{
	/* Convert qname string to qid */
	cs->mqc_queue = cpuset_qid(qname);

	/* Set cpuset attach flag to the request */
	req.md_request_type = MISER_CPUSET_ATTACH;

	/* Send miser cpuset attach request to the kernel */
	if (sysmp(MP_MISER_SENDREQUEST, &req) == -1) {
		fprintf(stderr, "\ncpuset: Failed to attach to cpuset "
			"[%s]: %s.\n\n", qname, strerror(errno));
		return 1;	/* error */
	}

	/* Attach successful - execute the command */
	execvp(cmdname, cmdargs);

	/* Command execution failed - print error and exit */
	fprintf(stderr, "\ncpuset: Unable to exec program [%s]: %s.\n\n", 
		cmdname, strerror(errno));

	return 1;	/* error */

} /* cpuset_attach */


int
cpuset_destroy(char* qname)
/* 
 * Destroys the specified cpuset. A cpuset can only be destroyed if there 
 * are no processes currently attached to it.
 */
{
	/* Convert qname string to qid */
	cs->mqc_queue = cpuset_qid(qname);

	/* Set cpuset queue destroy flag to the request */
	req.md_request_type = MISER_CPUSET_DESTROY;

	/* Send miser cpuset queue destroy request to the kernel */
	if (sysmp(MP_MISER_SENDREQUEST, &req) == -1) {
		fprintf(stderr, "\ncpuset: Failed to destroy cpuset "
			"[%s]: %s.\n\n", qname, strerror(errno));
		return 1;	/* error */
	}

	return 0;	/* success */

} /* cpuset_destroy */


int 
cpuset_list_procs(char* qname)
/*
 * Query and print list of attached processes to a cpuset queue.
 */
{
	pid_t *first, *last;	/* Pointer to a process id */

	/* Map miser_cpuset_pid structure onto the miser_data buffer */
	miser_cpuset_pid_t *cpuset_pid = (miser_cpuset_pid_t *) req.md_data;

	/* Point to the last and first pid position in the miser_data */
	last	= (pid_t *)(&req+1);
	first	= (pid_t *) cpuset_pid->mcp_pids;
	last	= first + (last - first);

	/* Initialize request buffer */
	cpuset_pid->mcp_queue = cpuset_qid(qname);
	cpuset_pid->mcp_count = 0;
	cpuset_pid->mcp_max_count = last - cpuset_pid->mcp_pids;

	/* Set cpuset list process flag to the request */
	req.md_request_type = MISER_CPUSET_LIST_PROCS;

	/* Send miser cpuset list process request to the kernel */
	if (sysmp(MP_MISER_SENDREQUEST, &req) == -1) {
		fprintf(stderr, "\ncpuset: Failed to list procs in "
			"cpuset [%s]: %s.\n\n", qname, strerror(errno));
		return 1;	/* error */
	}

	/* Point to the last pid in the result buffer */
	last = first + cpuset_pid->mcp_count;

	printf("\nAttached Processes to CPUSET Queue [%s]:\n", qname);
	printf("-----------------------------------------\n\n");

	/* Print pid per line */
	while(first != last) {
		printf("   %d\n", *first++);
	}

	printf("\n");

	return 0;	/* success */

} /* cpuset_list_procs */


int 
cpuset_move_procs(char* qname)
/*
 * Moves all the attached processes out of the cpuset queue.
 */
{
	/* Convert qname string to qid */
	cs->mqc_queue = cpuset_qid(qname);

	/* Set cpuset queue move process flag to the request */
	req.md_request_type = MISER_CPUSET_MOVE_PROCS;

	/* Send miser cpuset queue move process request to the kernel */
	if (sysmp(MP_MISER_SENDREQUEST, &req) == -1)  {
		fprintf(stderr, "\ncpuset: Failed to move procs in "
			"cpuset [%s]: %s.\n\n", qname, strerror(errno));
		return 1;	/* error */
	}

	return 0;	/* sucess */

} /* cpuset_move_procs */


int 
cpuset_query_current()
/*
 * Query and prints the cpuset queue name to which the process is 
 * currently attached.
 */
{
	/* Get total configured cpus in the system */
	int maxcpus = sysmp(MP_NPROCS);

	/* Map miser_queue_names structure onto the miser_data buffer */
	miser_queue_names_t *names = (miser_queue_names_t*) req.md_data;

	/* Set process attached cpuset query flag to the request */
        req.md_request_type = MISER_CPUSET_QUERY_CURRENT;

	/* Send process attached cpuset query request to the kernel */
        if (sysmp(MP_MISER_SENDREQUEST, &req) == -1) {
                fprintf(stderr, "\ncpuset: Failed to query current "
			"cpuset: %s.\n\n", strerror(errno));
                return 1;	/* error */
        }
	
	/* Process belongs to a cpu */
        if (names->mqn_queues[0] <= maxcpus ) {
                printf("\nCurrent Process MUSTRUN to cpu: [%d]\n\n", 
			strtol((char*) &cs->mqc_queue, 0, 0));

	/* Process belongs to a CPUSET */
	} else {
		char buf[10];

		memset(buf, 0, 10);
		strncpy(buf, (char *)&names->mqn_queues[0], sizeof(id_type_t));

                printf("\nCurrent Process Attached to CPUSET: [%s]\n\n", buf);
	}

        return 0;	/* success */

} /* cpuset_query_current */


int 
cpuset_query_names() 
/*
 * Query and lists the names of all the cpusets currently defined.	
 */
{
	int  i;	
	char buf[10];	/* Hold cpuset queue name */

	/* Get total configured cpus in the system */
	int maxcpus = sysmp(MP_NPROCS);

	/* Map miser_queue_names structure onto the miser_data buffer */
	miser_queue_names_t *names = (miser_queue_names_t*) req.md_data;

	/* Set cpuset queue names query flag to the request */
	req.md_request_type = MISER_CPUSET_QUERY_NAMES;


	/* Send cpuset queue names query request to the kernel */
	if (sysmp(MP_MISER_SENDREQUEST, &req)) {
		fprintf(stderr, "\ncpuset: Failed to query cpuset names: "
			"%s.\n\n", strerror(errno));
		return 1;	/* error */
	}

	if (names->mqn_count > 0) {
		printf("\nMiser CPUSET Queues:\n");
	} else {
		printf("\nNo Miser CPUSET Queues defined.\n");
	}

	/* Print list of cpusets configured */
	for (i = 0; i < names->mqn_count; i++) {

		if (names->mqn_queues[i] <= maxcpus)  {
			printf("   CPU %d is restricted.\n", 
			       (int)names->mqn_queues[i]);
			continue;
		}	

		/* Initialize buffer, copy string and print */
		memset(buf, 0, 10);
		strncpy(buf, (char *)&names->mqn_queues[i], sizeof(id_type_t));
                printf("   QUEUE[%s]\n", buf);
	}

	printf("\n");

	return 0; 	/* success */

} /* cpuset_query_names */


int 
cpuset_query_cpus(char *qname) 
/*
 * Query and lists all the cpus that belong to the specified cpuset.
 */
{
	int i;

	/* Convert qname string to qid */
	cs->mqc_queue = cpuset_qid(qname);

	/* Set cpuset query cpus flag to the request */
	req.md_request_type = MISER_CPUSET_QUERY_CPUS;

	/* Send cpuset query cpus request to the kernel */
	if (sysmp(MP_MISER_SENDREQUEST, &req)) {
		fprintf(stderr, "\ncpuset: Could not query cpus in "
			"cpuset %s: %s.\n\n", qname, strerror(errno));
		return 1;	/* error */
	}

	printf("\nMiser CPUSET Queue[%s] contains %d CPUs:\n",
		qname, cs->mqc_count);

	/* Print list of CPUs belongs to the cpuset */
	for (i = 0; i < cs->mqc_count ; i++) {
		printf ("   CPU[%d]\n", cs->mqc_cpuid[i]);
	}

	printf("\n");

	return 0;	/* success */

} /* cpuset_query_cpus */
