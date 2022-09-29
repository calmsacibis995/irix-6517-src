/*
** File:  clshm_barrier.c
** ----------------------
** Barriers for cross partition code, also a method to send keys across
** for testing.
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/SN/clshm.h>
#include <ulocks.h>
#include "clshm_barrier.h"
#include "xp_shm_wrap.h"

/* global debug flag */
int debug_flag;


/* statics for clshm_barrier */
static int cl_fds[MAX_PARTITIONS]; /* file descriptors for cl driver */
static int *part_barrier;		/* shared memory for part barrier */
static int part_bar_shmid;		/* shmid for part barrier */
static usptr_t *thread_bar_file;	/* file for thread barrier */
static barrier_t *thread_bar;		/* actual barrier struct for */
                                        /* thread barrier */

/* Stolen from irix/kern/io/cl.c
**
** Macro: 	CL_HEX
** Purpose: 	Convert # into 2 character hex representation.
** Parameters:	_x - # to convert
**		_s - string with at least 3 characters to fill in, null
**		     terminated.
*/
static		char _hex[] = "0123456789abcdef";
#define	CL_HEX(_x, _s)	(_s)[0] = _hex[((_x) >> 4)]; \
                        (_s)[1] = _hex[((_x) & 0xf)];\
                        (_s)[2] = '\0'

/* open_cl_dev
** -----------
** open a new craylink device to the given partition with the given flags
*/
static int open_cl_dev(int *fd, partid_t open_part, int flags, int channel)
{
    char devname[32];
    char part[3];
    char chan[3];

    CL_HEX(open_part, part);
    CL_HEX(channel, chan);
    sprintf(devname, "%s/%s/%s", CL_DEV_PATH, part, chan);
    dprintf(5, ("Trying to open %s\n", devname));
    *fd = open(devname, flags);
    if(*fd < 3)  {
	dprintf(0, ("ERROR: Couldn't open cl raw device %s\n", devname));
	perror("open_cl_dev");
	return(EXIT_FAILURE);
    }
    dprintf(5, ("cl raw device %s opened\n", devname));
    return(EXIT_SUCCESS);
}

/* communicate_number
** ------------------
** given the number of partitions and the our index into the cl_fd array,
** communicate the number given to all other partitions as many as
** num_coms times.  This is a helper function used in both barrier_init
** and barrier_to_all_parts.
*/
static int communicate_number(int num_parts, int my_index, int number,
			      int *answers)
{
    int i, result, failed = 0;

    result = number;

    /* communicate to each other partition */
    for(i = 0; i < num_parts; i++) {

	/* if they have a lower index than ours, then we write
	** first and then read.
	*/
	if(my_index < i) {
	    dprintf(10, ("writing then reading index %d, fd %d\n", 
			 i, cl_fds[i]));
	    if(write(cl_fds[i], &number, sizeof(int)) < sizeof(int)) {
		dprintf(0, ("ERROR: cl interface didn't send all data\n"));
		perror("communicate_number");
		failed = 1;
	    }
	    if(read(cl_fds[i], &result, sizeof(int)) < sizeof(int) ||
	       (!answers && result != number)) {
		dprintf(0, ("ERROR: cl interface read wrong number\n"
			    "\tresult = %d and should have been %d\n", 
			    result, number));
		if(result == number) perror("communicate_number");
		failed = 1;
	    } else if(answers) {
		answers[i] = result;
	    }
	}
	    
	/* if they have a higher index than ours, then we read and
	** then write */
	else if(my_index > i) {
	    dprintf(10, ("reading then writing index %d, fd %d\n", 
			 i, cl_fds[i]));
	    if(read(cl_fds[i], &result, sizeof(int)) < sizeof(int) ||
	       (!answers && result != number)) {
		dprintf(0, ("ERROR: cl interface read wrong number\n"
			    "\tresult = %d and should have been %d\n", 
			    result, number));
		if(result == number) perror("communicate_number");
		failed = 1;
	    } else if(answers) {
		answers[i] = result;
	    }
	    if(write(cl_fds[i], &number, sizeof(int)) < sizeof(int)) {
		dprintf(0, ("ERROR: cl interface didn't send all data\n"));
		failed = 1;
	    }
	}

	/* same index we already have the answer */
	else {
	    if(answers) {
		answers[i] = number;
	    }
	}

	/* if we have the same index as the partition we are trying
	** send do then we just don't send it at all. */
	if(failed) return -1;
    }
    return 0;
}


/* barrier_init
** ------------
** Initialize the barrier for all threads and all partitions to wait
** for all the other threads and partitions to reach before continuing.
*/
int barrier_init(int num_parts, partid_t *part_list, int my_index,
		 int num_tests, int *tests, key_t *keys)
{
    int i;
    key_t bar_key;
    key_t bar_key_answers[MAX_PARTITIONS];

    /* open all the cl devices for us to communicate to other parts */
    for(i = 0; i < num_parts; i++) {
	if(i != my_index) {
	    if(open_cl_dev(&(cl_fds[i]), part_list[i], O_RDWR, DEFAULT_CHAN))
		return(EXIT_FAILURE);
	}
    }

    for(i = 0; i < num_tests; i++) {
	if(communicate_number(num_parts, my_index, tests[i], NULL) < 0) {
	    dprintf(0, ("ERROR: not all partitions agree on user input "
			"numbers\n"
			"\t NOTE: all parameters on all partitions "
			"running this\n"
			"\t test should match (with exception of -v)\n"));
	    return(EXIT_FAILURE);
	}
    }

    if(my_index == 0) {
	bar_key = xp_ftok(NULL, 1);
    } else {
	bar_key = -1;
    }

    /* trade key for barrier */
    if(communicate_number(num_parts, my_index, bar_key, bar_key_answers) < 0) {
	dprintf(0, ("ERROR: error in trading keys for barrier shared mem\n"));
	return(EXIT_FAILURE);
    }

    /* trade keys */
    if(communicate_number(num_parts, my_index, keys[my_index], keys) < 0) {
	dprintf(0, ("ERROR: error in trading keys with other partitions\n"));
	return(EXIT_FAILURE);
    }

    /* init barrier shared memory */
    part_bar_shmid = xp_shmget(bar_key_answers[0], num_parts * sizeof(int), 0);
    if(part_bar_shmid == -1) {
	dprintf(0, ("ERROR: error in xp_shmget for 0x%x key\n", 
		    bar_key_answers[0]));
	return(EXIT_FAILURE);
    }
    if(xp_shmctl(part_bar_shmid, IPC_AUTORMID) < 0) {
	dprintf(0, ("ERROR: can't IPC_AUTORMID for xp_shmctl for 0x%x shmid\n",
		    part_bar_shmid));
	return(EXIT_FAILURE);
    }
    part_barrier = xp_shmat(part_bar_shmid, NULL, 0);
    if(!part_barrier) {
	dprintf(0, ("ERROR: error in xp_shmat for 0x%x key, 0x%x shmid\n", 
		    bar_key_answers[0], part_bar_shmid));
	return(EXIT_FAILURE);
    }

    /* init the barrier for the threads within this partition. */
    thread_bar_file = usinit(BARRIER_FILE);
    thread_bar = new_barrier(thread_bar_file);
    if(!thread_bar_file || !thread_bar) {
	dprintf(0, ("ERROR: couldn't set up thread barrier\n"));
	perror("barrier_init");
	return(EXIT_FAILURE);
    }

    for(i = 0; i < num_parts; i++) {
	if(i != my_index) 
	    close(cl_fds[i]);
    }

    return(EXIT_SUCCESS);
}


/* barrier_to_all_parts
** --------------------
** this serves as a barrier to all other partitions and all other threads
** so that all programs communicating here are all syncronized at this
** point.
*/
int barrier_to_all_parts(int num_parts, int my_index, int num_threads,
			 int thread_num)
{
    static int num_bar = 1;
    int done = 0;
    int i;

    /* first make sure that all the threads are in here so that we don't
    ** let another partition start going passed just cause thread 0 made
    ** it this far */
    dprintf(10, ("entering first thread barrier\n"));
    barrier(thread_bar, num_threads);

    /* tell other partitions we are here now */
    dprintf(5, ("BARRIER START #%d\n", num_bar));

    /* only one thread need to be spinning and writing since we have
    ** all the other threads hitting the intra partition barrier */
    if(thread_num == 0) {
	part_barrier[my_index] = num_bar;
	while(!done) {
	    done = 1;
	    for(i = 0; i < num_parts; i++) {
		if(part_barrier[i] != num_bar) {
		    done = 0;
		    break;
		}
	    }
	}
    }

    /* all threads are basically waiting for thread 0 to talk to other
    ** partitions. */
    dprintf(10, ("entering last thread barrier\n"));
    barrier(thread_bar, num_threads);
    dprintf(5, ("BARRIER DONE #%d\n", num_bar));
    num_bar++;
    return(EXIT_SUCCESS);
}


/* barrier_close
** -------------
** Close the barrier by closing the cl descriptors to other partitions
** and closing up the thread barrier.
*/
void barrier_close(int my_index, int thread_num)
{
    int i;

    /*    if(my_index == 0) { */
#if (_MIPS_SIM == _ABI64)
	dprintf(1, ("barrier_close: shmdt of 0x%llx\n", part_barrier));
#else
	dprintf(1, ("barrier_close: shmdt of 0x%x\n", part_barrier));
#endif
	xp_shmdt(part_barrier);
	/* }*/
    free_barrier(thread_bar);
    usdetach(thread_bar_file);
}


