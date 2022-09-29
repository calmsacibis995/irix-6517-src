/*
 * IP20.c - IP20 specific routines for symmon
 *
 */
#ident "symmon/IP20.c: $Revision: 1.13 $"


#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
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
extern struct reg_desc _cpu_parerr_desc[], _gio_parerr_desc[];
extern struct reg_desc _liointr0_desc[], _liointr1_desc[]; 

static struct reg_values except_values[] = {
	{ EXCEPT_UTLB,	"UTLB Miss" },
	{ EXCEPT_XUT,	"XTLB Miss" },
	{ EXCEPT_NORM,	"NORMAL" },
	{ EXCEPT_BRKPT,	"BREAKPOINT" },
	{ 0,		NULL }
};

static struct reg_desc except_desc[] = {
	/* mask	     shift      name   format  values */
	{ -1,		0,	"vector",NULL,	except_values },
	{ 0,		0,	NULL,	NULL,	NULL }
};

/*
 * Start of symmon's stack for boot processor.
 */
int _dbgstack = SYMMON_STACK;

/*
 * Platform-dependent command table.
 */
struct cmd_table pd_ctbl[] = {
	{ 0,		0,		"" }
};

/*
 * Platform-dependent arbitrary name/value table.
 */
pd_name_table_t pd_name_table[] = {
	{ 0, 0 }
};

/*
 * symmon_fault -- display info pertinent to fault
 */
void
symmon_fault(void)
{
    unsigned int parerr;

    printf("\nSYMMON EXCEPTION: %r\n", private.dbg_exc, except_desc);
    _show_inst((inst_t *)private.exc_stack[E_EPC]);	/* try to call disassembler */
    printf("Cause register: %llR\n", private.exc_stack[E_CAUSE], cause_desc);
    printf("Status register: %llR\n", private.exc_stack[E_SR], sr_desc);
    if ((private.dbg_exc == EXCEPT_UTLB) || (private.dbg_exc == EXCEPT_XUT))
	    goto printvaddr;
    switch ((unsigned)(private.exc_stack[E_CAUSE] & CAUSE_EXCMASK)) {
    case EXC_MOD:	case EXC_RMISS:	case EXC_WMISS:
    case EXC_RADE:	case EXC_WADE:
printvaddr:
	    printf("Bad Vaddress: 0x%llx\n", private.exc_stack[E_BADVADDR]);
	    break;
    }

    /* print parity and bus error info */
    parerr = private.exc_stack[E_CPU_PARERR] & 0x1fff;
    printf("Parity error register: %R\n", parerr, _cpu_parerr_desc );
    /*
     * Only print write bus errors if the client was enabled for them,
     * otherwise we might swipe on what was intended for client
     */
    if ((private.exc_stack[E_SR] & SR_IBIT8) 
	    && (private.exc_stack[E_CAUSE] & CAUSE_IP8)) {
	if (parerr) {
	    if (parerr & CPU_ERR_STAT_RD_PAR == CPU_ERR_STAT_RD_PAR)
		printf ("CPU parity error during read at 0x%lx\n",
		    private.exc_stack[E_CPUADDR]);
	    else if (parerr & CPU_ERR_STAT_PAR)
		printf ("CPU parity error during write at 0x%lx\n",
		    private.exc_stack[E_CPUADDR]);

	    if (parerr & CPU_ERR_STAT_SYSAD_PAR)
		printf ("SYSAD bus parity error\n");
	    if (parerr & CPU_ERR_STAT_SYSCMD_PAR)
		printf ("SYSCMD fifo parity error\n");

	    if (parerr & CPU_ERR_STAT_ADDR)
	    	printf("Write Bus Error Occurred\n");
	}
    }

    /* print LIO registers */
    printf("Local I/O interrupt register 0: 0x%llR\n",
	private.exc_stack[E_LIOINTR0], _liointr0_desc);
    printf("Local I/O interrupt register 1: 0x%llR\n",
	private.exc_stack[E_LIOINTR1], _liointr1_desc);
}

int
cpu_probe(void)
{
	return 0;
}
