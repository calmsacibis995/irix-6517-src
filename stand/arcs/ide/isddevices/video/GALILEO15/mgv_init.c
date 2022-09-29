/*
**               Copyright (C) 1989, Silicon Graphics, Inc.              
**                                                                      
**  These coded instructions, statements, and computer programs  contain 
**  unpublished  proprietary  information of Silicon Graphics, Inc., and 
**  are protected by Federal copyright law.  They  may  not be disclosed  
**  to  third  parties  or copied or duplicated in any form, in whole or
**  in part, without the prior written consent of Silicon Graphics, Inc. 
*/

#include "mgv_diag.h"
#ifdef IP30
#include "../../../godzilla/include/d_bridge.h"
#include "../../../godzilla/include/d_godzilla.h"
#endif

/*
 * Forward References 
 */
__uint32_t	mgv_probe(void);
__uint32_t	mgv_init(__uint32_t argc, char **argv);
__uint32_t	mgv_csc_tmi_probe(void);
__uint32_t 	_mgv_initCC1(void);
__uint32_t 	_mgv_initAB1(void);
__uint32_t 	_mgv_initVGI1(void);
__uint32_t	_mgv_inputStandardDetection(void);
__uint32_t	_mgv_VGI1GetChipID(void);
__uint32_t	_mgv_initXPoint(void);
__uint32_t	_mgv_initVideoInput(void);
__uint32_t	_mgv_initVideoOutput(void);
__uint32_t	_mgv_initGenlock(void);
__uint32_t 	_mgv_initCC1Int(void);
__uint32_t 	_mgv_initCSC(void);
__uint32_t 	_mgv_initTMI(void);
#ifdef IP30
__uint32_t	_mgv_probe_bridge(void);
__uint32_t      mgv_initBridge(void);
__uint32_t	mgv_initI2C(void);
void		mgv_reset_i2c(void);
__uint32_t 	_mgv_initEDH(void);
__uint32_t 	_mgv_initI2C(void);
#endif

__uint32_t
mgv_probe(void)
{
    __uint32_t port = 0;

    msg_printf(DBG, "Entering mgv_probe...\n");

#if HQ4
    mgvbase->mgbase = mgbase;
    mgras_probe_all_widgets();
    port = _mgv_probe_bridge();
    /*** Store port in global variable ***/
    bridge_port = port;
    msg_printf(DBG, "mgbase 0x%x; port 0x%x\n", mgbase, port);
    if (!port) {
	return(0);
    }
    mgvbase->vgi1_1_base = (void *)gio2xbase(port,
		((PHYS_TO_K1(MGRAS_BASE0_PHYS))));
   /***		((PHYS_TO_K1(MGRAS_BASE0_PHYS)) + 4)); ***/
    mgvbase->vgi1_2_base = (void *)gio2xbase(port,
		((PHYS_TO_K1(MGRAS_BASE0_PHYS))));
   /***		((PHYS_TO_K1(MGRAS_BASE0_PHYS)) + 4)); ***/
#else
    /*
     * XXX: Assumes that the Video in Slot 1 (expansion 0)
     */
    mgvbase->mgbase      = (mgras_hw *)PHYS_TO_K1(MGRAS_BASE0_PHYS);
    mgvbase->vgi1_1_base = (mgras_hw *)PHYS_TO_K1(MGRAS_BASE1_PHYS);
    mgvbase->vgi1_2_base = mgvbase->vgi1_1_base;
#endif
   /*
    * Get the base addresses from mgrashq3(4).h
    */
    mgvbase->ab      = (__paddr_t)(&(mgbase->pad_dcb_10[0]));
    mgvbase->cc      = (__paddr_t)(&(mgbase->pad_dcb_12[0]));
    mgvbase->gal_15  = (__paddr_t)(&(mgbase->pad_dcb_13[0]));
    mgvbase->csc_tmi = (__paddr_t)(&(mgbase->pad_dcb_14[0]));

    msg_printf(DBG, "mgbase 0x%x; brbase 0x%x; vgi1_1_base 0x%x; \n \
	vgi1_2_base 0x%x; ab 0x%x; cc 0x%x; gal_15 0x%x; csc_tmi 0x%x\n",
	mgvbase->mgbase, mgvbase->brbase, mgvbase->vgi1_1_base,
	mgvbase->vgi1_2_base, mgvbase->ab, mgvbase->cc, 
	mgvbase->gal_15, mgvbase->csc_tmi);

    msg_printf(DBG, "Leaving mgv_probe...\n");

    return(port);
}

__uint32_t
mgv_init(__uint32_t argc, char **argv)
{
    __uint32_t		i, device_num = 0, gio64_arb, errors = 0;
    mgv_vgi1_info_t     *pvgi1 = &(mgvbase->vgi1[0]);
    mgv_vgi1_info_t     *pvgi2 = &(mgvbase->vgi1[1]);

    msg_printf(SUM, "IMPV:- %s\n", mgv_err[MGV_INIT_TEST].TestStr);

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

    if ( device_num > MGV_LAST_DEVICE_NUM) {
	msg_printf(SUM,
	 "Usage: mgv_init -d <device_num>\n\
	 -d --> device number [1|2|3|4|5|6|7|8|9|10|11|12|13|14|15]\n\
		 1  - Galileo1.5 board \n\
		 2  - EDH \n\
		 3  - Cross Point \n\
		 4  - Input Standard \n\
		 5  - Video Input \n\
		 6  - Video Output \n\
		 7  - Genlock \n\
		 8  - CC1 \n\
		 9  - AB1 \n\
		10  - CSC \n\
		11  - TMI \n\
		12  - CC1-Int \n\
		13  - VGI1-1 \n\
		14  - VGI1-2 \n\
		15  - I2C \n");
	return(-1);
    }

    /*
     * initializes the DCB control registers in HQ3
     */
    mgbase->dcbctrl_cc1 = HQ3_CC1_PROTOCOL_NOACK /*HQ3_CC1_PROTOCOL_ASYNC_FRAME*/;
    mgbase->dcbctrl_ab1 = HQ3_AB1_PROTOCOL_ASYNC;
    mgbase->dcbctrl_gal = HQ3_GAL_PROTOCOL_NOACK;
    mgbase->dcbctrl_tmi = HQ3_TMI_PROTOCOL_SYNC /*HQ3_TMI_PROTOCOL_NOACK*/;

    /*
     * XXX: Setup the page size and masks
     */
    mgvbase->pageBits = VGI1_PAGE_SIZE_4K_BITS;
    mgvbase->pageSize = (0x1 << mgvbase->pageBits);
    mgvbase->pageMask = ((__paddr_t) ~0x0) << mgvbase->pageBits;

    msg_printf(DBG, "pageBits 0x%x; pageSize 0x%x; pageMask 0x%x\n",
	mgvbase->pageBits, mgvbase->pageSize, mgvbase->pageMask);

#if !HQ4
    /* Program GIO64_ARB register (Video in Slot 1 (expansion 0) */
    gio64_arb = *(unsigned int *)(PHYS_TO_K1(GIO64_ARB));
    gio64_arb |= GIO_CONFIG_VGI1;
    *(unsigned int *)(PHYS_TO_K1(GIO64_ARB)) = gio64_arb;
    msg_printf(DBG, "GIO_CONFIG_VGI1 = 0x%x; gio64_arb = 0x%x\n", 
	GIO_CONFIG_VGI1, gio64_arb);

    msg_printf(DBG, "GIO64_ARB register has 0x%x\n", gio64_arb);
#endif

    /*
     * Initialize some of the paramters for VGI1 DMA tests
     */
    hpNTSC.h_offset 	= MGV_VGI1_CCIR601_525_H_OFS;
    hpNTSC.h_len 	= MGV_VGI1_CCIR601_525_H_LEN;
    vpNTSC.odd_offset 	= MGV_VGI1_CCIR601_525_ODD_OFS;
    vpNTSC.odd_len 	= MGV_VGI1_CCIR601_525_ODD_LEN;
    vpNTSC.even_offset 	= MGV_VGI1_CCIR601_525_EVEN_OFS;
    vpNTSC.even_len 	= MGV_VGI1_CCIR601_525_EVEN_LEN;
    hpPAL.h_offset 	= MGV_VGI1_CCIR601_625_H_OFS;
    hpPAL.h_len 	= MGV_VGI1_CCIR601_625_H_LEN;
    vpPAL.odd_offset 	= MGV_VGI1_CCIR601_625_ODD_OFS;
    vpPAL.odd_len 	= MGV_VGI1_CCIR601_625_ODD_LEN;
    vpPAL.even_offset 	= MGV_VGI1_CCIR601_625_EVEN_OFS;
    vpPAL.even_len 	= MGV_VGI1_CCIR601_625_EVEN_LEN;
    hpNTSCSQ.h_offset 	= MGV_VGI1_NTSC_SQ_PIXEL_H_OFS;
    hpNTSCSQ.h_len 	= MGV_VGI1_NTSC_SQ_PIXEL_H_LEN;
    vpNTSCSQ.odd_offset	= MGV_VGI1_NTSC_SQ_PIXEL_ODD_OFS;
    vpNTSCSQ.odd_len 	= MGV_VGI1_NTSC_SQ_PIXEL_ODD_LEN;
    vpNTSCSQ.even_offset= MGV_VGI1_NTSC_SQ_PIXEL_EVEN_OFS;
    vpNTSCSQ.even_len 	= MGV_VGI1_NTSC_SQ_PIXEL_EVEN_LEN;
    hpPALSQ.h_offset 	= MGV_VGI1_PAL_SQ_PIXEL_H_OFS;
    hpPALSQ.h_len 	= MGV_VGI1_PAL_SQ_PIXEL_H_LEN;
    vpPALSQ.odd_offset 	= MGV_VGI1_PAL_SQ_PIXEL_ODD_OFS;
    vpPALSQ.odd_len 	= MGV_VGI1_PAL_SQ_PIXEL_ODD_LEN;
    vpPALSQ.even_offset = MGV_VGI1_PAL_SQ_PIXEL_EVEN_OFS;
    vpPALSQ.even_len 	= MGV_VGI1_PAL_SQ_PIXEL_EVEN_LEN;

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
    pvgi1->ch_vout_ctrl_reg_offset 	= GAL_CH1_CTRL;
    pvgi1->ch_vid_src			= MGV_VGI1_VID_SRC_1;

    pvgi2->ch_setup_reg_offset 		= VGI1_CH2_SETUP;
    pvgi2->ch_trig_reg_offset 		= VGI1_CH2_TRIG;
    pvgi2->ch_dma_stride_reg_offset	= VGI1_CH2_DMA_STRIDE;
    pvgi2->ch_data_adr_reg_offset	= VGI1_CH2_DATA_ADR;
    pvgi2->ch_desc_adr_reg_offset	= VGI1_CH2_DESC_ADR;
    pvgi2->ch_status_reg_offset 	= VGI1_CH2_STATUS;
    pvgi2->ch_int_enab_reg_offset 	= VGI1_CH2_INT_ENAB;
    pvgi2->ch_hparam_reg_offset 	= VGI1_CH2_H_PARAM;
    pvgi2->ch_vparam_reg_offset 	= VGI1_CH2_V_PARAM;
    pvgi2->ch_vout_ctrl_reg_offset 	= GAL_CH2_CTRL;
    pvgi2->ch_vid_src			= MGV_VGI1_VID_SRC_2;
    pvgi1->stride = pvgi2->stride = 0x0; 

    /* copy the trigger on, off and abort values for later use */
    bcopy((char *)&mgv_trig_on, (char *)&(pvgi1->trigOn), 
		sizeof(mgv_vgi1_ch_trig_t));
    bcopy((char *)&mgv_trig_off, (char *)&(pvgi1->trigOff), 
		sizeof(mgv_vgi1_ch_trig_t));
    bcopy((char *)&mgv_abort_dma, (char *)&(pvgi1->abortDMA),
		sizeof(mgv_vgi1_ch_trig_t));
    pvgi2->trigOn = pvgi1->trigOn;
    pvgi2->trigOff = pvgi1->trigOff;
    pvgi2->abortDMA = pvgi1->abortDMA;

    mgvbase->burst_len = 0xA0;
    mgvbase->force_unused = 0x0;
    mgvbase->force_insuf = 0x0;
    mgvbase->intEnableVal = 0x3f08;

    /*
     * Initialize ALL or selected device
     */
    if (!device_num) {
	for (i = MGV_GAL15_INIT_NUM; i <= MGV_LAST_DEVICE_NUM; i++) {
		errors += _mgv_reset(i);
	}
    } else {
    	errors += _mgv_reset(device_num);
    }

    return(_mgv_reportPassOrFail((&mgv_err[MGV_INIT_TEST]), errors));

}

__uint32_t
_mgv_probe_bridge(void)
{
    __uint32_t i, alive, present, mfg, rev, part, port = 0;

#if HQ4

    msg_printf(DBG, "Entering _mgv_probe_bridge...\n");

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
		mgvbase->brbase = mg_xbow_portinfo_t[i].base;

		msg_printf(SUM, "Galileo 1.5 for SR Found in port# %d\n", port);
		msg_printf(DBG, "port_base 0x%x\n", mgvbase->brbase);
		break;
	    }
	}
    }

    if (!port) {
	msg_printf(ERR, "Galileo 1.5 for SR NOT Found!!!\n");
    }

    msg_printf(DBG, "Leaving _mgv_probe_bridge...\n");
#endif

    return(port);
}

#ifdef IP30

__uint32_t
mgv_initBridge(void)
{

    int                     rv;
    int                     n;
    bridgereg_t             intr_device = 0;
    bridgereg_t		    write_val, sav_val;
    giobr_bridge_config_t   bridge_config = &srv_bridge_config;

/*** Initialize bridge for VGI1 DMA ***/
    msg_printf(DBG,"Entering mgv_initBridge\n");
    bridge_xbow_port = bridge_port;
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
            rv = mgv_br_rrb_alloc(n, bridge_config->rrb[n]);
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

	/*** Write widget id number ***/
	BR_REG_RD_32(BRIDGE_WID_CONTROL,BRIDGE_WID_CONTROL_MASK,sav_val);
    if (bridge_xbow_port == 12) {
	BR_REG_WR_32(BRIDGE_WID_CONTROL,BRIDGE_WID_CONTROL_MASK,0x7f0043fc);
    }
    else if (bridge_xbow_port == 10) {
	BR_REG_WR_32(BRIDGE_WID_CONTROL,BRIDGE_WID_CONTROL_MASK,0x7f0043fa);
    }
    else if (bridge_xbow_port == 11) {
	BR_REG_WR_32(BRIDGE_WID_CONTROL,BRIDGE_WID_CONTROL_MASK,0x7f0043fb);
    }
    else { 
	BR_REG_WR_32(BRIDGE_WID_CONTROL,BRIDGE_WID_CONTROL_MASK,0x7f0043f9);
    }

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

__uint32_t
mgv_initI2C(void) 
{

    if ( mgv_iic_war ) {
	_mgv_outb_I2C(GAL_IIC_CTL,0x80);
	_mgv_outb_I2C(GAL_IIC_DATA,(GAL_ADDR_8584>>1));
	_mgv_outb_I2C(GAL_IIC_CTL,GAL_IIC_CLOCK_REG);
	_mgv_outb_I2C(GAL_IIC_DATA,GAL_SCLK_8MHZ | GAL_SCL_45KHZ);
	_mgv_outb_I2C(GAL_IIC_CTL,0xC1);
    }
    return 0;
}

void
mgv_reset_i2c(void)
{
	GAL_IND_W1(mgvbase,GAL_IIC_RESET,0x00);
	delay(300);
	GAL_IND_W1(mgvbase,GAL_IIC_RESET,0x80);
}

#endif

__uint32_t
_mgv_reset(__uint32_t device_num)
{
    __uint32_t	errors = 0;

    msg_printf(DBG,"Entering mgv_reset\n");

    switch (device_num) {
    case MGV_GAL15_INIT_NUM:
    	/* reset the board and probe it */
    	GAL_W1(mgvbase, GAL_DIR3, 0x1);
	msg_printf(SUM, "Reset the board...\n");
    	delay (300);
	mgv_reset_i2c();
    	delay (300);
	break;

    case MGV_INPUT_INIT_NUM:
    	/* find out if input present and set the TV standard */
    	errors += _mgv_inputStandardDetection();
    	delay (300);
	break;

    case MGV_XPOINT_INIT_NUM:
    	/* initialize the VBAR */
    	errors += _mgv_initXPoint(); 
	break;

    case MGV_VIDEO_IN_INIT_NUM:
    	/* initialize Video Input */
    	errors += _mgv_initVideoInput();
	break;

    case MGV_VIDEO_OUT_INIT_NUM:
    	/* initialize Video Output */
    	errors += _mgv_initVideoOutput();
	break;

    case MGV_GENLOCK_INIT_NUM:
    	/* initialize Genlock */
    	errors += _mgv_initGenlock();
/*** added for debuging CC1 vs  ***/
    	delay (300);
	break;

    case MGV_CC1_INIT_NUM:
    	errors += _mgv_initCC1();
	break;

    case MGV_AB1_INIT_NUM:
    	delay (30);
	msg_printf(DBG,"Calling AB1 init routine from mgv_reset\n");
    	errors += _mgv_initAB1();
	break;

#if HQ4
    case MGV_CSC_INIT_NUM:
/*** Warn - reduced delay in 3 places from 300000 ***/
    	delay (30);
    	errors += _mgv_initCSC();
	break;

    case MGV_TMI_INIT_NUM:
    	delay (30);
    	errors += _mgv_initTMI();
	break;

    case MGV_VGI1_1_INIT_NUM:
	MGV_SET_VGI1_1_BASE;
    	errors += _mgv_initVGI1();
	break;

    case MGV_EDH_INIT_NUM:
    	errors += _mgv_initEDH();
	break;

    case MGV_I2C_INIT_NUM:
    	errors += _mgv_initI2C();
	break;

#if 0
    case MGV_VGI1_2_INIT_NUM:
	MGV_SET_VGI1_2_BASE;
    	errors += _mgv_initVGI1();
	break;
#endif

#else
    case MGV_CSC_INIT_NUM:
    	/* XXX: initialize CSC and TMI ????? */
    	mgv_csc_tmi_probe();
	break;

    case MGV_VGI1_1_INIT_NUM:
    case MGV_VGI1_2_INIT_NUM:
	MGV_SET_VGI1_1_BASE;
    	errors += _mgv_initVGI1();
	break;
#endif

    case MGV_CC1_INT_INIT_NUM:
    	/* initialize the CC1 interrupt */
    	errors += _mgv_initCC1Int();
	break;

    }

    return(errors);
}


/*
	Tries to find out whether CSC/TMI optional board
        is present. Returns 0 if present, -1 otherwise.
	It also stores rev numbers into mgvbase.
*/

__uint32_t
_mgv_initCSC(void)
{
    /*
     * Reset CSC chip for "N" ticks.
     */
    CSC_W1(mgvbase, CSC_RESET, 0x1);
    delay (30);
    CSC_W1(mgvbase, CSC_RESET, 0x0);

    return(0);
}

#ifdef IP30

__uint32_t
_mgv_initTMI(void)
{

        /* external hardware default register values */
        struct _mgv_ind_regs tmi_ind_def_ext[] = { 
                        /* register, (0x??) value pairs */

                TMI_MODE_STAT,          0x00,   /* 8 bit round, no mipmap, */
                                                /* RGB out, interrupt off, */
                                                /* NTSC, swap = A */

                TMI_LEFT_START_MS,      0x00,   /* h left edge (0 msb) */
                TMI_LEFT_START_LS,      0x00,   /* h left edge (0 lsb) */
                TMI_HIN_LENGTH_MS,      0x02,   /* h input length (640 msb) */
                TMI_HIN_LENGTH_LS,      0xd0,   /* h input length (640 lsb) */
                TMI_VIN_DELAY_MS,       0x00,   /* v input delay (19 msb) */
                TMI_VIN_DELAY_LS,       0x13,   /* v input delay (19 lsb) */
                TMI_VOUT_DELAY_MS,      0x00,   /* v output delay (200 msb) */
                TMI_VOUT_DELAY_LS,      0xc8,   /* v output delay (200 lsb) */
                /* 155520/2 = 77760 */
                TMI_PIX_COUNT_HIGH,     0x01,   /* pix count (77760 high) */
                TMI_PIX_COUNT_MID,      0x55,   /* pix count (77760 mid) */
                TMI_PIX_COUNT_LOW,      0xb8,   /* pix count (77760 low) */
                TMI_HOUT_LENGTH_MS,     0x01,   /* h output length (320 msb) */
                TMI_HOUT_LENGTH_LS,     0x68,   /* h output length (320 lsb) */
                TMI_TEST_MISC,          0x0,    /* power-on default value */
                TMI_HBLNK_DLY_COUNT_MS, 0x3,    /* blank delay (858 msb) */
                TMI_HBLNK_DLY_COUNT_LS, 0x5a,   /* blank delay (858 lsb) */
                -1,-1
                };

        /* scaler hardware default register values */
        struct _mgv_ind_regs tmi_ind_def_scl[] = {
                        /* register, (0x??) value pairs */
                TMI_Y_VSRC_CNT_LSB,     0xf3,   /* Y src lines (243) lsb */
                TMI_Y_VSRC_CNT_MSB,     0x00,   /* Y src lines (243) msb */
                TMI_Y_HSRC_CNT_LSB,     0xd0,   /* Y src pixels (640) lsb */
                TMI_Y_HSRC_CNT_MSB,     0x02,   /* Y src pixels (640) msb */
                TMI_Y_VTRG_CNT_LSB,     0xf3,   /* Y targ lines (243) lsb */
                TMI_Y_VTRG_CNT_MSB,     0x00,   /* Y targ lines (243) msb */
                TMI_Y_HTRG_CNT_LSB,     0xd0,   /* Y targ pixels (640) lsb */
                TMI_Y_HTRG_CNT_MSB,     0x02,   /* Y targ pixels (640) msb */
                TMI_UV_VSRC_CNT_LSB,    0xf3,   /* UV src lines (243) lsb */
                TMI_UV_VSRC_CNT_MSB,    0x00,   /* UV src lines (243) msb */
                TMI_UV_HSRC_CNT_LSB,    0xd0,   /* UV src pixels (640) lsb */
                TMI_UV_HSRC_CNT_MSB,    0x02,   /* UV src pixels (640) msb */
                TMI_UV_VTRG_CNT_LSB,    0xf3,   /* UV targ lines (243) lsb */
                TMI_UV_VTRG_CNT_MSB,    0x00,   /* UV targ lines (243) msb */
                TMI_UV_HTRG_CNT_LSB,    0xd0,   /* UV targ pixels (640) lsb */
                TMI_UV_HTRG_CNT_MSB,    0x02,   /* UV targ pixels (640) msb */
                TMI_VIN_DELAY_ODD,      0x00,   /* odd fld blank lines  = 0 */
                TMI_VIN_DELAY_EVEN,     0x00,   /* even fld blank lines  = 0 */
                -1,-1
                };

    mgv_ind_regs        *pRegs = tmi_ind_def_ext;
    mgv_ind_regs        *pRegsScl = tmi_ind_def_scl;
    uchar_t val;
/*for test*/
    uchar_t rcv;
    __uint32_t i;
/*end test*/
    msg_printf(SUM, "Entering mgv_initTMI\n");
    /*Venk*/
    /* Connect HQ4 to video texture */
    mgvbase->mgbase->tdma_format_ctl = 1;

    /* make graphics frame reset come from option card */
    GAL_IND_W1(mgvbase, GAL_GFX_FRAME_RESET, 0x1);

    /* Reset TMI ASIC */
    /* srv_galdir(srv_bd,GAL_INDIR3,0x4,VID_WRITE); */
    GAL_W1(mgvbase,GAL_DIR3,0x4);

    /* Clear TMI ASIC reset */
    /* srv_galdir(srv_bd,GAL_INDIR3,0x0,VID_WRITE); */
    GAL_W1(mgvbase,GAL_DIR3,0x0);

    /* Set TMI alpha range */
    /* srv_galdir(srv_bd,GAL_INDIR6,0x40,VID_WRITE); */
    /* srv_galdir(srv_bd,GAL_INDIR7,0,VID_WRITE); */
    GAL_W1(mgvbase,GAL_DIR6,0x40);
    GAL_W1(mgvbase,GAL_DIR7,0x0);

    /* Reset scaler (bit0) and TMI hardware (bit2) */
    /* srv_galdir(srv_bd,GAL_INDIR5,0x5,VID_WRITE); */
    GAL_W1(mgvbase,GAL_DIR5,0x5);
    us_delay(5000);

    /* init the external hardware registers */
    /* srv_tmiind_tbl(srv_bd,ext,VID_WRITE); */
    while (pRegs->reg != -1) {
        TMI_IND_W1(mgvbase, pRegs->reg, pRegs->val);
        pRegs++;
    }
    us_delay(5000);

    /* Reset TMI hardware (bit2) and pulse the scaler reset (bit3) */
    /* srv_galdir(srv_bd,GAL_INDIR5,0xc,VID_WRITE); */
    i=0;
    rcv= 0xf;
    GAL_W1(mgvbase,GAL_DIR5,0xc);
    us_delay(500000);

    while ( ((rcv == 0xf) || (rcv == 0xff)) && (i < 17)) {
        SCL_IND_W1(mgvbase, pRegsScl->reg, pRegsScl->val);
	msg_printf(DBG, "reg 0x%x val 0x%x\n", pRegsScl->reg, pRegsScl->val);
        us_delay(500000);
        SCL_IND_R1(mgvbase, i, rcv);
        SCL_IND_R1(mgvbase, i+32, rcv);
    	msg_printf(DBG, "SCL reg 0x%x, value 0x%x\n", pRegsScl->reg, rcv);
	i++;
	pRegsScl++;
	rcv=0xf;
    }

    pRegsScl = tmi_ind_def_scl;

    /* init the scaler registers */
    /* srv_tmiind_tbl(srv_bd,sclr,VID_WRITE); */
    while (pRegsScl->reg != -1) {
        SCL_IND_W1(mgvbase, pRegsScl->reg, pRegsScl->val);
    	GAL_W1(mgvbase,GAL_DIR5,0xc);
    	us_delay(5000);
        pRegsScl++;
    }
    us_delay(5000);

    /* clear the reset */
    /* srv_galdir(srv_bd,GAL_INDIR5,0x0,VID_WRITE); */
    GAL_W1(mgvbase,GAL_DIR5,0x0);

/*
    TMI_IND_R1(mgvbase, TMI_CHIP_ID, val);
    msg_printf(SUM, "TMI CHip ID = 0x%x\n", val);
*/
    msg_printf(SUM, "Leaving mgv_initTMI\n");
    return(0);
}

#endif

__uint32_t
mgv_csc_tmi_probe(void)
{

    unsigned char  rev ;

    /*
     * XXX: The CSC and TMI are ASICS for Speed Racer
     * We have to revisit the probing....
     */
    GAL_R1(mgvbase, GAL_DIR2, rev) ;
    mgvbase->info.bdrev = rev & 0xf ; /* low nibble*/
    mgvbase->info.CSC_TMIrev = (rev >> 4) & 0xf ; /* high nibble */
    if ( ((rev >> 4 ) & 0xf) == 0xf )
    {
        msg_printf(SUM, "No option card is attached\n");
        return(1);
    }

    /* If CSC/TMI optional board is present, initialize these
       internal values XXX
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
mgv_csc_reset(void)
{

    CSC_W1(mgvbase, CSC_RESET, 0x2 /*CSC reset XXX */) ;

    return (0);
}

__uint32_t      
_mgv_initI2C(void) 
{
    return(0);
}

__uint32_t
_mgv_inputStandardDetection(void)
{
    unsigned char vin_status_ch1, vin_status_ch2, video_std;
    __uint32_t	errors = 0;

    /* assume illegal values */
    mgvbase->videoMode = mgvbase->pixelMode = -1;

    /* check if input present */
    GAL_IND_R1(mgvbase, GAL_VIN_CH1_STATUS, vin_status_ch1);
    GAL_IND_R1(mgvbase, GAL_VIN_CH2_STATUS, vin_status_ch2);

    msg_printf(DBG, "vin_status_ch1 %d ; vin_status_ch2 %d\n",
	vin_status_ch1, vin_status_ch2);

    if ((vin_status_ch1 & 0x10) == 0) {
	video_std = vin_status_ch1 & 0x3;
    } else if ((vin_status_ch2 & 0x10) == 0) {
	video_std = vin_status_ch2 & 0x3;
    } else {
	msg_printf(SUM, "IMPV:- WARNING: Video input is not present\n");
	msg_printf(SUM, "IMPV:- Vid1 stat = 0x%x, Vid2 stat =  0x%x\n", vin_status_ch1,vin_status_ch2);
	video_std = 0;
    }

    /* set up TV standard */
    GAL_IND_W1(mgvbase, GAL_TV_STD, video_std);

    /* save the video and pixel modes */
    mgvbase->videoMode = (video_std & 0x2) >> 1;
    mgvbase->pixelMode = video_std & 0x1;

    msg_printf(DBG, "TV Standard 0x%x\n", video_std);

    return(errors);
}

__uint32_t
_mgv_initXPoint(void)
{
    __uint32_t	errors = 0;
    int real_reg, real_val;

    mgv_ind_regs mgv_xpoint_regs[] = {
	MUX_D1_CH1_OUT,  0x8,
	MUX_D1_CH2_OUT,  0x8,
	MUX_VGI1_CH1_IN, 0x7,
	MUX_VGI1_CH2_IN, 0x7,
	MUX_CC1_CH1_IN,  0x7,
	MUX_CC1_CH2_IN,  0x7,
	MUX_CSC_A,       0x7,
	MUX_CSC_B,       0x7,
	MUX_EXP_D_OUT,   0x7,
	MUX_EXP_SYNC_OUT,0x7,
	MUX_TMI_YUV_IN,  0x7,
	MUX_TMI_ALPHA_IN,0x7,
	MUX_EXP_A_OUT,   0xe, /* point to self => input */
	MUX_EXP_B_OUT,   0xf, /* point to self => input */
	-1,     -1
    };
    mgv_ind_regs	*pRegs = mgv_xpoint_regs;

    while (pRegs->reg != -1) {
	real_reg = GAL_MUX_ADDR;
	real_val = (pRegs->val<<4) | (pRegs->reg&0xf);
        GAL_IND_W1 (mgvbase, real_reg, real_val);

	pRegs++;
    }

    return(errors);
}

__uint32_t
_mgv_initVideoInput(void)
{
    __uint32_t	errors = 0;
    mgv_ind_regs mgv_vin_regs[] = {
	GAL_VIN_CTRL, 0x3,
	-1,     -1
    };
    mgv_ind_regs	*pRegs = mgv_vin_regs;

    while (pRegs->reg != -1) {
        GAL_IND_W1 (mgvbase, pRegs->reg, pRegs->val);
	pRegs++;
    }

    return(errors);
}

__uint32_t
_mgv_initVideoOutput(void)
{
    __uint32_t	errors = 0;
    mgv_ind_regs mgv_vout_regs[] = {
	GAL_CH1_HPHASE_LO, LOBYTE(HPHASE_CENTER),
	GAL_CH1_HPHASE_HI, HIBYTE(HPHASE_CENTER),
	GAL_CH2_HPHASE_LO, LOBYTE(HPHASE_CENTER),
	GAL_CH2_HPHASE_HI, HIBYTE(HPHASE_CENTER),
	GAL_CH1_CTRL,      0x0,
	GAL_CH2_CTRL,      0x0,
	-1,     -1
    };
    mgv_ind_regs	*pRegs = mgv_vout_regs;

    while (pRegs->reg != -1) {
        GAL_IND_W1 (mgvbase, pRegs->reg, pRegs->val);
	pRegs++;
    }

    return(errors);
}

__uint32_t
_mgv_initGenlock(void)
{
    __uint32_t	errors = 0;
    mgv_ind_regs mgv_genlock_regs[] = {
	GAL_VCXO_LO,  LOBYTE(DAC_CENTER),
	GAL_VCXO_HI,  HIBYTE(DAC_CENTER),
	GAL_REF_CTRL, 0x0,
	GAL_REF_OFFSET_LO, LOBYTE(BOFFSET_CENTER),
	GAL_REF_OFFSET_HI, HIBYTE(BOFFSET_CENTER), /*Venk next 2 lines*/
#ifdef IP30
        GAL_GFX_FRAME_RESET, 0x0, /* V from CC1 */
        GAL_30_60_REFRESH, 0x3,   /* V pulses to gfx, 60hz default */
#endif
	-1,     -1
    };
    mgv_ind_regs	*pRegs = mgv_genlock_regs;

    msg_printf(DBG, "Inside _mgv_initGenlock...\n");

    while (pRegs->reg != -1) {
        GAL_IND_W1 (mgvbase, pRegs->reg, pRegs->val);
	pRegs++;
    }

    msg_printf(DBG, "Outside _mgv_initGenlock...\n");

    return(errors);
}

#ifdef IP30

__uint32_t
_mgv_initEDH(void)
{
    __uint32_t	errors = 0;
    mgv_ind_regs mgv_edh_regs[] = {
	GAL_EDH_FLAGS1, 0xc0,
	GAL_EDH_FLAGS2, 0xc0,
	GAL_EDH_FLAGS3, 0x80,
	GAL_EDH_FLAGS4, 0x80,
	GAL_EDH_DEVADDR,0x00,
	-1,     -1
    };
    mgv_ind_regs	*pRegs = mgv_edh_regs;

    msg_printf(DBG, "Inside _mgv_initEDH...\n");

    while (pRegs->reg != -1) {
        GAL_IND_W1 (mgvbase, pRegs->reg, pRegs->val);
	pRegs++;
    }

    msg_printf(DBG, "Outside _mgv_initEDH...\n");

    return(errors);
}

#endif
__uint32_t
_mgv_initCC1(void)
{
    __uint32_t	tmp, errors=0;
    unsigned char sysctl = 0x4;
    mgv_ind_regs mgv_cc1_ind_regs[] = { /* register, (0x??) value pairs */
         0, 0x0,         1, 0x9,         2, 0x1,         3, 0x9,    4, 0x0,
         8, 0x0,         9, 0x8f,       10, 0x4,        11, 0x0,   12, 0xe9,
        13, 0x0,        14, 0x82,       16, 0x0,        17, 0x0,   18, 0x10,
        19, 0x0,        20, 0x0,        21, 0x0,        22, 0x0,   24, 0x0,
        25, 0x0,        26, 0x10,       27, 0x0,        28, 0x0,   29, 0x0,
        30, 0x0,        32, 0xa,        33, 0x0,        34, 0x1e,  35, 0x4,
        36, 0x1,        37, 0x0,        38, 0xa0,       39, 0x0,   40, 0x0,
        41, 0xa,        42, 0x0,        43, 0xa,        44, 0x0,   45, 0x0,
        46, 0x0,        47, 0x0,        48, 0x0,        49, 0x0,   50, 0x0,
        51, 0x80,       52, 0x80,       53, 0x80,       54, 0x0,   55, 0x40,
        56, 0x40,       57, 0x0,        58, 0x0,        59, 0x5,   64, 0x0,
        65, 0x0,        66, 0x0,        67, 0x0,        68, 0x0,   69, 0x0,
        70, 0x0,        71, 0x0,        72, 0x0,        73, 0x0,   74, 0x0,
        75, 0x0,        76, 0x0,        77, 0x0,        78, 0x0,   79, 0x0,
        80, 0x0,
        -1, -1 };

    if (mgvbase->pixelMode == PIXEL_13_5MHZ) {
	sysctl |= 0x2;
    }

    if (mgvbase->videoMode == VSTD_PAL) {
	sysctl |= 0x1;
	/* XXX: change the default values */
#if THIS_NEEDS_ATTENTION
	_mgv_change_default(cc_ind_default,1,17);
	_mgv_change_default(cc_ind_default,3,17);
	_mgv_change_default(cc_ind_default,36,34);
	_mgv_change_default(cc_ind_default,10,8);
	_mgv_change_default(cc_ind_default,12,63);
#endif
    }
    /* got from _mgv_init_cc1_io */
    GAL_IND_W1 (mgvbase, GAL_CC1_IO_CTRL5, 0x0);
    GAL_IND_W1 (mgvbase, GAL_CC1_IO_CTRL6, 0x80);
    GAL_IND_W1 (mgvbase, GAL_CC1_GFX_TOP_BLANK, 0x1);

    CC_W1(mgvbase, CC_SYSCON, 0x6 /* sysctl */);

    tmp = 0;
    while (mgv_cc1_ind_regs[tmp].reg != -1) {
	CC_W1 (mgvbase, CC_INDIR1, mgv_cc1_ind_regs[tmp].reg);
	CC_W1 (mgvbase, CC_INDIR2, mgv_cc1_ind_regs[tmp].val);
	tmp++;
    }

#if 1
#if 0
    CC_W1(mgvbase, CC_INDIR1, 38);
    CC_W1(mgvbase, CC_INDIR2, 0x2);

    do {
	CC_R1 (mgvbase, CC_INDIR2, tmp);
	timeout++;
	if (timeout == 10000) {
	    msg_printf(ERR, "CCIND38 bit 6 is set \n");
	    errors++;
	    return(errors);
	}
    } while ((tmp & 0x40) && (timeout < 10000));
#endif

    CC_W1 (mgvbase, CC_INDIR1, 54);
    CC_W1 (mgvbase, CC_INDIR2, 0x00);
    CC_W1 (mgvbase, CC_INDIR1, 55);
    CC_W1 (mgvbase, CC_INDIR2, 0x40);
    CC_W1 (mgvbase, CC_INDIR1, 56);
    CC_W1 (mgvbase, CC_INDIR2, 0x30);
    CC_W1 (mgvbase, CC_INDIR1, 57);
    CC_W1 (mgvbase, CC_INDIR2, 0x0);
    CC_W1 (mgvbase, CC_INDIR1, 58);
    CC_W1 (mgvbase, CC_INDIR2, 0x0);
#endif

    /* Setup frame buffer */
    CC_W1 (mgvbase, CC_FRAME_BUF, 0x40);
    CC_W1 (mgvbase, CC_FLD_FRM_SEL, 0);

#if 0
    /* do double buffer */
    CC_W1(mgvbase, CC_SYSCON, (sysctl|0x40));
#endif

    return(errors);
}

__uint32_t
_mgv_initCC1Int(void)
{
    __uint32_t	errors = 0;
   msg_printf(DBG, "Inside _mgv_initCC1Int...\n");
   /*
    *  Initialze the CC1 interrupt.  The input interrupt is
    *  always the interrupt source.  For now, interrupt at line:
    *
    *      NTSC - line 10 (CC1) = line 19 (TV)
    *      PAL  - line 10 (CC1) = line 14 (TV)
    *
    *  The interrupt remains disabled until needed.
    */
#if 0
    _mgv_singlebit(BT_ENABLE_INT, 0, BT_CC_INDIR_GROUP);
    _mgv_singlebit(BT_INT_SOURCE, 0, BT_CC_INDIR_GROUP);
    _mgv_singlebit(BT_INT_IN_LINE_LO, 10, BT_CC_INDIR_GROUP);
    _mgv_singlebit(BT_INT_IN_LINE_HI, 0, BT_CC_INDIR_GROUP);
    _mgv_singlebit(BT_ENABLE_INT, 1, BT_CC_INDIR_GROUP);
#endif
    _mgv_singlebit(BT_XP_INP, 0, BT_CC_DIR_GROUP);
    _mgv_singlebit(BT_XP_OUT, 0, BT_CC_DIR_GROUP);

   msg_printf(DBG, "Outside _mgv_initCC1Int...\n");

    return(errors);
}

__uint32_t
_mgv_initAB1(void)
{
    __uint32_t	tmp, errors=0;
    unsigned char sysctl = 0x1; /* assume NTSC */
    mgv_ind_regs mgv_ab1_ind_regs[] = { /* register, (0x??) value pairs */
		/* window A */
		0x0, 0x0,		/* X Offset Hi */
		0x1, 0x0, 	/* X off lo */
		0x2, 0x0, 	/* Y offset hi */
		0x3, 0x0, 	/* Y offset lo */
		0x4, 0x20,	/* X MOD 5 */
		0x5, 0x3, 	/* Win A status - Enabled, 1 to 1 size */
		0x6, 0x3, 	/* Win A info - graph to vid */
		0x7, 0x0, 	/* Decimation - full size */
		
		/* Window B */
		0x10, 0x0,	/* X Offset Hi */
		0x11, 0x0, 	/* X off lo */
		0x12, 0x0, 	/* Y offset hi */
		0x13, 0x0, 	/* Y offset lo */
		0x14, 0x0, 	/* X MOD 5 */
		0x15, 0x0, 	/* status - Disabled, */
		0x16, 0x3, 	/* info - graph to vid */
		0x17, 0x0, 	/* Decimation - full size */

		/* Window C */
		0x20, 0x0,	/* X Offset Hi */
		0x21, 0x0, 	/* X off lo */
		0x22, 0x0, 	/* Y offset hi */
		0x23, 0x0, 	/* Y offset lo */
		0x24, 0x0, 	/* X MOD 5 */
		0x25, 0x0, 	/* status - Disabled, */
		0x26, 0x3, 	/* info - graph to vid */
		-1,-1 };

    msg_printf(DBG,"Entering _mgv_initAB1\n");
    AB_W1 (mgvbase, AB_CS_ID, 0x10);  /* First Chip ID 0x10, red ab1 */
    AB_W1 (mgvbase, AB_CS_ID, 0x21);  /* second Chip ID 0x21, green ab1 */
    AB_W1 (mgvbase, AB_CS_ID, 0x42);  /* Third Chip ID 0x42, blue ab1 */
    AB_W1 (mgvbase, AB_CS_ID, 0x83);  /* Third Chip ID 0x42, blue ab1 */

    if (mgvbase->videoMode == VSTD_PAL) sysctl = 0x0;
    if (mgvbase->pixelMode == PIXEL_13_5MHZ) sysctl |= 0x20;

    AB_W1 (mgvbase, AB_SYSCON, sysctl);
    AB_W1 (mgvbase, AB_HIGH_ADDR, 0);
    AB_W1 (mgvbase, AB_LOW_ADDR, 0);
    AB_W1 (mgvbase, AB_TEST_REGS, 0x4);

    tmp = 0;
    while (mgv_ab1_ind_regs[tmp].reg != -1) {
	AB_W1 (mgvbase, AB_HIGH_ADDR, 0);
	AB_W1 (mgvbase, AB_LOW_ADDR, mgv_ab1_ind_regs[tmp].reg);
	AB_W1 (mgvbase, AB_INT_REG, mgv_ab1_ind_regs[tmp].val);
	tmp++;
    }

    msg_printf(DBG,"Leaving _mgv_init_AB1\n");

    return(errors);
}

__uint32_t
_mgv_initVGI1(void)
{
    mgv_vgi1_sys_mode_t	sys_mode = {0LL};
    __uint32_t		errors = 0;
    ulonglong_t		gio_id, tmp, mask = ~(ulonglong_t)0x0;

    /*
     * Read and Verify GIO ID register
     */
    VGI1_R64(mgvbase, VGI1_GIO_ID, gio_id, mask);
    /* reduced to 30 for IP30 */
    delay (30);
    if ( (gio_id & GIO_ID_VGI1_MASK) != GIO_ID_VGI1) {
	msg_printf(SUM, "IMPV:- WARNING - No VGI1 present\n");
    }

    /*
     * Place the VGI1 system mode register in its initial state.
     */
    sys_mode.page_size = MGV_VGI1_PGSZ_4K;
    sys_mode.arb_method = MGV_VGI1_ARB_VGI1;
    sys_mode.burst_hld_off = 4;
    sys_mode.video_reset = 1;
    sys_mode.unused0 = 0x0;
    tmp = 0x0;
    bcopy((char *)&sys_mode, (char *)&tmp, sizeof(mgv_vgi1_sys_mode_t));
    VGI1_W64(mgvbase, VGI1_SYS_MODE, tmp, mask);
    delay (300);
    sys_mode.video_reset = 0;
    tmp = 0x0;
    bcopy((char *)&sys_mode, (char *)&tmp, sizeof(mgv_vgi1_sys_mode_t));
    VGI1_W64(mgvbase, VGI1_SYS_MODE, tmp, mask);
/*** Warn - reduced elay from 30000 to 300 for SRV testing - VS ***/
    delay (300);

    /* disable all interrupts for both channels */
    VGI1_W64(mgvbase, VGI1_CH1_INT_ENAB, 0x0, mask);
    VGI1_W64(mgvbase, VGI1_CH2_INT_ENAB, 0x0, mask);

    /* abort any pending DMA and turn the trigger off for both channels */
    bcopy((char *)&mgv_abort_dma, (char *)&tmp,sizeof(mgv_vgi1_ch_trig_t));
    VGI1_W64(mgvbase, VGI1_CH1_TRIG, tmp, mask);
    VGI1_W64(mgvbase, VGI1_CH2_TRIG, tmp, mask);
    bcopy((char *)&mgv_trig_off, (char *)&tmp, sizeof(mgv_vgi1_ch_trig_t));
    VGI1_W64(mgvbase, VGI1_CH1_TRIG, tmp, mask);
    VGI1_W64(mgvbase, VGI1_CH2_TRIG, tmp, mask);

    /* clear all flags in the status register for both channels */
    VGI1_W64(mgvbase, VGI1_CH1_STATUS, MGV_VGI1_CH_ALLINTRS, mask);
    VGI1_W64(mgvbase, VGI1_CH2_STATUS, MGV_VGI1_CH_ALLINTRS, mask);
    if (_mvg_VGI1PollReg(VGI1_CH1_STATUS,0x0,MGV_VGI1_CH_ALLINTRS,-1,&tmp)) {
	msg_printf(ERR, "Channel-1 Status register is not ZERO... Time Out\n");
	msg_printf(DBG, "Channel-1 Status exp 0x%llx; rcv 0x%llx\n",
		0x0, (tmp & MGV_VGI1_CH_ALLINTRS));
	errors++;
    } 
    if (_mvg_VGI1PollReg(VGI1_CH2_STATUS,0x0,MGV_VGI1_CH_ALLINTRS,-1,&tmp)) {
	msg_printf(ERR, "Channel-2 Status register is not ZERO... Time Out\n");
	msg_printf(DBG, "Channel-2 Status exp 0x%llx; rcv 0x%llx\n",
		0x0, (tmp & MGV_VGI1_CH_ALLINTRS));
	errors++;
    }
    return(errors);
}
