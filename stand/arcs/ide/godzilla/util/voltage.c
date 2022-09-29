/*
 * voltage.c - utility to change the voltage
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
 *
 */
#ident "ide/godzilla/util/utility.c: $Revision: 1.8 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/RACER/racermp.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"
#include "d_prototypes.h"

/*
 * Following strcture and constants are used in util/voltage.c
 */
typedef union {
	struct _voltage {
		unsigned fanspeed	: 2;
		unsigned pwr_suply_hi 	: 1;
		unsigned pwr_suply_lo 	: 1;
		unsigned vterm_hi 	: 1;
		unsigned vterm_lo 	: 1;
		unsigned cpu_hi 	: 1;
		unsigned cpu_lo 	: 1;
	} bits;
	uchar_t val;
} d_voltage;

#define	CHG_VOLT_CPU		0
#define	CHG_VOLT_VTERM		1
#define	CHG_VOLT_PWR_SUPLY	2
#define	CHG_VOLT_ALL		3

#define	CHG_VOLT_LOW		0
#define	CHG_VOLT_NOMINAL	1
#define	CHG_VOLT_HIGH		2

int
chg_volt(int argc, char **argv)
{
	int module = CHG_VOLT_NOMINAL;
	int level = CHG_VOLT_NOMINAL;
	d_voltage vol_value = {0};
	int bad_arg = 0;
	int fan = -1;

	msg_printf(DBG, "Voltage margining utility.\n");

        argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	    switch (argv[0][1]) {
		case 'm':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], &module);
				argc--; argv++;
			} else {
				atob(&argv[0][2], &module);
			}
			break;
		case 'l':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], &level);
				argc--; argv++;
			} else {
				atob(&argv[0][2], &level);
			}
			break;
		case 'f':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], &fan);
				argc--; argv++;
			} else {
				atob(&argv[0][2], &fan);
			}
			break;
		default: 
			bad_arg++; break;
	    }
	    argc--; argv++;
   	}

   	if ( bad_arg || argc || (module > CHG_VOLT_ALL) ||
	    (level > CHG_VOLT_HIGH) ) {
	    msg_printf(ERR, "Usage: \n\
	    	-m <module> [0|1|2|3]\n\
			0 - CPU, 1 - VTERM, 2 - POWER SUPPLY, 3 - ALL\n\
	    	-l <level>  [0|1|2]\n\
			0 - LOW, 1 - NOMINAL, 2 - HIGH \n\
		-f <level>  [0|1|2]\n\
			0 - HW, 1 - FAST, 2 - SLOW\n");
	    return(1);
   	}

	vol_value.bits.pwr_suply_lo = vol_value.bits.pwr_suply_hi = 1;

	/* If -f is not used, honor MPCONF.
	 */
	if (fan == -1) {
		if (MPCONF->fanloads >= 1000)
			vol_value.bits.fanspeed = 1;
		else if (MPCONF->fanloads <= 1)
			vol_value.bits.fanspeed = 2;
		else
			vol_value.bits.fanspeed = 0;
	}
	else
		vol_value.bits.fanspeed = (fan >= 0 && fan <= 2) ?
			fan : 0;

	switch (module) {
	   case CHG_VOLT_CPU:
		switch (level) {
		   case CHG_VOLT_LOW:
			vol_value.bits.cpu_lo = 1;
			break;
		   case CHG_VOLT_NOMINAL:
			vol_value.bits.cpu_lo = vol_value.bits.cpu_hi = 0;
			break;
		   case CHG_VOLT_HIGH:
			vol_value.bits.cpu_hi = 1;
			break;
		}
		break;
	   case CHG_VOLT_VTERM:
		switch (level) {
		   case CHG_VOLT_LOW:
			vol_value.bits.vterm_lo = 1;
			break;
		   case CHG_VOLT_NOMINAL:
			vol_value.bits.vterm_lo = vol_value.bits.vterm_hi = 0;
			break;
		   case CHG_VOLT_HIGH:
			vol_value.bits.vterm_hi = 1;
			break;
		}
		break;
	   case CHG_VOLT_PWR_SUPLY:
		switch (level) {
		   case CHG_VOLT_LOW:
			vol_value.bits.pwr_suply_lo = 0;
			break;
		   case CHG_VOLT_NOMINAL:
			vol_value.bits.pwr_suply_lo = vol_value.bits.pwr_suply_hi = 1;
			break;
		   case CHG_VOLT_HIGH:
			vol_value.bits.pwr_suply_hi = 0;
			break;
		}
		break;
	   case CHG_VOLT_ALL:
		switch (level) {
		   case CHG_VOLT_LOW:
			vol_value.bits.cpu_lo = 1;
			vol_value.bits.vterm_lo = 1;
			vol_value.bits.pwr_suply_lo = 0;
			break;
		   case CHG_VOLT_NOMINAL:
			vol_value.bits.cpu_lo = vol_value.bits.cpu_hi = 0;
			vol_value.bits.vterm_lo = vol_value.bits.vterm_hi = 0;
			vol_value.bits.pwr_suply_lo = vol_value.bits.pwr_suply_hi = 1;
			break;
		   case CHG_VOLT_HIGH:
			vol_value.bits.cpu_hi = 1;
			vol_value.bits.vterm_hi = 1;
			vol_value.bits.pwr_suply_hi = 0;
			break;
		}
		break;
	}

	msg_printf(DBG, "module %d; level %d; vol_value 0x%x\n",
		module, level, (uchar_t)vol_value.val);

	/* Set the register in IOC3 to change the voltage */
	*IP30_VOLTAGE_CTRL = (uchar_t)vol_value.val;

	msg_printf(DBG, "Voltage margining done\n");

	return(0);
}
