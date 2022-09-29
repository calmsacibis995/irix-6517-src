/*
 * mp.h - Multiprocessor related definitions for symmon
 *
 * $Revision: 1.28 $
 */

#if LANGUAGE_C
#include <sys/types.h>
#include <sys/systm.h>
#include <genpda.h>
#include <libsc.h>
#include <libsk.h>
#endif /* LANGUAGE_C */

/* MASTER PROCESSOR ID */
#define MASTER 	0

#if EVEREST
#include <sys/EVEREST/everest.h>
#ifdef LARGE_CPU_COUNT_EVEREST
#define MAXCPU 128
#else
#define MAXCPU EV_MAX_CPUS
#endif
#endif

#if SN
#include <sys/SN/agent.h>
#if SN0
#include <sys/SN/SN0/arch.h>		/* ensure that MAXCPUS is defined */
#endif
#ifndef MAXCPU
#define MAXCPU MAXCPUS
#endif
#endif

#if IP30
#include <sys/RACER/IP30.h>		/* get MAXCPU definition */
#endif

#if !defined(MAXCPU)
#define MAXCPU	1
#endif

#if MAXCPU > 1
#define MULTIPROCESSOR 1
#endif

#undef cpuid
#ifndef _cpuid
#define _cpuid()	cpuid()
#endif

#if LANGUAGE_C
extern void init_mp(void);

#if !MULTIPROCESSOR
#define ACQUIRE_USER_INTERFACE()
#define RELEASE_USER_INTERFACE(id)
#define TRANSFER_USER_INTERFACE(id)
#define DBG_LOCK()
#define DBG_UNLOCK()
#else
extern void ACQUIRE_USER_INTERFACE(void);
extern void RELEASE_USER_INTERFACE(int);
extern void TRANSFER_USER_INTERFACE(int);
extern void DBG_LOCK(void);
extern void DBG_UNLOCK(void);
#define DBG_LOCK() dbg_spl=splockspl(brkpt_table_lock, splhi);
#define DBG_UNLOCK() spunlockspl(brkpt_table_lock, dbg_spl);
extern int cpuid_to_slot[MAXCPU];
extern int cpuid_to_cpu[MAXCPU];
extern int slave_func(int, void (*)(void *), void *);
#endif	/* MULTIPROCESSOR */

#undef private
#define	private		GEN_PDA_TAB(_cpuid()).gdata

extern void	init_symmon_gpda(void);
extern void	_cont_slave(int);
#if SN
extern void	init_slave_debugblock_ptr(void);
#endif

#define LOCK_USER_INTERFACE() ui_spl=splockspl(user_interface_lock, splhi);
#define UNLOCK_USER_INTERFACE() spunlockspl(user_interface_lock, ui_spl);
#define NO_CPUID -1
extern lock_t user_interface_lock;
extern int ui_spl;

#endif
