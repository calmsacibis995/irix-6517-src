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

#ifndef __MGRAS_MACRO_H__

#define	__MGRAS_MACRO_H__

#define	XBOWREG(ofst)	(*((volatile xbowreg_t *) PHYS_TO_COMPATK1(XBOW_BASE + (ofst))))

#define XB_REG_WR_32(offset, mask, value) {				\
	__paddr_t Addr;							\
	Addr = XBOWREG(offset);						\
	msg_printf(DBG, "XB_REG_WR_32: Addr 0x%x\n", Addr);		\
        XBOWREG(offset) =  value & (mask );				\
}

#define XB_REG_RD_32(offset, mask, value) {				\
	__paddr_t Addr;							\
	Addr = XBOWREG(offset);						\
	msg_printf(DBG, "XB_REG_RD_32: Addr 0x%x\n", Addr);             \
        value = XBOWREG(offset) &  (mask) ;				\
}

#define HQ_WR(reg, mask, value) {					\
	msg_printf(DBG, "Write: mgbase 0x%x; mask 0x%x; value 0x%x\n",\
		mgbase,  mask, value);				\
	reg = value & mask;						\
}

#define HQ_RD(reg, mask, value) {					\
	msg_printf(DBG, "Read: mgbase 0x%x; mask 0x%x;\n",	\
		mgbase, mask);					\
	value = (reg) & mask;						\
	msg_printf(DBG, "value 0x%x\n", value);				\
}


#define GET_PATHID(PathId){                                             \
        if (argc == 1) {                                                \
                PathId = 0;                                             \
        }else {                                                         \
                atohex(argv[1], (int*)&PathId);                         \
	}								\
}
#define mgras_vc3SetRam64(mgbase,data)  {             			\
	mgbase->vc3.ram64 = data;					\
}
#define mgras_vc3GetRam64(mgbase,data) {             			\
	data = mgbase->vc3.ram64;					\
}
#define mgras_cmapSetDiag(mgbase, CmapID, val)				\
{									\
	if (CmapID){ 							\
		mgbase->cmap1.reserved = val;				\
	} else { 							\
		 mgbase->cmap0.reserved = val;				\
	}								\
}
#define mgras_cmapGetDiag(mgbase, CmapID, val)				\
{									\
	if (CmapID){ 							\
		val = mgbase->cmap1.reserved ;				\
	} else { 							\
		 val = mgbase->cmap0.reserved ;				\
	}								\
}
#define mgras_dacGetAddrCmd(mgbase, addr, Rcv) 				\
{									\
	mgras_dacSetAddr(mgbase,addr);					\
	Rcv = mgbase->dac.cmd.bybyte.b1;				\
}
#define mgras_dacGetAddr(mgbase, Rcv)					\
{									\
	ushort_t _tmp;							\
	_tmp = mgbase->dac.addr.byshort;					\
	Rcv = ((_tmp << 8) & 0xff) | ((_tmp >>8) & 0xff);		\
}
#define mgras_dacGetMode(mgbase, Rcv)					\
{									\
	Rcv = mgbase->dac.mode;						\
}

#undef mgras_dacGetAddrCmd16
#undef mgras_dacGetAddr16
#undef mgras_dacSetCtrl
#undef mgras_dacGetCtrl
#undef mgras_dacGetAddrCmd

#define CONTINUITY_CHECK(Test, CmapId, Patrn, errors) \
	continuity_check(Test,CmapId,Patrn,errors)

/*
 * The addr field in the mgrashw structure is 16-bit always.(??)
 * So, boo boo
 */
#define mgras_dacGetAddrCmd16(mgbase, addr, Rcv) 			\
{									\
	mgras_dacSetAddr16(mgbase,addr);				\
	Rcv = mgbase->dac.cmd.bybyte.b1;				\
}

#define mgras_dacGetAddr16Cmd16(mgbase, addr, Rcv) 			\
{									\
	ushort_t _tmp, _low, _high;					\
	mgras_dacSetAddr16(mgbase,addr);				\
	_tmp = mgbase->dac.cmd.byshort;					\
	_low =  _tmp & 0xff ;						\
	_high = (_tmp >> 14) & 0x3 ;					\
	Rcv = ((_high << 8) & 0x300 ) | (_low & 0xff) ;			\
}

#define mgras_dacGetAddr16(base,address)                        \
        {                                                       \
        address = base->dac.addr.byshort;				\
        address = ((address << 8) & 0xff00) | ((address >> 8) & 0xff);	\
        }

#define mgras_dacSetCtrl(base,value) \
	(base->dac.cmd.bybyte.b1 = value)

#define mgras_dacGetCtrl(base,value) \
	(value = base->dac.cmd.bybyte.b1)

#define mgras_dacGetAddrCmd(mgbase, addr, Rcv)                 	\
{                                                              	\
        mgras_dacSetAddr16(mgbase,addr);                        \
        Rcv = mgbase->dac.cmd.bybyte.b1;                        \
}


#define WAIT_FOR_NOT_VBLANK() {		\
	ushort_t	lncount;	\
	do {				\
		mgras_vc3GetReg(mgbase,VC3_VERT_LINE_COUNT,lncount); \
	} while (lncount < 200);	\
}

#define WAIT_FOR_VBLANK() {		\
	ushort_t	lncount;	\
	do {				\
		mgras_vc3GetReg(mgbase,VC3_VERT_LINE_COUNT,lncount); \
	} while (lncount != FRAME_BOTTOM); \
}



#define	MGRAS_GFX_CHECK() 

#define MGRAS_GFX_SETUP(index,numge) \
	mgras_gfx_setup(index,numge)

#define Mgras_GetCmapRev(CmapID, RevReg){                               \
                if (CmapID)                                             \
                        RevReg = mgbase->cmap1.rev;                       \
                else                                                    \
                        RevReg = mgbase->cmap0.rev;                       \
        }

#define Mgras_WriteCmap(CmapID, Data){                                  \
                if (CmapID)                                             \
                        mgbase->cmap1.pal = (Data << 8);                \
                else                                                    \
                        mgbase->cmap0.pal = (Data << 8);                \
        }


#define mgras_xmapGetBufSelect(mgbase, Rcv)				\
{									\
	Rcv = mgbase->xmap.buf_sel;					\
}

#define mgras_xmapGetMainMode(mgbase, wid, Rcv)				\
{									\
	mgras_xmapSetAddr(mgbase,wid << 2);				\
	Rcv = mgbase->xmap.main_mode;					\
}

#define mgras_xmapGetOvlMode(mgbase, wid, Rcv)				\
{									\
	mgras_xmapSetAddr(mgbase,(wid & 0x1f) << 2);			\
	Rcv = mgbase->xmap.ovl_mode;					\
}


#define	mgras_video_toggle(OnOff) 

#define CORRUPT_DCB_BUS() {				\
        msg_printf(DBG, "Corrupting the DCB Bus...\n");	\
        mgras_dacSetRGB(mgbase, 0xc3, 0x3c, 0x5a);	\
}

#define CMAP_COMPARE(str,which,addr,exp,rcv,mask,errors) \
	cmap_compare(str,which,addr,exp,rcv,mask,&errors)

#define COMPARE(str,addr,exp,rcv,mask,errors) \
	compare(str,addr,exp,rcv,mask,&errors)

#define RE_IND_COMPARE(str, addr, exp, rcv, mask, errors)		\
	LUT_ADJUST();						\
	COMPARE(str, addr, exp, rcv,				\
		(green_texlut || blue_texlut || alpha_texlut) ? 0xff : mask, errors)

#define REPORT_PASS_OR_FAIL(Test, errors) \
	return report_passfail(Test,errors)

/* does not do a return */
#define REPORT_PASS_OR_FAIL_NR(Test, errors)\
	report_passfail(Test,errors)

#define RETURN_STATUS(errors) \
	return (errors ? -1 : 0)

/* convert RSS register address to the HQ address space */
#define HQ_RSS_SPACE(reg)\
	(reg << 2)

#define RSS_HQ_SPACE(reg)\
	(reg >> 2)

#ifdef DEBUG
#define HQ3_PIO_WR(offset, val, mask) {					\
	volatile __paddr_t Addr;					\
	Addr = (__paddr_t)mgbase ;					\
	Addr |= offset;							\
	*(volatile __uint32_t *)(Addr) = val & mask;				\
	msg_printf(VDBG, "PIO_WR: addr = 0x%x, mask = 0x%x,data = 0x%x\n", \
	Addr, mask, val);       \
}

#define HQ3_PIO_RD(offset, mask, actual) {				\
	volatile __paddr_t Addr;							\
	Addr = (__paddr_t)mgbase ;					\
	Addr |= offset;							\
	actual = (*(volatile __uint32_t *) Addr) & mask ;			\
	msg_printf(VDBG, "PIO_RD: addr = 0x%x, mask = 0x%x, rcv = 0x%x\n", Addr, mask, actual);       \
}

#if (_MIPS_SIM == _MIPS_SIM_ABI64)
#define HQ3_PIO_WR64(offset, hword, lword, mask) { 			\
	volatile __uint64_t Addr, val;						\
	Addr = (__uint64_t) mgbase;					\
	Addr |= offset;							\
	val = (__uint64_t) hword;					\
	val = val << 32;						\
	val |= lword;							\
	msg_printf(VDBG, "PIO_WR_DBL: Addr = 0x%llx, hword: 0x%x, lword: 0x%x\n", Addr, hword,lword);							\
	Addr = val;					\
}
#else
#define HQ3_PIO_WR64(offset, hword, lword, mask) {			\
	msg_printf(DBG, "mgbase 0x%x; offset 0x%x\n", mgbase, offset);	\
	ide_store_double(hword, lword, ((__uint32_t)mgbase|offset));	\
}
#endif
#else /* DEBUG */
#define HQ3_PIO_WR(offset, val, mask) {					\
	volatile __paddr_t Addr;							\
	Addr = (__paddr_t)mgbase ;					\
	Addr |= offset;							\
	*(volatile __uint32_t *)(Addr) = val & mask;				\
}
/*
	printf("PIO_RD: addr = 0x%x, mask = 0x%x, rcv = 0x%x\n", Addr, mask, actual);       \
*/
#define HQ3_PIO_RD(offset, mask, actual) {				\
	volatile __paddr_t Addr;							\
	Addr = (__paddr_t)mgbase ;					\
	Addr |= offset;							\
	actual = (*(volatile __uint32_t *) Addr) & mask ;			\
}
#if (_MIPS_SIM == _MIPS_SIM_ABI64)
#define HQ3_PIO_WR64(offset, hword, lword, mask) {			\
	*(volatile __uint64_t *)(((__uint64_t)mgbase)|offset) = 			\
		( (((__uint64_t)hword) << 32) | lword ); 		\
}
#else
#define HQ3_PIO_WR64(offset, hword, lword, mask) {			\
	ide_store_double(hword, lword, ((__uint32_t)mgbase|offset));	\
}
#endif
#endif /* DEBUG */

#define HQ3_PIO_WR64_RE_REAL(offset, hword, lword, mask) { 		\
	HQ3_PIO_WR64(offset, hword, lword, mask);			\
}

#define HQ3_POLL_WORD(exp, mask, offset) {		\
	__uint32_t val;					\
	do {						\
	     HQ3_PIO_RD(offset,mask, val); \
	} while (val != exp);	\
}


#define WAIT_TIL_SYNC_FIFO_EMPTY { 	\
	__uint32_t	timeout;	\
	timeout = 1000;			\
	while (timeout && 		\
	       (mgbase->busy_dma & (HQ_BUSY_HQ_SYNC_BIT | HQ_BUSY_HIF_BIT))) { \
		timeout--;		\
		DELAY(10);		\
	}				\
	if (timeout == 0) {		\
		msg_printf(ERR,"HQ3 Syncfifo timeout\n");	\
	}				\
}

/* Macro for GE_DIAG write data pointer BypAddresReg Data[31] = 0 */

#define GE11_DIAG_WR_PTR(ptr)  \
	HQ3_PIO_WR(ge11_diag_a_addr, ptr, 0x7fffffff)

/* Macro for GE_DIAG write data to location pointed by BypAddresReg,
   BypAddresReg = BypAddresReg +1 */

#define GE11_DIAG_WR_IND(val, mask) \
	HQ3_PIO_WR(ge11_diag_d_addr, val, mask)  


#define BUILD_CONTROL_COMMAND( stuff, convert, priv, sync, cmd, bytes ) \
	(((((__uint32_t)bytes)   & 0x00ff) <<  0) | \
	 ((((__uint32_t)cmd)     & 0x1fff) <<  8) | \
 	 ((((__uint32_t)sync)    & 0x0001) << 21) | \
	 ((((__uint32_t)0x1)    & 0x0001) << 22) | \
         ((((__uint32_t)convert) & 0x007f) << 23) | \
         ((((__uint32_t)stuff)   & 0x0003) << 29)   \
        )

#define BUILD_PIXEL_COMMAND( last, packed, ini_align, addr, bytes ) \
      (HQ3_PIX_CMD_WORD | \
	(((__uint32_t)(last)      &     0x1) << 26) | \
	(((__uint32_t)(packed)    &     0x1) << 25) | \
	(((__uint32_t)(ini_align) &     0x7) << 22) | \
	(((__uint32_t)(addr)      &     0x3) << 20) | \
	(((__uint32_t)(bytes)     & 0xfffff) <<  0)   \
       )
/* Find out how many RE4s exists. Returns a 'numRE4s'  __uint32_t */
/* Expects 'numRE4s' and 'status' to be define */

#define NUMBER_OF_RE4S() {\
	msg_printf(DBG, "numRE4s: %d\n", numRE4s); \
}

#define NUMBER_OF_TRAMS() { \
/* \
	HQ3_PIO_RD_RE((RSS_BASE + (HQ_RSS_SPACE(TEVERSION))), 0x7ff, status,  \
				CSIM_RSS_DEFAULT);\
	switch (status & NUMTRAM_BITS) {\
		case TEXMODE_1TRAM: numTRAMs = 1; break;\
		case TEXMODE_2TRAM: numTRAMs = 2; break;\
		case TEXMODE_4TRAM: numTRAMs = 4; \
		default: errors++; break;\
	}\
*/ \
	msg_printf(DBG, "numTRAMs: %d\n", numTRAMs); \
}

#define READ_RSS_REG(re4num, regnum, data, mask)\
	HQ3_PIO_RD_RE((RSS_BASE + (HQ_RSS_SPACE(regnum))), mask, actual, data)

#if HQ4
#define WRITE_RSS_REG_PRINTF(re4num, regnum, data, mask) {		\
	__write_rss_reg_printf(regnum, data, mask);			\
}
#define WRITE_RSS_REG_NOPRINTF(re4num, regnum, data, mask) {		\
	__write_rss_reg_noprintf(regnum, data, mask);			\
}
#else
#define WRITE_RSS_REG_PRINTF(re4num, regnum, data, mask) {	\
	msg_printf(DBG, "Reg: 0x%x, ", regnum);			\
	HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(regnum))), 	\
			data, mask);				\
}
#define WRITE_RSS_REG_NOPRINTF(re4num, regnum, data, mask)	\
	HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(regnum))), data, mask)
#endif

#ifdef DEBUG
#define WRITE_RSS_REG(re4num, regnum, data, mask)	\
	WRITE_RSS_REG_PRINTF(re4num, regnum, data, mask)
#else
#define WRITE_RSS_REG(re4num, regnum, data, mask)	\
	WRITE_RSS_REG_NOPRINTF(re4num, regnum, data, mask)
#endif

#define READ_RSS_REG_DBL(re4num, regnum, mask)				\
        HQ3_PIO_RD64_RE(((RSS_BASE + (HQ_RSS_SPACE(regnum))) - HQUC_ADDR), actual, 	\
		actual_lo, ~0x0)

#if HQ4
#define WRITE_RSS_REG_DBL(re4num, regnum, data_hi, data_lo, mask) {	\
	__write_rss_reg_dbl(regnum, data_hi, data_lo);			\
}
#else
#define WRITE_RSS_REG_DBL(re4num, regnum, data_hi, data_lo, mask)	\
        HQ3_PIO_WR64_RE((RSS_BASE + (HQ_RSS_SPACE(regnum))), \
			data_hi, data_lo, mask)
#endif

#define READ_RE_INDIRECT_REG(re4num, addr, mask, expected) {		\
	HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(DEVICE_ADDR))),addr, 	\
		THIRTYTWOBIT_MASK);					\
	HQ3_PIO_RD_RE((RSS_BASE + (HQ_RSS_SPACE(DEVICE_DATA))), mask, 	\
			actual, expected);				\
}

#define LUT_ADJUST() {							\
	if (green_texlut) {						\
		actual = ((actual & 0xff00) >> 8);			\
	}								\
	else if (blue_texlut) {						\
		actual = ((actual & 0xff0000) >> 16);			\
	}								\
	else if (alpha_texlut) {					\
		actual = ((actual & 0xff000000) >> 24);			\
	}								\
}

#define WRITE_RE_INDIRECT_REG(re4num, addr, data, mask)\
	HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(DEVICE_ADDR))), addr, \
		THIRTYTWOBIT_MASK);\
	HQ3_PIO_WR_RE((RSS_BASE + (HQ_RSS_SPACE(DEVICE_DATA))), data, mask);

#define HQ3_PIO_WR64_RE(offset, data_hi, data_lo, mask) \
	HQ3_PIO_WR64_RE_REAL((offset | 0x2000), data_hi, data_lo, mask)

#define HQ3_PIO_WR64_RE_EX(offset, data_hi, data_lo, mask) \
	HQ3_PIO_WR64_RE_REAL((offset | 0x3000), data_hi, data_lo, mask)

/* double reg load */
#define HQ3_PIO_WR64_RE_XT(offset, data_hi, data_lo, mask) \
	HQ3_PIO_WR64_RE_REAL((offset | 0x2800), data_hi, data_lo, mask)

#define WR_RSS_REG_DBL_MODE2(re4num, regnum, data_hi, data_lo, mask, ldex)
#define WRITE_RSS_REG_MODE2(re4num, regnum, data, mask, ldex)
#define HQ3_PIO_RD64_RE(offset,mask,dummy1,dummy2)

#if HQ4
#define	HQ3_PIO_WR_RE(offset, val, mask) {			\
	_hq3_pio_wr_re(offset, val, mask);			\
}
#define	HQ3_PIO_WR_RE_EX(offset, val, mask) {			\
	_hq3_pio_wr_re_ex(offset, val, mask);			\
}
#else
/* defines to be able to run both in ide and csim */
/* "OR" offset with 0x2000 to make it a rss write-address */
#define HQ3_PIO_WR_RE(offset, val, mask) \
        HQ3_PIO_WR((offset | 0x2000), val, mask)

/* load and execute */
#define HQ3_PIO_WR_RE_EX(offset, val, mask) \
        HQ3_PIO_WR((offset | 0x3000), val, mask)
#endif
#define HQ3_PIO_RD_RE(offset, mask, actual, exp) \
	if (hq3_pio_rd_re(offset,mask,&actual,exp,1)) {return(1);}

/* same as HQ3_PIO_RD_RE but does not check STATUS reg */
#define HQ3_PIO_RD_RE_NOSTATUS(offset, mask, actual, exp) \
	if (hq3_pio_rd_re(offset,mask,&actual,exp,0)) {return(1);}


#define __glMgrRE4ColorExt_diag(reg, val0, val1) {			\
	WRITE_RSS_REG(4, reg, (__int32_t)(val0 * 0xfff000),~0x0);\
	WRITE_RSS_REG(4, (reg+1), (__int32_t)(val1 * 0xfff000),~0x0);\
}

#define __glMgrRE4Index_diag(reg, val) {				\
	WRITE_RSS_REG(4, reg, (__int32_t)(val * 0x1000),~0x0); \
}

#define __glMgrRE4Position_diag(reg, val0, val1, single) {              \
	tmp0 = val0; tmp1 = val1;                                       \
	if (single) {                                                   \
	WRITE_RSS_REG(4, reg, *(__uint32_t *)&tmp0,~0x0);		\
	WRITE_RSS_REG(4, (reg+1), *(__uint32_t *)&tmp1,~0x0);		\
	}								\
	else {								\
	WRITE_RSS_REG_DBL(4, reg, *(__uint32_t *)&tmp0, *(__uint32_t *)&tmp1,~0x0);	\
	}								\
}

#define __glMgrRE4TriSlope_diag(reg, val) {				\
	longnum = (__int64_t)(val * 0x1000000);			\
	WRITE_RSS_REG(4, reg, ((__int32_t)(longnum >> 0x20)), ~0x0); \
	WRITE_RSS_REG(4, (reg+1), ((__int32_t)(longnum & 0xffffffff)), ~0x0); \
}


#define GET_RSS_NUM() {							\
	write_only = 0;							\
	nocompare = 0;							\
	global_stop_on_err = 1;						\
	pp_drb = 0x240;                                                 \
	argc--; argv++;							\
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {		\
                switch (argv[0][1]) {					\
                        case 'r':					\
                                if (argv[0][2]=='\0') {			\
                                        atob(&argv[1][0], (int*)&rssnum);\
                                        argc--; argv++;			\
                                }					\
                                else					\
                                        atob(&argv[0][2], (int*)&rssnum);\
                                rflag++; break;				\
                        case 'd':					\
                                do_crc = 1; break;			\
                        case 'c':					\
                                nocompare = 1; break;			\
                        case 'x':					\
                                stop_on_err = 0; 			\
                                global_stop_on_err = 0; break;		\
                        case 'w':					\
                                write_only = 1; break;			\
			case 'b':					\
                                if (argv[0][2]=='\0') {			\
                                        atob(&argv[1][0], (int*)&pp_drb);\
                                        argc--; argv++;			\
                                }					\
                                else					\
                                        atob(&argv[0][2], (int*)&pp_drb);\
                                break;					\
                        default: bad_arg++; break;			\
                }							\
                argc--; argv++;						\
        }								\
        if ((bad_arg) || (argc)) {					\
           msg_printf(SUM, "Usage: -d(optional crc check) -x (continue on error - default is to stop on error) -w (write only) -c(optional no compare)  -b (optional drb pointer)\n\n");		\
                return (0);						\
        }								\
        if (re_checkargs(rssnum, 0, rflag, 0))				\
                return (0);						\
}

#define WAIT_FOR_CFIFO(status,count,timeout) \
	(status = wait_for_cfifo(count,timeout))

#define hq_HQ_RE_WrReg_0(reg, nul, val) \
	WRITE_RSS_REG(4,reg, val, ~0x0)

#define hq_HQ_RE_WrReg_1(reg, val1, val2) {				\
	WRITE_RSS_REG(4, (reg & 0xff), val1, ~0x0);			\
	WRITE_RSS_REG(4,((reg & 0xff)+1), val2, ~0x0);			\
}

#if HQ4
#define hq_HQ_RE_WrReg_2(reg, val1, val2) \
	WRITE_RSS_REG_DBL(CONFIG_RE4_ALL, (reg & 0x1ff), val1, val2, ~0x0);
#else
#define hq_HQ_RE_WrReg_2(reg, val1, val2) \
	HQ3_PIO_WR64_RE_XT((RSS_BASE + HQ_RSS_SPACE(reg)), val1, val2, ~0x0)
#endif


/* For normal operation, wait for BFIFO to emtpy, then check CMAP0 status */
#define mgras_cmapFIFOWAIT_which(Which, base) {                                    			\
        switch(Which) {                                                                         	\
                case Cmap0:                                                                         	\
			mgras_BFIFOWAIT(base,HQ3_BFIFO_MAX);							\
			while (!(base->cmap0.status & 0x08));						\
                break;                                                                          	\
                case Cmap1:                                                                         	\
			mgras_BFIFOWAIT(base,HQ3_BFIFO_MAX);							\
			while (!(base->cmap1.status & 0x08));						\
                break;                                                                          	\
                case CmapAll:                                                                        	\
			mgras_BFIFOWAIT(base,HQ3_BFIFO_MAX);							\
			while (!(base->cmap0.status & 0x08));						\
			mgras_BFIFOWAIT(base,HQ3_BFIFO_MAX);							\
			while (!(base->cmap1.status & 0x08));						\
                break;                                                                          	\
		default:										\
			msg_printf(ERR, "Bad CMAP ID\n");						\
		break;											\
        }												\
}

#define mgras_cmapSetAddr_which(Which, base, reg) {							\
        switch(Which) {											\
                case Cmap0:										\
			base->cmap0.addr = ((reg << 8) & 0xff00) | ((reg >> 8) & 0xff); 		\
                break;                                                         				\
                case Cmap1:                                                                         	\
			base->cmap1.addr = ((reg << 8) & 0xff00) | ((reg >> 8) & 0xff); 		\
                break;                                                                          	\
                case CmapAll:                                                                        	\
			base->cmapall.addr = ((reg << 8) & 0xff00) | ((reg >> 8) & 0xff); 		\
                break;                                                                          	\
		default:										\
			msg_printf(ERR, "Bad CMAP ID\n");						\
		break;											\
        }                                                                                       	\
}

#define mgras_cmapSetRGB_which(base,Which,r,g,b) {                                                	\
	switch(Which) {											\
		case Cmap0:										\
			base->cmap0.pal = (r << 24 ) | ( g << 16 ) | (b << 8 );				\
		break;											\
		case Cmap1:										\
			base->cmap1.pal = (r << 24 ) | ( g << 16 ) | (b << 8 );				\
		break;											\
		case CmapAll:										\
			base->cmapall.pal = (r << 24 ) | ( g << 16 ) | (b << 8 );			\
		break;											\
		default:										\
			msg_printf(ERR, "Bad CMAP ID\n");						\
		break;											\
	}												\
}

#define time_delay(n)

#define	HQ3_DCBCTRLREG_TST(str, ctrlreg, writeval) {								\
	__uint32_t _tmp;											\
	__uint32_t mask;											\
	mask = 0x1ffff;												\
	writeval &= mask;											\
	ctrlreg = writeval ;											\
	msg_printf(DBG, "Wrote Addr 0x%x mgbase->%s\t with 0x%x\n" ,&ctrlreg, str, writeval);			\
	_tmp = 0xdead;												\
	_tmp = ctrlreg;												\
	_tmp &= mask;												\
	msg_printf(DBG, "READ Addrs 0x%x mgbase->%s\t with 0x%x\n" , &ctrlreg,str, _tmp);			\
	if (_tmp != writeval) {											\
		msg_printf(ERR, "HQ3 DCBCTRL Reg Addrs 0x%x %s  test failed exp 0x%x rcv 0x%x \n" ,&ctrlreg, str, writeval, _tmp); \
		++errors;											\
	}													\
}

#define POLL_HQCP_PC(n) {						\
	time_out = 0;							\
     	do {								\
	   HQ3_PIO_RD(HQPC_ADDR, HQ3_HQCP_MASK, current_pc);		\
	   time_out++;							\
    	} while (((current_pc - test_entry) != n) && (time_out < TIMEOUT) );\
	if (time_out == TIMEOUT) {					\
		msg_printf(ERR,"HQCP Polling Time-out\n");		\
		errors = -1;						\
		return (errors);					\
	}								\
}

#define	SET_RAM_SIZE(ram_size) {					\
		if (((mgbase->bdvers.bdvers1 >> 24)) == 0x21) {		\
		   ram_size = GE_UCODE_32RAM;			\
		   msg_printf(DBG, "32:GE_UCODE_RAM_SIZE 0x%x\n" ,ram_size);\
		} else{ 						\
		   ram_size = GE_UCODE_64RAM;				\
		   msg_printf(DBG, "64:GE_UCODE_RAM_SIZE 0x%x\n" ,ram_size);\
	        }							\
}

#define POLL_GE_READ_FLAG                                        	\
       do {                                             		\
          HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_FLAGS_MASK, current_status); \
       } while ((current_status & GE_READ_VAL) != GE_READ_VAL);


#define PRINT_DMA_PP_USAGE() {						\
	msg_printf(SUM, 						\
	 "Usage: -e[optional HQ Write Mode] -w[optional RE Write Mode] -d[optional HQ Read Mode] -r[optional RE Read Mode] -s[optional RSS number] -x[1|0] (optional stop-on-error, default=1) -y [optional # rows] -z [optional # columns] -t(optional write only) -a(optional read only) -c(optional don't compare)  -m (optional) [xstart] [ystart] -f (optional no overwrite) -u [user specified pattern] -8 (optional 8-bit mode) -p[optional dma pattern, default=0] -b [optional drb-ptr, default=0] -i (optional 2112x1488 screen)\n\
		 HQ_write       RE_write               HQ_read     RE_read\n\
		 -------------------------------------------------------------------\n\
		 DMA_OP (0)     DMA_BURST     (0)      DMA_OP(0)   DMA_BURST     (0)\n\
		 DMA_OP (0)     DMA_BURST     (0)      PIO_OP(1)   DMA_PIO       (1)\n\
		 PIO_OP (1)     DMA_BURST     (0)      DMA_OP(0)   DMA_BURST     (0)\n\
		 PIO_OP (1)     DMA_PIO       (1)      DMA_OP(0)   DMA_BURST     (0)\n\
		 PIO_OP (1)     DMA_BURST     (0)      PIO_OP(1)   DMA_PIO       (1)\n\
		 PIO_OP (1)     DMA_PIO       (1)      PIO_OP(1)   DMA_PIO       (1)\n");\
	msg_printf(SUM,"\nMemory tests: 0=Walk 0/1, 1=Page_Addr, 2=ffff0000, 3=pattern, 100= all patterns\n");\
}


#define PRINT_DMA_TRAM_USAGE() {					\
	msg_printf(SUM,							\
		"Usage: -e[HQ Write Mode] -r (to read TRAM revision) -p (optional dma-pio read mode) -s [optional # columns] -t [optional # rows] -w (write only) -d (read only) -n[optional RSS #] -x[1|0] (optional stop-on-error, default=1) -c (optional don't compare) -g [optional page end] -a [optional page start] -m [optional mem test #, default=0] -f [optional data pattern for writes]\n\
		 HQ_write\n\
		 --------\n\
		 DMA_OP (0)\n\
		 PIO_OP (1)\n");\
	msg_printf(SUM,"\nMemory tests: 0=Walk 0/1, 1=Page_Addr, 2=ffff0000, 3=pattern, 100= all patterns\n");\
}


#define	GE_INITITIALIZE()
#define	START_GIO_ARBITRATION()
#define STOP_GIO_ARBITRATION()
#define	SET_REBUS_SYNC()
#define	CLEAR_REBUS_SYNC()
#define	GFX_HQHOSTCALLBACK()
#define	DL_SETUP_SIM_MEMORY(ptr)
#define DL_SIM_MEMORY_DECL()
#define	DL_SETUP_DLIST_IN_SIM_MEMORY(addr, nwords, data)
#define IL_SIM_MEMORY_DECL()
#define IL_SETUP_SIM_MEMORY(dma_ptr, rss_ptr)
#define IL_WRITE_DATA_TO_SIM_MEMORY(dma_ptr, rss_ptr, data)
#define	IL_DESTROY_SIM_MEMORY()
#define	IL_READ_DATA_FROM_SIM_MEMORY()
#define	FormatterMode(value) {								\
	__uint32_t      _cmd;                                                           \
	_cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, HQ_FC_ADDR_PIXEL_FORMAT_MODE, 4);      \
	HQ3_PIO_WR(CFIFO1_ADDR, _cmd, ~0x0);                                 \
	HQ3_PIO_WR(CFIFO1_ADDR, (value), ~0x0);                              \
}

#define HQ3_POLL_DMA(exp, mask, offset, status) {			\
	volatile __uint32_t	_val, _t, levl, _x;				\
	_t = MGRAS_DMA_TIMEOUT_COUNT;					\
	status = 0x0;							\
	_x= 0x0;							\
	do {								\
	   HQ3_PIO_RD(offset, mask, _val);	\
	   while (_x < 30) {						\
		us_delay(100);						\
		_x++;							\
	   }								\
	   mgras_cfifoGetLevel(mgbase, levl);				\
	   _t--;							\
	} while ((_val != exp) && _t);					\
	if (!_t) {							\
	   msg_printf(SUM, "mgras DMA Timed Out!; cfifo level: %d\n", levl);			\
	   status = DMA_TIMED_OUT;					\
	}								\
}

#define	DMA_PP1_SELECT(base, rss_number, pp1_number) {		\
	__uint32_t pp1Select;					\
	pp1Select = (pp1_number << 3) | (rss_number << 2);	\
	if (pp1_number == DMA_PP1_AB) {				\
	   pp1Select |= 0x1; 					\
	}							\
	mgras_xmapSetPP1Select(base, pp1Select, 0);		\
}

#define CSIM_TEXLDDONE() { \
	cmd = BUILD_CONTROL_COMMAND(0, 0, 0, 0, (0x1800 + TEXSYNC), 4); \
	HQ3_PIO_WR(CFIFO1_ADDR, cmd, ~0x0); \
	HQ3_PIO_WR(CFIFO1_ADDR, TX_TEXLDDONE, ~0x0); \
}

#define GET_RE_REG_NAME(regnum)\
	strcpy(regname, "RE UNDEFINED");\
	for (i = 0; i < (sizeof(re4_rwreg)/sizeof(re4_rwreg[0])); i++) {\
                if (re4_rwreg[i].regNum == regnum) {\
                        strcpy(regname, re4_rwreg[i].str);\
			is_readable = re4_rwreg[i].readable;\
			mask = re4_rwreg[i].mask;\
                        break;\
                }\
        }

#define GET_TE_REG_NAME(regnum)\
	strcpy(regname, "TE UNDEFINED");\
	for (i = 0; i < (sizeof(te_rwreg)/sizeof(te_rwreg[0])); i++) {\
                if (te_rwreg[i].regNum == regnum) {\
                        strcpy(regname, te_rwreg[i].str);\
			is_readable = te_rwreg[i].readable;\
			mask = te_rwreg[i].mask;\
                        break;\
                }\
        }

#define GET_PP_REG_NAME(regnum)\
	strcpy(regname, "PP UNDEFINED");\
	for (i = 0; i < (sizeof(pp_rwreg)/sizeof(pp_rwreg[0])); i++) {\
                if (pp_rwreg[i].regNum == regnum) {\
                        strcpy(regname, pp_rwreg[i].str);\
			is_readable = pp_rwreg[i].readable;\
			mask = pp_rwreg[i].mask;\
                        break;\
                }\
        }

#define ALIVE_BAR(counter, timeout_val) {			\
	if ((counter & timeout_val) == timeout_val){ 		\
	   switch (j) {						\
		case 0: msg_printf(INFO, "| \r"); j++; break;	\
		case 1: msg_printf(INFO, "/ \r"); j++; break;	\
		case 2: msg_printf(INFO, "- \r"); j++; break;	\
		case 3: msg_printf(INFO, "\\ \r"); j=0; break;	\
	   }							\
	   counter = 0;						\
	}							\
}

#define __GL_MGRAS_FAST_FILL_MASK	(__GL_SHADE_SMOOTH	| \
					 __GL_SHADE_DEPTH_TEST	| \
					 __GL_SHADE_TEXTURE	| \
					 __GL_SHADE_STIPPLE)

#define PARSE_TRI_ARGS_C(r, g, b, a) {	\
	atob(&argv[1][0], (int*)&r);		\
	argc--; argv++;			\
	atob(&argv[1][0], (int*)&g);		\
	argc--; argv++;			\
	atob(&argv[1][0], (int*)&b);		\
	argc--; argv++;			\
	atob(&argv[1][0], (int*)&a);		\
	argc--; argv++;			\
}

#define PARSE_TRI_ARGS(x, y, z, r, g, b, a) {	\
	atob(&argv[1][0], (int*)&x);			\
	argc--; argv++;				\
	atob(&argv[1][0], (int*)&y);			\
	argc--; argv++;				\
	atob(&argv[1][0], (int*)&z);			\
	argc--; argv++;				\
	PARSE_TRI_ARGS_C(r, g, b, a);		\
}

#define PARSE_VERT_ARGS_C(r, g, b, a) {  \
        atob(&argv[1][0], (int*)&r);          \
        argc--; argv++;                 \
        atob(&argv[1][0], (int*)&g);          \
        argc--; argv++;                 \
        atob(&argv[1][0], (int*)&b);          \
        argc--; argv++;                 \
        atob(&argv[1][0], (int*)&a);          \
        argc--; argv++;                 \
}

#define PARSE_VERT_ARGS(x, y, z, r, g, b, a) {   \
        atob(&argv[1][0], (int*)&x);                  \
        argc--; argv++;                         \
        atob(&argv[1][0], (int*)&y);                  \
        argc--; argv++;                         \
        atob(&argv[1][0], (int*)&z);                  \
        argc--; argv++;                         \
        PARSE_VERT_ARGS_C(r, g, b, a);           \
}

/* Description:
** The twelve framebuffer config are derived from
** {L,LA,RGB,RGBA}:{12,8,4} component:bit combinations.
**
** An image generation function exists for each of these
** combinations which fills the image buffer with grey ramps.
**
** The memory allocation of the void *image buffer is assumed
** to be as follows (in bytes):
**
** L   : 12 -  X * Y * 2
**     :  8 -  X * Y 
**     :  4 -  X * Y / 2
**
** LA  : 12 -  X * Y * 4
**     :  8 -  X * Y * 2
**     :  4 -  X * Y
**		    
** RGB : 12 -  X * Y * 6
**     :  8 -  X * Y * 3
**     :  4 -  X * Y * 2
**		    
** RGBA: 12 -  X * Y * 8
**     :  8 -  X * Y * 4
**     :  4 -  X * Y * 2
**
*/

#define memcpy32(outputbuf, inputbuf, ysize, xsize, bytes_per_tex, pad) {\
	__uint32_t xctr, yctr, in_inc, out_inc, xmax;			\
	xmax = (xsize * bytes_per_tex)/4; /* xmax is size in words */	\
	in_inc = out_inc = 0;						\
	for (yctr = 0; yctr < ysize; yctr++) { 				\
	   for (xctr = 0; xctr < xmax; xctr++) 				\
		*(outputbuf + out_inc++ ) = *(inputbuf + in_inc++);	\
	   for (xctr = 0; xctr < (pad/4); xctr++) 			\
		*(outputbuf + out_inc++ ) = 0xdeadbeef;			\
	}								\
}

#define	PAGE_MASK(x) (((x) >> PAGE_BIT_SIZE) << PAGE_BIT_SIZE) 
	
#endif /* __MGRAS_MACRO_H__ */
