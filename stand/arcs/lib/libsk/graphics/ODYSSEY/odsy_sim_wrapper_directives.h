#ifndef __ODSY_SIM_WRAPPER_CRUFT_H_
#define __ODSY_SIM_WRAPPER_CRUFT_H_
/*
** Copyright 1998, Silicon Graphics, Inc.
** All Rights Reserved.
**
** This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
** the contents of this file may not be disclosed to third parties, copied or
** duplicated in any form, in whole or in part, without the prior written
** permission of Silicon Graphics, Inc.
**
** RESTRICTED RIGHTS LEGEND:
** Use, duplication or disclosure by the Government is subject to restrictions
** as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
** and Computer Software clause at DFARS 252.227-7013, and/or in similar or
** successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
** rights reserved under the Copyright Laws of the United States.
**
*/

#ifdef ODSY_DEFINE_DIRECTIVES


int ODSY_WRITE_SIM_DIRECTIVE_rpoll(odsy_hw *hw,
				   __uint64_t addr, 
				   unsigned   tnum, 
				   __uint32_t data,
				   __uint32_t mask,
				   __uint32_t count);
int ODSY_WRITE_SIM_DIRECTIVE_rsnoop(odsy_hw *hw,
				    __uint64_t addr,
				    __uint32_t expected,
				    __uint32_t mask,
				    unsigned   count);

#define RINTR_CHANNEL_FLAG     (0)
#define RINTR_CHANNEL_XT       (1)
#define RINTR_CHANNEL_DMA      (2)
#define RINTR_CHANNEL_VR       (3)
#define RINTR_CHANNEL_SSYNC    (4)
#define RINTR_CHANNEL_SWAPBFR  (5)
int ODSY_WRITE_SIM_DIRECTIVE_rintr(odsy_hw *hw,
				   unsigned   channel,
				   unsigned   block, 
				   __uint32_t count);
int ODSY_WRITE_SIM_DIRECTIVE_f(odsy_hw * hw);

#define J_OPCODE_JUMP  "JUMP"
#define J_OPCODE_FLUSH "FLUSH"

#define J_STREAM_DMA   "DMA"
#define J_STREAM_GFX   "GFX"
#define J_STREAM_DBE   "DBE"
int ODSY_WRITE_SIM_DIRECTIVE_j(odsy_hw *hw,
			       char     *opcode, 
			       char     *stream, 
			       unsigned  cycles);

#define S_MAX_U_PKT  9 
int ODSY_WRITE_SIM_DIRECTIVE_s(odsy_hw *hw,  __uint32_t stall_states[S_MAX_U_PKT]);
int ODSY_WRITE_SIM_DIRECTIVE_g(odsy_hw *hw,
			       __uint32_t enables,
			       unsigned   count);
int ODSY_WRITE_SIM_DIRECTIVE_d0(odsy_hw *hw,
				__uint32_t enables,
				unsigned   count);
int ODSY_WRITE_SIM_DIRECTIVE_d1(odsy_hw *hw,
				__uint32_t enables,
				unsigned   count);
int ODSY_WRITE_SIM_DIRECTIVE_d2(odsy_hw *hw,
				__uint32_t enables,
				unsigned   count);
int ODSY_WRITE_SIM_DIRECTIVE_d3(odsy_hw *hw,
				__uint32_t enables,
				unsigned   count);
int ODSY_WRITE_SIM_DIRECTIVE_format(odsy_hw *hw,
				    __uint32_t format);
int ODSY_WRITE_SIM_DIRECTIVE_path(odsy_hw *hw,
				  __uint32_t path);

#define XS_BFSZ (11)

#define XS_MODE_VAL      0
#define XS_SQUASH_VAL    1
#define XS_FXBOW_MODE    2
#define XS_FSB_VAL0      3
#define XS_FASB_VAL0     4
#define XS_FADMIN_BEFORE 5
#define XS_FORCE_SB0     6
#define XS_SQUASH_UPKT   7
#define XS_UPKT_STALL0   8
#define XS_SET_BUFFULL   9
#define XS_CLR_BUFFULL  10
int ODSY_WRITE_SIM_DIRECTIVE_xs(odsy_hw *hw,
				unsigned   upkt_nr,
				__uint32_t xs_data[XS_BFSZ]);

#define SDRAM_STREAM_CFIFO_READ      0
#define SDRAM_STREAM_CFIFO_WRITE     1
#define SDRAM_STREAM_DMA_ASYNC_READ  2
#define SDRAM_STREAM_DMA_ASYNC_WRITE 3
#define SDRAM_STREAM_DMA_SYNC_READ   4
#define SDRAM_STREAM_DMA_SYNC_WRITE  5

#define SDRAM_MODE_CONSTANT 0
#define SDRAM_MODE_RANDOM   1
#define SDRAM_MODE_USER     2
int ODSY_WRITE_SIM_DIRECTIVE_sdrammode(odsy_hw *hw,
				       unsigned stream, 
				       unsigned mode);
int ODSY_WRITE_SIM_DIRECTIVE_sdramdel(odsy_hw *hw,
				      unsigned   stream, 
				      __uint32_t delay2gnt,
				      __uint32_t delay2data[16]);
int ODSY_WRITE_SIM_DIRECTIVE_sdramdelecc(odsy_hw *hw);
int ODSY_WRITE_SIM_DIRECTIVE_sdramsamode(odsy_hw *hw);
int ODSY_WRITE_SIM_DIRECTIVE_sdramsacount(odsy_hw *hw);
int ODSY_WRITE_SIM_DIRECTIVE_sdramsaflush(odsy_hw *hw);
int ODSY_WRITE_SIM_DIRECTIVE_crn(odsy_hw *hw,
				 __uint32_t event,
				 __uint32_t custom,
				 __uint32_t assert,
				 __uint32_t delay,
				 __uint32_t trig,
				 __uint32_t type);
int ODSY_WRITE_SIM_DIRECTIVE_ip_toggle(odsy_hw *hw,
				       __uint32_t signal_name,
				       __uint32_t start_delay,
				       __uint32_t assert_for_clocks,
				       __uint32_t deassert_for_clocks);
int ODSY_WRITE_SIM_DIRECTIVE_ip_fifo(odsy_hw *hw,
				     __uint32_t cfifo,
				     __uint32_t xfifo,
				     __uint32_t start_delay,
				     __uint32_t assert_for_clocks,
				     __uint32_t deassert_for_clocks);
int ODSY_WRITE_SIM_DIRECTIVE_ip_hif(odsy_hw *hw,
				    __uint32_t delay,
				    __uint32_t cycles);
int ODSY_WRITE_SIM_DIRECTIVE_ip_token(odsy_hw *hw,
				      __uint32_t delay,
				      __uint32_t is_last);
int ODSY_WRITE_SIM_DIRECTIVE_ie_ctrl(odsy_hw *hw,
				     __uint32_t opcode,
				     __uint32_t delay,
				     __uint32_t cycles);
int ODSY_WRITE_SIM_DIRECTIVE_ie_delay(odsy_hw *hw,
				      __uint32_t opcode,
				      __uint32_t delay);
int ODSY_WRITE_SIM_DIRECTIVE_bf(odsy_hw *hw,
				__uint32_t wait,
				__uint32_t length,
				__uint32_t send_gnts,
				__uint32_t ctp_state);
int ODSY_WRITE_SIM_DIRECTIVE_comment(odsy_hw *hw,
				     char      *iface, 
				     char      *strop, 
				     __uint32_t num);



#else 


/* provide zapped out versions so the code doesn't look nasty */
#define ODSY_WRITE_SIM_DIRECTIVE_rpoll(hw,addr,tnum,data,mask,count) 
#define ODSY_WRITE_SIM_DIRECTIVE_rsnoop(hw,addr,expected,mask,count) 
#define ODSY_WRITE_SIM_DIRECTIVE_rintr(hw,channel,block,count) 
#define ODSY_WRITE_SIM_DIRECTIVE_f(hw) 
#define ODSY_WRITE_SIM_DIRECTIVE_j(hw,opcode,stream,cycles) 
#define ODSY_WRITE_SIM_DIRECTIVE_s(hw,stall_states) 
#define ODSY_WRITE_SIM_DIRECTIVE_g(hw,enables,count) 
#define ODSY_WRITE_SIM_DIRECTIVE_d0(hw,enables,count) 
#define ODSY_WRITE_SIM_DIRECTIVE_d1(hw,enables,count) 
#define ODSY_WRITE_SIM_DIRECTIVE_d2(hw,enables,count) 
#define ODSY_WRITE_SIM_DIRECTIVE_d3(hw,enables,count) 
#define ODSY_WRITE_SIM_DIRECTIVE_format(hw,format) 
#define ODSY_WRITE_SIM_DIRECTIVE_path(hw,path) 
#define ODSY_WRITE_SIM_DIRECTIVE_xs(hw,upkt_nr,xs_data) 
#define ODSY_WRITE_SIM_DIRECTIVE_sdrammode(hw,stream,mode) 
#define ODSY_WRITE_SIM_DIRECTIVE_sdramdel(hw,stream,delay2gnt,delay2data) 
#define ODSY_WRITE_SIM_DIRECTIVE_sdramdelecc(hw) 
#define ODSY_WRITE_SIM_DIRECTIVE_sdramsamode(hw) 
#define ODSY_WRITE_SIM_DIRECTIVE_sdramsacount(hw) 
#define ODSY_WRITE_SIM_DIRECTIVE_sdramsaflush(hw) 
#define ODSY_WRITE_SIM_DIRECTIVE_crn(hw,event,custom,assert,delay,trig,type) 
#define ODSY_WRITE_SIM_DIRECTIVE_ip_toggle(hw,signal_name,start_delay,assert_for_clocks,deassert_for_clocks) 
#define ODSY_WRITE_SIM_DIRECTIVE_ip_fifo(hw,cfifo,xfifo,start_delay,assert_for_clocks,deassert_for_clocks) 
#define ODSY_WRITE_SIM_DIRECTIVE_ip_hif(hw,delay,cycles) 
#define ODSY_WRITE_SIM_DIRECTIVE_ip_token(hw,delay,is_last) 
#define ODSY_WRITE_SIM_DIRECTIVE_ie_ctrl(hw,opcode,delay,cycles) 
#define ODSY_WRITE_SIM_DIRECTIVE_ie_delay(hw,opcode,delay) 
#define ODSY_WRITE_SIM_DIRECTIVE_bf(hw,wait,length,send_gnts,ctp_state) 
#define ODSY_WRITE_SIM_DIRECTIVE_comment(hw,iface, strop,num) 


#endif
#endif
