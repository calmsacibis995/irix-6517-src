/**************************************************************************
 *                                                                        *
 *       Copyright (C) 1997, Silicon Graphics, Inc.                       *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Asynchronous client push support.
 */
#include "rtmond.h"
#include "timer.h"

#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>

#define	IO_DEFBUFS		2		/* default # buffers */

/*
 * Event data is pushed to clients asynchronously from event
 * collection.  Data is buffered in "i/o buffers" that are
 * passed to a per-connection "push thread" that does the writes.
 * Each client is initially allocated 2 buffers for reach
 * CPU that has event collection enabled on with the number
 * of buffers dynamically grown as required up to a maximum
 * number.  Push buffers are all the same size.
 */
int	maxiobufs = _CONFIG_MAXIOBUFS;		/* # i/o bufs per client/CPU */
size_t	iobufsiz = _CONFIG_IOBUFSIZ;		/* i/o buffer size */

void
init_io(void)
{
    IFTRACE(DEBUG)(NULL, "Maximum of %d buffers/client/CPU, each %d KB",
	maxiobufs, iobufsiz/1024);
}

/*
 * Per-client event data "push" thread.  One instance
 * of this code is started for each client/connection.
 * When a buffer of event data is ready for transmission
 * it is placed on the "push queue" and the thread is
 * notified by incrementing a semaphore.  When the write
 * is completed the buffer is returned to the clients
 * free list.  If a client goese away the file descriptor
 * is closed, invalidated, and the thread terminates.
 */
#ifdef USE_SPROC
static void
io_push(void* vdp, size_t stacksize)
#else
static void*
io_push(void* vdp)
#endif
{
    client_common_t* com = (client_common_t*) vdp;

    IFTRACE(THREAD)(NULL, "Starting i/o thread for %s:%u",
	com->host, com->port);
#ifdef USE_SPROC
    (void) stacksize;
    /*
     * Immediately turn off par tracing for this thread.
     */
    disallow_tracing(NULL);
#else
    (void) pthread_detach(pthread_self());
#endif

    for (;;) {
	ioblock_t* io;

	/*
	 * Wait for something to be placed in the queue.
	 * We initialize the ioq semaphore to zero and
	 * io_post (below) increments the semaphore for
	 * each entry it inserts.  Thus we'll block here
	 * whenever the queue is empty; otherwise we'll
	 * make a trip around the loop for each entry.
	 */
	sem_wait(&com->ioq);
	sem_wait(&com->iolock);			/* lock queue and take 1st */
	    io = com->iohead;
	    if (io) {
		com->iohead = io->next;
		if (com->iotail == io)
		    com->iotail = NULL;
	    }
	sem_post(&com->iolock);
	if (io) {
	    client_t* cp = io->cp;
	    size_t off = io->off;		/* NB: copy for use below */
	    ssize_t cc;
	    uint64_t wstart, wlen;

	    /*
	     * Push buffered data to client.  NB, we are the
	     * only writer so we don't need to synchronize
	     * access to the file descriptor.
	     */
	    wstart = readcc();
	    cc = write(com->fd, io->u.buf, off);
	    wlen = readcc() - wstart;
	    IFTRACE(EVENTIO)(NULL, "Client %s:%u write %ld sent %ld",
		cp->host, cp->port, (long) off, (long) cc);
	    /*
	     * Place i/o buffer back on client's free list.
	     * We do this before checking the result so that
	     * if the client is purged the buffer will be on
	     * the free list and so be reclaimed.
	     */
	    sem_wait(&cp->iofreelock);
		io->off = cp->lowmark;
		io->next = cp->iofree;
		cp->iofree = io;
	    sem_post(&cp->iofreelock);

	    if (cc > 0) {
		cp->writes++;
		cp->totbytes += cc;
		com->iototal += cc;
		com->iotime += wlen;
		com->iocnt++;
		if (wlen > com->iomax)
		    com->iomax = wlen;
		if (wlen < com->iomin)
		    com->iomin = wlen;
	    }
	    if (cc != off) {
		/*
		 * Terminate thread.  Note that we avoid touching
		 * client state since there's a race with the
		 * thread that monitors the client command channel
		 * and does the purge_client call to cleanup state.
		 * Note also that when using sproc's we pause to
		 * wait to be terminated; otherwise the signal might
		 * be directed to another process if the pid is
		 * reassigned quickly by the system.
		 */
		IFTRACE(THREAD)(NULL, "I/O thread for %s:%u done",
		    com->host, com->port);
#ifdef USE_SPROC
		pause();			/* wait for death */
#else
		return (EXIT_SUCCESS);
#endif
	    }
	}
    }
}

/*
 * Place client i/o request at the tail of the push q
 * and take another ioblock from the client's free
 * list (if available) and make it the "current" one.
 */
void
io_post(daemon_info_t* dp, client_t* cp)
{
    client_common_t* com = cp->com;
    ioblock_t* io = cp->io;

    assert(io != NULL);
    io->next = NULL;
    sem_wait(&com->iolock);			/* place on push queue */
	if (com->iotail != NULL) {
	    com->iotail->next = io;
	    com->iotail = io;
	} else
	    com->iohead = com->iotail = io;
    sem_post(&com->iolock);
    sem_post(&com->ioq);			/* notify push thread */
    sem_wait(&cp->iofreelock);			/* take from free list */
	if (cp->io = cp->iofree)
	    cp->iofree = cp->io->next;
	else
	    cp->io = io_new_buf(dp, cp);
    sem_post(&cp->iofreelock);
    cp->lastpush = readcc();
}

/*
 * Allocate and initialize a push buffer.
 */
ioblock_t*
io_new_buf(daemon_info_t* dp, client_t* cp)
{
    if (cp->niobufs < maxiobufs) {
	ioblock_t* io = (ioblock_t*) malloc(sizeof (*io));

	if (io != NULL) {
	    io->u.buf = (char*) malloc(iobufsiz);
	    if (io->u.buf != NULL) {
		io->cp = cp;
		io->off = cp->lowmark;
		io->size = iobufsiz;
		(*cp->com->proto->initIOBlock)(dp, io);
		cp->niobufs++;
	    } else {
		Log(LOG_ERR, dp, "No space for client push buffer");
		free(io), io = NULL;
	    }
	} else
	    Log(LOG_ERR, dp, "No space for client i/o block");
	return (io);
    } else
	return (NULL);
}

/*
 * Resume push support for the specified client.
 */
int
io_client_resume(daemon_info_t* dp, client_t* cp)
{
    int i;

    cp->lastpush = (uint64_t) -1;
    cp->pushes = 0;
    cp->writes = 0;
    cp->totbytes = 0;
    cp->niobufs = 0;
    for (i = 0; i < IO_DEFBUFS; i++) {
	ioblock_t* io = io_new_buf(dp, cp);
	if (io == NULL)
	    return (0);
	io->next = cp->iofree;
	cp->iofree = io;
    }
    assert(cp->iofree != NULL);
    return (1);
}

/*
 * Initialize push support for a client_t.
 */
int
io_client_init(daemon_info_t* dp, client_t* cp)
{
    cp->io = NULL;
    cp->iofree = NULL;
    if (sem_init(&cp->iofreelock, 0, 1) != 0) {
	Log(LOG_ERR, dp, "Cannot initialize new client free list semaphore: %s",
	    strerror(errno));
	return (0);
    }
    return (io_client_resume(dp, cp));
}

/*
 * Reclaim push resources from a client being paused.
 */
void
io_client_pause(daemon_info_t* dp, client_t* cp)
{
    ioblock_t* io;

    if (io = cp->io) {
	free(io->u.buf);
	free(io);
	cp->io = NULL;
    }
    while (io = cp->iofree) {
	cp->iofree = io->next;
	free(io->u.buf);
	free(io);
    }

    dp->clientwrites += cp->writes;
    dp->clientdata += cp->totbytes;
}

/*
 * Reclaim resources from a client_t.
 */
void
io_client_cleanup(daemon_info_t* dp, client_t* cp)
{
    io_client_pause(dp, cp);
    sem_destroy(&cp->iofreelock);
}

/*
 * Resume push support for a client.
 */
int
io_com_resume(client_common_t* com)
{
    com->iototal = 0;
    com->iocnt = 0;
    com->iotime = 0;
    com->iomin = (uint64_t) -1;
    com->iomax = 0;
#ifdef USE_SPROC
    if ((com->ioproc = sprocsp(io_push, PR_SALL, com, NULL, _SPROC_IOPUSH_STACKSIZE)) < 0) {
#else
    if (errno = pthread_create(&com->ioproc, NULL, io_push, com)) {
#endif
	com->ioproc = 0;
	Log(LOG_ERR, NULL, "Could not start push thread: %s", strerror(errno));
	return (0);
    } else
	return (1);
}

/*
 * Initialize push resources in the common
 * data structure shared by all client instances.
 */
int
io_com_init(client_common_t* com)
{
    com->ioproc = 0;
    com->iohead = NULL;
    com->iotail = NULL;
    if (sem_init(&com->iolock, 0, 1) != 0)
	Log(LOG_ERR, NULL, "Could not create lock semaphore for io q");
    else if (sem_init(&com->ioq, 0, 0) != 0)
	Log(LOG_ERR, NULL, "Could not create count semaphore for io q");
    else if (io_com_resume(com))
	return (1);
    return (0);
}

/*
 * Reclaim push support from a client being paused.
 */
void
io_com_pause(daemon_info_t* dp, client_common_t* com)
{
    ioblock_t* io;

#ifdef USE_SPROC
    if (com->ioproc != 0 && kill(com->ioproc, SIGKILL) != 0)
#else
    if (com->ioproc != 0 && (errno = pthread_kill(com->ioproc, SIGKILL)))
#endif
	Log(LOG_ERR, dp, "Error terminating i/o thread %d: %s",
	    com->ioproc, strerror(errno));
    while (io = com->iohead) {
	com->iohead = io->next;
	free(io->u.buf);
	free(io);
    }
}

/*
 * Reclaim resources from a client_common_t.
 */
void
io_com_cleanup(daemon_info_t* dp, client_common_t* com)
{
    uint64_t clockfreq = NSEC_PER_SEC / cc.cycleval;
#define	TicksToUS(t)	(1000LL*(t)/clockfreq)
#define	NZ(x)		((x) ? (x) : 1)
    IFTRACE(PERF)(NULL, "Client %s:%u, %u io's for %llu bytes in %llu us (%.2f MB/sec),"
	" avg io %llu us, min %llu us, max %llu us"
	, com->host, com->port
	, com->iocnt
	, com->iototal
	, TicksToUS(com->iotime)
	, (1000000./(1024*1024.)) * com->iototal / NZ(TicksToUS(com->iotime))
	, TicksToUS(com->iotime / NZ(com->iocnt))
	, TicksToUS(com->iomin)
	, TicksToUS(com->iomax)
    );
    io_com_pause(dp, com);
    sem_destroy(&com->iolock);
    sem_destroy(&com->ioq);
#undef NZ
#undef TicksToUS
}
