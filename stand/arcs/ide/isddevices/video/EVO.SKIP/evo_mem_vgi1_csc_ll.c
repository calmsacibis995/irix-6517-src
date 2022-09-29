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
 *	Filename:  evo_mem_vgi1_csc_ll.c
 *	Purpose:  to test the memory-vgi-csc-memory path by sending a series of
 * 		  data patterns from memory through the vgi1 on to the CSC and 
 *		  looping it back.
 *		  
 *		  The VBAR is used to send data to and from the VGI1 and the CSC
 *		  The CSC is first set to byopass mode.
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

__uint32_t  _evo_vgi_mem_csc_loopback()
{
	__uint32_t t_yval = 0x08080808; 
	__uint32_t t_errors; 
	__uint32_t  i, AllBufSize, dataBufSize, tmp;


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
	dataBufSize =(evobase->transferSize + evobase->pageSize + 32);
        AllBufSize = dataBufSize + (CSC_MAX_LOAD * sizeof(u_int16_t)) + (CSC_MAX_COEF*sizeof(u_int16_t));

        msg_printf(DBG, "AllBufSize 0x%x\n", AllBufSize);
        if ((evobase->pVGIDMABuf = (uchar_t *)get_chunk(AllBufSize)) == NULL) {
            msg_printf(ERR, "Insufficient memory for Read Data Buffer\n");
            return(-1);
        } else {
                     for (i = 0; i < (dataBufSize >> 2); i++) {
                        *(((__uint32_t *)evobase->pVGIDMABuf) + i) = t_yval;
                     }
        }
        msg_printf(DBG, "pBuf 0x%x; dataBufSize 0x%x\n",
                              evobase->pVGIDMABuf, dataBufSize);

	evobase->lutbuf = (u_int16_t *)evobase->pVGIDMABuf + dataBufSize; 
	evobase->coefbuf = evobase->lutbuf + (CSC_MAX_LOAD * sizeof(u_int16_t)); 

	/*DMA Read Mem-Vid*/
        /*                   vgi_num,chan,op*/
        t_errors = evo_VGI1DMATest(1, 1, 1, evobase->vidFmt, evobase->tvStd, 
			evobase->nFields, evobase->vgi1[1].stride, evobase->vin_round, evobase->vout_expd,
				    evobase->vout_freeze, t_yval, 0, evobase->pageSize, 0);

	/* Set up VBAR to route output to CSC and back to VGI1 */
	/*XXX Check this portion...inputs are Ch1 and outputs are Ch2*/
	/*XXX So the data path is Mem-VGI1AC1-CSCC1-VGI1AC2-Mem*/

	EVO_SET_VBAR1_BASE;
	EVO_VBAR_W1(evobase, EVOA_CSC_1_A, MUX_VGI1_CH1_IN);
	EVO_VBAR_W1(evobase, EVOA_VGI1_CH2_IN, MUX_CSC_1_A);

	/*Set up CSC to be pass through: CSC1, YUV I/p & O/p Formats, I/p and O/p DIFs disabled, bypass mode*/

	/*         evo_cscTest(m_csc_num, mi_fmt, mo_fmt, mi_dif, mo_dif, m_bypass, m_a_kin, m_a_kop)*/
	t_errors = evo_cscTest(1, 1, 1, 0, 0, 1, 0, 0);
	

	/*DMA Write vid-mem*/
        /*                   vgi_num,chan,op*/
        t_errors = evo_VGI1DMATest(1, 2, 2, evobase->vidFmt, evobase->tvStd, 
			evobase->nFields, evobase->vgi1[1].stride, evobase->vin_round, evobase->vout_expd,
				    evobase->vout_freeze, t_yval, 1, evobase->pageSize, 0);


/*XXX Compare outputs in memory*/
/* XXX: compare the buffers */
    /* For now check if all the values are same and equal to t_yval */
    for (i = 0; i < (evobase->transferSize >> 2); i++) {
        tmp = *(((__uint32_t *)evobase->pVGIDMABuf) + i);
        if (tmp != t_yval) {
           msg_printf(ERR, "Data Mismatch at 0x%x; exp 0x%x; rcv 0x%x\n",
                i, t_yval, tmp);
           t_errors++;
        }
    }

    return(t_errors);
}
