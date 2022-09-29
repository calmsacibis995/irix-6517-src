
/*
** Basic definitions and data structures for the compiler MP library.
*/

/*************************************************************************/
/* Symbolic constants and useful values (useable by asm routines as well */
/*************************************************************************/
#define MAX_THREADS  64
/* Currently, MAX_THREADS is limited to CACHE_LINE_SIZE - 8 (see */
/* c4DataType definition) */

/* Threads taken in groups of 4 ( == sizeof(int32)) */
#define MAX_GROUPS  (MAX_THREADS / 4)

/* This many threads needs extra space in the shm arena */
#define ARENA_SIZE (128*1024)

/* Biggest line size on any machine (i.e. Everest) */
#define      CACHE_LINE_SIZE 128
#define LOG2_CACHE_LINE_SIZE   7

/* This is a desirable alignment because the R4400 maps 2 4K pages into
** one tlb slot.  So an 8K alignment increases the chances that we will
** only consume one tlb (admittedly a minor point, but why not).
*/
/* #define      DESIRABLE_ALIGNMENT 8192 */
/* #define LOG2_DESIRABLE_ALIGNMENT   13 */
/* As of this writing, the assembler only permits alignment up to 4K */
#define      DESIRABLE_ALIGNMENT 4096
#define LOG2_DESIRABLE_ALIGNMENT   12


/* "join" area info */
#define      NUM_JOIN_FLAGS_PER_LINE 4  /* Currently must be a power of 2 */
#define LOG2_NUM_JOIN_FLAGS_PER_LINE 2
#define NUM_JOIN_LINES  (MAX_THREADS/NUM_JOIN_FLAGS_PER_LINE)

/* Size of the taskCommon data structure */
#define TCOM_SIZE  ((8 + NUM_JOIN_LINES + MAX_THREADS)*CACHE_LINE_SIZE)

/* Info for the 64bit counters that control the syncs */
#define INITIAL_CONSTRUCT_INSTANCE 2
#define INITIAL_FLAG_VALUE (1024LL * 1024LL *1024LL * 1024LL * 1024LL *1024LL)
#define UNUSED_FLAG_VALUE  1
/* Values from INITIAL_CONSTRUCT_INSTANCE upto INITIAL_FLAG_VALUE are used
** for controling PCF style constucts inside of pregions.  Values greater
** than INITIAL_FLAG_VALUE control "outer" parallelism (e.g. doacross and
** the entry/exit from a PCF style region).  It is assumed the values
** never collide (we don't check).
*/


/* Number of pregion construct cb's initially allocated */
#define INITIAL_CB_ALLOCATION  100


/* The various values for the "state" byte */
#define INITIALIZED	0x01
#define USER_BLOCKED	0x02
#define MULTI_PROCESSING	0x04
#define PROFILE_MODE	0x08
#define SOFT_LOCKS	0x10
#define NORMAL_STATE (INITIALIZED)


/*************************************************************************/



#ifdef _LANGUAGE_C

#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <ulocks.h>

/* pollute my name space a bit with nicer names */
typedef __int32_t int32;
typedef __uint32_t uint32;
typedef __int64_t int64;
typedef __uint64_t uint64;
typedef unsigned char uint8;
typedef   signed char  int8;

typedef uint8 boolean;
#define TRUE (1==1)
#define FALSE (1==0)

/* An unsigned int that is the same size as a pointer. */
/* Must work for both 32 and 64 bit worlds! (note that ptrdiff_t is signed) */
typedef unsigned long int   uint_ptr;




/* Max # of contructs within a parallel region. */
/* This really only affects mpc style construct locks; control blocks
** are always allocated dynamically.
*/
#define MAX_CONSTRUCTS 256

#ifndef min
#define min(x,y) (((x)<(y)) ? (x) : (y))
#endif
#ifndef max
#define max(x,y) (((x)>(y)) ? (x) : (y))
#endif

/* For 32/64bit compatibility.  This type ensures we always allocate
** an aligned 64bit space for a pointer.
*/
typedef struct {
	union {
		volatile void *ptr;
		uint64 padding;
	} data;
} vptr_rec;
typedef struct {
	union {
		void *ptr;
		uint64 padding;
	} data;
} ptr_rec;


typedef char cacheLineType[CACHE_LINE_SIZE];


/* A line in the "join" area */
struct joinDataType {
        volatile uint64 flag[NUM_JOIN_FLAGS_PER_LINE];
};
struct joinLineType {
        union {
                struct joinDataType data;
                cacheLineType padding;
        } node;
};



/* Types for X3H5 parallel regions ("cb" means "control block") */
typedef struct {
	/* boolean done;  Is this used ?? */
	/* uint8 region_threads; */

	/* All the stuff that used to go here has slowly migrated to
	** other places.
	*/
	uint64 dummy;
} region_cb_type;


typedef struct {
	ulock_t controlLock; /* Used by Power C interface */

	/* The instance most recently started (possibly still going) */
	volatile uint64 construct_instance;
	/* The most recent instance that has had all work assigned */
	volatile uint64 all_allocated_instance;
	/* Note that this is NOT the same as "complete"; the assigned
	** work may still be executing.  What it actually signals
	** is that the given instance is now done using the construct_cb,
	** and so the cb is free for another instance to use.
	*/


	/* sched_type kept in thread_cb, not construct_cb */
	volatile int64 base;	/* "current base" for dynamic schedules */
	volatile int64 tripcount;	/* in "chunks" for dynamic schedules */
	volatile int64 stride;	/* "stride between chunks" for dyn sched */
	uint64 original_tripcount;

	/* info for dynamic schedules */
	uint32 full_chunk_size;
	uint32 last_chunk_size;

	/* "zero" is volatile so that when a store to it appears first in a
	** basic block (to get exclusive access) the store won't be moved.
	*/
	volatile uint8 zero;

	/* Used for enter/exit gate */
	volatile uint8 thread_count;

	/* info for gss schedules */
	uint8 shift_amount;
	boolean correction_needed;

} construct_cb_type;


typedef struct {
	union {
		region_cb_type region_cb;
		construct_cb_type construct_cb;
		cacheLineType padding;
	} data;
} aligned_cb_type;





/* Definition of the mp data area */
typedef struct {

	/* first cache line */
	/* Info used by the slave threads for "outer level" paralleism
	** (doacross or pregion).
	*/

	/* Flag that all slaves spin on while waiting for a parallel region */
	volatile uint64 startFlag;

	/* Since 64bit values may not be written atomically, we set this value
	** first, then set the startFlag.  Even if a slave sees a partial
	** update of startFlag, this will reliably hold the full value
	*/
	volatile uint64 reliableStartFlagValue;

	/* Info about the parallel region */
	vptr_rec proc;
	vptr_rec staticLink;

	volatile int64 base;
	volatile int64 stride;
	volatile uint64 totalTripcount;
	volatile uint64 chunkSize; /* Also trips/threads for F_SIMPLE_DOALL */

	volatile uint8 currentNumthreads;
	volatile uint8 schedType;
	volatile uint8 remainder; /* rem(trips/threads) for F_SIMPLE_DOALL */
	volatile uint8 interfaceType;  /* 32 or 64bit */
	volatile uint8 unused[4];

	/* Used to make memory consistent */
	vptr_rec memorySyncLock; /* ulock_t memorySyncLock */
	/* EVent Counter location */
	vptr_rec evcPtr;

	/* Pointer to the array of construct cb's for a
	** parallel region (only used by pregions, not by doacross).
	*/
	vptr_rec global_cb_array_ptr;
	volatile uint64 construct_instance_counter;

} c1DataType;


typedef struct {

	/* second cache line */

	/* Info that is (usually) looked at or changed only by
	** the master thread.
	*/
	volatile uint32 zero;
	volatile uint32 state;
	volatile uint32 suggestedNumthreads;
	volatile uint32 previousNumthreads;
	volatile uint32 maxNumthreads;

	volatile uint32 evcValue;
	uint64 flagValue;

	/* These make the special case asm code a tiny bit easier */
	ptr_rec waitEntry;
	ptr_rec forkEntry;
	ptr_rec wakeEntry;
	ptr_rec resetEntry;

	ptr_rec masters_cb_array_ptr;
	uint64 masters_construct_instance_counter;
	uint32 max_num_constructs;

} c2DataType;




typedef struct {

	/* Third cache line */
	/* Info for dynamic/gss scheduling */

	/* Almost everything that used to be in this line is now obsolete */

	/* volatile uint8 d_zero1; */
	/* volatile uint8 d_unused1; */
	/* volatile uint8 shiftAmount; */	/* Used in gss computation */
	/* volatile boolean correctionNeeded; */ /* ditto */

	/* volatile int64 currentBase; */
	/* volatile int64 remainingTrips; */

	ptr_rec iterationLockHandle; /* ulock_t iterationLockHandle */
	ptr_rec internalLockHandle; /* ulock_t internalLockHandle */

} c3DataType;


typedef struct {

	/* Forth and fifth cache lines */
	/* Flags to deal with auto-blocking */
	volatile int32 itersTillBlock;
	volatile int32 iterIncrement;
	union {
		volatile boolean intendsToBlock[MAX_THREADS];
		volatile uint32 intendsToBlockGroup[MAX_GROUPS];
	} autoBlockFlags;
} c4DataType;
typedef struct {
	union {
		volatile boolean isNowUnblocked[MAX_THREADS];
		volatile uint32 isNowUnblockedGroup[MAX_GROUPS];
	} autoUnblockFlags;
} c5DataType;





/* Special locks for "user friendly" locking routines */
typedef struct {
	ulock_t	userLock;
	barrier_t *userBarrier;
} userSyncType;
extern userSyncType __mp_userSync;

/*
** Definitions for "Control Blocks" used to control parallelism.
**
** Each *thread* has its own cb used for holding state info.  Some
** info is duplicated here is well for convienience.
**
** Each *region* has a cb for holding info about that region.
**
** Each *construct* in a region has a cb for holding info about
** that construct.
**
** The region cb is kept in the place where "construct cb #0"
** would go (this makes it easier to find things).
**
** For example, when using dynamic scheduling, the constuct cb
** has info about the current state of the iterations, and all
** threads use it.  With interleaved scheduling, each thread
** keeps track of where it is on its own.
**
** (Minor note: we let "current" tripcounts be "int" rather than "uint"
** because it is sometimes convienient (e.g. during dynamic scheduling)
** to let them go negative during the course of calculations.  Note that
** "original" tripcounts are uint.)
*/

/* Scopes for locking with a parallel region */
#define GLOBAL_LOCK  1  /* Whole program */
#define REGION_LOCK  2  /* Whole region */
#define BLOCK_LOCK   3  /* This construct */

extern ulock_t __mp_global_lock;
extern ulock_t __mp_region_lock[MAX_THREADS];
extern ulock_t __mp_construct_lock[MAX_CONSTRUCTS];




typedef struct {
	volatile uint64 thread_instance; /* volatile for __mp_exit_gate */
	uint64 gate_instance; /* used for enter/exit gate */

	int64 base;	/* This is "current base" for interleave */
	int64 tripcount;	/* This is "remaining chunks" for interleave */
	int64 stride;	/* This is "inter-chunk-distance" for interleave */

	volatile uint64 *done_flag_addr;
	aligned_cb_type *my_copy_cb_array_ptr;

	/* info for interleave schedules */
	uint32 full_chunk_size;
	uint32 last_chunk_size;

        volatile uint8 zero;
	uint8 sched_type;
	boolean done;
	boolean i_do_last_iteration;
	uint8 interface_type;
	uint8 num_threads;
} thread_cb_type;

typedef struct {
	union {
		thread_cb_type thread_cb;
		cacheLineType padding;
	} data;
} aligned_thread_cb_type;




/* Info about GSS scheduling for different numbers of threads */
typedef struct gss_info {
	uint8 shiftAmount;
	boolean correctionNeeded;
} gss_info_type;
extern gss_info_type __mp_gss_info[MAX_THREADS+1];



/* Type for "ordinal" synchronization */
typedef struct {
        volatile int32 ord_value;
        int32 ord_increment;
} ordinal_type;



/* Random variables needed in more than one place */
extern uint32 __mp_runtime_sched_type;
extern uint32 __mp_runtime_chunk_size;



/* Random defines */

/* Works as long as uninitialized PRDA is set to zero */
#define M_my_threadnum (PRDALIB->auto_mp_id)


/* If we are on a machine without strongly consistent memory (e.g. IP5, IP7)
** this brings memory up to date.  On machines that don't need it (e.g.
** everything else) we arrange for it to be harmless and fast.
*/
#define SYNC_MEM  (*((volatile int32 *) M_memorySyncLock))



/********************************************************************/
/* The major MP data structure */
/* Pretty much everything we need is all crammed into this structure.
** This gives us a single thing to dereference (which makes -xgot
** cheaper), and ensures that everything gets the alignment we want by
** forcing the alignment of just this one thing.
*/

/* If more cache lines are added to this structure, you also
** need to update the TCOM_SIZE #define
*/
typedef struct {

	union {
		c1DataType c1Data;
                cacheLineType padding;
	} c1;

	union {
		c2DataType c2Data;
                cacheLineType padding;
	} c2;

	union {
		c3DataType c3Data;
                cacheLineType padding;
	} c3;

	union {
		c4DataType c4Data;
                cacheLineType padding;
	} c4;

	union {
		c5DataType c5Data;
                cacheLineType padding;
	} c5;

	/* Having this copy can sometimes save a bus xact, and avoids
	** the potential problem of cache line conflicts in a direct
	** mapped cache when doing the "fast write" to c1.
	*/
	union {
		c1DataType c1Data;
                cacheLineType padding;
	} c6;

	union {
		/* Used by __mp_barrier */
		volatile uint64 barrier_flag;
                cacheLineType padding;
	} c7;

	/* c8: a "pre-allocated" construct_cb */
	aligned_cb_type  single_construct_cb;


	struct joinLineType joinArea[NUM_JOIN_LINES];

	aligned_thread_cb_type thread_cb[MAX_THREADS];

} taskCommonStructType;


extern taskCommonStructType __mp_taskCommon;

/* Field access macros */

#define M_syncData		(__mp_taskCommon.c1.c1Data)
#define M_startFlag		(__mp_taskCommon.c1.c1Data.startFlag)
#define M_reliableStartFlagValue (__mp_taskCommon.c1.c1Data.reliableStartFlagValue)
#define M_currentNumthreads	(__mp_taskCommon.c1.c1Data.currentNumthreads)
#define M_threadOfLastIter	(__mp_taskCommon.c1.c1Data.threadOfLastIter)
#define M_schedType		(__mp_taskCommon.c1.c1Data.schedType)
#define M_interfaceType	(__mp_taskCommon.c1.c1Data.interfaceType)
#define M_remainder		(__mp_taskCommon.c1.c1Data.remainder)
#define M_chunkSize		(__mp_taskCommon.c1.c1Data.chunkSize)
#define M_proc		(__mp_taskCommon.c1.c1Data.proc.data.ptr)
#define M_staticLink		(__mp_taskCommon.c1.c1Data.staticLink.data.ptr)
#define M_base		(__mp_taskCommon.c1.c1Data.base)
#define M_stride		(__mp_taskCommon.c1.c1Data.stride)
#define M_totalTripcount	(__mp_taskCommon.c1.c1Data.totalTripcount)
#define M_memorySyncLock	(__mp_taskCommon.c1.c1Data.memorySyncLock.data.ptr)
#define M_evcPtr		(__mp_taskCommon.c1.c1Data.evcPtr.data.ptr)
#define M_global_cb_array_ptr	(__mp_taskCommon.c1.c1Data.global_cb_array_ptr.data.ptr)
#define M_construct_instance_counter	(__mp_taskCommon.c1.c1Data.construct_instance_counter)

#define M_copy_syncData		(__mp_taskCommon.c6.c1Data)
#define M_copy_startFlag		(__mp_taskCommon.c6.c1Data.startFlag)
#define M_copy_reliableStartFlagValue (__mp_taskCommon.c6.c1Data.reliableStartFlagValue)
#define M_copy_currentNumthreads	(__mp_taskCommon.c6.c1Data.currentNumthreads)
#define M_copy_threadOfLastIter	(__mp_taskCommon.c6.c1Data.threadOfLastIter)
#define M_copy_schedType		(__mp_taskCommon.c6.c1Data.schedType)
#define M_copy_interfaceType		(__mp_taskCommon.c6.c1Data.interfaceType)
#define M_copy_remainder		(__mp_taskCommon.c6.c1Data.remainder)
#define M_copy_chunkSize		(__mp_taskCommon.c6.c1Data.chunkSize)
#define M_copy_proc		(__mp_taskCommon.c6.c1Data.proc.data.ptr)
#define M_copy_staticLink		(__mp_taskCommon.c6.c1Data.staticLink.data.ptr)
#define M_copy_base		(__mp_taskCommon.c6.c1Data.base)
#define M_copy_stride		(__mp_taskCommon.c6.c1Data.stride)
#define M_copy_totalTripcount	(__mp_taskCommon.c6.c1Data.totalTripcount)
#define M_copy_memorySyncLock	(__mp_taskCommon.c6.c1Data.memorySyncLock.data.ptr)
#define M_copy_evcPtr		(__mp_taskCommon.c6.c1Data.evcPtr.data.ptr)
#define M_copy_global_cb_array_ptr (__mp_taskCommon.c6.c1Data.global_cb_array_ptr.data.ptr)
#define M_copy_construct_instance_counter (__mp_taskCommon.c6.c1Data.construct_instance_counter)


#define M_zero		(__mp_taskCommon.c2.c2Data.zero)
#define M_state		(__mp_taskCommon.c2.c2Data.state)
#define M_suggestedNumthreads (__mp_taskCommon.c2.c2Data.suggestedNumthreads)
#define M_previousNumthreads (__mp_taskCommon.c2.c2Data.previousNumthreads)
#define M_maxNumthreads	(__mp_taskCommon.c2.c2Data.maxNumthreads)
#define M_evcValue		(__mp_taskCommon.c2.c2Data.evcValue)
#define M_flagValue		(__mp_taskCommon.c2.c2Data.flagValue)
#define M_waitEntry		(__mp_taskCommon.c2.c2Data.waitEntry.data.ptr)
#define M_forkEntry		(__mp_taskCommon.c2.c2Data.forkEntry.data.ptr)
#define M_wakeEntry		(__mp_taskCommon.c2.c2Data.wakeEntry.data.ptr)
#define M_resetEntry		(__mp_taskCommon.c2.c2Data.resetEntry.data.ptr)
#define M_masters_cb_array_ptr (__mp_taskCommon.c2.c2Data.masters_cb_array_ptr.data.ptr)
#define M_masters_construct_instance_counter (__mp_taskCommon.c2.c2Data.masters_construct_instance_counter)
#define M_max_num_constructs (__mp_taskCommon.c2.c2Data.max_num_constructs)

#define M_d_zero1		(__mp_taskCommon.c3.c3Data.d_zero1)
#define M_d_unused1		(__mp_taskCommon.c3.c3Data.d_unused1)
#define M_shiftAmount	(__mp_taskCommon.c3.c3Data.shiftAmount)
#define M_correctionNeeded	(__mp_taskCommon.c3.c3Data.correctionNeeded)
#define M_iterationLockHandle (__mp_taskCommon.c3.c3Data.iterationLockHandle.data.ptr)
#define M_internalLockHandle	(__mp_taskCommon.c3.c3Data.internalLockHandle.data.ptr)
#define M_currentBase	(__mp_taskCommon.c3.c3Data.currentBase)
#define M_remainingTrips	(__mp_taskCommon.c3.c3Data.remainingTrips)

#define M_itersTillBlock	(__mp_taskCommon.c4.c4Data.itersTillBlock)
#define M_iterIncrement	(__mp_taskCommon.c4.c4Data.iterIncrement)
#define M_intendsToBlock	(__mp_taskCommon.c4.c4Data.autoBlockFlags.intendsToBlock)
#define M_intendsToBlockGroup (__mp_taskCommon.c4.c4Data.autoBlockFlags.intendsToBlockGroup)
#define M_isNowUnblocked	(__mp_taskCommon.c5.c5Data.autoUnblockFlags.isNowUnblocked)
#define M_isNowUnblockedGroup (__mp_taskCommon.c5.c5Data.autoUnblockFlags.isNowUnblockedGroup)

#define M_barrier_flag	(__mp_taskCommon.c7.barrier_flag)

#define M_cb               (__mp_taskCommon.single_construct_cb.data.construct_cb)
#define M_cb_controlLock   (__mp_taskCommon.single_construct_cb.data.construct_cb.controlLock)
#define M_cb_construct_instance   (__mp_taskCommon.single_construct_cb.data.construct_cb.construct_instance)
#define M_cb_all_allocated_instance   (__mp_taskCommon.single_construct_cb.data.construct_cb.all_allocated_instance)
#define M_cb_base  (__mp_taskCommon.single_construct_cb.data.construct_cb.base)
#define M_cb_tripcount     (__mp_taskCommon.single_construct_cb.data.construct_cb.tripcount)
#define M_cb_stride        (__mp_taskCommon.single_construct_cb.data.construct_cb.stride)
#define M_cb_original_tripcount    (__mp_taskCommon.single_construct_cb.data.construct_cb.original_tripcount)
#define M_cb_full_chunk_size       (__mp_taskCommon.single_construct_cb.data.construct_cb.full_chunk_size)
#define M_cb_last_chunk_size       (__mp_taskCommon.single_construct_cb.data.construct_cb.last_chunk_size)
#define M_cb_zero  (__mp_taskCommon.single_construct_cb.data.construct_cb.zero)
#define M_cb_threads  (__mp_taskCommon.single_construct_cb.data.construct_cb.threads)
#define M_cb_shift_amount  (__mp_taskCommon.single_construct_cb.data.construct_cb.shift_amount)
#define M_cb_correction_needed     (__mp_taskCommon.single_construct_cb.data.construct_cb.correction_needed)


#define M_joinArea_all		(__mp_taskCommon.joinArea)
#define M_joinArea(_row,_col)	(__mp_taskCommon.joinArea[_row].node.data.flag[_col])
#define M_thread_cb_all		(__mp_taskCommon.thread_cb)
#define M_thread_cb(_thread)	(__mp_taskCommon.thread_cb[_thread].data.thread_cb)


void __mp_sugnumthd_init(int32 min, int32 max,int32 now);

void __mp_sugnumthd_exit();

#endif  /* ifdef _LANGUAGE_C */

