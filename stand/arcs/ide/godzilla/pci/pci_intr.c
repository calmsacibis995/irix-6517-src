/*
 * pci_intr.c
 *	
 *	PCI Interrupt Tests (using Serial/Parallel/Enet)
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.5 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/vdma.h"
#include "sys/time.h"
#include "sys/immu.h"
#include "sys/ns16550.h"
#include "sys/PCI/ioc3.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"
#include "d_prototypes.h"
#include "d_frus.h"
#include "d_bridge.h"
#include "d_pci.h"

/*
 * Forward References 
 */
bool_t 		pci_intr(__uint32_t argc, char **argv);

/* Extern Decl */
extern	__uint32_t      _pci_sio_cfg_port(bridgereg_t port_base, __uint32_t baud_rate);
extern	__uint32_t      _pci_sio_putc(bridgereg_t port_base, uchar_t data);
extern	__uint32_t      _pci_sio_getc(bridgereg_t port_base, uchar_t *data);


/*
 * Name:        pci_intr
 * Description: Tests the PCI Interrupt Mechanism
 * Input:       following switches are currently used: (with or without space)
 * Output:      Returns 0 if no error, 1 if error
 * Error Handling:
 * Side Effects: none
 * Remarks: Uses IOC3
 * Run:         pci_sio -d [0|1] (0 = port A; 1 is B)
 * Debug Status: compiles, simulated, not emulated, debugged
 */
bool_t
pci_intr(__uint32_t argc, char **argv)
{
	__uint32_t	intrVal, intrMask, timeOut;
	__uint32_t	ioc3_dev_num = IOC3_PCI_SLOT;
	int		device, baud, bad_arg = 0;
	bridgereg_t	ioc3_mem, portBase;
	uchar_t		ch, mask = ~0x0;

	msg_printf(DBG,"PCI Interrupt Test... Begin\n");

	/* assume default values */
	baud = 9600; 
	d_errors = 0x0;

   	/* get the args */
   	argc--; argv++;

	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	    switch (argv[0][1]) {
		case 'd':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], &(device));
				argc--; argv++;
			} else {
				atob(&argv[0][2], &(device));
			}
			break;
		default: 
			bad_arg++; break;
	    }
	    argc--; argv++;
   	}

   	if ( bad_arg || argc || 
	     (device < 0x0) || (device > PCI_INTR_TEST_LASTDEVICE) ) {
	    msg_printf(ERR, "Usage: \n\
	    	-d <Device Number> [0|1|2|3|4]\n");
	    return(1);
   	} 

	/* get the pointer to device space */
	ioc3_mem = IOC3_PCI_DEVIO_BASE;
	msg_printf(DBG, "ioc3_mem 0x%x\n", ioc3_mem);

	switch (device) {
	    case PCI_INTR_TEST_SERIAL_A:
		portBase = ioc3_mem + IOC3_SIO_UA_BASE;
		intrMask = SIO_IR_SA_INT;
		PIO_REG_WR_8((ioc3_mem+IOC3_SIO_IES), intrMask, intrMask);

		_pci_sio_cfg_port(portBase, baud);
	
		PIO_REG_WR_8((portBase+INTR_ENABLE_REG), mask, 0x1);
		
		_pci_sio_getc(portBase, &ch);
		
		/* For Debugging Only */

		break;

	    case PCI_INTR_TEST_SERIAL_B:
		portBase = ioc3_mem + IOC3_SIO_UB_BASE;
	
		intrMask = SIO_IR_SB_INT;
		
		PIO_REG_WR_8((ioc3_mem+IOC3_SIO_IES), intrMask, intrMask);
		
	msg_printf(DBG, "ioc3_mem+IOC3_SIO_IES 0x%x\n", ioc3_mem+IOC3_SIO_IES);
		_pci_sio_cfg_port(portBase, baud);
		
	msg_printf(DBG, "after cfg port");
		PIO_REG_WR_8((portBase+INTR_ENABLE_REG), mask, 0x1);
		
	msg_printf(DBG, "after dev intr write");
		_pci_sio_getc(portBase, &ch); /*hangs here as no data written*/
		
	msg_printf(DBG, "after sio get char");
		break;

	    case PCI_INTR_TEST_PARALLEL:
		break;

	    case PCI_INTR_TEST_KEYBOARD:
		break;

	    case PCI_INTR_TEST_REALTIME:
		break;
	}

	/* poll the SIO_IR for the interrupt bit */
	timeOut = 0x1000;
	do {
	    PIO_REG_RD_8((ioc3_mem+IOC3_SIO_IR), intrMask, intrVal);
	    timeOut--;
	} while ((intrVal != intrMask) && timeOut);

	if (!timeOut) d_errors++;

#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "PCI Interrupt ", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_PCI_0002], d_errors );
}
