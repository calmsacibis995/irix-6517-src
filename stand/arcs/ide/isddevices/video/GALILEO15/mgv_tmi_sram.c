/*
**               Copyright (C) 1989, Silicon Graphics, Inc.
**
**  These coded instructions, statements, and computer programs  contain
**  unpublished  proprietary  information of Silicon Graphics, Inc., and
**  are protected by Federal copyright law.  They  may  not be disclosed
**  to  third  parties  or copied or duplicated in any form, in whole or
**  in part, without the prior written consent of Silicon Graphics, Inc.
*/

#ifdef IP30

#include "mgv_diag.h"

/*
 * Forward Function References
 */
__uint32_t	mgv_TMISramAddrUniqTest(void);
__uint32_t	mgv_TMISramPatrnTest(void);
void 		_mgv_TMISramWrite(ushort_t addr, uchar_t val);
void		_mgv_TMIReadyForWrite(void);
void		_mgv_TMIReadyForRead(void);
__uint32_t 	_mgv_TMISramCompare(uchar_t type, uchar_t val, \
			__uint32_t *rcvBuf );
__uint32_t	_mgv_TE_Data(void);
void		 _mgv_TE_Init(void);

/*
 * NAME: mgv_TMISramAddrUniqTest
 *
 * FUNCTION: TMI SRAM Address Uniq Test
 *
 * INPUTS: none
 *
 * OUTPUTS: -1 if error, 0 if no error
 */
__uint32_t 
mgv_TMISramAddrUniqTest(void)
{
   ushort_t   addr    = 0;
   uchar_t    mask   	= 0xff;
   uchar_t    exp;
   __uint32_t *rcvBuf;
   __uint32_t errors 	= 0;
   __uint32_t i = 0;
	
	
   msg_printf(SUM, "IMPV:- %s\n", mgv_err[TMI_SRAM_ADDR_UNIQ_TEST].TestStr);

   rcvBuf = (__uint32_t *)get_chunk(TMI_MAX_SRAM_SIZE * 4);

   /* A[i] = i	*/
   msg_printf(DBG," Address Unique test - i for i\n");

   /*** Init TE side ***/
   _mgv_TE_Init();

   for(addr=0; addr < TMI_MAX_SRAM_SIZE; addr++) {
/***	exp = addr & 0xff; ***/
        exp = 0xde;
	_mgv_TMISramWrite(addr, exp);
   }

   /* read back */
   /* set up the read operation in TMI and call a HQ4 function to get the
      data to memory */

   _mgv_TMIReadyForRead();


#if 0 /* sankar added this */
    mgbase->tdma_format_ctl = 0x1 | 0x2 | 0x10 | 0x30000 ; 

   for (i=0; i < TMI_MAX_SRAM_SIZE; i++) {
      rcvBuf[i] = _mgv_TE_Data();
   }

   /* compare */
   errors += _mgv_TMISramCompare(TMI_ADDR0_TYPE, 0, rcvBuf);

   /* A[i] = ~i	*/
   msg_printf(DBG," Address Unique test - ~i for i\n");

   for(addr=0; addr < TMI_MAX_SRAM_SIZE; addr++) {
	exp = ~addr & 0xff;
	exp = 0xAA ;
	_mgv_TMISramWrite(addr, exp);
   }

   /* readback */
   _mgv_TMIReadyForRead();

   for (i=0; i < TMI_MAX_SRAM_SIZE; i++) {
      rcvBuf[i] = _mgv_TE_Data();
   }
   /* compare */
   errors += _mgv_TMISramCompare(TMI_ADDR1_TYPE, 0, rcvBuf);
   msg_printf(SUM, "IMPV:- Leaving %s\n", mgv_err[TMI_SRAM_ADDR_UNIQ_TEST].TestStr);
#endif 
   return(_mgv_reportPassOrFail((&mgv_err[TMI_SRAM_ADDR_UNIQ_TEST]), errors));
}

/*
 * NAME: mgv_TMISramPatrnTest
 *
 * FUNCTION: TMI SRAM Pattern Test
 *
 * INPUTS: none
 *
 * OUTPUTS: -1 if error, 0 if no error
 */
__uint32_t
mgv_TMISramPatrnTest(void)
{
   ushort_t   addr    = 0;
   uchar_t    mask   	= 0xff;
   uchar_t    exp;
   __uint32_t *rcvBuf;
   __uint32_t index, errors 	= 0;
	
	
   msg_printf(SUM, "IMPV:- %s\n", mgv_err[TMI_SRAM_PATRN_TEST].TestStr);

   rcvBuf = (__uint32_t *)get_chunk(TMI_MAX_SRAM_SIZE * 4);

   for (index = 0; index < (sizeof(mgv_patrn8)/sizeof(uchar_t)); index++) {
	exp = mgv_patrn8[index];

        msg_printf(DBG," TMISramPatrnTest - %0x\n", exp);
	/* write the pattern to all SRAM locations */
	for(addr=0; addr < TMI_MAX_SRAM_SIZE; addr++) {
	     _mgv_TMISramWrite(addr, exp);
	}

   	/* read back */
   	/* set up the read operation in TMI and call a HQ4 function to get the
      	data to memory */
   	/* compare */
	errors += _mgv_TMISramCompare(TMI_PATRN_TYPE, exp, rcvBuf);

   	/* call a function */
   }

   msg_printf(SUM, "IMPV:- Leaving %s\n", mgv_err[TMI_SRAM_PATRN_TEST].TestStr);
   return(_mgv_reportPassOrFail((&mgv_err[TMI_SRAM_ADDR_UNIQ_TEST]), errors));
}

void
_mgv_TMISramWrite(ushort_t addr, uchar_t val)
{

   msg_printf(DBG, "Entering _mgv_TMISramWrite function\n"); 

   /* write the address first */
   TMI_IND_W1(mgvbase, TMI_TEST_ADDR_MS, ((addr >> 8) & 0x7f));
   TMI_IND_W1(mgvbase, TMI_TEST_ADDR_LS, (addr & 0xff));

   /* write the data */
   TMI_IND_W1(mgvbase, TMI_TEST_DATA, val);
   _mgv_TMIReadyForWrite();

   msg_printf(DBG, "Leaving _mgv_TMISramWrite function\n"); 
}

void
_mgv_TMIReadyForWrite(void)
{
   uchar_t tmp_val;

   msg_printf(DBG, "Entering _mgv_TMIReadyForWrite function\n"); 
   /* Set TMI SRAM for write mode by setting TEST_WRITE_BIT and TEST_MODE_BIT */
   tmp_val = (TMI_TEST_WRITE_BIT | TMI_TEST_MODE_BIT);
   msg_printf(DBG,"Val = 0x%x\n", tmp_val);
   TMI_IND_W1(mgvbase, TMI_TEST_MISC, tmp_val);
   msg_printf(DBG, "Leaving _mgv_TMIReadyForWrite function\n"); 

}

void
_mgv_TMIReadyForRead(void)
{
   uchar_t tmp_val;

   msg_printf(DBG, "Entering _mgv_TMIReadyForRead function\n"); 
   /* specify start address to be 0 */
   TMI_IND_W1(mgvbase, TMI_TEST_ADDR_MS,0x0);
   TMI_IND_W1(mgvbase, TMI_TEST_ADDR_LS,0x0);

   /* Set TMI SRAM for Read mode by setting TEST_READ_BIT and TEST_MODE_BIT */
   tmp_val = (TMI_TEST_READ_BIT | TMI_TEST_MODE_BIT);
   msg_printf(DBG,"Val = 0x%x\n", tmp_val);
   TMI_IND_W1(mgvbase, TMI_TEST_MISC, tmp_val);
   msg_printf(DBG, "Leaving _mgv_TMIReadyForRead function\n"); 

}
__uint32_t
_mgv_TMISramCompare(uchar_t type, uchar_t val, __uint32_t *rcvBuf)
{
   __uint32_t i, errors = 0;
     uchar_t j; 
   __uint32_t exp_val;

   msg_printf(DBG, "Entering TMISramCompare function\n");
   switch (type) {
   case TMI_ADDR0_TYPE:
	for (i=0; i < TMI_MAX_SRAM_SIZE; i++) {
	   j = i & 0xff;
	   exp_val = ((j<<24) | (j<<16) | (j<<8) | j);
	   if (rcvBuf[i] != exp_val) {
	      msg_printf(ERR,"_mgv_TMISramCompare test,Addr %d,  Exp %0x, Rcv %0x\n", i, exp_val,rcvBuf[i]);
	      errors++;
              break;
	    }
	}
	break;
   case TMI_ADDR1_TYPE:
	for (i=0; i < TMI_MAX_SRAM_SIZE; i++) {
	   j = ~i & 0xff;
	   exp_val = ((j<<24) | (j<<16) | (j<<8) | j);
	   if (rcvBuf[i] != exp_val) {
	      msg_printf(ERR,"_mgv_TMISramCompare test,Addr %d,  Exp %0x, Rcv %0x\n", i, exp_val, rcvBuf[i]);
	      errors++;
              break;
	    }
	}
	break;
   case TMI_PATRN_TYPE: 
	for (i=0; i < TMI_MAX_SRAM_SIZE; i++) {
	   j = val;
	   exp_val = ((j<<24) | (j<<16) | (j<<8) | j);
	   if (rcvBuf[i] != exp_val) {
	      msg_printf(ERR,"_mgv_TMISramCompare test,Addr %d,  Exp %0x, Rcv %0x\n", i, exp_val,rcvBuf[i]);
	      errors++;
              break;
	    }
	}
	break;
   default:
	msg_printf(SUM,"_mgv_TMISramCompare not performed - Check arguments!\n");
	errors++;
	break;

   }  /* end of Switch */ 
   
   msg_printf(DBG, "Leaving TMISramCompare function\n");
   return(errors);
} /* end of _mgv_TMISramCompare */



__uint32_t
_mgv_TE_Data(void)
{

    ulonglong_t   recv64, tmp64, mask;
    __uint32_t    rcvb32 = 0x0;

    recv64 = mgbase->vdma_fifo ; /* this is where data is */

    msg_printf(DBG," Recv64 = 0x%lx\n",recv64); 
  /* A 32 bit RGBA is converted in to 64 bi quantity like this */
  /* 0x00ff0ff000ff0ff0 */
 /* mask = 0x00GG0RR0 00AA0BB0 */
  
  rcvb32 = ((recv64 >> 12) &0xff000000) | ((recv64 >> 32) & 0x00ff0000) | 
           ((recv64 >> 4) & 0xff) | ((recv64 >> 8) & 0xff00);
  msg_printf(DBG," Recv32 = 0x%x\n",rcvb32); 

  return (rcvb32);

}

void
_mgv_TE_Init()
{

    /* Test if dma_abort helps */
    mgbase->vdma_ctl = 0x2 ; /* clear the dma_abort bit */
    msg_printf(DBG, " setting the DMA abort bit \n");

    mgbase->tdma_format_ctl = 0x1 | 0x2 | 0x10 | 0x10000 ;
    /* Video input port, input = 2, , output = 0x10  , diagmode */
}

#endif
