/* $Header: /proj/irix6.5.7m/isms/stand/arcs/lib/libsk/io/adphim/RCS/him_init.c,v 1.1 1997/06/05 23:38:37 philw Exp $ */
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


/****************************************************************************
*
*  Module Name:   HIM_INIT.C
*
*  Description:
*                 Codes common to HIM at initialization are defined here. 
*                 It should always be included independent of configurations
*                 and modes of operation. These codes can be thrown away 
*                 after HIM initialization.
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
*     PH_GetNumOfBuses - Return number of PCI buses present in system
*     PH_FindHA        - Look for Host Adapter at Bus/Device "slot"
*     PH_GetConfig     - Initialize HIM data structures
*     PH_InitHA        - Initialize Host Adapter
*
*  Revisions -
*
****************************************************************************/

#ifndef SIMHIM

#include "sys/param.h"
#include "sys/sbd.h"
#include "him_scb.h"
#include "him_equ.h"
#include "seq_off.h"
#include "sequence.h"
#include "him_rel.h"
#include "sys/cmn_err.h"

#ifdef _DRIVER
#include "drvr_mac.h"
#endif /* _DRIVER */

extern char adp_verbose;

/*
 ****************************************************************************
 *
 * More initialization of the cfp struct.
 *
 ****************************************************************************
 */
void PH_GetConfig (cfp_struct *config_ptr)
{
   register AIC_7870 *base;
   UBYTE hcntrl_data;

   base = config_ptr->CFP_Base;

   config_ptr->CFP_AdapterIdH = INBYTE(AIC7870[DEVID0]);
   config_ptr->CFP_AdapterIdL = INBYTE(AIC7870[DEVID1]);
   config_ptr->Cf_ReleaseLevel = REL_LEVEL;  /* Current Release Level    */

   if (!((hcntrl_data = INBYTE(AIC7870[HCNTRL])) & CHIPRESET))
      Ph_Pause(base);

   PHM_GETCONFIG(config_ptr);

   config_ptr->Cf_BusRelease = INBYTE(AIC7870[LATTIME]) & 0xfc;
   config_ptr->Cf_Threshold = (DFTHRSH1 + DFTHRSH0) >> 6;
   config_ptr->CFP_ConfigFlags |= RESET_BUS + SCSI_PARITY;
   config_ptr->CFP_TerminationLow = config_ptr->CFP_TerminationHigh = 1;

   if (!(hcntrl_data & CHIPRESET))
   {
      Ph_WriteHcntrl(base, (UBYTE) (hcntrl_data));
   }

}

/*********************************************************************
*
*   PH_InitHA routine -
*
*   This routine initializes the host adapter.
*
*  Return Value:  0x00      - Initialization successful
*                 <nonzero> - Initialization failed
*                  
*  Parameters:    config_ptr
*                 h.a. config structure will be filled in
*                 upon initialization.
*
*  Activation:    Aspi layer, initialization.
*                  
*  Remarks:                
*
*********************************************************************/

UWORD PH_InitHA (cfp_struct *config_ptr)
{
   register AIC_7870 *base;

   UWORD i;
#ifdef MIPS_BE
   UBYTE le_index, conv_index;
#endif /* MIPS_BE */

   base = config_ptr->CFP_Base;

   Ph_WriteHcntrl(base, (UBYTE) (PAUSE));

   PHM_INITHA(config_ptr);

   if (config_ptr->CFP_InitNeeded)
      if (i = Ph_LoadSequencer(config_ptr))
         return(i);

   if (config_ptr->CFP_InitNeeded)
   {
      OUTBYTE(AIC7870[SCSIID], config_ptr->Cf_ScsiId);
      if (config_ptr->CFP_ResetBus)
      {
         Ph_ResetSCSI(base);
      }
      Ph_ResetChannel(config_ptr);

      i = INBYTE(AIC7870[SBLKCTL]);                      /* Turn off led   */
      OUTBYTE(AIC7870[SBLKCTL], i & ~(DIAGLEDEN + DIAGLEDON));
   }
   OUTBYTE(AIC7870[SEQCTL], FAILDIS + FASTMODE + SEQRESET);
   OUTBYTE(AIC7870[SEQADDR0], IDLE_LOOP_ENTRY >> 2);
   OUTBYTE(AIC7870[SEQADDR1], 00);        /* Entry points always low page  */
   OUTBYTE(AIC7870[BRKADDR1], BRKDIS);

   OUTBYTE(AIC7870[PCISTATUS], config_ptr->Cf_Threshold << 6);

   OUTBYTE(AIC7870[SXFRCTL0], DFON);   /* Turn on digital filter 1/7/94 */

   /* high byte termination disable/enable */
   /* enable/disable high byte termination only if  */
   /* it's wide bus                                 */
   if (config_ptr->Cf_MaxTargets == 16)
   {
      OUTBYTE(AIC7870[SEEPROM],SEEMS);                /* process high byte */
      while(INBYTE(AIC7870[SEEPROM]) & EXTARBACK);          /* termination */
      OUTBYTE(AIC7870[SEEPROM],SEEMS|SEECS);
   
      if (config_ptr->CFP_TerminationHigh)          
      {                                              /* enable termination */
         OUTBYTE(AIC7870[BRDCTL],BRDDAT6|BRDSTB|BRDCS); 
         OUTBYTE(AIC7870[BRDCTL],BRDDAT6|BRDCS);        
      }
      else
      {
         OUTBYTE(AIC7870[BRDCTL],BRDSTB|BRDCS);     /* disable termination */
         OUTBYTE(AIC7870[BRDCTL],BRDCS);               
      }
      OUTBYTE(AIC7870[BRDCTL],0);
      OUTBYTE(AIC7870[SEEPROM],0);
   }

   OUTBYTE(AIC7870[DISCON_OPTION], ~config_ptr->CFP_AllowDscntL);
#ifdef MIPS_BE
   le_index = osm_conv_byteptr(DISCON_OPTION) + 1;
   conv_index = osm_conv_byteptr(le_index);
   OUTBYTE(AIC7870[conv_index], ~config_ptr->CFP_AllowDscntH);
#else /* MIPS_BE */
   OUTBYTE(AIC7870[DISCON_OPTION + 1], ~config_ptr->CFP_AllowDscntH);
#endif /* MIPS_BE */

   Ph_WriteHcntrl(base, (UBYTE) (INTEN));

   return(0);
}


/*********************************************************************
*
*   Ph_LoadSequencer -
*
*   This routine will down load sequencer code into AIC-7870 host
*   adapter
*
*  Return Value:  0 - good
*                 Others - error
*
*  Parameters:    seqptr - pointer to sequencer code
*                 seqsize - sequencer code size
*
*  Activation:    PH_InitHA
*
*  Remarks:       timer has been reset before calling this routine
*
*********************************************************************/

int Ph_LoadSequencer (cfp_struct *config_ptr)
{
   UBYTE *seq_code;
   UWORD seq_size;
   int index,cnt;
   UBYTE i, readback;
   register AIC_7870 *base;

   base = config_ptr->CFP_Base;
   /* depending on mode we are going to load different sequqncer */
   if (config_ptr->Cf_AccessMode == 2)
   {
      index = 1;                    /* Load optima mode sequencer          */
#if !defined( _EX_SEQ01_ )
      seq_code = Seq_01;
#endif /* !_EX_SEQ01 */
   }
   else
   {
      index = 0;                    /* Load standard mode sequencer        */
#if !defined( _EX_SEQ00_ )
      seq_code = Seq_00;
#endif /* !_EX_SEQ00 */
   }

   /* if the sequencer code size is zero then we should return */
   /* with error immediately (no sequencer code available) */
   if ( ! ( seq_size = SeqExist[index].seq_size ) )
      return( -2 );
   
   /* patch sequencer */
   i = TARGET_ID;
   if (config_ptr->CFP_MultiTaskLun)
   {
      i |= LUN;
   }
             
   /* PAUL - if TARG_LUN_MASK0 = 0, skip patch */
   if (SeqExist[index].seq_trglunmsk0 != 0)
   {
      seq_code[SeqExist[index].seq_trglunmsk0] = i;
   }


   OUTBYTE(AIC7870[SEQCTL], PERRORDIS + LOADRAM);
   OUTBYTE(AIC7870[SEQADDR0], 00);
   OUTBYTE(AIC7870[SEQADDR1], 00);
   for (cnt = 0; cnt < (int) seq_size; cnt++)
      OUTBYTE(AIC7870[SEQRAM], seq_code[cnt]);


   OUTBYTE(AIC7870[SEQCTL], PERRORDIS);	   /* clear */
   OUTBYTE(AIC7870[SEQCTL], PERRORDIS + LOADRAM);  /* and set */
   OUTBYTE(AIC7870[SEQADDR0], 00);
   OUTBYTE(AIC7870[SEQADDR1], 00);
   for (cnt = 0; cnt < seq_size; cnt++)
   {
      if ((readback = INBYTE(AIC7870[SEQRAM])) != seq_code[cnt])
      {
	      cmn_err(CE_WARN, "Ph_LoadSequencer: expected %x got %x on index %d",
		      seq_code[cnt], readback, cnt);
        return(ERR);
      }
   }


   OUTBYTE(AIC7870[SEQCTL], PERRORDIS);

   return(0);
}

#endif /* SIMHIM */
