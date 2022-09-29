/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * apf_diag.h - Adobe Pixel Formatter defines for diags
 */

#define GIO_SLOT_0_ADDRESS 0x1f400000
#define GIO_SLOT_1_ADDRESS 0x1f600000

#define PF_INTERRUPT_REG_OFFSET     0x1fe00     /* write to set */
#define PF_CAUSE_REG_OFFSET	0x1fe04     /* write to clear */
#define PF_INTERRUPT_MASK	0x80008000

#define PRODUCT_ID_MASK     0xff	/* mask rest of GIO id */
#define PF_PRODUCT_ID		0x06	

/*
  revisions:
   0 - all pre-released versions of the PF board and ROMs (old sload, 32k RAM)
   1 - PF3 with new ROM (new sload, 32k RAM)
   2 - PF4 with new ROM (new sload, 32k RAM + 32k unshared RAM)
*/

#define GET_PRODUCT_REVISION(x) ( ((x) >> 8) & 0xff)

