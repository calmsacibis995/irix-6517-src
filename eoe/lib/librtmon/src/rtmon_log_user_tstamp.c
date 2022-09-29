/**************************************************************************
 *                                                                        *
 *       Copyright (C) 1995, Silicon Graphics, Inc.                       *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicrtmon_log_user_tstamp.c.good ated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                       *
 **************************************************************************/
#include <stdio.h>
#include <task.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syssgi.h>
#include <sys/schedctl.h>
#include <sys/systeminfo.h>

#include <rtmon.h>

#pragma weak rtmon_log_user_tstamp = _rtmon_log_user_tstamp
#define rtmon_log_user_tstamp _rtmon_log_user_tstamp

static volatile long long *timer_addr64 = 0;
static volatile long *timer_addr32 = 0;

static void map_timer(void);
static void rtmon_init_user_logging(int);
static user_queue_t **user_queue;
static pid_t user_pid;

static volatile int number_inited = 0;
static volatile int initialization_complete = 0;

#define INIT_PID_LIST_SIZE 50
#define INC_MARGIN 10
/* a list of the pids that have inidcated they have logged an entry */
int *pid_list;
static volatile int current_pid_list_size = INIT_PID_LIST_SIZE;
static int increasing_pid_list_size = 0;

static	int rtmon_debug = 0;		/* enable debugging messages */

static void
error(const char* fmt, ...)
{
    va_list ap;

    fprintf(stderr, "rtmon_log_user_tstamp: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, ".\n");
}

static void
fatal(const char* fmt, ...)
{
    va_list ap;

    fprintf(stderr, "rtmon_log_user_tstamp: Fatal error: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, ".\n");
    exit(-1);
}

static void
map_timer()
{
    ptrdiff_t phys_addr, raddr;
    long cycleval;
    int fd;
    int cyclecntr_size;
    int poffmask;

    /* map the cycle counter into user space. 
     * The cycle counter may either represent a 32 or 64 bit clock.
     */

    poffmask = getpagesize() - 1;
    phys_addr = syssgi(SGI_QUERY_CYCLECNTR, &cycleval);
    if ((__psint_t)phys_addr == -1)
	fatal("syssgi(SGI_QUERY_CYCLECNTR): %s", strerror(errno));
    raddr = phys_addr & ~poffmask;

    cyclecntr_size = (int) syssgi(SGI_CYCLECNTR_SIZE);
    if ((__psint_t)cyclecntr_size == -1)
	fatal("syssgi(SGI_CYCLECNTR_SIZE): %s", strerror(errno));

    if ((fd = open("/dev/mmem", O_RDONLY)) == -1)
	fatal("open(/dev/mmem): %s", strerror(errno));
    if (cyclecntr_size == 64) {		/* 64 bit clock */
        timer_addr64 = (volatile long long *) mmap(0,
						 poffmask,
						 PROT_READ,
						 MAP_PRIVATE, 
						 fd,
						 (__psint_t)raddr);
	if (timer_addr64 == (long long *)(-1))
	    fatal("Could not mmap cycle counter: %s", strerror(errno));
        timer_addr64 = (long long *)
	    ((__psunsigned_t)timer_addr64 + (phys_addr & poffmask));
    } else {				/* 32 bit clock */
        timer_addr32 = (volatile long *) mmap(0,
					    poffmask,
					    PROT_READ,
					    MAP_PRIVATE,
					    fd,
					    (__psint_t)raddr);
        if (timer_addr32 == (long *)(-1))
	    fatal("Could not mmap cycle counter: %s", strerror(errno));
        timer_addr32 = (long *)
	    ((__psunsigned_t)timer_addr32 + (phys_addr & poffmask));
    }
    (void) close(fd);
}

/* code determines how much of the initialization we need to do
   0 means everything 
   1 means we want to avoid (re)mallocing space
   see comment before rtmon_log_user_tstamp (the main purpose of this file) */
static void
rtmon_init_user_logging(int code)
{
    int i;
    int numb_procs;
    char numb_procs_str[32];
    int queue_size;
    void *queue_start;
    int queue_fd;

    if (code == 0) {
	pid_list = ((int *) malloc(INIT_PID_LIST_SIZE * sizeof(int)));
    }
    user_pid = get_pid();
    sysinfo (_MIPS_SI_NUM_PROCESSORS, numb_procs_str, 32);
    numb_procs = atoi(numb_procs_str);


    if ((queue_fd = open(RTMON_SHM_FILENAME, O_RDWR)) == -1) {
	error("Could not open %s: %s", RTMON_SHM_FILENAME, strerror(errno));
	error("Check if the rtmond daemon is running.\n");
	exit(-1);
    }

    queue_size = numb_procs*sizeof(user_queue_t);
    queue_start = mmap(0, queue_size, PROT_WRITE | PROT_READ,
		       MAP_SHARED | MAP_AUTOGROW, queue_fd, 0);

    if (queue_start == (unsigned *)-1)
	fatal("Could not mmap user event queue: %s", strerror(errno));

    map_timer();

    /* set up the ability to use the get_cpu() function call */
    schedctl(MPTS_SETHINTS);

    if (code == 0) {
	for (i=0;i<INIT_PID_LIST_SIZE;i++)
	    pid_list[i] = -1;
	pid_list[0] = get_pid();
    }

    if (code == 0) {
	if (user_queue == NULL)
	    user_queue = ((user_queue_t **) malloc(numb_procs * sizeof(user_queue_t)));
    }
    for (i=0;i<numb_procs;i++) {
	    user_queue[i] = (user_queue_t *) ((__psint_t)queue_start +
					      (i*sizeof(user_queue_t)));
	    
    }
    
}


/* there is some difficulty in determining when to intialize the data
   structures in this file since there is no central process or place
   where this gets called from.  To deal with this we keep an array of pids
   of processes in a share group.  When a process discovers that it is not
   in the array (this is the first time it has called (log user tstamp)
   then it checks the other pids in the list and determines what sharing 
   level it has with them.  If it finds it is sharing address space then its
   sharing partner has already initialized all the structures and they are 
   valid.  If not then it checks to see the sharing level of the other pids
   in the list.  It calls the initialization with the code to indicate the
   sharing level - it turns out that anything "below" sharing address we
   need to do everything but re malloc space */   

/* all entries will have their evt cleared to 0 at initialization and
   after the merging daemon reads them, only non-0 events will be 
   considered valid, evt 0 is the control evt BEGIN and will only
   occur once and never put put into either the kernel or user queue
   so this is fine */
void
rtmon_log_user_tstamp(event_t evt, unsigned long long qual0,
		      unsigned long long qual1, unsigned long long qual2, 
		      unsigned long long qual3)
{
	user_queue_t *uq;
	int index, next_index;
	int pid;
	int cpu;
	int i, ret_val;
	int loc_number_inited;
	int check_mask, mask_level;
	long long my_time64;
	unsigned long my_time32;
	const char* cp;

	pid = get_pid();

	loc_number_inited = atomicInc(&number_inited);

	cp = getenv("_RTMON_DEBUG");
	if (cp)
		rtmon_debug = atoi(cp);

	if (loc_number_inited == 0) {
		rtmon_init_user_logging(0);
		initialization_complete = 1;
	}
	else {
		while (initialization_complete == 0) 
			sginap(0);
		i = 0;
		if (pid_list[0] == -1) {
			/* no one in list - initialize everything */
			schedctl(MPTS_SETHINTS);
			rtmon_init_user_logging(0);
			goto initialized;
		}
		while ((pid_list[i] != -1) && (pid_list[i] != pid))
			i++;
		index = i;
		if (index > (current_pid_list_size - INC_MARGIN)) {
			ret_val = atomicInc(&increasing_pid_list_size);
			if (ret_val == 0) {
				/* we have the right and responsibility to increase the
				   size of the pid list array */
				current_pid_list_size += INC_MARGIN;
				pid_list = ((int *) realloc((void *) pid_list, 
							    current_pid_list_size * sizeof(int)));
				for (i=(current_pid_list_size - INC_MARGIN);
				     i<current_pid_list_size;
				     i++)
					pid_list[i] = -1;

			}
			atomicDec(&increasing_pid_list_size);
		}
		if (pid_list[i] == -1) {
			schedctl(MPTS_SETHINTS);
			/* now go through array and check to see what
			   type of sharing exists */
			i = 0;
			mask_level = 0;
			while (pid_list[i] != -1) {
				/*in adding ourselves to the pid_list array
				  there's a race and the addition may actually not 
				  get registered, but that's not the the end of the
				  world we'll just do it next time */

				check_mask = prctl(PR_GETSHMASK, pid_list[i]);
				if ((check_mask & PR_SADDR) == PR_SADDR) {
					/* also easy case we are sharing all address and mmapped files */
					pid_list[index] = pid;
					goto initialized;
				}
				i++;
			}
			/* if we're here we are not sharing everything or addresses
			   if we are sharing file descriptors re-initialize without
			   re-opening files by calling rtmon_init with code 1 else
			   reitialize everything */
			rtmon_init_user_logging(mask_level);
		}
	}
initialized:

	evt += USER_EVENT_BASE;
	if (!(MIN_USER_ID <= evt && evt < MIN_KERNEL_ID)) {
		error("Invalid event number %d, must be in the range [0,%d]",
		    evt - USER_EVENT_BASE, MIN_KERNEL_ID-MIN_KERNEL_ID);
		return;
	}

	cpu = get_cpu();
	uq = user_queue[cpu];

	if (!uq->enabled) {
		if (rtmon_debug)
			error("User event queue is not enabled");
		return;
	}

	/* we've already lost timestamps don't overwrite buffer any more */
    
	if (uq->state.lost_counter) {
		if (atomicInc(&(uq->state.lost_counter)) == 0)
			uq->state.lost_counter = 0;
		if (rtmon_debug)
			error("User event queue overflow; lost %lu events",
			    uq->state.lost_counter);
		return;
	}
	index = atomicIncWithWrap(&(uq->state.curr_index),uq->nentries);
	/*
	 * Verify the queue entry is free (the evt field
	 * must be zero); if not, then mark the entry lost
	 * and abandon the event data.
	 */
	if (uq->tstamp_event_entry[index].evt != 0) {
		uq->state.lost_counter++;
		if (rtmon_debug) {
			int i = index; 
			error("User event queue overflow; lost %lu events",
			    uq->state.lost_counter);
			do {
				fprintf(stderr,
				"uq[%4d] %d/%d/%d/%llu [%llu %llu %llu %llu]\n"
				    , i
				    , uq->tstamp_event_entry[i].evt
				    , uq->tstamp_event_entry[i].cpu
				    , uq->tstamp_event_entry[i].jumbocnt
				    , uq->tstamp_event_entry[i].tstamp
				    , uq->tstamp_event_entry[i].qual[0]
				    , uq->tstamp_event_entry[i].qual[1]
				    , uq->tstamp_event_entry[i].qual[2]
				    , uq->tstamp_event_entry[i].qual[3]
				);
			} while ((i = (i+1) % uq->nentries) != index &&
			    uq->tstamp_event_entry[i].evt != 0);
		}
		return;
	}

   	/*
	 * Collect the time.
	 */
	if (timer_addr64) {
#if (_MIPS_SIM == _ABIO32)
		unsigned int high, low;
	   
		do {
			high = *(unsigned int *)(timer_addr64);
			low = *(unsigned int *)(((unsigned int *)(timer_addr64)) + 1);
		} while (high != *(unsigned int *)(timer_addr64));
		my_time64 = ((long long)high << 32) + low;
#else 
		my_time64 = *(timer_addr64);
#endif /* _MIPS_SZLONG == 32 */
	} else {
		my_time32 = *(timer_addr32);
		my_time64 = (__int64_t)my_time32;
	}
	next_index = (index + 1) % uq->nentries;

	uq->tstamp_event_entry[index].qual[0] = qual0;
	uq->tstamp_event_entry[index].qual[1] = qual1;
	uq->tstamp_event_entry[index].qual[2] = qual2;
	uq->tstamp_event_entry[index].qual[3] = qual3;

	if (index == uq->state.curr_index ||
	    uq->tstamp_event_entry[next_index].evt == 0 ||
	    my_time64 < uq->tstamp_event_entry[next_index].tstamp) {
		uq->tstamp_event_entry[index].tstamp = my_time64;
	} else {
		uq->tstamp_event_entry[index].tstamp = uq->tstamp_event_entry[next_index].tstamp - 1;
	}
      
	uq->tstamp_event_entry[index].cpu = cpu;
	uq->tstamp_event_entry[index].jumbocnt = 0;	/* no jumbos yet */

	/* this must be last, by entering a non-0 evt we a certifying the
	   validity of this entry */
	uq->tstamp_event_entry[index].evt = evt;

	uq->state.tstamp_counter++;

	/* we've reached high water mark wake up daemon to empty queue */
	if (uq->state.tstamp_counter == uq->water_mark &&
	    syssgi(SGI_RT_TSTAMP_WAIT, cpu, 0) < 0)
		error("syssgi(SGI_RT_TSTAMP_WAIT): %s", strerror(errno));

	if (rtmon_debug > 1)
		fprintf(stderr,
"rtmon_log_user_tstamp: [evt %d cpu %d tstamp %llu data: %#llx %#llx %#llx %#llx]\n"
		    , evt
		    , cpu
		    , uq->tstamp_event_entry[index].tstamp
		    , qual0, qual1, qual2, qual3
		);
}
