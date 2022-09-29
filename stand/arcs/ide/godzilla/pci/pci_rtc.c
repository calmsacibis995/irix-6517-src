/*  
 * pci_rtc.c : real time clock Tests
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

#ident "ide/godzilla/pci/pci_rtc.c:  $Revision: 1.6 $"


#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/ds1687clk.h"
#include "sys/RACER/IP30nvram.h"
#include "sys/PCI/ioc3.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_rtc.h"
#include "d_godzilla.h"
#include "d_prototypes.h"
#include "d_frus.h"
#include "d_prototypes.h"

/*
RTC is a device connnected to the ioc3 that is byte addressable via the ioc3
rtc and data registers. These addresses in IP30.h are K1 converted so are 
written to directly.
*/
   
bool_t
pci_rtc(__uint32_t argc, char **argv)
{

	int result=0;
	msg_printf(INFO,"Starting RTC Functional Tests\n");

	d_errors = 0;
	result = rtcBattery();
	if (!result ) rtcTickTock();
	if (!result ) rtcRAM();
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(result, "Real Time Clock", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_PCI_0005], result );
}

/*Verify clock is ticking*/
bool_t rtcTickTock(void) {
   bool_t result = 0;
   char startTime=0, endTime=0;
   short i;

   msg_printf(INFO, "RTC Timer Test\n");
   startTime = rtcRead(RTC_SEC);
   if (startTime >= 54 ) {
      delay (1000); /*wait for clock to go to next minute*/
   }

   startTime = rtcRead(RTC_SEC);
   if (startTime >= 54) {
      msg_printf(INFO,"RTC Clock did not roll over\n");
      msg_printf(INFO,"RTC Clock Timer Test Failed\n");
   }

   startTime = rtcRead(RTC_SEC);
   delay(500); /*5 secs*/
   endTime = rtcRead(RTC_SEC);
   if ( ((endTime - startTime) < 4) || ((endTime - startTime) ) > 6) {
      result = -1;
      msg_printf(INFO,"RTC Clock Timer Test Failed\n");
   }
   msg_printf(DBG, "Start Time = %x, End Time = %x\n", startTime,endTime);
   return (result);
}

/*Test 50 user ram bytes*/
bool_t rtcRAM(void) {
   bool_t result = 0;
   char tmp;
   short i;
   char ramStorage[50];

   msg_printf(INFO, "RTC NVRAM Test\n");
   /*save nvram values e.g enet addr*/
   for (i = 0; i <= 49; i++) 
      ramStorage[i] = rtcRead(NVRAM_REG_OFFSET+i);  
   
   for (i = 0; i <= 49; i++) rtcWrite(NVRAM_REG_OFFSET+i,0x00);  
   
   for (i = 0; i <= 49; i++) {
      tmp = rtcRead(NVRAM_REG_OFFSET+i);
      if (tmp) { /*failed*/
	result = -1;
	msg_printf(INFO,"RTC RAM DATA 0  Test Failed\n");
	break;
      }
   } 

   for (i = 0; i <= 49; i++) rtcWrite(NVRAM_REG_OFFSET+i,0xFF);  
   
   for (i = 0; i <= 49; i++) {
      tmp = rtcRead(NVRAM_REG_OFFSET+i);
      if (tmp != 0xFF) { /*failed*/
	result = -1;
	msg_printf(INFO,"RTC RAM DATA 1 Test Failed\n");
	break;
      }
   } 
 
 /*Restore ram*/
 for (i = 0; i <= 49; i++) 
    rtcWrite(NVRAM_REG_OFFSET+i,ramStorage[i]);  

 return (result);

}

void rtcWrite (char reg, char value) {
   __uint32_t origValue, writeValue, old;
   
   /*set chip into slow dallas mode. Change clock CMD pulse width*/
   old = slow_ioc3_sio();

   *RTC_ADDR_PORT = RTC_REG_NUM(reg);
   *RTC_DATA_PORT = value;
   
   /*restore original value*/
   restore_ioc3_sio(old);
}

char rtcRead (char reg) {
   char readValue;
   __uint32_t origValue, writeValue, old;

   /*set chip into slow dallas mode.*/
   old = slow_ioc3_sio();

   *RTC_ADDR_PORT = RTC_REG_NUM(reg);
   readValue = *RTC_DATA_PORT;
   
   restore_ioc3_sio(old);

   return readValue;
}

/*Check if lithium battery is OK*/
bool_t rtcBattery(void) {
   char regD;

   msg_printf(INFO, "Battery voltage check\n");
   regD = rtcRead(RTC_CTRL_D);
   /*look at vrt bit in reg D. if 0 battery is low*/
   if (regD & 0x80) return 0;
   else {
      msg_printf(INFO, "Lithium auxillary battery is exhasted\n");
      msg_printf(INFO, "NVRAM may be corrupted\n");
      return -1;
   }

}
