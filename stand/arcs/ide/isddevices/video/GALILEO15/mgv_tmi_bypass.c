/*
**               Copyright (C) 1989, Silicon Graphics, Inc.
**
**  These coded instructions, statements, and computer programs  contain
**  unpublished  proprietary  information of Silicon Graphics, Inc., and
**  are protected by Federal copyright law.  They  may  not be disclosed
**  to  third  parties  or copied or duplicated in any form, in whole or
**  in part, without the prior written consent of Silicon Graphics, Inc.
*/

/*
**      FileName:       mgv_tmi_util.c
*/

#ifdef IP30

#include "mgv_diag.h"

/*
 * Forward Function References
 */
__uint32_t 	mgv_TMIByePassTest (void);
ushort_t        _mgv_TEXRead(void);
void		_mgv_TEXinit(void);

/*
 * NAME: mgv_TMIByePassTest
 *
 * FUNCTION: TMI Bye Pass Mode Test
 *
 * INPUTS: none
 *
 * OUTPUTS: -1 if error, 0 if no error
 */
__uint32_t
mgv_TMIByePassTest (void)
{

    uchar_t 	val, exp, rcv_lo, rcv_hi;
    __uint32_t	i, pat, errors=0;
    ushort_t	rcvBuf;

    msg_printf(SUM, "IMPV:- %s\n", mgv_err[TMI_BYE_PASS_TEST].TestStr);

    TMI_IND_R1(mgvbase, TMI_CHIP_ID, val);
    msg_printf(DBG, "TMI CHip ID = 0x%x\n", val);
    
    _mgv_TEXinit(); 

    val = 0;
    /* Set TMI for by-pass mode by setting TEST_TE_BIT*/
    /*    val = (TMI_TEST_MODE_BIT | TMI_TEST_TE_BIT);
    TMI_IND_W1(mgvbase, TMI_TEST_MISC, val);  */

    /*** Initialize graphics side ti receive data ***/

/*    mgbase->tdma_format_ctl = 0x10013; */

    /* Send the data over to TMI, in bye-pass mode */
	/* Write the data */
      	TMI_IND_W1(mgvbase, TMI_TEST_DATA, 0x12);
      	/***TMI_IND_W1(mgvbase, TMI_TEST_DATA, tmi_bypass_patrn[i]); ***/

        val = 0;
        /* Set TMI for by-pass mode by setting TEST_TE_BIT*/
        val = (TMI_TEST_MODE_BIT | TMI_TEST_TE_BIT);
        TMI_IND_W1(mgvbase, TMI_TEST_MISC, val);
        /* Read the data back from the HQ4 side */
        /* and will be in rcvBuf */
#if 0
        rcvBuf = 0x0;
        rcvBuf = _mgv_TEXRead();

        /* compare */
	rcv_lo = rcvBuf & 0xff;
	rcv_hi = (rcvBuf >> 8) & 0xff;
	exp = tmi_bypass_patrn[i];
	if ((rcv_lo != exp) || (rcv_hi != exp))  {
	   errors++;
	   msg_printf(ERR, "TMI BYE-PASS: exp 0x%x; rcv_hi 0x%x; rcv_lo 0x%x\n",
			exp, rcv_hi, rcv_lo);
	}
#endif
    return(_mgv_reportPassOrFail((&mgv_err[TMI_BYE_PASS_TEST]), errors));
}


ushort_t
_mgv_TEXRead()
{

    ulonglong_t   recv64, tmp64, mask;
    ushort_t      rcvb16 = 0x0;
    ushort_t      tmp16 = 0x0;


    recv64 = mgbase->vdma_fifo ; /* this is where data is */ 

  /* A 32 bit RGBA is converted in to 64 bi quantity like this */
  /* 0x00ff0ff000ff0ff0 */
 /* mask = 0x00GG0RR0 00AA0BB0 */
 /* 8=J,8=G,4=J,8=R,4=J,8=J,8=A,4=J,8=B,4=J */ 
  rcvb16 = (recv64 >> 4) & 0xff;
  tmp16 = ((recv64 >> 16) & 0xff);
  rcvb16 = (rcvb16 << 8) | tmp16;

  return (rcvb16);

}
void
_mgv_TEXinit()
{

    /* Test if dma_abort helps */
    mgbase->vdma_ctl = 0x2 ; /* clear the dma_abort bit */
    msg_printf(DBG, " setting the DMA abort bit \n");

    mgbase->tdma_format_ctl = 0x1 | 0x2 | 0x10 | 0x10000 ;
    /* Video input port, input = 2, , output = 0x10  , diagmode */

}

#endif
