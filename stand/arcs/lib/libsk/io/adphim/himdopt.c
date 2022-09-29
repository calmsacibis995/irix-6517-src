/* $Header: /proj/irix6.5.7m/isms/stand/arcs/lib/libsk/io/adphim/RCS/himdopt.c,v 1.2 1997/12/20 02:30:42 philw Exp $ */
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


#ifndef SIMHIM

#include "him_scb.h"
#include "him_equ.h"
#include "seq_off.h"
#include "osm.h"
#include "sys/systm.h"
#include "sys/mips_addrspace.h"
#include "libsk.h"
#include "libsc.h"
#include "sys/debug.h"
#include "sys/cmn_err.h"

extern int adp_verbose;
#ifdef LE_DISK
extern int soft_swizzle;
#endif

/*********************************************************************
*
*  Ph_OptimaEnque routine -
*
*  Queue an OPTIMA SCB for the Lance sequencer
*
*  Remarks:       Before calling this routine sequencer must have
*                 been paused already
*                  
* No locks should be OK.  We are only writing to our own scb, our own
* slot in the SCB_PTR_ARRAY, and our own slot in ACTPTR.
*********************************************************************/

void Ph_OptimaEnque (UBYTE scb_num, sp_struct *scb_ptr)
{
   hsp_struct *ha_ptr = scb_ptr->SP_ConfigPtr->CFP_HaDataPtr;
   AIC_7870 *base = scb_ptr->SP_ConfigPtr->CFP_Base;
   DWORD phys_scb_addr;
   int i;

   /* Calculate address to Start of SCB for use by Sequencer   */
   /* SCB should always be unswizzled.  I set it up in little endien format. */
   phys_scb_addr = KDM_TO_PHYS(scb_ptr->Sp_seq_scb.seq_scb_array) | PCI_NATIVE_VIEW;


   /* Write to table of physical and virtual addrs. */
   if (adp_verbose) {
	   int i;
	   PRINT("Ph_OptimaEnque: sending %d phys 0x%x virt 0x%x\n",
		 scb_num, phys_scb_addr, scb_ptr);
	   for (i=0; i < 8; i++)
		   PRINT("0x%x ", scb_ptr->Sp_seq_scb.seq_scb_array[i]);
	   PRINT("\n");
   }

   DCACHE_WBINVAL(scb_ptr->Sp_seq_scb.seq_scb_array, 32);
   DCACHE_WBINVAL(scb_ptr->Sp_CDB, MAX_CDB_LEN);
   flush_cache();

   for (i=0; i < 256; i++)
	   QOUT_PTR_ARRAY[i] = NULL_SCB;
	   
   SCB_PTR_ARRAY[scb_num] = phys_scb_addr;
   ASSERT(ACTPTR[scb_num] == (sp_struct *) 0);
   ACTPTR[scb_num] = scb_ptr;


   OUTBYTE(AIC7870[QINFIFO], scb_num);
}

/*********************************************************************
*
*  Ph_OptimaEnqueHead routine -
*
*  Queue an OPTIMA SCB for the Lance sequencer
*
*  Return Value:  None, for now
*                  
*  Parameters:    scb - scb number
*                 scb_ptr -  
*                 base - register base address
*
*  Activation:    Ph_SendCommand
*                  
*  Remarks:       Before calling this routine sequencer must have
*                 been paused already
*                  
*********************************************************************/
void Ph_OptimaEnqueHead (UBYTE scb,
                         sp_struct *scb_ptr,
                         register AIC_7870 *base)
{
   cfp_struct *config_ptr=scb_ptr->SP_ConfigPtr;
   hsp_struct *ha_ptr = config_ptr->CFP_HaDataPtr;
   DWORD phys_scb_addr;

   /* Calculate address to Start of SCB for use by Sequencer   */
   phys_scb_addr = KDM_TO_PHYS(scb_ptr);

   DCACHE_WBINVAL(scb_ptr->Sp_seq_scb.seq_scb_array, 32);
   DCACHE_WBINVAL(scb_ptr->Sp_CDB, MAX_CDB_LEN);

   /* Write to physical table */
   SCB_PTR_ARRAY[scb] = phys_scb_addr;
   DCACHE_WBINVAL(&SCB_PTR_ARRAY[scb], 4);

   /* SCB_PTR_ARRAY[scb] = phys_scb_addr -- was Already setup    */
   /*   by caller (i.e. Ph_BusDeviceReset)                       */

   /* Enqueue current scb as Next SCB for Sequencer to Execute   */
   Ph_OptimaQHead(scb, scb_ptr, base);

   return;
}

/*********************************************************************
*  Ph_OptimaQHead -
*
*  Enqueue the given scb as Next SCB for Sequencer to Execute.
*  If the sequencer already has a scb ready in SCRATCH_NEXT_SCB_ARRAY,
*  move that back by one.  Move all scb's in the QINFIFO by 1.
*
*  This is most often used by Ph_OptimaRequestSense.  In that case,
*  the given scb command is whatever caused the check condition.
*  The given scb command has been converted to a request sense.
*
*  Return Value:  
*                  
*  Parameters:    
*
*  Activation: Ph_OptimaEnqueHead
*              Ph_OptimaRequestSense
*
*  Remarks:       
*                 
*********************************************************************/
#ifdef   _FAULT_TOLERANCE
void Ph_OptimaQHead (UBYTE scb, sp_struct *scb_ptr, AIC_7870 *base)
{
   register cfp_struct *config_ptr = scb_ptr->SP_ConfigPtr;
   register hsp_struct *ha_ptr = config_ptr->CFP_HaDataPtr;
   UBYTE i, j;
   UBYTE port_addr;

   i = ((UBYTE) scb_ptr->SP_Tarlun) >> 4;
   port_addr = OSM_CONV_BYTEPTR(SCRATCH_NEXT_SCB_ARRAY) + i;
   port_addr = OSM_CONV_BYTEPTR(port_addr);

   DCACHE_WBINVAL(scb_ptr->Sp_seq_scb.seq_scb_array, 32);
   DCACHE_WBINVAL(scb_ptr->Sp_CDB, MAX_CDB_LEN);

   /*
    * Get the "Next_SCB_Array" Element (if any) and move it back by one place
    * in the QINFIFO
    */
   if ((j = INBYTE(AIC7870[port_addr])) != 0x7F)
   {
      OUTBYTE(AIC7870[port_addr], 0x7F);     /* Nullify NEXT_SCB_ARRAY entry */

      OUTBYTE(AIC7870[QINFIFO], (j & 0x7F)); /* Place removed SCB on QINFIFO */
      for (i = INBYTE(AIC7870[QINCNT]) - 1; i != 0; i--)
      {
         j = INBYTE(AIC7870[QINFIFO]);       /* Rotate QINFIFO to place */
         OUTBYTE(AIC7870[QINFIFO], j);       /* reinserted SCB at head of queue */
      }
    }

   /*
    * Now place the given SCB at head of queue, and move all other scb's back
    * by one place.
    */
   OUTBYTE(AIC7870[QINFIFO], scb);           /* Place removed SCB on QINFIFO */
   for (i = INBYTE(AIC7870[QINCNT]) - 1; i != 0; i--)
   {
      j = INBYTE(AIC7870[QINFIFO]);          /* Rotate QINFIFO to place */
      OUTBYTE(AIC7870[QINFIFO], j);          /* current SCB at head of queue */
   }



}
#endif   /* _FAULT_TOLERANCE */
/*********************************************************************
*
* Handle completed commands by reading the scb numbers from QOUT_PTR_ARRAY
* Only 1 command complete is possible in the prom.
*                  
*********************************************************************/
void
Ph_OptimaCmdComplete (cfp_struct *config_ptr, UBYTE scb_num)
{
   sp_struct *scb_ptr;
   hsp_struct *ha_ptr = config_ptr->CFP_HaDataPtr;
   AIC_7870 *base = config_ptr->CFP_Base;

   while (scb_num != NULL_SCB) {

         ha_ptr->qout_index -= 4;

	 if (scb_num >= config_ptr->Cf_NumberScbs) {
		 printf("Ph_OptimaCmdComplete: bad scb_num %d\n", scb_num);
		 break;
	 }

         scb_ptr = ACTPTR[scb_num];
	 if (scb_ptr == NULL) {
		 printf("Ph_OptimaCmdComplete: NULL scb_ptr scb_num %d\n",
			scb_num);
		 break;
	 }

         if (scb_ptr->SP_Cmd == HARD_RST_DEV)
            /* Since this is Completion for a "Hard Device Reset"*/
            /*  Command, we must Setup the Sequencer XFER_OPTION */
            /*  with NEEDNEGO Option (Need to Negotiate)         */

            Ph_SetNeedNego(scb_num, base);

         else if ((scb_ptr->SP_MgrStat == SCB_ABORTINPROG) &&
                  (scb_ptr->Sp_seq_scb.seq_scb_struct.Aborted == 1))
	       return;

         if (scb_ptr->SP_MgrStat == SCB_AUTOSENSE) {
            scb_ptr->SP_HaStat   = HOST_NO_STATUS;
            scb_ptr->SP_MgrStat  = SCB_DONE_ERR;
            scb_ptr->SP_TargStat = UNIT_CHECK;
         }

	 /* this is called for every completed command */
         Ph_TerminateCommand(scb_ptr, scb_num);

	 scb_num = NULL_SCB;
   }

   OUTBYTE(AIC7870[CLRINT], CLRCMDINT);
}


/*********************************************************************
*
*  Perform request sense for OPTIMA mode
*
*********************************************************************/
void Ph_OptimaRequestSense (sp_struct *scb_ptr,
                           UBYTE scb)

{
   cfp_struct *config_ptr=scb_ptr->SP_ConfigPtr;
   hsp_struct *ha_ptr = config_ptr->CFP_HaDataPtr;
   AIC_7870 *base = config_ptr->CFP_Base;
   UBYTE i;

   if (adp_verbose)
	   printf("OptimaRequestSense: sense_ptr 0x%x sense_len 0x%x\n",
		  scb_ptr->Sp_SensePtr, scb_ptr->Sp_SenseLen);

   /*
    * Clear 6 bytes of CDB, make it 8 to account for possible swizzling.
    * Then fill in the fields and swizzle.
    */
   for (i = 0; i < 8; i++)
	   scb_ptr->Sp_CDB[i] = 0;

   scb_ptr->Sp_CDB[0] = 0x03;
   scb_ptr->Sp_CDB[4] = (UBYTE) scb_ptr->Sp_SenseLen;

#ifdef LE_DISK
   if (soft_swizzle)
	   osm_swizzle(scb_ptr->Sp_CDB, 8);
#endif

   /* Setup to NOT Accept Modified Data Pointer within SEQUENCER */
   scb_ptr->SP_RejectMDP = 1;
   
   /* Must disable disconnect for request sense */
   scb_ptr->SP_DisEnable = 0;
   
   /* Must disable tagged queuing for request sense */
   scb_ptr->SP_TagEnable = 0;

   scb_ptr->SP_SegCnt   = 0x01;
   scb_ptr->SP_CDBLen   = 0x06;
   scb_ptr->SP_SegPtr   = scb_ptr->Sp_Sensesgp;
   scb_ptr->SP_TargStat = 0x00;
   scb_ptr->SP_ResCnt   = (DWORD) 0;

   /* Flush the mini-SG list and the sense buffer */
   DCACHE_WBINVAL(&(scb_ptr->Sp_SensePtr), (2 * sizeof(DWORD)));
   DCACHE_INVAL(scb_ptr->Sp_Sensebuf, (scb_ptr->Sp_SenseLen & 0x7fffffff));

   /**************************************************************/
   /* Initialize Last 16 bytes of SCB for Use by SEQUENCER Only  */
   /*                                                            */
   /*  ARROW USED : UBYTE SCB_RsvdX[16];                         */
   /*                                                            */
   /*  for LANCE  : Logically set start of "SCB_RsvdX" to the    */
   /*               16th byte of the SCB                         */
   /*                                                            */
   /*               ==> (DWORD) scb_ptr->seq_scb_array[4]        */
   /*                                                            */
   /**************************************************************/
   /* EXTERNAL SCB SCHEME needs all 32 bytes                     */
   /* RsvdX[8] = chain control,                                  */
   /*                                                            */
   /*  bits 7-4: 2's complement of progress                      */
   /*            count                                           */
   /*  bit 3:    aborted flag                                    */
   /*  bit 2:    abort_SCB flag                                  */
   /*  bit 1:    concurrent flag                                 */
   /*  bit 0:    holdoff_SCB flag                                */
   /*                                                            */
   /* RsvdX[10] = aborted HaStat                                 */
   /*                                                            */
   /**************************************************************/

   for (i = 4; i < 8; i++) scb_ptr->Sp_seq_scb.seq_scb_array[i] = 0;

   /**************************************************************/

   /* Enqueue current scb as Next SCB for Sequencer to Execute      */
   Ph_OptimaQHead(scb, scb_ptr, base);

   Ph_OptimaIndexClearBusy(config_ptr, ((UBYTE) scb_ptr->SP_Tarlun) >> 4);
   scb_ptr->SP_MgrStat = SCB_AUTOSENSE;
}


/*********************************************************************
*
*  Ph_OptimaClearDevQue routine -
*
*  Clear active scb for the specified device in OPTIMA mode
*
*  Return Value:  None
*                  
*  Parameters: config_ptr -
*              scb_ptr -
*
*  Activation: Ph_HardReset
*                  
*  Remarks:                
*                  
*********************************************************************/
void Ph_OptimaClearDevQue (sp_struct *scb_ptr,
                           AIC_7870 *base)
{
}


/*********************************************************************
*
*  Clear busy entry for the specified index
*
*********************************************************************/
void
Ph_OptimaIndexClearBusy (cfp_struct *config_ptr, UBYTE index)
{
   AIC_7870 *base = config_ptr->CFP_Base;

   OUTBYTE(AIC7870[SCBPTR], index);
   OUTBYTE(AIC7870[SCB31], INVALID_SCB_INDEX);
}


/*********************************************************************
*
*  Clear target busy entry for the specified scb
*
*  Activation: Ph_IntFree
*              Ph_IntSelTo
*              Ph_CheckCondition
*
*********************************************************************/
void
Ph_OptimaClearTargetBusy (cfp_struct *config_ptr, UBYTE scb)
{
   sp_struct *scb_ptr;
   hsp_struct *ha_ptr = config_ptr->CFP_HaDataPtr;

   scb_ptr = ACTPTR[scb];
   Ph_OptimaIndexClearBusy(config_ptr, ((UBYTE) scb_ptr->SP_Tarlun) >> 4);
}


/*********************************************************************
*
*  Ph_OptimaClearChannelBusy -
*
*  Clear all target busy associated with OPTIMA channel
*
*  Return Value:  none
*                  
*  Parameters:    config_ptr -
*
*  Activation:    Ph_ResetChannel
*
*  Remarks:       
*                 
*********************************************************************/
void Ph_OptimaClearChannelBusy (cfp_struct *config_ptr)
{
}

/*********************************************************************
*
* This should program the "scratch" areas of the sequencer memory
* only.  Right now, it does a little bit more.
*
*********************************************************************/
void
Ph_set_scratchmem(cfp_struct *config_ptr)
{
   AIC_7870 *base = config_ptr->CFP_Base;
   hsp_struct *ha_ptr = config_ptr->CFP_HaDataPtr;
   UWORD port_addr;
   int i, view=0;

   for (i=0; i <= config_ptr->Cf_NumberScbs; i++) {
	   ACTPTR[i] = (sp_struct *) 0;
	   FREE_STACK[i] = i;
   }

   ha_ptr->done_cmd = 0;
   ha_ptr->qout_index = 4;
   FREE_LO_SCB = 1;
   FREE_HI_SCB = (UBYTE)config_ptr->Cf_NumberScbs;

   /* clear target busy table */
   for (i = 0; i < config_ptr->Cf_NumberScbs; i++)
      Ph_OptimaIndexClearBusy(config_ptr,(UBYTE)i);

   OUTBYTE(AIC7870[SCRATCH_QIN_CNT],0);
   OUTBYTE(AIC7870[WAITING_SCB], INVALID_SCB_INDEX);
   OUTBYTE(AIC7870[ACTIVE_SCB], INVALID_SCB_INDEX);

   /*
    * Initialize Sequencer memory pointers.
    * The pointers should not be swizzled.
    */
#ifdef IP32
   view = PCI_NATIVE_VIEW;
#endif
   Ph_MovPtrToScratch(SCRATCH_SCB_PTR_ARRAY,
		      KDM_TO_PHYS(SCB_PTR_ARRAY) | view,
		      config_ptr);

   Ph_MovPtrToScratch(SCRATCH_QOUT_PTR_ARRAY,
		      KDM_TO_PHYS(QOUT_PTR_ARRAY) | view, 
		      config_ptr);

   Ph_MovPtrToScratch(SCRATCH_QIN_PTR_ARRAY,
		      0x0bad0001, config_ptr);

   Ph_MovPtrToScratch(SCRATCH_BUSY_PTR_ARRAY,
		      0x0bad0003, config_ptr);


   /* Initialize Sequencer "next_SCB_array" = 16 bytes of 7FH */
   port_addr = SCRATCH_NEXT_SCB_ARRAY;

   Ph_MovPtrToScratch(port_addr, 0x7F7F7F7f, config_ptr);
   Ph_MovPtrToScratch(port_addr+4, 0x7F7F7F7f, config_ptr);
   Ph_MovPtrToScratch(port_addr+8, 0x7F7F7F7f, config_ptr);
   Ph_MovPtrToScratch(port_addr+12, 0x7F7F7F7f, config_ptr);

   if (adp_verbose)
	   printf("SCB_PTR_ARRAY 0x%x QOUT_PTR_ARRAY 0x%x\n",
		  SCB_PTR_ARRAY, QOUT_PTR_ARRAY);
}


/*********************************************************************
*
*  Get free scb from free scb pool
*  Standalone is single threaded, so only 1 cmd goes out at a time.
*  So there is always 1 available.
*                  
*********************************************************************/
UBYTE Ph_OptimaGetFreeScb (hsp_struct *ha_ptr,
                           sp_struct *scb_ptr)
{
	return 1;
}


/*********************************************************************
*
*  Ph_MovPtrToScratch
*
*  Move physical memory pointer to scratch RAM in Intel order ( 0, 1, 2, 3 )
*
*
*  Return Value:  VOID
*
*  Parameters:    port_addr - starting address in Lance scratch/scb area
*
*                 ptr_buf - Physical address to be moved
*
*  Remarks:       
*
*********************************************************************/
void Ph_MovPtrToScratch (UWORD port_addr, DWORD ptr_buf,
                         cfp_struct *config_ptr)
{
   AIC_7870 *base = config_ptr->CFP_Base;
   union outbuf
   {
     UBYTE byte_buf[4];
     DWORD mem_ptr;
   } out_array;

   out_array.mem_ptr = ptr_buf;

   if (adp_verbose)
	   printf("MovPtrToScratch: 0x%x <- 0x%x\n", port_addr, ptr_buf);

#ifdef MIPS_BE
   OUTBYTE(AIC7870[port_addr],    out_array.byte_buf[3]);
   port_addr = (port_addr % 4) ? port_addr-1 : port_addr + 7;
   OUTBYTE(AIC7870[port_addr],  out_array.byte_buf[2]);
   port_addr = (port_addr % 4) ? port_addr-1 : port_addr + 7;
   OUTBYTE(AIC7870[port_addr],  out_array.byte_buf[1]);
   port_addr = (port_addr % 4) ? port_addr-1 : port_addr + 7;
   OUTBYTE(AIC7870[port_addr],  out_array.byte_buf[0]);
#else
   OUTBYTE(AIC7870[port_addr],    out_array.byte_buf[0]);
   OUTBYTE(AIC7870[port_addr+1],  out_array.byte_buf[1]);
   OUTBYTE(AIC7870[port_addr+2],  out_array.byte_buf[2]);
   OUTBYTE(AIC7870[port_addr+3],  out_array.byte_buf[3]);
#endif

}

/*********************************************************************
*  Ph_OptimaAbortActive -
*
*  Pause Arrow and initiate abort of active SCB. Remove from QIN or
*  QOUT fifos if needed.
*
*  Return Value:  0 -  SCB Abort completed
*                 1 -  SCB Abort in progress
*                 -1 - SCB not aborted
*                  
*  Parameters:    
*
*  Activation: Ph_ScbAbort()
*
*  Remarks: For active SCB in Optima mode
*                 
*********************************************************************/
#ifdef   _FAULT_TOLERANCE
int Ph_OptimaAbortActive ( sp_struct *scb_pointer)
{
   register cfp_struct *config_ptr = scb_pointer->SP_ConfigPtr;
   AIC_7870 *base = config_ptr->CFP_Base;
   hsp_struct *ha_ptr = config_ptr->CFP_HaDataPtr;
   int scb_index, i, retval = ABTACT_NOT;
   UBYTE scbsave, bytebuf, j, k;
   UBYTE bi, port_addr, tarlun, first_qout, qout_index;

   cmn_err(CE_WARN, "Ph_OptimaAbortActive scb 0x%x", scb_pointer);

   Ph_WriteHcntrl(base, (UBYTE) (INBYTE(AIC7870[HCNTRL]) | PAUSE));
   while (~INBYTE(AIC7870[HCNTRL]) & PAUSEACK);

   for (i = 0; i < 1000; i++)
   {
      if (~INBYTE(AIC7870[DFSTATUS]) & MREQPEND);
         break;
   }
   for (i = 1; i < config_ptr->Cf_NumberScbs + 1; i++)        
   {
      if (ACTPTR[i] == scb_pointer)
      {
         scb_index = i;             /* Found it! */
         break;
      }
   }

   /* Check various steps of SCB progression in sequencer. Handle abort
      based on step SCB is in when located, then exit. */

   for (;;)
   {
      if (i == config_ptr->Cf_NumberScbs + 1)
      {
         retval = ABTACT_NOT;           /* If SCB ptr not found, return */
         break;
      }
      /* If SCB is in QIN, QOUT or NEXT_SCB arrays, substitute bookmark
         SCB INDEX with aborted bit set in array. Complete aborted SCB
         to host.  */
      /* (Bookmarks are used so as not to require "shuffling" of array) */


#ifdef MIPS_BE
      bi = osm_conv_byteptr(ha_ptr->qout_index);
#else  /* MIPS_BE */
      bi = ha_ptr->qout_index;
#endif /* MIPS_BE */
      qout_index = bi;
      first_qout = 1;
      for (; QOUT_PTR_ARRAY[bi] != NULL_SCB; bi += 4)
      {                                      /* Check QOUT Array First */
         if (QOUT_PTR_ARRAY[bi] == scb_index) /* In QOUT array, complete now */
         {
	    /*
	     * If the aborted scb is not at the head of the qout array, 
	     * then shift all the other scbs up by 1 step.  Put a NULL_SCB
	     * in the aborted scbs place and bump the qout_index by 1 step.
	     */
            if (!first_qout) {
               for (j = bi-4; j != bi; j = j-4)       
               {                             /* Take it out of QOUT array */
                  QOUT_PTR_ARRAY[j+4] = QOUT_PTR_ARRAY[j];
                  if (j == qout_index)
                     break;
               }
	       QOUT_PTR_ARRAY[qout_index] = NULL_SCB;
	       ha_ptr->qout_index += 4;
	       retval = ABTACT_CMP;
            }
         }

	 first_qout = 0;
      }
/*      DCACHE_WBINVAL(QOUT_PTR_ARRAY, 256); */
      if (retval != ABTACT_NOT)              /* Exit if SCB aborted */
         break;

      /* Get LSB of Sequencer ptr to QIN Array */
      for (j = INBYTE(AIC7870[QINCNT]); j != 0; j--)
      {
         k = INBYTE(AIC7870[QINFIFO]);
         if (k == scb_index)
         {
            retval = ABTACT_CMP;
         }
         else
         {
            OUTBYTE(AIC7870[QINFIFO], k);
         }
      }
      
      if (retval != ABTACT_NOT)              /* Exit if SCB aborted */
         break;
   

      /* Check NEXT_SCB_ARRAY */

      tarlun = ((UBYTE) scb_pointer->SP_Tarlun) >> 4;  /* Index for NEXT_SCB_ARRAY */
      port_addr = OSM_CONV_BYTEPTR(SCRATCH_NEXT_SCB_ARRAY) + tarlun;
      port_addr = OSM_CONV_BYTEPTR(port_addr);
      if (scb_index == INBYTE(AIC7870[port_addr]))
      {
         scb_pointer->Sp_seq_scb.seq_scb_struct.Aborted = 1;
         retval = ABTACT_PRG;
         break;
      }


      /* Check active SCB */
      if (scb_index == INBYTE(AIC7870[ACTIVE_SCB]))
      {
         /* Abort active SCB in chip, also   */

         scbsave = INBYTE(AIC7870[SCBPTR]);        /* Save current SCB ptr */
         OUTBYTE(AIC7870[SCBPTR], HWSCB_DRIVER);   /* Select Driver SCB */

         /* Or aborted bit into current chain control byte */
         bytebuf = INBYTE(AIC7870[SCB13]) | SCB_ABORTED; 
         OUTBYTE(AIC7870[SCB13], bytebuf);         /* Set in HW SCB */
         OUTBYTE(AIC7870[SCBCNT], 00);             /* Reset SCB address count */
         OUTBYTE(AIC7870[SCBPTR], scbsave);        /* Restore SCB ptr */
      }
      scb_pointer->SP_MgrStat = SCB_ABORTINPROG;
      scb_pointer->Sp_seq_scb.seq_scb_struct.Aborted = 1;

                                          /* Set SpecFunc bit in SCB Control Byte */
      scb_pointer->Sp_seq_scb.seq_scb_struct.SpecFunc = SPEC_BIT3SET;

      scb_pointer->SP_SegCnt = SPEC_OPCODE_00;  /* Setup "SpecFunc" opcode */

                                          /* Setup Message Value     */
      if (scb_pointer->Sp_seq_scb.seq_scb_struct.TagEnable)
      {
         scb_pointer->SP_SegPtr = MSG0D;  /* "Abort Tag" for tagged  */
      }
      else
      {
         scb_pointer->SP_SegPtr = MSG06;  /* "Abort" for non-tagged  */
      }
      /* Insert at Head of Queue only if we are not in the process   */      
      /* of selecting the target. If the selection is in progress    */
      /* a interrupt to the HIM is guaranteed. Either the result of  */
      /* command execution or selection time out will hapen */ 
      if (!((INBYTE(AIC7870[SCSISEQ]) & ENSELO) &&
         (INBYTE(AIC7870[WAITING_SCB]) == scb_index)))
         ha_ptr->enquehead(scb_index, scb_pointer, base);

      retval = ABTACT_PRG;                /* Return Abort in Progress */
      break;
   }

   Ph_WriteHcntrl(base, (UBYTE) ((INBYTE(AIC7870[HCNTRL]) & ~PAUSE)));
   INBYTE(AIC7870[HCNTRL]);
   return( retval);
}
#endif   /* _FAULT_TOLERANCE */

#endif /* SIMHIM */
