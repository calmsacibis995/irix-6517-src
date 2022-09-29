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

/*
 * $Id: himdopt.c,v 1.9 1999/08/19 03:07:45 gwh Exp $
 */

#include "him_scb.h"
#include "him_equ.h"
#include "osm.h"
#include "sys/systm.h"
#include "sys/mips_addrspace.h"
#include "sys/kmem.h"
#ifdef IP32
#include "sys/mace.h"
#endif
#include "sys/debug.h"
#include "sys/ddi.h"

extern int adp_verbose;
extern int adp_coredump;


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

   /* Calculate address to Start of SCB for use by Sequencer   */
#ifdef IP32
   phys_scb_addr = kvtophys(scb_ptr->Sp_seq_scb.seq_scb_array) | PCI_NATIVE_VIEW;
#else
   phys_scb_addr = kvtophys(scb_ptr->Sp_seq_scb.seq_scb_array);
#endif

   DCACHE_WB(scb_ptr->Sp_seq_scb.seq_scb_array, 32);
   DCACHE_WB(scb_ptr->Sp_CDB, MAX_CDB_LEN);

   /* Write to table of physical and virtual addrs. */
   if (adp_verbose) {
	   int i;
	   printf("Ph_OptimaEnque: sending %d phys 0x%x virt 0x%x\n",
		 scb_num, phys_scb_addr, scb_ptr);
	   for (i=0; i < 8; i++)
		   printf("0x%x ", scb_ptr->Sp_seq_scb.seq_scb_array[i]);
	   printf("\n");
   }

   SCB_PTR_ARRAY[scb_num] = phys_scb_addr;
   ASSERT(ACTPTR[scb_num] == (sp_struct *) 0);
   ACTPTR[scb_num] = scb_ptr;

   OUTBYTE(AIC7870[QINFIFO], scb_num);
}

/*********************************************************************
*
*  Ph_OptimaEnqueHead routine -
*
*  Remarks:       Before calling this routine sequencer must have
*                 been paused already
*                  
*********************************************************************/
void Ph_OptimaEnqueHead (UBYTE scb_num,
                         sp_struct *scb_ptr,
                         register AIC_7870 *base)
{
   cfp_struct *config_ptr=scb_ptr->SP_ConfigPtr;
   hsp_struct *ha_ptr = config_ptr->CFP_HaDataPtr;
   DWORD phys_scb_addr;

   /* Calculate address to Start of SCB for use by Sequencer   */
#ifdef IP32
   phys_scb_addr = kvtophys(scb_ptr->Sp_seq_scb.seq_scb_array) | PCI_NATIVE_VIEW;
#else
   phys_scb_addr = kvtophys(scb_ptr->Sp_seq_scb.seq_scb_array);
#endif

   /* Write to table of physical and virtual addrs. */
   SCB_PTR_ARRAY[scb_num] = phys_scb_addr;
   ASSERT(ACTPTR[scb_num] == (sp_struct *) 0);
   ACTPTR[scb_num] = scb_ptr;

   /* Enqueue current scb as Next SCB for Sequencer to Execute   */
   Ph_OptimaQHead(scb_num, scb_ptr, base);
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

void Ph_OptimaQHead (UBYTE scb, sp_struct *scb_ptr, AIC_7870 *base)
{
   register cfp_struct *config_ptr = scb_ptr->SP_ConfigPtr;
   UBYTE i, j;
   UBYTE port_addr;

   i = ((UBYTE) scb_ptr->SP_Tarlun) >> 4;
   port_addr = OSM_CONV_BYTEPTR(SCRATCH_NEXT_SCB_ARRAY) + i;
   port_addr = OSM_CONV_BYTEPTR(port_addr);

   DCACHE_WB(scb_ptr->Sp_seq_scb.seq_scb_array, 32);
   DCACHE_WB(scb_ptr->Sp_CDB, MAX_CDB_LEN);

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


/*********************************************************************
*
* check for valid scb numbers in the QOUT_PTR_ARRAY
*
*********************************************************************/
void
Ph_OptimaCheckQout(cfp_struct *config_ptr, UBYTE scb_num)
{
   sp_struct *scb_ptr;
   hsp_struct *ha_ptr = config_ptr->CFP_HaDataPtr;
   AIC_7870 *base = config_ptr->CFP_Base;
   UBYTE qout_index;

   while (scb_num != INVALID_SCB_INDEX) {
	 if (scb_num >= config_ptr->Cf_NumberScbs) {
	     Ph_BadSeq(config_ptr, base, BADSEQ_SCBNUM);
	     return;
         }

         scb_ptr = ACTPTR[scb_num];
	 if (scb_ptr != (sp_struct *) 0) {

		 if (scb_ptr->SP_MgrStat == SCB_AUTOSENSE) {
			 scb_ptr->SP_HaStat   = HOST_NO_STATUS;
			 scb_ptr->SP_MgrStat  = SCB_DONE_ERR;
			 scb_ptr->SP_TargStat = UNIT_CHECK;
		 }

		 Ph_TerminateCommand(scb_ptr, scb_num);
	 }

         ha_ptr->qout_index -= 4;
	 qout_index = OSM_CONV_BYTEPTR(ha_ptr->qout_index);
	 scb_num = QOUT_PTR_ARRAY[qout_index];
   }
}


/*********************************************************************
*
* Handle one or more command completions
*
*********************************************************************/
void
Ph_OptimaCmdComplete (cfp_struct *config_ptr, UBYTE scb_num)
{
   hsp_struct *ha_ptr = config_ptr->CFP_HaDataPtr;
   AIC_7870 *base = config_ptr->CFP_Base;
   UBYTE qout_index;

   Ph_OptimaCheckQout(config_ptr, scb_num);
   OUTBYTE(AIC7870[CLRINT], CLRCMDINT);

   /*
    * there is a race condition between qout_ptr_array updates
    * by the sequencer and interrupts, so just check again.
    */
   qout_index = OSM_CONV_BYTEPTR(ha_ptr->qout_index);
   scb_num = QOUT_PTR_ARRAY[qout_index];
   Ph_OptimaCheckQout(config_ptr, scb_num);
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
   AIC_7870 *base = config_ptr->CFP_Base;
   int i, targ;
   UBYTE option;

   if (adp_verbose)
	   printf("OptimaRequestSense: sense_ptr 0x%x sense_len 0x%x\n",
		  scb_ptr->Sp_SensePtr, scb_ptr->Sp_SenseLen);

   targ = scb_ptr->SP_Tarlun >> 4;

   /*
    * Clear 6 bytes of CDB, make it 8 to account for possible swizzling.
    * Then fill in the fields and swizzle.
    */
   for (i = 0; i < 8; i++)
	   scb_ptr->Sp_CDB[i] = 0;

   scb_ptr->Sp_CDB[0] = 0x03;
   scb_ptr->Sp_CDB[4] = (UBYTE) scb_ptr->Sp_SenseLen;

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
   option = config_ptr->Cf_ScsiOption[targ].byte;
   if (config_ptr->Cf_ScsiOption[targ].byte & SOME_NEGO)
     PH_SetNeedNego (base, targ, option);

   /* Flush the mini-SG list and the sense buffer */
   DCACHE_WB(&(scb_ptr->Sp_SensePtr), (2 * sizeof(DWORD)));
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
Ph_OptimaClearTargetBusy (cfp_struct *config_ptr, UBYTE scb_num)
{
   sp_struct *scb_ptr;
   hsp_struct *ha_ptr = config_ptr->CFP_HaDataPtr;

   scb_ptr = ACTPTR[scb_num];
   if (scb_ptr == (sp_struct *) 0) 
	   return;

   Ph_OptimaIndexClearBusy(config_ptr, ((UBYTE) scb_ptr->SP_Tarlun) >> 4);
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
   int i = 0, view=0;

   for (i=0; i <= config_ptr->Cf_NumberScbs; i++) {
	   ACTPTR[i] = (sp_struct *) 0;
	   FREE_STACK[i] = i;
   }

   for (i = 0; i < 256 ; i++)
           QOUT_PTR_ARRAY[i] = INVALID_SCB_INDEX;

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
   ASSERT(IS_KSEGDM(SCB_PTR_ARRAY));
   Ph_MovPtrToScratch(SCRATCH_SCB_PTR_ARRAY,
		      KDM_TO_PHYS(SCB_PTR_ARRAY) | view,
		      config_ptr);

   ASSERT(IS_KSEGDM(QOUT_PTR_ARRAY));
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
}


/*********************************************************************
*
*  Get free scb from free scb pool
*
*  Return Value:  scb number. If = 0xff, No scb is Available.
*                  
*********************************************************************/
UBYTE Ph_OptimaGetFreeScb (hsp_struct *ha_ptr,
                           sp_struct *scb_ptr)
{
   return scb_ptr->Sp_scb_num;
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
*  Activation:    Ph_SetOptimaScratch
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
*
*  Pause sequencer and abort given SCB. Remove from QIN or
*  QOUT fifos if needed.
*
*  Returns 0 if the scb was aborted and an interrupt will be generated
*  by the sequencer.
*  Returns -1 if the scb was aborted before the sequencer got to it.
*                  
*********************************************************************/

int
PH_OptimaAbortActive ( sp_struct *scb_pointer)
{
   register cfp_struct *config_ptr = scb_pointer->SP_ConfigPtr;
   AIC_7870 *base = config_ptr->CFP_Base;
   hsp_struct *ha_ptr = config_ptr->CFP_HaDataPtr;
   int scb_index, retval;
   UBYTE scbsave, bytebuf, j, k;
   UBYTE bi, port_addr, tarlun;

   PH_Pause(base);

   /*
    * Make sure any outstanding PCI commands are completed (DFSTATUS).
    * XXX yuck, I really don't like these infinite while loops.
    */
   while(~INBYTE(AIC7870[DFSTATUS]) & MREQPEND);


   scb_index = scb_pointer->Sp_scb_num;
   ASSERT(ACTPTR[scb_index] = scb_pointer);


     /*
      * Look for the SCB in the QOUT_PTR_ARRAY.  It could have been done just
      * before we entered ha_deq_key().
      */
      bi = OSM_CONV_BYTEPTR(ha_ptr->qout_index);
      for (; QOUT_PTR_ARRAY[bi] != INVALID_SCB_INDEX; bi += 4) {
         if (QOUT_PTR_ARRAY[bi] == scb_index)  {
	     retval = 0;
	     goto abort_done;
         }
      }


      /*
       * Look for the SCB in the QINFIFO
       * If the SCB is found here, it is taken out of the fifo.  Unfortunately,
       * this means we never get an interrupt for this command.
       */
      for (j = INBYTE(AIC7870[QINCNT]); j != 0; j--) {
         k = INBYTE(AIC7870[QINFIFO]);
         if (k == scb_index) {
	    ACTPTR[scb_index] = (sp_struct *) 0;
            retval = -1;
	    goto abort_done;
	 } else {
            OUTBYTE(AIC7870[QINFIFO], k);
         }
      }
      

      /*
       * Check NEXT_SCB_ARRAY
       */
      tarlun = ((UBYTE) scb_pointer->SP_Tarlun) >> 4;
      port_addr = OSM_CONV_BYTEPTR(SCRATCH_NEXT_SCB_ARRAY) + tarlun;
      port_addr = OSM_CONV_BYTEPTR(port_addr);
      if (scb_index == INBYTE(AIC7870[port_addr]))
      {
         scb_pointer->Sp_seq_scb.seq_scb_struct.Aborted = 1;
	 DCACHE_WB(scb_pointer->Sp_seq_scb.seq_scb_array, 32);
         retval = 0;
	 goto abort_done;
      }


      /*
       * Check active SCB
       */
      if (scb_index == INBYTE(AIC7870[ACTIVE_SCB]))
      {
         /* Abort active SCB in chip, also   */

         scbsave = INBYTE(AIC7870[SCBPTR]);        /* Save current SCB ptr */
         OUTBYTE(AIC7870[SCBPTR], scb_index);   /* Select Driver SCB */

         /* Or aborted bit into current chain control byte */
         bytebuf = INBYTE(AIC7870[SCB13]) | SCB_ABORTED; 
         OUTBYTE(AIC7870[SCB13], bytebuf);         /* Set in HW SCB */
         OUTBYTE(AIC7870[SCBCNT], 00);             /* Reset SCB address count */
         OUTBYTE(AIC7870[SCBPTR], scbsave);        /* Restore SCB ptr */
      }

      scb_pointer->SP_MgrStat = SCB_ABORTED;
      scb_pointer->Sp_seq_scb.seq_scb_struct.Aborted = 1;

                                          /* Set SpecFunc bit in SCB Control Byte */
      scb_pointer->Sp_seq_scb.seq_scb_struct.SpecFunc = SPEC_BIT3SET;

      scb_pointer->SP_SegCnt = SPEC_OPCODE_00;  /* Setup "SpecFunc" opcode */

                                          /* Setup Message Value     */
      if (scb_pointer->Sp_seq_scb.seq_scb_struct.TagEnable)
         scb_pointer->SP_SegPtr = MSG0D;  /* "Abort Tag" for tagged  */
      else
         scb_pointer->SP_SegPtr = MSG06;  /* "Abort" for non-tagged  */

      DCACHE_WB(scb_pointer->Sp_seq_scb.seq_scb_array, 32);

      /* Insert at Head of Queue only if we are not in the process   */      
      /* of selecting the target. If the selection is in progress    */
      /* a interrupt to the HIM is guaranteed. Either the result of  */
      /* command execution or selection time out will hapen */ 
      if (!((INBYTE(AIC7870[SCSISEQ]) & ENSELO) &&
         (INBYTE(AIC7870[WAITING_SCB]) == scb_index)))
            Ph_OptimaEnqueHead(scb_index, scb_pointer, base);

      retval = 0;

 abort_done:

   PH_UnPause(base);
   return( retval);
}
