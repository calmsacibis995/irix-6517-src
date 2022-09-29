/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident	"lib/libsk/ml/xbow.c:  $Revision: 1.36 $"

/* xbow.c -- crossbow specific routines */
/*
 * NOTE:
 * This file needs to be shared between the RACER and SN0 projects.
 * At this time, (10/10/95), since it is not yet decided  as to how
 * to do it, I will do the following:
 * Add routines with different names, if they have a functionality
 * that is different from the RACER routines. If there is a common
 * functionality, I will try to use ifdef SN0.
 * For example, SN0 needs a init_xbow_discover, which disables
 * interrupts during discovery. This will be different from the
 * general 'init_xbow' routine. Also, the SN0 registers reside
 * in the HUB's IO Space and not PHYS_TO_K1. So, every 
 * assignment statement will be different.
 * At this stage, it is important to read the comments to understand
 * what is going on, rather than the code.
 *                                      -- Srinivasa Prasad
 *                                         sprasad@engr.sgi.com
 */

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <libsk.h>
#include <libsc.h>
#ifdef SN0
#include <sys/SN/xbow.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/promcfg.h>
#endif

/* forward functions */
int xtalk_probe(xbow_t *,int );

#ifdef IP30

/*
 * init_heart_xbow_credit: set up xbow credits
 * for the specified widget, both directions.
 */
void
init_xbow_credit(xbow_t *xbow, int port, int credits)
{
	xbowreg_t	lc;

	if (!xbow) return;

	/* set up credit count in the crossbow */
	lc = xbow->xb_link(port).link_control;
	lc &= ~XB_CTRL_WIDGET_CR_MSK;
	lc |= credits << XB_CTRL_WIDGET_CR_SHFT;
	xbow->xb_link(port).link_control = lc;
}

/*
 * init_heart_xbow_dev: call the proper
 * initialization routine for the specified widget.
 * credit counts have already been established.
 */
void
init_xbow_widget(xbow_t *xbow, int port, widget_cfg_t *widget)
{
    widgetreg_t	id;
    widgetreg_t ctrl;
    int		part;

    id = widget->w_id;
    part = (id & WIDGET_PART_NUM) >> WIDGET_PART_NUM_SHFT;

    /* set up credit count and widget id in the widget */
    ctrl = widget->w_control & ~(WIDGET_LLP_XBAR_CRD | WIDGET_WIDGET_ID);
    if (XWIDGET_REV_NUM(xbow->xb_wid_id) <= 2)
    	ctrl |= ((XBOW_CREDIT - 1) << WIDGET_LLP_XBAR_CRD_SHFT) | port;
    else
    	ctrl |= (XBOW_CREDIT << WIDGET_LLP_XBAR_CRD_SHFT) | port;
    widget->w_control = ctrl;

    switch (part) {
#if NOTDEV
    case XBOW_WIDGET_PART_NUM:
	/* this would be an "additional" xbow */
	break;
#endif

    case HEART_WIDGET_PART_NUM:
	init_xbow_credit(xbow, port, HEART_CREDIT);
	break;

    case BRIDGE_WIDGET_PART_NUM:
	init_xbow_credit(xbow, port, BRIDGE_CREDIT);
	init_bridge(widget);
	break;

    default:
	/* "not someone that needs early init." */
	break;
    }
}

/*
 * initialize an xbow ASIC
 */
/*ARGSUSED2*/
void
init_xbow(xbow_t *xbow)
{
#define	XBOW_LINK_INTR_IE	(XB_CTRL_ILLEGAL_DST_IE |	\
				 XB_CTRL_OALLOC_IBUF_IE |	\
				 XB_CTRL_BNDWDTH_ALLOC_IE |	\
				 XB_CTRL_RCV_CNT_OFLOW_IE |	\
				 XB_CTRL_XMT_CNT_OFLOW_IE |	\
				 XB_CTRL_XMT_MAX_RTRY_IE |	\
				 XB_CTRL_MAXREQ_TOUT_IE)

	int	port;

	/* route interrupt to the system interrupt controller */
	xbow->xb_wid_int_upper = INT_DEST_UPPER(XBOW_VEC);
	xbow->xb_wid_int_lower = INT_DEST_LOWER;

	/* enable all error checking in xbow widget */
	xbow->xb_wid_control = XB_WID_CTRL_REG_ACC_IE |
		XB_WID_CTRL_XTALK_IE;

	/* enable all error checking on all links */
	for (port = BASE_XBOW_PORT; port < MAX_PORT_NUM; port++) {
		xbowreg_t	link_ctrl;

		link_ctrl = xbow->xb_link(port).link_control;
		if (xbow->xb_link(port).link_status_clr & XB_STAT_LINKALIVE)
			link_ctrl |= XBOW_LINK_INTR_IE;
		else
			link_ctrl = (link_ctrl & ~XBOW_LINK_INTR_IE) |
				XB_CTRL_LINKALIVE_IE;
		xbow->xb_link(port).link_control = link_ctrl;

		xbow->xb_link(port).link_arb_upper = 0x0;
		xbow->xb_link(port).link_arb_lower = 0x0;
	}
}

#endif	/* IP30 */

#ifndef IP30_RPROM

extern struct reg_desc widget_err_cmd_word_desc[];

struct reg_desc xbow_widget_status_desc[] = {
	{0x1 << 31,	0,	"LINK_F",		NULL,	NULL},
	{0x1 << 30,	0,	"LINK_E",		NULL,	NULL},
	{0x1 << 29,	0,	"LINK_D",		NULL,	NULL},
	{0x1 << 28,	0,	"LINK_C",		NULL,	NULL},
	{0x1 << 27,	0,	"LINK_B",		NULL,	NULL},
	{0x1 << 26,	0,	"LINK_A",		NULL,	NULL},
	{0x1 << 25,	0,	"LINK_9",		NULL,	NULL},
	{0x1 << 24,	0,	"LINK_8",		NULL,	NULL},
	{0x1 << 23,	0,	"XBOW",			NULL,	NULL},
	{0x1 << 5,	0,	"REG_ACC",		NULL,	NULL},
	{0x1 << 2,	0,	"XTALK",		NULL,	NULL},
	{0x1 << 1,	0,	"CONNECT_TIMEOUT",	NULL,	NULL},
	{0x1 << 0,	0,	"MULTI",		NULL,	NULL},
	{0,		0,	NULL,			NULL,	NULL},
};

struct reg_desc xbow_link_status_desc[] = {
	{0x1 << 31,	0,	"LINK_ALIVE",		NULL,	NULL},
	{0x1 << 18,	0,	"MULTI",		NULL,	NULL},
	{0x1 << 17,	0,	"ILLEGAL_DST",		NULL,	NULL},
	{0x1 << 16,	0,	"IBUF_OVER_ALLOC",	NULL,	NULL},
	{0xff << 8,	-8,	"BANDWIDTH_ALLOC_PORT",	"0x%x",	NULL},
	{0x1 << 7,	0,	"RCV_RETRY_OVERFLOW",	NULL,	NULL},
	{0x1 << 6,	0,	"XMT_RETRY_OVERFLOW",	NULL,	NULL},
	{0x1 << 5,	0,	"XMT_MAX_RETRY",	NULL,	NULL},
	{0x1 << 4,	0,	"RCV_ERR",		NULL,	NULL},
	{0x1 << 3,	0,	"XMT_RETRY",		NULL,	NULL},
	{0x1 << 1,	0,	"MAXREQ_TIMEOUT",	NULL,	NULL},
	{0x1 << 0,	0,	"SRC_TIMEOUT",		NULL,	NULL},
	{0,		0,	NULL,			NULL,	NULL},
};

static struct reg_values port_size_values[] = {
	{0,	"16"},
	{1,	"8"},
	{0,	NULL},
};

static struct reg_values port_valid_values[] = {
	{0,	"EMPTY"},
	{1,	"ON"},
	{0,	NULL},
};

struct reg_desc xbow_link_aux_status_desc[] = {
	{0xffL << 24,	-24,	"RCV_RETRIES",	"%d",	NULL},
	{0xff << 16,	-16,	"XMT_RETRIES",	"%d",	NULL},
	{0xff << 8,	-8,	"TIMEOUT_SRC",	"0x%x",	NULL},
	{0x1  << 6,	-6,	"LINKFAIL_RST_BAD",	NULL,	NULL},
	{0x1  << 5,	-5,	"PORT",		NULL,	port_valid_values},
	{0x1  << 4,	-4,	"PORT_SIZE",	NULL,	port_size_values},
	{0,		0,	NULL,		NULL,	NULL},
};

struct reg_desc widget_llp_confg_desc[] = {
	{WIDGET_LLP_MAXRETRY,	-WIDGET_LLP_MAXRETRY_SHFT,
			"MAXRETRY",	"%d",	NULL},
	{WIDGET_LLP_NULLTIMEOUT,-WIDGET_LLP_NULLTIMEOUT_SHFT,
			"NULLTIMEOUT",	"%d",	NULL},
	{WIDGET_LLP_MAXBURST,	-WIDGET_LLP_MAXBURST_SHFT,
			"MAXBURST",	"%d",	NULL},
	{0,		0,	NULL,			NULL,	NULL},
};

static struct reg_values perf_md_values[] = {
	{0,	"MONITOR NONE"},
	{1,	"MONITOR SRC"},
	{2,	"MONITOR DST"},
	{3,	"MONITOR BUF"},
	{0,	NULL},
};

static struct reg_values ibuf_lvl_values[] = {
	{0x18,	"0 entries"},
	{0x19,	"1 entries"},
	{0x1A,	"2 entries"},
	{0x1B,	"3 entries"},
	{0x1C,	"4 entries"},
	{0,	NULL},
};

#define DESC_IBUF_LEVEL_MSK (XB_CTRL_PERF_CTR_MODE_MSK | XB_CTRL_IBUF_LEVEL_MSK)

struct reg_desc xbow_link_control_desc[] = {
	{XB_CTRL_LINKALIVE_IE,	     0,	"LA_IE",	NULL,	NULL},
	{XB_CTRL_PERF_CTR_MODE_MSK,-28,	"PERF",		NULL,	perf_md_values},
	{DESC_IBUF_LEVEL_MSK,   -25,	"IBUF_LVL",	NULL,	ibuf_lvl_values},
	{XB_CTRL_8BIT_MODE,	     0,	"8BIT",		NULL,	NULL},
	{XB_CTRL_WIDGET_CR_MSK,	   -18,	"WID_CR",	"%d",	NULL},
	{XB_CTRL_ILLEGAL_DST_IE,     0,	"ILLEGAL_DST",	NULL,	NULL},
	{XB_CTRL_OALLOC_IBUF_IE,     0,	"IBUF_OVER_ALLOC",NULL,	NULL},
	{XB_CTRL_BNDWDTH_ALLOC_IE,   0,	"BANDWIDTH_ALLOC",NULL,	NULL},
	{XB_CTRL_RCV_CNT_OFLOW_IE,   0,	"RCV_RETRY_OVERFLOW", NULL, NULL},
	{XB_CTRL_XMT_CNT_OFLOW_IE,   0,	"XMT_RETRY_OVERFLOW", NULL, NULL},
	{XB_CTRL_XMT_MAX_RTRY_IE,    0,	"XMT_MAX_RETRY",NULL,	NULL},
	{XB_CTRL_RCV_IE,	0,	"RCV_ERR",	NULL,	NULL},
	{XB_CTRL_XMT_RTRY_IE,	0,	"XMT_RETRY",	NULL,	NULL},
	{XB_CTRL_MAXREQ_TOUT_IE,0,	"MAXREQ_TIMEOUT",NULL,	NULL},
	{XB_CTRL_SRC_TOUT_IE,	0,	"SRC_TIMEOUT",	NULL,	NULL},
	{0,			0,	NULL,		NULL,	NULL},
};

#define RR_ARB(n)	(XB_ARB_RR_MSK  << XB_ARB_RR_SHFT(n))
#define GBR_ARB(n)	(XB_ARB_GBR_MSK << XB_ARB_GBR_SHFT(n))

struct reg_desc xbow_link_arb_upper_desc[] = {
	{RR_ARB(3),	-(XB_ARB_RR_SHFT(3)),	"PORTB_RR",	"%d",	NULL},
	{GBR_ARB(3),	-(XB_ARB_GBR_SHFT(3)),	"PORTB_GBR",	"%d",	NULL},
	{RR_ARB(2),	-(XB_ARB_RR_SHFT(2)),	"PORTA_RR",	"%d",	NULL},
	{GBR_ARB(2),	-(XB_ARB_GBR_SHFT(2)),	"PORTA_GBR",	"%d",	NULL},
	{RR_ARB(1),	-(XB_ARB_RR_SHFT(1)),	"PORT9_RR",	"%d",	NULL},
	{GBR_ARB(1),	-(XB_ARB_GBR_SHFT(1)),	"PORT9_GBR",	"%d",	NULL},
	{RR_ARB(0),	-(XB_ARB_RR_SHFT(0)),	"PORT8_RR",	"%d",	NULL},
	{GBR_ARB(0),	-(XB_ARB_GBR_SHFT(0)),	"PORT8_GBR",	"%d",	NULL},
	{0,			0,	NULL,		NULL,	NULL},
};

struct reg_desc xbow_link_arb_lower_desc[] = {
	{RR_ARB(3),	-(XB_ARB_RR_SHFT(3)),	"PORTF_RR",	"%d",	NULL},
	{GBR_ARB(3),	-(XB_ARB_GBR_SHFT(3)),	"PORTF_GBR",	"%d",	NULL},
	{RR_ARB(2),	-(XB_ARB_RR_SHFT(2)),	"PORTE_RR",	"%d",	NULL},
	{GBR_ARB(2),	-(XB_ARB_GBR_SHFT(2)),	"PORTE_GBR",	"%d",	NULL},
	{RR_ARB(1),	-(XB_ARB_RR_SHFT(1)),	"PORTD_RR",	"%d",	NULL},
	{GBR_ARB(1),	-(XB_ARB_GBR_SHFT(1)),	"PORTD_GBR",	"%d",	NULL},
	{RR_ARB(0),	-(XB_ARB_RR_SHFT(0)),	"PORTC_RR",	"%d",	NULL},
	{GBR_ARB(0),	-(XB_ARB_GBR_SHFT(0)),	"PORTC_GBR",	"%d",	NULL},
	{0,			0,	NULL,		NULL,	NULL},
};

#define	XBOW_INTR	(XB_WID_STAT_WIDGET0_INTR |	\
			 XB_WID_STAT_LINK_INTR(0x8) |	\
			 XB_WID_STAT_LINK_INTR(0x9) |	\
			 XB_WID_STAT_LINK_INTR(0xa) |	\
			 XB_WID_STAT_LINK_INTR(0xb) |	\
			 XB_WID_STAT_LINK_INTR(0xc) |	\
			 XB_WID_STAT_LINK_INTR(0xd) |	\
			 XB_WID_STAT_LINK_INTR(0xe) |	\
			 XB_WID_STAT_LINK_INTR(0xf))

xbowreg_t
print_xbow_status(__psunsigned_t xbow_base, int clear)
{

	xbowreg_t global_err_reg_set = 0;
	int link;
	xbowreg_t status;

	status = *(volatile xbowreg_t *)(xbow_base + XBOW_WID_STAT);
	if (!(status & XBOW_INTR))
		return status;

	global_err_reg_set = status & (XB_WID_STAT_REG_ACC_ERR | XB_WID_STAT_XTALK_ERR );

	printf("XBOW status: %R\n",
		(__uint64_t)status, xbow_widget_status_desc);

	for (link = BASE_XBOW_PORT; link < MAX_PORT_NUM; link++) {
		xbowreg_t link_status;
		xbowreg_t link_aux_status;

		if (!(status & XB_WID_STAT_LINK_INTR(link)))
			continue;

		link_status = *(volatile xbowreg_t *)
			(xbow_base + XB_LINK_STATUS(link));
		printf("   Link 0x%x status: %R\n",
			link, link_status, xbow_link_status_desc);
		if (link_status &
		    (XB_STAT_ILLEGAL_DST_ERR | XB_STAT_MAXREQ_TOUT_ERR))
			global_err_reg_set = 1;

		link_aux_status = *(volatile xbowreg_t *)
			(xbow_base + XB_LINK_AUX_STATUS(link));
		printf("      Aux status: %R\n",
			link_aux_status, xbow_link_aux_status_desc);

		if (clear)
			*(volatile xbowreg_t *)
				(xbow_base + XB_LINK_STATUS_CLR(link));
	}

	if (global_err_reg_set) {
		__uint64_t err_addr;
		xbowreg_t err_addr_lower;
		xbowreg_t err_addr_upper;
		xbowreg_t err_cmd_word;

		err_cmd_word = *(volatile xbowreg_t *)
			(xbow_base + XBOW_WID_ERR_CMDWORD);
		printf("	Error command word: %R\n",
			err_cmd_word, widget_err_cmd_word_desc);

		err_addr_lower = *(volatile xbowreg_t *)
			(xbow_base + XBOW_WID_ERR_LOWER);
		err_addr_upper = *(volatile xbowreg_t *)
			(xbow_base + XBOW_WID_ERR_UPPER);
		err_addr = ((__uint64_t)err_addr_upper << 32) | err_addr_lower;
		printf("        Error address: 0x%Lx\n", err_addr);

		if (clear)
			*(volatile xbowreg_t *)
				(xbow_base + XBOW_WID_ERR_CMDWORD) = 0;
	}

	return status;
}

void
print_xbow_perf_ctrs(xbow_t *xb)
{
	xbow_perfcount_t xperf;

	printf("Performance counters:\n");
	xperf.xb_counter_val = xb->xb_perf_ctr_a;
	printf("\tA: link select(%02d) count %d\n", 
	    xperf.xb_perf.link_select + BASE_XBOW_PORT, xperf.xb_perf.count);
	xperf.xb_counter_val = xb->xb_perf_ctr_b;
	printf("\tB: link select(%02d) count %d\n", 
	    xperf.xb_perf.link_select + BASE_XBOW_PORT, xperf.xb_perf.count);

}

void
print_xbow_link_regs(xb_linkregs_t *xlk, int link, int clear)
{
	xbowreg_t link_status;
	xbowreg_t link_aux_status;

	printf("Link(0x%x)\n", link);
	printf("\tcontrol: %R\n",
		xlk->link_control, xbow_link_control_desc);

	link_status = xlk->link_status;
	printf("\tstatus: %R\n",
		link_status, xbow_link_status_desc);
	if (link_status &
	    (XB_STAT_ILLEGAL_DST_ERR | XB_STAT_MAXREQ_TOUT_ERR))
		printf("\tlink error detected ",
			"ILLEGAL_DST_ERR/CNNCT_TOUT_ERR\n");

	printf("\tArbitration GBR and RR (add 1 for RR values)\n");
	printf("\tupper %R\n",
		xlk->link_arb_upper, xbow_link_arb_upper_desc);
	printf("\tlower %R\n",
		xlk->link_arb_lower, xbow_link_arb_lower_desc);

	printf("\taux status: %R\n",
		xlk->link_aux_status, xbow_link_aux_status_desc);
	if (clear)
		xlk->link_status_clr;
}

xbowreg_t
dump_xbow_regs(__psunsigned_t xbow_base, int clear)
{
	int link;
	xbow_t	*xb = (xbow_t *)xbow_base;
	xbowreg_t status;

	status = xb->xb_wid_stat;
	printf("XBOW widget status register: %R\n",
		status, xbow_widget_status_desc);

	if (status & (XB_WID_STAT_REG_ACC_ERR | XB_WID_STAT_XTALK_ERR))
		printf("\tREG_ACC|XTALK error detected\n");

	printf("Error command word: %R\n",
		xb->xb_wid_err_cmdword, widget_err_cmd_word_desc);
	printf("Error address: 0x%Lx\n",
		((__uint64_t)xb->xb_wid_err_upper << 32)|xb->xb_wid_err_lower);

	printf("LLP Configuration: %R\n",
	    xb->xb_wid_llp, widget_llp_confg_desc);

	for (link = BASE_XBOW_PORT; link < MAX_PORT_NUM; link++) {
		xb_linkregs_t *xlk;

		if (!xtalk_probe(xb, link))
			continue;

		print_xbow_link_regs(&xb->xb_link(link-BASE_XBOW_PORT),
				     link, clear);
	}
	print_xbow_perf_ctrs(xb);

	if (clear)
		xb->xb_wid_err_cmdword = 0;

	return status;
}
#endif /* IP30_RPROM */


static int
xlink_check(xbow_t *xbow, int port)
{
	return xbow->xb_link(port).link_aux_status & XB_AUX_STAT_PRESENT &&
		xbow->xb_link(port).link_status & XB_STAT_LINKALIVE;
							    
}

#if SA_DIAG
#include <sys/RACER/sflash.h>
flash_pds_ent_t *flash_findenv(flash_pds_ent_t *p, char *var);
void flash_pds_init_free(void);

/*
 * stupendous hack here, the env vars may not be initialized
 * so dive into the flash area where we keep the extend env vars
 * the 'PDS' area (initializing the sflash module, BTW but its
 * harmless when they get initialized again later) to look if
 * there is a xport_disable env var ...
 */
char *
check_disable(char *str)
{
	static char disable_str[8];
    	flash_pds_ent_t *ent;
	char *cp;
	int len;

	(void) flash_init();
	flash_pds_init_free();
	ent = flash_findenv(0, str);
	if (ent) {
	    cp = (char *)ent->data;
	    cp += strlen(str) + 1;
	    strncpy(disable_str, cp, 7); 
	    return(disable_str);
	}
	return(0);

}

/* If XTALK_PORT_DISABLE is defined,
 * we ignore stuff the defined XTalk port
 * for which xtalk_probe returns zero..
 */
#define XPORT_DISABLE(p)   xtalk_port_disable(p)
static int
xtalk_port_disable(int port)
{
	char *env_port;
	static int xport_disable;

	if (!xport_disable) {
	    if (env_port = check_disable("xport_disable")) {
		xport_disable = atoi(env_port);
		printf("XIO slot xport_disable=%d will not be probed\n",
			xport_disable);
		if (xport_disable < XBOW_PORT_8 || xport_disable > XBOW_PORT_F)
		    xport_disable = -1;
	    }
	}
	return(port == xport_disable);
}

int
xbow_link_alive(volatile __psunsigned_t xbow_base, int link)
{
	xb_linkregs_t 	*lp;

	lp = (xb_linkregs_t *)(xbow_base + XB_LINK_REG_BASE(link));
	if (lp->link_status & XB_STAT_LINKALIVE) 
	    return 1;

	return 0;
}

int
xbow_link_present(volatile __psunsigned_t xbow_base, int link)
{
	xb_linkregs_t 	*lp;

	lp = (xb_linkregs_t *)(xbow_base + XB_LINK_REG_BASE(link));
	if (lp->link_aux_status & XB_AUX_STAT_PRESENT)
	    return 1;

	return 0;
}


int
xbow_link_status(volatile __psunsigned_t xbow_base, int link)
{
	xb_linkregs_t 	*lp;

	lp = (xb_linkregs_t *)(xbow_base + XB_LINK_REG_BASE(link));
	if ((lp->link_status & XB_STAT_LINKALIVE) &&
	    (lp->link_aux_status & XB_AUX_STAT_PRESENT))
	    return 1;

        if ((lp->link_aux_status & XB_AUX_STAT_PRESENT) && 
		(!(lp->link_status & XB_STAT_LINKALIVE)))
		printf("WARNING: xbow_base: 0x%lx link: %d Widget present, "
			"but link not alive!\n", xbow_base, link);

	return 0;
}

#endif /* SA_DIAG */

int
xtalk_probe(xbow_t *xbow, int port)
{
	widget_cfg_t	*widget;
	int		probe_ok = 1;

#ifdef XPORT_DISABLE
	if (XPORT_DISABLE(port)) {
	    printf("xtalk_probe: not probing XTalk slot %d\n", port);
	    return 0;
	}
#endif
	/* check access to the 3 required widget registers */
	if (!xlink_check(xbow, port)) {
	    probe_ok = 0;
	}
#if IP30 && !IP30_RPROM
	else {
	    widget = (widget_cfg_t *)K1_MAIN_WIDGET(port);
	    if (badaddr(&widget->w_id, 4) ||
		badaddr(&widget->w_llp_cfg, 4) ||
		badaddr(&widget->w_control, 4)) {
		probe_ok = 0;

		/*
		 * This link is to be disabled, clear link(port) status and
		 * disable intrs previously enabled in init_xbow
		 */
		xbow->xb_link(port).link_status_clr;
		xbow->xb_link(port).link_control &= ~XBOW_LINK_INTR_IE;
	    }
	}
#endif	/* IP30 && !IP30_RPROM */

	return probe_ok;
}

void
init_xbow_link(__psunsigned_t xbow_base, int link)
{
	xb_linkregs_t 	*lp;

	lp = (xb_linkregs_t *)(xbow_base + XB_LINK_REG_BASE(link));

        lp->link_status; /* clear interrupts */
        lp->link_control = 0 ; /* NO Interrupts */
        lp->link_arb_upper = 0 ;
        lp->link_arb_lower = 0 ;
        lp->link_reset = 0 ;
}

/*
 * reset the widget and re-initialize to same config
 * by saving the link control and restoring them. 
 */
void
reset_xbow_link(__psunsigned_t xbow_base, int link)
{
	xb_linkregs_t 	*lp;
	xbowreg_t	save_link_control;

	lp = (xb_linkregs_t *)(xbow_base + XB_LINK_REG_BASE(link));
        save_link_control = lp->link_control;

	init_xbow_link(xbow_base, link);
	DELAY(1000);
	lp->link_control = save_link_control;
}

