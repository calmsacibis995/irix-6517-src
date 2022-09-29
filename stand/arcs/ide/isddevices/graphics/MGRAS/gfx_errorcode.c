/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990-1997, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include "mgras_diag.h"


/* Misc data that used to be in the header files.
 */
_Test_Info BackEnd[] = {
        {/* DIAG mg_cmap */ 		"Cmap0",                  "CMP000"}, 
        {/* DIAG mg_cmapaddrsbus */ 	"Cmap0 Address Bus",      "CMP001"}, 
        {/* DIAG mg_cmapdatabus */ 	"Cmap0 Data Bus",         "CMP002"},    
        {/* DIAG mg_cmapaddrsuniq */ 	"Cmap0 Address Uniqness", "CMP003"},
        {/* DIAG mg_cmappatrn */ 	"Cmap0 Pattern",          "CMP004"},

        {/* DIAG mg_cmap */ 		"Cmap1",                  "CMP100"},
        {/* DIAG mg_cmapaddrsbus */ 	"Cmap1 Address Bus",      "CMP101"},
        {/* DIAG mg_cmapdatabus */ 	"Cmap1 Data Bus",         "CMP102"},
        {/* DIAG mg_cmapaddrsuniq */ 	"Cmap1 Address Uniqness", "CMP103"},
        {/* DIAG mg_cmappatrn */ 	"Cmap1 Pattern",          "CMP104"},

        /* I assume this test all cmaps */
        {/* DIAG mg_cmapuniqtest */ 	"Cmap  Uniq",             "CMP999"},

        {/* Not used */			"VC3",                    "VCT000"},
        {/* DIAG mg_vc3addrsbus */	"VC3 Address Bus",        "VCT001"},
        {/* DIAG mg_vc3databus */	"VC3 Data Bus",           "VCT002"},
        {/* DIAG mg_vc3addrsuniq */	"VC3 Address Uniqness",   "VCT003"},
        {/* DIAG mg_vc3patrn */		"VC3 Pattern",            "VCT004"},
        {/* DIAG   *mg_vc3internalreg */	"VC3 Register",           "VCT005"},
    	{/* Not used */			"VC3 Sram 64 Bit",        "VCT006"},


        {/* DIAG mg_clrpalettewalkbit */ "DAC Color Palette Addr Bus","DAC001"},
        {/* DIAG mg_clrpaletteaddrUniq  */ "DAC Color Palette Addr Uniq", "DAC002"},
        {/* DIAG mg_clrpalettepatrn  */	"DAC Color Palette Patrn",    "DAC003"},
        {/* DIAG mg_dacctrlreg  */	"DAC Control Reg",            "DAC004"},
        {/* DIAG mg_dacmodereg  */	"DAC Mode Reg",               "DAC005"},
        {/* DIAG mg_dacaddrreg  */	"DAC Addr Reg",               "DAC006"},

    {/* DIAG mg_xmapdcbreg  */	"XMAP0 DCB Reg",               "XMP001"},
    {/* DIAG mg_xmapdcbreg  */	"XMAP1 DCB Reg",               "XMP101"},
    {/* DIAG mg_xmapdcbreg  */	"XMAP2 DCB Reg",               "XMP201"},
    {/* DIAG mg_xmapdcbreg  */	"XMAP3 DCB Reg",               "XMP301"},
    {/* DIAG mg_xmapdcbreg  */	"XMAP4 DCB Reg",               "XMP401"},
    {/* DIAG mg_xmapdcbreg  */	"XMAP5 DCB Reg",               "XMP501"},
    {/* DIAG mg_xmapdcbreg  */	"XMAP6 DCB Reg",               "XMP601"},
    {/* DIAG mg_xmapdcbreg  */	"XMAP7 DCB Reg",               "XMP701"},

    {/* DIAG mg_crcwalk  */	"CRC: Walk One PixelPath",         "XMP999"},

};

_Test_Info TRAM_err[] = {
    {/* DIAG mg_dma_tram */	"TRAM Revision",           "TRA000"}, /* 0 */
    {/* DIAG mg_dma_tram */	"TRAM DMA",                "TRA001"}, /* 1 */

    {/* DIAG mg_dma_tram */	"TRAM-01 Revision",           "TRA100"}, /* 2 */
    {/* DIAG mg_dma_tram */	"TRAM-01 DMA",                 "TRA101"},

    {/* DIAG mg_dma_tram */	"TRAM-02 Revision",            "TRA200"},
    {/* DIAG mg_dma_tram */	"TRAM-02 DMA",                 "TRA201"},

    {/* DIAG mg_dma_tram */	"TRAM-03 Revision",            "TRA300"},
    {/* DIAG mg_dma_tram */	"TRAM-03 DMA",                 "TRA301"},

    {/* DIAG mg_dma_tram */	"TRAM-10 Revision",            "TRB000"},
    {/* DIAG mg_dma_tram */	"TRAM-10 DMA",                 "TRB001"},

    {/* DIAG mg_dma_tram */	"TRAM-11 Revision",            "TRB100"},
    {/* DIAG mg_dma_tram */	"TRAM-11 DMA",                 "TRB101"},

    {/* DIAG mg_dma_tram */	"TRAM-12 Revision",            "TRB200"},
    {/* DIAG mg_dma_tram */	"TRAM-12 DMA",                 "TRB201"},

    {/* DIAG mg_dma_tram */	"TRAM-13 Revision",            "TRB300"},
    {/* DIAG mg_dma_tram */	"TRAM-13 DMA",                 "TRB301"},

    {/* DIAG mg_dma_tram */	"TRAM-20 Revision",            "TRC000"},
    {/* DIAG mg_dma_tram */	"TRAM-20 DMA",                 "TRC001"},

    {/* DIAG mg_dma_tram */	"TRAM-21 Revision",            "TRC100"},
    {/* DIAG mg_dma_tram */	"TRAM-21 DMA",                 "TRC101"},

    {/* DIAG mg_dma_tram */	"TRAM-22 Revision",            "TRC200"},
    {/* DIAG mg_dma_tram */	"TRAM-22 DMA",                 "TRC201"},

    {/* DIAG mg_dma_tram */	"TRAM-23 Revision",            "TRC300"},
    {/* DIAG mg_dma_tram */	"TRAM-23 DMA",                 "TRC301"},

    {/* DIAG mg_dma_tram */	"TRAM-30 Revision",            "TRD000"},
    {/* DIAG mg_dma_tram */	"TRAM-30 DMA",                 "TRD001"},

    {/* DIAG mg_dma_tram */	"TRAM-31 Revision",            "TRD100"},
    {/* DIAG mg_dma_tram */	"TRAM-31 DMA",                 "TRD101"},

    {/* DIAG mg_dma_tram */	"TRAM-32 Revision",            "TRD200"},
    {/* DIAG mg_dma_tram */	"TRAM-32 DMA",                 "TRD201"},

    {/* DIAG mg_dma_tram */	"TRAM-33 Revision",            "TRD300"},
    {/* DIAG mg_dma_tram */	"TRAM-33 DMA",                 "TRD301"}
};
_Test_Info TE_err[] = {
    {/* DIAG mg_read_te_version */	"TE1-0 REV",               "TEX000"},
    {/* DIAG mg_re_rdwr_regs  */	"TE1-0 RdWrRegs",          "TEX001"},
    {/* Not used */			"TE1 Video",               "TEX002"},
    {/* Not used */			"TE1-0 CntxSw",            "TEX003"},
    {/* Not used */			"TE1 LOD",                 "TEX004"},
    {/* Not used */			"TE1 LdSmpl",              "TEX005"},
    {/* Not used */			"TE1-0 Warp",              "TEX006"},
    {/* DIAG mg_retexlut  */		"RE TEXLUT",               "TEX007"},
    {/* Not used */			"TE1-0 WrOnlyRegs",        "TEX008"},
    {/* DIAG mg_tex_poly  */		"TE1 Texture Polygon",     "TEX009"},
    {/* DIAG mg_notex_poly  */		"TE1 No-Texture Polygon",  "TEX010"},
    {/* DIAG mg_tex_bordertall  */	"TE-TRAM 8Khigh Border",   "TEX011"},
    {/* DIAG mg_tex_borderwide  */	"TE-TRAM 8Kwide Border",    "TEX012"},
    {/* DIAG mg_tex_detail  */	"TE-TRAM Detail Texture",          "TEX013"},
    {/* DIAG mg_tex_mag  */	"TE-TRAM Magnification",           "TEX014"},
    {/* DIAG mg_tex_persp  */	"TE-TRAM Perspective",             "TEX015"},
    {/* DIAG mg_tex_linegl  */	"TE-TRAM GL Line",             "TEX016"},
    {/* DIAG mg_tex_scistri  */	"TE-TRAM Scissored Triangle",      "TEX017"},
    {/* DIAG mg_tex_load  */	"TE-TRAM Loading",             "TEX018"},
    {/* DIAG mg_tex_lut4d  */	"TE-TRAM LUT 4D",              "TEX019"},
    {/* DIAG mg_tex_mddma  */	"TE-TRAM MD DMA",              "TEX020"},
    {/* DIAG mg_tex_1d  */	"TE-TRAM 1D Texture",          "TEX021"},
    {/* DIAG mg_tex_3d  */	"TE-TRAM 3D Texture",          "TEX022"},

    {/* DIAG mg_read_te_version */	"TE1-1 REV",               "TEX100"},
    {/* DIAG mg_re_rdwr_regs  */	"TE1-1 RdWrRegs",          "TEX101"},
    {/* Not used */			"TE1 Video",               "TEX102"},
    {/* Not used */			"TE1-1 CntxSw",            "TEX103"},
    {/* Not used */			"TE1 LOD",                 "TEX104"},
    {/* Not used */			"TE1 LdSmpl",              "TEX105"},
    {/* Not used */			"TE1-1 Warp",              "TEX106"},
    {/* DIAG mg_retexlut  */		"RE TEXLUT",               "TEX107"},
    {/* Not used */			"TE1-1 WrOnlyRegs",        "TEX108"},
    {/* DIAG mg_tex_poly  */		"TE1 Texture Polygon",     "TEX109"},
    {/* DIAG mg_notex_poly  */		"TE1 No-Texture Polygon",  "TEX110"},
    {/* DIAG mg_tex_bordertall  */	"TE-TRAM 8Khigh Border",   "TEX111"},
    {/* DIAG mg_tex_borderwide  */	"TE-TRAM 8Kwide Border",    "TEX112"},
    {/* DIAG mg_tex_detail  */	"TE-TRAM Detail Texture",          "TEX113"},
    {/* DIAG mg_tex_mag  */	"TE-TRAM Magnification",           "TEX114"},
    {/* DIAG mg_tex_persp  */	"TE-TRAM Perspective",             "TEX115"},
    {/* DIAG mg_tex_linegl  */	"TE-TRAM GL Line",             "TEX116"},
    {/* DIAG mg_tex_scistri  */	"TE-TRAM Scissored Triangle",      "TEX117"},
    {/* DIAG mg_tex_load  */	"TE-TRAM Loading",             "TEX118"},
    {/* DIAG mg_tex_lut4d  */	"TE-TRAM LUT 4D",              "TEX119"},
    {/* DIAG mg_tex_mddma  */	"TE-TRAM MD DMA",              "TEX120"},
    {/* DIAG mg_tex_1d  */	"TE-TRAM 1D Texture",          "TEX121"},
    {/* DIAG mg_tex_3d  */	"TE-TRAM 3D Texture",          "TEX122"},

    {/* DIAG mg_read_te_version */	"TE1-2 REV",               "TEX200"},
    {/* DIAG mg_re_rdwr_regs  */	"TE1-2 RdWrRegs",          "TEX201"},
    {/* Not used */			"TE1 Video",               "TEX202"},
    {/* Not used */			"TE1-2 CntxSw",            "TEX203"},
    {/* Not used */			"TE1 LOD",                 "TEX204"},
    {/* Not used */			"TE1 LdSmpl",              "TEX205"},
    {/* Not used */			"TE1-2 Warp",              "TEX206"},
    {/* DIAG mg_retexlut  */		"RE TEXLUT",               "TEX207"},
    {/* Not used */			"TE1-2 WrOnlyRegs",        "TEX208"},
    {/* DIAG mg_tex_poly  */		"TE1 Texture Polygon",     "TEX209"},
    {/* DIAG mg_notex_poly  */		"TE1 No-Texture Polygon",  "TEX210"},
    {/* DIAG mg_tex_bordertall  */	"TE-TRAM 8Khigh Border",   "TEX211"},
    {/* DIAG mg_tex_borderwide  */	"TE-TRAM 8Kwide Border",    "TEX212"},
    {/* DIAG mg_tex_detail  */	"TE-TRAM Detail Texture",          "TEX213"},
    {/* DIAG mg_tex_mag  */	"TE-TRAM Magnification",           "TEX214"},
    {/* DIAG mg_tex_persp  */	"TE-TRAM Perspective",             "TEX215"},
    {/* DIAG mg_tex_linegl  */	"TE-TRAM GL Line",             "TEX216"},
    {/* DIAG mg_tex_scistri  */	"TE-TRAM Scissored Triangle",      "TEX217"},
    {/* DIAG mg_tex_load  */	"TE-TRAM Loading",             "TEX218"},
    {/* DIAG mg_tex_lut4d  */	"TE-TRAM LUT 4D",              "TEX219"},
    {/* DIAG mg_tex_mddma  */	"TE-TRAM MD DMA",              "TEX220"},
    {/* DIAG mg_tex_1d  */	"TE-TRAM 1D Texture",          "TEX221"},
    {/* DIAG mg_tex_3d  */	"TE-TRAM 3D Texture",          "TEX222"},

    {/* DIAG mg_read_te_version */	"TE1-3 REV",               "TEX300"},
    {/* DIAG mg_re_rdwr_regs  */	"TE1-3 RdWrRegs",          "TEX301"},
    {/* Not used */			"TE1 Video",               "TEX302"},
    {/* Not used */			"TE1-3 CntxSw",            "TEX303"},
    {/* Not used */			"TE1 LOD",                 "TEX304"},
    {/* Not used */			"TE1 LdSmpl",              "TEX305"},
    {/* Not used */			"TE1-3 Warp",              "TEX306"},
    {/* DIAG mg_retexlut  */		"RE TEXLUT",               "TEX307"},
    {/* Not used */			"TE1-3 WrOnlyRegs",        "TEX308"},
    {/* DIAG mg_tex_poly  */		"TE1 Texture Polygon",     "TEX309"},
    {/* DIAG mg_notex_poly  */		"TE1 No-Texture Polygon",  "TEX310"},
    {/* DIAG mg_tex_bordertall  */	"TE-TRAM 8Khigh Border",   "TEX311"},
    {/* DIAG mg_tex_borderwide  */	"TE-TRAM 8Kwide Border",    "TEX312"},
    {/* DIAG mg_tex_detail  */	"TE-TRAM Detail Texture",          "TEX313"},
    {/* DIAG mg_tex_mag  */	"TE-TRAM Magnification",           "TEX314"},
    {/* DIAG mg_tex_persp  */	"TE-TRAM Perspective",             "TEX315"},
    {/* DIAG mg_tex_linegl  */	"TE-TRAM GL Line",             "TEX316"},
    {/* DIAG mg_tex_scistri  */	"TE-TRAM Scissored Triangle",      "TEX317"},
    {/* DIAG mg_tex_load  */	"TE-TRAM Loading",             "TEX318"},
    {/* DIAG mg_tex_lut4d  */	"TE-TRAM LUT 4D",              "TEX319"},
    {/* DIAG mg_tex_mddma  */	"TE-TRAM MD DMA",              "TEX320"},
    {/* DIAG mg_tex_1d  */	"TE-TRAM 1D Texture",          "TEX321"},
    {/* DIAG mg_tex_3d  */	"TE-TRAM 3D Texture",          "TEX322"},
};

_Test_Info RDRAM_err[] = {
    {/* Not used */		    "RSS-0, PP1-0, RDRAM-0 READ REG", "RAA000"},
    {/* DIAG mg_rdram_addrbus */     "RSS-0, PP1-0, RDRAM-0 ADDRBUS", "RAA001"},
    {/* DIAG mg_rdram_databus */     "RSS-0, PP1-0, RDRAM-0 DATABUS", "RAA002"},
    {/* DIAG mg_rdram_unique */      "RSS-0, PP1-0, RDRAM-0 UNIQUE",  "RAA003"},
    {/* DIAG mg_rdram_pio_memtest */ "RSS-0, PP1-0, RDRAM-0 PIO MEM", "RAA004"},
    {/* Not used */		     "RSS-0, PP1-0, RDRAM-0 DMA MEM", "RAA005"},

    {/* Not used */		    "RSS-0, PP1-0, RDRAM-1 READ REG", "RAA100"},
    {/* DIAG mg_rdram_addrbus */     "RSS-0, PP1-0, RDRAM-1 ADDRBUS", "RAA101"},
    {/* DIAG mg_rdram_databus */     "RSS-0, PP1-0, RDRAM-1 DATABUS", "RAA102"},
    {/* DIAG mg_rdram_unique */      "RSS-0, PP1-0, RDRAM-1 UNIQUE",  "RAA103"},
    {/* DIAG mg_rdram_pio_memtest */ "RSS-0, PP1-0, RDRAM-1 PIO MEM", "RAA104"},
    {/* Not used */		     "RSS-0, PP1-0, RDRAM-1 DMA MEM", "RAA105"},

    {/* Not used */		    "RSS-0, PP1-0, RDRAM-2 READ REG", "RAA200"},
    {/* DIAG mg_rdram_addrbus */     "RSS-0, PP1-0, RDRAM-2 ADDRBUS", "RAA201"},
    {/* DIAG mg_rdram_databus */     "RSS-0, PP1-0, RDRAM-2 DATABUS", "RAA202"},
    {/* DIAG mg_rdram_unique */      "RSS-0, PP1-0, RDRAM-2 UNIQUE",  "RAA203"},
    {/* DIAG mg_rdram_pio_memtest */ "RSS-0, PP1-0, RDRAM-2 PIO MEM", "RAA204"},
    {/* Not used */		     "RSS-0, PP1-0, RDRAM-2 DMA MEM", "RAA205"},

    {/* Not used */		    "RSS-0, PP1-1, RDRAM-0 READ REG", "RAB000"},
    {/* DIAG mg_rdram_addrbus */     "RSS-0, PP1-1, RDRAM-0 ADDRBUS", "RAB001"},
    {/* DIAG mg_rdram_databus */     "RSS-0, PP1-1, RDRAM-0 DATABUS", "RAB002"},
    {/* DIAG mg_rdram_unique */      "RSS-0, PP1-1, RDRAM-0 UNIQUE",  "RAB003"},
    {/* DIAG mg_rdram_pio_memtest */ "RSS-0, PP1-1, RDRAM-0 PIO MEM", "RAB004"},
    {/* Not used */		     "RSS-0, PP1-1, RDRAM-0 DMA MEM", "RAB005"},

    {/* Not used */		    "RSS-0, PP1-1, RDRAM-1 READ REG", "RAB100"},
    {/* DIAG mg_rdram_addrbus */     "RSS-0, PP1-1, RDRAM-1 ADDRBUS", "RAB101"},
    {/* DIAG mg_rdram_databus */     "RSS-0, PP1-1, RDRAM-1 DATABUS", "RAB102"},
    {/* DIAG mg_rdram_unique */      "RSS-0, PP1-1, RDRAM-1 UNIQUE",  "RAB103"},
    {/* DIAG mg_rdram_pio_memtest */ "RSS-0, PP1-1, RDRAM-1 PIO MEM", "RAB104"},
    {/* Not used */		     "RSS-0, PP1-1, RDRAM-1 DMA MEM", "RAB105"},

    {/* Not used */		    "RSS-0, PP1-1, RDRAM-2 READ REG", "RAB200"},
    {/* DIAG mg_rdram_addrbus */     "RSS-0, PP1-1, RDRAM-2 ADDRBUS", "RAB201"},
    {/* DIAG mg_rdram_databus */     "RSS-0, PP1-1, RDRAM-2 DATABUS", "RAB202"},
    {/* DIAG mg_rdram_unique */      "RSS-0, PP1-1, RDRAM-2 UNIQUE",  "RAB203"},
    {/* DIAG mg_rdram_pio_memtest */ "RSS-0, PP1-1, RDRAM-2 PIO MEM", "RAB204"},
    {/* Not used */		     "RSS-0, PP1-1, RDRAM-2 DMA MEM", "RAB205"},

    {/* Not used */		    "RSS-1, PP1-0, RDRAM-0 READ REG", "RBA000"},
    {/* DIAG mg_rdram_addrbus */     "RSS-1, PP1-0, RDRAM-0 ADDRBUS", "RBA001"},
    {/* DIAG mg_rdram_databus */     "RSS-1, PP1-0, RDRAM-0 DATABUS", "RBA002"},
    {/* DIAG mg_rdram_unique */      "RSS-1, PP1-0, RDRAM-0 UNIQUE",  "RBA003"},
    {/* DIAG mg_rdram_pio_memtest */ "RSS-1, PP1-0, RDRAM-0 PIO MEM", "RBA004"},
    {/* Not used */		     "RSS-1, PP1-0, RDRAM-0 DMA MEM", "RBA005"},

    {/* Not used */		    "RSS-1, PP1-0, RDRAM-1 READ REG", "RBA100"},
    {/* DIAG mg_rdram_addrbus */     "RSS-1, PP1-0, RDRAM-1 ADDRBUS", "RBA101"},
    {/* DIAG mg_rdram_databus */     "RSS-1, PP1-0, RDRAM-1 DATABUS", "RBA102"},
    {/* DIAG mg_rdram_unique */      "RSS-1, PP1-0, RDRAM-1 UNIQUE",  "RBA103"},
    {/* DIAG mg_rdram_pio_memtest */ "RSS-1, PP1-0, RDRAM-1 PIO MEM", "RBA104"},
    {/* Not used */		     "RSS-1, PP1-0, RDRAM-1 DMA MEM", "RBA105"},

    {/* Not used */		    "RSS-1, PP1-0, RDRAM-2 READ REG", "RBA200"},
    {/* DIAG mg_rdram_addrbus */     "RSS-1, PP1-0, RDRAM-2 ADDRBUS", "RBA201"},
    {/* DIAG mg_rdram_databus */     "RSS-1, PP1-0, RDRAM-2 DATABUS", "RBA202"},
    {/* DIAG mg_rdram_unique */      "RSS-1, PP1-0, RDRAM-2 UNIQUE",  "RBA203"},
    {/* DIAG mg_rdram_pio_memtest */ "RSS-1, PP1-0, RDRAM-2 PIO MEM", "RBA204"},
    {/* Not used */		     "RSS-1, PP1-0, RDRAM-2 DMA MEM", "RBA205"},

    {/* Not used */		    "RSS-1, PP1-1, RDRAM-0 READ REG", "RBB000"},
    {/* DIAG mg_rdram_addrbus */     "RSS-1, PP1-1, RDRAM-0 ADDRBUS", "RBB001"},
    {/* DIAG mg_rdram_databus */     "RSS-1, PP1-1, RDRAM-0 DATABUS", "RBB002"},
    {/* DIAG mg_rdram_unique */      "RSS-1, PP1-1, RDRAM-0 UNIQUE",  "RBB003"},
    {/* DIAG mg_rdram_pio_memtest */ "RSS-1, PP1-1, RDRAM-0 PIO MEM", "RBB004"},
    {/* Not used */		     "RSS-1, PP1-1, RDRAM-0 DMA MEM", "RBB005"},

    {/* Not used */		    "RSS-1, PP1-1, RDRAM-1 READ REG", "RBB100"},
    {/* DIAG mg_rdram_addrbus */     "RSS-1, PP1-1, RDRAM-1 ADDRBUS", "RBB101"},
    {/* DIAG mg_rdram_databus */     "RSS-1, PP1-1, RDRAM-1 DATABUS", "RBB102"},
    {/* DIAG mg_rdram_unique */      "RSS-1, PP1-1, RDRAM-1 UNIQUE",  "RBB103"},
    {/* DIAG mg_rdram_pio_memtest */ "RSS-1, PP1-1, RDRAM-1 PIO MEM", "RBB104"},
    {/* Not used */		     "RSS-1, PP1-1, RDRAM-1 DMA MEM", "RBB105"},

    {/* Not used */		    "RSS-1, PP1-1, RDRAM-2 READ REG", "RBB200"},
    {/* DIAG mg_rdram_addrbus */     "RSS-1, PP1-1, RDRAM-2 ADDRBUS", "RBB201"},
    {/* DIAG mg_rdram_databus */     "RSS-1, PP1-1, RDRAM-2 DATABUS", "RBB202"},
    {/* DIAG mg_rdram_unique */      "RSS-1, PP1-1, RDRAM-2 UNIQUE",  "RBB203"},
    {/* DIAG mg_rdram_pio_memtest */ "RSS-1, PP1-1, RDRAM-2 PIO MEM", "RBB204"},
    {/* Not used */		     "RSS-1, PP1-1, RDRAM-2 DMA MEM", "RBB205"},
};

_Test_Info PP_err[] = {
    {/* Not used */	"RSS-0, PP1-0, RACREG",                "PPA000"},
    {/* Not used */	"RSS-0, PP1-0, OVERLAY",               "PPA001"},
    {/* Not used */	"RSS-0, PP1-0, CLIPID",                "PPA002"},
    {/* Not used */	"RSS-0, PP1-0, STENCIL",               "PPA003"},
    {/* Not used */	"RSS-0, PP1-0, ZFUNC",                 "PPA004"},
    {/* DIAG mg_logicop  */	"RSS, PP1, LOGICOP",               "PPA005"},
    {/* DIAG mg_dither  */	"RSS, PP1, DITHER",                "PPA006"},

    {/* Not used  */	"RSS-0, PP1-1, RACREG",                "PPA100"},
    {/* Not used  */	"RSS-0, PP1-1, OVERLAY",               "PPA101"},
    {/* Not used  */	"RSS-0, PP1-1, CLIPID",                "PPA102"},
    {/* Not used  */	"RSS-0, PP1-1, STENCIL",               "PPA103"},
    {/* Not used  */	"RSS-0, PP1-1, ZFUNC",                 "PPA104"},
    {/* DIAG mg_logicop  */	"RSS, PP1, LOGICOP",               "PPA105"},
    {/* DIAG mg_dither  */	"RSS, PP1, DITHER",                "PPA106"},


    {/* Not used  */	"RSS-1, PP1-0, RACREG",                "PPB000"},
    {/* Not used  */	"RSS-1, PP1-0, OVERLAY",               "PPB001"},
    {/* Not used  */	"RSS-1, PP1-0, CLIPID",                "PPB002"},
    {/* Not used  */	"RSS-1, PP1-0, STENCIL",               "PPB003"},
    {/* Not used  */	"RSS-1, PP1-0, ZFUNC",                 "PPB004"},
    {/* DIAG mg_logicop  */	"RSS, PP1, LOGICOP",               "PPB005"},
    {/* DIAG mg_dither  */	"RSS, PP1, DITHER",                "PPB006"},


    {/* Not used  */	"RSS-1, PP1-1, RACREG",                "PPB100"},
    {/* Not used  */	"RSS-1, PP1-1, OVERLAY",               "PPB101"},
    {/* Not used  */	"RSS-1, PP1-1, CLIPID",                "PPB102"},
    {/* Not used  */	"RSS-1, PP1-1, STENCIL",               "PPB103"},
    {/* Not used  */	"RSS-1, PP1-1, ZFUNC",                 "PPB104"},
    {/* DIAG mg_logicop  */	"RSS, PP1, LOGICOP",               "PPB105"},
    {/* DIAG mg_dither  */	"RSS, PP1, DITHER",                "PPB106"},

};

_Test_Info RE_err[] = {
    {/* DIAG mg_re_status_reg  */	"RE4-0 Status Reg",        "RRE000"},
    {/* DIAG mg_re_rdwr_regs  */	"RE4-0 RdWr Regs",         "RRE001"},
    {/* Not used */			"RE4-0 RAC Reg",           "RRE002"},
    {/* Not used */			"RE4-0 FIFOs",             "RRE003"},
    {/* DIAG mg_re_internal_ram   */	"RE4-0 InternalRAM",       "RRE004"},
    {/* DIAG mg_color_tri  */		"RE4 Color Tri",           "RRE005"},
    {/* DIAG mg_notex_line  */		"RE4 NoTex Line",          "RRE006"},
    {/* Not used */			"RE4-0 PPModes",           "RRE007"},
    {/* Not used */			"RE4 TEX Line",            "RRE008"},
    {/* Not used */			"RE4 FOG",                 "RRE009"},
    {/* Not used */			"RE4 ScrnMsK",             "RRE010"},
    {/* Not used */			"RE4 AA",                  "RRE011"},
    {/* DIAG mg_chars  */		"RE4 Chars",               "RRE012"},
    {/* DIAG mg_stip_tri  */		"RE4 PolyStiP",            "RRE013"},
    {/* DIAG mg_lines  */		"RE4 Line",                "RRE014"},
    {/* Not used */			"RE4 Scissor",             "RRE015"},
    {/* DIAG mg_points  */		"RE4 Points",              "RRE016"},
    {/* DIAG mg_z_tri  */		"RE4 Z",                   "RRE017"},
    {/* Not used */			"RE4 Xpoint",              "RRE018"},
    {/* DIAG mg_xblock  */		"RE4 Xblk",                "RRE019"},
    {/* Not used */			"RE4-0 WrOnlyRegs",        "RRE020"},
    {/* DIAG mg_alphablend  */		"RE4 Alpha Blend",         "RRE021"},

    {/* DIAG mg_re_status_reg  */	"RE4-1 Status Reg",        "RRE100"},
    {/* DIAG mg_re_rdwr_regs  */	"RE4-1 RdWr Regs",         "RRE101"},
    {/* Not used */			"RE4-1 RAC Reg",           "RRE102"},
    {/* Not used */			"RE4-1 FIFOs",             "RRE103"},
    {/* DIAG mg_re_internal_ram   */	"RE4-1 InternalRAM",       "RRE104"},
    {/* DIAG mg_color_tri  */		"RE4 Color Tri",           "RRE105"},
    {/* DIAG mg_notex_line  */		"RE4 NoTex Line",          "RRE106"},
    {/* Not used */			"RE4-1 PPModes",           "RRE107"},
    {/* Not used */			"RE4 TEX Line",            "RRE108"},
    {/* Not used */			"RE4 FOG",                 "RRE109"},
    {/* Not used */			"RE4 ScrnMsK",             "RRE110"},
    {/* Not used */			"RE4 AA",                  "RRE111"},
    {/* DIAG mg_chars  */		"RE4 Chars",               "RRE112"},
    {/* DIAG mg_stip_tri  */		"RE4 PolyStiP",            "RRE113"},
    {/* DIAG mg_lines  */		"RE4 Line",                "RRE114"},
    {/* Not used */			"RE4 Scissor",             "RRE115"},
    {/* DIAG mg_points  */		"RE4 Points",              "RRE116"},
    {/* DIAG mg_z_tri  */		"RE4 Z",                   "RRE117"},
    {/* Not used */			"RE4 Xpoint",              "RRE118"},
    {/* DIAG mg_xblock  */		"RE4 Xblk",                "RRE119"},
    {/* Not used */			"RE4-1 WrOnlyRegs",        "RRE120"},
    {/* DIAG mg_alphablend  */		"RE4 Alpha Blend",         "RRE121"},
};

_Test_Info HQ3_err[] = {
#if HQ4
    {/* DIAG mg_hq  */	"HQ XIO Bus",                 "HQT000"},
#else
    {/* DIAG mg_hq  */	"HQ GIOBus",                  "HQT000"},
#endif
    {/* DIAG mg_hq  */	"HQ HQBus",                   "HQT001"},
    {/* Not used */	"HQ Registers",                   "HQT002"},
    {/* DIAG mg_hq  */	"HQ TLB",                     "HQT003"},
    {/* DIAG mg_hq  */	"HQ REIF CTX",                    "HQT004"},
    {/* DIAG mg_hq  */	"HQ HAG CTX",                     "HQT005"},
    {/* DIAG mg_hq  */	"HQ Ucode DataBus",               "HQT006"},
    {/* DIAG mg_hq  */	"HQ Ucode AddrBus",               "HQT007"},
    {/* DIAG mg_hq  */	"HQ Ucode Pattern",               "HQT008"},
    {/* DIAG mg_hq  */	"HQ Ucode AddrUniquness",             "HQT009"},
    {/* DIAG mg_hq  */	"HQ HQ_RSS_DataBus",                 "HQT010"},
    {/* DIAG mg_hq_cp  */	"HQ CP_Functionality",                "HQT011"},
    {/* DIAG mg_hq_cfifo  */	"HQ CFIFO Functionality",             "HQT012"},
    {/* DIAG mg_hq_converter  */ "HQ Converter 32-bit",               "HQT013"},
    {/* DIAG mg_hq_converter  */	"HQ Converter 16-bit",        "HQT014"},
    {/* DIAG mg_hq_converter  */	"HQ Converter 8-bit Test",    "HQT015"},
    {/* DIAG mg_hq_converter  */	"HQ Convert Stuff Test",      "HQT016"},
    {/* Not used */	"HQ HQCP->GE->Rebus->HQ data path",       "HQT017"},
    {/* Not used */	"HQ HQCP->GE->Rebus->RE data path",       "HQT018"},
    {/* DIAG mg_host_hqdma */	"HQ HOST->HQ DMA data path",      "HQT019"},
    {/* DIAG mg_host_hq_cp_dma */ "HQ HOST->HQ->CP DMA data path",    "HQT020"},
};

_Test_Info GE11_err[] = {
    {/* DIAG mg_ge_ucode_dbus  */	"GE11 Ucode DataBus",         "GEE000"},
    {/* DIAG mg_ge_ucode_abus  */	"GE11 Ucode AddrBus",         "GEE001"},
    {/* DIAG mg_ge_ucode_m  */	"GE11 Ucode Pattern",          "GEE002"},
    {/* DIAG mg_ge_ucode_a  */	"GE11 Ucode AddrUniquness",        "GEE003"},
    {/* DIAG mg_ge_ucode_w  */	"GE11 Ucode Walking bit",        "GEE004"},
    {/* DIAG mg_ge_cram  */	"GE11 cram",        "GEE005"},
    {/* DIAG mg_ge_eram  */	"GE11 eram",        "GEE006"},
    {/* DIAG mg_ge_wram  */	"GE11 wram",        "GEE007"},
    {/* DIAG mg_ge_alu  */	"GE11 alu",        "GEE008"},
    {/* DIAG mg_ge_dreg  */	"GE11 dreg",        "GEE009"},
    {/* DIAG mg_ge_inst  */	"GE11 inst",        "GEE010"},
    {/* DIAG mg_ge_dma  */	"GE11 DMA",        "GEE011"},
    {/* DIAG mg_ge_aalu3  */	"GE11 aalu3",        "GEE012"},
    {/* DIAG mg_ge_mul  */	"GE11 mul",        "GEE013"},
    {/* DIAG mg_ge_areg1  */	"GE11 areg",        "GEE014"},
    {/* DIAG mg_ge_ucode_server */	"GE11 Ucode Server",        "GEE015"},
    {/* DIAG mg_ge_ucode_dnload */	"GE11 Ucode Dnload",        "GEE016"},

};

_Test_Info DMA_err[] = {
    {/* DIAG mg_dma_pp */ "Framebuffer RDRAM (Using DMA style 000)", "DMA000"},
    {/* DIAG mg_dma_pp */ "Framebuffer RDRAM (Using DMA style 001)", "DMA001"},
    {/* DIAG mg_dma_pp */ "Framebuffer RDRAM (Using DMA style 002)", "DMA002"},
    {/* DIAG mg_dma_pp */ "Framebuffer RDRAM (Using DMA style 003)", "DMA003"},
    {/* DIAG mg_dma_pp */ "Framebuffer RDRAM (Using DMA style 004)", "DMA004"},

    {/* DIAG mg_dma_pp */ "Framebuffer RDRAM (Using DMA style 005)", "DMA005"},
    {/* DIAG mg_dma_pp */ "Framebuffer RDRAM (Using DMA style 006)", "DMA006"},
    {/* DIAG mg_dma_pp */ "Framebuffer RDRAM (Using DMA style 007)", "DMA007"},
    {/* DIAG mg_dma_pp */ "Framebuffer RDRAM (Using DMA style 008)", "DMA008"},
    {/* DIAG mg_dma_pp */ "Framebuffer RDRAM (Using DMA style 009)", "DMA009"},
    {/* DIAG mg_dma_pp */ "Framebuffer RDRAM (Using DMA style 010)", "DMA010"},

    {/* DIAG mg_dma_tram */ "Texture TRAM (Using DMA style 011)",    "DMA011"},
    {/* DIAG mg_dma_tram */ "Texture TRAM (Using DMA style 012)",    "DMA012"},
    {/* DIAG mg_dma_pp  */ "Framebuffer RDRAM (Using DMA style 013)", "DMA013"},
    {/* DIAG mg_dma_tram */ "Texture TRAM (Using DMA style 014)",     "DMA014"},
};

_Test_Info HQ4_err[] = {
    {/* DIAG mg_hq4reg  */	"HQ4 Register Test",     "HQ4000"},

};
