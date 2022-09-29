/*
 * $Header: /proj/irix6.5.7m/isms/stand/arcs/lib/libsk/graphics/MGRAS/RCS/mgras_init.c,v 1.75 1998/09/17 22:49:17 macko Exp $
 */
#include "sys/types.h"	/* types.h must be before systm.h */
#include "sys/systm.h"
#include "sys/param.h"
#include "sys/sema.h"
#include "sys/cpu.h"
#include "sys/invent.h"
#include "string.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/mips_addrspace.h"
#include "sys/edt.h"
#include "sys/nic.h"

#ifdef _STANDALONE
# include "libsc.h"
# include "libsk.h"
# ifdef SN
#  include "libkl.h"
# endif
# include "cursor.h"
# include "stand_htport.h"
void pon_puts(char *);
void pon_puthex(__uint32_t);
#else
# include "sys/htport.h"
# include "sys/nic.h"
# include "sys/errno.h"
#endif /* STANDALONE */

#ifdef SN
# include <sys/SN/addrs.h>
#endif

# if HQ4
#  include <sys/xtalk/xtalk.h>
#  include <sys/xtalk/xbow.h>
# endif

#include "sys/mgrashw.h"
#include "sys/mgras.h"

#include "mgras_internals.h"
#include "mgras_timing.h"

#if HQ4
#define HQ4_BRINGUP_HACKS 1
#else
#define HQ4_BRINGUP_HACKS 0
#endif

#if defined (FLAT_PANEL)
#include "sys/fpanel.h"
void mgras_fp_probe(mgras_hw *base, struct mgras_info *info);
#endif /* FLAT_PANEL */

mgras_info MgrasInfos[MGRAS_MAXBOARDS];

# if HQ4
#  define INITIAL_BOARD0_ADDR	NULL
#  define INITIAL_BOARD1_ADDR	NULL
# else
#  define INITIAL_BOARD0_ADDR	PHYS_TO_K1(MGRAS_BASE0_PHYS)
#  define INITIAL_BOARD1_ADDR	PHYS_TO_K1(MGRAS_BASE1_PHYS)
# endif

#ifdef _STANDALONE

int		MgrasBdsInitted;
mgrasboards	MgrasBoards[MGRAS_MAXBOARDS];
mgrasboards	MG_nonvolatile_aggregate[MGRAS_MAXBOARDS] = {

#else /* !_STANDALONE */

mgrasboards	MgrasBoards[MGRAS_MAXBOARDS] = {

#endif /* STANDALONE */
    { (mgras_hw *) INITIAL_BOARD0_ADDR, NULL, &MgrasInfos[0], 0, 0, 0,
#if HQ4
       -1, NULL,
#else
       GIO_SLOT_GFX, PCON_CLR_SG_RETRACE_N
#endif
    },
    { (mgras_hw *) INITIAL_BOARD1_ADDR, NULL, &MgrasInfos[1], 0, 0, 0,
#if HQ4
       -1, NULL,
#else
       GIO_SLOT_0, PCON_CLR_S0_RETRACE_N,
#endif
    }
};

struct mgras_timing_info *mgras_video_timing[MGRAS_MAXBOARDS];
static __uint32_t VC3_display_control;

#ifdef _STANDALONE

extern unsigned char vc1_ptr_bits[];
extern unsigned char vc1_ptr_ol_bits[];

#else /* !_STANDALONE */

/* For user memory locking limit */
extern unsigned int MgrasMaxLockableMemPages;
extern unsigned int MgrasCurrLockedMemPages;
extern int MgrasLockedBoundaryMsgSent;
extern unsigned int MgrasXMaxLockableMemPages;
extern unsigned int MgrasXCurrLockedMemPages;
extern int gfxlockablemem;			/* Tunable parameter */
extern int gfxlockablemem_gt64;                 /* Tunable parameter */

#endif /* STANDALONE */

/* some prototypes */
#ifdef _STANDALONE
int MgrasProbeAll(char *[]);
#else
static void MgrasInitBoardGfx(mgras_hw *, mgras_info *, int);
#if HQ4
static void MgrasSetHQ4IntrVectors(mgras_data *);
void mgras_pcd_write(mgras_hw *, uchar_t *);
#else
static void MgrasSetGioVectors(int );
#endif /* HQ4 */
#endif /* !_STANDALONE */

int MgrasVerifyVC3SRAM(register mgras_hw *,
		       unsigned short *, 
		       unsigned int,
		       unsigned int);
int MgrasInitRasterBackend(mgras_hw *base, mgras_info *info);
void MgrasInitCMAP(mgras_hw *);
/* static void MgrasTextureMgrInitFixup(void); */
void MgrasSetDacTiming(mgras_hw *, struct mgras_timing_info *);
void MgrasSetVC3Timing(mgras_hw *, struct mgras_timing_info *);
void MgrasSetXMAPTiming(mgras_hw *, mgras_info *, struct mgras_timing_info *);
void MgrasSetRSSTiming(mgras_hw *, mgras_info *, struct mgras_timing_info *);
void MgrasRecordTiming(mgras_hw *, struct mgras_timing_info *);


/******************************************************************************
 * P R O M - O N L Y   I N I T I A L I Z A T I O N   R O U T I N E S
 *****************************************************************************/
#ifdef _STANDALONE

static void
MgrasFillBoardID(struct mgras_info *pinfo, char *idbuf)
{
    strcpy(idbuf, "SGI-" );
    if (pinfo->ProductID == MGRAS_PRODUCT ||
	pinfo->ProductID == MGRAS_PRODUCT_MG10) {
	if (pinfo->NumGEs == 1 &&
	    pinfo->NumREs == 1 &&
	    pinfo->NumTRAMs == 0)
	    strcat(idbuf,"Solid Impact");
	else if (pinfo->NumGEs == 1 &&
		 pinfo->NumREs == 1 &&
		 pinfo->NumTRAMs != 0)
	    strcat(idbuf,"High Impact");
	else if (pinfo->NumGEs == 2 &&
		 pinfo->NumREs == 1 &&
		 pinfo->NumTRAMs != 0)
	    strcat(idbuf,"High-AA Impact");
	else if (pinfo->NumGEs == 2 &&
		 pinfo->NumREs == 2 &&
		 pinfo->NumTRAMs != 0)
	    strcat(idbuf,"Maximum Impact");
	else
	    strcat(idbuf,"Unknown Impact" );
	
	if (pinfo->NumTRAMs > 1)
	    strcat(idbuf,"/TRAM option card" );
    } else if (pinfo->ProductID == MGRAS_PRODUCT_GM10 ||
	       pinfo->ProductID == MGRAS_PRODUCT_GM20) {
#if IP30
	if (pinfo->GE11Rev > 2)
	    strcat(idbuf, "E");
#endif
	if (pinfo->NumGEs == 1 &&
	    pinfo->NumREs == 1 &&
	    pinfo->NumTRAMs == 0)
	    strcat(idbuf,"SI");
	else if (pinfo->NumGEs == 1 &&
		 pinfo->NumREs == 1 &&
		 pinfo->NumTRAMs != 0)
	    strcat(idbuf,"SI with texture option");
	else if (pinfo->NumGEs == 2 &&
		 pinfo->NumREs == 2 &&
		 pinfo->NumTRAMs == 0)
	    strcat(idbuf,"SSI");
	else if (pinfo->NumGEs == 2 &&
		 pinfo->NumREs == 2 &&
		 pinfo->NumTRAMs != 0)
	    strcat(idbuf,"MXI");
	else
	    strcpy(idbuf,"SGI-Unknown Impact" );
    }
}

/* The IP27 IO PROM is quite a different beast from the IP30 prom, so
 * very little PROM initialization code is shared between platforms.
 */
#ifndef IP27

int
MgrasInit(mgras_hw *base, struct mgras_info *info)
{
    int board = mgras_baseindex(base);
    bcopy((char *) info, (char *) MgrasBoards[board].info, sizeof(mgras_info));
    mgrasInitBoard(base, board);
    return 0;
}

#if HQ4
/*
 * MgrasFindBoardAddr - return NULL if board not found.
 *	Has side effect of setting:	MgrasBoards[].base,
 *					MgrasBoards[].boardFound.
 * XXX - move core early prober into IP30.c add slot driver stuff.
 *	??? - dcs
 */
static mgras_hw *
MgrasFindBoardAddr(int which)
{
    xbowreg_t       in_port;
    __uint32_t      found = 0;
    __uint32_t      xt_control;
    widget_cfg_t *  base = (widget_cfg_t *)NULL;
    __uint32_t      mfg, /* rev, */ part, xt_widget_id;
    xbow_t	    *xbow_wid = (xbow_t *)K1_MAIN_WIDGET(XBOW_ID);

    /*
     * XXX there are some systems which have no xbow,
     * for the time being we need to support these front planes
     * by not probing the xbow registers, otherwise we panic
     */
    if ((HEART_PIU_K1PTR)->h_status & HEART_STAT_DIR_CNNCT)
	return (mgras_hw *)NULL;

    for ( in_port = XBOW_PORT_F ; in_port > XBOW_PORT_8 ; in_port-- ) {
	base  = (widget_cfg_t *) K1_MAIN_WIDGET(in_port);
	    
	if ( xtalk_probe(xbow_wid, in_port)) {
	    xt_widget_id = base->w_id;
	    mfg  = XWIDGET_MFG_NUM(xt_widget_id);
/*	    rev  = XWIDGET_REV_NUM(xt_widget_id);  not needed for match */
	    part = XWIDGET_PART_NUM(xt_widget_id);

	    if (mfg == HQ4_XT_ID_MFG_NUM_VALUE && part == HQ4_XT_ID_PART_NUM_VALUE) {
		found++;
		if (!MgrasBoards[which].boardFound) {
		    xbowreg_t link_control;
		    /* initialize credit field in the xbow link(x) control register */
		    link_control = xbow_wid->xb_link(in_port).link_control;
		    link_control &= ~XB_CTRL_WIDGET_CR_MSK;
		    link_control |= HQ4_XT_CREDIT << XB_CTRL_WIDGET_CR_SHFT;
		    xbow_wid->xb_link(in_port).link_control = link_control;
		}
		if (found > which) {
		    MgrasBoards[which].base = (mgras_hw *) base;
		    MgrasBoards[which].xbow_port = in_port;
		    MgrasBoards[which].boardFound = 1;
		    
		    /* 
		     * Save the control register so we can restore
		     * the value set by the prom in MgrasReset
		     */
		    MgrasBoards[which].w_control = ((mgras_hw *)base)->xt_control;
		    return (mgras_hw *) base;
		}
	    }
	}

    } 
    MgrasBoards[which].boardFound = 0;
    return (mgras_hw *) NULL;
}
#endif /* HQ4 */

int
MgrasProbeAll(char *base[])			/* standalone */
{
	mgras_hw *hw;
	mgras_info *info;
	int board, bd = 0;

	if (!MgrasBdsInitted) {
	    MgrasBdsInitted = 1;
	    bcopy((char *)MG_nonvolatile_aggregate, (char *)MgrasBoards,
	    sizeof(mgrasboards) * MGRAS_MAXBOARDS);
	}

	for (board = 0; board < MGRAS_MAXBOARDS; board++) {
	    /* For HQ4, MgrasBoards[board].base = NULL */
	    hw = MgrasBoards[board].base;
	    if (MgrasProbe(hw, board)) {
		hw = MgrasBoards[board].base;
		/*
		 * For standalone --
		 * Want to return probe results to pass to power-on diags.
		 */
		base[bd++] = (char *) hw;

		/*
		 * standalone resetcons calls mgras_probe expecting that
		 * MgrasReset will be called.  So here we unconditionally
		 * call mgrasInitBoard, which will call MgrasReset.
		 */
		mgrasInitBoard(hw, board);

		/* XXX - check out the following ... */
		/* The Indigo2 logo does not fit on the small screen */
		info = MgrasBoards[board].info;
		if (info->gfx_info.xpmax < 1280 || info->gfx_info.ypmax < 1024) {
			logoOff();
		}
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
mgras_probe(struct htp_fncs **fncs)		/* standalone */
{
	static char *base[MGRAS_MAXBOARDS];
	static int i;
#ifdef MFG_USED
extern	char *getenv(const char *);
	char *p = getenv("diagmode");

	if (p && (p[0] == 'N') && (p[1] == 'I')) {
		printf("\n\
		===============================================================\n\
		**WARNING: GFX KERNEL IS SKIPPING INITIALIZATION **\n\
		**WARNING: THE NVRAM VARIABLE diagmode IS SET TO NI**\n\
		This IDE must be used ONLY to repair IMPACT boards\n\
		To load USUAL IDE, go to PROM, unsetenv diagmode and re-boot IDE\n\
		===============================================================\n\n");
		return 0;
	}
#endif
	if (!MgrasBdsInitted) {
	    MgrasBdsInitted = 1;
	    bcopy((char *) MG_nonvolatile_aggregate, (char *) MgrasBoards,
	    sizeof(mgrasboards) * MGRAS_MAXBOARDS);
	}

	if (fncs) {
		*fncs = (struct htp_fncs *)&mgras_htp_fncs;
		i = 0;
		return (void *) 0;
	}

	if (i == 0) {
		bzero(base, sizeof(base));
		(void) MgrasProbeAll(base);
	}
	if (i < MGRAS_MAXBOARDS)
		return (void *) base[i++];
	else
		return (void *) 0;
}

extern COMPONENT montmpl;
extern int _gfx_strat();
/*
 * Re-probe for MGRAS graphics boards and add them to the hardware config
 */
void mgras_install(COMPONENT *root)		/* standalone */
{
	char idbuf[100];
	struct mgras_hw *pmg;
	struct mgras_info *pinfo;
	extern int gfxboard;
	COMPONENT *tmp, mgrastmpl;
	int idx;

#ifdef MFG_USED
extern	char *getenv(const char *);
	char *p = getenv("diagmode");

	if (p && (p[0] == 'N') && (p[1] == 'I')) {
		printf("\n\
		===============================================================\n\
		**WARNING: GFX KERNEL IS SKIPPING INITIALIZATION **\n\
		**WARNING: THE NVRAM VARIABLE diagmode IS SET TO NI**\n\
		This IDE must be used ONLY to repair IMPACT boards\n\
		To load USUAL IDE, go to PROM, unsetenv diagmode and re-boot IDE\n\
		===============================================================\n\n");
		return ;
	}
#endif

	if (!MgrasBdsInitted) {
	    MgrasBdsInitted = 1;
	    bcopy((char *) MG_nonvolatile_aggregate, (char *) MgrasBoards,
	    sizeof(mgrasboards) * MGRAS_MAXBOARDS);
	}

	mgrastmpl.Class = ControllerClass;
	mgrastmpl.Type = DisplayController;
	mgrastmpl.Flags = ConsoleOut|Output;
	mgrastmpl.Version = SGI_ARCS_VERS;
	mgrastmpl.Revision = SGI_ARCS_REV;
	mgrastmpl.Key = 0;
	mgrastmpl.AffinityMask = 0x01;
	mgrastmpl.ConfigurationDataSize = 0;
	mgrastmpl.Identifier = idbuf;
	mgrastmpl.IdentifierLength = 100;

	for (idx=0; idx < MGRAS_MAXBOARDS; idx++) {

	    /* Need to setup pmg addr for non-HQ4 MgrasProbe */
	    pmg = MgrasBoards[idx].base;
	    if (MgrasProbe(pmg, idx)) {
		pmg = MgrasBoards[idx].base;	/* pmg was NULL for HQ4 case */
		pinfo = MgrasBoards[idx].info;
		/*
		 * In standalone, must be prepared for BSS to be initted.
		 * We use Bitplanes only to indicate that info is ok.
		 */
		if (pinfo->Bitplanes == 0 && !MgrasBoards[idx].boardInitted) {
		    /*
		     * Initialize the pinfo structure.  Although we just
		     * want info here (i.e. we'd like to just call
		     * MgrasGatherInfo), it turns out that depending on how
		     * we've gotten here, it's necessary to call MgrasReset
		     * before reading board regs.
		     *
		     * mgrasInitBoard calls MgrasReset AND MgrasGatherInfo.
		     */
		    mgrasInitBoard(pmg, idx);
		}
		tmp = AddChild(root, &mgrastmpl, (void*) NULL);
		if (tmp == (COMPONENT*) NULL)
			cpu_acpanic("mgras");
		tmp->Key = gfxboard++;

		MgrasFillBoardID(pinfo, tmp->Identifier);
		tmp->IdentifierLength = strlen(tmp->Identifier);

		RegisterDriverStrategy(tmp, _gfx_strat);

		if (pinfo->MonitorType != 0xf) {
			tmp = AddChild(tmp, &montmpl, (void*)NULL);
			if (tmp == (COMPONENT*)NULL)
				cpu_acpanic("monitor");
		} 
	    }
	}
}

#else /* IP27 */

#include <pgdrv.h>

/* The probe routine no longer serves its traditional purpose on high
 * end systems.  All probing is done when the prom hardware graph is
 * initialized.  The only remaining purposes of mgras_probe are:
 * 1) if fncs=0, we return any pipe's base address
 * 2) if fncs!=0, install the textport callback functions and return 0
 */
void *
mgras_probe(struct htp_fncs **fncs)		/* standalone */
{
    if (!MgrasBdsInitted) {
	printf("mgras_probe: should not be called before mgras_install\n");
	return (void *) 0;
    }

    if (fncs) {
	*fncs = (struct htp_fncs *)&mgras_htp_fncs;
	return (void *) 0;
    }

    /* textport pipe is always pipe 0 since IP27 only supports one pipe */
    return MgrasBoards[0].base;
}


extern int _gfx_strat(COMPONENT *, IOBLOCK *);

/*
 * Add this board to the hardware config -- IP27 PROM ONLY
 */
void mgras_install(COMPONENT *c)
{
    char		idbuf[100];
    struct mgras_hw	*pmg;
    struct mgras_info	*pinfo;
    int idx;

    if (!MgrasBdsInitted) {
	MgrasBdsInitted = 1;
	bcopy((char *) MG_nonvolatile_aggregate, (char *) MgrasBoards,
	      sizeof(mgrasboards) * MGRAS_MAXBOARDS);
    }

    for (idx=0; (idx < MGRAS_MAXBOARDS) && MgrasBoards[idx].boardFound; idx++)
	;
    if (idx == MGRAS_MAXBOARDS) {
	printf("mgras_install: Bad news, too many MGRAS Boards!\n");
	return;
    }
    
    /*
     * The PROM needs to pass the device base address and the
     * device  cfg space address to the device drivers during
     * install. The COMPONENT->Key field is used for this purpose.
     * Macros needed by SN0 device drivers to convert the
     * COMPONENT->Key field to the respective base address.
     * Key field looks as follows:
     *
     *  +----------------------------------------------------+
     *  |devnasid | widget  |pciid |hubwidid|hstnasid | adap |
     *  |   2     |   1     |  1   |   1    |    2    |   1  |
     *  +----------------------------------------------------+
     *  |         |         |      |        |         |      |
     *  64        48        40     32       24        8      0
     *
     * These are used by standalone drivers till the io infrastructure
     * is in place. SN0 Macros in "irix/kern/sys/SN0/sn0addrs.h"
     * 	  SN0 saves addr (within COMPONENT Key)
     */
    MgrasBoards[idx].base = pmg = (mgras_hw *)GET_WIDBASE_FROM_KEY(c->Key);

    if (!MgrasBoards[idx].boardFound) {
	MgrasBoards[idx].xbow_port = WIDGETID_GET((__psunsigned_t)pmg);
	MgrasBoards[idx].boardFound = 1;
	/* 
	 * Save the control register so we can restore
	 * the value set by the prom in MgrasReset
	 */
	MgrasBoards[idx].w_control = pmg->xt_control;
    }
    pinfo = MgrasBoards[idx].info;

    /*
     * In standalone, must be prepared for BSS to be initted.
     * We use Bitplanes only to indicate that info is ok.
     */
    if (pinfo->Bitplanes == 0 && !MgrasBoards[idx].boardInitted) {
	/*
	 * Initialize the pinfo structure.  Although we just
	 * want info here (i.e. we'd like to just call
	 * MgrasGatherInfo), it turns out that depending on how
	 * we've gotten here, it's necessary to call MgrasReset
	 * before reading board regs.
	 *
	 * mgrasInitBoard calls MgrasReset AND MgrasGatherInfo.
	 */
	mgrasInitBoard(pmg, idx);
    }
    
    MgrasFillBoardID(pinfo, idbuf);
    c->Identifier = idbuf;
    c->IdentifierLength = strlen(idbuf);
}

#endif /* IP27 */

static int mgras_mco_probe(mgras_hw *base)
{
    return MCO_BOARD_NOT_PRESENT;
}

#endif /* _STANDALONE */

#ifndef _STANDALONE

/******************************************************************************
 * K E R N E L - O N L Y   I N I T I A L I Z A T I O N   R O U T I N E S
 *****************************************************************************/

extern pgno_t availrmem; /* pages of memory available for locking */

static void MgrasInitBoardGfx(mgras_hw *base, mgras_info *info, int board)
{
	mgras_data *bdata;

	/*
	 * XXX Add gfx configuration to hardware inventory 
	 */
	switch(info->GfxBoardRev) {
	case 0:	 /* only one so far */	
	default:
		break;
	}

	add_to_inventory(INV_GRAPHICS, INV_MGRAS, 0, board, 
#if HQ4
			 INV_MGRAS_HQ4	      |
#endif
			 (info->NumGEs << 16) |
			 (info->NumREs << 8)  |
			 info->NumTRAMs);

	if (info->MCOBdPresent) {
	    add_to_inventory(INV_DISPLAY, INV_ICO_BOARD, 0, board, 0);
	}

	/*
	 * Malloc space for per board data (not per board type!).
	 * MAKE SURE WE BZERO() THIS STRUCTURE!  Everything in the
	 * info structure starts out zero (false).
	 */
	if(!(bdata = (mgras_data *)kern_malloc(sizeof(mgras_data)))) {
	  cmn_err(CE_PANIC, "Could not allocate mem for MGRAS gfx\n");
	}
	bzero((void *) bdata, sizeof(mgras_data));

	bdata->base = base;
	bdata->info = info;
	MgrasBoards[board].data = bdata;

	/*  XXX Initialize dma pages */

	/* Initialize DMA user page locking limit */
	if (MgrasMaxLockableMemPages == 0) {
                /* new scheme for determining number of gfx lockable pages:
                 * for avail pages below 64M use gfxlockablemem as pct (p),
                 * for those above 64M use gfxlockablemem_gt64 as pct  (p')
                 * then we have :
                 * MgrasMaxLockableMemPages = p*64M + p'(avail-64M)
                 */
                
                int avail_M = 0;
                if (gfxlockablemem < 0)
                        gfxlockablemem = 0;
                if (gfxlockablemem > 100)
                        gfxlockablemem = 100;
                if (gfxlockablemem_gt64 < 0)
                        gfxlockablemem_gt64 = 0;
                if (gfxlockablemem_gt64 > 100)
                        gfxlockablemem_gt64 = 100;
                
                avail_M = availrmem * _PAGESZ;
                if (avail_M<=(64*(1<<20))){
                        MgrasMaxLockableMemPages =
                                (gfxlockablemem * availrmem + 99) / 100;
                }
                else {
                        /* we have more than 64M worth of pages lockable */
#if _PAGESZ == 16384
                        MgrasMaxLockableMemPages =
                                (gfxlockablemem * 4096 + 99) / 100;
                        ASSERT(availrmem-4096>0);
                        MgrasMaxLockableMemPages +=
                                (gfxlockablemem_gt64 * (availrmem-4096) + 99) / 100;
#elif _PAGESZ == 4096
                        MgrasMaxLockableMemPages =
                                (gfxlockablemem * 16384 + 99) / 100;
                        ASSERT(availrmem-16384>0);
                        MgrasMaxLockableMemPages +=
                                (gfxlockablemem_gt64 * (availrmem-16384) + 99) / 100;
#else
#error "Unknown page size"
#endif

                }

                MgrasCurrLockedMemPages = 0;
                MgrasLockedBoundaryMsgSent = 0;
#ifdef DEBUG
                printf("availrmem               : %d\n",availrmem);
                printf("gfxlockablemem          : %d\n",gfxlockablemem);
                printf("gfxlockablemem_gt64     : %d\n",gfxlockablemem_gt64);
                printf("MgrasMaxLockableMemPages: %d\n",MgrasMaxLockableMemPages);
#endif
		/* X locks out of a special allocation
		 * that is always provided and not available 
		 * to other applications. Set these pages 
		 * aside.
		 */
                MgrasXMaxLockableMemPages = MGRAS_DMA_X_LOCKABLE;
                if (MgrasXMaxLockableMemPages >= MgrasMaxLockableMemPages) {
                        MgrasMaxLockableMemPages = 0;
                } else {
                        MgrasMaxLockableMemPages -= MgrasXMaxLockableMemPages;
                }
                MgrasXCurrLockedMemPages = 0;
        }


	/* force RRM to call PcxSwap all the time for MGRAS */

	bdata->gfx_data.numpcx = 1; /* number of pipe contexts in HW */
	bdata->gfx_data.nummodes = MGRAS_XMAP_NUMMODES;
	bdata->gfx_data.loadedmodes = bdata->loadedmodes;


        /* init intr handler flags (intermediate MP fix) */
        bdata->gen_intr_flags = 0;

	MgrasEramManagerTimeout(bdata);

	/*
	 * Initialize retrace handler
	 */
	MgrasRetraceInit(bdata);
	ASSERT(bdata->Retrace);

	spinlock_init(&bdata->mgraslock, "mgraslock");
	spinlock_init(&bdata->gfxflowlock, "gfxflowlock");
	mutex_init(&bdata->mgras_sleep_lock,MUTEX_DEFAULT,"mgras_sleep_lock");
	/*
	 * Register this board with the graphics driver.
	 */
	GfxRegisterBoard((struct gfx_fncs *)&mgras_gfx_fncs,
			 (struct gfx_data *)bdata,
			 (struct gfx_info *)bdata->info);
}

/*
 * MgrasFindBoardAddr - return NULL if board not found.
 *	Has side effect of setting:	MgrasBoards[].base,
 *					MgrasBoards[].boardFound.
 */
static mgras_hw *
MgrasFindBoardAddr(int board)
{
	mgras_hw *base;
	
#if HQ4
	__uint32_t w_control, w_widgetid;

	if (MgrasBoards[board].boardFound) {
		/*
		 * We've already been called, due to 2nd mgras_earlyinit
		 * call.  Since mgras_attach() has already been called,
		 * don't call xtalk_early_piotrans_addr now.
		 */
		return MgrasBoards[board].base;
	}
	base = (mgras_hw *) xtalk_early_piotrans_addr(
		HQ4_XT_ID_PART_NUM_VALUE,
		HQ4_XT_ID_MFG_NUM_VALUE,
		board,
		MGRAS_REGS_OFFSET, sizeof(mgras_hw),
		0);	/* flags */
	MgrasBoards[board].boardFound = (base) ? 1 : 0;
	if (base) {
		MgrasBoards[board].base = base;
		w_control = base->xt_control;
		w_widgetid = w_control & WIDGET_WIDGET_ID;
		ASSERT (w_widgetid != 0xf);
		MgrasBoards[board].xbow_port = w_widgetid;
		
		/* 
		 * Save the control register so we can restore
		 * the value set by the prom in MgrasReset
		 */
		MgrasBoards[board].w_control = w_control;
	}
#else /* !HQ4 */
	base = MgrasBoards[board].base;
	if (!MgrasProbe(base, board))
		base = NULL;
#endif /* HQ4 */
	
	return base;
}

/*
 * Early initialization.  Done early in UNIX initialization so that we
 * can run the textport to get early output (printf's, panics, etc.)
 */
int
mgras_earlyinit(void)
{
	mgras_hw *base;
	mgras_info *info;
	int board;
	int boardsfound = 0;

	/* Called by textport to probe for boards */
	for (board = 0; board < MGRAS_MAXBOARDS; board++) {
		/*
		 * Initialize board if it exists.
		 */
		base = MgrasFindBoardAddr(board);
		if (base) {
			mgrasInitBoard(base, board);
			/*
			 * Register the first board found with dev. independent
			 * textport code.
			 */
			if (boardsfound++ == 0) {
				info = MgrasBoards[board].info;
				htp_register_board(base, &mgras_htp_fncs,
					info->gfx_info.xpmax,
					info->gfx_info.ypmax);
			}
		}
	}

#ifdef HQ4
	if (boardsfound) {
	    /*
	     * Use firsttime so that xtalk_early_XXX won't be called
	     * during subsequent mgras_earlyinit calls.
	     */
	    static int firsttime = 1;
	    xbow_t *xbow;
	    if (firsttime) {
		firsttime = 0;
		/* make sure that we have a xbow that works with gfx */
		xbow = (xbow_t *) xtalk_early_piotrans_addr(
		    XBOW_WIDGET_PART_NUM,
		    XBOW_WIDGET_MFGR_NUM,
		    0,
		    0, sizeof(xbow_t),
		    0);	/* flags */
		if (XWIDGET_REV_NUM(xbow->xb_wid_id) == XBOW_REV_1_0) {
		    cmn_err( CE_PANIC,
		    "mgras_earlyinit: Xbow 1.0 cannot support graphics");
		}
	    }
	}
#endif
	return boardsfound ? 1 : 0;
}

/*
 * Called from main() at boot time
 * Device table initialization for MGRAS 
 */

#if HQ4

void mgras_init(void)
{
	xwidget_driver_register(HQ4_XT_ID_PART_NUM_VALUE,
				HQ4_XT_ID_MFG_NUM_VALUE,
				"mgras_",
				0);
}

int mgras_attach(vertex_hdl_t conn)
{
	vertex_hdl_t vhdl;
	mgras_hw *base;
	mgras_info *info;
	int board, s;
	mgras_data *bdata;
	device_desc_t temp_intr_desc;

	hwgraph_char_device_add(conn, "mgras", "mgras_", &vhdl);

	/*
	 * Get a pointer to the HQ4 address space.
	 * This address space includes the standard widget registers
	 * in the low end of the address space.
	 */
	base = (mgras_hw *) xtalk_piotrans_addr 
		(conn, 0,		/* device and (override) dev_info */
		MGRAS_REGS_OFFSET, sizeof(mgras_hw),   /* xtalk base and size */
		0);			/* flag word */
	ASSERT(base);
	
	/*
	 * What index into MgrasBoards[] does this base addr
	 * correspond to?
	 */
	for (board = 0; board < MGRAS_MAXBOARDS; board++) {
		if (MgrasBoards[board].base == base)
			break;
	}
	if (board == MGRAS_MAXBOARDS) {
		__uint32_t w_control = base->xt_control;
		int w_widgetid = (int) (w_control & WIDGET_WIDGET_ID);
		for (board = 0; board < MGRAS_MAXBOARDS; board++) {
			if (w_widgetid == MgrasBoards[board].xbow_port) {
				MgrasBoards[board].base = base;
				break;
			}
		}
		ASSERT (board != MGRAS_MAXBOARDS);
	}

	ASSERT (MgrasBoards[board].boardFound);

	MgrasBoards[board].nicinfo = 
	  nic_hq4_vertex_info(conn, (nic_data_t)&base->microlan_access);

#if defined(DEBUG) && defined(DEBUG_NIC)
	printf("MgrasBoards[%d].nicinfo %s\n", board, 
		MgrasBoards[board].nicinfo);
#endif /* DEBUG */
	/*	MgrasTextureMgrInitFixup(); XXXnate */

	info = MgrasBoards[board].info;
	MgrasInitBoardGfx(base, info, board);

	bdata = MgrasBoards[board].data;
	bdata->dev = conn;

	/* Need to have a device descriptor to get default behavior */
	temp_intr_desc = device_desc_dup(vhdl);
	bdata->flags_intr = xtalk_intr_alloc(conn, temp_intr_desc, vhdl);
	bdata->status_intr = xtalk_intr_alloc(conn, temp_intr_desc, vhdl);
	device_desc_free(temp_intr_desc);

	/* XXXnate According to Olson and nigel, the retrace handler 
	   MUST run on a thread for audio to have a chance of working. 
	   Not running on a thread does not buy us a lot except guaranteeing
	   non-interruptability.  Since we should be the highest priority
	   thread, this should not matter.
	*/
	temp_intr_desc = device_desc_dup(vhdl);
	/* The next line gives us priority 250 */
	device_desc_intr_swlevel_set(temp_intr_desc, 250);
	bdata->retrace_intr = xtalk_intr_alloc(conn, temp_intr_desc, vhdl);
	device_desc_free(temp_intr_desc);

	temp_intr_desc = device_desc_dup(vhdl);
	device_desc_flags_set(temp_intr_desc,
			      device_desc_flags_get(temp_intr_desc) |
			      D_INTR_ISERR);
	bdata->err_intr = xtalk_intr_alloc(conn, temp_intr_desc, vhdl);
	device_desc_free(temp_intr_desc);

	bdata->flags_intr_vector_num =
		(short) xtalk_intr_vector_get(bdata->flags_intr);
	bdata->status_intr_vector_num =
		(short) xtalk_intr_vector_get(bdata->status_intr);
	bdata->retrace_intr_vector_num =
		(short) xtalk_intr_vector_get(bdata->retrace_intr);
	bdata->err_intr_vector_num =
		(short) xtalk_intr_vector_get(bdata->err_intr);
#if HQ4_BRINGUP_HACKS
    {
	extern void gdbg_mgstat(__psint_t x);

	bzero(&hq4IntrStat, sizeof(mgras_intr_stat));
#ifdef DEBUG
	idbg_addfunc("mgstat", gdbg_mgstat);
#endif
    }
#endif

	xtalk_intr_connect(bdata->flags_intr,
		(intr_func_t) MgrasGeneralInterrupt, bdata, NULL, NULL, NULL);
	xtalk_intr_connect(bdata->status_intr,
		(intr_func_t) mgras_hq4_lowater_intr, bdata, NULL, NULL, NULL);
	xtalk_intr_connect(bdata->retrace_intr,
		(intr_func_t) MgrasRetraceHandler, bdata, NULL, NULL, NULL);
	xtalk_intr_connect(bdata->err_intr,
		(intr_func_t) MgrasHQ4ErrInterrupt, bdata, NULL, NULL, NULL);
#if 0
	xwidget_error_register(conn,
			       (error_handler_f *)MgrasErrorHandler,
			       (error_handler_arg_t)bdata);
#endif


#if DEBUG && HQ4_BRINGUP_HACKS
    {
	extern void gdbg_mgintr(__psint_t x);
	extern int  idbg_addfunc(char *name, void (*func)());

	idbg_addfunc("mgintr", gdbg_mgintr);
    }
#endif
	/*
	 * Enable interrupts for this board
	 * XXX - How do we wait till the last board to do this?
	 */
	s = mutex_spinlock(&bdata->mgraslock);
	MgrasSetHQ4IntrVectors(bdata);
	mutex_spinunlock(&bdata->mgraslock, s);

	return 0;
}

typedef struct _HQ4InitialIntrData {
	__uint32_t intr_vec_reg;
	__uint32_t interrupt_ctl_hi, interrupt_ctl_lo;
	__uint32_t xt_int_dst_addr_hi, xt_int_dst_addr_lo;
	int expansion_intr_enab;
	int video_intr_enable;
} HQ4InitialIntrData;

static void MgrasSetHQ4IntrVectors(mgras_data *bdata)
{
	mgras_hw *base = bdata->base;
	static int intrDataInitted[MGRAS_MAXBOARDS];
	static HQ4InitialIntrData hq4InitialIntrData[MGRAS_MAXBOARDS];
	HQ4InitialIntrData *hq4InitDataPtr;
	int board;

	board = mgras_baseindex(base);
	hq4InitDataPtr = &hq4InitialIntrData[board];

	if (!intrDataInitted[board]) {
		iopaddr_t addr;
		xwidgetnum_t targ_id;
		__uint32_t addr_upper, addr_lower;

		intrDataInitted[board] = 1;

		addr = xtalk_intr_addr_get(bdata->flags_intr);
		bdata->intr_xaddr = addr;

		targ_id = xtalk_intr_target_get(bdata->flags_intr);
		bdata->hostid = targ_id;

		/* expansion intr not yet used */
		bdata->expansion_intr_vct_num = 0;
		hq4InitDataPtr->expansion_intr_enab = 0;

		/* XXX - video intr not yet used */
		bdata->video_intr_vct_num = 0;
		hq4InitDataPtr->video_intr_enable = 0;

		hq4InitDataPtr->intr_vec_reg =
			((bdata->video_intr_vct_num << HQ4_VIDEO_INTR_VCT_SHFT)
				& HQ4_VIDEO_INTR_VCT_MASK) |
			((bdata->retrace_intr_vector_num << HQ4_VERT_INTR_VCT_SHFT)
				& HQ4_VERT_INTR_VCT_MASK) |
			((bdata->flags_intr_vector_num << HQ4_FLAGS_INTR_VCT_SHFT)
				& HQ4_FLAGS_INTR_VCT_MASK) |
			((bdata->status_intr_vector_num << HQ4_STATUS_INTR_VCT_SHFT)
				& HQ4_STATUS_INTR_VCT_MASK);

		addr_upper = XTALK_ADDR_TO_UPPER(addr);
		addr_lower = XTALK_ADDR_TO_LOWER(addr);

		hq4InitDataPtr->interrupt_ctl_hi =
			HQ4_VERT_INTR_ENAB |
			hq4InitDataPtr->video_intr_enable |
			hq4InitDataPtr->expansion_intr_enab |
			((targ_id << HQ4_TGT_ID_SHFT) & HQ4_TGT_ID_MASK) |
			bdata->expansion_intr_vct_num |
			addr_upper;

		hq4InitDataPtr->interrupt_ctl_lo = addr_lower;

		hq4InitDataPtr->xt_int_dst_addr_hi = 
			((bdata->err_intr_vector_num << WIDGET_INT_VECTOR_SHFT)
				& WIDGET_INT_VECTOR) |
#ifdef DOESNT_WORK
			HQ4_XT_RESP_ERR_INT_ENAB |
#endif
			HQ4_XT_ADDR_ERR_INT_ENAB |
			((targ_id << WIDGET_TARGET_ID_SHFT) & WIDGET_TARGET_ID) |
			addr_upper;
		hq4InitDataPtr->xt_int_dst_addr_lo = addr_lower;
	}
	base->interrupt_vector = hq4InitDataPtr->intr_vec_reg;
	base->interrupt_ctl_hi = hq4InitDataPtr->interrupt_ctl_hi;
	base->interrupt_ctl_lo = hq4InitDataPtr->interrupt_ctl_lo;

	base->xt_int_dst_addr_hi = hq4InitDataPtr->xt_int_dst_addr_hi;
	base->xt_int_dst_addr_lo = hq4InitDataPtr->xt_int_dst_addr_lo;

	/* XXX - Also need to initialize:  base->dma_interrupt. */
}

#else /* !HQ4 */

void mgras_init(void)
{
	register mgras_hw *base;
	mgras_info *info;
	int board;
	int boardsfound = 0;
	mgras_data *bdata;
	static int mgrasdbg_probe = 0;
	
	/*	MgrasTextureMgrInitFixup(); XXXnate */
	/*
	 * Gather inventory of each board
	 */
	for (board = 0; board < MGRAS_MAXBOARDS; board++) {

		if (MgrasBoards[board].boardFound == 0) {
			DEBUGprintf1(mgrasdbg_probe,
				     "mgras_init: no board at index %d\n",board);
		} else {
			/*
			 * Probe and initialization done in mgras_earlyinit()
			 */
			base = MgrasBoards[board].base;
			info = MgrasBoards[board].info;
			DEBUGprintf1(mgrasdbg_probe,
				     "mgras_init: found board at 0x%x\n", base);
			boardsfound++;
			MgrasInitBoardGfx(base, info, board);
		}
	}

	MgrasInitIntrVectors();
	
#ifdef DEBUG
	if (boardsfound == 0)	/* If DEBUG print anyway */
		cmn_err(CE_CONT, "mgras: missing\n");
#endif

	return;
}

static void MgrasSetGioVectors(int board)
{
	int slot = MgrasBoards[board].slot;

	setgiovector(GIO_INTERRUPT_0, slot,
		MgrasFIFOInterrupt, board);
	setgiovector(GIO_INTERRUPT_1, slot,
		MgrasGeneralInterrupt, board);
	setgiovector(GIO_INTERRUPT_2, slot,
		MgrasRetraceInterrupt, board);
}

#endif /* !HQ4 */

void MgrasInitIntrVectors(void)
{
	/*  For multi-head case on giobus systems
	 *  we need to enable all retrace intrs at
	 *  once before we can take retrace interrupts.
	 */
	int s;
	int board = (MgrasBoards[0].boardFound) ? 0 : 1;
	mgras_data *bdata = MgrasBoards[board].data;

	s = mutex_spinlock(&bdata->mgraslock);

	for (board = 0; board < MGRAS_MAXBOARDS; board++) {
		if (MgrasBoards[board].boardFound) {
#if HQ4
			MgrasSetHQ4IntrVectors(MgrasBoards[board].data);
#else
			MgrasSetGioVectors(board);
#endif
		}
	}
	mutex_spinunlock(&bdata->mgraslock, s);
}

#endif /* !_STANDALONE */

/******************************************************************************
 * C O M M O N   I N I T I A L I Z A T I O N   R O U T I N E S
 *****************************************************************************/

#ifndef HQ4
extern uint _hpc3_write2;
#endif

static void mgrasConfigureXMAP(mgras_hw *base, mgras_info *info)
{
	  mgras_BFIFOPOLL(base, 4);
	  mgras_xmapSetAddr(base, 0x1);
	  mgras_xmapSetConfigByte(base, 0x48); /* turn on autoinc bit */
	  mgras_xmapSetAddr(base, MGRAS_XMAP_CONFIG_ADDR);
	  mgras_xmapSetConfig(base, MGRAS_XMAP_CONFIG_DEFAULT(info->NumREs));
}

void
mgrasInitBoard(mgras_hw *base, int board)
{
	int default_timing, i;
	mgras_info *info = MgrasBoards[board].info;

	
#if HQ4
	Mgras_FC_Disable();	/* disable flow control */
#endif
	MgrasReset(base);

	if (!MgrasBoards[board].infoFound) {
		MgrasGatherInfo(base, info);
	} else {
		MgrasInitDCBctrl(base);
		mgrasConfigureXMAP(base, info);	/* configure the xmap */
	}

#if !HQ4 && !defined(_STANDALONE)
	/* Rev-B PP's stay up better with a voltage tork down */
	/* MG10 and High/Max Rev 3+ have Rev C or higher PP1s */
	if ((info->ProductID == MGRAS_PRODUCT) && (info->RARev < 3)) {
	    _hpc3_write2 &= ~MARGIN_HI;
	    _hpc3_write2 |= MARGIN_LO;
	    *(volatile uint *)PHYS_TO_K1(HPC3_WRITE2) = _hpc3_write2;
	    flushbus();
	}
	MgrasReset(base);
	mgrasConfigureXMAP(base, info);
#endif
	/*
	 * Setup default timing table
	 */
	default_timing = 0;
	for (i = 0; i < mgras_num_monitor_defaults; i++) {
	    if (mgras_default_timing[i].montype == (int )(info->MonitorType)){
		    default_timing = mgras_default_timing[i].table;
	    }
	}

/*XXX--check this-- req'd if to remove X monitor setting */
	if (info->NumREs == 2) {
	    MgrasRecordTiming(base, mgras_timing_tables_2RSS[default_timing]);
	    MgrasCalcScreenSize(mgras_timing_tables_2RSS[default_timing],
				&info->gfx_info.xpmax, &info->gfx_info.ypmax);
	} else {
	    MgrasRecordTiming(base, mgras_timing_tables_1RSS[default_timing]);
	    MgrasCalcScreenSize(mgras_timing_tables_1RSS[default_timing],
				&info->gfx_info.xpmax, &info->gfx_info.ypmax);
	}

	/* Init the HW */
	MgrasInitHQ(base, 0);
	MgrasInitRasterBackend(base, info);

	info->Xabnormalexit = 1;

	MgrasBoards[board].boardInitted = 1;

#ifdef _STANDALONE
	/* The Indigo2 logo does not fit on the small screen */
	if ((info->gfx_info.xpmax < 1280) || (info->gfx_info.ypmax < 1024)) {
		logoOff();
	}
#endif
}

/*
 * MgrasProbe(mgras_hw *base, int board)     - Used by both kernel & standalone
 *
 * NOTE:  The HQ4 MgrasProbe uses the "board" arg in determining if
 * the board is present (and returns the board addr in
 * MgrasBoards[board].base), whereas the non-HQ4 MgrasProbe uses the
 * "base" arg.
 */
#if HQ4

# ifndef IP27
/*ARGSUSED*/
int
MgrasProbe(mgras_hw *base, int board)
{
	int found = MgrasBoards[board].boardFound;
	if (!found)
		found = (MgrasFindBoardAddr(board)) ? 1 : 0;
	return found;
}
# endif
#else /* !HQ4 */

/*ARGSUSED*/
int
MgrasProbe(mgras_hw *base, int board)
{
	unsigned int gio_id;
	unsigned int arb_bits;

#ifdef _STANDALONE
	if (badaddr(&base->gio_id, sizeof(int)))
		return 0;
#endif
	gio_id = base->gio_id;
	if (HQ3_GIO_ID_PRODUCT_ID(gio_id) != GIO_ID_MGRAS)
		return 0;
	if (HQ3_GIO_ID_GIO_INTERFACE_SIZE(gio_id) != 1)
		return 0;
	if (HQ3_GIO_ID_GIO_ROM(gio_id) != 0)
		return 0;
	if (HQ3_GIO_ID_MANUFACTURER_CODE(gio_id) != 1)
		return 0;
#ifdef _STANDALONE
	if (badaddr(&base->hq_config, sizeof(int)))
		return 0;
	if (badaddr(&base->gio_config, sizeof(int)))
		return 0;
#endif

	/* Since the HQ is a 64-bit device, we need to tell the MC to talk
	 * to it in 64-bit mode.  This needs to be done before any reads or
	 * writes to the HQ.  We'll also set the GIO_MASTER bit so that the
	 * MC knows the MGRAS board can be a GIO master (for DMA).
	 */
	arb_bits = (GIO64_ARB_GRX_MST | GIO64_ARB_GRX_SIZE_64) 
				<< board;
#ifdef _STANDALONE
	{
	  extern __uint32_t _gio64_arb;
	  _gio64_arb |= arb_bits;
	  *(unsigned int *)(PHYS_TO_K1(GIO64_ARB)) = _gio64_arb;
	}
#else
	{
	  extern u_int gio64_arb_reg;
	  gio64_arb_reg |= arb_bits;
	  *(unsigned int *)(PHYS_TO_K1(GIO64_ARB)) = gio64_arb_reg;
	}
#endif
	MgrasBoards[board].boardFound = 1;
	return 1;
}

#endif /* !HQ4 */

#if !defined(_STANDALONE)
int gevers_ucode[][3] = {
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x8cf80000, 0x39c007c8, 0x00000070,
    0x81780000, 0xfa41e7c8, 0x00000077,
    0x81520018, 0xfa41e7c8, 0x00000077,
    0x81d80000, 0xfa41e7c8, 0x00000077,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x40000000, 0x205fe7c2, 0x00000078,
    0x02200000, 0x205fe7c0, 0x00000078,
    0x42202000, 0x205fe7c1, 0x00000078,
    0x00000000, 0xc08007e0, 0x0000007b,
    0x00000000, 0xc18007e0, 0x0000007b,
    0x81200000, 0xfa41e7c8, 0x00000077,
    0x811b0000, 0xfa41e7c8, 0x00000077,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x80f80000, 0xfa43bfc8, 0x00000077,
    0x80f80001, 0xfa43bfc8, 0x00000077,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x0027dd80, 0x808647ea, 0x00000062,
    0x0027dd80, 0xc28647aa, 0x00000063,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x80f80027, 0xf24007c8, 0x00000077,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0xbaadcafe, 0xbaadcafe, 0xbaadcafe,
    0xbaadcafe, 0xbaadcafe, 0xbaadcafe,
    0xbaadcafe, 0xbaadcafe, 0xbaadcafe,
    0xbaadcafe, 0xbaadcafe, 0xbaadcafe,
    0xbaadcafe, 0xbaadcafe, 0xbaadcafe,
    0xbaadcafe, 0xbaadcafe, 0xbaadcafe,
    0xbaadcafe, 0xbaadcafe, 0xbaadcafe,
    0xbaadcafe, 0xbaadcafe, 0xbaadcafe,
    0xbaadcafe, 0xbaadcafe, 0xbaadcafe,
    0xbaadcafe, 0xbaadcafe, 0xbaadcafe,
    0xbaadcafe, 0xbaadcafe, 0xbaadcafe,
    0xbaadcafe, 0xbaadcafe, 0xbaadcafe,
};
#endif

#define GEVERS_UCODE_SIZE (sizeof(gevers_ucode))
#if !defined(_STANDALONE) && !HQ4
extern int mc_rev_level;	/* The real mc rev level */
#endif

#ifdef NOTDEF
static void MgrasTextureMgrInitFixup(void)
{
	/*
	 * Nasty Hack:  Global Texture Mgr can't handle multiple heads
	 * with h/w texturing, so we need to lie if there are two
	 * MG1[14]s in the box.
	 */
	if (!MgrasBoards[0].infoFound || !MgrasBoards[1].infoFound)
		return;
	if (MgrasBoards[0].info->NumTRAMs > 0 &&
	    MgrasBoards[1].info->NumTRAMs > 0) {
		mgras_info *info0 = MgrasBoards[0].info;
		mgras_info *info1 = MgrasBoards[1].info;
		if (info0->NumTRAMs < info1->NumTRAMs)
		    info0->NumTRAMs = info0->NumRATRAMs = info0->NumRBTRAMs = 0;
		else
		    info1->NumTRAMs = info1->NumRATRAMs = info1->NumRBTRAMs = 0;
	}
}
#endif

/*
 * MgrasGatherInfo - fill in the mgras_info struct for the given slot
 */
int
MgrasGatherInfo(mgras_hw *base, mgras_info *info)
{
	unsigned char bdvers0, bdvers1, cmap0vers, cmap1vers;
	int board = mgras_baseindex(base);

	/*
	 * We already know there is a board here, so we can just start
	 * getting the relevant info.
	 */ 
	bzero((void *) info, sizeof(mgras_info));
	MgrasInitDCBctrl(base);
#if HQ4
	info->HQRev = XWIDGET_REV_NUM(base->xt_widget_id);
#ifndef _STANDALONE
	if (info->HQRev == 1) {
		cmn_err(CE_WARN, "prototype rev-A HQ4 chip found, workarounds may degrade system performance");
	}
#endif
#else /* !HQ4 */
    {
	unsigned int gio_id = base->gio_id;
	info->HQRev = HQ3_GIO_ID_PRODUCT_REV(gio_id);
    }
#endif /* HQ4 */
	MgrasBoards[board].infoFound = 1;
	strncpy(info->gfx_info.name, GFX_NAME_MGRAS, GFX_INFO_NAME_SIZE);
	info->gfx_info.length = sizeof(mgras_info);

	/* Set HQ config registers to default values so we can read out some
	 * information.  These may be reset later based on some of the info
	 * collected.  
	 */
	mgras_hqSetHQConfig(base, HQ3_HQ_CONFIG_DEFAULT);
	mgras_hqSetGIOConfig(base, HQ3_GIO_CONFIG_DEFAULT);

	mgras_BFIFOPOLL(base, 2);

	/* Read and parse board version register 1 */
	MGRAS_GET_BV1(base, bdvers1);
	info->GfxBoardRev = MGRAS_BV1_BOARD_REV(bdvers1);
	info->NumGEs = MGRAS_BV1_NUM_GE(bdvers1);
	info->ProductID = MGRAS_BV1_PRODUCT_ID(bdvers1);
#ifndef _STANDALONE
    {
	/*
	 * environment variable, if present, overrides
	 * number of ge's in the board version register
	 */
	extern char *kopt_find(char *);
	char *cp;

	if ((cp = kopt_find("mgras_numge")) && *cp)
		info->NumGEs = *cp - '0';
    }
#endif

	MGRAS_GET_BV0(base, bdvers0);
	if (info->ProductID == MGRAS_PRODUCT_MG10) { /* solid impact */
	  info->NumTRAMs = info->NumRATRAMs = info->NumRBTRAMs = 0;
	  info->NumREs = 1;
	  info->RBPresent = 0;
	  info->RBRev = MGRAS_BV0_RB_NOT_INSTALLED;
	} else if (info->ProductID == MGRAS_PRODUCT) { /* high or max impact */
	  /* Read and parse board version register 0 */
	  info->NumRATRAMs =
	    MGRAS_BV0_DECODE_NUM_TRAM(MGRAS_BV0_NUM_RATRAM(bdvers0));
	  info->RBRev	= MGRAS_BV0_RB_REV(bdvers0);
	  if ((info->RBRev != MGRAS_BV0_RB_NOT_INSTALLED) && 
	      (info->RBRev != MGRAS_BV0_RB_NOT_INSTALLED_BAD_BIT2)) {
	    info->NumRBTRAMs =
	      MGRAS_BV0_DECODE_NUM_TRAM(MGRAS_BV0_NUM_RBTRAM(bdvers0));
	    info->RBPresent = 1;
	    info->NumREs = 2;
	  } else {
	    info->NumRBTRAMs = 0; 
	    info->RBPresent = 0;
	    info->NumREs = 1;
	  }
	  info->NumTRAMs = info->NumRATRAMs;
	} else {	/* MGRAS_PRODUCT_GM10 || MGRAS_PRODUCT_GM20 */
	  info->RBPresent = 0;
	  info->RBRev = MGRAS_BV0_RB_NOT_INSTALLED;
	  info->NumTRAMs = info->NumRATRAMs =
	    MGRAS_GAMERA_BV0_DECODE_NUM_TRAM(MGRAS_BV0_NUM_RATRAM(bdvers0));
	  info->NumRBTRAMs = 
	    MGRAS_GAMERA_BV0_DECODE_NUM_TRAM(MGRAS_BV0_NUM_RBTRAM(bdvers0));
	  info->NumREs = info->NumGEs;
	}
#ifndef _STANDALONE
    {
	/*
	 * environment variable, if present, overrides
	 * number of re's in the board version register
	 */
	extern char *kopt_find(char *);
	char *cp;

	if ((cp = kopt_find("mgras_numre")) && *cp) {
		info->NumREs = *cp - '0';
		if (info->NumREs < 2)
			info->RBPresent = 0;
	}
    } 
#endif
	if (info->NumREs > 0) {
#if defined(_STANDALONE) && !HQ4
	   if (badaddr(&base->cmap0.rev, sizeof(int)))
		return -1;
	   if (badaddr(&base->cmap1.rev, sizeof(int)))
		return -1;
#endif
	  /* Read and parse CMAP revision registers */
	  mgras_BFIFOPOLL(base, 2);
	  mgras_cmapGetRev(base, cmap0vers, 0);
	  mgras_cmapGetRev(base, cmap1vers, 1);
	  
	  info->MonitorType  = MGRAS_CMAP_MONITOR_TYPE(cmap0vers);
	  info->CmapRev	     = MGRAS_CMAP_CMAP_REV(cmap1vers);
	  info->RARev	     = MGRAS_CMAP_RA_REV(cmap1vers);
	  info->VidBdPresent = !MGRAS_CMAP_NO_VIDEO(cmap1vers);

	  /*	    
	   * Because of DCB address mapping, we need to set the index register
	   * then read at same location as Config register to get PP1 vers.
	   * See PP1 spec (XMAP section) for details.
	   */
	  mgrasConfigureXMAP(base, info);	/* First, configure the xmap */
	  mgras_BFIFOPOLL(base, 2);
	  mgras_xmapSetAddr(base,MGRAS_XMAP_REV_ADDR);
	  mgras_xmapGetConfig(base,info->PP1Rev);
	  info->PP1Rev &= 0x7;
	  
#if defined(_STANDALONE)  && !HQ4
	  if (badaddr(&base->rss.s.status, sizeof(int)))
	    return -1;
#endif
#if defined(FLAT_PANEL)
#if defined(_STANDALONE)
	    /*
	     * in standalone, there is no reason to care whether
	     * MCO or video is present.  We'll determine if the
	     * Presenter Adapter is present in MgrasSetTiming.
	     */
	    info->MCOBdRev = MCO_BOARD_NOT_PRESENT;
	    info->VidBdPresent = 0;
	    info->PanelType = 255;
#else /*!_STANDALONE*/
	  if (!(info->VidBdPresent)) {
	     mgras_fp_probe(base, info);
	     if (info->PanelType) {		/* flat panel adapter found, */
		info->VidBdPresent = 0;	/* which precludes video */
		if (info->PanelType != 255) {	/* display found as well */ 
			info->MonitorType = info->PanelType;
		}
		info->MCOBdRev = MCO_BOARD_NOT_PRESENT;
	     } else
		info->MCOBdRev = mgras_mco_probe(base);
	  } else {
		info->PanelType = 255;
		info->MCOBdRev = mgras_mco_probe(base);
	  }
	  if (info->MCOBdRev != MCO_BOARD_NOT_PRESENT) {
	      info->MCOBdPresent = 1;
	      info->VidBdPresent = 0;
	  }
#endif /*_STANDALONE*/
#endif /*FLAT_PANEL*/
#if HQ4_BRINGUP_HACKS
	  {
	      /*
	       * If this block is omitted, the subsequent mgras_re4Get
	       * hangs with buserr.  Since this write cannot get out to
	       * the rebus which is currently owned by some ge11, this is
	       * done only to get the side effect it has on the hq4.
	       * Later, when the write does get out on the rebus,
	       * it does not affect the re since the re status register
	       * is not writable.
	       */
	      mgras_re4Set(base, status.val, 0);
	  }
#endif
	  mgras_re4Get(base, status.bits.Version,info->RE4Rev);
	  /* mgras_re4Get(base, status.bits.RE4Count,info->NumREs); XXX */
	}

#if defined(_STANDALONE)
#if IP30
	{
	  /* XXX Get the GE11 rev without ucode.  Assumes GM=1 and MOT=3 
	   * NIC must be less than 256 bytes.  This code only works
	   * on IP30 since IP28 doesn't have a nic. */
	  char nicinfo[256];
	  extern nic_access_f    nic_access_mcr32;
	  nic_info_get(nic_access_mcr32, (nic_data_t)&base->microlan_access,
		       nicinfo);
	  if (strstr(nicinfo,"Name:MOT"))
		info->GE11Rev = 3;
	  else if (strstr(nicinfo,"Name:GM"))
		info->GE11Rev = 1;
	  else
	        info->GE11Rev = -1;
	}
#endif
#else
	/* Get GE11 Revision - Want to do this _last_ */
	if (info->NumGEs > 0) {
	    base->flag_clr = HQ_FLAG_GE_READ_BIT;
	    /* NOTE - since this routine is called form earlyinit, don't
	       change the printmsg param to 1 - printfs deadly during
	       earlyinit time. */
	    if (MgrasDownloadGE11Ucode(base, gevers_ucode, GEVERS_UCODE_SIZE,
				       info->NumGEs, 0)) {
		info->GE11Rev = -1;
	    } else {
#if !HQ4
		while (!(base->flag_set & HQ_FLAG_GE_READ_BIT))
		    ; /* spin */
#else
		/*
		 * GE_READ_BIT unreliable on HQ4, don't use it.
		 * each clock of 80MHz ge11 is 12.5nsec,
		 * 1040 clocks ought to be enough.
		 */
		DELAY(13);
#endif
		info->GE11Rev = base->rebus_read_hi;
		info->GE11Rev = base->rebus_read_lo;
	    }
	}
#endif /* !STANDALONE */

	/* Wait until MgrasStart to:
	 * 1) get VC3 rev - clock will have been programmed by then  
	 * 2) Do DMA to get TRAM version
	 */

#ifdef _STANDALONE
     	/*
	 * For standalone, fill bitplanes with deadbeef to indicate info is OK.
	 */
	/*XXX deadbeef dont fit into 8 bits*/
	info->Bitplanes = (unsigned char) 0xdeadbeef;
#else
# if !HQ4
	info->EARev = mc_rev_level;
# elif IP30
	info->EARev = heart_rev();
# endif
#endif /* !_STANDALONE */
	return 0;
}

/******************************************************************************
 * B O A R D   R E S E T
 *****************************************************************************/

#if HQ4
# if IP30
void Mgras_FC_Disable(void)
{
	/*
	 * Disable credit management for both flow control resources
	 */
	HEART_PIU_K1PTR->h_fc_mode &= ~(3 << 8);
}
# elif IP27
void Mgras_FC_Disable(void)
{
}
# else
!!BOMB!!
# endif
#endif

void MgrasReset(mgras_hw *base)
{
	int board = mgras_baseindex(base);
#if HQ4
# if IP27
#  if defined(_STANDALONE)
	/* XXXmdf need to clean this up */
	nasid_t		nasid = NASID_GET(base);
	xwidgetnum_t	widgetid = WIDGETID_GET((__psunsigned_t)base);
	xbow_t		*xbow = (xbow_t *)NODE_SWIN_BASE(nasid, 0);
	xbowreg_t	ctrl;
	iprb_t		prb;

	ctrl = xbow->xb_link(widgetid).link_control;
	xbow->xb_link(widgetid).link_reset = 0;
	DELAY(1000);

	/* restore link control register */
	xbow->xb_link(widgetid).link_control = ctrl;

	/*
	 * Use fire and forget for textport and reset the crossbow credit count.
	 * from hub_setup_prb in irix/kern/ml/SN0/hubio.c
	 */
	prb.reg_value = REMOTE_HUB_L(nasid, IIO_IOPRB(widgetid));
	prb.iprb_ovflow = 1;
	prb.iprb_bnakctr = 0;
	prb.iprb_anakctr = 0;
	prb.iprb_ff = 1;
	prb.iprb_xtalkctr = HQ4_XT_CREDIT; /* XXXmdf this also needs to be set in xbow */
	REMOTE_HUB_S(nasid, IIO_IOPRB(widgetid), prb.reg_value);

	/*
	 * Find a hub (any hub) connected to the gfx xbow so that we can
	 * set our DMA target id.  (if it ain't 9, it's 10.)
	 *
	 * stat.linkstatus = xbow->xb_link(9).link_status;
	 */

	/* restore widget control regester */
	base->xt_control = MgrasBoards[board].w_control;
    
#  else /* STANDALONE */
	!!BOMB!!
#  endif /* STANDALONE */
# else /* IPxx */
	/*
	 * A crosstalk link reset also resets the gfx board.
	 * Should really call xswitch_reset_link but hwgraph is not
	 * initialized when we call reset (see code below).
	 *
	 * vertex_hdl_t vhdl = MgrasBoards[board].data->dev;
	 * xswitch_reset_link(vhdl);
	 */
	int xbow_port = MgrasBoards[board].xbow_port;
	xbow_t *xbow_wid = (xbow_t *) K1_MAIN_WIDGET(XBOW_ID);
	__uint32_t link_control;

	/*
	 * Save the link control register to be restored later. The prom
	 * set the LLP widget credits and reseting will change it to 2.
	 * Also save the widget control register of HQ4.
	 */
	link_control = xbow_wid->xb_link(xbow_port).link_control;

	/*
	 * A write to the xbow link_reset register will act as a
	 * "one-shot," forcing link reset to be asserted for 4 core
	 * clock cycles.
	 */
	xbow_wid->xb_link(xbow_port).link_reset = 1;

	DELAY(1000);

	/* restore link control and widget control registers */
	xbow_wid->xb_link(xbow_port).link_control = link_control;
	base->xt_control = MgrasBoards[board].w_control;

# endif /* IPxx */

  	/* take re/pp out of reset */
	MgrasInitDCBctrl(base);
	mgras_BFIFOPOLL(base, 1);
	MGRAS_SET_BC1(base, MGRAS_BC1_REPP_RESET_BIT);
	
#else /* HQ */
	
	/*
	 * Reset the gfx board by twiddling the reset line.
	 */
	unsigned char mask;
	int slot = MgrasBoards[board].slot;
	mask = (slot == GIO_SLOT_GFX) ? PCON_SG_RESET_N: PCON_S0_RESET_N;

	*(volatile unsigned char *)PHYS_TO_K1(PORT_CONFIG) &= ~mask;
	flushbus();
	DELAY(10);
	*(volatile unsigned char *)PHYS_TO_K1(PORT_CONFIG) |= mask;
	flushbus();
	
#endif /* HQ */
	
	/* Set HQ config registers */
	mgras_hqSetHQConfig(base, HQ3_HQ_CONFIG_DEFAULT);
	mgras_hqSetGIOConfig(base, HQ3_GIO_CONFIG_DEFAULT);

	MgrasBoards[board].boardInitted = 0;
}

/******************************************************************************
 * H Q   I N I T I A L I Z A T I O N
 *****************************************************************************/

/* Because we cannot download ucode to the GE11s at earlyinit time, we
 * need to be able to initialize the HQ3 in such a way that it will think
 * there are no GE11s in the system.  Thus, the numge parameter to the
 * following function is designed to override the actual number of GE11s
 * in the system (that number is contained in the info struct).
 */

#ifdef _STANDALONE
static int ip22_bufpio_adjustment = 4; /* XXX assume it's there, to be safe */
#else
#ifndef HQ4
extern int ip22_bufpio_adjustment;
#endif
#endif

void MgrasInitHQ(register mgras_hw *base, int numge)
{
	/* Set HQ config registers registers */
	mgras_hqSetHQConfig(base, (HQ3_HQ_CONFIG_DEFAULT & ~HQ3_NUMGE_MASK) |
			     numge);
	mgras_hqSetGIOConfig(base, HQ3_GIO_CONFIG_DEFAULT);

	/* Set hi and low water marks and fifo full delays
	 * accomodate the PM5's buffered pios. The '* 2' comes
	 * from the use of fastpath commands: single writes that
	 * use two slots in the cfifo.
	 */

#ifndef HQ4
	if (ip22_bufpio_adjustment > 0)
	    base->cfifo_hw = HQ3_CFIFO_HIWATER - (ip22_bufpio_adjustment * 2);
	else
	    base->cfifo_hw = HQ3_CFIFO_HIWATER;
#else
	base->cfifo_hw = HQ_CFIFO_MAX - HQ4_FC_BIAS;
#endif

	base->cfifo_lw = HQ3_CFIFO_LOWATER;
	base->cfifo_delay =  HQ3_CFIFO_DELAY;

	base->dfifo_hw = HQ3_DFIFO_HIWATER;
	base->dfifo_lw = HQ3_DFIFO_LOWATER;
	base->dfifo_delay = HQ3_DFIFO_DELAY;
	
	base->bfifo_lw = HQ3_BFIFO_LOWATER;
#if !HQ4
	base->bfifo_hw = HQ3_BFIFO_HIWATER;
	base->bfifo_delay = HQ3_BFIFO_DELAY;
#endif

	/* formatter mode register */
	mgras_WRITECFIFO64(base, HQ_FC_PIXEL_FORMAT_MODE_WRITE, HQ_FC_CANONICAL);

	base->flag_clr_priv = ~0;	/* clear all flags */
	base->flag_enab_clr = ~0;				/* user may not touch any */
	base->flag_enab_set = (HQ_FLAG_CD_FLAG_BIT |		/* except for the CD flag */
			       HOST_FLAG_FLUSH_BEFORE_CTXSW |	/* and the flush_before_ctxsw flag */
			       HQ_FLAG_GE_FLAGS_BITS);
	/* At earlyinit time, no intr allowed */
#if HQ4
	base->flag_intr_enab_clr = ~0;	
	base->stat_intr_enab_clr = ~0;	
#else
	base->intr_enab_clr = ~0;
#endif

#ifndef _STANDALONE
	/* DMA registers */
	MgrasInitDMA(base);
#endif

	MgrasInitDCBctrl(base);
}

void MgrasInitDCBctrl( mgras_hw *base)
{
	__uint32_t pcd_ctrl;

	base->dcbctrl_pp1 =	MGRAS_DCBCTRL_PP1;
	base->dcbctrl_dac =	MGRAS_DCBCTRL_DAC;
	base->dcbctrl_cmapall = MGRAS_DCBCTRL_CMAPALL;
	base->dcbctrl_cmap0 =	MGRAS_DCBCTRL_CMAP0;
	base->dcbctrl_cmap1 =	MGRAS_DCBCTRL_CMAP1;
	base->dcbctrl_vc3 =	MGRAS_DCBCTRL_VC3;
	base->dcbctrl_bdvers =	MGRAS_DCBCTRL_BDVERS;
#if HQ4
	pcd_ctrl =		MGRAS_DCBCTRL_PCD;
	base->dcbctrl_pcd =	pcd_ctrl;
	base->ksync_ctl = 1679; /* XXXnate temporary hack to test ksync */
#endif
	
	base->bfifo_lw = HQ3_BFIFO_LOWATER;
#if !HQ4
	base->bfifo_hw = HQ3_BFIFO_HIWATER;
	base->bfifo_delay = HQ3_BFIFO_DELAY;
#endif
}

/******************************************************************************
 * R S S   &   B A C K E N D   I N I T I A L I Z A T I O N
 *****************************************************************************/

int
MgrasInitRasterBackend(mgras_hw *base, mgras_info *info)
{
	int rv;
	int boardNo = mgras_baseindex(base);

	if (info->NumREs > 0) { 
	  MgrasInitDac(base);

	  if ((rv = MgrasSyncREPPs(base, info)) != 0) {
		return rv;
	  } 
	  if ((rv = MgrasSyncPPRDRAMs(base, info)) != 0) {
		return rv;
	  } 
	  MgrasBlankScreen(base, 1);

	  MgrasSetTiming(base, info, 0, mgras_video_timing[boardNo], NULL);
	  MgrasInitVC3(base);
	  MgrasInitXMAP(base,info);
#ifdef _STANDALONE
	  MgrasInitCCR(base,info);
#else
	  MgrasInitCCR(base,info, 100);
#endif
	  MgrasInitRSS(base, info);

	  /* XMAP needs to be set up before we can DL to the CMAP */
	  MgrasInitCMAP(base);	     /* On RA bd - not there w/o RA */

	  /* Enable Display */
	  MgrasBlankScreen(base, 0);

	  return 0;
	}
	return -1;
}

#if HQ4
/*
 * PLL bytes, used for initializing the ICS1562 pixel clock doubler chip
 * on SpeedRacer.
 *
 * Which byte string is used depends on the monitor frequency:
 *
 * requency range			PLL bytes (decimal values)
 *  min freq               max freq	B0,B1,B2,B3,B4,B5,B6
 * ----------              ----------	---------------
 *  10.00 MHz <= PixClk <   12.50 MHz	05,32,00,37,09,04,00
 *  12.50 MHz <= PixClk <=  16.00 MHz	05,32,00,38,09,04,00
 *  16.00 MHz <  PixClk <=  25.00 MHz	05,16,00,37,04,02,00
 *  25.00 MHz <  PixClk <=  32.00 MHz	05,16,00,38,04,02,00
 *  32.00 MHz <  PixClk <=  50.00 MHz	05,00,00,37,04,02,01
 *  50.00 MHz <  PixClk <=  64.00 MHz	05,00,00,38,04,02,01
 *  64.00 MHz <  PixClk <= 100.00 MHz	01,00,00,37,04,02,03
 * 100.00 MHz <  PixClk <= 133.33 MHz	00,00,00,37,05,00,06
 * 133.33 MHz <  PixClk <= 160.00 MHz	00,00,00,38,05,00,06
 */
#define PCD_ARRAY_LEN 7
typedef struct _Pcd_info {
	int maxfreq;	/* rounded to nearest MHZ */
	uchar_t PCD_BYTES[PCD_ARRAY_LEN];
} Pcd_info;

static Pcd_info pcd_strings[] = {
	12, { 0x05, 0x20, 0x00, 0x25, 0x09, 0x04, 0x00 },
	16, { 0x05, 0x20, 0x00, 0x26, 0x09, 0x04, 0x00 },
	25, { 0x05, 0x10, 0x00, 0x25, 0x04, 0x02, 0x00 },
	32, { 0x05, 0x20, 0x00, 0x26, 0x09, 0x04, 0x00 },
	50, { 0x05, 0x00, 0x00, 0x25, 0x04, 0x02, 0x01 },
	64, { 0x05, 0x00, 0x00, 0x26, 0x04, 0x02, 0x01 },
	100, { 0x01, 0x00, 0x00, 0x25, 0x04, 0x02, 0x03 },
	133, { 0x00, 0x00, 0x00, 0x25, 0x05, 0x00, 0x06 },
	160, { 0x00, 0x00, 0x00, 0x26, 0x05, 0x00, 0x06 },
	0   /* maxfreq = 0 at end of table */
};

/*
 * The PLL bytes need to be converted to a serial bit stream, LSB of
 * Byte 0 first, ending with MSB of Byte 6.  Bits are written onto the
 * LSB of the DCB bus, and written to DCB device 1 using an ASYNC, no
 * ACK protocol, with setup/hold/width set to 2/2/2 or slower.
 */
void
mgras_pcd_write(mgras_hw *base, uchar_t *pPLLbyte)
{
	__uint32_t i, j;
	uchar_t twoBits, byte;

	for (i = 0; i < PCD_ARRAY_LEN; i++) {
		byte = *pPLLbyte++;
		mgras_BFIFOPOLL(base, 8);
		for (j = 0; j < 8; j++) {
			twoBits = byte & 0x1;
			if (i == PCD_ARRAY_LEN - 1 && j == 7) {
				/* the last bit on the last byte */
				twoBits |= 0x2;
			}
			base->pcd.write = twoBits;
			byte = byte >> 1;
		}
	}
}
#endif

/******************************************************************************
 * T I M I N G   T A B L E   D O W N L O A D
 *****************************************************************************/

int MgrasSetTiming(mgras_hw *base,
		   mgras_info *info,
		   int ucode_loaded,
		   struct mgras_timing_info *tinfo,
		   mgras_data * bdata)
{
    int ge, retval;
    unsigned short us_field_w,us_field_h;
    unsigned int field_h;

    
    ASSERT(base);
    ASSERT(info);
    ASSERT(tinfo);
    ASSERT(tinfo->ftab);
    ASSERT(tinfo->ltab);
    ASSERT(tinfo->dacplltab);

    /* Shut down PP1 memory/display refresh state machines */
    mgras_BFIFOPOLL(base, 4);
    mgras_xmapSetAddr(base, MGRAS_XMAP_REFCONTROL_ADDR);
    mgras_xmapSetRefControl(base, 0x0);
    mgras_xmapSetAddr(base, MGRAS_XMAP_DIBCONTROL0_ADDR);
    mgras_xmapSetDIBdata(base, 0x0);
    
    /* Reset VC3 chip */
    mgras_BFIFOPOLL(base, 1);
    MGRAS_SET_BC1(base, MGRAS_BC1_DEFAULT ^ MGRAS_BC1_VC3_RESET_BIT);

    /* start the pixel clock in the DAC */
    MgrasSetDacTiming(base, tinfo);
    
#if HQ4
    {
	uchar_t *pcd_string;
	int pixel_clock_freq = tinfo->cfreq;
	Pcd_info *pcd;
	/*
	 * Init pixel clock doubler.
	 * Initialization is dependent on timing tble being used.
	 */
	for (pcd = &pcd_strings[0]; pcd->maxfreq != 0; pcd++) {
	    if (pixel_clock_freq <= pcd->maxfreq) {
		    pcd_string = pcd->PCD_BYTES;
		    break;
	    }
	}
	if (pcd->maxfreq == 0) {
	    pcd--;
	    pcd_string = pcd->PCD_BYTES;
	    cmn_err(CE_WARN, "Pixel Clock Doubler initialized assuming \
monitor freq = %d MHZ\n", pcd->maxfreq);
	    cmn_err(CE_WARN, "Actual monitor freq = %d\n", pixel_clock_freq);
	}
	mgras_pcd_write(base, pcd_string);
    }
#endif

#ifdef HQ4
    /* Low resolution (NTSC/PAL/VGA) fmts are supportable on HQ4, but only
       if we set the pixel clock bit in the BC1 correctly */
    if(bdata) {
      if(tinfo->flag & MGRAS_LOWREZ)
	bdata->bc1_shadow |= MGRAS_BC1_SEL_NTSC_XTAL_BIT;
      else
	bdata->bc1_shadow &= ~MGRAS_BC1_SEL_NTSC_XTAL_BIT;
    }
#endif /* HQ4 */

    /* Take VC3 out of reset */
    mgras_BFIFOPOLL(base, 2);
    if(bdata) {
      MGRAS_SET_BC1(base, MGRAS_BC1_DEFAULT | bdata->bc1_shadow);
    } else {
      MGRAS_SET_BC1(base, MGRAS_BC1_DEFAULT);
    }

    mgras_vc3SetReg(base, VC3_CONFIG, VC3_UNRESET);

    /* set up the timing table in VC3 SRAM */
    MgrasSetVC3Timing(base, tinfo);

    /* turn on the timing table */
    MgrasEnableVideoTiming(base, 1);


    /* for stereo formats, the field height is
     * different than the frame height.  the GE/RE
     * fix below is relative to the field size,
     * not the frame. CalcScreenSize knows the right
     * value; we need to keep the calculation there...
     */
    MgrasCalcScreenSize(tinfo,&us_field_w,&us_field_h);
    field_h = us_field_h;

    /* load all of the timing-dependent registers in the RSS */
    if (!(tinfo->flag & MGRAS_SUPPRESS_FBCONFIG)) {
	MgrasSetRSSTiming(base, info, tinfo);
	if (ucode_loaded) {
	    bdata = MgrasBoards[mgras_baseindex(base)].data;
	    for (ge = 0; ge < bdata->info->NumGEs; ge++) {
		/* Send # of screen lines to GE (for HW workaround) */
		int space;
		space = HQ3_CFIFOHWM(base) - 7;
		mgras_CFIFOWAIT(base, space);
		mgras_WRITECFIFO64(base, MGRAS_HQ_CMDTOKEN(CP_PASS_THROUGH_GE_WRAM, 4), 2);
		mgras_WRITECFIFO32(base, 0x80000008);
		mgras_WRITECFIFO32HI(base, field_h-1);
		mgras_WRITECFIFO32HI(base, 0);	/* padding to make a doubleword */
		mgras_WRITECFIFO64(base, MGRAS_HQ_CMDTOKEN(CP_ERAM_WRITE_ABSOLUTE, 8),
		    bdata->ge11_config.em_screenres);
		mgras_WRITECFIFO32(base, 1);
		mgras_WRITECFIFO32(base, MGRAS_HQ_CMDTOKEN(CP_INC_GE, 0));
	    }
	}
    }

    /* load all of the timing-dependent registers in the XMAP */
    MgrasSetXMAPTiming(base, info, tinfo);
	    
    /* tuck away other useful info */
    MgrasRecordTiming(base, tinfo);

    /*
     * We should always report the size of the framebuffer, not the timing table
     */
    if (!(tinfo->flag & MGRAS_SUPPRESS_FBCONFIG))
            info->gfx_info.xpmax = us_field_w;
            info->gfx_info.ypmax = us_field_h;

    info->MonTiming = tinfo->refresh_rate;
#ifdef FLAT_PANEL
    if (info->MCOBdRev == MCO_BOARD_NOT_PRESENT) {
	/*
	 * Running the Corona display with the wrong video timing
	 * can damage the display.  If we have Corona, and we're
	 * not loading its timing, then turn the thing off before
	 * loading the new timing.  First though, verify a crt is
	 * attached, don't change the timing unless this is so.
	 *
	 * If we are loading Corona timing, and the panel is
	 * present, turn it on.
	 */
	/* XXX-- because reset now clears I2C status */
	mgras_fp_probe( base, info );
	if (info->PanelType == 0 ||  info->PanelType == MONITORID_NOFPD )
	    return 0;
	
	if (info->PanelType==MONITORID_CORONA || 
	    info->PanelType == MONITORID_ICHIBAN )
		    mgras_i2cPanelOn();
    }
#endif /* FLAT_PANEL */
    return 0;
}

void MgrasSetDacTiming(mgras_hw *base, struct mgras_timing_info *tinfo)
{
    unsigned char *pll_inits;
    
    /* Program DAC clock */
    pll_inits = tinfo->dacplltab;
    mgras_BFIFOPOLL(base, 6);
    mgras_dacSetAddrCmd(base, MGRAS_DAC_PLL_R_ADDR,   pll_inits[0]);
    mgras_dacSetAddrCmd(base, MGRAS_DAC_PLL_V_ADDR,   pll_inits[1]);
    mgras_dacSetAddrCmd(base, MGRAS_DAC_PLL_CTL_ADDR, pll_inits[2]);
    
    /* Delay long enough for the PLL to lock */
    DELAY(MGRAS_DAC_PLL_LOCKTIME);

    /* Delay long enough for the Mitsubishi monitor to lose lock.
       If we don't do this, switching from 60Hz to 50Hz is broken */
    DELAY(MGRAS_MITSUBISHI_MONITOR_DELAY);
}

void MgrasSetVC3Timing(mgras_hw *base, struct mgras_timing_info *tinfo)
{
    __uint32_t flag;
    
    /* If timing < 85MHz */ 
    mgras_BFIFOPOLL(base, 1);
    if (tinfo->cfreq < 85) {
	mgras_vc3SetReg(base, VC3_CONFIG, VC3_UNRESET | VC3_SLOW_CLOCK);
    } else {
	mgras_vc3SetReg(base, VC3_CONFIG, VC3_UNRESET);
    }
    
    /* Load video line table */
    MgrasLoadVC3SRAM(base, tinfo->ltab,
		     VC3_VID_LINETAB_ADDR,  tinfo->ltab_len); 
    
    /* Load and set pointer to video frame table */
    mgras_BFIFOPOLL(base, 1);
    mgras_vc3SetReg(base, VC3_VID_EP, VC3_VID_FRAMETAB_ADDR);
    MgrasLoadVC3SRAM(base, tinfo->ftab,
		     VC3_VID_FRAMETAB_ADDR,  tinfo->ftab_len);
    
    mgras_BFIFOPOLL(base, 3);

    /* Set horizontal resolution */
    mgras_vc3SetReg(base, VC3_SCAN_LENGTH, tinfo->w << 5);
    
    /* Load DID frame table pointers */
    mgras_vc3SetReg(base, VC3_DID_EP1, VC3_DID_FRAMETAB1_BASE);
    mgras_vc3SetReg(base, VC3_DID_EP2, VC3_DID_FRAMETAB2_BASE);
    
    /* Set up video timing control value */
    flag =  VC3_VIDEO_ENAB | VC3_DID_ENAB1 | VC3_DID_ENAB2 | VC3_BLACKOUT;
    if (tinfo->flag & MGRAS_INTERLACED)
	flag |= VC3_INTERLACED;
    if (tinfo->flag & MGRAS_GENSYNC) {
	flag |= VC3_GENSYNC;
	/*  XXX genlock?? */
	if (tinfo->flag & MGRAS_GENLOCK)
	    flag |= VC3_GENLOCK;
    }
    VC3_display_control = flag;
}

void MgrasSetXMAPTiming(mgras_hw *base,
			mgras_info *info,
			struct mgras_timing_info *tinfo)
{
    mgras_BFIFOPOLL(base, 3);
    mgras_xmapSetPP1Select(base,MGRAS_XMAP_WRITEALLPP1);
    
    /* Turn on the FIFO bypass mode */ 
    mgras_xmapSetAddr(base, 0x1);		
    mgras_xmapSetConfigByte(base, 0x48);
    
    /* Set the Refresh Control */
    mgras_BFIFOPOLL(base, 4);
    mgras_xmapSetAddr(base, MGRAS_XMAP_REFCONTROL_ADDR);
    mgras_xmapSetRefControl(base, tinfo->REFctl);
    
    /* Set the DIBSkip */
    mgras_xmapSetAddr(base, MGRAS_XMAP_DIBSKIP_ADDR);
    mgras_xmapSetDIBdata(base, tinfo->DIBskip);
    
    /* Set the DIBPtrs */
    if (!(tinfo->flag & MGRAS_SUPPRESS_FBCONFIG)) {
	mgras_BFIFOPOLL(base, 2);
	mgras_xmapSetAddr(base, MGRAS_XMAP_DIBPTRS_ADDR);
	mgras_xmapSetDIBdata(base, tinfo->DIBptr);
    }
    
    /* Set the DIBTopScan */
    mgras_BFIFOPOLL(base, 2);
    mgras_xmapSetAddr(base, MGRAS_XMAP_DIBTOPSCAN_ADDR);
    mgras_xmapSetDIBdata(base, tinfo->DIBtopscan);
    
    /* Set the DIB Control registers */
    mgras_BFIFOPOLL(base, 4);
    mgras_xmapSetAddr(base, MGRAS_XMAP_DIBCONTROL0_ADDR);
    mgras_xmapSetDIBdata(base, tinfo->DIBctl0  | MGRAS_XMAP_DISPLAY_ENABLE_BIT);
    mgras_xmapSetAddr(base, MGRAS_XMAP_DIBCONTROL1_ADDR);
    mgras_xmapSetDIBdata(base, tinfo->DIBctl1);
    mgras_BFIFOPOLL(base, 4);
    mgras_xmapSetAddr(base, MGRAS_XMAP_DIBCONTROL2_ADDR);
    mgras_xmapSetDIBdata(base, tinfo->DIBctl2);
    mgras_xmapSetAddr(base, MGRAS_XMAP_DIBCONTROL3_ADDR);
    mgras_xmapSetDIBdata(base, tinfo->DIBctl3);
    
    /* Turn off the FIFO bypass mode */
    mgras_BFIFOPOLL(base, 2);
    mgras_xmapSetAddr(base, MGRAS_XMAP_CONFIG_ADDR);
    mgras_xmapSetConfig(base, MGRAS_XMAP_CONFIG_DEFAULT(info->NumREs));
}

void MgrasSetRSSTiming(mgras_hw *base, 
			mgras_info *info,
		       struct mgras_timing_info *tinfo)
{
    ppdrbsizeu drbsize;
  
    /* Clean out the FIFOs */
    mgras_BFIFOWAIT(base, HQ3_BFIFO_MAX);

    /* turn off the timing table */
    MgrasEnableVideoTiming(base, 0);

    /* Change the DRBsize */
    drbsize.bits.DRB_Xtiles9 = (tinfo->w + 767) / 768;
    drbsize.bits.DRB_Xtiles36 = (tinfo->w + 191) / 192;
    drbsize.bits.DRB_RSS = ((info->NumREs == 2) ? PP_2_RSS : PP_1_RSS);
    drbsize.bits.DMA_line_count_mask = ((info->NumREs == 2) ?
					  PP_DMA_LCM_2_RSS : PP_DMA_LCM_1_RSS);
    drbsize.bits.AutoFlushEnab_n = 0; /* enabled */
    mgras_re4Set(base, DRBsize.val, drbsize.val);

    /* Change the DRBpointers */
    mgras_re4Set(base, DRBpointers.val, tinfo->DRBptrs.main);
    
    /* turn on the timing table */
    MgrasEnableVideoTiming(base, 1);
}

void MgrasRecordTiming(mgras_hw *base, struct mgras_timing_info *tinfo)
{
    struct mgras_timing_info *tbl;
    mgras_DRBptrs DRBptrs;

    /* remember this timing table */
    tbl = mgras_video_timing[mgras_baseindex(base)];
    if (tinfo->flag & MGRAS_SUPPRESS_FBCONFIG) {
	ASSERT(tbl);
	bcopy(&tbl->DRBptrs, &DRBptrs, sizeof(mgras_DRBptrs));
    }
    if (tbl != tinfo) {
	    if (tbl) {
		    kern_free(tbl->ftab);
		    kern_free(tbl->ltab);
		    kern_free(tbl->dacplltab);
		    kern_free(tbl);
	    }
	    tbl = (struct mgras_timing_info *)
		    kern_malloc (sizeof(struct mgras_timing_info));
	    bcopy(tinfo, tbl, sizeof(struct mgras_timing_info));
	    tbl->ftab = (unsigned short *)
		    kern_calloc (tbl->ftab_len, sizeof(unsigned short *));
	    bcopy(tinfo->ftab, tbl->ftab, tbl->ftab_len*sizeof(unsigned short));
	    tbl->ltab = (unsigned short *)
		    kern_calloc (tbl->ltab_len, sizeof(unsigned short *));
	    bcopy(tinfo->ltab, tbl->ltab, tbl->ltab_len*sizeof(unsigned short));
	    tbl->dacplltab = (unsigned char *)
		    kern_calloc (3, sizeof(unsigned char *));
	    bcopy(tinfo->dacplltab, tbl->dacplltab, 3*sizeof(unsigned char));
	    if (tinfo->flag & MGRAS_SUPPRESS_FBCONFIG) {/* retain old DRBptrs */
		bcopy(&DRBptrs, &tbl->DRBptrs, sizeof(mgras_DRBptrs));
	    }
	    mgras_video_timing[mgras_baseindex(base)] = tbl;
    }
}

void MgrasInitDac(register mgras_hw *base)
{
	int i;
	mgras_BFIFOWAIT(base, HQ3_BFIFO_MAX);

	/* Reset DAC */
	mgras_BFIFOPOLL(base, 3);
	mgras_dacSetMode(base, MGRAS_DAC_MODE_INIT);	
	mgras_dacSetMode(base, MGRAS_DAC_MODE_INIT & ~MGRAS_DAC_MODE_RESET_BIT);
	mgras_dacSetMode(base, MGRAS_DAC_MODE_INIT);	
	mgras_BFIFOPOLL(base, 4);
	mgras_dacSetAddrCmd(base, 0x5, 0x89);
	mgras_dacSetAddrCmd16(base,MGRAS_DAC_CMD5_ADDR, 0x40);
	/* Must be set on for PLL  - at power */

	/* Initialize DAC */
	mgras_BFIFOPOLL(base, 2);
	mgras_dacSetAddrCmd16(base,MGRAS_DAC_PIXMASK_ADDR,MGRAS_DAC_PIXMASK_INIT);

	/* 24 bit RGB True Color*/
	mgras_BFIFOPOLL(base, 4);
	mgras_dacSetAddrCmd16(base,MGRAS_DAC_CMD1_ADDR, MGRAS_DAC_CMD1_INIT);	
	mgras_dacSetAddrCmd16(base,MGRAS_DAC_CMD2_ADDR, MGRAS_DAC_CMD2_INIT);	

	mgras_dacSetAddrCmd16(base,MGRAS_DAC_CMD3_ADDR, MGRAS_DAC_CMD3_INIT);	

	/* 4:1 mux */
	mgras_BFIFOPOLL(base, 6);
	mgras_dacSetAddrCmd16(base,MGRAS_DAC_CMD4_ADDR, MGRAS_DAC_CMD4_INIT);	
	mgras_dacSetAddrCmd16(base,MGRAS_DAC_CURS_ADDR, MGRAS_DAC_CURS_DISABLE);

	/* Set up Gamma Tables to a ramp for now (Addr16 to clear hi bits) */
	mgras_dacSetAddr16(base, 0x0);	

	for (i = 0; i < 256 ; i++) {
		ushort_t pixval = ((i << 2) | 0x02);  /* need 10-bit vals */
		if ((i & 0x1) == 0)
			mgras_BFIFOPOLL(base, 6);
		/* dacSetRGB is 3 DCB writes */
		mgras_dacSetRGB(base,pixval,pixval,pixval);
	}
}

/*
 * Init VC3 DID tables.
 * Note that this is called AFTER MgrasSetTiming,
 *	which loads a timing table into VC3 SRAM.
 */
void MgrasInitVC3(register mgras_hw *base)
{
	int i;
	int board = mgras_baseindex(base);
	mgras_timing_info *mgras_timing = mgras_video_timing[board];

	/* Set DID 0, starting at x=0, END-OF-LINE flag << 5  */
	unsigned short initdid[] = { 0x0000, 0x7ff << 5 };

	/*
	** Load MAIN DID Frame table with line table from timing tbl
	 */
	mgras_BFIFOPOLL(base, 1);
	mgras_vc3SetReg(base,VC3_RAM_ADDR,VC3_DID_FRAMETAB1_BASE);

	mgras_BFIFOWAIT(base, HQ3_BFIFO_MAX);
	i = mgras_timing->h;
	while (i-- > 0) {
		mgras_BFIFOPOLL(base, 1);
		mgras_vc3SetRam(base, VC3_DID_LINETAB1_BASE);
	}
	mgras_BFIFOPOLL(base, 2);
	mgras_vc3SetRam(base, 0xffff); /* End of Line Table flag */	

	/*
	** Load OVERLAY DID Frame table with line table ptr from timing tbl
	*/
	mgras_vc3SetReg(base,VC3_RAM_ADDR, VC3_DID_FRAMETAB2_BASE);
	i = mgras_timing->h;
	while (i-- > 0) {
		mgras_BFIFOPOLL(base, 1);
		mgras_vc3SetRam(base, VC3_DID_LINETAB2_BASE);
	}
	mgras_BFIFOPOLL(base, 1);
	mgras_vc3SetRam(base, 0xffff); /* End of Line Table flag */	

	/* Set DID 0 and frame table pointers*/
	MgrasLoadVC3SRAM(base, initdid, VC3_DID_LINETAB1_BASE, 
					sizeof(initdid)/sizeof(short) ); 
	MgrasLoadVC3SRAM(base, initdid, VC3_DID_LINETAB2_BASE, 
					sizeof(initdid)/sizeof(short) ); 
	MgrasClearCursor(base);
#if defined(_STANDALONE)
	MgrasInitCursor(base);
#endif

	mgras_BFIFOPOLL(base, 2);
#if defined(_STANDALONE)
	/* enable the cursor */
	mgras_vc3SetReg(base, VC3_CURS_CONTROL,
			(VC3_ENA_CURS_FUNCTION| VC3_ENA_CURS_DISPLAY));
#else
	/* Do not enable the cursor. The X server will enable */
	mgras_vc3SetReg(base, VC3_CURS_CONTROL,
			~(VC3_ENA_CURS_FUNCTION| VC3_ENA_CURS_DISPLAY));
#endif
	
	/* Unmask the timing pipes */
#ifdef FLAT_PANEL
	mgras_cmapSetCmd(base,
		    MGRAS_CMAP_CMD_UNMASK_PIPES&(~MGRAS_CMAP_CMD_RESET_1));
#else
	mgras_cmapSetCmd(base, MGRAS_CMAP_CMD_UNMASK_PIPES);
#endif
}

void MgrasInitXMAP(register mgras_hw *base,mgras_info *info)
{
	int i;

	mgras_BFIFOPOLL(base, 1);
	mgras_xmapSetPP1Select(base,MGRAS_XMAP_WRITEALLPP1);

	mgras_BFIFOPOLL(base, 6);

	/* Turn on the FIFO bypass mode */ 
	mgras_xmapSetAddr(base, 0x1);		
	mgras_xmapSetConfigByte(base, 0x48);

	/* Set the retry time */
	mgras_xmapSetAddr(base, MGRAS_XMAP_RETRYTIME_ADDR);
	mgras_xmapSetRetryTime(base, MGRAS_XMAP_RETRYTIME_DEFAULT);

	/* Set the RAC Control */
	mgras_xmapSetAddr(base, MGRAS_XMAP_RERACCONTROL_ADDR);
	mgras_xmapSetRE_RAC(base, MGRAS_XMAP_RACCONTROL_DEFAULT);

#ifdef NOTDEF
	/* Set the Refresh Control */
	mgras_BFIFOWAIT(base, 2);
	mgras_xmapSetAddr(base, MGRAS_XMAP_REFCONTROL_ADDR);
	mgras_xmapSetRefControl(base, MGRAS_XMAP_REFCONTROL_DEFAULT);
#endif

	mgras_BFIFOPOLL(base, 6);

	/* Set Config Register */
	mgras_xmapSetAddr(base, MGRAS_XMAP_CONFIG_ADDR);
	mgras_xmapSetConfig(base, MGRAS_XMAP_CONFIG_DEFAULT(info->NumREs) |
			    MGRAS_XMAP_FIFO_BYPASS_BIT);

	/* Set the Buffer Selects */
	mgras_xmapSetAddr(base, MGRAS_XMAP_MAINBUF_ADDR);
	mgras_xmapSetBufSelect(base, MGRAS_XMAP_MAINBUF_DEFAULT);
	mgras_xmapSetAddr(base, MGRAS_XMAP_OVERLAYBUF_ADDR);
	mgras_xmapSetBufSelect(base, MGRAS_XMAP_OVERLAYBUF_DEFAULT);

#if 0  /* RGBA for verif suite */
	/* Initialize DID 0 to 8-bit CI */
	mgras_BFIFOPOLL(base, 2);
	mgras_xmapSetMainMode(base,0, MGRAS_XMAP_8BIT_CI);
#else
	/* Set all DIDs to 32-bit RGBA */
	mgras_BFIFOWAIT(base, HQ_BFIFO_MAX);	/* Clean out the BFIFO */
	for (i = 0; i < 32; i++) {
	    if ((i & (HQ_BFIFO_HIWATER-1)) == (HQ_BFIFO_HIWATER-1))
		mgras_BFIFOWAIT(base, HQ_BFIFO_MAX);
	    mgras_xmapSetMainMode(base, i, MGRAS_XMAP_12BIT_RGB);
	}
#endif
	/* Disable overlay. Let Xserver initialize */
	mgras_BFIFOWAIT(base, HQ_BFIFO_MAX);
	for (i = 0; i < 8; i++)	 {
	    if ((i & (HQ_BFIFO_HIWATER-1)) == (HQ_BFIFO_HIWATER-1))
		mgras_BFIFOWAIT(base, HQ_BFIFO_MAX);
	    mgras_xmapSetOvlMode(base, i, MGRAS_XMAP_OVL_DISABLE);
	}

	/* Turn off the FIFO bypass mode */
	mgras_BFIFOPOLL(base, 2);
	mgras_xmapSetAddr(base, MGRAS_XMAP_CONFIG_ADDR);
	mgras_xmapSetConfig(base, MGRAS_XMAP_CONFIG_DEFAULT(info->NumREs));
}

void
MgrasInitCMAP(mgras_hw *base)
{
	__uint32_t i, addr, pixval;
	
	addr = MGRAS_8BITCMAP_BASE;
	
	/* Unmask the timing pipes */
	mgras_BFIFOPOLL(base, 1);
#ifdef FLAT_PANEL
	mgras_cmapSetCmd(base,
	    MGRAS_CMAP_CMD_UNMASK_PIPES&(~MGRAS_CMAP_CMD_RESET_1));
#else
	mgras_cmapSetCmd(base, MGRAS_CMAP_CMD_UNMASK_PIPES);
#endif

	/* First Drain the BFIFO - DCB FIFO */
	mgras_BFIFOWAIT(base, HQ3_BFIFO_MAX);

	/* Force CBlank High (to prevent update of CMAP fifo - bug) */
	mgras_xmapSetPP1Select(base,MGRAS_XMAP_WRITEALLPP1);
	mgras_xmapSetAddr(base, MGRAS_XMAP_DIBTOPSCAN_ADDR);
	mgras_xmapSetDIBdata(base, MGRAS_XMAP_DIBTOPSCAN_DEFAULT |
			     MGRAS_XMAP_DISABLE_CBLANK_BIT);
	
	/* load the CMAP with a ramp */
	for (i = 0; i < MGRAS_CMAP_NCOLMAPENT; i++, addr++) {
		
		/* Setting the address twice every time we check cmapFIFOWAIT */
		/* is a s/w work around for the cmap problem. The address     */
		/* is lost ramdomly when the bus is switched from write to    */
		/* read mode or vice versa.				      */
		
		/* Every 16 colors, check cmap FIFO.
		 * Need to give 2 dummy writes after the read.
		 */
		if ((i & 0x0F) == 0x00) {
		  mgras_BFIFOPOLL(base, 2);
		  /* Let CBlank go low, so CMAP can drain fifo */
		  mgras_xmapSetAddr(base, MGRAS_XMAP_DIBTOPSCAN_ADDR);
		  mgras_xmapSetDIBdata(base, MGRAS_XMAP_DIBTOPSCAN_DEFAULT);

		  /* cmapFIFOWAIT calls BFIFOWAIT */
		  mgras_cmapFIFOWAIT(base);

		  mgras_cmapSetAddr(base, addr);
		  mgras_cmapSetAddr(base, addr);

		  /* Force CBlank High (to prevent update of CMAP fifo - bug) */
		  mgras_xmapSetAddr(base, MGRAS_XMAP_DIBTOPSCAN_ADDR);
		  mgras_xmapSetDIBdata(base, MGRAS_XMAP_DIBTOPSCAN_DEFAULT |
				       MGRAS_XMAP_DISABLE_CBLANK_BIT);
		}

		pixval = i & 0xff;
		mgras_BFIFOPOLL(base, 2);
		mgras_cmapSetAddr(base, addr);
		mgras_cmapSetRGB(base, pixval, pixval, pixval);
	}

	/* Put the XMAP back to normal */
	mgras_BFIFOPOLL(base, 2);
	mgras_xmapSetAddr(base, MGRAS_XMAP_DIBTOPSCAN_ADDR);
	mgras_xmapSetDIBdata(base, MGRAS_XMAP_DIBTOPSCAN_DEFAULT);
}
#ifdef MFG_USED
#define REPP_SYNC_TIME_OUT      300
#endif

int
MgrasSyncREPP(register mgras_hw *base, int NumREs, int re4_id, int pp1_id)
{
  unsigned int signature, bus_word;
  unsigned int rac_addr;
  unsigned int a,b,c;
#ifdef MFG_USED
  int i;
  printf(" mgbase in the MgrasSyncREPP function 0x%llx \n", base);
#endif
  /* select the right PP1 to listen to the DCB */
  mgras_BFIFOPOLL(base, 1);
  mgras_xmapSetPP1Select(base,
			 MGRAS_XMAP_SELECT_PP1((re4_id << 1) | pp1_id));

  /* turn on auto-increment and set the RAC control register in the xmap */
  mgras_BFIFOPOLL(base, 4);
  mgras_xmapSetAddr(base, 0x1);		     /* AI is off, so we set the addr */
  mgras_xmapSetConfigByte(base, 0x08);	     /* to hit that byte manually */
  mgras_xmapSetAddr(base, 0x0);
  mgras_xmapSetConfig(base, MGRAS_XMAP_CONFIG_DEFAULT(NumREs));
  mgras_BFIFOPOLL(base, 4);
  mgras_xmapSetAddr(base, MGRAS_XMAP_RERACCONTROL_ADDR);
  mgras_xmapSetRE_RAC(base, MGRAS_XMAP_RACCONTROL_REPPSYNC);
  mgras_xmapSetAddr(base, MGRAS_XMAP_RETRYTIME_ADDR);
  mgras_xmapSetRE_RAC(base, MGRAS_XMAP_RETRYTIME_DEFAULT);
  
  /* select Broadcast to all RE4s */
  mgras_CFIFOWAIT(base, HQ3_CFIFO_MAX);
  mgras_re4Set(base, config.val, CFG_BCAST);
  
  /* set the RAC control register in the RE */
  rac_addr = ((pp1_id == 0) ? MGRAS_RE4_DEVICE_RACCONTROL_A :
	      MGRAS_RE4_DEVICE_RACCONTROL_B);
  mgras_re4Set(base, device_addr, rac_addr);
  mgras_re4Set(base, device_data, MGRAS_RE4_RACCONTROL_SYNC);
  
  signature = 0;
#ifdef MFG_USED
/*
 * According to Bob Prevett, Repp_sync only needs 3 times
 * and shouldn't be in a forever loop
 */
  i = 0;
  while ((signature != 0x1) && (i < REPP_SYNC_TIME_OUT)) {
#else
  while (signature != 0x1) {
#endif

#ifdef _STANDALONE
    /* we need this for the PROM to function */
    DELAY(1000);
#endif

    /* Turn auto-incrementing off */
    mgras_BFIFOPOLL(base, 2);
    mgras_xmapSetAddr(base, 0x1);
    mgras_xmapSetConfigByte(base, 0x00);
    
    /* Do some magic to read the signature the RE is broadcasting */
    /* See PP1 spec for details on this operation */
    mgras_BFIFOPOLL(base, 1);
    mgras_xmapSetAddr(base, MGRAS_XMAP_RERACSIGNATURE_ADDR); 
    base->dcbctrl_pp1 = MGRAS_DCBCTRL_PP1_RESYNC;
    mgras_BFIFOPOLL(base, 1);
    mgras_xmapGetRE_RAC(base, bus_word);
    base->dcbctrl_pp1 = MGRAS_DCBCTRL_PP1;

    /* Turn Auto-incrementing back on */
    mgras_BFIFOPOLL(base, 4);
    mgras_xmapSetAddr(base, 0x1);
    mgras_xmapSetConfigByte(base, 0x08);
    mgras_xmapSetAddr(base, 0x0);
    mgras_xmapSetConfig(base, MGRAS_XMAP_CONFIG_DEFAULT(NumREs));
    
    /* extract the signature from the word we just read */
    a = EXTRACT_BITS(bus_word,  3,  0);
    b = EXTRACT_BITS(bus_word, 11,  8);
    c = EXTRACT_BITS(bus_word, 19, 16);
    signature = (a == b) ? a : c; /* see the PP1 spec for details */

#if 0
    if (signature == 0) {
	return (-1);
    }
#endif
    
    if (signature != 0x1) {	      /* need to shift the bus timing */
      mgras_BFIFOPOLL(base, 2);
      mgras_xmapSetAddr(base, MGRAS_XMAP_RERACCONTROL_ADDR);
      mgras_xmapSetRE_RAC(base, MGRAS_XMAP_RACCONTROL_REPPSYNC_SHIFT);
      DELAY(MGRAS_PP1_PLL_LOCKTIME); /* XXX Tune this parameter */
      mgras_BFIFOPOLL(base, 2);
      mgras_xmapSetAddr(base, MGRAS_XMAP_RERACCONTROL_ADDR);
      mgras_xmapSetRE_RAC(base, MGRAS_XMAP_RACCONTROL_REPPSYNC);
#ifdef MFG_USED
      if (i == 3)
	printf("Warning: Repp_sync retrying\n: re4_id = %d, pp1_id = %d\n",re4_id,pp1_id);
      i++;
#endif
    }
  }
  
  /* turn off the RE broadcast and reconfigure the PP1 RE_RAC */
  mgras_BFIFOPOLL(base, 2);
  mgras_re4SetDevice(base, rac_addr, MGRAS_RE4_RACCONTROL_DEFAULT);
  mgras_xmapSetAddr(base, MGRAS_XMAP_RERACCONTROL_ADDR); 
  mgras_xmapSetRE_RAC(base, MGRAS_XMAP_RACCONTROL_DEFAULT);

  /* put the PP1 select back to the default */
  mgras_BFIFOPOLL(base, 1);
  mgras_xmapSetPP1Select(base,MGRAS_XMAP_WRITEALLPP1);

#ifdef MFG_USED
  if(signature != 1)
  {
  /* If Repp_sync failed, printout which one failed */
     printf("Repp_sync failed: re4_id = %d, pp1_id = %d\n",re4_id,pp1_id);
     return(-1);
  }
  else 
     return 0;
#else
  return 0;
#endif
}

int
MgrasSyncREPPs(register mgras_hw *base, mgras_info *info)
{
  unsigned int re4_id, pp1_id;
  int rv;
  
  for (re4_id = 0; re4_id < info->NumREs; re4_id++) {
    for (pp1_id = 0; pp1_id < 2; pp1_id++) {
      if ((rv = MgrasSyncREPP(base, info->NumREs, re4_id, pp1_id)) != 0) {
	return rv;
       }
    }
  }
  return 0;
}

int
MgrasSyncPPRDRAMs(register mgras_hw *base, mgras_info *info)
{
  unsigned int curr_val, curr_field;
  unsigned int which_re, which_pp, which_ram;
  unsigned int row;
  /*REFERENCED*/
  unsigned int dummy_data;

/*
 * XXX DCS - this code is different from kernel code!
 */
#define ALL_PP1_RACCONTROLS  (MGRAS_RE4_DEVICE_PP1_BCAST|MGRAS_PP1_RACCONTROL)
#define ALL_RDRAMS       (MGRAS_RE4_DEVICE_PP1_BCAST|MGRAS_PP1_RDRAM_BCAST_REG)
#define ALL_RDRAM_DELAYS (ALL_RDRAMS|MGRAS_RDRAM_DELAY)
#define ALL_RDRAM_DEVIDS (ALL_RDRAMS|MGRAS_RDRAM_DEVICEID)
#define ALL_RDRAM_MODES  (ALL_RDRAMS|MGRAS_RDRAM_MODE)
#define ALL_RDRAM_MEMS   (MGRAS_RE4_DEVICE_PP1_BCAST|MGRAS_PP1_RDRAM_BCAST_MEM)
  
  /* talk to all of the REs at once */
  mgras_CFIFOWAIT(base, HQ3_CFIFO_MAX);
  while (!(base->status & 0x2))
	/* spin until RSS's are idle */ ;
  mgras_re4Set(base, config.val, CFG_BCAST); /**/
  
  /* Set up the PP1s for PIO */
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, MGRAS_PP1_RACCONTROL_SYNC);
  mgras_re4Set(base, PixCmd, MGRAS_PP1_PIXCMD_SYNC);
  mgras_re4Set(base, pp1fillmode.val, MGRAS_PP1_FILLMODE_SYNC);
  mgras_re4Set(base, pp1winmode.val, MGRAS_PP1_WINMODE_SYNC);
  mgras_re4Set(base, TAGmode.val, MGRAS_PP1_TAGMODE_SYNC);
  mgras_re4Set(base, DRBpointers.val, MGRAS_PP1_DRBPTRS_SYNC);
  mgras_re4Set(base, DRBsize.val, MGRAS_PP1_DRBSIZE_SYNC);
  
  /* Broadcast to all PP1s */
  mgras_BFIFOPOLL(base, 3);
  mgras_xmapSetPP1Select(base, MGRAS_XMAP_SELECT_BROADCAST);
  mgras_xmapSetAddr(base, MGRAS_XMAP_RETRYTIME_ADDR);
  mgras_xmapSetRetryTime(base, MGRAS_PP1_RETRYTIME_SYNC);
  
  /* NOTE: there are a LOT of non-symbolic constants in the following
     code.  These are all taken from the PP1 spec.  So are the comments.
     */
  mgras_CFIFOWAIT(base, HQ3_CFIFO_MAX);
  DELAY(MGRAS_PP1_TPAUSE);				    /* core settle */
  DELAY(MGRAS_PP1_TLOCKRESET);				    /* clocks settle */
  
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x00344);   /* cctlld=1 */
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x00244);   /* cctlld=0 */
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x80244);   /* ForceBE=1 */
  mgras_CFIFOWAIT(base, HQ3_CFIFO_MAX);
  DELAY(MGRAS_PP1_TMODEARMAX);				    /* reset RAMs */
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x00244);   /* ForceBE=0 */
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x40244);   /* preinit=1 */
  mgras_CFIFOWAIT(base, HQ3_CFIFO_MAX);
  DELAY(MGRAS_PP1_TINTRESET);				    /* rst internal */
  DELAY(MGRAS_PP1_TLOCKRESET);
  
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x40344);   /* cctlld=1 */
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x40244);   /* cctlld=0 */
  mgras_re4SetDevice(base, ALL_RDRAM_DELAYS, 0x00000800);   /* preinit delay */
  
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x00344);   /* cctlld=1 */
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x00244);   /* cctlld=0 */
  mgras_re4SetDevice(base, ALL_RDRAM_DELAYS, 0x08183828);   /* postinit delay */
  
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x00344);   /* cctlld=1 */
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x00244);   /* cctlld=0 */
  mgras_re4SetDevice(base, ALL_RDRAM_DEVIDS,    0x0    );
  
  if (MGRAS_PP1_BUSFREQ == MGRAS_450MHz)
    curr_val = MGRAS_PP1_CURRVAL450;
  else
    curr_val = MGRAS_PP1_CURRVAL500;
  
  curr_field = (((curr_val & 0x20) ? (0x1 << 15) : 0) |
		((curr_val & 0x10) ? (0x1 << 23) : 0) |
		((curr_val & 0x08) ? (0x1 << 31) : 0) |
		((curr_val & 0x04) ? (0x1 << 14) : 0) |
		((curr_val & 0x02) ? (0x1 << 22) : 0) |
		((curr_val & 0x01) ? (0x1 << 30) : 0) |
		0x000000c2);
  
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x00344);   /* cctlld=1 */
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x00244);   /* cctlld=0 */
  mgras_CFIFOWAIT(base, HQ3_CFIFO_MAX);
  for (row = 0; row < 64; row++) { /* XXX Hack for PP1 HW problem */
    mgras_re4SetDevice(base, ALL_RDRAM_MODES, curr_field);
    DELAY(1000);
  }

  /* individually set RasInterval for each RDRAM */
  mgras_CFIFOWAIT(base, HQ3_CFIFO_MAX);
  for (which_re = 0; which_re < info->NumREs; which_re++) {
    mgras_re4Set(base, config.val, which_re);
    for (which_pp = 0; which_pp < 2; which_pp++) {
      unsigned int pp_addr = ((which_pp == 0) ? MGRAS_RE4_DEVICE_PP1_A :
			      MGRAS_RE4_DEVICE_PP1_B);
      for (which_ram = 0; which_ram < 3; which_ram++) {
	unsigned int rdram_addr = pp_addr | MGRAS_PP1_RDRAM_N_REG(which_ram);
	unsigned int rdram_mfr_id;

	mgras_re4SetDevice(base, pp_addr | MGRAS_PP1_RACCONTROL, 0x00344);
	mgras_re4SetDevice(base, pp_addr | MGRAS_PP1_RACCONTROL, 0x00244);
	mgras_re4GetDevice(base, rdram_addr | MGRAS_RDRAM_MANUFACTURER,
			   rdram_mfr_id);
	if (EXTRACT_BITS(rdram_mfr_id, 31, 16) == MGRAS_PP1_NEC) {
	  mgras_re4SetDevice(base, rdram_addr | MGRAS_RDRAM_RASINTERVAL,
			     MGRAS_RASINTERVAL_NEC);
	} else {
	  mgras_re4SetDevice(base, rdram_addr | MGRAS_RDRAM_RASINTERVAL,
			     MGRAS_RASINTERVAL_TOSHIBA);
	}
      }
    }
  }

  /* talk to all REs again */
  mgras_re4Set(base, config.val, CFG_BCAST);
  mgras_re4SetDevice(base, ALL_PP1_RACCONTROLS, 0x30244);

  /* to init the RDRAM cores, read first 8 lines of each */
  for (row = 0; row < 8; row++) {
    mgras_re4GetDevice(base, ALL_RDRAM_MEMS + (2048*row), dummy_data);
  }

  return 0;
}

void MgrasInitRSS(register mgras_hw *base, mgras_info *info)
{
  rss_single s;

  bzero(&s, sizeof(rss_single));
  
  mgras_CFIFOWAIT(base, HQ3_CFIFO_MAX);

  s.config.bits.DestRE = CONFIG_RE4_ALL;
  s.config.bits.Yflip = CONFIG_YINC_BTOT;
  s.config.bits.BresRound = 0x00;
  s.config.bits.IterBias = 0;
  mgras_re4Set(base, config.val, s.config.val);
  
  s.PixCmd = PP_PIXCMD_DRAW;
  mgras_re4Set(base, PixCmd, s.PixCmd);
  
  mgras_re4Set(base, ColorMaskMSBs,	0x00000000);
  mgras_re4Set(base, ColorMaskLSBsA,	0xffffffff);
  mgras_re4Set(base, ColorMaskLSBsB,	0xffffffff);
  mgras_re4Set(base, BlendFactor.val,	0x58840000);

  s.Stencilmode.bits.St_func = PP_SF_ALWAYS;
  s.Stencilmode.bits.St_fail = PP_ST_KEEP;
  s.Stencilmode.bits.St_pass = PP_ST_KEEP;
  s.Stencilmode.bits.St_Zpass = PP_ST_KEEP;
  s.Stencilmode.bits.St_Ref = 0;
  mgras_re4Set(base, Stencilmode.val, s.Stencilmode.val);

  s.Stencilmask.bits.St_Cmpmask = 0xff;
  s.Stencilmask.bits.St_Wmask = 0xff;
  mgras_re4Set(base, Stencilmask.val, s.Stencilmask.val);

  s.Zmode.bits.Zfunc = PP_ZF_LESS;
  s.Zmode.bits.Zsrc = 0;
  s.Zmode.bits.Zwritemask = 0xffffff;
  mgras_re4Set(base, Zmode.val,	s.Zmode.val);

  s.Afuncmode.bits.AfuncCmp = PP_AF_ALWAYS;
  s.Afuncmode.bits.AfuncRef = 0;
  mgras_re4Set(base, Afuncmode.val, 0x00000007);

  s.Accmode.bits.ACC_size = PP_ACC_121212;
  s.Accmode.bits.ACC_op = PP_ACC_ACCUM;
  s.Accmode.bits.ACC_value = 0;
  mgras_re4Set(base, Accmode.val, 0x00000000);
  
  s.TAGmode.bits.TAGbits_FB = 0;
  s.TAGmode.bits.TAGbits_Z = 0;
  s.TAGmode.bits.TAGen_FB = 0;
  s.TAGmode.bits.TAGen_Z = 0;
  mgras_re4Set(base, TAGmode.val, s.TAGmode.val);

  s.TAGdata_R.bits.RedNoIncr = 0x19;
  s.TAGdata_R.bits.RedIncr = 0x1a;
  s.TAGdata_R.bits.RedFrac = 0x8;
  mgras_re4Set(base, TAGdata_R.val, s.TAGdata_R.val);

  s.TAGdata_G.bits.GreenNoIncr = 0x7f;
  s.TAGdata_G.bits.GreenIncr = 0x80;
  s.TAGdata_G.bits.GreenFrac = 0x8;
  mgras_re4Set(base, TAGdata_G.val, s.TAGdata_G.val);

  s.TAGdata_B.bits.BlueNoIncr = 0x7f;
  s.TAGdata_B.bits.BlueIncr = 0x80;
  s.TAGdata_B.bits.BlueFrac = 0x8;
  mgras_re4Set(base, TAGdata_B.val, s.TAGdata_B.val);

  s.TAGdata_A.bits.AlphaNoIncr = 0xff;
  s.TAGdata_A.bits.AlphaIncr = 0x00;
  s.TAGdata_A.bits.AlphaFrac = 0x0;
  mgras_re4Set(base, TAGdata_A.val, s.TAGdata_A.val);

  s.TAGdata_Z = 0x0;
  mgras_re4Set(base, TAGdata_Z, s.TAGdata_Z);
  
  mgras_re4Set(base, PIXcolorR, 0x01a08190);
  mgras_re4Set(base, PIXcolorG, 0x000807f8);
  mgras_re4Set(base, PIXcolorB, 0x000807f8);
  mgras_re4Set(base, PIXcolorA, 0xfff00ff0);

  s.pp1winmode.bits.WINxLSBs = 0;
  s.pp1winmode.bits.WINyLSBs = 0;
  s.pp1winmode.bits.CIDmatch = 0;
  s.pp1winmode.bits.CIDdata = 0;
  s.pp1winmode.bits.CIDmask = 0;
  mgras_re4Set(base, pp1winmode.val, s.pp1winmode.val);
  
  mgras_re4Set(base, z_hi,  0x00000000);
  mgras_re4Set(base, z_low, 0x00000000);

  mgras_CFIFOWAIT(base, HQ3_CFIFO_MAX);

  s.texmode1.bits.TexEn = 0;
  s.texmode1.bits.TexEnvMode = TEXMODE_TEV_MODULATE;
  s.texmode1.bits.TexComp = TEXMODE_1COMP;
  s.texmode1.bits.NumTram = ((info->NumRATRAMs == 4) ?
			    TEXMODE_4TRAM : TEXMODE_1TRAM);
  s.texmode1.bits.TexReadSelect = 0;
  s.texmode1.bits.TexLUTmode = TEXMODE_I_RGB;
  s.texmode1.bits.TexLUTbypass = 0x0; /* bypass all */
  s.texmode1.bits.BlendUnitCount = TEXMODE1_5PAIR;
  s.texmode1.bits.IAMode = 0;
  mgras_re4Set(base, texmode1.val, s.texmode1.val);

  s.scrmsk1x = info->gfx_info.xpmax-1;
  s.scrmsk1y = info->gfx_info.ypmax-1;
  s.scrmsk2x = info->gfx_info.xpmax-1;
  s.scrmsk2y = info->gfx_info.ypmax-1;
  s.scrmsk3x = info->gfx_info.xpmax-1;
  s.scrmsk3y = info->gfx_info.ypmax-1;
  s.scrmsk4x = info->gfx_info.xpmax-1;
  s.scrmsk4y = info->gfx_info.ypmax-1;
  
  mgras_re4Set(base, scissorx,		0xffffffff);
  mgras_re4Set(base, scissory,		0xffffffff);
  mgras_re4Set(base, xywin,		0x00000000);
  mgras_re4Set(base, txenv_rg.val,	0x00000000);
  mgras_re4Set(base, txenv_b,		0x00000000);
  mgras_re4Set(base, fog_rg.val,	0x00000000);
  mgras_re4Set(base, fog_b,		0x00000000);
  mgras_re4Set(base, scrmsk1x,		s.scrmsk1x);
  mgras_re4Set(base, scrmsk1y,		s.scrmsk1y);
  mgras_re4Set(base, scrmsk2x,		s.scrmsk2x);
  mgras_re4Set(base, scrmsk2y,		s.scrmsk2y);
  mgras_re4Set(base, scrmsk3x,		s.scrmsk3x);
  mgras_re4Set(base, scrmsk3y,		s.scrmsk3y);
  mgras_re4Set(base, scrmsk4x,		s.scrmsk4x);
  mgras_re4Set(base, scrmsk4y,		s.scrmsk4y);

  s.winmode.bits.WinMaskEn1 = 0;
  s.winmode.bits.WinMaskEn2 = 0;
  s.winmode.bits.WinMaskEn3 = 0;
  s.winmode.bits.WinMaskEn4 = 0;
  s.winmode.bits.WinMaskInOut1 = WINMODE_OUT;
  s.winmode.bits.WinMaskInOut2 = WINMODE_OUT;
  s.winmode.bits.WinMaskInOut3 = WINMODE_OUT;
  s.winmode.bits.WinMaskInOut4 = WINMODE_OUT;
  mgras_re4Set(base, winmode.val, s.winmode.val);
  
  mgras_re4Set(base, xfrmasklow,	0xffffffff);
  mgras_re4Set(base, xfrmaskhigh,	0xffffffff);

  s.xfrmode.bits.PixelCompType = XFRMODE_1U;
  s.xfrmode.bits.PixelFormat = XFRMODE_CI1;
  s.xfrmode.bits.BeginSkipBytes = 0;
  s.xfrmode.bits.BeginSkipBits = 0;
  s.xfrmode.bits.StrideSkipBytes = 0;
  s.xfrmode.bits.UnpackLSBfirst = 0;
  s.xfrmode.bits.EnableReadMask = 1;
  s.xfrmode.bits.EnableZoom = 0;
  s.xfrmode.bits.EnableDecimate = 0;
  s.xfrmode.bits.RGBorder = XFRMODE_ABGR;
  s.xfrmode.bits.BitMapOpaque = 0;
  mgras_re4Set(base, xfrmode.val, s.xfrmode.val);
  
  mgras_re4Set(base, lspat,		0xffffffff);

  s.lscrl.bits.LineStipCount = 0x00;
  s.lscrl.bits.LineStipRepeat = 0x00;
  s.lscrl.bits.LineStipLength = 0xf;
  mgras_re4Set(base, lscrl.val, s.lscrl.val);
  
  mgras_re4xSet(base, z, 0);

  /* Clear screen to black (keep flashs down) */
  mgras_CFIFOWAIT(base, HQ3_CFIFO_MAX);
  mgras_re4Set(base, PIXcolorR,		0x00100000);
  mgras_re4Set(base, PIXcolorG,		0x00100000);
  mgras_re4Set(base, PIXcolorB,		0x00100000);
  mgras_re4Set(base, PIXcolorA,		0xfff00ff0);
  mgras_re4Set(base, TAGdata_R.val,	0x00000100);
  mgras_re4Set(base, TAGdata_G.val,	0x000000ff);
  mgras_re4Set(base, TAGdata_B.val,	0x000000ff);
  mgras_re4Set(base, TAGdata_A.val,	0x000000ff);
  mgras_re4Set(base, fillmode.val,	0x00100000);
  mgras_re4Set(base, pp1fillmode.val,	0x00004203);
  mgras_re4Set(base, block_xystarti,	0x00000000);
  mgras_re4Set(base, block_xyendi,	0x04ff03ff);
  mgras_re4SetExec(base, ir_alias.val,	0x00000018);
  mgras_re4Set(base, fillmode.val,	0x00005000);
  mgras_re4Set(base, pp1fillmode.val,	0x0c004203);

  /* do 'initppregs' again, for good measure */
  mgras_CFIFOWAIT(base, HQ3_CFIFO_MAX);
  
  mgras_re4Set(base, PixCmd, s.PixCmd);
  mgras_re4Set(base, ColorMaskMSBs,	0x00000000);
  mgras_re4Set(base, ColorMaskLSBsA,	0xffffffff);
  mgras_re4Set(base, ColorMaskLSBsB,	0xffffffff);
  mgras_re4Set(base, BlendFactor.val,	0x58840000);
  mgras_re4Set(base, Stencilmode.val,	s.Stencilmode.val);
  mgras_re4Set(base, Stencilmask.val,	s.Stencilmask.val);
  mgras_re4Set(base, Zmode.val,		s.Zmode.val);
  mgras_re4Set(base, Afuncmode.val,	0x00000007);
  mgras_re4Set(base, Accmode.val,	0x00000000);
  mgras_re4Set(base, TAGmode.val,	s.TAGmode.val);
  mgras_re4Set(base, TAGdata_R.val,	s.TAGdata_R.val);
  mgras_re4Set(base, TAGdata_G.val,	s.TAGdata_G.val);
  mgras_re4Set(base, TAGdata_B.val,	s.TAGdata_B.val);
  mgras_re4Set(base, TAGdata_A.val,	s.TAGdata_A.val);
  mgras_re4Set(base, TAGdata_Z,		s.TAGdata_Z);
  mgras_re4Set(base, PIXcolorR,		0x01a08190);
  mgras_re4Set(base, PIXcolorG,		0x000807f8);
  mgras_re4Set(base, PIXcolorB,		0x000807f8);
  mgras_re4Set(base, PIXcolorA,		0xfff00ff0);
  mgras_re4Set(base, pp1winmode.val,	s.pp1winmode.val);
  mgras_re4Set(base, z_hi,		0x00000000);
  mgras_re4Set(base, z_low,		0x00000000);

  mgras_CFIFOWAIT(base, HQ3_CFIFO_MAX);

  mgras_re4Set(base, config.val,	s.config.val);
  mgras_re4Set(base, texmode1.val,	s.texmode1.val);
  mgras_re4Set(base, scissorx,		0xffffffff);
  mgras_re4Set(base, scissory,		0xffffffff);
  mgras_re4Set(base, xywin,		0x00000000);
  mgras_re4Set(base, txenv_rg.val,	0x00000000);
  mgras_re4Set(base, txenv_b,		0x00000000);
  mgras_re4Set(base, fog_rg.val,	0x00000000);
  mgras_re4Set(base, fog_b,		0x00000000);
  mgras_re4Set(base, scrmsk1x,		s.scrmsk1x);
  mgras_re4Set(base, scrmsk1y,		s.scrmsk1y);
  mgras_re4Set(base, scrmsk2x,		s.scrmsk2x);
  mgras_re4Set(base, scrmsk2y,		s.scrmsk2y);
  mgras_re4Set(base, scrmsk3x,		s.scrmsk3x);
  mgras_re4Set(base, scrmsk3y,		s.scrmsk3y);
  mgras_re4Set(base, scrmsk4x,		s.scrmsk4x);
  mgras_re4Set(base, scrmsk4y,		s.scrmsk4y);
  mgras_re4Set(base, winmode.val,	s.winmode.val);
  mgras_re4Set(base, xfrmasklow,	0xffffffff);
  mgras_re4Set(base, xfrmaskhigh,	0xffffffff);
  mgras_re4Set(base, xfrmode.val,	s.xfrmode.val);
  mgras_re4Set(base, lspat,		0xffffffff);
  mgras_re4Set(base, lscrl.val,		s.lscrl.val);
  mgras_re4xSet(base, z, 0);
}

/******************************************************************************
 * B A C K E N D   D O W N L O A D   F U N C T I O N S
 *****************************************************************************/

/*
 * MgrasVerifyVC3SRAM - check contents of SRAM
 * Returns: number of mistmatches
 */
int MgrasVerifyVC3SRAM(register mgras_hw *base,
		       unsigned short *data, 
		       unsigned int addr,
		       unsigned int length)
{
    __uint32_t i, failures = 0;
    unsigned short sample[3];
  
    mgras_BFIFOPOLL(base, 1);
    mgras_vc3SetReg(base,VC3_RAM_ADDR, addr);

    mgras_BFIFOWAIT(base, HQ_BFIFO_MAX);
    for (i = 0; i < length; i++) {
	if ((i & (HQ_BFIFO_HIWATER-1)) == (HQ_BFIFO_HIWATER-1))
	    mgras_BFIFOWAIT(base, HQ_BFIFO_MAX);
	mgras_vc3GetRam(base,sample[0]);
	if (sample[0] != data[i]) {
	    mgras_BFIFOPOLL(base, 1);
	    mgras_vc3SetReg(base,VC3_RAM_ADDR, addr+i);
	    mgras_BFIFOWAIT(base, HQ3_BFIFO_MAX);
	    mgras_vc3GetRam(base,sample[1]);
	    DELAY(1000);
	    mgras_vc3SetReg(base,VC3_RAM_ADDR, addr+i);
	    mgras_BFIFOWAIT(base, HQ3_BFIFO_MAX);
	    mgras_vc3GetRam(base,sample[2]);
	    cmn_err(CE_WARN, "mgras: VC3SRAM verify failed - ");
	    cmn_err(CE_WARN, "location 0x%04x: expected 0x%04x, got {0x%04x,0x%04x,0x%04x\n",
		    addr+i, data[i], sample[0], sample[1], sample[2]);
	    failures++;
	}
    }
  return failures;
}

void MgrasLoadVC3SRAM(register mgras_hw *base,
			unsigned short *data, 
			unsigned int addr,
			unsigned int length)
{
    int i;

    mgras_BFIFOPOLL(base, 1);
    mgras_vc3SetReg(base,VC3_RAM_ADDR, addr);

    mgras_BFIFOWAIT(base, HQ_BFIFO_MAX);
    for (i = 0; i < length; i++) {
	if ((i & (HQ_BFIFO_HIWATER-1)) == (HQ_BFIFO_HIWATER-1))
	    mgras_BFIFOWAIT(base, HQ_BFIFO_MAX);
	mgras_vc3SetRam(base, data[i]);
    }

    /* always verify that the download succeeded */
    MgrasVerifyVC3SRAM(base, data, addr, length);
}

void MgrasClearCursor(register mgras_hw *base)
{
	/* null cursor */
	unsigned short buf[128];

	bzero(buf, sizeof(buf));

	MgrasLoadVC3SRAM(base, buf, VC3_CURS_GLYPH_ADDR, sizeof(buf)/sizeof(short));

	mgras_BFIFOPOLL(base, 3);
	mgras_vc3SetReg(base, VC3_CURS_EP, VC3_CURS_GLYPH_ADDR);
	mgras_vc3SetReg(base, VC3_CURS_X_LOC, 640);
	mgras_vc3SetReg(base, VC3_CURS_Y_LOC, 512);

}

/******************************************************************************
 * V C 3   D I S P L A Y   C O N T R O L   F U N C T I O N S
 *****************************************************************************/

#ifndef _STANDALONE
void MgrasEnableRetraceInterrupts(mgras_hw *base)
{
#if HQ4_BRINGUP_HACKS
	extern int SuppressRetraceIntr;

	if (SuppressRetraceIntr)
	    return;
#endif
	mgras_BFIFOPOLL(base, 1);
	VC3_display_control |= VC3_VINTR;
	mgras_vc3SetReg(base, VC3_DISPLAY_CONTROL, VC3_display_control);
}

void MgrasBlackoutTiming(mgras_hw *base, int onoff)
{
	mgras_BFIFOPOLL(base, 1);
	if (onoff) {
	    VC3_display_control |= VC3_BLACKOUT;
	} else {
	    VC3_display_control &= ~VC3_BLACKOUT;
	}
	mgras_vc3SetReg(base, VC3_DISPLAY_CONTROL, VC3_display_control);
}
#endif /* !STANDALONE */

void MgrasEnableVideoTiming(mgras_hw *base, int onoff)
{
	mgras_BFIFOPOLL(base, 1);
	if (onoff) {
		mgras_vc3SetReg(base, VC3_DISPLAY_CONTROL, VC3_display_control);
	} else {
		mgras_vc3SetReg(base, VC3_DISPLAY_CONTROL, 0x0);
	}
}

void MgrasSetGenlock(mgras_hw *base, int enable, int source)
{
	if (enable) {
	    VC3_display_control |= VC3_GENSYNC;
	} else {
	    VC3_display_control &= ~VC3_GENSYNC;
	}
	if (source) {
	    VC3_display_control |= VC3_GENLOCK;
	} else {
	    VC3_display_control &= ~VC3_GENLOCK;
	}
	mgras_BFIFOPOLL(base, 1);
	mgras_vc3SetReg(base, VC3_DISPLAY_CONTROL, VC3_display_control);

#ifdef HQ4
	mgras_BFIFOPOLL(base, 1);
	if(source) {
	    MGRAS_SET_BC1(base, MGRAS_BC1_VC3_RESET_BIT);
	} else {
	    MGRAS_SET_BC1(base, MGRAS_BC1_SEL_GENSYNC_BIT | 
			MGRAS_BC1_VC3_RESET_BIT);
	}
#endif
}

#ifdef _STANDALONE

/******************************************************************************
 * C U R S O R  F U N C T I O N S
 *****************************************************************************/

__uint32_t   CmapData[] = {
	0x000000, 0xff0000, 0xffffff,
	0xff0000, 0x000000, 0x000000,
	0x000000, 0x000000, 0x000000,
	0x000000, 0x000000, 0x000000,
	0x000000, 0x000000, 0x000000,
	0x000000, 0x000000, 0x000000,
	0x000000, 0x000000, 0x000000,
	0x000000, 0x000000, 0x000000,
};

static void
mgras_LoadCmap(mgras_hw *base, __uint32_t StAddr,char *data, __uint32_t length) 
{
	 __uint32_t i, addr;

	addr = MGRAS_8BITCMAP_BASE + StAddr;
	/* First Drain the BFIFO - DCB FIFO*/
	mgras_BFIFOWAIT(base, HQ3_BFIFO_MAX);

	/* Force CBlank High (to prevent update of CMAP fifo - bug) */
	mgras_xmapSetPP1Select(base,MGRAS_XMAP_WRITEALLPP1);
	mgras_xmapSetAddr(base, MGRAS_XMAP_DIBTOPSCAN_ADDR);
	mgras_xmapSetDIBdata(base, MGRAS_XMAP_DIBTOPSCAN_DEFAULT |
			     MGRAS_XMAP_DISABLE_CBLANK_BIT);

	for (i = 0; i < length; i++, addr++, data+=4) {

		/* Setting the address twice every time we check cmapFIFOWAIT	 */
		/* is a s/w work around for the cmap problem. The address	 */
		/* is lost ramdomly when the bus is switched from write to	 */
		/* read mode or vice versa.					 */

		/* Every 16 colors, check cmap FIFO.
		 * Need to give 2 dummy writes after the read.
		 */
		if ((i & 0x0F) == 0x00) {
		    mgras_BFIFOPOLL(base, 2);
		    /* Let CBlank go low, so CMAP can drain fifo */
		    mgras_xmapSetAddr(base, MGRAS_XMAP_DIBTOPSCAN_ADDR);
		    mgras_xmapSetDIBdata(base, MGRAS_XMAP_DIBTOPSCAN_DEFAULT);

		    /* cmapFIFOWAIT calls BFIFOWAIT */
		    mgras_cmapFIFOWAIT(base);
		    mgras_cmapSetAddr(base, addr);
		    mgras_cmapSetAddr(base, addr);

		    /* Force CBlank High (to prevent update of CMAP fifo - bug) */
		    mgras_xmapSetAddr(base, MGRAS_XMAP_DIBTOPSCAN_ADDR);
		    mgras_xmapSetDIBdata(base, MGRAS_XMAP_DIBTOPSCAN_DEFAULT |
					MGRAS_XMAP_DISABLE_CBLANK_BIT);
		}

		mgras_BFIFOPOLL(base, 2);
		mgras_cmapSetAddr(base, addr);
		mgras_cmapSetRGB(base, *(data+1), *(data+2), *(data+3) );

	}

	/* Put the XMAP back to normal */
	mgras_BFIFOPOLL(base, 2);
	mgras_xmapSetAddr(base, MGRAS_XMAP_DIBTOPSCAN_ADDR);
	mgras_xmapSetDIBdata(base, MGRAS_XMAP_DIBTOPSCAN_DEFAULT);
}


void
MgrasInitCursor(mgras_hw *base)
{
	/* null cursor */
	unsigned char buf[256];
	__uint32_t ccr = 0xdead;
	int i;
	unsigned int idx = mgras_baseindex(base);
	mgras_info *info = MgrasBoards[idx].info;
	/*
	 * disable sequence
	 */
#if 0
	mgras_vc3GetReg(mgbase,VC3_CURS_CONTROL, ccr, ccr);
	ccr &= ~VC3_ENA_CURS_DISPLAY;
#else
	ccr = 0x0;	/* Hardwired hack from diags */
#endif
	mgras_BFIFOWAIT(base, HQ3_BFIFO_MAX);
	mgras_vc3SetReg(base,VC3_CURS_CONTROL, ccr);
	/*
	 * hook up the glyph and download
	 */
	/* XXX--load null cursor glyph */

	bzero(buf, sizeof(buf));
	for (i = 0; i < 16; i++) {
		buf[i*4] = vc1_ptr_bits[i*2];
		buf[i*4+1] = vc1_ptr_bits[i*2+1];
	}
	for (i = 0; i < 16; i++) {
		buf[128+i*4] = vc1_ptr_ol_bits[i*2];
		buf[128+i*4+1] = vc1_ptr_ol_bits[i*2+1];
	}

	MgrasLoadVC3SRAM(base,(unsigned short *) buf, VC3_CURS_GLYPH_ADDR,
                                     sizeof(buf)/sizeof(short) );

	mgras_BFIFOWAIT(base, HQ3_BFIFO_MAX);
        mgras_vc3SetReg(base, VC3_CURS_EP, VC3_CURS_GLYPH_ADDR );
        mgras_vc3SetReg(base, VC3_CURS_X_LOC, 0 );
        mgras_vc3SetReg(base, VC3_CURS_Y_LOC, 0 );

	mgras_BFIFOWAIT(base, HQ3_BFIFO_MAX);
	mgras_LoadCmap(base, (__uint32_t) (MGRAS_CURSOR_CMAP_MSB<<2),
						    (void *)CmapData, 8);
	mgras_BFIFOPOLL(base, 2);
	mgras_xmapSetAddr(base,MGRAS_XMAP_CONFIG_ADDR);
	mgras_xmapSetConfig(base,MGRAS_XMAP_CONFIG_DEFAULT(info->NumREs));
	/*
	 * enable sequence
	 */
#if 0
	mgras_BFIFOPOLL(base, 1);
	mgras_vc3GetReg(base,VC3_CURS_CONTROL, ccr, ccr);
	ccr |= VC3_ENA_CURS_DISPLAY;
#else
	ccr = 0x3;	/* Hardwired hack from diags */
#endif
	mgras_BFIFOPOLL(base, 1);
	mgras_vc3SetReg(base,VC3_CURS_CONTROL, ccr);

}
#endif /* STANDALONE */

/******************************************************************************
 * M I C R O C O D E   D O W N L O A D   F U N C T I O N S
 *****************************************************************************/

#ifndef _STANDALONE
/*ARGSUSED*/
int MgrasDownloadHQ3Ucode(register mgras_hw *base, void *in_hdr, int len, int print_msg)
{
	unsigned int *codep, rdata;
	int jj;
	int error;

	len /= 4;
	codep = (unsigned int *)in_hdr;
	for (jj = 0; jj < len; jj++)
		base->hquc[jj] = *codep++; 

	/* Read back and compare*/
	codep = (unsigned int *)in_hdr;
	for (error = 0, jj = 0; jj < len; jj++) {
		rdata = base->hquc[jj];
		rdata &= 0xffffff;
		if (rdata  != (*codep & 0xffffff)) {
#ifdef DEBUG
			if (print_msg) {
				printf("ERROR: HQ3 offset 0x%x: ",jj);
				printf("expected 0x%x, returned 0x%x\n",
					*codep & 0xffffff, rdata);
			}
#endif
			error++;
		}
		codep++;
	}

#ifdef DEBUG
	if (print_msg) {
		if (error) 
			printf("%d errors were detected in HQ3 ucode\n", error);
		else 
			printf("HQ3 ucode download was successful -- no errors detected\n");
	}
#endif /* DEBUG */
	return error;	
}

/*ARGSUSED*/
int MgrasDownloadGE11Ucode(register mgras_hw *base, void *in_hdr, int len, int numge, int print_msg)
{
	int *data, *codestart, rdata;
	int jj, ge, pass, raddr;
	int orig_hq_config, orig_gio_config, config, error=0, errflag;
	volatile __uint32_t *diag_a, *diag_d;

    {
	/*
	 * omit ge11 ucode download if
	 * environment variable is defined
	 */
	extern char *kopt_find(char *);
	char *cp;

	if ((cp = kopt_find("mgras_noge11bin")) && *cp)
		return(0);
    }
	if (numge <= 0)
		return(0);

#define WAIT_TILL_SYNC_FIFO_EMPTY() \
	while (base->busy_dma & HQ_BUSY_HQ_SYNC_BIT)

#if !HQ4
#define GE_DIAG_READ(hi, lo) \
	while ((base->flag_set_priv & HQ_FLAG_GE_DIAG_READ_BIT) == 0) \
		; \
	hi = base->reif_ctx.diag_read_wd_0; \
	lo = base->reif_ctx.diag_read_wd_1
#else
/* HQ_FLAG_GE_DIAG_READ_BIT unreliable on hq4, don't use it. */
#define GE_DIAG_READ(hi, lo) \
	hi = base->reif_ctx.diag_read_wd_0; \
	lo = base->reif_ctx.diag_read_wd_1
#endif

#if HQ4_BRINGUP_HACKS
	print_msg = 1;
#endif
	errflag = 0;	
	codestart = (int *)in_hdr;
	len /= 12;
	orig_hq_config = config = base->hq_config;
	orig_gio_config = base->gio_config;
	config |= HQ3_UCODE_STALL;

	for (ge = 0; ge < 2 && ge < numge; ge++) {
		/*
		 * prepare diag access to ge[01]
		 */
		config &= ~(HQ3_CONFIG_GEDIAG_MASK);
		if (ge == 0) {
			config |= HQ3_DIAG_READ_GE0;
			diag_a = &base->ge0_diag_a;
			diag_d = &base->ge0_diag_d;
		} else {
			config |= HQ3_DIAG_READ_GE1;
			diag_a = &base->ge1_diag_a;
			diag_d = &base->ge1_diag_d;
		}
		base->hq_config = config;
		jj = base->hq_config;	/* flush 33MHz-50MHz sync fifo */
		base->gio_config = orig_gio_config | HQ_GIOCFG_HQBUS_BYPASS_BIT;

		DELAY(25);

		/* Write ucode */
#ifdef DEBUG
		if (print_msg == 1)
			printf("starting ge11 ucode download\n");
#endif
		data = codestart;
		*diag_a = GE11_UCODE_BASE;
		WAIT_TILL_SYNC_FIFO_EMPTY();
		for (jj = 0; jj < len; jj++) {
			*diag_d = *data++; 
			WAIT_TILL_SYNC_FIFO_EMPTY();
			*diag_d = *data++; 
			WAIT_TILL_SYNC_FIFO_EMPTY();
			*diag_d = *data++; 
			WAIT_TILL_SYNC_FIFO_EMPTY();
		}
	    {
		/*
		 * omit ge11 ucode verify if
		 * environment variable is defined
		 */
		extern char *kopt_find(char *);
		char *cp;

		if ((cp = kopt_find("mgras_noverify")) && *cp)
			goto noverify;
	    }
		/*
		 * Read back and compare
		 * on pass 0, start on line 0, check every other word
		 * on pass 1, start on line 1, check every other word
		 * since there are 3 words per ucode line, we can verify all but 1 word.
		 */
#ifdef DEBUG
		if (print_msg == 1)
			printf("starting ge11 ucode verify\n");
#endif
		error = 0; 
		/* Set DIAG_MODE register in GE */
		*diag_a = GE11_DIAG_MODE_ADDR; 
		WAIT_TILL_SYNC_FIFO_EMPTY();
		*diag_d = GE11_DIAG_MODE | GE11_OFIFO_WM; 
		WAIT_TILL_SYNC_FIFO_EMPTY();
		for (pass = 0; pass < 2; pass++) {
			data = codestart + 3 * pass;

			*diag_a = GE11_UCODE_BASE + pass;
			WAIT_TILL_SYNC_FIFO_EMPTY();
			*diag_a = 0x80000000 | len;
			DELAY(50000); /* go slow since the emulator is involved */
			MGRAS_SET_BC1(base, MGRAS_BC1_DEFAULT |
					MGRAS_BC1_FORCE_DCB_STAT_BIT);
			DELAY(50000);	/* go slow since the emulator is involved */
			MGRAS_SET_BC1(base, MGRAS_BC1_DEFAULT);
			/* this should really be a read from dcb to make sure
			   that the preceeding writes have finished */
			DELAY(50000); /*  go slow since the emulator is involved */
			WAIT_TILL_SYNC_FIFO_EMPTY();
			DELAY(20);
			if (pass == 1) {
				/* ??? throw away 2 words at beginnin of pass 1 */
				GE_DIAG_READ(raddr, rdata);
				GE_DIAG_READ(raddr, rdata);
				raddr = raddr;
			}
			for (jj = 0; jj < len / 2; jj++, data += 6) {
				GE_DIAG_READ(raddr, rdata);
				raddr = raddr;
				if (rdata != data[0]) {
#ifdef DEBUG
					if (print_msg) {
						printf("ERROR: pass %d, line 0x%x, word 0, expected 0x%x, returned 0x%x\n",
							pass, jj, data[0], rdata);
					}
#endif
					error++;
				}
				GE_DIAG_READ(raddr, rdata);
				raddr = raddr;
				if ((rdata & 0xff) != (data[2] & 0xff)) {
#ifdef DEBUG
					if (print_msg) {
						printf("ERROR: pass %d, line 0x%x, word 2, expected 0x%x, returned 0x%x\n",
							pass, jj, data[2], rdata);
					}	
#endif
					error++;
				}
				if (pass == 1 && jj == (len / 2 - 1))
					continue;
				GE_DIAG_READ(raddr, rdata);
				raddr = raddr;
				if (rdata != data[4]) {
#ifdef DEBUG
					if (print_msg) {
						printf("ERROR: pass %d, line 0x%x, word 4, expected 0x%x, returned 0x%x\n",
							pass, jj, data[4], rdata);
					}
#endif
					error++;
				}
			}
		}
	noverify:
#ifdef DEBUG
		if (print_msg == 1) {
			printf("GE%d download completed\n", ge);
			if (error) 
				printf("%d errors were detected in GE%d ucode\n", error, ge);
			else 
				printf("GE%d ucode download was successful -- no errors detected\n", ge);
		}
#endif
		/* Disable DIAG_MODE register in GE and HQ */
		*diag_a = GE11_DIAG_MODE_ADDR; 
		WAIT_TILL_SYNC_FIFO_EMPTY();
		*diag_d = GE11_OFIFO_WM; 
		WAIT_TILL_SYNC_FIFO_EMPTY();

		if (error)
			errflag++;
		else {
			/* reset ge pc to beginning */
			*diag_a = GE11_UCODE_BASE;
			WAIT_TILL_SYNC_FIFO_EMPTY();
			*diag_d = codestart[0];
			WAIT_TILL_SYNC_FIFO_EMPTY();
			*diag_d = codestart[1];
			WAIT_TILL_SYNC_FIFO_EMPTY();
			*diag_d = codestart[2];
			WAIT_TILL_SYNC_FIFO_EMPTY();

			/* set diag pin as output */
			*diag_a = GE11_DIAG_SEL_REG;
			WAIT_TILL_SYNC_FIFO_EMPTY();
			*diag_d = GE11_DIAG_INPUT;
			WAIT_TILL_SYNC_FIFO_EMPTY();

			/* set cram trim delay */
			*diag_a = GE11_COTM_CONFIG_REG;
			WAIT_TILL_SYNC_FIFO_EMPTY();
			*diag_d = GE11_COTM_CONFIG_VAL;
			WAIT_TILL_SYNC_FIFO_EMPTY();

			/* kick start ge microcode */
			*diag_a = GE11_DIAG_EXEC_ADDR;
			WAIT_TILL_SYNC_FIFO_EMPTY();
			*diag_d = GE11_GO & GE11_UCODE_LAT_4;
			WAIT_TILL_SYNC_FIFO_EMPTY();
		}
	}
	base->gio_config = orig_gio_config;
	base->hq_config = orig_hq_config;

	print_msg = print_msg;	/* quiet compiler warning */

	return(errflag);
}
#endif /* !_STANDALONE */

/*
 * Return index into MgrasBoards.
 * If no matching entry, return -1.
 */
int
mgras_baseindex(mgras_hw *base)
{
	int i, retval = -1;
	for (i = 0; i < MGRAS_MAXBOARDS; i++) {
		if (MgrasBoards[i].base == base) {
			retval = i;
			break;
		}
	}
	return retval;
}

#ifdef FLAT_PANEL
void
mgras_fp_probe (struct mgras_hw *base, struct mgras_info *info)
{
	int panel = 0xdeadbeef;
#ifndef _STANDALONE
	inventory_t *panel_inv;
#endif
				
	if (mgras_i2cProbe(base) && mgras_i2cSetup()) {
	    if (mgras_i2cPanelID(&panel) ) {
#if !defined(_STANDALONE)
		    if (panel_inv = find_inventory(0, INV_DISPLAY,
					    INV_PRESENTER_BOARD, 0, 0, 0))
			    replace_in_inventory(panel_inv,
					    INV_DISPLAY,
					    INV_PRESENTER_PANEL, 0, 0, 0);
		    else
			    add_to_inventory(INV_DISPLAY,
					    INV_PRESENTER_PANEL, 0, 0, 0);
#endif
		    switch (panel) {
		    case PANEL_XGA_MITSUBISHI:
			    info->PanelType = MONITORID_CORONA;
			    break;
		    case PANEL_EWS_MITSUBISHI:
			    info->PanelType = MONITORID_ICHIBAN;
			    break;
		    default:
			    info->PanelType = 0;
		    }
	    } 
	    else {
		    /*
		     * Sometimes we don't read the panel id
		     * correctly, even though the panel is
		     * in fact present.  Turn off the panel
		     * to prevent damage to the display
		     * in case it really is there.
		     */

		    mgras_i2cPanelOff();
#if !defined(_STANDALONE)
		    if (panel_inv = find_inventory(0, INV_DISPLAY,
					    INV_PRESENTER_PANEL, 0, 0, 0))
			    replace_in_inventory(panel_inv,
					    INV_DISPLAY,
					    INV_PRESENTER_BOARD, 0, 0, 0);
		    else
			    add_to_inventory(INV_DISPLAY,
					    INV_PRESENTER_BOARD, 0, 0, 0);
#endif
		    info->PanelType = -1;
	    }
	}
}

#ifndef _STANDALONE
int
mgras_fp_reinit (struct mgras_data *bd)
{
	int panel;

	ASSERT (bd);
	ASSERT (bd->info);
	ASSERT (bd->base);

	if (bd->info->PanelType == 0) return (-1);

	mgras_fp_probe( bd->base, bd->info );
	if (bd->info->PanelType == 0) return (-1);

	if (mgras_i2cPanelID(&panel) ) {
		switch (panel) {
		case PANEL_XGA_MITSUBISHI:
			bd->info->PanelType = MONITORID_CORONA;
			break;
		case PANEL_EWS_MITSUBISHI:
			bd->info->PanelType = MONITORID_ICHIBAN;
			break;
		default:
			bd->panelalive = 0;
			bd->info->PanelType = 0; /* we'll never come back */
			return 0;
		}
		mgras_i2cPanelOn();
		return 0;
	}
	else
		bd->info->PanelType = MONITORID_NOFPD;
	return 0;
}
#endif /* _STANDALONE */

#endif /* FLAT_PANEL */

/* Blank screen through DAC.  Don't use VC3 blackout bit since
 * the BLANK signal to PP1 RDRAM would also be disabled.
 * This would result in needing a redraw of the entire screen,
 * after unblanking, in order to refresh the RDRAM.
 */
void MgrasBlankScreen(mgras_hw *base,int blank)
{
	mgras_BFIFOPOLL(base, 3);
	if (blank)
	{
		mgras_dacSetAddrCmd(base,MGRAS_DAC_PIXMASK_ADDR,
				    MGRAS_DAC_PIXMASK_BLANK);	
#ifdef FLAT_PANEL
		/* Unmask the timing pipes */
		mgras_cmapSetCmd(base,
		    MGRAS_CMAP_CMD_UNMASK_PIPES&(~MGRAS_CMAP_CMD_RESET_1));
#endif
	}
	else
	{
		mgras_dacSetAddrCmd(base,MGRAS_DAC_PIXMASK_ADDR,
				    MGRAS_DAC_PIXMASK_INIT);	
#ifdef FLAT_PANEL
		mgras_cmapSetCmd(base, MGRAS_CMAP_CMD_UNMASK_PIPES);
#endif
	}
}

#if defined(RE4_NON_INLINE) && defined(_STANDALONE)
/*
 * procedure call to replace the inlined mgras_re4 macros (for space
 * reasons) in the IP27prom
 */
void mgras_re4SendProc(void *base,
		       __uint32_t addr,
		       __uint32_t dataval)
{
    mgras_hw *mgbase = base;
    mgras_CHECK_CFIFO(mgbase, 1);  /* this macro is pretty big */
    mgras_WRITECFIFO64(mgbase, MGRAS_HQ_CMDTOKEN(addr, 4), dataval);
}
#endif /* RE4_NON_INLINE */
