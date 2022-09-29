/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef MGRAS_SIM_H

#define	MGRAS_SIM_H

extern unsigned int   curAddr, mgbaseAddr, addrOffset;
extern void *srcAddr;
extern unsigned int hregsBase;

#include "hq3test.h"

/*
 * HQ3_PIO Macros
 */
#undef HQ3_PIO_WR
#undef HQ3_PIO_RD
#undef WAIT_FOR_VBLANK
#undef HQ3_PIO_RD_RE
#undef HQ3_PIO_WR_RE
#undef MGRAS_GFX_CHECK

/*
 * offset 	address of the register
 * val		value to be written
 * mask		32-bit mask
 * module	HQ_MOD, RE_MOD or BE_MOD
 * startByte	0 or 4 depending on the address
 */
#define	HQ3_PIO_WR(offset, val, mask, module, startByte) {			\
	__uint32_t	loff, lval;						\
	lval = (val & mask);							\
	switch (module) {							\
	  case HQ_MOD:								\
	  case RE_MOD:								\
		loff = offset - HQUC_ADDR;					\
		break;								\
	  case BE_MOD:								\
		loff = offset;							\
		break;								\
	  default:								\
		msg_printf(ERR, "Incorrect module");				\
		break;								\
	}									\
	HostHqWriteWord(lval, startByte, (hregsBase + loff));			\
}

/*
 * offset 	address of the register
 * mask		32-bit mask (used only if mode == READ_MODE)
 * mode		READ_MODE or READ_VFY_MODE
 * actual	value to be read
 * expected	used only if mode == READ_VFY_MODE
 * module	HQ_MOD, RE_MOD or BE_MOD
 */
#define HQ3_PIO_RD(offset, mask, mode, actual, expected, module) {		\
	__uint32_t	loff;							\
	switch (module) {							\
	  case HQ_MOD:								\
	  case RE_MOD:								\
		loff = offset - HQUC_ADDR;					\
		break;								\
	  case BE_MOD:								\
		loff = offset;							\
		break;								\
	  default:								\
		msg_printf(ERR, "Incorrect module");				\
		break;								\
	}									\
	if (mode == READ_MODE) {						\
	    HostHqReadWord(&actual, (hregsBase + loff));			\
	    actual &= mask;							\
	} else { /* mode == READ_VFY_MODE */					\
	    HostHqReadVfyWord(expected, (hregsBase + loff));			\
	}									\
}

#define	WAIT_FOR_VBLANK()	

#define HQ3_PIO_RD64(offset, mask, hword, lword) \
        HostHqReadVfyDouble(hword, lword, (hregsBase + offset));
#define HQ3_PIO_WR64(offset, hword, lword, mask) \
        HostHqWriteDouble(hword, lword, srcAddr, (hregsBase + offset));

#define HQ3_PIO_WR64_RE_REAL(offset, hword, lword, mask) \
	HostHqWriteDouble(hword, lword, srcAddr, (hregsBase + offset));

/* stuff for HQ3 sim with wrappers */
#ifdef MGRAS_DIAG_SIM_HQSIM

#define HQ3_PIO_WR_RE(offset,val, mask)  			\
	HQ3_PIO_WR(offset,val, mask, RE_MOD, 0x0); 
#define HQ3_PIO_RD_RE(offset, mask, actual, expected) 		\
	HostHqReadVfyWord(expected, (hregsBase + (offset - HQUC_ADDR)));

#else

#define HQ3_PIO_WR_RE(offset,val, mask)  			\
	HQ3_PIO_WR((offset | 0x2000),val, mask, RE_MOD, 0x0); 
#define HQ3_PIO_RD_RE(offset, mask, actual, expected) 		\
	HostHqReadWord(&actual, (hregsBase + (offset - HQUC_ADDR)));

#endif

#if 1
#define MGRAS_GFX_CHECK()
#else
#define MGRAS_GFX_CHECK()							\
{										\
	__uint32_t      gio_status;						\
	HQ3_PIO_RD(GIO_CONFIG_ADDR, ~0x0, READ_MODE, gio_status, 0x0, HQ_MOD);	\
	if ( ((gio_status & HQ3_VERSION_MASK) != HQ3_VERSION) ||		\
	     ((gio_status & HQ3_ID_MASK) != HQ3_ID) ) {				\
		msg_printf(ERR, "MGRAS Graphics Probe Failure...\n");   	\
	}									\
}
#endif

/*
 * TEMPORARY Macros
 */
#undef us_delay
#undef atohex
#define us_delay()
#define atohex()

#undef mgras_BFIFOWAIT
#undef mgras_vc3SetIndex 
#undef mgras_vc3SetReg
#undef mgras_vc3GetReg
#undef mgras_vc3SetRam
#undef mgras_vc3GetRam

#undef mgras_vc3SetRam64
#undef mgras_vc3GetRam64

/*
 * VC3 Macros
 */
#define	mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX)

#define mgras_vc3SetIndex(mgbase, addr) {					\
	curAddr = (unsigned int)&(mgbase->vc3.index);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_WR(addrOffset, (addr << 24), ~0x0, BE_MOD, 0);		\
}

#define mgras_vc3SetCfgReg(mgbase, data16) {				\
	curAddr = (unsigned int)&(mgbase->vc3.indexdata);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_WR(addrOffset, ((0x1f << 24) | 				\
		((data16 & 0xffff) << 8)), ~0x0, BE_MOD, 0);		\
}

#define mgras_vc3SetReg(mgbase, reg, data16) {				\
	curAddr = (unsigned int)&(mgbase->vc3.indexdata);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_WR(addrOffset, ((reg << 24) | 				\
		((data16 & 0xffff) << 8)), ~0x0, BE_MOD, 0);		\
}

#define mgras_vc3GetReg(mgbase, reg, dummy, data16) {                     \
	mgras_vc3SetIndex(mgbase, reg);					\
	curAddr = (unsigned int)&(mgbase->vc3.data);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_RD(addrOffset, ~0x0, READ_VFY_MODE, 			\
		dummy, data16, BE_MOD);					\
}

#ifdef ENDIAN_BOBO
#define mgras_vc3SetRam(mgbase, data) {					\
	__uint32_t dataHi, dataLo;					\
	dataHi = (data >> 8) & 0xff;					\
	dataLo = data & 0xff;						\
	curAddr = (unsigned int)&(mgbase->vc3.ram);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_WR(addrOffset, ((dataLo << 24) | (dataHi << 16)), ~0x0, BE_MOD, 0);		\
}

#define mgras_vc3GetRam(mgbase, dummy, data) {				\
	__uint32_t dataHi, dataLo;					\
	dataHi = (data >> 8) & 0xff;					\
	dataLo = data & 0xff;						\
	curAddr = (unsigned int)&(mgbase->vc3.ram);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_RD(addrOffset, ~0x0, READ_VFY_MODE,			\
		dummy, ((dataLo << 24) | (dataHi << 16)), BE_MOD);				\
}
#else
#define mgras_vc3SetRam(mgbase, data) {                                   \
        curAddr = (unsigned int)&(mgbase->vc3.ram);                       \
        mgbase = (unsigned int)mgbase;                                  \
        addrOffset = curAddr - mgbaseAddr;                                \
        HQ3_PIO_WR(addrOffset, (data<<16), ~0x0, BE_MOD, 0);            \
}

#define mgras_vc3GetRam(mgbase, dummy, data) {                            \
        curAddr = (unsigned int)&(mgbase->vc3.ram);                       \
        mgbaseAddr = (unsigned int)mgbase;                                  \
        addrOffset = curAddr - mgbaseAddr;                                \
        HQ3_PIO_RD(addrOffset, ~0x0, READ_VFY_MODE,                     \
                dummy, (data<<16), BE_MOD);                             \
}
#endif

#define mgras_vc3SetRam64(mgbase, data) {					\
	unsigned int hiWord, loWord;					\
	curAddr = (unsigned int)&(mgbase->vc3.ram64);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	hiWord = (data >> 32);						\
	loWord = data;							\
	HQ3_PIO_WR64(addrOffset, hiWord, loWord, 0);			\
}

#define mgras_vc3GetRam64(mgbase, dummy, data) {				\
	curAddr = (unsigned int)&(mgbase->vc3.ram64);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_RD64(addrOffset, 0, data, (data << 32));		\
}

/*
/*
 * CMAP  macros
 */
/* For normal operation, wait for BFIFO to emtpy, then check CMAP0 status */
#undef mgras_cmapFIFOWAIT
#undef mgras_cmapSetAddr
#undef mgras_cmapSetAddr
#undef mgras_cmapSetRGB
#undef mgras_cmapSetCmd
#undef mgras_cmapGetCmd
#undef mgras_cmapGetStatus
#undef mgras_cmapGetRev
#undef mgras_cmapGetRGB
#undef Mgras_Get_Cmap_Rev
#undef Mgras_Write_Cmap
#undef mgras_cmapSetDiag
#undef mgras_cmapGetDiag

#define VRB 	0
#define ERR	1
#define SUM	2

#ifdef msg_printf		/* in case we are redefining it */
#undef msg_printf
#endif
#define msg_printf(token,Comment)					\
{									\
	HostHqComment(Comment);						\
}

#define mgras_cmapFIFOWAIT(mgbase)                                      

#define mgras_cmapSetAddr(mgbase, address) {				\
	__uint32_t dataHi, dataLo;					\
	dataHi = (address >> 8) & 0xff;					\
	dataLo = address & 0xff;						\
	curAddr = (unsigned int)&(mgbase->cmapall.addr);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_WR(addrOffset, ((dataLo << 24) | (dataHi << 16)), ~0x0, BE_MOD, 0);		\
}

#define mgras_cmapSetRGB(mgbase,r,g,b) {					\
	curAddr = (unsigned int)&(mgbase->cmapall.pal);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_WR(addrOffset, ((r << 24) | (g << 16) | (b << 8)), 	\
		~0x0, BE_MOD, 0);					\
}

#define mgras_cmapSetCmd(mgbase, d) {					\
	curAddr = (unsigned int)&(mgbase->cmapall.cmd);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_WR(addrOffset, d, ~0x0, BE_MOD, 0);			\
}

#define mgras_cmapGetRGB(mgbase, which, r, g, b , expected) {		\
	if (which) {							\
		curAddr = (unsigned int)&(mgbase->cmap1.pal);		\
	} else {							\
		curAddr = (unsigned int)&(mgbase->cmap0.pal);		\
	}								\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_RD(addrOffset, ~0x0, READ_VFY_MODE, r, 			\
		expected, BE_MOD);					\
}

#define Mgras_GetCmapRev(CmapID, RevReg, Exp){                          \
        if (CmapID){                                            	\
		curAddr = (unsigned int)&(mgbase->cmap1.rev);     	\
        }else {                                                 	\
		curAddr = (unsigned int)&(mgbase->cmap0.rev);     	\
	}								\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_RD(addrOffset, ~0x0, READ_VFY_MODE, RevReg,             \
		Exp, BE_MOD);						\
        }

#define mgras_cmapSetDiag(mgbase, CmapID, val)                            \
{									\
        if (CmapID){                                            	\
		curAddr = (unsigned int)&(mgbase->cmap1.reserved);     	\
        }else {                                                 	\
		curAddr = (unsigned int)&(mgbase->cmap0.reserved);     	\
	}								\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_WR(addrOffset, val, ~0x0, BE_MOD, 0);			\
}

#define mgras_cmapGetDiag(mgbase, CmapID, val)                            \
{									\
        if (CmapID){                                            	\
		curAddr = (unsigned int)&(mgbase->cmap1.reserved);     	\
        }else {                                                 	\
		curAddr = (unsigned int)&(mgbase->cmap0.reserved);     	\
	}								\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_RD(addrOffset, ~0x0, READ_VFY_MODE, val, val,BE_MOD);	\
}
#define Mgras_WriteCmap(CmapID, Data){                                  \
        if (CmapID)                                             	\
		curAddr = (unsigned int)&(mgbase->cmap1.pal);		\
        else                                                    	\
		curAddr = (unsigned int)&(mgbase->cmap0.pal);		\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_WR(addrOffset, Data, ~0x0, BE_MOD, 0);			\
        }




#undef	mgras_xmapSetPP1Select
#undef	mgras_xmapSetConfig
#undef	mgras_xmapGetConfig
#undef	mgras_xmapSetAddr
#undef	mgras_xmapSetBufSelect
#undef	mgras_xmapSetMainMode
#undef	mgras_xmapSetOvlMode
#undef	mgras_xmapSetDIBdata
#undef	mgras_xmapSetDIBdataDW
#undef	mgras_xmapSetRE_RAC
#undef 	mgras_xmapGetDIBdata
#undef 	mgras_xmapGetBufSelect
#undef 	mgras_xmapGetMainMode
#undef 	mgras_xmapGetOvlMode
#undef 	mgras_xmapGetRE_RAC

/*
 * XMAP Macros
 */
#define mgras_xmapSetPP1Select(mgbase, data) {				\
	curAddr = (unsigned int)&(mgbase->xmap.pp1_sel);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_WR(addrOffset, (data << 24), ~0x0, BE_MOD, 0);		\
}

#define mgras_xmapSetConfig(mgbase, data) {				\
	curAddr = (unsigned int)&(mgbase->xmap.config);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_WR(addrOffset, data, ~0x0, BE_MOD, 0);			\
}

#define mgras_xmapSetAddr(mgbase, addr) {					\
	curAddr = (unsigned int)&(mgbase->xmap.index);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_WR(addrOffset, (addr << 24), ~0x0, BE_MOD, 0);		\
}

#define mgras_xmapSetBufSelect(mgbase, data) {				\
	curAddr = (unsigned int)&(mgbase->xmap.buf_sel);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_WR(addrOffset, data, ~0x0, BE_MOD, 0);			\
}

#define mgras_xmapSetMainMode(mgbase, wid, data) {			\
	mgras_xmapSetAddr(mgbase, (wid << 2));				\
	curAddr = (unsigned int)&(mgbase->xmap.main_mode);		\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_WR(addrOffset, data, ~0x0, BE_MOD, 0);			\
}

#define mgras_xmapSetOvlMode(mgbase, wid, data) {				\
	mgras_xmapSetAddr(mgbase, ((wid & 0x1f) << 2));			\
	curAddr = (unsigned int)&(mgbase->xmap.ovl_mode);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_WR(addrOffset, data, ~0x0, BE_MOD, 0);			\
}

#define mgras_xmapSetDIBdata(mgbase, data) {				\
	curAddr = (unsigned int)&(mgbase->xmap.dib);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_WR(addrOffset, data, ~0x0, BE_MOD, 0);			\
}

#define mgras_xmapSetDIBdataDW(mgbase, data) {				\
	curAddr = (unsigned int)&(mgbase->xmap.dib_dw);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_WR(addrOffset, data, ~0x0, BE_MOD, 0);			\
}

#define mgras_xmapSetRE_RAC(mgbase, data) {				\
	curAddr = (unsigned int)&(mgbase->xmap.re_rac);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_WR(addrOffset, data, ~0x0, BE_MOD, 0);			\
}

#define mgras_xmapGetConfig(mgbase, dummy, exp) {				\
	curAddr = (unsigned int)&(mgbase->xmap.config);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_RD(addrOffset, ~0x0, READ_VFY_MODE, 			\
		dummy, exp, BE_MOD);					\
}

#define mgras_xmapGetDIBdata(mgbase, dummy, exp) {			\
	curAddr = (unsigned int)&(mgbase->xmap.dib);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_RD(addrOffset, ~0x0, READ_VFY_MODE, 			\
		dummy, exp, BE_MOD);					\
}

#define mgras_xmapGetBufSelect(mgbase, dummy, exp) {			\
	curAddr = (unsigned int)&(mgbase->xmap.buf_sel);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_RD(addrOffset, ~0x0, READ_VFY_MODE, 			\
		dummy, exp, BE_MOD);					\
}

#define mgras_xmapGetMainMode(mgbase, wid, dummy, exp) {			\
	mgras_xmapSetAddr(mgbase, (wid << 2));				\
	curAddr = (unsigned int)&(mgbase->xmap.main_mode);		\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_RD(addrOffset, ~0x0, READ_VFY_MODE, 			\
		dummy, exp, BE_MOD);					\
}

#define mgras_xmapGetOvlMode(mgbase, wid, dummy, exp) {			\
	mgras_xmapSetAddr(mgbase, ((wid & 0x1f) << 2));			\
	curAddr = (unsigned int)&(mgbase->xmap.ovl_mode);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_RD(addrOffset, ~0x0, READ_VFY_MODE, 			\
		dummy, exp, BE_MOD);					\
}

#define mgras_xmapGetRE_RAC(mgbase, dummy, exp) {				\
	curAddr = (unsigned int)&(mgbase->xmap.re_rac);			\
	mgbaseAddr = (unsigned int)mgbase;					\
	addrOffset = curAddr - mgbaseAddr;				\
	HQ3_PIO_RD(addrOffset, ~0x0, READ_VFY_MODE, 			\
		dummy, exp, BE_MOD);					\
}

#define mgras_HQ3Init(mgbase){                                            \
        HostHqComment("HQ Initialize ");                      		\
        HostHqWriteWord(0xf, srcAddr, &(h->gio_config)); 		\
        HostHqWriteWord(0x8, srcAddr, &(h->bfifo_hw));  		\
        HostHqWriteWord(0x2, srcAddr, &(h->bfifo_lw));  		\
        HostHqWriteWord(0x100, srcAddr, &(h->bfifo_delay));     	\
        HostHqWriteWord(0x30, srcAddr, &(h->cfifo_hw)); 		\
        HostHqWriteWord(0x2, srcAddr, &(h->cfifo_lw));  		\
        HostHqWriteWord(0xffffffff, srcAddr, &(h->cfifo_delay));        \
        HostHqWriteWord(0x30, srcAddr, &(h->dfifo_hw)); 		\
        HostHqWriteWord(0x2, srcAddr, &(h->dfifo_lw));  		\
        HostHqWriteWord(0xffffffff, srcAddr, &(h->dfifo_delay));        \
        HostHqWriteWord(0x44000000, srcAddr, &(h->intr_enab_set));      \
        HostHqWriteWord(0xbbffffff, srcAddr, &(h->intr_enab_clr));      \
        HostHqWriteWord(0x000001c8, srcAddr, &(h->hq_config));          \
        HostHqWriteWord(0x4f, srcAddr, &(h->gio_config));               \
        HostHqWriteWord((0xf | HQ3_VERSION | HQ3_ID), srcAddr, &(h->gio_config));                \
        HostHqWriteWord(0x000001c0, srcAddr, &(h->hq_config));          \
        HostHqComment("HQ Initialize Completed");                       \
}



#define mgras_DCBinit(mgbase) {						\
	HostHqComment("DCB Initialize");				\
        mgbaseAddr = (unsigned int)mgbase;                                  \
									\
        curAddr = (unsigned int)&(mgbase->dcbctrl_pp1);                   \
        addrOffset = curAddr - mgbaseAddr;                                \
	HQ3_PIO_WR(addrOffset, 0x2104, ~0x0, BE_MOD, 0);		\
									\
        curAddr = (unsigned int)&(mgbase->dcbctrl_dac);                   \
        addrOffset = curAddr - mgbaseAddr;                                \
	HQ3_PIO_WR(addrOffset, 0x1088, ~0x0, BE_MOD, 0);		\
									\
        curAddr = (unsigned int)&(mgbase->dcbctrl_cmapall);               \
        addrOffset = curAddr - mgbaseAddr;                                \
	HQ3_PIO_WR(addrOffset, 0x1088, ~0x0, BE_MOD, 0);		\
									\
        curAddr = (unsigned int)&(mgbase->dcbctrl_cmap0);                 \
        addrOffset = curAddr - mgbaseAddr;                                \
	HQ3_PIO_WR(addrOffset, 0x1088, ~0x0, BE_MOD, 0);		\
									\
        curAddr = (unsigned int)&(mgbase->dcbctrl_cmap1);                	\
        addrOffset = curAddr - mgbaseAddr;                                \
	HQ3_PIO_WR(addrOffset, 0x1088, ~0x0, BE_MOD, 0);		\
									\
        curAddr = (unsigned int)&(mgbase->dcbctrl_vc3);                   \
        addrOffset = curAddr - mgbaseAddr;                                \
	HQ3_PIO_WR(addrOffset, 0x1002, ~0x0, BE_MOD, 0);		\
									\
        curAddr = (unsigned int)&(mgbase->dcbctrl_bdvers);                \
        addrOffset = curAddr - mgbaseAddr;                                \
	HQ3_PIO_WR(addrOffset, MGRAS_DCBCTRL_BDVERS, ~0x0, BE_MOD, 0);	\
	mgras_HQ3Init(mgbase);						\
	mgras_vc3SetCfgReg(mgbase, 0x1);					\
	HostHqComment("VC3 Config register Initialized");		\
	HostHqComment("Write ALL PP1's ");				\
	curAddr = (unsigned int)&(mgbase->xmap.pp1_sel);			\
	addrOffset = curAddr - mgbaseAddr;                                \
	HQ3_PIO_WR(addrOffset, (0x1 <<24), ~0x0, BE_MOD, 0);          	\
	_pp1_xmap_init();						\
	_mgras_VC3Init(CURS_TIMING_TABLE);				\
	mgras_VC3CursorMode(GLYPH_32);					\
	HostHqComment("Basic Initialize Completed");			\
									\
}

#endif /* MGRAS_SIM_H */
