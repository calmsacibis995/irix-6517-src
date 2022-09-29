/*
 * IP30.c - IP30 specific routines for symmon
 *
 */
#ident "$Revision: 1.17 $"


#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/RACER/racermp.h>
#include <sys/sbd.h>
#include <setjmp.h>
#include <fault.h>
#include <parser.h>
#include <saioctl.h>
#include "dbgmon.h"
#include "mp.h"
#include <libsc.h>

/*
 * register descriptions
 */
extern struct reg_desc sr_desc[], cause_desc[];

static struct reg_values except_values[] = {
	{ EXCEPT_UTLB,	"UTLB Miss" },
	{ EXCEPT_XUT,	"XTLB Miss" },
	{ EXCEPT_NORM,	"NORMAL" },
	{ EXCEPT_BRKPT,	"BREAKPOINT" },
	{ EXCEPT_NMI,   "NMI" },
	{ 0,		NULL }
};

static struct reg_desc except_desc[] = {
	/* mask	     		shift	name   		format	values */
	{ (__scunsigned_t)-1,	0,	"vector",	NULL,	except_values },
	{ 0,			0,	NULL,		NULL,	NULL }
};

static int	_show_heart(int, char **, char **, struct cmd_table *);
static int	_show_xbow(int, char **, char **, struct cmd_table *);
static int	_show_bridge(int, char **, char **, struct cmd_table *);
static int	_show_htimer(int, char **, char **, struct cmd_table *);

/*
 * Start of symmon's stack for boot processor.
 */
__psunsigned_t _dbgstack = SYMMON_STACK;

/*
 * Platform-dependent command table.
 */
extern int _switch_cpus(int, char *[], char *[], struct cmd_table *xxx);

struct cmd_table pd_ctbl[] = {
	{ "cpu",	_switch_cpus,	"cpu:\t\tcpu [CPUID]" },
	{ "heart",	_show_heart,	"heart:\t\tshow heart registers" },
	{ "xbow",	_show_xbow,	"xbow:\t\tshow xbow registers" },
	{ "bridge",	_show_bridge,	"bridge [xbow_port]: show bridge registers" },
	{ "timer",	_show_htimer,	"timer: show heart timer value" },
	{ 0,		0,		"" }
};

/*
 * Platform-dependent arbitrary name/value table.
 */
pd_name_table_t pd_name_table[] = {
	{ "h_mode",		(long)&HEART_PIU_K1PTR->h_mode },
	{ "h_status",		(long)&HEART_PIU_K1PTR->h_status },
	{ "h_count",		(long)&HEART_PIU_K1PTR->h_count },
	{ "heart_piu",		(long)HEART_PIU_K1PTR },
	{ "xbow_wid",		(long)XBOW_K1PTR },
	{ "heart_wid",		(long)HEART_CFG_K1PTR },
	{ "bridge_wid",		(long)BRIDGE_K1PTR },
	{ 0, 0 }
};

/*
 * determine whether a given CPU is present and enabled. 
 */
uint 
cpu_enabled(int virtid) 
{
	return (HEART_PIU_K1PTR->h_status & HEART_STAT_PROC_ACTIVE(virtid)) &&
		!(HEART_PIU_K1PTR->h_mode & HM_PROC_DISABLE(virtid));
}

/*
 * CPU virtual id to physical id mapping.
 */
int
cpu_physical(int virtid)
{
	return virtid;
}


/*
 * symmon_fault -- display info pertinent to fault
 */
void
symmon_fault(void)
{
	static volatile int in_symmon_fault = 0;
	static volatile int in_symmon_double_fault = 0;
	static jmp_buf in_symmon_jmpbuf;
        int epc;

#if R4000 || R10000
        if (private.dbg_exc == EXCEPT_NMI)
                epc = E_ERREPC;
        else
#endif
                epc = E_EPC;

	if (in_symmon_fault) {
		if (in_symmon_double_fault)
			EnterInteractiveMode();
		in_symmon_double_fault = 1;
		printf("\nSYMMON DOUBLE EXCEPTION: %r\n", private.dbg_exc,
		       except_desc);
		in_symmon_double_fault = 0;
		longjmp(in_symmon_jmpbuf,1);
	}
	setjmp(in_symmon_jmpbuf);

	if (private.dbg_exc == EXCEPT_NMI)
		private.nmi_int_flag = 1;

	ACQUIRE_USER_INTERFACE();

	while (in_symmon_fault < 5) {
		switch (in_symmon_fault++) {
		case 0:
			printf("\nSYMMON EXCEPTION: %r\n", private.dbg_exc,
			       except_desc);
			break;

		case 1:
			/* try to call disassembler */
			_show_inst((inst_t *)private.exc_stack[epc]);
			break;

		case 2:
			printf("Cause register: %R\n",
			       private.exc_stack[E_CAUSE], cause_desc);
			break;

		case 3:
			printf("Status register: %R\n", private.exc_stack[E_SR],
			       sr_desc);
			break;

		case 4:
			if ((private.dbg_exc == EXCEPT_UTLB) ||
			    (private.dbg_exc == EXCEPT_XUT))
				goto printvaddr;

			switch ((unsigned)(private.exc_stack[E_CAUSE] &
				CAUSE_EXCMASK)) {
			case EXC_MOD: case EXC_RMISS: case EXC_WMISS:
			case EXC_RADE: case EXC_WADE:
printvaddr:
				printf("Bad Vaddress: 0x%lx\n",
				       private.exc_stack[E_BADVADDR]);
			default:
				break;
			}
			break;

		default:
			break;
		}
	}
	in_symmon_fault = 0;

	RELEASE_USER_INTERFACE(NO_CPUID);
}

void
notyetmess(char *mess)
{
        printf("%s not implemented for RACER\n", mess);
}

/*ARGSUSED*/
static int
_show_heart(int argc, char **ap, char **av, struct cmd_table *foo)
{
	/* just show registers, dont modify state */
	printf("\nHeart register dump:\n");
	dump_heart_regs();
	return 0;
}

/*ARGSUSED*/
static int
_show_xbow(int argc, char **ap, char **av, struct cmd_table *foo)
{
	extern xbowreg_t print_xbow_status(__psunsigned_t xbow_base, int clear);
	extern xbowreg_t dump_xbow_regs(__psunsigned_t xbow_base, int clear);

	/* just show registers, dont modify state */
	printf("Check Xbow int/err status:\n");
	(void)print_xbow_status((__psunsigned_t)XBOW_K1PTR, 0 /* dont clear */);
	printf("\nXbow register dump:\n");
	(void)dump_xbow_regs((__psunsigned_t)XBOW_K1PTR, 0 /* dont clear */);
	return 0;
}

/*ARGSUSED*/
static int
_show_bridge(int argc, char **ap, char **av, struct cmd_table *foo)
{
	bridge_t *bridge;
	int wid_no;

	if (argc == 2) {
		(void) atob(ap[1], &wid_no);
		if (wid_no <= XBOW_PORT_8 && wid_no > XBOW_PORT_F)
			wid_no = BRIDGE_ID;	/* default to BASE IO bridge */
	} else
		wid_no = BRIDGE_ID;

	bridge = (bridge_t *)K1_MAIN_WIDGET(wid_no);
	/* just show registers, dont modify state */
	printf("Check Bridge int/err status:\n");
	print_bridge_status(bridge, 0 /* dont clear */);
	printf("\nBridge register dump:\n");
	dump_bridge_regs(bridge);
	return 0;
}

/*ARGSUSED*/
static int
_show_htimer(int argc, char **ap, char **av, struct cmd_table *foo)
{
	heartreg_t timer = HEART_PIU_K1PTR->h_count;
	printf("Heart timer: 0x%llx %lld\n",timer,timer);
	return 0;
}

#ifdef SABLE
#include <arcs/spb.h>
#include <stringlist.h>

void
sableinit(void)
{
	extern struct string_list environ_str;
	extern int _prom;		/* so we initialize SBP */

	if (SPB->Signature != SPBMAGIC) {
		_prom = 1;

		/* init_env */
		new_str2("dbaud","9600",&environ_str);
	}
}

/*ARGSUSED stub to include nvram code */
int illegal_passwd(char *p) {return 0;}
#endif

int flash_init(void) {return 0;}
