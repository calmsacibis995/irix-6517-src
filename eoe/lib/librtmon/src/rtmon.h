#ifndef __RTMON_H__
#define __RTMON_H__
#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Support for collecting, logging, decoding, and printing
 * tstamp events.  Definitions for all user-level code are
 * found here; all system-level definitions (including the
 * format of events) are located in <sys/rtmon.h>.
 */
#include <sys/types.h>
#include <sys/rtmon.h>
#include <stdio.h>

/*
 * Support for applications that wish to log events.
 */

/*
 * Name of shared memory area used to pass user-level
 * events via rtmon_log_user_tstamp.
 */
#define RTMON_SHM_FILENAME	"/usr/tmp/.rtmond_shm_file"

/*
 * Interfaces for collecting CPU event data from rtmond.
 *
 * These routines allow users to setup CPU event data connections,
 * suspend and resume those connections, etc. without having to
 * worry about the client-server protocol.  Clients establish a
 * connection to the server with rtmond_open, setup any state/parameters
 * they want in the rtmond_t state structure, and then use either
 * rtmond_start_cpu or rtmond_start_ncpu to start event collection.
 * rtmond_suspend_data and rtmond_resume_data can be used to stop
 * and start event collection for an established connection.
 */
typedef struct { 		/* rtmond connection state */
    int		socket;		/* connection to server */
    uint	protocol;	/* event stream format */
#define	RTMON_PROTOCOL1	1	/* old WindView protocol */
#define	RTMON_PROTOCOL2	2	/* raw tstamp_event_entry data */
    uint	ncpus;		/* # CPUs on server */
    uint	maxindbytes;	/* max # indirect bytes for syscall params */
    uint	schedpri;	/* realtime scheduling priority of server */
    uint64_t	cookie;		/* connection cookie */
    uint64_t	mask;		/* event mask */
#define	RTMOND_MAXCPU	128	/* max # CPU's supported */
    uint64_t	cpus[2];	/* CPU mask */
    uint64_t	spare[10];	/* pad to 128 bytes */
} rtmond_t;

extern	rtmond_t* rtmond_open(const char* hostname, int port);
extern	int rtmond_start_ncpu(rtmond_t*,
		uint /*ncpu*/, const uint64_t[] /*cpumask*/,
		uint64_t /*eventmask*/);
extern	int rtmond_start_cpu(rtmond_t*, uint /*cpu*/, uint64_t /*evmask*/);
extern	int rtmond_suspend_data(rtmond_t*);
extern	int rtmond_resume_data(rtmond_t*);
extern	void rtmond_close(rtmond_t*);

/*
 * Interfaces for collecting CPU event data from an IRIX 6.4 rtmond.  These
 * are purely for backwards compatibility.  They are not likely to stay
 * around.
 *
 * The general mode of operation is that the client creates a socket to
 * accept connections on and then sends RPC messages to an rtmond requesting
 * that the rtmond connect to the socket with event data for a particular
 * CPU.  In order for this to work efficiently for both the single and
 * multiple connection cases we break up the work into 1. setting up control
 * state which connects us to an rtmond and 2. establishing CPU event data
 * connections using that control state.  (We use ``struct __client_s *'' so
 * we don't have to include RPC header files ...)
 */
typedef struct { /* rtmond64 control state */
    int socket;			/* socket to accept connections on */
    int port;			/* socket's local port # (network format) */
    int protocol;		/* rtmon protocol to use for connections */
    struct __client_s *client;	/* RPC client connection to rtmond64 */
} rtmond64_t;

extern int rtmond64_setup_control(rtmond64_t */*rtmond64*/,
				  const char */*hostname*/, int /*protocol*/);
extern void rtmond64_teardown_control(rtmond64_t */*rtmond64*/);
extern int rtmond64_get_ncpu(rtmond64_t */*rtmond64*/);
extern int rtmond64_get_connection(rtmond64_t */*rtmond64*/,
				   int /*cpu*/, uint64_t /*eventmask*/);
extern int rtmond64_get_CPU_connection(const char */*hostname*/, int /*protocol*/,
				       int /*cpu*/, uint64_t /*eventmask*/);
extern int rtmond64_suspend_connection(rtmond64_t */*rtmond64*/, int /*cpu*/);
extern int rtmond64_resume_connection(rtmond64_t */*rtmond64*/, int /*cpu*/);


#ifdef __RTMON_PROTO__
/*
 * rtmond client-server protocol definitions.
 */
#define	RTMON_CMD_OPEN		1
#define	RTMON_CMD_PARAMS	2
#define	RTMON_CMD_RESUME	3
#define	RTMON_CMD_SUSPEND	4
#define	RTMON_CMD_CLOSE		5

#define	RTMOND_DEFPORT		1455			/* default INET port */
#define	RTMOND_UNIXSOCKET	"/tmp/.rtmond_socket"	/* socket pathname */

typedef struct {
    u_short	ncpus;
    u_short	maxindbytes;
    u_short	schedpri;
    u_short	pad;
    uint64_t	spare[2];
} rtmon_cmd_prologue_t;

typedef struct {
    u_short	protocol;
    u_short	maxindbytes;
    u_short	ncpus;
    u_short	pad;
    uint64_t	events;   
    uint64_t	cpus[2];
    uint64_t	spare[4];
} rtmon_cmd_params_t;
#endif /* __RTMON_PROTO__ */

/*
 * Event stream decoding support.  An application that
 * wants to decode and process, but not print, events
 * can use this low-level support to handle decoding
 * CPU configuration events and KID<->name mapping events.
 */
typedef struct {
    int		mystorage;	/* 1 if allocated by rtmon_beginDecode */
    int64_t	starttime;	/* start time of event stream */
    int64_t	lasttime;	/* last event time for delta times */
    uint64_t	tstampadj;	/* adjustment for 32-bit wrapped time vals */
    uint64_t	tstampfreq;	/* tstamp frequency per config */
    float	tstampmulf;	/* floating version of ms divisor */
    int		kabi;		/* kernel ABI for decoding syscall args */
    uint	ncpus;		/* number of CPUs on target system */

    struct kidentry** kidcache;	/* cache of PID<->process names */
    struct kidentry* lastkid;	/* last KID looked up */
} rtmonDecodeState;

#define	_rtmon_adjtv(rs,t)	((t)+(rs)->tstampadj)
#define	_rtmon_toms(rs,t)	((1000ULL*(t))/(rs)->tstampfreq)
#define	_rtmon_tomsf(rs,t)	((float)(t)*(rs)->tstampmulf)
#define	_rtmon_tous(rs,t)	((1000000ULL*(t))/(rs)->tstampfreq)
#define	_rtmon_ncpus(rs)	((rs)->ncpus)

extern	rtmonDecodeState* rtmon_decodeBegin(rtmonDecodeState*);
extern	void rtmon_decodeEnd(rtmonDecodeState*);

extern	void rtmon_decodeEventBegin(rtmonDecodeState*,
		const tstamp_event_entry_t* ev);
extern	void rtmon_decodeEventEnd(rtmonDecodeState*,
		const tstamp_event_entry_t* ev);
extern	const char* rtmon_kidLookup(rtmonDecodeState*, int64_t);
extern  const char* rtmon_namebypid(rtmonDecodeState*, pid_t);
extern	pid_t rtmon_pidLookup(rtmonDecodeState*, int64_t);

extern	void* _rtmon_kid_getdata(rtmonDecodeState*, int64_t);
extern	void _rtmon_kid_setdata(rtmonDecodeState*, int64_t, void*, void(*)(void*));
extern	void _rtmon_kid_clrdata(rtmonDecodeState*, int64_t);
extern	void _rtmon_pid_trace(rtmonDecodeState*, pid_t);
extern	void _rtmon_kid_trace(rtmonDecodeState*, int64_t);
extern	void _rtmon_pid_untrace(rtmonDecodeState*, pid_t);
extern	int _rtmon_pid_istraced(rtmonDecodeState*, pid_t);
extern	int _rtmon_kid_istraced(rtmonDecodeState*, int64_t);

/*
 * Event stream printing support (layered on top
 * of the above decoding support).  Applications
 * typically use this interface and not the above
 * interface.
 */
typedef struct {
    rtmonDecodeState ds;	/* decoder state base class */
    const char*	qualfmt;	/* output base for raw event quals */
    int64_t	starttime;	/* start time of printed events */
    int64_t	lasttime;	/* last printed event time */

    int		flags;
#define	RTPRINT_BINARY		0x0001
#define	RTPRINT_ASCII		0x0002
#define	RTPRINT_INTERNAL	0x0004	/* display internal events */
#define	RTPRINT_ALLPROCS	0x0008	/* trace all process syscalls */
#define	RTPRINT_USEUS		0x0010	/* display time as ms+us */
#define	RTPRINT_SHOWCPU		0x0020	/* display CPU number */
#define	RTPRINT_SHOWPID		0x0040	/* display process name/PID */
#define	RTPRINT_INITSYS		0x0080	/* syscall trace needs setup */
#define RTPRINT_SHOWKID		0x0200  /* display process name/KID */	
    int		max_asciilen;	/* max chars in an ascii string */
    int		max_binlen;	/* max char of binary data to print */
    int		max_namelen;	/* max chars in process name string */
    int64_t	lastsyscall;	/* KID associated with last syscall */
    int64_t	syscalldelta;	/* threshold for printing BEGIN+END */
    uint64_t	eventmask;	/* mask of events to print */
} rtmonPrintState;

#define	rtmon_adjtv(rs,t)	_rtmon_adjtv(&(rs)->ds,t)
#define	rtmon_toms(rs,t)	_rtmon_toms(&(rs)->ds,t)
#define	rtmon_tomsf(rs,t)	_rtmon_tomsf(&(rs)->ds,t)
#define	rtmon_tous(rs,t)	_rtmon_tous(&(rs)->ds,t)
#define	rtmon_ncpus(rs)		_rtmon_ncpus(&(rs)->ds)

extern	rtmonPrintState* rtmon_printBegin(void);
extern	void rtmon_printEnd(rtmonPrintState* rs);

extern	int rtmon_setOutputBase(rtmonPrintState*, int base);
extern	int rtmon_getOutputBase(const rtmonPrintState*);
extern	void rtmon_traceProcess(rtmonPrintState*, pid_t);
extern	void rtmon_traceKid(rtmonPrintState*, int64_t);

extern	const char* _rtmon_tablookup(const char*, long);
extern	void _rtmon_printEventTime(rtmonPrintState*, FILE*, int64_t, int);
extern	void _rtmon_printProcess(rtmonPrintState*, FILE*, int64_t);
extern	void _rtmon_showEvent(rtmonPrintState*, FILE*,
		const tstamp_event_entry_t*, int64_t);
extern	void _rtmon_printEvent(rtmonPrintState*, FILE*,
		const tstamp_event_entry_t*, const char*, ...);

extern	int rtmon_printEvent(rtmonPrintState*,
		FILE* fd, const tstamp_event_entry_t* ev);
extern	void rtmon_printRawEvent(rtmonPrintState*,
		FILE*, const tstamp_event_entry_t*);

extern	void _rtmon_syscallBegin(rtmonPrintState*);
extern	void _rtmon_syscallEnd(rtmonPrintState*);
extern	void _rtmon_syscallEventBegin(rtmonPrintState*,
		FILE* fd, const tstamp_event_entry_t* ev);
extern	void _rtmon_syscallEventEnd(rtmonPrintState*,
		FILE* fd, const tstamp_event_entry_t* ev);
extern	void _rtmon_checkpid(rtmonPrintState*, FILE*, pid_t, int force);

extern	int rtmon_settracebyname(rtmonPrintState* rs,
		const char* name, int onoff);
extern	int rtmon_settracebynum(rtmonPrintState* rs, int callno, int onoff);
extern	void rtmon_walksyscalls(rtmonPrintState* rs, 
		void (*f)(rtmonPrintState*, const char*, int, int64_t));
#if defined(__cplusplus)
}
#endif
#endif /* __RTMON_H__ */
