/*
 * crm_gfx.h
 *
 *    Supplemental graphics header for Moosehead PROM/IDE
 *
 */

/*
 * Copyright 1998, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
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

/* PLEASE NOTE:
 * 
 * This header file has been created to avoid inconsistencies in the O2 stand
 * ism stream.  Several important data structures and defines existed only
 * in the gfx/kern/sys path which is not part of the O2 PROM or O2 IDE patch
 * streams.
 *
 * In order to avoid complications, affected variables/structures have been
 * reproduced here with slightly different names.  Stand ism .c files will
 * be altered to use these new names.
 *
 */

/* Crime video timing types. */

typedef enum {
    CRM_PROM_VT_NULL,

    CRM_PROM_VT_640_480_60,

    CRM_PROM_VT_800_600_60,
    CRM_PROM_VT_800_600_72,

    CRM_PROM_VT_1024_768_60,
    CRM_PROM_VT_1024_768_70,
    CRM_PROM_VT_1024_768_75,

    CRM_PROM_VT_1280_1024_48,
    CRM_PROM_VT_1280_1024_50,
    CRM_PROM_VT_1280_1024_60,
    CRM_PROM_VT_1280_1024_72,
    CRM_PROM_VT_1280_1024_75,

    CRM_PROM_VT_EXTRA_1,

    CRM_PROM_VT_1280_492_120,

    CRM_PROM_VT_EXTRA_2,

    CRM_PROM_VT_1600_1024_50,

    CRM_PROM_VT_UNKNOWN
} crime_timing_table_t;

#define CRM_PROM_VT_SIZE CRM_PROM_VT_UNKNOWN

/*****************************************/
/* Definitions for 7of9 (DMI2) flatpanel */
/*****************************************/


#define PANEL_DMI2_7of9             0x05
#define MONITORID_7of9              132

/* Master Control Unit Register */

#define DMI2_CntlRegAddr         0x70
#define DMI2_PowersaveMask       0xfb
#define DMI2_PowersaveOn         0x0
#define DMI2_PowersaveOff        0x4



/* Xicor X9221 Dual EEPotentiometer */

#define X9221_Addr               0x50

/* Xicor X9221 Instruction Defines */

#define X9221_ReadWCR            0x90
#define X9221_WriteWCR           0xa0
#define X9221_ReadReg            0xb0
#define X9221_WriteReg           0xc0
#define X9221_XferReg2WCR        0xd0
#define X9221_XferWCR2Reg        0xe0
#define X9221_GlobalXferReg2WCR  0x10
#define X9221_GlobalXferWCR2Reg  0x80
#define X9221_IncDecWiper        0x20

/* DMI2 Tube Defines (Xicor X9221 Potentiometer select) */

#define DMI2_CoolTube            0x4
#define DMI2_WarmTube            0x0

/* Xicor X9221 Registers */

#define X9221_Reg0               0x0
#define X9221_Reg1               0x1
#define X9221_Reg2               0x2
#define X9221_Reg3               0x3
#define X9221_DefaultReg         X9221_Reg0

/* Xicor X9221 Potentiometer Limits */

#define X9221_PotMin             0x0
#define X9221_PotMax             0x40


/**************************/
/* Additional GBE Defines */
/**************************/


#define GBE_POWERSAVE_MASK	0x7d
#define GBE_POWERSAVE_ON	0x2
#define GBE_POWERSAVE_OFF	0x0



