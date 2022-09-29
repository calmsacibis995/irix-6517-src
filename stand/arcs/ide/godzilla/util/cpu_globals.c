/*
 * cpu_globals.c - Global variables
 *
 * Copyright 1997, Silicon Graphics, Inc.
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
 *	These functions are used throughout the godzilla architecture ide.
 *	report_pass_or_fail: at the end of each test, returns 0/1
 *	pio_reg_mod_32/64: modify the content of a PIO register and
 *				return the incremental # of errors
 */

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"
#include "d_frus.h"
#include "d_prototypes.h"

_Test_Info CPU_Test_err[] = {

        { "Bridge Error",                              "BDG001" },
        { "Bridge Interrupt",                          "BDG002" },
        { "Bridge Bridge Internal/External RAM",       "BDG003" },
        { "Bridge Read-Write Register",                "BDG004" },
        { "Bridge Check Out",                          "BDG101" },
        { "Bridge Reset",                              "BDG105" },

        { "ECC",                                       "ECC001" },

        { "ENET Tests",                                "ETH001" },
        { "ENET External Loopback",                    "ETH002" },
        { "ENET Stress",                               "ETH003" },

        { "Heart, Xbow & Bridge Check Out",            "HXB001" },
        { "Heart, Xbow & Bridge Save Register",        "HXB002" },
        { "Heart, Xbow & Bridge Restore Register",     "HXB003" },

        { "Heart Interrupt Registers",                 "HRT001" },
        { "Heart Miscellaneous Registers",             "HRT002" },
        { "Heart PIO Access",                          "HRT003" },
        { "Heart PIU Access",                          "HRT004" },
        { "Heart Read-Write Register",                 "HRT005" },
        { "Heart Check Out",                           "HRT101" },
        { "Heart Reset",                               "HRT104" },

        { "Heart & Bridge Check Out",                  "HAB101" },
	{ "unused", ""},
	{ "unused", ""},
        { "Heart & Bridge Reset",                      "HAB106" },

        { "IOC3 DUART Read-Write Register",            "IOC301" },
        { "IOC3 Read-Write Register",                  "IOC302" },
        { "IOC3 SRAM",                                 "IOC303" },

        { "RTC Read-Write Register",                   "RTC001" },

        { "Memory",                                    "MEM001" },

        { "PCI Configuration Read",                    "PCI001" },
        { "PCI Configuration Write",                   "PCI002" },
        { "PCI Interrupt",                             "PCI003" },
        { "PCI Memory Space",                          "PCI004" },
        { "PCI Parallel Port",                         "PCI005" },
        { "PCI Real Time Clock",                       "PCI006" },
        { "PCI Serial Port",                           "PCI007" },
        { "PCI Parallel Port Loopback",                "PCI008" },
        { "PCI Shoebox All Tests",                     "PCI101" },

        { "RAD All Tests",                             "RAD001" },
        { "RAD Configuration Space",                   "RAD002" },
        { "RAD RAM",                                   "RAD003" },
        { "RAD Status DMA",                            "RAD103" },

        { "SCSI QLOGIC Controller",                    "SCS101" },
        { "SCSI Read-Write Register",                  "SCS102" },

        { "Heart/Xbow Bad LLP",                        "HXB101" },

        { "Xbow Access",                               "XBO101" },
        { "Xbow Read-Write Register",                  "XBO102" },

        { NULL,                                        NULL     }
};
