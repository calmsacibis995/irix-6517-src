/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_SN_LAUNCH_H__
#define __SYS_SN_LAUNCH_H__

#ident "$Revision: 1.10 $"

#include <sys/SN/addrs.h>

/*
 * The launch data structure resides at a fixed place in each node's memory
 * and is used to communicate between the master processor and the slave
 * processors.
 *
 * The master stores launch parameters in the launch structure
 * corresponding to a target processor that is in a slave loop, then sends
 * an interrupt to the slave processor.  The slave calls the desired
 * function, then returns to the slave loop.  The master may poll or wait
 * for the slaves to finish.
 *
 * There is an array of launch structures, one per CPU on the node.  One
 * interrupt level is used per local CPU.
 */

#define LAUNCH_MAGIC		0xaddbead2addbead3
#define LAUNCH_SIZEOF		0x100

#define LAUNCH_OFF_MAGIC	0x00	/* Struct offsets for assembly      */
#define LAUNCH_OFF_BUSY		0x08
#define LAUNCH_OFF_CALL		0x10
#define LAUNCH_OFF_CALLC	0x18
#define LAUNCH_OFF_CALLPARM	0x20
#define LAUNCH_OFF_STACK	0x28
#define LAUNCH_OFF_GP		0x30
#define LAUNCH_OFF_BEVUTLB	0x38
#define LAUNCH_OFF_BEVNORMAL	0x40
#define LAUNCH_OFF_BEVECC	0x48

#define LAUNCH_STATE_DONE	0	/* Return value of LAUNCH_POLL      */
#define LAUNCH_STATE_SENT	1
#define LAUNCH_STATE_RECD	2

/*
 * The launch routine is called only if the complement address is correct.
 *
 * Before control is transferred to a routine, the compliment address
 * is zeroed (invalidated) to prevent an accidental call from a spurious
 * interrupt.
 *
 * The slave_launch routine turns on the BUSY flag, and the slave loop
 * clears the BUSY flag after control is returned to it.
 */

#ifdef _LANGUAGE_C

typedef int launch_state_t;
typedef void (*launch_proc_t)(__int64_t call_parm);

typedef struct launch_s {
	volatile __int64_t magic;	/* Magic number                     */
	volatile __int64_t busy;	/* Slave currently active           */
	volatile launch_proc_t call_addr;	/* Func. for slave to call  */
	volatile __int64_t call_addr_c;	/* 1's complement of call_addr      */
	volatile __int64_t call_parm;	/* Single parm passed to call       */
	volatile void *stack_addr;	/* Stack pointer for slave function */
	volatile void *gp_addr;		/* Global pointer for slave func.   */
	volatile addr_t *bevutlb;	/* Address of bev utlb ex handler   */
	volatile addr_t *bevnormal;	/* Address of bev normal ex handler */
	volatile addr_t *bevecc;	/* Address of bev cache err handler */
	volatile char	 pad[160];	/* Pad to LAUNCH_SIZEOF             */
} launch_t;

/*
 * PROM entry points for launch routines are determined by IP27prom/start.s
 */

#if defined (SN0)
#define LAUNCH_SLAVE	(*(void (*)(int nasid, int cpu, \
				    launch_proc_t call_addr, \
				    __int64_t call_parm, \
				    void *stack_addr, \
				    void *gp_addr)) \
			 IP27PROM_LAUNCHSLAVE)

#define LAUNCH_WAIT	(*(void (*)(int nasid, int cpu, int timeout_msec)) \
			 IP27PROM_WAITSLAVE)

#define LAUNCH_POLL	(*(launch_state_t (*)(int nasid, int cpu)) \
			 IP27PROM_POLLSLAVE)

#define LAUNCH_LOOP	(*(void (*)(void)) \
			 IP27PROM_SLAVELOOP)

#define LAUNCH_FLASH	(*(void (*)(void)) \
			 IP27PROM_FLASHLEDS)

#elif defined (SN1)

#define LAUNCH_SLAVE	(*(void (*)(int nasid, int cpu, \
				    launch_proc_t call_addr, \
				    __int64_t call_parm, \
				    void *stack_addr, \
				    void *gp_addr)) \
			 IP33PROM_LAUNCHSLAVE)

#define LAUNCH_WAIT	(*(void (*)(int nasid, int cpu, int timeout_msec)) \
			 IP33PROM_WAITSLAVE)

#define LAUNCH_POLL	(*(launch_state_t (*)(int nasid, int cpu)) \
			 IP33PROM_POLLSLAVE)

#define LAUNCH_LOOP	(*(void (*)(void)) \
			 IP33PROM_SLAVELOOP)

#define LAUNCH_FLASH	(*(void (*)(void)) \
			 IP33PROM_FLASHLEDS)

#endif

#ifdef _STANDALONE

launch_t       *launch_get(int nasid, int cpu);
launch_t       *launch_get_current(void);
void		launch_loop(void);
void		launch_slave(int nasid, int cpu,
			     launch_proc_t call_addr,
			     __int64_t call_parm,
			     void *stack_addr,
			     void *gp_addr);
int		launch_wait(int nasid, int cpu, int timeout_msec);
launch_state_t	launch_poll(int nasid, int cpu);

#endif /* _STANDALONE */

#endif /* _LANGUAGE_C */

#endif /* __SYS_SN_LAUNCH_H__ */
