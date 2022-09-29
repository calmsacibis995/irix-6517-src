/*  
 * pci_parallel.c : parallel port Tests
 *
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S GOVERNMENT RESTRICTED RIGHTS LEGEND:
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

#ident "ide/godzilla/pci/pci_parallel.c:  $Revision: 1.7 $"


#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_ioc3.h"
#include "d_godzilla.h"
#include "d_prototypes.h"
#include "d_frus.h"
#include "d_prototypes.h"

/* forward declaration */
void initParallelPort(void);

bool_t
pci_par(__uint32_t argc, char **argv)
{

	int result=0;
	msg_printf(INFO,"IOC3 Parallel Port Test\n");

	d_errors = 0; /* init : it is incremented here, 
			not in the called subroutines */
	initParallelPort();
	result = testFillFIFO();
	if (!result ) testDataFIFO();
	/*if (!result ) testDataLinesFIFO();*/
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(result, "PCI Parallel Port", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_PCI_0004], result );
}

/*
The port is initialized as per ioc3 spec. The most note worthy thing
being that the port is set in the test fifo mode (see duart doc)
*/

void initParallelPort(void) {


   /*Clear dma buffers. P 51 ioc3 spec*/
   /*add #define writebit fields*/
   PIO_REG_WR_32(IOC3REG_PPCR, 0xFFFFFFFF, 0x4000000);
   /*Set up interrupts*/
   PIO_REG_WR_32(IOC3REG_SIO_IR, 0xFFFFFFFF, 0x003C0000);
   PIO_REG_WR_32(IOC3REG_SIO_IEC, 0xFFFFFFFF, 0x003C0000);
   /*Disable DMA*/
   PIO_REG_WR_32(IOC3REG_PPCR, 0xFFFFFFFF, 0x0000000);

   /*set up device control register on duart*/
   PIO_REG_WR_8(DU_SIO_PP_DCR, 0xFF, 0x1B);
   /*disable dma, enable intr, set fifo test mode 110*/
   PIO_REG_WR_8(DU_SIO_PP_ECR, 0xFF, 0xC4);
}

/*
The fifo is a 16 byte buffer. Writing to the fifo the data is stored
in the buffer. After writing 16 bytes the buffer is full as checked by a 
bit in the ecr register.
*/

bool_t testFillFIFO(void) {
   uchar_t	outChar,inChar, testRead, expChar;
   int i,result=-1; /*fail*/

   /*Fill Fifo*/
   outChar = 0x60; /*'a'*/
   for (i = 1; i<=17; i++) {	
 	PIO_REG_WR_8(DU_SIO_PP_FIFA, 0xFF, outChar+i);
	msg_printf(DBG, "FIFO Write= %x %c\n", i, outChar+i);
	PIO_REG_RD_8(DU_SIO_PP_ECR, 0xFF, testRead);
	msg_printf(DBG, "ECR Register= %x\n", testRead);
	if (testRead == 0xc6) result = 0;
   }

  if (!result) msg_printf(INFO, "FIFO Fill Test Passed\n");
  else msg_printf(INFO, "FIFO Fill Test Failed\n");

   return (result);
}

/*
The fifo buffer wraps around. First 16 chars 'a..o' are written.
Next search begins to find char 'a'. After that the next 15 chars are read
and compared for correctness. Internal duart write address/and read address
counters keep track of what is written where.
*/

bool_t testDataFIFO(void) {
   uchar_t	outChar,inChar, testRead, expChar;
   int i,result;

   /*write a char to test fifo register DU_SIO_PP_FIFA*/
   outChar = 0x60; /*'a'*/
   for (i = 1; i<=16; i++) {	
 	PIO_REG_WR_8(DU_SIO_PP_FIFA, 0xFF, outChar+i);
	msg_printf(DBG, "FIFO Write= %x %c\n", i, outChar+i);
   }
	
   /*Set read address counter. Increment it.*/
   inChar = 0;
   while (inChar !=  0x61) {
	PIO_REG_RD_8(DU_SIO_PP_FIFA, 0xFF, inChar); 
   	msg_printf(DBG, "FIFO Read Whl= %c\n", inChar);
   }
	
   for (i = 1; i<=15; i++) {
	PIO_REG_RD_8(DU_SIO_PP_FIFA, 0xFF, inChar); 	
	expChar = 0x61+i;
	msg_printf(DBG, "FIFO Read= %c\n", inChar);
	msg_printf(DBG, "FIFO Exp= %c\n", expChar);
 	if (inChar != expChar) {
	   result = -1;
	}
   }

 if (!result) msg_printf(INFO, "FIFO Data Test Passed\n");
  else msg_printf(INFO, "FIFO Data Test Failed\n");

   return (result);
}

/*
Does not work. Attempt to read data at the data port after writing to
fifo in the tfifo mode. Engineer says does not happen. Doc says it does.
*/

bool_t testDataLinesFIFO(void) {
   uchar_t	outChar,inChar, testRead=0, expChar;
   int i,result;

   /*write a char to test fifo register DU_SIO_PP_FIFA*/
   outChar = 0x60; /*'a'*/
   for (i = 1; i<=32; i++) {	
 	PIO_REG_WR_8(DU_SIO_PP_FIFA, 0xFF, outChar+i);
	msg_printf(DBG, "FIFO Write= %x %c\n", i, outChar+i);
	delay(10);
	PIO_REG_RD_8(DU_SIO_PP_DATA, 0xFF, testRead);
   	msg_printf(DBG, "DATA      I Read= %x\n", testRead);
   }
   	
   /*Set read address counter
   inChar = 0;
   while (inChar !=  0x61) {
	PIO_REG_RD_8(DU_SIO_PP_FIFA, 0xFF, inChar); 
   	msg_printf(DBG, "FIFO Read Whl= %c\n", inChar);
   }
	*/
   for (i = 1; i<=31; i++) {
	PIO_REG_RD_8(DU_SIO_PP_FIFA, 0xFF, inChar); 	
	expChar = 0x61+i;
	msg_printf(DBG, "FIFO Read= %c\n", inChar);
	msg_printf(DBG, "FIFO Exp= %c\n", expChar);
	delay(10);
	PIO_REG_RD_8(DU_SIO_PP_DATA, 0xFF, testRead);
   	msg_printf(DBG, "DATA    II Read= %x\n", testRead);
 	if (inChar != expChar) {
	   result = -1;
	}
   }

 if (!result) msg_printf(INFO, "FIFO Data Test Passed\n");
  else msg_printf(INFO, "FIFO Data Test Failed\n");

   return (result);
}

