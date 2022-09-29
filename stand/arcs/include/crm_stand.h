#ifndef __CRM_STAND_H__
#define __CRM_STAND_H__

/*
 * crm_stand.h
 *
 *    Standalone graphics header for Moosehead
 *
 */

/*
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

#ident "$Revision: 1.2 $"

/************************************************************************/
/* Temporary Space							*/
/************************************************************************/

#define CRM_RE_BASE_ADDRESS		0x15000000
#define CRM_CPU_INTERFACE_BASE_ADDRESS	0x14000000

#define CRM_REFIFOWAIT_TRITON133	20
#define CRM_REFIFOWAIT_TRITON180	10

#if 0
#define PROM_FB_DLIST1_BASE     K1_TO_PHYS(PROM_TILE_BASE+(PROM_TILE_CNT*0x00010000))
#define PROM_FB_DLIST2_BASE     K1_TO_PHYS(PROM_FB_DLIST1_BASE+0x00008000)
#define PROM_FB_TILE_BASE       K1_TO_PHYS(PROM_FB_DLIST1_BASE+0x00010000)
#else
#define PROM_FB_DLIST1_BASE     0x00d60000
#define PROM_FB_DLIST2_BASE     0x00d68000
#define PROM_FB_TILE_BASE       0x00d70000
#endif

#define IDE_FB_DLIST1_BASE      	0x01500000
#define IDE_FB_DLIST2_BASE      	0x01504000
#define IDE_SHADOW_FB_DLIST1_BASE	0x01508000
#define IDE_SHADOW_FB_DLIST2_BASE	0x0150c000
#define IDE_FB_TILE_BASE		0x01510000

#define PROM_TEST_TILE             0x00d50000

/* Following two constant defined the PROM FrameBuffer region   */
#define PROM_FB_TILE_START         0x00d50000
#define PROM_FB_TILE_LIMIT         0x01000000

/***********************************/
/* Definitions needed for crm_tp.c */
/***********************************/

#define CURSOR_XOFF	3
#define CURSOR_YOFF	1

/*******************************************/
/* Definitions for the CRIME CPU Interface */
/*******************************************/

typedef struct crm_cpu_interface
{
   unsigned long long crimeID;
   unsigned long long crimeControl;
   unsigned long long crimeIntStatus;
   unsigned long long crimeIntEnable;
   unsigned long long crimeSwInterrupt;
   unsigned long long crimeHwInterrupt;
   unsigned long long crimeWatchdogTimer;
   unsigned long long crimeCrimeTime;
   unsigned long long crimeCPUErrorAddr;
   unsigned long long crimeVICEErrorStatus;
   unsigned long long crimeVICEErrorEnable;
   unsigned long long crimeVICEErrorAddr;
} CrimeCPUInterface;

/*********************************************/
/* Definitions needed for setting up RE TLBs */
/*********************************************/

#define CRM_SETUP_TLB_A		0x01
#define CRM_SETUP_TLB_B		0x02
#define CRM_SETUP_TLB_C		0x04

/**********************************/
/* Definitions for monitor timing */
/**********************************/

#define CRM_EDID_ET2_1280_1024_75_MASK  0x01
#define CRM_EDID_ET2_1024_768_75_MASK   0x02
#define CRM_EDID_ET2_1024_768_60_MASK   0x08
#define CRM_EDID_ET2_800_600_75_MASK    0x40
#define CRM_EDID_ET2_800_600_72_MASK    0x80

#define CRM_EDID_ET1_800_600_60_MASK    0x40
#define CRM_EDID_ET1_640_480_75_MASK    0x04
#define CRM_EDID_ET1_640_480_72_MASK    0x08
#define CRM_EDID_ET1_640_480_60_MASK    0x20


/********************************/
/* Definitions for Visual Tests */
/********************************/

#define CRM_VISPAT1_COLORFLAG_BW	0x00000001
#define CRM_VISPAT1_COLORFLAG_RED	0x00000002
#define CRM_VISPAT1_COLORFLAG_GREEN	0x00000004
#define CRM_VISPAT1_COLORFLAG_BLUE	0x00000008

#define CRM_VISPAT1_DIRFLAG_LTOR	0x00000001
#define CRM_VISPAT1_DIRFLAG_RTOL	0x00000002
#define CRM_VISPAT1_DIRFLAG_TTOB	0x00000004
#define CRM_VISPAT1_DIRFLAG_BTOT	0x00000008

/*******************************/
/* Definition for Busy Waiting */
/*******************************/

#define CRM_BUSY_WAIT_PERIOD		128

#endif /* !__CRM_STAND_H__ */
