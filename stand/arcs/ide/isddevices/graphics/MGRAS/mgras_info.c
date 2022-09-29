/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *  MGRAS - INFO.
 *
 *  $Revision: 1.137 $
 */

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include "uif.h"
#include <math.h>
#include <libsc.h>
#include "sys/mgrashw.h"
#include "mgras_hq3.h"
#include "sys/mgras.h"

#include "ide_msg.h"
#include "libsc.h"
#include "mgras_diag.h"
#include "parser.h"

#include "mgras_xbow.h" /* need to move this to mgras_diag.h */


mgras_hw *mgbase  = (mgras_hw *)PHYS_TO_K1(MGRAS_BASE0_PHYS);
mgras_hw *re4base = (mgras_hw *)PHYS_TO_K1(MGRAS_BASE0_PHYS);

struct mgras_hinv MgHinv ;
struct mgras_timing_info *mg_tinfo;
struct mgras_info *mginfo;
extern struct mgras_timing_info mgras_1280_1024_60_1RSS;
extern struct mgras_timing_info *mgras_timing_tables_1RSS[];
extern void MgrasRecordTiming(mgras_hw *, struct mgras_timing_info *);
__uint32_t _pp1_xmap_init(void);

__uint32_t	_mg_solid_impact = 0x0; /* Not Soild Impact */
__uint32_t	has_csctmi= 0;

__uint32_t mg_timing_init = 0;	/* Indicates whether mg_tinfo has been set */

void
_mg_set_ZeroGE(void) 
{
	__uint32_t	config;

	msg_printf(DBG,"Setting to 0 GE\n");
	config = mgbase->hq_config;
	config &= ~0x1;		/* ZERO GE */
	mgbase->hq_config = config;
}

void
_mg_set_2GE(void)
{
	__uint32_t	config;

	msg_printf(DBG,"Setting to 2 GE\n");
	config = mgbase->hq_config;
	config &= ~0x3;	
	config |= 0x2;		/* TWO GEs */
	mgbase->hq_config = config;
}

/*
 * MgrasReset - reset the gfx board by twiddling the reset line
 */
/* #ifdef IPXX entire block if have a different CPU reset routine */
void __MgrasReset(void)
{
	gfxreset();
	resetcons(0, (char **)NULL);
}

/*
 * _mg_probe_video_csctmi(void)--not used any more since this causes exceptions 
 * for IP28, though it is okay for IP22.
 *
 * Probes for csc/tmi video option card. Returns 1 if found, 0 if not found
 * Similar to the mgv_csc_tmi_probe() routine in mgv_init.c, but we don't
 * include that file as part of the mardigras default, so it's reproduced
 * here.
 */
__uint32_t 
_mg_probe_video_csctmi(void)
{
	/* values taken from mgv_diag.h, but we don't include that file here
	 * since it is part of the GALILEO15 area
	 */
	__uint32_t mgv_base0_phys = 0x1f062800, gal_offset= 0xc00, gal_dir2 = 2;
	__uint32_t val;
	__paddr_t  tmp;
	__uint32_t *csctmi_addr;
	
	tmp = (__paddr_t) PHYS_TO_K1(mgv_base0_phys + gal_offset + MGRAS_DCB_CRS(gal_dir2) + MGRAS_DCB_DATAWIDTH_1);
	csctmi_addr = (__uint32_t *)tmp;

	msg_printf(DBG, "csctmi addr: 0x%x\n", csctmi_addr);

	val  = (__uint32_t) *csctmi_addr;

	msg_printf(DBG, "csctmi addr: 0x%x, val: 0x%x\n",csctmi_addr,(val>>28));

	if (((val >> 28 ) & 0xf) == 0xf ) { /* no option card */
		has_csctmi = 0;
		msg_printf(DBG, "probe: has_csctmi = 0\n");
		return (0);
	}
	else {
		has_csctmi = 1;
		msg_printf(DBG, "probe: has_csctmi = 1\n");
		return (1);
	}
}


#if !HQ4
void
_mgras_setup_gio64arb(void) {
	__uint32_t index;
	__uint32_t arb_bits;
	index = mgras_baseindex(mgbase);
	arb_bits = (GIO64_ARB_GRX_MST | GIO64_ARB_GRX_SIZE_64) << index;
	_gio64_arb |= arb_bits;
	msg_printf(DBG, "_gio64_arb 0x%x\n", _gio64_arb);
	(*(__uint32_t *)(PHYS_TO_K1(GIO64_ARB))) = _gio64_arb;
}
#endif

void
mgras_probe_all_widgets ( )
{
#if HQ4
    xbowreg_t   in_port;
    __uint32_t  offset, found = 0;
    __uint32_t  xt_control, xt_widget_id;
    void	    *port_base;


    mg_timing_init = 0;

    msg_printf (DBG, "Entering mgras_setboard...\n");

    for ( in_port = XBOW_PORT_9 ; in_port < XBOW_PORT_F ; in_port++ ) {
	    if ( _mgras_islinkalive (in_port) )  {
				offset = in_port-XBOW_PORT_9;
                port_base  = (void *) (K1_MAIN_WIDGET(in_port)+4);
                msg_printf(DBG, "port_base 0x%x port 0x%x\n", 
					port_base, in_port);

#if 0
	     		if (badaddr (&(mgbase->xt_widget_id), sizeof(__paddr_t)) ) {
#else
				if (0) {
#endif
   		      		msg_printf(ERR, " port num\n", in_port );
   		  		} else {
   		     		xt_widget_id = *(__uint32_t *)port_base ;
   		     		mg_xbow_portinfo_t[offset].mfg  = XWIDGET_MFG_NUM(xt_widget_id)     ;
   		     		mg_xbow_portinfo_t[offset].rev  = XWIDGET_REV_NUM(xt_widget_id);
					mg_xbow_portinfo_t[offset].part = XWIDGET_PART_NUM(xt_widget_id);
					mg_xbow_portinfo_t[offset].port = in_port;
					mg_xbow_portinfo_t[offset].base = (void *)port_base;
					mg_xbow_portinfo_t[offset].present = 1;
					mg_xbow_portinfo_t[offset].alive = 1;
				}
		}
	}
#endif
}

extern __uint32_t _mg_setport ;

__uint32_t
mgras_setport (int argc, char **argv )
{
#if HQ4
	_mg_setport = XBOW_PORT_9;
	if (argc < 2) {
		msg_printf(DBG, " mg_setport <port#>\n");
		return (0);
	}
	atobu(argv[1], (unsigned int *)&_mg_setport);
	msg_printf(SUM, " port set to %d\n", _mg_setport); 
	mgbase  = (mgras_hw *) K1_MAIN_WIDGET((xbowreg_t)_mg_setport);
	msg_printf(DBG, "mgbase 0x%x port 0x%x\n", mgbase, _mg_setport);
	if (_mgras_hq4probe (mgbase, (xbowreg_t)_mg_setport)) {
		return (1);
	} else {
		return (0);
	}
#else
	return 0;
#endif
}
	

/* return 1 = board found, 0 = no board at this slot */
__uint32_t
mgras_setboard(int argc, char ** argv) 
{
#if HQ4
    
#ifdef SN0
/* XXX must determine attached node */
#include "sys/SN0/sn0addrs.h"
#define K1_MAIN_WIDGET(in_port)	PHYS_TO_COMPATK1(NODE_SWIN_BASE(0,in_port))
#endif
    
	xbowreg_t 	in_port;
	__uint32_t 	found = 0;
	__uint32_t	xt_control;

    mg_timing_init = 0;

    msg_printf (DBG, "Entering mgras_setboard...\n");

    for ( in_port = _mg_setport ; in_port < XBOW_PORT_F ; in_port++ ) {
                mgbase  = (mgras_hw *) K1_MAIN_WIDGET(in_port);
                msg_printf(DBG, "mgbase 0x%x port 0x%x\n", mgbase, in_port);
                if (_mgras_hq4probe (mgbase, in_port)) {
					xt_control = (1 << 9) | (in_port) | (0x4 << 12);
					msg_printf(DBG, "xt_control 0x%x\n", xt_control);
                	found = 1;
                	msg_printf(SUM, " GAMERA found in port 0x%x\n", in_port );
                        _mgras_hq4linkreset(in_port); 
			/* KY: Some delay is needed to avoid a partial reset stage which
			   cause RT dma problems */ 
			us_delay(1000);
			mgbase->xt_control = xt_control;
                }
	if (found) break;
    }

	if (found == 0) {
		msg_printf(SUM, " GAMERA not found in any ports (8-15)\n");
		return (0);
	}

	MGRAS_GFX_SETUP(0x0, 1);

#if 1
{
ushort_t BdVer0_Reg, BdVer1_Reg;

	bzero(&MgHinv, sizeof(MgHinv)); 

	msg_printf(DBG, "======		MGRAS	HINV	======\n");

	/* First get the Board Revision Registers */
	BdVer0_Reg = (mgbase->bdvers.bdvers0 >> 24);
        BdVer1_Reg = (mgbase->bdvers.bdvers1 >> 24);

	msg_printf(DBG, "BdVer0_Reg 0x%x\t", BdVer0_Reg);
	msg_printf(DBG, "BdVer1_Reg 0x%x\n", BdVer1_Reg);

	MgHinv.ProductId = (BdVer1_Reg & 0x60) >> 5;
	MgHinv.GDRev = (BdVer1_Reg >>2) & 0x3;		/* GD Revision */
	MgHinv.GEs = BdVer1_Reg & 0x3;			/* #GE's Found */
	_mg_solid_impact = 0x0; /* Assume Not Solid Impact */

	if (MgHinv.ProductId == 3) { /* not GM10 */
 		/* Get everything from Board Version Register 0 */
		MgHinv.RA_TRAMs = (BdVer0_Reg & 0x3);           /* RA TRAMs */
  	     	MgHinv.RB_TRAMs = (BdVer0_Reg >> 2) & 0x3;      /* RB TRAMs */
		MgHinv.REs = numRE4s = 2;
		MgHinv.RBRev = 0x0;
	} else if (MgHinv.ProductId == 2) { /* is GM10 */
		MgHinv.RA_TRAMs = (BdVer0_Reg & 0x3);		/* RA TRAMs */
		MgHinv.RB_TRAMs = 0;				/* RB TRAMs */
		_mg_solid_impact = 0x1; /* Solid Impact */
		MgHinv.REs = numRE4s = 1;
		MgHinv.RBRev = 0x7;
	}

	if (MgHinv.RA_TRAMs == 0x1) {
		_mg_solid_impact = 0; 
		numTRAMs = 4;
	} else {
		_mg_solid_impact = 0x1; /* Solid Impact */
		numTRAMs = 0;
	}

	/* debug */
	msg_printf(DBG, "mgras_gfx_info: numRE4s = %d, numTRAMs = %d\n",
		numRE4s, numTRAMs);

}
	/* mgras_gfx_info(0,0); */
#else /* !1 */
	_mg_solid_impact = 1;
	MgHinv.REs = numRE4s = 1;
	MgHinv.RA_TRAMs = MgHinv.RB_TRAMs = 0;
	MgHinv.GEs = 1;
#endif /* 1 */

	/*_pp1_xmap_init(); */
	YMAX = 1024;
	XMAX = 1344;

	_mgras_init_gold_crc(); /* initialize the CRC values from the Gold HW */

	return(found);

#else /* !HQ4 */

	__uint32_t GioSlot = 0;
	__uint32_t found, NumGe = 1;
	__uint32_t port;
	__uint32_t arb_bits;

	
	if (argc > 1)
		atob(argv[1], (int*)&GioSlot);
	if (argc > 2)
		atob(argv[2], (int*)&NumGe);

#if IP22
	/* Indy does not have Impact but shares ide binary with IP22 */
	if (is_fullhouse() == 0)
		return 0;
#endif

	found = 0;
	msg_printf(DBG, "mgras_setboard: select GioSlot: %d\n", GioSlot);

	port = GioSlot;
	if (GioSlot == 0) {
		mgbase  = (mgras_hw *)PHYS_TO_K1(MGRAS_BASE0_PHYS);
		found = MgrasProbe(mgbase, port); /* port = slot if !HQ4 */
#ifdef MFG_USED
		_mgras_setup_gio64arb();
#endif
		if (found) {
			msg_printf(DBG, "MGRAS set to GioSlot 0x%x, BaseAddrs 0x%x mgbase 0x%x\n" ,GioSlot, MGRAS_BASE0_PHYS, mgbase);
			__MgrasReset();
			MGRAS_GFX_SETUP(0x0, NumGe);
		}
	}
	else if (GioSlot == 1) {
		mgbase  = (mgras_hw *)PHYS_TO_K1(MGRAS_BASE1_PHYS);
		found = MgrasProbe(mgbase, port);
#ifdef MFG_USED
		_mgras_setup_gio64arb();
#endif
		if (found) {
			msg_printf(DBG, "MGRAS set to GioSlot 0x%x, BaseAddrs 0x%x mgbase 0x%x\n" ,GioSlot, MGRAS_BASE1_PHYS, mgbase);
			__MgrasReset();
			MGRAS_GFX_SETUP(0x1, NumGe);
		}
	}
	msg_printf(DBG, "mgbase->hq_config set 0x%x\n" ,mgbase->hq_config);   
	return (found);
#endif /* HQ4 */ 
}

ushort_t        mg_queryGE(void)
{

	ushort_t        GECount;

	GECount = MgHinv.GEs ;
	msg_printf(SUM, "%d GE's found in the SYSTEM\n",GECount);

	return (GECount);
}


/*ARGSUSED*/

#ifdef 	COMMENT

	Board rev0 reg (read-only)
	Matches what is described in "Display Bus Mapping and Board Configuration 
	Registers" spec. If the RB board is not installed, assume that there are no
	TRAMs since TRAMs are only found on the RB board.

	Board rev1 reg (read-only)
	Bits
		7	Unused

		6:5	Product ID bits - used to distinguish between Speedracer chassis
			and Indigo2 chassis. For example, if the MGRAS board is in the  
			Speedracer chassis, the configuration bits may be 
			interpreted differently to reflect the revision number of a 
			future board.  

			00 = MGxx, has GD and RA (and perhaps RB), has TRAMs.
			01 = MG10, has only GD, no texture.

		4:2	GD board rev if 6:5 == 00. If ProdID=01, then it's MG10 rev.

		1:0	Number of GEs		


	Board rev1 reg (write-only)
	Bits
		7	Unused
		6	unused
		5	VC3 soft reset - used when changing the video timing format
		4	Set Dual head master sync signal in Indigo2 backplane 
		3:2	Unused
		1	DCB HQ stat 1
		0	Unused
#endif

int
mgras_gfx_info(int argc, char **argv)
{
	__uint32_t Vc3Rev = 0xdead;
	ushort_t Cmap0Rev, CmapBRevReg;
	ushort_t BdVer0_Reg, BdVer1_Reg;
	ushort_t DacRev, DacId;
#if 0
	int level = argc ? VRB : DBG;
#else
	int level = DBG;
#endif
	bzero(&MgHinv, sizeof(MgHinv)); 

	msg_printf(level, "======		MGRAS	HINV	======\n");

	/* First get the Board Revision Registers */
	BdVer0_Reg = (mgbase->bdvers.bdvers0 >> 24);
        BdVer1_Reg = (mgbase->bdvers.bdvers1 >> 24);

	msg_printf(level, "BdVer0_Reg 0x%x\t", BdVer0_Reg);
	msg_printf(level, "BdVer1_Reg 0x%x\n", BdVer1_Reg);

	MgHinv.ProductId = (BdVer1_Reg & 0x60) >> 5;
	MgHinv.GDRev = (BdVer1_Reg >>2) & 0x3;		/* GD Revision */
	MgHinv.GEs = BdVer1_Reg & 0x3;			/* #GE's Found */
	_mg_solid_impact = 0x0; /* Assume Not Solid Impact */

#if HQ4
	if (MgHinv.ProductId == 3) { /* not GM10 */
 		/* Get everything from Board Version Register 0 */
		MgHinv.RA_TRAMs = (BdVer0_Reg & 0x3);           /* RA TRAMs */
  	     	MgHinv.RB_TRAMs = (BdVer0_Reg >> 2) & 0x3;      /* RB TRAMs */
		MgHinv.REs = numRE4s = 2;
		MgHinv.RBRev = 0x0;
	} else if (MgHinv.ProductId == 2) { /* is GM10 */
		MgHinv.RA_TRAMs = (BdVer0_Reg & 0x3);		/* RA TRAMs */
		MgHinv.RB_TRAMs = 0;				/* RB TRAMs */
		_mg_solid_impact = 0x1; /* Solid Impact */
		MgHinv.REs = numRE4s = 1;
		MgHinv.RBRev = 0x7; 
	}

	if (MgHinv.RA_TRAMs == 0x1) {
		numTRAMs = 4;
	} else {
		numTRAMs = 0;
	}

	/* debug */
	msg_printf(DBG, "mgras_gfx_info: numRE4s = %d, numTRAMs = %d\n",
		numRE4s, numTRAMs);

#else

#define GIO_ID          0x00050190
#define GIO_ID_ADDR     0x0

	HQ3_PIO_RD(GIO_ID_ADDR, 0xffffffff, MgHinv.HQ3Rev);
	if (MgHinv.HQ3Rev != GIO_ID) {
                msg_printf(ERR,"HQ3 GIO_ID read error: actual = 0x%x, expected = 0x%x\n",MgHinv.HQ3Rev, GIO_ID);
	}

	if (MgHinv.ProductId == 0x0) { /* not MG10 */
 		/* Get everything from Board Version Register 0 */
		MgHinv.RA_TRAMs = (BdVer0_Reg & 0x3);           /* RA TRAMs */
  	     	MgHinv.RB_TRAMs = (BdVer0_Reg >> 2) & 0x3;      /* RB TRAMs */
		MgHinv.RBRev = (BdVer0_Reg >> 4) & 0x7;		/* RB Rev */
	}
	else if (MgHinv.ProductId == 0x1) { /* is MG10 */
		MgHinv.RA_TRAMs = 0;		/* RA TRAMs */
		MgHinv.RB_TRAMs = 0;	/* RB TRAMs */
		MgHinv.RBRev = 0x7;	/* No RB in mg10 */
		_mg_solid_impact = 0x1; /* Solid Impact */
	}

	numRE4s = 1;
	if ((MgHinv.RBRev == 0x7) || (MgHinv.RBRev == 0x3))  /* no RB board */
		numRE4s = 1;
	else
		numRE4s = 2;

	MgHinv.REs = numRE4s;

	/* set to 1 for now until we think of a good way of determing
	 * num of TRAMs since we can't read TEVERSION right now.
	 */
	if (MgHinv.ProductId == 0x1)
		numTRAMs = 0;
	else {
		if (MgHinv.RA_TRAMs == 0) 
			numTRAMs = 1;
		else
			numTRAMs = 2*MgHinv.RA_TRAMs; 
	}

	/* debug */
	msg_printf(DBG, "mg_reset: RBRev = 0x%x, numRE4s = %d, numTRAMs = %d\n",
		MgHinv.RBRev, numRE4s, numTRAMs);

	if (MgHinv.ProductId != 0x1) {
		if ((numTRAMs != 1) && (numTRAMs !=4)) {
			msg_printf(ERR, "Board Version Register #0's TRAM bits are 0x%x, but that is an invalid configuration. Please check Board Version Register #0\n", MgHinv.RA_TRAMs);
			msg_printf(SUM, "WARNING: Texture tests will not work properly.\n");
		}
	}

#endif

	/* Get VC3 Revision */
	mgras_vc3GetReg(mgbase,VC3_CONFIG, Vc3Rev);
	MgHinv.VC3Rev = (Vc3Rev >>6) & 0x3 ;
	
	/* Get CMAP-A Revision and Check */
	Cmap0Rev = mgras_CmapRevision();
	msg_printf(DBG,"Cmap0Rev %x\t", Cmap0Rev);

        if (Cmap0Rev  > CMAP_REV_E) {
                msg_printf(SUM, "WARNING, CMAP REVISION GREATER THAN EXPECTED.\n");
                msg_printf(SUM, "SOFTWARE MAY NOT BE COMPATIBLE\n");
        }


	if ((Cmap0Rev & 0x7) < CMAP_REV_D) {
		msg_printf(ERR, "Cmap0Rev %x\n", Cmap0Rev);
               	return (-1);
        }


	MgHinv.CmapRev = Cmap0Rev;

	/* Get CMAP-B Revison */
	mgras_cmapGetRev(mgbase, CmapBRevReg, 1);
	msg_printf(DBG, "CmapBRevReg %x\n", CmapBRevReg);

	/* Get everything from CMAP-B Revision Register */
	MgHinv.RARev = (CmapBRevReg >> 4) & 0x7;
	MgHinv.VidBdPresent = (CmapBRevReg >> 6 ) & 0x1;

#if !HQ4
/*
 * XXX: This function is bogus.
 */
	/* Get the Monitor Type */
	MgHinv.MonitorType = mgras_GetMon_Type();
#endif

	/* DAC REV */
	mgras_dacGetAddrCmd(mgbase, MGRAS_DAC_REV_ADDR, DacRev);
	MgHinv.DacRev = DacRev;

	/* DAC ID */
	mgras_dacGetAddrCmd(mgbase, MGRAS_DAC_ID_ADDR, DacId);
	MgHinv.DacId = DacId;

	msg_printf(level,"ProductId = %x ,  MonitorType = %x ,  VidBdPresent = %x\n", MgHinv.ProductId,  MgHinv.MonitorType,  MgHinv.VidBdPresent);
	msg_printf(level, "RARev = %x , RBRev = %x , GDRev = %x\n", MgHinv.RARev, MgHinv.RBRev, MgHinv.GDRev);
	msg_printf(level, "RA_TRAMs = %x , RB_TRAMs = %x\n", numTRAMs, numTRAMs);

	msg_printf(level, "CmapRev = %x , VC3Rev = %x , PP1Rev = %x\n", MgHinv.CmapRev, MgHinv.VC3Rev, MgHinv.PP1Rev);
	msg_printf(level, "HQ3Rev = %x , GE11Rev = %x\n" , MgHinv.HQ3Rev, MgHinv.GE11Rev);
	msg_printf(level, "RE4Rev = %x,  TE1Rev = %x , TRAMRev = %x\n" , MgHinv.RE4Rev, MgHinv.TE1Rev, MgHinv.TRAMRev);
	msg_printf(level, "GEs = %x , REs = %x\n", MgHinv.GEs, MgHinv.REs);
	msg_printf(level, "DAC Revision = %x , DAC Id = %x\n" , MgHinv.DacRev, MgHinv.DacId);

	msg_printf(level, "======		MGRAS	HINV	======\n");

	return 0;
}

/* 
 * mgras_gfx_rssinfo is the same as mgras_gfx_info with just the rss
 * areas being probed.
 */
int
mgras_gfx_rssinfo(int argc, char **argv)
{
	__uint32_t Vc3Rev = 0xdead;
	ushort_t Cmap0Rev, CmapBRevReg;
	ushort_t BdVer0_Reg, BdVer1_Reg;
	ushort_t DacRev, DacId;
	int level = argc ? VRB : DBG;

	bzero(&MgHinv, sizeof(MgHinv)); 

	msg_printf(level, "======		MGRAS	HINV	======\n");

	/* First get the Board Revision Registers */
#ifdef 	COMMENT

	Board rev0 reg (read-only)
	Matches what is described in "Display Bus Mapping and Board Configuration 
	Registers" spec. If the RB board is not installed, assume that there are no
	TRAMs since TRAMs are only found on the RB board.

	Board rev1 reg (read-only)
	Bits
		7	Unused

		6:5	Product ID bits - used to distinguish between Speedracer chassis
			and Indigo2 chassis. For example, if the MGRAS board is in the  
			Speedracer chassis, the configuration bits may be 
			interpreted differently to reflect the revision number of a 
			future board.  

			00 = MGxx, has GD and RA (and perhaps RB), has TRAMs.
			01 = MG10, has only GD, no texture.

		4:2	GD board rev if 6:5 == 00. If ProdID=01, then it's MG10 rev.

		1:0	Number of GEs		


	Board rev1 reg (write-only)
	Bits
		7	Unused
		6	unused
		5	VC3 soft reset - used when changing the video timing format
		4	Set Dual head master sync signal in Indigo2 backplane 
		3:2	Unused
		1	DCB HQ stat 1
		0	Unused
#endif

#if HQ4

#else

#define GIO_ID          0x00050190
#define GIO_ID_ADDR     0x0

	HQ3_PIO_RD(GIO_ID_ADDR, 0xffffffff, MgHinv.HQ3Rev);
	if (MgHinv.HQ3Rev != GIO_ID) {
                msg_printf(ERR,"HQ3 GIO_ID read error: actual = 0x%x, expected = 0x%x\n",MgHinv.HQ3Rev, GIO_ID);
	}

#endif

#ifndef MGRAS_BRINGUP
	BdVer0_Reg = (mgbase->bdvers.bdvers0 >> 24);
        BdVer1_Reg = (mgbase->bdvers.bdvers1 >> 24);
#else
	(mgbase->rex3).set.dcbmode = MGRASDCB_PXBDVERS | 
		DCB_CMAP_PROTOCOL | BDVER0_CRS_ADDR_REG | DCB_DATAWIDTH_1 ;
        BdVer0_Reg = (mgbase->rex3).set.dcbdata0.bybyte.b3;
	msg_printf(DBG, "-- MGRAS_BRINGUP -- BdVer0_Reg %x\t", BdVer0_Reg);

	(mgbase->rex3).set.dcbmode = MGRASDCB_PXBDVERS | 
		DCB_CMAP_PROTOCOL | BDVER1_CRS_ADDR_REG | DCB_DATAWIDTH_1 ;
        BdVer1_Reg = (mgbase->rex3).set.dcbdata0.bybyte.b3;
#endif  /* MGRAS_BRINGUP */

	msg_printf(level, "BdVer0_Reg 0x%x\t", BdVer0_Reg);
	msg_printf(level, "BdVer1_Reg 0x%x\n", BdVer1_Reg);

	MgHinv.ProductId = (BdVer1_Reg & 0x60) >> 5;

#if HQ4
    if (MgHinv.ProductId == 3) { /* not GM10 */
        /* Get everything from Board Version Register 0 */
        MgHinv.RA_TRAMs = (BdVer0_Reg & 0x3);           /* RA TRAMs */
            MgHinv.RB_TRAMs = (BdVer0_Reg >> 2) & 0x3;      /* RB TRAMs */
        MgHinv.REs = numRE4s = 2;
    } else if (MgHinv.ProductId == 2) { /* is GM10 */
        MgHinv.RA_TRAMs = (BdVer0_Reg & 0x3);       /* RA TRAMs */
        MgHinv.RB_TRAMs = 0;                /* RB TRAMs */
        _mg_solid_impact = 0x1; /* Solid Impact */
        MgHinv.REs = numRE4s = 1;
    }

    if (MgHinv.RA_TRAMs == 0x1) {
        numTRAMs = 4;
    } else {
        numTRAMs = 0;
    }

    /* debug */
    msg_printf(DBG, "mgras_gfx_info: numRE4s = %d, numTRAMs = %d\n",
        numRE4s, numTRAMs);

#else

	_mg_solid_impact = 0x0; /* Assume Not Solid Impact */
	if (MgHinv.ProductId == 0x0) { /* not MG10 */
 		/* Get everything from Board Version Register 0 */
		MgHinv.RA_TRAMs = (BdVer0_Reg & 0x3);           /* RA TRAMs */
  	     	MgHinv.RB_TRAMs = (BdVer0_Reg >> 2) & 0x3;      /* RB TRAMs */
		MgHinv.RBRev = (BdVer0_Reg >> 4) & 0x7;		/* RB Rev */
	}
	else if (MgHinv.ProductId == 0x1) { /* is MG10 */
		MgHinv.RA_TRAMs = 0;		/* RA TRAMs */
		MgHinv.RB_TRAMs = 0;	/* RB TRAMs */
		MgHinv.RBRev = 0x7;	/* No RB in mg10 */
		_mg_solid_impact = 0x1; /* Solid Impact */
	}

	numRE4s = 1;
	if ((MgHinv.RBRev == 0x7) || (MgHinv.RBRev == 0x3))  /* no RB board */
		numRE4s = 1;
	else
		numRE4s = 2;

	MgHinv.REs = numRE4s;

	/* set to 1 for now until we think of a good way of determing
	 * num of TRAMs since we can't read TEVERSION right now.
	 */
	if (MgHinv.ProductId == 0x1)
		numTRAMs = 0;
	else {
		if (MgHinv.RA_TRAMs == 0) 
			numTRAMs = 1;
		else
			numTRAMs = 2*MgHinv.RA_TRAMs; 
	}

	/* debug */
	msg_printf(DBG, "mg_reset: RBRev = 0x%x, numRE4s = %d, numTRAMs = %d\n",
		MgHinv.RBRev, numRE4s, numTRAMs);

	if (MgHinv.ProductId != 0x1) {
		if ((numTRAMs != 1) && (numTRAMs !=4)) {
			msg_printf(ERR, "Board Version Register #0's TRAM bits are 0x%x, but that is an invalid configuration. Please check Board Version Register #0\n", MgHinv.RA_TRAMs);
			msg_printf(SUM, "WARNING: Texture tests will not work properly.\n");
		}
	}
#endif  /* if not HQ4 */

 	/* Get everything from Board Version Register 1 */
	MgHinv.GDRev = (BdVer1_Reg >>2) & 0x3;		/* GD Revision */
	MgHinv.GEs = BdVer1_Reg & 0x3;			/* #GE's Found */

	msg_printf(level,"ProductId = %x\n", MgHinv.ProductId);
	msg_printf(level, "RARev = %x , RBRev = %x , GDRev = %x\n", MgHinv.RARev, MgHinv.RBRev, MgHinv.GDRev);
	msg_printf(level, "RA_TRAMs = %x , RB_TRAMs = %x\n", numTRAMs, numTRAMs);

	msg_printf(level, "PP1Rev = %x\n", MgHinv.PP1Rev);
	msg_printf(level, "HQ3Rev = %x , GE11Rev = %x\n" , MgHinv.HQ3Rev, MgHinv.GE11Rev);
	msg_printf(level, "RE4Rev = %x,  TE1Rev = %x , TRAMRev = %x\n" , MgHinv.RE4Rev, MgHinv.TE1Rev, MgHinv.TRAMRev);
	msg_printf(level, "GEs = %x , REs = %x\n", MgHinv.GEs, MgHinv.REs);

	msg_printf(level, "======		MGRAS	HINV	======\n");

	return 0;
}

__uint32_t
mgras_is_solid(void) {
	return(_mg_solid_impact);
}


__uint32_t
_pp1_xmap_init(void)
{
	 __uint32_t dcr, i, errors = 0;

       	msg_printf(DBG, "_pp1_xmap_init().\n");
       	msg_printf(DBG, "numre4s: %d\n", numRE4s);
	msg_printf(DBG,"MgHinv.RBRev %d\n", MgHinv.RBRev);

	/* Enable broadcast writes to all PP1s */
	/* Write MGRAS_XMAP_WRITEALLPP1 (0x01) to MGRAS_XMAP_PP1_SELECT */
	HQ3_PIO_WR(MGRAS_XMAP_PP1_SELECT_OFF, 0x01000000, ~0x0);
	
	/* Select Second byte of the config reg :- for AutoInc */
	/* Write 0x01 to MGRAS_XMAP_INDEX */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x01000000, ~0x0);

	msg_printf(DBG, "rb rev: 0x%x\n", MgHinv.RBRev);
	switch (MgHinv.RBRev) {

		/* Write 0x48 to MGRAS_XMAP_CONFIG_BYTE */
		/* 0x3 is valid if we have a csc/tmi board, but there is 
		   not a safe way to probe for csc/tmi in IP28 so we just
		   allow a value of 0x3 */
		case 3:
		case 7: HQ3_PIO_WR(MGRAS_XMAP_CONFIG_BYTE_OFF, 0x48000000, ~0x0); /* 1 RE4 */
			break;

		/* Write 0x58 to MGRAS_XMAP_CONFIG_BYTE */
		case 4:
		case 2:
		case 0: HQ3_PIO_WR(MGRAS_XMAP_CONFIG_BYTE_OFF, 0x58000000, ~0x0); /* 2 RE4 */
			break;
		default: 
			msg_printf(ERR, "The system is reporting an RB Revision of 0x%x which is an illegal value.\n", MgHinv.RBRev);
			return (1);
	}

	/* RTRY =  0x653 */
	/* Write 0x1c to MGRAS_XMAP_INDEX */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x1c000000, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_DIB_OFF, 0x00000653, ~0x0);

	/* RERACTR = 0x1fe14 */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x0, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_RE_RAC_OFF, 0x0001fe14, ~0x0);

    if (mg_timing_init) {
	/* REFCTR w 0x13ff801 */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x20000000, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_DIB_OFF, mg_tinfo->REFctl, ~0x0);
    }
    else {
	/* REFCTR w 0x13ff801 */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x20000000, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_DIB_OFF, 0x013ff801, ~0x0);
    }

	/* CFG w 0x4d5000 */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0, ~0x0);
	switch (MgHinv.RBRev) {
		/* 0x3 is valid if we have a csc/tmi board, but there is 
		   not a safe way to probe for csc/tmi in IP28 so we just
		   allow a value of 0x3 */
		case 3:
		case 7: HQ3_PIO_WR(MGRAS_XMAP_CONFIG_OFF, 0x004d5000, ~0x0);
			break;
		case 4:
		case 2:
		case 0: HQ3_PIO_WR(MGRAS_XMAP_CONFIG_OFF, 0x005d5000, ~0x0);
			break;
		default: msg_printf(ERR, "The system is reporting an RB Revision of 0x%x which is an illegal value.\n", MgHinv.RBRev);
	}

    if (mg_timing_init) {
	/* SKIP w 0x3fffff */
	/* Write 0x3ffffff to MGRAS_XMAP_DIB (DIBskip) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x08000000, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_DIB_OFF, mg_tinfo->DIBskip, ~0x0);

	/* DIBPTR w 0x1c0c8240 */
	/* Write 0x1c0c8240 to MGRAS_XMAP_DIB (DIBptr) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x0, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_DIB_OFF, mg_tinfo->DIBptr, ~0x0);

	/* TSCAN w 0x3ff */
	/* Write 0x3ff to MGRAS_XMAP_DIB (DIBtopscan) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x04000000, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_DIB_OFF, mg_tinfo->DIBtopscan, ~0x0);

	/* DIBCTR0 w 0x0bb */
	/* Write 0x0bb to MGRAS_XMAP_DIB (DIBctrl0) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x0c000000, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_DIB_OFF, mg_tinfo->DIBctl0, ~0x0);

	/* DIBCTR1 w 0x1bb */
	/* Write 0x1bb to MGRAS_XMAP_DIB (DIBctrl1) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x10000000, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_DIB_OFF, mg_tinfo->DIBctl1, ~0x0);

	/* DIBCTR2 w 0x124 */
	/* Write 0x124 to MGRAS_XMAP_DIB (DIBctrl2) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x14000000, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_DIB_OFF, mg_tinfo->DIBctl2, ~0x0);

	/* DIBCTR3 w 0x140 */
	/* Write 0x140 to MGRAS_XMAP_DIB (DIBctrl3) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x18000000, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_DIB_OFF, mg_tinfo->DIBctl3, ~0x0);
    }
    else {
	/* SKIP w 0x3fffff */
	/* Write 0x3ffffff to MGRAS_XMAP_DIB (DIBskip) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x08000000, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_DIB_OFF, 0x003fffff, ~0x0);

	/* DIBPTR w 0x1c0c8240 */
	/* Write 0x1c0c8240 to MGRAS_XMAP_DIB (DIBptr) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x0, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_DIB_OFF, 0x1c0c8240, ~0x0);

	/* TSCAN w 0x3ff */
	/* Write 0x3ff to MGRAS_XMAP_DIB (DIBtopscan) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x04000000, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_DIB_OFF, 0x000003ff, ~0x0);

	/* DIBCTR0 w 0x0bb */
	/* Write 0x0bb to MGRAS_XMAP_DIB (DIBctrl0) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x0c000000, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_DIB_OFF, 0x000000bb, ~0x0);

	/* DIBCTR1 w 0x1bb */
	/* Write 0x1bb to MGRAS_XMAP_DIB (DIBctrl1) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x10000000, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_DIB_OFF, 0x000001bb, ~0x0);

	/* DIBCTR2 w 0x124 */
	/* Write 0x124 to MGRAS_XMAP_DIB (DIBctrl2) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x14000000, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_DIB_OFF, 0x00000124, ~0x0);

	/* DIBCTR3 w 0x140 */
	/* Write 0x140 to MGRAS_XMAP_DIB (DIBctrl3) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x18000000, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_DIB_OFF, 0x00000140, ~0x0);
    }

	/* MBSEL w 0x0 */
	/* Write 0x0 to MGRAS_XMAP_BUF_SELECT (Main) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_BUF_SELECT_OFF, 0, ~0x0);

	/* OBSEL w 0x0 */
	/* Write 0x0 to MGRAS_XMAP_BUF_SELECT (Overlay) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x04000000, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_BUF_SELECT_OFF, 0, ~0x0);

	/* Init the Main mode register file (MGRAS_XMAP_MAIN_MODE) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0, ~0x0);
	for (i = 0; i < 32; i++) {
		HQ3_PIO_WR(MGRAS_XMAP_MAIN_MODE_OFF, 0x00000004, ~0x0);
	}
		
	/* Init the Olay mode register file (MGRAS_XMAP_OVERLAY_MODE) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0, ~0x0);
	for (i = 0; i < 8; i++) {
		HQ3_PIO_WR(MGRAS_XMAP_OVERLAY_MODE_OFF, 0x0, ~0x0);
	}
		
    if (mg_timing_init) {
	/* turn display back on */
	/* Write 0x1bb to MGRAS_XMAP_DIB (DIBctrl0) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x0c000000, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_DIB_OFF, mg_tinfo->DIBctl0, ~0x0);
    }
    else {
	/* turn display back on */
	/* Write 0x1bb to MGRAS_XMAP_DIB (DIBctrl0) */
	HQ3_PIO_WR(MGRAS_XMAP_INDEX_OFF, 0x0c000000, ~0x0);
	HQ3_PIO_WR(MGRAS_XMAP_DIB_OFF, 0x000001bb, ~0x0);
    }

       	msg_printf(DBG, "_pp1_xmap_init() Done.\n");

	return 0;
}
 

__uint32_t
mg_initppregs()
{
	__uint32_t rssnum = 4;
	__uint32_t Xtiles9 = 2;		/* Default for 1280x1024 */
	__uint32_t Xtiles36 = 7;	/* Default for 1280x1024 */
	__uint32_t drb_size = 0x25e;	/* Default for 1280x1024, 2RSS */
	__uint32_t drb_misc = 0x9;    /* DRB_RSS=0x1, DMAlinecnt=0x2, AFE=0 */
	__uint32_t drb_ptrs = 0xc8240;	/* Default for 1280x1024 */

	/* Calculate DRBsize */

#if !HQ4
	Xtiles9 = mg_tinfo->w/768;
	if ((Xtiles9 * 768) < XMAX) {
		Xtiles9 += 1;
	}

	Xtiles36 = mg_tinfo->w/192;
	if ((Xtiles36 * 192) < XMAX) {
		Xtiles36 += 1;
	}

	drb_size = (drb_misc << 6) | (Xtiles36 << 2) | Xtiles9;

#endif
	/* Set drb_ptrs */
	drb_ptrs = mg_tinfo->DIBptr;

	/* from mgshell.script.bob */
	WRITE_RSS_REG(rssnum, 0x112, 4, ~0x0);
	WRITE_RSS_REG(rssnum, 0x160, 0x00000000, ~0x0);	/* PixCmd Reg */
	WRITE_RSS_REG(rssnum, 0x161, 0x00004200, ~0x0);	/* Fillmode Reg */
	WRITE_RSS_REG(rssnum, 0x162, 0x000000ff, ~0x0);	/* ColMaskMSBs Reg */
	WRITE_RSS_REG(rssnum, 0x163, 0xffffffff, ~0x0);	/* ColMaskLSBsA Reg */
	WRITE_RSS_REG(rssnum, 0x164, 0xffffffff, ~0x0);	/* ColMaskLSBsB Reg */
	WRITE_RSS_REG(rssnum, 0x165, 0x58840000, ~0x0);	/* BlendFactor Reg */
	WRITE_RSS_REG(rssnum, 0x166, 0x00000007, ~0x0);	/* Stencilmode Reg */
	WRITE_RSS_REG(rssnum, 0x167, 0x0000ffff, ~0x0);	/* StencilMask Reg */
	WRITE_RSS_REG(rssnum, 0x168, 0x0ffffff1, ~0x0);	/* Zmode Reg */
	WRITE_RSS_REG(rssnum, 0x169, 0x00000007, ~0x0);	/* Afuncmode Reg */
	WRITE_RSS_REG(rssnum, 0x16a, 0x00000000, ~0x0);	/* Accmode Reg */
	WRITE_RSS_REG(rssnum, 0x16d, drb_ptrs,   ~0x0);	/* DRBpointers Reg */
#if !HQ4
	switch (MgHinv.RBRev) {
		/* 0x3 is valid if we have a csc/tmi board, but there is 
		   not a safe way to probe for csc/tmi in IP28 so we just
		   allow a value of 0x3 */
		case 3:
		case 7: WRITE_RSS_REG(rssnum, 0x16e, 0x0000031e, ~0x0);	/* DRBsize Reg */
			break;
		case 4:
		case 2:
		case 0: 
		    WRITE_RSS_REG(rssnum, 0x16e, drb_size, ~0x0);	/* DRBsize Reg */
			break;
		default: msg_printf(ERR, "The system is reporting an RB Revision of 0x%x which is an illegal value.\n", MgHinv.RBRev);
	}
#else
	if (numRE4s == 1) {
	    WRITE_RSS_REG(rssnum, 0x16e, 0x0000031e, ~0x0); /* DRBsize Reg */
	} else {
	    WRITE_RSS_REG(rssnum, 0x16e, 0x0000025e, ~0x0); /* DRBsize Reg */
	}
#endif
	WRITE_RSS_REG(rssnum, 0x16f, 0x00000000, ~0x0);	/* TAGmode Reg */
	WRITE_RSS_REG(rssnum, 0x068, 0x00000000, ~0x0);	
	WRITE_RSS_REG(rssnum, 0x069, 0x00000000, ~0x0);	
	WRITE_RSS_REG(rssnum, 0x176, 0x01a08190, ~0x0);	/* PIXcolorR Reg */
	WRITE_RSS_REG(rssnum, 0x177, 0x000807f8, ~0x0);	/* PIXcolorG Reg */
	WRITE_RSS_REG(rssnum, 0x178, 0x000807f8, ~0x0);	/* PIXcolorB Reg */
	WRITE_RSS_REG(rssnum, 0x179, 0xfff00ff0, ~0x0);	/* PIXcolorA Reg */
	WRITE_RSS_REG(rssnum, 0x170, 0x00081a19, ~0x0);	/* TAGdata_R Reg */
	WRITE_RSS_REG(rssnum, 0x171, 0x0008807f, ~0x0);	/* TAGdata_G Reg */
	WRITE_RSS_REG(rssnum, 0x172, 0x0008807f, ~0x0);	/* TAGdata_B Reg */
	WRITE_RSS_REG(rssnum, 0x173, 0x000000ff, ~0x0);	/* TAGdata_A Reg */
	WRITE_RSS_REG(rssnum, 0x17b, 0x00000000, ~0x0);	/* WINmode Reg */

	return 0;
}


__uint32_t mgras_Init(void)
{
	__uint32_t pp1_ctrl, dac_ctrl, cmapall_ctrl, cmap0_ctrl, cmap1_ctrl, vc3_ctrl, bdver_ctrl, errors = 0;

	msg_printf(DBG, "Initialize DCB registers\n");

#if !HQ4
        mgbase->bfifo_hw          = 0xa;
        mgbase->bfifo_delay       = 0xff;
#endif
        mgbase->bfifo_lw          = 0x2;

#if HQ4
        mgbase->cfifo_hw          = 40;
#else
        mgbase->cfifo_hw          = 40;
#endif
        mgbase->cfifo_lw          = 20;
        mgbase->cfifo_delay       = 10;

        mgbase->dfifo_hw          = 40;
        mgbase->dfifo_lw          = 20;
        mgbase->dfifo_delay       = 10;

#if 0
        mgras_vc3SetReg(mgbase, VC3_CONFIG, 0x1);

	mgras_xmapSetPP1Select(mgbase, 0x1);	/* Write ALL PP1's */
#endif
#if !HQ4

	/* determine RB revision -- repeated mgras_gfx_info... yeesh */
	MgHinv.ProductId = ((mgbase->bdvers.bdvers1 >> 24) & 0x60) >> 5;
	if (MgHinv.ProductId == 0x0) { /* not MG10 */
		MgHinv.RBRev = (mgbase->bdvers.bdvers0 >> 28)  & 0x7;
	}
	else if (MgHinv.ProductId == 0x1) { /* is MG10 */
		MgHinv.RBRev = 0x7;	/* No RB in mg10 */
	}

#else

	    MGRAS_GFX_SETUP(0x0, 1);

{
ushort_t BdVer0_Reg, BdVer1_Reg;


    bzero(&MgHinv, sizeof(MgHinv));

    msg_printf(DBG, "======     MGRAS   HINV    ======\n");

    /* First get the Board Revision Registers */
    BdVer0_Reg = (mgbase->bdvers.bdvers0 >> 24);
        BdVer1_Reg = (mgbase->bdvers.bdvers1 >> 24);

    msg_printf(DBG, "BdVer0_Reg 0x%x\t", BdVer0_Reg);
    msg_printf(DBG, "BdVer1_Reg 0x%x\n", BdVer1_Reg);

    MgHinv.ProductId = (BdVer1_Reg & 0x60) >> 5;
    MgHinv.GDRev = (BdVer1_Reg >>2) & 0x3;      /* GD Revision */
    MgHinv.GEs = BdVer1_Reg & 0x3;          /* #GE's Found */
    _mg_solid_impact = 0x0; /* Assume Not Solid Impact */

    if (MgHinv.ProductId == 3) { /* not GM10 */
        /* Get everything from Board Version Register 0 */
        MgHinv.RA_TRAMs = (BdVer0_Reg & 0x3);           /* RA TRAMs */
            MgHinv.RB_TRAMs = (BdVer0_Reg >> 2) & 0x3;      /* RB TRAMs */
        MgHinv.REs = numRE4s = 2;
        MgHinv.RBRev = 0x0;
    } else if (MgHinv.ProductId == 2) { /* is GM10 */
        MgHinv.RA_TRAMs = (BdVer0_Reg & 0x3);       /* RA TRAMs */
        MgHinv.RB_TRAMs = 0;                /* RB TRAMs */
        _mg_solid_impact = 0x1; /* Solid Impact */
        MgHinv.REs = numRE4s = 1;
        MgHinv.RBRev = 0x7;
    }

    if (MgHinv.RA_TRAMs == 0x1) {
        _mg_solid_impact = 0;
        numTRAMs = 4;
    } else {
        _mg_solid_impact = 0x1; /* Solid Impact */
        numTRAMs = 0;
    }

    /* debug */
    msg_printf(DBG, "mgras_gfx_info: numRE4s = %d, numTRAMs = %d\n",
        numRE4s, numTRAMs);

}

    /*_pp1_xmap_init(); */
    YMAX = 1024;
    XMAX = 1344;

#endif
	msg_printf(DBG, "mgras_Init: rbrev = 0x%x\n", MgHinv.RBRev);
	msg_printf(DBG, "mgras_Init: ProductId = 0x%x\n", MgHinv.ProductId);
	msg_printf(DBG, "mgras_Init: bdvers1 = 0x%x\n", mgbase->bdvers.bdvers1);
	msg_printf(DBG, "mgras_Init: bdvers0 = 0x%x\n", mgbase->bdvers.bdvers0);

	errors += _pp1_xmap_init();

	_mgras_init_gold_crc(); /* initialize the CRC values from the Gold HW */

	msg_printf(DBG, "mgras_Init Complete...\n");
	return (errors);
}

__uint32_t
mgras_hq_fifo_init(void)
{
	msg_printf(DBG, "Entering mgras_hq_fifo_init...\n");
#if !HQ4
        mgbase->bfifo_hw          = 0xa;
        mgbase->bfifo_delay       = 0xff;
#endif
        mgbase->bfifo_lw          = 0x2;

        mgbase->cfifo_hw          = 40;
        mgbase->cfifo_lw          = 20;
        mgbase->cfifo_delay       = 10;

        mgbase->dfifo_hw          = 40;
        mgbase->dfifo_lw          = 20;
        mgbase->dfifo_delay       = 10;

	msg_printf(DBG, "Leaving mgras_hq_fifo_init...\n");

	return 0;
}

__uint32_t
mgras_GetMon_Type(void) 
{
	ushort_t CmapARevReg, MonType = 0 /* XXX who sets MonType?? */;

	mgras_cmapGetRev(mgbase, CmapARevReg, 0);
	MonType >>= 4;
	switch (CmapARevReg & 0xf) {
		case 0x0:
				msg_printf(DBG, "MON_TYPE: 0x%x NOT DEFINED YET\n", MonType);
				_mgras_VC3Init(_1280_1024_76);
			break;
		case 0x1:
				msg_printf(DBG, "MON_TYPE: 0x%x :- 19inch, 1280 x 1024 Multi-Scan, Multi-Sync 30-82 kHz\n");
				_mgras_VC3Init(_1280_1024_60);
			break;
		case 0x2:
				msg_printf(DBG, "MON_TYPE: 0x%x :- 16inch, Multi-Scan, 30-82 kHz\n");
				_mgras_VC3Init(_1280_1024_60);
			break;
		case 0x3:
				msg_printf(DBG, "MON_TYPE: 0x%x NOT DEFINED YET\n", MonType);
				_mgras_VC3Init(_1280_1024_76);
			break;
		case 0x4:
				msg_printf(DBG, "MON_TYPE: 0x%x NOT DEFINED YET\n", MonType);
				_mgras_VC3Init(_1280_1024_76);
			break;
		case 0x5:
				msg_printf(DBG, "MON_TYPE:  0x%x NOT DEFINED YET\n", MonType);
				_mgras_VC3Init(_1280_1024_76);
			break;
		case 0x6:
				msg_printf(DBG, "MON_TYPE: 0x%x :- 15inch, 1024 x 768\n", MonType);
				_mgras_VC3Init(_1280_1024_60);
			break;
		case 0x7:
				msg_printf(DBG, "MON_TYPE: 0x%x NOT DEFINED YET\n", MonType);
				_mgras_VC3Init(_1280_1024_76);
			break;
		case 0x8:
				msg_printf(DBG, "MON_TYPE: 0x%x :- 1280 x 1024,  Stereo capable\n", MonType);
				_mgras_VC3Init(_1280_1024_60);
			break;
		case 0x9:
				msg_printf(DBG, "MON_TYPE: 0x%x :- 19inch, Multi-Scan and 72Hz Single Scan.\n", MonType);
				_mgras_VC3Init(_1280_1024_60);
			break;
		case 0xA:
				msg_printf(DBG, "MON_TYPE: 0x%x :- 16inch, Multi-Scan\n", MonType);
				_mgras_VC3Init(_1280_1024_60);
			break;
		case 0xB:
				msg_printf(DBG, "MON_TYPE: 0x%x :- 21inch, Multi-Scan, Stereo capable\n", MonType);
				_mgras_VC3Init(_1280_1024_60);
			break;
		case 0xC:
				msg_printf(DBG, "MON_TYPE: 0x%x :- 19inch, 1280 x 1024 and 1024 x 768\n", MonType);
				_mgras_VC3Init(_1280_1024_60);
			break;
		case 0xD:
				msg_printf(DBG, "MON_TYPE: 0x%x :- 16inch, 1280 x 1024 and 1024 x 768\n", MonType);
				_mgras_VC3Init(_1280_1024_60);
			break;
		case 0xE:
				msg_printf(DBG, "NON_EXISTANT --- MON_TYPE: 0x%x :- 14inch, 1024 x 768 @60Hz\n", MonType);
#if DOES_NOT_EXIST
				_mgras_VC3Init(_1024_768_60);
#endif
			break;
		case 0xF:
				msg_printf(DBG, "MON_TYPE: 0x%x :- 1280 x 1024 @60Hz\n", MonType);
				_mgras_VC3Init(_1280_1024_60);
			break;
		default:
				msg_printf(DBG, "MON_TYPE: 0x%x :- Default, 1280 x 1024 @60Hz, :- no monitor installed.\n", MonType);
				_mgras_VC3Init(_1280_1024_60);
			break;
	}

	return (MonType);

}


__uint32_t
mgras_Board_Config_Regs()
{
	 ushort_t CmapBRevReg;
	 ushort_t BdVer0_Reg, BdVer1_Reg, RaRev, MgVideo;

#ifndef MGRAS_BRINGUP
	
	mgras_GetMon_Type();

	mgras_cmapGetRev(mgbase, CmapBRevReg, 1);
	RaRev = (CmapBRevReg >> 4) & 0x7 ;
	RaRev &= 0x7;
	if (RaRev) 
		msg_printf(DBG, "RA Board version 0x%x\t" ,RaRev);

	MgVideo = (CmapBRevReg >> 6 ) & 0x1 ;
	if (MgVideo) {
		msg_printf(DBG, "MGRAS video board not installed\n");
	} else {
		msg_printf(DBG, "MGRAS video board found.\n");
	}

	BdVer0_Reg = mgbase->bdvers.bdvers0;
	msg_printf(DBG, "RA: 0x%x TRAMS\t", ((BdVer0_Reg >> 2) & 0x3) );
	if (((BdVer0_Reg >> 6) & 0x7) != 0x7) {
		msg_printf(DBG, "RB Rev0x%x, RB: 0x%x TRAMS\n", (BdVer0_Reg >> 6), (BdVer0_Reg & 0x3) );
	} else {
		msg_printf(DBG, "RB not installed.\n");
	}

	BdVer1_Reg = mgbase->bdvers.bdvers1;
	msg_printf(DBG, "Prod ID 0x%x, 0x%x GD Revision, 0x%x GE's Found.\n" ,((BdVer1_Reg >>3) & 0x3), ((BdVer1_Reg >>1) & 0x7), (BdVer1_Reg & 0x1));
#endif

	return 0;
}



__uint32_t
mgras_Reset(__uint32_t argc, char **argv)
{
	__uint32_t status;
	tram_cntrlu  tram_cntrl = {0};

	bzero(&MgHinv, sizeof(MgHinv)); 

	mg_timing_init = 0;	/* Set default to not initialized */

	/* get the args */
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	    switch (argv[0][1]) {
		case 's':
		    if (argv[0][2]=='\0') {
			mg_timing_init = 1;
			argc--; argv++;
		    } 
		    else {
			mg_timing_init = 1;
		    }
		    break;
		default: break;
	    }
	    argc--; argv++;
	}

	/* allocate space */
	if (mginfo == 0)
		mginfo = (struct mgras_info *)malloc(sizeof(struct mgras_info));

	/* Initialize MGRAS mginfo struct */
	MgrasGatherInfo(mgbase, mginfo);

	if (!mg_timing_init) {

	    /* Set mg_tinfo to point to timing table for _1280_1024_60 VOF */
	    MgrasRecordTiming(mgbase, mgras_timing_tables_1RSS[0]);
	    mg_tinfo = mgras_video_timing[mgras_baseindex(mgbase)];
	}

	msg_printf(VRB, "Mgras Initialize\n");
	status = mgras_Init();
	if (status) {
		msg_printf(ERR, "mgras_Init not properly Initialized\n");
		return 1;
	}

	msg_printf(VRB, "Mgras DacReset\n");
	status = mgras_DacReset();
	if (status) {
		msg_printf(ERR, "mgras_DacReset not properly Reset\n");
		return 1;
	}

	msg_printf(VRB, "Mgras VC3 Reset\n");
	status = mgras_VC3Reset();
	if (status) {
		msg_printf(ERR, "mgras_VC3Reset not properly Reset\n");
		return 1;
	}

	if (!mg_timing_init) {
            _mgras_VC3Init(_1280_1024_60);

	    /* Set mg_tinfo to point to timing table for _1280_1024_60 VOF */
	    mg_tinfo = &mgras_1280_1024_60_1RSS;
	    mg_tinfo = mgras_video_timing[mgras_baseindex(mgbase)];


	    /* Initialize XMAX and YMAX to suitable values for default VOF */
	    XMAX = 1344;
	    YMAX = 1024;
	}
	else {
	    MgrasSetTiming(mgbase, mginfo, 0, mg_tinfo, NULL);
	    MgrasInitVC3(mgbase);
	    XMAX = mg_tinfo->w;
	    YMAX = mg_tinfo->h;
	}

	msg_printf(VRB, "Loading Cmaps with a Linear Ramp\n");
	CmapLoad(LINEAR_RAMP);

	msg_printf(VRB, "Loading Gamma Tables with a Linear Ramp\n");
	GammaTableLoad(LINEAR_RAMP);

#if HQ4
#else
	/* set up tram refresh */
	tram_cntrl.bits.refresh = 8;         /* 8us refresh */
   	tram_cntrl.bits.ref_count = 1;       /* 1 page refreshed per refresh */

	WRITE_RSS_REG(4, CONFIG, 4, ~0x0);
	WRITE_RSS_REG(4, TRAM_CNTRL, tram_cntrl.val, ~0x0);
	WRITE_RSS_REG(4, XYWIN, 0x0, 0xffffffff);
#endif
	fpusetup();
	mgras_gfx_info(0,0);

	global_dbuf = 1; /* color buffer AB */

	if (!mg_timing_init) {
            _mgras_VC3Init(_1280_1024_60);
	}
	else {
	    MgrasInitVC3(mgbase);
	}

	return 0;
}

void
mgras_tram_control(void)
{
#if 0
	tram_cntrlu  tram_cntrl = {0};

	/* set up tram refresh */
	tram_cntrl.bits.refresh = 8;         /* 8us refresh */
   	tram_cntrl.bits.ref_count = 1;       /* 1 page refreshed per refresh */

	WRITE_RSS_REG(4, CONFIG, 4, ~0x0);
	WRITE_RSS_REG(4, TRAM_CNTRL, tram_cntrl.val, ~0x0);
	WRITE_RSS_REG(4, XYWIN, 0x0, 0xffffffff);
#endif
	fpusetup();

	global_dbuf = 1; /* color buffer AB */

    _mgras_VC3Init(_1280_1024_60);
}

/*
 * mg_re4reset is the same as mgras_Reset with just the re4 chip being touched
 *
 */
__uint32_t
mg_re4reset(__uint32_t argc, char **argv)
{
	__uint32_t status;
	tram_cntrlu  tram_cntrl = {0};

	bzero(&MgHinv, sizeof(MgHinv)); 

	mg_timing_init = 0;	/* Set default to not initialized */

	/* get the args */
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	    switch (argv[0][1]) {
		case 's':
		    if (argv[0][2]=='\0') {
			mg_timing_init = 1;
			argc--; argv++;
		    } 
		    else {
			mg_timing_init = 1;
		    }
		    break;
		default: break;
	    }
	    argc--; argv++;
	}

	/* allocate space */
	if (mginfo == 0)
		mginfo = (struct mgras_info *)malloc(sizeof(struct mgras_info));

	if (!mg_timing_init) {
	    /* Set mg_tinfo to point to timing table for _1280_1024_60 VOF */
	    MgrasRecordTiming(mgbase, mgras_timing_tables_1RSS[0]);
	    mg_tinfo = mgras_video_timing[mgras_baseindex(mgbase)];
	}

	msg_printf(VRB, "Mgras Initialize\n");
	status = mgras_Init();
	if (status) {
		msg_printf(ERR, "mgras_Init not properly Initialized\n");
		return 1;
	}

	/* set up tram refresh */
	tram_cntrl.bits.refresh = 8;         /* 8us refresh */
   	tram_cntrl.bits.ref_count = 1;       /* 1 page refreshed per refresh */

	WRITE_RSS_REG(4, CONFIG, 4, ~0x0);
	WRITE_RSS_REG(4, TRAM_CNTRL, tram_cntrl.val, ~0x0);
	WRITE_RSS_REG(4, XYWIN, 0x0, 0xffffffff);

	fpusetup();
	mgras_gfx_rssinfo(0,0);

	global_dbuf = 1; /* color buffer AB */

	return 0;
}

/*
 * mg_bkendreset is the same as mgras_Reset with just mgras_Init
 *
 */
__uint32_t
mg_bkendreset(__uint32_t argc, char **argv)
{
	__uint32_t status;
	tram_cntrlu  tram_cntrl = {0};

	bzero(&MgHinv, sizeof(MgHinv)); 

	mg_timing_init = 0;	/* Set default to not initialized */

	/* get the args */
	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	    switch (argv[0][1]) {
		case 's':
		    if (argv[0][2]=='\0') {
			mg_timing_init = 1;
			argc--; argv++;
		    } 
		    else {
			mg_timing_init = 1;
		    }
		    break;
		default: break;
	    }
	    argc--; argv++;
	}

	/* allocate space */
	if (mginfo == 0)
		mginfo = (struct mgras_info *)malloc(sizeof(struct mgras_info));

	/* Initialize MGRAS mginfo struct */
	MgrasGatherInfo(mgbase, mginfo);

	if (!mg_timing_init) {
	    /* Set mg_tinfo to point to timing table for _1280_1024_60 VOF */
	    MgrasRecordTiming(mgbase, mgras_timing_tables_1RSS[0]);
	    mg_tinfo = mgras_video_timing[mgras_baseindex(mgbase)];
	}

	msg_printf(VRB, "Mgras Initialize\n");
	status = mgras_Init();
	if (status) {
		msg_printf(ERR, "mgras_Init not properly Initialized\n");
		return 1;
	}

	msg_printf(DBG, "Mgras DacReset\n");
	status = mgras_DacReset();
	if (status) {
		msg_printf(ERR, "mgras_DacReset not properly Reset\n");
		return 1;
	}

	msg_printf(DBG, "Mgras VC3 Reset\n");
	status = mgras_VC3Reset();
	if (status) {
		msg_printf(ERR, "mgras_VC3Reset not properly Reset\n");
		return 1;
	}

	if (!mg_timing_init) {
            _mgras_VC3Init(_1280_1024_60);

	    /* Set mg_tinfo to point to timing table for _1280_1024_60 VOF */
	    mg_tinfo = &mgras_1280_1024_60_1RSS;
	    mg_tinfo = mgras_video_timing[mgras_baseindex(mgbase)];


	    /* Initialize XMAX and YMAX to suitable values for default VOF */
	    XMAX = 1344;
	    YMAX = 1024;
	}
	else {
	    MgrasSetTiming(mgbase, mginfo, 0, mg_tinfo, NULL);
	    MgrasInitVC3(mgbase);
	    XMAX = mg_tinfo->w;
	    YMAX = mg_tinfo->h;
	}

	msg_printf(DBG, "Loading Cmaps with a Linear Ramp...\n");
	CmapLoad(LINEAR_RAMP);

	msg_printf(DBG, "Loading Gamma Tables with a Linear Ramp...\n");
	GammaTableLoad(LINEAR_RAMP);

	mgras_gfx_info(0,0);

	if (!mg_timing_init) {
            _mgras_VC3Init(_1280_1024_60);
	}
	else {
	    MgrasInitVC3(mgbase);
	}

	return 0;
}

/* returns: 	1 = mgras board exists in slot 0
		2 = mgras in slot 1
		3 = mgras in both slots 0, 1
		0 = no mgras found
*/
__uint32_t
mg_probe()
{
	__uint32_t found, gioslot, port;

	/* check both available slots to see if a mgras board exists */

	found = 0;
#if !HQ4
	for (gioslot = 0; gioslot < 2; gioslot++) {
	   port = gioslot;
	   if (gioslot == 0)	
		mgbase  = (mgras_hw *)PHYS_TO_K1(MGRAS_BASE0_PHYS);
	   else if (gioslot == 1)
		mgbase  = (mgras_hw *)PHYS_TO_K1(MGRAS_BASE1_PHYS);

	   found |= (MgrasProbe(mgbase, port) << gioslot);
	}
#else
    for ( port = XBOW_PORT_9 ; port < XBOW_PORT_F ; port++ ) {
                mgbase  = (mgras_hw *) K1_MAIN_WIDGET(port);
                msg_printf(DBG, "mgbase 0x%x port 0x%x\n", mgbase, port);
                if (_mgras_hq4probe (mgbase, port)) {
	  				found = port;
				}
	}
#endif
	
	return(found);
}

void
mg_pon_puts(__uint32_t argc, char **argv)
{
   /* get the args */
   argc--; argv++;
   while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 's':
                        if (argv[0][2]=='\0') {
                                pon_puts(&argv[1][0]);
                                argc--; argv++;
                        } else {
                                pon_puts(&argv[1][0]);
                        }
                        break;
                default: break;
        }
        argc--; argv++;
   }
}

char
mgras_get_test_suite()
{
    char *pvar;
    int  suite;

    pvar = getenv("OSLoader");
    if (pvar) {
    	if (strcmp(pvar, "sash") == 0) {
		suite = 0;
    	} else if (strcmp(pvar, "ge") == 0) {
		suite = 2;
    	} else {
		suite = 1;
    	}
    } else {
		suite = 1;
    }

    return(suite);
}

void
mgras_brdcfg_write(__uint32_t argc, char **argv)
{
    __uint32_t bad_arg = 0, value = 0;

    msg_printf(DBG, "Entering mgras_brdcfg_write...\n");

   /* get the args */
   argc--; argv++;
   while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	switch (argv[0][1]) {
		case 'v':
			if (argv[0][2]=='\0') {
				atobu(&argv[1][0], &value);
				argc--; argv++;
			} else {
				atobu(&argv[0][2], &value);
			}
			break;
		default: bad_arg++; break;
	}
	argc--; argv++;
   }

   if ( bad_arg || argc ) {
	msg_printf(SUM,
	 "Usage: mg_cfg_wr -v[config value]\n");
	 return;
   }
   msg_printf(DBG, "value = 0x%x\n", value);
   mgbase->bdvers.bdvers0 = ((value & 0xff) << 24);

   msg_printf(DBG, "Leaving mgras_brdcfg_write...\n");
}


void
mgras_pixel_clock_doubler_write(__uint32_t argc, char **argv)
{
   __uint32_t i, j, tmp;
   uchar_t	PLLbyte[7];

   msg_printf(DBG, "Entering mgras_pixel_clock_doubler_write...\n");

   if (argc < (7 + 1)) {
    	msg_printf(ERR, "Usage: mg_pcd_wr byte0 byte1 etc.\n");
		return;
   }
   for (i=0; i< 7;i++ ) {
    	argv++;
    	atobu(*argv,&tmp);
	PLLbyte[i] = (uchar_t)tmp;	/* should be no truncation */
	msg_printf(DBG, "byte = 0x%x\n", PLLbyte[i]);
   }

   /* write all 7 bytes */
   _mgras_pcd_write(PLLbyte);

   msg_printf(DBG, "Leaving mgras_pixel_clock_doubler_write...\n");
}


void
_mgras_pcd_write(uchar_t *pPLLbyte)
{
#if HQ4
   __uint32_t i, j;
   uchar_t byteVal, nextByte;

   msg_printf(DBG, "Entering _mgras_pcd_write...\n");

   for (i=0; i< 7;i++ ) {
		msg_printf(DBG, " pPLLbyte 0x%x\n", *pPLLbyte);

		nextByte = *pPLLbyte;

		for (j=0;j<8;j++) {

			if((i == 6) && (j ==7)) {
					byteVal = (nextByte & 0x1) | 0x2;
			} else {
               	 	byteVal = nextByte & 0x1;
			}
			msg_printf(DBG, "pcd address 0x%x; data 0x%x\n",
				&(mgbase->pcd.write), byteVal);

            mgbase->pcd.write = byteVal;

			nextByte >>= 1;
		}
		pPLLbyte++;
   }

   msg_printf(DBG, "Leaving _mgras_pcd_write...\n");
#endif
}
