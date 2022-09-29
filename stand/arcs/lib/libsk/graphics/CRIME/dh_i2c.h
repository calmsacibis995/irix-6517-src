/*
 * dh_i2c.h
 *
 * Copyright 1996 - 1998, Silicon Graphics, Inc.
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

#ident "$Revision: 1.1 $"

/* Defines for the Moosehead Dual Channel board */

#define DH8574_OWNADR  0x7e   /* I2C addr of O2 Dual-Monitor board */
#define DH8574_VCORNG  0x80   /* Frequency range:  */
                              /* 1=(100-136MHz), 0=(50-100MHz). */
                              /* Also mux select for board id */
#define DH8574_ID      0x60   /* Multiplexed 4-bit board identification */
                              /* MSBs when VCORNG=1, LSBs when VCORNG=0 */
#define DH8574_PASS    0x10   /* Diagnostic: Test result */
#define DH8574_SETPASS 0x08   /* Diagnostic: Initialize test, same bit as SD */
#define DH8574_SD      0x08   /* Diagnostic: Test mode load, serial data */
#define DH8574_SCK     0x04   /* Diagnostic: Test mode load, serial clock */
#define DH8574_DMODE   0x03   /* Display mode: 3=Normal, 2=BothDirect, */
                                  /* 1=RightDirect, 0=LeftDirect */
#define DH_RDMASK      (DH8574_PASS | DH8574_ID)

/* Shifting and masking for writing fields to DCD control register */
#define DH_DMODE(x)     ((x)&3)
#define DH_SCK(x)       (((x)&1)<<2)
#define DH_SD(x)        (((x)&1)<<3)
#define DH_SETPASS(x)   (((x)&1)<<3)    /* same as SD */
#define DH_VCORNG(x)    (((x)&1)<<7)

/* Shifting and masking for reading fields from DCD control register */
#define DH_X_DMODE(x)   ((x)&3)         /* Extract Display Mode */
#define DH_X_PASS(x)    (((x)>>4)&1)    /* Extract Pass/Fail bit */
#define DH_X_ID(x)      (((x)>>5)&3)    /* Extract Board ID */
