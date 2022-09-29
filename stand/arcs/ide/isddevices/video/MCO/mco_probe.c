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
 *  MCO FPGA (Altera) diagnostics/loading routines
 *
 *  $Revision: 1.9 $
 */

#include <arcs/io.h>
#include <arcs/errno.h>
#include <libsc.h>

#include <math.h>
#include <sys/mgrashw.h>
#ifdef MGRAS_DIAG_SIM
#include "mgras_sim.h"
#else
#include "ide_msg.h"
#endif
/*
 * #include "mgras_diag.h"
 */

extern mgras_hw *mgbase;
extern void mco_set_dcbctrl(mgras_hw *base, int crs);
extern void sr_mco_set_dcbctrl(mgras_hw *base, int crs);
extern void msg_printf(int msglevel, char *fmt, ...);
	
#define MCO_FPGABUFSIZE	0x10000

/* XXX Stuff to go into mgrashw.h eventually */
#define MCO_FPGA_CMD_MODE	0x02	/* Altera chip is in command mode */
#define MCO_FPGA_RESET_DONE	0x00	/* Altera chip has been reset */
#define MCO_FPGA_NOERRS		0x02	/* No errors detected during Altera configuration */

#define MCO_FPGA_RESET		0x00	/* Resets the Altera chip */
#define MCO_FPGA_NORML		0x01	/* Altera chip normal operation */
#define MCO_FPGA_DATA_EN	0x03	/* Enable Altera Data Port */

#define MCO_FPGA_STATUS1_MASK	0x01
#define MCO_FPGA_STATUS2_MASK	0x02
#define MCO_FPGA_STATUS3_MASK	0x03

/* 
 * mco_probe() -- Probes for MCO board and verifies MCO ID
 *		     
 */
__uint32_t
_mco_Probe(void)
{
    __uint32_t mcofound=0;
    int brdid, id;


    /* Set DCBCTRL correctly for MCO CRS(0) and CRS(1) addresses */
    mco_set_dcbctrl(mgbase, MCO_IDREG);

    /* Clear the bus to 0x and 1x and try to read the MCO board ID */
    MCO_PROG_WRITE(mgbase, 0xff);
    MCO_ID_READ(mgbase, id);
    MCO_PROG_WRITE(mgbase, 0x00);
    id = mgbase->mco_mpa1.mco.fpga_config;
    brdid = id & MCO_BOARD_ID_MASK;
#if 0
    rev = id & MCO_BOARD_REV_MASK;
#endif

    msg_printf(DBG,"mco_Probe: Address 0x%x returned 0x%x\n",
    &mgbase->mco_mpa1.mco.fpga_config, id);

    switch (brdid) {
	case MCO_BOARD_ID:
	    mcofound++;
	    msg_printf(DBG, "mco_probe: Impact ICO Board is present\n");
	    break;
	case MCO_BOARD_NOT_PRESENT:
	    msg_printf(DBG, "mco_probe: No MCO Board detected\n");
	    break;
	default:
	    msg_printf(ERR, "mco_probe: Invalid Board ID = 0x%x\n",
		brdid);
	    break;
    }

    return(mcofound);
}

#ifdef IP30
/* 
 * sr_mco_probe() -- Probes for Speedracer MCO board and verifies MCO ID
 *		     
 */
__uint32_t
_sr_mco_Probe(void)
{
    __uint32_t mcofound=0;
    int brdid, id;

    /* Set DCBCTRL correctly for MCO CRS(0) and CRS(1) addresses */
    sr_mco_set_dcbctrl(mgbase, MCO_IDREG);

    /* Clear the bus to 0x and 1x and try to read the MCO board ID */
    MCO_PROG_WRITE(mgbase, 0xff);
#ifdef IP30
    SR_MCO_ID_READ(mgbase, id);
#endif /* IP30 */
    MCO_PROG_WRITE(mgbase, 0x00);
#ifdef IP30
    SR_MCO_ID_READ(mgbase, id);
#endif /* IP30 */
    brdid = id & MCO_BOARD_ID_MASK;
#if 0
    rev = id & MCO_BOARD_REV_MASK;
#endif

    msg_printf(DBG,"mco_Probe: Address 0x%x returned 0x%x\n",
    &mgbase->dcbctrl_0, id);

    switch (brdid) {
	case MCO_BOARD_ID:
	    mcofound++;
	    msg_printf(DBG, "mco_probe: Impact ICO Board is present\n");
	    break;
	case SR_MCO_BOARD_ID:
	    mcofound++;
	    msg_printf(DBG, "mco_probe: Gamera MCO Board is present\n");
	    break;
	case MCO_BOARD_NOT_PRESENT:
	    msg_printf(DBG, "mco_probe: No MCO Board detected\n");
	    break;
	default:
	    msg_printf(ERR, "mco_probe: Invalid Board ID = 0x%x\n",
		brdid);
	    break;
    }

    return(mcofound);
}
#endif /* IP30 */

__uint32_t
mco_Probe(int argc, char **argv)
{
    __uint32_t mcofound=0;
    unsigned char mco_status;
    int debug=0;

    argc--; argv++;	/* Skip Test name */
    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	switch (argv[0][1]) {
	    case 'd' :
		if (argv[0][2]=='\0') { /* has white space */
		    debug++;
		    argc--; argv++;
		}
		break;
	    default  :
		msg_printf(SUM, "usage: mco_probe [-d]\n");
		return debug;
	}
    }

#ifdef IP30
    mco_status = _sr_mco_Probe();
#else
    mco_status = _mco_Probe();
#endif /* IP30 */
    if (!mco_status) {
	mcofound=0;
	return(mcofound);
    }
    else {
	msg_printf(VRB, "mco_Probe: MCO Board is present\n");
    }

    if (debug) {
	msg_printf(DBG, "mco_probe: Extra MCO PAL verification\n");
	/* Twiddle some bits to make sure MCO is alive */

	/* Set all bits high */
	msg_printf(DBG, "Set all bits high\n"); 
	mgbase->mco_mpa1.mco.fpga_control = 0xff;
	mco_status = mgbase->mco_mpa1.mco.fpga_control;

	/* Now make sure bit 2 (NCONFIGRD) is high */
	if (!(mco_status & (1 << 2))) {
	    msg_printf(DBG, "mco_Probe: MCO FPGA Control bit 2 is not set to one\n");
	    return(mcofound);
	}
	else {
	    msg_printf(DBG, "FPGA Control\tBit 2: 1\t\t"); 
	}

	/* Now make sure bit 3 is still low (this bit is set low by the DCB PAL */
	if (mco_status & (1 << 3)) {
	    msg_printf(DBG, "\nmco_Probe: MCO FPGA Control bit 3 is not zero\n");
	    return(mcofound);
	}
	else {
	    msg_printf(DBG, "Bit 3: 0\n"); 
	}

	/* Set all bits low */
	msg_printf(DBG, "Set all bits low\n"); 
	mgbase->mco_mpa1.mco.fpga_control = 0x0;
	mco_status = mgbase->mco_mpa1.mco.fpga_control;

	/* Now make sure bit 2 is low */
	if (mco_status & (1 << 2)) {
	    msg_printf(DBG, "mco_Probe: MCO FPGA Control bit 2 is not zero\n");
	    return(mcofound);
	}
	else {
	    msg_printf(DBG, "FPGA Control\tBit 2: 0\t\t"); 
	}

	/* Now make sure bit 3 is still low (this bit is set low by the DCB PAL */
	if (mco_status & (1 << 3)) {
	    msg_printf(DBG, "\nmco_Probe: MCO FPGA Control bit 3 is not zero\n");
	    return(mcofound);
	}
	else {
	    msg_printf(DBG, "Bit 3: 0\n"); 
	}
    }

    mcofound++;
    msg_printf(VRB, "mco_Probe: MCO Board is present\n");

    return(mcofound);
}

