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
**      FileName:       evo_vgi1.c
*/

#include "evo_diag.h"
#if (defined(IP22) || defined(IP28))
#include "sys/cpu.h"
#endif

/*
 * Forward Function References
 */

__uint32_t  evo_VGI1(__uint32_t argc, char **argv);

	__uint32_t  _evo_VGI1DMAValidateArgs(__uint32_t chan);
	__uint32_t evo_VGI1DMATest (__uint32_t vgi_num, __uint32_t chan, __uint32_t op,
				    __uint32_t vidfmt, __uint32_t tvStd,  __uint32_t nFields,
				    ulonglong_t stride, __uint32_t round, __uint32_t expd, 
				    __uint32_t freeze, __uint32_t  yval, ulonglong_t inv, 
		 		    __uint32_t pgsiz, __uint32_t blank);


		__uint32_t  _evo_VGI1DMAGetBufSize(void);
		__uint32_t  _evo_dmaPixelDepth (__uint32_t giofmt);

		__uint32_t  _evo_VGI1DMA(void);
			/* evo_VGI1DMAGetBufSize called */	
			void  _evo_VGI1InitDMAParam (void);
			void  _evo_VGI1BuildDMADescTable(uchar_t *pDesc);
			__uint32_t  _evo_VGI1ProgramDMARegs(__uint32_t chan);
			__uint32_t  _evo_VGI1StartAndProcessDMA(void);

		__uint32_t  _evo_VGIDMAClear(__uint32_t chan);

		__uint32_t  _evo_VGI1DMADispErr(void);
			__uint32_t  _evo_VGI1DMADispErrActual(ulonglong_t err, char *str);

/*
 * NAME: evo_VGI1
 *
 * FUNCTION: top level function for VGI1
 *
 * INPUTS: argc and argv
 *
 * OUTPUTS: -1 if error, 0 if no error
 * 
 * Old Galileo format:
 *	Usage: evo_vgi1 -c [1|2] -o [1|2|3] -t [1|2|3|4] -v [0|1|2|3]\n\ -f [1|2] -z [0|1]\n\
 *			-c --> channel 1[2] \n\
 * 		        -o --> operation 1-mem2vid 2-vid2mem 3-both\n\
 *		        -t --> TV standard 0-NTSC601 1-NTSCSQ 2-PAL601 3-PALSQ\n\
 *		        -v --> Video Format 0-8YUV422 1-10YUV422 2-8ARB 3-10FULL ...\n\
 *		        -f --> Number of fields 1 or 2\n\
 *		        -a --> VGI Asic 1[2] \n\
 *		        -z --> Freeze 0 or #of fields to DMA\n\
 *
 * Proposed format:
 *      Usage: evo_vgi1 -a [1|2] -c [1|2] -o [1|2|3] -t [1|2|3|4] -v [0|1|2|3]\n\ -f [1|2] -z [0|1]\n\
 *                      -a --> VGI Asic 1[2] \n\
 *                      -c --> channel 1[2] \n\
 *                      -o --> operation 1-mem2vid 2-vid2mem 3-both\n\
 *                      -v --> Video Format 0-8YUV422 1-10YUV422 2-8ARB 3-10FULL ...\n\
 *                      -t --> TV standard 0-NTSC601 1-NTSCSQ 2-PAL601 3-PALSQ\n\
 *                      -f --> Number of fields 1 or 2\n\
 *			-s --> stride
 *			-r --> round
 *			-e --> expand
 *                      -z --> Freeze 0 or #of fields to DMA\n\
 *			-y --> yval
 *			-i --> invert
 *			-p --> page size
 *			-b --> blank
 *
 *
 */
__uint32_t
evo_VGI1(__uint32_t argc, char **argv)
{
    __uint32_t	m_errors = 0;
    __uint32_t	badarg = 0;
    __uint32_t	i, dataBufSize;
    __uint32_t	m_yval, tmp, m_op;
    ulonglong_t m_inv;
    __uint32_t  tmp_inv, tmp_stride;
    __uint32_t	m_chan, m_blank;
    __uint32_t	m_vgi_num = 1;


    m_chan = VGI1_CHANNEL_1;
    m_op = 0x3;
    evobase->vidFmt = EVO_VGI1_FMT_YUV422_8;
    evobase->tvStd = VGI1_NTSC_CCIR601;
    evobase->nFields = 2;
    evobase->vgi1[m_vgi_num].stride = 0;
    evobase->vin_round = EVO_VGI1_VIN_RND_OFF;
    evobase->vout_expd = EVO_VGI1_VOUT_EXPD_OFF;
    evobase->vout_freeze = 0;
    m_yval = 0x80808080;
    m_inv = EVO_VGI1_FLD_INV_OFF;
    evobase->pageSize = EVO_VGI1_PGSZ_64K;
    m_blank = EVO_VGI1_VO_BLK_YUVA;
    /* get the args */
    argc--; argv++;
    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 'a':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &m_vgi_num); /*which VGI into vgi_num*/
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &(m_vgi_num));
			}
                        break;
                case 'c':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &(m_chan)); /*reads channel number into chan*/
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &(m_chan));
			}
			m_chan--;
                        break;
                case 'o':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &m_op); /*reads operation type into op*/
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &m_op);
			}
                        break;
                case 'v':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &(evobase->vidFmt)); /*getting Video Format*/
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &(evobase->vidFmt));
			}
                        break;
                case 't':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &(evobase->tvStd)); /*getting TV Std.*/
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &(evobase->tvStd));
			}
                        break;
                case 'f':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &(evobase->nFields)); /*Getting number of Fields*/
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &(evobase->nFields));
			}
                        break;
                case 's':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &(tmp_stride)); /*Getting dma stride*/
			    evobase->vgi1[m_vgi_num].stride = (ulonglong_t)tmp_stride;
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &(tmp_stride));
			    evobase->vgi1[m_vgi_num].stride = (ulonglong_t)tmp_stride;
			}
                        break;
                case 'r':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &(evobase->vin_round)); /*Getting rounding / truncation options*/
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &(evobase->vin_round));
			}
                        break;
                case 'e':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &(evobase->vout_expd)); /*Expansion options*/
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &(evobase->vout_expd));
			}
                        break;
                case 'z':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &(evobase->vout_freeze)); /*Freeze options*/
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &(evobase->vout_freeze));
			}
                        break;
                case 'y':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &m_yval); 
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &m_yval);
			}
                        break;
                case 'i':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &tmp_inv); 
			    m_inv = (ulonglong_t)tmp_inv;
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &tmp_inv);
			    m_inv = (ulonglong_t)tmp_inv;
			}
                        break;
                case 'p':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &(evobase->pageSize)); 
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &(evobase->pageSize));
			}
                        break;
                case 'b':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &m_blank); 
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &m_blank);
			}
                        break;
                default: badarg++; break;
        }
        argc--; argv++;
    }

    if (badarg || argc || _evo_VGI1DMAValidateArgs(m_chan) ) {
	msg_printf(SUM, "\
	Usage: evo_vgi -a [1|2] -c [1|2] -o [1|2|3]  -v [0|1|2|3] -t [1|2|3|4]\n\
	        -f [1|2] -s [#Bytes] -r [0|1] -e [0|1] -z [0|1] -y [yval] -i [0|1] \n\
		-p [0|1|2] -b [0|1] \n\
	-a --> VGI Asic 1[2] \n\
	-c --> channel 1[2] \n\
	-o --> operation 1-mem2vid 2-vid2mem 3-both\n\
	-v --> Video Format 0-8YUV422 1-10YUV422 2-8ARB 3-10FULL ...\n\
	-t --> TV standard 0-NTSC601 1-NTSCSQ 2-PAL601 3-PALSQ\n\
	-f --> Number of fields 1 or 2\n\
	-s --> DMA Stride in Bytes\n\
	-r --> Rounding 0-Disable 1-Enable\n\
	-e --> Expand 0-Disable  1-Enable (Sets 2 LSBs to 0)\n\
	-z --> Freeze 0 or # of fields to DMA\n\
	-y --> yval (walking pattern)\n\
	-i --> Invert 0-Invert (DMA Read Normal), 1-No Invert (DMA Write Normal)\n\
	-p --> Page Size 0-4K 1-16K 2-64K\n\
	-b --> blanking level 0-YUVA 1-RGBA\n\
	");
	return(-1);
    } else {
             /* generate the data in memory */
             evobase->transferSize = _evo_VGI1DMAGetBufSize();
             dataBufSize = evobase->transferSize + evobase->pageSize + 32;

             msg_printf(DBG, "dataBufSize 0x%x\n", dataBufSize);
             if ((evobase->pVGIDMABuf = (uchar_t *)get_chunk(dataBufSize)) == NULL) {
	         msg_printf(ERR, "Insufficient memory for Read Data Buffer\n");
	         return(-1);
             } else {
	                  for (i = 0; i < (dataBufSize >> 2); i++) {
	                     *(((__uint32_t *)evobase->pVGIDMABuf) + i) = m_yval;
	                  }
             }
             msg_printf(DBG, "pBuf 0x%x; dataBufSize 0x%x\n",
	                           evobase->pVGIDMABuf, dataBufSize);

	    m_errors = evo_VGI1DMATest(m_vgi_num, m_chan, m_op, evobase->vidFmt, evobase->tvStd, 
	    evobase->nFields, evobase->vgi1[m_vgi_num].stride, evobase->vin_round, evobase->vout_expd,
	    evobase->vout_freeze, m_yval, m_inv, evobase->pageSize, m_blank);
    }
    return(m_errors);

}




__uint32_t
evo_VGI1DMATest (__uint32_t vgi_num, __uint32_t chan, __uint32_t op, __uint32_t vidfmt, 
		 __uint32_t tvStd,  __uint32_t nFields, ulonglong_t stride, __uint32_t round,
		 __uint32_t expd, __uint32_t freeze, __uint32_t  yval, ulonglong_t inv, 
		 __uint32_t pgsiz, __uint32_t blank)

{
    __uint32_t	errors = 0;
    __uint32_t	badarg = 0;
    __uint32_t	i, dataBufSize, tmp;
    unsigned char       xp;

    evobase->vidFmt = vidfmt;
    evobase->tvStd = tvStd;
    evobase->nFields = nFields;
    evobase->vgi1[vgi_num].stride = stride;
    evobase->vin_round = round;
    evobase->vout_expd = expd;
    evobase->vout_freeze = freeze;
    evobase->pageSize = pgsiz;
    dataBufSize = evobase->transferSize + evobase->pageSize + 32;
    /* Set vgibase address to VGI_1 or VGI_2 */
    if (vgi_num == 2) {
	EVO_SET_VGI1_2_BASE;
    } else { 
	EVO_SET_VGI1_1_BASE;
    }

    msg_printf(SUM, "IMPV:- %s -c 0x%x; -o 0x%x; -t 0x%x; -v 0x%x; -f 0x%x; -z 0x%x\n", 
	evo_err[VGI1_DMA_TEST].TestStr, (chan+1), op, evobase->tvStd,
	evobase->vidFmt, evobase->nFields, evobase->vout_freeze);

    /* set default timing */
    EVO_W1 (evobase, MISC_REF_OFFSET_LO, LOBYTE(BOFFSET_CENTER));
    EVO_W1 (evobase, MISC_REF_OFFSET_HI, HIBYTE(BOFFSET_CENTER));

    if (op & 0x1) {
	if (vgi_num == 1) {
    		/* Send Black Reference to both channels of VBAR1 for DMA READ */
		/* The path is BlackRef -> VBar2 (VBar1Ch1Out) -> VBar1 (VBar2Ch1In) -> VGI1A CH1/2 In*/
		EVO_SET_VBAR2_BASE;
    		xp =  MUXB_BLACK_REF_IN ;
    		EVO_VBAR_W1(evobase, EVOB_TO_OTHER_VBAR_CH1, xp); /*XXX Check VBAR1/2 Address*/
		EVO_SET_VBAR1_BASE;
    		xp = MUX_TO_OTHER_VBAR_CH1;
    		EVO_VBAR_W1(evobase, EVOA_VGI1_CH1_IN, xp); /*XXX Check VBAR1/2 Address*/
    		EVO_VBAR_W1(evobase, EVOA_VGI1_CH2_IN, xp); /*XXX Check VBAR1/2 Address*/

	} else {
    		/* Send Black Reference to both channels of VBAR2 for DMA READ */
		/* The path is BlackRef ->  VGI1B CH1/2 In*/
		EVO_SET_VBAR2_BASE;
    		xp =  MUXB_BLACK_REF_IN;
    		EVO_VBAR_W1(evobase, EVOB_VGI1B_CH1_IN, xp); /*XXX Check VBAR1/2 Address*/
    		EVO_VBAR_W1(evobase, EVOB_VGI1B_CH2_IN, xp); /*XXX Check VBAR1/2 Address*/
	} 
	/*XXX Setting up for VGI1-2 needs to be done
	      May use a module-parameter...*/

    	msg_printf(SUM, "\nDMA Read - from mem to video....\n");
    	evobase->chan 			= chan;
    	evobase->VGIDMAOp		= VGI1_DMA_READ;
    	evobase->VGI1DMAOpErrStatus 	= 0;
    	evobase->VGI1DMAClrErrStatus 	= 0;
	tmp = _evo_VGI1DMA();
	if (tmp == VGI1_NO_VIDEO_INPUT) {
	    return(0x0);
	} else {
	    errors += tmp;
	}
    	if (errors) goto __end_of_dma;

    	msg_printf(SUM, "Buffer: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		*(((__uint32_t *)evobase->pVGIDMABufAl) + 0), 
		*(((__uint32_t *)evobase->pVGIDMABufAl) + 1),
		*(((__uint32_t *)evobase->pVGIDMABufAl) + 2), 
		*(((__uint32_t *)evobase->pVGIDMABufAl) + 3),
		*(((__uint32_t *)evobase->pVGIDMABufAl) + 4), 
		*(((__uint32_t *)evobase->pVGIDMABufAl) + 5));
    }

    if (op & 0x2) {

    	/* corrupt the buffer since we're using the same */
    	for (i = 0; i < (dataBufSize >> 2); i++) {
   		*(((__uint32_t *)evobase->pVGIDMABuf) + i) = ~yval;
    	}

    	msg_printf(SUM, "\nDMA Write - from video to mem....\n");

	if (vgi_num == 1) {
    		/* Send Black Reference to both channels of VBAR1 for DMA READ */
		/* The path is BlackRef -> VBar2 (VBar1Ch1Out) -> VBar1 (VBar2Ch1In) -> VGI1A CH1/2 In*/
		EVO_SET_VBAR2_BASE;
    		xp =  MUXB_BLACK_REF_IN ;
    		EVO_VBAR_W1(evobase, EVOB_TO_OTHER_VBAR_CH1 , xp); /*XXX Check VBAR1/2 Address*/
		EVO_SET_VBAR1_BASE;
    		xp = MUX_TO_OTHER_VBAR_CH1;
    		EVO_VBAR_W1(evobase, EVOA_VGI1_CH1_IN, xp); /*XXX Check VBAR1/2 Address*/
    		EVO_VBAR_W1(evobase, EVOA_VGI1_CH2_IN, xp); /*XXX Check VBAR1/2 Address*/

	} else {
    		/* Send Black Reference to both channels of VBAR2 for DMA READ */
		/* The path is BlackRef ->  VGI1B CH1/2 In*/
		EVO_SET_VBAR2_BASE;
    		xp =  MUXB_BLACK_REF_IN;
    		EVO_VBAR_W1(evobase, EVOB_VGI1B_CH1_IN , xp); /*XXX Check VBAR1/2 Address*/
    		EVO_VBAR_W1(evobase, EVOB_VGI1B_CH2_IN , xp); /*XXX Check VBAR1/2 Address*/
	} 
    	evobase->chan 			= chan;
    	evobase->VGIDMAOp		= VGI1_DMA_WRITE;
    	evobase->VGI1DMAOpErrStatus 	= 0;
    	evobase->VGI1DMAClrErrStatus 	= 0;
	tmp = _evo_VGI1DMA();
	if (tmp == VGI1_NO_VIDEO_INPUT) {
	    return(0x0);
	} else {
	    errors += tmp;
	}
    	if (errors) goto __end_of_dma;

    	msg_printf(SUM, "Buffer: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		*(((__uint32_t *)evobase->pVGIDMABufAl) + 0), 
		*(((__uint32_t *)evobase->pVGIDMABufAl) + 1),
		*(((__uint32_t *)evobase->pVGIDMABufAl) + 2), 
		*(((__uint32_t *)evobase->pVGIDMABufAl) + 3),
		*(((__uint32_t *)evobase->pVGIDMABufAl) + 4), 
		*(((__uint32_t *)evobase->pVGIDMABufAl) + 5));
    }

#if 0
    /* XXX: compare the buffers */
    /* For now check if all the values are same and equal to yval */
    for (i = 0; i < (evobase->transferSize >> 2); i++) {
   	tmp = *(((__uint32_t *)evobase->pVGIDMABuf) + i);
	if (tmp != yval) {
	   msg_printf(ERR, "Data Mismatch at 0x%x; exp 0x%x; rcv 0x%x\n",
		i, yval, tmp);
	   errors++;
	   goto __end_of_dma;
	}
    }
#endif

__end_of_dma:
    /* Verify that the chip is good shape to accept DMA request */
    errors += _evo_VGIDMAClear(chan);

    errors += _evo_VGI1DMADispErr();

/* Set vgibase to VGI_1 before returning */
    EVO_SET_VGI1_1_BASE;

    return(_evo_reportPassOrFail((&evo_err[VGI1_DMA_TEST]), errors));
}

__uint32_t
_evo_VGI1DMA(void)
{
    __uint32_t	errors = 0;
    __uint32_t	chanOutCtrl;
    unsigned char	vin1Status, vin2Status;

    msg_printf(DBG, "Entering _evo_VGI1DMA...");

    /*
     * First make sure that the channel has input for DMA Write
     */
    if (evobase->VGIDMAOp == VGI1_DMA_WRITE) {
	EVO_R1(evobase, MISC_VIN_CH1_STATUS, vin1Status);
	EVO_R1(evobase, MISC_VIN_CH2_STATUS, vin2Status);
	if (evobase->dualLink) {
	    if ((vin1Status & 0x10) || (vin2Status & 0x10)) {
		if (vin1Status & 0x10) {
		   msg_printf(SUM, "No Input on Channel 1\n");
		}
		if (vin2Status & 0x10) {
		   msg_printf(SUM, "No Input on Channel 2\n");
		}
		return(VGI1_NO_VIDEO_INPUT);
	    }
	} else {
	    if (evobase->chan == VGI1_CHANNEL_1) {
		if (vin1Status & 0x10) {
		   msg_printf(SUM, "No Input on Channel 1\n");
		   return(VGI1_NO_VIDEO_INPUT);
		}
	    } else {
		if (vin2Status & 0x10) {
		   msg_printf(SUM, "No Input on Channel 2\n");
		   return(VGI1_NO_VIDEO_INPUT);
		}
	    }
	}
    }

    evobase->VGI1DMAOpErrStatus = 0;
    evobase->VGI1DMAClrErrStatus = 0;

    evobase->dualLink = 0x1;
    switch (evobase->vidFmt) {
        case EVO_VGI1_FMT_YUV422_8:
        case EVO_VGI1_FMT_ARB_8:
        case EVO_VGI1_FMT_YUV422_10:
		evobase->dualLink = 0x0;
		break;
    }

    /* figure out the DMA transfer size */
    evobase->transferSize = _evo_VGI1DMAGetBufSize();

    evo_pVGIDMADesc = evo_VGIDMADesc;

    msg_printf(DBG, "pDesc 0x%x;\n", evo_pVGIDMADesc);

    /* set up TV standard */
    EVO_W1(evobase, MISC_TV_STD, evobase->tvStd);
    delay(300);

    /* initialize the parameters for DMA in software */
    _evo_VGI1InitDMAParam();

    /* unfreeze the video out */
    chanOutCtrl = evobase->vgi1[evobase->chan].ch_vout_ctrl_reg_offset;
    msg_printf(DBG, "Freezing video out chanOutCtrl 0x%x\n",
		chanOutCtrl);
    EVO_W1 (evobase, chanOutCtrl, 0x1);

    /* build the DMA descriptor table */
    _evo_VGI1BuildDMADescTable(evo_pVGIDMADesc);

    /* program the Channel(s) */
    /* if dual-link program the other channel too */
    if (evobase->dualLink) {
	errors += _evo_VGI1ProgramDMARegs(VGI1_CHANNEL_1);
	errors += _evo_VGI1ProgramDMARegs(VGI1_CHANNEL_2);
    } else {
	errors += _evo_VGI1ProgramDMARegs(evobase->chan);
    }

    /* Start the DMA */
    errors += _evo_VGI1StartAndProcessDMA();

    /* Verify that the chip is good shape to accept DMA request */
    if (evobase->dualLink) {
    	errors += _evo_VGIDMAClear(VGI1_CHANNEL_1);
    	errors += _evo_VGIDMAClear(VGI1_CHANNEL_2);
    } else {
    	errors += _evo_VGIDMAClear(evobase->chan);
    }

    return(errors);
}

__uint32_t	
_evo_VGI1DMAValidateArgs(__uint32_t chan)
{
    __uint32_t	errors = 0;

    if ((chan != VGI1_CHANNEL_1) && (chan != VGI1_CHANNEL_2)) {
	msg_printf(ERR, "Incorrect channel number\n");
	errors++;
    }
    switch (evobase->tvStd) {
      	case VGI1_NTSC_CCIR601:
      	case VGI1_NTSC_SQUARE:
      	case VGI1_PAL_CCIR601:
      	case VGI1_PAL_SQUARE:
		break;
      	default:
		msg_printf(ERR, "Incorrect TV Standard\n");
		errors++;
		break;
    }
    switch (evobase->vidFmt) {
	case EVO_VGI1_FMT_YUV422_8:
	case EVO_VGI1_FMT_ARB_8:
	case EVO_VGI1_FMT_YUV422_10:
	case EVO_VGI1_FMT_AUYV4444_8:
	case EVO_VGI1_FMT_ABGR_8:
	case EVO_VGI1_FMT_VYUA4444_8:
	case EVO_VGI1_FMT_RGBA_8:
	case EVO_VGI1_FMT_ARB_10:
	case EVO_VGI1_FMT_AYUAYV4224_10:
	case EVO_VGI1_FMT_AUYV4444_10:
	case EVO_VGI1_FMT_VYUA4444_10:
	case EVO_VGI1_FMT_ABGR_10:
	case EVO_VGI1_FMT_RGBA_10:
		break;
      	default:
		msg_printf(ERR, "Incorrect Video (GIO) Format\n");
		errors++;
		break;
    }
    if ((evobase->nFields != 1) && (evobase->nFields != 2)) {
	msg_printf(ERR, "Incorrect number of fields\n");
	errors++;
    }
    return(errors);
}

__uint32_t
_evo_VGI1DMAGetBufSize(void)
{
    __uint32_t	bufSize, pixDepth;

    pixDepth = _evo_dmaPixelDepth(evobase->vidFmt);

    switch (evobase->tvStd) {
      case VGI1_NTSC_CCIR601:
	bufSize = (VSTD_CCIR_525_H_LEN * VSTD_CCIR_525_O_LEN * pixDepth);
	break;
      case VGI1_PAL_CCIR601:
	bufSize = (VSTD_CCIR_625_H_LEN * VSTD_CCIR_625_O_LEN * pixDepth);
	break;
      case VGI1_NTSC_SQUARE:
	bufSize = (VSTD_NTSC_SQR_H_LEN * VSTD_NTSC_SQR_O_LEN * pixDepth);
	break;
      case VGI1_PAL_SQUARE:
	bufSize = (VSTD_PAL_SQR_H_LEN * VSTD_PAL_SQR_O_LEN * pixDepth);
	break;
    }
    return(bufSize);
}

/*
 * NAME: _evo_VGI1InitDMAParam
 *
 * FUNCTION: initialize the DMA parameters in the structure
 *
 * INPUTS: 
 *
 * OUTPUTS: None
 */
void
_evo_VGI1InitDMAParam (void)
{
    __uint32_t	chan;
    evo_vgi1_info_t	*pvgi1 = &(evobase->vgi1[0]);
    evo_vgi1_info_t	*pvgi2 = &(evobase->vgi1[1]);
    evo_vgi1_info_t	*pvgi;
    evo_vgi1_ch_setup_t	chSetup = {0LL};
    evo_vgi1_h_param_t	hp = {0LL};
    evo_vgi1_v_param_t	vp = {0LL};

    msg_printf(DBG, "Entering _evo_VGI1InitDMAParam...\n"); 

    /* figure out the H and V parameters */
    switch (evobase->tvStd) {
	case  VGI1_NTSC_CCIR601:
		hp = hpNTSC; vp = vpNTSC;
		break;
	case  VGI1_PAL_CCIR601:
		hp = hpPAL; vp = vpPAL;
		break;
	case  VGI1_NTSC_SQUARE:
		hp = hpNTSCSQ; vp = vpNTSCSQ;
		break;
	case  VGI1_PAL_SQUARE:
		hp = hpPALSQ; vp = vpPALSQ;
		break;
    }
    bcopy((char *)&hp, (char *)&(pvgi1->hparam), sizeof(evo_vgi1_h_param_t));
    bcopy((char *)&vp, (char *)&(pvgi1->vparam), sizeof(evo_vgi1_v_param_t));
    pvgi2->hparam = pvgi1->hparam;
    pvgi2->vparam = pvgi1->vparam;

    chan = evobase->chan;
    pvgi = &(evobase->vgi1[chan]);
    /* XXX: Setup the Channel for DMA */
    chSetup.gio_fmt	= evobase->vidFmt;
    chSetup.vo_blank	= EVO_VGI1_VO_BLK_YUVA;
    chSetup.vtrig_sel	= EVO_VGI1_VTRIG_SEL_INT;
    chSetup.vtrig_pol	= EVO_VGI1_VTRIG_POL_HIGH;
    chSetup.vin_round	= evobase->vin_round;
    chSetup.vout_expd	= evobase->vout_expd;
    chSetup.rnd_type	= EVO_VGI1_RND_TYPE_SMPLE;
    chSetup.rnd_free	= EVO_VGI1_RND_DYN_RPT;
    chSetup.burst_len	= evobase->burst_len;
    chSetup.vin_trc	= EVO_VGI1_VIN_TRC_CCIR;
    /*
     * The hardware field mask was broken in the first version
     * of the chip (Aug 1995), but we also decided to go ahead
     * with software rate control for greater flexibility.  To
     * solve both problems, force the field mask to all ones, and
     * set the length to PAL.
     */
    chSetup.fld_mask	= NTSC_FRAME_RATE_30;
    chSetup.mask_len	= evobase->videoMode /* EVO_VGI1_MASK_LEN_PAL */;
    if (evobase->vidFmt == EVO_VGI1_FMT_ARB_10) {
    	chSetup.vout_trc	= EVO_VGI1_VOUT_TRC_FULL;
    } else {
    	chSetup.vout_trc	= EVO_VGI1_VOUT_TRC_CCIR;
    }

    if (evobase->VGIDMAOp == VGI1_DMA_READ) {
	chSetup.direction = EVO_VGI1_DMA_DIR_READ;
	chSetup.fld_inv_p = EVO_VGI1_FLD_INV_ON;
    } else {
	chSetup.direction = EVO_VGI1_DMA_DIR_WRITE;
	chSetup.fld_inv_p = EVO_VGI1_FLD_INV_OFF;
    }

    if (evobase->dualLink) {
	chSetup.vid_src = pvgi1->ch_vid_src;
	bcopy((char*)&chSetup,(char*)&(pvgi1->chSet), 
		sizeof(evo_vgi1_ch_setup_t));
	chSetup.vid_src = pvgi2->ch_vid_src;
	bcopy((char*)&chSetup,(char*)&(pvgi2->chSet), 
		sizeof(evo_vgi1_ch_setup_t));
    } else {
    	chSetup.vid_src	  = pvgi->ch_vid_src;
    	/* save the parameters in the structure */
    	bcopy((char*)&chSetup,(char*)&(pvgi->chSet), 
		sizeof(evo_vgi1_ch_setup_t)); 
    }

    msg_printf(DBG, "Leaving _evo_VGI1InitDMAParam***\n");
}

/*
 * NAME: _evo_VGI1BuildDMADescTable
 *
 * FUNCTION: build the DMA descriptor table 
 *
 * INPUTS: 
 *
 * OUTPUTS: None
 */
void
_evo_VGI1BuildDMADescTable (uchar_t *pDesc)
{
    evo_vgi1_info_t	*pvgi1 = &(evobase->vgi1[evobase->chan]);
    __uint32_t		i, nDescEntries;
    ulonglong_t		descVal;
    ulonglong_t		*tmp64;

    msg_printf(DBG, "Entering _evo_VGI1BuildDMADescTable..., pDesc 0x%x\n",
	pDesc);

    evobase->pVGIDMABufAl = _evo_PageAlign(evobase->pVGIDMABuf);
    pvgi1->pDMADataKseg = evobase->pVGIDMABufAl;
    pvgi1->pDMADataPhys = (uchar_t *)K1_TO_PHYS(pvgi1->pDMADataKseg);
    msg_printf(DBG, "pData=0x%x; pDataAl=0x%x; pDMADataPhys=0x%x\n",
		evobase->pVGIDMABuf,pvgi1->pDMADataKseg, pvgi1->pDMADataPhys);

    pvgi1->pFieldDescKseg = _evo_PageAlign(pDesc);
    pvgi1->pFieldDescPhys = (uchar_t *)K1_TO_PHYS(pvgi1->pFieldDescKseg);

    msg_printf(DBG, "pDesc=0x%x; pFieldDescKseg=0x%x; pFieldDescPhys=0x%x\n", 
		pDesc, pvgi1->pFieldDescKseg, pvgi1->pFieldDescPhys);

    descVal = (__paddr_t)pvgi1->pDMADataPhys >> evobase->pageBits;
    tmp64 = (ulonglong_t *)pvgi1->pFieldDescKseg;
    if (evobase->force_insuf) {
    	nDescEntries = 0x1; /* XXX: Really insufficient */
    } else {
    	nDescEntries = evobase->transferSize >> evobase->pageBits;
    }
    msg_printf(DBG, "descVal = 0x%llx; nDescEntries = 0x%x\n", 
		descVal, nDescEntries);
    for (i = 0; i < nDescEntries; i++) {
	msg_printf(DBG, "tmp64 0x%x\n", tmp64);
    	evo_vgi1_store_ll(descVal, (__paddr_t *)tmp64); 
	msg_printf(DBG, "Desc Addr 0x%x; Desc Entry 0x%llx\n", tmp64, descVal);
	descVal++;
	tmp64++;
    }
    if (!evobase->force_unused) {
    	descVal |= ((ulonglong_t)0xf << 32);
    }
    evo_vgi1_store_ll(descVal, (__paddr_t *)tmp64);
    msg_printf(DBG, "Desc Addr 0x%x; Desc Entry 0x%llx\n", tmp64, descVal);

    msg_printf(DBG, "Leaving _evo_VGI1BuildDMADescTable***\n");
}

/*
 * NAME: _evo_VGI1ProgramDMARegs
 *
 * FUNCTION: program the registers for DMA, including kick-off DMA
 *
 * INPUTS: channel number
 *
 * OUTPUTS: -1 if error, 0 if no error
 */
__uint32_t
_evo_VGI1ProgramDMARegs(__uint32_t chan)
{
    __uint32_t		errors = 0;
    evo_vgi1_info_t	*pvgi1 = &(evobase->vgi1[chan]);
    ulonglong_t		mask = ~0x0;
    __uint32_t		chStatRegOffset;

    msg_printf(DBG, "Entering _evo_VGI1ProgramDMARegs...\n"); 

    chStatRegOffset = pvgi1->ch_status_reg_offset;

    /* System Mode Setup is already programmed in evo_init */
    /* Verify that the chip is good shape to accept DMA request */
    if (_evo_VGIDMAClear(chan)) {
	errors++;
	goto __end_of_progregs;
    }

    /* Program the DMA channel Setup register */
    VGI1_W64(evobase, pvgi1->ch_setup_reg_offset, pvgi1->chSet, mask);

    /* Program H parameter FIRST */
    VGI1_W64(evobase, pvgi1->ch_hparam_reg_offset, pvgi1->hparam, mask);

    /* Program V parameter NEXT */
    VGI1_W64(evobase, pvgi1->ch_vparam_reg_offset, pvgi1->vparam, mask);

    /* Program the DMA stride */
    VGI1_W64(evobase, pvgi1->ch_dma_stride_reg_offset, pvgi1->stride, mask);

    if ((chan == VGI1_CHANNEL_2) && (evobase->dualLink)) {
	msg_printf(DBG, "Don't enable the intrs of ch2 for dual link\n");
	goto __end_of_progregs;
    }

    /* enable all interrupts for the channel */
    VGI1_W64(evobase, chStatRegOffset, EVO_VGI1_CH_ALLINTRS, mask);
    VGI1_W64(evobase, pvgi1->ch_int_enab_reg_offset,evobase->intEnableVal,mask);

    /* Program host data physical address DATA_ADR */
    VGI1_W64(evobase, pvgi1->ch_data_adr_reg_offset, 
		((__paddr_t)pvgi1->pDMADataPhys), mask);

    /* Program physical address of the descriptor address */
    VGI1_W64(evobase, pvgi1->ch_desc_adr_reg_offset, 
		((__paddr_t)pvgi1->pFieldDescPhys), mask);

    if ( (evobase->nFields == 2) && (!(evobase->vout_freeze)) ) {
	/* Assuming that DB_DONE bit is set */
	delay(1000);

	/* Program the DMA stride */
	VGI1_W64(evobase, pvgi1->ch_dma_stride_reg_offset,
			pvgi1->stride, mask);

	/* Program host data physical address DATA_ADR */
	VGI1_W64(evobase, pvgi1->ch_data_adr_reg_offset,
			((__paddr_t)pvgi1->pDMADataPhys), mask);

	/* program the descriptor address for the next field */
	VGI1_W64(evobase, pvgi1->ch_desc_adr_reg_offset,
			((__paddr_t)pvgi1->pFieldDescPhys), mask);
    }

__end_of_progregs:

    msg_printf(DBG, "Leaving _evo_VGI1ProgramDMARegs***\n");

    return(errors);
}

__uint32_t
_evo_VGI1StartAndProcessDMA(void)
{
    __uint32_t		i, j, k, tmpError, chanOutCtrl, errors = 0;
    evo_vgi1_info_t	*pvgi1 = &(evobase->vgi1[evobase->chan]);
    ulonglong_t		mask = ~0x0;
    ulonglong_t		intEnableVal = evobase->intEnableVal;
#if (defined(IP22) || defined(IP28))
    volatile unsigned char *lio_isr0 = K1_LIO_0_ISR_ADDR;
#endif
    msg_printf(DBG, "Entering _evo_VGI1StartAndProcessDMA...\n");

    flush_cache();
    i = 0;
    j = 0;
    k = 0;

    chanOutCtrl = pvgi1->ch_vout_ctrl_reg_offset;
    EVO_W1(evobase, chanOutCtrl, 1);

    VGI1_W64(evobase, pvgi1->ch_status_reg_offset, evobase->intEnableVal, mask);
    VGI1_W64(evobase, pvgi1->ch_int_enab_reg_offset,evobase->intEnableVal,mask);
    if (evobase->dualLink) {
    	VGI1_W64(evobase, VGI1_CH1_TRIG, pvgi1->trigOn, mask);
    	VGI1_W64(evobase, VGI1_CH2_TRIG, pvgi1->trigOn, mask);
    } else {
    	VGI1_W64(evobase, pvgi1->ch_trig_reg_offset, pvgi1->trigOn, mask);
    }
_start_dma:
    i = 0;
#if (defined(IP22) || defined(IP28))
    do {
	i++;
    } while ( (!((*lio_isr0) & LIO_GIO_1)) && (i < VGI1_POLL_LARGE_TIME_OUT));
#endif
    if (i == VGI1_POLL_LARGE_TIME_OUT) {
	msg_printf(ERR, "IMPV:- VGI1 DMA Interrupt not reached CPU\n");
	errors++;
	goto __end_of_process_dma;
    }
#if (defined(IP22) || defined(IP28))
    *lio_isr0 = 0x0;
#endif

    VGI1_R64(evobase, pvgi1->ch_status_reg_offset,
		evobase->VGI1DMAOpErrStatus, mask);
    tmpError = 0x1;
    if (evobase->dualLink||(evobase->VGI1DMAOpErrStatus & EVO_VGI1_CH_E_DMA)) {
	tmpError = 0x0;
    }

    if (!tmpError) {
	if ( (evobase->nFields == 2) || (evobase->vout_freeze) ) {
	    VGI1_W64(evobase, pvgi1->ch_status_reg_offset, intEnableVal, mask);
	    if (evobase->vout_freeze) {
		/* Program the DMA stride */
		VGI1_W64(evobase, pvgi1->ch_dma_stride_reg_offset,
			pvgi1->stride, mask);
		/* Program host data physical address DATA_ADR */
		VGI1_W64(evobase, pvgi1->ch_data_adr_reg_offset,
			((__paddr_t)pvgi1->pDMADataPhys), mask);
		/* program the descriptor address for the next field */
		VGI1_W64(evobase, pvgi1->ch_desc_adr_reg_offset,
			((__paddr_t)pvgi1->pFieldDescPhys), mask);
		if (k >= evobase->vout_freeze) {
		    goto _quit_dma;
		} else {
		    k++;
		    goto _start_dma;
		}
	    }
	    j = 0;
#if (defined(IP22) || defined(IP28))
	    do {
	    	j++;
	    } while ( (!((*lio_isr0) & LIO_GIO_1)) &&
		(j < VGI1_POLL_LARGE_TIME_OUT));
#endif
	    if (j == VGI1_POLL_LARGE_TIME_OUT) {
		msg_printf(ERR, "IMPV:- VGI1 DMA Interrupt not reached CPU\n");
		errors++;
		goto __end_of_process_dma;
	    }
#if (defined(IP22) || defined(IP28))
	    *lio_isr0 = 0x0;
#endif
	    if (evobase->dualLink) {
	    	VGI1_W64(evobase, VGI1_CH1_TRIG, pvgi1->trigOff, mask);
	    	VGI1_W64(evobase, VGI1_CH2_TRIG, pvgi1->trigOff, mask);
	    } else {
	    	VGI1_W64(evobase, pvgi1->ch_trig_reg_offset, 
			pvgi1->trigOff, mask);
	    }
    	    VGI1_R64(evobase, pvgi1->ch_status_reg_offset,
			evobase->VGI1DMAOpErrStatus, mask);
	    tmpError = 0x1;
	    if (evobase->dualLink || 
	       (evobase->VGI1DMAOpErrStatus & EVO_VGI1_CH_E_DMA)) {
		tmpError = 0x0;
	    }
	    if (tmpError) {
		 errors++;
		 goto __end_of_process_dma;
	    }
	}
    } else {
	 errors++;
	 goto __end_of_process_dma;
    }

_quit_dma:

    flush_cache();

    /* Verify that the chip is good shape to accept DMA request */
    if (_evo_VGIDMAClear(evobase->chan)) {
	errors++;
	goto __end_of_process_dma;
    }

#if 0
    fld_type = (statusBeforeTrigOn >> 32) & 0x1;
    line_cnt = (statusBeforeTrigOn >> 33) & 0xff;
    msg_printf(DBG, "BeforeTrigOn 0x%llx; fld_type 0x%llx; line_cnt 0x%llx\n",
	statusBeforeTrigOn, fld_type, line_cnt);
    
    fld_type = (statusAfterEDMA1 >> 32) & 0x1;
    line_cnt = (statusAfterEDMA1 >> 33) & 0xff;
    msg_printf(DBG, "AfterEDMA1   0x%llx; fld_type 0x%llx; line_cnt 0x%llx\n",
	statusAfterEDMA1, fld_type, line_cnt);
    
    fld_type = (statusAfterEDMA2 >> 32) & 0x1;
    line_cnt = (statusAfterEDMA2 >> 33) & 0xff;
    msg_printf(DBG, "AfterEDMA2   0x%llx; fld_type 0x%llx; line_cnt 0x%llx\n",
	statusAfterEDMA2, fld_type, line_cnt);
#endif

__end_of_process_dma:
    msg_printf(DBG, "\n\n");
    msg_printf(DBG, "Leaving _evo_VGI1StartAndProcessDMA\n");

    return(errors);
}

__uint32_t
_evo_VGIDMAClear(__uint32_t chan)
{
    __uint32_t		errors = 0;
    evo_vgi1_info_t	*pvgi1 = &(evobase->vgi1[chan]);
    ulonglong_t		status, mask = ~0x0;
#if (defined(IP22) || defined(IP28))
    volatile unsigned char *lio_isr0 = K1_LIO_0_ISR_ADDR;
#endif
    msg_printf(DBG, "Entering _evo_VGIDMAClear..., chan 0x%x\n", chan);
    /*
     * 1. disable all interrupts
     * 2. abort any previous DMAs
     * 3. turn the trigger off
     * 4. clear all interrupt flags
     */
    VGI1_W64(evobase, pvgi1->ch_int_enab_reg_offset, 0x0, mask);
    VGI1_W64(evobase, pvgi1->ch_trig_reg_offset, pvgi1->abortDMA, mask);
    VGI1_W64(evobase, pvgi1->ch_trig_reg_offset, pvgi1->trigOff, mask);
    VGI1_W64(evobase, pvgi1->ch_status_reg_offset, EVO_VGI1_CH_ALLINTRS, mask);

    /* Nothing should be set... check for that */
    if (_evo_VGI1PollReg(pvgi1->ch_status_reg_offset, 0x0, EVO_VGI1_CH_ALLINTRS,
		VGI1_POLL_LARGE_TIME_OUT, &status)) {
	msg_printf(ERR, "Status register is not ZERO... Time Out\n");
	errors++;
	evobase->VGI1DMAClrErrStatus = status;
    }

#if (defined(IP22) || defined(IP28))
    *lio_isr0 = 0x0;
#endif
    msg_printf(DBG, "Leaving _evo_VGIDMAClear...\n");

    return (errors);
}

/*
 *  _evo_dmaPixelDepth
 *
 *  Given a VGI1 GIO format specification, return the pixel depth (that
 *  is the bytes in memory required per pixel).
 *
 *  ARGUMENTS
 *	giofmt		Format specification
 *
 *  RETURN VALUE
 *	depth		Pixel depth, in bytes
 */
__uint32_t
_evo_dmaPixelDepth(__uint32_t giofmt)
{
    __uint32_t depth;

    switch(giofmt) {

	case EVO_VGI1_FMT_YUV422_8:
	case EVO_VGI1_FMT_ARB_8:
		depth = 2;
		break;

	case EVO_VGI1_FMT_YUV422_10:
	case EVO_VGI1_FMT_AUYV4444_8:
	case EVO_VGI1_FMT_ABGR_8:
	case EVO_VGI1_FMT_VYUA4444_8:
	case EVO_VGI1_FMT_RGBA_8:
	case EVO_VGI1_FMT_ARB_10:
	case EVO_VGI1_FMT_AYUAYV4224_10:
		depth = 4;
		break;
	
	case EVO_VGI1_FMT_AUYV4444_10:
	case EVO_VGI1_FMT_VYUA4444_10:
	case EVO_VGI1_FMT_ABGR_10:
	case EVO_VGI1_FMT_RGBA_10:
		depth = 8;
		break;
    }  /* End switch */

    return depth;
}

__uint32_t
_evo_VGI1DMADispErr(void)
{
    ulonglong_t	opErr, clrErr;
    __uint32_t errors = 0;

    opErr = evobase->VGI1DMAOpErrStatus;
    clrErr = evobase->VGI1DMAClrErrStatus;

    msg_printf(DBG, "opErr 0x%llx; clrErr 0x%llx\n", opErr, clrErr);
    errors += _evo_VGI1DMADispErrActual(opErr,"EVO:- VGI1 DMA Operation Error");
    errors += _evo_VGI1DMADispErrActual(clrErr,"EVO:- VGI1 DMA Clean-up Error");

    return(errors);
}

__uint32_t
_evo_VGI1DMADispErrActual(ulonglong_t err, char *str)
{
    __uint32_t errors = 0;

    if (err & EVO_VGI1_CH_B_FIFO) {
	msg_printf(ERR, "%s - Bad FIFO\n", str);
	errors++;
    }
    if (err & EVO_VGI1_CH_B_VCLK) {
	msg_printf(ERR, "%s - Bad video clock\n", str);
	errors++;
    }
    if (err & EVO_VGI1_CH_B_HL) {
	msg_printf(ERR, "%s - Bad horizontal length\n", str);
	errors++;
    }
    if (err & EVO_VGI1_CH_B_VL) {
	msg_printf(ERR, "%s - Bad vertical length\n", str);
	errors++;
    }
    if (err & EVO_VGI1_CH_UNUSED) {
	msg_printf(ERR, "%s - Unused last descriptor\n", str);
	errors++;
    }
    if (err & EVO_VGI1_CH_INSUF) {
	msg_printf(ERR, "%s - Insufficient descriptors\n", str);
	errors++;
    }

    return(errors);
}

