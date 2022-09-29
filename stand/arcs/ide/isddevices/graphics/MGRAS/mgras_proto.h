/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __MGRAS_PROTO_H__

#define __MGRAS_PROTO_H__

/**************************************************************************
 *                 STRUCTURE PROTOTYPES                                   *
 **************************************************************************/ 
typedef struct _Test_Info {
        char *TestStr;
        char *ErrStr;
} _Test_Info;

typedef union {
	struct  _mgras_tlb_test {
    __uint32_t : 27,      /* reserved bits */
               tlb_in:3,    
               tlb_bank:1,  
               tlb_tag:1;   
	} bits;
	__uint32_t value;
} mgras_tlb_test_t;

typedef union {
	struct  _realdma_word1 {
    __uint64_t data_len:16,
               data_start_addr:48;
	} bits;
	__uint64_t value; 
} realdma_word1_t;

typedef union {
	struct _realdma_word2 {
    __uint64_t :13, /* undefined */
               vsync_ctl:2,
               more_records:1,
               next_tbl_addr:48;
	} bits;
	__uint64_t value;
} realdma_word2_t;

typedef struct _realdma_words {
   realdma_word1_t word1;
   realdma_word2_t word2;
} realdma_words_t ;

typedef union {
	struct _vdma_tdma_regfields {
	__uint64_t :3, /* undefined */
               tdma_biff_inuse:3,
               dma_data_gbr:1,
               dma_tbl_gbr:1,
               dma_data_dev_id:4,
               dma_tbl_dev_id:4,
               dma_tbl_addr:48;
	} bits;
	__uint64_t value;
} vdma_tdma_addr_t;

typedef union {
     struct _vdma_tdma_ctlreg_fields {
   __uint32_t remote_map:16,
              tdma_in_ctx:1,
              tdma_addr_mode:1,
              flow_ctl_cnt:8,
              write_with_resp:1,
              coherent_transfer:1,
              peer_transfer:1,
              dma_stall:1,
              dma_abort:1,
              dma_start:1;
	} bits;
	__uint32_t value;
} vdma_tdma_ctlreg_t ;

typedef union {
    struct _tdma_format_ctl {
	__uint32_t :13, /* undefined */
               te_async_rdy:1,
               diag_stobe:1,
               diag_mode:1,
               vsync_cntl:2,
               bus_util:4,
               te_stall_en_mask:4,
               tex_out_format:2,
               video_in_format:3,
               video_in_port_en:1;
	} bits;
	__uint32_t value;
} tdma_format_ctl_t ;


struct mgras_hinv {
    __uint32_t MonitorType;
    __uint32_t RARev;
    __uint32_t VidBdPresent; /* video board present*/
    __uint32_t RA_TRAMs;
    __uint32_t RB_TRAMs;
    __uint32_t RBRev;
    __uint32_t GDRev;
    __uint32_t ProductId;
    __uint32_t GEs;

    __uint32_t CmapRev;
    __uint32_t VC3Rev;

    __uint32_t HQ3Rev;
    __uint32_t GE11Rev;
    __uint32_t RE4Rev;
    __uint32_t TE1Rev;
    __uint32_t PP1Rev;
    __uint32_t TRAMRev;
    __uint32_t REs;
    __uint32_t DacRev;
    __uint32_t DacId;
} ;
typedef struct _DacInternal_Reg_Info {
                char *str;
                ushort_t RegName;
                ushort_t mask;
} _DacInternal_Reg_Info ;

typedef struct _input{
         unsigned Bit0    : 1  ;
         unsigned blue    : 10 ;
         unsigned green    : 10 ;
         unsigned red      : 10 ;
         unsigned Bit31   : 1  ;
         unsigned Bit32   : 1  ;
} input;

typedef struct _test_matrix {
        __uint32_t pc_offset;
        __uint32_t data_out;
        __uint32_t expected;
        __uint32_t verif_flag; /* 1: Check cp_data against expected
                               then sent the data out.
                            0: Just send the data out */
} test_matrix;

typedef struct _hq3_cp_test {
        char *name;
        __uint32_t token;
        __uint32_t  bytecount;
        __uint32_t  send_data_first;
        test_matrix *cp;
} hq3_cp_test;

typedef struct _ge_ucode {
        unsigned short  uword2;
        __uint32_t      uword1;
        __uint32_t      uword0;
} ge_ucode;

typedef struct _RSS_Reg_Info_t {
                char *str;
                __uint32_t regNum;
                __uint32_t mask;
                __uint32_t readable;
} RSS_Reg_Info_t;

typedef struct _CoordType {
        float x, y, z, w;
} Coords;

typedef struct _ColorType {
        float r, g, b, a;
} Colors;

typedef struct _VertexType {
        Coords  coords;
        Colors  color;
        Colors  colors[2];
        Coords  texture;
} Vertex;

typedef struct _GammaSize{
         unsigned channel : 10 ;
}GammaSize;

typedef struct _PP1_Xmap_Regs {
        char            *str;
        __uint32_t      reg;
        __uint32_t      Mode;
}PP1_Xmap_Regs;

typedef GLushort * (*pfS)(GLushort*, GLushort*);
typedef GLubyte * (*pfB)(GLubyte*, GLubyte*);

typedef struct _HQ3_DMA_Params {
                __uint32_t DL_jump_nowait[2];
                __uint32_t DL_jump_wait[2];
                __uint32_t DL_push[2];
                __uint32_t DL_return;
                __uint32_t DL_push_addr;
                __uint32_t DL_table_base_addr[2];

                __uint32_t Stack_RAM_wd0_save;
                __uint32_t Stack_RAM_wd1_save;

                __uint32_t IM_pg_list[4];
                __uint32_t IM_pg_width;
                __uint32_t IM_page_height;
                __uint32_t IM_row_offset;
                __uint32_t IM_row_start_addr;
                __uint32_t IM_line_cnt;
                __uint32_t IM_first_sp_width;
                __uint32_t IM_last_sp_width;
                __uint32_t IM_y_dec_factor;
                __uint32_t IM_DMA_ctrl;
                __uint32_t IM_pg_number_range;
                __uint32_t IM_table_base_addr1[2];
                __uint32_t IM_table_base_addr2[2];
                __uint32_t IM_table_base_addr3[2];

                __uint32_t GL_table_base_addr[2];

                __uint32_t DMA_pg_size;
                __uint32_t DMA_pg_number_range_Im1;
                __uint32_t DMA_pg_number_range_Im2;
                __uint32_t DMA_pg_number_range_Im3;
                __uint32_t DMA_pg_number_range_GL;
                __uint32_t DMA_pg_number_range_DL;
                __uint32_t MAX_Im1_Tbl_ptr_offset;
                __uint32_t MAX_Im2_Tbl_ptr_offset;
                __uint32_t MAX_Im3_Tbl_ptr_offset;
                __uint32_t MAX_GL_Tbl_ptr_offset;
                __uint32_t MAX_DL_Tbl_ptr_offset;

                __uint32_t HAG_states_flgs;
                __uint32_t HAG_state_remainder;

                __uint32_t Restore_CFIFO;
                __uint32_t GIO_Restore_Address;

                __uint32_t pool_id;
                __uint32_t phys_base;
                __uint32_t page_size;
                __uint32_t physpages;
                __uint32_t physpages_per_impage;
} HQ3_DMA_Params;

typedef struct _RSS_DMA_Params {
                __uint32_t numRows;
                __uint32_t numCols;
                __uint32_t pixComponents;
                __uint32_t pixComponentSize;
                __uint32_t x;
                __uint32_t y;
                __uint32_t beginSkipBytes;
                __uint32_t strideSkipBytes;
                __uint32_t xfrDevice;
                __uint32_t rss_clrmsk_msb_pp1;
                __uint32_t rss_clrmsk_lsba_pp1;
                __uint32_t rss_clrmsk_lsbb_pp1;
                __uint32_t checksum_only;
} RSS_DMA_Params;

typedef struct 	_hq3reg {
		char *name;
		__uint32_t addr_offset;
		__uint32_t      mask;
} hq3reg;

typedef struct _hq4reg {
        char *name;
        __uint32_t address;
        __uint32_t mode;
        __uint32_t      mask;
        __uint32_t def;
} hq4reg ;

typedef struct	_rdram_error_info {
	__uint32_t	rssnum;
	__uint32_t	ppnum;
	__uint32_t	rdnum;
	__uint32_t	rd_addr;
	__uint32_t	x;
	__uint32_t	y;
	__uint32_t	exp;
	__uint32_t	rcv;
} rdram_error_info;

/**************************************************************************
 *                 FUNCTION PROTOTYPES                                    *
 **************************************************************************/ 
/* Common */
extern char * atob(const char *cp, int *iptr);
extern 	void us_delay(uint);
void 	flush_cache(void);
int	report_passfail(_Test_Info *Test, int errors);
void 	dbl_load(__uint64_t *, __uint64_t *);
void 	ide_store_double(__uint32_t , __uint32_t, __uint32_t);
int 	setled(int argc,char *argv[]);
void	continuity_check(_Test_Info *Test, int CmapId, __uint32_t Patrn, \
		int errors);
void	mgras_gfx_setup(int index, int numge);
void	compare(char *, int, unsigned int, unsigned int, unsigned int, \
		__uint32_t *errors);
void	gfxreset(void);
void	resetcons(int argc, char **argv);
__uint32_t	fpusetup(void);
__uint32_t	mgras_Reset(__uint32_t argc, char **argv);
__uint32_t	mgras_Init(void);
__uint32_t	mgras_GetMon_Type(void) ;
void		_mgras_init_gold_crc(void);
__uint32_t	mgras_is_solid(void);
void		__MgrasReset(void);
void		mg_pon_puts(__uint32_t argc, char **argv);
void		_mg_set_ZeroGE(void);
void		_mg_set_2GE(void);
void		pon_puts(char *);
int		mgras_baseindex(mgras_hw *base);
__uint32_t	ide_log(__uint32_t num);
float		floorf(float);
float		ceilf(float);
float		fabs_ide(float);
void		_mgras_setup_gio64arb(void);


/* HQ3 */
int		hq3_pio_rd_re(int offset, __uint32_t mask, __uint32_t *actual, \
			__uint32_t exp, int status);
int		wait_for_cfifo(int count, int time_out_value);
__uint32_t      mgras_HQ3dctctrl_test(void);
__uint32_t      mgras_HQGIObus(void);
__uint32_t      mgras_HQbus(void);
__uint32_t      mgras_tlb_test(void);
__uint32_t      mgras_hq3_reif_reg_test(void);
__uint32_t      mgras_hq3_hag_reg_test(void);
__uint32_t      mgras_ucode_test(void);
__uint32_t      mgras_RSS_DataBus(void);
__uint32_t      mgrasDownloadHQ3Ucode(mgras_hw *mgbase, \
			__uint32_t *UcodeTable, __uint32_t UcodeTableSize, \
			__uint32_t print_msg);
__uint32_t      mgras_HQCP_com(hq3_cp_test cp_test);
__uint32_t      mgras_HQ_CP_test(void);
__uint32_t      mgras_HQCP_GE_Rebus_HQ_test(void);
__uint32_t      mgras_HQCP_GE_Rebus_RE_test(void);
__uint32_t      mgras_cfifo_test(void);
__uint32_t      mgras_hq3_converter(void);
__uint32_t      mgras_stuff_test(void);
__uint32_t      mgras_conv_test16(void);
__uint32_t      mgras_conv_test8(void);
__uint32_t      mgras_conv_test32(void);

/* HQ4 */
void			mgras_brdcfg_write(__uint32_t argc, char **argv);
void			mgras_pixel_clock_doubler_write(__uint32_t argc, char **argv);
void			_mgras_pcd_write(uchar_t *pPLLbyte);

/* DMA */
__uint32_t      mgras_hq_redma_PP1(int argc, char **argv);
__uint32_t      mgras_hq_redma_TRAM(int argc, char **argv);
__uint32_t      _mgras_hq_redma(__uint32_t destDevice, __uint32_t hqWriteMode,\
                        __uint32_t reWriteMode, __uint32_t hqReadMode,  \
                        __uint32_t reReadMode, __uint32_t pattern,      \
                        __uint32_t row_cnt, __uint32_t col_cnt,         \
                        __uint32_t y_end, __uint32_t x_end,             \
                        __uint32_t db_enable, __uint32_t do_readback,   \
                        __uint32_t stop_on_err);
void            _mgras_hq_re_dma_init(void);
__uint32_t      _mgras_hqpio_redma_write(RSS_DMA_Params *rssDmaPar,     \
                        __uint32_t reWriteMode, __uint32_t *data,       \
                        __uint32_t db_enable);
void            _mgras_setupTile( RSS_DMA_Params *rssDmaPar, __uint32_t *data,\
                        __uint32_t pattern, __uint32_t *readdata);
__uint32_t      _mgras_hqdma_redma_rw(RSS_DMA_Params *rssDmaPar,        \
                        HQ3_DMA_Params *hq3DmaPar, __uint32_t reWriteMode, \
                        __uint32_t reReadMode, __uint32_t *data,        \
                        __uint32_t rwFlag, __uint32_t db_enable);
__uint32_t      _mgras_hqpio_redma_compare(RSS_DMA_Params *rssDmaPar,   \
                        __uint32_t *expected_data, __uint32_t *read_data, \
                        __uint32_t db_enable);
__uint32_t      _mgras_hqdma_redma_read_compare( RSS_DMA_Params *rssDmaPar, \
                        HQ3_DMA_Params *hq3DmaPar, __uint32_t reReadMode, \
                        __uint32_t *data, __uint32_t db_enable);
__uint32_t      _mgras_dma_hag_setup( HQ3_DMA_Params *hq3DmaPar,        \
                        __uint32_t dmaCtrl, __uint32_t *data);
__uint32_t      _mgras_hq_re_dma_WaitForDone(__uint32_t is_read,        \
                        __uint32_t is_dmapio);
__uint32_t      _mgras_DMA_TETRSetup(RSS_DMA_Params *rssDmaPar,         \
                        __uint32_t readOrWrite, __uint32_t db_enable);
__uint32_t      _mgras_DMA_TexReadSetup(RSS_DMA_Params *rssDmaPar,      \
                        __uint32_t dmakind, __uint32_t readOrWrite,     \
                        __uint32_t do_it, __uint32_t fb_linelength,     \
                        __uint32_t db_enable);
__uint32_t      _mgras_hqpio_redma(__uint32_t reReadMode,               \
                        __uint32_t reWriteMode, __uint32_t row_cnt,     \
                        __uint32_t col_cnt, __uint32_t stop_on_err);
__uint32_t      _mgras_hqpio_redma_read_compare(RSS_DMA_Params *rssDmaPar, \
                        __uint32_t reReadMode, __uint32_t *data,        \
                        __uint32_t db_enable);
void            _mgras_hq_re_dma_end(void);
__uint32_t      mgras_host_hqdma(void);
__uint32_t      mgras_host_hq_cp_dma(void);
__uint32_t      _mgras_host_hq_dl_dma(__uint32_t nWords, __uint32_t *data, \
                        __uint32_t cmd_addr);
__uint32_t      _mgras_dma_re_setup (RSS_DMA_Params *rssDmaPar,         \
                        __uint32_t dmakind, __uint32_t readOrWrite,     \
                        __uint32_t db_enable);
__uint32_t	mgras_hqpio_redma(int argc, char **argv);

void		__write_rss_reg_noprintf(__uint32_t regnum, __uint32_t data,\
			 __uint32_t mask);
void		__write_rss_reg_printf(__uint32_t regnum, __uint32_t data, \
			__uint32_t mask);
void		__write_rss_reg_dbl(__uint32_t regnum, __uint32_t data_hi, \
			__uint32_t data_lo);
void		_hq3_pio_wr_re(__uint32_t offset, __uint32_t val, \
			__uint32_t mask);
void		_hq3_pio_wr_re_ex(__uint32_t offset, __uint32_t val, 	\
			__uint32_t mask);

/* GE11 */
void            HQ3_DIAG_ON(void);
void            HQ3_DIAG_OFF(void);
void            GE11_DIAG_ON(void);
void            GE11_DIAG_OFF(void);
void            GE11_ROUTE_GE_2_HQ(void);
void            GE11_ROUTE_GE_2_RE(void);
__uint32_t      mgras_set_active_ge(int argc, char** argv);
__uint32_t      GE11_Diag_read_Dbl(__uint32_t mask, __uint32_t *data_h, \
			__uint32_t *data_l);
__uint32_t      GE11_Diag_read(__uint32_t mask);
__uint32_t      mgras_hq3_ge11_diagpath(int argc, char** argv);
__uint32_t      mgras_hq3_ge11_diag_wr(int argc, char** argv);
__uint32_t      mgras_hq3_ge11_ucode_wr(int argc, char** argv);
__uint32_t      mgras_hq3_ge11_ucode_rd(int argc, char** argv);
__uint32_t      mgras_hq3_ge11_diag_rd(int argc, char **argv);
__uint32_t      mgras_hq3_ge11_reg_test(int argc, char** argv);
__uint32_t      mgras_GE11_ucode_DataBus(void);
__uint32_t      mgras_GE11_ucode_AddrBus(void);
__uint32_t      mgras_GE11_ucode_Walking_bit(void);
__uint32_t      mgras_GE11_ucode_AddrUniq(void);
__uint32_t      mgras_GE11_ucode_mem_pat(void);
__uint32_t      mgras_ge11_cram(void);
__uint32_t      mgras_ge11_wram(void);
__uint32_t      mgras_ge11_eram(void);
__uint32_t      mgras_ge11_dreg(void);
__uint32_t      mgras_ge11_areg1(void);
__uint32_t      mgras_ge11_aalu3(void);
__uint32_t      mgras_ge11_iadd(void);
__uint32_t      mgras_ge11_imul(void);
__uint32_t      mgras_ge11_inst(void);
__uint32_t      mgras_ge11_mul_stress(void);
__uint32_t      mgras_nextge(void);
__uint32_t      mgras_rebus_read(void);
__uint32_t      mgras_ge_power(void);




/* Back End */
/* CRC */
__uint32_t      CrcCompare(void);
void    	ComputeCrc( __uint32_t testPattern);
void    	ResetDacCrc(void) ;
void    	GetSignature(void);
int     	mgras_DCB_PixelPath(int argc, char **argv);
int     	mgras_DCB_WalkOnePixelPath(int argc, char **argv);
void    	LoadDiagScanReg( ushort_t RGBmode,  __uint32_t RGBpatrn) ;
void    	GammaTableLoad( ushort_t GammaRamp);
void    	_PrintGammaTable(void);
void    	GetComputeSig(__uint32_t WhichCmap, __uint32_t testPattern);

/* CMAP */
void    	mgras_CmapSetup (mgras_hw *);
void    	mgras_LoadCmap(mgras_hw *, __uint32_t, char *, __uint32_t);
void		mgras_ReadCmap(__uint32_t CmapId,  __uint32_t StAddr, \
			char *data, __uint32_t length);
__uint32_t 	_mgras_CmapAddrsBusTest( __uint32_t WhichCmap);
__uint32_t 	_mgras_CmapAddrsUniqTest( __uint32_t WhichCmap);
__uint32_t 	_mgras_CmapPatrnTest( __uint32_t WhichCmap);
__uint32_t 	_mgras_CmapDataBusTest( __uint32_t WhichCmap);
__uint32_t 	_mgras_Cmap( __uint32_t WhichCmap);
void    	CmapLoad( ushort_t Ramp) ;
int 		mgras_CmapAddrsBusTest(int argc, char **argv);
int 		mgras_CmapAddrsUniqTest(int argc, char **argv);
int 		mgras_CmapPatrnTest(int argc, char **argv);
int 		mgras_CmapDataBusTest(int argc, char **argv);
int 		mgras_Cmap(int argc, char **argv);
int 		mgras_CmapProg(int argc, char **argv);
__uint32_t	 mgras_CmapUniqTest(void);
int 		mgras_PeekCmap(int argc, char **argv);
int 		mgras_PokeCmap(int argc, char **argv);
__uint32_t 	mgras_CmapRevision(void);
void		mgras_cmapToggleCblank(mgras_hw *mgbase, int OnOff);
void		cmap_compare(char *, int, int, unsigned int, unsigned int, \
			unsigned int, __uint32_t *errors);

/* DAC */
void 		mgras_ReadDac( ushort_t addr, GammaSize *red, GammaSize *green,\
			GammaSize *blue,  ushort_t length);
void 		mgras_WriteDac( ushort_t addr,GammaSize *red, GammaSize *green,\
          		GammaSize *blue,  ushort_t length,  ushort_t mask);
__uint32_t 	mgras_DacCompare(char *TestName, GammaSize *exp,  \
			GammaSize *rcv, ushort_t length,  ushort_t mask);
__uint32_t 	mgras_ClrPaletteAddrUniqTest(void);
__uint32_t 	mgras_ClrPalettePatrnTest(void);
__uint32_t 	mgras_ClrPaletteWalkBitTest(void);
__uint32_t 	mgras_DacAddrRegTest(void);
__uint32_t 	mgras_DacModeRegTest(void);
__uint32_t 	mgras_DacCtrlRegTest(void);
__uint32_t 	_mgras_DacPLLInit( __uint32_t TimingSelect);
__uint32_t 	mgras_DacReset(void);

/* VC3 */
__uint32_t	mgras_VC3Reset(void);
__uint32_t 	mgras_GetMon_Type(void) ;
void 		mgras_StartTiming(void);
void 		mgras_LoadVC3SRAM( __uint32_t addr,  ushort_t *data,  \
			__uint32_t length,  __uint32_t DataWidth);
void 		mgras_GetVC3SRAM( __uint32_t addr,  ushort_t *data,  \
			__uint32_t length,  __uint32_t DataWidth);
void 		mgras_VC3DisableDsply(void);
void 		mgras_VC3EnableDsply(void);
void 		mgras_VC3CursorEnable(void);
void 		mgras_VC3CursorDisable(void);
__uint32_t  	mgras_VC3CursorPositionTest(void);
__uint32_t  	_mgras_VC3CursorMode( __uint32_t mode);
__uint32_t  	mgras_VC3CursorMode(int argc, char **argv);
void 		mgras_VC3ClearCursor(void);
__uint32_t 	mgras_VC3AddrsBusTest(void);
__uint32_t 	mgras_VC3DataBusTest(void) ;
__uint32_t 	mgras_VC3AddrsUniqTest(void) ;
__uint32_t 	mgras_VC3PatrnTest(void) ;
int 		mgras_VC3Init(int argc, char **argv);
void 		mgras_StopTiming(void);
void 		_vc3_dump_regs();
__uint32_t 	_mgras_VC3Init(__uint32_t TimingSelect);
void 		mgras_MaxPLL_NoTiming(void);
__uint32_t 	mgras_VC3InternalRegTest(void);

/* XMAP */
int		_mgras_XmapDibTest(char *str,  __uint32_t addr);
int 		_mgras_XmapDcbRegTest( __uint32_t WhichPP1) ;
int 		mgras_XmapDcbRegTest(int argc, char **argv);


/* RSS */
__uint32_t	_mgras_rss_init(__uint32_t rssnum);
__uint32_t      re_checkargs(__uint32_t rssnum, __uint32_t regnum,      \
                        __uint32_t rflag, __uint32_t gflag);
void            __glMgrRE4Texture_diag (__uint32_t texmode2_bits,       \
                        __uint32_t reg, double val);
void		comp_bits_to_fb_bits(__uint32_t component, __uint32_t err_bits);
__uint32_t	_xy_to_rdram( __uint32_t x, __uint32_t y, 		\
			__uint32_t algorithm, __uint32_t mode, 		\
			__uint32_t exp_hi, __uint32_t exp_lo, 		\
			__uint32_t *actual_hi,__uint32_t *actual_lo, 	\
			__uint32_t drb_ptr);
__uint32_t	re_or_te_or_pp(__uint32_t regnum, __uint32_t *real_mask);
__uint32_t	rdwr_RSSReg( __uint32_t rssnum, __uint32_t regnum, 	\
			__uint32_t data, __uint32_t mask, __uint32_t access, \
			__uint32_t is_re4, __uint32_t dbl, __uint32_t data2, \
			__uint32_t mode, __uint32_t ld_ex);
__uint32_t	test_RSSRdWrReg(__uint32_t loc_rssnum,char * str,	\
			__uint32_t regnum, __uint32_t mask, 		\
			__uint32_t stop_on_err, __uint32_t is_re4);
int		rdram_checkargs( __uint32_t rssnum, __uint32_t rdram_addr, \
			__uint32_t ppnum, __uint32_t rflag, __uint32_t pflag, \
			__uint32_t aflag, __uint32_t rdend_addr, 	\
			__uint32_t is_reg);
__uint32_t	mkDMAbuf(GLenum datatype, GLenum format, long xsizet,	\
			long ysizet, __uint32_t *image, __uint32_t *dmaptr, \
			__uint32_t test);
void		__glMgrRE4LineSlope_diag(__uint32_t, float, float);
void		__glMgrRE4LineAdjust_diag(__uint32_t, float);
void 		__glMgrRE4Z_diag(__uint32_t reg, __uint32_t val0, 	\
			__uint32_t val1, __uint32_t kludge);

int		_mg0_block(__uint32_t, float, float, float, float, \
			__uint32_t, __uint32_t, __uint32_t, __uint32_t);
void		__glMgrRE4ColorSgl_diag(__uint32_t, float);
void		__glMgrRE4Position2_diag(__uint32_t, float, float);
__uint32_t	_mgras_pt(float, float, float, float, float, float,	\
			__uint32_t, __uint32_t, __uint32_t, __uint32_t);
__uint32_t	_draw_setup(__uint32_t, __uint32_t);
int		_load_aa_tables(void);
void		__glMgrRE4PP1_diag(__uint32_t reg, __uint32_t val);
void		__glMgrRE4Int_diag(__uint32_t reg, __uint32_t val);
void		__glMgrRE4IR_diag(__uint32_t val);
void		__glMgrRE4TEIR_diag(__uint32_t val);
__uint32_t	mg0_clear_color(int argc, char ** argv);
void		_fb_rgb121212_to_comp(__uint32_t val_hi, __uint32_t val_lo, \
			__uint32_t *red, __uint32_t *green, __uint32_t *blue);
void		_fb_rgba888_to_comp( __uint32_t val_hi, __uint32_t val_lo, \
			__uint32_t *red, __uint32_t *green, __uint32_t *blue, \
			__uint32_t *alpha);
void		_convert_fb_to_comp(__uint32_t mode, __uint32_t val_hi,	\
			__uint32_t val_lo);
int		_mg0_FillTriangle(Vertex *max_pt, Vertex *mid_pt, 	\
			Vertex *min_pt, __uint32_t depthkludge);
void		mkImage(GLenum datatype, GLenum format, long xsizet,	\
			long ysizet, void *image, GLuint offset,	\
			__uint32_t test);
void		_draw_char(__uint32_t, ushort_t*, __uint32_t xstart,	\
			__uint32_t, __uint32_t, __uint32_t, float, 	\
			float, float, float);
__uint32_t	_check_crc_sig(__uint32_t pattern, __uint32_t index);	
int		_mg0_Line(Vertex *, Vertex *, __uint32_t, __uint32_t,	\
			__uint32_t, __uint32_t, __uint32_t, __uint32_t,	\
			__uint32_t, RSS_DMA_Params *);
void 		_re4func_setup( __uint32_t rssnum, __uint32_t numCols,	\
        		__uint32_t numRows, RSS_DMA_Params *rssDmaPar,	\
        		HQ3_DMA_Params *hq3DmaPar, __uint32_t destDevice, \
        		__uint32_t do_clear);
__uint32_t 	_readback( __uint32_t rssnum, __uint32_t numRows,	\
        		__uint32_t numCols, __uint32_t *data,		\
        		__uint32_t reReadMode, __uint32_t db_enable,	\
        		RSS_DMA_Params *rssDmaPar, HQ3_DMA_Params *hq3DmaPar,\
        		__uint32_t pattern, __uint32_t exp_crc_index,	\
        		__uint32_t do_crc, __uint32_t dma_xstart,	\
        		__uint32_t dma_ystart, __uint64_t target_checksum, \
			__uint64_t target_checksum_odd);
int		tetr_checkargs(__uint32_t rssnum, __uint32_t regnum,	\
			 __uint32_t rflag, __uint32_t gflag);
int		_mg_tex_poly(__uint32_t tex_en);
int		_ldrsmpl_poly(void);
int		_tex_terev(void);
void 		_mg0_rect( __uint32_t llx, __uint32_t lly, __uint32_t urx, \
        		__uint32_t ury, __uint32_t max_r, __uint32_t max_g,\
        		__uint32_t max_b, __uint32_t max_a, 		\
			__uint32_t do_compare, __uint32_t rgb_mode);
__uint32_t	_tex_load(void);
int	 	_tex_scistri(void);
int	 	_tex_linegl(void);
int	 	_tex_1d(void);
int	 	_tex_3d(void);
int	 	_tex_persp(void);
int	 	_tex_mag(void);
int	 	_tex_detail(void);
int	 	_tex_border_8Khigh(void);
int	 	_tex_8Kwide(void);
int	 	_tex_lut4d(void);
int	 	_tex_mddma(void);
int 		_verif_setup(void);
int 		_send_data( __uint32_t height, __uint32_t bytes_per_line, \
			__uint32_t test);

/**************************************************************************
 *                 VARIABLE PROTOTYPES                                    *
 **************************************************************************/ 
extern	mgras_hw		*mgbase;
extern	struct			mgrasboards	MgrasBoards[];
extern	__uint32_t		_gio64_arb;
extern	__uint32_t		mgras_scratch_buf[MGRAS_CMAP_NCOLMAPENT];
extern	_Test_Info 		BackEnd[];
extern 	_Test_Info 		TRAM_err[];
extern 	_Test_Info 		TE_err[];
extern 	_Test_Info 		RDRAM_err[];
extern 	_Test_Info 		PP_err[];
extern 	_Test_Info 		RE_err[];
extern 	_Test_Info 		HQ3_err[];
extern 	_Test_Info 		GE11_err[];
extern 	_Test_Info 		DMA_err[];
extern 	_Test_Info 		HQ4_err[];
extern 	struct 			mgras_hinv 	MgHinv;
extern	__uint32_t		patrn[12];
extern 	ushort_t 		patrnShort[12];
extern 	ushort_t 		walk_1_0[34];
extern 	__uint32_t 		walk_1_0_32_rss[64];
extern 	__uint32_t  		walk_1_0_32[8];
extern 	ushort_t 		walking_patrn[16];
extern	input			Present,			*In ;
extern	input			Previous, 	*Prev ;
extern	input			Signature, 	*ExpSign ;
extern	input			HWSignature,	*RcvSign ;
extern	__uint32_t		numRE4s;
extern	__uint32_t		numTRAMs;
extern	__uint32_t		hq3_config, gio_config;
extern	__uint32_t		orig_gio_config, orig_hq3_config;
extern	HQ3_DMA_Params		mgras_Hq3DmaParams;
extern	RSS_DMA_Params		mgras_RssDmaParams;
extern	__uint32_t		trans_table[];
extern	__uint32_t		PIXEL_TILE_SIZE;
extern 	__uint32_t 		rssstart;
extern 	__uint32_t 		rssend;
extern 	__uint32_t 		num_tram_comps;
extern 	__uint32_t 		write_only;
extern 	__uint32_t 		dmawrite_only;
extern 	__uint32_t 		dmaread_only;
extern 	__uint32_t 		nocompare;
extern 	__uint32_t 		tram_mem_pat;
extern 	__uint32_t 		trampg_start;
extern 	__uint32_t 		pp_dma_pat;
extern 	__uint32_t 		pp_dma_mode;
extern __uint32_t 		ppstart;
extern __uint32_t 		ppend;
extern __uint32_t 		rdstart;
extern __uint32_t 		rdend;
extern __uint32_t 		re_racstart;
extern __uint32_t 		re_racend;
extern __uint32_t 		pp_racstart;
extern __uint32_t 		pp_racend;
extern __uint32_t 		numRE4s;
extern __uint32_t 		green_texlut;
extern __uint32_t 		blue_texlut;
extern __uint32_t 		alpha_texlut;
extern 	__uint32_t 		DMA_WRLine[24];
extern 	__uint32_t 		DMA_RDRAM_PIORead_RDLine[24];
extern 	ushort_t 		DMA_WRLine16[24];
extern 	__uint32_t 		DMA_RDData[1];
extern 	__uint32_t 		*DMA_WRTile;
extern 	__uint32_t 		*DMA_WRTile_notaligned;
extern 	ushort_t 		DMA_WRTile16[256];
extern 	__uint32_t 		*DMA_RDTile;
extern 	__uint32_t 		*DMA_RDTile_notaligned;
extern 	__uint32_t 		*DMA_CompareTile;
extern 	__uint32_t 		DMA_CompareTile_notaligned[];
extern 	__uint32_t 		*DMA_WRTile_ZERO;
extern 	__uint32_t 		DMA_WRTile_ZERO_notaligned[];
extern 	ushort_t 		DMA_WRTile16_ZERO[1];
extern 	__uint32_t 		dma_overwrite_flag;
extern 	__uint32_t 		tram_wr_flag;
extern 	__uint32_t 		tram_wr_data;
extern 	__uint32_t 		pp_wr_flag;
extern 	__uint32_t 		pp_wr_data;
extern 	__uint32_t 		set_xstart;
extern 	__uint32_t 		set_ystart;
extern 	__uint32_t 		set_dma;
extern 	__uint32_t 		YMAX;
extern 	__uint32_t 		XMAX;
extern 	__uint32_t 		pp_drb;
extern 	__uint32_t 		global_stop_on_err;
extern 	__uint64_t 		read_checksum;
extern	RSS_Reg_Info_t		te_rwreg[];
extern	__uint32_t		tri_shade_en;
extern	__uint32_t		tri_tex_en;
extern	__uint32_t		rgb121212;
extern	__uint32_t		is_old_pp;
extern	__uint32_t		dith_off;
extern	float			magic_num;
extern	__uint32_t		clean_disp;
extern	__uint32_t		tri_dith_en;
extern	__uint32_t		te_rev;
extern	int			activeGe;
extern	__uint32_t		ge11_diag_a_addr;
extern	__uint32_t		ge11_diag_d_addr;
extern void _mgras_store_rdram_error_info(__uint32_t rssnum, __uint32_t ppnum,\
	__uint32_t rdnum, __uint32_t rd_addr, __uint32_t x, __uint32_t y, \
	__uint32_t exp, __uint32_t rcv);
extern __uint32_t global_dbuf;
extern __uint32_t 		XMAX;
extern __uint32_t 		YMAX;
extern struct mgras_timing_info	*mgras_video_timing[MGRAS_MAXBOARDS];
extern struct mgras_timing_info	*mg_tinfo;

#if HQ4
#include "mgras_xbow.h"

__uint32_t _mgras_hq4linkreset (xbowreg_t port);
__uint32_t _mgras_islinkalive (xbowreg_t port);
__uint32_t _mgras_hq4probe(mgras_hw *, xbowreg_t );
__uint32_t rgba8out_read_compare (char *, __uint64_t, __uint64_t *, uchar_t);
__uint32_t te_load_bit_test(void);
__uint32_t rgba8out_read_compare (char *, __uint64_t, __uint64_t *, uchar_t);
__uint32_t mgras_rtpio_loop( int argc, char **argv );
void _fill_buffers (uchar_t *,  uchar_t *, __uint32_t, __uint32_t);
void rtdma2dcb_setup (uchar_t *, __uint32_t, realdma_words_t *, vdma_tdma_addr_t *);
void rtdma_records_setup(__uint32_t *, __uint32_t, realdma_words_t *, vdma_tdma_addr_t *);
__uint32_t te_load_bit_test (void);
__uint32_t mgras_vc3_dcbdma (uchar_t *, __uint32_t);

extern	_mg_xbow_portinfo_t mg_xbow_portinfo_t[6];
extern hq4reg hq4regs_t[];

extern void _mgras_create_buffer(void);
extern uchar_t mgras_checkdefs  (void );
extern uchar_t mgras_ident_test (void ) ;

extern __uint32_t mgras_walk1s0s (void );
extern uchar_t  PCD_1280_1024_60 [7];


#endif /* HQ4 */

extern __uint64_t DMA_wr_data[SIZE_DMA_WR_DATA/8];
extern __uint64_t DMA_rd_data[SIZE_DMA_WR_DATA/8];

extern realdma_words_t realdma_words, realdma_recv;
extern vdma_tdma_addr_t vdma_addr, tdma_addr;
extern realdma_words_t tbl_ptr_send[256], tbl_ptr_recv[256] ;

extern vdma_tdma_ctlreg_t vdma_cntl, tdma_cntl;
extern tdma_format_ctl_t tdma_format;

extern __uint64_t  exp_64 ,  recv_64 ;
extern __uint32_t  exp_32 ,  recv_32 ;
extern __uint32_t d_errors ;
extern __uint32_t flag_1 , flag_0  ;
extern uchar_t in_format, out_format;
extern __uint32_t _mgras_dcbdma_enabled ;

extern void _mgras_disp_rdram_error(void);
extern int _mg_alphablend(void);

#endif /* __MGRAS_PROTO_H__ */
