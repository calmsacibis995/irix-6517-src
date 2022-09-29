#ident	"lib/libsk/ml/ioc3.c:  $Revision: 1.18 $"

/* ioc3.c -- IOC3 specific routines */

#include <sys/types.h>
#include <sys/immu.h>
#include <sys/cpu.h>
#include <sys/PCI/PCI_defs.h>
#include <libsc.h>
#include <libsk.h>
#include <standcfg.h>

static void	size_ioc3_ssram(__psunsigned_t);

static slotdrvr_id_t ioc3ids[] = {
	{BUS_PCI, IOC3_VENDOR_ID_NUM, IOC3_DEVICE_ID_NUM, REV_ANY},
	{BUS_NONE, 0, 0, 0}
};
drvrinfo_t ioc3_drvrinfo = { ioc3ids, "ioc3" };

static COMPONENT ioc3tmpl = {
	AdapterClass,			/* Class */
	MultiFunctionAdapter,		/* Type  */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0,				/* Affinity */
	0,				/* ConfigurationDataSize */
	4,				/* IdentifierLength */
	"ioc3"				/* Identifier */
};

/* initialize a IOC3 ASIC */
int
ioc3_install(COMPONENT *parent, slotinfo_t *slot)
{
	COMPONENT	*ctrl;
	slotinfo_t      cslot;
	__psunsigned_t	ioc3_cfg_base = (__psunsigned_t)slot->cfg_base;
	__psunsigned_t	ioc3_dev_base = (__psunsigned_t)slot->mem_base;
	volatile ioc3reg_t	*ioc3_sio_cr;


	if ((ctrl = AddChild(parent, &ioc3tmpl, NULL)) == NULL)
		cpu_acpanic("ioc3");
	ctrl->Key = 0;

#if IP30
	/* for non-baseIO ioc3 just AddChild and no more */
	if (ioc3_cfg_base != IOC3_PCI_CFG_K1PTR) {
	    return(1);
	}
#endif	/* IP30 */

	/* make a slot for ioc3 ethernet */

	cslot.id.bus = BUS_IOC3;
	cslot.id.mfgr = IOC3_VENDOR_ID_NUM;
	cslot.id.part = IOC3_DEVICE_ID_NUM;
	cslot.id.rev = REV_ANY;
	cslot.cfg_base = (volatile unsigned char *)slot->cfg_base;
	cslot.mem_base = (volatile unsigned char *)slot->mem_base; 
	cslot.bus_base = (void *)IOC3_PCI_DEVIO_K1PTR;
	cslot.bus_slot = 0;
	slot_register(ctrl, &cslot);

	/*
	 * PCI_CMD_MEM_SPACE was set earlier for console i/o, this has the
	 * effect of clearing all the error status bits
	 */
	*(volatile ioc3reg_t *)(ioc3_cfg_base + IOC3_PCI_SCR) |= 
		PCI_CMD_BUS_MASTER | PCI_CMD_PAR_ERR_RESP | PCI_CMD_SERR_ENABLE;

	/* reset the DMA interface to get back into poll mode */
	*(volatile ioc3reg_t *)(ioc3_dev_base + IOC3_SSCR_A) = SSCR_RESET;
	*(volatile ioc3reg_t *)(ioc3_dev_base + IOC3_SSCR_B) = SSCR_RESET;

	/* poll for DMA interface to idle */
	ioc3_sio_cr = (volatile ioc3reg_t *)(ioc3_dev_base + IOC3_SIO_CR);
	while ((*ioc3_sio_cr & SIO_CR_ARB_DIAG_IDLE) == 0x0) 
		;

	/* unreset the serial ports */
	*(volatile ioc3reg_t *)(ioc3_dev_base + IOC3_SSCR_A) = 0x0;
	*(volatile ioc3reg_t *)(ioc3_dev_base + IOC3_SSCR_B) = 0x0;

	/* set command pulse width for serial port */
	*ioc3_sio_cr = 0x4 << SIO_CR_CMD_PULSE_SHIFT;

#if IP30
	/*
	 * set the latency timer to 1.2 us, see sec 2.8 of the
	 * ISD PCI White Paper on PCI arbitration.
	 * our PCI clock is running at 33MHz = 30ns/cycle
	 * 1200ns / (30ns/cycle) = 40 cycles
	 * other fields within the register are read only and ignored
	 * on write so we don't need to do a read-modify-write
	 */
	*(volatile ioc3reg_t *)(ioc3_cfg_base + IOC3_PCI_LAT) = 40 << 8;

	/*
	 * enable generic trisate pins for output
	 * pins 7/6: serial ports transceiver select, 0=RS232, 1=RS422
	 * pin 5: PHY reset, 0=reset, 1=unreset
	 */
	*(volatile ioc3reg_t *)(ioc3_dev_base + IOC3_GPCR_S) =
		(0x1 << 7) | (0x1 << 6) | (0x1 << 5);
	*(volatile ioc3reg_t *)(ioc3_dev_base + IOC3_GPPR(7)) = 0x0;
	*(volatile ioc3reg_t *)(ioc3_dev_base + IOC3_GPPR(6)) = 0x0;
	*(volatile ioc3reg_t *)(ioc3_dev_base + IOC3_GPPR(5)) = 0x1;
#endif	/* IP30 */

	size_ioc3_ssram(ioc3_dev_base);

	return 1;
}



/* size a IOC3 ASIC's ethernet SSRAM */
#define	PATTERN	0x5555		/* even parity bit is 0 */

static void
size_ioc3_ssram(__psunsigned_t ioc3_dev_base)
{
	volatile ioc3reg_t *ioc3_emcr = 
		(volatile ioc3reg_t *)(ioc3_dev_base + IOC3_EMCR);
	volatile __uint32_t *ssram_addr0 = (volatile __uint32_t *)
		(ioc3_dev_base + IOC3_SSRAM);
	volatile __uint32_t *ssram_addr1 = (volatile __uint32_t *)
		(ioc3_dev_base + IOC3_SSRAM + IOC3_SSRAM_LEN / 2);

	/* assume we have the larger size SSRAM and enable parity checking */
	*ioc3_emcr |= EMCR_BUFSIZ | EMCR_RAMPAR;

	*ssram_addr0 = PATTERN;
	*ssram_addr1 = ~PATTERN & IOC3_SSRAM_DM;

	/*
	 * if address aliasing happens, we have the smaller size SSRAM.
	 * be safe and AND the read date with IOC3_SSRAM_DM since IOC3 pads
	 * the upper half word with the parity bit
	 */
	if ((*ssram_addr0 & IOC3_SSRAM_DM) != PATTERN ||
	    (*ssram_addr1 & IOC3_SSRAM_DM) != (~PATTERN & IOC3_SSRAM_DM))
		*ioc3_emcr &= ~EMCR_BUFSIZ;
}

static struct reg_values arb_diag_values[] = {
	{ 0,	"tx desc read" },
	{ 1,	"tx buf1 read" },
	{ 2,	"tx buf2 read" },
	{ 3,	"rx desc read" },
	{ 4,	"rx buf write" },
	{ 0,    NULL },
};

static struct reg_values ssram_size_values[] = {
        { 0,	"64K"	},
	{ 1,	"128K"	},
	{ 0,    NULL },
};

static struct reg_desc ioc3_pci_emcr_desc[] = {
	{EMCR_DUPLEX,	0,	"DUPLEX",	NULL,	NULL},
	{EMCR_PROMISC,	0,	"PROMISC",	NULL,	NULL},
	{EMCR_PADEN,	0,	"PADEN",	NULL,	NULL},
	{EMCR_RXOFF_MASK,-3,	"RX_OFF",	"0x%x",	NULL},
	{EMCR_RAMPAR,	0,	"RAMPAR",	NULL,	NULL},
	{EMCR_BADPAR,	0,	"BADPAR",	NULL,	NULL},
	{EMCR_BUFSIZ,	0,	"BUFSIZ",	NULL,	ssram_size_values},
	{EMCR_TXDMAEN,	0,	"TXDMAEN",	NULL,	NULL},
	{EMCR_TXEN,	0,	"TXEN",		NULL,	NULL},
	{EMCR_RXDMAEN,	0,	"RXDMAEN",	NULL,	NULL},
	{EMCR_RXEN,	0,	"RXEN",		NULL,	NULL},
	{EMCR_LOOPBACK,	0,	"LOOPBACK",	NULL,	NULL},
	{EMCR_ARB_DIAG,	-18,	"ARB_DIAG",	"%d",	arb_diag_values},
	{EMCR_ARB_DIAG_IDLE,0,	"ARB_DIAG_IDLE",NULL,	NULL},
	{0,0,NULL,NULL,NULL}
};

static struct reg_desc ioc3_pci_eisr_desc[] = {
	{EISR_RXTIMERINT,	0,	"RXTIMERINT",	NULL,	NULL},
	{EISR_RXTHRESHINT,	0,	"RXTHRESHINT",	NULL,	NULL},
	{EISR_RXOFLO,		0,	"RXOFLO",	NULL,	NULL},
	{EISR_RXBUFOFLO,	0,	"RXBUFOFLO",	NULL,	NULL},
	{EISR_RXMEMERR,		0,	"RXMEMERR",	NULL,	NULL},
	{EISR_RXPARERR,		0,	"RXPARERR",	NULL,	NULL},
	{EISR_TXEMPTY,		0,	"TXEMPTY",	NULL,	NULL},
	{EISR_TXRTRY,		0,	"TXRTRY",	NULL,	NULL},
	{EISR_TXEXDEF,		0,	"TXEXDEF",	NULL,	NULL},
	{EISR_TXLCOL,		0,	"TXLCOL",	NULL,	NULL},
	{EISR_TXGIANT,		0,	"TXGIANT",	NULL,	NULL},
	{EISR_TXBUFUFLO,	0,	"TXBUFUFLO",	NULL,	NULL},
	{EISR_TXEXPLICIT,	0,	"TXEXPLICIT",	NULL,	NULL},
	{EISR_TXCOLLWRAP,	0,	"TXCOLLWRAP",	NULL,	NULL},
	{EISR_TXDEFERWRAP,	0,	"TXDEFERWRAP",	NULL,	NULL},
	{EISR_TXMEMERR,		0,	"TXMEMERR",	NULL,	NULL},
	{EISR_TXPARERR,		0,	"TXPARERR",	NULL,	NULL},
	{0,0,NULL,NULL,NULL}
};

#define	PCI_SCR_ERR	(PCI_SCR_RX_SERR |	\
			 PCI_SCR_SIG_PAR_ERR | 	\
			 PCI_SCR_SIG_TAR_ABRT |	\
			 PCI_SCR_RX_TAR_ABRT |	\
			 PCI_SCR_SIG_MST_ABRT | \
			 PCI_SCR_SIG_SERR |	\
			 PCI_SCR_PAR_ERR)
struct reg_desc ioc3_pci_scr_desc[] = {
	{0x1 << 16,	-22,	"RX_SERR",	NULL,	NULL},
	{0x1 << 24,	-24,	"SIG_PAR_ERR",	NULL,	NULL},
	{0x1 << 27,	-27,	"SIG_TAR_ABRT",	NULL,	NULL},
	{0x1 << 28,	-28,	"RX_TAR_ABRT",	NULL,	NULL},
	{0x1 << 29,	-29,	"SIG_MST_ABRT",	NULL,	NULL},
	{0x1 << 30,	-30,	"SIG_SERR",	NULL,	NULL},
	{0x1 << 31,	-31,	"PAR_ERR",	NULL,	NULL},
	{0,0,NULL,NULL,NULL}
};

struct reg_desc ioc3_pci_err_addr_desc[] = {
	{0x1L << 0,		-0,	"VALID",	NULL,		NULL},
	{0xfL << 1,		-1,	"MASTER_ID",	"0x%x",		NULL},
	{0x1L << 5,		-5,	"MUL_ERR",	NULL,		NULL},
	{0xffffffffffffffc0L,	-0,	"ADDR",		"0x%Lx", 	NULL},
	{0,0,NULL,NULL,NULL}
};

ioc3reg_t
print_ioc3_status(__psunsigned_t ioc3_cfg_base, int clear)
{
	__uint64_t pci_err_addr;
	ioc3reg_t pci_err_addr_h;
	ioc3reg_t pci_err_addr_l;
	ioc3reg_t pci_scr;

	pci_scr = *(volatile ioc3reg_t *)(ioc3_cfg_base + IOC3_PCI_SCR);
	if (!(pci_scr & PCI_SCR_ERR))
		return pci_scr;

	printf("IOC3 PCI status register: %R\n", pci_scr, ioc3_pci_scr_desc);
	if (clear)
		*(volatile ioc3reg_t *)(ioc3_cfg_base + IOC3_PCI_SCR) = pci_scr;
		
	pci_err_addr_l = 
		*(volatile ioc3reg_t *)(ioc3_cfg_base + IOC3_PCI_ERR_ADDR_L);
	if (pci_err_addr_l & PCI_ERR_ADDR_VLD) {
		pci_err_addr_h = *(volatile ioc3reg_t *)
			(ioc3_cfg_base + IOC3_PCI_ERR_ADDR_H);
		pci_err_addr = ((__uint64_t)pci_err_addr_h << 32) |
			pci_err_addr_l;
		printf("IOC3 PCI error address registers: %LR\n",
			pci_err_addr, ioc3_pci_err_addr_desc);

		/* unfreeze register */
		*(volatile ioc3reg_t *)(ioc3_cfg_base + IOC3_PCI_ERR_ADDR_L) =
			PCI_ERR_ADDR_VLD;
	}

	return pci_scr;
}

void
dump_ioc3_regs(__psunsigned_t ioc3_dev_base, __psunsigned_t ioc3_cfg_base)
{
	__uint64_t pci_err_addr;
	ioc3reg_t pci_id;
	ioc3reg_t pci_scr;
	ioc3reg_t pci_rev;
	ioc3reg_t pci_err_addr_h;
	ioc3reg_t pci_err_addr_l;

	ioc3reg_t pci_emcr;
	ioc3reg_t pci_eisr;

	printf("\nIOC3 mem base 0x%x cfg base 0x%x\n",
		ioc3_dev_base, ioc3_cfg_base);
	pci_id = *(volatile ioc3reg_t *)(ioc3_dev_base + IOC3_PCI_ID);
	pci_scr = *(volatile ioc3reg_t *)(ioc3_dev_base + IOC3_PCI_SCR);
	pci_rev = *(volatile ioc3reg_t *)(ioc3_dev_base + IOC3_PCI_REV);
	printf("PCI ID 0x%08x, SCR 0x%08x, REV 0x%08x\n",
		pci_id, pci_scr, pci_rev);

	pci_id = *(volatile ioc3reg_t *)(ioc3_cfg_base + IOC3_PCI_ID);
	pci_scr = *(volatile ioc3reg_t *)(ioc3_cfg_base + IOC3_PCI_SCR);
	pci_rev = *(volatile ioc3reg_t *)(ioc3_cfg_base + IOC3_PCI_REV);
	printf("PCI ID 0x%08x, SCR 0x%08x, REV 0x%08x\n",
		pci_id, pci_scr, pci_rev);
	printf("IOC3 PCI status register: %R\n", pci_scr, ioc3_pci_scr_desc);
	pci_err_addr_l =
	    *(volatile ioc3reg_t *)(ioc3_cfg_base + IOC3_PCI_ERR_ADDR_L);
	if (pci_err_addr_l & PCI_ERR_ADDR_VLD)
	    printf("pci_err_addr_l & PCI_ERR_ADDR_VLD true:\n");
	
	pci_err_addr_h = *(volatile ioc3reg_t *)
		(ioc3_cfg_base + IOC3_PCI_ERR_ADDR_H);
	pci_err_addr = ((__uint64_t)pci_err_addr_h << 32) |
		pci_err_addr_l;
	printf("IOC3 PCI error address registers: %LR\n",
		pci_err_addr, ioc3_pci_err_addr_desc);

	pci_emcr = *(volatile ioc3reg_t *)(ioc3_dev_base + IOC3_EMCR);
	printf("Enet Mac Control Register: %R\n", pci_emcr, ioc3_pci_emcr_desc);

	pci_eisr = *(volatile ioc3reg_t *)(ioc3_dev_base + IOC3_EISR);
	printf("Enet Intr Status Register: %R\n", pci_eisr, ioc3_pci_eisr_desc);
	pci_eisr = *(volatile ioc3reg_t *)(ioc3_dev_base + IOC3_EIER);
	printf("Enet Intr Enable Register: %R\n", pci_eisr, ioc3_pci_eisr_desc);
}
