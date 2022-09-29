#ifndef _RTMON_DECODE_
#define	_RTMON_DECODE_

#include <sys/types.h>
#include <sys/rtmon.h>
#include <stdio.h>

typedef struct {
    int		mystorage;	/* 1 if allocated by rtmon_beginDecode */
    __int64_t	starttime;	/* start time of event stream */
    __int64_t	lasttime;	/* last event time for delta times */
    __uint64_t	tstampadj;	/* adjustment for 32-bit wrapped time vals */
    __uint64_t	tstampfreq;	/* tstamp frequency per config */
    float	tstampmulf;	/* floating version of ms divisor */
    int		kabi;		/* kernel ABI for decoding syscall args */

    struct kidentry** kidcache;	/* cache of KID<->process names */
    struct kidentry* lastkid;	/* last KID looked up */
} rtmonDecodeState;

#define	_rtmon_adjtv(rs,t)	((t)+(rs)->tstampadj)
#define	_rtmon_toms(rs,t)	((1000ULL*(t))/(rs)->tstampfreq)
#define	_rtmon_tomsf(rs,t)	((float)(t)*(rs)->tstampmulf)
#define	_rtmon_tous(rs,t)	((1000000ULL*(t))/(rs)->tstampfreq)

extern	rtmonDecodeState* rtmon_decodeBegin(rtmonDecodeState*);
extern	void rtmon_decodeEnd(rtmonDecodeState*);

extern	void rtmon_decodeEventBegin(rtmonDecodeState*,
		const tstamp_event_entry_t* ev);
extern	void rtmon_decodeEventEnd(rtmonDecodeState*,
		const tstamp_event_entry_t* ev);
extern	const char* rtmon_pidLookup(rtmonDecodeState*, pid_t);

extern	void* _rtmon_pid_getdata(rtmonDecodeState*, pid_t);
extern	void _rtmon_pid_setdata(rtmonDecodeState*, pid_t, void*, void(*)(void*));
extern	void _rtmon_pid_clrdata(rtmonDecodeState*, pid_t);
extern	void _rtmon_pid_trace(rtmonDecodeState*, pid_t);
extern	void _rtmon_pid_untrace(rtmonDecodeState*, pid_t);
extern	int _rtmon_pid_istraced(rtmonDecodeState*, pid_t);

typedef struct {
    rtmonDecodeState ds;	/* decoder state base class */
    const char*	qualfmt;	/* output base for raw event quals */
    __int64_t	starttime;	/* start time of printed events */
    __int64_t	lasttime;	/* last printed event time */

    int		flags;
#define	RTPRINT_BINARY		0x0001
#define	RTPRINT_ASCII		0x0002
#define	RTPRINT_INTERNAL	0x0004	/* display internal events */
#define	RTPRINT_ALLPROCS	0x0008	/* trace all process syscalls */
#define	RTPRINT_USEUS		0x0010	/* display time as ms+us */
#define	RTPRINT_SHOWCPU		0x0020	/* display CPU number */
#define	RTPRINT_SHOWPID		0x0040	/* display process name/PID */
#define	RTPRINT_INITSYS		0x0080	/* syscall trace needs setup */
#define RTPRINT_SHOWKID		0x0100  /* display process name/KID */	
    int		max_asciilen;	/* max chars in an ascii string */
    int		max_binlen;	/* max char of binary data to print */
    int		max_namelen;	/* max chars in process name string */
    int64_t	lastsyscall;	/* KID associated with last syscall */
    __int64_t	syscalldelta;	/* threshold for printing BEGIN+END */
    __uint64_t	eventmask;	/* mask of events to print */
} rtmonPrintState;

#define	rtmon_adjtv(rs,t)	_rtmon_adjtv(&(rs)->ds,t)
#define	rtmon_toms(rs,t)	_rtmon_toms(&(rs)->ds,t)
#define	rtmon_tomsf(rs,t)	_rtmon_tomsf(&(rs)->ds,t)
#define	rtmon_tous(rs,t)	_rtmon_tous(&(rs)->ds,t)

extern	rtmonPrintState* rtmon_printBegin(void);
extern	void rtmon_printEnd(rtmonPrintState* rs);

extern	int rtmon_setOutputBase(rtmonPrintState*, int base);
extern	int rtmon_getOutputBase(const rtmonPrintState*);
extern	void rtmon_traceProcess(rtmonPrintState*, pid_t);

extern	const char* _rtmon_tablookup(const char*, long);
extern	void _rtmon_printEventTime(rtmonPrintState*, FILE*, __int64_t, int);
extern	void _rtmon_printProcess(rtmonPrintState*, FILE*, pid_t);
extern	void _rtmon_showEvent(rtmonPrintState*, FILE*,
		const tstamp_event_entry_t*, pid_t);
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
extern	void _rtmon_checkkid(rtmonPrintState*, FILE*, int64_t, int force);

extern	int rtmon_settracebyname(rtmonPrintState* rs,
		const char* name, int onoff);
extern	int rtmon_settracebynum(rtmonPrintState* rs, int callno, int onoff);
extern	void rtmon_walksyscalls(rtmonPrintState* rs, 
		void (*f)(rtmonPrintState*, const char*, int, __int64_t));
#endif /* _RTMON_DECODE_ */
