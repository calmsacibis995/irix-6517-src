/*
 * IP28.c - IP28 specific routines for symmon
 */
#ident "$Revision: 1.12 $"


#include <sys/types.h>
#include <sys/cpu.h>
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
extern struct reg_desc _cpu_parerr_desc[], _gio_parerr_desc[];
extern struct reg_desc _liointr0_desc[], _liointr1_desc[];

static struct reg_values except_values[] = {
	{ EXCEPT_UTLB,	"UTLB Miss" },
	{ EXCEPT_XUT,	"XTLB Miss" },
	{ EXCEPT_NORM,	"NORMAL" },
	{ EXCEPT_BRKPT,	"BREAKPOINT" },
	{ EXCEPT_NMI,   "NMI" },
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
__psint_t _dbgstack = SYMMON_STACK;

/*
 * Platform-dependent command table.
 */
static int _show_ucmem(int, char **, char **, struct cmd_table *);
struct cmd_table pd_ctbl[] = {
        { "ucmem",              _show_ucmem,            "show caller's mode" },
        { 0,                    0,                      0 }
};

/*
 * Platform-dependent arbitrary name/value table.
 */
pd_name_table_t pd_name_table[] = {
	{ "cpuctrl0", PHYS_TO_K1(CPUCTRL0) },
	{ "cpuctrl1", PHYS_TO_K1(CPUCTRL1) },
	{ 0, 0 }
};

int ip28_ucmem;				/* caller running in fast/slow mode */

/*
 * symmon_fault -- display info pertinent to fault
 */
void
symmon_fault(void)
{
	unsigned int parerr;
	static volatile int in_symmon_fault = 0;
	static volatile int in_symmon_double_fault = 0;
	static jmp_buf in_symmon_jmpbuf;
        int epc;

	if (private.dbg_exc == EXCEPT_ECC)
		epc = E_ERREPC;
	else
		epc = E_EPC;

	if (in_symmon_fault) {
		if (in_symmon_double_fault)
			EnterInteractiveMode();
		in_symmon_double_fault = 1;
		printf("\nSYMMON DOUBLE EXCEPTION: %r\n", private.dbg_exc,
		       except_desc);
		in_symmon_double_fault = 0;
		/* Need to update 2nd NMI on memory rescan. */
		if (private.dbg_exc == EXCEPT_NMI)
			ip28_ecc_error();
		longjmp(in_symmon_jmpbuf,1);
	}
	setjmp(in_symmon_jmpbuf);

	while (in_symmon_fault < 8) {
		switch (in_symmon_fault++) {
		case 0:
			printf("\nSYMMON EXCEPTION: %r\n", private.dbg_exc,
			       except_desc);
			if (private.dbg_exc == EXCEPT_NMI)
				ip28_ecc_error();
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
				break;
			}
			break;
            	case 5:

                    	/* print bus error info */
                    	parerr = (int)(private.exc_stack[E_CPU_PARERR] & 0x1fff);
                    	printf("CPU Error Stat: %R\n", parerr, _cpu_parerr_desc );
                     	break;

            	case 6:
                    	/*
                     	* Only print write bus errors if the client was enabled for them,
                     	* otherwise we might swipe on what was intended for client
                     	*/
                    	if ((private.exc_stack[E_SR] & SR_IBIT7)
                        	&& (private.exc_stack[E_CAUSE] & CAUSE_IP7)) {
                            if (parerr) {
				    /* All mem parity errors are ECC errors */
				    if (parerr & CPU_ERR_STAT_RD_PAR) {
					void ip26_clear_ecc(void);
					ip26_clear_ecc();
					if (parerr & ECC_MULTI_BIT_ERR)
					    printf("CPU Multi-bit ECC Error\n");
					if (parerr & ECC_FAST_UC_WRITE)
					    printf("Illegal uncached write\n");
				    }

                                    if (parerr & CPU_ERR_STAT_SYSAD_PAR)
                                            printf ("SYSAD bus parity error\n");
                                    if (parerr & CPU_ERR_STAT_SYSCMD_PAR)
                                            printf ("SYSCMD fifo parity error\n");

                                    if (parerr & CPU_ERR_STAT_ADDR)
                                            printf("Write Bus Error Occurred\n");
                            }
                    	}
                    	break;

            	case 7:

                    	/* print LIO registers */
                    	printf("Local I/O interrupt register 0: %llR\n",
                           private.exc_stack[E_LIOINTR0], _liointr0_desc);
                    	printf("Local I/O interrupt register 1: %llR\n",
                           private.exc_stack[E_LIOINTR1], _liointr1_desc);
                    	break;

		default:
			break;
		}
	}
	in_symmon_fault = 0;
}

int
cpu_probe(void)
{
	return 0;
}

void
notyetmess(char *mess)
{
        printf("%s not implemented for RACER\n", mess);
}

/*ARGSUSED*/
static int
_show_ucmem(int argc, char **ap, char **av, struct cmd_table *foo)
{
        printf("previous mode: %d (%s)\n",
                ip28_ucmem, ip28_ucmem ? "normal" : "slow");
        return 0;
}
