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
 * RTMON system call event decoding and printing support.
 *
 * TODO:
 *    sigsendset - 1st arg
 *    fcntl(GETFL) return decoding
 *    finish up readv/writev - mainly just writing the uio decode
 *    sendmsg, recvmsg, {get,put}{p}msg
 *    indirect stat params (requires kernel support)
 */

#ifdef notdef
#if (_MIPS_SZLONG == 64)
#error "This code will not work right for 64-bit compilation."
#endif
#endif

#include <sys/rtmon.h>
#include <sys/par.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

#include "rtmon.h"

#define	_ABI_SOURCE		/* XXX define to avoid termios problems */

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/attributes.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/usioctl.h>
#include <sys/stermio.h>
#include <sys/lock.h>
#include <sys/kabi.h>			/* for ABI values */
#include <sys/sysmp.h>
#include <sys/sysinfo.h>		/* for SAGET and MINFO structures */
#include <sys/sysmips.h>
#include <sys/syssgi.h>
#include <sys/sysmp.h>
#include <sys/schedctl.h>
#include <sys/cachectl.h>
#include <sys/mman.h>
#include <sys/quota.h>
#include <sys/wait.h>
#include <sys/swap.h>
#include <sys/sched.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/rrm.h>
#include <sys/usync.h>
#include <sys/idev.h>
#include <sys/imon.h>
#include <sys/procfs.h>
#include <sys/ckpt_procfs.h>
#include <sys/ckpt_sys.h>
#include <sys/stropts.h>
#include <sys/systeminfo.h>
#include <sys/psema_cntl.h>
#undef sigmask		/* use sigmask defined in ksignal.h */
#include <sys/ksignal.h>
#include <sys/prctl.h>
#include <sys/mkdev.h>

#include <net/if.h>
#include <net/route.h>
#include <net/soioctl.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <procfs/procfs64.h>

#define	RTMON_LINESIZE	8192	/* max length of an output line */

typedef struct {
    int		argn;		/* argument number */
    int		len;		/* length in bytes of additional info */
    char*	data;		/* ptr to data */
} indirect_t;

typedef struct subsyscall subsyscall_t;

typedef struct cl {			/* system call link list */
    int64_t	kid;
    ushort	callno;		/* call number */
    ushort	cpu;
    unchar	abi;		/* execution ABI */
    unchar	num;		/* number of direct parameters */
    unchar	numi;		/* number of indirect parameters */
    unchar	prstart;	/* if set then we have printed start */
    __int64_t	tstamp;
    subsyscall_t* sys;
    struct cl*	hnext;		/* hash chain */
    struct cl*	endrec;		/* ptr to endcall record */
    __uint64_t*	params;		/* direct parameter values */
    indirect_t*	iparams;	/* indirect parameter values */
} cl_t;

struct subsyscall {
    int		val;		/* arg0 value -1 --> actual sysent entry */
    const char*	name;		/* system call name NULL -> end of list */
    char	nargs;		/* # args */
    char	suparg;		/* suppress arg printing (for decoded arg)
			 	 * 0 - no suppression 1-n arg number */
    char	trace;		/* interested in this?? */
    char	inout;		/* has in-out args, print separately */
    const char* argfmt[FAWLTY_MAX_PARAMS];/* how to format args */
    const char* retfmt[2];	/* how to print return values */
    int		num;		/* number of times called */
    __int64_t	time;		/* accumulated time doing syscall */
};

typedef struct syscall_s syscall_t;
struct syscall_s {
    int		decodearg;	/* argument to decoder method */
    subsyscall_t* (*decode)(const rtmonPrintState*, const syscall_t*, cl_t *, int);
    subsyscall_t* subs;
};

/*
 * Special printing tables
 * The first letter of the argument format decides the style:
 * T - do table lookup for symbolic name(s) - multiple tables are
 *     separated by a '|'; tables can be matched by values and bitmasks
 * I - indirect arguments - prindirect parses the remainder
 * G - ignore argument (no printing)
 * L - print next 2 args as a long long
 * N - print number as decimal, followed by u,d,x,o, etc.; picks
 *     32-bit vs. 64-bit value based on kernel ABI
 * S - size_t per the ABI; ...
 * O - off_t per the ABI; ....
 * P - generic pointer per the ABI; ....
 */
typedef struct pp_s {
    long val;
    const char *name;
} pp_t;

typedef struct ptab_s {
    const char *tabname;	/* table name - used in 'T' formatting exp */
    const pp_t *tab;
    char flags;			/* are values a bit mask? */
    long mask;			/* if not a bit mask, which are valid values */
} ptab_t;

typedef struct {
    const char* tabname;		/* table name */
    void (*func)(rtmonPrintState*,	/* print func */
	const indirect_t*, const struct cl *, char *, int);
} iptab_t;

#define NV(n)	{n, #n}		/* note ANSI correctness: #n, instead of "n" */
#define NULLTAB	{0,0}
#define ALLVAL	(~0)

static	void callrectocl(rtmonPrintState*, const tstamp_event_entry_t*, struct cl**);
static	void printcall(rtmonPrintState* rs, FILE*, struct cl *, int);

static	subsyscall_t* gd(const rtmonPrintState*, const syscall_t*, cl_t*, int);
static	subsyscall_t* sigd(const rtmonPrintState*, const syscall_t*, cl_t*,int);
static	subsyscall_t* ad(const rtmonPrintState*, const syscall_t*, cl_t*, int);
static	subsyscall_t* kad(const rtmonPrintState*, const syscall_t*, cl_t*, int);
static	subsyscall_t* ssd(const rtmonPrintState*, const syscall_t*, cl_t*, int);
static	subsyscall_t* find_syscall(const rtmonPrintState*, cl_t *);
static	indirect_t* findindirect(struct cl *crecp, int argnum);
static	void prindirect(rtmonPrintState*, indirect_t*, char*, const char*, struct cl*, int);

/*
 * NB: XPG compatibility (sys/ucontext.h) redefines this
 *     in such a way as to break compilation (sigh); so
 *     we override here since we don't need the #define.
 */
#ifdef sigaltstack
#undef sigaltstack
#endif

#define	DEFCALL(name)		static subsyscall_t name##_call[] = {
#define	ENDCALL			{ 0 } };
#define	SYSCALL(name,nargs)	DEFCALL(name) {-1, #name, nargs, 0, 0, 0,
#define	SYSCALB(name,nargs)	DEFCALL(name) {-1, #name, nargs, 0, 0, 1,
#define	_SYSCALL_		}, ENDCALL
#define	OK			{" OK"}
/*
 * Individual system call tables
 */
SYSCALL(syscall,	8) {"Px","Px","Px","Px","Px","Px","Px","Px"}, {"Px"} _SYSCALL_
SYSCALL(exit,		1) {"Nd"}, OK _SYSCALL_
SYSCALL(fork,		0) {0}, {"Nd"} _SYSCALL_
SYSCALL(read,		3) {"Nd","IIascii","Su"}, {"Nd"} _SYSCALL_
SYSCALL(write,		3) {"Nd","IOascii","Su"}, {"Nd"} _SYSCALL_
SYSCALL(open,		3) {"IIs","Topenarg2|openarg2f","No"}, {"Nd"} _SYSCALL_
SYSCALL(close,		1) {"Nd"}, OK _SYSCALL_
SYSCALL(wait,		0) {0}, {"Nd"} _SYSCALL_
SYSCALL(creat,		2) {"IIs","No"}, {"Nd"} _SYSCALL_
SYSCALL(link,		2) {"IIs","IIs"}, OK _SYSCALL_
SYSCALL(unlink,		1) {"IIs"}, OK _SYSCALL_
SYSCALL(exec,		2) {"IIs","Px"}, OK _SYSCALL_
SYSCALL(chdir,		1) {"IIs"}, OK _SYSCALL_
SYSCALL(time,		0) {0}, {"Nd"} _SYSCALL_
SYSCALL(mknod,		3) {"IIs","No","Nx"}, OK _SYSCALL_
SYSCALL(chmod,		2) {"IIs","%#llo"}, OK _SYSCALL_
SYSCALL(chown,		3) {"IIs","Nd","Nd"}, OK _SYSCALL_
SYSCALL(brk,		1) {"Px"}, OK _SYSCALL_
SYSCALL(stat,		2) {"IIs","Px"}, OK _SYSCALL_
/*
 * NB: We format the offset arg with "Nd" instead of "Od" because
 *     N32 kernels incorrectly truncate the 64-bit value when
 *     recording it in the event record.  If the arg is formatted
 *     with Od then negative values are incorrectly handled.
 */
SYSCALL(lseek,		3) {"Nd","Nd","Tseekarg3"}, {"Nd"} _SYSCALL_
SYSCALL(getpid,		0) {0}, {"Nd",", ppid=%lld"} _SYSCALL_
SYSCALL(mount,		6) {"IIs","IIs","Nd","Px","Px","Nd"}, OK _SYSCALL_
SYSCALL(umount,		1) {"IIs"}, OK _SYSCALL_
SYSCALL(setuid,		1) {"Nd"}, OK _SYSCALL_
DEFCALL(getuid)
    {-1, "getuid",  0, 0, 0, 0, {0}, {"Nd",", euid=%lld"}},
    {-1, "geteuid", 0, 0, 0, 0, {0}, {"Nd",", euid=%lld"}},
ENDCALL
SYSCALL(stime,		1) {"Nd"}, OK _SYSCALL_
SYSCALL(ptrace,		4) {"Nd","Nd","Px","Nd"}, {"Nd"} _SYSCALL_
SYSCALL(alarm,		1) {"Nd"}, {"Nd"} _SYSCALL_
SYSCALL(fstat,		2) {"Nd","Px"}, OK _SYSCALL_
SYSCALL(pause,		0) {0}, OK _SYSCALL_
SYSCALL(utime,		2) {"IIs","Px"}, OK _SYSCALL_
SYSCALL(nosys,		8) {"Px","Px","Px","Px","Px","Px","Px","Px"}, {"Nd"} _SYSCALL_
SYSCALL(access,		2) {"IIs","Taccessarg2"}, OK _SYSCALL_
SYSCALL(nice,		1) {"Nd"}, {"Nd"} _SYSCALL_
SYSCALL(statfs,		4) {"IIs","IOstatfs","Nd","Nd"}, OK _SYSCALL_
SYSCALL(sync,		0) {0}, OK _SYSCALL_
SYSCALL(kill,		2) {"Nd","Tsignames"}, OK _SYSCALL_
SYSCALL(fstatfs,	4) {"Nd","IOstatfs","Nd","Nd"}, OK _SYSCALL_
SYSCALL(setpgrp,	0) {0}, {"Nd"} _SYSCALL_
/* ssd() knows how to decode the first field */
#define ABI_ARG(abi, arg)	((abi << 24) | arg)
#define ABI_ALL			(ABI_IRIX5|ABI_IRIX5_64|ABI_IRIX5_N32)
#define	SGI_ALL(arg)		ABI_ARG(ABI_ALL, SGI_##arg)
#define	SGI_TSTAMP(arg)		ABI_ARG(ABI_ALL, SGI_RT_TSTAMP_##arg)
DEFCALL(syssgi)
    {-1, "syssgi", 6, 0, 0, 0,{"Tsyssgiarg1","Px","Px","Px","Px","Px"}, {"Nd"}},
    {SGI_ALL(SETGROUPS),"setgroups", 2, 1, 0, 0, {"Nd","Px"}, {"Nd"}},
    {SGI_ALL(GETGROUPS),"getgroups", 2, 1, 0, 0, {"Nd","Px"}, {"Nd"}},
    {SGI_ALL(SETSID),	"setsid", 0, 1, 0, 0, {0}, {"Nd"}},
    {SGI_ALL(SETPGID),	"setpgid", 2, 1, 0, 0, {"Nd","Nd"}, {"Nd"}},
    {SGI_ALL(SYSCONF),	"sysconf", 1, 1, 0, 0, {"Tsysconfarg1"}, {"Nd"}},
    {SGI_ALL(PATHCONF),	"pathconf", 3, 1, 0, 0, {"Px","Tpathconfarg2","Nd"}, {"Nd"}},
    {SGI_ALL(SETTIMEOFDAY),"settimeofday", 1, 1, 0, 0, {"Px"}, {"Nd"}},
    {SGI_ALL(SPROFIL),	"sprofil", 4, 1, 0, 0, {"Px","Nd","Px","Nd"}, {"Nd"}},
    {SGI_ALL(RUSAGE),	"getrusage", 2, 1, 0, 0, {"Nd","Px"}, {"Nd"}},
    {SGI_ALL(SIGSTACK),	"sigstack", 2, 1, 0, 0, {"Px","Px"}, {"Nd"}},
    {SGI_ALL(GETPGID),	"getpgid", 1, 1, 0, 0, {"Nd"}, {"Nd"}},
    {SGI_ALL(GETSID),	"getsid", 1, 1, 0, 0, {"Nd"}, {"Nd"}},
    {SGI_ALL(ELFMAP),	"elfmap", 3, 1, 0, 0, {"Nd","Px","Nd"}, {"Nx"}},
    {SGI_ALL(TOSSTSAVE),"syssgi", 1, 0, 0, 0, {"Tsyssgiarg1"}, OK},
    {SGI_ALL(FDHI),	"syssgi", 1, 0, 0, 0, {"Tsyssgiarg1"}, {"Nd"}},
    {SGI_ALL(QUERY_CYCLECNTR),
	"syssgi", 2, 0, 0, 0, {"Tsyssgiarg1","Px"}, {"Px"}},
    {SGI_ALL(GET_CONTEXT_NAME),
	"syssgi", 4, 0, 0, 0, {"Tsyssgiarg1","I","Px","Nd"}, {"Nd"}},
    {ABI_ARG(ABI_IRIX5_N32, SGI_READB),
	"syssgi", 7, 0, 0, 0, {"Tsyssgiarg1","Nd","Px","G","Ld","Nd"}, "Nd"},
    {ABI_ARG(ABI_IRIX5_N32, SGI_WRITEB),
	"syssgi", 7, 0, 0, 0, {"Tsyssgiarg1","Nd","Px","G","Ld","Nd"}, "Nd"},
    {SGI_TSTAMP(CREATE),"tstamp_create", 3, 1, 0, 0, {"Nd","Nd","Nd","Nx"}, {"Nd","Nx"}},
    {SGI_TSTAMP(DELETE),"tstamp_delete", 1, 1, 0, 0, {"Nd","Nd"}, {"Nd"}},
    {SGI_TSTAMP(START),	"tstamp_start", 1, 1, 0, 0, {"Nd","Nd"}, {"Nd"}},
    {SGI_TSTAMP(STOP),	"tstamp_stop", 1, 1, 0, 0, {"Nd","Nd"}, {"Nd"}},
    {SGI_TSTAMP(ADDR),	"tstamp_addr", 2, 1, 0, 0, {"Nd","Nd","Px"}, {"Nd","Nx"}},
    {ABI_ARG(ABI_IRIX5_N32, SGI_RT_TSTAMP_MASK),
	"tstamp_mask", 3, 1, 0, 0, {"Nd","Nd","G" "Lx"}, {"Nd"}},
    {SGI_TSTAMP(MASK),	"tstamp_mask", 2, 1, 0, 0, {"Nd","Nd","Nx"}, {"Nd"}},
    {SGI_TSTAMP(EOB_MODE),"tstamp_eob_mode", 2, 1, 0, 0, {"Nd","Nd","Nd"}, {"Nd"}},
    {SGI_TSTAMP(WAIT),	"tstamp_wait", 2, 1, 0, 0, {"Nd","Nd","Nd"}, {"Nd"}},
    {SGI_TSTAMP(UPDATE),"tstamp_update", 2, 1, 0, 0, {"Nd","Nd","Nd"}, {"Nd"}},
ENDCALL
#undef SGI_TSTAMP
#undef SGI_ALL
SYSCALL(dup,		1) {"Nd"}, {"Nd"} _SYSCALL_
SYSCALL(pipe,		0) {0}, {"Nd",", %lld"} _SYSCALL_
SYSCALL(times,		1) {"Px"}, {"Nd"} _SYSCALL_
SYSCALL(profil,		4) {"Px","Nd","Px","Px"}, OK _SYSCALL_
SYSCALL(plock,		1) {"Tplockarg1"}, OK _SYSCALL_
SYSCALL(setgid,		1) {"Nd"}, OK _SYSCALL_
/* putting multiple names here permits -N to find it */
DEFCALL(getgid)
    {-1, "getgid",  0, 0, 0, 0, {0}, {"Nd"," egid=%lld"}},
    {-1, "getegid", 0, 0, 0, 0, {0}, {"Nd"," egid=%lld"}},
ENDCALL
DEFCALL(signal)
    {-1,	"ssig",      3, 0, 0, 0, {"Tsignames","Px","Px"}, {"Nx"}},
    {0,		"signal",    3, 0, 0, 0, {"Tsignames","Px","Px"}, {"Nx"}},
    {SIGHOLD,	"sighold",   1, 0, 0, 0, {"Tsignames"}, OK},
    {SIGRELSE,	"sigrelse",  1, 0, 0, 0, {"Tsignames"}, OK},
    {SIGIGNORE,	"sigignore", 1, 0, 0, 0, {"Tsignames"}, OK},
    {SIGPAUSE,	"sigpause",  1, 0, 0, 0, {"Tsignames"}, OK},
    {SIGDEFER,	"sigset",    3, 0, 0, 0, {"Tsignames","Px","Px"}, {"Nx"}},
ENDCALL
DEFCALL(msgsys)
    {-1, "msgsys", 6, 0, 0, 0, {"Nd","Px","Px","Px","Px","Px"}, {"Nx"}},
    { 0, "msgget", 2, 1, 0, 0, {"Nd","No"}, {"Nd"}},
    { 1, "msgctl", 3, 1, 0, 0, {"Nd","Nd","Px"}, OK},
    { 2, "msgrcv", 5, 1, 0, 0, {"Nd","Px","Su","Nd","Nd"}, {"Nd"}},
    { 3, "msgsnd", 4, 1, 0, 0, {"Nd","Px","Su","Nd"}, OK},
ENDCALL
SYSCALL(sysmips,	4) {"Tsysmipsarg1","Px","Px","Px"}, OK _SYSCALL_
SYSCALL(acct,		1) {"Px"}, OK _SYSCALL_
DEFCALL(shmsys)
    {-1, "shmsys", 4, 0, 0, 0, {"Nd","Px","Px","Px"}, {"Nx"}},
    { 0, "shmat",  3, 1, 0, 0, {"Nd","Px","Nd"}, {"Px"}},
    { 1, "shmctl", 3, 1, 0, 0, {"Nd","Tshmctlarg2","Px"}, OK},
    { 2, "shmdt",  1, 1, 0, 0, {"Px"}, OK},
    { 3, "shmget", 3, 1, 0, 0, {"Nu","Su","No"}, {"Nd"}},
ENDCALL
DEFCALL(semsys)
    {-1, "semsys", 5, 0, 0, 0, {"Nd","Px","Px","Px","Px"}, {"#llx"}},
    { 0, "semctl", 4, 1, 0, 0, {"Nd","Nd","Tsemctlarg3","Px"}, {"Nd"}},
    { 1, "semget", 3, 1, 0, 0, {"Nd","Nd","No"}, {"Nd"}},
    { 2, "semop",  3, 1, 0, 0, {"Nd","Px","Nd"}, OK},
ENDCALL
DEFCALL(psema_cntl)
    {-1,	    "psema_cntl",  5, 0, 0, 0, {"Nd","IIs","Px","Px","Px"}, {"Nx"}},
    {PSEMA_OPEN,    "sem_open",    4, 1, 0, 0, {"IIs","Nd","No","Nd"}, {"Nd"}},
    {PSEMA_CLOSE,   "sem_close",   1, 1, 0, 0, {"Nd"}, {"Nd"}},
    {PSEMA_UNLINK,  "sem_unlink",  1, 1, 0, 0, {"IIs"}, {"Nd"}},
    {PSEMA_POST,    "sem_post",    1, 1, 0, 0, {"Nd"}, {"Nd"}},
    {PSEMA_WAIT,    "sem_wait",    1, 1, 0, 0, {"Nd"}, {"Nd"}},
    {PSEMA_TRYWAIT, "sem_trywait", 1, 1, 0, 0, {"Nd"}, {"Nd"}},
    {PSEMA_GETVALUE,"sem_getvalue",2, 1, 0, 0, {"Nd","IIint"}, {"Nd"}},
ENDCALL
/*
 * NB: We only get indirect param values for ioctls
 *     that use the encoded-style command args or
 *     that are special-cased in the kernel.
 */
DEFCALL(ioctl)
    {-1,		"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","Px"}, OK},
    /* from sys/ioctl.h */
    {FIONREAD,		"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IOint"},OK},
    {FIONBIO,		"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IIint"},OK},
    {FIOASYNC,		"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IIint"},OK},
    {FIOSETOWN,		"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IIint"},OK},
    {FIOGETOWN,		"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IOint"},OK},
    /* from net/soioctl.h */
    {SIOCSHIWAT,	"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IIint"},OK},
    {SIOCGHIWAT,	"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IOint"},OK},
    {SIOCSLOWAT,	"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IIint"},OK},
    {SIOCGLOWAT,	"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IOint"},OK},
    {SIOCATMARK,	"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IOint"},OK},
    {SIOCSPGRP,		"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IIint"},OK},
    {SIOCGPGRP,		"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IOint"},OK},
    {SIOCNREAD,		"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IOint"},OK},
    /* from sys/termios.h */
    {__NEW_TCGETA,	"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IOntermio"},OK},
    {__NEW_TCSETA,	"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IIntermio"},OK},
    {__NEW_TCSETAW,	"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IIntermio"},OK},
    {__NEW_TCSETAF,	"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IIntermio"},OK},
    {__OLD_TCGETA,	"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IOotermio"},OK},
    {__OLD_TCSETA,	"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IIotermio"},OK},
    {__OLD_TCSETAW,	"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IIotermio"},OK},
    {__OLD_TCSETAF,	"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IIotermio"},OK},
    {TCSBRK,		"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IIint"},OK},
    {TCXONC,		"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IIint"},OK},
    {TCFLSH,		"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IIint"},OK},

    {TIOCSPGRP,		"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IIint"},OK},
    {TIOCGPGRP,		"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IOint"},OK},
    {TIOCSWINSZ,	"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IIwinsize"},OK},
    {TIOCGWINSZ,	"ioctl", 3,0,0,0, {"Nd","Tioctlarg2","IOwinsize"},OK},
ENDCALL
SYSCALL(uadmin,		3) {"Nd","Nd","Px"}, OK _SYSCALL_
DEFCALL(sysmp)
    {-1,		"sysmp", 5, 0, 0, 0, {"Tsysmparg1","Px","Px","Px","Px"}, {"Nd"}},
    {MP_NPROCS,		"sysmp", 1, 0, 0, 0, {"Tsysmparg1"}, {"Nd"}},
    {MP_NAPROCS,	"sysmp", 1, 0, 0, 0, {"Tsysmparg1"}, {"Nd"}},
    {MP_KERNADDR,	"sysmp", 2, 0, 0, 0, {"Tsysmparg1","Nd"}, {"Nx"}},
    {MP_SASZ,		"sysmp", 2, 0, 0, 0, {"Tsysmparg1","Nd"}, {"Nd"}},
    {MP_SAGET,		"sysmp", 4, 0, 0, 0, {"Tsysmparg1","Nd","Px","Px"}, OK},
    {MP_SAGET1,		"sysmp", 5, 0, 0, 0, {"Tsysmparg1","Nd","Px","Px","Nd"}, OK},
    {MP_EMPOWER,	"sysmp", 2, 0, 0, 0, {"Tsysmparg1","Nd"}, OK},
    {MP_RESTRICT,	"sysmp", 2, 0, 0, 0, {"Tsysmparg1","Nd"}, OK},
    {MP_CLOCK,		"sysmp", 2, 0, 0, 0, {"Tsysmparg1","Nd"}, OK},
    {MP_MUSTRUN,	"sysmp", 2, 0, 0, 0, {"Tsysmparg1","Nd"}, OK},
    {MP_RUNANYWHERE,	"sysmp", 1, 0, 0, 0, {"Tsysmparg1"}, OK},
    {MP_STAT,		"sysmp", 2, 0, 0, 0, {"Tsysmparg1","Px"}, OK},
    {MP_ISOLATE,	"sysmp", 2, 0, 0, 0, {"Tsysmparg1","Nd"}, OK},
    {MP_UNISOLATE,	"sysmp", 2, 0, 0, 0, {"Tsysmparg1","Nd"}, OK},
    {MP_PREEMPTIVE,	"sysmp", 2, 0, 0, 0, {"Tsysmparg1","Nd"}, OK},
    {MP_NONPREEMPTIVE,	"sysmp", 2, 0, 0, 0, {"Tsysmparg1","Nd"}, OK},
    {MP_FASTCLOCK,	"sysmp", 2, 0, 0, 0, {"Tsysmparg1","Nd"}, OK},
    {MP_CLEARNFSSTAT,	"sysmp", 1, 0, 0, 0, {"Tsysmparg1"}, OK},

    {MP_PGSIZE,		"getpagesize",	0, 1, 0, 0, {"Tsysmparg1"}, {"Nd"}},
    {MP_SCHED,		"schedctl",	3, 1, 0, 0, {"Tschedctlarg1","Nd","Nd"}, {"Nx"}},
ENDCALL
DEFCALL(utssys)
    {-1, "utssys", 3, 0, 0, 0, {"Px","Px","Nd"}, OK},
    { 0, "uname",  1, 3, 0, 0, {"Px"}, OK},
    { 2, "ustat",  2, 3, 0, 0, {"Px","Px"}, OK},
ENDCALL
/* putting multiple names here permits -N to find it */
DEFCALL(execve)
    {-1, "execve", 3, 0, 0, 0, {"IIs","Px","Px"}, OK},
    {-1, "execl",  3, 0, 0, 0, {"IIs","Px","Px"}, OK},
    {-1, "execv",  3, 0, 0, 0, {"IIs","Px","Px"}, OK},
    {-1, "execle", 3, 0, 0, 0, {"IIs","Px","Px"}, OK},
    {-1, "execlp", 3, 0, 0, 0, {"IIs","Px","Px"}, OK},
    {-1, "execvp", 3, 0, 0, 0, {"IIs","Px","Px"}, OK},
ENDCALL
SYSCALL(umask,		1) {"No"}, {"No"} _SYSCALL_
SYSCALL(chroot,		1) {"IIs"}, OK _SYSCALL_
DEFCALL(fcntl)
    {-1,	   "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","Px"}, {"Nd"}},
    {F_DUPFD,	   "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","Nd"}, {"Nd"}},
    {F_GETFD,	   "fcntl", 2, 0, 0, 0, {"Nd","Tfcntlarg2"}, {"Nd"}},
    {F_SETFD,	   "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","Nd"}, OK},
    {F_GETFL,	   "fcntl", 2, 0, 0, 0, {"Nd","Tfcntlarg2"}, {"Nx"}},
    {F_SETFL,	   "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","Tfcntlflags"}, OK},
    {F_GETLK,	   "fcntl", 3, 0, 0, 1, {"Nd","Tfcntlarg2","IBflock"}, OK},
    {F_SETLK,	   "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IIflock"}, OK},
    {F_SETLKW,	   "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IIflock"}, OK},
    {F_CHKFL,	   "fcntl", 2, 0, 0, 0, {"Nd","Tfcntlarg2"}, OK},
    {F_ALLOCSP,	   "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IIflock"}, OK},
    {F_FREESP,	   "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IIflock"}, OK},
    {F_SETBSDLK,   "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IIflock"}, OK},
    {F_SETBSDLKW,  "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IIflock"}, OK},
    {F_DIOINFO,	   "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IOdioinfo"}, OK},
    {F_FSGETXATTR, "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IOxattr"}, OK},
    {F_FSSETXATTR, "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IIxattr"}, OK},
    {F_GETLK64,	   "fcntl", 3, 0, 0, 1, {"Nd","Tfcntlarg2","IBflock64"}, OK},
    {F_SETLK64,	   "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IIflock64"}, OK},
    {F_SETLKW64,   "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IIflock64"}, OK},
    {F_ALLOCSP64,  "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IIflock64"}, OK},
    {F_FREESP64,   "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IIflock64"}, OK},
    {F_GETBMAP,	   "fcntl", 3, 0, 0, 1, {"Nd","Tfcntlarg2","IBgetbmap"}, OK},
    {F_FSSETDM,	   "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IIfssetdm"}, OK},
    {F_RESVSP,	   "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IIflock"}, OK},
    {F_UNRESVSP,   "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IIflock"}, OK},
    {F_RESVSP64,   "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IIflock64"}, OK},
    {F_UNRESVSP64, "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IIflock64"}, OK},
    {F_GETBMAPA,   "fcntl", 3, 0, 0, 1, {"Nd","Tfcntlarg2","IBgetbmap"}, OK},
    {F_FSGETXATTRA,"fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IOxattr"}, OK},
    {F_RSETLK,	   "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IIflock"}, OK},
    {F_RGETLK,	   "fcntl", 3, 0, 0, 1, {"Nd","Tfcntlarg2","IBflock"}, OK},
    {F_RSETLKW,	   "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","IIflock"}, OK},
    {F_GETOWN,	   "fcntl", 2, 0, 0, 0, {"Nd","Tfcntlarg2"}, {"Nd"}},
    {F_SETOWN,	   "fcntl", 3, 0, 0, 0, {"Nd","Tfcntlarg2","Nd"}, OK},
ENDCALL
SYSCALL(ulimit,		2) {"Nd","Nd"}, {"Nd"} _SYSCALL_
SYSCALL(getrlimit64,	2) {"Trlimitarg1","Px"}, OK _SYSCALL_
SYSCALL(setrlimit64,	2) {"Trlimitarg1","Px"}, OK _SYSCALL_
SYSCALL(nanosleep,	2) {"IItimespec","IOtimespec"}, OK _SYSCALL_
SYSCALL(lseek64,	5) {"Nd","G","Ld","Tseekarg3"}, {"Ld"} _SYSCALL_
SYSCALL(rmdir,		1) {"IIs","Nd"}, OK _SYSCALL_
SYSCALL(mkdir,		2) {"IIs","No"}, OK _SYSCALL_
SYSCALL(getdents,	3) {"Nd","Px","Nd"}, {"Nd"} _SYSCALL_
SYSCALL(sginap,		1) {"Nd"}, {"Nd"} _SYSCALL_
SYSCALL(sgikopt,	3) {"Px","Px","Nd"}, OK _SYSCALL_
SYSCALL(sysfs,		3) {"Nd","Px","Px"}, {"Nd"} _SYSCALL_
SYSCALL(getmsg,		4) {"Nd","Px","Px","Nd"}, {"Nd"} _SYSCALL_
SYSCALL(putmsg,		4) {"Nd","Px","Px","Nd"}, {"Nd"} _SYSCALL_
SYSCALB(poll,		3) {"IBpollfd","Nd","Nd"}, {"Nd"} _SYSCALL_
SYSCALL(sigreturn,	1) {"Px"}, OK _SYSCALL_
SYSCALL(accept,		3) {"Nd","IOsockadd","IBint"}, {"Nd"} _SYSCALL_
SYSCALL(bind,		3) {"Nd","IIsockadd","Nd"}, OK _SYSCALL_
SYSCALL(connect,	3) {"Nd","IIsockadd","Nd"}, OK _SYSCALL_
SYSCALL(gethostid,	0) {0}, {"Nd"} _SYSCALL_
SYSCALL(getpeername,	3) {"Nd","IOsockadd","IBint"}, OK _SYSCALL_
SYSCALL(getsockname,	3) {"Nd","IOsockadd","IBint"}, OK _SYSCALL_
SYSCALL(getsockopt,	5) {"Nd","Tproto","Tsockopt","Px","Px"}, OK _SYSCALL_
SYSCALL(listen,		2) {"Nd","Nd"}, OK _SYSCALL_
SYSCALL(recv,		4) {"Nd","IOascii","Nd","Tsendflags"}, {"Nd"} _SYSCALL_
SYSCALL(recvfrom,	6) {"Nd","IOascii","Nd","Tsendflags","IOsockadd","IBint"}, {"Nd"} _SYSCALL_
SYSCALL(recvmsg,	3) {"Nd","Px","Tsendflags"}, {"Nd"} _SYSCALL_
SYSCALB(select,		5) {"Nd","IBfd_set","IBfd_set","IBfd_set","IItimeval"}, {"Nd"} _SYSCALL_
SYSCALL(send,		4) {"Nd","IIascii","Nd","Tsendflags"}, {"Nd"} _SYSCALL_
SYSCALL(sendmsg,	3) {"Nd","Px","Tsendflags"}, {"Nd"} _SYSCALL_
SYSCALL(sendto,		6) {"Nd","IIascii","Nd","Tsendflags","IIsockadd","Nd"}, {"Nd"} _SYSCALL_
SYSCALL(sethostid,	1) {"Nd"}, {"Nd"} _SYSCALL_
SYSCALL(setsockopt,	5) {"Nd","Tproto","Tsockopt","Px","Px"}, OK _SYSCALL_
SYSCALL(shutdown,	2) {"Nd","Nd"}, OK _SYSCALL_
SYSCALL(socket,		3) {"Tsocketarg1","Tsocketarg2","Nd"}, {"Nd"} _SYSCALL_
SYSCALL(gethostname,	2) {"IOs","Nd"}, OK _SYSCALL_
SYSCALL(sethostname,	2) {"IIs","Nd"}, OK _SYSCALL_
SYSCALL(getdomainname,	2) {"IOs","Nd"}, OK _SYSCALL_
SYSCALL(setdomainname,	2) {"IIs","Nd"}, OK _SYSCALL_
SYSCALL(truncate,	2) {"IIs","Od"}, OK _SYSCALL_
SYSCALL(ftruncate,	2) {"Nd","Od"}, OK _SYSCALL_
SYSCALL(rename,		2) {"IIs","IIs"}, OK _SYSCALL_
SYSCALL(symlink,	2) {"IIs","IIs"}, OK _SYSCALL_
SYSCALL(readlink,	3) {"IIs","IOs","Nd"}, {"Nd"} _SYSCALL_
SYSCALL(lstat,		2) {"Px","Px"}, OK _SYSCALL_
SYSCALL(nfs_svc,	1) {"Nd"}, OK _SYSCALL_
SYSCALL(nfs_getfh,	2) {"Px","Px"}, OK _SYSCALL_
SYSCALL(async_daemon,	0) {0}, OK _SYSCALL_
SYSCALL(exportfs,	3) {"Px","Px","Px"}, OK _SYSCALL_
SYSCALL(setregid,	2) {"Nd","Nd"}, OK _SYSCALL_
SYSCALL(setreuid,	2) {"Nd","Nd"}, OK _SYSCALL_
SYSCALL(getitimer,	2) {"Titimerarg1","Px"}, OK _SYSCALL_
SYSCALL(setitimer,	3) {"Titimerarg1","Px","Px"}, OK _SYSCALL_
SYSCALL(adjtime,	2) {"Px","Px"}, OK _SYSCALL_
DEFCALL(bsdgettime)
    {-1, "gettimeofday", 1, 0, 0, 0, {"IOtimeval"}, OK},
ENDCALL
SYSCALL(sproc,		3) {"Px","Tsprocarg2","Px"}, {"Nd"} _SYSCALL_
DEFCALL(prctl)
    {-1,		"prctl", 3, 0, 0, 0, {"Tprctlarg1","Px","Px"}, {"Nd"}},
    {PR_MAXPROCS,	"prctl", 1, 0, 0, 0, {"Tprctlarg1"}, {"Nd"}},
    {PR_ISBLOCKED,	"prctl", 2, 0, 0, 0, {"Tprctlarg1","Nd"}, {"Nd"}},
    {PR_SETSTACKSIZE,	"prctl", 2, 0, 0, 0, {"Tprctlarg1","Nd"}, {"Nd"}},
    {PR_GETSTACKSIZE,	"prctl", 1, 0, 0, 0, {"Tprctlarg1"}, {"Nd"}},
    {PR_MAXPPROCS,	"prctl", 1, 0, 0, 0, {"Tprctlarg1"}, {"Nd"}},
    {PR_UNBLKONEXEC,	"prctl", 2, 0, 0, 0, {"Tprctlarg1","Nd"}, OK},
    {PR_SETEXITSIG,	"prctl", 2, 0, 0, 0, {"Tprctlarg1","Tsignames"}, OK},
    {PR_RESIDENT,	"prctl", 1, 0, 0, 0, {"Tprctlarg1"}, OK},
    {PR_ATTACHADDR,	"prctl", 3, 0, 0, 0, {"Tprctlarg1","Nd","Px"}, {"Nx"}},
    {PR_TERMCHILD,	"prctl", 1, 0, 0, 0, {"Tprctlarg1"}, OK},
    {PR_GETSHMASK,	"prctl", 2, 0, 0, 0, {"Tprctlarg1","Nd"}, {"Nx"}},
    {PR_GETNSHARE,	"prctl", 1, 0, 0, 0, {"Tprctlarg1"}, {"Nd"}},
    {PR_COREPID,	"prctl", 2, 0, 0, 0, {"Tprctlarg1","Nd"}, {"Nd"}},
    {PR_LASTSHEXIT,	"prctl", 1, 0, 0, 0, {"Tprctlarg1"}, {"Nd"}},
ENDCALL
DEFCALL(procblk)
    {-1, "procblk",		3, 0, 0, 0, {"Nd","Nd","Nd"}, OK},
    { 0, "blockproc",		1, 1, 0, 0, {"Nd"}, OK},
    { 1, "unblockproc",		1, 1, 0, 0, {"Nd"}, OK},
    { 2, "setblockproccnt",	2, 1, 0, 0, {"Nd","Nd"}, OK},
    { 3, "blockprocall",	1, 1, 0, 0, {"Nd"}, OK},
    { 4, "unblockprocall",	1, 1, 0, 0, {"Nd"}, OK},
    { 5, "setblockproccntall",	2, 1, 0, 0, {"Nd","Nd"}, OK},
ENDCALL
SYSCALL(sprocsp,	5) {"Px","Tsprocarg2","Px","Px","Px"}, {"Nd"} _SYSCALL_
SYSCALL(mmap,		6) {"Px","Su","Tmmaparg3","Tmmaparg4","Nd","Od"}, {"Nx"} _SYSCALL_
SYSCALL(munmap,		2) {"Px","Su"}, OK _SYSCALL_
SYSCALL(mprotect,	3) {"Px","Su","Tmmaparg3"}, OK _SYSCALL_
SYSCALL(msync,		3) {"Px","Su","Tmsyncarg3"}, OK _SYSCALL_
SYSCALL(madvise,	3) {"Px","Su","Tmadvisearg3"}, OK _SYSCALL_
DEFCALL(pagelock)
    {-1,     "pagelock", 3, 0, 0, 0, {"Px","Su","Tmsyncarg3"}, OK},
    {PGLOCK, "mpin",	 2, 3, 0, 0, {"Px","Su"}, OK},
    {UNLOCK, "munpin",	 2, 3, 0, 0, {"Px","Su"}, OK},
ENDCALL
SYSCALL(getpagesize,	0) {0}, {"Nd"} _SYSCALL_
SYSCALL(quotactl,	4) {"Tquotaarg1","Px","Nd","Px"}, OK _SYSCALL_
SYSCALL(BSDgetpgrp,	1) {"Nd"}, {"Nd"} _SYSCALL_
SYSCALL(BSDsetpgrp,	2) {"Nd","Nd"}, {"Nd"} _SYSCALL_
SYSCALL(vhangup,	0) {0}, OK _SYSCALL_
SYSCALL(fsync,		1) {"Nd"}, OK _SYSCALL_
SYSCALL(fchdir,		1) {"Nd"}, OK _SYSCALL_
SYSCALL(getrlimit,	2) {"Trlimitarg1","Px"}, OK _SYSCALL_
SYSCALL(setrlimit,	2) {"Trlimitarg1","Px"}, OK _SYSCALL_
SYSCALL(cacheflush,	3) {"Px","Nd","Tcacheflusharg3"}, OK _SYSCALL_
SYSCALL(cachectl,	3) {"Px","Nd","Tcachectlarg3"}, OK _SYSCALL_
SYSCALL(fchown,		3) {"Nd","Nd","Nd"}, OK _SYSCALL_
SYSCALL(fchmod,		2) {"Nd","No"}, OK _SYSCALL_
SYSCALL(wait3,		3) {"Px","Nd","Px"}, {"Nd"} _SYSCALL_
SYSCALL(socketpair,	4) {"Tsocketarg1","Tsocketarg2","Nd","IOint"}, OK _SYSCALL_
SYSCALL(sysinfo,	3) {"Tsysinfoarg1","Px","Nd"}, {"Nd"} _SYSCALL_
SYSCALL(nuname,		1) {"Px"}, OK _SYSCALL_
DEFCALL(xstat)
    {-1, "xstat",   3, 0, 0, 0, {"vers=%lld", "IIs","Px"}, OK},
    { 2, "stat",    2, 1, 0, 0, {"IIs","IOstat"}, OK},
    { 3, "stat64",  2, 1, 0, 0, {"IIs","IOstat64"}, OK},
ENDCALL
DEFCALL(lxstat)
    {-1, "lxstat",  3, 0, 0, 0, {"vers=%lld", "IIs","Px"}, OK},
    { 2, "lstat",   2, 1, 0, 0, {"IIs","IOstat"}, OK},
    { 3, "lstat64", 2, 1, 0, 0, {"IIs","IOstat64"}, OK},
ENDCALL
DEFCALL(fxstat)
    {-1, "fxstat",  3, 0, 0, 0, {"vers=%lld", "Nd","Px"}, OK},
    { 2, "fstat",   2, 1, 0, 0, {"Nd","IOstat"}, OK},
    { 3, "fstat64", 2, 1, 0, 0, {"Nd","IOstat64"}, OK},
ENDCALL
SYSCALL(xmknod,		4) {"vers=%lld", "IIs","No","Px"}, OK _SYSCALL_
/* NB: really 4 args but the last isn't user-visible */
SYSCALL(sigaction,	3) {"Tsignames","IIsigact","IOsigact"}, OK _SYSCALL_
SYSCALL(sigpending,	1) {"IOsigset"}, OK _SYSCALL_
SYSCALL(sigprocmask,	3) {"Tsigprocmaskarg1","IIsigset","IOsigset"}, OK _SYSCALL_
SYSCALL(sigsuspend,	1) {"IIsigset"}, OK _SYSCALL_
SYSCALL(sigpoll,	3) {"IIsigset", "IOsiginfo", "IItimespec"}, {"Nd"} _SYSCALL_
SYSCALL(swapctl,	2) {"Tswapctlarg1", "Px"}, {"Nd"} _SYSCALL_
SYSCALL(getcontext,	1) {"Px"}, {"Nd"} _SYSCALL_
SYSCALL(setcontext,	1) {"Px"}, {"Nd"} _SYSCALL_
SYSCALL(waitsys,	5) {"Twaittype","Nd","IOwaitsys","Twaitopt","Px"}, {"Nd"} _SYSCALL_
SYSCALL(sigstack,	2) {"Px","Px"}, {"Nd"} _SYSCALL_
SYSCALL(sigaltstack,	2) {"Px","Px"}, {"Nd"} _SYSCALL_
SYSCALL(sigsendset,	2) {"Px","Tsignames"}, {"Nd"} _SYSCALL_
SYSCALL(statvfs,	2) {"IIs","IOstatvfs"}, {"Nd"} _SYSCALL_
SYSCALL(fstatvfs,	2) {"Nd","IOstatvfs"}, {"Nd"} _SYSCALL_
SYSCALL(getpmsg,	5) {"Nd","Px","Px","Px","Px"}, {"Nd"} _SYSCALL_
SYSCALL(putpmsg,	5) {"Nd","Px","Px","Px","Px"}, {"Nd"} _SYSCALL_
SYSCALL(lchown,		3) {"IIs","Nd","Nd"}, OK _SYSCALL_
SYSCALL(priocntl,	0) {0}, OK _SYSCALL_
SYSCALL(sigqueue,	4) {"Nd","Tsignames","Px","Px"}, OK _SYSCALL_
SYSCALL(readv,		3) {"Nd","Px","Nd"}, {"Nd"} _SYSCALL_
SYSCALL(writev,		3) {"Nd","Px","Nd"}, {"Nd"} _SYSCALL_
SYSCALL(truncate64,	4) {"IIs","G","Ld"}, OK _SYSCALL_
SYSCALL(ftruncate64,	4) {"Nd","G","Ld"}, OK _SYSCALL_
SYSCALL(mmap64,		8) {"Px","Nd","Tmmaparg3","Tmmaparg4","Nd","G","Lu"}, {"Nx"} _SYSCALL_
SYSCALL(dmi,		8) {"Nd","Px","Px","Px","Px","Px","Px","Px"}, {"Nd"} _SYSCALL_
/* kad() knows how to decode the first value */
#define	KABI(kabi,abi)	(((kabi)<<16)|(abi))
DEFCALL(pread)
    {-1,	   "pread", 6,0,0,0, {"Nd","IIascii","Su","Px","Px","Px"}, {"Nd"}},
    {ABI_IRIX5,	   "pread", 6,0,0,0, {"Nd","IIascii","Su","G","Ld"}, {"Nd"}},
    {ABI_IRIX5_64, "pread", 4,0,0,0, {"Nd","IIascii","Su","Od"}, {"Nd"}},
    /* N32 apps on 64-bit kernels pass the offset as one value */
    {KABI(ABI_IRIX5_64,ABI_IRIX5_N32),
		   "pread", 4,0,0,0, {"Nd","IIascii","Su","Od"}, {"Nd"}},
    {KABI(ABI_IRIX5_N32,ABI_IRIX5_N32),
		   "pread", 6,0,0,0, {"Nd","IIascii","Su","G","Ld"}, {"Nd"}},
ENDCALL
DEFCALL(pwrite)
    {-1,	   "pwrite", 6,0,0,0, {"Nd","IOascii","Su","Px","Px","Px"}, {"Nd"}},
    {ABI_IRIX5,    "pwrite", 6,0,0,0, {"Nd","IOascii","Su","G","Ld"}, {"Nd"}},
    {ABI_IRIX5_64, "pwrite", 4,0,0,0, {"Nd","IOascii","Su","Od"}, {"Nd"}},
    /* N32 apps on 64-bit kernels pass the offset as one value */
    {KABI(ABI_IRIX5_64,ABI_IRIX5_N32),
		   "pwrite", 4,0,0,0, {"Nd","IOascii","Su","Od"}, {"Nd"}},
    {KABI(ABI_IRIX5_N32,ABI_IRIX5_N32),
		   "pwrite", 6,0,0,0, {"Nd","IOascii","Su","G","Ld"}, {"Nd"}},
ENDCALL
SYSCALL(fdatasync,	1) {"Sd"}, {"Nd"} _SYSCALL_
SYSCALL(sgifastpath,	6) {"Su","Su","Su","Su","Su","Su"}, {"Nu"} _SYSCALL_
SYSCALL(attr_get,	5) {"IIs","IIs","IIascii","IIint","Tattr_flags"}, {"Nd"} _SYSCALL_
SYSCALL(attr_getf,	5) {"Nd","IIs","IIascii","IIint","Tattr_flags"}, {"Nd"} _SYSCALL_
SYSCALL(attr_set,	5) {"IIs","IIs","IOascii","Nd","Tattr_flags"},{"Nd"} _SYSCALL_
SYSCALL(attr_setf,	5) {"Nd","IIs","IOascii","Nd","Tattr_flags"},{"Nd"} _SYSCALL_
SYSCALL(attr_remove,	3) {"IIs","IIs","Tattr_flags"}, {"Nd"} _SYSCALL_
SYSCALL(attr_removef,	3) {"Nd","IIs","Tattr_flags"}, {"Nd"} _SYSCALL_
SYSCALL(attr_list,	5) {"IIs","Px","Nd","Tattr_flags","Px"}, {"Nd"} _SYSCALL_
SYSCALL(attr_listf,	5) {"Nd","Px","Nd","Tattr_flags","Px"}, {"Nd"} _SYSCALL_
SYSCALL(attr_multi,	4) {"IIs","Px","Nd","Tattr_flags"}, {"Nd"} _SYSCALL_
SYSCALL(attr_multif,	4) {"Nd","Px","Nd","Tattr_flags"}, {"Nd"} _SYSCALL_
SYSCALL(statvfs64,	2) {"IIs","IOstatvfs64"}, {"Nd"} _SYSCALL_
SYSCALL(fstatvfs64,	2) {"Nd","IOstatvfs64"}, {"Nd"} _SYSCALL_
SYSCALL(getmountid,	2) {"IIs","Px"}, OK _SYSCALL_
SYSCALL(nsproc,		5) {"Px","Tsprocarg2","Px","Px","Px"}, {"Nd"} _SYSCALL_
SYSCALL(getdents64,	3) {"Nd","Px","Nd"}, {"Nd"} _SYSCALL_
SYSCALL(ngetdents,	4) {"Nd","Px","Nd","Px"}, {"Nd"} _SYSCALL_
SYSCALL(ngetdents64,	4) {"Nd","Px","Nd","Px"}, {"Nd"} _SYSCALL_
SYSCALL(pidsprocsp,	6) {"Px","Tsprocarg2","Px","Px","Px","Nd"}, {"Nd"} _SYSCALL_
/* putting multiple names here permits -N to find it */
DEFCALL(rexec)
    {-1, "rexecve", 4, 0, 0, 0, {"Nd","IIs","Px","Px"}, OK},
    {-1, "rexecl",  4, 0, 0, 0, {"Nd","IIs","Px","Px"}, OK},
    {-1, "rexecv",  4, 0, 0, 0, {"Nd","IIs","Px","Px"}, OK},
    {-1, "rexecle", 4, 0, 0, 0, {"Nd","IIs","Px","Px"}, OK},
    {-1, "rexeclp", 4, 0, 0, 0, {"Nd","IIs","Px","Px"}, OK},
    {-1, "rexecvp", 4, 0, 0, 0, {"Nd","IIs","Px","Px"}, OK},
ENDCALL
SYSCALL(timer_create,	3) {"Tclockid_arg","Px","Px"},{"Nd"} _SYSCALL_
SYSCALL(timer_delete,	1) {"Nd"}, {"Nd"} _SYSCALL_
SYSCALL(timer_settime,	4) {"Nd","Nd","Px", "Px"}, {"Nd"} _SYSCALL_
SYSCALL(timer_gettime,	2) {"Nd", "Px"}, {"Nd"} _SYSCALL_
SYSCALL(timer_getoverrun,1) {"Nd"}, {"Nd"} _SYSCALL_
SYSCALL(sched_rr_get_interval,	2)  {"Nd", "Px"}, {"Nd"} _SYSCALL_
SYSCALL(sched_yield,	0) {0}, {"Nd"} _SYSCALL_
SYSCALL(sched_getscheduler,1)  {"Nd"}, {"Nd"} _SYSCALL_
SYSCALL(sched_setscheduler,3)  {"Nd","Tsched_policy","Px"}, {"Nd"} _SYSCALL_
SYSCALL(sched_getparam,	2) {"Nd","Px"}, {"Nd"} _SYSCALL_
SYSCALL(sched_setparam,	2) {"Nd","Px"}, {"Nd"} _SYSCALL_
SYSCALL(usync_cntl,	2) {"Tusync_cntlarg1","Px"}, {"Nd"} _SYSCALL_
SYSCALL(ckpt_restartreturn,3)  {"Nd","Nd","Nd"}, {"Nd"} _SYSCALL_
SYSCALL(sysget,		5) {"Nd","Px","Nd","Nd","Px"}, {"Nd"} _SYSCALL_
SYSCALL(xpg4_recvmsg,	3) {"Nd","Px","Tsendflags"}, {"Nd"} _SYSCALL_
SYSCALL(linkfollow,	2) {"IIs","IIs"}, OK _SYSCALL_

#undef OK
#undef _SYSCALL_
#undef SYSCALL
#undef SYSCALB
#undef ENDCALL
#undef DEFCALL

/* master system call table */
static const syscall_t syscalltab[] = {
	{0, 0, syscall_call},	{0, 0, exit_call},	{0, 0, fork_call},
	{0, 0, read_call},	{0, 0, write_call},	{0, 0, open_call},
	{0, 0, close_call},	{0, 0, wait_call},	{0, 0, creat_call},
	{0, 0, link_call},	{0, 0, unlink_call},	{0, 0, exec_call},
/*12*/	{0, 0, chdir_call},	{0, 0, time_call},	{0, 0, mknod_call},
	{0, 0, chmod_call},	{0, 0, chown_call},	{0, 0, brk_call},
	{0, 0, stat_call},	{0, 0, lseek_call},	{0, 0, getpid_call},
/*21*/	{0, 0, mount_call},	{0, 0, umount_call},	{0, 0, setuid_call},
	{0, 0, getuid_call},	{0, 0, stime_call},	{0, 0, ptrace_call},
	{0, 0, alarm_call},	{0, 0, fstat_call},	{0, 0, pause_call},
	{0, 0, utime_call},	{0, 0, nosys_call},	{0, 0, nosys_call},
/*33*/	{0, 0, access_call}, 	{0, 0, nice_call}, 	{0, 0, statfs_call},
	{0, 0, sync_call}, 	{0, 0, kill_call},	{0, 0, fstatfs_call},
/*39*/	{0, 0, setpgrp_call},	{0, ssd, syssgi_call},	{0, 0, dup_call},
	{0, 0, pipe_call},	{0, 0, times_call},	{0, 0, profil_call},
	{0, 0, plock_call},	{0, 0, setgid_call},	{0, 0, getgid_call},
/*48*/	{0, sigd, signal_call},	{0, gd, msgsys_call},	{0, 0, sysmips_call},
/*51*/	{0, 0, acct_call},	{0, gd, shmsys_call},	{0, gd, semsys_call},
/*54*/	{1, gd, ioctl_call},	{0, 0, uadmin_call},	{0, gd, sysmp_call},
/*57*/	{2, gd, utssys_call},	{0, 0, nosys_call},	{0, 0, execve_call},
/*60*/	{0, 0, umask_call},	{0, 0, chroot_call},	{1, gd, fcntl_call},
/*63*/	{0, 0, ulimit_call},	{0, 0, nosys_call},	{0, 0, nosys_call},
/*67*/	{0, 0, nosys_call},	{0, 0, nosys_call},	{0, 0, nosys_call},
/*70*/	{0, 0, nosys_call},	{0, 0, nosys_call},	{0, 0, nosys_call},
/*73*/	{0, 0, nosys_call},	{0, 0, nosys_call},	{0, 0, nosys_call},
/*75*/	{0, 0,getrlimit64_call},{0, 0,setrlimit64_call},{0, 0, nanosleep_call},
/*78*/	{0, 0, lseek64_call},	{0, 0, rmdir_call},	{0, 0, mkdir_call},
/*81*/	{0, 0, getdents_call},	{0, 0, sginap_call},	{0, 0, sgikopt_call},
/*84*/	{0, 0, sysfs_call},	{0, 0, getmsg_call},	{0, 0, putmsg_call},
/*87*/	{0, 0, poll_call},	{0, 0, sigreturn_call},	{0, 0, accept_call},
/*90*/	{0, 0, bind_call},	{0, 0, connect_call},	{0, 0, gethostid_call},
/*93*/	{0, 0,getpeername_call},{0, 0,getsockname_call},{0, 0, getsockopt_call},
/*96*/	{0, 0, listen_call},	{0, 0, recv_call},	{0, 0, recvfrom_call},
/*99*/	{0, 0, recvmsg_call},	{0, 0, select_call},	{0, 0, send_call},
/*102*/	{0, 0, sendmsg_call},	{0, 0, sendto_call},	{0, 0, sethostid_call},
/*105*/	{0, 0, setsockopt_call},{0, 0, shutdown_call},	{0, 0, socket_call},
/*108*/	{0, 0,gethostname_call},{0,0,sethostname_call},{0,0,getdomainname_call},
/*111*/	{0,0,setdomainname_call},{0,0, truncate_call},  {0, 0, ftruncate_call},
/*114*/	{0, 0, rename_call},	{0, 0, symlink_call},	{0, 0, readlink_call},
/*117*/	{0, 0, lstat_call},	{0, 0, nosys_call},	{0, 0, nfs_svc_call},
/*120*/	{0, 0, nfs_getfh_call},	{0,0,async_daemon_call},{0, 0, exportfs_call},
/*123*/	{0, 0, setregid_call},	{0, 0, setreuid_call},	{0, 0, getitimer_call},
/*126*/	{0, 0, setitimer_call},	{0, 0, adjtime_call},	{0, 0, bsdgettime_call},
/*129*/	{0, 0, sproc_call},	{0, gd, prctl_call},	{0, gd, procblk_call},
/*132*/	{0, 0, sprocsp_call},	{0, 0, nosys_call},	{0, 0, mmap_call},
/*135*/	{0, 0, munmap_call},	{0, 0, mprotect_call},	{0, 0, msync_call},
/*138*/	{0, 0, madvise_call},	{2, gd, pagelock_call},	{0, 0,getpagesize_call},
/*141*/	{0, 0, quotactl_call},	{0, 0, nosys_call},	{0, 0, BSDgetpgrp_call},
/*144*/	{0, 0, BSDsetpgrp_call},{0, 0, vhangup_call},	{0, 0, fsync_call},
/*147*/	{0, 0, fchdir_call},	{0, 0, getrlimit_call},	{0, 0, setrlimit_call},
/*150*/	{0, 0, cacheflush_call},{0, 0, cachectl_call},	{0, 0, fchown_call},
/*153*/	{0, 0, fchmod_call},	{0, 0, wait3_call},	{0, 0, socketpair_call},
/*156*/	{0, 0, sysinfo_call},	{0, 0, nuname_call},	{0,gd, xstat_call},
/*159*/	{0,gd, lxstat_call},	{0,gd, fxstat_call},	{0, 0, xmknod_call},
/*162*/	{0, 0, sigaction_call},	{0, 0, sigpending_call},{0, 0,sigprocmask_call},
/*165*/	{0, 0, sigsuspend_call},{0, 0, sigpoll_call},	{0, 0, swapctl_call},
/*168*/	{0, 0, getcontext_call},{0, 0, setcontext_call},{0, 0, waitsys_call},
/*171*/	{0, 0, sigstack_call},	{0, 0,sigaltstack_call},{0, 0, sigsendset_call},
/*174*/ {0, 0, statvfs_call},	{0, 0, fstatvfs_call},	{0, 0, getpmsg_call},
/*177*/ {0, 0, putpmsg_call},	{0, 0, lchown_call},	{0, 0, priocntl_call},
/*180*/ {0, 0, sigqueue_call},  {0, 0, readv_call},  	{0, 0, writev_call}, 
/*183*/ {0, 0, truncate64_call},{0, 0,ftruncate64_call},{0, 0, mmap64_call},
/*186*/ {0, 0, dmi_call},	{0, kad, pread_call},	{0, kad, pwrite_call},
/*189*/ {0,0,fdatasync_call},	{0,0,sgifastpath_call},	{0,0,attr_get_call},
/*192*/ {0,0,attr_getf_call},	{0,0,attr_set_call},	{0,0,attr_setf_call},
/*195*/ {0,0,attr_remove_call},	{0,0,attr_removef_call},{0,0,attr_list_call},
/*198*/ {0,0,attr_listf_call},	{0,0,attr_multi_call},	{0,0,attr_multif_call},
/*201*/ {0, 0, statvfs64_call},	{0, 0, fstatvfs64_call},{0, 0, getmountid_call},
/*204*/ {0, 0, nsproc_call},	{0, 0, getdents64_call},{0, 0, nosys_call},
/*207*/	{0, 0, ngetdents_call},	{0,0,ngetdents64_call}, {0, 0, nosys_call},
/*210*/ {0, 0, pidsprocsp_call},{0, 0, rexec_call},
/*212*/	{0, 0, timer_create_call},
/*213*/	{0, 0, timer_delete_call},		{0,0,timer_settime_call},
/*215*/	{0, 0, timer_gettime_call},		{0,0,timer_getoverrun_call},
/*217*/	{0, 0, sched_rr_get_interval_call},	{0,0,sched_yield_call},
/*219*/	{0, 0, sched_getscheduler_call},{0, 0, sched_setscheduler_call},
/*221*/ {0, 0, sched_getparam_call},		{0,0,sched_setparam_call},
/*223*/	{0, 0, usync_cntl_call},		{0,gd,psema_cntl_call},
/*225*/ {0, 0, ckpt_restartreturn_call},	{0, 0, sysget_call},
/*227*/ {0, 0, xpg4_recvmsg_call},		{0, 0, nosys_call},
/*229*/ {0, 0, nosys_call},			{0, 0, nosys_call},
/*231*/ {0, 0, nosys_call},			{0, 0, nosys_call},
/*233*/ {0, 0, nosys_call},			{0, 0, linkfollow_call},
};
static int nsysent = sizeof(syscalltab)/sizeof(syscall_t);

/* various pretty printing tables */
static const pp_t signames[] = {
	NV(SIGHUP), NV(SIGINT), NV(SIGQUIT), NV(SIGILL),
	NV(SIGTRAP), NV(SIGABRT), NV(SIGEMT), NV(SIGFPE),
	NV(SIGKILL), NV(SIGBUS), NV(SIGSEGV), NV(SIGSYS),
	NV(SIGPIPE), NV(SIGALRM), NV(SIGTERM), NV(SIGUSR1),
	NV(SIGUSR2), NV(SIGCLD), NV(SIGPWR),
	NV(SIGWINCH), NV(SIGURG), NV(SIGPOLL), NV(SIGSTOP), NV(SIGTSTP),
	NV(SIGCONT), NV(SIGTTIN), NV(SIGTTOU), NV(SIGVTALRM), NV(SIGPROF),
	NV(SIGXCPU), NV(SIGXFSZ), NV(SIGCKPT), NV(SIGRESTART),
	NULLTAB
};
static const pp_t sigcodes[] = {
	NV(SI_USER), NV(SI_QUEUE), NV(SI_ASYNCIO), NV(SI_TIMER),
	NV(SI_MESGQ),
	NULLTAB
};

static const pp_t openarg2[] = {
	NV(O_RDONLY), NV(O_WRONLY), NV(O_RDWR),
	NULLTAB
};

static const pp_t openarg2f[] = {
	NV(O_NDELAY), NV(O_APPEND), NV(O_SYNC), NV(O_DSYNC), NV(O_RSYNC),
	NV(O_NONBLOCK), NV(O_CREAT), NV(O_TRUNC), NV(O_EXCL), NV(O_NOCTTY),
	NV(O_LARGEFILE), NV(O_DIRECT),
	NULLTAB
};

static const pp_t fcntlarg2[] = {
	NV(F_DUPFD), NV(F_GETFD), NV(F_SETFD), NV(F_GETFL), NV(F_SETFL),
	NV(F_SETLK), NV(F_SETLKW), NV(F_CHKFL), NV(F_ALLOCSP),
	NV(F_FREESP), NV(F_SETBSDLK), NV(F_SETBSDLKW), NV(F_GETLK),
	NV(F_RSETLK), NV(F_RGETLK), NV(F_RSETLKW), NV(F_GETOWN), NV(F_SETOWN),
	NV(F_DIOINFO), NV(F_FSGETXATTR), NV(F_FSSETXATTR), NV(F_GETLK64),
	NV(F_SETLK64), NV(F_SETLKW64), NV(F_ALLOCSP64), NV(F_FREESP64),
	NV(F_GETBMAP), NV(F_FSSETDM), NV(F_RESVSP), NV(F_UNRESVSP),
	NV(F_RESVSP64), NV(F_UNRESVSP64), NV(F_GETBMAPA), NV(F_FSGETXATTRA),
	NULLTAB
};

static const pp_t fcntlflags[] = {
	NV(FNDELAY), NV(FAPPEND), NV(FSYNC), NV(FNONBLOCK), NV(FASYNC),
	NV(FDIRECT),
	NULLTAB
};

static const pp_t accessarg2[] = {
	NV(F_OK), NV(R_OK), NV(W_OK), NV(X_OK), NV(EFF_ONLY_OK), NV(EX_OK),
	NULLTAB
};

static const pp_t sigprocmaskarg1[] = {
	NV(SIG_NOP), NV(SIG_BLOCK), NV(SIG_UNBLOCK), NV(SIG_SETMASK), 
	NV(SIG_SETMASK32),
	NULLTAB
};

static const pp_t sysconfarg1[] = {
	NV(_SC_ARG_MAX), NV(_SC_CHILD_MAX), NV(_SC_CLK_TCK),
	NV(_SC_NGROUPS_MAX), NV(_SC_OPEN_MAX), NV(_SC_JOB_CONTROL),
	NV(_SC_SAVED_IDS), NV(_SC_VERSION), NV(_SC_PASS_MAX),
	NV(_SC_LOGNAME_MAX), NV(_SC_PAGESIZE), NV(_SC_XOPEN_VERSION),
	NV(_SC_NACLS_MAX), NV(_SC_NPROC_CONF), NV(_SC_NPROC_ONLN),
	NV(_SC_STREAM_MAX), NV(_SC_TZNAME_MAX), NV(_SC_AIO_MAX),
	NV(_SC_AIO_LISTIO_MAX), NV(_SC_RTSIG_MAX), NV(_SC_SIGQUEUE_MAX),
	NV(_SC_ASYNCHRONOUS_IO), NV(_SC_REALTIME_SIGNALS),
	NV(_SC_PRIORITIZED_IO),
	NV(_SC_ACL), NV(_SC_AUDIT), NV(_SC_INF), NV(_SC_MAC),
	NV(_SC_CAP), NV(_SC_IP_SECOPTS), NV(_SC_KERN_POINTERS),

	NV(_SC_DELAYTIMER_MAX), NV(_SC_MQ_OPEN_MAX), NV(_SC_MQ_PRIO_MAX),
	NV(_SC_SEM_NSEMS_MAX), NV(_SC_SEM_VALUE_MAX), NV(_SC_TIMER_MAX),
	NV(_SC_FSYNC), NV(_SC_MAPPED_FILES), NV(_SC_MEMLOCK),
	NV(_SC_MEMLOCK_RANGE), NV(_SC_MEMORY_PROTECTION),
	NV(_SC_MESSAGE_PASSING), NV(_SC_PRIORITY_SCHEDULING),
	NV(_SC_SEMAPHORES), NV(_SC_SHARED_MEMORY_OBJECTS),
	NV(_SC_SYNCHRONIZED_IO), NV(_SC_TIMERS),

	NV(_SC_ASYNCHRONOUS_IO), NV(_SC_ABI_ASYNCHRONOUS_IO),
	NV(_SC_AIO_LISTIO_MAX), NV(_SC_AIO_MAX), NV(_SC_AIO_PRIO_DELTA_MAX),

	NV(_SC_XOPEN_SHM), NV(_SC_XOPEN_CRYPT), NV(_SC_BC_BASE_MAX),
	NV(_SC_BC_DIM_MAX), NV(_SC_BC_SCALE_MAX), NV(_SC_BC_STRING_MAX),
	NV(_SC_COLL_WEIGHTS_MAX), NV(_SC_EXPR_NEST_MAX), NV(_SC_LINE_MAX),
	NV(_SC_RE_DUP_MAX), NV(_SC_2_C_BIND), NV(_SC_2_C_DEV),
	NV(_SC_2_C_VERSION), NV(_SC_2_FORT_DEV), NV(_SC_2_FORT_RUN),
	NV(_SC_2_LOCALEDEF), NV(_SC_2_SW_DEV), NV(_SC_2_UPE),
	NV(_SC_2_VERSION), NV(_SC_2_CHAR_TERM), NV(_SC_XOPEN_ENH_I18N),
	NV(_SC_IOV_MAX), NV(_SC_ATEXIT_MAX), NV(_SC_XOPEN_UNIX),
	NV(_SC_XOPEN_XCU_VERSION), NV(_SC_GETGR_R_SIZE_MAX),
	NV(_SC_GETPW_R_SIZE_MAX),
	NV(_SC_LOGIN_NAME_MAX), NV(_SC_THREAD_DESTRUCTOR_ITERATIONS),
	NV(_SC_THREAD_KEYS_MAX), NV(_SC_THREAD_STACK_MIN),
	NV(_SC_THREAD_THREADS_MAX), NV(_SC_TTY_NAME_MAX),
	NV(_SC_THREADS), NV(_SC_THREAD_ATTR_STACKADDR),
	NV(_SC_THREAD_ATTR_STACKSIZE), NV(_SC_THREAD_PRIORITY_SCHEDULING),
	NV(_SC_THREAD_PRIO_INHERIT), NV(_SC_THREAD_PRIO_PROTECT),
	NV(_SC_THREAD_PROCESS_SHARED), NV(_SC_THREAD_SAFE_FUNCTIONS),
	NULLTAB
};

static const pp_t pathconfarg2[] = {
	NV(_PC_LINK_MAX), NV(_PC_MAX_CANON), NV(_PC_MAX_INPUT),
	NV(_PC_NAME_MAX), NV(_PC_PATH_MAX), NV(_PC_PIPE_BUF),
	NV(_PC_CHOWN_RESTRICTED), NV(_PC_NO_TRUNC), NV(_PC_VDISABLE),
	NV(_PC_SYNC_IO), NV(_PC_PRIO_IO),
	NV(_PC_ASYNC_IO), NV(_PC_ABI_ASYNC_IO), NV(_PC_ABI_AIO_XFER_MAX),
	NULLTAB
};

static const pp_t syssgiarg1[] = {
	NV(SGI_SYSID), NV(SGI_TUNE), NV(SGI_IDBG),
	NV(SGI_INVENT), NV(SGI_RDNAME), NV(SGI_SETLED), NV(SGI_SETNVRAM),
	NV(SGI_GETNVRAM), NV(SGI_SETKOPT),
	NV(SGI_QUERY_FTIMER), NV(SGI_QUERY_CYCLECNTR),
	NV(SGI_SETSID), NV(SGI_SETPGID),
	NV(SGI_SYSCONF), NV(SGI_PATHCONF), NV(SGI_TITIMER),
	NV(SGI_READB), NV(SGI_WRITEB), NV(SGI_SETGROUPS),
	NV(SGI_GETGROUPS), 
	NV(SGI_SETTIMEOFDAY), NV(SGI_SETTIMETRIM), NV(SGI_GETTIMETRIM),
	NV(SGI_SPROFIL), NV(SGI_RUSAGE), NV(SGI_SIGSTACK),
	NV(SGI_NETPROC), NV(SGI_SIGALTSTACK),
	NV(SGI_BDFLUSHCNT), NV(SGI_SSYNC), NV(SGI_NFSCNVT), NV(SGI_GETPGID),
	NV(SGI_GETSID), NV(SGI_IOPROBE), NV(SGI_CONFIG), NV(SGI_ELFMAP),
	NV(SGI_MCONFIG),

	NV(SGI_GETPLABEL), NV(SGI_SETPLABEL), NV(SGI_GETLABEL),
	NV(SGI_SETLABEL), NV(SGI_SATREAD), NV(SGI_SATWRITE),
	NV(SGI_SATCTL), NV(SGI_LOADATTR), NV(SGI_UNLOADATTR),
	NV(SGI_RECVLUMSG), NV(SGI_PLANGMOUNT), NV(SGI_GETPSOACL),
	NV(SGI_SETPSOACL), NV(SGI_CAP_GET), NV(SGI_CAP_SET),
	NV(SGI_PROC_ATTR_GET), NV(SGI_PROC_ATTR_SET), NV(SGI_REVOKE),
	NV(SGI_ACL_GET), NV(SGI_ACL_SET), NV(SGI_MAC_GET), NV(SGI_MAC_SET),

	NV(SGI_SBE_GET_INFO), NV(SGI_SBE_CLR_INFO), NV(SGI_GET_EVCONF),
	NV(SGI_MPCWAROFF), NV(SGI_SET_AUTOPWRON), NV(SGI_SPIPE),
	NV(SGI_SYMTAB), NV(SGI_SET_FP_PRECISE), NV(SGI_TOSSTSAVE), NV(SGI_FDHI),
	NV(SGI_SET_CONFIG_SMM), NV(SGI_SET_FP_PRESERVE), NV(SGI_MINRSS),
	NV(SGI_GRIO), NV(SGI_XLV_SET_TAB), NV(SGI_XLV_GET_TAB),
	NV(SGI_GET_FP_PRECISE), NV(SGI_GET_CONFIG_SMM),
	NV(SGI_FP_IMPRECISE_SUPP), NV(SGI_CONFIG_NSMM_SUPP),

	NV(SGI_RT_TSTAMP_CREATE), NV(SGI_RT_TSTAMP_DELETE),
	NV(SGI_RT_TSTAMP_START), NV(SGI_RT_TSTAMP_STOP),
	NV(SGI_RT_TSTAMP_ADDR), NV(SGI_RT_TSTAMP_MASK),
	NV(SGI_RT_TSTAMP_EOB_MODE),

	NV(SGI_USE_FP_BCOPY), NV(SGI_GET_UST), NV(SGI_SPECULATIVE_EXEC),
	NV(SGI_XLV_NEXT_RQST), NV(SGI_XLV_ATTR_CURSOR), NV(SGI_XLV_ATTR_GET),
	NV(SGI_XLV_ATTR_SET),
	NV(SGI_BTOOLSIZE), NV(SGI_BTOOLGET), NV(SGI_BTOOLREINIT),
	NV(SGI_CREATE_UUID), NV(SGI_NOFPE), NV(SGI_OLD_SOFTFP),
	NV(SGI_FS_INUMBERS), NV(SGI_FS_BULKSTAT),

	NV(SGI_RT_TSTAMP_WAIT), NV(SGI_RT_TSTAMP_UPDATE),

	NV(SGI_PATH_TO_HANDLE), NV(SGI_PATH_TO_FSHANDLE), NV(SGI_FD_TO_HANDLE),
	NV(SGI_OPEN_BY_HANDLE), NV(SGI_READLINK_BY_HANDLE),
	NV(SGI_ATTR_LIST_BY_HANDLE), NV(SGI_ATTR_MULTI_BY_HANDLE),
	NV(SGI_FSSETDM_BY_HANDLE),

	NV(SGI_READ_DANGID), NV(SGI_CONST),
	NV(SGI_XFS_FSOPERATIONS),

	NV(SGI_SETASH), NV(SGI_GETASH), NV(SGI_SETPRID), NV(SGI_GETPRID),
	NV(SGI_SETSPINFO), NV(SGI_GETSPINFO), NV(SGI_SHAREII),
	NV(SGI_NEWARRAYSESS), NV(SGI_GETDFLTPRID),

	NV(SGI_SET_DISMISSED_EXC_CNT), NV(SGI_GET_DISMISSED_EXC_CNT),
	NV(SGI_CYCLECNTR_SIZE), NV(SGI_QUERY_FASTTIMER), NV(SGI_PIDSINASH),
	NV(SGI_ULI),

	NV(SGI_CACHEFS_SYS), NV(SGI_NFSNOTIFY), NV(SGI_LOCKDSYS),
	NV(SGI_EVENTCTR), NV(SGI_GETPRUSAGE), NV(SGI_PROCMASK_LOCATION),
	NV(SGI_CKPT_SYS), NV(SGI_GETGRPPID),
	NV(SGI_GETSESPID), NV(SGI_ENUMASHS), NV(SGI_SETASMACHID),
	NV(SGI_GETASMACHID), NV(SGI_GETARSESS), NV(SGI_JOINARRAYSESS),
	NV(SGI_DBA_CONFIG), NV(SGI_RELEASE_NAME),
	NV(SGI_SYNCH_CACHE_HANDLER), NV(SGI_SWASH_INIT),
	NV(SGI_NUM_MODULES), NV(SGI_MODULE_INFO),
	NV(SGI_GET_CONTEXT_NAME), NV(SGI_GET_CONTEXT_INFO),
	NV(SGI_PART_OPERATIONS), NV(SGI_EARLY_ADD_SWAP),

	NV(SGI_NUMA_MIGR_PAGE), NV(SGI_NUMA_MIGR_PAGE_ALT),

	NV(SGI_KAIO_USERINIT), NV(SGI_KAIO_READ), NV(SGI_KAIO_WRITE),
	NV(SGI_KAIO_SUSPEND), NV(SGI_DBA_GETSTATS), NV(SGI_DBA_CLRSTATS),

	NV(SGI_IO_SHOW_AUX_INFO), NV(SGI_PMOCTL),
	NV(SGI_ALLOCSHARENA), NV(SGI_SETVPID), NV(SGI_GETVPID),
	NV(SGI_NUMA_TUNE),
	NV(SGI_ERROR_FORCE), 
	NV(SGI_NUMA_STATS_GET),
	NV(SGI_DPIPE_FSPE_BIND),
	NV(SGI_DYIELD), NV(SGI_TUNE_GET), NV(SGI_CHPROJ), NV(SGI_LCHPROJ),
	NV(SGI_FCHPROJ), NV(SGI_ARSESS_CTL), NV(SGI_ARSESS_OP), 
	NV(SGI_FETCHOP_SETUP), NV(SGI_FS_BULKSTAT_SINGLE),
	NV(SGI_WRITE_IP32_FLASH),
	NV(SGI_IS_DEBUG_KERNEL), NV(SGI_IS_TRAPLOG_DEBUG_KERNEL),
	NV(SGI_POKE), NV(SGI_PEEK),
	NV(SGI_XLV_INDUCE_IO_ERROR), NV(SGI_XLV_UNINDUCE_IO_ERROR),
	NV(SGI_DKSC_INDUCE_IO_ERROR), NV(SGI_DKSC_UNINDUCE_IO_ERROR),
	NV(SGI_FO_DUMP), NV(SGI_FO_SWITCH), NV(SGI_NOHANG), NV(SGI_UNFS),
	NV(SGI_NUMA_MIGR_INT_VADDR), NV(SGI_NUMA_MIGR_INT_PFN),
	NV(SGI_NUMA_PAGEMIGR_TEST), NV(SGI_NUMA_TESTS), NV(SGI_NUMA_RESERVED),
	NV(SGI_MEMPROF_START), NV(SGI_MEMPROF_GET),
	NV(SGI_MEMPROF_CLEARALL), NV(SGI_MEMPROF_STOP),
	NV(SGI_PHYSP), NV(SGI_KTHREAD), NV(SGI_DEBUGLPAGE), NV(SGI_MAPLPAGE),
	NV(SGI_CREATE_MISER_POOL), NV(SGI_CREATE_MISER_JOB),
	NV(SGI_MISER_CRITICAL), NV(SGI_CONTEXT_SWITCH), 
	NV(SGI_KMEM_TEST), NV(SGI_SHAKE_ZONES),

	NULLTAB
};

static const pp_t plockarg1[] = {
	NV(PROCLOCK), NV(TXTLOCK), NV(DATLOCK), NV(UNLOCK),
	NULLTAB
};

static const pp_t sysmipsarg1[] = {
	NV(SETNAME), NV(STIME), NV(FLUSH_CACHE), 
	NV(MIPS_FPSIGINTR), NV(MIPS_FPU), NV(MIPS_FIXADE),
	NV(POSTWAIT), NV(PPHYSIO), NV(PPHYSIO64),
	NULLTAB
};

static const pp_t shmctlarg2[] = {
	NV(IPC_RMID), NV(IPC_SET), NV(IPC_STAT), NV(SHM_LOCK),
	NV(SHM_UNLOCK),
	NULLTAB
};

static const pp_t semctlarg3[] = {
	NV(IPC_RMID), NV(IPC_SET), NV(IPC_STAT), NV(GETNCNT),
	NV(GETPID), NV(GETVAL), NV(GETALL), NV(GETZCNT), NV(SETVAL),
	NV(SETALL),
	NULLTAB
};

static const pp_t ioctlarg2[] = {
	/* rrm.h */
	NV(RRM_OPENRN), NV(RRM_CLOSERN), NV(RRM_BINDPROCTORN),
	NV(RRM_BINDRNTOCLIP), NV(RRM_UNBINDRNFROMCLIP), NV(RRM_SWAPBUF),
	NV(RRM_SETSWAPINTERVAL), NV(RRM_WAITFORRETRACE), NV(RRM_SETDISPLAYMODE),
	NV(RRM_MESSAGE), NV(RRM_INVALIDATERN), NV(RRM_VALIDATECLIP),
	NV(RRM_VALIDATESWAPBUF), NV(RRM_SWAPGROUP), NV(RRM_SWAPUNGROUP),
	NV(RRM_VALIDATEMESSAGE), NV(RRM_GETDISPLAYMODES), NV(RRM_LOADDISPLAYMODE),
	NV(RRM_CUSHIONBUFFER), NV(RRM_SWAPREADY), NV(RRM_MGR_SWAPBUF),
	NV(RRM_SETVSYNC), NV(RRM_GETVSYNC), NV(RRM_WAITVSYNC),
	NV(RRM_BINDRNTOREADANDCLIP), NV(RRM_MAPCLIPTOSWPBUFID),
	/* ioctl.h */
	NV(FIONREAD), NV(FIONBIO), NV(FIOASYNC), NV(FIOSETOWN),
	NV(FIOGETOWN),
	/* termios.h */
	NV(__NEW_TCGETA), NV(__NEW_TCSETA), NV(__NEW_TCSETAW), NV(__NEW_TCSETAF),
	NV(__OLD_TCGETA), NV(__OLD_TCSETA), NV(__OLD_TCSETAW), NV(__OLD_TCSETAF),
	NV(TCSBRK), NV(TCXONC), NV(TCFLSH),
	NV(TIOCFLUSH), NV(TCGETS), NV(TCSETS), NV(TCSETSW), NV(TCSETSF),
	NV(TCSETLABEL), NV(TCDSET), NV(TCBLKMD),
	NV(TIOCPKT), NV(TIOCNOTTY), NV(TIOCSTI), NV(TIOCSPGRP), NV(TIOCGPGRP),
	NV(TIOCCONS), NV(TIOCGWINSZ), NV(TIOCSWINSZ),
	NV(TCSANOW), NV(TCSADRAIN), NV(TCSAFLUSH),
	NV(TIOCGSID), NV(TIOCSSID),
	NV(TIOCMSET), NV(TIOCMBIS), NV(TIOCMBIC), NV(TIOCMGET),
	NV(ISPTM), NV(UNLKPT), NV(SVR4SOPEN),
	/* stermio.h */
	NV(STGET), NV(STSET), NV(STTHROW), NV(STWLINE), NV(STTSV),
	/* stropts.h */
	NV(I_NREAD), NV(I_PUSH), NV(I_POP), NV(I_LOOK), NV(I_FLUSH),
	NV(I_SRDOPT), NV(I_GRDOPT), NV(I_STR), NV(I_SETSIG), NV(I_GETSIG),
	NV(I_FIND), NV(I_LINK), NV(I_UNLINK), NV(I_PEEK), NV(I_FDINSERT),
	NV(I_SENDFD), NV(I_RECVFD),
#ifdef LATER
	NV(I_E_RECVFD),
#endif
	NV(I_SWROPT),
	NV(I_GWROPT), NV(I_LIST), NV(I_PLINK), NV(I_PUNLINK),
	NV(I_FLUSHBAND), NV(I_CKBAND), NV(I_ATMARK), NV(I_SETCLTIME),
	NV(I_GETCLTIME), NV(I_CANPUT), NV(I_S_RECVFD),
	/* gfx.h */
	NV(GFX_GETNUM_BOARDS), NV(GFX_GETBOARDINFO), NV(GFX_ATTACH_BOARD),
	NV(GFX_DETACH_BOARD), NV(GFX_IS_MANAGED), NV(GFX_INITIALIZE),
	NV(GFX_START), NV(GFX_DOWNLOAD), NV(GFX_BLANKSCREEN),
	NV(GFX_MAPALL), NV(GFX_LABEL),
	/* soioctl.h */
	NV(SIOCSHIWAT), NV(SIOCGHIWAT), NV(SIOCSLOWAT), NV(SIOCGLOWAT),
	NV(SIOCATMARK), NV(SIOCSPGRP), NV(SIOCGPGRP), NV(SIOCNREAD),
	NV(SIOCSIFADDR), NV(SIOCGIFADDR), NV(SIOCLIFADDR),
	NV(SIOCSIFDSTADDR), NV(SIOCGIFDSTADDR), NV(OSIOCSIFFLAGS),
	NV(OSIOCGIFFLAGS), NV(SIOCGIFBRDADDR), NV(SIOCSIFBRDADDR),
	NV(SIOCGIFCONF), NV(SIOCGIFNETMASK), NV(SIOCSIFNETMASK),
	NV(SIOCGIFMETRIC), NV(SIOCSIFMETRIC), NV(SIOCSARP),
	NV(SIOCGARP), NV(SIOCDARP), NV(SIOCADDMULTI),
	NV(SIOCDIFADDR), NV(SIOCAIFADDR), NV(SIOCGIFRECV), NV(SIOCGIFSEND),
	NV(SIOCSIFSEND), NV(SIOCSIFRECV),
	NV(SIOCGIFFLAGS), NV(SIOCSIFFLAGS), NV(SIOCGIFDATA),
	NV(SIOCDELMULTI), NV(SIOCSIFSTATS), NV(SIOCGIFSTATS), NV(SIOCSIFHEAD),
	/* idev.h */
	NV(IDEVGETDEVICEDESC), NV(IDEVGETVALUATORDESC), NV(IDEVGETKEYMAPDESC),
	NV(IDEVGETSTRDPYDESC), NV(IDEVGETINTDPYDESC), NV(IDEVGETBUTTONS),
	NV(IDEVGETVALUATORS), NV(IDEVGETLEDS), NV(IDEVGETSTRDPY),
	NV(IDEVGETINTDPYS), NV(IDEVENABLEBUTTONS), NV(IDEVENABLEVALUATORS),
	NV(IDEVSETVALUATORS), NV(IDEVCHANGEVALUATORS), NV(IDEVSETVALUATORDESC),
	NV(IDEVSETLEDS), NV(IDEVSETSTRDPY), NV(IDEVSETINTDPYS),
	NV(IDEVRINGBELL), NV(IDEVKEYBDCONTROL), NV(IDEVPTRCONTROL),
	NV(IDEVOTHERCONTROL), NV(IDEVSETPTRMODE), NV(IDEVOTHERQUERY),
	NV(IDEVINITDEVICE),
	/* imon.h */
	NV(IMONIOC_EXPRESS), NV(IMONIOC_REVOKE), NV(IMONIOC_QTEST),
	/* procfs.h */
	NV(PIOCSTATUS), NV(PIOCSTOP), NV(PIOCWSTOP), NV(PIOCRUN),
	NV(PIOCGTRACE), NV(PIOCSTRACE), NV(PIOCSSIG), NV(PIOCKILL),
	NV(PIOCUNKILL), NV(PIOCGHOLD), NV(PIOCSHOLD), NV(PIOCMAXSIG),
	NV(PIOCACTION), NV(PIOCGFAULT), NV(PIOCSFAULT), NV(PIOCCFAULT),
	NV(PIOCGENTRY), NV(PIOCSENTRY), NV(PIOCGEXIT), NV(PIOCSEXIT),
	NV(PIOCSFORK), NV(PIOCRFORK), NV(PIOCSRLC), NV(PIOCRRLC),
	NV(PIOCGREG), NV(PIOCSREG), NV(PIOCGFPREG), NV(PIOCSFPREG),
	NV(PIOCNICE), NV(PIOCPSINFO), NV(PIOCNMAP), NV(PIOCMAP),
	NV(PIOCOPENM), NV(PIOCCRED), NV(PIOCGROUPS),
	NV(PIOCSET), NV(PIOCRESET), NV(PIOCNWATCH),
	NV(PIOCGWATCH), NV(PIOCSWATCH), NV(PIOCUSAGE),
	NV(PIOCPGD_SGI), NV(PIOCMAP_SGI),
	NV(PIOCGETPTIMER),
	NV(IRIX5_64_PIOCSTATUS), NV(IRIX5_64_PIOCSTOP),
	NV(IRIX5_64_PIOCWSTOP), NV(IRIX5_64_PIOCRUN),
	NV(IRIX5_64_PIOCGTRACE), NV(IRIX5_64_PIOCSTRACE),
	NV(IRIX5_64_PIOCSSIG), NV(IRIX5_64_PIOCKILL),
	NV(IRIX5_64_PIOCUNKILL), NV(IRIX5_64_PIOCGHOLD),
	NV(IRIX5_64_PIOCSHOLD), NV(IRIX5_64_PIOCMAXSIG),
	NV(IRIX5_64_PIOCACTION), NV(IRIX5_64_PIOCGFAULT),
	NV(IRIX5_64_PIOCSFAULT), NV(IRIX5_64_PIOCCFAULT),
	NV(IRIX5_64_PIOCGENTRY), NV(IRIX5_64_PIOCSENTRY),
	NV(IRIX5_64_PIOCGEXIT), NV(IRIX5_64_PIOCSEXIT),
	NV(IRIX5_64_PIOCSFORK), NV(IRIX5_64_PIOCRFORK),
	NV(IRIX5_64_PIOCSRLC), NV(IRIX5_64_PIOCRRLC),
	NV(IRIX5_64_PIOCGREG), NV(IRIX5_64_PIOCSREG),
	NV(IRIX5_64_PIOCGFPREG), NV(IRIX5_64_PIOCSFPREG),
	NV(IRIX5_64_PIOCNICE), NV(IRIX5_64_PIOCPSINFO),
	NV(IRIX5_64_PIOCNMAP), NV(IRIX5_64_PIOCMAP),
	NV(IRIX5_64_PIOCOPENM), NV(IRIX5_64_PIOCCRED),
	NV(IRIX5_64_PIOCGROUPS), NV(IRIX5_64_PIOCSET),
	NV(IRIX5_64_PIOCRESET), NV(IRIX5_64_PIOCNWATCH),
	NV(IRIX5_64_PIOCGWATCH), NV(IRIX5_64_PIOCSWATCH),
	NV(IRIX5_64_PIOCUSAGE), NV(IRIX5_64_PIOCPGD_SGI),
	NV(IRIX5_64_PIOCMAP_SGI), NV(IRIX5_64_PIOCGETPTIMER),
	/* ckpt_procfs.h */
	NV(PIOCCKPTSHM), NV(PIOCCKPTSTOP), NV(PIOCCKPTUTINFO),
	NV(PIOCCKPTSIGTHRD), NV(PIOCCKPTSCHED), NV(PIOCCKPTFSWAP),
	NV(PIOCCKPTFSTAT), NV(PIOCCKPTOPEN), NV(PIOCCKPTGETMAP),
	NV(PIOCCKPTGETCTX), NV(PIOCCKPTSETCTX), NV(PIOCCKPTPSINFO),
	NV(PIOCCKPTSETPI), NV(PIOCCKPTGETSI),
	NV(PIOCCKPTQSIG), NV(PIOCCKPTSSIG), NV(PIOCCKPTABORT),
	NV(PIOCCKPTSTOP), NV(PIOCCKPTDUP),
	NV(PIOCCKPTOPENCWD), NV(PIOCCKPTOPENRT),
	NV(PIOCCKPTCHROOT), NV(PIOCCKPTMSTAT), NV(PIOCCKPTFORKMEM),
	NV(PIOCCKPTUSAGE),
	/* usioctl.h */
	NV(UIOCATTACHSEMA), NV(UIOCBLOCK), NV(UIOCABLOCK),
	NV(UIOCNOIBLOCK), NV(UIOCUNBLOCK), NV(UIOCAUNBLOCK), NV(UIOCINIT),

	NULLTAB
};

static const pp_t schedctlarg1[] = {
	NV(NDPRI), NV(SLICE), NV(RENICE),
	NV(RENICE_PROC), NV(RENICE_PGRP), NV(RENICE_USER),
	NV(GETNICE_PROC), NV(GETNICE_PGRP), NV(GETNICE_USER),
	NV(SETHINTS), NV(SCHEDMODE), NV(SETMASTER),
	NULLTAB
};

static const pp_t socketarg1[] = {
	NV(PF_INET), NV(PF_RAW), NV(PF_UNIX),
	NULLTAB
};

static const pp_t socketarg2[] = {
	NV(SOCK_STREAM), NV(SOCK_DGRAM), NV(SOCK_RAW),
	NULLTAB
};

static const pp_t itimerarg1[] = {
	NV(ITIMER_REAL), NV(ITIMER_VIRTUAL), NV(ITIMER_PROF),
	NULLTAB
};

static const pp_t mmaparg3[] = {
	NV(PROT_EXECUTE), NV(PROT_WRITE), NV(PROT_READ), NV(PROT_NONE),
	NULLTAB
};

static const pp_t mmaparg4[] = {
	NV(MAP_SHARED), NV(MAP_PRIVATE), NV(MAP_FIXED), NV(MAP_RENAME),
	NV(MAP_AUTOGROW), NV(MAP_LOCAL), NV(MAP_AUTORESRV),
	NULLTAB
};

static const pp_t sprocarg2[] = {
	NV(PR_SPROC), NV(PR_SFDS), NV(PR_SDIR), NV(PR_SUMASK),
	NV(PR_SULIMIT), NV(PR_SID), NV(PR_SADDR), NV(PR_BLOCK),
	NV(PR_NOLIBC),
	NULLTAB
};

static const pp_t msyncarg3[] = {
	NV(MS_ASYNC), NV(MS_INVALIDATE), NV(MS_SYNC),
	NULLTAB
};

static const pp_t madvisearg3[] = {
	NV(MADV_NORMAL), NV(MADV_RANDOM), NV(MADV_SEQUENTIAL),
	NV(MADV_WILLNEED), NV(MADV_DONTNEED),
	NULLTAB
};

static const pp_t quotaarg1[] = {
	NV(Q_QUOTAON), NV(Q_QUOTAOFF), NV(Q_SETQUOTA),
	NV(Q_SETQLIM), NV(Q_GETQUOTA), NV(Q_SYNC), NV(Q_ACTIVATE),
	NULLTAB
};

static const pp_t rlimitarg1[] = {
	NV(RLIMIT_CPU), NV(RLIMIT_FSIZE), NV(RLIMIT_DATA),
	NV(RLIMIT_STACK), NV(RLIMIT_CORE), NV(RLIMIT_RSS),
	NV(RLIMIT_NOFILE), NV(RLIMIT_VMEM),
	NULLTAB
};

static const pp_t cacheflusharg3[] = {
	NV(ICACHE), NV(DCACHE), NV(BCACHE),
	NULLTAB
};

static const pp_t cachectlarg3[] = {
	NV(CACHEABLE), NV(UNCACHEABLE),
	NULLTAB
};

static const pp_t waitopt[] = {
	NV(WNOHANG), NV(WSTOPPED), NV(WTRAPPED), NV(WEXITED),
	NV(WCONTINUED), NV(WNOWAIT),
	NULLTAB
};

static const pp_t waittype[] = {
	NV(P_PID), NV(P_ALL), NV(P_PGID), NV(P_PPID),
	NV(P_SID), NV(P_CID), NV(P_UID), NV(P_GID),
	NULLTAB
};

static const pp_t swapctlarg1[] = {
	NV(SC_ADD), NV(SC_LIST), NV(SC_REMOVE), NV(SC_GETNSWP),
	NV(SC_SGIADD), NV(SC_KSGIADD), NV(SC_LREMOVE),
	NV(SC_GETFREESWAP), NV(SC_GETSWAPMAX), NV(SC_GETSWAPVIRT),
	NV(SC_GETRESVSWAP), NV(SC_GETSWAPTOT), NV(SC_GETLSWAPTOT),
	NULLTAB
};

static const pp_t flock1[] = {
	NV(F_RDLCK), NV(F_WRLCK), NV(F_UNLCK),
	NULLTAB
};

static const pp_t flock2[] = {
	NV(SEEK_SET), NV(SEEK_CUR), NV(SEEK_END),
	NULLTAB
};

static const pp_t sigactflags[] = {
	NV(SA_ONSTACK), NV(SA_RESETHAND), NV(SA_RESTART),
	NV(SA_SIGINFO), NV(SA_NODEFER), NV(SA_NOCLDWAIT),
	NV(SA_NOCLDSTOP), NV(_SA_BSDCALL),
	NULLTAB
};

static const pp_t sicodeCLD[] = {
	NV(CLD_EXITED), NV(CLD_KILLED), NV(CLD_DUMPED), NV(CLD_TRAPPED),
	NV(CLD_STOPPED), NV(CLD_CONTINUED),
	NULLTAB
};

static const pp_t psetarg1[] = {
	NV(MPPS_CREATE), NV(MPPS_DELETE), NV(MPPS_ADD), NV(MPPS_REMOVE),
	NULLTAB
};

static const pp_t sendflags[] = {
	NV(MSG_OOB), NV(MSG_PEEK), NV(MSG_EOR),
	NULLTAB
};

static const pp_t sockopt[] = {
	NV(SO_DEBUG), NV(SO_ACCEPTCONN), NV(SO_REUSEADDR), NV(SO_KEEPALIVE),
	NV(SO_DONTROUTE), NV(SO_BROADCAST), NV(SO_USELOOPBACK), NV(SO_LINGER),
	NV(SO_OOBINLINE), NV(SO_REUSEPORT), NV(SO_ORDREL), NV(SO_IMASOCKET),
	NV(SO_CHAMELEON), NV(SO_SNDBUF), NV(SO_RCVBUF), NV(SO_SNDLOWAT),
	NV(SO_RCVLOWAT), NV(SO_SNDTIMEO), NV(SO_RCVTIMEO), NV(SO_ERROR),
	NV(SO_TYPE), NV(SO_PROTOTYPE),

	NV(IP_OPTIONS), NV(IP_HDRINCL), NV(IP_TOS), NV(IP_TTL), NV(IP_RECVOPTS),
	NV(IP_RECVRETOPTS), NV(IP_RECVDSTADDR), NV(IP_RETOPTS),
	NV(IP_MULTICAST_IF), NV(IP_MULTICAST_TTL), NV(IP_MULTICAST_LOOP),
	NV(IP_ADD_MEMBERSHIP), NV(IP_DROP_MEMBERSHIP),
	NV(TCP_NODELAY), NV(TCP_MAXSEG),
	NULLTAB
};

static const pp_t proto[] = {
	NV(SOL_SOCKET),
	NV(IPPROTO_IP), NV(IPPROTO_ICMP), NV(IPPROTO_IGMP),
	NV(IPPROTO_GGP), NV(IPPROTO_TCP), NV(IPPROTO_EGP),
	NV(IPPROTO_PUP), NV(IPPROTO_UDP), NV(IPPROTO_IDP),
	NV(IPPROTO_TP), NV(IPPROTO_XTP), NV(IPPROTO_HELLO),
	NV(IPPROTO_ND), NV(IPPROTO_EON), NV(IPPROTO_RAW),
	NV(IPPROTO_MAX),
	NULLTAB
};

static const pp_t prctlarg1[] = {
	NV(PR_MAXPROCS), NV(PR_ISBLOCKED), NV(PR_SETSTACKSIZE),
	NV(PR_GETSTACKSIZE), NV(PR_MAXPPROCS), NV(PR_UNBLKONEXEC),
	NV(PR_SETEXITSIG), NV(PR_RESIDENT),
	NV(PR_ATTACHADDR), NV(PR_TERMCHILD), NV(PR_GETSHMASK),
	NV(PR_GETNSHARE), NV(PR_COREPID), NV(PR_ATTACHADDRPERM),
	NV(PR_LASTSHEXIT),
	NULLTAB
};

static const pp_t sysmparg1[] = {
	NV(MP_NPROCS), NV(MP_NAPROCS), NV(MP_KERNADDR), NV(MP_SASZ),
	NV(MP_SAGET), NV(MP_SCHED), NV(MP_PGSIZE), NV(MP_SAGET1),
	NV(MP_EMPOWER), NV(MP_RESTRICT), NV(MP_CLOCK), NV(MP_MUSTRUN),
	NV(MP_RUNANYWHERE), NV(MP_STAT), NV(MP_ISOLATE), NV(MP_UNISOLATE),
	NV(MP_PREEMPTIVE), NV(MP_NONPREEMPTIVE), NV(MP_FASTCLOCK),
	NV(MP_CLEARNFSSTAT), NV(MP_GETMUSTRUN),
	NV(MP_MUSTRUN_PID), NV(MP_RUNANYWHERE_PID), NV(MP_GETMUSTRUN_PID),
	NV(MP_CLEARCFSSTAT),
	NULLTAB
};

static const pp_t seekarg3[] = {
	NV(SEEK_SET), NV(SEEK_CUR), NV(SEEK_END),
	NULLTAB
};

static const pp_t pollevents[] = {
	NV(POLLIN), NV(POLLPRI), NV(POLLOUT), NV(POLLRDNORM), NV(POLLWRNORM),
	NV(POLLRDBAND), NV(POLLWRBAND), NV(POLLNORM), NV(POLLERR),
	NV(POLLHUP), NV(POLLNVAL),
	NULLTAB
};

static const pp_t sysinfoarg1[] = {
	NV(SI_SYSNAME), NV(SI_HOSTNAME), NV(SI_RELEASE),
	NV(SI_VERSION), NV(SI_MACHINE), NV(SI_ARCHITECTURE),
	NV(SI_HW_SERIAL), NV(SI_HW_PROVIDER), NV(SI_SRPC_DOMAIN),
	NV(SI_INITTAB_NAME), NV(_MIPS_SI_VENDOR), NV(_MIPS_SI_OS_PROVIDER),
	NV(_MIPS_SI_OS_NAME), NV(_MIPS_SI_HW_NAME), NV(_MIPS_SI_NUM_PROCESSORS),
	NV(_MIPS_SI_HOSTID), NV(_MIPS_SI_OSREL_MAJ), NV(_MIPS_SI_OSREL_MIN),
	NV(_MIPS_SI_OSREL_PATCH), NV(_MIPS_SI_PROCESSORS),
	NV(_MIPS_SI_AVAIL_PROCESSORS), NV(SI_SET_HOSTNAME),
	NV(SI_SET_SRPC_DOMAIN),
	NULLTAB
};

static const pp_t attr_flags[] = {
	NV(ATTR_DONTFOLLOW), NV(ATTR_ROOT), NV(ATTR_CREATE), NV(ATTR_REPLACE),
	NULLTAB
};

static const pp_t usync_cntlarg1[] = {
	NV(USYNC_BLOCK), NV(USYNC_INTR_BLOCK), NV(USYNC_UNBLOCK_ALL),
	NV(USYNC_UNBLOCK), NV(USYNC_NOTIFY_REGISTER), NV(USYNC_NOTIFY),
	NV(USYNC_NOTIFY_DELETE), NV(USYNC_NOTIFY_CLEAR), NV(USYNC_GET_STATE),
	NULLTAB
};

/*
 * first argument for clock_gettime, clock_settime, clock_getres and
 * timer_create is of type clock_id
 */
static const pp_t clockid_arg[] = {
	NV(CLOCK_REALTIME), NV(CLOCK_SGI_CYCLE), NV(CLOCK_SGI_FAST),
	NULLTAB
};

static const pp_t sched_policy[] = {
	NV(SCHED_FIFO), NV(SCHED_RR), NV(SCHED_OTHER),
	NULLTAB
};

static const pp_t ckptsysarg1[] = {
	NV(CKPT_OPENDIR), NV(CKPT_EXECMAP), NV(CKPT_SETBRK),
	NV(CKPT_PIDFORK), NV(CKPT_FPADDR), NV(CKPT_SVR3PIPE),
	NV(CKPT_GETCPIDS), NV(CKPT_UNMAPPRDA), NV(CKPT_GETSGPIDS),
	NV(CKPT_FSETTIMES), NV(CKPT_SETTIMES), NV(CKPT_SETEXEC),
	NULLTAB
};
static const pp_t addr_family[] = {
	NV(AF_UNSPEC), NV(AF_UNIX), NV(AF_INET), NV(AF_IMPLINK),
	NV(AF_PUP), NV(AF_CHAOS), NV(AF_NS), NV(AF_ISO),
	NV(AF_ECMA), NV(AF_DATAKIT), NV(AF_CCITT), NV(AF_SNA),
	NV(AF_DECnet), NV(AF_DLI), NV(AF_LAT), NV(AF_HYLINK),
	NV(AF_APPLETALK), NV(AF_ROUTE), NV(AF_RAW), NV(AF_OSI),
	NV(AF_X25), NV(AF_OSINET), NV(AF_GOSIP), NV(AF_SDL),
	NV(AF_INET6), NV(AF_LINK),
	NULLTAB
};

/* values for ptab_s flags */
#define ISMASK		0x01
#define PRINTDECIMAL	0x02

/* table of tables for 'T' format */
static const ptab_t pptable[] = {
    {"accessarg2",	accessarg2,	ISMASK,	ALLVAL},
    {"addr_family",	addr_family,	PRINTDECIMAL, ALLVAL},
    {"attr_flags",	attr_flags,	ISMASK,	ALLVAL},
    {"cachectlarg3",	cachectlarg3,	0,	ALLVAL},
    {"cacheflusharg3",	cacheflusharg3,	0,	ALLVAL},
    {"ckptsysarg1",	ckptsysarg1,	0,	ALLVAL},
    {"clockid_arg",	clockid_arg,	0,	ALLVAL},
    {"fcntlarg2",	fcntlarg2,	0,	ALLVAL},
    {"fcntlflags",	fcntlflags,	ISMASK,	ALLVAL},
    {"flock1",		flock1,		0,	ALLVAL},
    {"flock2",		flock2,		0,	ALLVAL},
    {"ioctlarg2",	ioctlarg2,	0,	ALLVAL},
    {"itimerarg1",	itimerarg1,	0,	ALLVAL},
    {"madvisearg3",	madvisearg3,	0,	ALLVAL},
    {"mmaparg3",	mmaparg3,	ISMASK,	ALLVAL},
    {"mmaparg4",	mmaparg4,	ISMASK,	ALLVAL},
    {"msyncarg3",	msyncarg3,	ISMASK,	ALLVAL},
    {"openarg2",	openarg2,	0,	O_ACCMODE},
    {"openarg2f",	openarg2f,	ISMASK,	~O_ACCMODE},
    {"pathconfarg2",	pathconfarg2,	0,	ALLVAL},
    {"plockarg1",	plockarg1,	0,	ALLVAL},
    {"pollevents",	pollevents,	1,	ALLVAL},
    {"prctlarg1",	prctlarg1,	0,	ALLVAL},
    {"proto",		proto,		0,	ALLVAL},
    {"psetarg1",	psetarg1,	0,	ALLVAL},
    {"quotaarg1",	quotaarg1,	0,	ALLVAL},
    {"rlimitarg1",	rlimitarg1,	0,	ALLVAL},
    {"sched_policy",	sched_policy,	0,	ALLVAL},
    {"schedctlarg1",	schedctlarg1,	0,	ALLVAL},
    {"seekarg3",	seekarg3,	0,	ALLVAL},
    {"semctlarg3",	semctlarg3,	0,	ALLVAL},
    {"sendflags",	sendflags,	ISMASK,	ALLVAL},
    {"shmctlarg2",	shmctlarg2,	0,	ALLVAL},
    {"sicodeCLD",	sicodeCLD,	0,	ALLVAL},
    {"sigactflags",	sigactflags,	ISMASK,	ALLVAL},
    {"sigcodes",	sigcodes,	PRINTDECIMAL,	ALLVAL},
    {"signames",	signames,	PRINTDECIMAL,	SIGNO_MASK},
    {"sigprocmaskarg1",	sigprocmaskarg1,0,	ALLVAL},
    {"socketarg1",	socketarg1,	0,	ALLVAL},
    {"socketarg2",	socketarg2,	0,	ALLVAL},
    {"sockopt",		sockopt,	0,	ALLVAL},
    {"sprocarg2",	sprocarg2,	ISMASK,	ALLVAL},
    {"swapctlarg1",	swapctlarg1,	0,	ALLVAL},
    {"sysconfarg1",	sysconfarg1,	0,	ALLVAL},
    {"sysinfoarg1",	sysinfoarg1,	0,	ALLVAL},
    {"sysmipsarg1",	sysmipsarg1,	0,	ALLVAL},
    {"sysmparg1",	sysmparg1,	0,	ALLVAL},
    {"syssgiarg1",	syssgiarg1,	0,	ALLVAL},
    {"usync_cntlarg1",	usync_cntlarg1,	0,	ALLVAL},
    {"waitopt",		waitopt,	ISMASK,	ALLVAL},
    {"waittype",	waittype,	0,	ALLVAL},
    { 0 }
};

#define	DECL_INDIRECT(s) \
static void s##_indirect(rtmonPrintState*, const indirect_t*, const struct cl*, char*, int)
DECL_INDIRECT(string);
DECL_INDIRECT(int);
DECL_INDIRECT(flock);
DECL_INDIRECT(flock64);
DECL_INDIRECT(dioinfo);
DECL_INDIRECT(xattr);
DECL_INDIRECT(fssetdm);
DECL_INDIRECT(getbmap);
DECL_INDIRECT(sigset);
DECL_INDIRECT(sigact);
DECL_INDIRECT(fd_set);
DECL_INDIRECT(waitsys);
DECL_INDIRECT(ascii);
DECL_INDIRECT(sockadd);
DECL_INDIRECT(timeval);
DECL_INDIRECT(timespec);
DECL_INDIRECT(uio);
DECL_INDIRECT(pollfd);
DECL_INDIRECT(siginfo);
DECL_INDIRECT(stat);
DECL_INDIRECT(stat64);
DECL_INDIRECT(statfs);
DECL_INDIRECT(statvfs);
DECL_INDIRECT(statvfs64);
DECL_INDIRECT(rusage);
DECL_INDIRECT(winsize);
DECL_INDIRECT(ntermio);
DECL_INDIRECT(otermio);
#undef DECL_INDIRECT

/* table of indirect arguments printing functions */
static const iptab_t iptable[] = {
    {"ascii",		ascii_indirect},
    {"dioinfo",		dioinfo_indirect},
    {"fd_set",		fd_set_indirect},
    {"flock",		flock_indirect},
    {"flock64",		flock64_indirect},
    {"fssetdm",		fssetdm_indirect},
    {"getbmap",		getbmap_indirect},
    {"int",		int_indirect},
    {"ntermio",		ntermio_indirect},
    {"otermio",		otermio_indirect},
    {"pollfd",		pollfd_indirect},
    {"rusage",		rusage_indirect },
    {"s",		string_indirect},
    {"sigact",		sigact_indirect},
    {"siginfo",		siginfo_indirect},
    {"sigset",		sigset_indirect},
    {"sockadd",		sockadd_indirect},
    {"stat",		stat_indirect },
    {"stat64",		stat64_indirect },
    {"statfs",		statfs_indirect },
    {"statvfs",		statvfs_indirect },
    {"statvfs64",	statvfs64_indirect },
    {"timespec",	timespec_indirect},
    {"timeval",		timeval_indirect},
    {"uio",		uio_indirect},
    {"waitsys",		waitsys_indirect},
    {"winsize",		winsize_indirect},
    {"xattr",		xattr_indirect},
    { 0 }
};

/*
 * appending sprintf.  This routine assumes the
 * output buffer is at least RTMON_LINESIZE bytes
 * and truncates output past that point.
 */
static int
asprintf(char* line, const char* fmt, ...)
{
    char* op = strchr(line,'\0');
    va_list ap;
    va_start(ap, fmt);
    return vsnprintf(op, RTMON_LINESIZE - (op-line), fmt, ap);
}

/*
 * Free all the memory associated with given call record.
 */
static void
freecall(void* arg)
{
    struct cl* crecp = (struct cl*) arg;
    int i;
    for (i = 0; i < crecp->numi; i++)
	free(crecp->iparams[i].data);
    if (crecp->endrec)
	freecall(crecp->endrec);
    free(crecp);
}
#define	findcall(rs,kid)	((struct cl*) _rtmon_kid_getdata(&rs->ds,kid))
#define	putsyscall(rs,cp)	_rtmon_kid_setdata(&rs->ds,cp->kid,cp,freecall)
#define	rmcall(rs, kid)		_rtmon_kid_clrdata(&rs->ds,kid) 
#define	iscalltraced(rs, cl) \
    (((rs)->eventmask & RTMON_SYSCALL) && cl->sys->trace)

/*
 * if intermixed kids, then dump out any current syscall records
 */
void
_rtmon_checkkid(rtmonPrintState* rs, FILE* fd, int64_t kid, int force)
{
    if (rs->lastsyscall == -1) {
	rs->lastsyscall = kid;
    } else if (kid != rs->lastsyscall || force) {
	struct cl* callrec = findcall(rs, rs->lastsyscall);
	if (callrec != NULL) {
	    assert(rs->lastsyscall == callrec->kid);
	    if (iscalltraced(rs, callrec))
		printcall(rs, fd, callrec, 0);
	} else {
	}
	rs->lastsyscall = kid;
    }
}

void
_rtmon_syscallBegin(rtmonPrintState* rs)
{
    rs->max_asciilen = 30;	/* max chars in an ascii string */
    rs->max_binlen = 16;	/* max char of binary data to print */
    rs->lastsyscall = (int64_t) -1;
    rs->syscalldelta = 2*1000;	/* 2 ms */
    rs->flags |= RTPRINT_INITSYS;
}

void
_rtmon_syscallEnd(rtmonPrintState* rs)
{
    (void) rs;
}

static void
_rtmon_initsyscalltrace(rtmonPrintState* rs)
{
    int call;

    rs->flags &= ~RTPRINT_INITSYS;
    for (call = 0; call < nsysent; call++) {
	subsyscall_t* sp;
	for (sp = syscalltab[call].subs; sp->name; sp++)
	    sp->trace = 1;
    }
}

#define	iskidtraced(rs,kid) \
    (((rs)->flags&RTPRINT_ALLPROCS) != 0 || _rtmon_kid_istraced(&(rs)->ds, (int64_t) kid))

void
_rtmon_syscallEventBegin(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev)
{
    const tstamp_event_syscall_data_t* sdp =
	(const tstamp_event_syscall_data_t*) &ev->qual[0];
    struct cl* startclp;
    struct cl* endclp;
    if (!iskidtraced(rs, sdp->k_id))
	return;
    _rtmon_checkkid(rs, fd, (int64_t) sdp->k_id, 0);
    if (sdp->callno >= nsysent) {
	_rtmon_printEvent(rs, fd, ev, "Unknown syscall %d\n", sdp->callno);
	return;
    }
    if (rs->flags & RTPRINT_INITSYS)
	_rtmon_initsyscalltrace(rs);
    callrectocl(rs, ev, &startclp);
    startclp->sys->num++;
    /*
     * If there's already an outstanding call
     * record from this kid, print it out.
     */
    endclp = findcall(rs, (int64_t) sdp->k_id);
    if (endclp != NULL) {
	if (iscalltraced(rs, endclp))
	    printcall(rs, fd, endclp, 0);
	rmcall(rs, endclp->kid);
    }
    putsyscall(rs, startclp);
}

void
_rtmon_syscallEventEnd(rtmonPrintState* rs, FILE* fd, const tstamp_event_entry_t* ev)
{
    const tstamp_event_syscall_data_t* sdp =
	(const tstamp_event_syscall_data_t*) &ev->qual[0];
    struct cl* startclp;
    struct cl* endclp;
    if (!iskidtraced(rs, sdp->k_id))
	return;
    _rtmon_checkkid(rs, fd, (int64_t) sdp->k_id, 0);
    if (sdp->callno >= nsysent) {
	_rtmon_printEvent(rs, fd, ev, "Unknown syscall %d\n", sdp->callno);
	return;
    }
    if (rs->flags & RTPRINT_INITSYS)
	_rtmon_initsyscalltrace(rs);
    callrectocl(rs, ev, &endclp);
    startclp = findcall(rs, (int64_t) sdp->k_id);
    if (startclp && endclp->callno == startclp->callno) {
	/* tie start & end together */
	startclp->endrec = endclp;
	startclp->sys->time += endclp->tstamp - startclp->tstamp;
	if (iscalltraced(rs, startclp)) {
	    /*
	     * If the syscall has in-out parameters, print
	     * each record separately if there was no error.
	     */
	    if (startclp->sys->inout && endclp->params[1] == 0)
		printcall(rs, fd, startclp, 0);
	    /*
	     * Otherwise, if the duration exceeds a specified
	     * threshold print each record separately so we
	     * show the amount of time spent doing the call.
	     */
	    else if (rtmon_tous(rs, endclp->tstamp - startclp->tstamp) > rs->syscalldelta)
		    printcall(rs, fd, startclp, 0);
	    printcall(rs, fd, startclp, 1);
	}
	rmcall(rs, startclp->kid);
	return;
    } else if (startclp) {
	/*
	 * different call so dump end record
	 * seperately
	 */
	if (iscalltraced(rs, startclp))
	    printcall(rs, fd, startclp, 0);
	rmcall(rs, startclp->kid);
    }
    /* an orphan end call */
    if (iscalltraced(rs, endclp))
	printcall(rs, fd, endclp, 2);
    freecall(endclp);
}

/*
 * Walk system call tables and do callback for
 * each entry with accumulated statistics.
 */
void
rtmon_walksyscalls(rtmonPrintState* rs, void (*f)(rtmonPrintState*, const char*, int, __int64_t))
{
    int i;

    for (i = 0; i < nsysent; i++)  {
	const subsyscall_t* sp = syscalltab[i].subs;
	const subsyscall_t* xp;
	int count = 0;
	__int64_t time = 0;

	for (xp = sp; xp->name; xp++)
	    if (xp->num) {
		if (xp == sp || strcmp(xp->name, sp->name) == 0) {
		    count += xp->num;
		    time += xp->time;
		} else
		    (*f)(rs, xp->name, xp->num, xp->time);
	    }
	if (count)
	    (*f)(rs, sp->name, count, time);
    }
}

/* 
 * Control tracing for the given syscall name.
 * Returns 0 if the name is not recognized.
 * Note that we trace all system calls that we
 * display w/ the specified name; this handles
 * cases where multiple variants are displayed
 * with one name (e.g. stat).
 */
int
rtmon_settracebyname(rtmonPrintState* rs, const char* name, int onoff)
{
    int i;
    int found = 0;

    if ((rs->flags & RTPRINT_INITSYS) && !onoff)
	_rtmon_initsyscalltrace(rs);
    rs->flags &= ~RTPRINT_INITSYS;
    for (i = 0; i < nsysent; i++) {
	subsyscall_t* sp;
	for (sp = syscalltab[i].subs; sp->name; sp++)
	    if (strcasecmp(sp->name, name) == 0) {
		sp->trace = onoff;
		found++;
	    }
    }
    return (found != 0);
}

/* 
 * Control tracing for the given syscall number.
 * Returns 0 if the call number is invalid.
 */
int
rtmon_settracebynum(rtmonPrintState* rs, int callno, int onoff)
{
    if ((rs->flags & RTPRINT_INITSYS) && !onoff)
	_rtmon_initsyscalltrace(rs);
    rs->flags &= ~RTPRINT_INITSYS;
    if (callno < nsysent) {
	subsyscall_t* sp;
	for (sp = syscalltab[callno].subs; sp->name; sp++)
	    sp->trace = onoff;
	return (1);
    } else
	return (0);
}

/*
 * Construct a new syscall record from a call record in the input stream.
 * return a pointer to the new syscall record and the number of bytes it
 * occupied in the input stream.
 */
static void
callrectocl(rtmonPrintState* rs, const tstamp_event_entry_t* ev, struct cl** clpp)
{
    const tstamp_event_syscall_data_t* sdp =
	(const tstamp_event_syscall_data_t*) &ev->qual[0];
    struct cl* clp = (struct cl*) malloc(sizeof (struct cl)
	+ sdp->numparams * sizeof (__int64_t)
	+ sdp->numiparams * sizeof (indirect_t));
    int np;

    clp->kid = (int64_t) sdp->k_id;
    clp->callno = sdp->callno;
    clp->cpu = ev->cpu;
    clp->abi = sdp->abi;
    clp->num = sdp->numparams;
    clp->numi = sdp->numiparams;
    clp->tstamp = rtmon_adjtv(rs, ev->tstamp);
    clp->hnext = NULL;
    clp->endrec = NULL;
    clp->prstart = 0;
    clp->params = (__uint64_t*)(((char*) clp) + sizeof (*clp));
    if (ABI_IS_64BIT(rs->ds.kabi)) {
	memcpy(clp->params, sdp->params, sdp->numparams*sizeof (__uint64_t));
    } else {
	for (np = 0; np < sdp->numparams; np++)
	    clp->params[np] = sdp->params[np];
    }
    if (clp->numi > 0) {		/* collect any indirect parameters */
	const unsigned char* bp = ((const unsigned char*) &sdp->params[0]) +
	    sdp->numparams * (ABI_IS_64BIT(rs->ds.kabi) ?
				sizeof (__int64_t) : sizeof (__int32_t));
	clp->iparams = (indirect_t*) &clp->params[clp->num];
	for (np = 0; np < sdp->numiparams; np++) {
	    sysargdesc_t desc = (bp[0]<<8) + bp[1];
	    unsigned short len = (bp[2]<<8) + bp[3];

	    clp->iparams[np].argn = SY_GETARG(desc);
	    clp->iparams[np].len = len;
	    clp->iparams[np].data = malloc(len);
	    memcpy(clp->iparams[np].data, bp+4, len);
	    bp += 4+len;		/* desc+len+indirect data */
	}
    } else
	clp->iparams = NULL;
    clp->sys = find_syscall(rs,clp);	/* look up syscall */
    assert(clp->sys != NULL);		/* should always parse call */

    *clpp = clp;
}

static void
printf_size_t(char* line, int abi, char fmtx, __uint64_t v)
{
    static char fmt[5] = { '%', 'l', 'l', 'X', '\0' };
    fmt[3] = fmtx;
    if (!ABI_IS_64BIT(abi))
	v &= 0xffffffff;
    asprintf(line, fmt, v);
}

static void
printf_off_t(char* line, int abi, char fmtx, __uint64_t v)
{
    static char fmt[5] = { '%', 'l', 'l', 'X', '\0' };
    (void) abi;				/* 64-bit for all ABI's */
    fmt[3] = fmtx;
    asprintf(line, fmt, v);
}

static void
printf_ptr(char* line, int abi, char fmtx, __uint64_t v)
{
    static char fmt[6] = { '%', '#', 'l', 'l', 'X', '\0' };
    fmt[4] = fmtx;
    if (!ABI_IS_64BIT(abi))
	v &= 0xffffffff;
    asprintf(line, fmt, v);
}

static void
printf_longlong(char* line, char fmtx, __uint64_t v)
{
    static char fmt[5] = { '%', 'l', 'l', 'X', '\0' };
    fmt[3] = fmtx;
    asprintf(line, fmt, v);
}

/*
 * print out a call
 * flag = 0 - beginning of a call
 *	  1 - end of a matched set
 *	  2 - a lonesome end
 */
static void
printcall(rtmonPrintState* rs, FILE* fd, struct cl *crecp, int flag)
{
    subsyscall_t* sp = crecp->sys;
    int uargnum, argnum, params;
    int printrvals = 0;
    int inp, outp;
    struct cl *ecrecp, *pcrecp;
    char oline[RTMON_LINESIZE];
    __uint64_t err = 0;

    oline[0] = '\0';

    if (flag == 0 && crecp->prstart)		/* already printing */
	return;

    /*
     * We print arguments if:
     * 1) flag == 0 - print all input (indirect) args only
     * 2) flag == 1 - print all input & output (indirect) args
     * 3) flag == 1 && already printed start && have indirect output args -
     *	print output args only and print 'END-' before syscall name
     */
    pcrecp = crecp;
    inp = outp = 0;
    if (flag != 2) {
	if (flag == 1) {
	    printrvals = 1;
	    ecrecp = crecp->endrec;
	    /*
	     * grab error now - don't print indirect output args if
	     * we have an error
	     */
	    err = ecrecp->params[1];
	    if (crecp->prstart) {
		inp = 0;
		/* print pid/time info from end record */
		pcrecp = ecrecp;
		if (crecp->endrec->iparams)
		    outp = 1;
		/*
		 * If no indirect output args and have already
		 * printed start, we'll drop down and just
		 * print error returns.
		 */
	    } else {
		outp = 1;
		inp = 1;
	    }
	} else {
	    /* input args only */
	    ecrecp = NULL;
	    inp = 1;
	    outp = 0;
	}
    }

    /* print basic info */
    _rtmon_printEventTime(rs, fd, pcrecp->tstamp, pcrecp->cpu);
    _rtmon_printProcess(rs, fd, pcrecp->kid);
    fputc(' ', fd);

    if (inp || outp) {
	/* print syscall args */
	crecp->prstart = 1;

	if (inp == 0)
	    asprintf(oline, "END-");
	if (strcmp(sp->name, "nosys") == 0)
	    asprintf(oline, "#%d(", crecp->callno);
	else
	    asprintf(oline, "%s(", sp->name);

	if (params = sp->nargs) {
	    argnum = 0;
	    for (uargnum = 0; params; uargnum++) {
		const char* af;
		__uint64_t* paramp;
		indirect_t* ip;

		/* ugh - suparg is 1..n, not 0..n-1 */
		if (sp->suparg-1 == uargnum) {
		    /*
		     * note that the subsyscall tables have
		     * already decreased the # parameters
		     * to account for the suppressed one,
		     * as well as not having a format string
		     * for it
		     */
		    continue;
		}
		paramp = &crecp->params[uargnum];

		af = sp->argfmt[argnum];
		switch (af[0]) {
		case 'I':			/* indirect parameter value */
		    /*
		     * Note that we can't use argnum since
		     * it is affected by supressed args.
		     * For indirect lookup we need the actual
		     * argument # as the user specified it.
		     */
		    if (inp && (ip = findindirect(crecp, uargnum))) {
			/* an input parameter */
			prindirect(rs, ip, oline, af, crecp, inp && outp);
		    } else if (!err && outp && (ip = findindirect(ecrecp, uargnum))) {
			/* an output parameter */
			prindirect(rs, ip, oline, af, crecp,
			    inp && outp ? 2 : 0);
		    } else {
			printf_ptr(oline, crecp->abi, 'x', *paramp);
		    }
		    break;
		case 'G':			/* ignore */
		    break;
		case 'L':			/* long long as 2 params */
		    printf_longlong(oline, af[1], (paramp[0]<<32)|paramp[1]);
		    params--;
		    break;
		case 'N':			/* number */
		    switch (af[1]) {
		    case 'd':
			if (ABI_IS_64BIT(crecp->abi))
			    asprintf(oline, "%lld", *paramp);
			else
			    asprintf(oline, "%d", (__int32_t) *paramp);
			break;
		    case 'o':
			asprintf(oline, "%#llo", *paramp);
			break;
		    case 'x':
			asprintf(oline, "%#llx", *paramp);
			break;
		    case 'u':
			asprintf(oline, "%llu", *paramp);
			break;
		    }
		    break;
		case 'S':			/* size_t or ssize_t */
		    printf_size_t(oline, crecp->abi, af[1], *paramp);
		    break;
		case 'O':			/* off_t */
		    printf_off_t(oline, crecp->abi, af[1], *paramp);
		    break;
		case 'P':			/* generic pointer */
		    printf_ptr(oline, crecp->abi, af[1], *paramp);
		    break;
		case 'T':			/* table name */
		    asprintf(oline, "%s",
			_rtmon_tablookup(af+1, (long) *paramp));
		    break;
		default:			/* straight printf format */
		    asprintf(oline, af, *paramp);
		    break;
		}
		if (af[0] != 'G' && params != 1)
		    asprintf(oline, ", ");
		params--, argnum++;
	    }
	    asprintf(oline, ")");
	} else {				/* no params */
	    asprintf(oline, ")");
	}
    }
    if (flag == 0) {
	/* only wanted syscall start */
	fprintf(fd, "%s\n", oline);
	return;
    }

    /*
     * end call processing.
     * If flag == 2 then crecp points to an end record and
     * we can't print any indirect args.
     */
    if (flag == 2) {
	ecrecp = crecp;
	err = ecrecp->params[1];
	printrvals = 1;
    }

    if (flag == 2 || (inp == 0 && outp == 0)) {
	if (strcmp(sp->name, "nosys") == 0)
	    asprintf(oline, "END-#%d()", crecp->callno);
	else
	    asprintf(oline, "END-%s()", sp->name);
    }

    if (printrvals) {
	/* all args have been output - just do the return codes */
	err = ecrecp->params[1];
	if (err) {
	    asprintf(oline, " errno = %lld (%s)", err, strerror((int) err));
	} else {
	    const char* af = sp->retfmt[0];
	    switch (af[0]) {
	    case 'L':
		asprintf(oline, " = ");
		printf_longlong(oline, af[1],
		    ecrecp->params[0]<<32|ecrecp->params[2]);
		break;
	    case 'N':
		switch (af[1]) {
		case 'd':
		    if (ABI_IS_64BIT(crecp->abi))
			asprintf(oline, " = %lld", ecrecp->params[0]);
		    else
			asprintf(oline, " = %d", (__int32_t) ecrecp->params[0]);
		    break;
		case 'o':
		    asprintf(oline, " = %#llo", ecrecp->params[0]);
		    break;
		case 'x':
		    asprintf(oline, " = %#llx", ecrecp->params[0]);
		    break;
		case 'u':
		    asprintf(oline, " = %llu", ecrecp->params[0]);
		    break;
		}
		break;
	    default:
		if (af[0] == '%')
		    asprintf(oline, " = ");
		asprintf(oline, af, ecrecp->params[0]);
		break;
	    }
	    if (sp->retfmt[1])		/* print rval2 */
		asprintf(oline, sp->retfmt[1], ecrecp->params[2]);
	}
    }
    fprintf(fd, "%s\n", oline);
}

/*
 * generic decoder - use specified arg as an index
 */
static subsyscall_t*
gd(const rtmonPrintState* rs, const syscall_t* s, cl_t* ptr, int argnum)
{
    register subsyscall_t *sp;

    sp = s->subs;
    while (sp->name) {
	if (sp->val == ptr->params[argnum])
	    break;
	sp++;
    }
    return (sp->name == NULL ? s->subs : sp);
}

/*
 * abi decoder - use abi of process as index
 */
static subsyscall_t*
ad(const rtmonPrintState* rs, const syscall_t* s, cl_t* ptr, int argnum)
{
    register subsyscall_t *sp;

    (void) argnum;
    sp = s->subs;
    while (sp->name) {
	if (sp->val == ptr->abi)
	    break;
	sp++;
    }
    return (sp->name == NULL ? s->subs : sp);
}

/*
 * abi+kernel abi decoder - use abi of process and abi of kernel as index
 */
static subsyscall_t*
kad(const rtmonPrintState* rs, const syscall_t* s, cl_t* ptr, int argnum)
{
    register subsyscall_t *sp;

    (void) argnum;
    for (sp = s->subs; sp->name; sp++) {
	int kabi = sp->val>>16;
	int abi = sp->val&0xffff;
	if ((!kabi || kabi == rs->ds.kabi) && abi == ptr->abi)
	    break;
    }
    return (sp->name == NULL ? s->subs : sp);
}

/*
 * syssgi decoder.  Need to look at both the proc's abi and first
 * argument.  The upper 8 bits of the val field of every syssgi
 * subsyscall entry contains a bitmask of all the abi's for which
 * that entry is good for.
 */
static subsyscall_t*
ssd(const rtmonPrintState*rs, const syscall_t* s, cl_t* ptr, int argnum)
{
    register subsyscall_t *sp;
    int abi, val;

    sp = s->subs;
    while (sp->name) {
	abi = sp->val >> 24;
	val = sp->val & 0x00ffffff;

	if ((ptr->abi & abi) && ptr->params[argnum] == val)
	    break;
	sp++;
    }
    return (sp->name == NULL ? s->subs : sp);
}

/*
 * sigd - signal system call decoder
 */
static subsyscall_t*
sigd(const rtmonPrintState*rs, const syscall_t* s, cl_t* ptr, int argnum)
{
    register subsyscall_t *sp;
    register usysarg_t flag;

    (void) argnum;
    flag = ptr->params[0] & ~SIGNO_MASK;
    sp = s->subs;
    while (sp->name) {
	if (sp->val == flag)
	    break;
	sp++;
    }
    return (sp->name == NULL ? s->subs : sp);
}

static subsyscall_t*
find_syscall(const rtmonPrintState* rs, cl_t* ptr)
{
    int callno = ptr->callno;

    if (callno < nsysent) {
	const syscall_t* s = &syscalltab[callno];
	return (s->decode ? (*s->decode)(rs, s, ptr, s->decodearg) : s->subs);
    } else
	return (NULL);
}

static indirect_t*
findindirect(struct cl* crecp, int argnum)
{
    int i;
    for (i = 0; i < crecp->numi; i++)
	if (crecp->iparams[i].argn == argnum)
	    return (&crecp->iparams[i]);
    return (NULL);
}

/*
 * Print indirect args - these are signified by a format character of 'I'
 * These are usually complicated enough that they each have their own function
 * to do the actual argument decoding and printing
 * The character following the I says what the parameter is:
 *	I - input only
 *	O - output only
 *	B - both input and output
 */
static void
prindirect(rtmonPrintState* rs, indirect_t* ip, char* oline, const char* format, struct cl* crecp, int both)
{
    const iptab_t *pt;
    int doingin = 0;

    if (*format++ != 'I')
	return;
    if (*format == 'I' || (both == 1 && *format == 'B'))
	doingin = 1;
    if (both && *format == 'B') {
	if (both == 1)
	    asprintf(oline, "IN:");
	else
	    asprintf(oline, " OUT:");
    }
    format++;	/* skip over 'I', 'O', ... */
    pt = iptable;
    while (pt->tabname) {
	if (strcmp(pt->tabname, format) == 0)
	    break;
	pt++;
    }
    assert(pt->tabname);

    (*pt->func)(rs, ip, crecp, oline, doingin);
}

static void
string_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl* crecp, char* oline, int in)
{
    (void) rs; (void) in; (void) crecp;
    if (ip->len == 0 || (ip->len == 1 && ip->data[0] == '\0'))
	asprintf(oline, "\"(null)\"");
    else {
#ifdef notdef
	/* XXX should we really limit strings?? use max_ascii??
	 * this may make more sense when we can control string
	 * len in prf.c
	 */
	asprintf(oline, "\"%.*s\"", ip->len > rs->max_asciilen ?
			rs->max_asciilen : ip->len, ip->data);
#else
	asprintf(oline, "\"%.*s\"", ip->len, ip->data);
#endif
    }
}

static void
int_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl* crecp, char* oline, int in)
{
    int i, tmp;

    (void) rs; (void) crecp; (void) in;
    if ((ip->len % sizeof(int)) != 0) {
	asprintf(oline, "<bad indirect int param - len %d>", ip->len);
    } else {
	for (i = 0; i < ip->len; i += sizeof(int)) {
	    tmp = ip->data[i] << 24;
	    tmp |= ip->data[i+1] << 16;
	    tmp |= ip->data[i+2] << 8;
	    tmp |= ip->data[i+3] << 0;
	    if (i != 0)
		asprintf(oline, " %d", tmp);
	    else
		asprintf(oline, "%d", tmp);
	}
    }
}

static struct ccdef {
    int		ix;
    const char* name;
    cc_t	def;
} ccdefs[] = {
    { VINTR,	"intr",		CINTR },
    { VQUIT,	"quit",		CQUIT },
    { VERASE,	"erase",	CERASE },
    { VKILL,	"kill",		CKILL },
    { VEOF,	"eof",		CEOF },
    { VEOL,	"eol",		CEOL },
    { VEOL2,	"eol2",		CEOL2 },
    { VSWTCH,	"swtch",	CSWTCH },
    { VSTART,	"start",	CSTART },
    { VSTOP,	"stop",		CSTOP },
    { VSUSP,	"susp",		CSUSP },
    { VDSUSP,	"dsusp",	CDSUSP },
    { VRPRNT,	"rprnt",	CRPRNT },
    { VDISCARD,	"flush",	CFLUSH },
    { VWERASE,	"werase",	CWERASE },
    { VLNEXT,	"lnext",	CLNEXT },
};

static void
pcc(char* oline, const cc_t cc[])
{
#define	N(a)	(sizeof (a) / sizeof (a[0]))
    const char* sep = "";
    int i;

    asprintf(oline, ", cc=[");
    for (i = 0; i < N(ccdefs); i++) {
	const struct ccdef* cd = &ccdefs[i];
	int c = cc[cd->ix];
	if (c != cd->def) {
	    if (c < ' ')
		asprintf(oline, "%s%s=^%c", sep, cd->name, c+'@');
	    else if (c == 0177)
		asprintf(oline, "%s%s=^?", sep, cd->name);
	    else if (c == 0377)
		asprintf(oline, "%s%s=<undef>", sep, cd->name);
	    else
		asprintf(oline, "%s%s=%c", sep, cd->name, c);
	    sep = ",";
	}
    }
    asprintf(oline, "%smin=%u,time=%u]", sep, cc[VMIN], cc[VTIME]);
#undef N
}

static void
ntermio_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl* crecp, char* oline, int in)
{
    const struct __new_termio* tio = (const struct __new_termio*) ip->data;

    (void) rs; (void) crecp; (void) in;
    if (ip->len != sizeof (*tio)) {
	asprintf(oline, "<bad indirect __new_termio param - len %d>", ip->len);
    } else {
	asprintf(oline, "{iflag=%#o, oflag=%#o, cflag=%#o, lflag=%#o, ospeed=%d, ispeed=%d,line=%d"
	    , tio->c_iflag, tio->c_oflag, tio->c_cflag, tio->c_lflag
	    , tio->c_ospeed, tio->c_ispeed, tio->c_line
	);
	pcc(oline, tio->c_cc);
	asprintf(oline, "}");
    }
}

static void
otermio_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl* crecp, char* oline, int in)
{
    const struct __old_termio* tio = (const struct __old_termio*) ip->data;

    (void) rs; (void) crecp; (void) in;
    if (ip->len != sizeof (*tio)) {
	asprintf(oline, "<bad indirect __old_termio param - len %d>", ip->len);
    } else {
	asprintf(oline, "{iflag=%#o, oflag=%#o, cflag=%#o, lflag=%#o, line=%d"
	    , tio->c_iflag, tio->c_oflag, tio->c_cflag, tio->c_lflag
	    , tio->c_line
	);
	pcc(oline, tio->c_cc);
	asprintf(oline, "}");
    }
}

static void
prascii(rtmonPrintState* rs, char* oline, const char* data, size_t len)
{
    const char* sp;
    char* cp;
    char* ep;
    int c, i, binary;

    /*
     * if entire data is printable, do it, else print in hex
     */
    binary = 0;
    for (i = 0, sp = data; i < len; i++, sp++) {
	c = *sp;
	if (!isascii(c) || (iscntrl(c) && !isspace(c) && c != 0)) {
	    binary = 1;
	    break;
	}
    }
    cp = strchr(oline,'\0');
    ep = &oline[RTMON_LINESIZE] - 5;		/* -5 for trailing >...\0 */
    if (binary || (rs->flags & (RTPRINT_BINARY|RTPRINT_ASCII)) == RTPRINT_BINARY) {
	*cp++ = '<';
	i = 0, sp = data;
	for (; i < len && i < rs->max_binlen && cp+3 < ep; i++, sp++) {
	    if (i != 0) {
		sprintf(cp, " %02x", *sp & 0xff);
		cp += 3;
	    } else {
		sprintf(cp, "%02x", *sp & 0xff);
		cp += 2;
	    }
	}
	strcpy(cp, i < len ? ">..." : ">");
    } else {
	*cp++ = '"';
	i = 0, sp = data;
	for (; i < len && i < rs->max_asciilen && cp+3 < ep; i++, sp++) {
	    c = *sp;
	    switch (c) {
	    case '\t': *cp++ = '\\'; *cp++ = 't'; break;
	    case '\n': *cp++ = '\\'; *cp++ = 'n'; break;
	    case '\b': *cp++ = '\\'; *cp++ = 'b'; break;
	    case '\f': *cp++ = '\\'; *cp++ = 'f'; break;
	    case '\r': *cp++ = '\\'; *cp++ = 'r'; break;
	    case '\v': *cp++ = '\\'; *cp++ = 'v'; break;
	    case '\0': *cp++ = '\\'; *cp++ = '0'; break;
	    default:
		if (binary) {
		    /*
		     * There was some binary data
		     * but we're being asked to output
		     * it as characters - output it as
		     * \xx
		     */
		    if (isascii(c) && isgraph(c))
			*cp++ = c;
		    else {
			*cp++ = '\\';
			sprintf(cp, "%x", c);
			cp++;
			if (c / 16)
			    cp++;
		    }
		} else
		    *cp++ = c;
		break;
	    }
	}
	strcpy(cp, i < len ? "\"..." : "\"");
    }
}

static void
ascii_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl* crecp, char* oline, int in)
{
    (void) crecp; (void) in;
    prascii(rs, oline, ip->data, ip->len);
}

static void
sockadd_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    const struct sockaddr* sa = (const struct sockaddr*) ip->data;

    switch (sa->sa_family) {
    case AF_UNIX:
	asprintf(oline, "{sun_family=AF_UNIX, sun_path=");
	prascii(rs, oline, sa->sa_data, strlen(sa->sa_data));
	asprintf(oline, "}");
	break;
    case AF_INET:
	asprintf(oline, "{sin_family=AF_INET, sin_port=%u, sin_addr=%s}",
	    ntohs(((const struct sockaddr_in*) sa)->sin_port),
	    inet_ntoa(((const struct sockaddr_in*) sa)->sin_addr));
	break;
    default:
	asprintf(oline, "{sa_family=%s, sa_data=",
	    _rtmon_tablookup("addr_family", sa->sa_family));
	prascii(rs, oline, sa->sa_data, sizeof (sa->sa_data));
	asprintf(oline, "}");
	break;
    }
}

static void
flock_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    const struct flock* fp = (const struct flock*) ip->data;
    int op = crecp->params[1];

    (void) rs; (void) in;
    asprintf(oline, "{");
    switch (op) {
    case F_ALLOCSP:
    case F_FREESP:
    case F_RESVSP:
    case F_UNRESVSP:
	break;
    default:
	asprintf(oline, "type=%s, ", _rtmon_tablookup("flock1", fp->l_type));
	break;
    }
    asprintf(oline, "whence=%s, start=%lld, len=%lld"
	, _rtmon_tablookup("flock2", fp->l_whence)
	, fp->l_start
	, fp->l_len
    );
    switch (op) {
    case F_GETLK:
    case F_RGETLK:
	if (fp->l_type == F_UNLCK)
	    break;
	/* fall thru... */
    case F_RSETLK:
    case F_RSETLKW:
	asprintf(oline, ", pid=%d, sysid=%d", fp->l_pid, fp->l_sysid);
	break;
    }
    asprintf(oline, "}");
}

static void
flock64_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    const struct flock64* fp = (const struct flock64*) ip->data;
    int op = crecp->params[1];

    (void) rs; (void) in;
    asprintf(oline, "{");
    switch (op) {
    case F_SETLK64:
    case F_SETLKW64:
    case F_GETLK64:
	asprintf(oline, "type=%s, ", _rtmon_tablookup("flock1", fp->l_type));
    }
    asprintf(oline, "whence=%s, start=%lld, len=%lld"
	, _rtmon_tablookup("flock2", fp->l_whence)
	, fp->l_start
	, fp->l_len
    );
    if (op == F_GETLK64 && fp->l_type != F_UNLCK)
	asprintf(oline, ", pid=%d, sysid=%d", fp->l_pid, fp->l_sysid);
    asprintf(oline, "}");
}

static void
dioinfo_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    const struct dioattr* dio = (const struct dioattr*) ip->data;

    (void) rs; (void) crecp; (void) in;
    asprintf(oline, "{mem=%d, miniosz=%d, maxiosz=%d}",
	dio->d_mem, dio->d_miniosz, dio->d_maxiosz);
}

static void
xattr_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    const struct fsxattr* fsx = (const struct fsxattr*) ip->data;

    (void) rs; (void) in;
    asprintf(oline, "{xflags=%#x, extsize=%d",
	fsx->fsx_xflags, fsx->fsx_extsize);
    if (crecp->params[1] != F_FSSETXATTR)
	asprintf(oline, ", nextents=%d", fsx->fsx_nextents);
    asprintf(oline, "}");
}

static void
fssetdm_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    const struct fsdmidata* dmp = (const struct fsdmidata*) ip->data;

    (void) rs; (void) crecp; (void) in;
    asprintf(oline, "{evmask=%#x, dmstate=%#x}",
	dmp->fsd_dmevmask, dmp->fsd_dmstate);
}

static void
getbmap_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    const struct getbmap* bm = (const struct getbmap*) ip->data;

    (void) rs; (void) crecp; (void) in;
    asprintf(oline,
	"{offset=%lld, block=%lld, length=%lld, count=%d, entries=%d}"
	, bm->bmv_offset
	, bm->bmv_block
	, bm->bmv_length
	, bm->bmv_count
	, bm->bmv_entries
    );
}

extern int sig2str(int, char *);        /* XXX not protoyped anywhere */

static void
prsigset(const sigset_t* s, char* oline)
{
    sigset_t fs;

    /*
     * check for the fairly common case of all signals
     * being set or empty
     */
    asprintf(oline, "[");
    sigfillset(&fs);
    if (memcmp(s, &fs, sizeof(fs)) == 0) {
	asprintf(oline, "<all>");
    } else if (sgi_sigffset((sigset_t*) s, 0) == 0) {
	asprintf(oline, "<none>");
    } else {
	int i;
	int first;
	const char* sep = "";

	for (i = 1; i < 32; i++)
	    if (sigismember(s, i)) {
		asprintf(oline, "%s%s", sep, _rtmon_tablookup("signames", i));
		sep = " ";
	    }
	/* these are less interesting - try to compress output */
	first = 0;
	for (i = 32; i <= NSIG; i++) {
	    /*
	     * This trick gets us through 1 additional time to pick up
	     * signal masks including the last legal signal
	     */
	    if (i < NSIG && sigismember(s, i)) {
		if (first == 0)
		    first = i;
	    } else if (first) {
		if (i == first + 1)
		    asprintf(oline, "%s%d", sep, first);
		else
		    asprintf(oline, "%s%d-%d", sep, first, i - 1);
		first = 0;
		sep = " ";
	    }
	}
    }
    asprintf(oline, "]");
}

static void
sigset_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    (void) rs; (void) crecp; (void) in;
    prsigset((const sigset_t*) ip->data, oline);
}

static void
sigact_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    struct sigaction *sa;

    (void) rs; (void) in;
#if (_MIPS_SZLONG == 64)
    /* we handle 32 and 64 bit apps */
    if (!ABI_IS_IRIX5_64(crecp->abi)) {
	irix5_sigaction_t *sa5 = (irix5_sigaction_t *)ip->data;

	asprintf(oline, "{flags=%s handler=",
	     _rtmon_tablookup("sigactflags", sa5->sa_flags));
	if ((SIG_PF)(ptrdiff_t)sa5->k_sa_handler == SIG_DFL)
	    asprintf(oline, "SIG_DFL");
	else if ((SIG_PF)(ptrdiff_t)sa5->k_sa_handler == SIG_IGN)
	    asprintf(oline, "SIG_IGN");
	else
	    printf_ptr(oline, crecp->abi, 'x', sa5->k_sa_handler);
	asprintf(oline, " mask=");
	prsigset(&sa5->sa_mask, oline);
	asprintf(oline, "}");
    } else {
#endif
	sa = (struct sigaction *)ip->data;

	asprintf(oline, "{flags=%s handler=",
	    _rtmon_tablookup("sigactflags", sa->sa_flags));
	if (sa->sa_handler == SIG_DFL)
	    asprintf(oline, "SIG_DFL");
	else if (sa->sa_handler == SIG_IGN)
	    asprintf(oline, "SIG_IGN");
	else
	    printf_ptr(oline, crecp->abi, 'x', (__uint64_t) sa->sa_handler);
	asprintf(oline, " mask=");
	prsigset(&sa->sa_mask, oline);
	asprintf(oline, "}");
#if (_MIPS_SZLONG == 64)
    }
#endif
}

static void
fd_set_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    int nfds = (int) crecp->params[0];		/* nfds parameter */
    int maxfds = ip->len * __NBBY;		/* max # fds in data */
    int fds = 0;				/* count of fds set */
    const char* sep = "";
    int i;

    (void) rs; (void) in;
    asprintf(oline, "[");
    if (nfds > maxfds)
	nfds = maxfds;
    for (i = 0; nfds > 0; nfds -= __NFDBITS, i++) {
	/* NB: alignment should be ok to do struct copy */
	ufd_mask_t bits = ((ufd_mask_t*) ip->data)[i];
	int j = 0;
	if (nfds < __NFDBITS)
	    bits &= (1<<nfds)-1;
	for (; bits != 0; bits >>= 1, j++)
	    if (bits & 1) {
		asprintf(oline, "%s%d", sep, i*__NFDBITS+j), sep = ":";
		fds++;
	    }
    }
    if (fds == 0)				/* treat empty set specially */
	asprintf(oline, "<empty>");
    nfds = (int) crecp->params[0];
    if (nfds > maxfds)				/* indicate missing data */
	asprintf(oline, "...<%d fd's truncated>", nfds - maxfds);
    asprintf(oline, "]");
}

static void
prsiginfo(const struct siginfo* sp, char* oline)
{
    asprintf(oline, "{signo=%s, ", _rtmon_tablookup("signames", sp->si_signo));
    if (sp->si_errno)
	asprintf(oline, "errno=<%s>, ", strerror(sp->si_errno));
    else
	asprintf(oline, "errno=0, ");
    if (SI_FROMUSER(sp)) {
	asprintf(oline, "code=%s, ",
	    _rtmon_tablookup("sigcodes", sp->si_code));
	if (sp->si_code == SI_USER)
	    asprintf(oline, "pid=%d, uid=%d", sp->si_pid, sp->si_uid);
	else
	    asprintf(oline, "value=%#lx", sp->si_value);
    } else {
	switch (sp->si_signo) {
	case SIGCLD:
	    asprintf(oline, "code=%s, pid=%d, status=%#x",
		_rtmon_tablookup("sicodeCLD", sp->si_code),
		sp->si_pid, sp->si_status);
	    break;
	case SIGSEGV:
	case SIGBUS:
	case SIGILL:
	case SIGFPE:
	    asprintf(oline, "code=%d addr=%#lx", sp->si_code, sp->si_addr);
	    break;
	default:
	    asprintf(oline, "code=%d", sp->si_code);
	    break;
	}
    }
    asprintf(oline, "}");
}

static void
waitsys_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    const struct siginfo* sp = (const struct siginfo*) ip->data;

    (void) rs; (void) crecp; (void) in;
    /* 
     * waitsys one should always return a pid ...
     */
    if (sp->si_pid == 0)
	asprintf(oline, "NO PID");
    else
	prsiginfo(sp, oline);
}

static void
winsize_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    const struct winsize* ws = (const struct winsize*) ip->data;

    (void) rs; (void) crecp; (void) in;
    asprintf(oline, "{row=%d, col=%d, xpixel=%d, ypixel=%d}",
	ws->ws_row, ws->ws_col, ws->ws_xpixel, ws->ws_ypixel);
}

static void
siginfo_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    const struct siginfo* sp = (const struct siginfo*) ip->data;

    (void) rs; (void) crecp; (void) in;
    prsiginfo(sp, oline);
}

static void
timeval_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    (void) rs; (void) in;
    if (ABI_IS_IRIX5_64(crecp->abi)) {
	struct _64timeval {
	    __int32_t	:32;
	    __int32_t	sec;
	    __int64_t	usec;
	};
	const struct _64timeval* tvp = (const struct _64timeval*) ip->data;
	asprintf(oline, "{sec=%d, usec=%lld}", tvp->sec, tvp->usec);
    } else {
	const struct timeval* tvp = (const struct timeval*) ip->data;
	asprintf(oline, "{sec=%ld, usec=%ld}", tvp->tv_sec, tvp->tv_usec);
    }
}

static void
timespec_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    (void) rs; (void) in;
    if (ABI_IS_IRIX5_64(crecp->abi)) {
	struct _64timespec {
	    __int32_t	:32;
	    __int32_t	sec;
	    __int64_t	nsec;
	};
	const struct _64timespec* tsp = (const struct _64timespec*) ip->data;
	asprintf(oline, "{sec=%d, nsec=%lld}", tsp->sec, tsp->nsec);
    } else {
	const struct timespec* tsp = (const struct timespec*) ip->data;
	asprintf(oline, "{sec=%ld, nsec=%ld}", tsp->tv_sec, tsp->tv_nsec);
    }
}

static void
uio_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    (void) rs; (void) ip; (void) crecp; (void) oline; (void) in;
}

static void
pollfd_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    unsigned long nfds = (unsigned long) crecp->params[1];

    (void) rs;
    if (nfds != 0) {
	const struct pollfd* fds = (const struct pollfd*) ip->data;
	const char* sep = "";
	int i;

	asprintf(oline, "[");
	if (ip->len != nfds * sizeof (*fds))
	    asprintf(oline, "<bad set len %d>", ip->len);
	for (i = 0; i < ip->len; i += sizeof (*fds)) {
	    asprintf(oline, "%sfd=%d %s=%s"
		, sep
		, fds->fd
		, in ? "event" : "revent"
		, _rtmon_tablookup("pollevents",
		    in ? fds->events : fds->revents)
	    );
	    fds++;
	    sep = ", ";
	}
	asprintf(oline, "]");
    } else
	asprintf(oline, "[<none>]");
}

/*
 * NB: this stuff is untested (requires kernel support)...
 */

static void
fmtdev(char* oline, const char* tag, dev_t dev)
{
    asprintf(oline, "%s=makedev(%u,%u)", tag, major(dev), minor(dev));
}
static void
fmtmode(char* oline, mode_t mode)
{
    asprintf(oline, "st_mode=%s%s%s%s|%#o"
	, _rtmon_tablookup("statmodes", (long)(mode&S_IFMT))
	, mode&S_ISUID ? "|S_ISUID" : ""
	, mode&S_ISGID ? "|S_ISGID" : ""
	, mode&S_ISVTX ? "|S_ISVTX" : ""
	, mode&0777
    );
}
static void
stat_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    (void) rs; (void) in;
    if (ABI_IS_IRIX5_64(crecp->abi)) {
	typedef struct {
	    __int32_t	:32;
	    __int32_t	sec;
	    __int64_t	nsec;
	} _64timespec_t;
	struct _64stat {
	    dev_t	st_dev;
	    __int64_t	sta_pad1[_ST_PAD1SZ];
	    ino_t	st_ino;
	    mode_t	st_mode;
	    nlink_t	st_nlink;
	    uid_t	st_uid;
	    gid_t	st_gid;
	    dev_t	st_rdev;
	    __int64_t	st_pad2[_ST_PAD2SZ];
	    off_t	st_size;
	    __int64_t	st_pad3;
	    _64timespec_t st_atim;
	    _64timespec_t st_mtim;
	    _64timespec_t st_ctim;
	    __int64_t	st_blksize;
	    blkcnt_t	st_blocks;
	    char	st_fstype[_ST_FSTYPSZ];
	    __int64_t	st_pad4[_ST_PAD4SZ];
	};
	const struct _64stat* sp = (const struct _64stat*) ip->data;

	fmtdev(oline, "{st_dev", sp->st_dev);
	asprintf(oline, ", st_ino=%llu, ", sp->st_ino);
	fmtmode(oline, sp->st_mode);
	asprintf(oline , ", st_nlink=%u, st_uid=%u, st_gid=%u, ",
	    sp->st_nlink , sp->st_uid , sp->st_gid);
	if (S_ISCHR(sp->st_mode) || S_ISBLK(sp->st_mode))
	    fmtdev(oline, "st_rdev", sp->st_dev);
	asprintf(oline,
	    ", st_size=%llu ... st_blksize=%llu, st_blocks=%u, st_fstype=%.*s}"
	    , sp->st_size
	    , sp->st_blksize
	    , sp->st_blocks
	    , sizeof (sp->st_fstype), sp->st_fstype
	);
    } else {
	const struct stat* sp = (const struct stat*) ip->data;

	fmtdev(oline, "{st_dev", sp->st_dev);
	asprintf(oline, ", st_ino=%llu, ", sp->st_ino);
	fmtmode(oline, sp->st_mode);
	asprintf(oline , ", st_nlink=%u, st_uid=%u, st_gid=%u, ",
	    sp->st_nlink , sp->st_uid , sp->st_gid);
	if (S_ISCHR(sp->st_mode) || S_ISBLK(sp->st_mode))
	    fmtdev(oline, "st_rdev", sp->st_dev);
	asprintf(oline,
	    ", st_size=%llu ... st_blksize=%llu, st_blocks=%u, st_fstype=%.*s}"
	    , sp->st_size
	    , sp->st_blksize
	    , sp->st_blocks
	    , sizeof (sp->st_fstype), sp->st_fstype
	);
    }
}

static void
stat64_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    (void) rs; (void) crecp; (void) in;
    asprintf(oline, "stat64=%#llx", ip);		/*XXX*/
}

static void
statfs_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    (void) rs; (void) crecp; (void) in;
    asprintf(oline, "statfs=%#llx", ip);		/*XXX*/
}

static void
statvfs_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    (void) rs; (void) crecp; (void) in;
    asprintf(oline, "statfs=%#llx", ip);		/*XXX*/
}

static void
statvfs64_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    (void) rs; (void) crecp; (void) in;
    asprintf(oline, "statfs=%#llx", ip);		/*XXX*/
}

static void
rusage_indirect(rtmonPrintState* rs, const indirect_t* ip, const struct cl *crecp, char *oline, int in)
{
    (void) rs; (void) crecp; (void) in;
    asprintf(oline, "rusage=%#llx", ip);		/*XXX*/
}

static int
nbits(unsigned long val)
{
    int bits = 0;
    while (val) {
	if (val & 1)
	    bits++;
	val >>= 1;
    }
    return(bits);
}

/*
 * _rtmon_tablookup - return a pretty string to print, given a table name
 *	and a value
 * We do some special checking to be sure that all values are
 * matched, so we still print the hex value if there are some new
 * flags that par doesn't know about
 * Also, for complex tables like ioctl that may have duplicate values
 * we search them all
 */
const char *
_rtmon_tablookup(const char* tabname, long val)
{
    const ptab_t *pt;
    const pp_t *pp;
    int first = 1;
    int found;
    int matches = 0;
    int totbits = 0;
    char *tok;
    char* tb = strdup(tabname);		/* NB: strtok destroys! */
    static char hv[RTMON_LINESIZE];

    hv[0] = '\0';
    for (tok = strtok(tb, "|"); tok; tok = strtok(NULL, "|")) {
	/* find table first */
	pt = pptable;
	while (pt->tabname) {
	    if (strcmp(pt->tabname, tok) == 0)
		break;
	    pt++;
	}
	assert(pt->tabname);
	pp = pt->tab;

	/* we have a table with a mask - compute # of bits
	 * in val that need to match
	 */
	if (pt->flags & ISMASK)
	    totbits += nbits(val & pt->mask);
	else
	    /* XXX requires a match */
	    totbits++;
	/*
	 * 2 cases - a bit mask and a value
	 */
	while (pp->name) {
	    if (pt->flags & ISMASK)
		found = (pp->val & (val & pt->mask)) != 0;
	    else
		found = (pp->val == (val & pt->mask));
	    if (found) {
		matches++;
		if (!first)
		    strcat(hv, "|");
		first = 0;
		strcat(hv, pp->name);
	    }
	    pp++;
	}
    }

    free(tb);
    if (hv[0] == '\0') {
	/* not found - return hex version as a string */
	if (pt->flags & PRINTDECIMAL)
	    snprintf(hv, sizeof (hv), "%ld", val);
	else
	    snprintf(hv, sizeof (hv), "%#lx", val);
    } else if (totbits != matches)
	asprintf(hv, "(%#lx)", val);
    return (hv);
}
