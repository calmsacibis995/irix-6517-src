/*
 * Pthread Commands
 */

#include "spyCommon.h"
#include "spyBase.h"
#include "spyIO.h"
#include "spyCmd.h"
#include "spyThread.h"

#include "ptCmd.h"
#include "ptGrok.h"
#include <pt/ptABI.h>
#include <pt/pt.h>

#include <alloca.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#define OUTMSG(str)	{ *out = strdup(str); return (EINVAL); }

static int	vpIterator(spyProc_t*, off64_t, void*);
static int	ptIterator(spyProc_t*, off64_t, void*);

static int	ptcReadyq(spyProc_t*, char*, char**);
static int	ptcExecq(spyProc_t*, char*, char**);
static int	ptcIdleq(spyProc_t*, char*, char**);
static int	ptcMtx(spyProc_t*, char*, char**);
static int	ptcCV(spyProc_t*, char*, char**);
static int	ptcPT(spyProc_t*, char*, char**);
static int	ptcVP(spyProc_t*, char*, char**);
static int	ptcMem(spyProc_t*, char*, char**);
static int	ptcUtMem(spyProc_t*, char*, char**);
static int	ptcLib(spyProc_t*, char*, char**);
static int	ptcTrc(spyProc_t*, char*, char**);

static spySubCmd_t	ptCmds[] = {
	{ "rq",		"(pt ready queue)",	ptcReadyq },
	{ "xq",		"(vp exec queue)",	ptcExecq },
	{ "iq",		"(vp idle queue)",	ptcIdleq },

	{ "mtx",	"<addr> (mtx details)",	ptcMtx },
	{ "cv",		"<addr> (cv details)",	ptcCV },

	{ "pt",		"<pt-id>|<addr> (pt details)",	ptcPT },
	{ "vp",		"<vp>  (vp details)",	ptcVP },

	{ "mem",	"<addr> <count>",	ptcMem },
	{ "utmem",	"<ut> <addr> <count>",	ptcUtMem },

	{ "lib",	"(internal info)",	ptcLib },
	{ "trc",	"<flags> (tracing)",	ptcTrc },
	0
};

spyCmdList_t		ptCmdList = {
	"l", "pthread lib commands", ptCmds, 0
};


typedef struct string {
	char*	string;
	size_t	length;
} string_t;

static int
strGrow(string_t* s, size_t l)
{
	char*	tmp = s->string;
	if (!(s->string = realloc(s->string, s->length + l))) {
		free(tmp);
		return (errno);
	}
	return (0);
}


static int
vpIterator(spyProc_t* p, off64_t vp, void* a)
{
	char*		vpBuf = alloca(sizeOf(p, vp_t));
	char*		s = 0;
	int		len;
	string_t*	str = (string_t*)a;
	int		e;

	if ((e = vpDescribe(p, vp, vpBuf, &s))
	   || (e = strGrow(str, str->length + strlen(s) + 2))) {
		free(s);
		return (e);
	}

	len = sprintf(str->string + str->length, "%s\n", s);
	str->length += len;
	free(s);
	return (0);
}


static int
ptIterator(spyProc_t* p, off64_t pt, void* a)
{
	char*		ptBuf = alloca(sizeOf(p, pt_t));
	char*		s = alloca(STC_INFO0BUFLEN);
	int		len;
	string_t*	str = (string_t*)a;
	uint_t		ptLabel;
	int		e;

	if ((e = ptDescribe(p, pt, ptBuf, &s, 0))
	   || (e = strGrow(str, str->length + strlen(s) + 60))) {
		return (e);
	}

	ioXInt(p, addrOf(p, ptBuf, pt_t, _pt_label), &ptLabel);
	len = sprintf(str->string + str->length, "pt %#x/%s\n", 
		(uint_t)PT_MAKE_ID(PT_GEN_BITS(ptLabel), PT_PTR_TO_ID(p, pt)),
		s);

	str->length += len;
	return (0);
}


/* ARGSUSED */
static int
ptcReadyq(spyProc_t* p, char* in, char** out)
{
	string_t	l;
	off64_t		qHead;
	int		e;

	l.string = 0;
	l.length = 0;

	if ((e = SPC(spc_symbol_addr, p,  "_SGIPT_pt_readyq", &qHead, 0, 0))
	    || (e = ioListScan(p, qHead, ptIterator, &l))) {
		return (e);
	}
	*out = l.string;
	return (0);
}


/* ARGSUSED */
static int
ptcExecq(spyProc_t* p, char* in, char** out)
{
	string_t	l;
	off64_t		qHead;
	int		e;

	l.string = 0;
	l.length = 0;

	if ((e = SPC(spc_symbol_addr, p, "vp_execq", &qHead, 0, 0))
	    || (e = ioListScan(p, qHead, vpIterator, &l))) {
		return (e);
	}
	*out = l.string;
	return (0);
}


/* ARGSUSED */
static int
ptcIdleq(spyProc_t* p, char* in, char** out)
{
	string_t	l;
	off64_t		qHead;
	int		e;

	l.string = 0;
	l.length = 0;

	if ((e = SPC(spc_symbol_addr, p, "vp_idleq", &qHead, 0, 0))
	    || (e = ioListScan(p, qHead, vpIterator, &l))) {
		return (e);
	}
	*out = l.string;
	return (0);
}


static int
ptcMtx(spyProc_t* p, char* in, char** out)
{
	char*		arg2;
	char*		arg1 = ioToken(in, &arg2);
	char*		mtxBuf = alloca(sizeOf(p, mtx_t));
	char*		attrBuf;
	off64_t		mtx;
	off64_t		pt;
	pid_t		pid;
	uint_t		waiters;
	char		shared;
	char		type;
	char		protocol;
	string_t	l;
	off64_t		qHead;
	int		e;

	l.string = 0;
	l.length = 0;

	if (e = strGrow(&l, 60)) {
		return (e);
	}

	if (!arg1 || ((mtx = strtoll(arg1, 0, 0)) < 0)) {
		OUTMSG("missing mtx addr");
	}
	if (e = ioFetchBytes(p, mtx, mtxBuf, sizeOf(p, mtx_t))) {
		return (e);
	}

	/* determine mutex layout based on type */
	attrBuf = mtxBuf + offSet(p, mtx_t, _mtx_attr);
	type = *(char*)addrOf(p, attrBuf, mtxattr_t, _ma_type);
	shared = ((char*)addrOf(p, attrBuf, mtxattr_t, _ma_type))[1];
	protocol = *(char*)addrOf(p, attrBuf, mtxattr_t, _ma_protocol);

	l.length = sprintf(l.string, "%s, %s, %s, ",
		shared ? "shared" : "private",
		type == PTHREAD_MUTEX_NORMAL ? "normal"
			: (type == PTHREAD_MUTEX_ERRORCHECK ? "errorcheck"
			: (type == PTHREAD_MUTEX_RECURSIVE ? "recursive"
			: "default")),
		protocol == PTHREAD_PRIO_NONE ? "no protocol"
			: (protocol == PTHREAD_PRIO_PROTECT ? "ceiling"
			: "inherit")
			);

	if (shared) {
		ioXPointer(p, addrOf(p, mtxBuf, mtx_t, _mtx_thread), &pt);
		ioXInt(p, addrOf(p, mtxBuf, mtx_t, _mtx_pid), (uint_t*)&pid);
		ioXInt(p, addrOf(p, mtxBuf, mtx_t, _mtx_waiters), &waiters);

		l.length += sprintf(l.string + l.length,
				    "owner %#llx %d waiters %d ",
				    pt, pid, waiters);
	} else {
		ioXPointer(p, addrOf(p, mtxBuf, mtx_t, _mtx_owner), &pt);
		l.length += sprintf(l.string + l.length,
				    "owner %#llx, waiters ", pt);

		qHead = mtx + offSet(p, mtx_t, _mtx_waitq);
		if ((e = ioListScan(p, qHead, ptIterator, &l))) {
			return (e);
		}
	}
	*out = l.string;
	return (0);
}


static int
ptcCV(spyProc_t* p, char* in, char** out)
{
	char*		arg2;
	char*		arg1 = ioToken(in, &arg2);
	char*		cvBuf = alloca(sizeOf(p, mtx_t));
	off64_t		cv;
	string_t	l;
	off64_t		qHead;
	int		e;

	l.string = 0;
	l.length = 0;

	if (!arg1 || ((cv = strtoll(arg1, 0, 0)) < 0)) {
		OUTMSG("missing mtx addr");
	}
	if (e = ioFetchBytes(p, cv, cvBuf, sizeOf(p, mtx_t))) {
		return (e);
	}
	qHead = cv + offSet(p, mtx_t, _mtx_waitq);
	if ((e = ioListScan(p, qHead, ptIterator, &l))) {
		return (e);
	}

	*out = l.string;
	return (0);
}


static int
ptcVP(spyProc_t* p, char* in, char** out)
{
	char*		arg2;
	char*		arg1 = ioToken(in, &arg2);
	char*		vpBuf = alloca(sizeOf(p, vp_t));
	off64_t		vp;

	if (!arg1 || ((vp = strtoll(arg1, 0, 0)) < 0)) {
		OUTMSG("missing vp id");
	}
	return (vpDescribe(p, vp, vpBuf, out));
}


static int
ptcPT(spyProc_t* p, char* in, char** out)
{
	char*		arg2;
	char*		arg1 = ioToken(in, &arg2);
	char*		ptBuf = alloca(sizeOf(p, pt_t));
	ushort_t	ptMax;
	int		ptId;
	off64_t		ptAddr;

	if (!arg1) {
		OUTMSG("missing pt id");
	}
	if (!(*out = malloc(256))) {
		OUTMSG("unable to allocate space");
	}
	if (arg1[0] == '*') {
		if ((ptAddr = strtoll(arg1+1, 0, 0)) < 0
		    || ptAddr & 0x3) {
			OUTMSG("pt address is bad");
		}
	} else if ((ptId = (int)strtol(arg1, 0, 0)) < 0) {
		OUTMSG("pt id is bad");
	} else {
		ptAddr = PT_ID_TO_PTR(p, ptId);
	}
	if (ioFetchShort(p, ptProc(p)->pi_max, &ptMax)) {
		OUTMSG("unable to get lib info");
	}
	if (ptAddr < ptProc(p)->pi_table || PT_PTR_TO_ID(p, ptAddr) > ptMax) {
		OUTMSG("pt id is out of range");
	}
	return (ptDescribe(p, ptAddr, ptBuf, out, 1));
}


static int
printMem(spyProc_t* p, char* arg1, char* arg2, char* arg3)
{
	int		b;
	int		w;
	off64_t		addr;
	char*		buf;
	int		bytes;
	int		e;
	tInfo_t		ti;
	tid_t		tid;

	addr = strtoll(arg2, 0, 0);
	bytes = (int)strtoul(arg3, 0, 0);
	if (bytes > 0x10000) {
		return (ERANGE);
	}
	buf = alloca(bytes);

	if (arg1) {
		ti.ti_proc = p;
		ti.ti_kt = (tid_t)strtoul(arg1, 0, 0);
		tid = KtToIO(&ti);
		if (e = iotFetchBytes(p, tid, addr, buf, bytes)) {
			return (e);
		}

	} else if (e = ioFetchBytes(p, addr, buf, bytes)) {
		return (e);
	}

	for (; bytes; addr += 16) {
		printf("%#llx: ", addr);
		for (w = 0; w < 4 && bytes; w++) {
			putchar(' ');
			for (b = 0; b < 4 && bytes; b++, bytes--) {
				printf("%02x", *buf++);
			}
		}
		putchar('\n');
	}
	return (0);
}


static int
ptcMem(spyProc_t* p, char* in, char** out)
{
	char*		arg2;
	char*		eol;
	char*		arg1 = ioToken(in, &arg2);

	if (!arg1 || !(arg2 = ioToken(arg2, &eol))) {
		OUTMSG("usage <addr> <count>");
	}
	*out = 0;
	return (printMem(p, 0, arg1, arg2));
}


static int
ptcUtMem(spyProc_t* p, char* in, char** out)
{
	char*		arg2;
	char*		arg3;
	char*		eol;
	char*		arg1 = ioToken(in, &arg2);

	if (!arg1 || !(arg2 = ioToken(arg2, &arg3))
	    || !(arg3 = ioToken(arg3, &eol))) {
		OUTMSG("usage <tid> <addr> <count>");
	}
	*out = 0;
	return (printMem(p, arg1, arg2, arg3));
}


/* ARGSUSED */
static int
ptcLib(spyProc_t* p, char* in, char** out)
{
#define LIBPRINT(val, fmt) \
	l += sprintf(*out + l, "%20s: " fmt "\n", #val, val)
#define LIBSYMGET(name, val, fetch, fmt) \
	if (SPC(spc_symbol_addr, p, name, &addr, 0, 0)	\
	    || fetch (p, addr, &val))			\
		l += sprintf(*out + l, "%20s: <failed>\n", #val); \
	else LIBPRINT(val, fmt)

	off64_t		addr;
	uint_t		vp_target_count;
	uint_t		vp_active;
	off64_t		exiting_pt;
	uint_t		pt_readyq_len;
	uint_t		sched_balance;
	ushort_t	sched_rr_quantum;
	ushort_t	sched_fifo_quantum;
	ushort_t	ptMax;
	int		l;

	if (!(*out = malloc(512))) {
		return (errno);
	}
	l = sprintf(*out, "Variables:\n");
	LIBSYMGET("vp_target_count", vp_target_count, ioFetchInt, "%d");
	LIBSYMGET("__vp_active", vp_active, ioFetchInt, "%d");
	LIBSYMGET("pt_readyq_len", pt_readyq_len, ioFetchInt, "%d");
	LIBSYMGET("sched_balance", sched_balance, ioFetchInt, "%d");
	LIBSYMGET("exiting_pt", exiting_pt, ioFetchPointer, "%#llx");
	LIBSYMGET("_SGIPT_sched_rr_quantum", sched_rr_quantum,
		ioFetchShort, "%d");
	LIBSYMGET("_SGIPT_sched_fifo_quantum", sched_fifo_quantum,
		ioFetchShort, "%d");

	LIBPRINT(ptProc(p)->pi_table, "%#llx");
	if (!ioFetchShort(p, ptProc(p)->pi_max, &ptMax)) {
		LIBPRINT(ptMax, "%d");
	}
	LIBPRINT(sizeOf(p, pt_t), "%#x");
	LIBPRINT(sizeOf(p, vp_t), "%#x");
	LIBPRINT(sizeOf(p, mtx_t), "%#x");

	return (0);
}


/* ARGSUSED */
static int
ptcTrc(spyProc_t* p, char* in, char** out)
{
	char*		arg2;
	char*		arg1 = ioToken(in, &arg2);

	extern ulong_t	spyTrace;

	if (!arg1) {
#ifdef DEBUG
#		define TFlag(l)	printf("\t"  #l " %#x\n", l)
		printf("Trace %#lx\n", spyTrace);
		TFlag(TL0);
		TFlag(TL1);
		TFlag(TL2);

		TFlag(TIO);
		TFlag(TPR);
#else
		printf("Trace is disabled\n");
#endif
		return (0);
	}
	spyTrace = strtoul(arg1, 0, 0);

	return (0);
}
