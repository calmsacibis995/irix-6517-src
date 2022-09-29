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
**      FileName:       mgv_vgi1_dma.c
*/

#include "mgv_diag.h"
#if (defined(IP22) || defined(IP28))
#include "sys/cpu.h"
#endif
#ifdef IP30
#include "../../../godzilla/include/d_bridge.h"
#include "../../../godzilla/include/d_godzilla.h"
#endif

/*
 * Forward Function References
 */

__uint32_t  mgv_VGI1DMATest (__uint32_t argc, char **argv);
void  _mgv_VGI1InitDMAParam (void);
void  _mgv_VGI1BuildDMADescTable(uchar_t *pDesc);
__uint32_t  _mgv_VGI1ProgramDMARegs(__uint32_t chan);
__uint32_t  _mgv_dmaPixelDepth (__uint32_t giofmt);
__uint32_t  _mgv_VGI1DMAGetBufSize(void);
__uint32_t  _mgv_VGI1DMAValidateArgs(__uint32_t chan);
__uint32_t  _mgv_VGIDMAClear(__uint32_t chan);
__uint32_t  _mgv_VGI1StartAndProcessDMA(void);
__uint32_t  _mgv_VGI1DMA(void);
__uint32_t  _mgv_VGI1DMADispErr(void);
__uint32_t  _mgv_VGI1DMADispErrActual(ulonglong_t err, char *str);

/*
 * NAME: mgv_VGI1DMATest
 *
 * FUNCTION: top level function for VGI1 DMA
 *
 * INPUTS: argc and argv
 *
 * OUTPUTS: -1 if error, 0 if no error
 */
__uint32_t
mgv_VGI1DMATest (__uint32_t argc, char **argv)
{
    __uint32_t	errors = 0;
    __uint32_t	badarg = 0;
    __uint32_t	i, dataBufSize;
    __uint32_t	yval, tmp, op;
    __uint32_t	chan;
    __uint32_t	vgi_num = 1;

    unsigned char	xp;

    chan = VGI1_CHANNEL_1;
    mgvbase->tvStd = VGI1_NTSC_CCIR601;
    mgvbase->vidFmt = MGV_VGI1_FMT_YUV422_8;
    mgvbase->nFields = 2;
    mgvbase->vin_round = MGV_VGI1_VIN_RND_OFF;
    mgvbase->vout_expd = MGV_VGI1_VOUT_EXPD_OFF;
    mgvbase->vout_freeze = 0;
    yval = 0x80808080;
    op = 0x3;

    /* get the args */
    argc--; argv++;
    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 'c':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &chan);
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &chan);
			}
			chan--;
                        break;
                case 'o':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &op);
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &op);
			}
                        break;
                case 't':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &(mgvbase->tvStd));
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &(mgvbase->tvStd));
			}
                        break;
                case 'v':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &(mgvbase->vidFmt));
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &(mgvbase->vidFmt));
			}
                        break;
                case 'f':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &(mgvbase->nFields));
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &(mgvbase->nFields));
			}
                        break;
                case 'a':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &vgi_num);
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &(vgi_num));
			}
                        break;
                case 'z':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &(mgvbase->vout_freeze));
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &(mgvbase->vout_freeze));
			}
                        break;
                case 'y':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &yval);
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &yval);
			}
                        break;
                default: badarg++; break;
        }
        argc--; argv++;
    }

    if (badarg || argc || _mgv_VGI1DMAValidateArgs(chan) ) {
	msg_printf(SUM, "\
	Usage: mgv_vgidma -c [1|2] -o [1|2|3] -t [1|2|3|4] -v [0|1|2|3]\n\
	       -f [1|2] -z [0|1]\n\
	-c --> channel 1[2] \n\
	-o --> operation 1-mem2vid 2-vid2mem 3-both\n\
	-t --> TV standard 0-NTSC601 1-NTSCSQ 2-PAL601 3-PALSQ\n\
	-v --> Video Format 0-8YUV422 1-10YUV422 2-8ARB 3-10FULL ...\n\
	-f --> Number of fields 1 or 2\n\
	-a --> VGI Asic 1[2] \n\
	-z --> Freeze 0 or #of fields to DMA\n\
	");
	return(-1);
    }

/* Set giobase address to VGI_1 or VGI_2 */
    if (vgi_num == 2) {
	MGV_SET_VGI1_2_BASE;
    } else { 
	MGV_SET_VGI1_1_BASE;
    }

    msg_printf(SUM, "IMPV:- %s -c 0x%x; -o 0x%x; -t 0x%x; -v 0x%x; -f 0x%x; -z 0x%x\n", 
	mgv_err[VGI1_DMA_TEST].TestStr, (chan+1), op, mgvbase->tvStd,
	mgvbase->vidFmt, mgvbase->nFields, mgvbase->vout_freeze);

    /* generate the data in memory */
    mgvbase->transferSize = _mgv_VGI1DMAGetBufSize();
    dataBufSize = mgvbase->transferSize + mgvbase->pageSize + 32;

    msg_printf(DBG, "dataBufSize 0x%x\n", dataBufSize);
    if ((mgvbase->pVGIDMABuf = (uchar_t *)get_chunk(dataBufSize)) == NULL) {
	msg_printf(ERR, "Insufficient memory for Read Data Buffer\n");
	return(-1);
    } else {
	for (i = 0; i < (dataBufSize >> 2); i++) {
	   *(((__uint32_t *)mgvbase->pVGIDMABuf) + i) = yval;
	}
    }
    msg_printf(DBG, "pBuf 0x%x; dataBufSize 0x%x\n",
	mgvbase->pVGIDMABuf, dataBufSize);

    /* set default timing */
    GAL_IND_W1 (mgvbase, GAL_REF_OFFSET_LO, LOBYTE(BOFFSET_CENTER));
    GAL_IND_W1 (mgvbase, GAL_REF_OFFSET_HI, HIBYTE(BOFFSET_CENTER));

    if (op & 0x1) {
	if (vgi_num == 1) {
    		/* Send Black Reference to both channels for DMA READ */
    		xp = GAL_OUT_CH1_GIO | GAL_BLACK_REF_IN_SHIFT;
    		GAL_IND_W1(mgvbase, GAL_MUX_ADDR, xp);
    		xp = GAL_OUT_CH2_GIO | GAL_BLACK_REF_IN_SHIFT;
    		GAL_IND_W1(mgvbase, GAL_MUX_ADDR, xp);

    		/* setup VBAR according to inchan and outchan */
    		xp = GAL_OUT_CH1 | GAL_VGI1_CH1_OUT_SHIFT;
    		GAL_IND_W1(mgvbase, GAL_MUX_ADDR, xp);
    		xp = GAL_OUT_CH2 | GAL_VGI1_CH2_OUT_SHIFT;
    		GAL_IND_W1(mgvbase, GAL_MUX_ADDR, xp);
	} else {
		msg_printf(SUM, "How will the VGI1-2 get the Black ref ?\n");
	}

    	msg_printf(SUM, "\nDMA Read - from mem to video....\n");
    	mgvbase->chan 			= chan;
    	mgvbase->VGIDMAOp		= VGI1_DMA_READ;
    	mgvbase->VGI1DMAOpErrStatus 	= 0;
    	mgvbase->VGI1DMAClrErrStatus 	= 0;
	tmp = _mgv_VGI1DMA();
	if (tmp == VGI1_NO_VIDEO_INPUT) {
	    return(0x0);
	} else {
	    errors += tmp;
	}
    	if (errors) goto __end_of_dma;

    	msg_printf(SUM, "Buffer: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		*(((__uint32_t *)mgvbase->pVGIDMABufAl) + 0), 
		*(((__uint32_t *)mgvbase->pVGIDMABufAl) + 1),
		*(((__uint32_t *)mgvbase->pVGIDMABufAl) + 2), 
		*(((__uint32_t *)mgvbase->pVGIDMABufAl) + 3),
		*(((__uint32_t *)mgvbase->pVGIDMABufAl) + 4), 
		*(((__uint32_t *)mgvbase->pVGIDMABufAl) + 5));
    }

    if (op & 0x2) {

    	/* corrupt the buffer since we're using the same */
    	for (i = 0; i < (dataBufSize >> 2); i++) {
   		*(((__uint32_t *)mgvbase->pVGIDMABuf) + i) = ~yval;
    	}

    	msg_printf(SUM, "\nDMA Write - from video to mem....\n");

	if (vgi_num == 1) {
    		/* Send Black Reference to both channels for DMA READ */
    		xp = GAL_OUT_CH1_GIO | GAL_DIGITAL_CH1_IN_SHIFT;
    		GAL_IND_W1(mgvbase, GAL_MUX_ADDR, xp);
    		xp = GAL_OUT_CH2_GIO | GAL_DIGITAL_CH2_IN_SHIFT;
    		GAL_IND_W1(mgvbase, GAL_MUX_ADDR, xp);

    		/* setup VBAR according to inchan and outchan */
    		xp = GAL_OUT_CH1 | GAL_VGI1_CH1_OUT_SHIFT;
    		GAL_IND_W1(mgvbase, GAL_MUX_ADDR, xp);
    		xp = GAL_OUT_CH2 | GAL_VGI1_CH2_OUT_SHIFT;
    		GAL_IND_W1(mgvbase, GAL_MUX_ADDR, xp);
	} else {
		msg_printf(SUM, "How will the VGI1-2 get the Black ref ?\n");
	}
    	mgvbase->chan 			= chan;
    	mgvbase->VGIDMAOp		= VGI1_DMA_WRITE;
    	mgvbase->VGI1DMAOpErrStatus 	= 0;
    	mgvbase->VGI1DMAClrErrStatus 	= 0;
	tmp = _mgv_VGI1DMA();
	if (tmp == VGI1_NO_VIDEO_INPUT) {
	    return(0x0);
	} else {
	    errors += tmp;
	}
    	if (errors) goto __end_of_dma;

    	msg_printf(SUM, "Buffer: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		*(((__uint32_t *)mgvbase->pVGIDMABufAl) + 0), 
		*(((__uint32_t *)mgvbase->pVGIDMABufAl) + 1),
		*(((__uint32_t *)mgvbase->pVGIDMABufAl) + 2), 
		*(((__uint32_t *)mgvbase->pVGIDMABufAl) + 3),
		*(((__uint32_t *)mgvbase->pVGIDMABufAl) + 4), 
		*(((__uint32_t *)mgvbase->pVGIDMABufAl) + 5));
    }

#if 0
    /* XXX: compare the buffers */
    /* For now check if all the values are same and equal to yval */
    for (i = 0; i < (mgvbase->transferSize >> 2); i++) {
   	tmp = *(((__uint32_t *)mgvbase->pVGIDMABuf) + i);
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
    errors += _mgv_VGIDMAClear(chan);

    errors += _mgv_VGI1DMADispErr();

/* Set giobase to VGI_1 before returning */
    MGV_SET_VGI1_1_BASE;

    return(_mgv_reportPassOrFail((&mgv_err[VGI1_DMA_TEST]), errors));
}

__uint32_t
_mgv_VGI1DMA(void)
{
    __uint32_t	errors = 0;
    __uint32_t	chanOutCtrl;
    unsigned char	vin1Status, vin2Status;

    msg_printf(DBG, "Entering _mgv_VGI1DMA...");

    /*
     * First make sure that the channel has input for DMA Write
     */
    if (mgvbase->VGIDMAOp == VGI1_DMA_WRITE) {
	GAL_IND_R1(mgvbase, GAL_VIN_CH1_STATUS, vin1Status);
	GAL_IND_R1(mgvbase, GAL_VIN_CH2_STATUS, vin2Status);
	if (mgvbase->dualLink) {
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
	    if (mgvbase->chan == VGI1_CHANNEL_1) {
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

    mgvbase->VGI1DMAOpErrStatus = 0;
    mgvbase->VGI1DMAClrErrStatus = 0;

    mgvbase->dualLink = 0x1;
    switch (mgvbase->vidFmt) {
        case MGV_VGI1_FMT_YUV422_8:
        case MGV_VGI1_FMT_ARB_8:
        case MGV_VGI1_FMT_YUV422_10:
		mgvbase->dualLink = 0x0;
		break;
    }

    /* figure out the DMA transfer size */
    mgvbase->transferSize = _mgv_VGI1DMAGetBufSize();

    mgv_pVGIDMADesc = mgv_VGIDMADesc;

    msg_printf(DBG, "pDesc 0x%x;\n", mgv_pVGIDMADesc);

    /* set up TV standard */
    GAL_IND_W1(mgvbase, GAL_TV_STD, mgvbase->tvStd);
    delay(300);

    /* initialize the parameters for DMA in software */
    _mgv_VGI1InitDMAParam();

    /* unfreeze the video out */
    chanOutCtrl = mgvbase->vgi1[mgvbase->chan].ch_vout_ctrl_reg_offset;
    msg_printf(DBG, "Freezing video out chanOutCtrl 0x%x\n",
		chanOutCtrl);
    GAL_IND_W1 (mgvbase, chanOutCtrl, 0x1);

    /* build the DMA descriptor table */
    _mgv_VGI1BuildDMADescTable(mgv_pVGIDMADesc);

    /* program the Channel(s) */
    /* if dual-link program the other channel too */
    if (mgvbase->dualLink) {
	errors += _mgv_VGI1ProgramDMARegs(VGI1_CHANNEL_1);
	errors += _mgv_VGI1ProgramDMARegs(VGI1_CHANNEL_2);
    } else {
	errors += _mgv_VGI1ProgramDMARegs(mgvbase->chan);
    }

    /* Start the DMA */
    errors += _mgv_VGI1StartAndProcessDMA();

    /* Verify that the chip is good shape to accept DMA request */
    if (mgvbase->dualLink) {
    	errors += _mgv_VGIDMAClear(VGI1_CHANNEL_1);
    	errors += _mgv_VGIDMAClear(VGI1_CHANNEL_2);
    } else {
    	errors += _mgv_VGIDMAClear(mgvbase->chan);
    }

    return(errors);
}

__uint32_t	
_mgv_VGI1DMAValidateArgs(__uint32_t chan)
{
    __uint32_t	errors = 0;

    if ((chan != VGI1_CHANNEL_1) && (chan != VGI1_CHANNEL_2)) {
	msg_printf(ERR, "Incorrect channel number\n");
	errors++;
    }
    switch (mgvbase->tvStd) {
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
    switch (mgvbase->vidFmt) {
	case MGV_VGI1_FMT_YUV422_8:
	case MGV_VGI1_FMT_ARB_8:
	case MGV_VGI1_FMT_YUV422_10:
	case MGV_VGI1_FMT_AUYV4444_8:
	case MGV_VGI1_FMT_ABGR_8:
	case MGV_VGI1_FMT_VYUA4444_8:
	case MGV_VGI1_FMT_RGBA_8:
	case MGV_VGI1_FMT_ARB_10:
	case MGV_VGI1_FMT_AYUAYV4224_10:
	case MGV_VGI1_FMT_AUYV4444_10:
	case MGV_VGI1_FMT_VYUA4444_10:
	case MGV_VGI1_FMT_ABGR_10:
	case MGV_VGI1_FMT_RGBA_10:
		break;
      	default:
		msg_printf(ERR, "Incorrect Video (GIO) Format\n");
		errors++;
		break;
    }
    if ((mgvbase->nFields != 1) && (mgvbase->nFields != 2)) {
	msg_printf(ERR, "Incorrect number of fields\n");
	errors++;
    }
    return(errors);
}

__uint32_t
_mgv_VGI1DMAGetBufSize(void)
{
    __uint32_t	bufSize, pixDepth;

    pixDepth = _mgv_dmaPixelDepth(mgvbase->vidFmt);

    switch (mgvbase->tvStd) {
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
 * NAME: _mgv_VGI1InitDMAParam
 *
 * FUNCTION: initialize the DMA parameters in the structure
 *
 * INPUTS: 
 *
 * OUTPUTS: None
 */
void
_mgv_VGI1InitDMAParam (void)
{
    __uint32_t	chan;
    mgv_vgi1_info_t	*pvgi1 = &(mgvbase->vgi1[0]);
    mgv_vgi1_info_t	*pvgi2 = &(mgvbase->vgi1[1]);
    mgv_vgi1_info_t	*pvgi;
    mgv_vgi1_ch_setup_t	chSetup = {0LL};
    mgv_vgi1_h_param_t	hp = {0LL};
    mgv_vgi1_v_param_t	vp = {0LL};

    msg_printf(DBG, "Entering _mgv_VGI1InitDMAParam...\n"); 

    /* figure out the H and V parameters */
    switch (mgvbase->tvStd) {
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
    bcopy((char *)&hp, (char *)&(pvgi1->hparam), sizeof(mgv_vgi1_h_param_t));
    bcopy((char *)&vp, (char *)&(pvgi1->vparam), sizeof(mgv_vgi1_v_param_t));
    pvgi2->hparam = pvgi1->hparam;
    pvgi2->vparam = pvgi1->vparam;

    chan = mgvbase->chan;
    pvgi = &(mgvbase->vgi1[chan]);
    /* XXX: Setup the Channel for DMA */
    chSetup.gio_fmt	= mgvbase->vidFmt;
    chSetup.vo_blank	= MGV_VGI1_VO_BLK_YUVA;
    chSetup.vtrig_sel	= MGV_VGI1_VTRIG_SEL_INT;
    chSetup.vtrig_pol	= MGV_VGI1_VTRIG_POL_HIGH;
    chSetup.vin_round	= mgvbase->vin_round;
    chSetup.vout_expd	= mgvbase->vout_expd;
    chSetup.rnd_type	= MGV_VGI1_RND_TYPE_SMPLE;
    chSetup.rnd_free	= MGV_VGI1_RND_DYN_RPT;
    chSetup.burst_len	= mgvbase->burst_len;
    chSetup.vin_trc	= MGV_VGI1_VIN_TRC_CCIR;
    /*
     * The hardware field mask was broken in the first version
     * of the chip (Aug 1995), but we also decided to go ahead
     * with software rate control for greater flexibility.  To
     * solve both problems, force the field mask to all ones, and
     * set the length to PAL.
     */
    chSetup.fld_mask	= NTSC_FRAME_RATE_30;
    chSetup.mask_len	= mgvbase->videoMode /* MGV_VGI1_MASK_LEN_PAL */;
    if (mgvbase->vidFmt == MGV_VGI1_FMT_ARB_10) {
    	chSetup.vout_trc	= MGV_VGI1_VOUT_TRC_FULL;
    } else {
    	chSetup.vout_trc	= MGV_VGI1_VOUT_TRC_CCIR;
    }

    if (mgvbase->VGIDMAOp == VGI1_DMA_READ) {
	chSetup.direction = MGV_VGI1_DMA_DIR_READ;
	chSetup.fld_inv_p = MGV_VGI1_FLD_INV_ON;
    } else {
	chSetup.direction = MGV_VGI1_DMA_DIR_WRITE;
	chSetup.fld_inv_p = MGV_VGI1_FLD_INV_OFF;
    }

    if (mgvbase->dualLink) {
	chSetup.vid_src = pvgi1->ch_vid_src;
	bcopy((char*)&chSetup,(char*)&(pvgi1->chSet), 
		sizeof(mgv_vgi1_ch_setup_t));
	chSetup.vid_src = pvgi2->ch_vid_src;
	bcopy((char*)&chSetup,(char*)&(pvgi2->chSet), 
		sizeof(mgv_vgi1_ch_setup_t));
    } else {
    	chSetup.vid_src	  = pvgi->ch_vid_src;
    	/* save the parameters in the structure */
    	bcopy((char*)&chSetup,(char*)&(pvgi->chSet), 
		sizeof(mgv_vgi1_ch_setup_t)); 
    }

    msg_printf(DBG, "Leaving _mgv_VGI1InitDMAParam***\n");
}

/*
 * NAME: _mgv_VGI1BuildDMADescTable
 *
 * FUNCTION: build the DMA descriptor table 
 *
 * INPUTS: 
 *
 * OUTPUTS: None
 */
void
_mgv_VGI1BuildDMADescTable (uchar_t *pDesc)
{
    mgv_vgi1_info_t	*pvgi1 = &(mgvbase->vgi1[mgvbase->chan]);
    __uint32_t		i, nDescEntries;
    ulonglong_t		descVal;
    ulonglong_t		*tmp64;

    msg_printf(DBG, "Entering _mgv_VGI1BuildDMADescTable..., pDesc 0x%x\n",
	pDesc);

    mgvbase->pVGIDMABufAl = _mgv_PageAlign(mgvbase->pVGIDMABuf);
    pvgi1->pDMADataKseg = mgvbase->pVGIDMABufAl;

#ifdef IP30
    msg_printf(DBG,"Translating address to phys\n");
    pvgi1->pDMADataPhys = (uchar_t *)kv_to_bridge32_dirmap(mgvbase->brbase,
		(__psunsigned_t)pvgi1->pDMADataKseg);
#else
    pvgi1->pDMADataPhys = (uchar_t *)K1_TO_PHYS(pvgi1->pDMADataKseg);
#endif

    msg_printf(DBG, "pData=0x%x; pDataAl=0x%x; pDMADataPhys=0x%x\n",
		mgvbase->pVGIDMABuf,pvgi1->pDMADataKseg, pvgi1->pDMADataPhys);

    pvgi1->pFieldDescKseg = _mgv_PageAlign(pDesc);

#ifdef IP30
    msg_printf(DBG,"Translating desc address to phys\n");
    pvgi1->pFieldDescPhys = (uchar_t *)kv_to_bridge32_dirmap(mgvbase->brbase,
		(__psunsigned_t)pvgi1->pFieldDescKseg);
#else
    pvgi1->pFieldDescPhys = (uchar_t *)K1_TO_PHYS(pvgi1->pFieldDescKseg);
#endif

    msg_printf(DBG, "pDesc=0x%x; pFieldDescKseg=0x%x; pFieldDescPhys=0x%x\n", 
		pDesc, pvgi1->pFieldDescKseg, pvgi1->pFieldDescPhys);

    descVal = (__paddr_t)pvgi1->pDMADataPhys >> mgvbase->pageBits;
    tmp64 = (ulonglong_t *)pvgi1->pFieldDescKseg;
    if (mgvbase->force_insuf) {
    	nDescEntries = 0x1; /* XXX: Really insufficient */
    } else {
    	nDescEntries = mgvbase->transferSize >> mgvbase->pageBits;
    }
    msg_printf(DBG, "Transfer size = 0x%llx; Pagebits  = 0x%x\n", 
		mgvbase->transferSize, mgvbase->pageBits);
    msg_printf(DBG, "descVal = 0x%llx; nDescEntries = 0x%x\n", 
		descVal, nDescEntries);
    for (i = 0; i < nDescEntries; i++) {
	msg_printf(DBG, "tmp64 0x%x\n", tmp64);
    	mgv_vgi1_store_ll(descVal, (void *)tmp64); 
	msg_printf(DBG, "Desc Addr 0x%x; Desc Entry 0x%llx\n", tmp64, descVal);
	descVal++;
	tmp64++;
    }
    if (!mgvbase->force_unused) {
    	descVal |= ((ulonglong_t)0xf << 32);
    }
    mgv_vgi1_store_ll(descVal, (void *)tmp64);
    msg_printf(DBG, "Desc Addr 0x%x; Desc Entry 0x%llx\n", tmp64, descVal);

    msg_printf(DBG, "Leaving _mgv_VGI1BuildDMADescTable***\n");
}

/*
 * NAME: _mgv_VGI1ProgramDMARegs
 *
 * FUNCTION: program the registers for DMA, including kick-off DMA
 *
 * INPUTS: channel number
 *
 * OUTPUTS: -1 if error, 0 if no error
 */
__uint32_t
_mgv_VGI1ProgramDMARegs(__uint32_t chan)
{
    __uint32_t		errors = 0;
    mgv_vgi1_info_t	*pvgi1 = &(mgvbase->vgi1[chan]);
    ulonglong_t		mask = ~0x0;
    __uint32_t		chStatRegOffset;

    msg_printf(DBG, "Entering _mgv_VGI1ProgramDMARegs...\n"); 

    chStatRegOffset = pvgi1->ch_status_reg_offset;

    /* System Mode Setup is already programmed in mgv_init */
    /* Verify that the chip is good shape to accept DMA request */
    if (_mgv_VGIDMAClear(chan)) {
	errors++;
	goto __end_of_progregs;
    }

    /* Program the DMA channel Setup register */
    VGI1_W64(mgvbase, pvgi1->ch_setup_reg_offset, pvgi1->chSet, mask);

    /* Program H parameter FIRST */
    VGI1_W64(mgvbase, pvgi1->ch_hparam_reg_offset, pvgi1->hparam, mask);

    /* Program V parameter NEXT */
    VGI1_W64(mgvbase, pvgi1->ch_vparam_reg_offset, pvgi1->vparam, mask);

    /* Program the DMA stride */
    VGI1_W64(mgvbase, pvgi1->ch_dma_stride_reg_offset, pvgi1->stride, mask);

    if ((chan == VGI1_CHANNEL_2) && (mgvbase->dualLink)) {
	msg_printf(DBG, "Don't enable the intrs of ch2 for dual link\n");
	goto __end_of_progregs;
    }

    /* enable all interrupts for the channel */
    VGI1_W64(mgvbase, chStatRegOffset, MGV_VGI1_CH_ALLINTRS, mask);
    VGI1_W64(mgvbase, pvgi1->ch_int_enab_reg_offset,mgvbase->intEnableVal,mask);

    /* Program host data physical address DATA_ADR */
    VGI1_W64(mgvbase, pvgi1->ch_data_adr_reg_offset, 
		((__paddr_t)pvgi1->pDMADataPhys), mask);

    /* Program physical address of the descriptor address */
    VGI1_W64(mgvbase, pvgi1->ch_desc_adr_reg_offset, 
		((__paddr_t)pvgi1->pFieldDescPhys), mask);

#if 0
    if ( (mgvbase->nFields == 2) && (!(mgvbase->vout_freeze)) ) {
	/* Assuming that DB_DONE bit is set */
	for (_tmp = 0; _tmp < 1000; _tmp++) {
		us_delay(1000);
	}

	/* Program the DMA stride */
	VGI1_W64(mgvbase, pvgi1->ch_dma_stride_reg_offset,
			pvgi1->stride, mask);

	/* Program host data physical address DATA_ADR */
	VGI1_W64(mgvbase, pvgi1->ch_data_adr_reg_offset,
			((__paddr_t)pvgi1->pDMADataPhys), mask);

	/* program the descriptor address for the next field */
	VGI1_W64(mgvbase, pvgi1->ch_desc_adr_reg_offset,
			((__paddr_t)pvgi1->pFieldDescPhys), mask);
    }
#endif

__end_of_progregs:

    msg_printf(DBG, "Leaving _mgv_VGI1ProgramDMARegs***\n");

    return(errors);
}

__uint32_t
_mgv_VGI1StartAndProcessDMA(void)
{
    __uint32_t		i, j, k, tmpError, chanOutCtrl, errors = 0;
    mgv_vgi1_info_t	*pvgi1 = &(mgvbase->vgi1[mgvbase->chan]);
    ulonglong_t		mask = ~0x0;
    ulonglong_t		intEnableVal = mgvbase->intEnableVal;
#if (defined(IP22) || defined(IP28))
    volatile unsigned char *lio_isr0 = K1_LIO_0_ISR_ADDR;
#else 
    __uint32_t          int_val = 0x0;
#endif
    msg_printf(DBG, "Entering _mgv_VGI1StartAndProcessDMA...\n");

    flush_cache();
    i = 0;
    j = 0;
    k = 0;

    chanOutCtrl = pvgi1->ch_vout_ctrl_reg_offset;
    GAL_IND_W1(mgvbase, chanOutCtrl, 1);

    VGI1_W64(mgvbase, pvgi1->ch_status_reg_offset, mgvbase->intEnableVal, mask);
    VGI1_W64(mgvbase, pvgi1->ch_int_enab_reg_offset,mgvbase->intEnableVal,mask);
    if (mgvbase->dualLink) {
    	VGI1_W64(mgvbase, VGI1_CH1_TRIG, pvgi1->trigOn, mask);
    	VGI1_W64(mgvbase, VGI1_CH2_TRIG, pvgi1->trigOn, mask);
    } else {
    	VGI1_W64(mgvbase, pvgi1->ch_trig_reg_offset, pvgi1->trigOn, mask);
    }
_start_dma:
    i = 0;
#if (defined(IP22) || defined(IP28))
    do {
	i++;
    } while ( (!((*lio_isr0) & LIO_GIO_1)) && (i < VGI1_POLL_LARGE_TIME_OUT));
#else 
    do {
	/* us_delay(25); */
	i++;
   	BR_REG_RD_32(BRIDGE_INT_STATUS,0xffffffff,int_val);
        int_val = int_val & 0x10;
    } while ( (!int_val) && (i < VGI1_POLL_LARGE_TIME_OUT));
#endif
    if (i == VGI1_POLL_LARGE_TIME_OUT) {
	msg_printf(ERR, "IMPV:- VGI1 DMA Interrupt not reached CPU\n");
	errors++;
	goto __end_of_process_dma;
    }

    VGI1_R64(mgvbase, pvgi1->ch_status_reg_offset,
                mgvbase->VGI1DMAOpErrStatus, mask);

#if (defined(IP22) || defined(IP28))
    *lio_isr0 = 0x0;
#else
    VGI1_W64(mgvbase, pvgi1->ch_status_reg_offset, intEnableVal, mask);
#endif

    tmpError = 0x1;
    if (mgvbase->dualLink||(mgvbase->VGI1DMAOpErrStatus & MGV_VGI1_CH_E_DMA)) {
	tmpError = 0x0;
    }

    if (!tmpError) {
        if ( (mgvbase->nFields == 2) || (mgvbase->vout_freeze) ) {
            VGI1_W64(mgvbase, pvgi1->ch_status_reg_offset, intEnableVal, mask);
            /* Program the DMA stride */
            VGI1_W64(mgvbase, pvgi1->ch_dma_stride_reg_offset,
                        pvgi1->stride, mask);
            /* Program host data physical address DATA_ADR */
            VGI1_W64(mgvbase, pvgi1->ch_data_adr_reg_offset,
                        ((__paddr_t)pvgi1->pDMADataPhys), mask);
            /* program the descriptor address for the next field */
            VGI1_W64(mgvbase, pvgi1->ch_desc_adr_reg_offset,
                        ((__paddr_t)pvgi1->pFieldDescPhys), mask);
            if (mgvbase->vout_freeze) {
                if (k >= mgvbase->vout_freeze) {
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
#else 
    	    do {
		j++;
   		BR_REG_RD_32(BRIDGE_INT_STATUS,0xffffffff,int_val);
        	int_val = int_val & 0x10;
    	    } while ( (!int_val) && (j < VGI1_POLL_LARGE_TIME_OUT));
#endif

            if (j == VGI1_POLL_LARGE_TIME_OUT) {
                msg_printf(ERR, "IMPV:- VGI1 DMA Interrupt not reached CPU\n");
                errors++;
                goto __end_of_process_dma;
            }

     	    VGI1_R64(mgvbase, pvgi1->ch_status_reg_offset,
                 mgvbase->VGI1DMAOpErrStatus, mask);

#if (defined(IP22) || defined(IP28))
            *lio_isr0 = 0x0;
#else
    	    VGI1_W64(mgvbase, pvgi1->ch_status_reg_offset, intEnableVal, mask);
#endif
            if (mgvbase->dualLink) {
                VGI1_W64(mgvbase, VGI1_CH1_TRIG, pvgi1->trigOff, mask);
                VGI1_W64(mgvbase, VGI1_CH2_TRIG, pvgi1->trigOff, mask);
            } else {
                VGI1_W64(mgvbase, pvgi1->ch_trig_reg_offset,
                        pvgi1->trigOff, mask);
            }
            tmpError = 0x1;
            if (mgvbase->dualLink ||
               (mgvbase->VGI1DMAOpErrStatus & MGV_VGI1_CH_E_DMA)) {
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
    if (_mgv_VGIDMAClear(mgvbase->chan)) {
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
    msg_printf(DBG, "Leaving _mgv_VGI1StartAndProcessDMA\n");

    return(errors);
}

__uint32_t
_mgv_VGIDMAClear(__uint32_t chan)
{
    __uint32_t		errors = 0;
    mgv_vgi1_info_t	*pvgi1 = &(mgvbase->vgi1[chan]);
    ulonglong_t		status, mask = ~0x0;
#if (defined(IP22) || defined(IP28))
    volatile unsigned char *lio_isr0 = K1_LIO_0_ISR_ADDR;
#endif
    msg_printf(DBG, "Entering _mgv_VGIDMAClear..., chan 0x%x\n", chan);
    /*
     * 1. disable all interrupts
     * 2. abort any previous DMAs
     * 3. turn the trigger off
     * 4. clear all interrupt flags
     */
    VGI1_W64(mgvbase, pvgi1->ch_int_enab_reg_offset, 0x0, mask);
    VGI1_W64(mgvbase, pvgi1->ch_trig_reg_offset, pvgi1->abortDMA, mask);
    VGI1_W64(mgvbase, pvgi1->ch_trig_reg_offset, pvgi1->trigOff, mask);
    VGI1_W64(mgvbase, pvgi1->ch_status_reg_offset, MGV_VGI1_CH_ALLINTRS, mask);

    /* Nothing should be set... check for that */
    if (_mvg_VGI1PollReg(pvgi1->ch_status_reg_offset, 0x0, MGV_VGI1_CH_ALLINTRS,
		VGI1_POLL_LARGE_TIME_OUT, &status)) {
	msg_printf(ERR, "Status register is not ZERO... Time Out\n");
	errors++;
	mgvbase->VGI1DMAClrErrStatus = status;
    }

#if (defined(IP22) || defined(IP28))
    *lio_isr0 = 0x0;
#endif
    msg_printf(DBG, "Leaving _mgv_VGIDMAClear...\n");

    return (errors);
}

/*
 *  _mgv_dmaPixelDepth
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
_mgv_dmaPixelDepth(__uint32_t giofmt)
{
    __uint32_t depth;

    switch(giofmt) {

	case MGV_VGI1_FMT_YUV422_8:
	case MGV_VGI1_FMT_ARB_8:
		depth = 2;
		break;

	case MGV_VGI1_FMT_YUV422_10:
	case MGV_VGI1_FMT_AUYV4444_8:
	case MGV_VGI1_FMT_ABGR_8:
	case MGV_VGI1_FMT_VYUA4444_8:
	case MGV_VGI1_FMT_RGBA_8:
	case MGV_VGI1_FMT_ARB_10:
	case MGV_VGI1_FMT_AYUAYV4224_10:
		depth = 4;
		break;
	
	case MGV_VGI1_FMT_AUYV4444_10:
	case MGV_VGI1_FMT_VYUA4444_10:
	case MGV_VGI1_FMT_ABGR_10:
	case MGV_VGI1_FMT_RGBA_10:
		depth = 8;
		break;
    }  /* End switch */

    return depth;
}

__uint32_t
_mgv_VGI1DMADispErr(void)
{
    ulonglong_t	opErr, clrErr;
    __uint32_t errors = 0;

    opErr = mgvbase->VGI1DMAOpErrStatus;
    clrErr = mgvbase->VGI1DMAClrErrStatus;

    msg_printf(DBG, "opErr 0x%llx; clrErr 0x%llx\n", opErr, clrErr);
    errors += _mgv_VGI1DMADispErrActual(opErr,"MGV:- VGI1 DMA Operation Error");
    errors += _mgv_VGI1DMADispErrActual(clrErr,"MGV:- VGI1 DMA Clean-up Error");

    return(errors);
}

__uint32_t
_mgv_VGI1DMADispErrActual(ulonglong_t err, char *str)
{
    __uint32_t errors = 0;

    if (err & MGV_VGI1_CH_B_FIFO) {
	msg_printf(ERR, "%s - Bad FIFO\n", str);
	errors++;
    }
    if (err & MGV_VGI1_CH_B_VCLK) {
	msg_printf(ERR, "%s - Bad video clock\n", str);
	errors++;
    }
    if (err & MGV_VGI1_CH_B_HL) {
	msg_printf(ERR, "%s - Bad horizontal length\n", str);
	errors++;
    }
    if (err & MGV_VGI1_CH_B_VL) {
	msg_printf(ERR, "%s - Bad vertical length\n", str);
	errors++;
    }
    if (err & MGV_VGI1_CH_UNUSED) {
	msg_printf(ERR, "%s - Unused last descriptor\n", str);
	errors++;
    }
    if (err & MGV_VGI1_CH_INSUF) {
	msg_printf(ERR, "%s - Insufficient descriptors\n", str);
	errors++;
    }

    return(errors);
}

