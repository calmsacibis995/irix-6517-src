/*
* Copyright 1998 - PsiTech, Inc.
* THIS DOCUMENT CONTAINS CONFIDENTIAL AND PROPRIETARY
* INFORMATION OF PSITECH.  Its receipt or possession does
* not create any right to reproduce, disclose its contents,
* or to manufacture, use or sell anything it may describe.
* Reproduction, disclosure, or use without specific written
* authorization of Psitech is strictly prohibited.
*
* 
* 9/15/98       PAG     created from rad1drv.c
*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/open.h>
#include <sys/cred.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/unistd.h> 
#include <sys/mman.h>
#include <sys/invent.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/immu.h>
#include <sys/sema.h>
#include <sys/ksynch.h>
#include <sys/ddi.h>
#ifdef IRIX63
void ddi_sv_wait(sv_t *svp, void *lkp, int rv);
#endif /*  IRIX63 */
#include <sys/kmem.h>
#include <stropts.h>
#include <sys/termio.h>
#include <sys/stermio.h>
#ifdef IRIX63
#include <sys/edt.h>
#include <sys/user.h>
#endif /*  IRIX63 */

#ifndef IRIX63
#include <sys/hwgraph.h>
#endif /*  IRIX63 */
#include <sys/PCI/pciio.h>
#include <sys/PCI/PCI_defs.h>
#ifdef IRIX65
#include <ksys/ddmap.h> /* */
#else /* IRIX65 */
#include <sys/region.h> /* */
#endif /* IRIX65 */
#include <sys/kmem.h>
#include "sys/htport.h"

#include "rad1hw.h"
#include "rad1.h"
#include "rad4pci.h"
#include "rad1sgidrv.h"

#ifdef _STANDALONE
#define pon_puts(msg)  /*  printf(msg)  */
#else /* _STANDALONE */
#define pon_puts(msg)   /* cmn_err(CE_DEBUG, msg) */
#endif /* _STANDALONE */

extern volatile int vol;

void
rad1_pinit(per2_info *p2, int console_flag)
{
        volatile int *registers = p2->registers;
        unsigned char *char_ptr;
        volatile unsigned char *saved_ptr;
        int mclkctl;
        int tv, hsp, vsp, sync;
        int i;
#ifdef _STANDALONE
        while (*(registers + INFIFOSPACE_REG/4) < 100)
        {
                vol = 0;
        } /* wait for room in register FIFO */
#else /* _STANDALONE */
        WAIT_IN_FIFO(100);      /* wait for room in register FIFO */
#endif /* _STANDALONE */
        
        p2->p2_intenable.all = 0;       /* disable interrupts */
        *(registers + INTENABLE_REG/4) = 0;

        p2->p2_aperture_1.all = 0;
        *(registers + APERTUREONE_REG/4) = 0;
        p2->p2_aperture_2.all = 0;
        p2->p2_aperture_2.bits.ROMAccess = RAD1_ENABLE;
        *(registers + APERTURETWO_REG/4) = p2->p2_aperture_2.all;
        
        p2->p2_videocontrol.all = 0;
        p2->p2_fbreadmode.all = 0;
                /* includes windowOrigin = WINDOWORIGIN_TOPLEFT & dataType = DATATYPE_FBDEFAULT */
        p2->p2_bydmacontrol.all = 0;
        p2->p2_lbreadmode.all = 0;
        p2->p2_texturemapformat.all = 0;

        if (!console_flag)
        {
                if (p2->device_id == RAD1_2V_DEVICE_ID)
                {
                        *(registers + RDINDEXLOW_2V_REG/4) = RDMCLKCONTROL_2V_REG;
                        *(registers + RDINDEXHIGH_2V_REG/4) = RDMCLKCONTROL_2V_REG >> 8;
                        mclkctl = *(registers + RDINDEXEDDATA_2V_REG/4) & ~PLL_ENABLED_2V_BIT;
                        WR_INDEXED_REGISTER_2V(registers, RDMCLKCONTROL_2V_REG, mclkctl);
                                /* stop memory PLL */

/* NOTE: memory clock frequency should be adjusted to reduce interference with*/
/*      pixel clock per Application Note PLL Programming */
                        /* set memory clock */
                        WR_INDEXED_REGISTER_2V(registers, RDMCLKPRESCALE_2V_REG,
                                RAD1_DEFAULT_MEM_N);
                        WR_INDEXED_REGISTER_2V(registers, RDMCLKFEEDBACKSCALE_2V_REG,
                                RAD1_DEFAULT_MEM_M);
                        WR_INDEXED_REGISTER_2V(registers, RDMCLKPOSTSCALE_2V_REG,
                                RAD1_DEFAULT_MEM_P);
                        WR_INDEXED_REGISTER_2V(registers, RDMCLKCONTROL_2V_REG,
                                mclkctl | PLL_ENABLED_2V_BIT);  /* start memory PLL */
                        /* wait for memory PLL to lock */
                        for (i = 0; i < 500; i++)
                        {
                                *(registers + RDINDEXLOW_2V_REG/4) = RDMCLKCONTROL_2V_REG;
                                *(registers + RDINDEXHIGH_2V_REG/4) = RDMCLKCONTROL_2V_REG >> 8;
                                if (*(registers + RDINDEXEDDATA_2V_REG/4) & PLL_LOCKED_2V_FLAG)
                                        break;
                        }
                }
#ifndef _STANDALONE
                else if (p2->device_id == RAD1_DEVICE_ID)
                {
                        WR_INDEXED_REGISTER(registers, RDMEMORYCLOCK3_REG, 6);  /* stop memory PLL */

/* NOTE: memory clock frequency should be adjusted to reduce interference with*/
/*      pixel clock per Application Note PLL Programming */
                        /* set memory clock */
                        WR_INDEXED_REGISTER(registers, RDMEMORYCLOCK2_REG,
                                RAD1_DEFAULT_MEM_N);
                        WR_INDEXED_REGISTER(registers, RDMEMORYCLOCK1_REG,
                                RAD1_DEFAULT_MEM_M);
                        WR_INDEXED_REGISTER(registers, RDMEMORYCLOCK3_REG,
                                RAD1_DEFAULT_MEM_P | PIXELCLOCK_EN_BIT);
                        /* wait for memory PLL to lock */
                        for (i = 0; i < 500; i++)
                        {
                                *(registers + RDPALETTEWRITEADDRESS_REG/4) = RDMEMORYCLOCKSTATUS_REG;
                                if (*(registers + RDINDEXEDDATA_REG/4) & PLL_LOCKED_FLAG)
                                        break;
                        }
                }
#endif /*  _STANDALONE */
                else
                {	printf("bad device id 0x%x\n",p2->device_id);
                }
        }

        p2->p2_scissormode.all = 0;
/*      p2->p2_scissormode.bits.screenScissorEnable = RAD1_ENABLE;    */
        *(registers + SCISSORMODE_REG/4) = p2->p2_scissormode.all;
        
        p2->p2_window.all = 0;
        p2->p2_window.bits.forceLBUpdate = RAD1_FALSE;
        p2->p2_window.bits.LBUpdateSource = LBUPDATESOURCE_REGISTERS;
        p2->p2_window.bits.disableLBUpdate = RAD1_TRUE;
        *(registers + WINDOW_REG/4) = p2->p2_window.all;

        /* disable graphics units */
        p2->p2_areastipplemode.all = 0;
        *(registers + AREASTIPPLEMODE_REG/4) = 0;
        p2->p2_depthmode.all = 0;
        *(registers + DEPTHMODE_REG/4) = 0;
        p2->p2_stencilmode.all = 0;
        *(registers + STENCILMODE_REG/4) = 0;
        *(registers + TEXTUREADDRESSMODE_REG/4) = 0;
        *(registers + TEXTUREREADMODE_REG/4) = 0;
        *(registers + TEXELLUTMODE_REG/4) = 0;
        p2->p2_yuvmode.all = 0;
        *(registers + YUVMODE_REG/4) = 0;
        p2->p2_colorddamode.all = 0;
        *(registers + COLORDDAMODE_REG/4) = 0;
        *(registers + TEXTURECOLORMODE_REG/4) = 0;
        p2->p2_fogmode.all = 0;
        *(registers + FOGMODE_REG/4) = 0;
        p2->p2_alphablendmode.all = 0;  /* this will give us COLORFORMAT_8888_2V */
        *(registers + ALPHABLENDMODE_REG/4) = p2->p2_alphablendmode.all;
        p2->p2_logicalopmode.all = 0;
        *(registers + LOGICALOPMODE_REG/4) = 0;
        p2->p2_statisticmode.all = 0;
        *(registers + STATISTICMODE_REG/4) = 0;
        p2->p2_fbwritemode.all = 0;
        p2->p2_fbwritemode.bits.writeEnable = RAD1_ENABLE;
        *(registers + FBWRITEMODE_REG/4) = p2->p2_fbwritemode.all;
        p2->p2_filtermode.all = 0;
        *(registers + FILTERMODE_REG/4) = 0;
        p2->p2_dithermode.all = 0;      /* this will give us COLORFORMAT_8888_2V */
        p2->p2_dithermode.bits.unitEnable = RAD1_ENABLE;
        *(registers + DITHERMODE_REG/4) = p2->p2_dithermode.all;
        *(registers + LBWRITEFORMAT_REG/4) = 0;
        *(registers + LBWRITEMODE_REG/4) = 0;
        *(registers + RASTERIZERMODE_REG/4) = 0;
        *(registers + DELTAMODE_REG/4) = 0;
        *(registers + WINDOWORIGIN_REG/4) = 0;
        *(registers + FBWINDOWBASE_REG/4) = 0;
        *(registers + LBWINDOWBASE_REG/4) = 0;
        *(registers + FBSOURCEBASE_REG/4) = 0;
        *(registers + FBPIXELOFFSET_REG/4) = 0;
        *(registers + TEXEL0_REG/4) = 0;
        *(registers + DXDOM_REG/4) = 0;
        *(registers + DXSUB_REG/4) = 0;
        *(registers + DY_REG/4) = INTtoFIXED16_16(1);
        *(registers + SSTART_REG/4) = 0;
        *(registers + DSDX_REG/4) = INTtoFIXED12_18(1);
        *(registers + DSDYDOM_REG/4) = 0;
        *(registers + TSTART_REG/4) = 0;
        *(registers + DTDX_REG/4) = 0;
        *(registers + DTDYDOM_REG/4) = 0;
        *(registers + TEXTUREDATAFORMAT_REG/4) = 0;
        *(registers + DMACONTROL_REG/4) = 0x08;

        p2->hard_mask = RAD1_WRITEMASK_RGB;
        *(registers + FBSOFTWAREWRITEMASK_REG/4) = RAD1_WRITEMASK_ALL;
        *(registers + FBHARDWAREWRITEMASK_REG/4) = RAD1_WRITEMASK_ALL;
        p2->mask = RAD1_WRITEMASK_RGB;
        *(registers + BYPASSWRITEMASK_REG/4) = RAD1_WRITEMASK_RGB;
        *(registers + FBREADPIXEL_REG/4) = FBREADPIXEL_32BITS;
        
        /* read saved setup info from EEPROM */
        char_ptr = (unsigned char *)(&p2->saved_info);
        saved_ptr = (((unsigned char *)p2->ram2) + SAVED_INFO_OFFSET);
        for (i = 0; i < RAD1_SECTOR_SIZE; i++) /* warm up the cache line */
	{ char_ptr[i ^ 3] = saved_ptr[i];
	}
        for (i = 0; i < RAD1_SECTOR_SIZE; i++)
	{ char_ptr[i ^ 3] = saved_ptr[i];
	}
        
#if 1
        tv = 0x7f & p2->saved_info.item[RESOLUTION_INDEX];
        hsp = (0x100 & p2->saved_info.item[RESOLUTION_INDEX]) >> 8;
        vsp = (0x400 & p2->saved_info.item[RESOLUTION_INDEX]) >> 10;
        sync = (0x3000 & p2->saved_info.item[RESOLUTION_INDEX]) >> 12;
#else
        tv = 6; /* */
/*      tv = 0;  */
        hsp = SYNC_NEGATIVE;
        vsp = SYNC_NEGATIVE;
        sync = SEPARATE;
#endif
	if(tv >= NumPER2Modes)
	{
		printf("rad1 resolution unknown mode 0x%x set to 0\n",tv);
		tv = 0; /* */
		hsp = SYNC_NEGATIVE;
		vsp = SYNC_NEGATIVE;
		sync = SEPARATE;
	}
        rad1_set_video_timing(p2, tv, sync, hsp, vsp, console_flag);
        p2->p2_videocontrol.bits.enable = RAD1_DISABLE; /* turn off display refresh */
        *(registers + VIDEOCONTROL_REG/4) = p2->p2_videocontrol.all;

        if (!console_flag)
        {
                /* set up memory configuration */
                *(registers + MEMCONTROL_REG/4) = MEMCONTROL_SGRAM;
                if (p2->device_id == RAD1_DEVICE_ID)
                {
                        /* NOTE: this set up is for Samsung -010 SGRAM chips */
#if 1
                        *(registers + BOOTADDRESS_REG/4) = 0x20;
                        p2->p2_memconfig.all = 0xE6002021;
#else
                        *(registers + BOOTADDRESS_REG/4) = 0x31;
                                /* set to burst length 2, sequential burst, CAS latency 3 */
                        p2->p2_memconfig.all = 0;
                        p2->p2_memconfig.bits.timeRP = 2;
                        p2->p2_memconfig.bits.timeRCD = 0;
                        p2->p2_memconfig.bits.timeRC = 5;
                        p2->p2_memconfig.bits.timeRASMin = 2;
                        p2->p2_memconfig.bits.CASLatency = CAS_LATENCY_3;
                        p2->p2_memconfig.bits.refreshCount = 48;
                        p2->p2_memconfig.bits.numberBanks = NUMBER_OF_BANKS_4;
                        p2->p2_memconfig.bits.burst1Cycle = RAD1_TRUE;
#endif
                }
                else if (p2->device_id == RAD1_2V_DEVICE_ID)
                {
                        /* NOTE: this set up is for MoSys -010 SGRAM chips */
#if 1
                        *(registers + BOOTADDRESS_REG/4) = 0x20;
                        p2->p2_memconfig.all = 0xEc000021;
#else
/*                      *(registers + BOOTADDRESS_REG/4) = 0x31;  */
                        *(registers + BOOTADDRESS_REG/4) = 0x21;
                                /* set to burst length 2, sequential burst, CAS latency 3 */
                        p2->p2_memconfig.all = 0;
                        p2->p2_memconfig.bits.timeRP = 1;
                        p2->p2_memconfig.bits.timeRCD = 0;
                        p2->p2_memconfig.bits.timeRC = 4;
/*                      p2->p2_memconfig.bits.timeRASMin = 2;  */
                        p2->p2_memconfig.bits.timeRASMin = 0;
/*                      p2->p2_memconfig.bits.CASLatency = CAS_LATENCY_3;  */
                        p2->p2_memconfig.bits.CASLatency = CAS_LATENCY_2;
                        p2->p2_memconfig.bits.refreshCount = 48;
                        p2->p2_memconfig.bits.numberBanks = NUMBER_OF_BANKS_4;
                        p2->p2_memconfig.bits.burst1Cycle = RAD1_TRUE;
#endif
                }
                *(registers + MEMCONFIG_REG/4) = p2->p2_memconfig.all;
        }

        rad1_init_RAMDAC(p2);   /* initialize RAMDAC */
}

void
rad1_set_video_timing(per2_info *p2, int index, unsigned int sync_mode,
        unsigned int h_polarity, unsigned int v_polarity, int console_flag)
{
        PVDATA pData = &PER2Modes[index].VData;
        int width = PER2Modes[index].modeInformation.VisScreenWidth;
        volatile int *registers = p2->registers;
        int i;
        int pclkctl;
        
        p2->v_info.x_size = width;
        p2->v_info.y_size = PER2Modes[index].modeInformation.VisScreenHeight;
        p2->v_info.v_rate = PER2Modes[index].modeInformation.Frequency;
        p2->v_info.index = index;

        for (i = 0; i < NumPER2pps; i++)
        {
                if (width == PER2pps[i].width)
                        break;
        }
        if (i >= NumPER2pps)
        {
                pon_puts("rad1_set_video_timing: unsupported width %d\n");
                i = 0;
        }
        
        p2->screen_base = 0;    /* in 64 bit units */
        p2->p2_videocontrol.bits.enable = RAD1_ENABLE;
        p2->p2_videocontrol.bits.blankCtl = BLANKCTL_HI;
        p2->p2_videocontrol.bits.data64Enable = RAD1_ENABLE;
        p2->p2_videocontrol.bits.hSyncCtl = SYNC_ACTIVE_HI;
        p2->p2_videocontrol.bits.vSyncCtl = SYNC_ACTIVE_HI;
        p2->p2_fbreadmode.bits.pp0 = PER2pps[i].pp0;
        p2->p2_fbreadmode.bits.pp1 = PER2pps[i].pp1;
        p2->p2_fbreadmode.bits.pp2 = PER2pps[i].pp2;
        p2->p2_bydmacontrol.bits.pp0 = PER2pps[i].pp0;
        p2->p2_bydmacontrol.bits.pp1 = PER2pps[i].pp1;
        p2->p2_bydmacontrol.bits.pp2 = PER2pps[i].pp2;
        p2->p2_lbreadmode.bits.pp0 = PER2pps[i].pp0;
        p2->p2_lbreadmode.bits.pp1 = PER2pps[i].pp1;
        p2->p2_lbreadmode.bits.pp2 = PER2pps[i].pp2;
        p2->p2_texturemapformat.bits.pp0 = PER2pps[i].pp0;
        p2->p2_texturemapformat.bits.pp1 = PER2pps[i].pp1;
        p2->p2_texturemapformat.bits.pp2 = PER2pps[i].pp2;

        p2->p2_packeddatalimits.bits.xEnd = width - 1;

        p2->p2_vclkctl.all = 0;
        p2->p2_vclkctl.bits.vClkCtl = 2;        /* select PLL C */
        p2->p2_vclkctl.bits.recoveryTime = 4 *
                (RAD1_ACTUAL_PLL_FREQ(pData->pllm, pData->plln, pData->pllp) / 33000000);
        if (p2->p2_vclkctl.bits.recoveryTime < 5)
                p2->p2_vclkctl.bits.recoveryTime = 5;

#ifdef _STANDALONE
        while (*(registers + INFIFOSPACE_REG/4) < 40)
        {
                vol = 0;
        } /* wait for room in register FIFO */
#else /* _STANDALONE */
        WAIT_IN_FIFO(40);       /* wait for room in register FIFO */
        SLP_LOCK_LOCK(&p2->hardware_mutex_lock);        /* lock the hardware */
#endif /* _STANDALONE */
        
        if (!console_flag)
        {
                *(registers + VGACONTROL_REG/4) = 0x40; /* disable SVGA */
                p2->p2_videocontrol.bits.enable = RAD1_DISABLE; /* turn video off */
                *(registers + VIDEOCONTROL_REG/4) = p2->p2_videocontrol.all;

                *(registers + VCLKCTL_REG/4) = p2->p2_vclkctl.all;

                /* set PLL registers */
                if (p2->device_id == RAD1_2V_DEVICE_ID)
                {
                        *(registers + RDINDEXLOW_2V_REG/4) = RDDCLKCONTROL_2V_REG;
                        *(registers + RDINDEXHIGH_2V_REG/4) = RDDCLKCONTROL_2V_REG >> 8;
                        pclkctl = *(registers + RDINDEXEDDATA_2V_REG/4) & ~PLL_ENABLED_2V_BIT;
                        WR_INDEXED_REGISTER_2V(registers, RDDCLKCONTROL_2V_REG, pclkctl);
                                /* stop pixel PLL */

/* NOTE: memory clock frequency should be adjusted to reduce interference with*/
/*      pixel clock per Application Note PLL Programming */
                        WR_INDEXED_REGISTER_2V(registers, RDDCLK2PRESCALE_2V_REG, pData->plln);
                        WR_INDEXED_REGISTER_2V(registers, RDDCLK2FEEDBACKSCALE_2V_REG, pData->pllm);
                        WR_INDEXED_REGISTER_2V(registers, RDDCLK2POSTSCALE_2V_REG, pData->pllp);
                        WR_INDEXED_REGISTER_2V(registers, RDDCLK3PRESCALE_2V_REG, pData->plln);
                        WR_INDEXED_REGISTER_2V(registers, RDDCLK3FEEDBACKSCALE_2V_REG, pData->pllm);
                        WR_INDEXED_REGISTER_2V(registers, RDDCLK3POSTSCALE_2V_REG, pData->pllp);
                        WR_INDEXED_REGISTER_2V(registers, RDDCLKCONTROL_2V_REG,
                                pclkctl | PLL_ENABLED_2V_BIT);  /* start pixel PLL */
                        /* wait for pixel PLL to lock */
                        for (i = 0; i < 500; i++)
                        {
                                *(registers + RDINDEXLOW_2V_REG/4) = RDDCLKCONTROL_2V_REG;
                                *(registers + RDINDEXHIGH_2V_REG/4) = RDDCLKCONTROL_2V_REG >> 8;
                                if (*(registers + RDINDEXEDDATA_2V_REG/4) & PLL_LOCKED_2V_FLAG)
                                        break;
                        }
                }
                else if (p2->device_id == RAD1_DEVICE_ID)
                {
                        WR_INDEXED_REGISTER(registers, RDPIXELCLOCKC3_REG, 0);  /* stop pixel PLL */

/* NOTE: memory clock frequency should be adjusted to reduce interference with*/
/*      pixel clock per Application Note PLL Programming */
                        WR_INDEXED_REGISTER(registers, RDPIXELCLOCKC2_REG, pData->plln);
                        WR_INDEXED_REGISTER(registers, RDPIXELCLOCKC1_REG, pData->pllm);
                        WR_INDEXED_REGISTER(registers, RDPIXELCLOCKC3_REG, 
                                pData->pllp | PIXELCLOCK_EN_BIT);
                        /* wait for pixel PLL to lock */
                        for (i = 0; i < 500; i++)
                        {
                                *(registers + RDPALETTEWRITEADDRESS_REG/4) = RDPIXELCLOCKSTATUS_REG;
                                if (*(registers + RDINDEXEDDATA_REG/4) & PLL_LOCKED_FLAG)
                                        break;
                        }
                }

                /* set up video timing */
                *(registers + HTOTAL_REG/4) = pData->htotal/2;
                *(registers + HGEND_REG/4) = pData->hgend/2;
                *(registers + HBEND_REG/4) = pData->hbend/2;
                *(registers + HSSTART_REG/4) = pData->hsstart/2;
                *(registers + HSEND_REG/4) = pData->hsend/2;
                *(registers + VTOTAL_REG/4) = pData->vtotal;
                *(registers + VBEND_REG/4) = pData->vbend;
                *(registers + VSSTART_REG/4) = pData->vsstart;
                *(registers + VSEND_REG/4) = pData->vsend;
                *(registers + SCREENBASE_REG/4) = p2->screen_base;      /* in 64 bit units */
                *(registers + SCREENBASERIGHT_REG/4) = 0;       /* in 64 bit units */
                *(registers + SCREENSTRIDE_REG/4) = pData->screenstride;        /* in 64 bit units */
                *(registers + FIFOCONTROL_REG/4) = 0x0B07; /* */
/*              *(registers + FIFOCONTROL_REG/4) = 0x1007;  */
        
                p2->p2_videocontrol.bits.enable = RAD1_ENABLE;
                *(registers + VIDEOCONTROL_REG/4) = p2->p2_videocontrol.all;
        }

        if (p2->device_id == RAD1_2V_DEVICE_ID)
        {
                p2->p2_rdsynccontrol_2v.all = 0;
                if (sync_mode == SYNC_ON_GREEN)
                        pon_puts("rad1_set_video_timing: Sync-on-Green not supported\n");
                if (h_polarity == SYNC_NEGATIVE)
                        p2->p2_rdsynccontrol_2v.bits.hSyncCtl = ACTIVE_LOW_AT_PIN_2V;
                else
                        p2->p2_rdsynccontrol_2v.bits.hSyncCtl = ACTIVE_HIGH_AT_PIN_2V;
                if (v_polarity == SYNC_NEGATIVE)
                        p2->p2_rdsynccontrol_2v.bits.vSyncCtl = ACTIVE_LOW_AT_PIN_2V;
                else
                        p2->p2_rdsynccontrol_2v.bits.vSyncCtl = ACTIVE_HIGH_AT_PIN_2V;
                WR_INDEXED_REGISTER_2V(registers, RDSYNCCONTROL_2V_REG,
                        p2->p2_rdsynccontrol_2v.all);
        }
        else if (p2->device_id == RAD1_DEVICE_ID)
        {
                p2->p2_rdmisccontrol.all = 0;
                p2->p2_rdmisccontrol.bits.paletteWidth = PALETTEWIDTH_8BITS;
/*              p2->p2_rdmisccontrol.bits.blankPedestal = RAD1_ENABLE;   */
                if (sync_mode == SYNC_ON_GREEN)
                        p2->p2_rdmisccontrol.bits.syncOnGreen = RAD1_ENABLE;
                if (h_polarity == SYNC_NEGATIVE)
                        p2->p2_rdmisccontrol.bits.hSyncPolarity = SYNC_INVERTED;
                else
                        p2->p2_rdmisccontrol.bits.hSyncPolarity = SYNC_NONINVERTED;
                if (v_polarity == SYNC_NEGATIVE)
                        p2->p2_rdmisccontrol.bits.vSyncPolarity = SYNC_INVERTED;
                else
                        p2->p2_rdmisccontrol.bits.vSyncPolarity = SYNC_NONINVERTED;
                WR_INDEXED_REGISTER(registers, RDMISCCONTROL_REG, p2->p2_rdmisccontrol.all);
        }

        *(registers + FBREADMODE_REG/4) = p2->p2_fbreadmode.all;
        *(registers + BYDMACONTROL_REG/4) = p2->p2_bydmacontrol.all;
        *(registers + LBREADMODE_REG/4) = p2->p2_lbreadmode.all;
        *(registers + TEXTUREMAPFORMAT_REG/4) = p2->p2_texturemapformat.all;
        *(registers + SCREENSIZE_REG/4) = SCREENSIZE_HW(p2->v_info.y_size, width);
        *(registers + PACKEDDATALIMITS_REG/4) = p2->p2_packeddatalimits.all;

#ifndef _STANDALONE
        SLP_LOCK_UNLOCK(&p2->hardware_mutex_lock);      /* release the hardware */
#endif /* _STANDALONE */
pon_puts("!leaving rad1_set_video_timing\n");
}


void
rad1_init_RAMDAC(per2_info *p2)
{
        int i, j;
        volatile int *registers = p2->registers;
#ifdef _STANDALONE
        while (*(registers + INFIFOSPACE_REG/4) < 32)
        {
                vol = 0;
        } /* wait for room in register FIFO */

#else /* _STANDALONE */
        WAIT_IN_FIFO(32);       /* wait for room in register FIFO */
#endif /* _STANDALONE */
        
        *(registers + RDPIXELMASK_REG/4) = 0xFF;
                /* enable all pixel bits */

        if (p2->device_id == RAD1_2V_DEVICE_ID)
        {
                *(registers + RDINDEXCONTROL_2V_REG/4) =  RAD1_ENABLE;
                        /* enable indexed register address auto-increment */
                WR_INDEXED_REGISTER_2V(registers, RDCURSORCONTROL_2V_REG, 0);

                p2->p2_rdcursormode_2v.all = 0;
                p2->p2_rdcursormode_2v.bits.format = FORMAT_64X64_2V;
                p2->p2_rdcursormode_2v.bits.type = TYPE_X_2V;
                p2->p2_rdcursormode_2v.bits.cursorEnable = RAD1_DISABLE;
                WR_INDEXED_REGISTER_2V(registers, RDCURSORMODE_2V_REG,
                        p2->p2_rdcursormode_2v.all);

                p2->p2_rdcolorformat_2v.all = 0;
                p2->p2_rdcolorformat_2v.bits.RGB = RAD1_ENABLE;
                p2->p2_rdcolorformat_2v.bits.colorFormat = COLORFORMAT_8888_2V;
                WR_INDEXED_REGISTER_2V(registers, RDCOLORFORMAT_2V_REG,
                        p2->p2_rdcolorformat_2v.all);

                WR_INDEXED_REGISTER_2V(registers, RDPIXELSIZE_2V_REG,
                        PIXELSIZE_32BITS_2V);

                p2->p2_rddaccontrol_2v.all = 0;
                p2->p2_rddaccontrol_2v.bits.blankPedestal = RAD1_ENABLE;
                WR_INDEXED_REGISTER_2V(registers, RDDACCONTROL_2V_REG,
                        p2->p2_rddaccontrol_2v.all);

                p2->p2_rdmisccontrol_2v.all = 0;
                p2->p2_rdmisccontrol_2v.bits.highColorResolution = RAD1_ENABLE;
                p2->p2_rdmisccontrol_2v.bits.directColor = RAD1_ENABLE; /* */
                p2->p2_rdmisccontrol_2v.bits.overlay = RAD1_ENABLE;
                WR_INDEXED_REGISTER_2V(registers, RDMISCCONTROL_2V_REG,
                        p2->p2_rdmisccontrol_2v.all);

                WR_INDEXED_REGISTER_2V(registers, RDOVERLAYKEY_2V_REG, 0);
                WR_INDEXED_REGISTER_2V(registers, RDPAN_2V_REG, 0);
        }
        else if (p2->device_id == RAD1_DEVICE_ID)
        {
                p2->p2_rdcursorcontrol.all = 0; /* cursor off */
                p2->p2_rdcursorcontrol.bits.cursorSize = CURSORSIZE_64X64;
                WR_INDEXED_REGISTER(registers, RDCURSORCONTROL_REG, 
                        p2->p2_rdcursorcontrol.all);

                p2->p2_rdcolormode.all = 0;
                p2->p2_rdcolormode.bits.pixelFormat = PIXFMT_RGBA8888;
                p2->p2_rdcolormode.bits.gui = RAD1_ENABLE;      /*RAD1_DISABLE;  */
                p2->p2_rdcolormode.bits.RGB = RAD1_ENABLE;
                p2->p2_rdcolormode.bits.trueColor = RAD1_ENABLE;
                WR_INDEXED_REGISTER(registers, RDCOLORMODE_REG, 
                        p2->p2_rdcolormode.all);

                *(registers + RDPALETTEWRITEADDRESS_REG/4) = RDMODECONTROL_REG;
                        /* write mode control register index */
                *(registers + RDINDEXEDDATA_REG/4) = 0;
        }

/*#define LOAD_LUT_LINEAR  */
#ifdef LOAD_LUT_LINEAR
        for (i = 0; i < MAX_LUT_ENTRIES; i++)
                p2->lut_shadow[i] = (i << 16) + (i << 8) + i;
#else /* LOAD_LUT_LINEAR */
        for (i = 0xFF, j = 0; i > 0; i /= 2)
        {
                p2->lut_shadow[j++] = 0;
                p2->lut_shadow[j++] = i * (65536);
                p2->lut_shadow[j++] = i * (256);
                p2->lut_shadow[j++] = i * (256 + 65536);
                p2->lut_shadow[j++] = i * (1);
                p2->lut_shadow[j++] = i * (1 + 65536);
                p2->lut_shadow[j++] = i * (1 + 256);
                p2->lut_shadow[j++] = i * (1 + 256 + 65536);
        }
        for (; j < 256; j++)
                p2->lut_shadow[j] = j * (1 + 256 + 65536);
#endif /* LOAD_LUT_LINEAR */
        rad1_load_lut(p2);
pon_puts("!leaving rad1_init_RAMDAC\n");
}

void
rad1_load_lut(per2_info *p2)
{
        int i;
        int *colors = &p2->lut_shadow[0];
        volatile int *registers = p2->registers;

#ifdef _STANDALONE
        while (*(registers + INFIFOSPACE_REG/4) < 2)
        {
                vol = 0;
        } /* wait for room in register FIFO */
#else /* _STANDALONE */
        WAIT_IN_FIFO(2);        /* wait for room in register FIFO */
#endif /* _STANDALONE */
        
        *(registers + RDPALETTEWRITEADDRESS_REG/4) = 0;
        *(registers + RDPALETTEREADADDRESS_REG/4) = 0;
                /* write palette index */

        for (i = 0; i < MAX_LUT_ENTRIES; i++)
        {
#ifdef _STANDALONE
                while (*(registers + INFIFOSPACE_REG/4) < 3)
        {
                vol = 0;
        } /* wait for room in register FIFO */
#else /* _STANDALONE */
                WAIT_IN_FIFO(3);        /* wait for room in register FIFO */
#endif /* _STANDALONE */

                *(registers + RDPALETTEDATA_REG/4) = (colors[i] >> 16) & 0xFF;
                *(registers + RDPALETTEDATA_REG/4) = (colors[i] >> 8) & 0xFF;
                *(registers + RDPALETTEDATA_REG/4) = colors[i] & 0xFF;
        }
}


