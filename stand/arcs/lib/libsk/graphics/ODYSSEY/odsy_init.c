/*
** Copyright 1997, Silicon Graphics, Inc.
** All Rights Reserved.
**
** This Is Unpublished Proprietary Source Code Of Silicon Graphics, Inc.;
** The Contents Of This File May Not Be Disclosed To Third Parties, Copied Or
** Duplicated In Any Form, In Whole Or In Part, Without The Prior Written
** Permission Of Silicon Graphics, Inc.
**
** Restricted Rights Legend:
** Use, Duplication Or Disclosure By The Government Is Subject To Restrictions
** As Set Forth In Subdivision (C)(1)(Ii) Of The Rights In Technical Data
** And Computer Software Clause At Dfars 252.227-7013, And/Or In Similar Or
** Successor Clauses In The Far, Dod Or Nasa Far Supplement. Unpublished -
** Rights Reserved Under The Copyright Laws Of The United States.
**
*/

#include <sys/kopt.h>

#if defined(_STANDALONE)

#include "odsy_internals.h"
#include "sys/odsyhw.h"

#include "libsc.h"
#include "libsk.h"

#if defined(DEBUG)
void pon_puts(char *);
void pon_puthex(__uint32_t);
#define DPUTS(msg) pon_puts(msg)
#define DPUTHEX(num) pon_puthex(num)
#else
#define DPUTS(msg)
#define DPUTHEX(num)
#endif /* DEBUG */

void init_xbow_credit(xbow_t *xbow, int port, int credits);
void reset_xbow_link(__psunsigned_t xbow_base, int link);
odsy_hw *odsy_xtalk_probe(int part, int mfg, int board);
#else	/* !_STANDALONE */


#define DPUTS(msg)
#define DPUTHEX(num)


#include <sys/invent.h>

/*XXX --- let this build on current roots like 6.5.4(m,f)*/
/*someone will have to add this to the IRIX ism -- chuck jerian*/
#ifndef INV_ODSY
#define INV_ODSY        21      /* Odyssey graphics */
/* States for graphics class ODSY */
#define INV_ODSY_ARCHS      0xff000000 /* architectures */
#define INV_ODSY_REVA_ARCH  0x01000000 /* Buzz Rev A */
#define INV_ODSY_REVB_ARCH  0x02000000 /* Buzz Rev B */
#define INV_ODSY_MEMCFG     0x00ff0000 /* memory configs */
#define INV_ODSY_MEMCFG_32  0x00010000 /* 32MB memory */
#define INV_ODSY_MEMCFG_64  0x00020000 /* 64MB memory */
#define INV_ODSY_MEMCFG_128 0x00030000 /* 128MB memory */
#define INV_ODSY_MEMCFG_256 0x00040000 /* 256MB memory */
#define INV_ODSY_MEMCFG_512 0x00050000 /* 512MB memory */



#endif
#include "odsy_internals.h"

#endif	/* _STANDALONE */

static void odsyPushSysConfig(odsy_data *, int);

struct reg_desc flag_desc[] = {
	{ ODSY_FLAG_REG_GEN_LOCK_INTR, 0, "GEN_LOCK_INTR" },
	{ ODSY_FLAG_REG_CLOCK_STOPPED_INTR, 0, "CLOCK_STOPPED_INTR" },
	{ ODSY_FLAG_REG_READ_DURING_DIAG_ERROR, 0, "READ_DURING_DIAG_ERR" },
	{ ODSY_FLAG_REG_PUSH_CMD_ERROR, 0, "PUSH_CMD_ERROR" },
	{ ODSY_FLAG_REG_GFX_OFLOW_INTR, 0, "GFX_OFLOW_INTR" },
	{ ODSY_FLAG_REG_CFIFO_HW_FLAG, 0, "CFIFO_HW_FLAG" },
	{ ODSY_FLAG_REG_DBE_OFLOW_INTR, 0, "DBE_OFLOW_INTR" },
	{ ODSY_FLAG_REG_XFORM_DFIFO_OFLOW_INTR, 0, "XFORM_DFIFO_OFLOW_INTR" },
	{ ODSY_FLAG_REG_XFORM_IFIFO_OFLOW_INTR, 0, "XFORM_IFIFO_INTR" },
	{ ODSY_FLAG_REG_XFORM_UCODE_OFLOW_INTR, 0, "XFORM_UCODE_OFLOW_INTR" },
	{ ODSY_FLAG_REG_XFORM_PTABLE_OFLOW_INTR, 0, "XFORM_PTABLE_OFLOW_INTR" },
	{ ODSY_FLAG_REG_GFX_XTALK_STALL, 0, "GFX_XTALK_STALL" },
	{ ODSY_FLAG_REG_HOST_FLAG_INTR, 0, "HOST_FLAG_INTR" },
	{ ODSY_FLAG_REG_PRIV_VIOLATE_GFX, 0, "PRIV_VIOLATE_GFX" },
	{ ODSY_FLAG_REG_CTXSW_DONE, 0, "CTXSW_DONE" },
	{ ODSY_FLAG_REG_DMA_DONE_FLAG, 0, "DMA_DONE_FLAG" },
	{ ODSY_FLAG_REG_DMA_CMD_OFLO_INTR, 0, "DMA_CMD_OFLO_INTR" },
	{ ODSY_FLAG_REG_CFIFO_LW_FLAG, 0, "CFIFO_LW_FLAG" },
	{ ODSY_FLAG_REG_ILLEGAL_OUT_FORMAT, 0, "ILLEGAL_OUT_FORMAT" },
	{ ODSY_FLAG_REG_MULTGET_INTR, 0, "MULTGET_INTR" },
	{ ODSY_FLAG_REG_CORRECTABLE_ECC_ERROR, 0, "CORRECTABLE_ECC_ERROR" },
	{ ODSY_FLAG_REG_MLT_CORRECTABLE_ECC_ERROR, 0, "MLT_CORRECTABLE_ECC_ERROR" },
	{ ODSY_FLAG_REG_UNCORRECTABLE_ECC_ERROR, 0, "UNCORRECTABLE_ECC_ERROR" },
	{ ODSY_FLAG_REG_SYNC_TIMEOUT_FLAG, 0, "SYNC_TIMEOUT_FLAG" },
	{ ODSY_FLAG_REG_HEURISTIC_OVERFLOW, 0, "HEURISTIC_OVERFLOW" },
	{ 0, 0, 0}
};
static struct reg_values src_dma[] = {
	{ 0, "unset" },
	{ 1, "unset" },
	{ 2, "SRC_DMA" },
	{ 3, "SRC_PIX_PIPE" },
	{0}
};
struct reg_desc status0_desc[] = {
	{ ODSY_STATUS0_CFIFO_ENABLED, 0, "CFIFO_ENABLED" },
	{ ODSY_STATUS0_SHIFT_DONE, 0, "SHIFT_DONE" },
	{ ODSY_STATUS0_ONE_SHIFT_DONE, 0, "ONE_SHIFT_DONE" },
	{ ODSY_STATUS0_SCAN_DONE, 0, "SCAN_DONE" },
	{ ODSY_STATUS0_CLK_DISABLED, 0, "CLK_DISABLED" },
	{ ODSY_STATUS0_XFORM_ACTV, 0, "XFORM_ACTV", },
	{ ODSY_STATUS0_CTXSW_ACTV, 0, "CTXSW_ACTV", },
	{ ODSY_STATUS0_DMA_DONE, 0, "DMA_DONE" },
	{ ODSY_STATUS0_COPY_DONE, 0, "COPY_DONE" },
	{ ODSY_STATUS0_CFIFO_LW, 0, "CFIFO_LW" },
	{ ODSY_STATUS0_CFIFO_MT, 0, "CFIFO_MT" },
	{ ODSY_STATUS0_CFIFO_HW, 0, "CFIFO_HW" },
	{ ODSY_STATUS0_XRFIFO_LW, 0, "XRFIFO_LW" },
	{ ODSY_STATUS0_XRFIFO_HW, 0, "XRFIFO_HW" },
	{ ODSY_STATUS0_SYNC_DST_RASTER|ODSY_STATUS0_RASTER_SYNC_SRC, -11,
		"RASTER_SYNC_SRC", NULL, src_dma },
	{ ODSY_STATUS0_SYNC_DST_CFIFO|ODSY_STATUS0_CFIFO_SYNC_SRC, -9,
		 "CFIFO_SYNC_SRC", NULL, src_dma },
	{ ODSY_STATUS0_SYNC_DST_DMA|ODSY_STATUS0_DMA_SYNC_SRC, -7,
		 "DMA_SYNC_SRC", NULL, src_dma },
	{ ODSY_STATUS0_GFX_REG_RD_ACTV, 0, "GFX_REG_RD_ACTV" },
	{ ODSY_STATUS0_SYNC_PUSH_ACTV, 0, "SYNC_PUSH_ACTV" },
	{ ODSY_STATUS0_PERF_STATS_PUSH_ACTV, 0, "PERF_STATS_PUSH_ACTV" },
	{ ODSY_STATUS0_DIAG_HIST_PUSH_ACTV, 0, "DIAG_HIST_PUSH_ACTV" },
	{ 0, 0, 0 }
};

/*
** NOTE: Build variables affecting the textport:
**
** ODSY_TEXTPORT_DISABLED - don't ever register the odyssey textport
** ODSY_TEXTPORT_ALWAYS_ON - register textport as non-console device when gfx is not console 
** ODSY_SIM_KERN - don't register textport as console device
*/

/*
 * The following items are for the set of ODSY_MAXBOARDS odsy boards.
 */
odsy_data OdsyBoards[ODSY_MAXBOARDS];
static int OdsyNrBoards;
int Odsy_is_multi_buzz;		/* If 1, there are 2 buzzes chained together */

/* default timing table - used by kernel textport */
odsy_brdmgr_timing_info *default_timing = &odsy_default_timing1;

odsy_brdmgr_timing_info *current_timing;


int odsyPipeNrFromBase(odsy_hw * hwaddr)
{
	int i;
	for (i=0;i<ODSY_MAXBOARDS;i++)
		if (OdsyBoards[i].hw == hwaddr) return i;
	return -1;
}

/*
 * Determine number of buzzes in the system, which one is master, etc.
 * This must be done after all the boards have been initted.
 */
static int odsy_checkBuzzConfig(void)
{
    int board;
    int slaves = 0;
    int masters = 0;
    int master_id, slave_id;
    pbj_HIF_status_reg pbj_HIF_status;

#ifdef ODSY_DEBUG_INIT
    printf("checkBuzzConfig: # ody boards = %d\n", OdsyNrBoards);
#endif

    for (board = 0; board < OdsyNrBoards; board++) {
#if defined(ODSY_SIM_KERN)
	pbj_HIF_status.w32 = 0;
	pbj_HIF_status.bf.opt_out_detect_n = 1;		/* no option board */
	pbj_HIF_status.bf.opt_in_detect_n = (board ? 0 : 1); /* board 0 is master */
#else
	ODSY_DFIFO_PSEMA(&OdsyBoards[board]);
	if (ODSY_POLL_DFIFO_LEVEL(OdsyBoards[board].hw, 0)) {
		ODSY_DFIFO_VSEMA(&OdsyBoards[board]);
		return -1;
	}
	ODSY_DFIFO_VSEMA(&OdsyBoards[board]);
	ODSY_READ_HW(OdsyBoards[board].hw, dbe_diag_rd.PBJ_HIF_status, pbj_HIF_status.w32);
#endif
#ifdef ODSY_DEBUG_INIT
	printf("bd %d:  PBJ_HIF_status = 0x%x, in_detect = %d\n",
		board, pbj_HIF_status.w32, pbj_HIF_status.bf.opt_in_detect_n);
#endif
	/* assume this is an option board first */
	if (!pbj_HIF_status.bf.opt_out_detect_n)
	    OdsyBoards[board].buzz_config.has_opt_board = 1;

	if (!pbj_HIF_status.bf.opt_in_detect_n) {
	    slaves++;
	    slave_id = board;
	} else {
	    masters++;
	    master_id = board;
	    OdsyBoards[board].buzz_config.is_master = 1;
	}
    }

    /* figure out whether this is a two buzz configuration [or more :-) ] */
    if (slaves) {
	/* illegal configurations */
	if ((slaves == OdsyNrBoards) || (masters > 1)) {
#ifdef ODSY_DEBUG_INIT
	    printf("checkBuzzConfig: Illegal config\n");
#endif
	    return -1;
	}

	Odsy_is_multi_buzz = 1;

	/* XXX - if >2 buzz system, how to figure out how they are chained? */
	for (board = 0; board < OdsyNrBoards; board++) {
	    /* save board# of ody master */
	    OdsyBoards[board].buzz_config.master_buzz_id = master_id;

	    OdsyBoards[board].buzz_config.other_buzz_id = 
			(board == master_id) ? slave_id : master_id;

	    /* If we're master, we're hooked up to a slave;
	     * we don't have an opt_board
	     */
	    if (OdsyBoards[board].buzz_config.is_master)
		OdsyBoards[board].buzz_config.has_opt_board = 0;
	}
    } else {
	Odsy_is_multi_buzz = 0;

	/* everyone is their own master */
	for (board = 0; board < OdsyNrBoards; board++) {
	    OdsyBoards[board].buzz_config.master_buzz_id = board;
	    OdsyBoards[board].buzz_config.other_buzz_id = -1;
	}
    }

#ifdef ODSY_DEBUG_INIT
    printf("checkBuzzConfig: multi-buzz = %s\n", Odsy_is_multi_buzz ? "yes" : "no");
    for (board = 0; board < OdsyNrBoards; board++) {
	printf("checkBuzzConfig: brd %d\n", board);
	printf("  master = %d, other = %d, is_master = %d, optbrd = %d\n",
		OdsyBoards[board].buzz_config.master_buzz_id,
		OdsyBoards[board].buzz_config.other_buzz_id,
		OdsyBoards[board].buzz_config.is_master,
		OdsyBoards[board].buzz_config.has_opt_board);
    }
#endif
    /* hardware configuration is OK */
    return 0;
}


/*
** odsy_earlyinit
**
** Called from gfx/kern/textport/tport.c waaay early on, before odsy_init.
** 
** As the boardmanager for gfx board 0 dies, cn_init will call this
** again (but only if we're gfx board 0).
*/
int odsy_earlyinit(void)
{
	static int firsttime = 1;
	static int in_earlyinit = 0;
	odsy_data *bdata;
	odsy_hw *hw;
	int brd;

	/* if we panic during earlyinit, do not attempt to re-init */
	if (in_earlyinit) {
		cmn_err(CE_WARN,"odsy_earlyinit re-entered.  "
				"Init code probably paniced!\n");
		return 0;
	}

	in_earlyinit = 1;

	OdsyNrBoards = 0;	/* start with no boards each time */

#ifdef GFX_IDBG
	if (firsttime)
		odsyInitIDBG();
#endif
#ifdef ODSY_DEBUG_INIT
	printf("odsy_earlyinit\n");
#endif
		
	/*
	 * Init the boards/data struct, and count & init all boards.
	 */
	for (brd = 0; brd < ODSY_MAXBOARDS; brd++) {
		OdsyBoards[brd].board_found = -1;
			
		/* odsyFindBoardAddr determines the odsy board nbr ordering */
		hw = odsyFindBoardAddr(brd);
		if (hw == NULL) {
			break;		// no more boards
		}

		OdsyNrBoards++;
		odsyInitBoard(brd);
	}

	/* Determine board configuration */
	if (OdsyNrBoards > 0) {
		/* Determine Buzz configuration */
		if (odsy_checkBuzzConfig() == -1) {
			OdsyNrBoards = 0;
			in_earlyinit = 0;
			return 0;
		}
	}

	/* Initialize the cfifo and frame buffer origin on each board */
	for (brd = 0; brd < OdsyNrBoards; brd++) {
		bdata = &OdsyBoards[brd];
		odsyMMinit(bdata->hw, &bdata->mm);
	}

	/*
	 * Initialize backend, now that # of buzzes is known.
	 * If multi-buzz, startup up display (i.e. init DBE) only on slave.
	 */
	for (brd = 0; brd < OdsyNrBoards; brd++) {

		bdata = &OdsyBoards[brd];

		odsyPushSysConfig(bdata, 1);	/* re-write sys.config */

		odsyInitCfifo(brd, 0);

		/* DBE/htp init used to be dependent on multibuzz */
		odsyInitDBE(bdata, default_timing, DoDownload);

		/* don't register textport under simulation */
#if !defined(ODSY_TEXTPORT_DISABLED) && !defined(ODSY_SIM_KERN) && !defined(_STANDALONE)
		htp_register_board(bdata->hw, &odsy_htp_fncs,
				   bdata->info.gfx_info.xpmax,
				   bdata->info.gfx_info.ypmax);
#endif
		bdata->board_initted = 1;
	}

	/*
	** HACK TO SET UP THE TEXTPORT AS NON-CONSOLE OUTPUT DEVICE
	*/

#if !defined(ODSY_TEXTPORT_DISABLED) && defined(ODSY_TEXTPORT_ALWAYS_ON)
	if (OdsyNrBoards > 0) {
		int tp_init(void);
		static char cons;

		if (is_specified(arg_console))
			cons = arg_console[0];
		else
			cons = 'g';

		/* If nvram console == '[gG], then tp_init() will be
		 * called soon.  But if that's not the case, then
		 * override console and gfx here, and call tp_init()
		 * explicitly. */

		if (cons != 'g' && cons != 'G') {

			char save_console[10], save_gfx[10];
			strcpy(save_console, arg_console);
			strcpy(arg_console, "g");
			
			strcpy(save_gfx, arg_gfx);
			strcpy(arg_gfx, "alive");

			cmn_err(CE_NOTE, "Initializing non-console textport\n"); 

			/* register the textport under simulation only if console != 'g' */
#if defined(ODSY_SIM_KERN)
			bdata = &OdsyBoards[0];
			htp_register_board(bdata->hw, &odsy_htp_fncs,
					   bdata->info.gfx_info.xpmax, bdata->info.gfx_info.ypmax);
#endif
			tp_init();

			strcpy(arg_console, save_console);
			strcpy(arg_gfx, save_gfx);
		}
	}
#endif /* !ODSY_TEXTPORT_DISABLED && ODSY_TEXTPORT_ALWAYS_ON */

	firsttime = in_earlyinit = 0;

	return OdsyNrBoards ? 1 : 0;
}

#if !defined(_STANDALONE)
   
/* 
** odsy_init
**
** called from main() at boot time.  we register
** to have odsy_attach (by default) called as the 
** XIO bus discovery handler for odsy boards.
**
** in simulation mode this means calling the discover
** call-back directly for the boards "installed".
*/
void odsy_init(void)
{
#ifdef ODSY_SIM_KERN
	// The odsy pseudo-driver needs to piggy back on either
	// a MGRAS board or a real ODYSSEY board.
	// If MGRAS is present, MGRAS is board 0, ODYSSEYSIM is board 1.
	// if ODYSSEY is present, ODYSSEYSIM is board 0.
	// if neither is present, then odsy_attach will not be called,
	// and there is no gfx board at all.

	// use literal constants since it is not useful to include mgrashw.h
	int part = 0xc003,	// HQ4_XT_ID_PART_NUM_VALUE
	    mfg = 0x2aa,	// HQ4_XT_ID_MFG_NUM_VALUE
	    boardnum = 0;	// assume only one pseudo-odsy board

	if (xtalk_early_piotrans_addr(part, mfg, boardnum, 0, sizeof(odsy_hw), 0)) {
		xwidget_driver_register(part, mfg, "odsy_", 0);
		return;
	}
#endif

	xwidget_driver_register(ODSY_XT_ID_PART_NUM_VALUE,
				ODSY_XT_ID_MFG_NUM_VALUE,
				"odsy_",0);

}


/*
** odsy_attach
**
** called as the xio bus-walker finds each odsy board.
**
** in simulation mode 'conn' is the odsy board number.
*/
int odsy_attach(vertex_hdl_t conn)
{
	vertex_hdl_t vhdl;

	odsy_hw *base;
	int brd;
	
	GFXLOGT(ODSY_odsy_attach);

	ASSERT(conn);

	hwgraph_char_device_add(conn, "odsy", "odsy_", &vhdl);
	base = (odsy_hw *)xtalk_piotrans_addr(conn,	/* device */
					      0,	/* override dev_info */
					      0,	/* offset */
					      sizeof(odsy_hw),	/* size */
					      0);	/* flag */
	ASSERT(base);

#if defined(ODSY_SIM_KERN)
	{
	    static pseudo_boards_inited_already;

	    if (pseudo_boards_inited_already == 0) {
		for (brd = 0; brd < ODSY_SIM_NR_PSEUDO_BOARDS; brd++) {
			odsyInitBoardGfx(brd);
			odsyInitIntr(brd, conn, vhdl);
		}
		pseudo_boards_inited_already = 1;
	    }
	}
#else
	if ((brd = odsyPipeNrFromBase(base))== -1) {
		for(brd=0; brd < ODSY_MAXBOARDS; brd++) {
			if (OdsyBoards[brd].hw == 0) {
				OdsyBoards[brd].hw = base;
				break;
			}
		}
		if (brd == ODSY_MAXBOARDS) {
			ASSERT(0);
		}
	}
	
	odsyInitBoardGfx(brd);
	odsyInitIntr(brd, conn, vhdl);
#endif
	return 0;
}
#endif	/* !_STANDALONE */

/*
 * odsyInitBoard
 */
void odsyInitBoard(int board)
{
	odsy_data *bdata = &OdsyBoards[board];
#if defined(ODSY_SIM_KERN)
	LOCK_INIT(&bdata->sim.pio_buffer_lock, NULL, PL_ZERO, NULL);
	initnsema_mutex(&bdata->sim.pio_host_serialize_sema, NULL);
	SV_INIT(&bdata->sim.pio_wait_for_symb_event_sv, SV_DEFAULT, NULL);
	SV_INIT(&bdata->sim.pio_wait_for_host_write_sv, SV_DEFAULT, NULL);
	bdata->sim.pio_count = 0;
	bdata->sim.pio_writep = bdata->sim.pio_readp = 0; 
	bdata->sim.pio_uthreads = 0;
	bdata->sim.pio_symbiote_gfxp   = 0;
	bdata->sim.dma_symbiote_gfxp   = 0;
#endif


#if !defined(_STANDALONE)
	MUTEX_INIT(&bdata->dma.lock, MUTEX_DEFAULT,0);
	SV_INIT(&bdata->dma.sv, SV_DEFAULT, 0);
#endif	/* !_STANDALONE */

	bdata->dma.sync_writep 
		= bdata->dma.async_writep
		= bdata->dma.sync_readp
		= bdata->dma.async_readp
		= 0;

#if !defined(_STANDALONE)
	initnsema_mutex(&bdata->dfifo_sema, "odsy dfifo sema");
	spinlock_init(&bdata->mouse_pos_lock, "odsy mouse pos lock");

	INITSEMA(&bdata->teardown_sema,1);

#endif	/* !_STANDALONE */

	if (!bdata->info_found)
		odsyGatherInfo(board);

	// might want odsyReset before the GatherInfo XXX
	odsyReset(board);
}

#if !defined(_STANDALONE)
/*
 * intr related initialization and bookkeeping
 * called exactly once for each board, from odsy_attach
 */
void odsyInitIntr(int board, vertex_hdl_t conn, vertex_hdl_t vhdl)
{
	device_desc_t intr_desc;
	odsy_data *bdata = &OdsyBoards[board];

	bdata->dev = conn;

#ifdef ODSY_DEBUG_INTR
	MUTEX_INIT(&bdata->intr.trace_lock,MUTEX_DEFAULT,0);
	bdata->intr.trace_rec_nr = 0;
#endif


	intr_desc = device_desc_dup(vhdl);

	bdata->intr.xt_intr	= xtalk_intr_alloc(conn, intr_desc, vhdl);
	bdata->intr.flag_intr	= xtalk_intr_alloc(conn, intr_desc, vhdl);
	bdata->intr.dma_intr	= xtalk_intr_alloc(conn, intr_desc, vhdl);
	bdata->intr.vtrace_intr	= xtalk_intr_alloc(conn, intr_desc, vhdl);
	bdata->intr.ssync_intr	= xtalk_intr_alloc(conn, intr_desc, vhdl);

	device_desc_free(intr_desc);

	bdata->intr.xt_intr_vector_num =
		(short) xtalk_intr_vector_get(bdata->intr.xt_intr);
	bdata->intr.flag_intr_vector_num =
		(short) xtalk_intr_vector_get(bdata->intr.flag_intr);
	bdata->intr.dma_intr_vector_num =
		(short) xtalk_intr_vector_get(bdata->intr.dma_intr);
	bdata->intr.vtrace_intr_vector_num =
		(short) xtalk_intr_vector_get(bdata->intr.vtrace_intr);
	bdata->intr.ssync_intr_vector_num =
		(short) xtalk_intr_vector_get(bdata->intr.ssync_intr);

	xtalk_intr_connect(bdata->intr.xt_intr,
		(intr_func_t) odsyXtErrorInterrupt, bdata, NULL, NULL, NULL);
	xtalk_intr_connect(bdata->intr.flag_intr,
		(intr_func_t) odsyFlagInterrupt, bdata, NULL, NULL, NULL);
	xtalk_intr_connect(bdata->intr.dma_intr,
		(intr_func_t) odsyDmaInterrupt, bdata, NULL, NULL, NULL);
	xtalk_intr_connect(bdata->intr.vtrace_intr,
		(intr_func_t) odsyRetraceHandler, bdata, NULL, NULL, NULL);
	xtalk_intr_connect(bdata->intr.ssync_intr,
		(intr_func_t) odsySsyncInterrupt, bdata, NULL, NULL, NULL);

	bdata->intr.xt_intr_dst_addr = xtalk_intr_addr_get(bdata->intr.xt_intr);
	bdata->intr.intr_ctl = xtalk_intr_addr_get(bdata->intr.flag_intr);

	/*
	 * device-side initialization done in odsyInitInterrupts 
	 */
}
#endif	/* !_STANDALONE */


/*
** odsyFindBoardAddr
** 
** called many times from earlyinit to determine if
** odsy boardnr 'brdnr' exists.  thus this routine
** determines if that particular boardnr is installed
** on the system (presumably by badaddr'ing a given
** xio slot + offset).
**
** the return value is the physical offset to the device
** or zero if not found.
**
** in simulation mode we return brdnr+1 if the board exists
** as the hw addr (and the pio macros know about this, so 
** don't change it).
*/
odsy_hw *odsyFindBoardAddr(int brdnr)
{
	odsy_hw *hw;
	int widget_id;

	if (OdsyBoards[brdnr].board_found != -1) {
		/*
		 * If odsy_attach has already been called, be sure we
		 * don't call xtalk_early_piotrans_addr.
		 */
#if !defined( _STANDALONE )
		cmn_err(CE_WARN,"odsy board hw found 0x%x\n",OdsyBoards[brdnr].hw);
#endif
		return OdsyBoards[brdnr].hw;
	}
#ifdef ODSY_SIM_KERN
	if (brdnr < 0 || brdnr >= ODSY_SIM_NR_PSEUDO_BOARDS)
		hw = NULL;
	else
		hw = (odsy_hw*) (__uint64_t) (brdnr + 1);	/* brdnr > 0 */
	widget_id = 1;
#else	/* ! ODSY_SIM_KERN */

#if defined(_STANDALONE)
	hw = odsy_xtalk_probe(ODSY_XT_ID_PART_NUM_VALUE,
			      ODSY_XT_ID_MFG_NUM_VALUE,
			      brdnr);
#else	/* !_STANDALONE */


	hw = (odsy_hw *) xtalk_early_piotrans_addr(
		ODSY_XT_ID_PART_NUM_VALUE,
		ODSY_XT_ID_MFG_NUM_VALUE,
		brdnr, 0, sizeof(odsy_hw),
		0);	/* flags */

#endif	/* _STANDALONE */
#endif /* ODSY_SIM_KERN */

	if (hw) {
#ifndef ODSY_SIM_KERN
		__uint32_t w_control = hw->xt.control;
		widget_id = w_control & WIDGET_WIDGET_ID;
#endif

		OdsyBoards[brdnr].widget_id = widget_id;
		OdsyBoards[brdnr].hw = hw;
		OdsyBoards[brdnr].boardnr = brdnr;	/* ody boardnr, not gfx boardnr */
	}
	OdsyBoards[brdnr].board_found = hw ? 1 : 0;
	return hw;
}

#if !defined(_STANDALONE)
/*
** odsyInitBoardGfx
**
** this routine is only ever called ONCE - as a result of XIO bus discovery (i.e. from odsy_attach)
** 
** it will NOT be called for mbuzz slaves!  thus the master in an mbuzz config needs to init its slave.
**
** IMPORTANT: This routine is called after the first odsy_earlyinit
** call.  It should only initialize the data structures that will be
** used by the RRM and device-dependent X/OpenGL interfaces, and
** should not be used to initialize the hardware except for uses
** unique to those interfaces. 
*/

void odsyInitBoardGfx(int board)
{
	odsy_data *bdata = &OdsyBoards[board];
	unsigned odsy_inv_bits = 0;
	int i;

#ifdef ODSY_DEBUG_INIT
	cmn_err(CE_DEBUG,"in odsyInitBoardGfx\n");
#endif

	bzero((void *)(&(bdata->gfx_data)), sizeof(struct gfx_data));



	INITSEMA(&bdata->hw_ctx_id_sema, 1);

	INITSEMA(&bdata->hw_wr_slot_sema, 1);
	OdsyInit32kSlotMask(&bdata->hw_wr_slots, &bdata->hw_wr_slot_sema);

	odsyInitDDRNs();

	odsyInitHwWrRgn(board);

        bdata->mm.kv_global_rgn = kmem_zalloc(ODSY_GLOBAL_RGN_BYTES,
					      KM_SLEEP|KM_PHYSCONTIG|KM_CACHEALIGN);

	bdata->kv_ucode_rgn = (char*) bdata->mm.kv_global_rgn + sizeof(odsy_mm_global_rgn);

	switch( bdata->info.dd_info.sdram_size ) {
		case 32:
			odsy_inv_bits |= INV_ODSY_MEMCFG_32;
			break;
		case 64:
			odsy_inv_bits |= INV_ODSY_MEMCFG_64;
			break;
		case 128:
			odsy_inv_bits |= INV_ODSY_MEMCFG_128;
			break;
	}
	switch ( bdata->info.dd_info.buzz_type ) {
		case ODSY_BUZZ_REVA_TYPE:
			odsy_inv_bits |= INV_ODSY_REVA_ARCH; 
			break;
		case ODSY_BUZZ_REVB_TYPE:
			odsy_inv_bits |= INV_ODSY_REVB_ARCH;
			break;
	}

       /* Register ody board for reading by hinv. */
	add_to_inventory(INV_GRAPHICS,INV_ODSY,
			 0,
			 0, /* XXX board nr */
			 odsy_inv_bits); 

	bdata->gfx_data.numpcx = 1; /* number of pipe contexts in HW */
	bdata->gfx_data.nummodes = ODSY_NUM_DISPLAYMODES; 
	bdata->gfx_data.loadedmodes = bdata->loadedmodes;

	/* we have one external swap ready line */
	bdata->gfx_data.swapready_line_cnt = ODSY_NUM_SWAPREADY_LINES;
 
	mutex_init(&bdata->diag.scanbuf_lock, MUTEX_DEFAULT, "scanbuf_lock");
	mutex_init(&bdata->diag.hw_shadow_ptrs_lock, MUTEX_DEFAULT, "shdwptr_lock");
	mutex_init(&bdata->pfstat_lock, MUTEX_DEFAULT, "pfstat_lock");

	GfxRegisterBoard((struct gfx_fncs *)&odsy_gfx_fncs,
			 (struct gfx_data *)&(bdata->gfx_data),
			 (struct gfx_info *)&(bdata->info));

}
#endif	/* _STANDALONE */


/*
** odsyGatherInfo
**
** called only ONCE for each earlyinit discovered board.  at this 
** point we read all the chip revs, and determine whether or not
** the back-end thinks we are attached to another buzz (i.e. we're
** either the master or slave in an mbuzz config).
*/
int odsyGatherInfo(int board)
{
	odsy_data	*bdata;
	odsy_ddinfo     *odsyinfo;
	struct gfx_info *gfxinfo;
	/*REFERENCED --- below */
	odsy_hw         *hw;
        __odsyreg_fifo_levels levels;
	unsigned short 	sdram_size;
	unsigned short 	sdram_casl;
	unsigned short 	sdram_banks;
	unsigned short 	board_ver;
	__odsyreg_widget_id xt_widget;
	pbj_HIF_status_reg pbj_HIF_status;

	bdata = &OdsyBoards[board];

	odsyinfo = &(bdata->info.dd_info);
	gfxinfo  = &(bdata->info.gfx_info);
	hw       = bdata->hw;

	bzero((void *)&(bdata->info), sizeof(odsy_info));
	bdata->info_found = 1;

#if !defined(ODSY_SIM_KERN)
	strncpy(bdata->info.gfx_info.name, GFX_NAME_ODSY, GFX_INFO_NAME_SIZE);
#else 
	strncpy(bdata->info.gfx_info.name, GFX_NAME_ODSYSIM, GFX_INFO_NAME_SIZE);
#endif

#ifdef ODSY_DEBUG_INIT
	cmn_err(CE_NOTE,"setting name to %s\n",bdata->info.gfx_info.name);
#endif
	bdata->info.gfx_info.length = sizeof(odsy_info);

	/* Must be set no matter what - gets overwritten when loading timing */
	gfxinfo->xpmax = 512;
	gfxinfo->ypmax = 512;
	odsyinfo->unpriv_top   = -1;
	odsyinfo->unpriv_bottom= -1;
	odsyinfo->product_id   = -1; /* unused */

	/* Get the buzz rev id from xtalk reg (widget_id) */
	ODSY_READ_HW(hw,xt.widget_id,*(__uint32_t*)&xt_widget);
	odsyinfo->buzz_rev = xt_widget.rev_num;

	/* Get the pbj rev id from the pbj_HIF_status_reg */
	ODSY_DFIFO_PSEMA(bdata);
	if (ODSY_POLL_DFIFO_LEVEL(hw, 0)) {
		ODSY_DFIFO_VSEMA(bdata);
		return -1;
	}
	ODSY_DFIFO_VSEMA(bdata);
	ODSY_READ_HW(hw,dbe_diag_rd.PBJ_HIF_status,pbj_HIF_status.w32);
	odsyinfo->pbj_rev = pbj_HIF_status.bf.pbj_rev_id;

	if (odsy_i2c_init_port_handle(&(bdata->i2c_port[ODSY_I2C_MAIN_PORT]),
				      bdata,
				      ODSY_I2C_MAIN_PORT) != 0) {
		cmn_err(CE_ALERT,"odsyGatherInfo(): cannot initialize main i2c port");
	}
	if (odsy_i2c_init_port_handle(&(bdata->i2c_port[ODSY_I2C_OPT_PORT]),
				      bdata,
				      ODSY_I2C_OPT_PORT) != 0) {
		cmn_err(CE_ALERT,"odsyGatherInfo(): cannot initialize opt i2c port");
	}

	/* Read fifo levels for sdram config info and board version */
	ODSY_READ_HW(hw,sys.fifo_levels,*(__uint32_t*)&levels);
	sdram_size  = (levels.sdram_window_pins & ODSY_SDRAM_WINPIN_SZ_MSK) >> 6; /* size is in upper 2 bits */
	sdram_banks = (levels.sdram_window_pins & ODSY_SDRAM_WINPIN_BANK_MSK) >> 5; /* banks is in 3rd of upper 4 */
	sdram_casl  = (levels.sdram_window_pins & ODSY_SDRAM_WINPIN_CASL_MSK) >> 4; /* casl is in 4th of upper 4 */
	board_ver   = levels.sdram_window_pins & ODSY_SDRAM_WINPIN_BRD_VER_MSK; /* not sure if this is right, will
										 * keep for now */
	odsyinfo->board_rev = board_ver;

#if ODSY_CONFIG_DEBUG
	printf("0x%x sdram_size: %d, sdram_casl: %d, sdram_banks: %d, board_ver: %d\n",
		levels.sdram_window_pins,sdram_size,sdram_casl,sdram_banks,board_ver);
#endif
	switch(sdram_size) 
	{
		case ODSY_CONFIG_SDRAM_SIZE_32MB:
			odsyinfo->sdram_size = 32;
			break;
		case ODSY_CONFIG_SDRAM_SIZE_64MB:
			odsyinfo->sdram_size = 64;
                        break;
		case ODSY_CONFIG_SDRAM_SIZE_128MB:
			odsyinfo->sdram_size = 128;
                        break;
		default:
			cmn_err(CE_ALERT,"odsyGatherInfo:unknown sdram_size\n");
	}

	switch(sdram_casl) 
	{
		case ODSY_CONFIG_SDRAM_CASL_2:
			odsyinfo->sdram_casl = 2;
			break;
		case ODSY_CONFIG_SDRAM_CASL_3:
			odsyinfo->sdram_casl = 3;
			break;
		default:
			cmn_err(CE_ALERT,"odsyGatherInfo:unknown sdram casl setting\n");
	}

	switch(sdram_banks) 
	{
		case ODSY_CONFIG_SDRAM_BANKS_2:
			odsyinfo->sdram_banks = 2;
			break;
		case ODSY_CONFIG_SDRAM_BANKS_4:
			odsyinfo->sdram_banks = 4;
			break;
		default:
			cmn_err(CE_ALERT,"odsyGatherInfo:unknow number of sdram banks\n");
	}

	switch(odsyinfo->buzz_rev)
	{
		case ODSY_BUZZ_REV_1:
		case ODSY_BUZZ_REV_2:
		case ODSY_BUZZ_REV_3:
			odsyinfo->buzz_type = ODSY_BUZZ_REVA_TYPE;
			break;
		case ODSY_BUZZ_REV_9:
			odsyinfo->buzz_type = ODSY_BUZZ_REVB_TYPE;
			odsyinfo->buzz_rev = ODSY_BUZZ_REV_1;
			break;
		case ODSY_BUZZ_REV_10:
			odsyinfo->buzz_type = ODSY_BUZZ_REVB_TYPE;
                        odsyinfo->buzz_rev = ODSY_BUZZ_REV_2;
                        break;
		case ODSY_BUZZ_REV_11:
			odsyinfo->buzz_type = ODSY_BUZZ_REVB_TYPE;
			odsyinfo->buzz_rev = ODSY_BUZZ_REV_3;
			break;
		case ODSY_BUZZ_REV_12:
			odsyinfo->buzz_type = ODSY_BUZZ_REVB_TYPE;
                        odsyinfo->buzz_rev = ODSY_BUZZ_REV_4;
                        break;
		case ODSY_BUZZ_REV_13:
			odsyinfo->buzz_type = ODSY_BUZZ_REVB_TYPE;
			odsyinfo->buzz_rev = ODSY_BUZZ_REV_5;
			break;
		case ODSY_BUZZ_REV_14:
			odsyinfo->buzz_type = ODSY_BUZZ_REVB_TYPE;
                        odsyinfo->buzz_rev = ODSY_BUZZ_REV_6;
                        break;
		case ODSY_BUZZ_REV_15:
			odsyinfo->buzz_type = ODSY_BUZZ_REVB_TYPE;
                        odsyinfo->buzz_rev = ODSY_BUZZ_REV_7;
                        break;
		default:
			cmn_err(CE_ALERT, "odsyGatherInfo: Invalid buzz rev type %d\n",odsyinfo->buzz_rev);
	}

#if ODSY_CONFIG_DEBUG
	printf("sdram_size: %d, sdram_casl: %d, sdram_banks: %d\n",
		odsyinfo->sdram_size,odsyinfo->sdram_casl,odsyinfo->sdram_banks);
#endif
	return 0;
}

#if !defined(_STANDALONE)
void odsy_init_ddrncount_timeout(sema_t * sema)
{
	cmn_err(CE_WARN,"odsy-kern: ddrncount timeout\n");
	vsema(sema);
}

/* 
 * This gets called by gfxinit (before the board manager gets running)
 */
int gf_OdsyInitialize(struct gfx_gfx *gfxp)
{
	int waitCount;
	odsy_data *bdata = (odsy_data*)gfxp->gx_bdata;
	unsigned int old_gx_flags;
  
	GFXLOGT(ODSY_gf_OdsyInitialize);

	for (waitCount = 0; bdata->nropen_ddrn; waitCount++) {
		cmn_err(CE_WARN, "%d rnodes still open when OdsyInitialize called\n",
			bdata->nropen_ddrn);
		INITSEMA(&bdata->init_ddrncount_sleeper, 0);
		timeout(odsy_init_ddrncount_timeout, &bdata->init_ddrncount_sleeper, 5 * HZ);
		psema(&bdata->init_ddrncount_sleeper, PZERO);
		if (waitCount > 10)
			return -1;
	}

	/* make sure we own gfxsema on exit */
	if (gfxp->gx_bdata->gfxsema_owner != gfxp) {
		GET_GFXSEMA(gfxp);
	}

#if !defined(ODSY_TEXTPORT_DISABLED) && (!defined(ODSY_SIM_KERN) || defined(ODSY_TEXTPORT_ALWAYS_ON))
	htp_register_board(0, 0, 0, 0);
#endif
			
	odsyReset(bdata->boardnr);

#if !defined(ODSY_NO_FC)
	/*
	 * (re)initialize flow control
	 *
	 * note that we own gfxsema and we're (probably?) mapped.  we
	 * need to detach flow control in order to dealloc and realloc
	 * the flow_hdl, but we must also guarantee that we don't get
	 * reattached unexpectedly due to kpreemption.  since these
	 * operations are too large to do at splhi, we must unset the
	 * mapped &or inuse flag for the duration.
	 */
	if (old_gx_flags = gfxp->gx_flags & (GFX_MAPPED | GFX_INUSE)) {
		UNSET_FC_AND_FLAG(ODSY_UNSET_FC_REINIT, old_gx_flags);
	}
	if (bdata->flow_hdl) {
		gfx_flow_dealloc(bdata->flow_hdl);
	}
	bdata->flow_hdl = gfx_flow_alloc(
			bdata->dev,
			(iopaddr_t)bdata->hw | ODY_FC_ADDR_LO,
			(iopaddr_t)ODY_FC_MASK,
			ODY_FC_START_CREDIT * sizeof(long),
			ODY_FC_TIMER_NS,
			(gfx_flow_intr_func_t)odsyHiwaterInterrupt,
			bdata,
			GFX_FLOW_IMPL_REQUIRES_DWORD_CREDITS);
	ASSERT(bdata->flow_hdl);

	/*
	* initialize flow control in gfx board
	*/
	ODSY_POLL_CFIFO_LEVEL(bdata->hw,0);
	ODSY_WRITE_HW(bdata->hw, sys.flow_ctl_clr, 0);
	ODSY_WRITE_HW(bdata->hw, sys.flow_ctl_clr, 1);
	ODSY_WRITE_HW(bdata->hw, sys.flow_ctl_clr, 0);

	UINT32(bdata->sysreg_shadow.flow_ctl_config)
#if IP30
		= ODSY_FLOWCTL_HEART_MODE
#elif IP27
		= ODSY_FLOWCTL_ASD_HACK_MODE	/* Hub */
#endif
		| ODSY_FLOWCTL_TIMO(ODY_FLOW_CTL_TIMEOUT)
		| ODSY_FLOWCTL_COHERENT(0)
		| ODSY_FLOWCTL_GBR(0)
		| ODSY_FLOWCTL_DIDN(ODY_FLOW_CTL_DIDN)
		| ODSY_FLOWCTL_DBE_FL_CTL_EN(0);

	PUSH_SHADOWED_SYSREG(bdata, flow_ctl_config);

        /* now we're ready to be flow controlled again */
	if (old_gx_flags) {
		SET_FC_AND_FLAG(ODSY_SET_FC_REINIT, old_gx_flags);
	}
#endif

	odsyPushSysConfig(bdata, 1);	/* re-write sys.config */

	/* Init CFIFO here because ucode download (which comes before GFX_START->gf_OdsyStart) needs it */
	odsyInitCfifo(bdata->boardnr, 1);


	// Reset the sw sync hw write slot allocator.
	
	OdsyInit32kSlotMask(&bdata->hw_wr_slots, &bdata->hw_wr_slot_sema);		

	// Allocate a dead (0) slot.  Too easy to get by accident.  Call it the scratch sw sync slot :)

	OdsyAlloc32kSlot(&bdata->hw_wr_slots);

	// This sw sync slot can be used to debug pipe hangs...
	bdata->debug_hw_wr_slot  =  OdsyAlloc32kSlot(&bdata->hw_wr_slots);
	bdata->debug_hw_wr       = bdata->mm.kv_hw_wr_rgn + bdata->debug_hw_wr_slot;
	bdata->debug_hw_wr->val  = 0x0;


	odsyInitDMA(bdata->boardnr);

	bdata->diag.scanbuf_owner = 0;
	bdata->diag.hw_shadow_ptrs_owner = 0;

	bdata->board_started = 0;

	odsyInitRetrace(bdata); /* do at or before interrupts are enabled */

	odsyInitInterrupts(bdata->boardnr); 

	/*
	 * Init back end, but wait for X to load a timing table.  Used
	 * to only do this on the slave for multi-buzz, but that is dead.
	 */
	odsyInitDBE(bdata, default_timing, NoDownload);

	return 0;
}


/* 
** Called by gfxinit, but after gf_OdsyInitialize.
** We _do_ own gfxsema at this point.
*/
int gf_OdsyStart(struct gfx_gfx *gfxp)
{
	odsy_data *bdata;
	odsy_hw	  *hw;

	GFXLOGT(ODSY_gf_OdsyStart);

	ASSERT(gfxp);
	bdata = (odsy_data*)gfxp->gx_bdata;
	ASSERT(bdata);
	hw = bdata->hw;
	ASSERT(hw);
	

	bdata->gfx_data.currentrn   = 0;
	bdata->gfx_data.numrns      = 0;
	bdata->gfx_data.gfxbackedup = 0;
	bdata->gfx_data.gfxresched  = 0;
	bdata->gfx_data.gfxbusylock = 0;
	
	bdata->abnormal_teardown = 0;
	bdata->board_started = 1;
	bdata->last_opengl_hw_ctx_id = ODSY_BOGUS_HW_CONTEXT_ID;
 
	odsyInitDrawables(bdata);

	/* setup software sync push back location */
	{
		paddr_t physaddr = kvtophys(bdata->mm.kv_hw_wr_rgn);

		bdata->sysreg_shadow.sync_base_addr_hi = (__uint32_t)((physaddr>>32)&0xffff);
		bdata->sysreg_shadow.sync_base_addr_lo = (__uint32_t)(physaddr&0xffffffff);
		PUSH_SHADOWED_SYSREG(bdata,sync_base_addr_hi);
		PUSH_SHADOWED_SYSREG(bdata,sync_base_addr_lo);
	}

	SET_FC_SPLHI(ODSY_SET_FC_START);

	{	/*
		 * zero all shadow ram locations (mostly so 
		 * there are no X's during hw simulation).
		 */
		static uint32_t zero;
		int ii, iiMax, jj;

		iiMax = sizeof(odsy_hw_shadow) / sizeof(uint32_t);
		ODSY_WRITE_HW(hw, cfifo_priv.by_32.w0,
			      ODSY_GFE_CMD(SHADOW_RST,
					   SETADDR,
					   BUZZ_0,
					   1));
		ODSY_WRITE_HW(hw, cfifo_priv.by_32.w0,
			      ODSY_HW_SHADOW_FIRST_WORD);
		for (ii = 0; ii < iiMax; ii += 16) {
			if (ii + 16 <= iiMax) {
				ODSY_WRITE_HW(hw, cfifo_priv.by_32.w0,
					      ODSY_GFE_CMD(SHADOW_RST,
							   PUSHDATA,
							   BUZZ_0,
							   15));
				jj = 16;
			} else if (ii + 15 != iiMax) {
				jj = iiMax - ii;
				ODSY_WRITE_HW(hw, cfifo_priv.by_32.w0,
					      ODSY_GFE_CMD(SHADOW_RST,
							   PUSHDATA,
							   BUZZ_0,
							   jj));
			} else {
				ODSY_WRITE_HW(hw, cfifo_priv.by_32.w0,
					      ODSY_GFE_CMD(SHADOW_RST,
							   PUSHDATA,
							   BUZZ_0,
							   1));
				jj = 1;
				/* write one word, then 14 */
				ii -= 15;
			}
			while (jj-- > 0) {
				ODSY_WRITE_HW(hw, cfifo_priv.by_32.w0, 0);
			}
		}
		
		/* prevent X's in the xform */
		BUZZ_VtxLoadNoShadow(VB_CScratch1, BUZZ_FMT_Color4i, 4);
		ODY_STORE_i(0);
		ODY_STORE_i(0);
		ODY_STORE_i(0);
		ODY_STORE_i(0);
	}


	// Program the hwsync timeout to something less than 80 seconds.
	// The units here are buzz clocks.  So...  off-handing these.
	{
		unsigned int sync_clocks;
		sync_clocks = 1000000000;  // 5 seconds at 200 MHz.
		
		ODSY_WRITE_HW(bdata->hw,cfifo_priv.by_64,
			      ODSY_DW_CFIFO_PAYLOAD(ODSY_GFE_CMD(SYNC_TIMEOUT,0,BUZZ_ALL,1),
						    sync_clocks));
		
	}


	UNSET_FC_SPLHI(ODSY_UNSET_FC_START);

	return 0;
}
#endif	/* !_STANDALONE */

static void odsyLLPReset(odsy_data *bdata)
{
#ifndef ODSY_SIM_KERN
	odsy_sysreg_shadow *sshdw = &(bdata->sysreg_shadow);
	odsy_hw *hw               = bdata->hw;
	__odsyreg_abort abort;
	volatile __uint32_t w_control = hw->xt.control;

#ifdef XXX
	// XXX the ody flush bit freezes the system
	//     this bit was suppose to be the global reset bit
	//     for buzz but its broke.
	// Scan flush board reset
	ODSY_READ_HW(bdata->hw, sys.abort, *((__uint32_t*)&abort));
	abort.ody_flush = 1;
	ODSY_WRITE_HW(bdata->hw, sys.abort, *(uint32_t*)&abort);

	// there is a huge capacitor in the reset line.
	DELAY(10000);
#endif /*XXX*/	

	// reset buzz
#if !defined(_STANDALONE)
	if (bdata->dev) {
		/* will not get here if odsy_attach not yet called */
		xwidget_reset((vertex_hdl_t)bdata->dev);    
	} else {
		/*
		 * muck with xbow registers directly
		 * need this for textport
		 */
#if IP30
		xbow_t *xbow = (xbow_t *)K1_MAIN_WIDGET(XBOW_ID);
#elif IP27
		#include "sys/xtalk/xtalk.h"
		#include "sys/xtalk/xbow.h"
		xbow_t *xbow = (xbow_t *)NODE_SWIN_BASE(NASID_GET((__psunsigned_t)hw), 0);
#endif
		xbowreg_t link_control;
		int widget_id = bdata->widget_id;

		/*
		 * Save the link control register to be restored later. The prom
		 * set the LLP widget credits and reseting will change it to 2.
		 * Also save the widget control register.
		 */
		link_control = xbow->xb_link(widget_id).link_control;

		/*
		 * A write to the xbow link_reset register will act as a
		 * "one-shot," forcing link reset to be asserted for 4 core
		 * clock cycles.
		 */
		xbow->xb_link(widget_id).link_reset = 0;
		DELAY(10000);

		/* restore link control register */
		xbow->xb_link(widget_id).link_control = link_control;
	}
#else /* _STANDALONE */
	reset_xbow_link((__psunsigned_t)XBOW_K1PTR, bdata->widget_id);
#endif /* !_STANDALONE */
	hw->xt.control = w_control;
#endif
}


void odsyReset(int board)
{
	odsy_data *bdata          = ODSY_BDATA_FROM_PIPENR(board);
	odsy_sysreg_shadow *sshdw = &(bdata->sysreg_shadow);
	odsy_hw *hw               = bdata->hw;
	__odsyreg_abort abort;
	buzz_HIF_config_reg buzz_hif_config;
	bdata->board_started     = 0;
	bdata->abnormal_teardown = 0;

    DPUTS("Calling odsyReset\r\n");

	// Reset the link
	odsyLLPReset(bdata);

	/*
	 * toggle all the bits in the abort register
	 * (except ody_flush, related to scan, which seems to cause ECC errors)
	 */
	*(__uint32_t *)&abort = 0;
	abort.dbe_fifo_flush = 1;
	abort.dma = 1;
	abort.copy = 1;
	abort.pio_fmt = 1;
	abort.dma_fmt = 1;
	abort.hw_sync = 1;
	ODSY_WRITE_HW(bdata->hw, sys.abort, *(uint32_t*)&abort);
	DELAY(200);
	abort.dbe_fifo_flush = 0;
	abort.dma = 0;
	abort.copy = 0;
	abort.pio_fmt = 0;
	abort.dma_fmt = 0;
	abort.hw_sync = 0;
	ODSY_WRITE_HW(bdata->hw, sys.abort, *(uint32_t*)&abort);

	// Soft dbe/pbj reset 
	/* Assert DBE soft reset */
	abort.dbe_sreset = 1;
	ODSY_WRITE_HW(bdata->hw, sys.abort, *(uint32_t*)&abort);

	/* Wait 1uSec which is about 10x longer than we need to*/
	DELAY(1);

	/* Deassert DBE soft reset */
	abort.dbe_sreset = 0;
	ODSY_WRITE_HW(bdata->hw, sys.abort, *(uint32_t*)&abort);

	/* Wait 1uSec which is about 10x longer than we need to*/
	DELAY(1);

	ODSY_DFIFO_PSEMA(bdata);
	if (ODSY_POLL_DFIFO_LEVEL(hw, ODSY_DFIFO_LEVEL_REQ(2))) {
		ODSY_DFIFO_VSEMA(bdata);
		return;
	}

	/* Put PBJ in reset by writing to the buzz HIF_Config reg */
	buzz_hif_config.w32 = 0;
	buzz_hif_config.bf.dcb_clk_mod = 1; /* XXX hack me */
	buzz_hif_config.bf.pbj_reset = 1;
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, buzz_HIF_config, buzz_hif_config.w32));

	ODSY_DFIFO_VSEMA(bdata);
	/* Delay 1uSec, PBJ HIF reset with DCB clock on */
	DELAY(1);

	/* Now take PBJ out of reset */
	buzz_hif_config.bf.dcb_clk_mod = 1; /* XXX hack me */
	buzz_hif_config.bf.pbj_reset = 0;
	ODSY_WRITE_HW(bdata->hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, buzz_HIF_config, buzz_hif_config.w32));

	/* Delay 1uSec, Release of reset */
	DELAY(1);

	ODSY_CLEAR_HW_TOUCH_ERROR(board+1);

	/*
	** yeah, this looks silly.  but it accomplishes
	** two things.  puts the hardware in a known
	** state (same as out of hard reset + sw sync mods) and
	** ensures our system register shadow is up to date.
	*/

#if defined(ODSY_SIM_KERN)
	/* xt.control is normally set by the prom and saved in odsyFindBoardAddr */
	UINT32(sshdw->control) = (3 << 12)	/* LLP crossbar credit */
			      | (1 << 9)	/* big endian */
			      | 0xe;		/* widget id */
#else
	ODSY_READ_HW(hw,xt.control,*(__uint32_t*)&sshdw->control);
	sshdw->control.widget_id = bdata->widget_id;
#endif
	PUSH_SHADOWED_XTREG(bdata, control);

	odsyPushSysConfig(bdata, 0);

	UINT32(sshdw->abort) = 0;
	PUSH_SHADOWED_SYSREG(bdata,abort);
	
	UINT32(sshdw->flag_intr_enab_set) = 0;
	ODSY_WRITE_HW(bdata->hw,sys.flag_intr_enab_clr,~UINT32(sshdw->flag_intr_enab_set));

	UINT32(sshdw->flag_set_priv) = 0;
	ODSY_WRITE_HW(bdata->hw,sys.flag_clr_priv,~UINT32(sshdw->flag_set_priv));

	UINT32(sshdw->intr_ctl_hi) = 0;
	PUSH_SHADOWED_SYSREG(bdata,intr_ctl_hi);
	
	UINT32(sshdw->intr_ctl_lo) = 0;
	PUSH_SHADOWED_SYSREG(bdata,intr_ctl_lo);
		       
	UINT32(sshdw->intr_vector) = 0;
	PUSH_SHADOWED_SYSREG(bdata,intr_vector);

	UINT32(sshdw->intr_header) = 0;
	PUSH_SHADOWED_SYSREG(bdata,intr_header);

	UINT32(sshdw->flow_ctl_config) = 0;
	PUSH_SHADOWED_SYSREG(bdata,flow_ctl_config);

	UINT32(sshdw->flow_ctl_detach) = 0;
	PUSH_SHADOWED_SYSREG(bdata,flow_ctl_detach);

	UINT32(sshdw->scan_config) = 0;
	PUSH_SHADOWED_SYSREG(bdata,scan_config);

	UINT32(sshdw->scan_addr_hi) = 0;
	PUSH_SHADOWED_SYSREG(bdata,scan_addr_hi);

	UINT32(sshdw->scan_addr_lo) = 0;
	PUSH_SHADOWED_SYSREG(bdata,scan_addr_lo);

	UINT32(sshdw->scan_go) = 0;
	PUSH_SHADOWED_SYSREG(bdata,scan_go);

	UINT32(sshdw->pf_base_addr_hi) = 0;
	PUSH_SHADOWED_SYSREG(bdata,pf_base_addr_hi);

	UINT32(sshdw->pf_base_addr_lo) = 0;
	PUSH_SHADOWED_SYSREG(bdata,pf_base_addr_lo);
			
	UINT32(sshdw->pf_mode) = 0;
	PUSH_SHADOWED_SYSREG(bdata,pf_mode);

	UINT32(sshdw->sync_base_addr_hi) = 0;
	PUSH_SHADOWED_SYSREG(bdata,sync_base_addr_hi);

	UINT32(sshdw->sync_base_addr_lo) = 0;
	PUSH_SHADOWED_SYSREG(bdata,sync_base_addr_lo);

	/* 
	** we don't want any sync address wrapping, and
	** we always want them to be coherent...  the offset 
	** space for sw syncs we care to use is 15b.  
	*/
	sshdw->sync_mode.extent   = 0xc;  /* 3 bits (dword, natural) + 0:14 (15 bits) means we need to be 256Kb aligned! */
	sshdw->sync_mode.coherent = 1;
	sshdw->sync_mode.didn     = ODSY_XT_HEART_ID;
	/*
	 * assume a 1-buzz system
	 * in a 2-buzz system, should be 0x80 for buzz-0 and 0x40 for buzz-1
	 */
	sshdw->sync_mode.enables  = 0xc0;
	sshdw->sync_mode._pad     = 0;
	PUSH_SHADOWED_SYSREG(bdata,sync_mode);

	/*
	** init glget offset
	*/
	sshdw->glget_offset.cl_offset = 18; /* hw shadow is 18 cachelines */
	sshdw->glget_offset.extent    = 4;  /* 7 bits (cacheline,natural) + 0:4 (5 bits) means we need to be 4Kb aligned */
	sshdw->glget_offset.coherent  = 1;
	sshdw->glget_offset.didn      = ODSY_XT_HEART_ID;
	PUSH_SHADOWED_SYSREG(bdata,glget_offset);

	/*
	** init diag stuff
	*/
	UINT32(sshdw->diag_base_addr_hi) = 0;
	PUSH_SHADOWED_SYSREG(bdata,diag_base_addr_hi);

	UINT32(sshdw->diag_base_addr_lo) = 0;
	PUSH_SHADOWED_SYSREG(bdata,diag_base_addr_lo);
	
	UINT32(sshdw->diag_mode) = 0;
	PUSH_SHADOWED_SYSREG(bdata,diag_mode);

	/*
	 * Init regs related to swapbuf.
	 */

	/* swap on vert-retrace, not swap-ready */
	ODSY_WRITE_HW(hw, sys.swap_on_ready_or_vert, 0);

	/* init all WIDs to display buffer A */
	ODSY_WRITE_HW(hw, sys.swapb_selB_clr, 0xffffffff);

	/* <<<< RESET SDRAM >>>>
	** send sdram reset pulse after putting the proper bits 
	** (read out of window pins earlier) into the config
	*/
	switch(	bdata->info.dd_info.sdram_banks ) {
		case 2:
			sshdw->config.sdram_banks = ODSY_CONFIG_SDRAM_BANKS_2;
			break;
		case 4:
			sshdw->config.sdram_banks = ODSY_CONFIG_SDRAM_BANKS_4;
			break;
		default:
			printf("odsyReset: invalid sdram banks: %d\n",bdata->info.dd_info.sdram_banks);
			break;
	}
	switch( bdata->info.dd_info.sdram_casl ) {
		case 2:
			sshdw->config.sdram_casl = ODSY_CONFIG_SDRAM_CASL_2;
			break;
		case 3:
			sshdw->config.sdram_casl = ODSY_CONFIG_SDRAM_CASL_3;
			break;
		default:
			printf("odsyReset: invalid sdram casl: %d\n",bdata->info.dd_info.sdram_casl);
			break;
	}
	switch( bdata->info.dd_info.sdram_size ) {
		case 32:
			sshdw->config.sdram_size = ODSY_CONFIG_SDRAM_SIZE_32MB;
			break;
		case 64:
			sshdw->config.sdram_size = ODSY_CONFIG_SDRAM_SIZE_64MB;
			break;
		case 128:
			sshdw->config.sdram_size = ODSY_CONFIG_SDRAM_SIZE_128MB;
			break;
		default:
			printf("odsyReset: invalid sdram size: %dMB\n",bdata->info.dd_info.sdram_size);
			break;
	}
	
	DELAY(400);
	sshdw->config.sdram_reset = 0;
	PUSH_SHADOWED_SYSREG(bdata,config);
	DELAY(400);
	sshdw->config.sdram_reset = 1;	
	PUSH_SHADOWED_SYSREG(bdata,config);
	PUSH_SHADOWED_SYSREG(bdata,config);
	sshdw->config.sdram_reset = 0;
	PUSH_SHADOWED_SYSREG(bdata,config);

	ODSY_WRITE_HW(bdata->hw, xt.intr_dst_addr_hi, 0);
	ODSY_WRITE_HW(bdata->hw, sys.intr_ctl_hi, 0);
	ODSY_WRITE_HW(bdata->hw, sys.flag_intr_enab_clr, ~0);

	ODSY_WRITE_HW(bdata->hw, sys.flow_ctl_clr, 0);
	ODSY_WRITE_HW(bdata->hw, sys.flow_ctl_clr, 1);
	ODSY_WRITE_HW(bdata->hw, sys.flow_ctl_clr, 0);
	ODSY_WRITE_HW(bdata->hw, sys.flow_ctl_config, 0);
	ODSY_WRITE_HW(bdata->hw, sys.flag_clr_priv, ~0);
	ODSY_WRITE_HW(bdata->hw, xt.intr_config, 0);

	/* enable cfifo bypass */
	ODSY_WRITE_HW(hw,cfifo_priv.by_64,
		      ODSY_DW_CFIFO_PAYLOAD(ODSY_GFE_CMD(FLOW_CTL,CFIFO_DISABLE_BYPASS,BUZZ_ALL,1),
					    0));

	/* enable xform output */
	BUZZ_RegLoad(XFORMADDR_OfifoHighWater, 0x380);
	// set the maximum exponent for s3
	BUZZ_RegLoad(XFORMADDR_s3MaxWExp,127);
}


/*
** odsyInitCfifo
**
** writes gfe commands to configure the cfifo base and depth, then 
** disables bypass mode.  XXXyou better have come out of a soft
** reset!
** see: http://crush.engr.sgi.com/website/odyssey/b/sys_prog/index.html#user_cfifo
*/
void odsyInitCfifo(int board, int useFlowControl)
{
	odsy_data   *bdata    = ODSY_BDATA_FROM_PIPENR(board);
	odsy_hw	    *hw       = bdata->hw;
	odsy_ddinfo *odsyinfo = &bdata->info.dd_info;
	struct gfx_gfx *gfxp = bdata->gfx_data.gfxsema_owner;

	if (useFlowControl) {
		SET_FC_SPLHI(ODSY_INIT_CFIFO);
	}

	/* place cfifo at 0 */
	ODSY_WRITE_HW(hw,cfifo_priv.by_64,
		      ODSY_DW_CFIFO_PAYLOAD(ODSY_GFE_CMD(CFIFO_CFG,BASE,BUZZ_ALL,1),0));

	/* cfifo is how many 2bpp tiles wide? */
	ODSY_WRITE_HW(hw,cfifo_priv.by_64,
		      ODSY_DW_CFIFO_PAYLOAD(ODSY_GFE_CMD(CFIFO_CFG,NRTILES,BUZZ_ALL,1),
					    ODSY_CFIFO_NR_2BPP_TILES));
	
	/* set phys lowater in words */
	ODSY_WRITE_HW(hw,cfifo_priv.by_64,
		      ODSY_DW_CFIFO_PAYLOAD(ODSY_GFE_CMD(CFIFO_CFG,PHYS_LW,BUZZ_ALL,1),
					    ODSY_CFIFO_PHYS_LW_WORDS));

	/* set phys hiwater in words */
	ODSY_WRITE_HW(hw,cfifo_priv.by_64,
		      ODSY_DW_CFIFO_PAYLOAD(ODSY_GFE_CMD(CFIFO_CFG,PHYS_HW,BUZZ_ALL,1),
					    ODSY_CFIFO_PHYS_HW_WORDS));

	/* disable cfifo bypass */
	ODSY_WRITE_HW(hw,cfifo_priv.by_64,
		      ODSY_DW_CFIFO_PAYLOAD(ODSY_GFE_CMD(FLOW_CTL,CFIFO_DISABLE_BYPASS,BUZZ_ALL,1), 
					    1));

#if defined(TEXTPORT_BRINGUP) && 0
	// turn on window pins, for debugging textport
	bdata->sysreg_shadow.config.window_enab = 1;
	bdata->sysreg_shadow.config.window_sel = SRC_266_DATA;
	PUSH_SHADOWED_SYSREG(bdata, config);
	bdata->sysreg_shadow.diag_mode.block_select = (ADDR_iwGfxData & 0x3ff80) >> 7;
	bdata->sysreg_shadow.diag_mode.reg_select = (ADDR_iwGfxData & 0x78) >> 3;
	bdata->sysreg_shadow.diag_mode.enab_diag_win = 1;
	bdata->sysreg_shadow.diag_mode.immed         = 1;
	PUSH_SHADOWED_SYSREG(bdata, diag_mode);

#endif
	/* 
	** from the memory configuration of the board, determine the
	** address space where the cfifo ISN'T and then tell rts that.
	** `rf won't allow non cfifo writes into the cfifo after this.
	** NOTE: the mem protect token is the only one you will ever find treating
	** sdram as a linear byte array.  Don't be confused by this code!
	*/
	{
		__uint32_t unpriv_bottom, unpriv_top;

		unpriv_bottom =  (ODSY_CFIFO_NR_32BPP_TILES << 8/*16x16 pixels*/) << 5/*32Bpp*/;
		unpriv_top    =  odsyinfo->sdram_size <<20;

#ifdef ODSY_SIM_KERN
		if (odsyinfo->unpriv_bottom != -1)
			unpriv_bottom = odsyinfo->unpriv_bottom;
		if (odsyinfo->unpriv_top    != -1)
			unpriv_top    = odsyinfo->unpriv_top;
#endif
		ODSY_WRITE_HW(hw, cfifo_priv.by_32.w0, ODSY_RASTEX_CMD(MEM_PROTECT,2,0));
		ODSY_WRITE_HW(hw, cfifo_priv.by_32.w0, unpriv_bottom);
		ODSY_WRITE_HW(hw, cfifo_priv.by_32.w0, unpriv_top);
	}

	if (useFlowControl) {
		UNSET_FC_SPLHI(ODSY_INIT_CFIFO);
	}
}


#if !defined(_STANDALONE)
/*
** odsyInitDMA
**
** Set the xtalk ids of the host and any mbuzz slaves/masters.
*/
void odsyInitDMA(int board)
{
	odsy_data *bdata = ODSY_BDATA_FROM_PIPENR(board);

	odsyPushSysConfig(bdata, 1);

#if defined(ODSY_SIM_KERN)
	/*
	** zap any outstanding dma gfx->host range check region
	*/
	bdata->sim.dma_range_check_rgn = 0;
#endif

	MUTEX_INIT(&bdata->dma.lock, MUTEX_DEFAULT,0);
	SV_INIT(&bdata->dma.sv, SV_DEFAULT, 0);


	/* clear the global memory manager counters */
	bdata->mm.kv_global_rgn->last_sync_xfr_sent = 0;
	bdata->mm.kv_global_rgn->last_sync_xfr_done = 0;
	bdata->mm.kv_global_rgn->last_async_xfr_sent = 0;
	bdata->mm.kv_global_rgn->last_async_xfr_done = 0;

	// setup the outstanding dma operation tracking state 
	// We use two of the 32k sw sync write back region slots 
	// for tracking  outstanding synchronous and asynchronous dmas.

	bdata->dma.sync_hw_wr_slot  =  OdsyAlloc32kSlot(&bdata->hw_wr_slots);
	bdata->dma.sync_hw_wr = bdata->mm.kv_hw_wr_rgn + bdata->dma.sync_hw_wr_slot;
	bdata->dma.sync_hw_wr->val = 0xffff;

	bdata->dma.async_hw_wr_slot =  OdsyAlloc32kSlot(&bdata->hw_wr_slots);
	bdata->dma.async_hw_wr = bdata->mm.kv_hw_wr_rgn + bdata->dma.async_hw_wr_slot;
	bdata->dma.sync_hw_wr->val = 0xffff;

#ifdef ODSY_DEBUG_DMA	
	cmn_err(CE_DEBUG,"odsy: dma sync slot %d, async slot %d\n",
		bdata->dma.sync_hw_wr_slot,bdata->dma.async_hw_wr_slot);
	
#endif

	bdata->dma.sync_op_nr_low = bdata->dma.sync_op_nr_high
		= bdata->dma.async_op_nr_low = bdata->dma.async_op_nr_high
		= 0;

	bdata->dma.sync_writep 
		= bdata->dma.async_writep
		= bdata->dma.sync_readp
		= bdata->dma.async_readp
		= 0;
}
#endif	/* !_STANDALONE */

static void odsyPushSysConfig(odsy_data *bdata, int use_buzz_config_flags)
{
	int buzz_nbr = bdata->boardnr;		/* buzz0 or buzz1 */

	if (use_buzz_config_flags) {
		if (Odsy_is_multi_buzz) {
			int other_brdnr = bdata->buzz_config.other_buzz_id;
			int is_slave = !bdata->buzz_config.is_master;

			bdata->sysreg_shadow.config.obuzz_id =
				OdsyBoards[other_brdnr].widget_id;
			bdata->sysreg_shadow.config.gfx_cfg    = 1;
			bdata->sysreg_shadow.config.dma_cfg    = 1;
			bdata->sysreg_shadow.config.dbe_cfg    = 1;
			bdata->sysreg_shadow.config.raster_cfg = 1;
			bdata->sysreg_shadow.config.gfx_slave  = is_slave;
			bdata->sysreg_shadow.config.dma_slave  = is_slave;
		} else {
			bdata->sysreg_shadow.config.obuzz_id   = 0;
			bdata->sysreg_shadow.config.gfx_cfg    = 0;
			bdata->sysreg_shadow.config.dma_cfg    = 0;
			bdata->sysreg_shadow.config.dbe_cfg    = 0;
			bdata->sysreg_shadow.config.raster_cfg = 0;
			bdata->sysreg_shadow.config.gfx_slave  = 0;
			bdata->sysreg_shadow.config.dma_slave  = 0;
		}

		bdata->sysreg_shadow.config.dma_slave  = 0; /* master or slave   */
	} else {
		bdata->sysreg_shadow.config.obuzz_id   = 0;
		bdata->sysreg_shadow.config.gfx_cfg    = 0;
		bdata->sysreg_shadow.config.dma_cfg    = 0;
		bdata->sysreg_shadow.config.dbe_cfg    = 0;
		bdata->sysreg_shadow.config.raster_cfg = 0;
		bdata->sysreg_shadow.config.gfx_slave  = 0;
		bdata->sysreg_shadow.config.dma_slave  = 0;
	}
	bdata->sysreg_shadow.config.dma_id     = buzz_nbr;
	bdata->sysreg_shadow.config.raster_id  = buzz_nbr;
	bdata->sysreg_shadow.config.host_id    = ODSY_XT_HEART_ID;

	bdata->sysreg_shadow.config.swaprdy_en = 0;

	PUSH_SHADOWED_SYSREG(bdata, config);
}

#if !defined(_STANDALONE)
/*
** odsyInitInterrupts
**
** setup the gfx board for interrupts
*/
void odsyInitInterrupts(int board)
{
	odsy_data *bdata = ODSY_BDATA_FROM_PIPENR(board);
	odsy_sysreg_shadow *sshdw = &bdata->sysreg_shadow;


	sshdw->intr_dst_addr_lo = (__uint32_t) XTALK_ADDR_TO_LOWER(bdata->intr.xt_intr_dst_addr);
	PUSH_SHADOWED_XTREG(bdata, intr_dst_addr_lo);

	sshdw->intr_dst_addr_hi.xt_intr_vector  = bdata->intr.xt_intr_vector_num;
	sshdw->intr_dst_addr_hi.resp_intr_enab  = 1;
	sshdw->intr_dst_addr_hi.frame_intr_enab = 1;
	sshdw->intr_dst_addr_hi.addr_intr_enab  = 1;
	// XXX use xtalk_intr_target_get() instead of hardcoding (jeffs)
	sshdw->intr_dst_addr_hi.target_id       = ODSY_XT_HEART_ID;
	sshdw->intr_dst_addr_hi.intr_addr_hi = (__uint32_t) XTALK_ADDR_TO_UPPER(bdata->intr.xt_intr_dst_addr);
	PUSH_SHADOWED_XTREG(bdata, intr_dst_addr_hi);

	sshdw->intr_config.xt_intr_barrier = 1;
	sshdw->intr_config.xt_intr_tnum    = BUZZ_INTR_TNUM;
	sshdw->intr_config.intr_den        = 0xff;
	PUSH_SHADOWED_XTREG(bdata, intr_config);

	sshdw->intr_vector.sw_sync = bdata->intr.ssync_intr_vector_num;
	sshdw->intr_vector.retrace = bdata->intr.vtrace_intr_vector_num;
	sshdw->intr_vector.dma     = bdata->intr.dma_intr_vector_num;
	sshdw->intr_vector.flag    = bdata->intr.flag_intr_vector_num;
	PUSH_SHADOWED_SYSREG(bdata, intr_vector);

	sshdw->intr_ctl_lo = (__uint32_t) XTALK_ADDR_TO_LOWER(bdata->intr.intr_ctl);
	PUSH_SHADOWED_SYSREG(bdata, intr_ctl_lo);

	sshdw->intr_header.retrace_dst_id =
	sshdw->intr_header.dma_dst_id =
	sshdw->intr_header.flag_dst_id = ODSY_XT_HEART_ID;
#ifdef ODSY_SIM_KERN
	sshdw->intr_header.retrace_tnum = BUZZ_INTR_TNUM >> 1;/* XXX don't let funcwhif see this! */
#else
	sshdw->intr_header.retrace_tnum = BUZZ_INTR_TNUM;
#endif
	sshdw->intr_header.dma_tnum =
	sshdw->intr_header.flag_tnum = BUZZ_INTR_TNUM;
	sshdw->intr_header.retrace_barrier =
	sshdw->intr_header.dma_barrier =
	sshdw->intr_header.flag_barrier = 1;
	PUSH_SHADOWED_SYSREG(bdata, intr_header);

	sshdw->intr_ctl_hi.ipt_pend = 1;
	sshdw->intr_ctl_hi.sw_sync_didn = ODSY_XT_HEART_ID;
	sshdw->intr_ctl_hi.sw_sync_tnum = BUZZ_INTR_TNUM;
	sshdw->intr_ctl_hi.sw_sync_barrier = 1;
	sshdw->intr_ctl_hi.vretrace_enab =
	sshdw->intr_ctl_hi.dma_enab =
	sshdw->intr_ctl_hi.sw_sync_enab = 1;
	sshdw->intr_ctl_hi.intr_addr_hi = (__uint32_t) XTALK_ADDR_TO_UPPER(bdata->intr.intr_ctl);
	PUSH_SHADOWED_SYSREG(bdata, intr_ctl_hi);


	// Enable the flags interrupts we care about.
	sshdw->flag_intr_enab_set.gen_lock      = 1;  //XXX get from info whether or not we care.
	sshdw->flag_intr_enab_set.clk_stop      = 1;  // Probably fatal if we aren't expecting it :)
	sshdw->flag_intr_enab_set.rd_dur_diag   = 1;  // Ditto
	sshdw->flag_intr_enab_set.push_cmd_err  = 1;
	sshdw->flag_intr_enab_set.gfx_ovfl      = 1;
	sshdw->flag_intr_enab_set.cfifo_hw_flag = 0;  // Using real fc on octane...
	sshdw->flag_intr_enab_set.dbe_ovfl      = 1;
	sshdw->flag_intr_enab_set.xform_dfifo_ovfl = 1;
	sshdw->flag_intr_enab_set.xform_ififo_ovfl = 1;
	sshdw->flag_intr_enab_set.xform_ucode_ovfl = 1;
	sshdw->flag_intr_enab_set.xtalk_stall      = 1; // Informational
#ifdef ODSY_NO_FC
	sshdw->flag_intr_enab_set.xtalk_stall      = 0; // Can cause bus errors if stalled 
#endif
	sshdw->flag_intr_enab_set.host_flags       = 0; // Mask these for now...
	sshdw->flag_intr_enab_set.priv_violation   = 1;
	sshdw->flag_intr_enab_set.ctxsw_done       = 0; // Don't want to know about this yet.
	sshdw->flag_intr_enab_set.dma_done         = 0;
	sshdw->flag_intr_enab_set.dma_cmd_ovfl     = 1;
	sshdw->flag_intr_enab_set.cfifo_lw         = 0; // Real fc uses this... but will enable later.
	sshdw->flag_intr_enab_set.illegal_out_fmt  = 1;
	sshdw->flag_intr_enab_set.multi_get        = 1;
	sshdw->flag_intr_enab_set.corr_ecc         = 1; // information don't worry about correctables.
	sshdw->flag_intr_enab_set.multi_corr_ecc   = 1; // more info... probably should be warning.
	sshdw->flag_intr_enab_set.uncorr_ecc       = 1;
	sshdw->flag_intr_enab_set.sync_timeout     = 1; 
	sshdw->flag_intr_enab_set.heuristic_ovfl   = 0; // shouldn't be enabled...

	PUSH_SHADOWED_SYSREG(bdata, flag_intr_enab_set);
}
#endif	/* _STANDALONE */

/*
 * odsyInitDBE
 *
 * setup whatever needs to happen back here (i2c, ddc, etc)
 * probably a good idea to download the default timing table.
 */
void odsyInitDBE(odsy_data *bdata,
    odsy_brdmgr_timing_info *tinfo,
    OdsyDoDownloadType download)
{
	int i, wid;
	odsy_hw *hw = bdata->hw;
	buzz_HIF_config_reg buzz_HIF_config;
	pbj_HIF_control_reg  pbj_HIF_control;
	odsy_brdmgr_config_fb_arg k_cfbarg;
	pbj_dpath_control_reg dpcon;
	struct main_mode_reg main_mode;
	struct overlay_mode_reg ovly_mode;

	ODSY_DFIFO_PSEMA(bdata);

	if (ODSY_POLL_DFIFO_LEVEL(hw, ODSY_DFIFO_LEVEL_REQ(6)))
		goto error_return;

	buzz_HIF_config.w32 = 0;
				/* Take PBJ out of reset */
	buzz_HIF_config.bf.pbj_reset = PBJ_RESET_INACTIVE;
	buzz_HIF_config.bf.dcb_clk_mod = DCB_CLK_DIV8;
#if defined(ODSY_SIM_KERN)
	/* double the dbe clock speed (undocumented verif feature) */
	buzz_HIF_config.w32 |= 8;
#endif

	ODSY_WRITE_HW(hw, dbe,
			DBE_CMD1_REG(DBE_UPDATE_IMMED, buzz_HIF_config,
			buzz_HIF_config.w32));

	/*
	 * Unfortunately we have to wait a while (several ody-brd clk
	 * cycles) before we can do a dbe write.
	 */
	for (i = 0; i < 4; i++) {
		ODSY_WRITE_HW(hw, dbe,
			DBE_CMD1_REG(DBE_UPDATE_IMMED, buzz_HIF_config,
			buzz_HIF_config.w32));
	}

	pbj_HIF_control.w32 = 0;
	pbj_HIF_control.bf.pbj_soft_reset_n = 1; /* take out of soft reset */
	pbj_HIF_control.bf.i2c_main_clk_sel = ODSY_PBJ_I2C_CLK_DEFAULT;
	pbj_HIF_control.bf.i2c_opt_clk_sel  = ODSY_PBJ_I2C_OPT_CLK_DEFAULT;
	pbj_HIF_control.bf.osc_en           = 1;
	pbj_HIF_control.bf.i2c_main_pwr_en  = 1;
	pbj_HIF_control.bf.i2c_main_en      = 1;
	pbj_HIF_control.bf.i2c_opt_en       = 1;
	/*
	 * XXX - What is vint_swap_en?  Similar to sys.swap_on_ready_or_vert?
	 */
	pbj_HIF_control.bf.vint_swap_en     = 1;

	ODSY_WRITE_HW(hw, dbe,
			DBE_CMD1_REG(DBE_UPDATE_IMMED, PBJ_HIF_control,
			pbj_HIF_control.w32));

#if defined(ODSY_SIM_KERN)
	bdata->vtg_enable = 0;
	bdata->vtg_initialState = 0;
#else
	// Shadow vtg_enable/initState early.
	if (ODSY_POLL_DFIFO_LEVEL(bdata->hw, 0))
		goto error_return; // flush writes
	ODSY_READ_HW(hw, dbe_diag_rd.VTG_enable, bdata->vtg_enable);
	ODSY_READ_HW(hw, dbe_diag_rd.VTG_initialState, bdata->vtg_initialState);
#endif

	ODSY_DFIFO_VSEMA(bdata);

	/* Set up the framebuffer and cfifo for the textport */

#if ODSY_SIM_KERN
	k_cfbarg.width  = 512;
	k_cfbarg.height = 512;
#else
	k_cfbarg.width  = 1280;
	k_cfbarg.height = 1024;
#endif
	k_cfbarg.depth  = 8;
	k_cfbarg.acbuf_type = ODSY_ACCUM_NONE;
	odsyConfigFb(bdata, &k_cfbarg);

#ifndef TB6_HACKERY
	if (download == DoDownload)
		odsyLoadTimingTable(bdata, tinfo);
#endif
#ifdef ODSY_SIM_KERN
	dpcon.w32 = 0;
	dpcon.bf.extdacen = 1;
	ODSY_WRITE_HW(hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED,
		      PBJ_dpath_ctl, dpcon.w32));
#endif

	/* Load the display mode for the textport */
	main_mode.pixel_field_format = MM_PFF_B8_FMT_DE;
	main_mode.pixel_display_format = MM_PDF_CI8;
	main_mode.clut_base = 0;
	main_mode.gamma_comp = 0;
	main_mode.true_color = 0;
	main_mode.stereo = 0;
	main_mode.reserved = 0;

	ODSY_DFIFO_PSEMA(bdata);

	/* Load all the main modes so we don't need to set up wid planes */
	for (wid = 0; wid < ODSY_NUM_DISPLAYMODES_MAIN; wid++) {
		if ((wid % ODSY_DFIFO_DWORDS) == 0) {
			if (ODSY_POLL_DFIFO_LEVEL(hw, 0))
				goto error_return;
		}
		ODSY_WRITE_HW(hw, dbe,
			      DBE_CMD1_DBEADDR(DBE_UPDATE_IMMED,
					       DBE_ADDR(XMAP_mainMode) + wid,
					       *(unsigned*) &main_mode));
	}

	ovly_mode.enable = 0;
	ovly_mode.reserved = 0;
	ovly_mode.opaque = 0;
	ovly_mode.clut_base = 0;
#ifdef ODSY_TEXTPORT_IN_OVERLAY
	ovly_mode.enable = 1;
	ovly_mode.opaque = 1;
#endif

	/* Disable the overlay planes */
	for (wid = 0; wid < ODSY_NUM_DISPLAYMODES_OVLY; wid++) {
		if ((wid % ODSY_DFIFO_DWORDS) == 0) {
			if (ODSY_POLL_DFIFO_LEVEL(hw, 0))
				goto error_return;
		}
		ODSY_WRITE_HW(hw, dbe,
			      DBE_CMD1_DBEADDR(DBE_UPDATE_IMMED,
					       DBE_ADDR(XMAP_ovlyMode) + wid,
					       *(unsigned*) &ovly_mode));
	}

	/* Reset XMAP stuff */
	ODSY_WRITE_HW(hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, XMAP_config, 0));
	ODSY_WRITE_HW(hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, XMAP_redOverride, 0));
	ODSY_WRITE_HW(hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, XMAP_greenOverride, 0));
	ODSY_WRITE_HW(hw, dbe, DBE_CMD1_REG(DBE_UPDATE_IMMED, XMAP_blueOverride, 0));

	ODSY_DFIFO_VSEMA(bdata);

	/* =================================================== */
	/* additional, post-asbrown initialization for textport */
	/* =================================================== */
#if TEXTPORT_BRINGUP && 0
       {// don't know if this is necessary
	static odsy_gctx foo;
	static odsy_sw_shadow bar;static odsy_hw_shadow baz;
	static struct rrm_rnode rn;
	static struct gfx_gfx gg;
	odsy_gctx *gctx;

	gctx = &foo;    
	gctx->kv_sw_shadow = &bar;
	gctx->hw_shadow = &baz;
	gctx->rnp = &rn;
	rn.gr_gfxp = &gg;
	rn.gr_bdata = (struct gfx_data *)bdata;
	gg.gx_bdata = (struct gfx_data *)bdata;
	// pretend to be an opengl ctx
	gctx->hw_ctx_id = ODSY_BRDMGR_HW_CONTEXT_ID + 1;
	bdata->last_opengl_hw_ctx_id = ODSY_BRDMGR_HW_CONTEXT_ID;
	odsyRestoreRaster(bdata, gctx);
       }
#endif
#if TEXTPORT_BRINGUP && 0
       {// don't know if this is necessary
	// snippets from odsyInitDefaultHW

	int i;
	__BUZZpackets buzz;
	__BUZZpackets *sw_shadow = &buzz;
	/* Make sure ctx sw shadow is clean */
	bzero(&buzz, sizeof(__BUZZpackets));

	buzz.xform_register[XFORMADDR_clLightSize] = CL_LightSize;
	buzz.xform_register[XFORMADDR_pmLightSize] = PARAM_Lightsize;
	buzz.xform_register[XFORMADDR_clFmt0] = 0x0;
	buzz.xform_register[XFORMADDR_clFmt1] = 0x2;
	buzz.xform_register[XFORMADDR_clFmt2] = 0xc;
	buzz.xform_register[XFORMADDR_clFmt3] = 0xe;
	buzz.xform_register[XFORMADDR_s3MaxWExp] = 0; /* never referenced ? */
	buzz.xform_register[XFORMADDR_UAddr] = 0;
	buzz.xform_register[XFORMADDR_TableAddrs] = 0;
	buzz.xform_register[XFORMADDR_VertexFuncAddr] = uFastUnlit_start;
	buzz.xform_register[XFORMADDR_VertexLoopAddr] = uFastUnlit_loop;
	buzz.xform_register[XFORMADDR_VertexCopyCount] = 0; /* never referenced ? */
	buzz.xform_register[XFORMADDR_VertexLoopAltAddr] = uFastUnlit_exit;
	buzz.xform_register[XFORMADDR_DMAcount] = 0; /* never referenced ? */
	buzz.xform_register[XFORMADDR_OfifoHighWater] = 0x380;     /* 7/8 of fifo */
	buzz.xform_register[XFORMADDR_EdgeFlagSet] = 0;
	buzz.xform_register[XFORMADDR_EdgeFlagReset] = 0;
	buzz.xform_register[XFORMADDR_pmLightBase] = PARAM_LIGHT0;
	buzz.xform_register[XFORMADDR_clLightBase] = CL_LIGHT0;
	buzz.xform_register[XFORMADDR_TableAddrsBase] = 0;
	buzz.xform_register[XFORMADDR_RegFileBase] = 0;
	buzz.xform_register[XFORMADDR_clMatCopyEnable] = 0;

	for(i=XFORMADDR_clLightSize;i<=XFORMADDR_clMatCopyEnable;i++){
	    if ((i!= XFORMADDR_DMAcount) && (i!= XFORMADDR_VertexCopyCount)
		/*&& (i!= XFORMADDR_EdgeFlagSet) && (i!=XFORMADDR_EdgeFlagReset)*/
		&& (i!= XFORMADDR_s3MaxWExp))
		    BUZZ_RegLoad(i,buzz.xform_register[i]);
	}

	/* set the enable others fields required for PIO data */
	TEX_MODE_TEX_BASE_FMT_PUT(buzz.tex_mode.flags, TEX_MODE_TEX_BASE_FMT_RGBA)
	BUZZ_CMD_TEX_MODE();
	TEX_ENV_TEX_ENV_PUT(buzz.tex_env.op, TEX_ENV_TEX_ENV_GL_REPLACE);
	BUZZ_CMD_TEX_ENV();
	EN_OTHERS_MP_TEX_ENV1_PUT(buzz.en_others, EN_OTHERS_MP_TEX_ENV1_TED);
	EN_OTHERS_MP_LGT_ENV_PUT(buzz.en_others, EN_OTHERS_MP_LGT_ENV_LET);
	BUZZ_CMD_ENABLE_OTHERS();
	buzz.tf_alpha_mode = __BUZZ_TF_ALPHA_MODE_RGBA;
	BUZZ_CMD_TF_ALPHA();
	buzz.tf_gain_mode = __BUZZ_TF_GAIN_MODE_RGBA;
	buzz.tf_gain_a.gain_red = buzz.tf_gain_a.gain_green =
	buzz.tf_gain_a.gain_blue = buzz.tf_gain_a.gain_alf =
	buzz.tf_gain_b.gain_red = buzz.tf_gain_b.gain_green =
	buzz.tf_gain_b.gain_blue = buzz.tf_gain_b.gain_alf =
	buzz.tf_gain_c.gain_red = buzz.tf_gain_c.gain_green =
	buzz.tf_gain_c.gain_blue = buzz.tf_gain_c.gain_alf =
	buzz.tf_gain_d.gain_red = buzz.tf_gain_d.gain_green =
	buzz.tf_gain_d.gain_blue = buzz.tf_gain_d.gain_alf = 0x1000;
	BUZZ_CMD_TF_GAIN();

	/* set polygon mode */
	buzz.poly_mode = 0x2; /*fill*/
	POLY_MODE_FRONT_MODE_PUT(buzz.poly_mode,0x2);
	POLY_MODE_BACK_MODE_PUT(buzz.poly_mode,0x2);
	BUZZ_CMD_POLY_MODE();

	/* set line and point width */
	buzz.line_width = 0x80;
	buzz.point_size = 0x80;
	BUZZ_CMD_WIDTH();

	/*
	* Set buffer select for front buffers
	*/
	buzz.buffer_select = 0x00000003;
	BUZZ_CMD_BUFFER_SELECT();

	/* set raster mode */
	buzz.raster_mode = 0;
	RASTER_MODE_SGN_PROJ_MAT23_PUT(buzz.raster_mode, 0x1);
	BUZZ_CMD_RASTER_MODE();

	/* set multifrag token */
	buzz.multifrag = 0;
	BUZZ_CMD_MULTIFRAG();
       }
#endif

	/* Enable Display */
	odsyBlankScreen(bdata, 0);
	return;

error_return:
	ODSY_DFIFO_VSEMA(bdata);
}


#if !defined(_STANDALONE)
#define ALIGNMASK ((1<<16)-1) /* 64Kb */
/* 
** allocate the hardware write region (where sw syncs write back to).  there
** is an alignment constraint that the region be naturally (i.e. 64Kb, for its size) 
** aligned.  so we allocate twice that and use the extra space as a page cache for
** use during allocation of ctxsw write-back regions...  we also use one page
** for the globally-visible memory manager data.
*/
void odsyInitHwWrRgn(int board)
{
	int i;
	odsy_data *bdata = &OdsyBoards[board];
	static void *hw_wr_rgns[ODSY_MAXBOARDS] = {0,0};

	/*
	** XXXadamsk: do this to keep from ever creating the region twice, just in case.
	** if i understood the init sequences better this might be unnecessary.
	** NOTE: this is 64Kb given 15 bits of double-byte offsets.
	*/
	if(hw_wr_rgns[board] == 0) {
#if !defined(_STANDALONE)
		hw_wr_rgns[board] =  
			kmem_zalloc( 2 * ODSY_HW_WR_RGN_BYTES, /* enough to guarantee we can get 64Kb aligned */
				     KM_SLEEP|KM_PHYSCONTIG|KM_CACHEALIGN);
#else	/* _STANDALONE */
		hw_wr_rgns[board] =  
			align_malloc( 2 * ODSY_HW_WR_RGN_BYTES,
				      128 /* 128byte cache alignment */);
		bzero(hw_wr_rgns[board], 2 * ODSY_HW_WR_RGN_BYTES);
#endif	/* !_STANDALONE */
	}



	bdata->mm.kv_hw_wr_rgn = 0;
	for (i=0; i < ODSY_HW_SHADOW_CACHE_PAGES; i++ ) {
		bdata->hw_shadow_cache_page_in_use[i] = 0;
		bdata->hw_shadow_cache_pages[i]        = 0;
	}

	if (!hw_wr_rgns[board]) {
		printf("odsy: couldn't allocate the sw sync wr/hw shadow cache for odsy board %d\n",board);
	}

	{
		__uint64_t cptr        = (__uint64_t)(void*)hw_wr_rgns[board];
		int    ccache_nr   = 0;

		ASSERT(((1<<16)%_PAGESZ) == 0);  /* silly, no? but it is an assumption :)   */

		/* fill in the cache pages until we get aligned */
		while ( cptr & ALIGNMASK ) {
#if defined(DEBUG) 
			printf("odsy: hw shadow cache pointer %d 0x%x\n",ccache_nr,cptr);
#endif
			bdata->hw_shadow_cache_pages[ccache_nr++] = (void*)cptr;
			cptr += _PAGESZ;
		}

		/* we're aligned, record the pointer */
		bdata->mm.kv_hw_wr_rgn = (odsy_mm_hw_wr_slot *)cptr;
#if defined(DEBUG) 
		printf("odsy: hw wr region for odsy board nr %d is at 0x%x\n",board,bdata->mm.kv_hw_wr_rgn);
#endif 

		/* fill in the rest of the cache pages */
		cptr += ODSY_HW_WR_RGN_BYTES;
		for ( ; ccache_nr < ODSY_HW_SHADOW_CACHE_PAGES; ccache_nr ++ ) {
#if defined(DEBUG) 
			printf("odsy: hw shadow cache pointer %d 0x%x\n",ccache_nr,cptr);
#endif
			bdata->hw_shadow_cache_pages[ccache_nr] = (void*)cptr;
			cptr += _PAGESZ;			
		}
	}
}
#undef ALIGNMASK


/*
**
*/
void odsy_teardown(odsy_data *bdata)
{
	struct GfxKiller_args GfxKiller_args;
	int s;

	psema(&bdata->teardown_sema,PZERO);

	if (++bdata->abnormal_teardown != 1){
		vsema(&bdata->teardown_sema); 
		return;
	}

	cmn_err(CE_ALERT,"Graphics error");

	odsyPrintRegs(bdata->hw);

#ifdef ODSY_DBG_ON_TEARDOWN
	// Turn this on to catch gfx crashes a bit earlier.
	if (kdebug) {
		conbuf_flush(CONBUF_UNLOCKED|CONBUF_DRAIN);
		debug("ring");
	}
#endif

	/*XXX: take any necessary sdram/history loggin' snapshots here? */

	/*
	** Kill all graphics processes on this board.
	*/
	GfxKiller_args.signal = SIGKILL;
	GfxKiller_args.bdata = (struct gfx_data *) bdata;
	GfxKiller_args.gthd = NULL;
	gthread_scan(GfxKiller,&GfxKiller_args);

	/* Wake up all apps waiting for swap-finish, immediately */
	/*OdsyValidateAllRetrace(bdata);*/

#if defined(ODSY_SIM_KERN)
	/* wake up anyone waiting for a symbiote event */
	s = LOCK(&bdata->sim.pio_buffer_lock, PL_ZERO);
	sv_signal(&bdata->sim.pio_wait_for_symb_event_sv);
	UNLOCK(&bdata->sim.pio_buffer_lock, s);
#endif	
	/* 
	** no need to reset the board here.  we won't allow
	** a reinitialize until all ddrn are released.
	*/

	vsema(&bdata->teardown_sema);
}


#else /* _STANDALONE */


static void
odsyFillBoardID(odsy_info *pinfo, char *idbuf)
{
	DPUTS("odsyFillBoardID\n\r");
	strcpy(idbuf, "SGI-ODYSSEY" );
}

/*
 * return 1 if OdsyBoards[board] is found 
 */
int
odsyProbe(int board)
{
	DPUTS("odsyProbe\n\r");
	return((odsyFindBoardAddr(board)) ? 1 : 0);
}

static void
odsyBdSetup(void)
{
	int board;
	static int bdSetup;

	if (bdSetup)
	    return;
	bzero((void*) &OdsyBoards, sizeof(odsy_data) * ODSY_MAXBOARDS);
	for (board=0; board < ODSY_MAXBOARDS; board++) {
	    /*
	     * first time we're probing set board_found to -1
	     */
	    OdsyBoards[board].board_found = -1;
	}
	bdSetup = 1;
}

/*
 * called by probe_graphics_base, for power on diag
 * and odsy_install for prom boot init
 * zero the base[]
 * with the odsy boards found and the
 * base ptr to the odsy hw set in OdsyBoards[].base
 * by calling odsyProbe for each possbile board slot
 */
int
odsyProbeAll(char *base[])
{
	odsy_hw *hw;
	odsy_info *info;
	int board;		/* scratch board index */
	int bd = 0;		/* number of odsy boards found */

	DPUTS("odsyProbeAll\n\r");
	odsyBdSetup();

	for (board=0; board < ODSY_MAXBOARDS; board++) {

	    if (hw = odsyFindBoardAddr(board)) {
		/*
		 * return probe results to pass to power-on diags.
		 */
		base[bd++] = (char *) hw;
		/*
		 * standalone resetcons calls odsy_probe expecting that
		 * odsyReset might be called. So here we unconditionally
		 * call odsyInitBoard, which call odsyReset.
		 */
		OdsyNrBoards++;
		odsyInitBoard(board);
	    }
	}

	return bd;
}

/*
 * Probe routine set by .cf file and used by the textport.  The tp calls
 * with fncs=0 to find base addresses.  Each call with fncs=0 should return
 * the base of the next board.  We return 0 to indicate all the boards
 * have been reported.  If fncs is != 0, we return the functions for the
 * graphics board.
 */
void *
odsy_probe(struct htp_fncs **fncs)
{
	static char *base[ODSY_MAXBOARDS];
	static int i;

	DPUTS("odsy_probe\n\r");

	if (fncs) {
	    *fncs = (struct htp_fncs *)&odsy_htp_fncs;
	    i = 0;
	    return((void *) 0);
	}
	if (i == 0) {
	    bzero(base, sizeof(base));
	    odsyProbeAll(base);
	}
	if (i < ODSY_MAXBOARDS)
	    return((void *) base[i++]);
	else
	    return((void *) 0);
}

extern COMPONENT montmpl;
extern int _gfx_strat();
/*
 * Re-probe for ODYSSEY graphics boards and add them to the hardware config
 */
int
odsy_install(COMPONENT *root)
{
	char idbuf[100];
	struct odsy_hw *pmg;
	struct odsy_info *pinfo;
	extern int gfxboard;
	COMPONENT *tmp, odsytmpl;
	int idx;

    DPUTS("odsy_install\n\r");

	odsytmpl.Class = ControllerClass;
	odsytmpl.Type = DisplayController;
	odsytmpl.Flags = ConsoleOut|Output;
	odsytmpl.Version = SGI_ARCS_VERS;
	odsytmpl.Revision = SGI_ARCS_REV;
	odsytmpl.Key = 0;
	odsytmpl.AffinityMask = 0x01;
	odsytmpl.ConfigurationDataSize = 0;
	odsytmpl.Identifier = idbuf;
	odsytmpl.IdentifierLength = 100;

	/*
	 * for reboot types where the pon sequence dont
	 * happen we need to make sure the board re-init
	 * sequence happens, so setup the board struct such
	 * that we go thru the init all over again.
	 */

	odsyBdSetup();

	for (idx=0; idx < ODSY_MAXBOARDS; idx++)
    {
        if( odsyFindBoardAddr( idx ) != NULL )
        {
            // OdsyNrBoards++;
            // odsyInitBoard(idx);

    		pinfo = &OdsyBoards[idx].info;
	    	tmp = AddChild(root, &odsytmpl, (void*) NULL);
		    if (tmp == (COMPONENT*) NULL)
		        cpu_acpanic("odyssey");
    		tmp->Key = gfxboard++;

	    	odsyFillBoardID(pinfo, tmp->Identifier);
		    tmp->IdentifierLength = strlen(tmp->Identifier);

    		RegisterDriverStrategy(tmp, _gfx_strat);

            /*  This is where we would identify the monitor.  Impact
             *  does not list a monitor, so we won't either.
             */

            /*  If it is needed, this will add it to the ARCS tree
    		 * if ( 1 )
             * {
	    	 *  	tmp = AddChild(tmp, &montmpl, (void*)NULL);
		     * 	if (tmp == (COMPONENT*)NULL)
			 *     	cpu_acpanic("monitor");
    		 *	tmp->Identifier = "unknown monitor type";
	    	 *	tmp->IdentifierLength = strlen(tmp->Identifier);
             * }
             */ 
	    }
	}
	return(0);
}

/*
 * For all the possible gfx slots on IP30 look for buzz
 * returns: kvaddr to the base of the xwidget registers of board
 *	    or NULL if not odsy board.
 */
odsy_hw *
odsy_xtalk_probe(int part, int mfg, int board)
{
#if IP30
	xbowreg_t xport;
	widget_cfg_t *base = (widget_cfg_t *)NULL;
	__uint32_t xt_mfg;
	__uint32_t xt_part;
	__uint32_t xt_widget_id;
	__uint32_t xt_control;
	xbow_t *xbp = (xbow_t *)K1_MAIN_WIDGET(XBOW_ID);

	for (xport = XBOW_PORT_F; xport > XBOW_PORT_8; xport--) {
	    base  = (widget_cfg_t *) K1_MAIN_WIDGET(xport);

	    if (xtalk_probe(xbp, xport)) {
		xt_widget_id = base->w_id;
		xt_mfg  = XWIDGET_MFG_NUM(xt_widget_id);
		xt_part = XWIDGET_PART_NUM(xt_widget_id);

		if (xt_mfg == mfg && xt_part == part) {
		    DPUTS("odsy_xtalk_probe found device: mfg=");
		    DPUTHEX(mfg);
		    DPUTS(" part=");
		    DPUTHEX(part);
		    DPUTS(" board=");
		    DPUTHEX(board);
		    DPUTS("\n\r");
		    /*
		     * the board value indicates we're looking for
		     * nth instance of odsy board so ignore the ones
		     * below the orginal board value
		     */
		    if (board == 0) {
			init_xbow_credit(xbp, xport, ODSY_BUZZ_XT_CREDIT);
			return((odsy_hw *)base);
		    }
		    board--;
	        } 
	    }
	} /* for */
	return(NULL);
#else	/* !IP30 */
#error odsy_xtalk_probe() need for $PRODUCT
#endif
}

#endif  /* _STANDALONE */

#if !defined( _STANDALONE )

void odsyPrintRegs(odsy_hw *hw)
{
	__uint32_t status0;
	__uint32_t flags;

	ODSY_READ_HW(hw,sys.status0,status0);
	ODSY_READ_HW(hw,sys.flag_set_priv,flags);

	cmn_err(CE_DEBUG,"odsy flags: %R\n",flags,flag_desc);
	cmn_err(CE_DEBUG,"odsy status0: %R\n",status0,status0_desc);
}

#endif	/* !_STANDALONE */
