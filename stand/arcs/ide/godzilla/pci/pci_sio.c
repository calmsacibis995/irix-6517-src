/*
 * pci_sio.c
 *	
 * PCI Serial Port loopback Test. Requires plug.
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

#ident "$Revision: 1.11 $"

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
bool_t 		pci_sio(__uint32_t argc, char **argv);
__uint32_t 	_pci_sio_cfg_port(bridgereg_t port_base, __uint32_t baud_rate);
__uint32_t 	_pci_sio_putc(bridgereg_t port_base, uchar_t data);
__uint32_t 	_pci_sio_getc(bridgereg_t port_base, uchar_t *data);
bool_t 		_pci_sio_dataready(bridgereg_t port_base);

/*
 * Name:        pci_sio
 * Description: Tests the PCI Serial Ports
 * Input:       following switches are currently used: (with or without space)
 *    		-p <Port Number> [0=A|1=B]	
 *    		-b <Baud Rate> [6..458333]
 *    		-t <Text>
 * pci_sio -p 1 -t abc -b 6; pci_sio -p 1 -t abc -b 458333; pci_sio -p 1 -t abc
 * Output:      Returns 0 if no error, 1 if error
 * Error Handling:
 * Side Effects: none
 * Remarks: Uses IOC3
 * Debug Status: compiles, simulated, not emulated, debugged
 */
bool_t
pci_sio(__uint32_t argc, char **argv)
{
	__uint32_t	i, ioc3_dev_num = IOC3_PCI_SLOT;
	int		port, baud, bad_arg = 0;
	bridgereg_t	ioc3_mem, portBase;
	char		testStr[500], portString[256],tmp[256];
	uchar_t		inChar, outChar;

	msg_printf(DBG,"PCI Serial Port Test... Begin\n");

	/* assume default values */
	d_errors = 0;
	port = 0x0; /* assume Port A */
	baud = 9600; 

   	/* get the args */
   	argc--; argv++;

	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	    switch (argv[0][1]) {
		case 'p':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], &(port));
				argc--; argv++;
			} else {
				atob(&argv[0][2], &(port));
			}
			break;
		case 'b':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], &(baud));
				argc--; argv++;
			} else {
				atob(&argv[0][2], &(baud));
			}
			break;
		case 't':
			if (argv[0][2]=='\0') {
				strcpy(&(testStr[0]), &(argv[1][0]));
				argc--; argv++;
			} else {
				strcpy(&(testStr[0]), &(argv[0][2]));
			}
			break;
		default: 
			bad_arg++; break;
	    }
	    argc--; argv++;
   	}

   	if ( bad_arg || argc || 
	     ((port != 0) && (port != 1)) ||   
	     (baud <= 0) || (baud <= 0) ) {	
	    msg_printf(ERR, "Usage: \n\
	    	-p <Port Number> [0|1]\n\
	    	-b <Input Baud Rate>\n\
	    	-t <Text>\n");
	    return(1);
   	} 

	/* get the pointer to device space */
	ioc3_mem = IOC3_PCI_DEVIO_BASE;

	/* get the base to the serial port */
	portBase = ioc3_mem;
	if (!port) { /*port A*/
	   portBase += IOC3_SIO_UA_BASE;
	   strcpy(portString, "Serial Port A Test");
	   strcpy(tmp,"Running "); 
	   strcat(tmp, portString); strcat(tmp, "\n");
	   msg_printf(SUM,tmp);
	}
	else {
	   portBase += IOC3_SIO_UB_BASE;
	   strcpy(portString, "Serial Port B Test");
	   strcpy(tmp,"Running "); 
	   strcat(tmp, portString); strcat(tmp, "\n");
	msg_printf(SUM,tmp);
	}

	/* configure the serial port */
	_pci_sio_cfg_port(portBase, baud);

	/* perform serial output operation only */
	i = 0;
	do {
	   outChar = testStr[i];
	   _pci_sio_putc(portBase, outChar);
	   if (!_pci_sio_dataready(portBase)) {
	      d_errors++; goto exit;
	   }
	   _pci_sio_getc(portBase, &inChar); 
	   msg_printf(DBG, "Loopback Write chr = %c\n", inChar);
	   msg_printf(DBG, "Loopback Read chr = %c\n", outChar);
	   if (inChar != outChar) {
	      d_errors++; goto exit;
	   }
	   i++;
	} while (outChar != NULL);
exit:
	if (d_errors) msg_printf(INFO, "Ensure that Serial Port Plug is connected\n");
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "PCI Serial Port", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_PCI_0006], d_errors );
}

__uint32_t
_pci_sio_cfg_port(bridgereg_t port_base, __uint32_t baud_rate)
{
	bridgereg_t	tmp1;
	uchar_t		tmp, mask = ~0x0;
	__uint32_t	baud, errors = 0;

	/* first configure the port */
	/* make sure the transmit FIFO is empty */
	do {
	   tmp1 = port_base+LINE_STATUS_REG; 
	   PIO_REG_RD_8(tmp1, mask, tmp);
	} while (!(tmp & LSR_XMIT_EMPTY));

	/* divide the baud rate and set it up */
	baud = SER_DIVISOR(baud_rate, SER_CLK_SPEED(SER_PREDIVISOR));
	PIO_REG_WR_8((port_base+LINE_CNTRL_REG), mask, LCR_DLAB);
	PIO_REG_WR_8((port_base+DIVISOR_LATCH_MSB_REG), mask, (baud >> 8)&0xff);
	PIO_REG_WR_8((port_base+DIVISOR_LATCH_LSB_REG), mask, baud & 0xff);

	/* set operating parameters and set DLAB to 0 */
	PIO_REG_WR_8((port_base+LINE_CNTRL_REG), mask, 
		(LCR_8_BITS_CHAR | LCR_1_STOP_BITS));

	/* no interrupt */
	PIO_REG_WR_8((port_base+INTR_ENABLE_REG), mask, 0x0);

	/* enable FIFO mode and reset both FIFOs */
	PIO_REG_WR_8((port_base+FIFO_CNTRL_REG), mask, FCR_ENABLE_FIFO);
	PIO_REG_WR_8((port_base+FIFO_CNTRL_REG), mask,
		(FCR_ENABLE_FIFO | FCR_RCV_FIFO_RESET | FCR_RCV_FIFO_RESET));

	return(errors);
}

__uint32_t
_pci_sio_putc(bridgereg_t port_base, uchar_t data)
{
	__uint32_t	errors = 0;
	uchar_t		tmp, mask = ~0x0;

	do {
	   PIO_REG_RD_8((port_base+LINE_STATUS_REG), mask, tmp);
	} while (!(tmp & LSR_XMIT_BUF_EMPTY));
	PIO_REG_WR_8((port_base+XMIT_BUF_REG), mask, data);

	return(errors);
}

__uint32_t
_pci_sio_getc(bridgereg_t port_base, uchar_t *data)
{
	__uint32_t	errors = 0;
	uchar_t		tmp, mask = ~0x0;

	PIO_REG_RD_8((port_base+RCV_BUF_REG), mask, tmp);
	*data = tmp;

	return(errors);
}

bool_t
_pci_sio_dataready(bridgereg_t port_base)
{
	__uint32_t	errors = 0;
	uchar_t		tmp, mask = ~0x0;
	int timeOut;
	timeOut = 0x20;
	do {
	    PIO_REG_RD_8((port_base+LINE_STATUS_REG), mask, tmp);
	    delay(10); timeOut--;
	} while(!(tmp & LSR_DATA_READY) && timeOut);

	if (!timeOut) return 0; else return (1);

}

