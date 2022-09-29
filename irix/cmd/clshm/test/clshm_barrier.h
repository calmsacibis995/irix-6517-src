/* 
** File: clshm_lib.h
** -----------------
** define all defaults for share_mem program and functions in clshm_lib.c
** that are used in share_mam and runon_all programs for dealing with the
** clshm ioctls.
*/

#ifndef CLSHM_BARRIER_H
#define CLSHM_BARRIER_H

/* device paths */
#define CL_DEV_PATH     "/hw/xplink/raw"
#define DEFAULT_CHAN	9

/* temporary barrier file */
#define BARRIER_FILE    "/tmp/CLSHM_TESTING_BARRIER_FILE"

/* same as  CL_XPC_CHAN_LIMIT - CL_XPC_CHAN_RAW in cl.c */
#define MAX_RAW_CHAN    16

/* normally want no debug flag */
#define DEFAULT_DEBUG_FLAG	0

/* debug printing */
#define dprintf(level, x)    \
if (debug_flag >= level)  {  \
    printf x;                \
    fflush(stdout);          \
}

extern int debug_flag;


/* prototypes */

int barrier_init(int num_parts, partid_t *part_list, int my_index,
		 int num_tests, int *tests, key_t *keys);
int barrier_to_all_parts(int num_parts, int my_index, int num_threads,
			 int thread_num);
void barrier_close(int my_index, int thread_num);

#endif

