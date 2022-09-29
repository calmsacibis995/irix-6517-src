/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 *	Include file for STREAMS on multiprocessors
 *
 *	$Revision: 1.44 $
 */

#ifndef __SYS_STRMP_H__
#define __SYS_STRMP_H__

#ifdef __cplusplus
extern "C" {
#endif

struct queue;
struct monitor;

typedef void (*strintrfunc_t)(void *, void *, void *);
typedef void (*strtimeoutfunc_t)(void *, void *, void *);

extern int streams_interrupt(strintrfunc_t, void *, void *, void *);
extern int mp_streams_interrupt(struct monitor **, uint, strintrfunc_t,
				void *, void *, void *);
extern void streams_lock(void);
extern void streams_unlock(void);
extern void streams_mpdetach(struct queue *);

#define SI_QENAB		0x1

#include <sys/sema.h>


#include <sys/pda.h>  /* for cpuid() */
#include <sys/uio.h>

extern toid_t itimeout(void (*)(), void *, long, pl_t, ...);

struct strintr {
	struct strintr *next;
	struct strintr *prev;
	int gen;
	toid_t id;
	uint flags;
	mon_t **monpp;
	void (*func)(void *, void *, void *);
	void *arg1;
	void *arg2;
	void *arg3;
};

struct streams_mon_data {
	int smd_flags;
	int smd_assoc_cnt;
	struct stdata *smd_headp;
	struct stdata *smd_tailp;
	mon_t *smd_joining;
};

#define SMD_PRIVATE		0x1

extern mon_t streams_mon;
extern mon_t *streams_monp;

extern void str_mp_init(void);
extern toid_t streams_timeout(struct monitor **, strtimeoutfunc_t, int,
			      void *, void *, void *);
extern void streams_untimeout(toid_t);
extern void streams_timeout_interrupt(struct strintr *, int);
extern void streams_service(void *);

extern int streams_delay(int);
extern int mp_streams_delay(struct queue *, int);

extern int streams_copyin(void *, void *, unsigned int, char *, int);
extern int streams_copyout(void *, void *, unsigned int, char *, int);
extern int streams_uiomove(void *, int, enum uio_rw, struct uio *);

struct stdata;
extern void str_globmon_attach(struct stdata *);
extern int str_privmon_attach(struct stdata *, mon_t *);
extern mon_t *str_mon_detach(struct stdata *);
extern mon_t *str_privmon_alloc(void);
extern void str_mon_free(mon_t *);
extern int joinstreams(struct queue *, struct queue **);
extern int useglobalmon(struct queue *);
extern int stream_locked(struct queue *);

extern pl_t plstr;

extern char qqrun;

#define STREAMS_BLOCKQ()
#define STREAMS_UNBLOCKQ()

#define MP_STREAMS_INTERRUPT(m,f,a1,a2,a3) mp_streams_interrupt(m,0,\
							(strintrfunc_t)f,\
							(void *)a1,\
							(void *)a2,\
							(void *)a3)
#define STREAMS_ENAB_INTERRUPT(m,f,a1,a2,a3) mp_streams_interrupt(m,SI_QENAB,\
							(strintrfunc_t)f,\
							(void *)a1, \
							(void *)a2, \
							(void *)a3)
#define MP_STREAMS_TIMEOUT(m,f,a,t) \
		streams_timeout(m,(strtimeoutfunc_t)f,t,a,0,0)
#define MP_STREAMS_TIMEOUT1(m,f,a,t,a1) \
		streams_timeout(m,(strtimeoutfunc_t)f,t,a,a1,0)
#define MP_STREAMS_TIMEOUT2(m,f,a,t,a1,a2) \
		streams_timeout(m,(strtimeoutfunc_t)f,t,a,a1,a2)
#define MP_STREAMS_LOCK(mp)	pmonitor(mp,PZERO,&(mp))
#define MP_STREAMS_UNLOCK(mp)	vmonitor(mp)
/* For backward compatibility */
#define STREAMS_INTERRUPT(f,a1,a2,a3) streams_interrupt((strintrfunc_t)f,\
							(void *)a1,\
							(void *)a2,\
							(void *)a3)
#define STREAMS_TIMEOUT(f,a,t) \
		streams_timeout(&streams_monp,(strtimeoutfunc_t)f,t,a,0,0)
#define STREAMS_TIMEOUT1(f,a,t,a1) \
		streams_timeout(&streams_monp,(strtimeoutfunc_t)f,t,a,a1,0)
#define STREAMS_TIMEOUT2(f,a,t,a1,a2) \
		streams_timeout(&streams_monp,(strtimeoutfunc_t)f,t,a,a1,a2)

#define STREAMS_LOCK()			pmonitor(&streams_mon,PZERO,\
						 &streams_monp)
#define STREAMS_UNLOCK()		vmonitor(&streams_mon)


#define MP_STREAMS_DELAY(q,t)		mp_streams_delay(q,t)
#define STREAMS_DELAY(t)		streams_delay(t)

#define STREAMS_COPYIN(s,d,l,f,c)	streams_copyin(s,d,l,f,c)
#define STREAMS_COPYOUT(s,d,l,f,c)	streams_copyout(s,d,l,f,c)
#define STREAMS_UIOMOVE(c,n,f,p)	streams_uiomove(c,n,f,p)

#define STREAM_LOCKED(q)		stream_locked(q)

/* For MP_STREAMS, spl doesn't provide protection */
#define STR_LOCK_SPL(s)			(s) = (s)
#define STR_UNLOCK_SPL(s)

extern lock_t strhead_monp_lock;

#define LOCK_STRHEAD_MONP(s) { \
	(s) = mutex_spinlock(&strhead_monp_lock); \
}

#define UNLOCK_STRHEAD_MONP(s) { \
	nested_spinunlock(&strhead_monp_lock); \
	splx(s); \
}

#define STRHEAD_LOCK(stpp, stp) \
	{ \
	    mon_t *monp; \
	    int sl; \
	    LOCK_STRHEAD_MONP(sl); \
	    if (((stp) = *(stpp)) && (monp = (stp)->sd_monp)) { \
		/* XXXrs Hope this works. */ \
		spinunlock_pmonitor(&strhead_monp_lock, sl, &(stp)->sd_monp,\
				    PZERO, &(stp)->sd_monp); \
		if (!((stp) = *(stpp))) \
			vmonitor(monp);\
	    } else { \
	    	UNLOCK_STRHEAD_MONP(sl); \
		(stp) = 0; \
	    } \
	}

#define STRQUEUE_LOCK(monpp, locked) \
	{ \
	    mon_t *monp; \
	    int sl; \
	    LOCK_STRHEAD_MONP(sl); \
	    if ((monp = *(monpp))) { \
		/* XXXrs Hope this works. */ \
		spinunlock_pmonitor(&strhead_monp_lock, sl, (monpp),\
				    PZERO, (monpp)); \
	    	if (!(monp = *(monpp))) { \
			vmonitor(monp);\
			(locked) = 0; \
		} else { \
			(locked) = 1; \
		} \
	    } else { \
	    	UNLOCK_STRHEAD_MONP(sl); \
		(locked) = 0; \
	    } \
	}

#define STRHEAD_UNLOCK(stp)		vmonitor((stp)->sd_monp)
#define STRQUEUE_UNLOCK(monpp)		vmonitor(*(monpp))

extern lock_t str_intr_lock;

#define STREAMS_BLOCK_INTR(s) { \
	(s) = mutex_spinlock(&str_intr_lock); \
}

#define STREAMS_UNBLOCK_INTR(s) { \
	mutex_spinunlock(&str_intr_lock, s); \
}

extern lock_t str_resource_lock;

#define LOCK_STR_RESOURCE(s) { \
	(s) = mutex_spinlock(&str_resource_lock); \
}

#define UNLOCK_STR_RESOURCE(s) { \
	mutex_spinunlock(&str_resource_lock, s); \
}

#define LOCK_MONITOR(mp, s) { \
	(s) = mutex_spinlock(&((mp)->mon_lock)); \
}

#define UNLOCK_MONITOR(mp, s) { \
	mutex_spinunlock(&((mp)->mon_lock), s); \
}

#define JOINSTREAMS(qp, qpp)		joinstreams(qp, qpp)


#ifdef __cplusplus
}
#endif

#endif /* __SYS_STRMP_H__ */
