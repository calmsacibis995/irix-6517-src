#ifndef _UTIL_
#define	_UTIL_

#include <sys/types.h>
#include <rtmon.h>
#include <sys/rtmon.h>

#define	RTMON_PAR \
   RTMON_SIGNAL|RTMON_SYSCALL|RTMON_TASK|RTMON_DISK|\
   RTMON_NETSCHED|RTMON_NETFLOW

#define	MAXCPU	128		/* max number of cpus we can handle */

extern	int numb_processors;
extern	const char* appName;
extern	const char* hostname;

extern	void fatal(const char* fmt, ...);
extern	void parse_proc_str(const char*, uint64_t[]);
extern	uint64_t parse_event_str(const char*);

extern  void log_event(tstamp_event_entry_t *ev);

#endif /* _UTIL_ */
