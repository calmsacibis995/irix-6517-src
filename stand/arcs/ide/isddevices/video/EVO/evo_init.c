/*
**               Copyright (C) 1989, Silicon Graphics, Inc.              
**                                                                      
**  These coded instructions, statements, and computer programs  contain 
**  unpublished  proprietary  information of Silicon Graphics, Inc., and 
**  are protected by Federal copyright law.  They  may  not be disclosed  
**  to  third  parties  or copied or duplicated in any form, in whole or
**  in part, without the prior written consent of Silicon Graphics, Inc. 
*/

#include "evo_diag.h"
#include "../../../godzilla/include/d_bridge.h"
#include "../../../godzilla/include/d_godzilla.h"


/*
 * Forward References 
 */
__uint32_t	evo_probe(void);
__uint32_t	evo_init(__uint32_t argc, char **argv);
__uint32_t	evo_csc_probe(void);
__uint32_t	_evo_initI2C(void);
__uint32_t 	_evo_initVGI1(void);
/*__uint32_t	_evo_inputStandardDetection(void);*/
__uint32_t	_evo_VGI1GetChipID(void);
__uint32_t	_evo_initXPoint(int xpoint_id);
/*__uint32_t	_evo_initVideoInput(void);*/
/*__uint32_t	_evo_initVideoOutput(void);*/
__uint32_t 	_evo_initCSC(void);
__uint32_t	_evo_probe_bridge(void);
__uint32_t      evo_initBridge(void);
__uint32_t	_evo_board_probe(void);
__uint32_t	_evo_probe_voc(void);        /*XXX this portion to be defined*/
__uint32_t	_evo_initVOC(void);        /*XXX this portion to be defined*/
__uint32_t	_evo_probe_enc(void);        /*XXX this portion to be defined*/
__uint32_t	_evo_initENC(void);        /*XXX this portion to be defined*/
__uint32_t	_evo_probe_dec(void);        /*XXX this portion to be defined*/
__uint32_t	_evo_initDEC(void);        /*XXX this portion to be defined*/
__uint32_t	_evo_initCfpga(void);        
__uint32_t	_evo_reset_scl(void);        /*XXX this portion to be defined*/
__uint32_t	_evo_initSCL(int scl_id);        /*XXX this portion to be defined*/
__uint32_t	_evo_VOCInt(void);        /*XXX this portion to be defined*/
/*__uint32_t	_evo_initGenlock(void);        XXX this portion to be defined*/

__uint32_t
evo_probe(void)
{
    __uint32_t port;

    msg_printf(SUM, "--> Evo_probe started\n");
    mgras_probe_all_widgets();

    port = _evo_probe_bridge();
    evo_bridge_port = port;
    msg_printf(DBG, "port 0x%x\n", port);
    if (!port) {
        msg_printf(SUM, "--> Evo_probe failed\n");
	return(0);
    }

    evobase->vgi1_1_base = (void *)gio2xbase(port,
                ((PHYS_TO_K1(EVO_VGI1_1_DEV_ADDR))));
   /***         ((PHYS_TO_K1(EVO_VGI1_1_DEV_ADDR)) + 4)); ***/
    evobase->vgi1_2_base = (void *)gio2xbase(port,
                ((PHYS_TO_K1(EVO_VGI1_2_DEV_ADDR))));
   /***         ((PHYS_TO_K1(EVO_VGI1_2_DEV_ADDR)) + 4)); ***/
    evobase->gio_pio_base = (void *)gio2xbase(port,
                ((PHYS_TO_K1(EVO_GIO_PIO_DEV_ADDR))));
   /***         ((PHYS_TO_K1(EVO_GIO_PIO_DEV_ADDR)) + 4)); ***/


   /* XXX procedures to get addresses of all ASICS*/
   /* add appropriate offsets to the VGI addresses*/

    evobase->scl1= (__paddr_t)evobase->gio_pio_base + (__paddr_t)SCL_1_OFFSET;
    evobase->scl2= (__paddr_t)evobase->gio_pio_base + (__paddr_t)SCL_2_OFFSET;

    evobase->voc= (__paddr_t)evobase->gio_pio_base + (__paddr_t)VOC_OFFSET;

    evobase->evo_misc= (__paddr_t)evobase->gio_pio_base + (__paddr_t)(MISC_OFFSET);

    /*************Initailaizing VBAR addresses*/
    evobase->vbar1= (__paddr_t)evobase->gio_pio_base + (__paddr_t)(VBAR_1_OFFSET);
    evobase->vbar2= (__paddr_t)evobase->gio_pio_base + (__paddr_t)(VBAR_2_OFFSET);

    /****XXX********* Offsets to encoder/decoder not given */
    evobase->i2c= (__paddr_t)evobase->gio_pio_base + (__paddr_t)I2C_OFFSET;
    evobase->enc= (__paddr_t)evobase->gio_pio_base + (__paddr_t)ENC_OFFSET;
    evobase->dec= (__paddr_t)evobase->gio_pio_base + (__paddr_t)DEC_OFFSET;

   /*
    * Get the base addresses from mgrashq4.h 
    *
    * XXX CSC procedure needs to be changed for EVO
    */
    evobase->dcb_crs = (__paddr_t)evobase->gio_pio_base + (__paddr_t)DCB_CRS_OFFSET;
    evobase->csc1= (__paddr_t)evobase->gio_pio_base + (__paddr_t)CSC_1_OFFSET;
    /*evobase->csc1=(__paddr_t)0x900000001c073800;*/
    evobase->csc2= (__paddr_t)evobase->gio_pio_base + (__paddr_t)CSC_2_OFFSET;
    evobase->csc3= (__paddr_t)evobase->gio_pio_base + (__paddr_t)CSC_3_OFFSET;

    msg_printf(DBG, "brbase 0x%x; vgi1_1_base 0x%x; vgi1_2_base 0x%x; \n \
	gio_pio_base 0x%x; \n\
	csc1 0x%x; csc2 0x%x; csc3 0x%x; scl1 0x%x; \n \
	scl2 0x%x; voc 0x%x; enc 0x%x; dec 0x%x \n",
	evobase->brbase, evobase->vgi1_1_base, evobase->vgi1_2_base,
	evobase->gio_pio_base,
	evobase->csc1, evobase->csc2, evobase->csc3, evobase->scl1, 
	evobase->scl2, evobase->voc, evobase->enc, evobase->dec);

    msg_printf(SUM, "--> Evo_probe passed\n");

    return(port);
}

__uint32_t
evo_init(__uint32_t argc, char **argv)
{
    __uint32_t		i, device_num = 0, gio64_arb, errors = 0;
    evo_vgi1_info_t     *pvgi1 = &(evobase->vgi1[0]);
    evo_vgi1_info_t     *pvgi2 = &(evobase->vgi1[1]);

    msg_printf(SUM, "--> Evo_init started\n");
    msg_printf(SUM, "IMPV:- %s\n", evo_err[EVO_INIT_TEST].TestStr);

    /* get the args */
    argc--; argv++;
    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 'd':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &device_num);
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &device_num);
			}
                        break;
                default: break;
        }
        argc--; argv++;
    }

    /*XXX requires correlation with earlier definitions*/
    if ( device_num > EVO_LAST_DEVICE_NUM) { 
	msg_printf(SUM, 
	"Usage: evo_init -d <device_num> \n \
	 -d --> device number [1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20] \n\
		 1  - EVO board \n\
		 2  - Bridge \n\
		 3  - Moosecam Input \n\
		 4  - Cross Point 1 \n\
		 5  - Cross Point 2 \n\
		 6  - VGI1-1 \n\
		 7  - VGI1-2 \n\
		 8  - CSC1 \n\
		 9  - CSC2 \n\
		10  - CSC3 \n\
		11  - SCL1 \n\
		12  - SCL2 \n\
		13  - VOC \n\
		14  - VOC Interrupt \n\
		15  - I2C \n\
		16  - Encoder \n\
		17  - Decoder \n \
		18  - Control FPGA \n \
		19  - Genlock \n \
		20  - All VGIs \n \
		21  - All CSCs \n \
		22  - Scaler Resets \n ");
	return(-1);
    }


    /*
     * XXX: Setup the page size and masks
     */
    evobase->pageBits = VGI1_PAGE_SIZE_4K_BITS;
    evobase->pageSize = (0x1 << evobase->pageBits);
    evobase->pageMask = ((__paddr_t) ~0x0) << evobase->pageBits;

    msg_printf(DBG, "pageBits 0x%x; pageSize 0x%x; pageMask 0x%x\n",
	evobase->pageBits, evobase->pageSize, evobase->pageMask);

    /*
     * Initialize some of the paramters for VGI1 DMA tests
     */
    hpNTSC.h_offset 	= EVO_VGI1_CCIR601_525_H_OFS;
    hpNTSC.h_len 	= EVO_VGI1_CCIR601_525_H_LEN;
    vpNTSC.odd_offset 	= EVO_VGI1_CCIR601_525_ODD_OFS;
    vpNTSC.odd_len 	= EVO_VGI1_CCIR601_525_ODD_LEN;
    vpNTSC.even_offset 	= EVO_VGI1_CCIR601_525_EVEN_OFS;
    vpNTSC.even_len 	= EVO_VGI1_CCIR601_525_EVEN_LEN;
    hpPAL.h_offset 	= EVO_VGI1_CCIR601_625_H_OFS;
    hpPAL.h_len 	= EVO_VGI1_CCIR601_625_H_LEN;
    vpPAL.odd_offset 	= EVO_VGI1_CCIR601_625_ODD_OFS;
    vpPAL.odd_len 	= EVO_VGI1_CCIR601_625_ODD_LEN;
    vpPAL.even_offset 	= EVO_VGI1_CCIR601_625_EVEN_OFS;
    vpPAL.even_len 	= EVO_VGI1_CCIR601_625_EVEN_LEN;
    hpNTSCSQ.h_offset 	= EVO_VGI1_NTSC_SQ_PIXEL_H_OFS;
    hpNTSCSQ.h_len 	= EVO_VGI1_NTSC_SQ_PIXEL_H_LEN;
    vpNTSCSQ.odd_offset	= EVO_VGI1_NTSC_SQ_PIXEL_ODD_OFS;
    vpNTSCSQ.odd_len 	= EVO_VGI1_NTSC_SQ_PIXEL_ODD_LEN;
    vpNTSCSQ.even_offset= EVO_VGI1_NTSC_SQ_PIXEL_EVEN_OFS;
    vpNTSCSQ.even_len 	= EVO_VGI1_NTSC_SQ_PIXEL_EVEN_LEN;
    hpPALSQ.h_offset 	= EVO_VGI1_PAL_SQ_PIXEL_H_OFS;
    hpPALSQ.h_len 	= EVO_VGI1_PAL_SQ_PIXEL_H_LEN;
    vpPALSQ.odd_offset 	= EVO_VGI1_PAL_SQ_PIXEL_ODD_OFS;
    vpPALSQ.odd_len 	= EVO_VGI1_PAL_SQ_PIXEL_ODD_LEN;
    vpPALSQ.even_offset = EVO_VGI1_PAL_SQ_PIXEL_EVEN_OFS;
    vpPALSQ.even_len 	= EVO_VGI1_PAL_SQ_PIXEL_EVEN_LEN;

    /* setup the register offsets for both channels */
    pvgi1->ch_setup_reg_offset 		= VGI1_CH1_SETUP;
    pvgi1->ch_trig_reg_offset 		= VGI1_CH1_TRIG;
    pvgi1->ch_dma_stride_reg_offset	= VGI1_CH1_DMA_STRIDE;
    pvgi1->ch_data_adr_reg_offset	= VGI1_CH1_DATA_ADR;
    pvgi1->ch_desc_adr_reg_offset	= VGI1_CH1_DESC_ADR;
    pvgi1->ch_status_reg_offset 	= VGI1_CH1_STATUS;
    pvgi1->ch_int_enab_reg_offset 	= VGI1_CH1_INT_ENAB;
    pvgi1->ch_hparam_reg_offset 	= VGI1_CH1_H_PARAM;
    pvgi1->ch_vparam_reg_offset 	= VGI1_CH1_V_PARAM;
    /*pvgi1->ch_vout_ctrl_reg_offset 	= GAL_CH1_CTRL;*/
    pvgi1->ch_vid_src			= EVO_VGI1_VID_SRC_1;

    pvgi2->ch_setup_reg_offset 		= VGI1_CH2_SETUP;
    pvgi2->ch_trig_reg_offset 		= VGI1_CH2_TRIG;
    pvgi2->ch_dma_stride_reg_offset	= VGI1_CH2_DMA_STRIDE;
    pvgi2->ch_data_adr_reg_offset	= VGI1_CH2_DATA_ADR;
    pvgi2->ch_desc_adr_reg_offset	= VGI1_CH2_DESC_ADR;
    pvgi2->ch_status_reg_offset 	= VGI1_CH2_STATUS;
    pvgi2->ch_int_enab_reg_offset 	= VGI1_CH2_INT_ENAB;
    pvgi2->ch_hparam_reg_offset 	= VGI1_CH2_H_PARAM;
    pvgi2->ch_vparam_reg_offset 	= VGI1_CH2_V_PARAM;
    /*pvgi2->ch_vout_ctrl_reg_offset 	= GAL_CH2_CTRL;*/
    pvgi2->ch_vid_src			= EVO_VGI1_VID_SRC_2;
    pvgi1->stride = pvgi2->stride = 0x0; 

    /* copy the trigger on, off and abort values for later use */
    bcopy((char *)&evo_trig_on, (char *)&(pvgi1->trigOn), 
		sizeof(evo_vgi1_ch_trig_t));
    bcopy((char *)&evo_trig_off, (char *)&(pvgi1->trigOff), 
		sizeof(evo_vgi1_ch_trig_t));
    bcopy((char *)&evo_abort_dma, (char *)&(pvgi1->abortDMA),
		sizeof(evo_vgi1_ch_trig_t));
    pvgi2->trigOn = pvgi1->trigOn;
    pvgi2->trigOff = pvgi1->trigOff;
    pvgi2->abortDMA = pvgi1->abortDMA;

    evobase->burst_len = 0x40;
    evobase->force_unused = 0x0;
    evobase->force_insuf = 0x0;
    evobase->intEnableVal = 0x3f08;

    /*
     * Initialize ALL or selected device
     */

    msg_printf(DBG,"Calling evo_reset for device # %d\n",device_num);
    if (!device_num) {
	for (i = EVO_BOARD_INIT_NUM; i <= EVO_LAST_DEVICE_NUM; i++) {
		if (i == EVO_BOARD_INIT_NUM){
			evo_probe();
		}
		errors += _evo_reset(i);
	}
    } else {
    	errors += _evo_reset(device_num);
    }
    if(errors == 0){
	msg_printf(SUM, "--> Evo_init passed\n");
    } else {
	msg_printf(SUM, "--> Evo_init failed\n");
    }

    return(_evo_reportPassOrFail((&evo_err[EVO_INIT_TEST]), errors));

}

__uint32_t
_evo_probe_bridge(void)
{
    __uint32_t i, alive, present, mfg, rev, part, port = 0;

#if HQ4

    msg_printf(SUM, "--> Evo_probe_bridge started\n");

    for (i = 0; i < MAX_XTALK_PORT_INFO_INDEX; i++) {
	alive = mg_xbow_portinfo_t[i].alive;
	present = mg_xbow_portinfo_t[i].present;

	msg_printf(DBG, "index %d; alive 0x%x\n", i, alive);

	if (alive && present) {
	    mfg = mg_xbow_portinfo_t[i].mfg;
	    rev = mg_xbow_portinfo_t[i].rev;
	    part = mg_xbow_portinfo_t[i].part;
	    msg_printf(DBG, "mfg 0x%x; rev 0x%x; part 0x%x\n",
		mfg, rev, part);
	    if ((mfg == BRIDGE_WIDGET_MFGR_NUM) &&
	        (part == BRIDGE_WIDGET_PART_NUM)) {
		port = mg_xbow_portinfo_t[i].port;
		evobase->brbase = mg_xbow_portinfo_t[i].base;

		msg_printf(SUM, "EVO for SR Found in port# %d\n", port);
		msg_printf(DBG, "port_base 0x%x\n", evobase->brbase);
		break;
	    }
	}
    }

    if (!port) {
	msg_printf(ERR, "EVO for SR NOT Found!!!\n");
	msg_printf(SUM, "--> Evo_probe_bridge failed\n");
    } else {
		msg_printf(SUM, "--> Evo_probe bridge passed\n");
    }
#endif

    return(port);
}

__uint32_t
evo_initBridge(void)
{

    int                     rv;
    int                     n;
    bridgereg_t             intr_device = 0;
    bridgereg_t             write_val, sav_val;
    bridgereg_t             br_addr_val = 0x7f0043f0; /*XXX OR with port # for real addr. Hardcoded value*/
    giobr_bridge_config_t   bridge_config = &evo_srv_bridge_config_2vgi1;

/*** Initialize bridge for VGI1 DMA ***/
    msg_printf(SUM,"--> Evo_initBridge started\n");
    bridge_xbow_port = evo_bridge_port;
    br_addr_val = br_addr_val | bridge_xbow_port; /*XXX OR with port number to get real address*/
    /*
     * RRB's are initialized to all zeros
     */
    PIO_REG_WR_64(BRIDGE_EVEN_RESP,0xffffffff,0x0);
    msg_printf(DBG,"Wrote in BRIDGE_EVEN_RESP \n");
    PIO_REG_WR_64(BRIDGE_ODD_RESP,0xffffffff,0x0);
    msg_printf(DBG,"Wrote in BRIDGE_ODD_RESP \n");

    for (n = 0; n < 8; n++) {
        /*
         * Set up the Device(x) registers
         */
        if (bridge_config->mask & GIOBR_DEV_ATTRS) {
            write_val = GIOBR_DEV_CONFIG(bridge_config->device_attrs[n], n);
            PIO_REG_WR_64(BRIDGE_DEVICE(n),0x00ffffff, write_val);
    msg_printf(DBG,"Write to BRIDGE_DEVICE(%d) %x \n",n, write_val);
            PIO_REG_RD_64(BRIDGE_DEVICE(n),0x00ffffff, sav_val);
    msg_printf(DBG,"Read from BRIDGE_DEVICE(%d) %0x \n",n,sav_val);
        }
        /*
         * setup the RRB's, assumed to have been all cleared
         */
        if (bridge_config->mask & GIOBR_RRB_MAP) {
            rv = evo_br_rrb_alloc(n, bridge_config->rrb[n]);
            if (rv){
		 msg_printf(SUM, "--> Evo_initBridge failed\n");
	   	 return(1); /*** Warn - check return value ***/
            }
        }
        if (bridge_config->mask & GIOBR_INTR_DEV) {
            intr_device &= ~BRIDGE_INT_DEV_MASK(n);
            intr_device |= BRIDGE_INT_DEV_SET(bridge_config->intr_devices[n], n);
        }
     }


    if (bridge_config->mask & GIOBR_ARB_FLAGS) {
        PIO_REG_RD_64(BRIDGE_ARB,BRIDGE_ARB_MASK,sav_val);
        write_val = bridge_config->arb_flags | sav_val;
        PIO_REG_WR_64(BRIDGE_ARB,BRIDGE_ARB_MASK,write_val);
    }
    if (bridge_config->mask & GIOBR_INTR_DEV) {
        /* PIO_REG_WR_64(BRIDGE_INT_DEVICE,BRIDGE_INT_DEVICE_MASK,intr_device); */
        PIO_REG_WR_64(BRIDGE_INT_DEVICE,BRIDGE_INT_DEVICE_MASK,0x184000);   /*XXX Hardcoded value*/
        PIO_REG_RD_64(BRIDGE_INT_DEVICE,BRIDGE_INT_DEVICE_MASK,sav_val);
    msg_printf(DBG,"Read from BRIDGE_INT_DEVICE - %0x\n", sav_val);
    }

        /*** Write widget id number ***/
        PIO_REG_RD_64(BRIDGE_WID_CONTROL,BRIDGE_WID_CONTROL_MASK,sav_val);
        PIO_REG_WR_64(BRIDGE_WID_CONTROL,BRIDGE_WID_CONTROL_MASK,br_addr_val);
        PIO_REG_RD_64(BRIDGE_WID_CONTROL,BRIDGE_WID_CONTROL_MASK,sav_val);
    msg_printf(DBG,"Read from BRIDGE_WID_CONTROL - %0x\n", sav_val);

        /*** Write dir map and int_enable registers ***/
        PIO_REG_WR_64(BRIDGE_DIR_MAP,BRIDGE_DIR_MAP_MASK,0x800000);   /*XXX Hardcoded value*/
        PIO_REG_RD_64(BRIDGE_DIR_MAP,BRIDGE_DIR_MAP_MASK,sav_val);
    msg_printf(DBG,"Read from BRIDGE_DIR_MAP - %0x\n", sav_val);

        PIO_REG_WR_64(BRIDGE_INT_ENABLE,BRIDGE_INT_ENABLE_MASK,0x0);
        PIO_REG_RD_64(BRIDGE_INT_ENABLE,BRIDGE_INT_ENABLE_MASK,sav_val);
    msg_printf(DBG,"Read from BRIDGE_INT_ENABLE - %0x\n", sav_val);
        msg_printf(SUM, "--> Evo_initBridge passed\n");
        return(0);
}

/*Begin EVO BOARD reset/probe*/

__uint32_t
_evo_board_probe()
{
    __uint32_t errors = 0;
    unsigned short reset_word, temp_word;

	msg_printf(SUM, "--> Evo_reset started\n");
#if 0
	/*Reset the board*/
   	/* master s/w reset (bit 0 = 0) will reset the entire board */
   	/* including the VGI1. VGI1 reset (bit2 = 1) will keep the VGI1 */
   	/* in reset after we release the master reset below. */
	_evo_outb((evobase->evo_misc+(__paddr_t)MISC_SW_RESET), 0x4);
    	us_delay(100);
	msg_printf(DBG, "After writing 0x4 to MISC_SW_RESET\n");

   	/* release master s/w reset (bit0 = 1), but hold the VGI1 */
   	/* in reset (bit2 = 1) to let the VGI1 PLL's come up */
	_evo_outb((evobase->evo_misc+(__paddr_t)MISC_SW_RESET), 0x5);
    	us_delay(1);
	msg_printf(DBG, "After writing 0x5 to MISC_SW_RESET\n");

   	/* release the VGI1 reset (bit2 = 0) and let it come up */
	_evo_outb((evobase->evo_misc+(__paddr_t)MISC_SW_RESET), 0x1);
    	us_delay(100);
	msg_printf(DBG, "After writing 0x1 to MISC_SW_RESET\n");
#else 
	_evo_outb((evobase->evo_misc+(__paddr_t)MISC_SW_RESET), 0x1);
    	us_delay(1000);
#endif
#if 0
	/*reset scaler*/
	reset_word = _evo_inhw(evobase->evo_misc+(__paddr_t)MISC_STATUS_CONF); /*read register first */
	msg_printf(DBG, "status word is 0x%x\n",reset_word);
	temp_word = reset_word & 0xfbff; /*scaler reset (active low) is bit 10 -> 0xf[1011]ff -> 0xfbff*/
	msg_printf(DBG, "word being written to status register is 0x%x\n",temp_word);
	_evo_outhw(evobase->evo_misc+(__paddr_t)MISC_STATUS_CONF, temp_word); /*reset scaler */
	us_delay(500); /*scaler must stay low for at least 3 video lines*/

	/*take scaler out of reset*/
	temp_word = reset_word | 0x0400; /*scaler reset (bit 10) is made high -> 0x0[0100]00 ->0x400*/
	msg_printf(DBG, "word being written after reset to status register is 0x%x\n",temp_word);
	_evo_outhw(evobase->evo_misc+(__paddr_t)MISC_STATUS_CONF, reset_word); /*reset scaler */
	us_delay(500); 
#endif
    	/*Initialize MISC registers VOC,SCL,PLL */
	EVO_PIO_W1(evobase, PIO_VOC_PIXELS, 720);
        /*XXX tvStd code will need changing NTSC => 243, PAL=>288*/
        EVO_PIO_W1(evobase, PIO_VOC_LINES, 244); /*NTSC_CCIR_601*/
        EVO_PIO_W1(evobase, PIO_SCL1_PIXELS, 772);
        /*XXX tvStd code will need changing NTSC => 243, PAL=>288*/
        EVO_PIO_W1(evobase, PIO_SCL1_LINES, 244);
        EVO_PIO_W1(evobase, PIO_SCL2_PIXELS, 772);
        /*XXX tvStd code will need changing NTSC => 243, PAL=>288*/
        EVO_PIO_W1(evobase, PIO_SCL2_LINES, 244);

        /* Bit 5. Sel Ref Lock, Bit 4. lock to ref, Bit 3. PLL lock,
         * Bit 2. Color Frame lock, Bit 1. NTSC 525 Source Lines, Bit 0. NSQ Pixels*/
        _evo_outb((evobase->evo_misc+(__paddr_t)MISC_PLL_CONTROL), 0x26); /*Default values*/

        /* Bit 5. Normal Pixel, Bit 4. NTSC 525 lines, Bit 3. Filter Bypass,
         * Bit 2. VGI1A Trigger Normal, Bit 1. VGI1B Trigger Normal,    **********/
        _evo_outb((evobase->evo_misc+(__paddr_t)MISC_FILT_TRIG_CTL), 0x6); /*Default values*/
	msg_printf(SUM, "--> Evo_reset passed\n");
	return(0);

}

/*end EVO BOARD reset/probe*/

/*Begin EVO reset function*/

__uint32_t
_evo_reset(__uint32_t device_num)
{
    __uint32_t	errors = 0;

    msg_printf(DBG,"Entering evo_reset\n");

    switch (device_num) {
    case EVO_BOARD_INIT_NUM:
    	/* reset the board and probe it */
 	_evo_board_probe();	
	break;
	/*********************done***************************/

    case EVO_BRIDGE_INIT_NUM:
	/*initialize the bridge*/
	_evo_probe_bridge();
	evo_initBridge();
	break;
	/*********************done***************************/

    /*XXX Moosecam Input...procedure for switch below may not work
    case EVO_INPUT_INIT_NUM:
    	* find out if input present and set the TV standard *
    	errors += _evo_inputStandardDetection();
    	delay (300);
	break;
     *********************************************************/

    case EVO_XPOINT_1_INIT_NUM:
    	/* initialize the VBAR */
	msg_printf(SUM, "--> Evo_xbar1_init started\n");
    	errors += _evo_initXPoint(1); 
	if (errors==0){
		msg_printf(SUM, "--> Evo_xbar1_init passed\n");
	} else {
		msg_printf(SUM, "--> Evo_xbar1_init failed\n");
	}
	break;

    case EVO_XPOINT_2_INIT_NUM:
    	/* initialize the VBAR */
	msg_printf(SUM, "--> Evo_xbar2_init started\n");
    	errors += _evo_initXPoint(2); 
	if (errors==0){
		msg_printf(SUM, "--> Evo_xbar2_init passed\n");
	} else {
		msg_printf(SUM, "--> Evo_xbar2_init failed\n");
	}
	break;

    case EVO_VGI1_1_INIT_NUM:
	EVO_SET_VGI1_1_BASE;
	msg_printf(SUM, "--> Evo_vgi1A_init started\n");
	if (errors==0){
		msg_printf(SUM, "--> Evo_vgi1A_init passed\n");
	} else {
		msg_printf(SUM, "--> Evo_vgi1A_init failed\n");
	}
	errors += _evo_initVGI1();
	break;

    case EVO_VGI1_2_INIT_NUM:
	EVO_SET_VGI1_2_BASE;
	msg_printf(SUM, "--> Evo_vgi1B_init started\n");
    	errors += _evo_initVGI1();
	if (errors==0){
		msg_printf(SUM, "--> Evo_vgi1B_init passed\n");
	} else {
		msg_printf(SUM, "--> Evo_vgi1B_init failed\n");
	}
	break;

    case EVO_CSC_1_INIT_NUM:
    	delay (30);
	EVO_SET_CSC_1_BASE;
	msg_printf(SUM, "--> Evo_csc1_init started\n");
    	errors += _evo_initCSC();
	if (errors==0){
		msg_printf(SUM, "--> Evo_csc1_init passed\n");
	} else {
		msg_printf(SUM, "--> Evo_csc1_init failed\n");
	}
	break;

    case EVO_CSC_2_INIT_NUM:
    	delay (30);
	EVO_SET_CSC_2_BASE;
	msg_printf(SUM, "--> Evo_csc2_init started\n");
    	errors += _evo_initCSC();
	if (errors==0){
		msg_printf(SUM, "--> Evo_csc2_init passed\n");
	} else {
		msg_printf(SUM, "--> Evo_csc2_init failed\n");
	}
	break;

    case EVO_CSC_3_INIT_NUM:
    	delay (30);
	EVO_SET_CSC_3_BASE;
	msg_printf(SUM, "--> Evo_csc3_init started\n");
    	errors += _evo_initCSC();
	if (errors==0){
		msg_printf(SUM, "--> Evo_csc3_init passed\n");
	} else {
		msg_printf(SUM, "--> Evo_csc3_init failed\n");
	}
	break;

    case EVO_SCL_2_INIT_NUM:
		EVO_SET_SCL1_BASE;
		msg_printf(SUM, "--> Evo_scaler1_init started\n");
    		errors += _evo_initSCL(1);
		if (errors==0){
			msg_printf(SUM, "--> Evo_scaler1_init passed\n");
		} else {
			msg_printf(SUM, "--> Evo_scaler1_init failed\n");
		}
		break;

    case EVO_SCL_1_INIT_NUM:
		EVO_SET_SCL2_BASE;
		msg_printf(SUM, "--> Evo_scaler2_init started\n");
    		errors += _evo_initSCL(2);
		if (errors==0){
			msg_printf(SUM, "--> Evo_scaler2_init passed\n");
		} else {
			msg_printf(SUM, "--> Evo_scaler2_init failed\n");
		}
		break;

    case EVO_VOC_INIT_NUM:
    	/* initialize Video Input */
	msg_printf(SUM, "--> Evo_voc_init started\n");
    	errors += _evo_initVOC();
	if (errors==0){
		msg_printf(SUM, "--> Evo_voc_init passed\n");
	} else {
		msg_printf(SUM, "--> Evo_voc_init failed\n");
	}
	break;

    case EVO_VOC_INT_INIT_NUM:
    	/* initialize Video Input */
	msg_printf(SUM, "--> Evo_voc_int_init started\n");
    	errors += _evo_VOCInt();
	if (errors==0){
		msg_printf(SUM, "--> Evo_voc_int_init passed\n");
	} else {
		msg_printf(SUM, "--> Evo_voc_int_init failed\n");
	}
	break;

    case EVO_I2C_INIT_NUM:
    	/* initialize Video Output */
	msg_printf(SUM, "--> Evo_i2c_init started\n");
    	errors += _evo_initI2C();
	if (errors==0){
		msg_printf(SUM, "--> Evo_i2c_init passed\n");
	} else {
		msg_printf(SUM, "--> Evo_i2c_init failed\n");
	}
	break;

    case EVO_ENC_INIT_NUM:
    	/* initialize Video Output */
	msg_printf(SUM, "--> Evo_encoder_init started\n");
    	errors += _evo_initENC();
	if (errors==0){
		msg_printf(SUM, "--> Evo_encoder_init passed\n");
	} else {
		msg_printf(SUM, "--> Evo_encoder_init failed\n");
	}
	break;

    case EVO_DEC_INIT_NUM:
    	/* initialize Video Output */
	msg_printf(SUM, "--> Evo_decoder_init started\n");
    	errors += _evo_initDEC();
	if (errors==0){
		msg_printf(SUM, "--> Evo_decoder_init passed\n");
	} else {
		msg_printf(SUM, "--> Evo_decoder_init failed\n");
	}
	break;

    case EVO_CFPGA_INIT_NUM:
    	/* Readback the control FPGA */
	msg_printf(SUM, "--> Evo_control_fpga started\n");
    	errors += _evo_initCfpga();
	if (errors==0){
		msg_printf(SUM, "--> Evo_control_fpga passed\n");
	} else {
		msg_printf(SUM, "--> Evo_control_fpga failed\n");
	}
	break;

    case EVO_GENLOCK_INIT_NUM:
    	/* initialize Genlock */
    	/*errors += _evo_initGenlock();*/
	break;

    case EVO_VGIALL_INIT_NUM:
    	/* initialize all VGIs */
	msg_printf(SUM, "--> Evo_vgiall_reset started\n");
    	/*Bit 2 is VGI_RESET (reset is active low) */
    	_evo_outb((evobase->evo_misc+(__paddr_t)MISC_SW_RESET), 0xe);
	us_delay(1000000);
	msg_printf(DBG, "after writing 0xe in reset \n");
    	_evo_outb((evobase->evo_misc+(__paddr_t)MISC_SW_RESET), 0x9);
	us_delay(1000000);
	msg_printf(DBG, "after VGI1/PLL resetting write\n");
	/*VGIPLL should  be set to normal mode before setting VGI1 to normal mode*/ 
    	_evo_outb((evobase->evo_misc+(__paddr_t)MISC_SW_RESET), 0xe);
	us_delay(1000000);
	msg_printf(DBG, "after final normal 0xf write\n");
	EVO_SET_VGI1_1_BASE;
    	errors += _evo_initVGI1();
	EVO_SET_VGI1_2_BASE;
    	errors += _evo_initVGI1();
	if (errors==0){
		msg_printf(SUM, "--> Evo_vgiall_reset passed\n");
	} else {
		msg_printf(SUM, "--> Evo_vgiall_reset failed\n");
	}
	break;

    case EVO_CSCALL_INIT_NUM:
    	/* initialize all CSCs */
	msg_printf(SUM, "--> Evo_cscall_reset started\n");
    	/*Bit 3 is CSC_RESET (normal operation is active high) */
    	_evo_outb((evobase->evo_misc+(__paddr_t)MISC_SW_RESET), 0xe);
	us_delay(1000000);
    	/*Bit 3 is CSC_RESET (reset is active low) */
    	_evo_outb((evobase->evo_misc+(__paddr_t)MISC_SW_RESET), 0x7);
	us_delay(1000000);
    	/*Bit 3 is CSC_RESET (normal operation is active high) */
    	_evo_outb((evobase->evo_misc+(__paddr_t)MISC_SW_RESET), 0xe);
	us_delay(1000000);
	EVO_SET_CSC_1_BASE;
    	errors += _evo_initCSC();
    	delay (30);
	EVO_SET_CSC_2_BASE;
    	errors += _evo_initCSC();
    	delay (30);
	EVO_SET_CSC_3_BASE;
    	errors += _evo_initCSC();
	if (errors==0){
		msg_printf(SUM, "--> Evo_cscall_reset passed\n");
	} else {
		msg_printf(SUM, "--> Evo_cscall_reset failed\n");
	}
	break;

    case EVO_SCLALL_INIT_NUM:
	/*reset scalers*/
		msg_printf(SUM, "--> Evo_scl_reset started\n");
		errors += _evo_reset_scl();
		if (!errors) {
			msg_printf(SUM, "--> Evo_scl_reset passed\n");
		} else {
			msg_printf(SUM, "--> Evo_scl_reset failed\n");
		}
		break;
    }

    return(errors);
}

__uint32_t
_evo_reset_scl(void)
{
    __uint32_t i_val;

    i_val = 0xffffffff; /*used to be 0x1ff*/
    i_val &= (0xffffffff & ~(0x1 << 10));
    /*i_val |= (0x0 << 10);*/
    _evo_outw(evobase->evo_misc, i_val);
    us_delay(100);

    i_val = 0xffffffff; /*used to be 0x1ff*/
    i_val &= (0xffffffff & ~(0x1 << 10));
    i_val |= (0x1 << 10);
    _evo_outw(evobase->evo_misc, i_val);
    us_delay(100);
    return(0);
}


/*
	Tries to find out whether CSC/TMI optional board
        is present. Returns 0 if present, -1 otherwise.
	It also stores rev numbers into evobase.
*/

__uint32_t
_evo_initCSC(void)
{
    /*
     * Reset CSC chip for "N" ticks.
     */
     
    CSC_W1(evobase, CSC_MODE, 0x1);
    delay (300);
    CSC_W1(evobase, CSC_MODE, 0x2); /*0x2 implies bypass mode*/

    return(0);
}


__uint32_t
evo_csc_probe(void)
{

    unsigned char  rev ;

    /*
     * XXX: The CSC and TMI are ASICS for Speed Racer
     * We have to revisit the probing....
     */
    CSC_R1(evobase, CSC_ID, rev); 
    evobase->info.bdrev = rev & 0xf ; /* low nibble*/
    evobase->info.CSCrev = (rev >> 4) & 0xf ; /* high nibble */

    /* XXX Needs Revisiting. Initialize these
       internal values Check values XXX
    */

    csc_inlut        = 4 ;
    csc_alphalut     = 4 ; /* XXX */
    csc_coef         = 0x100 ;
    csc_x2k          = 0x800 ;
    csc_outlut       = 4 ;
    csc_inlut_value  = 0 ;
    csc_tolerance    = 0;
    csc_clipping     = 0;
    csc_constant_hue = 1;
    csc_each_read    = 0;

    msg_printf(DBG, "CSC/TMI probe successful\n");
    return (0);
}

__uint32_t
evo_csc_reset(void)
{

    CSC_W1(evobase, CSC_MODE, 0x2) ;/*CSC reset XXX*/ 

    return (0);
}


__uint32_t
_evo_initI2C(void)
{

	__uint32_t  i_val;
	unsigned short temp_word, reset_word;
#if 0
/*reset i2c*/
reset_word = _evo_inhw(evobase->evo_misc+(__paddr_t)MISC_STATUS_CONF); /*read register first */
msg_printf(SUM, "status register is 0x%x\n",reset_word);
temp_word = reset_word & 0xfdff; /*I2C reset (active low) is bit 9 -> 0xf[1101]ff = 0xfdff*/
msg_printf(SUM, "word being written to status register is 0x%x\n",temp_word);
_evo_outhw(evobase->evo_misc+(__paddr_t)MISC_STATUS_CONF, temp_word); /*reset I2C */
us_delay(100); /*stays low for delay(100) in kernel code*/

/*take I2C out of reset*/
temp_word = reset_word | 0x0200; /* bit 9 goes high -> 0x0[0010]00 -> 0x200*/
msg_printf(SUM, "word being written after reset to status register is 0x%x\n",temp_word);
_evo_outhw(evobase->evo_misc+(__paddr_t)MISC_STATUS_CONF, reset_word); /*operate I2C */
us_delay(100); 
#endif

    i_val = 0xffffffff; /*used to be 0x1ff*/
    i_val &= (0xffffffff & ~(0x1 << 9));
    /*i_val |= (0x0 << 9);*/
    _evo_outw(evobase->evo_misc, i_val);
    us_delay(100);

    i_val = 0xffffffff; /*used to be 0x1ff*/
    i_val &= (0xffffffff & ~(0x1 << 9));
    i_val |= (0x1 << 9);
    _evo_outw(evobase->evo_misc, i_val);
    us_delay(100);


/*old if 0 starts here*/
#if 0
    _evo_outw(evobase->evo_misc, 0xfdff);
    us_delay(100);
    _evo_outw(evobase->evo_misc, 0xffff);
    us_delay(100);
#endif
    _evo_outb((evobase->i2c+(__paddr_t)EVO_I2C_CONTROL), I2C_PIN_CTL);
    us_delay(100);
    _evo_outb((evobase->i2c+(__paddr_t)EVO_I2C_DATA), (I2C_ADDR_8584 >> 1));
    us_delay(100);
    _evo_outb((evobase->i2c+(__paddr_t)EVO_I2C_CONTROL), I2C_ESI_CLOCK_REG);
    us_delay(100);
    _evo_outb((evobase->i2c+(__paddr_t)EVO_I2C_DATA), I2C_CLOCK_SCLK_12MHZ | I2C_CLOCK_SCL_1_5KHZ);
    us_delay(100);
    _evo_outb((evobase->i2c+(__paddr_t)EVO_I2C_CONTROL), I2C_PIN_CTL | I2C_ESO_CTL | I2C_ACK_CTL);
    us_delay(100);
    return (0);
}


__uint32_t
_evo_initDEC(void)
{
	evo_ind_regs evo_dec_regs[] = {
                1,              0,
                VIP_REG_ACT1,   (VIP_FUSE_AMP_AA << VIP_FUSE_SHFT),
                VIP_REG_ACT2,   (VIP_GAI18_MSK << VIP_GAI18_SHFT) | 
                                (VIP_GAI28_MSK << VIP_GAI28_SHFT) | 
                                (VIP_VBSL_MSK << VIP_VBSL_SHFT),
                VIP_REG_GAI1,   0,
                VIP_REG_GAI2,   0,
                VIP_REG_HSB,    0xeb,
                VIP_REG_HSS,    0xe0,
                VIP_REG_SYNC,   (VIP_AUFD_MSK << VIP_AUFD_SHFT) | 
                                (VIP_VTRC_MSK << VIP_VTRC_SHFT),
                VIP_REG_LUMA,   (0x02 << VIP_APER_SHFT),
                VIP_REG_BRIG,   127,
                VIP_REG_CONT,   75,
                VIP_REG_SATN,   0x44,
                VIP_REG_HUEC,   254,
                VIP_REG_CHRC,   (VIP_CHBW_NOMINAL << VIP_CHBW_SHFT),
                0xf,            0,
                VIP_REG_FMTC,   (VIP_OFTS_YUVCCIR656 << VIP_OFTS_SHFT) |
                                (7 << VIP_YDEL_SHFT),
                VIP_REG_OUC1,   (VIP_OEHV_MSK << VIP_OEHV_SHFT) | 
                                (VIP_OEYC_MSK << VIP_OEYC_SHFT),
                VIP_REG_OUC2,   (VIP_AOSL_INPUT_AD2 << VIP_AOSL_SHFT),
                0x13,           0,
                0x14,           0,
                0x15,           0,
                0x16,           0,
                0x17,           0,
                0x18,           0,
                0x19,           0,
                0x1D,           0,
                0x1E,           0,
                -1,             -1,
	};

	evo_ind_regs        *pRegs = evo_dec_regs;
	uchar_t 	    bval[10];
	__uint32_t	    ack,errors=0;

    ack=0;
    while (pRegs->reg != -1) {
		bval[0]= pRegs->reg;
		bval[1]= pRegs->val;
		ack = _evo_i2c_dev_wr(VIP_WRITE_SLAVEADDR &~ 0x01, bval, 2);
		if(ack != 0){
			errors++;
			msg_printf(ERR, "Error writing to DEC register 0x%x\n",pRegs->reg);
		}
		pRegs++;
    }
    return(errors);	

}

__uint32_t
_evo_initENC(void)
{
        evo_ind_regs nsqrflt_default[] = {
/*                MISC_SQR_VGI1TRIG, 0x02,   * NTSC* */
		  30, 0x02,	/*equivalent values to constants above*/
                -1, -1
                };

        evo_ind_regs 	evo_denc_regs[] = {
                DENC_REG_UNUSED0,       0,
                DENC_REG_UNUSED1,       0,
                DENC_REG_UNUSED2,       0,
                DENC_REG_UNUSED3,       0,
                DENC_REG_UNUSED4,       0,
                DENC_REG_UNUSED5,       0,
                DENC_REG_UNUSED6,       0,
                DENC_REG_UNUSED7,       0,
                DENC_REG_UNUSED8,       0,
                DENC_REG_UNUSED9,       0,
                DENC_REG_UNUSED10,      0,
                DENC_REG_UNUSED11,      0,
                DENC_REG_UNUSED12,      0,
                DENC_REG_UNUSED13,      0,
                DENC_REG_UNUSED14,      0,
                DENC_REG_UNUSED15,      0,
                DENC_REG_UNUSED16,      0,
                DENC_REG_UNUSED17,      0,
                DENC_REG_UNUSED18,      0,
                DENC_REG_UNUSED19,      0,
                DENC_REG_UNUSED20,      0,
                DENC_REG_UNUSED21,      0,
                DENC_REG_UNUSED22,      0,
                DENC_REG_UNUSED23,      0,
                DENC_REG_UNUSED24,      0,
                DENC_REG_UNUSED25,      0,
                DENC_REG_UNUSED26,      0,
                DENC_REG_UNUSED27,      0,
                DENC_REG_UNUSED28,      0,
                DENC_REG_UNUSED29,      0,
                DENC_REG_UNUSED30,      0,
                DENC_REG_UNUSED31,      0,
                DENC_REG_UNUSED32,      0,
                DENC_REG_UNUSED33,      0,
                DENC_REG_UNUSED34,      0,
                DENC_REG_UNUSED35,      0,
                DENC_REG_UNUSED36,      0,
                DENC_REG_UNUSED37,      0,
                DENC_REG_UNUSED38,      0,
                DENC_REG_UNUSED39,      0,
                DENC_REG_UNUSED40,      0,
                DENC_REG_UNUSED41,      0,
                DENC_REG_UNUSED42,      0,
                DENC_REG_UNUSED43,      0,
                DENC_REG_UNUSED44,      0,
                DENC_REG_UNUSED45,      0,
                DENC_REG_UNUSED46,      0,
                DENC_REG_UNUSED47,      0,
                DENC_REG_UNUSED48,      0,
                DENC_REG_UNUSED49,      0,
                DENC_REG_UNUSED50,      0,
                DENC_REG_UNUSED51,      0,
                DENC_REG_UNUSED52,      0,
                DENC_REG_UNUSED53,      0,
                DENC_REG_UNUSED54,      0,
                DENC_REG_UNUSED55,      0,
                DENC_REG_UNUSED56,      0,
                DENC_REG_UNUSED57,      0,
                DENC_REG_INCTL,         (DENC_V656_MSK << DENC_V656_SHFT) |
                                        (DENC_VY2C_MSK << DENC_VY2C_SHFT) |
                                        (DENC_VUV2C_MSK << DENC_VUV2C_SHFT) |
                                        (DENC_MY2C_MSK << DENC_MY2C_SHFT) |
                                        (DENC_MUV2C_MSK << DENC_MUV2C_SHFT),
                DENC_REG_LUTY0,         0x6b,
                DENC_REG_LUTU0,         0x00,
                DENC_REG_LUTV0,         0x00,
                DENC_REG_LUTY1,         0x52,
                DENC_REG_LUTU1,         0x90,
                DENC_REG_LUTV1,         0x12,
                DENC_REG_LUTY2,         0x2A,
                DENC_REG_LUTU2,         0x26,
                DENC_REG_LUTV2,         0x90,
                DENC_REG_LUTY3,         0x11,
                DENC_REG_LUTU3,         0xB6,
                DENC_REG_LUTV3,         0xA2,
                DENC_REG_LUTY4,         0xEA,
                DENC_REG_LUTU4,         0x4A,
                DENC_REG_LUTV4,         0x5E,
                DENC_REG_LUTY5,         0xD1,
                DENC_REG_LUTU5,         0xDA,
                DENC_REG_LUTV5,         0x70,
                DENC_REG_LUTY6,         0xA9,
                DENC_REG_LUTU6,         0x70,
                DENC_REG_LUTV6,         0xEE,
                DENC_REG_LUTY7,         0x90,
                DENC_REG_LUTU7,         0x00,
                DENC_REG_LUTV7,         0x00,
                DENC_REG_CHPS,          0x00,
                DENC_REG_GAINU,         0x76,
                DENC_REG_GAINV,         0xA5,
                DENC_REG_BLCKL,         (0x63 << DENC_BLCK_SHFT),
                DENC_REG_BLNKL,         (0x63 << DENC_BLNL_SHFT),
                DENC_REG_UNUSED95,      0,
                DENC_REG_CCRS,          (DENC_CCRS_NONE << DENC_CCRS_SHFT),
                DENC_REG_STDRD,         (DENC_YGS_MSK << DENC_YGS_SHFT) |
                                        (DENC_SCBW_MSK << DENC_SCBW_SHFT) |
                                        (DENC_FISE_MSK << DENC_FISE_SHFT),
                DENC_REG_BSTA,          0x66,
                DENC_REG_SUBC0,         0x1F,
                DENC_REG_SUBC1,         0x7C,
                DENC_REG_SUBC2,         0xF0,
                DENC_REG_SUBC3,         0x21,
                DENC_REG_L21O0,         0x00,
                DENC_REG_L21O1,         0x00,
                DENC_REG_L21E0,         0x00,
                DENC_REG_L21E1,         0x00,
                DENC_REG_ENCTL,         (DENC_MODIN_VPPORT << DENC_MODIN_SHFT),
                DENC_REG_RCVCTL,        (DENC_SRCV_SEQSYNC << DENC_SRCV_SHFT) |
                                        (DENC_TRCV2_MSK << DENC_TRCV2_SHFT) |
                                        (DENC_CBLF_MSK << DENC_CBLF_SHFT),
                DENC_REG_RCMCC,         (DENC_SRCM_SEQSYNC << DENC_SRCM_SHFT) |
                                        (DENC_CCENT_OFF << DENC_CCENT_SHFT),
                DENC_REG_HTRIGL,        0x32,
                DENC_REG_HTRIGH,        0x00,
                DENC_REG_VTRIG,         (DENC_PHRES_NONE << DENC_PHRES_SHFT) |
                                        (DENC_SBLBN_MSK << DENC_SBLBN_SHFT),
                DENC_REG_BMRQ,          0xF9,
                DENC_REG_EMRQ,          0x86,
                DENC_REG_MPHI,          (0x06 << DENC_EMPH_SHFT),
                DENC_REG_UNUSED116,     0,
                DENC_REG_UNUSED117,     0,
                DENC_REG_UNUSED118,     0,
                DENC_REG_BRCV,          0xF9,
                DENC_REG_ERCV,          0x86,
                DENC_REG_RCV2HI,        (0x06 << DENC_ERCVH_SHFT),
                DENC_REG_FLEN,          0x0C,
                DENC_REG_FAL,           0x10,
                DENC_REG_LAL,           0x03,
                DENC_REG_ALNHI,         (0x01 << DENC_LALH_SHFT) |
                                                (0x02 << DENC_FLENH_SHFT),
                -1,     -1
        };

	evo_ind_regs        *pRegs = evo_denc_regs;
	uchar_t 	    bval[10];
	__uint32_t	    ack,errors=0;

#if 0
        if (evo_bd->video_mode == VSTD_PAL) {
                _evo_change_default(philips_7185_default,DENC_REG_STDRD,
                                        (DENC_YGS_MSK << DENC_YGS_SHFT) |
                                        (DENC_SCBW_MSK << DENC_SCBW_SHFT) |
                                        (DENC_PAL_MSK << DENC_PAL_SHFT) |
                                        (DENC_FISE_MSK << DENC_FISE_SHFT));
                _evo_change_default(philips_7185_default,DENC_REG_FLEN,
                                        0x70);
                _evo_change_default(philips_7185_default,DENC_REG_FAL,
                                        0x15);
                _evo_change_default(philips_7185_default,DENC_REG_LAL,
                                        0x30);
                _evo_change_default(philips_7185_default,DENC_REG_ALNHI,
                                        (0x01 << DENC_LALH_SHFT) |
                                        (0x02 << DENC_FLENH_SHFT));
                }


        if (evo_bd->video_mode == VSTD_PAL) {
                _evo_change_default(nsqrflt_default,MISC_SQR_VGI1TRIG,0x0);
                }

        evo_miscdir_tbl(evo_bd, nsqrflt_default, VID_WRITE);
#endif

    ack=0;
    while (pRegs->reg != -1) {
		bval[0]= pRegs->reg;
		bval[1]= pRegs->val;
		ack = _evo_i2c_dev_wr(DENC_WRITE_SLAVEADDR &~ 0x01, bval, 2);
		if(ack != 0){
			errors++;
			msg_printf(ERR, "Error writing to DENC register 0x%x\n",ack);
		}
		pRegs++;
    }
    return(errors);	
}



/********************************************XXX May not be required
__uint32_t
_evo_inputStandardDetection(void)
{
    unsigned char vin_status_ch1, vin_status_ch2, video_std;
    __uint32_t	errors = 0;

    * assume illegal values 
    evobase->videoMode = evobase->pixelMode = -1;

    *  May not be Required: check if input present 
    EVO_PIO_R1(evobase, MISC_VIN_CH1_STATUS, vin_status_ch1);
    EVO_PIO_R1(evobase, MISC_VIN_CH2_STATUS, vin_status_ch2);

    msg_printf(DBG, "vin_status_ch1 %d ; vin_status_ch2 %d\n",
	vin_status_ch1, vin_status_ch2);

    if ((vin_status_ch1 & 0x10) == 0) {
	video_std = vin_status_ch1 & 0x3;
    } else if ((vin_status_ch2 & 0x10) == 0) {
	video_std = vin_status_ch2 & 0x3;
    } else {
	msg_printf(SUM, "IMPV:- WARNING: Video input is not present\n");
	video_std = 0;
    }

    * set up TV standard 
    EVO_PIO_W1(evobase, MISC_TV_STD, video_std);

    * save the video and pixel modes *
    evobase->videoMode = (video_std & 0x2) >> 1;
    evobase->pixelMode = video_std & 0x1;

    msg_printf(DBG, "TV Standard 0x%x\n", video_std);

    return(errors);
}
*********************************************************************/

__uint32_t
_evo_initXPoint(xpoint_id)
int xpoint_id; 
{
    __uint32_t	errors = 0;
    /*********************************************************/
    /*XXX These default definitions will need to be revisited*/
    /*********************************************************/
    evo_ind_regs evo_xpoint_regs2[] = {
  EVOB_CSC_2_A,               MUXB_BLACK_REF_IN,
  EVOB_CSC_2_B,               MUXB_BLACK_REF_IN,
  EVOB_SCALER2_OUT,           MUXB_BLACK_REF_IN,
  EVOB_VGI1B_CH1_OUT,         MUXB_BLACK_REF_IN,
  EVOB_VGI1B_CH2_OUT,         MUXB_BLACK_REF_IN,
  EVOB_TO_VBARA_CH1,          MUXB_BLACK_REF_IN,
  EVOB_TO_VBARA_CH2,          MUXB_BLACK_REF_IN,
	-1,     -1
    };

    evo_ind_regs evo_xpoint_regs1[] = {
  EVOA_CSC_1_A,             MUX_VGI1_CH1_IN,
  EVOA_CSC_1_B,             MUX_VGI1_CH2_IN,
  EVOA_SCALER1_OUT,         MUX_VGI1_CH1_IN,
  EVOA_VGI1_CH1_OUT,        MUX_FROM_VBARB_CH1,
  EVOA_VGI1_CH2_OUT,        MUX_FROM_VBARB_CH2,
  EVOA_TO_VBARB_CH1,        MUX_CAM_IN,
  EVOA_TO_VBARB_CH2,        MUX_CAM_IN,
  EVOA_VIDEO_CH1_OUT,       MUX_FROM_VBARB_CH1,
  EVOA_VIDEO_CH2_OUT,       MUX_FROM_VBARB_CH2,
	-1,     -1
    };

    evo_ind_regs	*pRegs = evo_xpoint_regs1;
    evo_ind_regs	*pRegs2 = evo_xpoint_regs2;

    while (pRegs->reg != -1) {
	/*XXX the driver code writes VID_WRITE (= 1) here*/
	if (xpoint_id == 1){
		_evo_outb((evobase->vbar1+((__paddr_t)pRegs->reg)), pRegs->val);
	} else if (xpoint_id == 2){ 
		_evo_outb((evobase->vbar2+((__paddr_t)pRegs2->reg)), pRegs2->val);
	}
	pRegs++;
    }

    return(errors);
}

__uint32_t
_evo_initCfpga(void)
{
    __uint32_t	errors = 0;
    __uint64_t	 _val;
	
	_val = *(__uint64_t *)((__paddr_t)evobase->gio_pio_base);
	if ((_val & 0xffffffffffffffff) == 0x42) {
		msg_printf(SUM, "EVO Readback Register Test Passed: _val = 0x%x\n",_val); 
	} else {
		msg_printf(SUM, "EVO Readback Register Test Failed: _val = 0x%x\n",_val); 
		errors++;
	}
	return(errors);
}


/*XXX***********************************************************
 * The following three functions may not be needed
 ***************************************************************
__uint32_t
_evo_initVideoInput(void)
{
    __uint32_t	errors = 0;
    evo_ind_regs evo_vin_regs[] = {
	MISC_VIN_CTRL, 0x3,
	-1,     -1
    };
    evo_ind_regs	*pRegs = evo_vin_regs;

    while (pRegs->reg != -1) {
        EVO_W1 (evobase, pRegs->reg, pRegs->val);
	pRegs++;
    }

    return(errors);
}


__uint32_t
_evo_initVideoOutput(void)
{
    __uint32_t	errors = 0;
    evo_ind_regs evo_vout_regs[] = {
	MISC_CH1_HPHASE_LO, LOBYTE(HPHASE_CENTER),
	MISC_CH1_HPHASE_HI, HIBYTE(HPHASE_CENTER),
	MISC_CH2_HPHASE_LO, LOBYTE(HPHASE_CENTER),
	MISC_CH2_HPHASE_HI, HIBYTE(HPHASE_CENTER),
	MISC_CH1_CTRL,      0x0,
	MISC_CH2_CTRL,      0x0,
	-1,     -1
    };
    evo_ind_regs	*pRegs = evo_vout_regs;

    while (pRegs->reg != -1) {
        EVO_W1 (evobase, pRegs->reg, pRegs->val);
	pRegs++;
    }

    return(errors);
}


__uint32_t
_evo_initGenlock(void)
{
    __uint32_t	errors = 0;
    evo_ind_regs evo_genlock_regs[] = {
	MISC_VCXO_LO,  LOBYTE(DAC_CENTER),
	MISC_VCXO_HI,  HIBYTE(DAC_CENTER),
	MISC_REF_CTRL, 0x0,
	MISC_REF_OFFSET_LO, LOBYTE(BOFFSET_CENTER),
	MISC_REF_OFFSET_HI, HIBYTE(BOFFSET_CENTER),
	-1,     -1
    };
    evo_ind_regs	*pRegs = evo_genlock_regs;

    msg_printf(DBG, "Inside _evo_initGenlock...\n");

    while (pRegs->reg != -1) {
        EVO_W1 (evobase, pRegs->reg, pRegs->val);
	pRegs++;
    }

    msg_printf(DBG, "Outside _evo_initGenlock...\n");

    return(errors);
}
********************************************************************/

__uint32_t
_evo_initVGI1(void)
{
	/*XXX Check whether this needs changin to evo_*/
    evo_vgi1_sys_mode_t	sys_mode = {0LL};
    __uint32_t		errors = 0;
    ulonglong_t		tmp, mask = ~(ulonglong_t)0x0;
    ulonglong_t		gio_id = ~(ulonglong_t)0x0;

    /*
     * Read and Verify GIO ID register
     */
    VGI1_R64(evobase, VGI1_GIO_ID, gio_id, mask);
    delay (300);
    msg_printf(SUM, " gio_id 0x%x \n", gio_id);
    if ( (gio_id & GIO_ID_VGI1_MASK) != GIO_ID_VGI1) {
	msg_printf(SUM, "IMPV:- WARNING - No VGI1 present\n");
	errors++;
	return(errors);
    }

    /*
     * Place the VGI1 system mode register in its initial state.
     */
    sys_mode.page_size = EVO_VGI1_PGSZ_4K;
    sys_mode.arb_method = EVO_VGI1_ARB_VGI1;
    sys_mode.burst_hld_off = 4; /*XXX mgv has 4 as hold off value*/
    sys_mode.video_reset = 1;
    sys_mode.unused0 = 0x0;
    tmp = 0x0;
    bcopy((char *)&sys_mode, (char *)&tmp, sizeof(evo_vgi1_sys_mode_t));
    VGI1_W64(evobase, VGI1_SYS_MODE, tmp, mask);
    delay (300);
    sys_mode.video_reset = 0;
    tmp = 0x0;
    bcopy((char *)&sys_mode, (char *)&tmp, sizeof(evo_vgi1_sys_mode_t));
    VGI1_W64(evobase, VGI1_SYS_MODE, tmp, mask);
    delay (300);

    /* disable all interrupts for both channels */
    VGI1_W64(evobase, VGI1_CH1_INT_ENAB, 0x0, mask);
    VGI1_W64(evobase, VGI1_CH2_INT_ENAB, 0x0, mask);

    /* abort any pending DMA and turn the trigger off for both channels */
    bcopy((char *)&evo_abort_dma, (char *)&tmp,sizeof(evo_vgi1_ch_trig_t));
    VGI1_W64(evobase, VGI1_CH1_TRIG, tmp, mask);
    VGI1_W64(evobase, VGI1_CH2_TRIG, tmp, mask);
    bcopy((char *)&evo_trig_off, (char *)&tmp, sizeof(evo_vgi1_ch_trig_t));
    VGI1_W64(evobase, VGI1_CH1_TRIG, tmp, mask);
    VGI1_W64(evobase, VGI1_CH2_TRIG, tmp, mask);

    /* clear all flags in the status register for both channels */
    VGI1_W64(evobase, VGI1_CH1_STATUS, EVO_VGI1_CH_ALLINTRS, mask);
    VGI1_W64(evobase, VGI1_CH2_STATUS, EVO_VGI1_CH_ALLINTRS, mask);

    /*XXX VGI1PollReg requires updation in evo_vgi1reg.c */ 
    if (_evo_VGI1PollReg(VGI1_CH1_STATUS,0x0,EVO_VGI1_CH_ALLINTRS,-1,&tmp)) {
	msg_printf(ERR, "Channel-1 Status register is not ZERO... Time Out\n");
	msg_printf(DBG, "Channel-1 Status exp 0x%llx; rcv 0x%llx\n",
		0x0, (tmp & EVO_VGI1_CH_ALLINTRS));
	errors++;
    } 

    /*XXX VGI1PollReg requires updation in evo_vgi1reg.c */ 
    if (_evo_VGI1PollReg(VGI1_CH2_STATUS,0x0,EVO_VGI1_CH_ALLINTRS,-1,&tmp)) {
	msg_printf(ERR, "Channel-2 Status register is not ZERO... Time Out\n");
	msg_printf(DBG, "Channel-2 Status exp 0x%llx; rcv 0x%llx\n",
		0x0, (tmp & EVO_VGI1_CH_ALLINTRS));
	errors++;
    }
    return(errors);
}

/*************VOC begins here**************************/
/*XXX**********Needs to be Attended to**************************/

/* evoInitVoc - Initialize the VOC. Came from dgMakeVocFetal(). */
__uint32_t
_evo_initVOC(void)
{
    int regval,nHighWater;
    __uint32_t		errors = 0;
    
#if 0
    _evo_outhw(evobase->evo_misc+(__paddr_t)MISC_FMBUF_DATA, 0x0000);
    _evo_outhw(evobase->evo_misc+(__paddr_t)MISC_FMBUF_CONTROL, 0x47);
#endif

    EVO_PIO_W1(evobase, PIO_VOC_PIXELS, 852);
    if(evobase->tvStd == VGI1_NTSC_CCIR601){          /*XXX tvStd and other features need changing*/
		EVO_PIO_W1(evobase, PIO_VOC_LINES, 243);
    } else if(evobase->tvStd == VGI1_PAL_CCIR601){
		EVO_PIO_W1(evobase, PIO_VOC_LINES, 288);
    }
    /* init control */
    _evo_outw((evobase->voc+(__paddr_t)(VOC_CONTROL << 4)),0);

    /* init channel id */
    _evo_outw((evobase->voc+(__paddr_t)(VOC_IWSC_CONTROL << 4)),0);
    
    /* select 8 field double buffer */
    _evo_outw((evobase->voc+(__paddr_t)(VOC_VOF_CONTROL << 4)),0x10);
    
    /* init control registers */
    _evo_outw((evobase->voc+(__paddr_t)(VOC_VOF_CONTROL << 4)),0);
    msg_printf(DBG, "after VOF_CONTROL initialization\n");
    _evo_outw((evobase->voc+(__paddr_t)(VOC_VOF_SIGNAL_INIT << 4)),0);
    msg_printf(DBG, "after VOF_VOF_SIGNAL_INIT initialization\n");
    _evo_outw((evobase->voc+(__paddr_t)(VOC_PFD_CONTROL << 4)),0);
    msg_printf(DBG, "after VOF_PFD_CONTROL initialization\n");
    _evo_outw((evobase->voc+(__paddr_t)(VOC_PSD_CONTROL << 4)),0);
    msg_printf(DBG, "after VOF_PSD_CONTROL initialization\n");
    _evo_outw((evobase->voc+(__paddr_t)(VOC_VDRC_CONTROL << 4)),0);
    msg_printf(DBG, "after VOF_VDRC_CONTROL initialization\n");
    _evo_outw((evobase->voc+(__paddr_t)(VOC_PLL_CONTROL << 4)),0);
    msg_printf(DBG, "after VOF_PLL_CONTROL initialization\n");
    _evo_outw((evobase->voc+(__paddr_t)(VOC_GEN_CONTROL << 4)),0);
    msg_printf(DBG, "after VOF_GEN_CONTROL initialization\n");
    _evo_outw((evobase->voc+(__paddr_t)(VOC_SAC_CONTROL << 4)),0);
    msg_printf(DBG, "after VOF_SAC_CONTROL initialization\n");
    _evo_outw((evobase->voc+(__paddr_t)(VOC_SIZ_CONTROL << 4)),0);
    msg_printf(DBG, "after VOF_SIZ_CONTROL initialization\n");
    
    /* blank the output early to cut down sim trace size */
    _evo_outw((evobase->voc+(__paddr_t)(VOC_VOF_PIXEL_BLANK << 4)),0x10000);
    msg_printf(DBG, "after VOC_VOF_PIXEL_BLANK\n");
    
    /* init field request table */
    _evo_outw((evobase->voc+(__paddr_t)(VOC_VDRC_REQ0 << 4)),0);
    msg_printf(DBG, "after VOF_VDRC_REQ initialization\n");
    _evo_outw((evobase->voc+(__paddr_t)(VOC_VDRC_REQ_PATTERN0 << 4)),0);
    msg_printf(DBG, "after VOF_VDRC_REQ_PATTERN initialization\n");
    
    /* worst-case value for 321 groupxel lines */
    nHighWater = (2 * VOF_PIXFIFO_SIZE) -
                ((VOF_PIXFIFO_SIZE % (VOF_PIXFIFO_SIZE / 2) + 1));

    /* reset voc fifo pointers - highwater and reset fifo ptrs every field */
    _evo_outw((evobase->voc+(__paddr_t)(VOC_FIFO_CONTROL << 4)),(nHighWater & 0xfff) | 0x2000);
    msg_printf(DBG, "after resetting VOC FIFO pointers\n");
    /* should toggle frame/field start to really "reset"... no need yet */
    
    /* clear any voc interrupts */
    _evo_outw((evobase->voc+(__paddr_t)(VOC_STATUS << 4)),0);
    msg_printf(DBG, "after resetting interrupts\n");

    return(errors); /*XXX Errors need to be set*/
}


__uint32_t
_evo_initSCL(scl_id)
int scl_id;
{
    __uint32_t		errors = 0;
    uchar_t low, high; 
    uchar_t value; 
    u_int16_t reg_value;

msg_printf(DBG, "entering evo_initSCL for Scaler %d\n", scl_id);
if( scl_id == 1){
	EVO_PIO_W1(evobase, PIO_SCL1_PIXELS, 772);
	/*XXX tvStd code will need changing NTSC => 244, PAL=>288*/
	EVO_PIO_W1(evobase, PIO_SCL1_LINES, 244);

} else if (scl_id == 2) {
	EVO_PIO_W1(evobase, PIO_SCL2_PIXELS, 772);
	/*XXX tvStd code will need changing NTSC => 243, PAL=>288*/
	EVO_PIO_W1(evobase, PIO_SCL2_LINES, 244);
}

/*XXX set scaler to bypass mode */
/*Scaler is in bypass when corresponding source and target registers hold identical values*/
reg_value = 244; /*Source lines for NTSC_CCIR_601 = 00f3*/
value = LOBYTE(reg_value);
EVO_SCL_W1(evobase, Y_SCALE_VSRC_CNT_LSB, value);   /*Y channel Source LSB   */
EVO_SCL_W1(evobase, UV_SCALE_VSRC_CNT_LSB, value);  /*UV Channel Source LSB  */
EVO_SCL_W1(evobase, Y_SCALE_VTRG_CNT_LSB, value);   /*Y Channel Target  LSB  */
EVO_SCL_W1(evobase, UV_SCALE_VTRG_CNT_LSB, value);  /*UV Channel Target  LSB */

value = HIBYTE(reg_value);
EVO_SCL_W1(evobase, Y_SCALE_VSRC_CNT_MSB, value);  /*Y  channel Source MSB  */
EVO_SCL_W1(evobase, UV_SCALE_VSRC_CNT_MSB, value); /*UV channel Source MSB  */
EVO_SCL_W1(evobase, Y_SCALE_VTRG_CNT_MSB, value);  /*Y channel Target MSB   */
EVO_SCL_W1(evobase, Y_SCALE_VTRG_CNT_MSB, value);  /*UV channel Source MSB  */

reg_value = 640; /*Source pixels/line for NTSC_CCIR_601  */
value = LOBYTE(reg_value);
EVO_SCL_W1(evobase, Y_SCALE_HSRC_CNT_LSB, value);   /*Y Channel Source LSB   */
EVO_SCL_W1(evobase, UV_SCALE_HSRC_CNT_LSB, value);  /*UV Channel Source LSB  */
EVO_SCL_W1(evobase, Y_SCALE_HTRG_CNT_LSB, value);   /*Y Channel Target  LSB  */
EVO_SCL_W1(evobase, UV_SCALE_HTRG_CNT_LSB, value);  /*UV Channel Target LSB  */

value = HIBYTE(reg_value);
EVO_SCL_W1(evobase, Y_SCALE_HSRC_CNT_MSB, value);   /*Y Channel Source MSB  */
EVO_SCL_W1(evobase, UV_SCALE_HSRC_CNT_MSB, value);  /*UV Channel Source MSB */
EVO_SCL_W1(evobase, Y_SCALE_HTRG_CNT_MSB, value);   /*Y Channel Target MSB  */
EVO_SCL_W1(evobase, UV_SCALE_HTRG_CNT_MSB, value);  /*UV Channel Target MSB */

value = 0; /*XXX Number of Initial Blank Lines may need changing*/
EVO_SCL_W1(evobase, SCALE_VIN_DELAY_ODD, value);   /*Initial Blank Lines in Odd Field */
EVO_SCL_W1(evobase, SCALE_VIN_DELAY_EVEN, value);  /*Initial Blank Lines in Even Field */
return(errors);
}


__uint32_t
_evo_VOCInt(void)
{
    __uint32_t		errors = 0;

msg_printf(DBG, "entering evo_VOCInt\n");
return(errors);
}

