#ifndef _CONFIG_
#define	_CONFIG_
/*
 * Configuration defaults.
 */
#define	_CONFIG_DEFTRACE	0		/* default tracing */

/*
 * Only one rtmond server should be operating at a time.
 * To guard against multiple servers being started a lock
 * file holds the PID of the currently running server.
 * If a server is started and encounters a lock file then
 * it checks if the associated process is still present
 * on the system and, if so, it terminates.
 */
#define _PATH_PID_FILENAME	"/tmp/.rtmond_pid"

/*
 * rtmond processes are usually run with a realtime
 * scheduling priority to reduce the possibility of losing
 * events.  The default priority was selected to be below
 * certain system processes such as netproc that would be
 * seriously impacted by rtmond operation.  Setting this
 * value to zero disables realtime scheduling.
 */
#define	_CONFIG_DEFPRI		88		/* realtime scheduling pri. */

/*
 * Client access to the server is controlled on a per-machine
 * basis with provision for restricting the classes of events
 * that each client may receive.  By default access controls
 * are specified through a file located on the server.  A command
 * line option is also provided to override the contents of
 * this file so that, for example, one can easily restrict
 * access only to local (on-machine) clients.
 */
#define	_CONFIG_ACCESS_FILENAME	"/etc/rtmond"	/* client access control file */
#define	_CONFIG_GLOBAL_ACCESS	NULL		/* global access control spec */

/*
 * Event data is pushed to clients asynchronously from event
 * collection.  Data is buffered in "i/o buffers" that are
 * passed to a per-connection "push thread" that does the writes.
 * Each client is initially allocated 2 buffers for reach
 * CPU that has event collection enabled on with the number
 * of buffers dynamically grown as required up to a maximum
 * number.  Push buffers are all the same size.
 */
#define	_CONFIG_MAXIOBUFS	5		/* max buffers per client/CPU */
#define	_CONFIG_IOBUFSIZ	16*1024		/* push buffer size */

/*
 * When merging events from the user and kernel queues, if no
 * events are in one queue, we start taking events out of the
 * other queue until we are within TSTAMP_TIME_THRESHOLD ms of
 * the most recent event's time or we have more than
 * TSTAMP_NUMB_THRESHOLD events in the buffer.  We do this to
 * avoid getting an "old event" too late to properly merge it.
 *
 * NB: We only delay events in this way if we might receive
 *     events through the user queue.
 */
#define TSTAMP_TIME_THRESHOLD	10
#define TSTAMP_NUMB_THRESHOLD	100

/*
 * When no new events are available in either the user or
 * kernel queues we do a syssgi call to wait for a period
 * of time or for the kernel queue to reach half full.
 */
#define	_CONFIG_WAITTIME	100		/* 100ms polling interval */
/*
 * Interval for issuing "null records" to help clients in merging
 * event streams from multiple CPUs.  If a client hasn't received
 * any data for this period of time then we send it a SORECORD event
 * with no real events.  The client can then use the timestamp info
 * in the record to trigger event merging.  If we didn't do this
 * then clients might have to buffer very large amounts of data to
 * insure merged events are properly ordered by time.
 */
#define	_CONFIG_QUIETTIME	2*100		/* 200ms between pings */

#ifdef USE_SPROC
/*
 * When using sprocs we are limited to a fixed max number
 * of simultaneous clients because we have to do a usconfig
 * call at startup that specifies the max number of sprocs
 * we'll use.  Since we create one sproc per client/connection
 * this sets an upper bound.  Note that this isn't the case
 * when using pthreads since the library dynamically creates
 * execution contexts for us.
 */
#define	_SPROC_MAX_CLIENTS		10	/* max simultaneous clients */
/*
 * sproc-based event collection threads are given a fixed-size
 * stack to minimize the resources consumed.  This is important
 * on larger systems.
 */
#define	_SPROC_EVTHREAD_STACKSIZE	32*1024		/* 32KB */
/*
 * Stack size for sproc-based "push" threads that implement the
 * async writes of event data to clients.
 */
#define	_SPROC_IOPUSH_STACKSIZE		32*1024		/* 32KB */
#endif
#endif /* _CONFIG_ */
