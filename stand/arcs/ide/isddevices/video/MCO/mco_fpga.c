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

/*
 *  MCO FPGA (Altera) diagnostics/loading routines
 *
 *  $Revision: 1.17 $
 */

#include <arcs/io.h>
#include <arcs/errno.h>
#include <libsc.h>
#include <libsk.h>
#include <uif.h>

#include <math.h>
#include "sys/mgrashw.h"
#ifdef MGRAS_DIAG_SIM
#include "mgras_sim.h"
#else
#include "ide_msg.h"
#endif
/*
 * #include "mgras_diag.h"
 */

#include "mco_diag.h"

extern mgras_hw *mgbase;
extern void mco_set_dcbctrl(mgras_hw *base, int crs);
extern void sr_mco_set_dcbctrl(mgras_hw *base, int crs);
	
#define MCO_FPGABUFSIZE	0x10000

/* XXX Stuff to go into mgrashw.h eventually */
#define MCO_FPGA_CMD_MODE	0x02	/* Altera chip is in command mode */
#define MCO_FPGA_RESET_DONE	0x00	/* Altera chip has been reset */
#define MCO_FPGA_NOERRS		0x02	/* No errors detected during Altera configuration */

/* FPGA Control Bit defines */
#define nCONFIG_WR		BIT(0)	
#define nCONFIG_RD		BIT(2)	
#define nSTATUS			BIT(1)
#define alteraCONFDONE		BIT(0)

/* Speedracer MCO register value defines */
#define nSTATUS_reg		0xff
#define alteraCONFDONE_reg	0xff

#define MCO_FPGA_RED_INPUT_SWIZ		BIT(0)
#define MCO_FPGA_GREEN_INPUT_SWIZ	BIT(1)
#define MCO_FPGA_BLUE_INPUT_SWIZ	BIT(2)
#define MCO_FPGA_DCB			BIT(3)
#define MCO_FPGA_OUTPUT_SWIZZLE_01	BIT(4)
#define MCO_FPGA_OUTPUT_SWIZZLE_23	BIT(5)
#define MCO_FPGA_OUTPUT_SWIZZLE_45	BIT(6)
#define MCO_FPGA_OUTPUT_SWIZZLE_67	BIT(7)

#define MCO_FPGA_RESET		0x00	/* Resets the Altera chip */
#define MCO_FPGA_NORML		0x01	/* Altera chip normal operation */
#define MCO_FPGA_DATA_EN	0x03	/* Enable Altera Data Port */

#define MCO_FPGA_STATUS1_MASK	0x01
#define MCO_FPGA_STATUS2_MASK	0x02
#define MCO_FPGA_STATUS3_MASK	0x03

extern __uint32_t _mco_Probe(void);

__uint32_t _mco_initfpga(char *);
__uint32_t _sr_mco_initfpga(char *);

char *fpgadefs[] = {	"Red Input Swizzle FPGA (UXXXX)",
			"Green Input Swizzle FPGA (UXXXX)",
			"Blue Input Swizzle FPGA (UXXXX)",
			"DCB FPGA (UXXXX)",
			"Output Swizzle 01 (UXXXX)",
			"Output Swizzle 23 (UXXXX)",
			"Output Swizzle 45 (UXXXX)",
			"Output Swizzle 67 (UXXXX)"	};
/* 
 * mco_initfpga() -- initializes all eight (count'em! 8! count'em!)
 *	Altera FPGA chips (EPF8452AQC160-3)
 *		     
 *	This routine loads Altera microcode from internal
 *	static array (default) or from external file on
 *	local disk or over the network
 *
 *	Invoke using:
 *		mco_initfpga -f bootp()machine:/path/fpga.file
 *	  OR
 *		mco_initfpga -f dksc(0,1,0)/path/fpga.file
 */

__uint32_t
_mco_initfpga(char *ucodefilename)
{
    register char *bp;
    ULONG fd, cc;
    long rc;
    __uint32_t lderr=0;
    __uint32_t i, nconfig_poll, fpga_datasize;
    unsigned char *fpga_data;
    unsigned char fpga_status;
    char buf[512];

    if ((rc = Open ((CHAR *)ucodefilename, OpenReadOnly, &fd)) != ESUCCESS) {
	msg_printf(SUM, "Error: Unable to access file %s\n", ucodefilename);
	lderr++;
	return lderr;
    }

    /* Allocate memory chunk for Altera FPGA data */
    fpga_data = (unsigned char *)get_chunk(MCO_FPGABUFSIZE);

    /* Read in contents of Altera FPGA file into buffer */
    i = 0;
    while (((rc = Read(fd, buf, sizeof(buf), &cc))==ESUCCESS) && cc) {
	for (bp = buf; bp < &buf[cc]; bp++) {
	    /* fpga_data[i++] = (__uint32_t)(*bp); */
	    fpga_data[i++] = *bp;
	}
    }
    fpga_datasize = i;

    if (rc != ESUCCESS)
	msg_printf(VRB,"*** error termination on read\n");

    Close(fd);

    /* Set DCBCTRL correctly for MCO CRS(0) and CRS(1) addresses */
    mco_set_dcbctrl(mgbase, 1);

    /* Set nCONFIG_WR bit low */
    fpga_status = mgbase->mco_mpa1.mco.fpga_control;
    mgbase->mco_mpa1.mco.fpga_control = (fpga_status & ~(nCONFIG_WR | nSTATUS | alteraCONFDONE));

#ifdef IP30
    /* Wait around a bit */
    us_delay(25000);	/* 25 milliseconds */
#endif /* IP30 */

    /* Check the FPGA control bits */
    fpga_status = mgbase->mco_mpa1.mco.fpga_control;

    if (fpga_status & nCONFIG_RD) {
	msg_printf(VRB, "MCO FPGA Reset Failed: nCONFIG is not low (Expected = 0x%x, Actual 0x%x)\n", 
	(fpga_status & ~nCONFIG_RD), fpga_status);
	lderr++;
	return(lderr);
    }

    if (fpga_status & nSTATUS) {
	msg_printf(VRB, "MCO FPGA Reset Failed: nSTATUS is not low (Expected = 0x%x, Actual 0x%x)\n", 
	(fpga_status & ~nSTATUS), fpga_status);
	lderr++;
	return(lderr);
    }

    if (fpga_status & alteraCONFDONE) {
	msg_printf(VRB, "MCO FPGA Reset Failed: CONFDONE is not low (Expected = 0x%x, Actual 0x%x)\n", 
	(fpga_status & ~alteraCONFDONE), fpga_status);
	lderr++;
	return(lderr);
    }

    /* Set nCONFIG_WR bit high */
    fpga_status = mgbase->mco_mpa1.mco.fpga_control;
#ifdef NOTYET
    mgbase->mco_mpa1.mco.fpga_control = (fpga_status | nCONFIG_WR);
#endif /* NOTYET */
    mgbase->mco_mpa1.mco.fpga_control = nCONFIG_WR;

    /* Wait around a bit */
    us_delay(25000);	/* 25 milliseconds */

    /* Wait until nCONFIG_RD goes high */
    nconfig_poll = 1;
    while ((nconfig_poll >= 1) && (nconfig_poll < 0x1000000)) {
	fpga_status = mgbase->mco_mpa1.mco.fpga_control;
	if (fpga_status & (nCONFIG_RD | nSTATUS)) {
	    nconfig_poll = 0;
	}
	else {
	    nconfig_poll++;
	}
    }

    if (nconfig_poll >= 0x1000000) {
	msg_printf(VRB, "MCO FPGA Reset Failed: nCONFIG never went high (Expected = 0x%x, Actual 0x%x)\n", 
	(fpga_status | nCONFIG_RD), fpga_status);
	lderr++;
	return(lderr);
    }

    if (!(fpga_status & nSTATUS)) {
	msg_printf(VRB, "MCO FPGA Reset Failed: nSTATUS never went high (Expected = 0x%x, Actual 0x%x)\n", 
	(fpga_status | nSTATUS), fpga_status);
	lderr++;
	return(lderr);
    }

    if (fpga_status & alteraCONFDONE) {
	msg_printf(VRB, "MCO FPGA Reset Failed: CONFDONE went high (Expected = 0x%x, Actual 0x%x)\n", 
	(fpga_status & ~alteraCONFDONE), fpga_status);
	lderr++;
	return(lderr);
    }

    /* Load Altera FPGA code into FPGA */
    fpga_status = MCO_FPGA_NOERRS;
    i = 0;
#ifdef NOTYET
    msg_printf(DBG,"mco_initfpga: Writing data: \n");
#endif /* NOTYET */
    while (i < fpga_datasize) {

#ifdef NOTYET
	msg_printf(DBG, "0x%x ", fpga_data[i]);
#endif /* NOTYET */
#ifdef IP30
	mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
#endif /* IP30 */
	mgbase->mco_mpa1.mco.fpga_config = fpga_data[i++];

	/* Make sure Altera FPGA has no errors */
	fpga_status = mgbase->mco_mpa1.mco.fpga_control;
	if (!(fpga_status & nCONFIG_RD)) {
	    msg_printf(VRB, "MCO FPGA Download Failed: nCONFIG_RD is not high (Expected = 0x%x, Actual 0x%x)\n", 
	    (fpga_status | nCONFIG_RD), fpga_status);
	    lderr++;
	    return(lderr);
	}

	if (!(fpga_status & nSTATUS)) {
	    msg_printf(VRB, "MCO FPGA Download Failed: nSTATUS indicates error (Expected = 0x%x, Actual 0x%x)\n", 
	    (fpga_status | nSTATUS), fpga_status);
	    lderr++;
	    return(lderr);
	}

	if (fpga_status & alteraCONFDONE) {
	    msg_printf(VRB, "MCO FPGA Download Failed: CONFDONE is not low (Expected = 0x%x, Actual 0x%x)\n", 
	    (fpga_status & ~alteraCONFDONE), fpga_status);
	    lderr++;
	    return(lderr);
	}

#ifdef NOTYET
	if (( i % 16 ) == 0 ) {
	    msg_printf(DBG,"\nmco_initfpga: 0x%x ", fpga_data[i]);
	}
#endif /* NOTYET */
    }

    /* Now send additional clocks */
    for (i=0 ; i<20; i++)  {
	mgbase->mco_mpa1.mco.fpga_config = 0;
#ifdef IP30
	mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
#endif /* IP30 */
	fpga_status = mgbase->mco_mpa1.mco.fpga_control;

    }

    /* Now check if download succeeded */
    fpga_status = mgbase->mco_mpa1.mco.fpga_control;
    if (!(fpga_status & nSTATUS)) {
	msg_printf(VRB, "MCO FPGA Download Failed: nSTATUS indicates error (Expected = 0x%x, Actual 0x%x)\n", 
	(fpga_status | nSTATUS), fpga_status);
	lderr++;
	return(lderr);
    }

    if (!(fpga_status & alteraCONFDONE)) {
	msg_printf(VRB, "MCO FPGA Download Failed: CONFDONE never went high (Expected = 0x%x, Actual 0x%x)\n", 
	(fpga_status | alteraCONFDONE), fpga_status);
	lderr++;
	return(lderr);
    }
    msg_printf(DBG,"\nmco_initfpga: Done Writing FPGA file %s  (%d bytes)\n",
    ucodefilename, fpga_datasize);

    msg_printf(DBG,"mco_initfpga: Starting VC2 clock\n");
    mgbase->dcbctrl_11 = MCO_DCBCTRL_IO_START;
    MCO_SETINDEX(mgbase, MCO_CONTROL2, 0);
    MCO_IO_WRITE(mgbase, 5);

    return(lderr);
}

#ifdef IP30
__uint32_t
_sr_mco_initfpga(char *ucodefilename)
{
    register char *bp;
    ULONG fd, cc;
    long rc;
    __uint32_t lderr=0;
    __uint32_t i, nconfig_poll, fpga_datasize;
    unsigned char *fpga_data;
    unsigned char fpga_status;
    char buf[512];
    int bit;

    if ((rc = Open ((CHAR *)ucodefilename, OpenReadOnly, &fd)) != ESUCCESS) {
	msg_printf(SUM, "Error: Unable to access file %s\n", ucodefilename);
	lderr++;
	return lderr;
    }

    /* Allocate memory chunk for Altera FPGA data */
    fpga_data = (unsigned char *)get_chunk(MCO_FPGABUFSIZE);

    /* Read in contents of Altera FPGA file into buffer */
    i = 0;
    while (((rc = Read(fd, buf, sizeof(buf), &cc))==ESUCCESS) && cc) {
	for (bp = buf; bp < &buf[cc]; bp++) {
	    /* fpga_data[i++] = (__uint32_t)(*bp); */
	    fpga_data[i++] = *bp;
	}
    }
    fpga_datasize = i;

    if (rc != ESUCCESS)
	msg_printf(VRB,"*** error termination on read\n");

    Close(fd);

    /* Set DCBCTRL correctly for MCO CRS(0) and CRS(1) addresses */
    sr_mco_set_dcbctrl(mgbase, 1);

    /* Set nCONFIG_WR bit low */
    fpga_status = mgbase->mco_mpa1.mco.fpga_control;
    mgbase->mco_mpa1.mco.fpga_control = (fpga_status & ~(nCONFIG_WR | nSTATUS | alteraCONFDONE));

    /* Wait around a bit */
    us_delay(25000);	/* 25 milliseconds */

    /* Check the FPGA control bits */
    fpga_status = mgbase->mco_mpa1.mco.fpga_control;

    if (fpga_status & nCONFIG_RD) {
	msg_printf(VRB, "SR MCO FPGA Reset Failed: nCONFIG is not low (Expected = 0x%x, Actual 0x%x)\n", 
	(fpga_status & ~nCONFIG_RD), fpga_status);
	lderr++;
	return(lderr);
    }

    /* Now read nSTATUS bits and verify that NSTATUS went low on all FPGAs */
    fpga_status = (mgbase->option_id.oco.nstatus & 0xff);  /* CRS(1) on DCB Dev 0 */
    if (fpga_status & nSTATUS_reg) {
	for (bit=0; bit<8; bit++) {
	    if (fpga_status & (1<<bit)) {
		msg_printf(VRB, "SR MCO FPGA Reset on %s Failed: nSTATUS is not low\n",
		fpgadefs[bit]);
	    }
	}
	lderr++;
	return(lderr);
    }

    /* Check CONFDONE bits and verify that CONFDONE went low on all FPGAs */
    fpga_status = (mgbase->option_id.oco.conf_done & 0xff);	/* CRS(2) on DCB Dev 0 */
    if (fpga_status & alteraCONFDONE_reg) {
	for (bit=0; bit<8; bit++) {
	    if (fpga_status & (1<<bit)) {
		msg_printf(VRB, "SR MCO FPGA Reset on %s Failed: CONFDONE is not low\n",
		fpgadefs[bit]);
	    }
	}
	lderr++;
	return(lderr);
    }

    /* Set nCONFIG_WR bit high */
    fpga_status = mgbase->mco_mpa1.mco.fpga_control;
    mgbase->mco_mpa1.mco.fpga_control = nCONFIG_WR;

    /* Wait around a bit */
    us_delay(25000);	/* 25 milliseconds */

    /* Wait until nCONFIG_RD goes high */
    nconfig_poll = 1;
    while ((nconfig_poll >= 1) && (nconfig_poll < 0x1000000)) {
	fpga_status = mgbase->mco_mpa1.mco.fpga_control;
	if (fpga_status & nCONFIG_RD) {
	    fpga_status = (mgbase->option_id.oco.nstatus & 0xff); 
	    if ((fpga_status & nSTATUS_reg) == nSTATUS_reg) {
		nconfig_poll = 0;
	    }
	    else {
		for (bit=0; bit<8; bit++) {
		    if (!(fpga_status & (1<<bit))) {
			msg_printf(VRB, "SR MCO FPGA download on %s Failed: nSTATUS is not high when nCONFIG went high\n",
			fpgadefs[bit]);
		    }
		}
	    }
	}
	else {
	    nconfig_poll++;
	}
    }

    if (nconfig_poll >= 0x1000000) {
	msg_printf(VRB, "SR MCO FPGA Reset Failed: nCONFIG never went high (Expected = 0x%x, Actual 0x%x)\n", 
	(fpga_status | nCONFIG_RD), fpga_status);
	lderr++;
	return(lderr);
    }

    /* Now read nSTATUS bits */
    fpga_status = mgbase->option_id.oco.nstatus;  /* CRS(1) on DCB Dev 0 */
    if ((fpga_status & nSTATUS_reg) != nSTATUS_reg) {
	for (bit=0; bit<8; bit++) {
	    if (!(fpga_status & (1<<bit))) {
		msg_printf(VRB, "SR MCO FPGA Reset on %s Failed: nSTATUS is not high\n",
		fpgadefs[bit]);
	    }
	}
	lderr++;
	return(lderr);
    }

    /* Check that CONFDONE is still low */
    fpga_status = mgbase->option_id.oco.conf_done;  /* CRS(2) on DCB Dev 0 */
    if (fpga_status & alteraCONFDONE) {
	for (bit=0; bit<8; bit++) {
	    if (fpga_status & (1<<bit)) {
		msg_printf(VRB, "SR MCO FPGA Reset on %s Failed: CONFDONE went high\n", 
		fpgadefs[bit]);
	    }
	}
	lderr++;
	return(lderr);
    }

    /* Load Altera FPGA code into FPGA */
    fpga_status = MCO_FPGA_NOERRS;
    i = 0;
    sr_mco_set_dcbctrl(mgbase, 0);
#ifdef NOTYET
    msg_printf(DBG,"mco_initfpga: Writing data: \n");
#endif /* NOTYET */
    while (i < fpga_datasize) {

#ifdef NOTYET
	msg_printf(DBG, "0x%x ", fpga_data[i]);
#endif /* NOTYET */
	mgbase->mco_mpa1.mco.fpga_config = fpga_data[i++];

	/* Make sure Altera FPGA has no errors */
	fpga_status = mgbase->mco_mpa1.mco.fpga_control;
	if (!(fpga_status & nCONFIG_RD)) {
	    msg_printf(VRB, "SR MCO FPGA Download Failed: nCONFIG_RD is not high (Expected = 0x%x, Actual 0x%x)\n", 
	    (fpga_status | nCONFIG_RD), fpga_status);
	    lderr++;
	    return(lderr);
	}

	/* Verify that nSTATUS bits are high */
	fpga_status = (mgbase->option_id.oco.nstatus & 0xff);  /* CRS(1) on DCB Dev 0 */
	if ((fpga_status & nSTATUS_reg) != nSTATUS_reg) {
	    for (bit=0; bit<8; bit++) {
	        if (!(fpga_status & (1<<bit))) {
		    msg_printf(VRB, "SR MCO FPGA Download on %s Failed: nSTATUS is not high\n",
		    fpgadefs[bit]);
	        }
	    }
	    lderr++;
	    return(lderr);
	}

	/* Check that CONFDONE bits are still low */
	fpga_status = (mgbase->option_id.oco.conf_done & 0xff);  /* CRS(2) on DCB Dev 0 */
	if (fpga_status & alteraCONFDONE_reg) {
	    for (bit=0; bit<8; bit++) {
		if (fpga_status & (1<<bit)) {
		    msg_printf(VRB, "SR MCO FPGA Download on %s Failed: CONFDONE is not low\n", 
		    fpgadefs[bit]);
		}
	    }
	    lderr++;
	    return(lderr);
	}

#ifdef NOTYET
	if (( i % 16 ) == 0 ) {
	    msg_printf(DBG,"\nmco_initfpga: 0x%x ", fpga_data[i]);
	}
#endif /* NOTYET */
    }

    /* Now send additional clocks */
    for (i=0 ; i<20; i++)  {
	mgbase->mco_mpa1.mco.fpga_config = 0;
	fpga_status = mgbase->mco_mpa1.mco.fpga_control;

    }

    /* Now check if download succeeded */

    /* Check if nSTATUS bits are all high */
    fpga_status = mgbase->option_id.oco.nstatus;  /* CRS(1) on DCB Dev 0 */
    if ((fpga_status & nSTATUS_reg) != nSTATUS_reg) {
	for (bit=0; bit<8; bit++) {
	    if (!(fpga_status & (1<<bit))) {
		msg_printf(VRB, "SR MCO FPGA Download on %s Failed: nSTATUS is not high\n",
		fpgadefs[bit]);
	    }
	}
	lderr++;
	return(lderr);
    }

    /* Check if CONFDONE bits all went high */
    fpga_status = mgbase->option_id.oco.conf_done;  /* CRS(2) on DCB Dev 0 */
    if ((fpga_status & alteraCONFDONE_reg) != alteraCONFDONE_reg) {
	msg_printf(VRB, "MCO FPGA Download Failed: CONFDONE never went high (Expected = 0x%x, Actual 0x%x)\n", 
	(fpga_status | alteraCONFDONE), fpga_status);
	for (bit=0; bit<8; bit++) {
	    if (!(fpga_status & (1<<bit))) {
		msg_printf(VRB, "SR MCO FPGA Download on %s Failed: CONFDONE is not high\n",
		fpgadefs[bit]);
	    }
	}
	lderr++;
	return(lderr);
    }
    msg_printf(DBG,"\nmco_initfpga: Done Writing FPGA file %s  (%d bytes)\n",
    ucodefilename, fpga_datasize);

#ifdef NOTYET
    msg_printf(DBG,"mco_initfpga: Starting VC2 clock\n");
    mgbase->dcbctrl_11 = MCO_DCBCTRL_IO_START;
    MCO_SETINDEX(mgbase, MCO_CONTROL2, 0);
    MCO_IO_WRITE(mgbase, 5);
#endif	/* NOTYET */

    return(lderr);
}
#endif /* IP30 */

__uint32_t
mco_Initfpga(int argc, char **argv)
{
    __uint32_t lderr=0;
    char ucodefilename[80];

    if (argc < 3) {
	msg_printf(SUM, "usage: mco_initfpga [-f bootp()machine:/path/fpga.file]\n");
	lderr++;
	return(lderr);
    }

    argc--; argv++;	/* Skip Test name */
    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	switch (argv[0][1]) {
	    case 'f' :
		if (argv[0][2]=='\0') { /* has white space */
		    sprintf(ucodefilename, "%s", &argv[1][0]);
		    argc--; argv++;
		}
		else { /* no white space */
		    sprintf(ucodefilename, "%s", &argv[0][2]);
		}
		break;
	    default  :
		msg_printf(SUM, "usage: mco_initfpga [-f bootp()machine:/path/fpga.file]\n");
	}
    }
#ifdef IP30
    lderr = _sr_mco_initfpga(ucodefilename);
#else	/* IP30 */
    lderr = _mco_initfpga(ucodefilename);
#endif	/* IP30 */

    if (lderr) {
	msg_printf(VRB,"mco_initfpga: ERROR: Couldn't download FPGA file %s\n",
	ucodefilename);
    }
    else {
	msg_printf(VRB,"mco_initfpga: Done Writing FPGA file %s\n",
	ucodefilename);
    }

    return(lderr);
}

/* 
 * mco_loadfpga() -- writes specified file to all eight (count'em! 8! count'em!)
 *	Altera FPGA chips (EPF8452AQC160-3)
 *		     
 *	This routine loads Altera microcode from internal
 *	static array (default) or from external file on
 *	local disk or over the network
 *
 *	Invoke using:
 *		mco_loadfpga -f bootp()machine:/path/fpga.file
 *	  OR
 *		mco_loadfpga -f dksc(0,1,0)/path/fpga.file
 */
/*ARGSUSED*/
int
mco_loadfpga(int argc, char **argv)
{
#ifdef IP30
    msg_printf(SUM, "mco_loadfpga:  Not available on GAMERA systems\n");
    return(1);
#else	/* IP30 */
    register char *bp;
    ULONG fd, cc;
    long rc;
    __uint32_t lderr=0;
    __uint32_t i, fpga_datasize;
    unsigned char *fpga_data;
    char buf[512], ucodefilename[256];

    if (argc < 3) {
	msg_printf(SUM, "usage: mco_loadfpga [-f bootp()machine:/path/fpga.file]\n");
	lderr++;
	return(lderr);
    }

    argc--; argv++;	/* Skip Test name */
    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	switch (argv[0][1]) {
	    case 'f' :
		if (argv[0][2]=='\0') { /* has white space */
		    sprintf(ucodefilename, "%s", &argv[1][0]);
		    argc--; argv++;
		}
		else { /* no white space */
		    sprintf(ucodefilename, "%s", &argv[0][2]);
		}
		break;
	    default  :
		msg_printf(SUM, "usage: mco_initfpga [-f bootp()machine:/path/fpga.file]\n");
	}
    }
#ifdef NOTYET
    msg_printf(DBG, "mco_loadfpga: Attempting to open file %s\n", 
    ucodefilename);
#endif /* NOTYET */
    if ((rc = Open ((CHAR *)ucodefilename, OpenReadOnly, &fd)) != ESUCCESS) {
	msg_printf(SUM, "Error: Unable to access file %s\n", ucodefilename);
	lderr++;
	return lderr;
    }

    /* Allocate memory chunk for Altera FPGA data */
    /* fpga_data = (__uint32_t *)get_chunk(MCO_FPGABUFSIZE); */
    fpga_data = (unsigned char *)get_chunk(MCO_FPGABUFSIZE);

    /* Read in contents of Altera FPGA file into buffer */
    msg_printf(VRB, "Reading contents of file %s:\n", ucodefilename);
    i = 0;
    while (((rc = Read(fd, buf, sizeof(buf), &cc))==ESUCCESS) && cc) {
	for (bp = buf; bp < &buf[cc]; bp++) {
	    /* fpga_data[i++] = (__uint32_t)(*bp); */
	    fpga_data[i++] = *bp;
#ifdef ORIG
	    putchar(*bp);
#endif /* ORIG */
	}
    }
    fpga_datasize = i;
    /* fpga_dp = &fpga_data[0]; */

    if (rc != ESUCCESS) {
	printf("*** error termination on read\n");
	msg_printf(VRB, "Error: error termination on read of file %s\n",
	ucodefilename);
    }

    Close(fd);

    i = 0;

    /* Set DCBCTRL correctly for MCO CRS(0) and CRS(1) addresses */
    mco_set_dcbctrl(mgbase, 0);

#ifdef NOTYET
    msg_printf(DBG,"mco_loadfpga: Writing data: \n");
#endif /* NOTYET */
    while (i < fpga_datasize) {

#ifdef NOTYET
	msg_printf(DBG, "0x%x ", fpga_data[i]);
#endif /* NOTYET */
	mgbase->mco_mpa1.mco.fpga_config = fpga_data[i++];

#ifdef NOTYET
	if (( i % 16 ) == 0 ) {
	    msg_printf(DBG,"\nmco_loadfpga: 0x%x ", fpga_data[i]);
	}
#endif /* NOTYET */
    }
#ifdef NOTYET
    msg_printf(DBG,"\nmco_loadfpga: Done Writing data (%d bytes)\n", fpga_datasize);
#endif /* NOTYET */

    return(lderr);
#endif	/* IP30 */
}

int
_mco_indextest(void)
{
    uchar_t i, val;
    int j;
    int errors=0;

    /* Initialize the dcb interface for MCO Index Register. */
#ifdef IP30
    sr_mco_set_dcbctrl(mgbase, 2);
#else	/* IP30 */
    mco_set_dcbctrl(mgbase, 2);
#endif	/* IP30 */

    for (j=0; j<1000; j++) {
	for (i=0; i<255; i++) {

	    MCO_SETINDEX(mgbase, 0, i);

	    /* This is just here to clear the DCB bus -- I want to be sure 
	     * the DCBFPGA is driving the wires ...
	     */

	    MCO_IO_WRITE(mgbase, 0);

	    /* Read MCO Index Register */
	    val = mgbase->mco_mpa1.mco.index;

	    if (val != i) {
		msg_printf(VRB,"MCO Index Test Failed -- Exp %d Actual %d\n",
		i, val);
		errors++;
		return(errors);
	    }
	}
    }

    return(errors);

}

__uint32_t
mco_IndexTest(void)
{
	int errors = 0;

	errors = _mco_indextest(); 
	REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_INDEX_REG_TEST]), errors);
	/*NOTREACHED*/
}
