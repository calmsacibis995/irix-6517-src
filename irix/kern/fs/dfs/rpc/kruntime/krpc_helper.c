/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: krpc_helper.c,v 65.9 1998/05/06 18:56:41 lmc Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: krpc_helper.c,v $
 * Revision 65.9  1998/05/06 18:56:41  lmc
 * Make krpc_queues_inited extern so we can find it in idbg.  Added
 * bzero of structures if running with DEBUG turned on.
 *
 * Revision 65.8  1998/05/01  23:05:26  bdr
 * Change the kernel helper to cause dfsbind to block on a read.
 * This prevents all of the endless spinning in dfsbind for no
 * reason.  Also this will speed up dfsbind as it used to sleep
 * for 5 seconds before it would try another read.
 *
 * Revision 65.7  1998/04/01  14:16:55  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	removed include for dcedfs/osi.h.  This had been included to
 * 	pick up the definition for uprintf.  There were too many problems
 * 	with other data types, however.  A definition for uprintf is
 * 	now provided in sysconf.h.
 *
 * Revision 65.6  1998/03/23  17:27:04  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.5  1998/03/03 15:57:31  lmc
 * Added an include for osi.h so that we pick up the #define of
 * uprintf to printf.
 *
 * Revision 65.4  1998/01/07  17:20:55  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:56:54  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:16:26  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 64.2  1997/03/28  15:36:03  lord
 * Make RPC mutex debug conditional on DEBUG being set
 *
 * Revision 64.1  1997/02/14  19:45:23  dce
 * *** empty log message ***
 *
 * Revision 1.4  1996/12/18  03:08:06  vrk
 * Removed some old debug statements and added some new ones.
 *
 * Revision 1.3  1996/05/29  21:08:24  bhim
 * Modified definition of tid to be unsigned int, replaced all usages of transaction
 * ids to be 32-bit, esp., all rpc__uiomove calls.
 *
 * Revision 1.2  1996/04/15  17:46:49  vrk
 * Old Changes. Has some cmn_err (follow VRK) that will be removed later.
 *
 * Revision 1.1  1994/07/13  22:33:18  dcebuild
 * Original File from OSF
 *
 * Revision 1.1.95.5  1994/07/13  22:33:18  devsrc
 * 	merged with bl-10
 * 	[1994/06/29  12:17:53  devsrc]
 *
 * 	Make dfsbind/krpc helper buffers configurable
 * 	[1994/04/25  15:01:18  delgado]
 *
 * 	dfsbind configurable request buffers
 * 	[1994/03/23  20:43:05  delgado]
 *
 * Revision 1.1.95.4  1994/05/19  21:14:15  hopkins
 * 	Serviceability work
 * 	[1994/05/19  02:18:23  hopkins]
 * 
 * 	Serviceability work.
 * 	[1994/05/18  21:33:56  hopkins]
 * 
 * Revision 1.1.95.3  1994/02/02  21:49:02  cbrooks
 * 	OT9855 code cleanup breaks KRPC
 * 	[1994/02/02  21:00:20  cbrooks]
 * 
 * Revision 1.1.95.2  1994/01/23  21:37:10  cbrooks
 * 	RPC Code Cleanup - CR 9797
 * 	[1994/01/23  21:05:21  cbrooks]
 * 
 * Revision 1.1.95.1  1994/01/21  22:32:06  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:26  cbrooks]
 * 
 * Revision 1.1.93.1  1993/10/05  13:41:44  root
 * 	Add ports restriction support for krpc.  Dfsbind will be
 * 	responsible for configuring the ports restrictions for
 * 	krpc.
 * 	[1993/10/05  13:37:32  root]
 * 
 * Revision 1.1.7.10  1993/03/24  21:50:45  rsarbo
 * 	Fix from IBM: Add brackets to if conditional around multi-line
 * 	macro
 * 	[1993/03/24  20:12:23  rsarbo]
 * 
 * Revision 1.1.7.9  1993/03/10  22:40:59  delgado
 * 	krpch_write should use uio_resid rather than uio_offset
 * 	when determining the number of bytes to transfer.
 * 	[1993/03/10  22:39:58  delgado]
 * 
 * Revision 1.1.7.8  1993/01/29  19:13:30  delgado
 * 	 krpc_PutHelper assumed that the handle being release was
 * 	always attached to some queue and tried to dequeue it.
 * 	This is not always the case when dfsbind aborts between
 * 	the time a client is given a handle and the time the client
 * 	makes its first call to krpc_ReadHelper only to discover that
 * 	dfsbind has gone away.  The fix is to check that the handle is
 * 	on a queue before employing the DEQUEUE operation.
 * 	[1993/01/29  19:12:44  delgado]
 * 
 * Revision 1.1.7.7  1993/01/27  19:05:02  delgado
 * 	Add support for krpc test driver
 * 	[1993/01/27  19:04:35  delgado]
 * 
 * Revision 1.1.7.6  1993/01/03  22:36:29  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:52:49  bbelch]
 * 
 * Revision 1.1.7.5  1992/12/23  19:39:26  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:53:33  zeliff]
 * 
 * Revision 1.1.7.4  1992/12/06  17:33:25  rsarbo
 * 	        Define TIME macro to be used instead of the kernel's "time"
 * 	        variable; the solution we submitted in the previous delta
 * 	        broke the OSF1.1 port
 * 	[1992/12/06  17:33:01  rsarbo]
 * 
 * Revision 1.1.7.3  1992/12/03  22:12:07  delgado
 * 	The new krpc helper driver-based interface
 * 	[1992/12/03  22:11:33  delgado]
 * 
 * Revision 1.1.7.2  1992/10/06  00:59:04  bolinger
 * 	     Patch log message.
 * 	     [1992/10/06  00:58:28  bolinger]
 * 
 * 	     Fix OT defect 4310:  in krpc_helperSendResponse(), use the returned
 * 	     status in the response buffer, rather than the status of the uiomove()
 * 	     that reads it, to control whether a second uiomove) of
 * 	     operation-related data is attempted.  There are no data present when
 * 	     the buffer status indicates an error, and the superfluous uiomove() to
 * 	     read them may have been causing an ENOMEM error return on RIOS.  Also
 * 	     avoid clearing uio_offset in the midst of using a uio buffer (the
 * 	     other candidate for causing the ENOMEM error).
 * 	     [1992/10/06  00:56:51  bolinger]
 * 
 * Revision 1.1.3.4  1992/06/30  21:45:11  rsalz
 * 	Misplaced paren had RPC_COND_WAIT_INTR calling rpc__cond_wait_intr with
 * 	wrong number of args; turning on rpc_e_dbg_mutex would cause kernel fault
 * 	[1992/06/30  19:56:51  rsalz]
 * 
 * Revision 1.1.3.3  1992/05/01  17:56:00  rsalz
 * 	"Changed as part of rpc6 drop."
 * 	[1992/05/01  17:50:48  rsalz]
 * 
 * Revision 1.1.3.2  1992/04/02  16:53:49  toml
 * 	Make krpc_helperGetRequest() init KRPC if necessary.
 * 	[1992/04/02  16:53:28  toml]
 * 
 * Revision 1.1  1992/01/19  03:16:37  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/
/*
**  Copyright (c) 1991 by
**      Hewlett-Packard Company, Palo Alto, Ca.
**
**  NAME
**
**      krpc_helper.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  Implement upcall from kernel space to user space helper process.
**
*/

/* Copyright (C) 1990 Transarc Corporation - All rights reserved */

#ifdef DEBUG
#define RPC_MUTEX_DEBUG
#endif

#include <dce/dce.h>
#include <commonp.h>
#include <com.h>
#include <cominit.h>
#include <sys/ioctl.h>
#include <krpc_helper.h>
#include <krpc_helper_data.h>
#ifdef SGIMIPS
#ifdef	PRE64
#include <sys/user.h>
#else
#include <sys/cmn_err.h>
#endif
#endif


/*
 * Helper related macros
 */
#define MIN_QUEUE_SIZE 4              
/*
 * How often we wake up and look for hung helpers.
 */
#define HELPER_MONITOR_INTERVAL 60        /* seconds */

/*
 * Maximum amount of time we want to wait for an unresponsive helper.
 * This is a global variable so that it can be patched if necessary.
 * This could be turned into a configurable item?
 */

unsigned long krpc_helper_gc_time = 180;
unsigned long krpc_helper_buffer_size;

/*
 * krpc_helpers's states bits
 */
#define KRPC_HELPER_DONE          0       /* buffer not in use */
#define KRPC_HELPER_REQREADY      1       /* client has finished request buffer */
#define KRPC_HELPER_RESPREADY     2       /* response/error ready for kernel client */
#define KRPC_HELPER_WORKING       4       /* helper is working on request */


/* states of the krpc-helper subsystem (krpc_helperflag) */

#define KRPC_HELPER_CLOSED             0x01 /* subsystem is shutdown */
#define KRPC_HELPER_CLOSE_IN_PROGRESS  0x02 /* subsystem shutting down */
#define KRPC_HELPER_OPEN               0x04 /* subsystem ready to process requests */
#define KRPC_HELPER_OPEN_IN_PROGRESS   0x08 /* subsystem coming on line */

/*
 * krpc_helper related globals
 */
char *helper_start;
rpc_mutex_t krpc_helperlock;    /* lock for this module */
rpc_cond_t krpc_helperCond;     /* condition for this module */

int krpc_helperflag = 0, krpc_helper_inited=0;        
static rpc_timer_t gc_timer;
static pthread_once_t krpc_helper_once = pthread_once_init;
#ifdef SGIMIPS
/*  So we can get it from idbg */
int krpc_queues_inited = 0;
#else
static int krpc_queues_inited = 0;
#endif

unsigned long tid=0;

struct krpc_helper *pending, *freelist, *in_service;
struct rpc_cond_t freelist_cond;


/* Other prototypes are in kprc_helper.h */

int initialize_queues _DCE_PROTOTYPE_ (( long /* howmany */));

/*
 * various counters to prevent the nopriority
 * requests from consuming all the available
 * helper handles.  We basically reserve 25%
 * of all the helper handles for high priority
 * requests.
 */ 
long lwm, freecount, highpriority;


/*
 * the following perhaps belongs in rpcmutex.h:
 */

#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)

#  define RPC_COND_WAIT_INTR(cond,mutex,error)  \
    { \
        if (RPC_DBG(rpc_es_dbg_mutex, 1)) \
        { \
            if (! rpc__cond_wait_intr(&(cond), &(mutex), __FILE__, __LINE__, &error)) \
            { \
                DIE("aborting"); \
            } \
        } else { \
            error = pthread_cond_wait_intr_np(&(cond).c, &(mutex).m); \
        } \
    }
#else
#  define RPC_COND_WAIT_INTR(cond,mutex,error) \
    { \
        RPC_LOG_COND_WAIT_NTR; \
        error = pthread_cond_wait_intr_np(&(cond).c, &(mutex).m); \
        RPC_LOG_COND_WAIT_XIT; \
    }
#endif


#if defined(RPC_MUTEX_DEBUG) || defined(RPC_MUTEX_STATS)
extern pthread_t rpc_g_null_thread_handle;
#define NULL_THREAD rpc_g_null_thread_handle

/*
 *  R P C _ _ C O N D _ W A I T _ I N T R
 *
 * The mutex is automatically released and reacquired by the wait.
 */

boolean rpc__cond_wait_intr
#ifdef _DCE_PROTO_
(
    rpc_cond_p_t cp,
    rpc_mutex_p_t mp,
    char *file,
    int line,
    int *error
)
#else 
(cp, mp, file, line, error)
    rpc_cond_p_t cp;
    rpc_mutex_p_t mp;
    char *file;
    int line;
    int *error;
#endif 
{
    pthread_t my_thread;
    volatile boolean dbg;
    extern int pthread_cond_wait_intr_np(pthread_cond_t *, pthread_mutex_t *); 

    cp->stats.wait++;
    dbg = RPC_DBG(rpc_es_dbg_mutex, 5);
    if (dbg)
    {
        if (! rpc__mutex_lock_assert(mp))
        {
            EPRINTF("(rpc__cond_wait_intr) mutex usage error\n");
            return(false);
        }
        if (cp->mp != mp)
        {
            EPRINTF("(rpc__cond_wait_intr) incorrect mutex\n");
            return(false);
        }
        my_thread = pthread_self();
        mp->owner = NULL_THREAD;
    }
    mp->is_locked = false;
    TRY
        *error = pthread_cond_wait_intr_np(&cp->c, &mp->m);
    CATCH_ALL
        mp->is_locked = true;
        if (dbg)
        {
            mp->owner = my_thread;
            mp->locker_file = file;
            mp->locker_line = line;
        }
        RERAISE;
    ENDTRY
    mp->is_locked = true;
    if (dbg)
    {
        mp->owner = my_thread;
        mp->locker_file = file;
        mp->locker_line = line;
    }
    return(true);
  }
#endif

/* 
 * Functions for dealing with helper processes, like the one we use for
 * handling remote cell lookup requests.
 *
 * Basic idea is:
 *
 * Client gets a upcall handle via krpc_GetHelper; it then places the
 * request information in the buffer associated with the handle via
 * krpc_WriteHelper.  When it is finished writing its data, it then calls
 * krpc_ReadHelper to place the request in the pending queue to be picked
 * up by the dfsbind process; it will sleep in krpc_ReadHelper until a response
 * is returned or the upcall aborted.
 *
 * The dfsbind process uses a pseudo-device driver interface to get into the 
 * krpc_helper subsystem.  The dfsbind process is implemented using cma
 * threads, and therefore it depends on select (krpch_select) to tell it 
 * if there are any upcall requests available; we do not sleep in the kernel 
 * waiting for requests to come in.  The dfsbind process will, after obtaining
 * a file descriptor, use krpch_read to pick up an upcall request and transfer
 * it to user space, and use krpch_write to transfer the response back
 * to the kernel client.
 *
 * The pseudo-device driver interface will use the open system call to
 * get a file descriptor for the helper subsystem (this eventually calls
 * krpch_open which performs some one-time initialization for the helper
 * subsystem).  After the open, the krpc subsystem will be configured
 * via krpch_ioctl to set up the number of request handles the kernel
 * will allocate for the system.  Lastly, the initialization is completed
 * when we perform a secion ioctl (KRPCH_ENABLE) to notify clients that
 * the subsystem is completely initialized and the helper process is now
 * actively handling requests.
 *
 */

/*
 * Abort helper with error status of "code".
 * Caller should hold krpc_helperlock.
 *
 */

static void krpc_AbortHelper
#ifdef _DCE_PROTO_
(
    struct krpc_helper *ahp,
    unsigned32 code
)
#else 
(ahp, code)
struct krpc_helper *ahp;
unsigned32 code;
#endif
{
    ahp->states = KRPC_HELPER_RESPREADY; /* abort this dude */
    ahp->returnCode = code;
    printf("krpc_AbortHelper: code = %d\n", code);
    RPC_COND_SIGNAL(ahp->ccond, krpc_helperlock);
}


/* 
 * Caller periodically to look for hopelessly bogus requests 
 * (helper probably died) 
 */

static void krpc_gc_helper 
#ifdef _DCE_PROTO_
( void ) 
#else 
()
#endif
{
    
    register struct krpc_helper *thp;
    register unsigned long gctime = (unsigned long)TIME - krpc_helper_gc_time;
    
    RPC_MUTEX_LOCK(krpc_helperlock);
    if (krpc_helperflag != KRPC_HELPER_OPEN){
        RPC_MUTEX_UNLOCK(krpc_helperlock);
        return;
    }
    for (thp = in_service->next; thp != in_service; thp = thp->next) {
        if ((thp->states & KRPC_HELPER_WORKING) && gctime > thp->started)
            krpc_AbortHelper(thp, rpc_s_helper_catatonic);
    }
    RPC_MUTEX_UNLOCK(krpc_helperlock);
}


/* 
 * Called by kernel client to get a request handle and
 * a buffer. If there are no available request handles, then
 * the caller will sleep.  If the helper subsystem is not 
 * ready (active) then an error is returned.  If the helper system
 * is undergoing initialization then the caller will sleep until
 * the initialization is complete.
 *
 * This routine is also careful to not give out all helper handles
 * to the nopriority requests, thus ensuring that high priority 
 * requests always receive some service.
 */
struct krpc_helper *krpc_GetHelper
#ifdef _DCE_PROTO_
(
  long opcode 
)
#else 
(opcode) 
    long opcode;
#endif
{
    struct krpc_helper *rhp = (struct krpc_helper *) 0;
    

    RPC_MUTEX_LOCK(krpc_helperlock);
    if ((krpc_helperflag == KRPC_HELPER_CLOSED) || (krpc_helperflag ==
        KRPC_HELPER_CLOSE_IN_PROGRESS)){
        RPC_MUTEX_UNLOCK(krpc_helperlock);
        return((struct krpc_helper *) NULL);
    }
    while(krpc_helperflag == KRPC_HELPER_OPEN_IN_PROGRESS){
        RPC_COND_WAIT(krpc_helperCond, krpc_helperlock);
      }
    if (HIGH_PRIORITY(opcode)){
        while (QUEUE_EMPTY(freelist)){
             RPC_COND_WAIT(freelist_cond, krpc_helperlock);
         }
        highpriority++;
    }else{  
        while(((freecount+highpriority) <= lwm) || QUEUE_EMPTY(freelist)){
             RPC_COND_WAIT(freelist_cond, krpc_helperlock);
	   }
    }
    freecount --;
    DEQUEUE_NEXT(freelist, rhp);
    RPC_MUTEX_UNLOCK(krpc_helperlock);
#if defined(SGIMIPS) && defined(DEBUG)
    bzero(rhp, sizeof(struct krpc_helper));
#endif
    RPC_MEM_ALLOC (rhp->buffer, unsigned char *, krpc_helper_buffer_size,
            RPC_C_MEM_UTIL, RPC_C_MEM_WAITOK);
    rhp->bufptr = rhp->buffer;
    rhp->states = KRPC_HELPER_DONE;
    rhp->opcode = opcode;
    return rhp;
}

/* 
 * Called by kernel client to release a helper handle.
 * The helper handle is placed back on the free list.
 */
void krpc_PutHelper
#ifdef _DCE_PROTO_
(
  register struct krpc_helper * ahp 
)
#else 
(ahp)
    register struct krpc_helper *ahp; 
#endif
{


    RPC_MUTEX_LOCK(krpc_helperlock);
    if (ahp->buffer) {
        RPC_MEM_FREE (ahp->buffer, RPC_C_MEM_UTIL);
        ahp->buffer = (unsigned char *) 0;
    }
    ahp->states = KRPC_HELPER_DONE;
    if ((ahp->next != NULL) && (ahp->prev != NULL)) {
        DEQUEUE_ITEM(ahp);
    }
    ENQUEUE_ITEM(freelist, ahp);    
    if (HIGH_PRIORITY(ahp->opcode))
        highpriority--;
    freecount++;
#if defined(SGIMIPS) && defined(DEBUG)
    bzero(ahp, sizeof(struct krpc_helper));
#endif
    RPC_COND_SIGNAL(freelist_cond, krpc_helperlock);
    RPC_MUTEX_UNLOCK(krpc_helperlock);
}


/* 
 * Called by client to marshall some data into
 * the helper buffer.
 */
unsigned32 krpc_WriteHelper
#ifdef _DCE_PROTO_
(
    register struct krpc_helper *ahp,
    unsigned char *abufferp,
    long alen
)
#else 
(ahp, abufferp, alen)
    register struct krpc_helper *ahp;
    unsigned char *abufferp;
    long alen;
#endif
{
    RPC_MUTEX_LOCK(krpc_helperlock);
    /* check for buffer overflow
     *if ((ahp->bufptr + alen) > (ahp->buffer+krpc_helper_buffer_size))
     *  return(E2BIG);
     */
    bcopy(abufferp, ahp->bufptr, alen); 

    ahp->bufptr += alen;
    RPC_MUTEX_UNLOCK(krpc_helperlock);
    return 0;
}


/* 
 * Called by client to get some response bytes 
 * This will acutally fire off the request to the user-space
 * helper and sleep waiting for the response.
 *
 * We need to watchout for the helper subsystem's being
 * shutdown while we are attempting to request service.
 * In this case we check the state of the subsystem and
 * return rpc_s_helper_aborted if the system is in a state
 * other than open.
 */
unsigned32 krpc_ReadHelper
#ifdef _DCE_PROTO_
(
    register struct krpc_helper *ahp,
    unsigned char *abufferp,
    long alen,
    long *rlen
)
#else 
(ahp, abufferp, alen, rlen)
    register struct krpc_helper *ahp;
    unsigned char *abufferp;
    long alen;
    long *rlen;
#endif
{
    register long temp, code, nmoved;
    extern void krpc_wakeup(void);     /* aka SELECT_NOTIFY */

    RPC_MUTEX_LOCK(krpc_helperlock);
    if (krpc_helperflag != KRPC_HELPER_OPEN){
        code = ahp->returnCode = rpc_s_helper_aborted;
         goto out;
    }
    if (ahp->states == KRPC_HELPER_DONE) {
        ahp->states = KRPC_HELPER_REQREADY;
	ENQUEUE_ITEM(pending, ahp);
        /* tell select there's something available */
#ifdef SGIMIPS
	RPC_COND_BROADCAST(krpc_helperCond, krpc_helperlock);
#else  /* SGIMIPS */
        SELECT_NOTIFY();
#endif /* SGIMIPS */
    }
    /* 
     * Wait for either an abort or a response.  
     */
    while (ahp->states != KRPC_HELPER_RESPREADY){
        RPC_COND_WAIT (ahp->ccond, krpc_helperlock);
    }
    code = ahp->returnCode;
    if (rlen)
        *rlen = 0;

    if (code == rpc_s_ok)
    {
        if (ahp->bytes < alen)
        {
            if (!rlen)
            {
                code = rpc_s_helper_short_read;
                goto out;
            } else {
                alen = ahp->bytes;
            }
        }

        /* 
         * Copy out the data 
         */
        bcopy(ahp->bufptr, abufferp, alen);

        ahp->bufptr += alen;
        ahp->bytes -= alen;

        if (rlen)
            *rlen = alen;
    }

out:
    RPC_MUTEX_UNLOCK (krpc_helperlock);
    return code;
}


/*
 * Things which are performed only once during
 * the life of the system will go here
 */

static void krpc__helper_init( void ) 
{

    RPC_VERIFY_INIT();

    RPC_MUTEX_INIT(krpc_helperlock);
    RPC_COND_INIT(krpc_helperCond, krpc_helperlock);
    RPC_COND_INIT(freelist_cond, krpc_helperlock);

    krpc_helperflag = KRPC_HELPER_CLOSED;
    rpc__timer_set (&gc_timer, (rpc_timer_proc_p_t)krpc_gc_helper,
        (pointer_t) 0, RPC_CLOCK_SEC(HELPER_MONITOR_INTERVAL));
    KRPC_HELPER_MACHINIT();
}

void krpc_helper_init( void ) 
{

    if (krpc_helper_inited == 0){
        krpc_helper_inited = 1;
        pthread_once (&krpc_helper_once, krpc__helper_init); 
      }
}


/* 
 * Function called by helper to get a request.  Returns a transaction id
 * followd by a length at the start of the buffer, and then that many bytes.
 * If there is no work to do we go back to user space and fool around for
 * awhile.
 *
 * Requests are taken in fifo order from the pending queue and placed in
 * the in-service queue.
 *
 * data returned in buffer is 4 byte transaction id, followed by the
 * data representing the request.
 */

int
krpch_read
#ifdef _DCE_PROTO_
(
   struct uio * uio
)
#else 
(uio)
     struct uio *uio;
#endif 
{
    register long i;
    int error = 0;
    register struct krpc_helper *thp;
#ifdef SGIMIPS
    extern int pthread_cond_wait_intr_np( 
		pthread_cond_t *condition, pthread_mutex_t *mutex);
#endif /* SGIMIPS */


    RPC_MUTEX_LOCK (krpc_helperlock);
    if (krpc_helperflag != KRPC_HELPER_OPEN){
        RPC_MUTEX_UNLOCK(krpc_helperlock);
        return (EINVAL);
    }
#ifdef SGIMIPS
    /*
     *  With POSIX pthreads support it is ok for read to block .
     */
    while (QUEUE_EMPTY(pending)){
        RPC_COND_WAIT_INTR(krpc_helperCond, krpc_helperlock, error);
        if (error) {
            RPC_MUTEX_UNLOCK(krpc_helperlock);
            return(error);
        }
    }
#else  /* SGIMIPS */
    if (QUEUE_EMPTY(pending)){
         RPC_MUTEX_UNLOCK(krpc_helperlock);
         /* we need to return EWOULDBLOCK so that cma
          * will perform a select in the user space
          * dfsbind process.  For character special 
          * devices, cma will attempt to read first
          * and if read returns EWOULDBLOCK it will begin
          * selecting; the wrapped read call will then not
          * actually return until there is data present.
          */
	 return(EWOULDBLOCK);
       }
#endif /* SGIMIPS */
    DEQUEUE_NEXT(pending, thp);
    thp->started = TIME;
    thp->states = KRPC_HELPER_WORKING;
    ENQUEUE_ITEM(in_service, thp);
    
    /* so we can match up response */
#ifdef SGIMIPS
    thp->requestID = OSI_TRANSACTION_ID; 
    error = rpc__uiomove((caddr_t)(&(thp->requestID)), sizeof(int), UIO_READ, uio);   
#else
    thp->requestID = (long) OSI_TRANSACTION_ID; 
    error = rpc__uiomove((caddr_t)(&(thp->requestID)), sizeof(long), UIO_READ, uio);   
#endif /* SGIMIPS */
    if (error == 0){ 
       i = thp->bufptr - thp->buffer;
       error = rpc__uiomove ((caddr_t)(thp->buffer), i, UIO_READ, uio);
      }
    RPC_MUTEX_UNLOCK(krpc_helperlock);
    return error;
}

/* 
 * Function called by helper to send a response.  Returns a 4 byte transaction
 * id, followed by a 4 byte return code and then the data bytes.  
 */
int
krpch_write
#ifdef _DCE_PROTO_
(
   struct uio * uio
)
#else 
(uio)
     struct uio *uio;
#endif 

{
    register long i,error=0;
    register struct krpc_helper *thp, tmp;
#ifdef SGIMIPS
    unsigned int id;
#else
    unsigned long id;
#endif /* SGIMIPS */

    RPC_MUTEX_LOCK(krpc_helperlock);
    if (krpc_helperflag != KRPC_HELPER_OPEN){
       error = EINVAL;
       goto done;
     }
    /* extract transaction id  and find matching request*/
#ifdef SGIMIPS
    error = rpc__uiomove((caddr_t)(&id), sizeof(int), UIO_WRITE, uio);
#else
    error = rpc__uiomove((caddr_t)(&id), sizeof(long), UIO_WRITE, uio);
#endif /* SGIMIPS */
    if (error)  /* let the helper garbage collection take care of it */
        goto done;
    for (thp = in_service->next; thp != in_service; thp = thp->next){
        if (thp->requestID == id)
            break;
    }
    if (thp == in_service) {
        RPC_MUTEX_UNLOCK(krpc_helperlock);
        return ENOENT;
    }
    thp->bufptr = thp->buffer;
#ifdef SGIMIPS
    error = rpc__uiomove ((caddr_t)&thp->returnCode, sizeof(int), UIO_WRITE, uio);
#else
    error = rpc__uiomove ((caddr_t)&thp->returnCode, sizeof(long), UIO_WRITE, uio);
#endif /* SGIMIPS */
    if ((error == 0 ) && (thp->returnCode == 0)) {  
        thp->bytes = uio->uio_resid;
        error = rpc__uiomove ((caddr_t)(thp->bufptr), krpc_helper_buffer_size,
                              UIO_WRITE, uio);
    }
    if (error != 0) 
       thp->returnCode = error;
    if (thp->returnCode !=0)
        thp->bytes=0;
    thp->states = KRPC_HELPER_RESPREADY;
    RPC_COND_SIGNAL(thp->ccond, krpc_helperlock);
done:
    RPC_MUTEX_UNLOCK(krpc_helperlock);
    return(error);
}

/*
 * Generic helper open(): krpch_open()
 *
 * Inititalize the helper subsystem; allocate request handles and
 * place them on the free list.  Only one open is allowed to succeed.
 *
 * The open process is actually completed when we issue the appropriate
 * ioctls to configure the queues and enable the system.
 *
 */
int
krpch_open( void ) 
{
    int error;

    RPC_VERIFY_INIT();
    RPC_MUTEX_LOCK(krpc_helperlock);
          /* only allow one open to succeed */
    switch(krpc_helperflag){
        case KRPC_HELPER_CLOSE_IN_PROGRESS:
            error = EINPROGRESS;
            break;
 	case KRPC_HELPER_OPEN_IN_PROGRESS:
        case KRPC_HELPER_OPEN:
            error = EALREADY;
            break;
        case KRPC_HELPER_CLOSED:
            error = 0;
            break;
	default:
            error = EINVAL;
      }
    if (!error){
        krpc_helperflag = KRPC_HELPER_OPEN_IN_PROGRESS;
    }
    RPC_MUTEX_UNLOCK(krpc_helperlock);
    return(error);
  }

/*
 * Allocate and initialize the specified number of
 * helper request handles.
 */


int
initialize_queues
#ifdef _DCE_PROTO_
(
 long howmany
)
#else 
(howmany)
    long howmany;
#endif
{
    struct krpc_helper *helper_ptr;
 
    int size, i;
    size = ROUNDUP(sizeof(struct krpc_helper),ALIGNMENT);
    RPC_MEM_ALLOC(helper_start, char *, size*(howmany+3),
                  RPC_C_MEM_UTIL, RPC_C_MEM_WAITOK);
    freelist = (struct krpc_helper *)helper_start;
    freelist->next = freelist->prev = freelist;
    helper_start += size;
    pending = (struct krpc_helper *)helper_start;
    helper_start += size;
    in_service = (struct krpc_helper *)helper_start;
    helper_start += size;
    in_service->next = in_service->prev = in_service;
    pending ->next = pending->prev = pending;
    for(i=0; i< howmany; i++, helper_start += size){
        helper_ptr = (struct krpc_helper *) helper_start;
        ENQUEUE_ITEM(freelist, helper_ptr);
        RPC_COND_INIT (helper_ptr->ccond, krpc_helperlock);
        helper_ptr->states = KRPC_HELPER_DONE;
        helper_ptr->requestID = -1;
     }
     krpc_queues_inited = 1;
     highpriority=0;
     /* 25% of slots always allocated to high priority
      * we are just guessing at the 25% -no magic here!
      * feel free to change if you come up with something
      * better
      */
     lwm = (long)(freecount/MIN_QUEUE_SIZE);
     return (0);
}

/*
 * Close down the helper subsystem;
 * Eventually we want to deallocate the request handles or
 * make some kind of adjustment if dfsbind is restarted with
 * different configuration parameters.
 */

void
krpch_close( void )
{
  struct krpc_helper *thp;
  
  RPC_MUTEX_LOCK(krpc_helperlock);
  krpc_helperflag = KRPC_HELPER_CLOSE_IN_PROGRESS;
  /*
   * could be that the helper was aborted before it completed
   * initialization of the queues - in this case there is nothing
   * to do.
   */
  if (pending == (struct krpc_helper *) NULL)
      goto done;
  for(thp = in_service->next ; thp != in_service; thp = thp->next){
     if (thp->states & KRPC_HELPER_WORKING) {
	 /* VRK */
  	 printf("krpch_close: Calling krpc_AbortHelper for thp=%x\n", thp);
         krpc_AbortHelper(thp, rpc_s_helper_aborted);
     }
   }
  for (thp = pending->next; thp != pending; thp = thp->next){
    /* VRK */
    printf("krpch_close: Calling krpc_AbortHelper for thp=%x, proc=%x\n", thp, (0));
    krpc_AbortHelper(thp,rpc_s_helper_aborted);
  }
done:
  krpc_helperflag = KRPC_HELPER_CLOSED;
 RPC_MUTEX_UNLOCK(krpc_helperlock);
}

/*
 * These ioctls initialized the krpc subystem.
 *
 * From user-space we first do an open to get a file descriptor
 * and initialize krpc.  We then do an ioctl KPRCH_CONFIGURE
 * to configure the number of request handles the kernel will
 * allocate (currently we do this just once per system lifetime -
 * eventually we will want to do it everytime user-space is shut
 * down).  We then do a KRPCH_ENABLE to tell our clients that
 * the helper is actively serving requests.
 */


krpch_ioctl
#ifdef _DCE_PROTO_
(
    int cmd,
    caddr_t data
)
#else 
(cmd, data)
    int cmd;
    caddr_t data;
#endif
{
  unsigned32 error;
  struct uio krpc_uio;
  struct iovec iov;
  struct krpch_config *kpc;
#ifdef	SGIMIPS
  struct krpch_config loc_kpc;
#endif

  error = 0;
  RPC_MUTEX_LOCK(krpc_helperlock);
  switch(cmd){
  case KRPCH_ENABLE:
        if (krpc_helperflag != KRPC_HELPER_OPEN_IN_PROGRESS)
            error = EALREADY;      
        else{
            krpc_helperflag = KRPC_HELPER_OPEN;
     	    RPC_COND_BROADCAST(krpc_helperCond, krpc_helperlock);
            error = 0;
	  }
     break;

  case KRPCH_CONFIGURE:
        if (krpc_helperflag != KRPC_HELPER_OPEN_IN_PROGRESS)
            error = EALREADY;      
        else{
#ifdef	SGIMIPS
		copyin(data, &loc_kpc, sizeof(loc_kpc));
                kpc = (struct krpch_config *)&loc_kpc;
#else
                kpc = (struct krpch_config *)data;
#endif
                freecount = kpc->qsize;
                krpc_helper_buffer_size = kpc->bufsize;
                if (krpc_queues_inited == 0){
                    if (freecount <=0)
                       error = EINVAL;
                    else{
                        if (krpc_helper_buffer_size < KRPC_MIN_BUFSIZE)
                            error = EINVAL;
                        else{
                            if (freecount < MIN_QUEUE_SIZE)
                                freecount = MIN_QUEUE_SIZE;
                            error = initialize_queues(freecount);
                            krpc_queues_inited=1;
			  }
		      }
                }else
                    error = 0;
	      }
       break;

  case (KRPCH_RESTRICT_PORTS):  
         if (krpc_helperflag != KRPC_HELPER_OPEN_IN_PROGRESS)
            error = EALREADY;      
        else{
#ifdef	SGIMIPS
	int howmany = 0;
	unsigned_char_p_t datp = (unsigned_char_p_t)data;

	    if(datp != 0){
		while(1){
		    if(fubyte(datp) == 0)
			break;
		    howmany++;
		    datp++;
	    	}
		RPC_MEM_ALLOC(datp, unsigned_char_p_t, howmany+1,
			RPC_C_MEM_UTIL, RPC_C_MEM_WAITOK);
	    }
	    copyin(data, datp, howmany+1);

            rpc__set_port_restriction_from_string(
                         (unsigned_char_p_t)datp, &error);
	    RPC_MEM_FREE(datp, RPC_C_MEM_UTIL);
#else
            rpc__set_port_restriction_from_string(
                         (unsigned_char_p_t)data, &error);
#endif
            if (error != 0){
                uprintf("rpc__set_port_restriction_from_string failed %ld\n",
                     error);
               krpc_helperflag = KRPC_HELPER_CLOSED;
	      }
	  }

       break;

  default:
       error = EINVAL;
      }
  RPC_MUTEX_UNLOCK(krpc_helperlock);
  return(error);
}




/*
 *  This is used only by test/rpc/kdriver/<mach>/ncsdev_sys.c
 *  This avoids requiring the tests to configure the queue size 
 *  from user space.  It also allows the device to be opened
 *  simultaneously by more than one caller, which is required
 *  for these tests.
 */

krpch_test_driver_open( void ) 
{
    int error;

  RPC_VERIFY_INIT();
  RPC_MUTEX_LOCK(krpc_helperlock);
    if (krpc_queues_inited == 0){
        initialize_queues(MIN_QUEUE_SIZE);
        krpc_queues_inited=1;
      }
    krpc_helperflag=KRPC_HELPER_OPEN;
  RPC_MUTEX_UNLOCK(krpc_helperlock);
  return 0;
}












