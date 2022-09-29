/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __SYS_SYSMP_H__
#define __SYS_SYSMP_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 3.109 $"

#include <sgidefs.h>

/*
 * sysmp.h - defined/structs for sysmp() - multi-processor specific calls
 *
 * Format:
 *	sysmp(opcode, [opcode_specific_args])
 */

/* possible opcodes */
#define MP_NPROCS	1	/* # processor in complex */
#define MP_NAPROCS	2	/* # active processors in complex */
#define MP_SPACE	3	/* set space sharing */
#define MP_ENABLE	4	/* enable a processor OBSOLETE */
#define MP_DISABLE	5	/* disable a processor OBSOLETE */
#define MP_KERNADDR	8	/* get address of kernel structure */
#define MP_SASZ		9
#define MP_SAGET	10	/* get system activity for all cpus summed */
#define MP_SCHED	13	/* scheduler control calls */
#define MP_PGSIZE	14	/* get system page size */
#define MP_SAGET1	15	/* get system activity for a single CPU */
#define MP_EMPOWER	16	/* allow processor to sched any process */
#define MP_RESTRICT	17	/* restrict processor to mustrun processes */
#define MP_CLOCK	18	/* processor becomes (software) clock handler */
#define MP_MUSTRUN	19	/* assign process to this processor */
#define MP_RUNANYWHERE	20	/* let process run on any processor (default) */
#define MP_STAT		21	/* return processor status */
#define MP_ISOLATE	22	/* isolate a processor from inter-cpu intr */
#define MP_UNISOLATE	23	/* un-isolate a processor */
#define MP_PREEMPTIVE	24	/* turn clock back on an isolated processor */
#define MP_NONPREEMPTIVE  25	/* turn clock off of an isolated processor */
#define MP_FASTCLOCK	26	/* cpu becomes the sw fast clock handler */
#define MP_CLEARNFSSTAT	28	/* clear nfsstat */
#define MP_GETMUSTRUN   29      /* return current mustrun for current process */	
#define MP_MUSTRUN_PID	30	/* assign process to this processor */
#define MP_RUNANYWHERE_PID	31	/* let process run on any processor (default) */
#define MP_GETMUSTRUN_PID   32      /* return current mustrun for process */	

#define MP_CLEARCFSSTAT     33

#define MP_MISER_GETREQUEST    36   /* interface for request processing */
#define MP_MISER_SENDREQUEST   37   /* send a request to miser */
#define MP_MISER_RESPOND       38   /* send a request to miser */
#define MP_MISER_GETRESOURCE   39   /* send a request to miser */
#define MP_MISER_SETRESOURCE   40   /* send a request to miser */
#define MP_MISER_CHECKACCESS   41   /* send a request to miser */
/*
 * NUMA-specific calls.
 */
#define	MP_NUMNODES		42	/* Number of nodes in a system */
#define	MP_NUMA_GETDISTMATRIX	43	/* get node-node distance matrix */
#define	MP_NUMA_GETCPUNODEMAP	44	/* get cpu->node mapping table */

/* Disabling/enabling cpus and getting cpu path */
#define MP_DISABLE_CPU         45      	/* disable cpu on reboot */
#define MP_ENABLE_CPU          46      	/* don't disable cpu on reboot */
#define MP_CPU_PATH	       47	/* get hwgraph path of cpu */

/* Kernel lock statistics - (SGI use only */
#define MP_KLSTAT	       48	/* kernel lock statistics (SGI only) */

/* kernel address sub-opcodes */
#define MPKA_VAR		2	/* kernel vars */
#define MPKA_SWPLO		3	/* swap lo */
#define MPKA_INO		4	/* inode table */
#define	MPKA_SEMAMETER		7	/* spin lock metering flag */
#define MPKA_PROCSIZE		9	/* size of proc struct */
#define MPKA_TIME		10	/* time */
#define MPKA_MSG		11	/* msgque */
#define MPKA_MSGINFO		14	/* msginfo */
#define	MPKA_SPLOCKMETER	17	/* spin lock metering flag */
#define	MPKA_SPLOCKMETERTAB	18	/* k_splockmeter table */
#define MPKA_AVENRUN		19	/* load average */
#define MPKA_PHYSMEM		20	/* &physmem */
#define MPKA_KPBASE		21	/* &kpbase */
#define MPKA_PFDAT		22	/* &pfdat */
#define MPKA_FREEMEM		23	/* &freemem */
#define MPKA_USERMEM		24	/* &usermem */
#define MPKA_PDWRIMEM		25	/* &dchunkpages */
#define MPKA_BUFMEM		26	/* &bufmem */
#define MPKA_BUF		27	/* buf */
#define MPKA_CHUNKMEM		30	/* &chunkpages */
#define MPKA_MAXCLICK		31	/* &maxclick */
#define MPKA_PSTART		32	/* &start-of-physical-memory */
#define MPKA_TEXT		33	/* text[] */
#define MPKA_ETEXT		34	/* etext[] */
#define MPKA_EDATA		35	/* edata[] */
#define MPKA_END		36	/* end[] */
#define MPKA_SYSSEGSZ		37	/* &syssegsz */
#define MPKA_SEM_MAC		38	/* sem Mandatory Access Control */
#define MPKA_MSG_MAC		40	/* msg Mandatory Access Control */
#define MPKA_BSD_KERNADDRS	41	/* addresses needed by netstat */
#define MPKA_MAX		41	/* Max entries, >>End-of-Table<< */
#ifdef _KERNEL
extern caddr_t sysmp_mpka[];
#endif

/* system activities sub-opcodes */
#define MPSA_SINFO	1	/* get added-up sysinfo */
#define MPSA_MINFO	2	/* get minfo */
#define MPSA_DINFO	3	/* get dinfo */
#define MPSA_SERR	4	/* get syserr */
#define MPSA_NCSTATS	5	/* get ncstats (pathname cache) */
#define MPSA_EFS	6	/* get efs stats */
#define MPSA_RMINFO	8	/* real memory info */
#define MPSA_BUFINFO	9	/* buffer cache info */
#define MPSA_RUNQ	10	/* runq root data structure */
#define MPSA_DISPQ	11	/* a particular dispatching queue */
#define MPSA_VOPINFO	13	/* vnode operations */
#define MPSA_TCPIPSTATS 14	/* TCP/IP stats for a particular processor */
#define MPSA_RCSTAT 	15	/* nfs rcstat for a processor */
#define MPSA_CLSTAT 	16	/* nfs clstat for a processor */
#define MPSA_RSSTAT 	17	/* nfs rsstat for a processor */
#define MPSA_SVSTAT 	18	/* nfs svstat for a processor */
#define	MPSA_XFSSTATS	20	/* get xfs stats */
#define	MPSA_CLSTAT3	21	/* nfs3 clstat for a processor */
#ifdef TILES_TO_LPAGES
#define MPSA_TILEINFO	22	/* kernel tile info */
#endif /* TILES_TO_LPAGES */
#define MPSA_CFSSTAT	23	/* CacheFS stats */
#define MPSA_SVSTAT3	24
#define MPSA_NODE_INFO	25	/* Per node info */
#define MPSA_LPGSTATS	26	/* Large page statistics */
#define MPSA_SHMSTAT	27	/* Shared memory stats */
#define MPSA_KSYM 	28	/* Arbitrary kernel object */ 
#define MPSA_MSGQUEUE	29	/* msgque list */
#define MPSA_SEM	30	/* sema list */
#define MPSA_SOCKSTATS  31	/* System wide socket statistics */
#define MPSA_SINFO_CPU  32	/* cpu portion of sysinfo */
#define MPSA_STREAMSTATS 33	/* Stream statistics for one/all processors */
#define	MPSA_MAX	33	/* Max entries, >>End-of-Table<< */

/* scheduler control call sub-opcodes */
#define MPTS_RTPRI	1	/* give a process a real-time priority */
#define MPTS_SLICE	2	/* alter a process time slice */
#define MPTS_RENICE	3	/* alter the nice value */

/* getpriority and setpriority emulation */
#define MPTS_RENICE_PROC 4	/* alter nice value of a process */
#define MPTS_RENICE_PGRP 5	/* alter nice value of a process group */
#define MPTS_RENICE_USER 6	/* alter nice value of a user */
#define MPTS_GTNICE_PROC 7	/* get nice value of process */
#define MPTS_GTNICE_PGRP 8	/* get nice value of process group */
#define MPTS_GTNICE_USER 9	/* get nice value of user */

/* More scheduler control call subcodes */
#define MPTS_SETHINTS	12	/* create a user scheduling hints structure */
#define MPTS_SCHEDMODE	13	/* set scheduling mode */
#define MPTS_SETMASTER	14	/* set master proc in a share group */

/* Scheduler Control Inquiry Codes */
#define MPTS_GETRTPRI	21	/* Read a process' current rt priority */

#define MPTS_EVENT	23	/* wait for an event */

/*
 * Old frame scheduler calls.
 * We return "not supported"
 * for calls issued from old
 * binaries using any of these
 * calls.
 */
#define MPTS_OLDFRS_CREATE    30
#define MPTS_OLDFRS_ENQUEUE   31
#define MPTS_OLDFRS_DEQUEUE   32
#define MPTS_OLDFRS_START     33
#define MPTS_OLDFRS_STOP      34
#define MPTS_OLDFRS_DESTROY   35
#define MPTS_OLDFRS_YIELD     36
#define MPTS_OLDFRS_JOIN      37
#define MPTS_OLDFRS_INTR      38
#define MPTS_OLDFRS_GETQUEUELEN 39
#define MPTS_OLDFRS_READQUEUE 40
#define MPTS_OLDFRS_PREMOVE   41
#define MPTS_OLDFRS_PINSERT   42
#define MPTS_OLDFRS_RESUME    43
#define MPTS_OLDFRS_SETATTR   44
#define MPTS_OLDFRS_GETATTR   45

/*
 * Frame Scheduler Calls
 */
#define MPTS_FRS_CREATE    50   /* create a frame scheduler object */
#define MPTS_FRS_ENQUEUE   51   /* enqueue process on frame sched queue */
#define MPTS_FRS_DEQUEUE   52   /* dequeue process from frame sched queue */
#define MPTS_FRS_START     53   /* start frame scheduler */
#define MPTS_FRS_STOP      54   /* stop frame scheduler */
#define MPTS_FRS_DESTROY   55   /* destruct frame scheduler */
#define MPTS_FRS_YIELD     56   /* yield to next process on frame sched queue */
#define MPTS_FRS_JOIN      57   /* join a frame scheduler */ 
#define MPTS_FRS_INTR      58   /* generate an event interrupt */
#define MPTS_FRS_GETQUEUELEN 59 /* get minor frame queue length */
#define MPTS_FRS_READQUEUE 60   /* get a list of the processes enqueued on a frame */
#define MPTS_FRS_PREMOVE   61   /* remove a specific process from a frame queue */
#define MPTS_FRS_PINSERT   62   /* insert a process on a frame queue */
#define MPTS_FRS_RESUME    63   /* resume frame scheduler after stop */
#define MPTS_FRS_SETATTR   64   /* set frame scheduler attributes */
#define MPTS_FRS_GETATTR   65   /* get frame scheduler attributes */
#define MPTS_AFFINITY_ON   66   /* turns on affinity */
#define MPTS_AFFINITY_OFF  67	/* turns off affinity */
#define MPTS_AFFINITY_GET  68	/* returns current affinity status */ 

#define MPTS_MISER_QUANTUM 69   /* apply miser action to quantum */
#define MPTS_MISER_CPU     70	/* apply miser action to cpus */
#define MPTS_MISER_MEMORY  71   /* apply miser action to memory */
#define MPTS_MISER_BIDS	   72   /* get active bids */
#define MPTS_MISER_JOB	   73	/* get information about job */ 

/* processor set manipulation subcodes */
#define MPPS_CREATE	1	/* create a new processor set */
#define MPPS_DELETE	2	/* delete an existing processor set */
#define MPPS_ADD	3	/* add processors to a set */
#define MPPS_REMOVE	4	/* remove processors from a set */
#define	MPPS_MAX	4	/* Max entries, >>End-of-Table<< */

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
/* struct for real memory info */
struct rminfo {
	__uint32_t	freemem;	/* current pages free memory */
	__uint32_t	availsmem;	/* available real and swap memory */
	__uint32_t	availrmem;	/* available real memory */
	__uint32_t	bufmem;		/* attached to metadata cache */
	__uint32_t	physmem;	/* total real memory */
	__uint32_t	dchunkpages;	/* delwri pages */
	__uint32_t	pmapmem;	/* pages in pmap */
	__uint32_t	strmem;		/* pages of streams resources */
	__uint32_t	chunkpages;	/* chunk pages */
	__uint32_t	dpages;		/* dirty, unattached pages */
	__uint32_t	emptymem;	/* free pages not caching data */
	__uint32_t	ravailrmem;	/* unlocked real memory */
};

#ifdef TILES_TO_LPAGES
struct tileinfo {
	__int32_t	tavail;		/* tiles available: no locked pgs */
	__int32_t	avfree;		/* free pages in tavail */
	__int32_t	tfrag;		/* tiles fragmented: some locked pgs */
	__int32_t	fraglock;	/* locked pages in tfrags */
	__int32_t	fragfree;	/* free pages in tfrags */
	__int32_t	tfull;		/* tiles full: all locked pgs */
	__int32_t	ttile;		/* tiles allocated via tile_alloc */
	__int32_t	pglocks;	/* tile page locks */
	__int32_t	tallocmv;	/* pgs relocated for tile_alloc */
	__int32_t	tiledmv;	/* pgs relocated by tiled */
};
#endif /* TILES_TO_LPAGES */

#ifdef _KERNEL
#include <sys/kthread.h>
extern int mp_mustrun(int, int, int, int);
extern void mp_runthread_anywhere(kthread_t*);
extern int mp_runanywhere(int, int, int);
extern int mp_isolate(int);
extern int mp_unisolate(int);
#ifdef TILES_TO_LPAGES
extern struct tileinfo tileinfo;
#endif /* TILES_TO_LPAGES */
#endif /* KERNEL */

#ifndef _KERNEL
#include <stddef.h>
extern ptrdiff_t sysmp(int, ...);
#endif /* !_KERNEL */
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SYS_SYSMP_H__ */
