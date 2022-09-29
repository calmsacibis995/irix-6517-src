 /*************************************************************************
 #									  *
 # 		 Copyright (C) 1994, 1995  Silicon Graphics, Inc.	  *
 #									  *
 #  These coded instructions, statements, and computer programs  contain  *
 #  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 #  are protected by Federal copyright law.  They  may  not be disclosed  *
 #  to  third  parties  or copied or duplicated in any form, in whole or  *
 #  in part, without the prior written consent of Silicon Graphics, Inc.  *
 #									  *
 #************************************************************************/

#ident "$Revision: 1.7 $"

#ifdef __STDC__
	#pragma weak clock_gettime = _clock_gettime
#endif
#include "synonyms.h"

#include <time.h>
#include <errno.h>
#include <timers.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/syssgi.h>
#include <malloc.h>
#include "mplib.h"
#include "sys_extern.h"

static void read_cycle_counter(struct timespec *tp);

/*
 * POSIX 1003.1b-1993 style gettimeofday(2) 
 */
int
clock_gettime(clockid_t clock_id, struct timespec *tp)
{
	struct timeval tv;
	int retval = 0;
	switch(clock_id) {
	    case CLOCK_REALTIME: 
		if (tp) {
			retval = BSD_getime(&tv);
			/* timeval_to_timespec(tp, &tv);*/
			tp->tv_sec = (time_t)tv.tv_sec;
			tp->tv_nsec = tv.tv_usec * 1000;
		}
		break;
	    case CLOCK_SGI_CYCLE:
		read_cycle_counter(tp);
		break;
	    default:
		setoserror(EINVAL);
		retval = -1;
	}
	return (retval);
}


/*
 * This uses the SYSSGI call SGI_CYCLECNTR_SIZE to write code that
 * should run on all SGI machines. The biggest problem is that on some
 * machines you read the counter into a 32 bit value, and on other
 * machines you read the counter into a 64 bit value. This code does
 * not try and handle the wrap as that would add limitations to how
 * often the user would have to call this function for correctness */

typedef unsigned long long iotimer64_t;
typedef unsigned int iotimer32_t;

struct cyclecounter {
    volatile iotimer64_t *iotimer_addr64;
    volatile iotimer32_t *iotimer_addr32;
    unsigned int cycleval;
    ptrdiff_t iotimer_size;
};

static void
read_cycle_counter(struct timespec *tp)
{
    ptrdiff_t phys_addr, raddr;
    volatile iotimer32_t counter_value32;
    volatile iotimer64_t counter_value64;
    int poffmask;
    static struct cyclecounter *cc = NULL;
#if (_MIPS_SIM == _ABIO32)
    unsigned int high, low;
#endif

    /* Fast path skips cyclecounter allocation
     */
    if (!cc) {
	LOCKDECLINIT(l, LOCKMISC);

	/* Double-check under lock
	 */
	if (!cc) {
	    unsigned int cycleval;
	    struct cyclecounter *cc_tmp;
	    int fd;

	    cc_tmp = (struct cyclecounter *)malloc(sizeof(struct cyclecounter));
	    cc_tmp->iotimer_size = syssgi(SGI_CYCLECNTR_SIZE);
	    poffmask = getpagesize() - 1;
	    phys_addr = syssgi(SGI_QUERY_CYCLECNTR, &cycleval);

	    /* Change from picoseconds to nanoseconds */
	    cc_tmp->cycleval = cycleval/1000; 
	    raddr = phys_addr & ~poffmask;
	    fd = open("/dev/mmem", O_RDONLY);
	    if (cc_tmp->iotimer_size == 64){
		cc_tmp->iotimer_addr64 = (volatile iotimer64_t *)mmap(0,
							      poffmask,
							      PROT_READ,
							      MAP_PRIVATE,
							      fd,
							      (__psint_t)raddr);
		cc_tmp->iotimer_addr64 = (iotimer64_t *)
			((__psunsigned_t)(cc_tmp->iotimer_addr64)
			+ (phys_addr & poffmask));
	    } else {
		cc_tmp->iotimer_addr32 = (volatile iotimer32_t *)mmap(0,
							      poffmask,
							      PROT_READ,
							      MAP_PRIVATE,
							      fd,
							      (__psint_t)raddr);
		cc_tmp->iotimer_addr32 = (iotimer32_t *)
			((__psunsigned_t)(cc_tmp->iotimer_addr32)
			+ (phys_addr & poffmask));
	    }
	    close(fd);
	    cc = cc_tmp;
	}
	UNLOCKMISC(l);
    }

    if (cc->iotimer_size == 64) {
	/*
	 * Only for O32 do we have to do two reads. N32 and N64 both can
	 * read the clock in one try.
	 */
#if (_MIPS_SIM == _ABIO32)
	    
	    do {
		    high = *(unsigned int *)(cc->iotimer_addr64);
		    low = *(unsigned int *)(((unsigned int *)(cc->iotimer_addr64)) + 1);
	    } while (high != *(unsigned int *)(cc->iotimer_addr64));
	    counter_value64 = ((long long)high << 32) + low;
	    
#else 
	    counter_value64 = *(cc->iotimer_addr64);
#endif /* _MIPS_SZLONG == 32 */
    } else {
	    counter_value32 = *(cc->iotimer_addr32);
	    counter_value64 = counter_value32;
    }
    counter_value64 *= cc->cycleval;
    tp->tv_sec = (time_t)(counter_value64 / (long long)NSEC_PER_SEC);
    tp->tv_nsec = (long)(counter_value64 - (tp->tv_sec * (long long)NSEC_PER_SEC));
}
