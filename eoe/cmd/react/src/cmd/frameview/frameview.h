/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************

 **************************************************************************
 *	 								  *
 * File:	frameview.h 						  *          
 *									  *
 * Description:	This file defines the structures and decalred all the     *
 *              variables needed by the textual portion of the            *
 *              real-time performance monitoring tools                    *
 *                                                               	  *
 *			                				  *
 * Copyright (c) 1994 by Silicon Graphics, Inc.  All Rights Reserved.	  *
 *									  *
 **************************************************************************/


#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <sys/syssgi.h>
#define _BSD_SIGNALS
#include <sys/signal.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <sys/lock.h>
#include <ulocks.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/times.h>
/*#include <math.h>*/
#include <sys/sysmp.h>
#include <sys/wait.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/rtmon.h>
#include <sys/systeminfo.h>
#include <sys/cpu.h>
#include <malloc.h>

#include <sys/procfs.h>
#include <sys/dir.h>
#include <paths.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <rpc/rpc.h>


/*#define DEBUG*/

#define NO_FRAME_FILENAME ".fv-no_frame"

#define MAX_PROCESSORS 36

#define FALSE 0
#define TRUE 1

#define FILE_ACK -1

#define EX_EV_INTR_ENTRY -1
#define EX_EV_INTR_EXIT -2
#define EX_EV_KERN -3
#define EX_EV_USER -4
#define EX_EV_INIT -6
#define EX_EV_STATECHANGE -7

#define KERNEL_TSTAMPS 1
#define USER_TSTAMPS 2

#define MAX_TSTAMP_NUMB 65535
#define MAX_TSTAMP_NAME_LENGTH 20

#define MAX_MAJ_FRAME_EVENTS 4096
#define MAX_NUMBER_NF_EVENTS 1000

/* this must be the greater of MAX_MAJ_FRAME_EVENTS and MAX_NUMBER_NF_EVENTS */
#define EVENT_LIST_SIZE (4096 * EVENT_LINE_LENGTH)



#define DEFAULT_KTSTAMP_ENTRIES         8192 /* number of entries */
#define DEFAULT_UTSTAMP_ENTRIES         8192 /* number of entries */

#define EVENT_MASK TAGMASK
#define EVENT_SHIFT TAGSHIFT

#define FATAL_MSG 0
#define ERROR_MSG 1
#define WARNING_MSG 2
#define ALERT_MSG 3
#define NOTICE_MSG 4
#define DEBUG_MSG 5
void grtmon_message(char *, int, int);

/* don't change these without knowing what you're doing
   they are dependent upon menu position */
#define REAL_DATA 0
#define FILE_DATA 1

#define OFF 0
#define ON 1

#define GRAPH 0
#define LOG 1

#define FIRST_MAJOR_FRAME 1

#define MAX_PID_LENGTH 16
#define MAX_PID 32768

#define MAX_CHOICE_EVENTS 32

#define MERGE_Q_ENTRIES 50000 /* size of the merge queue */
#define LOG_ENTRIES 3000 /* the number of log entries we want to log in one
			    block, the merging routine will unblock the 
			    file log process every this many entries */
		       
#define NUMBER_MERGE_EVENTS 100000  /* the number of merge events to gather
				    before stopping */

#define INF_TIME 999999999  /* a number larger than possible biggest time interval */

#define PERM_ALL          511 /* All permission to a shared memory segment */

/* careful tic period must be in usecs */
#define TIC2USECL(TICS)  ((long long)(((float)TICS)*tic_period))
#define TIC2USEC(TICS)  ((int)(((float)TICS)*tic_period))

#define RTMON_WAIT_TIME 100

typedef int boolean;

typedef struct {
    int numb_minors;
    int minor_time;
    int numb_processes;
    int *numb_in_minor; /* declare dynamically */
    int template_minors;     /* what our           */
    int template_processes;  /* current template   */
    int *template_in_minor;  /* looks like         */
} major_frame_account_t;




typedef struct {
    uint nentries;
    uint ls_counter;                   /* count lost timestamps       */
    uint tstamps_mask;                 /* the active time stamps      */
    boolean enabled;                   /* is user gatherig active     */
    boolean kenabled;                  /* is kernel gatherig active   */
    char filename[32];                 /* filename with this stream   */
    int start_entries;                 /* number of entries to major 3*/


    /* we need to keep track of these for destroying them in cleanup
       these are for the processes the user request for gathering
       and displaying stats of each process */
    int log_file_proc_id, merge_proc_id;
    int writing_log_file;

    /* this index keeps track of where in the stream we are reading
       for the the user and merge count inidicate how many have been
       written by merge, read count how many read by user, so we
       know not to give the user another event if these are equal, 
       we use two values to avoid have to perform atomic ops on
       a single count value */
    int stream_index, stream_merge_count, stream_user_count;

    /* merging is a per cpu variable that the merging routine will
       check every iteration and exit if it sees it set to FALSE */
    boolean merging;

} user_state_t;



typedef struct {
    long long time;
    int event;
    int pid;
    int type; /* 0 kernel, 1 user */
    long long quals[6];
} merge_event_t;



/* data used to scan through a file */
typedef struct {
    merge_event_t *merge_queue;
    user_state_t *merge_state;
} scan_data_t;

/* a list used to keep track of the number of unique pids we see
   in a major frame */
typedef struct mpl_elem{
    int pid;
    char name[MAX_PID_LENGTH];
    struct mpl_elem *next;
} maj_pid_list_t;


/* this is used to determine which cpus the user has requested
   that rt onitoring be set up on, it is an array containing the
   cpu number and the mask */

typedef struct {
    int cpu;
    uint mask;
} cpu_mapping_t;


typedef struct {
    int pid;
    boolean logging,stats;
    boolean read_from_file;
    char filename[32];
    int start_index;
    int end_index;
    int cpu;
} rtmon_merge_info_t;

typedef struct {
    int pid;
    usema_t* ave_sem;
    boolean from_file;
} log_file_state_t;

typedef struct {
    char name[32];
    int numb;
} choice_event_t;


void init_stats();
void start_stats(int cpu);

boolean old_valid_cpu(int cpu);


void init_stat_structures(int cpu);
void display_graphics(void *);



/* major_frame_account keeps track of the number of minors on each
   cpu and the number of processes within each minor */

MAIN maj_pid_list_t maj_pid_list[MAX_PROCESSORS];
MAIN volatile major_frame_account_t major_frame_account[MAX_PROCESSORS];

MAIN volatile int graphics_proc_id;
MAIN int read_stream_proc_id[MAX_PROCESSORS];


MAIN usptr_t *sem_shared_arena;
MAIN usptr_t *event_list_sem;
MAIN usema_t *ave_sem[MAX_PROCESSORS];



/* use this to indicate if the real time data stream is
   about to call major frame stats and thereby update
   the graph info, there's still a timing window here
   because it's only a flag, but no biggy, it should never
   happen and if it does the user can clear again */
MAIN volatile int going_to_update_graph_info[MAX_PROCESSORS];
MAIN volatile int scan_type; /* REAL_DATA from real data   FILE_DATA from file */
MAIN volatile int numb_maj_frames[MAX_PROCESSORS];  /* total we've seen */
MAIN volatile int maj_frame_numb[MAX_PROCESSORS];  /* the one we are on */

MAIN volatile long long absolute_start_time;  /* in ticks */
MAIN volatile int absolute_start_count, absolute_start_access;

/* first index kernel/user  second index GRAPH/LOG   */
MAIN char tstamp_event_name[MAX_TSTAMP_NUMB][MAX_TSTAMP_NAME_LENGTH];
MAIN volatile boolean rtmon_program_running;


/* TIMESTAMP_ADJ_COMMENT : as of 12-01-95 we've decided to eliminate the
   ability to subtract out the time taken to do timestamps, because: it's
   not clear how useful it is in non-frs mode - it actually varies quite
   a bit from tstamp to tstamp - bob, jeff
   all the code related to this
   should go away after 06-01-96 unless someone complains prior 
MAIN volatile int adjust_mode;
*/
MAIN volatile int no_frame_mode;
MAIN volatile int kern_funct_mode;
MAIN volatile int collecting_nf_events;
MAIN volatile int number_user_tstamps;

extern int atomicInc(volatile int *addr);

/* +++++++++++++++++++++++++++++++++++ */
/* +++++++++++++++++++++++++++++++++++ */
/* +++++++++++++++++++++++++++++++++++ */
/* +++++++++++++++++++++++++++++++++++ */
/* +++++++++++++++++++++++++++++++++++ */
/* +++++++++++++++++++++++++++++++++++ */
typedef long long tag_and_time_t;


void read_stream(void *cpu_val);
#define RTMON_KERNEL_EVENT 0
#define RTMON_USER_EVENT 1

typedef enum {
    TSTAMP_EVENT_USER_FIRST,
    TSTAMP_EVENT_USER_START,  /* control returned to user */
    TSTAMP_EVENT_USER_END,    /* control surrended by user yield() */
    TSTAMP_EVENT_USER_BASE    /* base for user to define tstamp_events */
} tt_t;

void exit_frameview();

void rtmon_start_graphics();
void rtmon_write_def_file(char *filename);
void rtmon_read_def_file(char *filename);

/*****************  new stuff I added after daemon merge ***************/
MAIN volatile boolean cpu_on[MAX_PROCESSORS];
MAIN volatile boolean verify_on[MAX_PROCESSORS];
MAIN int debug;
MAIN int show_live_stream;  /* true when we want to constantly change
			       what the view looks like */
MAIN int live_stream;  /* true if we want frameview to read from a 
			  live stream of data versus a file */
MAIN int logging;  /* true if we want frameview to write to live stream to file */
#ifdef DEBUG
MAIN FILE *fp[MAX_PROCESSORS];
#endif

/* a file descriptor for each file we want access to */
MAIN int fd[MAX_PROCESSORS];
MAIN int fd_fvw[MAX_PROCESSORS];
MAIN int fd_fvr[MAX_PROCESSORS];
MAIN volatile int major_frame_account_valid;

MAIN volatile boolean template_matching;

#define EVENT_LINE_LENGTH 40



MAIN volatile int parent_id;
MAIN volatile boolean erase_event_list;
MAIN char event_list[EVENT_LIST_SIZE];

#define NUMB_MAJ_FRAMES_PER_BLOCK 1000

/* we need a structure to keep track of the byte offsets into the 
   stream of every major frame start, since we want to allow for
   any number of major frames but can't allocate infinite space and
   don't want to allocate space every major frame, we will group the
   offsets into blocks.  
   Once we fill up a block we'll realloc we can adjust the frequency 
   of this by setting the constant NUMB_MAJ_FRAMES_PER_BLOCK */
typedef struct {
  int maj_frame_off;
  int nf_ex_ev_count;
  long long time;
} maj_frame_block_t;

MAIN maj_frame_block_t *maj_frame_block[MAX_PROCESSORS];
MAIN int maj_frame_block_allocs;

struct pid_elem {
    int pid;
    struct pid_elem *next;
};
MAIN struct pid_elem *master_pid_list;

/* since only one process can communicate with the graphics
   we will use this as a flag and indicate when one of the
   processes calculating stats wants the graphics to put
   up a define template window */
MAIN int fix_template[MAX_PROCESSORS];
MAIN volatile int defining_template, defining_template_cpu;

/* the file descriptors we will use for reading and
   writing files */
MAIN int fd[MAX_PROCESSORS], fd_fv[MAX_PROCESSORS];

/* offset 10000 into reserved range a special event I need to 
 indicate we have fallen back to idle loop even though there
 was no explicit event */
#define MY_EVENT_EXIT_TO_USER 30000

MAIN char hostname[128];

MAIN int getting_data_from;

MAIN int reading_count, reading_count_done;
MAIN int my_extension;

MAIN long long no_frame_event_start_time[MAX_PROCESSORS];

#define MAX_KERN_FUNCT_NAME_SIZE 24
#define KERN_FUNCT_ID_START 100000

MAIN char pid_name_map[MAX_PID][MAX_PID_LENGTH];
MAIN char kern_funct_name_map[MAX_PID][MAX_KERN_FUNCT_NAME_SIZE];

/* variables for allowing user to define their own frame start events */
MAIN int major_frame_start_event, minor_frame_start_event;
MAIN int frame_trigger_event;

MAIN volatile long long *timer_addr;
void map_timer(void);


/* careful tic period must be in usecs */
MAIN float tic_period;
MAIN int tstamp_adj_time;
MAIN int numb_processors;
MAIN int computed_minor_time;
MAIN int numb_trigger_events, numb_maj_events, numb_min_events;
MAIN choice_event_t trigger_event[MAX_CHOICE_EVENTS];
MAIN choice_event_t maj_event[MAX_CHOICE_EVENTS];
MAIN choice_event_t min_event[MAX_CHOICE_EVENTS];
MAIN int choice_trigger_event, choice_maj_event, choice_min_event;
MAIN int glob_getting_data;

/* defaults settable in .frameview.defaults */
MAIN char default_filename[128];
MAIN int default_no_frame_mode;
MAIN int default_kern_funct_mode;
MAIN int default_trigger_event, default_maj_event, default_min_event;
MAIN char tmp_dir_str[128];

MAIN int message_level;  /* level of messages user wants */

MAIN char cl_filename[128], cl_kern_mode[16];
MAIN char cl_no_frame_mode[16], cl_message_level[16];

typedef struct 
{
    char msg[256];
    int type;
    int identifier;
    int access_count;
    int ready;
} msg_data_t;

MAIN msg_data_t *msg_data;

#define SEARCH_ALL 0
#define SEARCH_MAJ 1
MAIN int current_search_index, current_search_major;
MAIN int current_search_mode, current_search_cpu, current_search_on, current_search_found;
MAIN long long search_line;

/* ======= Installation =============================================== */

#define INSTALL_DIR	"/usr/traceview"
