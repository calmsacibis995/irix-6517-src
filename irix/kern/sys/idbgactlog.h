#ifndef SYS_ACTLOG_H
#define SYS_ACTLOG_H

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1993, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident  "$Revision: 1.2 $"

#if defined(DEBUG) && defined(ACTLOG)

struct actlogentry {
	int	ae_act;
	void   *ae_info1;
	void   *ae_info2;
	void   *ae_info3;
        time_t  ae_sec;
        long    ae_usec;
        int     ae_cpu;
};

#define ACTLOG_SIZE 256

struct actlog {
	struct actlogentry al_log[ACTLOG_SIZE];
	int al_wind;
};

extern struct actlog sysactlog;

extern void log_act(struct actlog *, int, void *, void *, void *);

#define LOG_ANY_CPU	-1
#define ACTLOG_SIZE	256	/* must be a power of 2 */

#define DISABLE_ACTLOG() \
	{ \
		extern int actlog_enabled; \
		if (actlog_enabled) actlog_enabled -= 1; \
	}

#define ENABLE_ACTLOG() \
	{ \
		extern int actlog_enabled; \
		actlog_enabled += 1; \
	}

#define INIT_ACTLOG_LOCK() \
	{ \
		extern lock_t actlog_lock; \
		initnlock(&actlog_lock, "actlog"); \
	}


#define LOG_ACT(act, i1, i2, i3) \
	{\
		extern int actlog_enabled; \
		if (actlog_enabled) \
		    log_act(&sysactlog, act, \
			    (void *)(i1), (void *)(i2), (void *)(i3)); \
	}

#define LOG_ACT_NR(act, i1, i2, i3) \
	{\
		extern int actlog_enabled; \
		extern int actlog_active; \
		if (actlog_enabled && !actlog_active) \
		    log_act(&sysactlog, act, \
			    (void *)(i1), (void *)(i2), (void *)(i3)); \
	}

#define PRIV_LOG_ACT(log, act, i1, i2, i3) \
	{\
		extern int actlog_enabled; \
		if (actlog_enabled) \
		    log_act(log, act, \
			    (void *)(i1), (void *)(i2), (void *)(i3)); \
	}

#define PRIV_LOG_ACT_NR(log, act, i1, i2, i3) \
	{\
		extern int actlog_enabled; \
		extern int actlog_active; \
		if (actlog_enabled && !actlog_active) \
		    log_act(log, act, \
			    (void *)(i1), (void *)(i2), (void *)(i3)); \
	}

#else /* no logging */

#define DISABLE_ACTLOG()
#define ENABLE_ACTLOG()
#define INIT_ACTLOG_LOCK()
#define LOG_ACT(act, i1, i2, i3)
#define LOG_ACT_NR(act, i1, i2, i3)
#define PRIV_LOG_ACT(log, act, i1, i2, i3)
#define PRIV_LOG_ACT_NR(log, act, i1, i2, i3)

#endif /* ! (DEBUG && ACTLOG) */


/* System-level ae_act values */

#define AL_NULL		0

#define AL_INTR_IN	1
#define AL_INTR_OUT	2
#define AL_INTR_BAD	3

#define AL_SYSCALL	10

#define AL_GETRUNQ	20

#define AL_PMON		30
#define AL_PMON_COMP	31
#define AL_PMON_TRIP	32
#define AL_VMON		33
#define AL_VMON_TRIP	34
#define AL_QMON		35
#define AL_QMON_Q	36
#define AL_AMON		37
#define AL_RMON		38
#define AL_MON_RUNQ	39

#define AL_ULCK_PMON	40
#define AL_ULCK_PMON_TRIP 41
#define AL_ULCK_PMON_COMP 42
#define AL_ULCK_QMON	43
#define AL_ULCK_QMON_Q	44

#define AL_STROPEN	50
#define AL_STRCLOSE	51
#define AL_STRSERV	52

#define AL_FLREFINC	60
#define AL_FLREFDEC	61
#define AL_FLCR		62
#define AL_FLDEL	63

#define AL_FIFOFLUSH	70

#define AL_MARK0	900
#define AL_MARK1	901
#define AL_MARK2	902
#define AL_MARK3	903
#define AL_MARK4	904
#define AL_MARK5	905
#define AL_MARK6	906
#define AL_MARK7	907
#define AL_MARK8	908
#define AL_MARK9	909

#endif /* SYS_ACTLOG_H */
