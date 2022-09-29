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

#include <sys/rtmon.h>
#include <sys/par.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/kabi.h>
#include <sys/param.h>
#include <sys/prf.h>
#include <sys/sysmacros.h>

#include "rtmon.h"

/*
 * Decode and print RTMON events.
 */
#define	TRUE	1
#define	FALSE	0

#define	kid_lookup(rs, kid)	rtmon_kidLookup(&rs->ds, kid)
#define pid_lookup(rs, kid)	rtmon_pidLookup(&rs->ds, kid)

#define	iskidtraced(rs,kid) \
    (((rs)->flags&RTPRINT_ALLPROCS) != 0 || _rtmon_kid_istraced(&(rs)->ds, (int64_t) kid))

#define	ispidtraced(rs,pid) \
    (((rs)->flags&RTPRINT_ALLPROCS) != 0 || _rtmon_pid_istraced(&(rs)->ds, (pid_t) pid))

static	const char hexfmt[] = "\tqual[%2d] %#018llx (%#010lx %#010lx)\n";
static	const char octfmt[] = "\tqual[%2d] %#018llo (%#010lo %#010lo)\n";
static	const char decfmt[] = "\tqual[%2d] %#018lld (%#010ld %#010ld)\n";

int
rtmon_setOutputBase(rtmonPrintState* rs, int base)
{
    switch (base) {
    case 8:	rs->qualfmt = octfmt; break;
    case 10:	rs->qualfmt = decfmt; break;
    case 16:	rs->qualfmt = hexfmt; break;
    default:	return (FALSE);
    }
    return (TRUE);
}
int
rtmon_getOutputBase(const rtmonPrintState* rs)
{
    return rs->qualfmt == octfmt ? 8
	:  rs->qualfmt == decfmt ? 10
	:  rs->qualfmt == hexfmt ? 16
	:  -1;
}

void
rtmon_traceProcess(rtmonPrintState* rs, pid_t pid)
{
    rs->flags &= ~RTPRINT_ALLPROCS;
    _rtmon_pid_trace(&rs->ds, pid);
}
void
rtmon_traceKid(rtmonPrintState* rs, int64_t kid)
{
    rs->flags &= ~RTPRINT_ALLPROCS;
    _rtmon_kid_trace(&rs->ds, kid);
}

void
_rtmon_printEventTime(rtmonPrintState* rs, FILE* fd, __int64_t tv, int cpu)
{
    if (rs->starttime == 0)
	rs->starttime = rs->lasttime = tv;
    if (rs->flags & RTPRINT_USEUS)
	fprintf(fd, "%9.3fmS(+%5llduS)"
	    , rtmon_tomsf(rs, tv - rs->starttime)
	    , rtmon_tous(rs, tv - rs->lasttime)
	);
    else
	fprintf(fd, "%5lldmS", rtmon_toms(rs, tv - rs->starttime));
    if (rs->flags & RTPRINT_SHOWCPU)
	fprintf(fd, "[%3d]", cpu);
    rs->lasttime = tv;
}

void
_rtmon_printProcess(rtmonPrintState* rs, FILE* fd, int64_t kid)
{
    if (rs->flags & RTPRINT_SHOWPID) {
	    if (kid != (int64_t) -1) {
		    if (rs->flags & RTPRINT_SHOWKID)
			    fprintf(fd, "%16.*s(%d:%lld):",
				    rs->max_namelen, kid_lookup(rs, kid), pid_lookup(rs,kid), kid);
		    else {
			    pid_t pid;
			    pid = pid_lookup(rs,kid);
			    fprintf(fd, "%16.*s(%5lld):",
				    rs->max_namelen, kid_lookup(rs, kid), pid? (int64_t)pid : kid);
		    }
		    return;
	    }
    } 
    fprintf(fd, "%17s:", " ");
}

void
_rtmon_showEvent(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev, int64_t kid)
{
    _rtmon_printEventTime(rs, fd, rtmon_adjtv(rs, ev->tstamp), ev->cpu);
    _rtmon_printProcess(rs, fd, kid);
    fputc(' ', fd);
}

static void
printRawData(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev)
{
    int i;

    for (i = 0; i < TSTAMP_NUM_QUALS; i++)
	fprintf(fd, rs->qualfmt
	    , i
	    , ev->qual[i]
	    , (ev->qual[i]>>32) & 0xffffffff
	    , ev->qual[i] & 0xffffffff
	);
    if (ev->jumbocnt > 0) {
	const __uint64_t* lp = (const __uint64_t*) &ev[1];
	int n = (ev->jumbocnt * sizeof (*ev)) / sizeof (*lp);
	do {
	    fprintf(fd, rs->qualfmt
		, i
		, *lp
		, (*lp>>32) & 0xffffffff
		, *lp & 0xffffffff
	    );
	    i++, lp++;
	} while (--n);
    }
}

void
_rtmon_printEvent(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev, const char* fmt, ...)
{
    va_list ap;

    _rtmon_showEvent(rs, fd, ev, (int64_t) -1);
    va_start(ap, fmt);
    vfprintf(fd, fmt, ap);
    va_end(ap);
}

static void
printConfigEvent(rtmonPrintState* rs, FILE* fd, const tstamp_config_event_t* config)
{
    _rtmon_printEvent(rs, fd, (const tstamp_event_entry_t*) config,
	"Config: revision %d cputype ", config->revision);
    switch (config->cputype) {
    case TSTAMP_CPUTYPE_MIPS:	fprintf(fd, "MIPS"); break;
    case TSTAMP_CPUTYPE_R3000:	fprintf(fd, "R3000"); break;
    case TSTAMP_CPUTYPE_R4000:	fprintf(fd, "R4000"); break;
    case TSTAMP_CPUTYPE_R4300:	fprintf(fd, "R4300"); break;
    case TSTAMP_CPUTYPE_R4400:	fprintf(fd, "R4400"); break;
    case TSTAMP_CPUTYPE_R4600:	fprintf(fd, "R4600"); break;
    case TSTAMP_CPUTYPE_R5000:	fprintf(fd, "R5000"); break;
    case TSTAMP_CPUTYPE_R8000:	fprintf(fd, "R8000"); break;
    case TSTAMP_CPUTYPE_R10000:	fprintf(fd, "R10000"); break;
    case TSTAMP_CPUTYPE_R12000:	fprintf(fd, "R12000"); break;
    default:	fprintf(fd, "<unknown> (%d)", config->cputype); break;
    }
    fprintf(fd, " cpufreq ");
    if (config->cpufreq != 0)
	fprintf(fd, "%d", config->cpufreq);
    else
	fprintf(fd, "<unknown>");
    fprintf(fd, " eventmask %#llx tstampfreq %lu",
	config->eventmask, (u_long) config->tstampfreq);
    switch (config->kabi) {
    case ABI_IRIX5:	fprintf(fd, " kabi IRIX5\n"); break;
    case ABI_IRIX5_64:	fprintf(fd, " kabi IRIX5_64\n"); break;
    case ABI_IRIX5_N32:	fprintf(fd, " kabi IRIX5_N32\n"); break;
    default:		fprintf(fd, " kabi %#x\n", config->kabi);
    }
}

static void
printIdleReasons(FILE* fd, int tkn)
{
    if (tkn & NO_STACK)
	fprintf(fd, " (no stack)");
    if (tkn & NO_SLOAD)
	fprintf(fd, " (not in core)");
    if (tkn & MUSTRUN)
	fprintf(fd, " (mustrun on other cpu)");
    if (tkn & NO_FP)
	fprintf(fd, " (fp not avail)");
    if (tkn & NO_GFX)
	fprintf(fd, " (gfx not avail)");
    if (tkn & EMPTY)
	fprintf(fd, " (runq is empty)");
}

static void
printIdleEvent(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev)
{
    _rtmon_checkkid(rs, fd, (int64_t) -1, 1);
    _rtmon_printEvent(rs, fd, ev, "idle");
    printIdleReasons(fd, (int) ev->qual[0]);
    fprintf(fd, "\n");
}

#define	RTMON_UNPACKPRI(v, f, p, bp) { \
    f = (int)(v>>32); p = (short)(v&0xffff); bp = (short)((v>>16)&0xffff); \
}

static void
printPriority(FILE* fd, int pri, int basepri)
{
    if (pri <= PZERO || basepri == 0)
	fprintf(fd, " pri = %d", pri);
    else
	fprintf(fd, " rtpri = %d", basepri);
}

static void
printSwitchEvent(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev)
{
    int64_t kid = (int64_t) ev->qual[0];

    if (iskidtraced(rs, kid)) {
	int flags, pri, basepri;

	_rtmon_checkkid(rs, fd, kid, 1);
	RTMON_UNPACKPRI(ev->qual[1], flags, pri, basepri);
	_rtmon_showEvent(rs, fd, ev, kid);
	fprintf(fd, "switch in");
	printPriority(fd, pri, basepri);
	printIdleReasons(fd, flags);
	fprintf(fd, "\n");
    }
}

static void
printTaskSwitchReasons(FILE* fd, int reasons, const tstamp_event_entry_t* ev)
{
    if (reasons & RESCHED_LOCK) {
	const char* locktype;
	switch (reasons) {
	case SEMAWAIT:	locktype = "sema"; break;
	case MUTEXWAIT:	locktype = "mutex"; break;
	case SVWAIT:	locktype = "sv"; break;
	case MRWAIT:	locktype = "mrlock"; break;
	default:	locktype = "unknown lock type"; break;
	}
	fprintf(fd, " (%s wait: %#llx", locktype, ev->qual[2]);
	if (ev->jumbocnt > 0) {
	    const tstamp_event_resched_data_t* rdp =
		(const tstamp_event_resched_data_t*) ev->qual;
	    if (rdp->wchanname[0])
		fprintf(fd, " [%.*s]",
		    (int) sizeof (rdp->wchanname), rdp->wchanname);
	}
	if (ev->qual[3])
	    fprintf(fd, ", %#llx", ev->qual[3]);
	fputc(')', fd);
    } else {
	switch (reasons) {
	case MUSTRUNCPU:fprintf(fd, " (mustrun)"); break;
	case GANGWAIT:	fprintf(fd, " (gang wait)"); break;
	case RESCHED_P:	fprintf(fd, " (preempted)"); break;
	case RESCHED_KP:fprintf(fd, " (kernel preemption)"); break;
	case RESCHED_Y:	fprintf(fd, " (yield)"); break;
	case RESCHED_S:	fprintf(fd, " (job ctrl|trace)"); break;
	case RESCHED_D:	fprintf(fd, " (exit)"); break;
	default:	fprintf(fd, " (unknown reasons %#x)", reasons); break;
	}
    }
}

static void
printStateChangeEvent(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev)
{
    int64_t kid = (int64_t) ev->qual[0];

    if (iskidtraced(rs, kid)) {
	int flags, pri, basepri;

	_rtmon_checkkid(rs, fd, kid, 1);
	RTMON_UNPACKPRI(ev->qual[1], flags, pri, basepri);
	_rtmon_showEvent(rs, fd, ev, kid);
	fprintf(fd, "switch out");
	printPriority(fd, pri, basepri);
	if (flags)
	    printTaskSwitchReasons(fd, flags, ev);
	fprintf(fd, "\n");
    }
}

static void
printEODispEvent(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev)
{
    int64_t kid = (int64_t) ev->qual[0];

    if (iskidtraced(rs, kid)) {
	int cpu, pri, basepri;

	_rtmon_checkkid(rs, fd, kid, 1);
	RTMON_UNPACKPRI(ev->qual[1], cpu, pri, basepri);
	_rtmon_showEvent(rs, fd, ev, kid);
	fprintf(fd, cpu == rtmon_ncpus(rs) ?
	    "set on runq" : "set on CPU[%u] runq", cpu);
	printPriority(fd, pri, basepri);
	fprintf(fd, "\n");
    }
}

static void
printDiskEvent(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev, const char* type)
{
    int64_t kid = (int64_t) ev->qual[0];

    if (iskidtraced(rs, kid)) {
	uint64_t flags = (uint64_t)ev->qual[1];
	dev_t dev = (dev_t)(ev->qual[2]>>32);
	unsigned count = (unsigned)(ev->qual[2]&0xffffffff);
	daddr_t blkno = (daddr_t)ev->qual[3];

	_rtmon_checkkid(rs, fd, kid, 1);
	_rtmon_showEvent(rs, fd, ev, kid);
	fprintf(fd, " %s; flags 0x%llx dev %d,%d count %d blkno %lld\n",
		type, flags, major(dev), minor(dev), count, blkno);
    }

}

static void
printAllocEvent(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev)
{
    _rtmon_checkkid(rs, fd, (int64_t) -1, 1);
    fprintf(fd, "heap \"%8.8s\"(%#llx, %#llx) called from %#llx\n",
	    (char *)&ev->qual[0], ev->qual[1], ev->qual[2], ev->qual[3]);
}

static void
printTaskNameEvent(rtmonPrintState* rs, FILE* fd, const tstamp_taskname_event_t* tev)
{
    int64_t kid = tev->k_id;

    if (iskidtraced(rs, kid) && (rs->flags & RTPRINT_INTERNAL)) {
	_rtmon_checkkid(rs, fd, kid, 1);
	_rtmon_showEvent(rs, fd, (const tstamp_event_entry_t*) tev, kid);
	fprintf(fd, "process name \"%s\"\n" , tev->name);
    }
}

static void
printForkEvent(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev)
{
    const tstamp_event_fork_data_t* fdp =
	(const tstamp_event_fork_data_t*) &ev->qual[0];
    pid_t pid = 0;
    if (iskidtraced(rs, (int64_t) fdp->pkid)) {
	_rtmon_checkkid(rs, fd, (int64_t) fdp->pkid, 1);
	if (rs->flags & RTPRINT_INTERNAL) {
	    _rtmon_showEvent(rs, fd, ev, (int64_t) fdp->pkid);
	    if (!(rs->flags & RTPRINT_SHOWKID)) {
		    pid = pid_lookup(rs, fdp->ckid);
	    }
	    fprintf(fd, "process fork; child %s %lld, name %s\n",
		    pid? "pid" : "kid",
		    pid ? (int64_t)pid :(int64_t) fdp->ckid,
		    fdp->name);
	}
    }

}

static void
printExecEvent(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev)
{
    const tstamp_event_exec_data_t* edp =
	(const tstamp_event_exec_data_t*) &ev->qual[0];
    if (iskidtraced(rs,  edp->k_id)) {
	_rtmon_checkkid(rs, fd, (int64_t) edp->k_id, 1);
	if (rs->flags & RTPRINT_INTERNAL) {
	    _rtmon_showEvent(rs, fd, ev, (int64_t) edp->k_id);
	    fprintf(fd, "process exec; now running %.*s\n", 
		(int) sizeof (edp->name), edp->name);
	}
    }
}

static void
printExitEvent(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev)
{
    int64_t kid = (int64_t) ev->qual[0];

        if (iskidtraced(rs, kid)) { 
		_rtmon_checkkid(rs, fd, kid, 1);
		if (rs->flags & RTPRINT_INTERNAL) {
			_rtmon_showEvent(rs, fd, ev, kid);
			fprintf(fd, "process exit\n");
		}
	}
}

#define	N(A)	(sizeof (A) / sizeof (A[0]))

static void
printSigSendEvent(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev)
{
    pid_t pid = (pid_t) ev->qual[0];

    if (ispidtraced(rs, pid)) {
	int signo = (int) ev->qual[2];
	_rtmon_printEventTime(rs, fd, rtmon_adjtv(rs, ev->tstamp), ev->cpu);
	fprintf(fd, "%16.*s(%5d):",
		rs->max_namelen, "", pid);
	fputc(' ', fd);
	fprintf(fd, "was sent signal %s\n",
		_rtmon_tablookup("signames", signo));
    }
}

static void
printSigRecvEvent(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev)
{
    int64_t kid = ev->qual[0];

    if (iskidtraced(rs, kid)) {
	__psint_t handler = (__psint_t) ev->qual[1];
	int signo = (int) ev->qual[2];

	_rtmon_checkkid(rs, fd, kid, 1);
	_rtmon_showEvent(rs, fd, ev, kid);
	fprintf(fd, "received signal %s", _rtmon_tablookup("signames", signo));
	if (handler)
	    fprintf(fd, " (handler %#lx)", handler);
	fputc('\n', fd);
    }
}

static const char* intType[] = {
    "1st level interrupt",
    "Interrupt level 1",
    "Interrupt level 2",
    "Interrupt level 3",
    "User level interrupt",
    "CPU yield interrupt",
    "CPU counter interrupt",
    "RTC counter interrupt",
    "PROF counter interrupt",
    "Group interrupt",
    "Interrupt level 10",
    "Inter-CPU interrupt",
    "Network interrupt",
    "Vertical sync interrupt"
};

static void
printInterruptEvent(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev)
{
    if (INT_LEVEL(ev->evt) < N(intType))
	_rtmon_printEvent(rs, fd, ev, "%s entry\n", intType[INT_LEVEL(ev->evt)]);
    else
	_rtmon_printEvent(rs, fd, ev, "interrupt level %d entry\n", INT_LEVEL(ev->evt));
}

static void
printInterruptExitEvent(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev)
{
    int level = INT_LEVEL((int) ev->qual[0]);

    if (level < N(intType))
	_rtmon_printEvent(rs, fd, ev, "%s exit\n", intType[level]);
    else
	_rtmon_printEvent(rs, fd, ev, "interrupt level %d exit\n", level);
}

static void
printProfStack(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev)
{
    int i;

    _rtmon_printEvent(rs, fd, ev, "Stack trace:");
    if (ev->qual[0] & PRF_STACKSTART)
	fprintf(fd, " [");
    for (i = 0; i < TSTAMP_NUM_QUALS; i++) {
	fprintf(fd, " %#llx", ev->qual[i] &~ (PRF_STACKSTART|PRF_STACKEND));
	if (ev->qual[i] & PRF_STACKEND) {
	    fprintf(fd, " ]\n");
	    return;
	}
    }
    fprintf(fd, " ...\n");
}

rtmonPrintState*
rtmon_printBegin(void)
{
    rtmonPrintState* rs = (rtmonPrintState*) malloc(sizeof (*rs));

    (void) rtmon_decodeBegin(&rs->ds);
    /* show everything by default */
    rs->flags = RTPRINT_ALLPROCS
	      | RTPRINT_SHOWCPU
	      | RTPRINT_SHOWPID;
    rs->qualfmt = decfmt;		/* base 10 */
    rs->starttime = 0;
    rs->lasttime = 0;
    rs->max_namelen = 16;
    rs->eventmask = (__uint64_t) -1;	/* print everything */
    _rtmon_syscallBegin(rs);

    return (rs);
}

void
rtmon_printEnd(rtmonPrintState* rs)
{
    rtmon_decodeEnd(&rs->ds);
    _rtmon_syscallEnd(rs);
    free(rs);
}

void
rtmon_printRawEvent(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev)
{
    rtmon_decodeEventBegin(&rs->ds, ev);
    _rtmon_showEvent(rs, fd, ev, (pid_t) -1);
    fprintf(fd, "event id %d (%#x) jumbocnt %d tstamp %llu\n",
	ev->evt, ev->evt, ev->jumbocnt, ev->tstamp);
    printRawData(rs, fd, ev);
    rtmon_decodeEventEnd(&rs->ds, ev);
}

int
rtmon_printEvent(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev)
{
    rtmon_decodeEventBegin(&rs->ds, ev);
    switch (ev->evt) {
    case EVENT_TIMER_SYNC:
	if (rs->flags & RTPRINT_INTERNAL)
	    _rtmon_printEvent(rs, fd, ev, "time sync\n");
	break;
    case TSTAMP_EV_TASKNAME:
	printTaskNameEvent(rs, fd, (const tstamp_taskname_event_t*) ev);
	break;
    case TSTAMP_EV_EXEC:
	printExecEvent(rs, fd, ev);
	break;
    case TSTAMP_EV_FORK:
	printForkEvent(rs, fd, ev);
	break;
    case TSTAMP_EV_CONFIG:
	if (rs->flags & RTPRINT_INTERNAL)
	    printConfigEvent(rs, fd, (const tstamp_config_event_t*) ev);
	break;
    case TSTAMP_EV_SORECORD:
	if (rs->flags & RTPRINT_INTERNAL)
	    _rtmon_printEvent(rs, fd, ev,
		"SORECORD cc %llu checksum %llu tlast %llu\n",
		ev->qual[0], ev->qual[1], ev->qual[2]);
	break;
    case TSTAMP_EV_EOSWITCH:
    case TSTAMP_EV_EOSWITCH_RTPRI:
	if (rs->eventmask & (RTMON_TASK|RTMON_TASKPROC))
	    printSwitchEvent(rs, fd, ev);
	break;
    case TSTAMP_EV_SIGSEND:
	if (rs->eventmask & RTMON_SIGNAL)
	    printSigSendEvent(rs, fd, ev);
	break;
    case TSTAMP_EV_SIGRECV:
	if (rs->eventmask & RTMON_SIGNAL)
	    printSigRecvEvent(rs, fd, ev);
	break;
    case TSTAMP_EV_EXIT:
	printExitEvent(rs, fd, ev);
	break;
    case EVENT_TASK_STATECHANGE:
	if (rs->eventmask & (RTMON_TASK|RTMON_TASKPROC))
	    printStateChangeEvent(rs, fd, ev);
	break;
    case TSTAMP_EV_EODISP:
	if (rs->eventmask & (RTMON_TASK|RTMON_TASKPROC))
	    printEODispEvent(rs, fd, ev);
	break;
    case EVENT_WIND_EXIT_IDLE:
	if (rs->eventmask & (RTMON_TASK|RTMON_TASKPROC))
	    printIdleEvent(rs, fd, ev);
	break;
    case TSTAMP_EV_SYSCALL_BEGIN:
	_rtmon_syscallEventBegin(rs, fd, ev);
	break;
    case TSTAMP_EV_SYSCALL_END:
	_rtmon_syscallEventEnd(rs, fd, ev);
	break;
    case EVENT_INT_EXIT:
	if (rs->eventmask & RTMON_INTR)
	    printInterruptExitEvent(rs, fd, ev);
	break;
    case TSTAMP_EV_LOST_TSTAMP:
	_rtmon_printEvent(rs, fd, ev, "LOST %llu EVENTS\n", ev->qual[0]);
	break;
    case TSTAMP_EV_PROF_STACK32:
    case TSTAMP_EV_PROF_STACK64:
	printProfStack(rs, fd, ev);
	break;
    case VM_EVENT_TFAULT_ENTRY:
    case VM_EVENT_TFAULT_EXIT:
    case VM_EVENT_PFAULT_ENTRY:
    case VM_EVENT_PFAULT_EXIT:
    case VM_EVENT_PFAULT_RLACQ:
    case VM_EVENT_PFAULT_NOTHV:
    case VM_EVENT_PFAULT_ISMOD:
    case VM_EVENT_PFAULT_STARTF:
    case VM_EVENT_PFAULT_NOTCW:
    case VM_EVENT_PFAULT_CW:
    case VM_EVENT_VFAULT_ENTRY:
    case VM_EVENT_VFAULT_EXIT:
    case VM_EVENT_VFAULT_DFILLSTART:
    case VM_EVENT_VFAULT_DFILLEND:
    case VM_EVENT_VFAULT_ANONINS:
    case VM_EVENT_VFAULT_ADDMAP_START:
    case VM_EVENT_VFAULT_ADDMAP_END:
    case VM_EVENT_VFAULT_DROPIN:
	if (rs->eventmask & RTMON_VM)
	    rtmon_printRawEvent(rs, fd, ev);
	break;
    case DISK_EVENT_QUEUED:
	if (rs->eventmask & RTMON_DISK)
	    printDiskEvent(rs, fd, ev, "I/O queued");
	break;
    case DISK_EVENT_START:
	if (rs->eventmask & RTMON_DISK)
	    printDiskEvent(rs, fd, ev, "I/O started");
	break;
    case DISK_EVENT_DONE:
	if (rs->eventmask & RTMON_DISK)
	    printDiskEvent(rs, fd, ev, "I/O done");
	break;
    case TSTAMP_EV_ALLOC:
	if (rs->eventmask & RTMON_ALLOC)
	    printAllocEvent(rs, fd, ev);
	break;
    case NET_EVENT_NEW:
    case NET_EVENT_SLEEP:
    case NET_EVENT_WAKEUP:
    case NET_EVENT_WAKING:
    case NET_EVENT_FLOW:
    case NET_EVENT_DROP:
    case NET_EVENT_EVENT_DONE:
	break;
    default:
	if (IS_INT_ENT_EVENT(ev->evt)) {
	    if (rs->eventmask & RTMON_INTR)
		printInterruptEvent(rs, fd, ev);
	    break;
	} else if (IS_KERNEL_USER_EVENT(ev->evt)) {
		_rtmon_printEvent(rs, fd, ev, "Kernel event %d\n", ev->evt - MIN_KERNEL_ID);
		printRawData(rs, fd, ev);
		break;	
	}else if (IS_USER_EVENT(ev->evt)) {
	    _rtmon_printEvent(rs, fd, ev, "User event %d\n", ev->evt - MIN_USER_ID);
	    printRawData(rs, fd, ev);
	    break;
	} else {
	    rtmon_decodeEventEnd(&rs->ds, ev);
	    return (FALSE);		/* NB: not decoded */
	}
    }
    rtmon_decodeEventEnd(&rs->ds, ev);
    return (TRUE);
}
