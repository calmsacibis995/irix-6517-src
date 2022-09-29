/* $Header: /proj/irix6.5.7m/isms/irix/kern/io/adphim/RCS/himdiopt.c,v 1.2 1996/01/16 03:54:52 ack Exp $ */
/***************************************************************************
*                                                                          *
* Copyright 1993 Adaptec, Inc.,  All Rights Reserved.                      *
*                                                                          *
* This software contains the valuable trade secrets of Adaptec.  The       *
* software is protected under copyright laws as an unpublished work of     *
* Adaptec.  Notice is for informational purposes only and does not imply   *
* publication.  The user of this software may make copies of the software  *
* for use with parts manufactured by Adaptec or under license from Adaptec *
* and for no other use.                                                    *
*                                                                          *
***************************************************************************/

/***************************************************************************
*
*  Module Name:   HIMDIOPT.C
*
*  Description:
*                 Codes specific to initialize HIM configured for driver
*                 OPTIMA mode are defined here. It should only be included 
*                 if HIM is configured for driver with OPTIMA mode. 
*                 It can be thrown away after HIM get initialized.                 
*
*  Programmers:   Paul von Stamwitz
*                 Chuck Fannin
*                 Jeff Mendiola
*                 Harry Yang
*    
*  Notes:         NONE
*
*  Entry Point(s):
*
*  Revisions -
*
***************************************************************************/

#include "him_scb.h"
#include "him_equ.h"
#include "seq_off.h"

/*********************************************************************
*
*   Ph_GetOptimaConfig routine -
*
*   This routine initializes the members of the ha_Config and ha_struct
*   structures which are specific to OPTIMA mode only.
*
*  Return Value:  None
*                  
*  Parameters:    config_ptr
*
*  Activation:    PH_GetDrvrConfig
*                  
*  Remarks:                
*                  
*********************************************************************/

void Ph_GetOptimaConfig (cfp_struct *config_ptr)
{
   /* The maximum no of SCBs can actually be 254 only */
   if (config_ptr->Cf_NumberScbs > MAX_SCBS - 1)
      config_ptr->Cf_NumberScbs = MAX_SCBS - 1;

   /* always enable MultiTaskLun for optima mode */
   config_ptr->CFP_MultiTaskLun = 1;
   config_ptr->Cf_HimDataSize = Ph_CalcOptimaSize(config_ptr->Cf_NumberScbs);
}



/*********************************************************************
*
*   PH_CalcOptimaSize routine -
*
*   This routine will calculate the size of the HIM data structure
*   according to the number of SCBs
*
*  Return Value:  size of HIM data structure
*
*  Parameters:    unsigned int number_scbs
* 
*  Activation:    PH_CalcDataSize
*
*  Remarks:
*
*********************************************************************/

UWORD Ph_CalcOptimaSize (UWORD number_scbs)
{
   UWORD size;

   hsp_struct *ha_ptr = (hsp_struct *) 0;


   /* The maximum no of SCBs can actually be 254 only */
   if (number_scbs > MAX_SCBS - 1)
      number_scbs = MAX_SCBS - 1;

   /*
    *  Calculate Size of (ha_ptr) ====> (ha_ptr->max_additional)
    *                              TO
    *
    */

   size = (WORD) ((DWORD) ha_ptr->max_additional - ((DWORD) ha_ptr));

   /*
    *  Calculate Size of AREA from: (ha_ptr->max_additional) ====>
    *
    *                         to  : the End of "scb_ptr_array"
    *
    *          (+) 4 * Number_of_Scbs  ===> for active_ptr_array
    *          (+) 1 * Number_of_Scbs  ===> for free_stack
    *          (+) 1 * Number_of_Scbs  ===> for done_stack
    *
    *          (+) 4 * Number_of_Scbs  ===> for scb_ptr_array
    *
    *          (+) 256                 ===> Needed inorder to align the start
    *                                       of qin_ptr_array on a 256 byte
    *                                       boundary
    */ 

   size += (4+1+1+4) * (number_scbs + 1) + 256; 

   /*
    *  Add the area designated for the following:
    *
    *
    *          (+) 256   ===> for qin_ptr_array
    *          (+) 256   ===>     qout_ptr_array
    *          (+) 256   ===>     busy_ptr_array
    */

   size += 256 + 256 + 256;


   return( size );
}



