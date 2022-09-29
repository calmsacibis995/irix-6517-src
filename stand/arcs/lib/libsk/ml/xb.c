/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#if SA_DIAG
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include <fault.h>
#include "setjmp.h"
#include "libsc.h"
#include "libsk.h"
#include "uif.h"
#include <sys/xtalk/xbow_info.h>
#include <sys/nic.h>

#include "sa.h"

extern xbowreg_t	dump_xbow_regs(__psunsigned_t, int);
extern void		print_xbow_link_regs(xb_linkregs_t *, int, int);
extern void 		print_xbow_perf_ctrs(xbow_t *xb);
extern void		dump_widget_regs(widget_cfg_t *wp);
extern int		xbow_link_alive(volatile __psunsigned_t xbow_base, int link);
extern int		xbow_link_present(volatile __psunsigned_t xbow_base, int link);
extern int		xbow_link_status(volatile __psunsigned_t xbow_base, int link);
extern void		reset_xbow_link(__psunsigned_t xbow_base, int link);

/*
 * xbow utility
 */
static const char	xb_str[] = "xb";

static xbow_t		*xb;
static widget_cfg_t	*wid;

static int perf_enbled;
/*
 * flags
 */
#define XBF_LINKOK	0x0001		/* LINKOK flag */
#define XBF_NICREAD	0x0002		/* read NIC for testing */
#define XBF_REGRD	0x0004		/* read wid registers for testing */
#define XBF_REGWR	0x0008		/* write wid registers for testing */

#define XBF_VERBOSE	0x8000		/* VERBOSE flag */

static uint f_flags;

static void
ff_flag(struct SaFlag *flp, int on)
{
	printf("xb: %s (%s) 0x%x turned %s\n",
		flp->name, flp->help, flp->bit, (on)?"ON":"OFF");

}

static struct SaFlag xbf[] = {
    /* flags		bit		help	(*func)(struct SaFlag *, int) */
    { "link_ok",	XBF_LINKOK,	"link port OK", ff_flag },
    { "nic_rd",		XBF_NICREAD,	"read NIC", 	ff_flag },
    { "reg_rd",		XBF_REGRD,	"read REGS", 	ff_flag },
    { "reg_wr",		XBF_REGWR,	"write REGS", 	ff_flag },
    { "verb",		XBF_VERBOSE,	"verbose", 	ff_flag },
    { 0 },
};

/*
 * xb variables
 */
static int v_link;		/* default link ID */
static int v_perfa;		/* default perfa link */
static int v_perfb;		/* default perfb link */
static int v_perfa_mode;	/* mode values: count src (1), dst (2) */
static int v_perfb_mode;	/*              input pkt buf level (3) */
static int v_deltat;		/* perf sampling period */
static int v_count;		/* iteration count */
static int v_resetloop;
static int v_reg0;
static int v_reg1;
static int v_reg2;
static int v_reg3;
static int v_reg4;
static int v_reg5;
static int v_reg6;
static int v_reg7;
static int v_reg8;
static int v_reg9;
static int v_regf0;
static int v_regf1;
static int v_regf2;
static int v_regf3;
static int v_regf4;
static int v_regf5;
static int v_regf6;
static int v_regf7;
static int v_regf8;
static int v_regf9;

static volatile ulong	perfa_prev_ticks, perfb_prev_ticks;
static xbow_perfcount_t xperfa_prev, xperfb_prev;

#define RD	0x1
#define WR 	0x2
#define RDWR 	0x4

static void
xb_init_vars (void)
{
	f_flags = XBF_VERBOSE | XBF_REGRD |XBF_REGWR;

	v_count = 1;
	v_resetloop = -1;
	v_reg0 = WIDGET_ID;
	v_regf0 = ((RD) << 4)    | sizeof(uint32_t);
	v_reg1 = WIDGET_STATUS;
	v_regf1 = ((RD) << 4)    | sizeof(uint32_t);
	v_reg2 = WIDGET_ERR_UPPER_ADDR;
	v_regf2 = ((RD) << 4)    | sizeof(uint32_t);
	v_reg3 = WIDGET_ERR_LOWER_ADDR;
	v_regf3 = ((RD) << 4)    | sizeof(uint32_t);
	v_reg4 = WIDGET_CONTROL;
	v_regf4 = ((RD|WR|RDWR) << 4) | sizeof(uint32_t);
	v_reg5 = WIDGET_REQ_TIMEOUT;
	v_regf5 = ((RD|WR|RDWR) << 4) | sizeof(uint32_t);
	v_reg6 = WIDGET_INTDEST_UPPER_ADDR;
	v_regf6 = ((RD|WR|RDWR) << 4) | sizeof(uint32_t);
	v_reg7 = WIDGET_INTDEST_LOWER_ADDR;
	v_regf7 = ((RD|WR|RDWR) << 4) | sizeof(uint32_t);
	v_reg8 = WIDGET_ERR_CMD_WORD;
	v_regf8 = ((RD) << 4)    | sizeof(uint32_t);
	v_reg9 = WIDGET_LLP_CFG;
	v_regf9 = ((RD|WR|RDWR) << 4) | sizeof(uint32_t);
}

static void
vf_link(struct SaVar *varp, int val)
{
	*varp->val = val;
	if (xtalk_probe(xb, val)) {
	    f_flags |= XBF_LINKOK;
	    wid = (widget_cfg_t *)K1_MAIN_WIDGET(v_link);
	} else {
	    printf("WARNING no widget in port (0x%x)\n", val);
	    wid = 0;
	    f_flags &= ~XBF_LINKOK;
	}
}

static void
vf_perf_mode(struct SaVar *varp, int val)
{
	printf("%s: ", varp->name);
	switch (val) {

	case XBOW_MONITOR_NONE:
	    printf("set to NONE (0x%x)\n", val);
	    break;

	case XBOW_MONITOR_SRC_LINK:
	    printf("set to SRC_LINK (0x%x)\n", val);
	    break;

	case XBOW_MONITOR_DEST_LINK:
	    printf("set to DST_LINK (0x%x)\n", val);
	    break;

	case XBOW_MONITOR_INP_PKT:
	    printf("set to INP PKT (0x%x)\n", val);
	    break;

	default:
	    printf("bogus mode (0x%x)\n", val);
	    printf("no action taken\n");
	    return;
	}
	*varp->val = val;		/* set to v_perfm */
}

void
vf_perf_port(struct SaVar *varp, int val)
{ 
	xbow_perfcount_t xperf;

	if (val < BASE_XBOW_PORT || val >= MAX_PORT_NUM)
	    printf("Bogus, port 0x%x\n", val);

	*varp->val = val;		/* set to v_perfb */
	xperf.xb_counter_val = xb->xb_perf_ctr_b;
	xperf.xb_perf.link_select = val - BASE_XBOW_PORT;
	if (varp->arg)
	    xb->xb_perf_ctr_a =  xperf.xb_counter_val;
	else
	    xb->xb_perf_ctr_b =  xperf.xb_counter_val;
	printf("%s: is 0x%x, xb reg set\n", varp->name, val);
}

static struct SaVar xb_vars[] = {
	/* name		val	(*func)(struct SaVar *, int)	arg */
        { "link",	&v_link,	vf_link,		0 },
        { "modea",	&v_perfa_mode,	vf_perf_mode,		0 },
        { "modeb",	&v_perfb_mode,	vf_perf_mode,		0 },
        { "porta",	&v_perfa,	vf_perf_port,		1 },
        { "portb",	&v_perfb,	vf_perf_port,		0 },
        { "deltat",	&v_deltat,	0,			0 },
        { "count",	&v_count,	0,			0 },
        { "resetloop",	&v_resetloop,	0,			0 },
	{ "reg0",	&v_reg0,	0,			0 },
	{ "r0rw_sz",	&v_regf0,	0,			0 },
	{ "reg1",	&v_reg1,	0,			0 },
	{ "r1rw_sz",	&v_regf1,	0,			0 },
	{ "reg2",	&v_reg2,	0,			0 },
	{ "r2rw_sz",	&v_regf2,	0,			0 },
	{ "reg3",	&v_reg3,	0,			0 },
	{ "r3rw_sz",	&v_regf3,	0,			0 },
	{ "reg4",	&v_reg4,	0,			0 },
	{ "r4rw_sz",	&v_regf4,	0,			0 },
	{ "reg5",	&v_reg5,	0,			0 },
	{ "r5rw_sz",	&v_regf5,	0,			0 },
	{ "reg6",	&v_reg6,	0,			0 },
	{ "r6rw_sz",	&v_regf6,	0,			0 },
	{ "reg7",	&v_reg7,	0,			0 },
	{ "r7rw_sz",	&v_regf7,	0,			0 },
	{ "reg8",	&v_reg8,	0,			0 },
	{ "r8rw_sz",	&v_regf8,	0,			0 },
	{ "reg9",	&v_reg9,	0,			0 },
	{ "r9rw_sz",	&v_regf9,	0,			0 },
        { 0 },
};

extern struct reg_desc widget_stat_desc[];
extern struct reg_desc widget_llp_confg_desc[];
extern struct reg_desc xbow_link_status_desc[];
extern struct reg_desc xbow_link_aux_status_desc[];

void
xb_reset_link(int port)
{
	widget_cfg_t *wid = (widget_cfg_t *)K1_MAIN_WIDGET(port);
	widgetreg_t control = wid->w_control;

	reset_xbow_link((__psunsigned_t)XBOW_K1PTR, port);
	wid->w_control = control;
	if (f_flags & (XBF_VERBOSE))
	    printf("widget at port=%d reset and re-configed\n",
		    v_link);
}

int
xb_check_alive(int port)
{
/*
    	xbow_t *xb = (xbow_t *)XBOW_K1PTR;
	xb_linkregs_t *xlk = &xb->xb_link(port);
	xbwX_stat_t *linkstatus = (xbwX_stat_t *) &xlk->link_status;
	
	return(linkstatus->xb_linkstatus.alive);
*/
	return(xbow_link_status((__psunsigned_t)XBOW_K1PTR, port));
}


/*
 * print_llp_errs
 */
void
xb_print_llp_info(int port)
{
    	xbow_t *xb = (xbow_t *)XBOW_K1PTR;
	xb_linkregs_t *xlk = &xb->xb_link(port);
	widget_cfg_t *wid = (widget_cfg_t *)K1_MAIN_WIDGET(port);
	widgetreg_t x_sts, x_aux, w_sts, x_llp, w_llp;

	x_sts = (widgetreg_t)xlk->link_status;
	x_aux = (widgetreg_t)xlk->link_aux_status;
	x_llp = (widgetreg_t)xb->xb_wid_llp;
	w_sts = (widgetreg_t)wid->w_status;
	w_llp = (widgetreg_t)wid->w_llp_cfg;

	printf("xbow Link(%d):\n", port);
	printf("  llp cfg: %R\n",
		x_llp, widget_llp_confg_desc);
	printf("  status:  %R\n",
		x_sts, xbow_link_status_desc);
	printf("  aux st:  %R\n",
		x_aux, xbow_link_aux_status_desc);

	printf("widget(%d):\n", port);
	printf("  llp cfg: %R\n",
		w_llp, widget_llp_confg_desc);
	printf("  status:  %R\n",
		w_sts, widget_stat_desc);
}

void
xb_read_nic(int port)
{
	char *nic_buf;
    	bridge_t *bridge;

	if (!(f_flags & XBF_NICREAD))
	   return;

    	bridge = (bridge_t *)K1_MAIN_WIDGET(port);

	nic_buf = malloc(4096);
	if (nic_buf == 0) {
	    printf("cant malloc 4k\n");
	    return;
	}
	cfg_get_nic_info((nic_data_t)&bridge->b_nic, nic_buf);
	free(nic_buf);
}

#define NUM_WREG_TEST 10

struct widget_reg_rw {
    	int	*offset;
	int	*flags;
} reg_rw[NUM_WREG_TEST] = {
   {	&v_reg0, &v_regf0 },
   {	&v_reg1, &v_regf1 },
   {	&v_reg2, &v_regf2 },
   {	&v_reg3, &v_regf3 },
   {	&v_reg4, &v_regf4 },
   {	&v_reg5, &v_regf5 },
   {	&v_reg6, &v_regf6 },
   {	&v_reg7, &v_regf7 },
   {	&v_reg8, &v_regf8 },
   {	&v_reg9, &v_regf9 },
};


typedef union reg_test_u {
	uint8_t		u8[8];
	uint16_t	u16[4];
	uint32_t	u32[2];
	uint64_t	u64;
} reg_test_t;

reg_test_t reg_rw_val[NUM_WREG_TEST];

void
xb_reg_write(volatile void *p, reg_test_t *val, int size)
{
    	switch(size) {
	case (sizeof(uint8_t)): {
	    *(uint8_t *)p = val->u8[7];
	    break;
	}
	case (sizeof(uint16_t)): {
	    *(uint16_t *)p = val->u16[3];
	    break;
	}
	case (sizeof(uint32_t)): {
	    *(uint32_t *)p = val->u32[1];
	    break;
	}
	case (sizeof(uint64_t)): {
	    *(uint64_t *)p = val->u64;
	    break;
	}
	default:
	    break;
	}
}

void
xb_reg_read(volatile void *p, reg_test_t *val, int size)
{
	val->u64 = 0;

    	switch(size) {
	case (sizeof(uint8_t)): {
	    val->u8[7] = *(uint8_t *)p;
	    break;
	}
	case (sizeof(uint16_t)): {
	    val->u16[3] = *(uint16_t *)p;
	    break;
	}
	case (sizeof(uint32_t)): {
	    val->u32[1] = *(uint32_t *)p;
	    break;
	}
	case (sizeof(uint64_t)): {
	    val->u64 = *(uint64_t *)p;
	    break;
	}
	default:
	    break;
	}
}

void
xb_reg_rw(int port)
{
	int i;
    	widgetreg_t *wreg;

	if (!(f_flags | (XBF_REGWR|XBF_REGRD)))
		return;
	for(i=0; i < NUM_WREG_TEST; i++) {
	    wreg = (widgetreg_t *) (K1_MAIN_WIDGET(port) | *(reg_rw[i].offset));
	    /*
	     * check if we need to pre-load the write value
	     */
	    if (f_flags & XBF_REGWR &&
		*(reg_rw[i].flags)>>4 & RDWR) {
		xb_reg_read(wreg, &reg_rw_val[i], (*(reg_rw[i].flags))&0xf);
	    }
	    if (f_flags & XBF_REGWR && *(reg_rw[i].flags)>>4 & WR) {
		xb_reg_write(wreg, &reg_rw_val[i], (*(reg_rw[i].flags))&0xf);
	    }
	    if (f_flags & XBF_REGRD && *(reg_rw[i].flags)>>4 & RD) {
		xb_reg_read(wreg, &reg_rw_val[i], (*(reg_rw[i].flags))&0xf);
	    }
	    if (f_flags & (XBF_VERBOSE))
		printf("offset: %x addr %x val %x [%s%s]\n",
		    *(reg_rw[i].offset), wreg, reg_rw_val[i].u64,
		    (*(reg_rw[i].flags)>>4 & RD)?"R":"",
		    (*(reg_rw[i].flags)>>4 & WR)?"W":"");
	}
}

/*
 * we disable the perf_mode registers
 * but we keep the perf_mode variables as is
 */
void
xb_disable_perf(void)
{
	volatile ulong heart_tick;
	xbow_linkctrl_t ctla, ctlb;
	volatile xbowreg_t *xctla, *xctlb;

	/*
	 * set the perf mode on link(perfa) and link(perfb)
	 */
	xctla = &xb->xb_link_raw[v_perfa-BASE_XBOW_PORT].link_control;
	ctla.xbl_ctrlword = *xctla;
	ctla.xb_linkcontrol.perf_mode = XBOW_MONITOR_NONE;

	xctlb = &xb->xb_link_raw[v_perfb-BASE_XBOW_PORT].link_control;
	ctlb.xbl_ctrlword = *xctlb;
	ctlb.xb_linkcontrol.perf_mode = XBOW_MONITOR_NONE;

	*xctla = ctla.xbl_ctrlword;
	*xctlb = ctlb.xbl_ctrlword;
	flushbus();
}

void
xb_enable_perf(void)
{
	volatile ulong heart_tick;
	xbow_linkctrl_t ctla, ctlb;
	volatile xbowreg_t *xctla, *xctlb;

	/*
	 * get the initial perf value
	 */
	xperfa_prev.xb_counter_val = xb->xb_perf_ctr_a;
	xperfb_prev.xb_counter_val = xb->xb_perf_ctr_b;
	/*
	 * set the perf mode on link(perfa) and link(perfb)
	 */
	xctla = &xb->xb_link_raw[v_perfa-BASE_XBOW_PORT].link_control;
	ctla.xbl_ctrlword = *xctla;
	ctla.xb_linkcontrol.perf_mode = v_perfa_mode;

	xctlb = &xb->xb_link_raw[v_perfb-BASE_XBOW_PORT].link_control;
	ctlb.xbl_ctrlword = *xctlb;
	ctlb.xb_linkcontrol.perf_mode = v_perfb_mode;
	/*
	 * delay the store to the bss based perfa_prev_ticks until
	 * after we start the counters, for now use save the counter
	 * to register
	 */
	heart_tick = HEART_PIU_K1PTR->h_count;
	*xctla = ctla.xbl_ctrlword;
	*xctlb = ctlb.xbl_ctrlword;
	perfa_prev_ticks = perfb_prev_ticks = heart_tick;
	flushbus();
}
/*
 * usage
 */
static char *ustr[] = {
        "                   - if perf counting enabled same as delta",
        "                     else dump xbow registers",
	"dump               - dump xbow regs",
	"link               - dump xbow link regs for slot=<link>",
	"linkreset          - reset xbow link fregs or slot=<link>",
	"wid                - dump widget regs at XIO slot=<link>",
	"counters           - show xbow perf counters a & b",
	"llps               - show xbow llp config register",
	"en                 - set modea/b to enable perf counters",
	"dis                - disable enable perf counters",
	"delta              - show perf counter deltas",
	"sample1            - equivalent to 'en wait(deltat) delta dis'",
	"sample2            - equivalent to 'en llp  delta dis'",
	"llp                - stress llp, read/write wid regs, read nic",
	"llpstat            - show xbow and wid stats for link",
	" ",
        0,
};

/************************ do_command functions ****************************/


/*
 * cmd: ?
 * print usage help
 */
static int
xb_dousage(int argc, char **argv)
{
        sa_usage(xb_str, ustr);
        sa_fusage(xb_str);		/* flags */
        sa_vusage(xb_str);		/* variables */
	printf("\nexample: start monitoring upkts from bridge to heart for 100 ms\n");
	printf("xb flag +reg_rd +reg_wr\n");
	printf("xb set modea=1 modeb=2 porta=0xf portb=8 link=0xf deltat=100\n");
	printf("xb sample1\n");
	printf("\nexample: start monitoring upkts from bridge to heart for 1000 sets of PIO R/W\n");
	printf("xb flag +reg_rd +reg_wr\n");
	printf("xb set modea=1 modeb=2 porta=0xf portb=8 link=0xf count=1000\n");
	printf("xb sample2\n");
	return(0);
}

/*
 * cmd: flag
 * set flags
 */
static int
xb_doflag(int argc, char **argv)
{
        return(sa_flag(argc, argv, xb_str, xbf, &f_flags));
}

/*
 * cmd: set
 * set variable
 */
static int
xb_doset(int argc, char **argv)
{
        if(argc < 3)
            return(sa_listvars(xb_vars, "xb diag cmd"));
        sa_getvars(xb_vars, argv + 2, xb_str);
        return(0);
}

/*
 * cmd: llpstat
 * show llp stats for link n
 */
static int
xb_dollpstats(int argc, char **argv)
{
	xb_print_llp_info(v_link);
	return(0);
}

static int
xb_dollp(int argc, char **argv)
{
	int n = v_count;

	while (n--) {
	    xb_read_nic(v_link);
	    xb_reg_rw(v_link);
	}
	return(0);
}

/*
 * cmd: dump
 * dump registers
 */
static int
xb_dodump(int argc, char **argv)
{
	dump_xbow_regs((__psunsigned_t)xb, 0 /* dont clear*/);
	return(0);
}

/*
 * cmd: link
 * dump link(vlink) registers
 */
static int
xb_dolink_regs(int argc, char **argv)
{
	print_xbow_link_regs(&xb->xb_link(v_link), v_link, 0);
	return(0);
}

/*
 * cmd: linkreset
 * reset link(vlink)
 */
static int
xb_doody_flush(int argc, char **argv)
{
	widget_cfg_t *wid = (widget_cfg_t *)K1_MAIN_WIDGET(v_link);
	widgetreg_t control = wid->w_control;
    	widgetreg_t *wreg, wsav, wxxx;

	wreg = (widgetreg_t *) (K1_MAIN_WIDGET(v_link) | v_reg0);
	wsav = *wreg;
	if (f_flags & (XBF_VERBOSE))
	    printf("will assert ody flush 0x%x(v0) = 0x%x(v1) for %d(v2)us\n",
	    	wreg, (wsav|v_reg1), v_reg2);
	*wreg = (wsav | v_reg1);
	wxxx = *wreg;
	DELAY(v_reg2);
	/**wreg = wsav;*/

	reset_xbow_link((__psunsigned_t)XBOW_K1PTR, v_link);
	wid->w_control = control;
	if (f_flags & (XBF_VERBOSE))
	    printf("widget at port=%d reset and re-configed\n",
		    v_link);

    	return(0);
}


/*
 * cmd: linkreset
 * reset link(vlink)
 */
static int
xb_dolink_reset(int argc, char **argv)
{
	xb_reset_link(v_link);
	while (v_resetloop && !xb_check_alive(v_link)) {
	    xb_reset_link(v_link);
	    if (v_resetloop > 0)
		v_resetloop--;
	}
	return(0);
}

/*
 * cmd: wid 
 * dump registers for widget at XIO slot (v_link) 
 */
static int
xb_dowid_regs(int argc, char **argv)
{
	if (v_link < XBOW_PORT_8 || v_link > XBOW_PORT_F) {
	    printf("bad xio port(%d), set valid link value\n", v_link);
	    return(0);
	}
	printf("Widget Registers at XIO port=%d\n", v_link);
	dump_widget_regs((widget_cfg_t *)K1_MAIN_WIDGET(v_link));
	return(0);
}

/*
 * cmd: perf_ctrs
 * dump perf registers
 */
static int
xb_doperf_ctrs(int argc, char **argv)
{
	print_xbow_perf_ctrs(xb);
	return(0);
}

/*
 * cmd: retrys
 * dump retrys registers
 */
static int
xb_dollpsts(int argc, char **argv)
{
	xbow_aux_link_status_t aux_sts;

	aux_sts.aux_linkstatus
		= xb->xb_link_raw[v_link-BASE_XBOW_PORT].link_aux_status;
	printf("link(0x%x): LLP STATS: rcv errs %d, tx retrys %d\n",
		v_link,
		aux_sts.xb_aux_linkstatus.rx_err_cnt,
		aux_sts.xb_aux_linkstatus.tx_retry_cnt);
	return(0);
}

static int
xb_doperf_dis(int argc, char **argv)
{
	xb_disable_perf();
	perf_enbled = 0;
	return(0);
}

static int
xb_doperf_en(int argc, char **argv)
{
	perf_enbled = 1;
	xb_enable_perf();
	return(0);
}

#define K(_n)	(1000 * (_n))
#define MILL	1000000L
#define BILL	1000000000L

static int
xb_doperf_delta(int argc, char **argv)
{
	xbow_perfcount_t xperfa, xperfb;
	volatile ulong heart_tick;
	ulong d, count, ups, kbs;

	xperfa.xb_counter_val = xb->xb_perf_ctr_a;
	xperfb.xb_counter_val = xb->xb_perf_ctr_b;
	heart_tick = HEART_PIU_K1PTR->h_count;
	
	d = (heart_tick - perfa_prev_ticks) * 80;	/* in ns */
	/*
	 * if we rolled over just once its still OK
	 * if more than once than all bets are off
	 */
	if (xperfa.xb_perf.count >= xperfa_prev.xb_perf.count)
	    count = xperfa.xb_perf.count - xperfa_prev.xb_perf.count;
	else
	    count = (XBOW_COUNTER_MASK + 1 - xperfa_prev.xb_perf.count)
	    	    + xperfa.xb_perf.count;

	ups = (count * BILL)/d;
	if (v_deltat < 1000 && d < BILL)
	    kbs = ((ups) * (ulong)v_deltat)/1000;
	else
	    kbs = ups;
	kbs = ((kbs * 128) + 512)/1024;
	printf("Counter Delta:\telapsed time (ns)\tupkts/s\tKB/s\n");
	printf("  A: %7d\t%3d,%03d,%03d,%03d\t\t%u\t%u,%03u\n",
		count,
		d/BILL, (d/MILL) % 1000, (d/1000) % 1000, d % 1000,
		ups, kbs/1000, kbs%1000);

	d = (heart_tick - perfb_prev_ticks) * 80;	/* in ns */
	if (xperfb.xb_perf.count >= xperfb_prev.xb_perf.count)
	    count = xperfb.xb_perf.count - xperfb_prev.xb_perf.count;
	else
	    count = (XBOW_COUNTER_MASK + 1 - xperfb_prev.xb_perf.count)
	    	    + xperfb.xb_perf.count;

	ups = (count * BILL)/d;
	if (v_deltat < 1000 && d < BILL)
	    kbs = ((ups) * (ulong)v_deltat)/1000;
	else
	    kbs = ups;
	kbs = ((kbs * 128) + 512)/1024;
	printf("  B: %7d\t%3d,%03d,%03d,%03d\t\t%u\t%u,%03u\n",
		count,
		d/BILL, (d/MILL) % 1000, (d/1000) % 1000, d % 1000,
		ups, kbs/1000, kbs%1000);
	xperfa_prev = xperfa;
	xperfb_prev = xperfb;
	perfa_prev_ticks = perfb_prev_ticks = heart_tick;
	return(0);
}

static int
xb_doperf_sample1(int argc, char **argv)
{
	printf("Sampling perf counters for %d msecs\n\n", v_deltat);
	xb_doperf_en(argc, argv);
	us_delay(K(v_deltat));
	xb_doperf_delta(argc, argv);
	xb_doperf_dis(argc, argv);
	return(0);
}

static int
xb_doperf_sample2(int argc, char **argv)
{
	printf("Sampling perf counters for %d count of PIO R/W\n\n", v_count);
	xb_doperf_en(argc, argv);
	xb_dollp(argc, argv);
	xb_doperf_delta(argc, argv);
	xb_doperf_dis(argc, argv);
	return(0);
}
/********************************/
/*
 * probe the device
 * returns 0 if error, else 1
 */
int
xb_probe(void)
{
	uint base;
	jmp_buf faultjmp;
	volatile int intstat;
	char *msg;

	if(setjmp(faultjmp)) {
	    show_fault();
	    printf("xb_probe failed for xbow or link(0x%x)\n",
	    	v_link);
	    return(0);
	}
	nofault = faultjmp;

	/* initializations to access the device h/w */

	xb = XBOW_K1PTR;
	xb->xb_wid_id;

	wid = (widget_cfg_t *)K1_MAIN_WIDGET(v_link);
	wid->w_id;

	/* peek and poke the device registers */

	nofault = 0;
	return(1);
}

/*
 * cm commands
 */
static  struct SaCmd xb_cmds[] = {
	/* flags	cmd		(*func)(int, char **) */
        { 0,	"set",		xb_doset },
        { 0,	"flag",		xb_doflag },
        { 0,	"?",		xb_dousage },
        { 0,	"llpstat",	xb_dollpstats },
        { 0,	"llp",		xb_dollp },
        { 0,	"dump",		xb_dodump },
        { 0,	"link",		xb_dolink_regs },
        { 0,	"linkreset",	xb_dolink_reset },
        { 0,	"wid",		xb_dowid_regs },
        { 0,	"counters",	xb_doperf_ctrs },
        { 0,	"llps",		xb_dollpsts },
        { 0,	"en",		xb_doperf_en },
        { 0,	"dis",		xb_doperf_dis },
	{ 0,	"delta",	xb_doperf_delta },
	{ 0,	"odyflush",	xb_doody_flush },
	{ 0,	"sample1",	xb_doperf_sample1 },
        { 0,	"sample2",	xb_doperf_sample2 },
        { 0 },
};

static jmp_buf restart_buf;
static void
main_intr(void)
{
	printf("Interrupt ... \n");
	longjmp (restart_buf, 1);
}

/*ARGSUSED*/
/*
 * main entry
 */
int
xb_diag(int argc, char **argv, char **envp)
{
	if(!xb) {
	    xb = XBOW_K1PTR;
	    xb_init_vars();
	}
	if(!wid && (f_flags & XBF_LINKOK))
	    wid = (widget_cfg_t *)K1_MAIN_WIDGET(v_link);

	Signal(SIGINT, main_intr);
	if (setjmp(restart_buf)) {
	    return(0);
	}

        if(argc < 2)
	    if (perf_enbled)
		return(xb_doperf_delta(argc, argv));
	    else
		return(xb_dodump(argc, argv));
        return(sa_cmd(argc, argv, xb_cmds, xb_str, xb_probe));
}
#endif /* SA_DIAG */
