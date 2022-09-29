 /*************************************************************************
 *                                                                        *
 *               Copyright (C) 1995 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *	Filename:  evo_mem_vgi1_mem.c
 *	Purpose:  to test the memory-vgi-memory path by sending a series of
 * 		  data patterns from memory through the vgi1 and looping it
 *  		  back.
 * 		
 *		  Pass/Fail is determined based on comparing data to and from
 *		  memory.
 *
 *		  RETURNS: None
 *		  
 *      $Revision: 1.1 $
 *
 */

#include "evo_diag.h"

__uint32_t _evo_vgi_mem_loopback()
{
	__uint32_t t_yval = 0x08080808; 
	__uint32_t t_errors, dataBufSize; 
	int i = 0;
	__uint32_t tmp = 0;

/* Put as many working patterns in this array as you want displayed on the screen.  However ALWAYS end the array with 0xFF!! */
vout_patterns_t vid_gonogo[] = { WALK1_PTRN, WALK0_PTRN, ALT01_PTRN, ALT10_PTRN, 0xFF };


#if 0
	/* reset EVO board */
	msg_printf(JUST_DOIT,"\nTHE EVO BOARD IS BEING RESET- PLEASE WAIT\n");
	_evo_reset(1);
	delay( 3000 );
	msg_printf(JUST_DOIT,"\nRESET COMPLETE\n");
	SET_EVO_VGI1_1_BASE;
	_evo_init_VGI1();
	SET_EVO_VGI1_2_BASE;
	_evo_init_VGI1();
	/*XXX Reset other components */
#endif

       /* generate the data in memory */
       evobase->transferSize = _evo_VGI1DMAGetBufSize();
       dataBufSize = evobase->transferSize + evobase->pageSize + 32;

       msg_printf(DBG, "dataBufSize 0x%x\n", dataBufSize);
       if ((evobase->pVGIDMABuf = (uchar_t *)get_chunk(dataBufSize)) == NULL) {
           msg_printf(ERR, "Insufficient memory for Read Data Buffer\n");
           return(-1);
       } else {

                    for (i = 0; i < (dataBufSize >> 2); i++) {
                       *(((__uint32_t *)evobase->pVGIDMABuf) + i) = t_yval;
                    }
       }
       msg_printf(DBG, "pBuf 0x%x; dataBufSize 0x%x\n",
                             evobase->pVGIDMABuf, dataBufSize);



	/*DMA Read Mem-Vid*/
        /*                   vgi_num,chan,op*/
        t_errors = evo_VGI1DMATest(1, 1, 1, evobase->vidFmt, evobase->tvStd, 
			evobase->nFields, evobase->vgi1[1].stride, evobase->vin_round, evobase->vout_expd,
				    evobase->vout_freeze, t_yval, 0, evobase->pageSize, 0);

	/*DMA Write vid-mem*/
        /*                   vgi_num,chan,op*/
        t_errors = evo_VGI1DMATest(1, 1, 2, evobase->vidFmt, evobase->tvStd, 
			evobase->nFields, evobase->vgi1[1].stride, evobase->vin_round, evobase->vout_expd,
				    evobase->vout_freeze, t_yval, 1, evobase->pageSize, 0);


/*XXX Compare outputs in memory*/
    /* For now check if all the values are same and equal to yval */
    for (i = 0; i < (evobase->transferSize >> 2); i++) {
        tmp = *(((__uint32_t *)evobase->pVGIDMABuf) + i);
        if (tmp != t_yval) {
           msg_printf(ERR, "Data Mismatch at 0x%x; exp 0x%x; rcv 0x%x\n",
                i, t_yval, tmp);
           t_errors++;
        }
    }
    return(_evo_reportPassOrFail((&evo_err[VGI1_DMA_TEST]), t_errors));

}
