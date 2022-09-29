#ident  "lib/libsk/ml/heart.c:  $Revision: 1.63 $"

/* heart.c -- HEART specific routines */

#define	VPRINTF(x)	/* printf x */
#define	DPRINTF(x)	/* printf x */

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/RACER/racermp.h>
#include <libsk.h>
#include <libsc.h>
#include <standcfg.h>

/* If XTALK_PORT_DISABLE is defined,
 * we ignore stuff in XTalk ports
 * for which it returns nonzero..
#define	XTALK_PORT_DISABLE(p)	(p != 15)
 */

extern int master_cpuid;

#ifdef HEART_COHERENCY_WAR
int		fibbed_heart;
static int	heart_read_coherency_war;
static int	heart_write_coherency_war;
#endif

/*
 * init_heart: initialize the heart chip, and
 * call init routines for chips "outboard".
 */
void
init_heart(heart_piu_t *heart, heart_cfg_t *heartcfg)
{
	heartreg_t	enable_intr;
	heartreg_t	heart_mode;
	heartreg_t	heart_stat;
	heartreg_t	imr_softpower;
	heartreg_t	imr_widevec;
	int		i;
	int		port;
	widget_cfg_t   *widget;
	xbow_t		*xbow = XBOW_K1PTR;
	int		countfan = (MPCONF->fanloads == 0);
	/* Though this value was set before mem init in csu.s,
	 * we do it again here incase we re-flash the prom from irix
	 * and user does reboot. This allows the new prom to set the
	 * value here incase the 'older' version hadn't.
	 */
	heart->h_mem_ref = HEART_MEMREF_VAL; 

#define	HEART_W_STAT_8BIT	(0x1 << 7)
#define	HEART_MODE_DEFAULT	(HM_COR_ECC_LCK |	\
				HM_PE_SYS_COR_ERE |	\
				HM_GLOBAL_ECC_EN |	\
				HM_INT_EN |		\
				HM_DATA_CHK_EN |	\
				HM_REF_EN |		\
				HM_BAD_SYSWR_ERE |	\
				HM_SYSCMD_ERE |		\
				HM_NCOR_SYS_ERE |	\
				HM_COR_SYS_ERE |	\
				HM_DATA_ELMNT_ERE |	\
				HM_MEM_ADDR_PROC_ERE |	\
				HM_MEM_ADDR_IO_ERE |	\
				HM_NCOR_MEM_ERE	|	\
				HM_COR_MEM_ERE)

	heart_stat = heart->h_status;
	heart_mode = heart->h_mode;

	heart_mode |= HEART_MODE_DEFAULT;	/* keep cache/disable bits */

	if ((heart_stat & (HEART_STAT_TRITON|HEART_STAT_R4K)) == 0)
		heart_mode |= HM_IO_COH_EN | HM_SYSSTATE_ERE;
	else
		heart_mode |= HM_BAD_SYSRD_ERE;/* R4K no speculation */
	heart->h_mode = heart_mode;

	/* enable widget type interrupts */
	heartcfg->h_wid_err_mask = ERRTYPE_ALL;

	imr_softpower = heart->h_imr[master_cpuid] & (1ull << IP30_HVEC_POWER);

	/* allow bus error interrupt on all enabled CPUs */
	enable_intr = 0x1ull << (HEART_INT_L4SHIFT + HEART_INT_CPUPBERRSHFT);
	for (i = 0; i < MAXCPU; i++) {
		if (heart_stat & HEART_STAT_PROC_ACTIVE(i))
			heart->h_imr[i] = enable_intr << i;
		else
			heart->h_imr[i] = 0;
	}

	/*
	 * if warm llp reset done prior to coming here
	 * clear the WID_ERR interrupt cause before we enable
	 * interrupts
	 */
	if (heart->h_isr & (HEART_INT_EXC << HEART_INT_L4SHIFT)) {
	    heart->h_cause |= HC_WIDGET_ERR;
	    flushbus();
	}

	heart->h_imr[master_cpuid] |= (HEART_INT_EXC << HEART_INT_L4SHIFT) | imr_softpower;

#if HEART1_TIMEOUT_WAR
	/* ~51.2 usecs */
	if (heart_rev() != HEART_REV_A)
#endif	/* !HEART1_TIMEOUT_WAR */
		heartcfg->h_widget.w_req_timeout = 40;

	/* initialize stuff "beyond" heart */

	init_xbow(xbow);
	init_xbow_widget(xbow, HEART_ID, &heartcfg->h_widget);
#ifdef IP30_RPROM
	init_xbow_widget(xbow, BRIDGE_ID,
			 (widget_cfg_t *)K1_MAIN_WIDGET(BRIDGE_ID));
#else
	imr_widevec = (HEARTCONST 1 << IP30_HVEC_WIDERR_XBOW);
	for (port = BRIDGE_ID; port > HEART_ID; port--) {
		if (xtalk_probe(xbow, port)) {
			widget = (widget_cfg_t *)K1_MAIN_WIDGET(port);
#ifdef XTALK_PORT_DISABLE
			if (XTALK_PORT_DISABLE(port)) {
			    printf("ignoring XTalk board in port %d"
				   " (ID register is 0x%X)\n",
				   port, widget->w_id);
			    continue;
			}
#endif
			init_xbow_widget(xbow, port, widget);
			imr_widevec |= WIDGET_ERRVEC(port);
			 if (countfan && port != BRIDGE_ID)
				MPCONF->fanloads++;
		}
	}
#endif /* !IP30_RPROM */

	/* clear all pending interrupts resulted from all the probing */
	heartcfg->h_wid_err_cmd_word = 0x0;
	heartcfg->h_wid_err_type = ERRTYPE_ALL;
	heart->h_clr_isr = WIDGET_ERRVEC_MASK;
	wbflush();

#if IP30_RPROM || defined(XTALK_PORT_DISABLE)
	/* only enable widget exception interrupts for xbow and BaseIO */
	heart->h_imr[master_cpuid] |= (HEARTCONST 1 << IP30_HVEC_WIDERR_XBOW) |
				      (HEARTCONST 1 << IP30_HVEC_WIDERR_BASEIO);
#else
	/* enable widget exception interrupts */
	heart->h_imr[master_cpuid] |= imr_widevec;
#endif

#ifdef HEART_COHERENCY_WAR
	heart_read_coherency_war = (heart_rev() <= HEART_REV_B) &&
		!fibbed_heart;
	heart_write_coherency_war = (heart_rev() <= HEART_REV_B);
#endif
}

static COMPONENT hearttmpl = {
	AdapterClass,			/* Class */
	XTalkAdapter,			/* Type  */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0,				/* Affinity */
	0,				/* ConfigurationDataSize */
	5,				/* IdentifierLength */
	"heart"				/* Identifier */
};

static void
heart_do_port(COMPONENT *parent, int portno, int isxbow)
{
	widget_cfg_t   *widget;
	slotinfo_t	slot;
	widgetreg_t	id;
	xbow_t		*xbow = XBOW_K1PTR;

	/*
	 * check the xbow for presence of a widget at portno
	 */
	if (isxbow && !xtalk_probe(xbow, portno))
		return;

	/* bridge is always there on direct connect mode */

	widget = (widget_cfg_t *)K1_MAIN_WIDGET(portno);

	id = widget->w_id;
	slot.id.bus = BUS_XTALK;
	slot.id.mfgr = (id & WIDGET_MFG_NUM) >> WIDGET_MFG_NUM_SHFT;
	slot.id.part = (id & WIDGET_PART_NUM) >> WIDGET_PART_NUM_SHFT;
	slot.id.rev = (id & WIDGET_REV_NUM) >> WIDGET_REV_NUM_SHFT;
	slot.cfg_base = 0;
	slot.mem_base = (void *)widget;
	slot.bus_base = (void *)HEART_PIU_K1PTR;
	slot.bus_slot = portno;

	slot_register(parent,&slot);
}

int
heart_install(COMPONENT *parent)
{
	int		port;

	parent = AddChild(parent, &hearttmpl, (void *)0);

#ifndef IP30_RPROM

	/* Probe the slots in reverse order, so widget 0xF (baseio)
	 * is found first.
	 */
	for (port = BRIDGE_ID; port > HEART_ID; port--) {
#ifdef	XTALK_PORT_DISABLE
		if (XTALK_PORT_DISABLE(port)) {
			continue;
		}
#endif
		heart_do_port(parent, port, 1);
	}
#else	/* IP30_RPROM */
	/* If RPROM only do BaseIO */
	heart_do_port(parent, BRIDGE_ID, 1);
#endif	/* !IP30_RPROM */
	return 1;
}

#ifndef IP30_RPROM
/* defined and set up in lib/libsk/ml/bridge.c */
extern struct reg_desc widget_err_cmd_word_desc[];

struct reg_values hm_trigger[] = {
	{ HM_TRIG_HEART_EXC,	"HEART_EXC" },
	{ HM_TRIG_REG_BIT,	"REG_BIT" },
	{ HM_TRIG_SYSCLK,	"SYSCLK" },
	{ HM_TRIG_MEMCLK_2X,	"MEMCLK_2X" },
	{ HM_TRIG_MEMCLK,	"MEMCLK" },
	{ HM_TRIG_IOCLK,	"IOCLK" },
	{ 0x6,			"Unused6" },
	{ 0x7,			"Unused7" },
	{ 0,			NULL },
};

#define LO_M(x)	((x)>>32)
#define LO_S(x)	((x)+32)
struct reg_desc heart_mode_hi_desc[] = {
    /* mask			shift	name		format	values */
    {LO_M(HM_PROC_DISABLE(3)),	0,	"DIS3",		NULL,	NULL},
    {LO_M(HM_PROC_DISABLE(2)),	0,	"DIS2",		NULL,	NULL},
    {LO_M(HM_PROC_DISABLE(1)),	0,	"DIS1",		NULL,	NULL},
    {LO_M(HM_PROC_DISABLE(0)),	0,	"DIS0",		NULL,	NULL},
    {LO_M(HM_MAX_PSR),	LO_S(-57),	"MAX_PSR",	"%#x",	NULL},
    {LO_M(HM_MAX_IOSR),	LO_S(-54),	"MAX_IOSR",	"%#x",	NULL},
    {LO_M(HM_MAX_PEND_IOSR), LO_S(-51),	"MAX_PEND_IOSR","%#x",NULL},
    {LO_M(HM_TRIG_SRC_SEL_MSK),	0,	"TRIG_SRC_SEL",	NULL,	hm_trigger},
    {LO_M(HM_PIU_TEST_MODE), LO_S(-40),	"PIU_TEST_MODE","%#x",NULL},
    {LO_M(HM_GP_FLAG_MSK),   LO_S(-36),	"GP_FLAG_MSK", "%#x",	NULL},
    {LO_M(HM_MAX_PROC_HYST), LO_S(-32),	"MAX_PROC_HYST","%#x",NULL},
    {0,				0,	NULL,		NULL,	NULL}
};

struct reg_desc heart_mode_lo_desc[] = {
	{HM_LLP_WRST_AFTER_RST,	0,	"LLP_WRST_AFTER_RST",NULL,NULL},
	{HM_LLP_LINK_RST,	0,	"LLP_LINK_RS",	NULL,	NULL},
	{HM_LLP_WARM_RST,	0,	"LLP_WARM_RS",	NULL,	NULL},
	{HM_COR_ECC_LCK,	0,	"COR_ECC_LCK",	NULL,	NULL},
	{HM_REDUCED_PWR,	0,	"REDUCED_PWR",	NULL,	NULL},
	{HM_COLD_RST,		0,	"COLD_RST",	NULL,	NULL},
	{HM_SW_RST,		0,	"SW_RST",	NULL,	NULL},
	{HM_MEM_FORCE_WR,	0,	"MEM_FORCE_WR",	NULL,	NULL},
	{HM_DB_ERR_GEN,		0,	"DB_ERR_GEN",	NULL,	NULL},
	{HM_CACHED_PIO_EN,	0,	"CACHED_PIO_EN",NULL,	NULL},
	{HM_PE_SYS_COR_ERE,	0,	"PE_SYS_COR_ERE",NULL,	NULL},
	{HM_GLOBAL_ECC_EN,	0,	"GLOBAL_ECC_EN",NULL,	NULL},
	{HM_IO_COH_EN,		0,	"IO_COH_EN",	NULL,	NULL},
	{HM_INT_EN,		0,	"INT_EN",	NULL,	NULL},
	{HM_DATA_CHK_EN,	0,	"DATA_CHK_EN",	NULL,	NULL},
	{HM_REF_EN,		0,	"REF_EN",	NULL,	NULL},
	{HM_BAD_SYSWR_ERE,	0,	"BAD_SYSWR_ERE",NULL,	NULL},
	{HM_BAD_SYSRD_ERE,	0,	"BAD_SYSRD_ERE",NULL,	NULL},
	{HM_SYSSTATE_ERE,	0,	"SYSSTATE_ERE",	NULL,	NULL},
	{HM_SYSCMD_ERE,		0,	"SYSCMD_ERE",	NULL,	NULL},
	{HM_NCOR_SYS_ERE,	0,	"NCOR_SYS_ERE",	NULL,	NULL},
	{HM_COR_SYS_ERE,	0,	"COR_SYS_ERE",	NULL,	NULL},
	{HM_DATA_ELMNT_ERE,	0,	"DATA_ELMNT_ERE",NULL,	NULL},
	{HM_MEM_ADDR_PROC_ERE,	0,	"MEM_ADDR_PROC_ERE",NULL,NULL},
	{HM_MEM_ADDR_IO_ERE,	0,	"MEM_ADDR_IO_ERE",NULL,NULL},
	{HM_NCOR_MEM_ERE,	0,	"NCOR_MEM_ERE",	NULL,NULL},
	{HM_COR_MEM_ERE,	0,	"COR_MEM_ERE",	NULL,NULL},
	{0,			0,	NULL,		NULL,	NULL}
};

struct reg_desc heart_err_type_desc[] = {
	{0x1L << 29,	0,	"XBAR_CREDIT_OVER",	NULL,	NULL},
	{0x1L << 28,	0,	"XBAR_CREDIT_UNDER",	NULL,	NULL},
	{0x1L << 27,	0,	"IO_NONHEAD",		NULL,	NULL},
	{0x1L << 26,	0,	"IO_BAD_FORMAT",	NULL,	NULL},
	{0x1L << 25,	0,	"IO_UNEXPECTED_RESP_ERR",NULL,	NULL},
	{0x1L << 24,	0,	"IORWRB_OFLOW_ERR",	NULL,	NULL},
	{0x1L << 23,	0,	"IOR_CMD_ERR",		NULL,	NULL},
	{0x1L << 22,	0,	"IOR_CMD_WARN",		NULL,	NULL},
	{0x1L << 21,	0,	"IOR_INT_VEC_ERR",	NULL,	NULL},
	{0x1L << 20,	0,	"IOR_INT_VEC_WARN",	NULL,	NULL},
	{0x1L << 18,	0,	"LLP_RCV_WARM_RST",	NULL,	NULL},
	{0x1L << 17,	0,	"LLP_RCV_LNK_RST",	NULL,	NULL},
	{0x1L << 16,	0,	"LLP_RCV_SN_ERR",	NULL,	NULL},
	{0x1L << 15,	0,	"LLP_RCV_CB_ERR",	NULL,	NULL},
	{0x1L << 14,	0,	"LLP_RCV_SQUASH_DATA",	NULL,	NULL},
	{0x1L << 13,	0,	"LLP_TX_RETRY_TIMEOUT",	NULL,	NULL},
	{0x1L << 12,	0,	"LLP_TX_RETRY",		NULL,	NULL},
	{0x1L << 3,	0,	"PIO_RD_TIMEOUT",	NULL,	NULL},
	{0x1L << 2,	0,	"PIO_WR_TIMEOUT",	NULL,	NULL},
	{0x1L << 1,	0,	"PIO_XTLK_ACC_ERR",	NULL,	NULL},
	{0x1L << 0,	0,	"PIO_WCR_ACC_ERR",	NULL,	NULL},
	{0,		0,	NULL,			NULL,	NULL}
};

struct reg_desc heart_pio_err_addr_desc[] = {
	{0x3L << 54,	-54,	"CPU",			"%d",	NULL},
	{0x3L << 52,	-52,	"UNCACHED_ATTRIB",	"%#x",	NULL},
	{0x7ffL << 40,	-40,	"SYSCMD",		"%#x",	NULL},
	{0xffffffffffL,	0,	"ADDR",			"%#x",	NULL},
	{0,		0,	NULL,			NULL,	NULL}
};

struct reg_desc heart_pio_rdto_addr_desc[] = {
	{0x3L << 18,	-18,	"CPU",			"%d",	NULL},
	{0x3L << 16,	-16,	"IO_SPACE",		"%#x",	NULL},
	{0xfL << 12,	-12,	"DIDN",			"%#x",	NULL},
	{0xfffL,	0,	"ADDR",			"%#x",	NULL},
	{0,		0,	NULL,			NULL,	NULL}
};

struct reg_desc heart_pbus_err_misc_desc[] = {
	{0x1L << 23,	0,	"BAD_RD_WRBACK",	NULL,	NULL},
	{0x3L << 21,	-21,	"CPU",			"%d",	NULL},
	{0xffL << 13,	-13,	"SYNDROME",		"%#x",	NULL},
	{0x1L << 12,	0,	"SYSCMD_PAR",		NULL,	NULL},
	{0xfffL,	0,	"SYSCMD",		"%#x",	NULL},
	{0,		0,	NULL,			NULL,	NULL}
};

struct reg_desc heart_mem_err_desc[] = {
	{0x7L << 43,	-43,	"SRC",			"%#x",	NULL},
	{0x1L << 42,	0,	"TYPE",			NULL,	NULL},
	{0xffL << 34,	-34,	"SYNDROME",		"%#x",	NULL},
	{0x3ffffffff,	0,	"ADDR",			"%#x",	NULL},
	{0,		0,	NULL,			NULL,	NULL}
};

struct reg_desc heart_piur_acc_err_desc[] = {
	{0x1L << 22,	0,	"TYPE",		NULL,	NULL},
	{0x3L << 20,	-20,	"CPU",		"%d",	NULL},
	{0xfffffL,	0,	"ADDR",		"%#x",	NULL},
	{0,		0,	NULL,		NULL,	NULL}
};

struct reg_desc heart_isr_desc[] = {
	{0x1L << 63,		0,	"HEART_EXC",		NULL,	NULL},

	{0x1L << 62,		-62,	"BERR3",		NULL,	NULL},
	{0x1L << 61,		-61,	"BERR2",		NULL,	NULL},
	{0x1L << 60,		-60,	"BERR1",		NULL,	NULL},
	{0x1L << 59,		-59,	"BERR0",		NULL,	NULL},

	{0x1L << 58,		-58,	"XBOW",			NULL,	NULL},
	{0x1L << 57,		-57,	"BASEIO",		NULL,	NULL},
	{0x1L << 56,		-56,	"WIDE",			NULL,	NULL},
	{0x1L << 55,		-55,	"WIDD",			NULL,	NULL},
	{0x1L << 54,		-54,	"WIDC",			NULL,	NULL},
	{0x1L << 53,		-53,	"WIDB",			NULL,	NULL},
	{0x1L << 52,		-52,	"WIDA",			NULL,	NULL},
	{0x1L << 51,		-51,	"WID9",			NULL,	NULL},

	{0x1L << 50,		0,	"TIMER",		NULL,	NULL},

	{0x1L << 49,		-49,	"IPI3",			NULL,	NULL},
	{0x1L << 48,		-48,	"IPI2",			NULL,	NULL},
	{0x1L << 47,		-47,	"IPI1",			NULL,	NULL},
	{0x1L << 46,		-46,	"IPI0",			NULL,	NULL},

	{0x1L << 45,		-45,	"DBG3",			NULL,	NULL},
	{0x1L << 44,		-44,	"DBG2",			NULL,	NULL},
	{0x1L << 43,		-43,	"DBG1",			NULL,	NULL},
	{0x1L << 42,		-42,	"DBG0",			NULL,	NULL},

	{0x1L << 41,		-41,	"POWER",		NULL,	NULL},
	{0x1L << 40,		-40,	"ACFAIL",		NULL,	NULL},

	{0x1L << 39,		-39,	"vec39",		NULL,	NULL},
	{0x1L << 38,		-38,	"vec38",		NULL,	NULL},
	{0x1L << 37,		-37,	"vec37",		NULL,	NULL},
	{0x1L << 36,		-36,	"vec36",		NULL,	NULL},
	{0x1L << 35,		-35,	"vec35",		NULL,	NULL},
	{0x1L << 34,		-34,	"vec34",		NULL,	NULL},
	{0x1L << 33,		-33,	"vec33",		NULL,	NULL},
	{0x1L << 32,		-32,	"vec32",		NULL,	NULL},

	{0x1L << 31,		-31,	"vec31",		NULL,	NULL},
	{0x1L << 30,		-30,	"vec30",		NULL,	NULL},
	{0x1L << 29,		-29,	"vec29",		NULL,	NULL},
	{0x1L << 28,		-28,	"vec28",		NULL,	NULL},
	{0x1L << 27,		-27,	"vec27",		NULL,	NULL},
	{0x1L << 26,		-26,	"vec26",		NULL,	NULL},
	{0x1L << 25,		-25,	"vec25",		NULL,	NULL},
	{0x1L << 24,		-24,	"vec24",		NULL,	NULL},
	{0x1L << 23,		-23,	"vec23",		NULL,	NULL},
	{0x1L << 22,		-22,	"vec22",		NULL,	NULL},
	{0x1L << 21,		-21,	"vec21",		NULL,	NULL},
	{0x1L << 20,		-20,	"vec20",		NULL,	NULL},
	{0x1L << 19,		-19,	"vec19",		NULL,	NULL},
	{0x1L << 18,		-18,	"vec18",		NULL,	NULL},
	{0x1L << 17,		-17,	"vec17",		NULL,	NULL},
	{0x1L << 16,		-16,	"vec16",		NULL,	NULL},

	{0x1L << 15,		-15,	"ETHER",		NULL,	NULL},
	{0x1L << 14,		-14,	"SERIAL",		NULL,	NULL},

	{0x1L << 13,		-13,	"vec13",		NULL,	NULL},
	{0x1L << 12,		-12,	"vec12",		NULL,	NULL},
	{0x1L << 11,		-11,	"vec11",		NULL,	NULL},
	{0x1L << 10,		-10,	"vec10",		NULL,	NULL},
	{0x1L << 9,		-9,	"vec9",			NULL,	NULL},
	{0x1L << 8,		-8,	"vec8",			NULL,	NULL},
	{0x1L << 7,		-7,	"vec7",			NULL,	NULL},
	{0x1L << 6,		-6,	"vec6",			NULL,	NULL},
	{0x1L << 5,		-5,	"vec5",			NULL,	NULL},
	{0x1L << 4,		-4,	"vec4",			NULL,	NULL},
	{0x1L << 3,		-3,	"vec3",			NULL,	NULL},

	{0x1L << 2,		-2,	"FC1",			NULL,	NULL},
	{0x1L << 1,		-1,	"FC0",			NULL,	NULL},
	{0x1L << 0,		0,	"IRQ",			NULL,	NULL},

	{0,			0,	NULL,			NULL,	NULL}
};

struct reg_desc heart_cause_desc[] = {
	{0x1L << 63,	0,	"PE_SYS_COR_ERR_3",	NULL,	NULL},
	{0x1L << 62,	0,	"PE_SYS_COR_ERR_2",	NULL,	NULL},
	{0x1L << 61,	0,	"PE_SYS_COR_ERR_1",	NULL,	NULL},
	{0x1L << 60,	0,	"PE_SYS_COR_ERR_0",	NULL,	NULL},
	{0x1L << 44,	0,	"PIOWDB_OFLOW",		NULL,	NULL},
	{0x1L << 43,	0,	"PIORWRB_OFLOW",	NULL,	NULL},
	{0x1L << 42,	0,	"PIU_REG_ACC_ERR",	NULL,	NULL},
	{0x1L << 41,	0,	"SYSAD_WR_ERR",		NULL,	NULL},
	{0x1L << 40,	0,	"SYSAD_RD_ERR",		NULL,	NULL},
	{0x1L << 39,	0,	"SYSSTATE_ERR_3",	NULL,	NULL},
	{0x1L << 38,	0,	"SYSSTATE_ERR_2",	NULL,	NULL},
	{0x1L << 37,	0,	"SYSSTATE_ERR_1",	NULL,	NULL},
	{0x1L << 36,	0,	"SYSSTATE_ERR_0",	NULL,	NULL},
	{0x1L << 35,	0,	"SYSCMD_ERR_3",		NULL,	NULL},
	{0x1L << 34,	0,	"SYSCMD_ERR_2",		NULL,	NULL},
	{0x1L << 33,	0,	"SYSCMD_ERR_1",		NULL,	NULL},
	{0x1L << 32,	0,	"SYSCMD_ERR_0",		NULL,	NULL},
	{0x1L << 31,	0,	"NCOR_SYSAD_ERR_3",	NULL,	NULL},
	{0x1L << 30,	0,	"NCOR_SYSAD_ERR_2",	NULL,	NULL},
	{0x1L << 29,	0,	"NCOR_SYSAD_ERR_1",	NULL,	NULL},
	{0x1L << 28,	0,	"NCOR_SYSAD_ERR_0",	NULL,	NULL},
	{0x1L << 27,	0,	"COR_SYSAD_ERR_3",	NULL,	NULL},
	{0x1L << 26,	0,	"COR_SYSAD_ERR_2",	NULL,	NULL},
	{0x1L << 25,	0,	"COR_SYSAD_ERR_1",	NULL,	NULL},
	{0x1L << 24,	0,	"COR_SYSAD_ERR_0",	NULL,	NULL},
	{0x1L << 23,	0,	"DATA_ELMNT_ERR_3",	NULL,	NULL},
	{0x1L << 22,	0,	"DATA_ELMNT_ERR_2",	NULL,	NULL},
	{0x1L << 21,	0,	"DATA_ELMNT_ERR_1",	NULL,	NULL},
	{0x1L << 20,	0,	"DATA_ELMNT_ERR_0",	NULL,	NULL},
	{0x1L << 16,	0,	"WIDGET_ERR",		NULL,	NULL},
	{0x1L << 7,	0,	"MEM_ADDR_ERR_3",	NULL,	NULL},
	{0x1L << 6,	0,	"MEM_ADDR_ERR_2",	NULL,	NULL},
	{0x1L << 5,	0,	"MEM_ADDR_ERR_1",	NULL,	NULL},
	{0x1L << 4,	0,	"MEM_ADDR_ERR_0",	NULL,	NULL},
	{0x1L << 2,	0,	"MEM_ADDR_ERR_IO",	NULL,	NULL},
	{0x1L << 1,	0,	"NCOR_MEM_ERR",		NULL,	NULL},
	{0x1L << 0,	0,	"COR_MEM_ERR",		NULL,	NULL},
	{0,		0,	NULL,			NULL,	NULL}
};

static void
heart_print_isr(char *nfmt, int index, heartreg_t isr)
{
	char		name[32];
	heartreg_t	misr;

	sprintf(name, nfmt, index);
	printf("HEART %s:\t%LR\n", name, isr, heart_isr_desc);
}

heartreg_t
print_heart_status(int clear)
{
	int		cpu = cpuid();
	heart_cfg_t    *heart_cfg_ptr = HEART_CFG_K1PTR;
	heartreg_t	heart_wid_err_type;
	heartreg_t	heart_wid_err_cword;
	heartreg_t	heart_wid_pio_err_addr;
	heartreg_t	heart_pio_rdto_addr;
	__uint64_t	heart_wid_err_addr;
	heart_piu_t    *heart = HEART_PIU_K1PTR;
	heartreg_t	heart_isr;
	heartreg_t	heart_imsr;
	heartreg_t	heart_cause;
	heartreg_t	heart_piuae;
	heartreg_t	heart_beaddr;
	heartreg_t	heart_bemisc;
	heartreg_t	heart_meaddr;
	heartreg_t	heart_medata;

	heart_isr = heart->h_isr;
	if (!heart_isr)
		return 0;	/* no interrupts pending, quit */

	/* snapshot the heart registers before starting
	 */
	heart_imsr = heart->h_imsr;
	heart_cause = heart->h_cause;
	heart_piuae = heart->h_piur_acc_err;
	heart_beaddr = heart->h_berr_addr;
	heart_bemisc = heart->h_berr_misc;
	heart_meaddr = heart->h_memerr_addr;
	heart_medata = heart->h_memerr_data;
	heart_wid_err_type = heart_cfg_ptr->h_wid_err_type;
	heart_wid_err_cword = heart_cfg_ptr->h_widget.w_err_cmd_word;
	heart_wid_pio_err_addr = heart_cfg_ptr->h_wid_pio_err_lower | 
		((__uint64_t)heart_cfg_ptr->h_wid_pio_err_upper << 32);
	heart_wid_err_addr = heart_cfg_ptr->h_widget.w_err_lower_addr |
		((__uint64_t)heart_cfg_ptr->h_widget.w_err_upper_addr << 32);
	heart_pio_rdto_addr = heart_cfg_ptr->h_wid_pio_rdto_addr;

	heart_print_isr("ISR ", 0, heart_isr);
	heart_print_isr("IMSR", 0, heart_imsr);

	if (heart_cause != 0) {

		printf("     Cause: %LR\n", heart_cause, heart_cause_desc);

		if (heart_cause & HC_PIUR_ACC_ERR) {
			printf("	PIUR access error: %LR\n",
				heart_piuae,
				heart_piur_acc_err_desc);
		}

#define	PBUS_ERR_GRP	(HC_BAD_SYSWR_ERR |		\
			 HC_BAD_SYSRD_ERR |		\
			 HC_SYSSTATE_ERR_MSK |		\
			 HC_SYSCMD_ERR_MSK |		\
			 HC_NCOR_SYSAD_ERR_MSK |	\
			 HC_COR_SYSAD_ERR_MSK)

		if (heart_cause & PBUS_ERR_GRP) {
			printf("	PBUS error address: %#x\n",
				heart_beaddr);
			printf("	PBUS error misc: %LR\n",
				heart_bemisc,
				heart_pbus_err_misc_desc);
		}

#define	MBUS_ERR_GRP	(HC_MEM_ADDR_ERR_PROC_MSK |	\
			 HC_MEM_ADDR_ERR_IO |		\
			 HC_NCOR_MEM_ERR |		\
			 HC_COR_MEM_ERR)

		if (heart_cause & MBUS_ERR_GRP) {
			printf("	Memory error: %LR\n",
				heart_meaddr, heart_mem_err_desc);

			if (heart_cause &
			    (HC_NCOR_MEM_ERR | HC_COR_MEM_ERR)) {
				extern char *cpu_memerr_to_dimm(heartreg_t);

				printf("	Bad memory data: %#x, (%s)\n",
					heart_medata,
					cpu_memerr_to_dimm(heart_meaddr));
			}
		}

		if (heart_cause & HC_WIDGET_ERR) {

			printf("        Widget Error type: %LR\n",
				heart_wid_err_type, heart_err_type_desc);

#define	ERRTYPE_WIDGET_PIO_ERR	(ERRTYPE_PIO_WR_TIMEOUT |	\
				 ERRTYPE_PIO_XTLK_ACC_ERR |	\
				 ERRTYPE_PIO_WCR_ACC_ERR)
				 
			if (heart_wid_err_type & ERRTYPE_WIDGET_PIO_ERR) {
				printf("        PIO error address: %LR\n",
				       heart_wid_pio_err_addr,
				       heart_pio_err_addr_desc);
			}
			else if (heart_wid_err_type & ERRTYPE_PIO_RD_TIMEOUT) {
				printf("        PIO rd timeout address: %LR\n",
				       heart_pio_rdto_addr,
				       heart_pio_rdto_addr_desc);
			}
			else {
				printf("        Error command word: %R\n",
					heart_wid_err_cword,
					widget_err_cmd_word_desc);

				printf("        Error address: %#x\n",
				       heart_wid_err_addr);
			}

			if (clear) {
				heart_cfg_ptr->h_widget.w_err_cmd_word = 0x0;
				heart_cfg_ptr->h_wid_err_type = heart_wid_err_type;
			}
		}

		if (clear) {
			heart->h_cause = heart_cause;
			heart->h_clr_isr = heart_isr;
			flushbus();
		}
	}

	return heart_isr;
}

void
dump_heart_regs(void)
{
	heart_cfg_t    *heart_cfg_ptr = HEART_CFG_K1PTR;
	heart_piu_t    *heart = HEART_PIU_K1PTR;
	heartreg_t	pio_err, mode, isr;
	heartreg_t	heart_isr  = heart->h_isr;
	heartreg_t	heart_imsr = heart->h_imsr;
	heartreg_t	heart_imr[4];
	int		cpu;
	__uint64_t	addr;

	for (cpu=0; cpu<MAXCPU; ++cpu)
		heart_imr[cpu] = heart->h_imr[cpu];

	heart_print_isr("ISR ", 0, heart_isr);
	heart_print_isr("IMSR", 0, heart_imsr);
	for (cpu=0; cpu<MAXCPU; ++cpu)
	    heart_print_isr("IMR%d", cpu, heart_imr[cpu]);

	printf("	Cause: %LR\n", heart->h_cause, heart_cause_desc);
	printf("	PIUR access error: %LR\n",
		heart->h_piur_acc_err,
		heart_piur_acc_err_desc);

	printf("	PBUS error address: %#x\n",
		heart->h_berr_addr);
	printf("	PBUS error misc: %LR\n",
		heart->h_berr_misc,
		heart_pbus_err_misc_desc);

	printf("	Memory error: %LR\n",
		heart->h_memerr_addr, heart_mem_err_desc);
	printf("	Bad memory data: %#x\n",
		heart->h_memerr_data);


	printf("	WID Error type: %LR\n",
		heart_cfg_ptr->h_wid_err_type, heart_err_type_desc);


	pio_err = heart_cfg_ptr->h_wid_pio_err_lower | 
		(heart_cfg_ptr->h_wid_pio_err_upper << 32);
	printf("	PIO error address: %LR\n",
		pio_err, heart_pio_err_addr_desc);


	printf("	WID Error command word: %R\n",
		heart_cfg_ptr->h_widget.w_err_cmd_word,
		widget_err_cmd_word_desc);

	addr = (__uint64_t)heart_cfg_ptr->h_widget.w_err_upper_addr << 32;
	addr |= heart_cfg_ptr->h_widget.w_err_lower_addr;
	printf("	WID Error address: %#x\n", addr);

	mode = heart->h_mode;
	printf(" Mode       : 0x%x\n", mode);
	printf("  bits 63-32: %LR\n", LO_M(mode), heart_mode_hi_desc);
	printf("  bits 31-00: %LR\n", mode&0xFFFFFFFF, heart_mode_lo_desc);

	printf("Status: 0x%x\n",heart->h_status);
}
#endif /* !IP30_RPROM */

void
heart_clearnofault(void)
{
	heart_cfg_t    *heart_cfg_ptr = HEART_CFG_K1PTR;
	heart_piu_t    *heart = HEART_PIU_K1PTR;
	heartreg_t	heart_isr;
	heartreg_t	heart_cause;
	heartreg_t	heart_wid_err_type;

	heart_cause = heart->h_cause;
	if (heart_cause & HC_WIDGET_ERR) {
		heart_wid_err_type = heart_cfg_ptr->h_wid_err_type;
		heart_cfg_ptr->h_widget.w_err_cmd_word = 0x0;
		heart_cfg_ptr->h_wid_err_type = heart_wid_err_type;
	}
	if (heart_isr = heart->h_isr) {
		heart->h_cause = heart->h_cause;
		heart->h_clr_isr = heart_isr;
		flushbus();
	}
}

int
heart_rev(void)
{
	heart_cfg_t *heart_cfg = HEART_CFG_K1PTR;

	return (heart_cfg->h_widget.w_id & HW_ID_REV_NUM_MSK) >>
		HW_ID_REV_NUM_SHFT;
}

#ifdef HEART_COHERENCY_WAR
void
heart_dcache_wb_inval(void *addr, int len)
{
	extern void __dcache_wb_inval(void *, int);

	if (heart_read_coherency_war)
		__dcache_wb_inval(addr, len);
}

void
heart_dcache_inval(void *addr, int len)
{
	extern void __dcache_inval(void *, int);

	if (heart_read_coherency_war)
		__dcache_inval(addr, len);
}

void
heart_write_dcache_wb_inval(void *addr, int len)
{
	extern void __dcache_wb_inval(void *, int);

	if (heart_write_coherency_war)
		__dcache_wb_inval(addr, len);
}

void
heart_write_dcache_inval(void *addr, int len)
{
	extern void __dcache_inval(void *, int);

	if (heart_write_coherency_war)
		__dcache_inval(addr, len);
}
#endif
