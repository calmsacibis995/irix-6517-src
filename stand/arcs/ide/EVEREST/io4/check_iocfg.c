/*
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

/***********************************************************************
*	File:		check_iocfg.c					*
*									*
*       Check the io4 configuration registers against the NVRAM values  *
*									*
***********************************************************************/

#ident "arcs/ide/IP19/io/io4/check_iocfg.c $Revision: 1.5 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/addrs.h>
#include <uif.h>
#include <ide_msg.h>
#include <io4_tdefs.h>
#include <prototypes.h>

#define IOCFG_TESTP	"check_iocfg Passed\n"
#define IOCFG_TESTF	"FAILED: check_iocfg\n"

#define	MIN_CONFIG_ADAP	0x1e		/* epc, f, f, s1 */

static int read_io4_cfg(int slot);

int
check_iocfg(int argc, char **argv)
{
    int slot, retval;

    retval = 0;

    if (io4_select(FALSE, argc, argv)) {
	retval++;
	goto EXITPT;
    }

    msg_printf(INFO, "\ncheck_iocfg - Checks IO4 config against NVRAM\n");

    /*
     * iterate through all io adapters in the system
     */
    for (slot = EV_MAX_SLOTS; slot > 0; slot--) {

	/*
	 * if a slot was selected on the command line, start there
	 */
	if (io4_tslot)
	    slot = io4_tslot;

	if (EVCFGINFO->ecfg_board[slot].eb_type == EVTYPE_IO4)
	    retval += read_io4_cfg(slot);
	
	/*
	 * if slot given on command line, we are done
	 */
	if (io4_tslot)
	    break;
    }

EXITPT:
    msg_printf(INFO, (retval == 0) ? IOCFG_TESTP : IOCFG_TESTF);

    return (retval);
}

static int
read_io4_cfg(int slot)
{
    evbrdinfo_t *brd;
    evioacfg_t *adap;
    char *aname;
    int adap_num, retval;
    unsigned char cur_type;
    unsigned long readback, iodev[2];

    retval = 0;

    /* 
     * get pointer to NVRAM configuration for this io4 board
     */
    brd = &EVCFGINFO->ecfg_board[(slot)];

    if (brd->eb_type != EVTYPE_IO4) {
	msg_printf (ERR, "\nERROR: check_iocfg, slot 0x%x\n", slot);
	msg_printf (INFO, "  EVCFGINFO: expected board type 0x%x, got 0x%x\n",
		EVTYPE_IO4, brd->eb_type);
	return(1);
    }
    else {
        msg_printf(VRB, "\ncheck_iocfg, testing slot 0x%x\n", slot);
    }

    /* check big window address */
    msg_printf(VRB, "check_iocfg, IO Window Large Window is 0x%x\n",
	    EV_GET_CONFIG(slot, IO4_CONF_LW));

    /* check little window address */
    msg_printf(VRB, "check_iocfg, IO Window Small Window is 0x%x\n",
	    EV_GET_CONFIG(slot, IO4_CONF_SW));

    msg_printf(VRB, "check_iocfg, NVRAM Window Number is 0x%x\n",
	    brd->eb_io.eb_winnum);

    readback = EV_GET_CONFIG(slot, IO4_CONF_ADAP);
    msg_printf(VRB, "check_iocfg, IO Config Register 2 was 0x%x\n", readback);

    if ((readback & MIN_CONFIG_ADAP) != MIN_CONFIG_ADAP) {
	msg_printf(ERR, "ERROR: check_iocfg, slot 0x%x - missing adapters\n",
	   	     slot);
	msg_printf(VRB, "IO Config Register 2 was 0x%x\n", readback);
	msg_printf(VRB, "  bits 0x%x should be set\n", MIN_CONFIG_ADAP);
	retval = 1;
    }

    /*
     * Get information about the IO adapaters and buiild
     * up the configuration mask appropriately.
     */
    iodev[0] = EV_GET_CONFIG(slot, IO4_CONF_IODEV0);
    iodev[1] = EV_GET_CONFIG(slot, IO4_CONF_IODEV1);

    /*
     * make sure that installed adapter type matches NVRAM config
     */
    for (adap_num = 1; adap_num < IO4_MAX_PADAPS; adap_num++) {
        cur_type = (unsigned char)(iodev[adap_num>>2] >> (8 *(adap_num & 0x3)));
	adap = &brd->eb_ioarr[adap_num];

	/*
	 * get the proper adapter name
	 */
	switch (cur_type) {
	    case IO4_ADAP_NULL:
		aname = "No Adapter Installed";
		break;

	    case IO4_ADAP_SCSI:
	        aname = "S Chip";
	        break;

	    case IO4_ADAP_EPC:
		aname = "EPC Chip";
		break;
	    case IO4_ADAP_FCHIP:
		aname = "F Chip";
		break;

	    case IO4_ADAP_DANG:
		aname = "DANG Chip";
		break;

	    default:
		aname = "Unknown Adapter Type";
		break;
	}

	if (cur_type != adap->ioa_type) {
	    msg_printf(ERR, "ERROR: check_iocfg, slot 0x%x, adap 0x%x\n",
			slot, adap_num);
	    msg_printf(INFO, "  expected 0x%x (%s), was 0x%x\n",
			adap->ioa_type, aname, cur_type);
	    retval = 1;
	}
	else {
	    msg_printf(VRB, "slot 0x%x, adap 0x%x was type 0x%x (%s)\n",
			slot, adap_num, cur_type, aname);
	}
    }

    /* 
     * could read and compare 0xe, 0xf, and 0x10 - endian, ebus_timeout,
     * and read TO, but these are not currentely stored in NVRAM, just in
     * a .h file for the prom code
     */

    return(retval);
}
