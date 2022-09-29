
/* unixperf - an x11perf-style Unix performance benchmark */

/* tests.c contains the unixperf test list */

#include "test_c4.h"
#include "test_cmd.h"
#include "test_cpu.h"
#include "test_des.h"
#include "test_fileio.h"
#include "test_ipc.h"
#include "test_jmp.h"
#include "test_misc.h"
#include "test_regex.h"
#include "test_rpc.h"
#include "test_stdio.h"
#include "test_vm.h"
#include "test_naming.h"

/* XXX Please avoid putting a colon in the test name since it confuses x11perfcomp's
   parsing of the results. */

Test tests[] = {
   /* test_cpu.c */
   { "-dmatrixmult",	"4 by 4 double matrix multiply",
                        NullInitProc, DoMatrixMultDouble, NullProc, NullProc,
			8000, TF_NONE, V1, FALSE },
   { "-fmatrixmult",	"4 by 4 float matrix multiply",
                        NullInitProc, DoMatrixMultFloat, NullProc, NullProc,
			8000, TF_NONE, V1, FALSE },
   { "-deeprecursion",	"recurse 250 simple function calls and return",
                        NullInitProc, DoDeepRecursion, NullProc, NullProc,
			2000, TF_NONE, V1, FALSE },
   { "-bcopy256",	"bcopy of 256 bytes",
                        NullInitProc, DoBcopy256, NullProc, NullProc,
			4000, TF_NONE, V1, FALSE },
   { "-bcopy16k",	"bcopy of 16 kilobytes",
                        NullInitProc, DoBcopy16K, NullProc, NullProc,
			1000, TF_NONE, V1, FALSE },
   { "-bcopy100k",	"bcopy of 100 kilobytes",
                        NullInitProc, DoBcopy100K, NullProc, NullProc,
			1000, TF_NONE, V1, FALSE },
   { "-bcopy1meg",	"bcopy of 1 megabyte",
                        NullInitProc, DoBcopyMeg, NullProc, NullProc,
			8, TF_NONE, V1, FALSE },
   { "-bcopy10meg",	"bcopy of 10 megabytes",
                        NullInitProc, DoBcopy10Meg, NullProc, NullProc,
			8, TF_NONE, V1, FALSE },
   { "-bcopy10mega",	"bcopy of 10 megabytes (cacheline-aligned)",
                        NullInitProc, DoBcopy10Mega, NullProc, NullProc,
			8, TF_NONE, V1, FALSE },
   { "-bcopy10megu",	"bcopy of 10 megabytes (word mis-aligned)",
                        NullInitProc, DoBcopy10Megu, NullProc, NullProc,
			8, TF_NONE, V1, FALSE },
   { "-bcopy10megudw",	"bcopy of 10 megabytes (dword mis-aligned)",
                        NullInitProc, DoBcopy10Megudw, NullProc, NullProc,
			8, TF_NONE, V1, FALSE },

   { "-bzero256",	"bzero of 256 bytes",
                        NullInitProc, DoBzero256, NullProc, NullProc,
			4000, TF_NONE, V1, FALSE },
   { "-bzero16k",	"bzero of 16 kilobytes",
                        NullInitProc, DoBzero16K, NullProc, NullProc,
			1000, TF_NONE, V1, FALSE },
   { "-bzero100k",	"bzero of 100 kilobytes",
                        NullInitProc, DoBzero100K, NullProc, NullProc,
			1000, TF_NONE, V1, FALSE },
   { "-bzero1meg",	"bzero of 1 megabyte",
                        NullInitProc, DoBzeroMeg, NullProc, NullProc,
			8, TF_NONE, V1, FALSE },
   { "-bzero10meg",	"bzero of 10 megabytes",
                        NullInitProc, DoBzero10Meg, NullProc, NullProc,
			8, TF_NONE, V1, FALSE },
   { "-bzero10mega",	"bzero of 10 megabytes (cacheline-aligned)",
                        NullInitProc, DoBzero10Mega, NullProc, NullProc,
			8, TF_NONE, V1, FALSE },
   { "-bzero10megu",	"bzero of 10 megabytes (word mis-aligned)",
                        NullInitProc, DoBzero10Megu, NullProc, NullProc,
			8, TF_NONE, V1, FALSE },
   { "-bzero10megudw",	"bzero of 10 megabytes (dword mis-aligned)",
                        NullInitProc, DoBzero10Megudw, NullProc, NullProc,
			8, TF_NONE, V1, FALSE },
   { "-qsort400",	"bcopy then qsort 400 32-bit integers",
                        InitQsort400, DoQsort, NullProc, CleanupQsort,
			100, TF_NONE, V1, FALSE },
   { "-qsort16k",	"bcopy then qsort 16,000 32-bit integers",
                        InitQsort16K, DoQsort, NullProc, CleanupQsort,
			3, TF_NONE, V1, FALSE },
   { "-qsort64k",	"bcopy then qsort 64,000 32-bit integers",
                        InitQsort64K, DoQsort, NullProc, CleanupQsort,
			1, TF_NONE, V1, FALSE },
   { "-ftrig",		"basic float trigonometric function (sinf/cosf/tanf)",
                        NullInitProc, DoTrigFuncsFloat, NullProc, NullProc,
			75000, TF_NONE, V1, FALSE },
   { "-dtrig",		"basic double trigonometric function (sin/cos/tan)",
                        NullInitProc, DoTrigFuncsDouble, NullProc, NullProc,
			75000, TF_NONE, V1, FALSE },
   { "-dhrystone",	"Dhrystone 2 (from Byte Unix benchmark)",
                        InitDhrystone, DoDhrystone, NullProc, CleanupDhrystone,
			15000, TF_NONE, V1, FALSE },
   { "-hanoi10", 	"Tower of Hanoi recursion test (10 deep)",
                        NullInitProc, DoHanoi10, NullProc, NullProc,
			800, TF_NONE, V1, FALSE },
   { "-hanoi15", 	"Tower of Hanoi recursion test (15 deep)",
                        NullInitProc, DoHanoi15, NullProc, NullProc,
			1, TF_NONE, V1, FALSE },

   /* test_des.c */
   { "-des",		"encrypt/decrypt of 64-bit block using Levy88 fast DES",
                        InitDesCrypt, DoDesCrypt, NullProc, NullProc,
			400, TF_NONE, V1, FALSE },

   /* test_fileio.c */
   { "-writelog",	"write 80 bytes to a log file",
                        InitWriteLog, DoWriteLog, NullProc, CleanupWriteLog,
			800, TF_NONE, V1, FALSE },
   { "-createunlink",	"per-file cost of creating 100 files then unlinking them all",
                        InitSetupDir, DoCreateAndUnlink,
                        NullProc, CleanupSetupDir,
			10, TF_NONE, V1, FALSE },
   { "-openclose",	"per-file cost of open and immediate close of 100 files",
                        InitSetupFiles, DoOpenAndClose,
                        NullProc, CleanupSetupFiles,
			10, TF_NONE, V1, FALSE },
   { "-chmod",		"per-chmod cost of chmod permissions toggle of 100 files",
                        InitSetupFiles, DoChmod,
                        NullProc, CleanupSetupFiles,
			10, TF_NONE, V1, FALSE },
   { "-stat",		"cost of stat'ing current dir",
                        InitSetupDir, DoStat,
                        NullProc, CleanupSetupDir,
			10, TF_NONE, V1, FALSE },
   { "-statlong",	"cost of stat'ing 5 component path",
                        InitSetupDirLong, DoStatLong,
                        NullProc, CleanupSetupDirLong,
			10, TF_NONE, V1, FALSE },
   { "-statdir",	"open directory and stat 100 files in it",
                        InitSetupFiles, DoStatInDir,
                        NullProc, CleanupSetupFiles,
			10, TF_NONE, V1, FALSE },
   { "-mkdirrmdir",	"per-dir cost of making 100 dirs then rmdiring them all",
                        InitSetupDir, DoMkdirAndRmdir,
                        NullProc, CleanupSetupDir,
			10, TF_NONE, V1, FALSE },
   { "-dbmlookup",	"ndbm lookup into database with 900 entries",
                        InitDbmLookup, DoDbmLookup, NullProc, CleanupDbmLookup,
			2000, TF_NONE, V1, FALSE },

   /* test_ipc.c */
   { "-pipeping1",	"1 byte token passing between two processes using two pipes",
                        InitPipePing1, DoPipePing, NullProc, CleanupPipePing,
			1000, TF_NONE, V1, FALSE },
   { "-pipeping1k",	"1 kilobyte token passing between two processes using two pipes",
                        InitPipePing1K, DoPipePing, NullProc, CleanupPipePing,
			1000, TF_NONE, V1, FALSE },
   /* XXX the -tcpecho and -tcpspawn calibrate number is large on purpose to
      force lots of TCP connections in TIME WAIT state.  */
   { "-tcpecho",	"server echoes byte using local TCP during TIME WAIT backlog",
			InitStandaloneServerTCP, DoEchoClientTCP, NullProc, CleanupServerTCP,
			4000, TF_NONSTANDARD, V1, FALSE },
			/* tcpecho is NONSTANDARD because it stresses TCP socket creation. */
   { "-tcpspawn",	"server spawns child to echo byte using local TCP during TIME WAIT backlog",
			InitSpawningServerTCP, DoEchoClientTCP, NullProc, CleanupServerTCP,
			4000, TF_NONSTANDARD, V1, FALSE },
			/* tcpspawn is NONSTANDARD because it stresses TCP socket creation. */

   /* test_jmp.c */
   { "-setjmp", 	"setjmp/longjmp",
                        NullInitProc, DoSetjmp, NullProc, NullProc,
			4000, TF_NONE, V1, FALSE },
   { "-sigsetjmp", 	"sigsetjmp/siglongjmp (saving signal mask)",
                        NullInitProc, DoSigsetjmp, NullProc, NullProc,
			4000, TF_NONE, V1, FALSE },

   /* test_rpc.c */
   /* all of the RPC tests are NOMULTI because only one process can
      bind the RPC program number */
   { "-tcp_createrpc", 	"create/destroy client TCP-based SunRPC handle",
                        InitSunRPC, DoCreateDestroyTcpRpcHnd, NullProc, CleanupSunRPC,
			600, TF_NONSTANDARD|TF_NOMULTI, V1, FALSE },
			/* tcp_createrpc is NONSTANDARD because it stresses TCP socket creation. */
   { "-udp_createrpc", 	"create/destroy client UDP-based SunRPC handle",
                        InitSunRPC, DoCreateDestroyUdpRpcHnd, NullProc, CleanupSunRPC,
			600, TF_NOMULTI, V1, FALSE },
   { "-tcptinyrpc", 	"TCP-based SunRPC to negate an integer",
                        InitTcpSunRPC, DoTinySunRPC, NullProc, CleanupSunRPC,
			400, TF_NOMULTI, V1, FALSE },
   { "-udptinyrpc", 	"UDP-based SunRPC to negate an integer",
                        InitUdpSunRPC, DoTinySunRPC, NullProc, CleanupSunRPC,
			400, TF_NOMULTI, V1, FALSE },
   { "-tcpnullrpc", 	"TCP-based SunRPC no-op",
                        InitTcpSunRPC, DoNullSunRPC, NullProc, CleanupSunRPC,
			400, TF_NOMULTI, V1, FALSE },
   { "-udpnullrpc", 	"UDP-based SunRPC no-op",
                        InitUdpSunRPC, DoNullSunRPC, NullProc, CleanupSunRPC,
			400, TF_NOMULTI, V1, FALSE },

   /* test_misc.c */
   { "-getpid",		"getpid system call",
                        NullInitProc, DoGetPid, NullProc, NullProc,
			1000, TF_NONE, V1, FALSE },
   { "-ezsyscalls",	"easy system calls, ie. dup/close/getpid/getuid/umask",
                        NullInitProc, DoEzSysCalls, NullProc, NullProc,
			1000, TF_NONE, V1, FALSE },
   { "-gettimeofday",	"gettimeofday system call",
                        NullInitProc, DoGetTimeOfDay, NullProc, NullProc,
			1000, TF_NONE, V1, FALSE },
   { "-forkwait",	"fork, child immediately exits, parent waits",
                        NullInitProc, DoForkAndWait, NullProc, NullProc,
			50, TF_NONE, V1, FALSE },
   { "-sleep1",		"sleep for one second (validates test time)",
                        NullInitProc, DoSleep1, NullProc, NullProc,
			1, TF_NONE, V1, FALSE },
   { "-select0",	"select on no fds with no timeout",
			InitSelect0, DoSelect, NullProc, CleanupSelect,
			400, TF_NONE, V1, FALSE },
   { "-select16r",	"select on 16 pipe read fds with immediate timeout",
			InitSelect16R, DoSelect, NullProc, CleanupSelect,
			400, TF_NONE, V1, FALSE },
   { "-select32r",	"select on 32 pipe read fds with immediate timeout",
			InitSelect32R, DoSelect, NullProc, CleanupSelect,
			400, TF_NONE, V1, FALSE },
   { "-select16w",	"select on 16 pipe write fds with immediate timeout",
			InitSelect16W, DoSelect, NullProc, CleanupSelect,
			400, TF_NONE, V1, FALSE },
   { "-select32w",	"select on 32 pipe write fds with immediate timeout",
			InitSelect32W, DoSelect, NullProc, CleanupSelect,
			400, TF_NONE, V1, FALSE },
   { "-select64rw",	"select on 64 pipe read & 64 pipe write fds with immediate timeout",
			InitSelect64RW, DoSelect, NullProc, CleanupSelect,
			400, TF_NONE, V1, FALSE },

   /* test_stdio.c */
   { "-popenecho",	"popen and read from \"/bin/echo hello\"",
			InitPopenEchoHello, DoPopenEchoHello, NullProc, NullProc,
			40, TF_NONE, V1, FALSE },

   /* test_vm.c */
   { "-devzeromap",	"mmap megabyte /dev/zero region ",
                        InitDevZero, DoDevZeroMap, NullProc, CleanupDevZero,
			10, TF_NONE, V1, FALSE },
   { "-devzerowalk",	"touch newly mmaped megabyte /dev/zero region at every 1024 bytes",
                        InitDevZero, DoDevZeroWalk, NullProc, CleanupDevZero,
			10, TF_NONE, V1, FALSE },
   { "-mmapmegfile",	"copy every byte of newly mmaped megabyte file",
                        InitMmapMegabyteFile, DoMmapMegabyteFile, NullProc, CleanupMmapMegabyteFile,
			5, TF_NONE, V1, FALSE },

   /* test_cmd.c */
   { "-compress",	"compress & uncompress 52 kilobyte text file of numbers",
                        InitCompress, DoCompress, NullProc, CleanupCompress,
			2, TF_NONE, V1, FALSE },
   { "-compile",	"compile ANSI C `hello world' with no special options",
                        InitCompile, DoCompile, NullProc, CleanupCompile,
			2, TF_NONE, V1, FALSE },
   { "-dcsqrt2x1",	"use dc to output sqrt(2) to 500 digits, 1 job parallel",
                        NullInitProc, DoDcSqrt2x1, NullProc, NullProc,
			3, TF_NONE, V1, FALSE },
   { "-dcsqrt2x2",	"use dc to output sqrt(2) to 500 digits, 2 job parallel",
                        NullInitProc, DoDcSqrt2x2, NullProc, NullProc,
			3, TF_NONE, V1, FALSE },
   { "-dcsqrt2x4",	"use dc to output sqrt(2) to 500 digits, 4 job parallel",
                        NullInitProc, DoDcSqrt2x4, NullProc, NullProc,
			3, TF_NONE, V1, FALSE },
   { "-dcsqrt2x8",	"use dc to output sqrt(2) to 500 digits, 8 job parallel",
                        NullInitProc, DoDcSqrt2x8, NullProc, NullProc,
			3, TF_NONE, V1, FALSE },
   { "-dcsqrt2x12",	"use dc to output sqrt(2) to 500 digits, 12 job parallel",
                        NullInitProc, DoDcSqrt2x12, NullProc, NullProc,
			3, TF_NONE, V1, FALSE },
   { "-dcsqrt2x16",	"use dc to output sqrt(2) to 500 digits, 16 job parallel",
                        NullInitProc, DoDcSqrt2x16, NullProc, NullProc,
			3, TF_NONE, V1, FALSE },
   { "-dcfact",		"use dc to output first 1,000 factorials",
                        NullInitProc, DoDcFactorial, NullProc, NullProc,
			1, TF_NONE, V1, FALSE },

   /* test_c4.c */
   { "-c4",		"Connect 4 game boards evaluated during alpha-beta search",
                        NullInitProc, DoConnect4, NullProc, NullProc,
			1, TF_NOWARMUP, V1, FALSE },

#ifndef HAS_NO_REGEX
   /* test_regex.c */
   { "-longlitregcomp",	"use regcomp to compile 51 char literal, then regfree",
                        NullInitProc, DoLongLiteralStrRegcomp, NullProc, NullProc,
			40, TF_NONE, V1, FALSE },
   { "-longlitregexec",	"use regexec with 51 char literal to search 142 kilobyte text",
                        InitLongLiteralStrRegex, DoPosixTxtRegexec, NullProc, CleanupRegex,
			1, TF_NONE, V1, FALSE },
   { "-longdotregcomp",	"use regcomp to compile long .* pattern, then regfree",
                        NullInitProc, DoLongDotStrRegcomp, NullProc, NullProc,
			40, TF_NONE, V1, FALSE },
   { "-longdotregexec",	"use regexec with long .* pattern to search 142 kilobyte text",
                        InitLongDotStrRegex, DoPosixTxtRegexec, NullProc, CleanupRegex,
			1, TF_NONE, V1, FALSE },
#endif

   /* test_naming.c */
   { "-getpwnam",       "use getpwnam() to get root password entry",
			NullInitProc, DoGetpwnam, NullProc, NullProc,
			1, TF_NONE, V1, FALSE },
   { "-getpwuid",       "use getpwuid() to get root password entry",
			NullInitProc, DoGetpwuid, NullProc, NullProc,
			1, TF_NONE, V1, FALSE },
   { "-getpwentall",    "use getpwent() to scan the entire password database",
			NullInitProc, DoGetpwentAll, NullProc, NullProc,
			1, TF_NONSTANDARD, V1, FALSE },
   /* -getpwentall is NONSTANDARD because it might yield varying
      results depending on system configuration */
   { "-gethostbyname",  "use gethostbyname() to lookup localhost",
			NullInitProc, DoGethostbyname, NullProc, NullProc,
			1, TF_NONE, V1, FALSE },
};

int NumberOfTests = sizeof(tests)/sizeof(Test);

char *bcopyAlias[] = { "-bcopy256", "-bcopy16k", "-bcopy100k", "-bcopy1meg", "-bcopy10meg", "-bcopy10mega", "-bcopy10megu", "-bcopy10megudw", NULL };
char *bzeroAlias[] = { "-bzero256", "-bzero16k", "-bzero100k", "-bzero1meg", "-bzero10meg", "-bzero10mega", "-bzero10megu", "-bzero10megudw", NULL };
char *qsortAlias[] = { "-qsort400", "-qsort16k", "-qsort64k", NULL };
char *trigAlias[] = { "-ftrig", "-dtrig", NULL };
char *jmpAlias[] = { "-setjmp", "-sigsetjmp", NULL };
char *rpcAlias[] = { "-udp_createrpc", "-tcptinyrpc", "-udptinyrpc", "-tcpnullrpc", "-udpnullrpc", NULL };
char *tcpAlias[] = { "-tcpecho", "-tcpspawn", NULL };
char *pipeAlias[] = { "-pipeping1", "-pipeping1k", NULL };
char *devZeroAlias[] = { "-devzeromap", "-devzerowalk", NULL };
char *selectAlias[] = { "-select0", "-select16r", "-select32r", "-select16w", "-select32w", "-select64rw", NULL };
char *recurseAlias[] = { "-deeprecursion", "-hanoi10", "-hanoi15", NULL };
char *jobsAlias[] = { "-dcsqrt2x1", "-dcsqrt2x2", "-dcsqrt2x4", "-dcsqrt2x8", "-dcsqrt2x12", "-dcsqrt2x16", NULL };
#ifndef HAS_NO_REGEX
char *regexecAlias[] = { "-longlitregcomp", "-longlitregexec", "-longdotregcomp", "-longdotregexec", NULL };
#endif
char *getpwAlias[] = { "-getpwnam", "-getpwuid", "-getpwentall", NULL };
char *gethostAlias[] = { "-gethostbyname", NULL };

Alias aliases[] = {
   { "-bcopy", bcopyAlias },
   { "-bzero", bzeroAlias },
   { "-qsort", qsortAlias },
   { "-trig", trigAlias },
   { "-jmp", jmpAlias },
   { "-tcp", tcpAlias },
   { "-rpc", rpcAlias },
   { "-pipe", pipeAlias },
   { "-devzero", devZeroAlias },
   { "-select", selectAlias },
   { "-recurse", recurseAlias },
   { "-jobs", jobsAlias },
#ifndef HAS_NO_REGEX
   { "-regexec", regexecAlias },
#endif
   { "-getpw", getpwAlias },
   { "-gethost", gethostAlias },
};

int NumberOfAliases = sizeof(aliases)/sizeof(Alias);

