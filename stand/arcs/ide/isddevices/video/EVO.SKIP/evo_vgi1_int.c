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
**      FileName:       evo_vgi1_int.c
*/

#include "evo_diag.h"
#if (defined(IP22) || defined(IP28))
#include "sys/cpu.h"
#endif


/*
 * NAME: evo_VGI1IntTest
 *
 * INPUTS: channel number
 *
 * OUTPUTS: -1 if error, 0 if no error
 */
__uint32_t
evo_VGI1IntTest (__uint32_t argc, char **argv)
{
    __uint32_t	errors = 0;
    __uint32_t	chan = VGI1_CHANNEL_1;
    __uint32_t	intType = 0x0;
    __uint32_t	badarg = 0;
    __uint32_t	xp, intReg, statusReg, dataBufSize;
    __uint32_t	tmp, savBurstLen;
    ulonglong_t	status, saveIntEnabVal, mask = ~0x0;
    ulonglong_t	saveHOff, saveHLen, saveVOff, saveVLen;
    evo_vgi1_info_t	*pvgi1;
#if (defined(IP22) || defined(IP28))
    volatile unsigned char *lio_isr0 = K1_LIO_0_ISR_ADDR;
#endif

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
                case 'i':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &intType);
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &intType);
			}
                        break;
                default: badarg++; break;
        }
        argc--; argv++;
    }

    if ( badarg || argc || (intType > 0x20) || (!intType) ||
	((chan != VGI1_CHANNEL_1) && (chan != VGI1_CHANNEL_2))) {
	msg_printf(SUM,
	 "Usage: evo_vgiint -c [1|2] -i[0x1|0x2|0x4|0x8|0x10|0x20]\n\
	 -c --> channel [1|2]\n\
	 -i --> interrupt type [0x1|0x2|0x4|0x8|0x10|0x20]\n\
		 0x1  - Vertical Interrupt\n\
		 0x2  - Bad Horizontal Length\n\
		 0x4  - Bad Vertical Length\n\
		 0x8  - Bad FIFO\n\
		 0x10 - Unused Last Descriptor\n\
		 0x20 - Insufficient Descriptors\n");
	return(-1);
    }

    msg_printf(SUM, "IMPV:- %s channel 0x%x ; int type 0x%x\n", 
	evo_err[VGI1_INTERRUPT_TEST].TestStr, (chan+1), intType);

    /* get the pointer to the structure */
    pvgi1 = &(evobase->vgi1[chan]);
    statusReg = pvgi1->ch_status_reg_offset;
    intReg = pvgi1->ch_int_enab_reg_offset;

    /* Send the black signal to the channel 1(2) input of the VGI1 */
    EVO_SET_VBAR2_BASE;
    xp =  MUXB_BLACK_REF_IN ;
    EVO_VBAR_W1(evobase, EVOB_TO_OTHER_VBAR_CH1 , xp); /*XXX Check VBAR1/2 Address*/
    EVO_SET_VBAR1_BASE;
    xp = MUX_TO_OTHER_VBAR_CH1;
    EVO_VBAR_W1(evobase, EVOA_VGI1_CH1_IN, xp); /*XXX Check VBAR1/2 Address*/
    EVO_VBAR_W1(evobase, EVOA_VGI1_CH2_IN, xp); /*XXX Check VBAR1/2 Address*/

    evobase->chan = chan;
    evobase->tvStd = VGI1_NTSC_CCIR601;
    evobase->vidFmt = EVO_VGI1_FMT_YUV422_8;
    evobase->nFields = 2;
    evobase->vin_round = EVO_VGI1_VIN_RND_OFF;
    evobase->vout_expd = EVO_VGI1_VOUT_EXPD_OFF;
    evobase->VGIDMAOp = VGI1_DMA_READ;
    evobase->vout_freeze = 0x0;
    evobase->VGI1DMAOpErrStatus = 0;
    evobase->VGI1DMAClrErrStatus = 0;

    /* generate the data in memory */
    evobase->transferSize = _evo_VGI1DMAGetBufSize();
    dataBufSize = evobase->transferSize + evobase->pageSize + 32;
    if ((evobase->pVGIDMABuf = (uchar_t *)get_chunk(dataBufSize)) == NULL) {
	msg_printf(ERR, "Insufficient memory for Read Data Buffer\n");
	return(-1);
    }

    saveIntEnabVal = evobase->intEnableVal;

    /* Vertical Interrupt Test */
    if (intType & 0x1) {
#if (defined(IP22) || defined(IP28))
	/* clear the interrupt */
	*lio_isr0 = 0x0;
#endif

	/* clear the status register */
	VGI1_W64(evobase, statusReg, EVO_VGI1_CH_ALLINTRS, mask);

	/* enable the vertical interrupt bit */
	VGI1_W64(evobase, intReg, EVO_VGI1_CH_V_INT, mask);

#if (defined(IP22) || defined(IP28))
	/* wait for the interrupt */
	tmp = 0x0;
	do {
	   tmp++;
	} while ((!((*lio_isr0)&LIO_GIO_1)) && (tmp<VGI1_POLL_LARGE_TIME_OUT));
#endif

	/* read the status register and verify the interrupt bit */
	VGI1_R64(evobase, statusReg, status, EVO_VGI1_CH_V_INT);
	if (!(status & EVO_VGI1_CH_V_INT)) {
	   msg_printf(ERR, "Vertical Interrupt Not Set\n");
	   errors++;
	} else {
	   msg_printf(DBG, "Vertical Interrupt Bit Set\n");
	}
	VGI1_R64(evobase, statusReg, status, mask);
	msg_printf(DBG, "statusReg 0x%llx\n", status);
    } else if (intType & 0x2) {
    	/* Bad Horizontal Length */
	saveHOff = hpNTSC.h_offset;
	saveHLen = hpNTSC.h_len;
	hpNTSC.h_offset = 0x1;
	hpNTSC.h_len = 0x1;
	evobase->intEnableVal = EVO_VGI1_CH_B_HL;
	_evo_VGI1DMA();
	if (!(evobase->VGI1DMAOpErrStatus & EVO_VGI1_CH_B_HL)) {
	   msg_printf(ERR, "Bad Horizontal Length Interrupt Not Set\n");
	   errors++;
	} else {
	   msg_printf(DBG, "Bad Horizontal Length Bit Set\n");
	}
	hpNTSC.h_offset = saveHOff;
	hpNTSC.h_len = saveHLen;
	msg_printf(DBG, "statusReg 0x%llx\n", evobase->VGI1DMAOpErrStatus);
    } else if (intType & 0x4) {
    	/* Bad Vertical Length */
	saveVOff = vpNTSC.odd_offset;
	saveVLen = vpNTSC.odd_len;
	vpNTSC.odd_offset = 0x0;
	vpNTSC.odd_len = 0x0;
	evobase->intEnableVal = EVO_VGI1_CH_B_VL;
	_evo_VGI1DMA();
	if (!(evobase->VGI1DMAOpErrStatus & EVO_VGI1_CH_B_VL)) {
	   msg_printf(ERR, "Bad Vertical Length Interrupt Not Set\n");
	   errors++;
	} else {
	   msg_printf(DBG, "Bad Vertical Length Bit Set\n");
	}
	vpNTSC.odd_offset = saveVOff;
	vpNTSC.odd_len = saveVLen;
	msg_printf(DBG, "statusReg 0x%llx\n", evobase->VGI1DMAOpErrStatus);
    } else if (intType & 0x8) {
    	/* Bad FIFO Test */
	savBurstLen = evobase->burst_len;
	evobase->burst_len = 0xff;
	evobase->intEnableVal = EVO_VGI1_CH_B_FIFO;
	_evo_VGI1DMA();
	if (!(evobase->VGI1DMAOpErrStatus & EVO_VGI1_CH_B_FIFO)) {
	   msg_printf(ERR, "Bad FIFO Interrupt Not Set\n");
	   errors++;
	} else {
	   msg_printf(DBG, "Bad FIFO Bit Set\n");
	}
	evobase->burst_len = savBurstLen;
	msg_printf(DBG, "statusReg 0x%llx\n", evobase->VGI1DMAOpErrStatus);
    } else if (intType & 0x10) {
    	/* Unused Last Descriptor */
	evobase->force_unused = 0x1;
	evobase->intEnableVal = EVO_VGI1_CH_UNUSED;
	_evo_VGI1DMA();
	if (!(evobase->VGI1DMAOpErrStatus & EVO_VGI1_CH_UNUSED)) {
	   msg_printf(ERR, "Unused Last Descriptor Interrupt Not Set\n");
	   errors++;
	} else {
	   msg_printf(DBG, "Unused Last Descriptor Bit Set\n");
	}
	evobase->force_unused = 0x0;
	msg_printf(DBG, "statusReg 0x%llx\n", evobase->VGI1DMAOpErrStatus);
    } else if (intType & 0x20) {
    	/* Insufficient Descriptors */
	evobase->force_insuf = 0x1;
	evobase->intEnableVal = EVO_VGI1_CH_INSUF;
	_evo_VGI1DMA();
	if (!(evobase->VGI1DMAOpErrStatus & EVO_VGI1_CH_INSUF)) {
	   msg_printf(ERR, "Insufficient Descriptors Interrupt Not Set\n");
	   errors++;
	} else {
	   msg_printf(DBG, "Insufficient Descriptors Bit Set\n");
	}
	evobase->force_insuf = 0x0;
	msg_printf(DBG, "statusReg 0x%llx\n", evobase->VGI1DMAOpErrStatus);
    }

    evobase->intEnableVal = saveIntEnabVal;

    return(_evo_reportPassOrFail((&evo_err[VGI1_INTERRUPT_TEST]), errors));
}

