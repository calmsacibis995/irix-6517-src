#ifndef _RTMOND_
#define	_RTMOND_

#include <sys/types.h>
#define	__RTMON_PROTO__
#include <rtmon.h>
#include <ulocks.h>
#include <semaphore.h>
#ifndef USE_SPROC
#include <pthread.h>
#endif
#include <syslog.h>
#include <errno.h>
#include <assert.h>

#include "config.h"

#define FALSE	0
#define TRUE	1

typedef	struct daemon_info daemon_info_t;
typedef struct client client_t;
typedef struct kidentry kidentry_t;
typedef	struct ioblock ioblock_t;

/*
 * Client-server protocol operations are handled by
 * routines called indirectly through a protosw_t
 * handle.  Note that all events are buffered in
 * per-client buffers and written asynchronously
 * except for the config event which must arrive
 * first in the event stream for clients that collate
 * event streams from multiple CPUs.
 */
typedef struct {
    int	proto;				/* protocol number */
    const char* name;			/* protocol name */
    void (*initClient)(daemon_info_t*, client_t*);
    void (*initIOBlock)(daemon_info_t*, ioblock_t*);
    void (*writeHeader)(daemon_info_t*, client_t*, uint64_t);
    void (*flush)(daemon_info_t*, client_t*, uint64_t, uint64_t);
    void (*kernelEvent)(daemon_info_t*, client_t*, const tstamp_event_entry_t*);
    void (*userEvent)(daemon_info_t*, client_t*, const tstamp_event_entry_t*);
    void (*lostEvent)(daemon_info_t*, client_t*, __int64_t, int lost_events);
    void (*writeTaskName)(daemon_info_t*, client_t*, int64_t kid, int64_t tv);
} protosw_t;

extern	void registerProtocol(protosw_t* sw);
extern	protosw_t* findProtocol(int proto);

/*
 * Clients setup one or more IPC connections to the server
 * and register interest in classes of events for one or
 * more CPUs.  The server then returns matching events to
 * the client formatted according to the protocol specified
 * at the time the connection is setup.  The level of
 * multiplexing varies according to the ancestry of the
 * client application.  Old programs use one connection per
 * CPU while newer programs can request that events for
 * multiple CPUs be returned on a single connection.  The
 * exact scheme is specified through control requests.
 */

/*
 * Clients receive names for threads that are
 * referenced in their event stream.  Names are sent once,
 * in-band, prior to their first reference.   We track the
 * names that have been sent on a per-client basis to avoid
 * a central data structure that requires locking.  The
 * table of PIDs that have had their name sent to the client
 * is managed as a list of blocks with the first block stored
 * directly in the client state block.
 */
typedef struct kidblock {
    struct kidblock* next;
    int64_t	kids[128];		/* table of PIDs handled */
} kidblock_t;

/*
 * One instance of a client_t state block exists for each
 * event collection thread sending data to a client.  Info
 * common to all such clients is stored in a shared state
 * block that is reference counted.
 *
 * Each client connection has a thread that pushes event
 * data (i.e. does write calls on the file descriptor).
 * Event collection data is copied to io buffers which
 * are then passed to the push thread.  Using a single
 * thread serializes writes (which the kernel does not
 * do) and the separate threads decouples event collection
 * from event delivery.  This asynchronous i/o mechanism
 * also means that a slow or stopped client won't impact
 * other active clients.
 */
typedef struct client_common {
    volatile int refs;			/* reference count */
    int		fd;			/* client file descriptor */
    int		maxindbytes;		/* max # indirect bytes for syscalls */
    uint64_t	cookie;			/* connection cookie */
    uint64_t	events;			/* mask of interesting events */
    uint64_t	evmask;			/* access control mask */
    protosw_t*	proto;			/* protocol implementation handle */
    uint64_t	cpus[2];		/* mask of CPUs being traced */
    const char*	host;			/* peer host identity */
    int		port;			/* peer port number */
#ifdef USE_SPROC
    pid_t	ioproc;			/* push thread */
#else
    pthread_t	ioproc;			/* push thread */
#endif
    ioblock_t*	iohead;			/* head of push queue */
    ioblock_t*	iotail;			/* tail of push queue */
    sem_t	iolock;			/* lock on push queue */
    sem_t	ioq;			/* queue sleep/wakeup sync */
    uint64_t	iototal;		/* total bytes of push i/o */
    uint64_t	iotime;			/* total ticks for push i/o */
    uint64_t	iomax;			/* max time for push i/o op */
    uint64_t	iomin;			/* min time for push i/o op */
    int		iocnt;			/* count of push i/o ops */
    int		ioactive;		/* if TRUE, push i/o permitted */
    struct client_common* next;		/* global list of clients */
} client_common_t;

struct client {
    client_t*	next;			/* per-CPU list of active clients */
    client_common_t* com;		/* info shared by all instances */
    /*
     * The following items are copied from the common
     * area to avoid extra pointer deferences.  If any
     * of these items become changeable over the lifetime
     * of a client setup (e.g. the event mask) then they
     * should be referenced directly from the common area.
     */
    uint64_t	cookie;			/* connection cookie */
    uint64_t	events;			/* mask of interesting events */
    protosw_t*	proto;			/* protocol implementation handle */
    const char*	host;			/* peer host identity */
    int		port;			/* peer port number */
    /*
     * The remaining elements of this structure are
     * private to each client.  Note that i/o buffer
     * space is allocated separately as one or more
     * ioblock_t's that is (initially) placed on a free
     * list.  A client is initially given two buffers
     * with more allocated on demand up to a maximum
     * number than can be specified when the server
     * is started up.  Filled buffers are handed to
     * the push thread who returns them to the client's
     * free list after the data has been sent.
     */
    uint64_t	lastevt;		/* time of last event q'd to client */
    int64_t	lastkid;		/* last pid looked up */
    kidblock_t	kids;			/* table of pids with sent names */
    u_long	kevents;		/* # events sent to client */
    u_long	kdrops;			/* # events dropped for lack of space */
    u_long	writes;			/* # writes to client */
    u_long	pushes;			/* # writes due to timeout push */
    uint64_t	totbytes;		/* # bytes sent to client */
    size_t	lowmark;		/* low water mark on buffer */
    ioblock_t*	io;			/* current output buffer */
    ioblock_t*	iofree;			/* free list of io buffers */
    sem_t	iofreelock;		/* lock on free list */
    uint64_t	lastpush;		/* time of last i/o buffer push */
    int		niobufs;		/* # push buffers allocated */
};

/*
 * I/O (really only "O") data buffer.  Event data is
 * copied to these buffers as it comes from the kernel
 * and is formatted according to the per-protocol
 * requirements.  Buffers are handed to per-client
 * "push threads" that write the data asynchronously
 * and then place the buffers back on the per-client
 * free lists.  This mechanism serializes data (since
 * writes are done synchronously) and decouples data
 * transfer from event collection.  If output gets
 * behind event collection then the events are dropped
 * for the client.
 */
struct ioblock {
    ioblock_t*	next;			/* free list or push queue */
    client_t*	cp;			/* owner */
    size_t	off;			/* offset into buffer */
    size_t	size;			/* size of data buffer */
    union {
	tstamp_event_entry_t* ev;
	char*	buf;
    } u;				/* data is kept separate */
};

/*
 * Macros to simplify per-client write buffering operations.
 */
#ifdef notdef
    /*
     * We'd like to use the following macro because it's simple and doesn't
     * require the ``{}'' that the one following it does, but the
     * -TENV:misalignment=3 switch that we were using under the Ragnarok
     * compiler doesn't seem to be working under the Mongoose 7.00MR
     * compilers (see bug #404891).
     */
    #define WRITE_BUF(io, type, value) \
        (*(type *)((io)->u.buf + (io)->off) = (type)(value), \
         (io)->off += sizeof (type))
#else
    /*
     * The following macro works under both Ragnarok and Mongoose 7.00MR
     * because we explicitly deal with the misalignment issues.  Since
     * we know that our buffer pointer will always be aligned on a two-byte
     * boundary, we copy in two-byte chunks.
     */
    #define WRITE_BUF(io, type, value) \
        { \
            type valbuf = (type)(value); \
            int16_t* vp = (int16_t*) &valbuf; \
	    int16_t* bp = (int16_t*) ((io)->u.buf + (io)->off); \
            int i; \
	    assert(((__psunsigned_t) bp & 1) == 0); \
            for (i = 0; i < sizeof (type)/sizeof (*bp) ; i++) \
                bp[i] = vp[i]; \
	    (io)->off += sizeof (type); \
        }
#endif
#define WRITE_BUF_MEMCPY(io, type, valuep) \
    (memcpy((io)->u.buf + (io)->off, (valuep), sizeof (type)), \
     (io)->off += sizeof (type))
#define WRITE_BUF_ALIGNED(io, type, value) \
    (*(type *)((io)->u.buf + (io)->off) = (value), (io)->off += sizeof (type))
#define WRITE_BUF_VARIABLE(io, len, valuep) \
    (memcpy((io)->u.buf + (io)->off, (valuep), (len)), (io)->off += (len))

/*
 * Per-thread information.  One or more threads are created
 * to read events from the kernel and format them for transmission
 * to clients (according to the supported client-server protocols).
 * Thread state includes (memory-mapped) references to shared
 * kernel data structures, the list of clients being serviced,
 * and other information.
 *
 * It would be nice to control the alignment of this
 * data structure to avoid cache thrashing.
 */
struct daemon_info {
#ifndef USE_SPROC
    pthread_t	pthread;	/* POSIX thread handle */
#endif
    volatile int isrunning;	/* thread is currently running */
    int		cpu;		/* CPU# of this CPU */
    int		cpu_type;	/* processor type; e.g. R10K */
    uint	eobmode;	/* eob mode inherited at collection startup */
    uint64_t	omask;		/* mask inherited at collection startup */
    uint64_t	mask;		/* union of events all clients want */
    kern_queue_t kern_queue;	/* mmap'd kernel event queue */
    int		kern_count;
    int		kern_index;
    user_queue_t* user_queue;	/* shared user event queue */
    int		user_count;
    int		user_index;
    volatile client_t* clients;	/* active clients for this CPU */
    sem_t	clients_sem;	/* for clients accesses */
    client_t*	paused;		/* paused clients for this CPU */
    kidentry_t** kidcache;	/* cache of PID<->process names */
    kidentry_t*	lastkid;	/* last PID looked up */
    uint64_t	kevents;	/* # kernel events processed */
    uint64_t	uevents;	/* # user events processed */
    uint64_t	tstampwaits;	/* # times waiting for new tstamps */
    uint64_t	kdelayed;	/* # kernel events w/ delayed processing */
    uint64_t	udelayed;	/* # user events w/ delayed processing */
    uint64_t	klost;		/* total # kernel events lost */
    uint64_t	ulost;		/* total # user events lost */
    u_long	clientwrites;	/* # writes to clients */
    uint64_t	clientdata;	/* total # bytes written to clients */
};

extern	int trace;
#define	TRACE_CLIENT	0x00001		/* trace client state operations */
#define	TRACE_RPC	0x00002		/* trace RPC requests */
#define	TRACE_PERF	0x00004		/* trace tstamp performance stats */
#define	TRACE_THREAD	0x00008		/* trace thread-related actions */
#define	TRACE_EVENTS	0x00010		/* trace low-level event processing */
#define	TRACE_EVENTIO	0x00020		/* trace event xmit to clients */
#define	TRACE_TSTAMP	0x00040		/* trace tstamp operations */
#define	TRACE_SYNC	0x00080		/* trace sync event handling */
#define	TRACE_KID	0x00100		/* trace pid handling */
#define	TRACE_ACCESS	0x00200		/* trace access control work */
#define	TRACE_LOSTEVENTS 0x00400	/* trace lost events */
#define	TRACE_DEBUG	0x00800		/* trace debugging info */

#define	IFTRACE(v)	if (trace & TRACE_##v) Trace

extern	void Trace(daemon_info_t*, const char* message, ...);
extern	void Log(int priority, daemon_info_t*, const char* message, ...);
extern	void Fatal(daemon_info_t*, const char* message, ...);

extern	uint64_t parse_event_str(const char* arg);
extern	void disallow_tracing(daemon_info_t*);
extern	int setmaxindbytes(int);
extern	uint getncpu(void);
extern	uint getschedpri(void);
extern	daemon_info_t* getdaemoninfo(int cpu);

extern	void init_thread();
extern	void new_cpu_mask(daemon_info_t *dp);
extern	void startMerge(daemon_info_t* dp);

extern	void init_network(int port, const char* sockname);
extern	void network_cleanup(void);
extern	void network_run(void);
struct in_addr;
uint64_t check_access(const char* hostname, struct in_addr in);

extern	void kid_init(daemon_info_t*);
extern	void kid_flush(daemon_info_t*);
extern	void kid_purge_kid(daemon_info_t*, int64_t);
extern	void kid_check_kid(daemon_info_t*, int64_t, int64_t);
extern	void kid_lookup_kid(daemon_info_t*, int64_t, pid_t*, char*, size_t*);
extern	void kid_rename_kid(daemon_info_t*, int64_t, const char*, int64_t,pid_t);
extern	void kid_dup_kid(daemon_info_t*, int64_t, int64_t);

extern	void init_io(void);
extern	int io_client_init(daemon_info_t* dp, client_t* cp);
extern	int io_client_resume(daemon_info_t* dp, client_t* cp);
extern	void io_client_cleanup(daemon_info_t* dp, client_t* cp);
extern	void io_client_pause(daemon_info_t* dp, client_t* cp);
extern	int io_com_init(client_common_t* com);
extern	int io_com_resume(client_common_t* com);
extern	void io_com_cleanup(daemon_info_t* dp, client_common_t* com);
extern	void io_com_pause(daemon_info_t* dp, client_common_t* com);
extern	void io_post(daemon_info_t* dp, client_t*);
extern	ioblock_t* io_new_buf(daemon_info_t*, client_t*);

extern	void init_client(void);
extern	void purge_client(client_common_t*);
extern	client_common_t* find_client(int fd);
struct sockaddr;
extern	client_common_t* client_create(int fd, struct sockaddr* sockname);
extern	int client_open(client_common_t* com, uint64_t cookie);
extern	int client_setparams(client_common_t* com, rtmon_cmd_params_t* params);
extern	int client_suspend(client_common_t* com);
extern	int client_resume(client_common_t* com);
extern	void client_start(client_common_t*);
#endif /* _RTMOND_ */
