/*
 * spy IO management
 */
#define _KMEMUSER		/* yuk */
#include <sys/kucontext.h>

#include "spyCommon.h"
#include "spyBase.h"
#include "spyIO.h"

#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/kucontext.h>
#include <sys/procfs.h>
#include <unistd.h>

ulong_t	spyTrace;	/* Debug trace flags */
int	spyTraceIndent;	/* Debug trace indentation - not thread safe */
static int	spyTraceFile = 1;	/* Debug trace fd */


/* Register sets:
 *
 * The format of the register set is determined by the target process
 * architecture/ABI.  The caller may request we copy or widen this
 * format - we reject 64->32 bit conversion requests.
 */

/* We use the procfs format (gregset_t, fpregset_t) buffers and must
 * translate them to pthread library jmpbuf[]s.
 * Kernel contexts are larger because they must save _all_ registers
 * but pthreads only needs the callee preserved ones.

   O32 (mips1/mips2) jmp_buf[]
	[02..13]	sp, pc, v0, s0..s8,
	[14..25]	f21,f20,f23,f22,f25,f24,f27,f26,f29,f28,f31,f30
	[25]		sr

   N32/N64 (mips3/mips4) jmp_buf[]
	[00..11]	sp, pc, v0, s0..s8,
	N32 [12,14,15,16,17]	f20, f22, f24, f26, f28
	N64 [12..17]		f24..f29,
	[18..19]	f30, f31,
	[20..21]	sr, gp
 */

/* Map jmpbuf[] to gregset_t
 * Either 32 or 64 bit registers.
 * jbXlate maps jmpbuf[...] to gregset[...]
 */
static int jbRegXlate32[_JBLEN] = {	/* _MIPS_SIM_ABI32 */
	0, 0, CTX_SP, CTX_EPC, CTX_V0,
	CTX_S0, CTX_S1, CTX_S2, CTX_S3, CTX_S4, CTX_S5, CTX_S6, CTX_S7, CTX_S8,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	/* No CTX_SR for mips2, CTX_GP is callee saved */
};

static int jbRegXlate64[_JBLEN] = {	/* _MIPS_SIM_NABI32, _MIPS_SIM_ABI64 */
	CTX_SP, CTX_EPC, CTX_V0,
	CTX_S0, CTX_S1, CTX_S2, CTX_S3, CTX_S4, CTX_S5, CTX_S6, CTX_S7, CTX_S8,
	0, 0, 0, 0, 0, 0, 0, 0,
	CTX_SR, CTX_GP
};

/* Map jmpbuf[] to fpregset_t
 * Format depends on ABI.
 */
#define FPREGSTART	12
static int jbFPRegXlateO32[_JBLEN - FPREGSTART] = {	/* _MIPS_SIM_ABI32 */
	0, 0, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
};

static int jbFPRegXlateN32[_JBLEN - FPREGSTART] = {	/* _MIPS_SIM_NABI32 */
	20, 0, 22, 24, 26, 28, 30, 31
};

static int jbFPRegXlateN64[_JBLEN - FPREGSTART] = {	/* _MIPS_SIM_ABI64 */
	24, 25, 26, 27, 28, 29, 30, 31
};


/* Extract gregs data from jmpbuf.
 */
int
ioXJBGregs(spyProc_t* p, int destFormat, caddr_t buf, void* outp)
{
	typedef irix5_gregset_t	irix5_32_gregset_t;
	int	j;

	TEnter( TIO, ("ioXJBGregs(0x%p, 0x%p, 0x%p)\n", p, buf, outp) );

	if (p->sp_abi != SP_O32 && destFormat == SP_O32) {
		TReturn (EINVAL);
	}

	/* Very nasty - jmpbuf to gregset copier */
#	define XLATEGREGS(src, dst)					\
	{								\
	__uint ## src ## _t*	jbuf = (__uint ## src ## _t*)buf;	\
	__uint ## dst ## _t*	greg = (__uint ## dst ## _t*)outp;	\
									\
	memset(greg, 0, sizeof(irix5_ ## dst ## _gregset_t));		\
	for (j = 0; j < _JBLEN; j++) {					\
		if (jbRegXlate ## src [j]) {				\
			greg[jbRegXlate ## src [j]] = jbuf[j];		\
		}							\
	}								\
	}

	/* There are two formats for the general jmpbuf regs.
	 * We just copy straight or widen to 64.
	 */
	if (destFormat == SP_O32) {
		XLATEGREGS(32, 32);		/* 32 bit copy */
	} else if (p->sp_abi == SP_O32) {
		XLATEGREGS(32, 64);		/* 32 to 64 bit copy */
	} else {
		XLATEGREGS(64, 64);		/* 64 bit copy */
	}
#	undef	XLATEGREGS
	TReturn (0);
}


/* Extract fpregs data from jmpbuf.
 */
int
ioXJBFPregs(spyProc_t* p, int destFormat, caddr_t buf, void* outp)
{
	typedef irix5_fpregset_t	irix5_32_fpregset_t;
	int	r;
	int	j;

	TEnter( TIO, ("ioXJBFPregs(0x%p, 0x%p, 0x%p)\n", p, buf, outp) );

	if (p->sp_abi != SP_O32 && destFormat == SP_O32) {
		TReturn (EINVAL);
	}

	/* Even nastier - jmpbuf to fpregset copier */
#	define XLATEFPREGS(bits, tgt)					\
	{								\
	__uint ## bits ## _t*	jbuf = (__uint ## bits ## _t*)buf;	\
	__uint ## bits ## _t*	freg = ((irix5_ ## bits ## _fpregset_t*)outp) \
					->fp_r.fp_regs;			\
	int*			xlate = jbFPRegXlate ## tgt;		\
									\
	memset(freg, 0, sizeof(irix5_ ## bits ## _fpregset_t));		\
	for (j = FPREGSTART, r = 0; j < _JBLEN; j++, r++) {		\
		if (xlate[r]) {						\
			freg[xlate[r]] = jbuf[j];			\
		}							\
	}								\
	}

	/* ABIs differ in jmpbuf layout for fp regs.
	 * Don't do widening, just copy as the process ABI dictates.
	 */
	switch (p->sp_abi) {
	case SP_O32	:
		XLATEFPREGS(32, O32);	/* 32 bit copy */
		break;
	case SP_N32	:
		XLATEFPREGS(64, N32);	/* 64 bit copy, n32 */
		break;
	case SP_N64	:
		XLATEFPREGS(64, N64);	/* 64 bit copy, n64 */
		break;
	}
#	undef	XLATEFPREGS
	TReturn (0);
}


/* Insert gregs data to jmpbuf.
 */
int
ioPlaceGregsJB(spyProc_t* p, int srcFormat, void* inp,
	       caddr_t buf, off64_t addr)
{
	int	j;
	size_t	size;

	TEnter( TIO, ("ioPlaceGregsJB(0x%p, 0x%p, %#llx)\n", p, inp, addr) );

	if (srcFormat != SP_O32 && p->sp_abi == SP_O32) {
		TReturn (EINVAL);
	}

	/* gregset to jmpbuf copier */
#	define XLATEGREGS(src, dst)					\
	{								\
	__uint ## src ## _t*	greg = (__uint ## src ## _t*)inp;	\
	__uint ## dst ## _t*	jbuf = (__uint ## dst ## _t*)buf;	\
									\
	size = _JBLEN * sizeof(*jbuf);		/* side effect */	\
	for (j = 0; j < _JBLEN; j++) {					\
		if (jbRegXlate ## dst [j]) {				\
			jbuf[j] = greg[jbRegXlate ## dst [j]];		\
		}							\
	}								\
	}

	/* There are two formats for the general jmpbuf regs.
	 * We just copy straight or widen to 64 in a local buffer.
	 */
	if (p->sp_abi == SP_O32) {
		XLATEGREGS(32, 32);		/* 32 bit copy */
	} else if (srcFormat == SP_O32) {
		XLATEGREGS(32, 64);		/* 32 to 64 bit copy */
	} else {
		XLATEGREGS(64, 64);		/* 64 bit copy */
	}
#	undef	XLATEGREGS

	/* Write the regs data to the target.
	 */
	TReturn (ioPlaceBytes(p, addr, buf, size));
}


/* Insert fpregs data to jmpbuf.
 */
int
ioPlaceFPregsJB(spyProc_t* p, int srcFormat, void* inp,
		caddr_t buf, off64_t addr)
{
	typedef irix5_fpregset_t	irix5_32_fpregset_t;
	int	r;
	int	j;
	size_t	size;

	TEnter( TIO, ("ioPlaceFPregsJB(0x%p, 0x%p, %#llx)\n", p, inp, addr) );

	if (srcFormat != SP_O32 && p->sp_abi == SP_O32) {
		TReturn (EINVAL);
	}

	/* fpregset to jmpbuf copier */
#	define XLATEFPREGS(bits, tgt)				\
	{								\
	__uint ## bits ## _t*	freg = ((irix5_ ## bits ## _fpregset_t*)inp) \
					->fp_r.fp_regs;			\
	__uint ## bits ## _t*	jbuf = (__uint ## bits ## _t*)buf;	\
	int*			xlate = jbFPRegXlate ## tgt;		\
									\
	size = _JBLEN * sizeof(*jbuf);		/* side effect */	\
	for (j = FPREGSTART, r = 0; j < _JBLEN; j++, r++) {		\
		if (xlate[r]) {						\
			jbuf[j] = freg[xlate[r]];			\
		}							\
	}								\
	}

	/* ABIs differ in jmpbuf layout for fp regs.
	 * Don't do widening, just copy as the process ABI dictates.
	 */
	if (p->sp_abi == SP_O32) {
		XLATEFPREGS(32, O32);		/* 32 bit copy, O32 */
	} else if (srcFormat == SP_O32) {
		switch (p->sp_abi) {
		case SP_N32	:
			XLATEFPREGS(32, N32);	/* 32 bit copy, n32 */
			break;
		case SP_N64	:
			XLATEFPREGS(32, N64);	/* 32 bit copy, n64 */
			break;
		}
	} else {
		switch (p->sp_abi) {
		case SP_N32	:
			XLATEFPREGS(64, N32);	/* 64 bit copy, n32 */
			break;
		case SP_N64	:
			XLATEFPREGS(64, N64);	/* 64 bit copy, n64 */
			break;
		}
	}
#	undef	XLATEFPREGS

	/* Write the regs data to the target.
	 */
	TReturn (ioPlaceBytes(p, addr, buf, size));
}


void
ioXPointer(spyProc_t* p, caddr_t buf, off64_t* outp)
{
	switch (p->sp_abi) {
	case SP_O32	:
	case SP_N32	:
		*outp = (off64_t)(*(__uint32_t*)buf);
		break;
	case SP_N64	:
		*outp = (off64_t)(*(__uint64_t*)buf);
		break;
	}
	TTrace( TIO, ("ioXPointer(0x%p) %#llx\n", p, *outp) );
}


/* ARGSUSED */
void
ioXInt(spyProc_t* p, caddr_t buf, uint_t* outp)
{
	*outp = *(uint_t*)buf;
	TTrace( TIO, ("ioXInt(0x%p) %#x\n", p, *outp) );
}


/* ARGSUSED */
void
ioXShort(spyProc_t* p, caddr_t buf, ushort_t* outp)
{
	*outp = *(ushort_t*)buf;
	TTrace( TIO, ("ioXShort(0x%p) %#x\n", p, *outp) );
}


int
ioFetchPointer(spyProc_t* p, off64_t addr, off64_t* out)
{
	off64_t	buf;
	int	e;

	TEnter( TIO, ("ioFetchPointer(0x%p, %#llx)\n", p, addr) );

	if (e = ioFetchBytes(p, addr, (caddr_t)&buf, IOPTRSIZE(p))) {
		TReturn (e);
	}
	ioXPointer(p, (caddr_t)&buf, out);
	TReturn (0);
}


int
ioFetchInt(spyProc_t* p, off64_t addr, uint_t* out)
{
	uint_t	buf;
	int	e;

	TEnter( TIO, ("ioFetchInt(0x%p, %#llx)\n", p, addr) );

	if (e = ioFetchBytes(p, addr, (caddr_t)&buf, 4)) {
		TReturn (e);
	}
	ioXInt(p, (caddr_t)&buf, out);
	TReturn (0);
}


int
ioFetchShort(spyProc_t* p, off64_t addr, ushort_t* out)
{
	ushort_t	buf;
	int	e;

	TEnter( TIO, ("ioFetchShort(0x%p, %#llx)\n", p, addr) );

	if (e = ioFetchBytes(p, addr, (caddr_t)&buf, 2)) {
		TReturn (e);
	}
	ioXShort(p, (caddr_t)&buf, out);
	TReturn (0);
}


/* Read bytes using call back or procfs
 */
int
ioFetchBytes(spyProc_t* p, off64_t addr, caddr_t out, size_t size)
{
	ssize_t	e;

	TEnter( TIO, ("ioFetchBytes(0x%p, %#llx, %#lx)\n", p, addr, size));

	if (!SP_LIVE(p)) {
		TReturn (SPC(spc_memory_read, p, 0, addr, out, size));
	}

	if ((e = pread64(p->sp_procfd, out, size, addr)) != size) {
		TReturn ((e == -1) ? errno : EAGAIN);
	}
	TReturn (0);
}


/* Read thread specific bytes using call back or procfs
 */
int
iotFetchBytes(spyProc_t* p, tid_t t, off64_t addr, caddr_t out, size_t size)
{
	prthreadctl_t	ptc;
	prio_t		pi;

	TEnter( TIO, ("iotFetchBytes(0x%p, %#x, %#llx, %#lx)\n",
			p, t, addr, size));

	if (!SP_LIVE(p)) {
		TReturn (SPC(spc_memory_read, p, t, addr, out, size));
	}

	ptc.pt_tid = t;
	ptc.pt_flags = PTFD_EQL;
	ptc.pt_cmd = PIOCREAD;	/* always use native abi */
	ptc.pt_data = (caddr_t)&pi;
	pi.pi_offset = addr;
	pi.pi_base = out;
	pi.pi_len = (ssize_t)size;

	TTrace( TPR, ("iotFetchBytes: procfs(%d, %#x:READ, 0x%p)\n",
			p->sp_procfd, ptc.pt_tid, &ptc) );
	if (ioctl(p->sp_procfd, PIOCTHREAD, &ptc) != 0) {
		TReturn (errno);
	}
	TReturn (0);
}


/* Fetching general register values using spy call back
 * No translation is necessary
 */
int
iotFetchGregs(spyProc_t* p, tid_t t, caddr_t out)
{
	void*		regs = alloca(sizeof(irix5_64_mcontext_t));
	void*		addr;
	size_t		size;
	int		e;

	TEnter( TIO, ("iotFetchGregs(0x%p, %#x)\n", p, t) );

	if (e = SPC(spc_register_read, p, t, regs, 0, 0)) {
		TReturn (e);
	}
	switch (p->sp_abi > IONATIVE ? p->sp_abi : IONATIVE) {
	case SP_O32	:
		size = sizeof(irix5_gregset_t);
		addr = &((irix5_mcontext_t*)regs)->gregs;
		break;
	case SP_N32	:
	case SP_N64	:
		size = sizeof(irix5_64_gregset_t);
		addr = &((irix5_64_mcontext_t*)regs)->gregs;
		break;
	}
	memcpy(out, addr, size);
	TReturn (0);
}


/* Fetching floating point register values using spy call back
 * No translation is necessary
 */
int
iotFetchFPregs(spyProc_t* p, tid_t t, caddr_t out)
{
	void*		regs = alloca(sizeof(irix5_64_mcontext_t));
	void*		addr;
	size_t		size;
	int		e;

	TEnter( TIO, ("iotFetchFPregs(0x%p, %#x)\n", p, t) );

	if (e = SPC(spc_register_read, p, t, regs, 0, 0)) {
		TReturn (e);
	}
	switch (p->sp_abi > IONATIVE ? p->sp_abi : IONATIVE) {
	case SP_O32	:
		size = sizeof(irix5_fpregset_t);
		addr = &((irix5_mcontext_t*)regs)->fpregs;
		break;
	case SP_N32	:
	case SP_N64	:
		size = sizeof(irix5_64_fpregset_t);
		addr = &((irix5_64_mcontext_t*)regs)->fpregs;
		break;
	}
	memcpy(out, addr, size);
	TReturn (0);
}



/* Write bytes using procfs
 */
int
ioPlaceBytes(spyProc_t* p, off64_t addr, caddr_t out, size_t size)
{
	ssize_t	e;

	TEnter( TIO, ("ioPlaceBytes(0x%p, %#llx, %#lx)\n", p, addr, size));

	if (!SP_LIVE(p)) {
		TReturn (EACCES);
	}

	if ((e = pwrite64(p->sp_procfd, out, size, addr)) != size) {
		TReturn ((e == -1) ? errno : EAGAIN);
	}
	TReturn (0);
}


char*
ioToken(char* first, char** rest)
{
#	define WHITESPACE	" \t\n"
	size_t	len;

	first += strspn(first, WHITESPACE);
	if ((len = strcspn(first, WHITESPACE)) == 0) {
		return (0);
	}
	first[len] = '\0';
	*rest = first + len + 1;
	return (first);
}


int
ioListScan(spyProc_t* p, off64_t qHead,
	   int (*func)(spyProc_t*, off64_t, void*), void* arg)
{
	off64_t	qNext = qHead;
	int	e;

	TEnter( TIO,
		("ioListScan(0x%p, %#llx, 0x%p, 0x%p)\n", p, qHead, func, arg));
	while (!(e = ioFetchPointer(p, qNext, &qNext)) && qNext != qHead) {
		if (e = func(p, qNext, arg)) {
			TReturn (e);
		}
	}
	TReturn (e);
}


void
ioSetTrace(char* flags)
{
	char*	file;
	int	fd;

	if ((file = getenv("SPY_TRACE_FILE"))
	    && (fd = open(file, O_APPEND|O_CREAT|O_WRONLY, 0777)) >= 0) {
		spyTraceFile = fd;
	}
	if (!flags) {
		return;
	}
	spyTrace = strtol(flags, 0, 0);
}


void
ioTrace(char* fmt, ...)
{
	char	buf[256];
	int 	cnt;
	va_list	va;

	va_start(va, fmt);
	cnt = vsprintf(buf, fmt, va);
	va_end(va);
	write(spyTraceFile, buf, cnt);
}
