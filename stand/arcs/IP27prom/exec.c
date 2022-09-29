#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/mips_addrspace.h>
#include <sys/nic.h>
#include <sys/SN/agent.h>
#include <sys/SN/router.h>
#include <sys/SN/launch.h>
#include <sys/SN/intr.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/promcfg.h>
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/SN0/klhwinit.h>
#include <sys/PCI/ioc3.h>
#include <ksys/elsc.h>
#include <sys/SN/SN0/IP31.h>

#include <corp.h>
#include <libkl.h>
#include <rtc.h>

#include "biendian.h"
#include "pod.h"
#include "prom_error.h"
#include "rtr_diags.h"
#include "ip27prom.h"
#include "pod_failure.h"
#include "libasm.h"
#include "cache.h"
#include "tlb.h"
#include "prom_leds.h"
#include "router.h"
#include "hub.h"
#include "exec.h"
#include "lex.h"
#include "cmd.h"
#include "mdir.h"
#include "mtest.h"
#include "memory.h"
#include "discover.h"
#include "symbol.h"
#include "unzip.h"
#include "pvers.h"
#include "bist.h"
#include "junkuart.h"
#include "ioc3uart.h"
#include "kdebug.h"
#include "nasid.h"

#define TOP_BYTE(x, b)	((x) & ~(0xffUL << 56) | (i64) (b) << 56)

extern	hwreg_set_t	hwreg_hub;
extern	hwreg_set_t	hwreg_xbow;
extern	hwreg_set_t	hwreg_router;
extern	hwreg_set_t	hwreg_c0;

__scunsigned_t	jump_addr(__psunsigned_t,
			  __scunsigned_t, __scunsigned_t,
			  struct flag_struct *);

static int bad_mode(parse_data_t *pd, int mode)
{
    if (pd->flags->mode == mode) {
	printf("Can't run this command in %s mode (see 'go' command)\n",
	       mode == POD_MODE_CAC ? "Cac" :
	       mode == POD_MODE_UNC ? "Unc" : "Dex");
	return 1;
    }

    return 0;
}

static void warn_cache(parse_data_t *pd, i64 addr)
{
    if (((addr >> 56) == 0xa8) &&
	(pd->flags->mode == POD_MODE_DEX) && ((get_BSR() & BSR_KDEBUG) == 0))
	printf("Warning: illegal to access cached addresses in Dex mode\n");
}

/*
 * Commands called from parse.y
 */

void exec_delay(parse_data_t *pd)
{
#ifndef SABLE
    rtc_sleep(parse_pop(pd));
#endif
}

void exec_sleep(parse_data_t *pd)
{
#ifndef SABLE
    rtc_sleep(parse_pop(pd) * 1000000);
#endif
}

void exec_echo(parse_data_t *pd, i64 nargs)
{
    char	str[3][LEX_STRING_MAX];
    i64		i;

    for (i = 0; i < nargs; i++)
	parse_pop_string(pd, str[i]);

    while (nargs--)
	puts(str[nargs]);
}

void exec_modt(parse_data_t *pd, i64 topbyte)
{
    i64		v	= parse_pop(pd);

    parse_push(pd,
	       topbyte == ~0ULL ?
	       (i64) ((__int64_t) (v << 32) >> 32) :
	       TOP_BYTE(v, topbyte));
}

void exec_modn(parse_data_t *pd)
{
    i64		value, node;

    value = parse_pop(pd);
    node = parse_pop(pd);

    value = value & ~(0xffULL << 32) | node << 32;
    if ((value & 0xffffff00ef000000ULL) == 0x9200000001000000ULL &&
	(int) node != net_node_get())
	value |= 0x800000;	/* Remote hub register */

    parse_push(pd, value);
}

void exec_modw(parse_data_t *pd)
{
    i64		value, widget;

    value = parse_pop(pd);
    widget = parse_pop(pd);

    parse_push(pd, value | NODE_SWIN_BASE(0, widget));
}

void exec_modb(parse_data_t *pd)
{
    i64		value, bank;

    value = parse_pop(pd);
    bank = parse_pop(pd);

    parse_push(pd, value | MD_BANK_OFFSET(bank));
}

void exec_modd(parse_data_t *pd, i64 type)
{
    i64		value = parse_pop(pd);

    switch (type) {
    case 0:
	value = BDDIR_ENTRY_LO(value);
	break;
    case 1:
	value = BDDIR_ENTRY_HI(value);
	break;
    case 2:
	value = BDPRT_ENTRY(value, parse_pop(pd));
	break;
    case 3:
	value = BDECC_ENTRY(value);
	break;
    }

    parse_push(pd, value);
}

void exec_arith(parse_data_t *pd, i64 op)
{
    i64		arg1, arg2, result;

    if ((op & 0x10000) == 0)
	arg2 = parse_pop(pd);

    arg1 = parse_pop(pd);

    switch (op & 0xffff) {
    case '~':
	result = ~arg1;
	break;
    case 'i':
    case parse_LD:
	result = *(volatile __uint64_t *) arg1;
	break;
    case parse_LW:
	result = *(volatile uint *) arg1;
	break;
    case parse_LH:
	result = *(volatile ushort_t *) arg1;
	break;
    case parse_LB:
	result = *(volatile uchar_t *) arg1;
	break;
    case '!':
	result = ! arg1;
	break;
    case 'n':
	result = (i64) -((__int64_t) arg1);
	break;
    case '+':
	result = arg1 + arg2;
	break;
    case '-':
	result = arg1 - arg2;
	break;
    case '*':
	result = arg1 * arg2;
	break;
    case '/':
	if (arg2 == 0)
	    printf("Warning: dividing by zero\n");
	result = arg1 / arg2;
	break;
    case '%':
	if (arg2 == 0)
	    printf("Warning: modulus by zero\n");
	result = arg1 % arg2;
	break;
    case '|':
	result = arg1 | arg2;
	break;
    case '&':
	result = arg1 & arg2;
	break;
    case '^':
	result = arg1 ^ arg2;
	break;
    case parse_LSHIFT:
	result = arg1 << arg2;
	break;
    case parse_RSHIFT:
	result = arg1 >> arg2;
	break;
    case parse_ISEQUAL:
	result = arg1 == arg2;
	break;
    case parse_NOTEQUAL:
	result = arg1 != arg2;
	break;
    case '<':
	result = arg1 < arg2;
	break;
    case '>':
	result = arg1 > arg2;
	break;
    case parse_LTEQUAL:
	result = arg1 <= arg2;
	break;
    case parse_GTEQUAL:
	result = arg1 >= arg2;
	break;
    case parse_ANDAND:
	result = arg1 && arg2;
	break;
    case parse_OROR:
	result = arg1 || arg2;
	break;
    default:
	printf("Bad arith op: %d\n", op & 0x7f);
	break;
    }

    if ((op & 0x20000) == 0)
	parse_push(pd, result);
}

void exec_getcop0(parse_data_t *pd, i64 which_reg)
{
    i64		value;

    switch (which_reg) {
    case C0_SR:
	value = pd->regs->sr;
	break;
    case C0_CAUSE:
	value = pd->regs->cause;
	break;
    case C0_EPC:
	value = pd->regs->epc;
	break;
    case C0_BADVADDR:
	value = pd->regs->badva;
	break;
    case C0_ERROR_EPC:
	value = pd->regs->error_epc;
	break;
    default:
	value = get_cop0(which_reg);
	break;
    }

    parse_push(pd, value);
}

void exec_setcop0(parse_data_t *pd, i64 which_reg)
{
    i64		value	= parse_pop(pd);
    int		saved	= 0;

    switch (which_reg) {
    case C0_SR:
	pd->regs->sr = value;
	saved = 1;
	break;
    case C0_CAUSE:
	pd->regs->cause = value;
	saved = 1;
	break;
    case C0_EPC:
	pd->regs->epc = value;
	saved = 1;
	break;
    case C0_BADVADDR:
	pd->regs->badva = value;
	saved = 1;
	break;
    case C0_ERROR_EPC:
	pd->regs->error_epc = value;
	saved = 1;
	break;
    default:
	set_cop0(which_reg, value);
	break;
    }

    if (saved)
	printf("Note: changed saved value, not actual C0 register.\n");
}

void exec_print(parse_data_t *pd, i64 base)
{
    char       *fmt;

    switch (base) {
    case 16:
	fmt	= "%y\n";
	break;
    case 10:
	fmt	= "%ld\n";
	break;
    case 8:
	fmt	= "%#lo\n";
	break;
    case 2:
	fmt	= "%#lb\n";
	break;
    }

    printf(fmt, parse_pop(pd));
}

void exec_printreg(parse_data_t *pd, i64 which_reg)
{
    int		i;

    if (which_reg == ~0ULL)
	which_reg = parse_pop(pd);

    if (which_reg > 32)
	printf("Illegal register number\n");
    else {
	if (which_reg == 32)
	    which_reg = 31;		/* pc is effectively ra */
	printf("r%02d/%s: %y\n",
	       which_reg, gpr_names[which_reg], pd->regs->gpr[which_reg]);
    }
}

void exec_printregall(parse_data_t *pd)
{
    char	buf[40];
    int		i;

    for (i = 0; i < 32; i += 2)
	printf("r%02d/%s: %y    r%02d/%s: %y\n",
	       i + 0, gpr_names[i + 0], pd->regs->gpr[i + 0],
	       i + 1, gpr_names[i + 1], pd->regs->gpr[i + 1]);
}

/*
 * If bit 8 is 0: use actual value of register
 * If bit 8 is 1: use value from arg stack
 */

void exec_printcop0(parse_data_t *pd, i64 which_reg)
{
    int		source	= (which_reg & 0x100) ? 1 : 0;
    i64		value;

    which_reg &= 0xff;

    if (which_reg > 30)
	printf("cop0 register number out of range\n");
    else {
	if (source)
	    value = parse_pop(pd);
	else {
	    switch (which_reg) {
	    case C0_SR:
		value = pd->regs->sr;
		break;
	    case C0_CAUSE:
		value = pd->regs->cause;
		break;
	    case C0_EPC:
		value = pd->regs->epc;
		break;
	    case C0_BADVADDR:
		value = pd->regs->badva;
		break;
	    case C0_ERROR_EPC:
		value = pd->regs->error_epc;
		break;
	    default:
		value = get_cop0(which_reg);
		source = 2;
		break;
	    }
	}

	printf("Register: %s (%d)\n", cop0_names[which_reg], which_reg);
	printf("Value   : %y (%s)\n",
	       value,
	       source == 0 ? "as saved on POD entry" :
	       source == 1 ? "as requested" :
	       source == 2 ? "loaded from register" : "");

	hwreg_decode(&hwreg_c0,
		     &hwreg_c0.regs[which_reg],
		     0, 0, 16, value, printf);
    }
}

void exec_printcop1(parse_data_t *pd, i64 nargs)
{
    i64		reg;

    if (nargs) {
	reg = parse_pop(pd);
	if (reg > 31)
	    printf("Floating point register number out of range\n");
	else
	    printf("$f%02ld = %y\n", reg, get_cop1(reg));
    } else
	for (reg = 0; reg < 32; reg += 2)
	    printf("$f%02ld = %y    $f%02ld = %y\n",
		   reg, get_cop1(reg), reg + 1, get_cop1(reg + 1));
}

void exec_setcop1(parse_data_t *pd, i64 reg_or_stk)
{
    i64		value	= parse_pop(pd);
    i64		reg	= (reg_or_stk == ~0ULL) ? parse_pop(pd) : reg_or_stk;

    if (reg > 31)
	printf("Floating point register number out of range\n");
    else
	set_cop1(reg, value);
}

void exec_getcop1(parse_data_t *pd)
{
    i64		which_reg	= parse_pop(pd);

    parse_push(pd, get_cop1(which_reg));
}

#define	PROM_REGION		0xffffffffbfc00000
#define	KB			0x400ULL
#define MB			0x100000ULL
#define REGION(a)		((a) & 0xffffff0000000000)
#define REMOVE_REGION(a)	(a &= ~0xffffff0000000000)
#define	REMOVE_NODE(a)		(a &= ~(0xffULL << 32))
#define	SWIN(a)			((a) >> 24 & 0xf)
#define	BWIN(a)			((a) >> 29 & 7)

static void decode_prom(i64 a, int bit)
{
    i64		phy_addr;
    char	buf[32];

    if ((a >= IP27PROM_BASE_MAPPED) &&
	(a < (IP27PROM_BASE_MAPPED + IP27PROM_SIZE_MAX))) {

	printf("\t is in IP27prom space.\n");

	phy_addr = a - IP27PROM_BASE_MAPPED + K0_TO_PHYS(IP27PROM_BASE);
	mdir_xlate_addr_mem(buf, phy_addr & NODE_ADDRSPACE_MASK, bit);

	printf("\tWARNING: Addr can be either in PROM or MEMORY.\n");
	printf("\tif addr is in memory:\n");
	printf("\tbit %d on its LOCAL NODE MEMORY %s\n", bit, buf);
    }

    else if ((a >= IO6PROM_BASE_MAPPED) &&
	(a < (IO6PROM_BASE_MAPPED + IO6PROM_SIZE))) {

	printf("\t is in IO6prom space.\n");

	phy_addr = a - IO6PROM_BASE_MAPPED + K0_TO_PHYS(IO6PROM_BASE);
	mdir_xlate_addr_mem(buf, phy_addr & NODE_ADDRSPACE_MASK, bit);

	printf("\tbit %d is in CACHED MEMORY space on LOCAL NODE %s\n",
	    bit, buf);
    }

    else
	printf("\tis in inknown MAPPED space\n");
}

static void decode_cac_uncac(i64 a, int cac, int bit)
{
    char	buf[32], mod[8];
    nasid_t	nasid;
    int		module, slot;

    mdir_xlate_addr_mem(buf, a & NODE_ADDRSPACE_MASK, bit);
    nasid =  NASID_GET(a);
    slot = SLOTNUM_GETSLOT(get_node_slotid(nasid));

    ip27log_getenv(nasid, IP27LOG_MODULE_KEY, mod, "0", 0);
    module = atoi(mod);

    printf("\tbit %d is in %sCACHED MEMORY space on NASID %d "
	"%S: %s\n", bit, (cac ? "" : "UN"), nasid, module,
	slot, buf);
}

static void decode_hspec(i64 a, int bit)
{
    int		node;
    i64		mc;
    int		premium, dimm0_sel;
    char	buf[32];

    printf("\tis in HSPEC space\n");

    node = NASID_GET(a);
    REMOVE_NODE(a);

    if (node == 0) {
	if (a < 256 * MB) {
	    printf("\tualias, offset 0x%016llx\n", a);
	    return;
	}

	if (a < 512 * MB) {
	    printf("\tlboot, offset 0x%016llx\n", a);
	    return;
	}

	if (a < 768 * MB) {
	    printf("\treserved area "
		   "(node 0 HSPEC 512 MB to 768 MB), "
		   "offset 0x%016llx\n", a);
	    return;
	}
    }

    if (a < 768 * MB) {
	printf("\tualias reserved area, offset 0x%016llx\n", a);
	return;
    }

    if (a < 1024 * MB) {
	printf("\trboot, offset 0x%016llx\n", a);
	return;
    }

    if (a < (1024 * MB + 512 * KB)) {
	printf("\tBDECC space\n");
	return;
    }

    if (a < 2048 * MB) {
	mc = LD(REMOTE_HUB(node, MD_MEMORY_CONFIG));

	premium = (int) GET_FIELD(mc, MMC_DIR_PREMIUM);
	dimm0_sel = (int) GET_FIELD(mc, MMC_DIMM0_SEL);

	(premium ? mdir_xlate_addr_pdir : mdir_xlate_addr_sdir)
	    (buf, a, bit, dimm0_sel);

	printf("\t%s Directory bit %d: %s\n",
	       premium ? "Premium" : "Standard", bit, buf);
    }
}

static void decode_pci(i64 a)
{
    if (a < 0x010000)
	printf("\tis in XBOW local register space\n");
    else if ((a >= 0x010000) && (a < 0x020000))
	printf("\tis in XBOW internal addr xlation RAM\n");
    else if ((a >= 0x020000) && (a < 0x021000))
	printf("\tis in PCI DEVICE 0 config space\n");
    else if ((a >= 0x021000) && (a < 0x022000))
	printf("\tis in PCI DEVICE 1 config space\n");
    else if ((a >= 0x022000) && (a < 0x023000))
	printf("\tis in PCI DEVICE 2 config space\n");
    else if ((a >= 0x023000) && (a < 0x024000))
	printf("\tis in PCI DEVICE 3 config space\n");
    else if ((a >= 0x024000) && (a < 0x025000))
	printf("\tis in PCI DEVICE 4 config space\n");
    else if ((a >= 0x025000) && (a < 0x026000))
	printf("\tis in PCI DEVICE 5 config space\n");
    else if ((a >= 0x026000) && (a < 0x027000))
	printf("\tis in PCI DEVICE 6 config space\n");
    else if ((a >= 0x027000) && (a < 0x028000))
	printf("\tis in PCI DEVICE 7 config space\n");
    else if ((a >= 0x028000) && (a <= 0x028fff))
	printf("\tis in PCI TYPE 1 config space\n");
    else if ((a >= 0x030000) && (a <= 0x030007))
	printf("\tis in PCI Intr Ack cycle\n");
    else if ((a >= 0x080000) && (a <= 0x0fffff))
	printf("\tis in PCI externel sync SSRAM\n");
    else if ((a >= 0x100000) && (a <= 0x1fffff))
	printf("\tis in RESERVED PCI space\n");
    else if ((a >= 0x200000) && (a < 0x400000))
	printf("\tis in PCI/GIO DEVICE 0 space\n");
    else if ((a >= 0x400000) && (a < 0x600000))
	printf("\tis in PCI/GIO DEVICE 1 space\n");
    else if ((a >= 0x600000) && (a < 0x700000))
	printf("\tis in PCI/GIO DEVICE 2 space\n");
    else if ((a >= 0x700000) && (a < 0x800000))
	printf("\tis in PCI/GIO DEVICE 3 space\n");
    else if ((a >= 0x800000) && (a < 0x900000))
	printf("\tis in PCI/GIO DEVICE 4 space\n");
    else if ((a >= 0x900000) && (a < 0xa00000))
	printf("\tis in PCI/GIO DEVICE 5 space\n");
    else if ((a >= 0xa00000) && (a < 0xb00000))
	printf("\tis in PCI/GIO DEVICE 6 space\n");
    else if ((a >= 0xb00000) && (a < 0xc00000))
	printf("\tis in PCI/GIO DEVICE 7 space\n");
    else if ((a >= 0xc00000) && (a <= 0xffffff))
	printf("\tis in externel FLASH PROMS space\n");
    else
	printf("\tis in UNKNOWN PCI space\n");
}

static void decode_io(i64 a)
{
    int		bwin, swin, nasid, wid_id, xbow_slot, xb_link;
    char	name[4];
    lboard_t	*brd, *io_brd;
    klxbow_t	*xbow;

    printf("\tis in IO space\n");

    nasid = NASID_GET(a);

    bwin = BWIN(a);

    swin = SWIN(a);

    xbow_slot = (int) get_node_crossbow(nasid);

    wid_id = (int) WIDGETID_GET(a);

    xb_link = wid_id - BASE_XBOW_PORT;

    printf("\tsmall window (widget) 0x%x\n", swin);

    if (bwin) {
	printf("\tbig Window decoding. Not implemented\n");
	return;
    }

    get_widget_slotname(xbow_slot, wid_id, name);
    printf("\tcorresponds to slot %s\n", name);

    if (config_find_xbow(nasid, &brd, &xbow)) {
	printf("\tfailed to find board: Failed to find PCI device\n");
	return;
    }

    if (!XBOW_PORT_IS_ENABLED(xbow, wid_id)) {
	printf("\tillegal port address: Failed to find PCI device\n");
	return;
    }

    if (XBOW_PORT_TYPE_HUB(xbow, wid_id)) {
	printf("\tis a HUB on wid_id %d\n", wid_id);
	return;
    }

    if (XBOW_PORT_TYPE_IO(xbow, wid_id)) {

	io_brd = (lboard_t *)
	    NODE_OFFSET_TO_K1(xbow->xbow_port_info[xb_link].port_nasid,
			      xbow->xbow_port_info[xb_link].port_offset);
	switch (io_brd->brd_type) {
	    case  KLTYPE_IO6:
		printf("\tcorresponds to a IO6 board\n");
		decode_pci(SWIN_WIDGETADDR(a));
		break;
	    case KLTYPE_WEIRDIO:
		printf("\tcorresponds to a WEIRD IO (NIC unknown??) board\n");
		break;
	    case KLTYPE_MSCSI:
		printf("\tcorresponds to a MSCSI board\n");
		decode_pci(SWIN_WIDGETADDR(a));
		break;
	    case KLTYPE_MENET:
		printf("\tcorresponds to a MENET board\n");
		decode_pci(SWIN_WIDGETADDR(a));
		break;
	    case KLTYPE_FDDI:
		printf("\tcorresponds to a FDDI board\n");
		break;
	    case KLTYPE_PCI:
		printf("\tcorresponds to a HAROLD(PCI SHOEBOX) board\n");
		break;
	    case KLTYPE_VME:
		printf("\tcorresponds to a VME board\n");
		break;
	    case KLTYPE_MIO:
		printf("\tcorresponds to a MIO board\n");
		break;
	    case KLTYPE_FC:
		printf("\tcorresponds to a FC board\n");
		break;
	    case KLTYPE_LINC:
		printf("\tcorresponds to a LINC board\n");
		break;
	    case KLTYPE_TPU:
		printf("\tcorresponds to a TPU board\n");
		break;
	    case KLTYPE_GSN_A:
		printf("\tcorresponds to a primary GSN board\n");
		break;
	    case KLTYPE_GSN_B:
		printf("\tcorresponds to a auxiliary GSN board\n");
		break;
	    case KLTYPE_XTHD:
		printf("\tcorresponds to a XTHD board\n");
		break;
	    default:
	        if (KLCLASS(io_brd->brd_type) == KLCLASS_GFX)
		    printf("\tcorresponds to a GRAPHICS board\n");
		else
		    printf("\tcorresponds to a UNKNOWN board\n");
		break;
	}
    }
}

static void decode_mspec(i64 a)
{
    printf("\tis in MSPEC space\n");
}

void exec_printaddr(parse_data_t *pd, i64 nargs)
{
    i64		addr, a, region;
    int		bit;

    bit		= nargs > 1 ? (int) parse_pop(pd) : 0;
    addr	= parse_pop(pd);

    a = addr;

    printf("Addr 0x%lx:\n", addr);

    if ((a >= PROM_REGION) && (a < (COMPAT_K1BASE + COMPAT_KSIZE))) {
	printf("\tis on the NODE BOARD CPUPROM.\n");
	return;
    }

    if ((a >= K2BASE) && (a <= (K2BASE + K2SIZE))) {
	decode_prom(a, bit);
	return;
    }

    region = REGION(a);
    REMOVE_REGION(a);

    switch (region) {
    case CAC_BASE:
	decode_cac_uncac(a, 1, bit);
	break;
    case HSPEC_BASE:
	decode_hspec(a, bit);
	break;
    case IO_BASE:
	decode_io(a);
	break;
    case MSPEC_BASE:
	decode_mspec(a);
	break;
    case UNCAC_BASE:
	decode_cac_uncac(a, 0, bit);
	break;
    default:
	printf("Addr 0x%lx does not correspond to a valid region.\n", addr);
    }
}

static i64 load(i64 addr, i64 size)
{
    i64		value;

    switch (size) {
    case 1:
	value = LBU(addr);
	break;
    case 2:
	value = LHU(addr);
	break;
    case 4:
	value = LWU(addr);
	break;
    case 8:
	value = LDU(addr);
	break;
    }

    return value;
}

static void store(i64 addr, i64 size, i64 value)
{
    switch (size) {
    case 1:
	SB(addr, value);
	break;
    case 2:
	SH(addr, value);
	break;
    case 4:
	SW(addr, value);
	break;
    case 8:
	SD(addr, value);
	break;
    }
}

void exec_load(parse_data_t *pd, i64 opts)
{
    i64		nargs	= opts & 0xff;
    i64		size	= opts >> 8;
    i64		count	= nargs > 1 ? parse_pop(pd) : 1;
    i64		addr	= parse_pop(pd);
    int		col	= 0;
    int		maxcol;

    warn_cache(pd, addr);

    /*
     * Special-case size == 0 for ASCII dump
     */

    maxcol = size ? (16 / size) : 8;

    while (count-- && ! kbintr(&pd->next)) {
	if (col == 0)
	    printf("%016llx:", addr);
	if (size)
	    printf(" %0*llx", size * 2, load(addr, size));
	else {
	    int c = (int) load(addr, 1);
	    printf(isprint(c) ? "    %c" : " \\%03o", c);
	}
	if (++col == maxcol) {
	    printf("\n");
	    col = 0;
	}
	addr += size ? size : 1;
    }

    if (col)
	printf("\n");
}

void exec_store(parse_data_t *pd, i64 opts)
{
    i64		nargs	= opts & 0xff;
    i64		size	= opts >> 8;
    i64		count	= nargs > 2 ? parse_pop(pd) : 1;
    i64		value	= nargs > 1 ? parse_pop(pd) : 0;
    i64		addr	= parse_pop(pd);

    warn_cache(pd, addr);

    if (nargs < 2) {
	char		buf[80];

	/* Edit */

	while (1) {
	    printf("%016llx: %0*llx ", addr, size * 2, load(addr, size));
	    if (gets(buf, sizeof (buf)) == 0)
		break;
	    if (buf[0]) {
		if (! isxdigit(buf[0]))	/* Quit */
		    break;
		store(addr, size, strtoull(buf, 0, 16));
	    }
	    addr += size;
	}
    } else {
	/* Fill */

	while (count--) {
	    store(addr, size, value);
	    addr += size;
	}
    }
}

/*
 * sdv
 *
 *   Stores a double, verifies it, and prints if there's a mismatch.
 */

void exec_sdv(parse_data_t *pd)
{
    i64		value	= parse_pop(pd);
    i64		addr	= parse_pop(pd);
    i64		test;

    SD(addr, value);

    test = LD(addr);

    if (test != value)
	printf("*** Stored %y at %y, but read back %y\n",
	       value, addr, test);
}

void exec_getgpr(parse_data_t *pd)
{
    i64		which_reg	= parse_pop(pd);

    parse_push(pd, pd->regs->gpr[which_reg]);
}

void exec_setgpr(parse_data_t *pd)
{
    i64		value		= parse_pop(pd);
    i64		which_reg	= parse_pop(pd);

    if (which_reg == 32)
	which_reg = 31;

    pd->regs->gpr[which_reg] = value;
}

void exec_jump(parse_data_t *pd, i64 nargs)
{
    i64		a1		= nargs > 2 ? parse_pop(pd) : 0;
    i64		a0		= nargs > 1 ? parse_pop(pd) : 0;
    i64		addr		= parse_pop(pd);

    printf("Returned 0x%lx\n",
	   jump_addr(addr, a0, a1, pd->flags));
}

#if 0
void exec_call(parse_data_t *pd, i64 nargs)
{
    i64		a3		= nargs > 4 ? parse_pop(pd) : 0;
    i64		a2		= nargs > 3 ? parse_pop(pd) : 0;
    i64		a1		= nargs > 2 ? parse_pop(pd) : 0;
    i64		a0		= nargs > 1 ? parse_pop(pd) : 0;
    i64		addr		= parse_pop(pd);

    pd->regs->gpr[2] = ((__uint64_t (*)()) addr)(a0, a1, a2, a3);
}
#endif

void exec_call(parse_data_t *pd, i64 nargs)
{
    extern docall(i64, i64, i64, i64, i64, i64);
    i64		a3		= nargs > 4 ? parse_pop(pd) : 0;
    i64		a2		= nargs > 3 ? parse_pop(pd) : 0;
    i64		a1		= nargs > 2 ? parse_pop(pd) : 0;
    i64		a0		= nargs > 1 ? parse_pop(pd) : 0;
    i64		addr		= parse_pop(pd);

    if ((get_BSR() & BSR_KDEBUG) == 0)
	pd->regs->gpr[2] = ((__uint64_t (*)()) addr)(a0, a1, a2, a3);
    else
	pd->regs->gpr[2] =
	    docall(a0, a1, a2, a3, addr, pd->regs->gpr[28]);

}

#define HELP_LINES		23

void exec_help(parse_data_t *pd, i64 nargs)
{
    char	cmd[LEX_STRING_MAX];
    char       *help;

    if (nargs == 0) {
	int		i, lines = 0;

	for (i = 0; cmds[i].help; i++)
	    if (LBYTE(cmds[i].help) != '.') {
		if (! more(&lines, HELP_LINES))
		    break;
		printf("%s%s\n", cmds[i].token ? "   " : "", cmds[i].help);
	    }
    } else {
	parse_pop_string(pd, cmd);

	if ((help = cmd_name2help(cmd)) == 0)
	    printf("Sorry, no help available on \"%s\"\n", cmd);
	else
	    printf("   %s\n", help);
    }
}

/*
 * phwreg
 *
 *   Generic register printer
 */

char *SourceName[4] = {
    "as requested",
    "reset default",
    "loaded from register",
    "loaded from remote register"
};

static void phwreg(hwreg_set_t *regset, hwreg_t *hwreg,
		   i64 addr, i64 value, int source)
{
    printf("Register: %s (%y)\n", regset->strtab + hwreg->nameoff, addr);
    printf("Value   : %y (%s)\n", value, SourceName[source]);

    hwreg_decode(regset, hwreg, 0, 0, 16, value, printf);
}

/*
 * Hub register printing
 */

void exec_phreg_value(parse_data_t *pd, hwreg_t *hwreg)
{
    phwreg(&hwreg_hub, hwreg, (i64) LOCAL_HUB(hwreg->address),
	   parse_pop(pd), 0);
}

void exec_phreg_defl(parse_data_t *pd, hwreg_t *hwreg)
{
    phwreg(&hwreg_hub, hwreg, (i64) LOCAL_HUB(hwreg->address),
	   hwreg_reset_default(&hwreg_hub, hwreg), 1);
}

void exec_phreg_actual(parse_data_t *pd, hwreg_t *hwreg)
{
    __uint64_t		addr	= (i64) LOCAL_HUB(hwreg->address);

    phwreg(&hwreg_hub, hwreg, addr, LD(addr), 2);
}

void exec_phreg_remote(parse_data_t *pd, hwreg_t *hwreg)
{
    nasid_t		nasid	= parse_pop(pd);
    __uint64_t		addr	= (i64) REMOTE_HUB(nasid, hwreg->address);

    phwreg(&hwreg_hub, hwreg, addr, LD(addr), 3);
}

/*
 * Router register printing
 */

void exec_prreg_value(parse_data_t *pd, hwreg_t *hwreg)
{
    phwreg(&hwreg_router, hwreg, hwreg->address, parse_pop(pd), 0);
}

void exec_prreg_defl(parse_data_t *pd, hwreg_t *hwreg)
{
    phwreg(&hwreg_router, hwreg, hwreg->address,
	   hwreg_reset_default(&hwreg_router, hwreg), 1);
}

/*
 * XBOW register printing
 */

void exec_pxreg_value(parse_data_t *pd, hwreg_t *hwreg)
{
    phwreg(&hwreg_xbow, hwreg, XBOW_SN0_BASE(get_nasid()) | hwreg->address,
	   parse_pop(pd), 0);
}

void exec_pxreg_defl(parse_data_t *pd, hwreg_t *hwreg)
{
    phwreg(&hwreg_xbow, hwreg, XBOW_SN0_BASE(get_nasid()) | hwreg->address,
	   hwreg_reset_default(&hwreg_xbow, hwreg), 1);
}

void exec_pxreg_actual(parse_data_t *pd, hwreg_t *hwreg)
{
    __uint64_t		addr	= XBOW_SN0_BASE(get_nasid()) | hwreg->address;

    phwreg(&hwreg_xbow, hwreg, addr, LW(addr), 2);
}

void exec_pxreg_remote(parse_data_t *pd, hwreg_t *hwreg)
{
    nasid_t		nasid	= parse_pop(pd);
    __uint64_t		addr	= (i64) REMOTE_HUB(nasid, hwreg->address);

    phwreg(&hwreg_xbow, hwreg, addr, LW(addr), 3);
}

/*
 * Reset
 */

void exec_reset(parse_data_t *pd)
{
    printf("Resetting the system...\n");
    reset_system();
}

void exec_resume(parse_data_t *pd)
{
    pod_resume();
}

void exec_inval(parse_data_t *pd, i64 nargs)
{
    char		spec[LEX_STRING_MAX];
    int			any	= 0;

    if (nargs)
	parse_pop_string(pd, spec);
    else
	strcpy(spec, "s");

    if (bad_mode(pd, POD_MODE_CAC))
	return;

    if (strchr(spec, 's')) {
	cache_inval_s();
	any = 1;
    } else {
	if (strchr(spec, 'i')) {
	    cache_inval_i();
	    any = 1;
	}
	if (strchr(spec, 'd')) {
	    cache_inval_d();
	    any = 1;
	}
    }

    if (! any)
	printf("No cache(s) specified -- nothing done\n");
}

void exec_flush(parse_data_t *pd)
{
    if (! bad_mode(pd, POD_MODE_DEX))
	cache_flush();
}

void exec_tlbc(parse_data_t *pd, i64 nargs)
{
    if (nargs == 0)
	tlb_flush();
    else {
	printf("Flush by index not implemented\n");
	parse_pop(pd);
    }
}

void exec_tlbr(parse_data_t *pd, i64 nargs)
{
    if (nargs == 0)
	dump_tlb(-1);
    else
	dump_tlb(parse_pop(pd));
}

void exec_dtag(parse_data_t *pd, i64 nargs)
{
    if (nargs == 0)
	dumpPrimaryCache(1);
    else {
	i64	index	= parse_pop(pd);

	dumpPrimaryDataLine((int) index, 0, 0);
	dumpPrimaryDataLine((int) index, 1, 0);
    }
}

void exec_itag(parse_data_t *pd, i64 nargs)
{
    if (nargs == 0)
	printf("Dump-whole-I-cache not implemented\n");
    else {
	i64	index	= parse_pop(pd);

	dumpPrimaryInstructionLine((int) index, 0, 0);
	dumpPrimaryInstructionLine((int) index, 1, 0);
    }
}

void exec_stag(parse_data_t *pd, i64 nargs)
{
    if (nargs == 0)
	dumpSecondaryCache(1);
    else {
	i64	index	= parse_pop(pd);

	dumpSecondaryLine((int) index, 0, 0);
	dumpSecondaryLine((int) index, 1, 0);
    }
}

void exec_dline(parse_data_t *pd)
{
    i64	index	= parse_pop(pd);

    dumpPrimaryDataLine((int) index, 0, 1);
    dumpPrimaryDataLine((int) index, 1, 1);
}

void exec_iline(parse_data_t *pd)
{
    i64	index	= parse_pop(pd);

    dumpPrimaryInstructionLine((int) index, 0, 1);
    dumpPrimaryInstructionLine((int) index, 1, 1);
}

void exec_sline(parse_data_t *pd)
{
    i64	index	= parse_pop(pd);

    dumpSecondaryLine((int) index, 0, 1);
    dumpSecondaryLine((int) index, 1, 1);
}

void exec_adtag(parse_data_t *pd)
{
    i64	index	= parse_pop(pd);

    dumpPrimaryDataLineAddr((int) index, 0, 0);
    dumpPrimaryDataLineAddr((int) index, 1, 0);
}

void exec_aitag(parse_data_t *pd)
{
    i64	index	= parse_pop(pd);

    dumpPrimaryInstructionLineAddr((int) index, 0, 0);
    dumpPrimaryInstructionLineAddr((int) index, 1, 0);
}

void exec_astag(parse_data_t *pd)
{
    i64	index	= parse_pop(pd);

    dumpSecondaryLineAddr((int) index, 0, 0);
    dumpSecondaryLineAddr((int) index, 1, 0);
}

void exec_adline(parse_data_t *pd)
{
    i64	index	= parse_pop(pd);

    dumpPrimaryDataLineAddr((int) index, 0, 1);
    dumpPrimaryDataLineAddr((int) index, 1, 1);
}

void exec_ailine(parse_data_t *pd)
{
    i64	index	= parse_pop(pd);

    dumpPrimaryInstructionLineAddr((int) index, 0, 1);
    dumpPrimaryInstructionLineAddr((int) index, 1, 1);
}

void exec_asline(parse_data_t *pd)
{
    i64	index	= parse_pop(pd);

    dumpSecondaryLineAddr((int) index, 0, 1);
    dumpSecondaryLineAddr((int) index, 1, 1);
}

void exec_sscache(parse_data_t *pd)
{
    i64	base, taglo, taghi;

    taghi = parse_pop(pd);
    taglo = parse_pop(pd);
    base = parse_pop(pd);

    exec_sscache_asm(base, taglo, taghi);
}

void exec_sstag(parse_data_t *pd, i64 nargs)
{
    i64	base, taglo, taghi, way;

    if(nargs == 4)
	way = parse_pop(pd);
    else
	way = 0;
    taghi = parse_pop(pd);
    taglo = parse_pop(pd);
    base = parse_pop(pd);

    printf("%d args.. sstag %lx %lx %lx %lx\n",nargs, base, taglo, taghi, way);
    exec_sstag_asm(base, taglo, taghi,way);
}

void exec_go(parse_data_t *pd)
{
    char	where[LEX_STRING_MAX];

    parse_pop_string(pd, where);

    if (strcmp(where, "dex") == 0)
	pod_mode(POD_MODE_DEX, KLDIAG_PASSED, "Requested DEX mode");

    if (pd->flags->mode != POD_MODE_DEX) {
	printf("Must be in Dex mode before switching to Cac or Unc.\n");
	return;
    }

    read_dip_switches((uchar_t *)NULL);

    if (strcmp(where, "unc") == 0 || strcmp(where, "cac") == 0) {
        printf("Testing/Initializing memory\n");
	memory_copy_prom(DIP_DIAG_MODE());
	memory_clear_prom(DIP_DIAG_MODE());

	if (strcmp(where, "unc") == 0)
	    pod_mode(POD_MODE_UNC, KLDIAG_PASSED, "Requested UNC mode");
	else
	    pod_mode(POD_MODE_CAC, KLDIAG_PASSED, "Requested CAC mode");
    }

    if (strcmp(where, "hell") == 0)
	reset_system();

    printf("Unknown argument: go %s\n", where);
}

void exec_hubsde()
{
    hub_send_data_error_test(DIAG_MODE_HEAVY);
}

void exec_rtrsde()
{
    rtr_send_data_error_test(DIAG_MODE_HEAVY);
}

void exec_chklink()
{
    hub_local_link_diag(DIAG_MODE_HEAVY);
}

/*
 * exec_bist
 *
 *   If opt is 0: BIST string value (BIST on remote hub)
 *   If opt is 1: BIST string (BIST on local hub)
 *   If opt is 2: BIST string value (BIST on router)
 */

void exec_bist(parse_data_t *pd, i64 opt)
{
    char	buf[LEX_STRING_MAX];
    int		hub;
    int		logic;
    int		array;
    int		execute;
    int		results;
    int		force;
    nasid_t	nasid;
    net_vec_t	vec;

    /*
     * Pop args
     */

    switch (opt) {
    case 0:
	hub = 1;
	nasid = (nasid_t) parse_pop(pd);
	break;
    case 1:
	hub = 1;
	nasid = get_nasid();
	break;
    case 2:
	hub = 0;
	vec = parse_pop(pd);
	break;
    }

    parse_pop_string(pd, buf);

    /*
     * Check args
     */

    logic   = (strchr(buf, 'l') || strchr(buf, 'L'));
    array   = (strchr(buf, 'a') || strchr(buf, 'A'));
    execute = (strchr(buf, 'e') || strchr(buf, 'E'));
    results = (strchr(buf, 'r') || strchr(buf, 'R'));
    force   = (strchr(buf, 'f') || strchr(buf, 'F'));

    if (logic & array)	{
	printf("Specify exactly one: L (logic) or A (array).\n");
	return;
    }

    if (execute & results)  {
	printf("Must specify only one of the following options:\n");
	printf("E (execute) or R (results).\n");
	return;
    }

    if (hub) {
	if (execute && ! force) {
	    printf("Warning:  Running BIST will reset system.  Continue? ");
	    gets(buf, sizeof (buf));
	    if (buf[0] != 'y' && buf[0] != 'Y') {
		printf("Cancelled.\n");
		return;
	    }
	}
    }

    if (hub) {
	if (logic ) {
	    if (execute)
		lbist_hub_execute(nasid);
	    if (results)
		lbist_hub_results(nasid, DIAG_MODE_MFG);
	}
	if (array) {
	    if (execute)
		abist_hub_execute(nasid);
	    if (results)
		abist_hub_results(nasid, DIAG_MODE_MFG);
	}
    }

    else { /* router */
	if (logic) {
	    if (execute)
		lbist_router_execute(vec);
	    if (results)
		lbist_router_results(vec, DIAG_MODE_MFG);
	}
	if (array) {
	    if (execute)
		abist_router_execute(vec);
	    if (results)
		abist_router_results(vec, DIAG_MODE_MFG);
	}
    }

}

void exec_enable(parse_data_t *pd, i64 opt)
{
    i64			nasid;
    char		cpu[LEX_STRING_MAX];
    char		mem[MD_MEM_BANKS];
    char		*chr;
    int			bank;

    bzero(cpu, LEX_STRING_MAX);
    bzero(mem, MD_MEM_BANKS);

    if (opt & 1)
	parse_pop_string(pd, cpu);
    else {
	strcpy(cpu, "AB");
	for (bank = 0; bank < MD_MEM_BANKS; bank++)
	   cpu[bank + 2] = '0' + bank;
    }

    nasid = parse_pop(pd);

    if ((nasid == get_nasid()) && (pd->flags->mode == POD_MODE_DEX)) {
        printf("Cannot enable/disable on local node while running in DEX "
		"mode. Try 'go cac'.\n");
        return;
    }

    if (strchr(cpu, 'a') || strchr(cpu, 'A'))
	set_unit_enable(nasid, 0, opt & 4, opt & 2);

    if (strchr(cpu, 'b') || strchr(cpu, 'B'))
	set_unit_enable(nasid, 1, opt & 4, opt & 2);

    for (bank = 0; bank < MD_MEM_BANKS; bank++)
       if (chr = strchr(cpu, '0' + bank)) {
	  memcpy(mem, chr, strlen(chr));
	  set_mem_enable(nasid, mem, opt & 4);
	  break;
       }

    printf("Warning: reset required to take effect\n");
}

void exec_dirinit(parse_data_t *pd)
{
    i64		length	= parse_pop(pd);
    i64		loaddr	= parse_pop(pd);
    int		premium;

    premium = (LD(REMOTE_HUB(NASID_GET(loaddr),
			     MD_MEMORY_CONFIG)) & MMC_DIR_PREMIUM) != 0;

    if ((loaddr | length) & 0xfff)
	printf("Address and length must be page-aligned\n");
    else if (length > 0)
	mdir_init_bddir(premium, loaddr, loaddr + length);
}

void exec_meminit(parse_data_t *pd)
{
    i64		length	= parse_pop(pd);
    i64		loaddr	= parse_pop(pd);
    i64		reg	= (i64) REMOTE_HUB(NASID_GET(loaddr),
					   MD_MEMORY_CONFIG);
    i64		mc, ignore_ecc;

    warn_cache(pd, loaddr);

    mc = LD(reg);

    ignore_ecc = mc & MMC_IGNORE_ECC;

    if (ignore_ecc == 0)
	SD(reg, mc | MMC_IGNORE_ECC);

    memset((char *) loaddr, 0, length);

    if (ignore_ecc == 0)
	SD(reg, mc);
}

void exec_dirtest(parse_data_t *pd)
{
    i64			length	= parse_pop(pd);
    i64			loaddr	= parse_pop(pd);
    int			premium;
    mtest_fail_t	fail;
    i64			obsr;

    premium = (LD(REMOTE_HUB(NASID_GET(loaddr),
			     MD_MEMORY_CONFIG)) & MMC_DIR_PREMIUM) != 0;

    obsr = get_BSR();
    set_BSR(obsr | BSR_VERBOSE);

    if ((loaddr | length) & 0xfff)
	printf("Address and length must be page-aligned\n");
    else if (length > 0)
	mtest_dir(premium,
		  loaddr, loaddr + length,
		  DIAG_MODE_HEAVY, &fail, pd->flags->maxerr);

    set_BSR(obsr);

    printf("Warning: directory contents are now randomized (see dirinit)\n");
}

void exec_memtest(parse_data_t *pd)
{
    i64			length	= parse_pop(pd);
    i64			loaddr	= parse_pop(pd);
    mtest_fail_t	fail;
    i64			obsr;

    warn_cache(pd, loaddr);

    obsr = get_BSR();
    set_BSR(obsr | BSR_VERBOSE);

    mtest(loaddr, loaddr + length,
	  DIAG_MODE_HEAVY, &fail, pd->flags->maxerr);

    set_BSR(obsr);
}

void exec_santest(parse_data_t *pd)
{
    i64		addr	= parse_pop(pd);
    int		premium;

    warn_cache(pd, addr);

    premium = (LD(REMOTE_HUB(NASID_GET(addr),
			     MD_MEMORY_CONFIG)) & MMC_DIR_PREMIUM) != 0;

    printf("Sanity test %s.\n",
	   mtest_basic(premium, addr, 1) < 0 ? "failed" : "passed");
}

void exec_slave(parse_data_t *pd)
{
    LAUNCH_LOOP();
}

void exec_segs(parse_data_t *pd, i64 nargs)
{
    int		flag = 1;

    if (nargs)
	flag = parse_pop(pd);

    load_execute_segment(0, flag, DIP_DIAG_MODE());	/* NULL seg_name just lists */
}

void exec_exec(parse_data_t *pd, i64 nargs)
{
    char	seg[LEX_STRING_MAX];
    int		flag = 1;

    if (bad_mode(pd, POD_MODE_DEX))
	return;

    switch (nargs) {
    case 0:
	strcpy(seg, DEFAULT_SEGMENT);
	break;
    case 1:
	parse_pop_string(pd, seg);
	break;
    case 2:
	flag = parse_pop(pd);
	parse_pop_string(pd, seg);
	break;
    }

    load_execute_segment(seg, flag, DIP_DIAG_MODE());
}

void exec_why(parse_data_t *pd)
{
    char	tmp[80];

    print_exception(pd->regs, pd->flags);

    printf(" POD mode was called from: %y (%P)\n",
	   pd->flags->pod_ra, pd->flags->pod_ra);

    pd->flags->scroll_msg = 1;
}

/*
 * Simple stack backtrace (7.1 and up compilers only)
 */

#define ISN_OP()	((*(__uint32_t *) pc) >> 16)
#define ISN_ARG()	((*(__uint32_t *) pc) & 0xffff)

void exec_btrace(parse_data_t *pd, i64 nargs)
{
    jmp_buf	fault_buf;
    void       *old_buf;
    i64		sp, pc;
    i64		ra, func;
    int		frm, i, x, y;

    if (nargs) {
	sp = parse_pop(pd);
	pc = parse_pop(pd);
    } else {
	sp = pd->regs->gpr[29];		/* sp */
	pc = pd->regs->epc;
    }

    if (setfault(fault_buf, &old_buf)) {
	printf("Exception terminated trace\n");
	return;
    }

    printf("  %-32sCALLED FROM:\n", "FUNCTION:");

    for (frm = 0; frm < 32; frm++) {
	/* Search backwards for "daddiu sp,sp,-x" */
	for (i = 10000; i > 0; i--) {
	    if (ISN_OP() == 0x67bd && ISN_ARG() > 0x8000)
		break;
	    pc -= 4;
	}
	if (! i) {
	    printf("Can't find start of function\n");
	    return;
	}
	func = pc;
	x = 0x10000 - ISN_ARG();
	/* Search forwards for "sd ra,y(sp)" */
	for (i = 32; i > 0; i--) {
	    pc += 4;
	    if (ISN_OP() == 0xffbf)
		break;
	}
	if (! i) {
	    printf("Can't find where ra was saved\n");
	    return;
	}
	y = ISN_ARG();
	ra = LD(sp + y);
	printf("    %-32P%P\n", func, ra - 8);
	sp += x;
	pc = ra - 8;
    }

    restorefault(old_buf);
}

void exec_reconf(parse_data_t *pd)
{
    u_short prem_mask;
    if (bad_mode(pd, POD_MODE_CAC) || bad_mode(pd, POD_MODE_UNC))
	return;

    mdir_init(get_nasid(), IP27CONFIG.freq_hub);
    mdir_config(0, &prem_mask);
}

void exec_clear(parse_data_t *pd)
{
    mdir_error_get_clear(get_nasid(), 0);
    printf("Cleared memory errors.\n");
#if 0
    error_clear_io6();
    printf("Cleared IO6 errors.\n");
#endif
}

void exec_error(parse_data_t *pd)
{
    mdir_error_decode(get_nasid(), 0);
}

void exec_ioc3(parse_data_t *pd)
{
    console_t		*console = KL_CONFIG_CH_CONS_INFO(get_nasid());
    console_t		ioc3_console;
    __psunsigned_t	ioc3_base;

    if (!(ioc3_base = get_ioc3_base(get_nasid()))) {
        printf("No IOC3 found.\n");
        return;
    }

    bzero(&ioc3_console, sizeof(console_t));

    discover_baseio_console(ioc3_base, KLTYPE_IO6, &ioc3_console,
		NULL, DIAG_MODE_NONE, 1);

    if (!ioc3_console.uart_base) {
        printf("IOC3 found, but its not console.\n");
        return;
    }

    memcpy(console, &ioc3_console, sizeof(console_t));

    printf("Switching to IOC3 UART\n");
    ioc3uart_init(console);
    libc_device(&dev_ioc3uart);
    putc('\n');
}

void exec_junk(parse_data_t *pd)
{
    if (junkuart_probe()) {
	printf("Switching to Junk UART\n");
	libc_device(&dev_junkuart);
	putc('\n');
    } else
	printf("Junk UART not responding.\n");
}

void exec_elsc(parse_data_t *pd)
{
    if (elscuart_probe()) {
	printf("Switching to ELSC UART\n");
	libc_device(&dev_elscuart);
	putc('\n');
    } else
	printf("ELSC not responding.\n");
}

void exec_nm(parse_data_t *pd)
{
    printf("%P\n", parse_pop(pd));
}

#define VEC_TRIES		10000
#define PIO_ID			0x123ULL
#define WRITE_ID		0ULL

static int vecop(i64 vec, i64 address, i64 piotype, i64 *data)
{
    i64		status;
    int		tries;
    int		r = -1;

#if defined(SABLE) || 0
    /*
     * Fake vector I/O in Sable by just asking user!
     */

    printf("VECOP: vec=0x%llx, address=0x%llx, type=%s",
	   vec, address & 0xfffffULL,
	   piotype == PIOTYPE_READ ? "read" :
	   piotype == PIOTYPE_WRITE ? "write" : "exch");

    if (piotype != PIOTYPE_READ)
	printf(", data=0x%llx\n", *data);
    else
	printf("\n");

    if (piotype != PIOTYPE_WRITE) {
	char	buf[80];
	printf("ANSWER: ");
	gets(buf, sizeof (buf));
	if (buf[0] != 'q') {	/* q is a way to simulate failure */
	    *data = strtoull(buf, 0, 0);
	    r = 0;
	}
    } else
	r = 0;

#else /* SABLE */

    if (! net_link_up()) {
	printf("Local network link is down.\n");
	goto done;
    }

    LD(LOCAL_HUB(NI_VECTOR_CLEAR));
    SD(LOCAL_HUB(NI_VECTOR), vec);
    SD(LOCAL_HUB(NI_RETURN_VECTOR), 0xdeadbeefdeadbeef);
    SD(LOCAL_HUB(NI_VECTOR_READ_DATA), 0xdeadbeefdeadbeef);
    SD(LOCAL_HUB(NI_VECTOR_DATA),
       (piotype != PIOTYPE_READ) ? *data : 0xdeadbeefdeadbeef);

    SD(LOCAL_HUB(NI_VECTOR_PARMS),
       PIO_ID << NVP_PIOID_SHFT |	/* Unique PIO ID number */
       WRITE_ID << NVP_WRITEID_SHFT |
       address & NVP_ADDRESS_MASK |
       piotype);

    for (tries = 0; tries < VEC_TRIES; tries++)
	if ((status = LD(LOCAL_HUB(NI_VECTOR_STATUS))) & NVS_VALID)
	    break;

    if (tries == VEC_TRIES)
	printf("Vector status not valid after %d polls.\n", tries);
    else {
	int	err = 0;

	printf("Status:");

	if (status & NVS_VALID)
	    printf(" VALID");

	if (status & NVS_OVERRUN) {
	    printf(" OVERRUN");
	    err = 1;
	}

	if ((status & NVS_TYPE_MASK) == PIOTYPE_ADDR_ERR) {
	    printf(" ADDR_ERR");
	    err = 1;
	}

	if ((status & NVS_TYPE_MASK) == PIOTYPE_CMD_ERR) {
	    printf(" CMD_ERR");
	    err = 1;
	}

	if ((status & NVS_TYPE_MASK) == PIOTYPE_PROT_ERR) {
	    printf(" PROT_ERR");
	    err = 1;
	}

	if (GET_FIELD(status, NVS_PIOID) != PIO_ID) {
	    printf(" PIO_ID_MISMATCH");
	    err = 1;
	}

	if (GET_FIELD(status, NVS_WRITEID) != WRITE_ID) {
	    printf(" WRITE_ID_MISMATCH");
	    err = 1;
	}

	if ((status & NVS_ADDRESS_MASK) != (address & NVP_ADDRESS_MASK)) {
	    printf(" ADDRESS_MISMATCH");
	    err = 1;
	}

	if (err)
	    printf(" TARGET=%ld\n", GET_FIELD(status, NVS_TARGET));

	printf(" (%d polls)\n", tries);

	if (err) {
	    printf(" NI_RETURN_VECTOR    = 0x%lx\n",
		   LD(LOCAL_HUB(NI_RETURN_VECTOR)));
	    printf(" NI_VECTOR_READ_DATA = 0x%lx\n",
		   LD(LOCAL_HUB(NI_VECTOR_READ_DATA)));
	}

	if (! err)
	    r = 0;
    }

    if (piotype != PIOTYPE_WRITE)
	*data = LD(LOCAL_HUB(NI_VECTOR_READ_DATA));

#endif /* SABLE */

 done:

    return r;
}

void exec_vecr(parse_data_t *pd)
{
    i64		address		= parse_pop(pd);
    i64		vec		= parse_pop(pd);
    i64		value;

    if (vecop(vec, address, PIOTYPE_READ, &value) == 0)
	printf("RdData: %y\n", value);
}

void exec_vecw(parse_data_t *pd)
{
    i64		value		= parse_pop(pd);
    i64		address		= parse_pop(pd);
    i64		vec		= parse_pop(pd);

    (void) vecop(vec, address, PIOTYPE_WRITE, &value);
}

void exec_vecx(parse_data_t *pd)
{
    i64		value		= parse_pop(pd);
    i64		address		= parse_pop(pd);
    i64		vec		= parse_pop(pd);

    if (vecop(vec, address, PIOTYPE_EXCHANGE, &value) == 0)
	printf("RdData: %y\n", value);
}

void exec_disc(parse_data_t *pd)
{
    read_dip_switches((uchar_t *)NULL);
    run_discover(DIP_DIAG_MODE());
}

void exec_pcfg(parse_data_t *pd, i64 nargs)
{
    nasid_t	nasid;
    int		dflags = DDUMP_PCFG_ALL;

    if (nargs & 0x2) {
       char	flags[LEX_STRING_MAX];

       parse_pop_string(pd, flags);
       if (strchr(flags, 'v'))
	  dflags = DDUMP_PCFG_VERS;
    }

    nasid = (nargs & 0x1) ? (nasid_t) parse_pop(pd) : get_nasid();

    printf("SN0net Topology (node %d):\n", nasid);

    discover_dump_promcfg(PCFG(nasid), printf, dflags);
}

void exec_node(parse_data_t *pd, i64 nargs)
{
    i64		vec	= nargs > 1 ? parse_pop(pd) : 0;
    i64		value	= nargs > 0 ? parse_pop(pd) : 0;
    i64		reg;

    printf("Warning: changing the node ID is very dangerous.\n");

    if (nargs == 1) {
	printf("Warning: the memory/directories may be set up for node %d.\n",
	       get_nasid());
	/* Set local node ID to value */
	net_node_set((int) value);
    } else {
	printf("Warning: remote node will crash.\n");
	/* Set remote node ID to value via vector r/m/w */
	if (vecop(vec, NI_STATUS_REV_ID, PIOTYPE_READ, &reg) < 0)
	    return;
	SET_FIELD(reg, NSRI_NODEID, value);
	if (vecop(vec, NI_STATUS_REV_ID, PIOTYPE_WRITE, &reg) < 0)
	    return;
    }
}

void exec_crb(parse_data_t *pd, i64 nargs)
{
    i64		base;
    int		ind;

    base = (i64) (nargs ?
		  REMOTE_HUB((nasid_t) parse_pop(pd), 0) :
		  LOCAL_HUB(0));

    printf("   "
	   "_____CRB_A______ _____CRB_B______ "
	   "_____CRB_C______ _____CRB_D______\n");

    for (ind = 0; ind < 15; ind++)
	printf("%x: %016llx %016llx %016llx %016llx\n",
	       ind,
	       LD(base + IIO_ICRB_A(ind)),
	       LD(base + IIO_ICRB_B(ind)),
	       LD(base + IIO_ICRB_C(ind)),
	       LD(base + IIO_ICRB_D(ind)));
}

/*
 * Extended CRB dump courtesy of Chris Satterlee
 */

void exec_crbx(parse_data_t *pd, i64 nargs)
{
    nasid_t	nasid;
    int		ind;
    icrba_t	crb_a;
    icrbb_t	crb_b;
    icrbc_t	crb_c;
    icrbd_t	crb_d;
    char	imsgtype[3];
    char	imsg[8];
    char	reqtype[6];

    nasid = nargs ? (nasid_t) parse_pop(pd) : get_nasid();

    crbx(nasid, printf);		/* libkl */
}

/*
 * exec_route
 *
 *   Takes a vector and a the node ID of a remote node.
 *   Programs the remote node's node ID.
 *   Programs the local node's router tables, the remote node's router
 *   tables, and the intervening router's router tables, to establish
 *   valid routing between the local and remote node.
 *   Remote memory accesses to the remote node will become possible.
 */

static void do_route(nasid_t nasid, net_vec_t vec)
{
    int			r;
    i64			reg;

    if (nasid > 31)
	printf("*** Warning: NASIDs greater than 31 not handled.\n");

    printf("*** Programming nasid %d path 0x%lx\n", nasid, vec);

    if ((r = net_program(vec, get_nasid(), nasid)) < 0) {
	printf("Network programming failed: %s\n", net_errmsg(r));
	goto done;
    }

    printf("Set remote node ID to %d\n", nasid);

    if ((r = net_node_set_remote(vec, nasid)) < 0)
	printf("Failed: %s\n", net_errmsg(r));

    /*
     * Send an NMI to the remote hub right ASAP to reduce the amount
     * of time spends running around in chicken-with-it's-head-cut-off
     * mode.
     */

    SD(REMOTE_HUB(nasid, PI_NMI_A), 1);
    SD(REMOTE_HUB(nasid, PI_NMI_B), 1);

    printf("Programming complete.\n");

 done:
    return;
}

void exec_route(parse_data_t *pd, i64 nargs)
{
    nasid_t		nasid;
    i64			vec, reg;

    if (nargs != 0) {
	nasid	= (nasid_t) parse_pop(pd);
	vec	= parse_pop(pd);
    }

#ifndef SABLE
    if (! net_link_up()) {
	printf("Local network link is down.\n");
	return;
    }
#endif /* SABLE */

    if (nargs > 0)
	do_route(nasid, vec);
    else {
	pcfg_t	       *p;
	int		i, r;

	if (bad_mode(pd, POD_MODE_DEX))
	    return;

	p = PCFG(get_nasid());

	if (run_discover(DIAG_MODE_NONE) < 0)
	    return;

	if (nasid_assign(p, 0) < 0)
	    return;

	printf("NASIDS are:");

	for (i = 1; i < p->count; i++)
	    if (p->array[i].any.type == PCFG_TYPE_HUB)
		printf(" %d\n", p->array[i].hub.nasid);

	printf("\nUse pcfg to see more info.\n");

	if (route_assign(p) < 0)
	    return;

	if (distribute_tables() < 0)
	    return;

	db_printf("Distributing NASIDs\n");

	for (i = 1; i < p->count; i++)
	    if (p->array[i].any.type == PCFG_TYPE_HUB)
		if ((r = net_node_set_remote(vec,
					     p->array[i].hub.nasid)) < 0) {
		    printf("Could not set remote node ID: %s\n",
			   net_errmsg(r));
		    return;
		}

	printf("Done\n");
    }
}

/*
 * exec_flash
 *
 *   Flashes a PROM on up to 8 remote hubs with a copy of the contents
 *   of the PROM on the flashing node.
 */

static int hub_accessible(nasid_t nasid)
{
    jmp_buf		fault_buf;
    void	       *old_buf;
    i64			reg, tst;
    int			r = 0;

    if (setfault(fault_buf, &old_buf)) {
	printf("%03d> Remote access resulted in an exception\n", nasid);
	goto done;
    }

    reg = (i64) REMOTE_HUB(nasid, MD_PERF_CNT5);

    tst = 0x5555ULL;
    SD(reg, tst);
    if ((LDU(reg) & 0xffffULL) != tst) {
	printf("%03d> Remote scratch register write/read 5 miscompared\n",
	       nasid);
	goto done;
    }

    tst = 0xaaaaULL;
    SD(reg, tst);
    if ((LDU(reg) & 0xffffULL) != tst) {
	printf("%03d> Remote scratch register write/read A miscompared\n",
	       nasid);
	goto done;
    }

    r = 1;

 done:

    restorefault(old_buf);
    return r;
}

static void
copy_buffer_adj_sum(ip27config_t *old, ip27config_t *new)
{
    __uint64_t             old_sum, new_sum;

    old_sum = memsum((void *) old, sizeof(ip27config_t))%256;

    new_sum = memsum((void *) new, sizeof(ip27config_t))%256;

    /* 
     * now set the appropiate check sum adjust value so
     * that we're back to summing to 0 % 256 
     */

    if (old_sum>new_sum)
        new->check_sum_adj = (uint) (old_sum - new_sum);
    else
        new->check_sum_adj = (uint) (256 - (new_sum - old_sum));

    new->check_sum_adj &= 0xff;
}

static int
parse_megahertz(char *s)
{
    int f = 1000000, n = atoi(s) * f;

    if ((s = strchr(s, '.')) != 0)
        while (*++s)
            n += (*s - '0') * (f /= 10);

    return n;
}


/*
 * returns the CPU mode bits setting for secondary
 * cache size.
 *
 * same format and options for IP27 and IO6
 */

char *cache_size_mb[8] = { "0.5","1","2","4","8","16","RSV","RSV" };

__uint32_t
get_scache_size(__uint32_t def_mode)
{
    __uint32_t result;
    char       buf[8];
    int        cache;

    cache = -1;

    while (cache == -1) {

        printf("%-35s [%5s] : ","Scache size in MB",
               cache_size_mb[((def_mode & IP27C_R10000_SCS_MASK) >> 
					  IP27C_R10000_SCS_SHFT)]);

        if (gets(buf, sizeof buf) && strlen(buf)) {

            if (!strcmp(buf,"0.5"))
                cache = 0;

            switch(atoi(buf)) {
                case  1 : cache =  1 ; break;
                case  2 : cache =  2 ; break;
                case  4 : cache =  3 ; break;
                case  8 : cache =  4 ; break;
                case 16 : cache =  5 ; break;
                default :              break;
            }

            if (cache == -1)
                    printf("Invalid secondary cache size, try again\n");
        } else {
                return def_mode;
        }
    }

    result  = def_mode & (~IP27C_R10000_SCS_MASK); 
    result |= (cache << IP27C_R10000_SCS_SHFT);

    return result;
}

#define	MHZ(f)			(f / 1000000)
extern	int ip31_get_config(nasid_t nasid, ip27config_t *c);

/*
 * the define should move to /usr/include/sys/R10k.h
 */
#ifndef R10000_DDR_SHFT
#define R10000_DDR_SHFT 23
#endif


static int
fill_in_sn_cfg(nasid_t nasid, ip27config_t *c)
{
    	char buf[16];
    	int  tap,cache,div,onetap,is_r14k,SysClkDiv,SCClkDiv,outreq;

    	printf("\nEnter values for nasid %d\n", nasid);

    	memcpy((void *) c, (void *) IP27CONFIG_ADDR_NODE(nasid),
		sizeof(ip27config_t));

    	if (ip31_pimm_psc(nasid, NULL,NULL) >= 0)
        	ip31_get_config(nasid, c);

    	printf("%-35s [%5d] : ","CPU frequency (MHZ)", MHZ(c->freq_cpu));
    	if (gets(buf, 16) && strlen(buf))
        	c->freq_cpu = parse_megahertz(buf);

    	printf("%-35s [%5d] : ","Hub frequency (MHZ)", MHZ(c->freq_hub));
    	if (gets(buf, 16) && strlen(buf))
        	c->freq_hub = parse_megahertz(buf);

    	printf("%-35s [%5d] : ","Machine type (0)SN0 (1)SN00",c->mach_type);
    		if (gets(buf, 16) && strlen(buf))
        		c->mach_type = (uint) atoi(buf);

	is_r14k = 0;
        printf("%-35s [%5d] : ","CPU type (0)R10K/R12K (1)R14K",is_r14k);
        if (gets(buf,16) && strlen(buf) && atoi(buf) == 1)
                is_r14k = 1;

    	c->r10k_mode = get_scache_size(c->r10k_mode);

        if (is_r14k) {
                printf("%-35s [%5d] : ","DDR SRam",0);
                if (gets(buf,16) && strlen(buf) && atoi(buf) == 1)
                        c->r10k_mode |=  (1 << R10000_DDR_SHFT);
		else
			c->r10k_mode &= ~(1 << R10000_DDR_SHFT);
        }


    	printf("%-35s [%5d] : ","Tandem Mode bit",
		(c->r10k_mode & R10000_SCCE_MASK) >> R10000_SCCE_SHFT);
    	if (gets(buf, 16) && strlen(buf)) {
		if (atoi(buf) == 0)
			c->r10k_mode &= ~(1 << R10000_SCCE_SHFT);
		else
			c->r10k_mode |=  (1 << R10000_SCCE_SHFT);
    	}
	      


        /*
         * we have a pretty good idea how the divisor should look like
         * given the hub and CPU frequencies, so we'll use this as
         * the default
         */
        div = ((MHZ(c->freq_cpu) * 2) / MHZ(c->freq_hub)) - 1;
    	printf("%-35s [%5d] : ","SysClk Divisor (PClk/SysClk ratio)",div);
    	if (gets(buf, 16) && strlen(buf))
        	div = atoi(buf);
    	c->r10k_mode &= ~R10000_SCD_MASK;
    	c->r10k_mode |= ((div << R10000_SCD_SHFT) & R10000_SCD_MASK);

    	div = (c->r10k_mode & R10000_SCCD_MASK) >> R10000_SCCD_SHFT;
    	printf("%-35s [%5d] : ","SCClk Divisor (PClk/SCClk ratio)", div);
    	if (gets(buf, 16) && strlen(buf))
        	div = atoi(buf);
    	c->r10k_mode &= ~R10000_SCCD_MASK;
    	c->r10k_mode |= ((div << R10000_SCCD_SHFT) & R10000_SCCD_MASK);

    	tap = (c->r10k_mode & R10000_SCCT_MASK) >> R10000_SCCT_SHFT;
    	printf("%-35s [  0x%x] : ","SCClk Tap", tap);
    	if (gets(buf, 16) && strlen(buf))
        	tap = strtoull(buf, NULL, 16);
    	c->r10k_mode &= ~R10000_SCCT_MASK;
    	c->r10k_mode |= ((tap << R10000_SCCT_SHFT) & R10000_SCCT_MASK);

	outreq = 0x3;
	printf("%-35s [  0x%x] : ","Outstanding requests", outreq);
	if (gets(buf, 16) && strlen(buf))
		outreq = strtoull(buf, NULL, 16) & 0x3;
	c->r10k_mode &= ~R10000_PRM_MASK;
	c->r10k_mode |= ((outreq << R10000_PRM_SHFT) & R10000_PRM_MASK);

    	c->check_sum_adj = (uint) 0;

    	c->flash_count++;


	printf("\n");

	/*
	 *	Verification
	 *	------------
	 */
    	printf("CPU    freq\t%3d MHz\n", MHZ(c->freq_cpu));
	printf("CPU    type\t%s\n"     , is_r14k ? "R14K":"R10K/R12k");
    	printf("Hub    freq\t%3d MHz\n", MHZ(c->freq_hub));

    	/*
     	 * Secondary cache information
     	 */
	SysClkDiv = (c->r10k_mode & R10000_SCD_MASK) 
						>> R10000_SCD_SHFT;
	SCClkDiv  = (c->r10k_mode & R10000_SCCD_MASK)
						>> R10000_SCCD_SHFT;

	/*
	 * if this is R14K running in DDR mode we need to twist 
	 * the SCClkDiv a bit in order to compute the correct
	 * scache speed. Also handle siliconn test modes in R14k
	 *
	 *      Code    R10K/R12K    R14K SDR     R14K DDR
	 *        0       rsv          rsv           rsv
	 *        1	   1            1             2
	 *        2        1.5          1.5           2
	 *        3        2            2	      2
	 *        4        2.5          2.5           2
	 *        5        3            3             3
	 *        6       rsv           6             6
	 *        7       rsv           4             4
 	 *
	 */
	if (is_r14k) {
		/*
		 * handle silicon test modes. Code 7 translates
		 * correctly to a div 4.0
		 */
		if (SCClkDiv == 6)
			SCClkDiv = 11;

		if (c->r10k_mode & (1 << R10000_DDR_SHFT)) {
			switch(SCClkDiv) {
			case 1 :
			case 2 :
			case 4 :
				SCClkDiv = 3;
				break;
			default:
				break;
			}
		}

		printf("Scache mode\t%s\n",
			c->r10k_mode & (1 << R10000_DDR_SHFT) ?
			"DDR":"SDR");
	}

	if (is_r14k && (c->r10k_mode & (1 << R10000_DDR_SHFT))) {
    		printf("Scache freq\tread %3d MHz / write %3d Mhz\n",
         		(MHZ(c->freq_hub) * 
				(SysClkDiv + 1) / (SCClkDiv + 1)) * 2,
         		MHZ(c->freq_hub) * 
				(SysClkDiv + 1) / (SCClkDiv + 1));
	} else {
    		printf("Scache freq\t%3d MHz\n",
         		MHZ(c->freq_hub) * 
				(SysClkDiv + 1) / (SCClkDiv + 1));
	}

    	printf("Scache Tap\t[0x%x]  ",tap);

    	onetap = (int) 1E6 / MHZ(c->freq_cpu) / 12;

    	switch (tap) {
    	case 0x0 : printf("same phase as internal clock"); break;
    	case 0x1 : printf("%d ps",onetap * 1);             break;
    	case 0x2 : printf("%d ps",onetap * 2);             break;
    	case 0x3 : printf("%d ps",onetap * 3);             break;
    	case 0x4 : printf("%d ps",onetap * 4);             break;
    	case 0x5 : printf("%d ps",onetap * 5);             break;
    	case 0x6 : printf("UNDEFINED");                    break;
    	case 0x7 : printf("UNDEFINED");                    break;
    	case 0x8 : printf("%d ps",onetap * 6);             break;
    	case 0x9 : printf("%d ps",onetap * 7);             break;
    	case 0xa : printf("%d ps",onetap * 8);             break;
    	case 0xb : printf("%d ps",onetap * 9);             break;
    	case 0xc : printf("%d ps",onetap * 10);            break;
    	case 0xd : printf("%d ps",onetap * 11);            break;
    	case 0xe : printf("UNDEFINED");                    break;
    	case 0xf : printf("UNDEFINED");                    break;
    	default  : printf("INVAL \"%d\"",tap);             break;
    	}
    	printf(", tap inc %d ps\n",onetap);


    	printf("Machine type\t%s\n", 
		c->mach_type == SN00_MACH_TYPE ? "SN00" : "SN0");
    	printf("Cache size\t%s MB\n",
         	cache_size_mb[((c->r10k_mode & 0x00070000) 
					    >> IP27C_R10000_SCS_SHFT)]);
    	printf("Tandem mode\t%d\n",
         	(c->r10k_mode & R10000_SCCE_MASK) 
					    >> R10000_SCCE_SHFT);
    	printf("R10K mode\t0x%x\n", c->r10k_mode);

        printf("RTC freq (KHZ)\t%d\n",c->freq_rtc / 1000);

    	copy_buffer_adj_sum((ip27config_t *) IP27CONFIG_ADDR, c);

    	printf("\nProceed ? [n] ");
    	if (!gets(buf, 16) || ((buf[0] != 'y') && (buf[0] != 'Y')))
        	return -1;

    	return 0;
}

typedef struct flasher_parm_s {
    corp_t	       *corp;
    rtc_time_t		kbtime;
} flasher_parm_t;

static int flasher_abort(fprom_t *f)
{
    flasher_parm_t     *fp = (flasher_parm_t *) f->aparm;

    if (fp->corp)
	corp_sleep(fp->corp, 0);

    return kbintr(&fp->kbtime);
}

typedef struct flasher_info_s {
    nasid_t		nasid;
    ip27config_t	ip27config;
} flasher_info_t;

/*
 * FLASH_READLEN should entirely contain ip27config
 * i.e. ASSERT(IP27CONFIG_OFFSET + sizeif(ip27config_t) < FLASH_READLEN)
*/

#define	FLASH_READLEN	256

static void flasher(corp_t *c, __uint64_t arg)
{
    i64			value;
    char		buf[FLASH_READLEN];
    char	       *src;
    int			n, r, wr;
    int			manu_code, dev_code;
    off_t		amt_done;
    fprom_t		f;
    flasher_parm_t     *fp;
    flasher_info_t     *fi = (flasher_info_t *) arg;
    nasid_t		nasid = fi->nasid;
    promlog_t		l;

    /*
     * Set up the remote FPROM_CYC and FPROM_WR values in case the node
     * is headless and these are not yet initialized.
     * XXX avoid bogus fprom_wr (temporary)
     */

    wr = IP27CONFIG.fprom_wr ? IP27CONFIG.fprom_wr : 1;

    value = LD(REMOTE_HUB(nasid, MD_MEMORY_CONFIG));
    value &= ~(MMC_FPROM_CYC_MASK | MMC_FPROM_WR_MASK);
    value |= ((__uint64_t) IP27CONFIG.fprom_cyc << MMC_FPROM_CYC_SHFT |
	      (__uint64_t) wr << MMC_FPROM_WR_SHFT);
    SD(REMOTE_HUB(nasid, MD_MEMORY_CONFIG), value);

    /*
     * Erase the PROM
     */

    f.base	= (void *) NODE_RBOOT_BASE(nasid);
    f.dev	= FPROM_DEV_HUB;
    f.afn	= flasher_abort;

    fp		= (flasher_parm_t *) f.aparm;

    fp->corp	= c;
    fp->kbtime	= 0;

    r = fprom_probe(&f, &manu_code, &dev_code);

    if (r < 0 && r != FPROM_ERROR_DEVICE) {
	printf("%03d> Error initializing remote PROM: %s\n",
	       nasid, fprom_errmsg(r));
	goto done;
    }

    printf("%03d> Manufacturer: 0x%02x, Device: 0x%02x\n",
	   nasid, manu_code, dev_code);

    if (r == FPROM_ERROR_DEVICE)
	printf("%03d> *** Warning: Unknown manufacture/device code\n", nasid);

    printf("%03d> Erasing code sectors (15 to 20 seconds)\n", nasid);

    hub_led_set(~0UL);

    hub_led_set_remote(nasid, 0, ~0UL);
    hub_led_set_remote(nasid, 1, ~0UL);

    if ((r = fprom_flash_sectors(&f, 0x3fff)) < 0) {
	printf("%03d> Error erasing remote PROM: %s\n",
	       nasid, fprom_errmsg(r));
	goto done;
    }

    printf("%03d> Erasure verified\n", nasid);

    /*
     * Program the PROM
     */

    printf("%03d> Programming\n", nasid);

    src = (char *) LBOOT_BASE;
    amt_done = 0;

    while ((n = TEXT_LENGTH - amt_done) > 0) {
	hub_led_set(~(0x7f >> amt_done * 8 / TEXT_LENGTH));

	hub_led_set_remote(nasid, 0, 0xfe << amt_done * 8 / TEXT_LENGTH);
	hub_led_set_remote(nasid, 1, ~(1 << (rtc_time() >> 16 & 7)));

	if (n > sizeof (buf))
	    n = sizeof (buf);

	memcpy(buf, src, n);


        /*
         * ip27config is contained in the first buf entirely. Copy the one
         * that we wanted to flash
         */

        if (!amt_done)
            memcpy((void *) (buf + CONFIG_INFO_OFFSET), 
		(void *) &fi->ip27config, sizeof(ip27config_t));

	if ((r = fprom_write(&f, amt_done, buf, n)) < 0) {
	    printf("%03d> Error programming remote PROM: %s\n",
		   nasid, fprom_errmsg(r));
	    goto done;
	}

	src += n;
	amt_done += n;

	if ((amt_done & 0xffff) == 0 || amt_done == TEXT_LENGTH)
	    printf("%03d> Wrote 0x%05lx bytes\n", nasid, amt_done);
    }

    printf("%03d> Program data verified\n", nasid);

    l.f = f;

    if (ip27log_setup(&l, nasid, IP27LOG_MY_FPROM_T) == PROMLOG_ERROR_MAGIC) {
	printf("%03d> Initializing invalid PROM Log\n", nasid);

	(void) ip27log_setup(&l, nasid,
			     IP27LOG_MY_FPROM_T | IP27LOG_DO_INIT |
			     IP27LOG_VERBOSE);
    }

 done:
    ;
}

void exec_flash(parse_data_t *pd, i64 op)
{
    int			nodes, i, diff = 0;
    nasid_t		nasid;
    __uint64_t		x;
    flasher_info_t	fi[IP27PROM_CORP_MAX];

    if (TEXT_LENGTH > 0xe0000) {
	printf("*** PROM too large (exceeds 14 sectors) ***\n");
	return;
    }

    if (pd->flags->mode == POD_MODE_DEX) {
	if (get_sp() < POD_STACKVADDR) {
		printf("*** POD stack overflow ***\n");
		return;
	}

	printf("*** WARNING: Flashing in Dex mode is very slow "
	       "(use 'go cac' if possible)\n");
    }

    /*
     * Pop list of nodes off stack
     */

    nodes = 0;

    while ((x = parse_pop(pd)) != 0xbabababaUL)
	if (nodes < IP27PROM_CORP_MAX)
            fi[nodes++].nasid = (nasid_t) x;

    for (i = 0; i < nodes; i++)
	if (! hub_accessible(fi[i].nasid)) {
	    printf("%03d> Remote hub is not responding\n", fi[i].nasid);
	    return;
	}

    for (i = 0; i < nodes; i++)
        if (op & 0x1) {
            if (fill_in_sn_cfg(fi[i].nasid, &fi[i].ip27config) < 0)
                return;
        }
        else {
            memcpy((void *) &fi[i].ip27config, (void *) IP27CONFIG_ADDR,
			sizeof(ip27config_t));

            if (ip31_pimm_psc(fi[i].nasid, NULL, NULL) >= 0)
                ip31_get_config(fi[i].nasid, &fi[i].ip27config);

            fi[i].ip27config.check_sum_adj = (uint) 0;

            copy_buffer_adj_sum((ip27config_t *) IP27CONFIG_ADDR, 
		&fi[i].ip27config);
        }

    for (i = 0; i < nodes; i++)
        if ((fi[i].ip27config.time_const != IP27CONFIG.time_const) ||
		(fi[i].ip27config.r10k_mode != IP27CONFIG.r10k_mode) ||
		(fi[i].ip27config.magic != IP27CONFIG.magic) ||
		(fi[i].ip27config.freq_cpu != IP27CONFIG.freq_cpu) ||
		(fi[i].ip27config.freq_hub != IP27CONFIG.freq_hub) ||
		(fi[i].ip27config.freq_rtc != IP27CONFIG.freq_rtc) ||
		(fi[i].ip27config.ecc_enable != IP27CONFIG.ecc_enable) ||
		(fi[i].ip27config.fprom_cyc != IP27CONFIG.fprom_cyc) ||
		(fi[i].ip27config.mach_type != IP27CONFIG.mach_type) ||
		(fi[i].ip27config.fprom_wr != IP27CONFIG.fprom_wr) ||
		(fi[i].ip27config.config_type != IP27CONFIG.config_type)) {
            diff = 1;
            printf("N.B.: Current PROM in nasid %d shows different values\n",
		fi[i].nasid);
        }

    if (diff) {
        char	buf[8];

        printf("Do you wish to proceed ? [n] ");

        if (!gets(buf, 8) || ((buf[0] != 'y') && (buf[0] != 'Y')))
            return;
    }

    /*
     * Quiesce remote CPUs so they aren't accessing the PROM while
     * we're trying to flash it.
     */

    for (i = 0; i < nodes; i++) {
	nasid = fi[i].nasid;

	if (nasid == get_nasid()) {
	    printf("%03d> Can't flash local node\n", nasid);
	    return;
	}

	printf("%03d> Quiescing CPU(s)\n", nasid);

	SD(REMOTE_HUB(nasid, PI_CPU_ENABLE_A), 0);
	SD(REMOTE_HUB(nasid, PI_CPU_ENABLE_B), 0);
    }

    /*
     * Do the flashing.  Use coroutines only if more than one node.
     */

    if (nodes == 1)
	flasher(0, (__uint64_t) fi);
    else {
	corp_t	       *c;
	__uint64_t     *cstk;

	switch (pd->flags->mode) {
	case POD_MODE_DEX:
	    printf("Can only flash 1 node at a time in Dex mode\n");
	    return;
	case POD_MODE_CAC:
	    c		= (corp_t *) TO_NODE(get_nasid(),
					     IP27PROM_CORP);
	    cstk	= (__uint64_t *) TO_NODE(get_nasid(),
						 IP27PROM_CORP_STK);
	    break;
	case POD_MODE_UNC:
	    c		= (corp_t *) TO_NODE(get_nasid(),
					     IP27PROM_CORP);
	    cstk	= (__uint64_t *) TO_NODE(get_nasid(),
						 IP27PROM_CORP_STK);
	    break;
	}

	corp_init(c);

	for (i = 0; i < nodes; i++)
	    corp_fork(c, flasher,
		      cstk + (i + 1) * (IP27PROM_CORP_STKSIZE / 8),
		      (__uint64_t) (fi + i));

	corp_wait(c);
    }

    /*
     * Quiesce remote CPUs so they aren't accessing the PROM while
     * we're trying to flash it.
     */

    for (i = 0; i < nodes; i++) {
	nasid = fi[i].nasid;

	/*
	 * Restart remote Hub.  Send them an NMI so they have a
	 * chance at returning to the POD prompt.
	 */

	printf("%03d> Restarting CPU(s); reset required.\n", nasid);

	SD(REMOTE_HUB(nasid, PI_CPU_ENABLE_A), 1);
	SD(REMOTE_HUB(nasid, PI_CPU_ENABLE_B), 1);

	SD(REMOTE_HUB(nasid, PI_NMI_A), 1);
	SD(REMOTE_HUB(nasid, PI_NMI_B), 1);

#if 0
	/*
	 * Do not use soft reset.  There seems to be a nasty bug where
	 * if a node is soft reset, memory accesses become unpredictable
	 * and memory tests fail.
	 */

	SD(REMOTE_HUB(nasid, PI_SOFTRESET), 0);
#endif
    }
}

void exec_nic(parse_data_t *pd, i64 nargs)
{
    nasid_t		nasid;
    nic_t		nic;

    nasid = nargs ? (nasid_t) parse_pop(pd) : get_nasid();

    if (hub_nic_get(nasid, 1, &nic) < 0)
	printf("Could not access NIC.\n");
    else
	printf("%y\n", nic);
}

void exec_rnic(parse_data_t *pd, i64 nargs)
{
    i64			vec;
    nic_t		nic;
    int			r;

    vec = nargs ? parse_pop(pd) : 0;

    if ((r = router_nic_get(vec, &nic)) < 0)
	printf("Could not get router nic: %s\n", net_errmsg(r));
    else
	printf("%y\n", nic);
}

static void
dump_ip27config(ip27config_t *c)
{
    printf("\tCPU speed\t\t= %d MHz\n", MHZ(c->freq_cpu));
    printf("\tHUB speed\t\t= %d MHz\n", MHZ(c->freq_hub));
    printf("\tRegister: Config (%d)\t%y\n", C0_CONFIG, c->r10k_mode);
    dump_r10k_mode(c->r10k_mode, 0);
}

void exec_cfg(parse_data_t *pd, i64 nargs)
{
    i64			nasid;
    ip27config_t	c;
    __uint64_t		config;

    nasid = (nargs & 0x1 ? parse_pop(pd) : get_nasid());

    memcpy((void *) &c, (void *) IP27CONFIG_ADDR_NODE(nasid), 
	sizeof(ip27config_t));

    printf("*** Config values flashed in the PROM ***\n");
    dump_ip27config(&c);

    if (nasid == get_nasid()) {
        printf("\n*** Config values loaded from nasid %d CPU %c ***\n", nasid,
		(LOCAL_HUB_L(PI_CPU_NUM) ? 'B' : 'A'));
        config = get_cop0(C0_CONFIG);
        printf("\tRegister: Config (%d)\t%y\n", C0_CONFIG, config);
        dump_r10k_mode(config, 1);
    }

    if (ip31_pimm_psc(nasid, NULL, NULL) >= 0) {
        if (ip31_get_config(nasid, &c) >= 0) {

            printf("\n*** Config values loaded from PIMM ***\n");
            dump_ip27config(&c);
        }
        else
            printf("\tIP31 detected but unknown PIMM_PSC type\n");
    }
}

void exec_softreset(parse_data_t *pd)
{
    i64			nasid	= parse_pop(pd);

    printf("Sending soft reset to all CPUs on NASID %d\n", nasid);

    *REMOTE_HUB(nasid, PI_HARDRESET_BIT) = 0;
    *REMOTE_HUB(nasid, PI_SOFTRESET) = 1;
}

void exec_nmi(parse_data_t *pd, i64 nargs)
{
    i64			nasid;
    char		cpu[LEX_STRING_MAX];

    if (nargs > 1)
	parse_pop_string(pd, cpu);
    else
	strcpy(cpu, "AB");

    nasid = parse_pop(pd);

    if (strchr(cpu, 'a') || strchr(cpu, 'A')) {
	printf("Sending NMI to NASID %d CPU A\n", nasid);
	*REMOTE_HUB(nasid, PI_NMI_A) = ~0ULL;
    }

    if (strchr(cpu, 'b') || strchr(cpu, 'B')) {
	printf("Sending NMI to NASID %d CPU B\n", nasid);
	*REMOTE_HUB(nasid, PI_NMI_B) = ~0ULL;
    }
}

void exec_dumpspool(parse_data_t *pd, i64 nargs)
{
    i64			nasid;
    char		cpu[LEX_STRING_MAX];

    if (nargs > 1) {
	parse_pop_string(pd, cpu);
	nasid = parse_pop(pd);
    } else {
	nasid = get_nasid();
	strcpy(cpu, LD(LOCAL_HUB(PI_CPU_NUM)) ? "B" : "A");
    }

    if (strchr(cpu, 'a') || strchr(cpu, 'A')) {
	dump_error_spool(nasid, 0, printf);
    }

    if (strchr(cpu, 'b') || strchr(cpu, 'B')) {
	dump_error_spool(nasid, 1, printf);
    }
}

static void elsc_monitor_hack(void)
{
    int			c;
    char		msg[80];

    while (1) {
	/*
	 * Running poll will also cause any out-of-band messages that
	 * the ELSC sends to be queued.
	 */

	if (elscuart_poll()) {
	    c = elscuart_getc();
	    if (c == 3)
		return;
	    printf("Keyboard input from ELSC: %d\n", c);
	}

	/*
	 * See if the interrupt bit turned on.
	 */

	if (hub_int_test(UART_INTR)) {
	    printf("Interrupt bit is on -- clearing.\n");
	    hub_int_clear(UART_INTR);
	}

	/*
	 * See if any messages were queued.
	 */

	if (elsc_msg_check(get_elsc(), msg, sizeof (msg))) {
	    printf("Message from ELSC: %s\n", msg);
	}

	/*
	 * Sleep a while to avoid printing real fast if the interrupt
	 * bit came on...
	 */

	rtc_sleep(100000);
    }
}

void exec_sc(parse_data_t *pd, i64 nargs)
{
    int		r, resplen;
    elsc_t     *e	= get_elsc();

    if (nargs) {
	parse_pop_string(pd, e->cmd);

	if (strcmp(e->cmd, "mon") == 0)
	    elsc_monitor_hack();
	else if ((r = elsc_command(e, 0)) < 0)
	    printf("Command failed: %s\n", elsc_errmsg(r));
	else
	    printf("Response: %s\n", e->resp);
    } else
	while ((r = elscuart_getc()) != 3 && ! kbintr(&pd->next)) {
	    elscuart_putc(r);
	    elscuart_flush();
	}
}

void exec_scw(parse_data_t *pd, i64 nargs)
{
    i64		count	= nargs > 2 ? parse_pop(pd) : 1;
    i64		value	= nargs > 1 ? parse_pop(pd) : 0;
    i64		addr	= parse_pop(pd);
    elsc_t     *e	= get_elsc();
    char	data;
    int		r;

    if (nargs < 2) {
	char		buf[80];

	/* Edit */

	while (1) {
	    if (addr >= 2048) {
		printf("Address 0x%04lx out of range (0x0000 to 0x07ff)\n",
		       addr);
		break;
	    }

	    if ((r = elsc_nvram_read(e, addr, &data, 1)) < 0) {
		printf("Error reading from system controller NVRAM: %s\n",
		       elsc_errmsg(r));
		break;
	    }

	    printf("%04llx: %02x ", addr, (int) (uchar_t) data);

	    if (gets(buf, sizeof (buf)) == 0)
		break;

	    if (buf[0] != 0) {
		if (! isxdigit(buf[0]))		/* Quit */
		    break;

		data = (char) strtoull(buf, 0, 16);

		if ((r = elsc_nvram_write(e, addr, &data, 1)) < 0) {
		    printf("Error writing to system controller NVRAM: %s\n",
			   elsc_errmsg(r));
		    break;
		}
	    }

	    addr++;
	}
    } else {
	/* Fill */

	data = (char) value;

	while (count--) {
	    if (addr >= 2048) {
		printf("Address 0x%04lx out of range (0x0000 to 0x07ff)\n",
		       addr);
		break;
	    }

	    if ((r = elsc_nvram_write(e, addr, &data, 1)) < 0) {
		printf("Error writing to system controller NVRAM: %s\n",
		       elsc_errmsg(r));
		break;
	    }

	    addr++;
	}
    }
}

void exec_scr(parse_data_t *pd, i64 nargs)
{
    i64		count	= nargs > 1 ? parse_pop(pd) : 1;
    i64		addr	= parse_pop(pd);
    elsc_t     *e	= get_elsc();
    int		col, maxcol;
    char	data;
    int		r;

    col		= 0;
    maxcol	= 16;

    while (count-- && ! kbintr(&pd->next)) {
	if (addr >= 2048) {
	    if (col)
		printf("\n");
	    printf("Address 0x%04lx out of range (0x0000 to 0x07ff)\n",
		   addr);
	    break;
	}

	if ((r = elsc_nvram_read(e, addr, &data, 1)) < 0) {
	    if (col)
		printf("\n");
	    printf("Error reading from system controller NVRAM: %s\n",
		   elsc_errmsg(r));
	    break;
	}

	if (col == 0)
	    printf("%04llx:", addr);

	printf(" %02x", (int) (uchar_t) data);

	if (++col == maxcol) {
	    printf("\n");
	    col = 0;
	}

	addr++;
    }

    if (col)
	printf("\n");
}

static void putsw(int r)
{
    int			i;

    putchar(' ');

    for (i = 7; i >= 0; i--)
	printf(r & 1 << i ? " XX" : " --");
}

void exec_dips(parse_data_t *pd)
{
    int			r;
    u_char		dbg_p, dbg_v;
    elsc_t	       *e	= get_elsc();

    if ((r = elsc_debug_get(e, &dbg_v, &dbg_p)) < 0 ||
	(r = elsc_dip_switches(e)) < 0) {
	printf("Error reading debug switches: %s\n", elsc_errmsg(r));
	return;
    }

    printf("Debug Switch No.:          "
	   "16 15 14 13 12 11 10 09  08 07 06 05 04 03 02 01\n");
    printf("ELSC dbg switches   %02x %02x", dbg_v, dbg_p);
    putsw(dbg_v);
    putsw(dbg_p);
    printf("\nHardware switches      %02x                         ", r);
    putsw(r);
    printf("\nFinal (dbg XOR hw)  %02x %02x", dbg_v, dbg_p ^ r);
    putsw(dbg_v);
    putsw(dbg_p ^ r);
    putchar('\n');
}

void exec_dbg(parse_data_t *pd, i64 nargs)
{
    u_char		byte1, byte2;
    int			r;
    elsc_t	       *e	= get_elsc();

    if (nargs) {
	byte2	= (u_char) parse_pop(pd);
	byte1	= (u_char) parse_pop(pd);

	if ((r = elsc_debug_set(e, byte1, byte2)) < 0)
	    printf("Could not set debug switches: %s\n", elsc_errmsg(r));
    } else if ((r = elsc_debug_get(e, &byte1, &byte2)) < 0)
	printf("Could not get debug switches: %s\n", elsc_errmsg(r));
    else
	printf("%x %x\n", byte1, byte2);
}

void exec_pas(parse_data_t *pd, i64 nargs)
{
    char		pas[LEX_STRING_MAX];
    int			r;
    elsc_t	       *e	= get_elsc();

    if (nargs) {
	parse_pop_string(pd, pas);

	if ((r = elsc_password_set(e, pas)) < 0)
	    printf("Could not set password: %s\n", elsc_errmsg(r));
    } else if ((r = elsc_password_get(e, pas)) < 0)
	    printf("Could not get password: %s\n", elsc_errmsg(r));
    else
	printf("ELSC password is: %s\n", pas);
}

void exec_module(parse_data_t *pd, i64 nargs)
{
    int		r;
    elsc_t     *e	= get_elsc();

    if (nargs == 1) {
	lboard_t	 *brds_in_module[NODESLOTS_PER_MODULE];
	i64		module;

	module = parse_pop(pd);

	if (!SN00)
	    if (elsc_module_set(e, module) < 0)
		printf("Can't talk to elsc!\n");

	if (pd->flags->mode == POD_MODE_DEX) {
		if (SN00)
		    printf("Cannot set module number in DEX mode. Use "
				"'go cac'\n");
		return;
	}

	if (module_brds(get_nasid(), brds_in_module, NODESLOTS_PER_MODULE)
				>= 0) {
	    int		i;
	    char	buf[8];

	    sprintf(buf, "%d", module);

	    for (i = 0; i < NODESLOTS_PER_MODULE; i++)
		if (brds_in_module[i])
		    ip27log_setenv(brds_in_module[i]->brd_nasid,
				IP27LOG_MODULE_KEY, buf, 0);
	}
    }
    else {
	int	module;

	if (SN00) {
	    char	buf[8];

	    ip27log_getenv(get_nasid(), IP27LOG_MODULE_KEY, buf, "0", 0);
	    module = atoi(buf);
	}
	else
	    module = elsc_module_get(e);

	printf("Module number is %d\n", module);
    }
}

void exec_partition(parse_data_t *pd, i64 nargs)
{
    if (!nargs)
        printf("Partition id = %d\n", elsc_partition_get(get_elsc()));
    else {
        i64	partid;

        partid = parse_pop(pd);

        elsc_partition_set(get_elsc(), (int) partid);
    }
}

void exec_modnic(parse_data_t *pd)
{
    i64		nic;
    int		r;

    if ((r = elsc_nic_get(get_elsc(), &nic, 1)) < 0)
	printf("Could not get module NIC: %s\n", elsc_errmsg(r));
    else
	printf("Module NIC: %y\n", nic);
}

void exec_qual(parse_data_t *pd, i64 nargs)
{
    i64		reg	= (i64) LOCAL_HUB(PI_SYSAD_ERRCHK_EN);

    if (nargs)
	SD(reg, parse_pop(pd) ? PI_SYSAD_CHECK_ALL : 0);
    else
	printf("Quality checking is %s\n", LD(reg) ? "on" : "off");
}

void exec_ecc(parse_data_t *pd, i64 nargs)
{
    i64		reg	= (i64) LOCAL_HUB(MD_MEMORY_CONFIG);

    if (nargs) {
	if (parse_pop(pd))
	    SD(reg, LD(reg) & ~MMC_IGNORE_ECC);
	else
	    SD(reg, LD(reg) | MMC_IGNORE_ECC);
    } else
	printf("ECC checking is %s (IGNORE_ECC=%d)\n",
	       LD(reg) & MMC_IGNORE_ECC ? "off" : "on",
	       LD(reg) & MMC_IGNORE_ECC ? 1 : 0);
}

void exec_cpu(parse_data_t *pd, i64 nargs)
{
    char	name[LEX_STRING_MAX];
    char	cpu;
    void	SwitchHandler(void);
    i64		nasid;

    if (nargs == 0) {
	/* print local cpu ID */
	printf("Talking to node %d CPU %c\n", get_nasid(),
	       'A' + LD(LOCAL_HUB(PI_CPU_NUM)));
    } else {
	parse_pop_string(pd, name);

	nasid = (nargs == 2) ? parse_pop(pd) : get_nasid();
	cpu = toupper(*name);

	if (nasid == get_nasid() && hub_num_local_cpus() < 2) {
	    printf("Only one CPU on this node.\n");
	    return;
	}

	if (cpu != 'A' && cpu != 'B')
	    printf("Unknown CPU; use A or B\n");
	else {
	    printf("Switching to node %d CPU %c\n", nasid, cpu);

	    LAUNCH_SLAVE(nasid, cpu - 'A',
			 (launch_proc_t)tlb_to_prom_addr(SwitchHandler),
			 0, 0, (void *) get_gp());

	    launch_loop();
	}
    }
}

void exec_dis(parse_data_t *pd, i64 nargs)
{
    int			ibytes, count;
    __psunsigned_t	addr;
    syminfo_t		syminfo;

    count = (nargs > 1) ? parse_pop(pd) : -1;

    addr = parse_pop(pd);

    if (count < 0) {
	syminfo.sym_addr = (void *)addr;
	if (symbol_lookup_addr(&syminfo) >= 0) {
	    __psunsigned_t offset;
	    offset = addr - (__psunsigned_t) syminfo.sym_addr;
	    count = (syminfo.sym_size - offset) >> 2;
	}

	if (count <= 0)
	    count = 12;
    }

    while (count > 0 && ! kbintr(&pd->next)) {
	ibytes = symbol_dis((inst_t *) addr);
	addr += ibytes;
	count -= ibytes >> 2;
    }
}

void exec_dmc(parse_data_t *pd, i64 nargs)
{
    int		bank;
    __uint64_t	md_mem_cfg;
    char	dis_mem_mask[32], dis_mem_sz[32], dis_mem_reason[32];
    nasid_t	nasid = (nargs ? parse_pop(pd) : get_nasid());

    md_mem_cfg = translate_hub_mcreg(REMOTE_HUB_L(nasid, MD_MEMORY_CONFIG));

    *dis_mem_mask = 0;

    ip27log_getenv(nasid, "DisableMemSz", dis_mem_sz, "00000000", 0);
    ip27log_getenv(nasid, "MemDisReason", dis_mem_reason, "00000000", 0);
    ip27log_getenv(nasid, "DisableMemMask", dis_mem_mask, 0, 0);

    printf("\nMemory config for nasid %d\n\n", nasid);

    for (bank = 0; bank < MD_MEM_BANKS; bank++) {
        int	reason;
        char	*disabled;
        int	size = (md_mem_cfg & MMC_BANK_MASK(bank)) >> 
			MMC_BANK_SHFT(bank);

        if (disabled = strchr(dis_mem_mask, '0' + bank)) {
            if (!size)
                size = dis_mem_sz[bank] - '0';
            reason = dis_mem_reason[bank] - '0';
        }

        printf("\tBank %d (software bank %d): Size %3d MBytes\n", bank, 
	       MD_SIZE_MBYTES(size));

        if (disabled) {
	    switch (reason) {
		case MEM_DISABLE_USR:
		   printf("\t\t*** Disabled: Reason %s\n", 
			get_diag_string(KLDIAG_MEMORY_DISABLED)); 
		   break;
		case MEM_DISABLE_AUTO:
               	   printf("\t\t*** Disabled: Reason %s\n", 
			get_diag_string(KLDIAG_MEMTEST_FAILED));
		   break;
		case MEM_DISABLE_256MB:
               	   printf("\t\t*** Disabled: Reason %s\n", 
			get_diag_string(KLDIAG_IP27_256MB_BANK));
		   break;
		default:
               	   printf("\t\t*** Disabled: Reason Unknown\n"); 
		   break;
	    }
	}
        else
            printf("\t\t*** Enabled\n");
    }
}

void exec_im(parse_data_t *pd, i64 nargs)
{
    i64		sr, im;

    if (nargs) {
	im = parse_pop(pd);
	sr = get_cop0(C0_SR);
	sr = sr & ~0xff00 | im << 8 | (im ? 1 : 0);
	set_cop0(C0_SR, sr);
    }

    sr = get_cop0(C0_SR);

    printf("Interrupt mask is 0x%02llx (IE=%lld)\n",
	   sr >> 8 & 0xff, sr & 1);
}

void exec_error_dump(parse_data_t *pd)
{
    kl_log_hw_state();
    kl_error_show_log("Hardware Error State: (Forced error dump)\n",
		       "END Hardware Error State (Forced error dump)\n");
}

void exec_reset_dump(parse_data_t *pd)
{
	error_show_reset();
}

void probe_edump_bri(nasid_t nasid)
{
    volatile __psunsigned_t	wid_base;
    reg32_t			wid_id, link_widget_id;
    jmp_buf			new_buf;
    void			*old_buf;
    int				link = 0, hub_link = hub_xbow_link(nasid);
    klbridge_err_t		bri_err;
    edump_param_t		edp;

    edp.dp_flag = KL_ERROR_DUMP;
    edp.dp_printf = (void (*)())(printf);
    edp.dp_prarg = 0;

    if (setfault(new_buf, &old_buf)) {
	printf("*** probe_err_dump_bridge: Exception occurred on NASID %d\n", nasid);
	printf("\tIgnoring and continuing\n");
	restorefault(old_buf);
	return;
    }

    wid_base = SN0_WIDGET_BASE(nasid, 0);

    wid_id =  LD32(wid_base + WIDGET_ID);

    switch ((XWIDGET_PART_NUM(wid_id))) {
	case BRIDGE_WIDGET_PART_NUM:
	    save_bridge_err(nasid, &bri_err, 8, &edp);
	    show_bridge_err(&bri_err, &edp);
	    break;
	case XBOW_WIDGET_PART_NUM:
	    while ((link = xbow_get_active_link(wid_base, link)) >= 0) {
		if (link == hub_xbow_link(nasid)) {
		    link++;
		    continue;
		}

		/*
                 * Should actually do a xbow_check_link_hub first; But
                 * arb_upper is cleared; so we go ahead and read the
                 * widgetid
                 */

		link_widget_id = io_get_widgetid(nasid, link);

		if (XWIDGET_PART_NUM(link_widget_id) !=
				BRIDGE_WIDGET_PART_NUM) {
		    link++;
		    continue;
		}

		save_bridge_err(nasid, &bri_err, link, &edp);
		show_bridge_err(&bri_err, &edp);
		link++;
	    }
	    break;
	default:
	    break;
    }
    restorefault(old_buf);
}

void exec_edump_bri(parse_data_t *pd, i64 nargs)
{
    pcfg_t		*p = PCFG(get_nasid());
    int			i;

    printf(" BRIDGE error state: (Forced error dump)\n");

    if (nargs & 0x1) {
	nasid_t		nasid;

	nasid = parse_pop(pd);
	probe_edump_bri(nasid);
	printf(" END BRIDGE error state: (Forced error dump)\n");
	return;
    }

    if (p->magic != PCFG_MAGIC) {
	printf("error_dump_bri: promcfg PCFG_MAGIC incorrect. Cannot determine system state\n");
	printf("error_dump_bri: Try printing out individual bridge registers for each node\n");
	return;
    }

    for (i = 0; i < p->count; i++) {
	pcfg_hub_t	*hub_cf;

	if (p->array[i].any.type != PCFG_TYPE_HUB)
	    continue;

	hub_cf = &p->array[i].hub;

	if (hub_cf->nasid == INVALID_NASID)
	    continue;

	probe_edump_bri(hub_cf->nasid);
    }
    printf(" END BRIDGE error state: (Forced error dump)\n");
}

void exec_maxerr(parse_data_t *pd, i64 nargs)
{
    if (nargs)
	pd->flags->maxerr = parse_pop(pd);
    else
	printf("maxerr = %d\n", pd->flags->maxerr);
}

void exec_rtab(parse_data_t *pd, i64 nargs)
{
    i64		vec, reg, is_hub, is_local;
    int		i, r;

    is_hub	= 1;
    is_local	= 1;

    if (nargs > 0) {
	is_local = 0;

	vec = parse_pop(pd);

	if ((r = vector_read(vec, 0, NI_STATUS_REV_ID, &reg)) < 0) {
	neterr:
	    printf("\nNetwork vector read error: %s\n", net_errmsg(r));
	    return;
	}

	is_hub = (GET_FIELD(reg, NSRI_CHIPID) == CHIPID_HUB);
    }

    printf("%s Local Table:\n", is_hub ? "Hub" : "Router");

    for (i = 0; i < 16; i++) {
	if (is_local)
	    reg = LD(LOCAL_HUB(NI_LOCAL_TABLE(i)));
	else if ((r = vector_read(vec, 0,
				   is_hub ? NI_LOCAL_TABLE(i) :
				   RR_LOCAL_TABLE(i), &reg)) < 0)
	    goto neterr;

	printf("%s%02x:%06lx%s",
	       i % 4 == 0 ? "\t" : "",
	       i, reg & 0xffffff,
	       i % 4 == 3 ? "\n" : "  ");
    }

    printf("%s Meta Table:\n", is_hub ? "Hub" : "Router");

    for (i = 0; i < 32; i++) {
	if (is_local)
	    reg = LD(LOCAL_HUB(NI_META_TABLE(i)));
	else if (vector_read(vec, 0,
			      is_hub ? NI_META_TABLE(i) : RR_META_TABLE(i),
			      &reg) < 0)
	    goto neterr;

	printf("%s%02x:%06lx%s",
	       i % 4 == 0 ? "\t" : "",
	       i, reg & 0xffffff,
	       i % 4 == 3 ? "\n" : "  ");
    }

    return;
}

void exec_rstat(parse_data_t *pd)
{
    i64				vec, reg, hist;
    struct rr_status_error_fmt	rserr;
    int				i, r;

    vec = parse_pop(pd);

    rstat(vec);

    return;
}

/*
 * xtalk credits command
 */

unsigned char credit_data[] =
    "\037\213\b\000\000\000\000\000\000\003\255\224Mo\332@\020\206\357\376"
    "\025/\2479\340\014wsr\234 \2602\264\252\013U\216Qb\225HQ*\245\264U$~|"
    "gw\355\365\372\003\223\b\306 \355\332\363>\363\341Y\003\347\330=Gg\351"
    "\261\021\260\234\311\330\nD\200\270\312\205cE\366\234\230Y\032\323]\370"
    "\220\364\016\220Z\016\013\031\016C\022\310\034\bP!!\260\270\005\212\223"
    ";\bM,en\211\nj\022:\302hs\024C\232\"\331\316\310\\\364\207D\344$\301\226"
    "V\007*\276\255\326\253mZ\244\221%\316\205\\\237\2149W\277\360\346\357"
    "XJ\034\013\213f!\256%\372\210\362\333\305\242\203\350@\302;\252%#7%X\204"
    "\336\212\021wd#i\324<\220\251\332\025b`\025~\274\023\235\216PE\000\021"
    "\323\335\355\232N\266\262S\224K\312\rY!\031\372\265\1770\013\307\340\264"
    "\302&\037\322\333F\006y8\210\024\036R\261G\bl\000\"\211\313\205<\205e"
    ")\356\210$:\253\211{\345G\031D&\356\266\225\265y\271\251H\312|\275L\327"
    "\233*\3021\222I\204\\!+\n,j\265\263\036x\262A{\204\214*\006Kn\007\303"
    "[\322\034\002\225ML\022W\366\"\233\017\371c&i\347\313#\215\322\rJ\023"
    "0tc\271\357\013[\312a\343\372 }R\3112 \364\311\336\340\013\326 |\327\365"
    "W\244\272^!\303\244r\351\353\272U\016\307\033\224\265>\2531\347\234\253"
    "q\240\032\210\326\252\220rg\320\277\201e\315$\017\311\252A\315\003Uuf"
    "\207\002y\225\021R\255S\321\250\"<6^\241v\334\037\315d\344\336F\335\275"
    "\354S\336p\211\344\221q\236^5\026\256O\356\233\315T93\035\226\372\n\327"
    "\247\366\301fv\210.\202\3014\272DQ\230E\007\\\227\257X<\354w\3178 \373"
    "\363\266\207<\336\374\372W\276\274\350\276\330\227\177K\374\330=\357_"
    "\313w\335o\236\036\336\261,\177>\225\306yW\226\277\3137\034f\027I&\372"
    "\017\366\3071&O\t\000\000";

static u_char *credit_ptr;

static int credit_getc(void)
{
    return *credit_ptr++;
}

void exec_credits(parse_data_t *pd)
{
    if (! bad_mode(pd, POD_MODE_DEX)) {
	credit_ptr = credit_data;

	unzip(credit_getc,
	      (void (*)(int)) putc,
	      (char *) TO_NODE(get_nasid(), IP27PROM_DECOMP_BUF),
	      IP27PROM_DECOMP_SIZE);
    }
}

void exec_version(parse_data_t *pd)
{
    printf("SN0 IP27 PROM\n%s\n", prom_version);
}

static char *do_cmp(char *src1, char *src2, i64 length)
{
    while (length--) {
	if (LBYTE(src1) != LBYTE(src2))
	    return src1;
	src1++;
	src2++;
    }

    return (char *) ~0ULL;
}

void exec_memop(parse_data_t *pd, i64 op)
{
    i64		a1, a2, a3;
    char       *bad;

    if (op != 3)
	a3 = parse_pop(pd);

    a2 = parse_pop(pd);
    a1 = parse_pop(pd);

    switch (op) {
    case 0:
	memset((char *) a1, a2, a3);
	break;
    case 1:
	memcpy((char *) a1, (char *) a2, a3);
	break;
    case 2:
	if ((bad = do_cmp((char *) a1, (char *) a2, a3)) == (char *) ~0ULL)
	    printf("Same\n");
	else
	    printf("Different at address %y\n", bad);
	break;
    case 3:
	printf("Sum = %y\n", memsum((char *) a1, a2));
	break;
    }
}

/*
 * Directory entry scanner
 */

static void scan_show(int premium, i64 addr, i64 lo, i64 hi)
{
    char	buf[80];

    sprintf(buf, "%016llx: ", addr);
    mdir_dir_decode(buf + strlen(buf), premium, lo, hi);
    printf("%s\n", buf);
}

void exec_scandir(parse_data_t *pd, i64 nargs)
{
    i64		loaddr, hiaddr, addr, prev_addr;
    i64		lo, prev_lo;
    i64		hi, prev_hi;
    i64		len;
    int		premium, set;

    len		= (nargs > 1) ? parse_pop(pd) : 1;
    len		= (len + CACHE_SLINE_SIZE - 1) & ~(CACHE_SLINE_SIZE - 1);
    loaddr	= parse_pop(pd) & ~(CACHE_SLINE_SIZE - 1);
    hiaddr	= loaddr + len;

    premium	= (LD(REMOTE_HUB(NASID_GET(loaddr),
				 MD_MEMORY_CONFIG)) & MMC_DIR_PREMIUM) != 0;

    for (prev_addr = addr = loaddr;
	 addr < hiaddr && ! kbintr(&pd->next);
	 addr += CACHE_SLINE_SIZE) {

	lo	= LD(BDDIR_ENTRY_LO(addr));
	hi	= LD(BDDIR_ENTRY_HI(addr));

	if (addr > loaddr && (lo != prev_lo || hi != prev_hi)) {
	    scan_show(premium, prev_addr, prev_lo, prev_hi);
	    prev_addr = addr;
	}

	prev_lo	  = lo;
	prev_hi	  = hi;
    }

    scan_show(premium, prev_addr, prev_lo, prev_hi);
}

void
exec_dirstate(parse_data_t *pd, i64 nargs)
{
    __uint32_t size, bank_size;
    __psunsigned_t base = 0, bank_base;
    nasid_t nasid = get_nasid();
    int chk_state;
    hubreg_t mem_cfg;
    int start_bank, end_bank, i;
    int premium;

    chk_state = (nargs > 2) ? parse_pop(pd) : -1;
    size      = (nargs > 1) ? parse_pop(pd) : 0;

    if (nargs > 0) {
	base	  = parse_pop(pd);
	nasid	  = NASID_GET(base);
    }
    else {
	printf("Checking all possible directory states\n");
	mdir_dirstate_usage();
    }

    mem_cfg = *REMOTE_HUB(nasid, MD_MEMORY_CONFIG);
    premium = mem_cfg & MMC_DIR_PREMIUM;

    if (base) start_bank = (base & NODE_ADDRSPACE_MASK) / MD_BANK_SIZE;
    else start_bank = 0;

    if (size) end_bank = (start_bank + size/MD_BANK_SIZE + 1);
    else end_bank = MD_MEM_BANKS;

    printf("Node   ___Off___    __Owner__      __State__   (DirLo)\n");

    for (i = start_bank; i < end_bank; i++) {
	bank_size = MD_SIZE_MBYTES((mem_cfg & MMC_BANK_MASK(i))
				   >> MMC_BANK_SHFT(i));
	bank_size <<= 20;
	bank_base = TO_NODE(nasid, i * MD_BANK_SIZE);

	if (base == 0) base = bank_base;
	bank_size -= (base - bank_base);
	if (!size || (size > bank_size))
	    size = bank_size;

	if (size) {
	    if (premium) {
		if (mdir_pstate(chk_state, base, size))
		    break;
	    }
	    else {
		if (mdir_sstate(chk_state, base, size))
		    break;
	    }
	}
	base = 0;
	size = 0;
    }
}

void
exec_verbose(parse_data_t *pd, i64 nargs)
{
    if (nargs)
	set_libc_verbosity(parse_pop(pd));
    else
	printf("Verbose is %s\n", get_BSR() & BSR_VERBOSE ? "on" : "off");
}

void
exec_altregs(parse_data_t *pd, i64 nargs)
{
    __uint64_t		regs_num = 0;

    if (nargs != 0)
	regs_num = parse_pop(pd);
    else {
	pd->flags->alt_regs = 0;
	set_BSR(get_BSR() & ~BSR_KREGS);
	return;
    }
    kdebug_alt_regs(regs_num, pd->flags);
}


void
exec_kern_sym(parse_data_t *pd)
{
    kdebug_syms_toggle();
}

void
exec_kdebug(parse_data_t *pd, i64 nargs)
{
    __uint64_t parm = 0;

    if (nargs)
	parm = parse_pop(pd);

    kdebug_init(parm);
}


void exec_initlog(parse_data_t *pd, i64 op)
{
    promlog_t		l;
    nasid_t		nasid;
    char		buf[4];

    nasid = (op & 4) ? parse_pop(pd) : get_nasid();

    if ((pd->flags->mode == POD_MODE_DEX) && (nasid == get_nasid())) {
	printf("Can't initialize PROM log while running out of PROM\n");
	return;
    }

    printf("Clear all logs%s? [n] ",
	(op & 1) ? "" : " environment variables, and aliases ");

    if (gets_timeout(buf, sizeof (buf), 10000000, "n") == 0 ||
	(buf[0] != 'y' && buf[0] != 'Y')) {
	printf("Aborted\n");
	return;
    }

    if (ip27log_setup(&l, nasid, (((op & 1) ? IP27LOG_DO_CLEAR : 
		IP27LOG_DO_INIT) | IP27LOG_VERBOSE)) < 0)
	return;

    printf("Done\n");
}

void exec_initalllogs(parse_data_t *pd, i64 op)
{
    pcfg_t	*p = PCFG(get_nasid());
    promlog_t	l;
    int		i;
    char	buf[4];

    if (pd->flags->mode == POD_MODE_DEX) {
	printf("Can't initialize all PROM logs while running out of PROM. Use 'go cac'\n");
	return;
    }

    printf("*** This must be run only after CrayLink discovery is complete.\n");
    printf("*** This will clear all previous log variables such as: \n");
    printf("*** moduleids, nodeids, etc. for all nodes.\n");
    printf("Clear all logs%s? [n] ",
	(op & 1) ? "" : " environment variables, and aliases ");

    if (gets_timeout(buf, sizeof (buf), 10000000, "n") == 0 ||
	(buf[0] != 'y' && buf[0] != 'Y')) {
	printf("Aborted\n");
	return;
    }

    for (i = 0; i < p->count; i++) {
        nasid_t		nasid;

        if (p->array[i].any.type != PCFG_TYPE_HUB)
            continue;

        nasid = p->array[i].hub.nasid;

        if (ip27log_setup(&l, nasid, (((op & 1) ? IP27LOG_DO_CLEAR : 
			IP27LOG_DO_INIT) | IP27LOG_VERBOSE)) < 0)
            printf("%S: ERROR clearing PROM log\n", 
			p->array[i].hub.module, p->array[i].hub.slot);
    }

    printf("All PROM logs cleared!\n");
}

void exec_setenv(parse_data_t *pd, i64 op)
{
    char		key[LEX_STRING_MAX];
    char		value[PROMLOG_VALUE_MAX];
    nasid_t		nasid;

    if (op & 1)
	parse_pop_string(pd, value);

    if (op & 2)
	parse_pop_string(pd, key);

    nasid = (op & 4) ? parse_pop(pd) : get_nasid();

    if (pd->flags->mode == POD_MODE_DEX && nasid == get_nasid()) {
	printf("Can't set PROM variable while running out of PROM\n");
	return;
    }

    if ((op & 1) == 0) {
	printf("Set \"%s\" to? ", key);
	if (gets(value, sizeof (value)) == 0)
	    return;
    }

    ip27log_setenv(nasid, key, value, IP27LOG_VERBOSE);
}

void exec_unsetenv(parse_data_t *pd, i64 op)
{
    char		key[LEX_STRING_MAX];
    nasid_t		nasid;

    parse_pop_string(pd, key);

    nasid = (op & 4) ? parse_pop(pd) : get_nasid();

    if (pd->flags->mode == POD_MODE_DEX && nasid == get_nasid()) {
	printf("Can't unset PROM variable while running out of PROM\n");
	return;
    }

    ip27log_unsetenv(nasid, key, IP27LOG_VERBOSE);
}

void exec_printenv(parse_data_t *pd, i64 op)
{
    promlog_t		l;
    nasid_t		nasid;
    char		key[LEX_STRING_MAX];
    char		value[PROMLOG_VALUE_MAX];
    int			r;

    if (op & 2)
	parse_pop_string(pd, key);

    nasid = (op & 4) ? parse_pop(pd) : get_nasid();

    if (ip27log_setup(&l, nasid, IP27LOG_VERBOSE) < 0)
	return;

    if (promlog_first(&l, PROMLOG_TYPE_VAR) < 0)
	return;

    if (op & 2) {
	if (promlog_find(&l, key, PROMLOG_TYPE_VAR) < 0 ||
	    promlog_get(&l, key, value) < 0) {
	    printf("Variable not set.\n");
	    return;
	}

	printf("%-16s%s\n", key, value);
    } else {
	do {
	    if (promlog_get(&l, key, value) >= 0)
		printf("%-16s%s\n", key, value);
	} while (promlog_next(&l, PROMLOG_TYPE_VAR) >= 0);
    }
}

void exec_log(parse_data_t *pd, i64 op)
{
    promlog_t		l;
    nasid_t		nasid;
    int			tail, head;
    char		key[LEX_STRING_MAX];
    char		value[PROMLOG_VALUE_MAX];
    int			r, lines = 0;

    head  = (op & 1) ? parse_pop(pd) : 0x7ffffff;
    tail  = (op & 2) ? parse_pop(pd) : -1;
    nasid = (op & 4) ? (nasid_t) parse_pop(pd) : get_nasid();

    if (ip27log_setup(&l, nasid, IP27LOG_VERBOSE) < 0)
	return;

    /*
     * Position log to 'tail' entries from the end, then display
     * up to 'head' entries.
     */

    if (tail < 0)
	r = promlog_first(&l, PROMLOG_TYPE_LOG);
    else if ((r = promlog_last(&l)) >= 0)
	while (tail-- > 0)
	    if ((r = promlog_prev(&l, PROMLOG_TYPE_LOG)) < 0)
		break;

    if (r < 0)
	goto done;

    while (head-- > 0) {
	if ((r = promlog_get(&l, key, value)) < 0)
	    break;

	if (! more(&lines, 23))
	    break;

	printf("%c %-5s: %s", 'A' + l.ent.cpu_num, key, value);

	if ((r = promlog_next(&l, PROMLOG_TYPE_LOG)) < 0)
	    break;
    }

 done:

    if (r < 0 && r != PROMLOG_ERROR_EOL)
	printf("Error reading log: %s\n", promlog_errmsg(r));
}

void
exec_fru(parse_data_t *pd,i64 op)
{

#if 1
	i64	mode = parse_pop(pd);
	extern	void kl_save_hw_state_node(nasid_t,edump_param_t *edp);
	extern	void kl_error_show_node(nasid_t,edump_param_t *edp);
	extern	int  sn0_fru_entry(int (*print)(const char *,...));
	extern	void sn0_fru_node_entry(nasid_t nasid,
					int (*print)(const char *,...));


	switch(mode) {
	case	1:
	{

		/* Dump the hardware error state for the local node
		 * and do the fru analysis on it
		 */
		edump_param_t	edp;
		nasid_t		nasid = get_nasid();

		/* Sanity checking to make sure that local klconfig
		 * is available to proceed with the fru analysis
		 */

#if 0
		if (!(get_BSR() & BSR_INITCONFIG))  {
			printf("FRU:Local klconfig not setup\n");
			return;
		}
#else
		/* Make sure that klconfig is setup properly */
		if (!(KL_CONFIG_CHECK_MAGIC(nasid))) {
			printf("FRU:Local klconfig not setup properly\n");
			return;
		}
#endif
		edp.dp_flag = KL_ERROR_DUMP | KL_ERROR_RESTORE;
		edp.dp_printf = (void (*)())(printf);
		edp.dp_prarg = 0;
		/* Dump the hardware error state */
		printf("Hardware Error State: (Forced error dump)\n");
		kl_save_hw_state_node(nasid,&edp);
		kl_error_show_node(nasid,&edp);
		printf("END Hardware Error State (Forced error dump)\n");
		/* Check if we are in DEX mode. FRU cannot run in this mode */
		if (pd->flags->mode == POD_MODE_DEX) {
#if 0
			printf("Switching to cac mode and running fru\n");
#endif
			memory_copy_prom(DIP_DIAG_MODE());
			memory_clear_prom(DIP_DIAG_MODE());

			pod_mode(POD_MODE_CAC, KLDIAG_FRU_LOCAL,
				 "Changing to cac mode");
			/* NOT REACHED */
		}
		/* analyze for the local node */
		sn0_fru_node_entry(nasid,printf);
		break;
	}
	case	2:
	{

		/* Dump the hardware error state for all the nodes
		 * and do the fru analysis
		 */
#if 0
		/* Sanity check to make sure that all klconfig is
		 * setup for all nodes.Otherwise fru cannot
		 * proceed with the analysis
		 */
		if (!(get_BSR() & BSR_NETCONFIG)) {
			printf("FRU: All the klconfigs are not set up yet\n");
			return;
		}
#endif
		kl_dump_hw_state();
		kl_error_show_dump("Hardware Error State: (Forced error dump)\n",
				   "END Hardware Error State (Forced error dump)\n");

		/* Check if we are in DEX mode. FRU cannot run in this mode */
		if (pd->flags->mode == POD_MODE_DEX) {
#if 0
			printf("Switching to cac mode and running fru\n");
#endif
			memory_copy_prom(DIP_DIAG_MODE());
			memory_clear_prom(DIP_DIAG_MODE());

			pod_mode(POD_MODE_CAC, KLDIAG_FRU_ALL,
				 "Changing to cac mode");
			/* NOT REACHED */
		}

		sn0_fru_entry(printf);
		break;
	}
	default:
		printf("Invalid option.Type \"help fru\"\n");
		break;
	}
#else
	printf("Fru command in the prom is currently disabled\n");
#endif
}

void exec_setpart(parse_data_t *pd)
{
   int		i;
   char		rtrs[16];
   i64		rtrnos = parse_pop(pd);
   __uint64_t	rr_reg, rr_reg_mask = 0xffffffffffffffff;
   pcfg_t	*p = PCFG(get_nasid());
   net_vec_t	vec;

   sprintf(rtrs, "%lx", rtrnos);

   for (i = 1; i <= MAX_ROUTER_PORTS; i++)
      if (strchr(rtrs, '0' + i))
	 rr_reg_mask &= (~((__uint64_t) 1 << (i - 1)));

   for (i = 0; i < p->count; i++) {
      int	j;

      if (p->array[i].any.type != PCFG_TYPE_ROUTER)
	 continue;

      vec = discover_route(p, 0, i, 0);

      (void) vector_read(vec, 0, RR_PROT_CONF, &rr_reg);
      /* turning off RESET_OK for ports specified */
      rr_reg &= rr_reg_mask;
      vector_write(vec, 0, RR_PROT_CONF, rr_reg);

      for (j = 1; j <= MAX_ROUTER_PORTS; j++) {
	 if (p->array[i].router.port[j].index == -1)
	    continue;

	 vector_read(vec, 0, RR_RESET_MASK(j), &rr_reg);
	 rr_reg &= rr_reg_mask;
	 vector_write(vec, 0, RR_RESET_MASK(j), rr_reg);
      }
   }

   printf("Partition done. OK to reset partitions\n");
}


/*
 * parseDiagOptions()
 *
 * This function will parse any command line options that may be
 * specified for the diagnostics and set the corresponding parameters
 * appropriately.  Passing 0 for any of the option pointers indicates
 * that the corresponding parameter is not valid for the associated
 * command.  0 will be returned if an error is detected during
 * parsing; 1 otherwise.
 */

static int parseDiagOptions(parse_data_t *pd, i64 nargs, int *diagMode,
			    nasid_t *NASID, slotid_t *slot, int *PCIdevNum)
{
  int optCount;
  char cmdOption[LEX_STRING_MAX];
  char *endConvert;

  for(optCount = 0; optCount < nargs; optCount++)
  {
    /* pop command line option and parse it */
    parse_pop_string(pd, cmdOption);
    switch(cmdOption[0])
    {
      case 'm':
      case 'M':
	/* specifying diag mode */
	switch(cmdOption[1])
	{
	  case 'n':
	  case 'N':
	    /* normal mode */
	    *diagMode = DIAG_MODE_NORMAL;
	    break;

	  case 'h':
	  case 'H':
	    /* heavy mode */
	    *diagMode = DIAG_MODE_HEAVY;
	    break;

	  case 'm':
	  case 'M':
	    /* manufacturing mode */
	    *diagMode = DIAG_MODE_MFG;
	    break;

	  case 'x':
	  case 'X':
	    /* external loopback */
	    *diagMode = DIAG_MODE_EXT_LOOP;
	    break;

	  default:
	    printf("Invalid Diag Mode: %c\n", cmdOption[1]);
	    return(0);
	}
	break;

      case 'n':
      case 'N':
	/* specifying NASID */
	*NASID = (nasid_t)strtoull((cmdOption + 1), &endConvert, 0);
	if(((*NASID == 0) && (endConvert == (cmdOption + 1))) ||
	   (*NASID > 255))
	{
	  printf("Invalid NASID: %s\n", (cmdOption + 1));
	  return(0);
	}
	break;

      case 's':
      case 'S':
	/* specifying slot # */
	if(slot)
	{
	  *slot = (slotid_t)strtoull((cmdOption + 1), &endConvert, 0);
	  if(((*slot == 0) && (endConvert == (cmdOption + 1))) || (*slot > 12))
	  {
	    printf("Invalid Slot #: %s\n", (cmdOption + 1));
	    return(0);
	  }
	}
	break;

      case 'p':
      case 'P':
	/* specifying PCI device # */
	if(PCIdevNum)
	{
	  *PCIdevNum = (int)strtoull((cmdOption + 1), &endConvert, 0);
	  if(((*PCIdevNum == 0) && (endConvert == (cmdOption + 1))) ||
	     (*PCIdevNum > 7))
	  {
	    printf("Invalid PCI Device #: %s\n", (cmdOption + 1));
	    return(0);
	  }
	}
	break;

      default:
	printf("Invalid Command Line Option: %c\n", cmdOption[0]);
	return(0);
    }
  }

  return(1);
}

void exec_xbowtest(parse_data_t *pd, i64 nargs)
{
  int diagMode;
  nasid_t NASID;

  /* set options to default values */
  diagMode = DIAG_MODE_NORMAL;
  NASID = get_nasid();

  /* parse any command line options */
  if(!parseDiagOptions(pd, nargs, &diagMode, &NASID, 0, 0))
    return;

  /* disable any reset value checking in the diag */
  diagMode |= DIAG_FLAG_NORESET;

  /* filter out external loopback diag mode */
  if(diagMode == DIAG_MODE_EXT_LOOP)
    diagMode = DIAG_MODE_MFG;

  diag_xbowSanity(diagMode, NODE_IO_BASE(NASID));
}

void exec_Diag(parse_data_t *pd, i64 nargs)
{
  int diagMode, type, call;
  nasid_t NASID;
  slotid_t slot;
  int (*diag_fn1)(int, __psunsigned_t, int);
  int (*diag_fn2)(int, __psunsigned_t);
  int PCIdevNum, diagWhat = (nargs & 0xf0) >> 4;;

  if (((diagWhat == SRL_DMA)) && (pd->flags->mode == POD_MODE_DEX)) {
    printf("Cannot test Serial DMA in Dex mode "
	       "(use 'go cac' if possible)\n");
    return;
  }

  /* set options to default values */
  diagMode = DIAG_MODE_NORMAL;
  NASID = get_nasid();
  slot = 1;
  PCIdevNum = 2;

  /* parse any command line options */
  if(!parseDiagOptions(pd, nargs & 0xf, &diagMode, &NASID, &slot, &PCIdevNum))
    return;

  /* filter out external loopback diag mode */
  if ((diagWhat != SRL_DMA) && (diagWhat != SRL_PIO))
    if(diagMode == DIAG_MODE_EXT_LOOP)
      diagMode = DIAG_MODE_MFG;

  /* disable any reset value checking in the diag */
  diagMode |= DIAG_FLAG_NORESET;

  call = 1;
  type = 0;

  switch(diagWhat) {
    case DIAG_PCI:
       diag_fn1 = diag_pcibus_sanity;
       type = 1;
       break;
    case SRL_DMA:
       diag_fn1 = diag_serial_dma;
       type = 1;
       break;
    case SRL_PIO:
       diag_fn1 = diag_serial_pio;
       type = 1;
       break;
    case DIAG_IO6:
       diag_fn2 = diag_io6confSpace_sanity;
       type = 2;
       break;
    case DIAG_BRI:
       diag_fn2 = diag_bridgeSanity;
       type = 2;
       break;
    default:
       call = 0;
       break;
  }

  if (call) {
    if (type == 1)
      (*diag_fn1)(diagMode, NODE_SWIN_BASE(NASID,
		  (slot_to_widget(slot - 1) & 0x0F)), PCIdevNum);
    else
      (*diag_fn2)(diagMode, NODE_SWIN_BASE(NASID,
		  (slot_to_widget(slot - 1) & 0x0F)));
  }
}
