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


#include "mgras_diag.h"

#include "sys/sbd.h"
#include "sys/cpu.h"
#include "mgras_xbow.h"

__uint64_t  exp_64 ,  recv_64 ;
__uint32_t  exp_32 ,  recv_32 ;

__uint32_t flag_1 , flag_0  ;

#if HQ4

_mg_xbow_portinfo_t mg_xbow_portinfo_t[6];

#define MGBASE_PTR_TO_OFFET 0

hq4reg hq4regs_t[] = {
{"WIDGET_INDENT_REG", MGBASE_PTR_TO_OFFET, _RDONLY_, THIRTYTWOBIT_MASK, 
                                                WIDGET_INDENT_DEF},
{"WIDGET_STAT_REG", MGBASE_PTR_TO_OFFET, _RDONLY_, THIRTYTWOBIT_MASK, 
                                                WIDGET_STAT_DEF},
{"WIDGET_ERRHI_REG", MGBASE_PTR_TO_OFFET,  _RDONLY_, SIXTEENBIT_MASK, 
                                                WIDGET_ERRHI_DEF},
{"WIDGET_ERRLO_REG", MGBASE_PTR_TO_OFFET,  _RDONLY_, THIRTYTWOBIT_MASK, 
                                                WIDGET_ERRLO_DEF},
{"WIDGET_CNTL_REG", MGBASE_PTR_TO_OFFET,  _RDWR_, THIRTYTWOBIT_MASK, 
                                                WIDGET_CNTL_DEF},
{"WIDGET_REQ_TIMEOUT_REG", MGBASE_PTR_TO_OFFET,  _RDWR_, THIRTYTWOBIT_MASK, 
                                                WIDGET_REQ_TIMEOUT_DEF},
{"WIDGET_INTR_DST_HI_REG", MGBASE_PTR_TO_OFFET,  _RDWR_, THIRTYTWOBIT_MASK, 
                                                WIDGET_INTR_DST_HI_DEF},
{"WIDGET_INTR_DST_LO_REG", MGBASE_PTR_TO_OFFET,  _RDWR_, THIRTYTWOBIT_MASK, 
                                                WIDGET_INTR_DST_LO_DEF},
{"WIDGET_ERR_CMD_REG", MGBASE_PTR_TO_OFFET,  _RDWR_, THIRTYTWOBIT_MASK, 
                                                WIDGET_ERR_CMD_DEF},
{"WIDGET_LLP_CONFIG_REG", MGBASE_PTR_TO_OFFET,  _RDWR_, THIRTYTWOBIT_MASK, 
                                                WIDGET_LLP_CONFIG_DEF},
{"WIDGET_TARGET_FLUSH_REG", MGBASE_PTR_TO_OFFET,  _RDONLY_, THIRTYTWOBIT_MASK, 
                                                WIDGET_TARGET_FLUSH_DEF},
{"", -1, 0, -1}
};

void
_mgras_hq4regs_t_inint(void)
{
hq4regs_t[0].address = (__paddr_t)&(mgbase->xt_widget_id);
hq4regs_t[1].address = (__paddr_t)&(mgbase->xt_status);
hq4regs_t[2].address = (__paddr_t)&(mgbase->xt_error_addr_hi);
hq4regs_t[3].address = (__paddr_t)&(mgbase->xt_error_addr_lo);
hq4regs_t[4].address = (__paddr_t)&(mgbase->xt_control);
hq4regs_t[5].address = (__paddr_t)&(mgbase->xt_req_timeout);
hq4regs_t[6].address = (__paddr_t)&(mgbase->xt_int_dst_addr_hi);
hq4regs_t[7].address = (__paddr_t)&(mgbase->xt_int_dst_addr_lo);
hq4regs_t[8].address = (__paddr_t)&(mgbase->xt_error);
hq4regs_t[9].address = (__paddr_t)&(mgbase->xt_llp_config);
hq4regs_t[0xa].address = (__paddr_t)&(mgbase->xt_target_flush);
}
#endif

/* bit patterns */

ushort_t patrnShort[12] = {
    0x5a5a, 0x3c3c, 0xf0f0,
    0xa5a5, 0xc3c3, 0x0f0f,
    0x5a5a, 0x3c3c, 0xf0f0,
    0xa5a5, 0xc3c3, 0x0f0f
};

__uint32_t patrn[12] = {
    0x5a5a5a5a, 0x3c3c3c3c, 0xf0f0f0f0,
    0xa5a5a5a5, 0xc3c3c3c3, 0x0f0f0f0f,
    0x5a5a5a5a, 0x3c3c3c3c, 0xf0f0f0f0,
    0xa5a5a5a5, 0xc3c3c3c3, 0x0f0f0f0f
};

ushort_t walk_1_0[34]  = {
    0x0001, 0x0002, 0x0004, 0x0008,
    0x0010, 0x0020, 0x0040, 0x0080,
    0x0100, 0x0200, 0x0400, 0x0800,
    0x1000, 0x2000, 0x4000, 0x8000,

    0xFFFE, 0xFFFD, 0xFFFB, 0xFFF7,
    0xFFEF, 0xFFDF, 0xFFBF, 0xFF7F,
    0xFEFF, 0xFDFF, 0xFBFF, 0xF7FF,
    0xEFFF, 0xDFFF, 0xBFFF, 0x7FFF,

    0x0000, 0xFFFF
};

__uint32_t walk_1_0_32_rss[WALK_1_0_32_RSS]  = {
    0x00000001, 0x00000002, 0x00000004, 0x00000008,
    0x00000010, 0x00000020, 0x00000040, 0x00000080,
    0x00000100, 0x00000200, 0x00000400, 0x00000800,
    0x00001000, 0x00002000, 0x00004000, 0x00008000,
    0x00010000, 0x00020000, 0x00040000, 0x00080000,
    0x00100000, 0x00200000, 0x00400000, 0x00800000,
    0x01000000, 0x02000000, 0x04000000, 0x08000000,
    0x10000000, 0x20000000, 0x40000000, 0x80000000,
    0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFB, 0xFFFFFFF7,
    0xFFFFFFEF, 0xFFFFFFDF, 0xFFFFFFBF, 0xFFFFFF7F,
    0xFFFFFEFF, 0xFFFFFDFF, 0xFFFFFBFF, 0xFFFFF7FF,
    0xFFFFEFFF, 0xFFFFDFFF, 0xFFFFBFFF, 0xFFFF7FFF,
    0xFFFEFFFF, 0xFFFDFFFF, 0xFFFBFFFF, 0xFFF7FFFF,
    0xFFEFFFFF, 0xFFDFFFFF, 0xFFBFFFFF, 0xFF7FFFFF,
    0xFEFFFFFF, 0xFDFFFFFF, 0xFBFFFFFF, 0xF7FFFFFF,
    0xEFFFFFFF, 0xDFFFFFFF, 0xBFFFFFFF, 0x7FFFFFFF
};

__uint32_t  walk_1_0_32[8]  = {
    0x00000001, 0x00000002, 0x00000004, 0x00000008,
    0x10000000, 0x20000000, 0x40000000, 0x80000000
};

ushort_t walking_patrn[16]= {
        0x01, 0x02, 0x04, 0x08,
        0x10, 0x20, 0x40, 0x80,
        0xFE, 0xFD, 0xFB, 0xF7,
        0xEF, 0xDF, 0xBF, 0x7F
};

tdma_format_ctl_t tdma_format_ctl;


__uint64_t DMA_wr_data[SIZE_DMA_WR_DATA/8] = {
    0x1122334455667788ull, 0x99aabbccddeeff00ull,
    0x1111222233334444ull, 0x5555666677778888ull
}; 

__uint32_t rgba[SIZE_RGBA_DATA] = {
0x11223344, 0x55667788, 0x99aabbcc, 0xddeeff00,
0x11112222, 0x33334444, 0x55556666, 0x77778888
}; 

__uint32_t rgba_recv[SIZE_RGBA_DATA] ;

__uint64_t DMA_rd_data[SIZE_DMA_WR_DATA/8];

realdma_words_t realdma_words, realdma_recv;
realdma_words_t tbl_ptr_send[256], tbl_ptr_recv[256] ;

vdma_tdma_addr_t vdma_addr, tdma_addr;
vdma_tdma_ctlreg_t vdma_cntl, tdma_cntl;
tdma_format_ctl_t tdma_format;

uchar_t in_format, out_format;

__uint32_t _mgras_dcbdma_enabled = 0;

__uint32_t mgras_scratch_buf[MGRAS_CMAP_NCOLMAPENT];

#if HQ4
__uint32_t _mg_setport = XBOW_PORT_9;
#endif
