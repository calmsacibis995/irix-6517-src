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
__uint32_t	_evo_initSCL(int scl_id);        /*XXX this portion to be defined*/
__uint32_t	_evo_VOCInt(void);        /*XXX this portion to be defined*/
/*__uint32_t	_evo_initGenlock(void);        XXX this portion to be defined*/

__uint32_t
evo_probe(void)
{
    __uint32_t port;

    msg_printf(DBG, "Entering evo_probe...\n");
    mgras_probe_all_widgets();

    port = _evo_probe_bridge();
    msg_printf(DBG, "port 0x%x\n", port);
    if (!port) {
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

    /****XXX********* Offsets to encoder/decoder not given */
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

    msg_printf(DBG, "Leaving evo_probe...\n");

    return(port);
}

__uint32_t
evo_init(__uint32_t argc, char **argv)
{
    __uint32_t		i, device_num = 0, gio64_arb, errors = 0;
    evo_vgi1_info_t     *pvgi1 = &(evobase->vgi1[0]);
    evo_vgi1_info_t     *pvgi2 = &(evobase->vgi1[1]);

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
		15  - Encoder \n\
		16  - Decoder \n \
		17  - Control FPGA \n \
		18  - Genlock \n \
		19  - All VGIs \n \
		20  - All CSCs \n ");
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

    evobase->burst_len = 0xA0;
    evobase->force_unused = 0x0;
    evobase->force_insuf = 0x0;
    evobase->intEnableVal = 0x3f08;

    /*
     * Initialize ALL or selected device
     */

    msg_printf(DBG,"Calling evo_reset for device # %d\n",device_num);
    if (!device_num) {
	for (i = EVO_BOARD_INIT_NUM; i <= EVO_LAST_DEVICE_NUM; i++) {
		errors += _evo_reset(i);
	}
    } else {
    	errors += _evo_reset(device_num);
    }

    return(_evo_reportPassOrFail((&evo_err[EVO_INIT_TEST]), errors));

}

__uint32_t
_evo_probe_bridge(void)
{
    __uint32_t i, alive, present, mfg, rev, part, port = 0;

#if HQ4

    msg_printf(DBG, "Entering _evo_probe_bridge...\n");

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
    }

    msg_printf(DBG, "Leaving _evo_probe_bridge...\n");
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
    giobr_bridge_config_t   bridge_config = &srv_bridge_config;

/*** Initialize bridge for VGI1 DMA ***/
    msg_printf(DBG,"Entering evo_initBridge\n");
    bridge_xbow_port = 0xb;
    /*
     * RRB's are initialized to all zeros
     */
    BR_REG_WR_32(BRIDGE_EVEN_RESP,0xffffffff,0x0);
    msg_printf(DBG,"Wrote in BRIDGE_EVEN_RESP \n");
    BR_REG_WR_32(BRIDGE_ODD_RESP,0xffffffff,0x0);
    msg_printf(DBG,"Wrote in BRIDGE_ODD_RESP \n");

    for (n = 0; n < 8; n++) {
        /*
         * Set up the Device(x) registers
         */
        if (bridge_config->mask & GIOBR_DEV_ATTRS) {
            write_val = GIOBR_DEV_CONFIG(bridge_config->device_attrs[n], n);
            BR_REG_WR_32(BRIDGE_DEVICE(n),0x00ffffff, write_val);
    msg_printf(DBG,"Write to BRIDGE_DEVICE(%d) %x \n",n, write_val);
            BR_REG_RD_32(BRIDGE_DEVICE(n),0x00ffffff, sav_val);
    msg_printf(DBG,"Read from BRIDGE_DEVICE(%d) %0x \n",n,sav_val);
        }
        /*
         * setup the RRB's, assumed to have been all cleared
         */
        if (bridge_config->mask & GIOBR_RRB_MAP) {
            rv = evo_br_rrb_alloc(n, bridge_config->rrb[n]);
            if (rv) return(1); /*** Warn - check return value ***/
        }
        if (bridge_config->mask & GIOBR_INTR_DEV) {
            intr_device &= ~BRIDGE_INT_DEV_MASK(n);
            intr_device |= BRIDGE_INT_DEV_SET(bridge_config->intr_devices[n], n);
        }
     }


    if (bridge_config->mask & GIOBR_ARB_FLAGS) {
        BR_REG_RD_32(BRIDGE_ARB,BRIDGE_ARB_MASK,sav_val);
        write_val = bridge_config->arb_flags | sav_val;
        BR_REG_WR_32(BRIDGE_ARB,BRIDGE_ARB_MASK,write_val);
    }
    if (bridge_config->mask & GIOBR_INTR_DEV) {
        /* BR_REG_WR_32(BRIDGE_INT_DEVICE,BRIDGE_INT_DEVICE_MASK,intr_device); */
        BR_REG_WR_32(BRIDGE_INT_DEVICE,BRIDGE_INT_DEVICE_MASK,0x184000);
        BR_REG_RD_32(BRIDGE_INT_DEVICE,BRIDGE_INT_DEVICE_MASK,sav_val);
    msg_printf(DBG,"Read from BRIDGE_INT_DEVICE - %0x\n", sav_val);
    }

        /*** Write widget id number to be 0xb ***/
        BR_REG_RD_32(BRIDGE_WID_CONTROL,BRIDGE_WID_CONTROL_MASK,sav_val);
        write_val = (sav_val & ~0xf) | 0xb;
        BR_REG_WR_32(BRIDGE_WID_CONTROL,BRIDGE_WID_CONTROL_MASK,0x7f0043fb);
        BR_REG_RD_32(BRIDGE_WID_CONTROL,BRIDGE_WID_CONTROL_MASK,sav_val);
    msg_printf(DBG,"Read from BRIDGE_WID_CONTROL - %0x\n", sav_val);

        /*** Write dir map and int_enable registers ***/
        BR_REG_WR_32(BRIDGE_DIR_MAP,BRIDGE_DIR_MAP_MASK,0x800000);
        BR_REG_RD_32(BRIDGE_DIR_MAP,BRIDGE_DIR_MAP_MASK,sav_val);
    msg_printf(DBG,"Read from BRIDGE_DIR_MAP - %0x\n", sav_val);

        BR_REG_WR_32(BRIDGE_INT_ENABLE,BRIDGE_INT_ENABLE_MASK,0x0);
        BR_REG_RD_32(BRIDGE_INT_ENABLE,BRIDGE_INT_ENABLE_MASK,sav_val);
    msg_printf(DBG,"Read from BRIDGE_INT_ENABLE - %0x\n", sav_val);
        return(0);
}

/*Begin EVO BOARD reset/probe*/

__uint32_t
_evo_board_probe()
{
    __uint32_t errors = 0;

	_evo_outb((evobase->evo_misc+(__paddr_t)MISC_SW_RESET), 0x0);
    	delay (300);

    	/*Initialize MISC registers VOC,SCL,PLL */
	EVO_PIO_W1(evobase, PIO_VOC_PIXELS, 852);
        /*XXX tvStd code will need changing NTSC => 243, PAL=>288*/
        EVO_PIO_W1(evobase, PIO_VOC_LINES, 243); /*NTSC_CCIR_601*/
        EVO_PIO_W1(evobase, PIO_SCL1_PIXELS, 852);
        /*XXX tvStd code will need changing NTSC => 243, PAL=>288*/
        EVO_PIO_W1(evobase, PIO_SCL1_LINES, 243);
        EVO_PIO_W1(evobase, PIO_SCL2_PIXELS, 852);
        /*XXX tvStd code will need changing NTSC => 243, PAL=>288*/
        EVO_PIO_W1(evobase, PIO_SCL2_LINES, 243);

        /* Bit 5. Sel Ref Lock, Bit 4. lock to ref, Bit 3. PLL lock,
         * Bit 2. Color Frame lock, Bit 1. NTSC 525 Source Lines, Bit 0. NSQ Pixels*/
        _evo_outb((evobase->evo_misc+(__paddr_t)MISC_PLL_CONTROL), 0x26); /*Default values*/

        /* Bit 5. Normal Pixel, Bit 4. NTSC 525 lines, Bit 3. Filter Bypass,
         * Bit 2. VGI1A Trigger Normal, Bit 1. VGI1B Trigger Normal,    **********/
        _evo_outb((evobase->evo_misc+(__paddr_t)MISC_FILT_TRIG_CTL), 0x6); /*Default values*/
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
    	errors += _evo_initXPoint(1); 
	break;

    case EVO_XPOINT_2_INIT_NUM:
    	/* initialize the VBAR */
    	errors += _evo_initXPoint(2); 
	break;

    case EVO_VGI1_1_INIT_NUM:
	EVO_SET_VGI1_1_BASE;
    	errors += _evo_initVGI1();
	break;

    case EVO_VGI1_2_INIT_NUM:
	EVO_SET_VGI1_2_BASE;
    	errors += _evo_initVGI1();
	break;

    case EVO_CSC_1_INIT_NUM:
    	delay (30);
	EVO_SET_CSC_1_BASE;
    	errors += _evo_initCSC();
	break;

    case EVO_CSC_2_INIT_NUM:
    	delay (30);
	EVO_SET_CSC_2_BASE;
    	errors += _evo_initCSC();
	break;

    case EVO_CSC_3_INIT_NUM:
    	delay (30);
	EVO_SET_CSC_3_BASE;
    	errors += _evo_initCSC();
	break;

    case EVO_SCL_1_INIT_NUM:
	EVO_SET_SCL1_BASE;
    	errors += _evo_initSCL(1);
	break;

    case EVO_SCL_2_INIT_NUM:
	EVO_SET_SCL2_BASE;
    	errors += _evo_initSCL(2);
	break;

    case EVO_VOC_INIT_NUM:
    	/* initialize Video Input */
    	errors += _evo_initVOC();
	break;

    case EVO_VOC_INT_INIT_NUM:
    	/* initialize Video Input */
    	errors += _evo_VOCInt();
	break;

    case EVO_ENC_INIT_NUM:
    	/* initialize Video Output */
    	/*errors += _evo_initENC();*/
	break;

    case EVO_DEC_INIT_NUM:
    	/* initialize Video Output */
    	/*errors += _evo_initDEC();*/
	break;

    case EVO_CFPGA_INIT_NUM:
    	/* Readback the control FPGA */
    	errors += _evo_initCfpga();
	break;

    case EVO_GENLOCK_INIT_NUM:
    	/* initialize Genlock */
    	/*errors += _evo_initGenlock();*/
	break;

    case EVO_VGIALL_INIT_NUM:
    	/* initialize all VGIs */
    	/*Bit 2 is VGI_RESET (reset is active low) */
    	_evo_outb((evobase->evo_misc+(__paddr_t)MISC_SW_RESET), VAL_EVO_VGIPLL_RESET);
	/*VGIPLL should  be set to normal mode before setting VGI1 to normal mode*/ 
    	_evo_outb((evobase->evo_misc+(__paddr_t)MISC_SW_RESET), VAL_EVO_VGI_RESET);
    	/*Bit 2 is VGI_RESET (normal operation is active high) */
    	_evo_outb((evobase->evo_misc+(__paddr_t)MISC_SW_RESET), VAL_EVO_BOARD_NORM);
	EVO_SET_VGI1_1_BASE;
    	errors += _evo_initVGI1();
	EVO_SET_VGI1_2_BASE;
    	errors += _evo_initVGI1();
	break;

    case EVO_CSCALL_INIT_NUM:
    	/* initialize all CSCs */
    	/*Bit 3 is CSC_RESET (reset is active low) */
    	_evo_outb((evobase->evo_misc+(__paddr_t)MISC_SW_RESET), 0x7);
    	/*Bit 3 is CSC_RESET (normal operation is active high) */
    	_evo_outb((evobase->evo_misc+(__paddr_t)MISC_SW_RESET), 0xf);
    	delay (30);
	EVO_SET_CSC_1_BASE;
    	errors += _evo_initCSC();
    	delay (30);
	EVO_SET_CSC_2_BASE;
    	errors += _evo_initCSC();
    	delay (30);
	EVO_SET_CSC_3_BASE;
    	errors += _evo_initCSC();
	break;

    }

    return(errors);
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
    CSC_W1(evobase, CSC_MODE, 0x0);

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
    evo_ind_regs evo_xpoint_regs1[] = {
  EVOB_MUXB_UNUSED1,          0x0, 
  EVOB_MUXB_UNUSED2,          0x0,
  EVOB_CSC_2_A,               0x6,
  EVOB_CSC_2_B,               0x7,
  EVOB_SCALER2_IN,            0x2,
  EVOB_MUXB_UNUSED3,          0x0,
  EVOB_VGI1B_CH1_IN,          0x5,
  EVOB_VGI1B_CH2_IN,          0x7,
  EVOB_TO_OTHER_VBAR_CH1,     0x8,
  EVOB_TO_OTHER_VBAR_CH2,     0x9,
  EVOB_MUX_UNUSED4,           0x0,
  EVOB_MUX_UNUSED5,           0x0,
  EVOB_MUX_UNUSED6,           0x0,
  EVOB_MUX_UNUSED7,           0x0,
  EVOB_MUX_UNUSED8,           0x0,
  EVOB_MUX_UNUSED9,           0x0,
	-1,     -1
    };

    evo_ind_regs evo_xpoint_regs2[] = {
  EVOA_MUX_UNUSED1,         0x5,
  EVOA_MUX_UNUSED2,         0x5,
  EVOA_CSC_1_A,             0x6,
  EVOA_CSC_1_B,             0x7,
  EVOA_SCALER1_IN,          0x2,
  EVOA_MUX_UNUSED3,         0x5,
  EVOA_VGI1_CH1_IN,         0x8,
  EVOA_VGI1_CH2_IN,         0x2,
  EVOA_TO_OTHER_VBAR_CH1,   0x8,
  EVOA_TO_OTHER_VBAR_CH2,   0x9,
  EVOA_VIDEO_CH1_OUT,       0xa,
  EVOA_VIDEO_CH2_OUT,       0xb,
  EVOA_MUX_UNUSED4,         0x5,
  EVOA_MUX_UNUSED5,         0x5,
  EVOA_MUX_UNUSED6,         0x5,
  EVOA_MUX_UNUSED7,         0x5,
	-1,     -1
    };

    evo_ind_regs	*pRegs = evo_xpoint_regs1;

    while (pRegs->reg != -1) {
	/*XXX the driver code writes VID_WRITE (= 1) here*/
	if(xpoint_id == 2) {
		pRegs = evo_xpoint_regs2;
	}
	if (xpoint_id == 1){
		_evo_outb((evobase->vbar1+(__paddr_t)pRegs->reg), pRegs->val);
	} else if (xpoint_id == 2){ 
		_evo_outb((evobase->vbar2+(__paddr_t)pRegs->reg), pRegs->val);
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
    sys_mode.burst_hld_off = 4;
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
    
    EVO_PIO_W1(evobase, PIO_VOC_PIXELS, 852);
    if(evobase->tvStd == VGI1_NTSC_CCIR601){          /*XXX tvStd and other features need changing*/
		EVO_PIO_W1(evobase, PIO_VOC_LINES, 243);
    } else if(evobase->tvStd == VGI1_PAL_CCIR601){
		EVO_PIO_W1(evobase, PIO_VOC_LINES, 288);
    }
    /* init control */
    _evo_outw((evobase->voc+(__paddr_t)VOC_CONTROL),0);

    /* init channel id */
    _evo_outw((evobase->voc+(__paddr_t)VOC_IWSC_CONTROL),0);
    
    /* select 8 field double buffer */
    _evo_outw((evobase->voc+(__paddr_t)VOC_VOF_CONTROL),0x10);
    
    /* init control registers */
    _evo_outw((evobase->voc+(__paddr_t)VOC_VOF_CONTROL),0);
    msg_printf(DBG, "after VOF_CONTROL initialization\n");
    _evo_outw((evobase->voc+(__paddr_t)VOC_VOF_SIGNAL_INIT),0);
    msg_printf(DBG, "after VOF_VOF_SIGNAL_INIT initialization\n");
    _evo_outw((evobase->voc+(__paddr_t)VOC_PFD_CONTROL),0);
    msg_printf(DBG, "after VOF_PFD_CONTROL initialization\n");
    _evo_outw((evobase->voc+(__paddr_t)VOC_PSD_CONTROL),0);
    msg_printf(DBG, "after VOF_PSD_CONTROL initialization\n");
    _evo_outw((evobase->voc+(__paddr_t)VOC_VDRC_CONTROL),0);
    msg_printf(DBG, "after VOF_VDRC_CONTROL initialization\n");
    _evo_outw((evobase->voc+(__paddr_t)VOC_PLL_CONTROL),0);
    msg_printf(DBG, "after VOF_PLL_CONTROL initialization\n");
    _evo_outw((evobase->voc+(__paddr_t)VOC_GEN_CONTROL),0);
    msg_printf(DBG, "after VOF_GEN_CONTROL initialization\n");
    _evo_outw((evobase->voc+(__paddr_t)VOC_SAC_CONTROL),0);
    msg_printf(DBG, "after VOF_SAC_CONTROL initialization\n");
    _evo_outw((evobase->voc+(__paddr_t)VOC_SIZ_CONTROL),0);
    msg_printf(DBG, "after VOF_SIZ_CONTROL initialization\n");
    
    /* blank the output early to cut down sim trace size */
    _evo_outw((evobase->voc+(__paddr_t)VOC_VOF_PIXEL_BLANK),0x10000);
    msg_printf(DBG, "after VOC_VOF_PIXEL_BLANK\n");
    
    /* init field request table */
    _evo_outw((evobase->voc+(__paddr_t)VOC_VDRC_REQ0),0);
    msg_printf(DBG, "after VOF_VDRC_REQ initialization\n");
    _evo_outw((evobase->voc+(__paddr_t)VOC_VDRC_REQ_PATTERN0),0);
    msg_printf(DBG, "after VOF_VDRC_REQ_PATTERN initialization\n");
    
    /* worst-case value for 321 groupxel lines */
    nHighWater = (2 * VOF_PIXFIFO_SIZE) -
                ((VOF_PIXFIFO_SIZE % (VOF_PIXFIFO_SIZE / 2) + 1));

    /* reset voc fifo pointers - highwater and reset fifo ptrs every field */
    _evo_outw((evobase->voc+(__paddr_t)VOC_FIFO_CONTROL),(nHighWater & 0xfff) | 0x2000);
    msg_printf(DBG, "after resetting VOC FIFO pointers\n");
    /* should toggle frame/field start to really "reset"... no need yet */
    
    /* clear any voc interrupts */
    _evo_outw((evobase->voc+(__paddr_t)VOC_STATUS),0);
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
	EVO_PIO_W1(evobase, PIO_SCL1_PIXELS, 852);
	/*XXX tvStd code will need changing NTSC => 243, PAL=>288*/
	EVO_PIO_W1(evobase, PIO_SCL1_LINES, 243);
} else if (scl_id == 2) {
	EVO_PIO_W1(evobase, PIO_SCL2_PIXELS, 852);
	/*XXX tvStd code will need changing NTSC => 243, PAL=>288*/
	EVO_PIO_W1(evobase, PIO_SCL2_LINES, 243);
}
/*XXX set scaler to bypass mode */
/*Scaler is in bypass when corresponding source and target registers hold identical values*/
reg_value = 243; /*Source lines for NTSC_CCIR_601*/
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

reg_value = 852; /*Source pixels/line for NTSC_CCIR_601*/
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

