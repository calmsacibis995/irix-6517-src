/*
 * salink.c - routines for machines that link with libsa.a and are not MP
 *
 */
#ident "symmon/salink.c: $Revision: 1.113 $"

struct sockaddr;		/* needed to coax compiler on stubs */

#include <fault.h>
#include <sys/sbd.h>
#include <arcs/io.h>
#include <arcs/spb.h>
#include <arcs/debug_block.h>
#include <arcs/signal.h>
#include "dbgmon.h"
#include "mp.h"
#include "parser.h"
#include <libsc.h>
#include <libsk.h>

#if EVEREST
#include "sys/loaddrs.h"
#include "sys/EVEREST/gda.h"
#endif

#if SN0
#include <pgdrv.h>
#include "sys/loaddrs.h"
#include "sys/SN/gda.h"
#include "sys/SN/addrs.h"
#include "sys/SN/nmi.h"
#endif

#if IP28
#include "sys/mc.h"
#endif

#if IP28 || IP30 
#include "sys/RACER/gda.h"
#endif

#if IP32
#define K0_RAMBASE 0x80000000
#include "sys/gda_ip32.h"
#endif

void main_intr (void);
extern int cpu_probe(void);
extern char *getversion(void); 

#ifdef NETDBX
#include <errno.h>
#include <net/in.h>
#include <saioctl.h>
#include "coord.h"

int	_nwkopen(int, char *[], char *[], struct cmd_table *);
int	_nwkread(int, char *[], char *[], struct cmd_table *);
int	_nwkwrite(int, char *[], char *[], struct cmd_table *);
int	_nwkclose(int, char *[], char *[], struct cmd_table *);
int	_drdebug(int, char *[], char *[], struct cmd_table *);
int	_showtrace(int, char *[], char *[], struct cmd_table *);
#endif /* NETDBX */

#ifdef MULTIPROCESSOR
int	_autostop(int, char *[], char *[], struct cmd_table *);
#endif /* MULTIPROCESSOR */

int	_doadump(int, char *[], char *[], struct cmd_table *);

/*
 * commands to be included in ctbl
 */
/*
 * ctbl -- interface between parser and command execution routines
 * Add new commands by making entry here.
 */
struct cmd_table ctbl[] = {
	{ "?",		help,		"help:\t\t? [COMMAND]" },
#ifdef NETDBX
	{ "nwkopen", 	_nwkopen,	"nwk:\ttest network interface" },
	{ "nwkread", 	_nwkread,	"nwkread:\ttest network interface" },
	{ "nwkwrite", 	_nwkwrite,	"nwkwrite:\ttest network interface" },
	{ "nwkclose", 	_nwkclose,	"nwkclose:\ttest network interface" },
	{ "rdebug", 	_drdebug,	"rdebug:\tDebug Remote Debugging" },
#ifdef MULTIPROCESSOR
	{ "showtrace", 	_showtrace,	"Debug Remote Debugging: showtrace" },
#endif /* MULTIPROCESSOR */
#endif /* NETDBX */
#ifdef MULTIPROCESSOR
	{ "autostop",	_autostop,	"auto stop:\tautostop [0|1]" },
#endif
	{ "brk",	_brk,		"breakpoint:\tbrk [ADDRLIST]" },
	{ "bt",		_dobtrace,	"backtrace:\tbt [MAX_FRM]" },
	{ "c",		_cont,		"continue:\tc [CPUNUM|all]" },
	{ "cacheflush",	_cacheflush,	"cache flush:\tcacheflush [RANGE]" },
	{ "call",	_call,		"call:\t\tcall ADDR [ARGLIST]" },
	{ "clear",	clear,		"clear screen:\tclear" },
	{ "debug",	_rdebug,	"debug:\t\tdebug [-r] CHAR_DEVICE" },
	{ "dis",	_disass,	"disassemble:\tdis [RANGE]" },
	{ "doadump",    _doadump,       "kernel dump:\tdoadump" },
	{ "dump",	_dump,		"dump memory:\tdump [-(b|h|w|d)] "
					"[-(o|t|x|c)] [RANGE]" },
	{ "g",		_get,		"get memory:\tg [-(b|h|w|d)] "
					"ADDRESS_OR_REGISTER" },
	{ "goto",	_go_to,		"goto:\t\tgoto ADDR" },
	{ "help",	help,		"help:\t\thelp [COMMAND]" },
	{ "hx",		nmtohx,		"name to hex:\thx name" },
	{ "kp",		_kp,		"kernel print:\tkp FUNC_NAME "
					"[FUNC_ARGS]" },
	{ "lkaddr",	lkaddr,		"lookup addr:\tlkaddr [ADDR]" },
	{ "lkup",	lkup,		"lookup str:\tlkup STRING" },
#if DEBUG
	{ "mfind",      mfind,          "mfind:\t\tmfind [-b|h|w|d] base#cnt [-s STRING|VALUE]" },
#endif
	{ "nm",		hxtonm,		"hex to name:\tnm addr" },
	{ "p",		_put,		"put memory:\tp [-(b|h|w|d)] "
					"ADDRESS_OR_REGISTER VALUE"},
	{ "px",		_calc,		"print expr:\tpx cexp" },
	{ "quit",	_quit,		"quit:\t\tquit" },
	{ "r",		printregs,	"print regs:\tr [-(u|s|a)]: "
					"display (user|sys|all) registers"},
	{ "s",		_step,		"step instr:\ts [COUNT]" },
	{ "S",		_Step,		"step call:\tS [COUNT]" },
	{ "string",	_string,	"string:\t\tstring ADDR [MAXLEN]" },
	{ "tlbdump",	_tlbdump,	"tlbdump:\ttlbdump [-v] [-p pid] [RANGE]" },
	{ "tlbflush",	_tlbflush,	"tlbflush:\ttlbflush [RANGE]" },
	{ "tlbpid",	_tlbpid,	"tlbpid:\t\ttlbpid [PID]" },
#if R4000 || R10000
	{ "tlbmap",	_tlbmap,	"tlbmap:\t\ttlbmap" },
	{ "tlbpfntov",	_tlbpfntov,	"tlbpfntov:\ttlbpfntov PFN" },
#elif TFP
	{ "tlbmap",	_tlbmap,	"tlbmap:\t\ttlbmap [-c cachalg] "
					"[-(v|d)]* VADDR PFN ASID"},
	{ "tlbptov",	_tlbptov,	"tlbptov:\ttlbptov ADDR" },
	{ "tlbx",	_tlbx,		"tlbx:\ttlbx [RANGE]" },
#endif
	{ "tlbvtop",	_tlbvtop,	"tlbvtop:\ttlbvtop ADDR [PID]" },
	{ "unbrk",	_unbrk,		"remove bkpt:\tunbrk [BPNUMLIST]" },
#if R4000 || R10000
	{ "wpt",	_wpt,		"watchpt:\twpt [r|w|rw] [physaddr]" },
	{ "cidx",	_c_tags,
			"cidx:\t\tcidx [-(i|d|s)] slotnumber [COUNT]" },
	{ "caddr",	_c_tags,
			"caddr:\t\tcaddr [-(i|d|s|a)] K0ADDR [RANGE]" },
	{ "cecc",	_c_tags,
			"cecc:\t\tcecc [-(i|d|s)] K0ADDR [RANGE]" },
#if R10000
	{ "scache",	dumpSecondaryCache, "scache:\t\tdump scache " },
	{ "dcache",	dumpPrimaryCache, "dcache:\t\tdump dcache " },
	{ "asline",	_dump_sline, "asline:\t\tasline K0ADDR " },
	{ "adline",	_dump_dline, "adline:\t\tadline K0ADDR " },
	{ "ailine",	_dump_iline, "ailine:\t\tailine K0ADDR " },
#ifdef EVEREST
	{ "ct",		_dump_ct, "ct:\t\tCompare CC/T5 2ndary tags " },
	{ "cta",	_dump_cta, "cta::\t\tCompare all CC/T5 2ndary tags " },
	{ "cts",	_dump_cts, "cts:\t\tCompare CC/T5 2ndary tags - strict" },
	{ "ctas",	_dump_ctas, "ctas:\t\tCompare all CC/T5 2ndary tags - strict" },
#endif /* EVEREST */
#endif /* R10000 */
#endif /* R4000 || R10000 */
	{ 0,		0,		"" }
};

/* ARGSUSED */
int
_doadump(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
    char *strs[2];
    strs[0] = "doadump";
    strs[1] = 0;
    return _kpcheck(1, strs, 1);
}

char *debug_argv[] = {
	"debug",
	"serial(1)",
	0
};
#define	DEBUG_ARGC	(sizeof(debug_argv)/sizeof(debug_argv[0]) - 1)
extern int Debug;

db_t default_db;

#ifdef NETDBX
ULONG nwkfd;

#ifdef SN0
#ifdef SN_PDI
static char *NetworkName = "network(0)" ;
#else
static char *NetworkName = 
"/xwidget/master_bridge/pci/master_ioc3/ef/" ;
#endif
#else
static char *NetworkName = "network(0)";
#endif

/* ARGSUSED */
int
_nwkopen(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	ULONG 	err;

	if (argc != 1)
		printf("Usage:  nwk\n");

	Debug = 1;
	if  ((err = Open(NetworkName, OpenReadWrite, &nwkfd)) != ESUCCESS) {
		printf("Error in opening file %s (err = %d)\n",NetworkName,err);
		Debug = 0;
		return((int) err);
	}
	Debug = 0;
	if  ((err = Bind(nwkfd, (u_short) 3152)) != ESUCCESS) {
		printf("Error in binding (err = %d)\n",err);
		return((int) err);
	}
	printf("Bind successful\n");
	if  ((err = ioctl(nwkfd, NIOCREADANY, 1)) != ESUCCESS) {
		printf("Error in ioctl (NIOCREADANY) (err = %d)\n",err);
		return((int) err);
	}
	printf("Ioctl (NIOCREADANY) successful\n");
	return(0);
}

#ifdef MULTIPROCESSOR
/* ARGSUSED */
int
_showtrace(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	extern void showtrace(void);

	if (argc != 1)
		printf("Usage:  showtrace\n");
	showtrace();
	return 0;
}
#endif /* MULTIPROCESSOR */

/* ARGSUSED */
int
_nwkread(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	ULONG 	cnt,err;
	char 	buf[81];
	struct sockaddr_in	saddr;

	if (argc != 1)
		printf("Usage:  nwkread\n");

	Debug = 1;
	cnt = 0;
	while (cnt == 0) {
		if ((err = Read(nwkfd, buf, 80, &cnt)) != ESUCCESS) {
			printf("Error in reading file (err = %d)\n", err);
			return((int) err);
		}
	}
	Debug = 0;
	if (cnt > (ULONG) 0)
		buf[cnt] = NULL;
	printf("%s", buf);
	printf("Attempting to get source ...\n");
	GetSource(nwkfd, &saddr);
	BindSource(nwkfd, saddr);
	printf("Source is bound.\n");
	return 0;
}

/* ARGSUSED */
int
_nwkwrite(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	ULONG 	cnt,err;
	char 	buf[81];

	if (argc != 1)
		printf("Usage:  nwkwrite\n");

	Debug = 1;
	while (1) {
		printf("INPUT: ");
		gets(buf);
		if ((buf[0] == 'e') && (buf[1] == 'n') && (buf[2] == 'd'))
			break;

		if ((err = Write(nwkfd, buf, 80, &cnt)) != ESUCCESS) {
			printf("Error in reading file (err = %d)\n", err);
			return((int) err);
		}
	}
	Debug = 0;
	return 0;
}

/* ARGSUSED */
int
_nwkclose(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	ULONG 	err;

	if (argc != 1)
		printf("Usage:  nwkclose\n");

	Debug = 1;
	if  ((err = Close(nwkfd)) != ESUCCESS) {
		printf("Error in closing file %s (err = %d)\n",NetworkName,err);
		Debug = 0;
		return((int) err);
	}
	Debug = 0;
	return 0;
}

/* ARGSUSED */
int
_drdebug(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
#ifdef MULTIPROCESSOR
	extern int	nwkdown;
	extern int 	lastturn;

	int		i;
	int		vid_me = cpuid();

	if (argc != 1) {
		printf("Usage:  drdebug\n");
		return(-1);
	}
	printf("Remote Debug State:\n");
	printf("\t Kernel's Network = %s\n", nwkdown ? "DOWN" : "UP");
	printf("\t Symmon's Network = %s\n", nwkopen ? "OPEN" : "CLOSED");
	printf("\t Lastturn = %d\n", lastturn);
	printf("\t Symmon Owner = %d\n", symmon_owner);
	printf("\t No. of Cpus = %d\n", ncpus);
	for (i = 0; i < ncpus; i++)
		printf("cpu %d [%s,0x%llx,%s] ", i, itocs(getcpustate(i)),
			brkptaddr[i], ((i == vid_me) && (safely_check_zingbp() == 0)) ? "Zing" : "Unknown");
	printf("\n");
#endif
	return 0;
}
#endif /* NETDBX */

#ifdef SN0
void
init_slave_debugblock_ptr()
{
	/* symmon data initialization */

	_init_spb();

	if (GDEBUGBLOCK == 0)
		SDEBUGBLOCK(&default_db);
}
#endif /* SN0 */

/*
 * _dbgmon -- called from csu
 */
void
_dbgmon(int argc, char *argv[], char *cl_environ[])
{
	db_t *db;
	__psint_t entry_pc = 0;
#if defined (SN0)
	nmi_t	*nmi_addr;
	extern void setup_nmi_handler(void (*)());
	extern nasid_t compact_to_nasid_node[];
	extern void elscuart_setup(void);
#endif /* SN0 */
	
	extern void symmon_brkhandler();
#if defined(EVEREST) && defined(MULTIKERNEL)
	extern void symmon_exceptnorm();
#endif

	/*
	 * Don't mess with things in uncached space (e.g., SPB) without
	 * flushing first.
	 */
	flush_cache();
	
	/* symmon data initialization */
	if (GDEBUGBLOCK == 0)
		SDEBUGBLOCK(&default_db);
	db = (db_t *)GDEBUGBLOCK;
	db->db_bpaddr = symmon_brkhandler;
#if defined(EVEREST) && defined(MULTIKERNEL)
	db->db_excaddr = symmon_exceptnorm;
#endif
	db->db_magic = DB_MAGIC;

#if EVEREST || SN0
	{
		extern void symmon_nmi();

		/* 
		 * Initialize NMI vector.  Prom jumps to the location
		 * specified here when it receives an NMI.
		 */
#if EVEREST
		if (IS_KSEG1(symmon_nmi))
			GDA->g_nmivec = KPHYSTO32K1(symmon_nmi);
		else
			GDA->g_nmivec = KPHYSTO32K0(symmon_nmi);
#endif
#if SN0
		setup_nmi_handler(symmon_nmi);
#endif /* SN0 */

	}
#endif
#if IP28 || IP30 || IP32
	{
		extern void symmon_nmi();

                /*
                 * Initialize NMI vector.  Prom jumps to the location
                 * specified here when it receives an NMI.
                 */
		GDA->g_nmivec = (__psunsigned_t)symmon_nmi;
		
#if IP32
                GDA->g_magic = GDA_MAGIC;
                GDA->g_nmi_sr_save = 0;
                GDA->g_nmi_epc_save = 0;
#endif
	}
#endif

	idbg_cleanup();

	setscalable(_MIPS_SZPTR/sizeof(char));

#if MULTIPROCESSOR
	init_mp();
#endif /* MULTIPROCESSOR */
#if SN0 || IP30
	{
	extern int cons_lock();

	db->db_conslock = cons_lock;
	}
#else
	db->db_conslock = 0;
#endif

#ifndef SABLE
	/* set texport to autowrap */
	if (isgraphic(StandardOut)) {
		ULONG rc;
		Write(StandardOut,"\033[37h",5,&rc);
	}
#endif

	if (setjmp(private._restart_buf)) {
	    putchar('\n');
	    goto command_mode;
	}

	atob(getenv("DEBUG"), &Debug);
	atob_ptr((char *)getargv("startpc"), &entry_pc);

	if (getenv("dbgtty")) {
	    ULONG dummyfd;
	    /* Change stdio to alternate port */
	    (void)Close(0);
	    (void)Close(1);
	    (void)Open(getenv("dbgtty"), OpenReadOnly, &dummyfd);
	    (void)Open(getenv("dbgtty"), OpenWriteOnly, &dummyfd);
	}

	if (getenv("rdebug")) {
	    printf("\nRemote debugging mode\n");
	    private.regs[R_EPC] = entry_pc;
	    private.regs[R_A0] = (k_machreg_t) argc;
	    private.regs[R_A1] = (k_machreg_t) argv;
	    private.regs[R_A2] = (k_machreg_t) cl_environ;
	    private.regs[R_A3] = (k_machreg_t) entry_pc;
	    _rdebug(DEBUG_ARGC, debug_argv, 0, 0);
#ifdef NETDBX
	} else if (getenv("netrdebug")) {
	    printf("\nRemote debugging mode over Ethernet\n");
	    netrdebug = 1;
	    private.regs[R_EPC] = entry_pc;
	    private.regs[R_A0] = (k_machreg_t) argc;
	    private.regs[R_A1] = (k_machreg_t) argv;
	    private.regs[R_A2] = (k_machreg_t) cl_environ;
	    private.regs[R_A3] = (k_machreg_t) entry_pc;
	    if (getenv("keepnwkdown")) {
		printf("Symmon:Will assume exclusive ownership of network\n");
		keepnwkdown = 1;
	    }
	    _rdebug(DEBUG_ARGC, debug_argv, 0, 0);
#endif /* NETDBX */
	} else if (entry_pc) {
#if _MIPS_SIM == _ABI64
	    printf("\nSymbolic Debugger %s - 64 Bit\n", getversion());
#else
	    printf("\nSymbolic Debugger %s\n", getversion());
#endif
	    printf("Entering client program at 0x%x\n",entry_pc);
	    invoke((inst_t *)entry_pc, argc, (long)argv, (long)cl_environ,
		   entry_pc, 0,0,0,0);
	}

command_mode:
	Signal (SIGINT, main_intr);

	symmon_spl();
	command_parser(ctbl, PROMPT, 0, 0);
}

/*
 * where symmon goes on a sigint
 */
void
main_intr (void)
{
    private.dbg_mode = MODE_DBGMON;
    longjmp (private._restart_buf, 1);
}

/* ARGSUSED */
extern nasid_t master_nasid;
void
mpsetup(unsigned int *ep)
{
	(void)cpu_probe();

	/* set up symmon half of gpda */
	init_symmon_gpda();

#if MULTIPROCESSOR
	/* 
	 * Copy exception code down to exception vector locations.
	 * The client may overwrite these with its own handlers.  If
	 * so, he must arrange for the handler stub located at the
	 * vector location to jump to the location of the actual
	 * handler.  It should store the location of the actual
	 * handler in a processor-private data area which is mapped
	 * to the same K2-seg address on all processors.  He should
	 * then store the address of the processor-private pointer
	 * in the restart block.
	 *
	 * The debugger then interacts with the client merely by
	 * changing the current value in the processor-private data
	 * area.  Note that one cannot take breakpoints between the
	 * time the client copies down its exception vectors and the
	 * time the processor-private data area and restart block
	 * pointers are established.
	 */

	_default_exceptions();
#else
	/* copy exception code down to UT_VEC, E_VEC */

	/* by doing these in reverse order, we set the initial remembered
	 * exceptions to our own.
	 */
	_hook_exceptions();
	_save_exceptions();
#endif	/* EVEREST || SN0 */
}

#if defined (SN0)
void
setup_nmi_handler(void (*func)())
{
	nmi_t *nmi_addr;

	nmi_addr = (nmi_t *)NMI_ADDR(get_nasid(), 
				     *LOCAL_HUB(PI_CPU_NUM));
	nmi_addr->magic = NMI_MAGIC;
	nmi_addr->call_addr = (addr_t *)(func);
	nmi_addr->call_addr_c = 
	    (addr_t *)(~((__psunsigned_t)(nmi_addr->call_addr)));
	nmi_addr->call_parm = 0;
}
#endif /* SN0 */

/*
 * _save_vectors - save client vectors
 */
void
_save_vectors(void)
{
#if IP19 || IP25 || IP27 || IP30
	if (private.dbg_exc == EXCEPT_NMI) {
		_save_nmi_exceptions();
		_hook_nmi_exceptions();
		return;
	} 
#endif

	_save_exceptions();
	_hook_exceptions();
}

/*
 * _restore_vectors - restore client vectors
 */
void
_restore_vectors()
{
#if IP19 || IP25 || IP27 || IP30
	if (private.dbg_exc == EXCEPT_NMI)
		_restore_nmi_exceptions();
	else
		_restore_exceptions();

#else	/* ! (IP19 || IP25 || IP27 || IP30) */

	_restore_exceptions();

#endif	/* IP19 || IP25 || IP27 || IP30 */
}


#ifndef NETDBX
void
p_curson(void)
{
	if ( isgraphic(1) ) (void)puts("\033[25h");
}
#endif /* NETDBX */

int symmon=1;			/* let _ttyinput() hold off on debug() */

/* these are all dummy routines; primarily needed to avoid
 * dragging in unwanted junk from libs[ck].a.
 */
#ifndef NETDBX
void _init_mbufs(void) {}
#ifndef SN0
void _init_sockets(void) {}
#endif
#endif /* !NETDBX */
void exceptutlb(void) {}
void exceptnorm(void) {}

#if !SABLE || EVEREST || IP28
#ifndef NETDBX
/* ARGSUSED */
int setenv_nvram(char *x, char *y) { return 0; }
/* ARGSUSED */
int get_nvram_tab(char *x, int y) { return 0; }
/* ARGSUSED */
int _setenv(char *x, char *y, int z) { return 0; }
#endif /* !NETDBX */
#endif
void scsiinit(void) {}

#ifndef NETDBX
#ifndef SN0			/* SN0 pulls this in somehow with IPA anyway */
/* keep gui code out */
void guiCheckMouse(void) {return;}
void initGfxGui(void) {return;}
void logoOff(void) {return;}
#endif

/* forced in by EISA/ARCSS callback.c */
/* ARGSUSED */

/* ARGSUSED */
struct mbuf *_m_get(int x, int y) { return 0; }
/* ARGSUSED */
void _m_freem(struct mbuf *a) {}

/* The ethernet driver is included in this build for SN0
   IO6Prom. Hence these stubs are not required. For now I
   have ifdef ed it as SN0.
*/
#ifndef SN0
char *ether_sprintf(u_char *x) { return 0; }
/* ARGSUSED */
int _so_append(struct so_table *x, struct sockaddr *y, struct mbuf *z) { return 0; }
/* ARGSUSED */
struct so_table * _find_socket(u_short x) { return 0; }
#endif /* !SN0 */

#endif /* !NETDBX */

void load_init(void) {}
/* ARGSUSED */
LONG Load(CHAR *a, ULONG b, ULONG *c, ULONG *d) { return 0; }
/* ARGSUSED */
LONG Invoke(ULONG a, ULONG b, LONG c, CHAR *d[], CHAR *e[]) { return 0; }
/* ARGSUSED */
LONG Execute(CHAR *a, LONG b, CHAR *c[], CHAR *d[]) { return 0; }
/* ARGSUSED */
LONG load_abs(CHAR *x, ULONG *y) { return 0; }
/* ARGSUSED */
LONG invoke_abs(ULONG a, ULONG b, LONG d, CHAR *e[], CHAR *f[]) { return 0; }
/* ARGSUSED */
LONG exec_abs(CHAR *a, LONG b, CHAR *c[], CHAR *d[]) { return 0; }
int notfirst, envdirty;

#ifndef NETDBX
/* ARGSUSED */
STATUS _null_strat(struct component *a, struct ioblock *b) { return 0; }
/* ARGSUSED */
int nullsetup(struct cfgnode_s **a, struct eiob *b, char *c) { return 0; }
/* ARGSUSED */
STATUS _mem_strat(struct component *self, struct ioblock *iob) { return 0; }
/* ARGSUSED */
int memsetup(struct cfgnode_s **cfg, struct eiob *eiob, char *arg) { return 0; }


/* 
 * little_endian is extern'ed in hpc.c which gets pulled in from IP20.c.
 * It's not used and to avoid pulling in nvram.o, IP12nvram.o, nvcompat.o,
 * passwd_cmd.o and countless unnecessary libsc .o's, define it here.
 */
#if	IP20 || IP22 || IP26 || IP28
int little_endian;
#endif
#endif /* !NETDBX */
