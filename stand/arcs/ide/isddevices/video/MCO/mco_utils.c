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
 *  MCO diag utilities
 *
 *  $Revision: 1.13 $
 */

#include <arcs/io.h>
#include <arcs/errno.h>
#include <libsc.h>
#include <libsk.h>
#include <uif.h>

#include <math.h>
#include "sys/mgrashw.h"
#include "mco_diag.h"
#ifdef MGRAS_DIAG_SIM
#include "mgras_sim.h"
#else
#include "ide_msg.h"
#endif
/*
 * #include "mgras_diag.h"
 */

extern mgras_hw *mgbase;
extern __int32_t _xtalk_nic_probe(__int32_t, char *, __int32_t);
extern __uint32_t mg_probe(void);
	
/* 
 *	Macros to set dcbctrl_0 register
 */

void
mco_set_dcbctrl(mgras_hw *base, int crs) {
#ifdef IP30
    mgras_BFIFOWAIT(base,HQ3_BFIFO_MAX);
#endif /* IP30 */
    switch (crs) {
	case 0:
	case 1:
		base->dcbctrl_11 = MCO_DCBCTRL_PAL;
		break;
	case 2:
	case 3:
		base->dcbctrl_11 = MCO_DCBCTRL_IO;
		break;
	case 4:
	case 5:
	case 6:
	case 7:
		base->dcbctrl_11 = MCO_DCBCTRL_VC2;
		break;
	default:
		base->dcbctrl_11 = MCO_DCBCTRL_PAL;
    }
}

void
sr_mco_set_dcbctrl(mgras_hw *base, int crs) {
    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
    switch (crs) {
	case 0:
	case 1:
		base->dcbctrl_0 = MCO_DCBCTRL_PAL;
		base->dcbctrl_11 = MCO_DCBCTRL_PAL;
		break;
	case 2:
		base->dcbctrl_0 = MCO_DCBCTRL_PAL;
		base->dcbctrl_11 = MCO_DCBCTRL_IO;
		break;
	case 3:
		base->dcbctrl_11 = MCO_DCBCTRL_IO;
		break;
	case 4:
	case 5:
	case 6:
	case 7:
		base->dcbctrl_11 = MCO_DCBCTRL_VC2;
		break;
	default:
		base->dcbctrl_11 = MCO_DCBCTRL_PAL;
    }
}

/*
 * mco_get -- read memory
 * mco_get [b|h|w] CRSADDRESS
 */
int
mco_get(int argc, char **argv)
{
#ifdef NOTYET
        int width = SW_WORD;
#endif
        unsigned short sval;
        unsigned char bval;
	unsigned int val;
        unsigned address;
	char addrstr[80];
	int crsaddr;

        argv++; argc--;
        if (argc == 2 && **argv == '-') {
                switch ((*argv)[1]) {
                case 'b':
#ifdef NOTYET
                        width = SW_BYTE;
#endif
                        break;

                case 'h':
#ifdef NOTYET
                        width = SW_HALFWORD;
#endif
                        break;

                case 'w':
#ifdef NOTYET
                        width = SW_WORD;
#endif
                        break;

                case 'd':
#ifdef NOTYET
                        width = SW_DOUBLEWORD;
#endif
                        break;

                default:
                        return(1);
                }
                argv++; argc--;
        }

        if (argc != 1)
                return(-1);

	crsaddr = atoi(argv[0]);

	/* Set DCBCTRL to appropriate value */
	mco_set_dcbctrl(mgbase, crsaddr);

	/* Wait around a bit */
	us_delay(25000);	/* 25 milliseconds */

	switch (crsaddr) {
	    case 0:	/* CRS(0) -> MCO_FPGA_CONFIG */
		sprintf(addrstr, "0x%x", &mgbase->mco_mpa1.mco.fpga_config);
        	if (*atobu(addrstr, &address)) {
			msg_printf(VRB,"mco_get: atobu() error\n");
                	return(-1);
		}

		bval = mgbase->mco_mpa1.mco.fpga_config;
       	msg_printf(VRB, "mco_get: CRS: %d  0x%x:\t%d\t0x%x\t'", crsaddr,
        address, bval, bval);
        showchar(bval);
        printf("'\n");
		val = (int)bval;
		break;
	    case 1:	/* CRS(1) -> MCO_FPGA_CONTRL */
		sprintf(addrstr, "0x%x", &mgbase->mco_mpa1.mco.fpga_control);
        	if (*atobu(addrstr, &address)) {
			msg_printf(VRB,"mco_get: atobu() error\n");
                	return(-1);
		}

		bval = mgbase->mco_mpa1.mco.fpga_control;
       	msg_printf(VRB, "mco_get: CRS: %d  0x%x:\t%d\t0x%x\t'", crsaddr,
        address, bval, bval);
        showchar(bval);
        printf("'\n");
		val = (int)bval;
		break;
	    case 2:	/* CRS(2) -> MCO_INDEX */
		sprintf(addrstr, "0x%x", &mgbase->mco_mpa1.mco.index);
        	if (*atobu(addrstr, &address)) {
			msg_printf(VRB,"mco_get: atobu() error\n");
                	return(-1);
		}

		bval = mgbase->mco_mpa1.mco.index;
       	msg_printf(VRB, "mco_get: CRS: %d  0x%x:\t%d\t0x%x\t'", crsaddr,
        address, bval, bval);
        showchar(bval);
        printf("'\n");
		val = (int)bval;
		break;
	    case 3:	/* CRS(3) -> MCO_DATA_IO */
		sprintf(addrstr, "0x%x", &mgbase->mco_mpa1.mco.dataio);
        	if (*atobu(addrstr, &address)) {
			msg_printf(VRB,"mco_get: atobu() error\n");
                	return(-1);
		}

		bval = mgbase->mco_mpa1.mco.dataio;
       	msg_printf(VRB, "mco_get: CRS: %d  0x%x:\t%d\t0x%x\t'", crsaddr,
        address, bval, bval);
        showchar(bval);
        printf("'\n");
		val = (int)bval;
		break;
	    case 4:	/* CRS(4) -> MCO_VC2_INDEX */
		sprintf(addrstr, "0x%x", &mgbase->mco_mpa1.mco.vc2index);
        	if (*atobu(addrstr, &address)) {
			msg_printf(VRB,"mco_get: atobu() error\n");
                	return(-1);
		}

		bval = mgbase->mco_mpa1.mco.vc2index;
       	msg_printf(VRB, "mco_get: CRS: %d  0x%x:\t%d\t0x%x\t'", crsaddr,
        address, bval, bval);
        showchar(bval);
        printf("'\n");
		val = (int)bval;
		break;
	    case 5:	/* CRS(5) -> MCO_VC2_DATA_HIGH */
		sprintf(addrstr, "0x%x", &mgbase->mco_mpa1.mco.vc2data);
        	if (*atobu(addrstr, &address)) {
			msg_printf(VRB,"mco_get: atobu() error\n");
                	return(-1);
		}

		sval = mgbase->mco_mpa1.mco.vc2data;
       	msg_printf(VRB, "mco_get: CRS: %d  0x%x:\t%d\t0x%x\t'", crsaddr,
        address, sval, sval);
        showchar(sval);
        printf("'\n");
		val = (int)sval;
		break;
	    case 7:	/* CRS(7) -> MCO_VC2_RAMDATA */
		sprintf(addrstr, "0x%x", &mgbase->mco_mpa1.mco.vc2ram);
        	if (*atobu(addrstr, &address)) {
			msg_printf(VRB,"mco_get: atobu() error\n");
                	return(-1);
		}

		sval = mgbase->mco_mpa1.mco.vc2ram;
       	msg_printf(VRB, "mco_get: CRS: %d  0x%x:\t%d\t0x%x\t'", crsaddr,
        address, sval, sval);
        showchar(sval);
        printf("'\n");
		val = (int)sval;
		break;
	    default:
		msg_printf(VRB,"mco_get: I don't know about CRS(%d)\n", crsaddr);
		return(-1);
	}

#ifdef NOTYET
        if (*atobu(addrstr, &address)) {
		msg_printf(VRB,"mco_get: atobu() error\n");
                return(-1);
	}

        val = get_memory(address, width);
#endif /* NOTYET */

       	printf("mco_get: CRS: %d  0x%x:\t%d\t0x%x\t'", crsaddr,
        address, val, val);
        showchar(val);
        printf("'\n");
	return(val);
}

/*
 * mco_put [b|h|w] CRSADDRESS VALUE
 */
__uint32_t
mco_put(int argc, char **argv)
{
        int width = SW_WORD;
        unsigned address;
        unsigned val;
	int crsaddr;
	char addrstr[80];

        argv++; argc--;
        if (argc == 3 && **argv == '-') {
                switch ((*argv)[1]) {
                case 'b':
                        width = SW_BYTE;
                        break;

                case 'h':
                        width = SW_HALFWORD;
                        break;

                case 'w':
                        width = SW_WORD;
                        break;

                default:
                        return(1);
                }
                argv++; argc--;
        }

        if (argc != 2)
                return(1);

	crsaddr = atoi(argv[0]);

	switch (crsaddr) {
	    case 0:	/* CRS(0) -> MCO_FPGA_CONFIG */
		sprintf(addrstr, "0x%x", &mgbase->mco_mpa1.mco.fpga_config);
		break;
	    case 1:	/* CRS(0) -> MCO_FPGA_CONTRL */
		sprintf(addrstr, "0x%x", &mgbase->mco_mpa1.mco.fpga_control);
		break;
	    case 2:	/* CRS(0) -> MCO_INDEX */
		sprintf(addrstr, "0x%x", &mgbase->mco_mpa1.mco.index);
		break;
	    case 3:	/* CRS(0) -> MCO_DATA_IO */
		sprintf(addrstr, "0x%x", &mgbase->mco_mpa1.mco.dataio);
		break;
	    case 4:	/* CRS(0) -> MCO_VC2_INDEX */
		sprintf(addrstr, "0x%x", &mgbase->mco_mpa1.mco.vc2index);
		break;
	    case 5:	/* CRS(0) -> MCO_VC2_DATA_HIGH */
		sprintf(addrstr, "0x%x", &mgbase->mco_mpa1.mco.vc2data);
		break;
	    case 7:	/* CRS(0) -> MCO_VC2_RAMDATA */
		sprintf(addrstr, "0x%x", &mgbase->mco_mpa1.mco.vc2ram);
		break;
	    default:
		msg_printf(VRB,"mco_put: I don't know about CRS(%d)\n", crsaddr);
		return(-1);
	}

        if (*atobu(addrstr, &address)) {
		msg_printf(VRB,"mco_put: atobu() error\n");
                return(1);
	}

        if (*atobu(argv[1], &val)) {
		msg_printf(VRB,"mco_put: atobu() error\n");
                return(1);
	}

	
	msg_printf(VRB, "mco_put: CRS: %d  Address: 0x%x   Value: 0x%x\n",
	crsaddr, address,val);

	/* Set DCBCTRL to appropriate value */
	mco_set_dcbctrl(mgbase, crsaddr);

	/* Wait around a bit */
	us_delay(25000);	/* 25 milliseconds */

        set_memory((void *)(__psint_t)address, width, val);
        return(0);
}

__uint32_t
mco_setdcbctrl(int argc, char **argv)
{
	__uint32_t errors = 0;
	__uint32_t dcbctrlreg;
	int crsval = 0;	/* default to CRS(0) */

	if (argc != 2) {
		printf("usage: %s CRSval (CRSval = 0-7)\n", argv[0]);
		return -1;
	}

	msg_printf(VRB, "%s: CRS(%s)\n", argv[0], argv[1]);
	crsval = atoi(argv[1]);

	/* Set the MG DCB CTRL register to proper setting for MCO */
	mco_set_dcbctrl(mgbase, crsval);
	dcbctrlreg = mgbase->dcbctrl_11;

	msg_printf(VRB, "%s: Address 0x%x = 0x%x\n", argv[0], 
	&(mgbase->dcbctrl_11), dcbctrlreg);

	return (errors);
}

#ifdef NOTYET
__uint32_t
mco_PrintAddrs(void)
{
	__uint32_t errors = 0;

       	printf("mco_printaddrs: CRS: 0  FPGA_CONFIG address = 0x%x\n",
	&mgbase->mco_mpa1.mco.fpga_config);
       	printf("mco_printaddrs: CRS: 1  FPGA_CONTROL address = 0x%x\n",
	&mgbase->mco_mpa1.mco.fpga_control);
       	printf("mco_printaddrs: CRS: 2  INDEX address = 0x%x\n",
	&mgbase->mco_mpa1.mco.index);
       	printf("mco_printaddrs: CRS: 3  DATAIO address = 0x%x\n",
	&mgbase->mco_mpa1.mco.dataio);
       	printf("mco_printaddrs: CRS: 2  7162 DAC A address register = 0x%x\n",
	(MCO_ADV7162A | MCO_ADV7162_ADDR));
       	printf("mco_printaddrs: CRS: 2  7162 DAC A LUT = 0x%x\n",
	(MCO_ADV7162A | MCO_ADV7162_LUT));
       	printf("mco_printaddrs: CRS: 2  7162 DAC A Control Register = 0x%x\n",
	(MCO_ADV7162A | MCO_ADV7162_CTRL));
       	printf("mco_printaddrs: CRS: 2  7162 DAC A Mode Register = 0x%x\n",
	(MCO_ADV7162A | MCO_ADV7162_MODE));
       	printf("mco_printaddrs: CRS: 2  7162 DAC B address register = 0x%x\n",
	(MCO_ADV7162B | MCO_ADV7162_ADDR));
       	printf("mco_printaddrs: CRS: 2  7162 DAC B LUT = 0x%x\n",
	(MCO_ADV7162B | MCO_ADV7162_LUT));
       	printf("mco_printaddrs: CRS: 2  7162 DAC B Control Register = 0x%x\n",
	(MCO_ADV7162B | MCO_ADV7162_CTRL));
       	printf("mco_printaddrs: CRS: 2  7162 DAC B Mode Register = 0x%x\n",
	(MCO_ADV7162B | MCO_ADV7162_MODE));
}
#endif /* NOTYET */

#ifdef IP30
/* 
 * mco_nic_check looks for a NIC on the GAMERA port with a name "MCO"
 * returns 0 if successful, else returns non-zero value
 */
__int32_t
mco_nic_check(void)
{
	__int32_t errors = 0, do_compare = 1;
	__uint32_t port;
	char name[10];

	strcpy(name, "MCO");

	port = mg_probe();

	if (port)
		errors = _xtalk_nic_probe(port, name, do_compare);


	return (errors);
}
#endif /* IP30 */

int 
mco_report_passfail(_Test_Info *Test, int errors)
{
	if (!errors) {
		msg_printf(SUM, "     %s Test passed.\n" ,Test->TestStr);
		return 0;
	}
	else {
		msg_printf(ERR, "**** %s Test failed.  ErrCode %s Errors %d\n",
			   Test->TestStr, Test->ErrStr, errors);
		return -1;
	}
}

