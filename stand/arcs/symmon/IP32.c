/*
 * IP32.c - IP32 specific routines for symmon
 *
 */
#ident "symmon/IP32.c: $Revision: 1.2 $"


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
#include <sys/gda_ip32.h>

/* defines for serial dma disable/restore */
#define SER0_TX_DMA_CNTRL_REG   0xbf318000
#define SER0_RX_DMA_CNTRL_REG   0xbf318020
#define SER1_TX_DMA_CNTRL_REG   0xbf31c000
#define SER1_RX_DMA_CNTRL_REG   0xbf31c020
#define IP32_SER_DMA_ENABLE     0x0200

 /* for stopping serial DMA when entering and resuming when leaving */
static long long _ip32_ser0_tx_cntrl;
static long long _ip32_ser0_rx_cntrl;
static long long _ip32_ser1_tx_cntrl;
static long long _ip32_ser1_rx_cntrl;

/*
 * register descriptions
 */
extern struct reg_desc sr_desc[], cause_desc[];
extern struct reg_desc _cpu_parerr_desc[];

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
    static volatile int in_symmon_fault = 0;
    static volatile int	in_symmon_double_fault = 0;
    static jmp_buf in_symmon_jmpbuf;
    long long tmp;
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
	    printf("\nSYMMON DOUBLE EXCEPTION: %r\n", private.dbg_exc, except_desc);
	    in_symmon_double_fault = 0;
	    longjmp(in_symmon_jmpbuf,1);
    }
    setjmp(in_symmon_jmpbuf);

    if (private.dbg_exc == EXCEPT_NMI) {
          /*
           * for nmi the SR & ERREPC was save in GDA areas
           */
          private.exc_stack[E_SR] = GDA->g_nmi_sr_save;
          private.exc_stack[E_ERREPC] = GDA->g_nmi_epc_save;
    }

    while (in_symmon_fault < 8) {
	    switch (in_symmon_fault++) {
	    case 0:
		    printf("\nSYMMON EXCEPTION: %r\n", private.dbg_exc, except_desc);
		    break;
	    case 1:
		   printf("epc = %llx\n",private.exc_stack[epc]);
		    break;

	    case 2:
		    printf("Cause register: %llR\n", private.exc_stack[E_CAUSE], cause_desc);
		    break;

	    case 3:
		    printf("Status register: %llR\n", private.exc_stack[E_SR], sr_desc);
		    break;

	    case 4:
		    if ((private.dbg_exc == EXCEPT_UTLB) || (private.dbg_exc == EXCEPT_XUT))
			    goto printvaddr;
		    switch ((unsigned)(private.exc_stack[E_CAUSE] & CAUSE_EXCMASK)) {
		    case EXC_MOD:	case EXC_RMISS:	case EXC_WMISS:
		    case EXC_RADE:	case EXC_WADE:
		    printvaddr:
			    printf("Bad Vaddress: 0x%llx\n", private.exc_stack[E_BADVADDR]);
			    break;
		    }
		    break;

	    case 5:

		    /* print parity and bus error info */
			printf("Crime CPU Error Addr: 0x%llx\n", private.exc_stack[E_CRM_CPU_ERROR_ADDR]);
			printf("Crime CPU Error Stat: 0x%llx\n", private.exc_stack[E_CRM_CPU_ERROR_STAT]);
			printf("Crime VICE Error Addr: 0x%llx\n", private.exc_stack[E_CRM_CPU_ERROR_VICE_ERR_ADDR]);
			printf("Crime MEM Error Stat: 0x%llx\n", private.exc_stack[E_CRM_MEM_ERROR_STAT]);
			printf("Crime MEM Error Addr: 0x%llx\n", private.exc_stack[E_CRM_MEM_ERROR_ADDR]);
			printf("Crime MEM Ecc Syn: 0x%llx\n", private.exc_stack[E_CRM_MEM_ERROR_ECC_SYN]);
			printf("Crime MEM Ecc Chk: 0x%llx\n", private.exc_stack[E_CRM_MEM_ERROR_ECC_CHK]);
			printf("Crime MEM Ecc Repl: 0x%llx\n", private.exc_stack[E_CRM_MEM_ERROR_ECC_REPL]);
		    break;

	    case 6:
		    if ((private.exc_stack[E_SR] & SR_IBIT8) 
			&& (private.exc_stack[E_CAUSE] & CAUSE_IP8)) {
			    if (parerr) {
			/****	    if (parerr & CPU_ERR_STAT_RD_PAR == CPU_ERR_STAT_RD_PAR)
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
			*****/
			    }
		    }
		    break;

	    case 7:
		    /* print registers */
		    printf("RA: 0x%llx\n", private.exc_stack[E_RA]);
		    printf("SP: 0x%llx\n", private.exc_stack[E_SP]);
		    printf("PC: 0x%llx\n", private.exc_stack[E_ERREPC]);
		    _show_inst((inst_t *)private.exc_stack[epc]); /* try to call disassembler */
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
_ip32_disable_serial_dma(void)
{
	long long tmp;

	_ip32_ser0_tx_cntrl = READ_REG64(SER0_TX_DMA_CNTRL_REG, long long);
	_ip32_ser0_rx_cntrl = READ_REG64(SER0_RX_DMA_CNTRL_REG, long long);
	_ip32_ser1_tx_cntrl = READ_REG64(SER1_TX_DMA_CNTRL_REG, long long);
	_ip32_ser1_rx_cntrl = READ_REG64(SER1_RX_DMA_CNTRL_REG, long long);

	tmp = _ip32_ser0_tx_cntrl & ~IP32_SER_DMA_ENABLE;
	WRITE_REG64(tmp,SER0_TX_DMA_CNTRL_REG, long long);

	tmp = _ip32_ser0_rx_cntrl & ~IP32_SER_DMA_ENABLE;
	WRITE_REG64(tmp,SER0_RX_DMA_CNTRL_REG, long long);

	tmp = _ip32_ser1_tx_cntrl & ~IP32_SER_DMA_ENABLE;
	WRITE_REG64(tmp,SER1_TX_DMA_CNTRL_REG, long long);

	tmp = _ip32_ser1_rx_cntrl & ~IP32_SER_DMA_ENABLE;
	WRITE_REG64(tmp,SER1_RX_DMA_CNTRL_REG, long long);
}

void
_ip32_restore_serial_dma(void)
{

	long long tmp;
	tmp = _ip32_ser0_tx_cntrl;
	WRITE_REG64(tmp,SER0_TX_DMA_CNTRL_REG, long long);

	tmp = _ip32_ser0_rx_cntrl;
	WRITE_REG64(tmp,SER0_RX_DMA_CNTRL_REG, long long);

	tmp = _ip32_ser1_tx_cntrl;
	WRITE_REG64(tmp,SER1_TX_DMA_CNTRL_REG, long long);
	tmp = _ip32_ser1_rx_cntrl;
	WRITE_REG64(tmp,SER1_RX_DMA_CNTRL_REG, long long);
}

void
run_cached(void) {}
/* stub of ide function */
