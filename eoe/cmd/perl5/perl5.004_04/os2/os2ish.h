#include <signal.h>

/* HAS_IOCTL:
 *	This symbol, if defined, indicates that the ioctl() routine is
 *	available to set I/O characteristics
 */
#define	HAS_IOCTL		/**/
 
/* HAS_UTIME:
 *	This symbol, if defined, indicates that the routine utime() is
 *	available to update the access and modification times of files.
 */
#define HAS_UTIME		/**/

#define HAS_KILL
#define HAS_WAIT
#define HAS_DLERROR
#define HAS_WAITPID_RUNTIME (_emx_env & 0x200)

/* USEMYBINMODE
 *	This symbol, if defined, indicates that the program should
 *	use the routine my_binmode(FILE *fp, char iotype) to insure
 *	that a file is in "binary" mode -- that is, that no translation
 *	of bytes occurs on read or write operations.
 */
#undef USEMYBINMODE

/* USE_STAT_RDEV:
 *	This symbol is defined if this system has a stat structure declaring
 *	st_rdev
 */
#define USE_STAT_RDEV 	/**/

/* ACME_MESS:
 *	This symbol, if defined, indicates that error messages should be 
 *	should be generated in a format that allows the use of the Acme
 *	GUI/editor's autofind feature.
 */
#undef ACME_MESS	/**/

/* ALTERNATE_SHEBANG:
 *	This symbol, if defined, contains a "magic" string which may be used
 *	as the first line of a Perl program designed to be executed directly
 *	by name, instead of the standard Unix #!.  If ALTERNATE_SHEBANG
 *	begins with a character other then #, then Perl will only treat
 *	it as a command line if if finds the string "perl" in the first
 *	word; otherwise it's treated as the first line of code in the script.
 *	(IOW, Perl won't hand off to another interpreter via an alternate
 *	shebang sequence that might be legal Perl code.)
 */
#define ALTERNATE_SHEBANG "extproc "

#ifndef SIGABRT
#    define SIGABRT SIGILL
#endif
#ifndef SIGILL
#    define SIGILL 6         /* blech */
#endif
#define ABORT() kill(getpid(),SIGABRT);

#define BIT_BUCKET "/dev/nul"  /* Will this work? */

#if defined(I_SYS_UN) && !defined(TCPIPV4)
/* It is not working without TCPIPV4 defined. */
# undef I_SYS_UN
#endif 
 
void Perl_OS2_init(char **);

/* XXX This code hideously puts env inside: */

#define PERL_SYS_INIT(argcp, argvp) STMT_START {	\
    _response(argcp, argvp);			\
    _wildcard(argcp, argvp);			\
    Perl_OS2_init(env);	} STMT_END

#define PERL_SYS_TERM()

/* #define PERL_SYS_TERM() STMT_START {	\
    if (Perl_HAB_set) WinTerminate(Perl_hab);	} STMT_END */

#define dXSUB_SYS OS2_XS_init()

#ifdef PERL_IS_AOUT
/* #  define HAS_FORK */
/* #  define HIDEMYMALLOC */
/* #  define PERL_SBRK_VIA_MALLOC */ /* gets off-page sbrk... */
#else /* !PERL_IS_AOUT */
#  ifndef PERL_FOR_X2P
#    ifdef EMX_BAD_SBRK
#      define USE_PERL_SBRK
#    endif 
#  else
#    define PerlIO FILE
#  endif 
#  define SYSTEM_ALLOC(a) sys_alloc(a)

void *sys_alloc(int size);

#endif /* !PERL_IS_AOUT */
#if !defined(PERL_CORE) && !defined(PerlIO) /* a2p */
#  define PerlIO FILE
#endif 

#define TMPPATH tmppath
#define TMPPATH1 "plXXXXXX"
extern char *tmppath;
PerlIO *my_syspopen(char *cmd, char *mode);
/* Cannot prototype with I32 at this point. */
int my_syspclose(PerlIO *f);
FILE *my_tmpfile (void);
char *my_tmpnam (char *);

#define tmpfile	my_tmpfile
#define tmpnam	my_tmpnam
#define isatty	_isterm
#define rand	random
#define srand	srandom

/*
 * fwrite1() should be a routine with the same calling sequence as fwrite(),
 * but which outputs all of the bytes requested as a single stream (unlike
 * fwrite() itself, which on some systems outputs several distinct records
 * if the number_of_items parameter is >1).
 */
#define fwrite1 fwrite

#define my_getenv(var) getenv(var)
#define flock	my_flock

void *emx_calloc (size_t, size_t);
void emx_free (void *);
void *emx_malloc (size_t);
void *emx_realloc (void *, size_t);

/*****************************************************************************/

#include <stdlib.h>	/* before the following definitions */
#include <unistd.h>	/* before the following definitions */

#define chdir	_chdir2
#define getcwd	_getcwd2

/* This guy is needed for quick stdstd  */

#if defined(USE_STDIO_PTR) && defined(STDIO_PTR_LVALUE) && defined(STDIO_CNT_LVALUE)
	/* Perl uses ungetc only with successful return */
#  define ungetc(c,fp) \
	(FILE_ptr(fp) > FILE_base(fp) && c == (int)*(FILE_ptr(fp) - 1) \
	 ? (--FILE_ptr(fp), ++FILE_cnt(fp), (int)c) : ungetc(c,fp))
#endif

#define OP_BINARY O_BINARY

#define OS2_STAT_HACK 1
#if OS2_STAT_HACK

#define Stat(fname,bufptr) os2_stat((fname),(bufptr))
#define Fstat(fd,bufptr)   fstat((fd),(bufptr))
#define Fflush(fp)         fflush(fp)
#define Mkdir(path,mode)   mkdir((path),(mode))

#undef S_IFBLK
#undef S_ISBLK
#define S_IFBLK		0120000
#define S_ISBLK(mode)	(((mode) & S_IFMT) == S_IFBLK)

#else

#define Stat(fname,bufptr) stat((fname),(bufptr))
#define Fstat(fd,bufptr)   fstat((fd),(bufptr))
#define Fflush(fp)         fflush(fp)
#define Mkdir(path,mode)   mkdir((path),(mode))

#endif

/* With SD386 it is impossible to debug register variables. */
#if !defined(PERL_IS_AOUT) && defined(DEBUGGING) && !defined(register)
#  define register
#endif

/* Our private OS/2 specific data. */

typedef struct OS2_Perl_data {
  unsigned long flags;
  unsigned long phab;
  int (*xs_init)();
  unsigned long rc;
  unsigned long severity;
} OS2_Perl_data_t;

extern OS2_Perl_data_t OS2_Perl_data;

#define Perl_hab		((HAB)OS2_Perl_data.phab)
#define Perl_rc			(OS2_Perl_data.rc)
#define Perl_severity		(OS2_Perl_data.severity)
#define errno_isOS2		12345678
#define OS2_Perl_flags	(OS2_Perl_data.flags)
#define Perl_HAB_set_f	1
#define Perl_HAB_set	(OS2_Perl_flags & Perl_HAB_set_f)
#define set_Perl_HAB_f	(OS2_Perl_flags |= Perl_HAB_set_f)
#define set_Perl_HAB(h) (set_Perl_HAB_f, Perl_hab = h)
#define OS2_XS_init() (*OS2_Perl_data.xs_init)()
/* The expressions below return true on error. */
/* INCL_DOSERRORS needed. rc should be declared outside. */
#define CheckOSError(expr) (!(rc = (expr)) ? 0 : (FillOSError(rc), 1))
/* INCL_WINERRORS needed. */
#define SaveWinError(expr) ((expr) ? : (FillWinError, 0))
#define CheckWinError(expr) ((expr) ? 0: (FillWinError, 1))
#define FillOSError(rc) (Perl_rc = rc,					\
			errno = errno_isOS2,				\
			Perl_severity = SEVERITY_ERROR) 
#define FillWinError (Perl_rc = WinGetLastError(Perl_hab),		\
			errno = errno_isOS2,				\
			Perl_severity = ERRORIDSEV(Perl_rc),		\
			Perl_rc = ERRORIDERROR(Perl_rc)) 
#define Acquire_hab() if (!Perl_HAB_set) {				\
	   Perl_hab = WinInitialize(0);					\
	   if (!Perl_hab) die("WinInitialize failed");			\
	   set_Perl_HAB_f;						\
	}

#define STATIC_FILE_LENGTH 127

#define PERLLIB_MANGLE(s, n) perllib_mangle((s), (n))
char *perllib_mangle(char *, unsigned int);

char *os2error(int rc);

/* ************************************************************ */
#define Dos32QuerySysState DosQuerySysState
#define QuerySysState(flags, pid, buf, bufsz) \
	Dos32QuerySysState(flags, 0,  pid, 0, buf, bufsz)

#define QSS_PROCESS	1
#define QSS_MODULE	4
#define QSS_SEMAPHORES	2
#define QSS_FILE	8		/* Buggy until fixpack18 */
#define QSS_SHARED	16

#ifdef _OS2EMX_H

APIRET APIENTRY Dos32QuerySysState(ULONG func,ULONG arg1,ULONG pid,
			ULONG _res_,PVOID buf,ULONG bufsz);
typedef struct {
	ULONG	threadcnt;
	ULONG	proccnt;
	ULONG	modulecnt;
} QGLOBAL, *PQGLOBAL;

typedef struct {
	ULONG	rectype;
	USHORT	threadid;
	USHORT	slotid;
	ULONG	sleepid;
	ULONG	priority;
	ULONG	systime;
	ULONG	usertime;
	UCHAR	state;
	UCHAR	_reserved1_;	/* padding to ULONG */
	USHORT	_reserved2_;	/* padding to ULONG */
} QTHREAD, *PQTHREAD;

typedef struct {
	USHORT	sfn;
	USHORT	refcnt;
	USHORT	flags1;
	USHORT	flags2;
	USHORT	accmode1;
	USHORT	accmode2;
	ULONG	filesize;
	USHORT  volhnd;
	USHORT	attrib;
	USHORT	_reserved_;
} QFDS, *PQFDS;

typedef struct qfile {
	ULONG		rectype;
	struct qfile	*next;
	ULONG		opencnt;
	PQFDS		filedata;
	char		name[1];
} QFILE, *PQFILE;

typedef struct {
	ULONG	rectype;
	PQTHREAD threads;
	USHORT	pid;
	USHORT	ppid;
	ULONG	type;
	ULONG	state;
	ULONG	sessid;
	USHORT	hndmod;
	USHORT	threadcnt;
	ULONG	privsem32cnt;
	ULONG	_reserved2_;
	USHORT	sem16cnt;
	USHORT	dllcnt;
	USHORT	shrmemcnt;
	USHORT	fdscnt;
	PUSHORT	sem16s;
	PUSHORT	dlls;
	PUSHORT	shrmems;
	PUSHORT	fds;
} QPROCESS, *PQPROCESS;

typedef struct sema {
	struct sema *next;
	USHORT	refcnt;
	UCHAR	sysflags;
	UCHAR	sysproccnt;
	ULONG	_reserved1_;
	USHORT	index;
	CHAR	name[1];
} QSEMA, *PQSEMA;

typedef struct {
	ULONG	rectype;
	ULONG	_reserved1_;
	USHORT	_reserved2_;
	USHORT	syssemidx;
	ULONG	index;
	QSEMA	sema;
} QSEMSTRUC, *PQSEMSTRUC;

typedef struct {
	USHORT	pid;
	USHORT	opencnt;
} QSEMOWNER32, *PQSEMOWNER32;

typedef struct {
	PQSEMOWNER32	own;
	PCHAR		name;
	PVOID		semrecs; /* array of associated sema's */
	USHORT		flags;
	USHORT		semreccnt;
	USHORT		waitcnt;
	USHORT		_reserved_;	/* padding to ULONG */
} QSEMSMUX32, *PQSEMSMUX32;

typedef struct {
	PQSEMOWNER32	own;
	PCHAR		name;
	PQSEMSMUX32	mux;
	USHORT		flags;
	USHORT		postcnt;
} QSEMEV32, *PQSEMEV32;

typedef struct {
	PQSEMOWNER32	own;
	PCHAR		name;
	PQSEMSMUX32	mux;
	USHORT		flags;
	USHORT		refcnt;
	USHORT		thrdnum;
	USHORT		_reserved_;	/* padding to ULONG */
} QSEMMUX32, *PQSEMMUX32;

typedef struct semstr32 {
	struct semstr *next;
	QSEMEV32 evsem;
	QSEMMUX32  muxsem;
	QSEMSMUX32 smuxsem;
} QSEMSTRUC32, *PQSEMSTRUC32;

typedef struct shrmem {
	struct shrmem *next;
	USHORT	hndshr;
	USHORT	selshr;
	USHORT	refcnt;
	CHAR	name[1];
} QSHRMEM, *PQSHRMEM;

typedef struct module {
	struct module *next;
	USHORT	hndmod;
	USHORT	type;
	ULONG	refcnt;
	ULONG	segcnt;
	PVOID	_reserved_;
	PCHAR	name;
	USHORT	modref[1];
} QMODULE, *PQMODULE;

typedef struct {
	PQGLOBAL	gbldata;
	PQPROCESS	procdata;
	PQSEMSTRUC	semadata;
	PQSEMSTRUC32	sem32data;
	PQSHRMEM	shrmemdata;
	PQMODULE	moddata;
	PVOID		_reserved2_;
	PQFILE		filedata;
} QTOPLEVEL, *PQTOPLEVEL;
/* ************************************************************ */

PQTOPLEVEL get_sysinfo(ULONG pid, ULONG flags);

#endif /* _OS2EMX_H */

