 /* regs.c : 
 *	Debug Tool Register/Memory read/write Tests
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

#ident "ide/IP30/debugtool/regs.c:  $Revision: 1.8 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_ioc3.h"
#include "d_godzilla.h" 
#include "d_frus.h"
#include "d_prototypes.h"
#include "parser.h"
#include "regs.h"

const __uint64_t LowMemWrite = 0x1000000; /*test after this range. IDE load range omit*/
const __uint64_t LowMemRead = 0x0;
const short ArgLength = 25;
const short ArgLengthData = 18;

bool_t HELP (void) {
   
	__uint64_t memTop;
	
	msg_printf(INFO, "IOC3 Config Base = 0x%x\n",IOC3_PCI_CFG_BASE);
	msg_printf(INFO, "RAD Config Base = 0x%x\n",RAD_PCI_CFG_BASE);
	msg_printf(INFO, "SCSI Config Base = 0x%x\n",SCSI0_PCI_CFG_BASE);
	msg_printf(INFO, "IOC3 MEM Base = 0x%x\n",IOC3_PCI_DEVIO_BASE);
	msg_printf(INFO, "RAD Mem Base = 0x%x\n",RAD_PCI_DEVIO_BASE);
	msg_printf(INFO, "SCSI Mem Base = 0x%x\n",SCSI0_PCI_DEVIO_BASE);
	msg_printf(INFO, "Heart Base = 0x%x\n",HEART_BASE);
	msg_printf(INFO, "XBOW Base = 0x%x\n",XBOW_BASE);
	msg_printf(INFO, "Bridge Base = 0x%x\n",BRIDGE_BASE);

	memTop = cpu_get_memsize();
	msg_printf(INFO, "Memory Range  = 0x0..0x%x \n", memTop);
	msg_printf(INFO, "Examples to copy and execute...\n");
	msg_printf(INFO, "ip30_reg_peek -a h -o 0x18000000 #Heart Widget ID Register\n");
	msg_printf(INFO, "ip30_reg_poke -a h -o 0x18000028 -d 0xa5a5a5a5a5a5a5a5\n");
	msg_printf(INFO, "ip30_mem_peek -a 0x1000000:0x100000C -l 1 -s 1 \n");
	msg_printf(INFO, "ip30_mem_poke -a 0x1000000:0x100000C -l 1 -d 0xFFFFFFFF -s 1\n");
	msg_printf(INFO, "ip30_ioc3_peek\n");
	msg_printf(INFO, "ip30_ioc3_poke -d 0x0\n");
	msg_printf(INFO, "Options:>\n");
	msg_printf(INFO, "-a <register address>\n");
	msg_printf(INFO, "-a <memory start address:memory end address>\n");
	msg_printf(INFO, "-l <loop count <1..n>\n");
	msg_printf(INFO, "-s <data size> 0=8bit, 1=16bit, 2=32Bit, 3=64Bit\n");
	msg_printf(INFO, "-d <write data> 8..64 bits\n");
	msg_printf(INFO, "Report Bugs to Epicurus at rattan@mfg.sgi.com\n");
	return 0;
}

/*
 * Name:	regs.c
 * Description:	Individual chip register read
 * Input:	none
 * Output:	display register value read
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 * size 0=8bit, 1=16bit, 2=32Bit, 3=64Bit                         
 * ip30_reg_peek -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count>   
 */

bool_t
ip30_reg_peek(__uint32_t argc, char **argv)
{
	short size, i, bad_arg = 0;
	long long regAddr64; /*64 bits*/
	int loop, offset; /*32 bits*/
	char	testStr[50];  
	bool_t addrArgDefined = 0, offsetArgDefined = 0;
	char *errStr;
	msg_printf(INFO,"Starting Register Read\n");

	/*defaults*/
	regAddr64 = IOC3REG_PCI_ID+4;
	loop = 1;

	/* get the args */
	argc--; argv++;

	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {
			case 'a':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex address argument required\n");
					msg_printf(INFO, "Usage: ip30_reg_peek -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> \n");
					return 1;	
				}

				/*check for string length*/
				if (strlen(argv[1])>1) {
					msg_printf(INFO, "Hex address argument incorrect or too long\n");
					msg_printf(INFO, "Usage: ip30_reg_peek -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> \n");
					return 1;	
				}
				if (argv[0][2]=='\0') {
					strcpy(&(testStr[0]), &(argv[1][0]));
					argc--; argv++;
					addrArgDefined = 1;
				} else {
					strcpy(&(testStr[0]), &(argv[0][2]));
				}
			break;     
			case 'o':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex offset argument required\n");
					msg_printf(INFO, "Usage: ip30_reg_peek -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> \n");
					return 1;	
				}

				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Hex offset argument too long\n");
					msg_printf(INFO, "Usage: ip30_reg_peek -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> \n");
					return 1;	
				}
				if (argv[0][2]=='\0') {
					errStr = atob(&argv[1][0], &(offset));
					argc--; argv++;
					/*if converted string is not an legal hex number*/	
					if (errStr[0] != '\0') {	
						msg_printf(INFO, "Hex offset argument required\n");
						msg_printf(INFO, "Usage: ip30_reg_peek -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> \n");
						return 1;	
           		} 
					offsetArgDefined = 1;
				} else {
					atob(&argv[0][2], &(offset));
				}
			break;
			case 'l':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex loop argument required\n");
					msg_printf(INFO, "Usage: ip30_reg_peek -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> \n");
					return 1;	
				}
				
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Loop argument too long\n");
					msg_printf(INFO, "Usage: ip30_reg_peek -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> \n");
					return 1;	
				}
            
				if (argv[0][2]=='\0') {
					errStr = atob(&argv[1][0], &(loop));
					argc--; argv++;
					/*if converted string is not an legal hex number*/	
					if (errStr[0] != '\0') {	
						msg_printf(INFO, "Hex loop argument required\n");
					msg_printf(INFO, "Usage: ip30_reg_peek -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> \n");
						return 1;	
           		} 
            } else {
					errStr = atob(&argv[0][2], &(loop));
            }
			break;
			default: 
			bad_arg++; break;
		}
		argc--; argv++;
	}

	if (  (!addrArgDefined) || (!offsetArgDefined)) {   
		msg_printf(INFO,"Args -a and -o are required\n");
		msg_printf(INFO, "Usage: ip30_reg_peek -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> \n");
		return 1;
	}

	switch (testStr[0]) {
		case 'h':
			heart_ind_peek(offset, regAddr64, loop);
		break;
		case 'b':
			bridge_ind_peek(offset, regAddr64, loop);
		break; 
		case 'x':
			xbow_ind_peek(offset, regAddr64, loop);
		break;
		case 'i':
			ioc3_ind_peek(offset, regAddr64, loop);
		break;
		case 'r':	
			rad_ind_peek(offset, regAddr64, loop);
		break;
		case 'd':	
			duart_ind_peek(offset, regAddr64, loop);
		break;
		case 's':	
			initSCSI();
			scsi_ind_peek(offset, regAddr64, loop);
			restoreSCSI();
		break;
		case 'p':	
			phy_ind_peek(offset, regAddr64, loop);
		break;
		default: /*error*/
		msg_printf(INFO,"Incorrect arg -a\n");
		msg_printf(INFO, "Usage: ip30_reg_peek -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> \n");
   	return 1;
   }
     
   return 0;
	
}

/*
 * Name:	regs.c
 * Description:	register read
 * Input:	none
 * Output:	display register value read
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 * size 0=8bit, 1=16bit, 2=32Bit, 3=64Bit                         
 * ip30_reg_poke -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> -d <data> 
 */

bool_t
ip30_reg_poke(__uint32_t argc, char **argv)
{
  	short size, i, bad_arg = 0;
  	long long regAddr64, data; /*64 bits*/
  	int loop, offset; /*32 bits*/
  	char	testStr[50];  
	bool_t addrArgDefined = 0, offsetArgDefined = 0;
	bool_t dataArgDefined = 0;
  	char *errStr;

  	msg_printf(INFO,"Starting Register Write\n");

  	/*defaults*/
  	regAddr64 = IOC3REG_PCI_ID+4;
  	loop = 1;
	
  	data = 0xFFFFFFFFFFFFFFFF;

  	/* get the args */
  	argc--; argv++;

	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {
			case 'a':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex address argument required\n");
					msg_printf(INFO, "Usage: ip30_reg_poke -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> -d <data>\n");
					return 1;	
				}

				/*check for string length*/
				if (strlen(argv[1])>1) {
					msg_printf(INFO, "Hex address argument incorrect or too long\n");
					msg_printf(INFO, "Usage: ip30_reg_poke -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> -d <data>\n");
					return 1;	
				}
				if (argv[0][2]=='\0') {
					strcpy(&(testStr[0]), &(argv[1][0]));
					argc--; argv++;
					addrArgDefined = 1;
				} else {
					strcpy(&(testStr[0]), &(argv[0][2]));
				}
			break;     
			case 'o':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex offset argument required\n");
					msg_printf(INFO, "Usage: ip30_reg_poke -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> -d <data>\n");
					return 1;	
				}

				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Hex offset argument too long\n");
					msg_printf(INFO, "Usage: ip30_reg_poke -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> -d <data>\n");
					return 1;	
				}
				if (argv[0][2]=='\0') {
					errStr = atob(&argv[1][0], &(offset));
					argc--; argv++;
					/*if converted string is not an legal hex number*/	
					if (errStr[0] != '\0') {	
						msg_printf(INFO, "Hex offset argument required\n");
						msg_printf(INFO, "Usage: ip30_reg_poke -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> -d <data>\n");
						return 1;	
           				} 
					offsetArgDefined = 1;
				} else {
					atob(&argv[0][2], &(offset));
				}
			break;
			case 'l':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex loop argument required\n");
					msg_printf(INFO, "Usage: ip30_reg_poke -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> -d <data>\n");
					return 1;	
				}
				
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Loop argument too long\n");
					msg_printf(INFO, "Usage: ip30_reg_poke -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> -d <data>\n");
					return 1;	
				}
            
				if (argv[0][2]=='\0') {
					errStr = atob(&argv[1][0], &(loop));
					argc--; argv++;
					/*if converted string is not an legal hex number*/	
					if (errStr[0] != '\0') {	
						msg_printf(INFO, "Hex loop argument required\n");
						msg_printf(INFO, "Usage: ip30_reg_poke -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> -d <data>\n");
						return 1;	
           				} 
            			} else {
					errStr = atob(&argv[0][2], &(loop));
            			}
			break;
			case 'd':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex data argument required\n");
					msg_printf(INFO, "Usage: ip30_reg_poke -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> -d <data>\n");
					return 1;	
				}
           
				/*check for string length*/
				if (strlen(argv[1])>ArgLengthData) {
					msg_printf(INFO, "Data argument too big, 0..0xFFFFFFFFFFFFFFFF required\n");
					msg_printf(INFO, "Usage: ip30_reg_poke -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> -d <data>\n");
					return 1;	
				}
			
				if (argv[0][2]=='\0') {
					errStr = atob_L(&argv[1][0], &(data));
					argc--; argv++; 
					/*if converted string is not an legal hex number*/	
					if (errStr[0] != '\0') {	
						msg_printf(INFO, "Hex data argument required\n");
						msg_printf(INFO, "Usage: ip30_reg_poke -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> -d <data>\n");
						return 1;	
        			} 
				} else {
					errStr = atob_L(&argv[0][2], &(data));
				}
				dataArgDefined = 1;
			break;
	 		default: 
            bad_arg++; break;
      }
	    argc--; argv++;
   }

  	if (  (!addrArgDefined) || (!offsetArgDefined) || (!dataArgDefined)  ) {   
  		msg_printf(INFO,"Args -a and -o are required\n");
  		msg_printf(INFO, "Usage: ip30_reg_poke -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> -d <data>\n");
		return 1;
  	}

   switch (testStr[0]) {
      case 'h':
			if (data == 0)  {
	   		msg_printf(INFO, "Cannot write 0x0 to heart registers\n");
	   		return 1;
			}
			heart_ind_poke(offset, regAddr64, loop, data);
      break;
      case 'b':
			bridge_ind_poke(offset, regAddr64, loop, data);
      break; 
      case 'x':
			xbow_ind_poke(offset, regAddr64, loop, data);
      break;
      case 'i':
			ioc3_ind_poke(offset, regAddr64, loop, data);
      break;
      case 'r':	
			rad_ind_poke(offset, regAddr64, loop, data);
      break;
      case 'd':	
			duart_ind_poke(offset, regAddr64, loop, data);
      break;
      case 's':	
			initSCSI();
			scsi_ind_poke(offset, regAddr64, loop, data);
			restoreSCSI();
      break;
      case 'p':	
			phy_ind_poke(offset, regAddr64, loop, data);
      break;
      default: /*error*/
      	msg_printf(INFO,"Incorrect arg -a\n");
 	msg_printf(INFO, "Usage: ip30_reg_poke -a <chip name, i=ioc3,r=rad,s=scsi,h=heart,b=bridge,x=xbow...> -o <register offset> -l<loop count> -d <data>\n");
	return 1;
   }
   
   return 0;
	
}

/*returns true if error occured*/
bool_t MemoryAlligned(__uint32_t startAddress, __uint32_t endAddress, short size) {

	bool_t err = 1;

	msg_printf(DBG, "start mod = %d, end mod = %d, err = %d, dataSz %d\n", (startAddress % 2 ), (endAddress % 2), err, size);
	
	switch (size) {
		case EightBit:
			err = 0;
		break;
		
		case SixteenBit:
		if ( (startAddress % 2 ) || (endAddress % 2)  ) {
				err = 1;	
			} else err = 0; 
		break;

		case ThirtyTwoBit:
			if ( (startAddress % 4 ) || (endAddress % 4)  ) {
				err = 1;	
			} else  err = 0; 
		break;

		case SixtyFourBit:
			if ( (startAddress % 8 ) || (endAddress % 8)  ) {
				err = 1;	
			} else err = 0; 
		break;

		default:
			msg_printf(DBG, "SW Error\n");
			err = 1;
	}

	return err;
}

/*
 * Name:	regs.c
 * Description:	memory read
 * Input:	none
 * Output:	display value read
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 * ip30_mem_peek -a 0x1000000:0x100000C -l 1 -s 0
 * ip30_mem_peek -a 0x1000000:0x100000C -l 1 -s 1
 * ip30_mem_peek -a 0x1000000:0x100000C -l 1 -s 2
 * ip30_mem_peek -a 0x1000000:0x100000C -l 1 -s 3
 * ip30_mem_peek -a 0x1000000:0x100000C -l 1 -s 3
 * ip30_mem_peek -a 0x7FFFFFC:0x8000000 -l 1 -s 1
 * access to non existing string argv[x] creates exception
 * 0x9000000021000000 0x9000000028000000
 */

bool_t
ip30_mem_peek(__uint32_t argc, char **argv)
{
   int loop;
   char	testStr[50], startStr[50], endStr[50];
   short i, bad_arg = 0,nextStart, j;
   long long regStartAddr, regEndAddr, readValue64, *readAddr64, mask, readAddress64;
   
   char readValue8, *readAddr8;
   short readValue16, *readAddr16;
	int readValue32, *readAddr32;
	char *errStr;
	bool_t addrArgDefined = 0;
	__uint64_t memTop;
	bool_t foundColon = 0; /*false*/
	unsigned int loopCount;
   int dataSize;
	
	/*check for nil args*/
  	if ( argc == 1 ) {
		msg_printf(INFO, "Args -a is required\n");
		msg_printf(INFO, "Usage: ip30_mem_peek -a<start:end> -l<loopcount> -s <size> \n");
		return 1;	
	}
	
	/* get the args */
	argc--; argv++;
   loopCount = 1;
   strcpy(testStr, "0x0:0xc");
   mask = ~0; 
   dataSize = 2; /*32 bit operation*/

   while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
      switch (argv[0][1]) {    
       	case 'a':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex address argument required\n");
					msg_printf(INFO, "Usage: ip30_mem_peek -a<start:end> -l<loopcount> -s <size> \n");
					return 1;	
				}

				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Hex address argument too long\n");
					msg_printf(INFO, "Usage: ip30_mem_peek -a<start:end> -l<loopcount> -s <size> \n");
					return 1;	
				}

	   		if (argv[0][2]=='\0') {
		 			strcpy(&(testStr[0]), &(argv[1][0]));
					argc--; argv++;
		 			addrArgDefined = 1;
	    		} else {
					strcpy(&(testStr[0]), &(argv[0][2]));
				}
	 	break;
         case 'l':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex loop argument required\n");
					msg_printf(INFO, "Usage: ip30_mem_peek -a<start:end> -l<loopcount> -s <size> \n");
					return 1;	
				}
				
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Loop argument too long\n");
					msg_printf(INFO, "Usage: ip30_mem_peek -a<start:end> -l<loopcount> -s <size> \n");
					return 1;	
				}
            
				if (argv[0][2]=='\0') {
					errStr = atob(&argv[1][0], &(loop));
					loopCount = loop;
					argc--; argv++;
					/*if converted string is not an legal hex number*/	
					if (errStr[0] != '\0') {	
						msg_printf(INFO, "Hex loop argument required\n");
						msg_printf(INFO, "Usage: ip30_mem_peek -a<start:end> -l<loopcount> -s <size> \n");
						return 1;	
           		} 
            } else {
					errStr = atob(&argv[0][2], &(loop));
            }
	 break;	 
	 case 's':
				
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex size argument required\n");
					msg_printf(INFO, "Usage: ip30_mem_peek -a<start:end> -l<loopcount> -s <size> \n");
					return 1;	
				}
            
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Size argument too long\n");
					msg_printf(INFO, "Usage: ip30_mem_peek -a<start:end> -l<loopcount> -s <size> \n");
					return 1;	
				}
				
				if (argv[0][2]=='\0') {
					errStr = atob(&argv[1][0], &(dataSize));
					argc--; argv++; 
					/*if converted string is not an legal hex number*/	
					if (errStr[0] != '\0') {	
						msg_printf(INFO, "Hex size argument required\n");
						msg_printf(INFO, "Usage: ip30_mem_peek -a<start:end> -l<loopcount> -s <size> \n");
						return 1;	
           		} 
					/*if converted string is not the right size*/	
					if (dataSize > 3) {	
						msg_printf(INFO, "Hex size arguemnt 0..3 required\n");
						msg_printf(INFO, "Usage: ip30_mem_peek -a<start:end> -l<loopcount> -s <size> \n");
						return 1;	
           		} 
				} else {
					errStr = atob(&argv[0][2], &(dataSize));
            }
	break;
	 default: 
		msg_printf(INFO, "Illegal option\n");
		msg_printf(INFO, "Usage: ip30_mem_peek -a<start:end> -l<loopcount> -s <size> \n");
		bad_arg++; 
		return 1;	
	    }
	    argc--; argv++;
   	}
	
	/*set memTop*/
	switch (dataSize) {
		case EightBit:
			memTop = cpu_get_memsize()-1;
		break;
		case SixteenBit:
			memTop = cpu_get_memsize()-2;
		break;
		case ThirtyTwoBit:
			memTop = cpu_get_memsize()-4;
		break;
		case SixtyFourBit:
			memTop = cpu_get_memsize()-8;
		break;
		default:
			memTop = cpu_get_memsize()-4;
	}

	msg_printf(DBG, "Memory Range 0x%x:0x%x\n", LowMemRead,  memTop);
   
	if (!addrArgDefined) {   
      msg_printf(INFO,"Args -a is required\n");
		msg_printf(INFO, "Usage: ip30_mem_peek -a<start:end> -l<loopcount> -s <size> \n");
		return 1;	
	}
	
	/*look for colon and null second string*/
   i = 0;
   do {
		if (testStr[i] == ':') {
			foundColon = 1;
			break;	
		}
      i++;
   } while (testStr[i-1] != '\0');

	if (!foundColon) {
		msg_printf(INFO, "Illegal argument -  address (1)\n");
		msg_printf(INFO, "Usage: ip30_mem_peek -a<start:end> -l<loopcount> -s <size> \n");
		return 1;
	} else {
		msg_printf(DBG, "Found Colon :\n");
	}

	/*look for null string 2*/	
	if (testStr[i+1] == '\0') {
		msg_printf(INFO, "Illegal argument -  second address\n");
		msg_printf(INFO, "Usage: ip30_mem_peek -a<start:end> -l<loopcount> -s <size> \n");
		return 1;	
	}
	
	/*parse addr agrs*/
   i = 0;
   do {
     startStr[i] = testStr[i];	     
      i++;
		/*look for colon*/
		if (i > 17) {
			msg_printf(INFO, "Illegal argument -  address (2)\n");
			msg_printf(INFO, "Usage: ip30_mem_peek -a<start:end> -l<loopcount> -s <size> \n");
			return 1;
		}
   } while (testStr[i] != ':');
	
	startStr[i] = '\0';
   i++;

   j = 0;
   do {
     endStr[j] = testStr[i];	    
      i++; j++;
   } while (testStr[i] != NULL);
   endStr[j] = '\0'; 

   msg_printf(DBG,"Addr input string = %s; Start Address str = %s; End Addr str = %s\n", testStr, startStr, endStr);

   /*get hex addr*/
   errStr = atob_L(startStr, &(regStartAddr));
	/*if converted string is not an legal hex number*/	
	if (errStr[0] != '\0') {	
		msg_printf(INFO, "Hex address argument required\n");
		msg_printf(INFO, "Usage: ip30_mem_peek -a<start:end> -l<loopcount> -s <size> \n");
		return 1;	
  	} 
   errStr = atob_L(endStr, &(regEndAddr));
	/*if converted string is not an legal hex number*/	
	if (errStr[0] != '\0') {	
		msg_printf(INFO, "Hex address argument required\n");
		msg_printf(INFO, "Usage: ip30_mem_peek -a<start:end> -l<loopcount> -s <size> \n");
		return 1;	
  	} 
   
	/*end address should be > start*/
	if (regStartAddr > regEndAddr) {
		msg_printf(INFO, "End address must be greater than start address\n");
		msg_printf(INFO, "Usage: ip30_mem_peek -a<start:end> -l<loopcount> -s <size> \n");
		return 1;
	}

	/*is address in range*/
	if (   (regStartAddr < LowMemRead)  || (regEndAddr > memTop)  )  {
		msg_printf(INFO, "Memory argument range error. Valid Range 0x%x:0x%x\n", LowMemRead,  memTop);
		return 1;
	}

	/*check memory allignment*/
	if ( MemoryAlligned(regStartAddr, regEndAddr, dataSize )  ) {
		msg_printf(INFO, "Misaligned Memory Access\n");
		msg_printf(INFO, "Usage: ip30_mem_peek -a<start:end> -l<loopcount> -s <size> \n");
		return 1;
	}

	msg_printf(DBG,"Start Addr %x; End Address %x\n", regStartAddr, regEndAddr);
	
	for (i = 1; i <= loopCount; i++) {
		switch (dataSize) {
			 case EightBit:
	    		msg_printf(INFO,"Starting Memory Read (8)\n");
            for (readAddr8 = (char *)regStartAddr; readAddr8 <= (char *)regEndAddr; readAddr8++) {     
					if ((long long)readAddr8 < 0x4000) readAddress64 = (long long)readAddr8; 
					else readAddress64 = (long long)readAddr8 + PHYS_RAMBASE;
					PIO_REG_RD_8(readAddress64, (char)mask, readValue8);
	       		msg_printf(INFO,"Read Addr (8) 0x%x = 0x%x\n", readAddress64, readValue8);     
            } 
         break;
	 		case SixteenBit:
	   		msg_printf(INFO,"Starting Memory Read (16)\n");
            for (readAddr16 = (short *)regStartAddr; readAddr16 <= (short *)regEndAddr; readAddr16++) {     
					readAddress64 = ((long long)readAddr16 < 0x4000) ? (long long)readAddr16 : (long long)readAddr16 + PHYS_RAMBASE;
	       		PIO_REG_RD_16(readAddress64, (short)mask, readValue16);
	       		msg_printf(INFO,"Read Addr (16) 0x%x = 0x%x\n", readAddress64, readValue16);     
            } 
         break;
         case SixtyFourBit:
	    		msg_printf(INFO,"Starting Memory Read (64)\n");
            for (readAddr64 = (long long *)regStartAddr; readAddr64<= (long long *)regEndAddr; readAddr64++) {               
					readAddress64 = ((long long)readAddr64 < 0x4000) ? (long long)readAddr64 : (long long)readAddr64 + PHYS_RAMBASE;
               PIO_REG_RD_64(readAddress64, mask, readValue64);
	       		msg_printf(INFO,"Read Addr (64) 0x%x = 0x%x\n", readAddress64, readValue64);
            } 
         break;
         case ThirtyTwoBit:
         default:
	    		msg_printf(INFO,"Starting Memory Read (32)\n");
            for (readAddr32 = (int *)regStartAddr; readAddr32 <= (int *)regEndAddr; readAddr32++) {         
					readAddress64 = ((long long)readAddr32 < 0x4000) ? (long long)readAddr32 : (long long)readAddr32 + PHYS_RAMBASE;
	       		PIO_REG_RD_32(readAddress64, (int)mask, readValue32);
	       		msg_printf(INFO,"Read Addr (32) 0x%x = 0x%x\n", readAddress64, readValue32);
            } 
      }
   }

   return 0;
	
}

/*Check data size*/
bool_t CheckDataArg(__uint64_t data, short size) {

	bool_t err = 1;
	
	switch (size) {
		case EightBit:
			if (data > 0xFF) {
				err = 1;	
				msg_printf(INFO, "Incorrect data argument, expected 0x0..0xFF\n");
			} else err = 0; 
		break;
		
		case SixteenBit:
			if (data > 0xFFFF) {
				err = 1;	
				msg_printf(INFO, "Incorrect data argument, expected 0x0..0xFFFF\n");
			} else err = 0; 
		break;

		case ThirtyTwoBit:
			if (data > 0xFFFFFFFF) {
				err = 1;	
				msg_printf(INFO, "Incorrect data argument, expected 0x0..0xFFFFFFFF\n");
			} else err = 0; 
		break;

		case SixtyFourBit:
			/*cannot happen*/
			if (data > 0xFFFFFFFFFFFFFFFF) {
				err = 1;	
				msg_printf(INFO, "Incorrect data argument, expected 0x0..0xFFFFFFFFFFFFFFFF\n");
			} else err = 0; 
		break;

		default:
			msg_printf(DBG, "SW Error\n");
			err = 1;
	}

	return err;
}
/*
 * Name:	regs.c
 * Description:	memory read
 * Input:	none
 * Output:	display value read
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, emulated, 
 * ip30_mem_poke -a 0x1000000:0x100000C -l 1 -s 0 -d 0xFF
 * ip30_mem_poke -a 0x1000000:0x100000C -l 1 -s 1 -d 0xFFFF
 * ip30_mem_poke -a 0x1000000:0x100000C -l 1 -s 2 -d 0xFFFFFFFF
 * ip30_mem_poke -a 0x1000000:0x100000C -l 1 -s 3 -d 0xFFFFFFFFFFFFFFFF
 * ip30_mem_poke -a 0x1000000:0x100000C -l 1 -s 3 -d 0xFFFFFFFFFFFFFFFF
 * ip30_mem_poke -a 0x7FFFFFC:0x8000000 -l 1 -s 1 -d 0xFFFF
 * access to non existing string argv[x] creates exception
 * 0x9000000021000000 0x9000000028000000
 */

bool_t
ip30_mem_poke(__uint32_t argc, char **argv)
{
   int loop;
   char	testStr[50], startStr[50], endStr[50];
   short i, bad_arg = 0,nextStart, j;
   long long regStartAddr, regEndAddr, *readAddr64, mask, readAddress64;
   
   char *readAddr8;
   short *readAddr16;
	int *readAddr32;
	char *errStr;
	bool_t addrArgDefined = 0;
	bool_t dataArgDefined = 0;
	__uint64_t memTop; 
	long long data;
	bool_t foundColon = 0; /*false*/
	unsigned int loopCount;
   int dataSize;
	
	/*check for nil args*/
  	if ( argc == 1 ) {
		msg_printf(INFO, "Args -a is required\n");
		msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
		return 1;	
	}
	
	/* get the args */
	argc--; argv++;
   loopCount = 1;
   strcpy(testStr, "0x0:0xc");
   mask = ~0; 
   dataSize = 2; /*32 bit operation*/

   while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
      switch (argv[0][1]) {    
       	case 'a':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex address argument required\n");
					msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
					return 1;	
				}

				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Hex address argument too long\n");
					msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
					return 1;	
				}

	   		if (argv[0][2]=='\0') {
		 			strcpy(&(testStr[0]), &(argv[1][0]));
					argc--; argv++;
		 			addrArgDefined = 1;
	    		} else {
					strcpy(&(testStr[0]), &(argv[0][2]));
				}
	 		break;
         case 'l':
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex loop argument required\n");
					msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
					return 1;	
				}
				
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Loop argument too long\n");
					msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
					return 1;	
				}
            
				if (argv[0][2]=='\0') {
					errStr = atob(&argv[1][0], &(loop));
					loopCount = loop;
					argc--; argv++;
					/*if converted string is not an legal hex number*/	
					if (errStr[0] != '\0') {	
						msg_printf(INFO, "Hex loop argument required\n");
						msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
						return 1;	
           		} 
            } else {
					errStr = atob(&argv[0][2], &(loop));
            }
	 		break;	 
	 		case 's':
				
				/*if argument string is null*/	
				if (argc < 2) {
					argc--; argv++; 
					msg_printf(INFO, "Hex size argument required\n");
					msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
					return 1;	
				}
            
				/*check for string length*/
				if (strlen(argv[1])>ArgLength) {
					msg_printf(INFO, "Size argument too long\n");
					msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
					return 1;	
				}
				
				if (argv[0][2]=='\0') {
					errStr = atob(&argv[1][0], &(dataSize));
					argc--; argv++; 
					/*if converted string is not an legal hex number*/	
					if (errStr[0] != '\0') {	
						msg_printf(INFO, "Hex size argument required\n");
						msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
						return 1;	
           		} 
					/*if converted string is not the right size*/	
					if (dataSize > 3) {	
						msg_printf(INFO, "Hex size arguemnt 0..3 required\n");
						msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
						return 1;	
           		} 
				} else {
					errStr = atob(&argv[0][2], &(dataSize));
            }
		break;
		case 'd':
			/*if argument string is null*/	
			if (argc < 2) {
				argc--; argv++; 
				msg_printf(INFO, "Hex data argument required\n");
				msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
				return 1;	
			}
           
			/*check for string length*/
			if (strlen(argv[1])>ArgLengthData) {
				msg_printf(INFO, "Data argument too big, 0..0xFFFFFFFFFFFFFFFF required\n");
				msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
				return 1;	
			}
			
			if (argv[0][2]=='\0') {
				errStr = atob_L(&argv[1][0], &(data));
				argc--; argv++; 
				/*if converted string is not an legal hex number*/	
				if (errStr[0] != '\0') {	
					msg_printf(INFO, "Hex data argument required\n");
					msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
					return 1;	
        		} 
			} else {
				errStr = atob_L(&argv[0][2], &(data));
			}
			dataArgDefined = 1;
	 break;
	 default: 
		msg_printf(INFO, "Illegal option\n");
		msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
		bad_arg++; 
		return 1;	
	    }
	    argc--; argv++;
   	}

	/*set memTop*/
	switch (dataSize) {
		case EightBit:
			memTop = cpu_get_memsize()-1;
		break;
		case SixteenBit:
			memTop = cpu_get_memsize()-2;
		break;
		case ThirtyTwoBit:
			memTop = cpu_get_memsize()-4;
		break;
		case SixtyFourBit:
			memTop = cpu_get_memsize()-8;
		break;
		default:
			memTop = cpu_get_memsize()-4;
	}

	msg_printf(DBG, "Memory Range 0x%x:0x%x\n", LowMemRead,  memTop);
   
	if (!addrArgDefined) {   
      msg_printf(INFO,"Args -a is required\n");
		msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data> \n");
		return 1;
	}
	
	if (!dataArgDefined) {   
      msg_printf(INFO,"Args -d is required\n");
		msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
		return 1;
	}
	
	/*look for colon and null second string*/
   i = 0;
   do {
		if (testStr[i] == ':') {
			foundColon = 1;
			break;	
		}
      i++;
   } while (testStr[i-1] != '\0');

	if (!foundColon) {
		msg_printf(INFO, "Illegal argument -  address (1)\n");
		msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
		return 1;
	} else {
		msg_printf(DBG, "Found Colon :\n");
	}

	/*look for null string 2*/	
	if (testStr[i+1] == '\0') {
		msg_printf(INFO, "Illegal argument -  second address\n");
		msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
		return 1;	
	}
	
	/*parse addr agrs*/
   i = 0;
   do {
     startStr[i] = testStr[i];	     
      i++;
		/*look for colon*/
		if (i > 17) {
			msg_printf(INFO, "Illegal argument -  address (2)\n");
			msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
			return 1;
		}
   } while (testStr[i] != ':');
	
	startStr[i] = '\0';
   i++;

   j = 0;
   do {
     endStr[j] = testStr[i];	    
      i++; j++;
   } while (testStr[i] != NULL);
   endStr[j] = '\0'; 

   msg_printf(DBG,"Addr input string = %s; Start Address str = %s; End Addr str = %s\n", testStr, startStr, endStr);

   /*get hex addr*/
   errStr = atob_L(startStr, &(regStartAddr));
	/*if converted string is not an legal hex number*/	
	if (errStr[0] != '\0') {	
		msg_printf(INFO, "Hex address argument required\n");
		msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
		return 1;	
  	} 
   errStr = atob_L(endStr, &(regEndAddr));
	/*if converted string is not an legal hex number*/	
	if (errStr[0] != '\0') {	
		msg_printf(INFO, "Hex address argument required\n");
		msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
		return 1;	
  	} 
   
	/*end address should be > start*/
	if (regStartAddr > regEndAddr) {
		msg_printf(INFO, "End address must be greater than start address\n");
		msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
		return 1;
	}

	/*is address in range*/
	if (   (regStartAddr < LowMemRead)  || (regEndAddr > memTop)  )  {
		msg_printf(INFO, "Memory argument range error. Valid Range 0x%x:0x%x\n", LowMemRead,  memTop);
		return 1;
	}

	/*check memory allignment*/
	if (  MemoryAlligned(regStartAddr, regStartAddr, dataSize)  ) {
		msg_printf(INFO, "Misaligned Memory Access\n");
		msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
		return 1;
	}

	/*
	check memory allignment
	if (  CheckDataArg(data, dataSize)  ) {
		msg_printf(INFO, "Usage: ip30_mem_poke -a<start:end> -l<loopcount> -s <size> -d <data>\n");
		return 1;
	}
	*/
	
	msg_printf(DBG,"Start Addr %x; End Address %x\n", regStartAddr, regEndAddr);

	/*Memory after 16K (0x400) is mapped at 0x20000000*/
	for (i = 1; i <= loopCount; i++) {
		switch (dataSize) {
			 case EightBit:
	    		msg_printf(INFO,"Starting Memory Write (8)\n");
            for (readAddr8 = (char *)regStartAddr; readAddr8 <= (char *)regEndAddr; readAddr8++) {     
					if ((long long)readAddr8 < 0x4000) readAddress64 = (long long)readAddr8; 
					else readAddress64 = (long long)readAddr8 + PHYS_RAMBASE;
					PIO_REG_WR_8(readAddress64, (char)mask,(char)data); 
	       		msg_printf(INFO,"Write Addr (8) 0x%x\n", readAddress64);     
            } 
         break;
	 		case SixteenBit:
	   		msg_printf(INFO,"Starting Memory Write (16)\n");
            for (readAddr16 = (short *)regStartAddr; readAddr16 <= (short *)regEndAddr; readAddr16++) {     
					readAddress64 = ((long long)readAddr16 < 0x4000) ? (long long)readAddr16 : (long long)readAddr16 + PHYS_RAMBASE;
	       		PIO_REG_WR_16(readAddress64, (short)mask, (short)data);
	       		msg_printf(INFO,"Write Addr (16) 0x%x\n", readAddress64);     
            } 
         break;
         case SixtyFourBit:
	    		msg_printf(INFO,"Starting Memory Write (64)\n");
            for (readAddr64 = (long long *)regStartAddr; readAddr64<= (long long *)regEndAddr; readAddr64++) {               
					readAddress64 = ((long long)readAddr64 < 0x4000) ? (long long)readAddr64 : (long long)readAddr64 + PHYS_RAMBASE;
               PIO_REG_WR_64(readAddress64, mask, data);
	       		msg_printf(INFO,"Write Addr (64) 0x%x\n", readAddress64);
            } 
         break;
         case ThirtyTwoBit:
         default:
	    		msg_printf(INFO,"Starting Memory Write (32)\n");
            for (readAddr32 = (int *)regStartAddr; readAddr32 <= (int *)regEndAddr; readAddr32++) {         
					readAddress64 = ((long long)readAddr32 < 0x4000) ? (long long)readAddr32 : (long long)readAddr32 + PHYS_RAMBASE;
	       		PIO_REG_WR_32(readAddress64, (int)mask, (int)data);
	       		msg_printf(INFO,"Write Addr (32) 0x%x\n", readAddress64);
            } 
      }
   }

   return 0;
	
}
