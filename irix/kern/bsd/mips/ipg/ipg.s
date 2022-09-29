;FDDIXPress "firmware"
;	This code is downloaded to the board each time it is reset.
;
; Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
;	"$Revision: 1.45 $"

;	.equ	RX_CONT,1		;have RX buffer continuations
;	.equ	PERF, 1			;performance test points
;	.equ	CK_DMA_LEN, 1		;look for DMA bug

	.file	"firm.s"

	.equ	FEW_VS,1
	.include "std.h"
	.include "hard.h"
	.include "eprom.h"
	.include "if_ipg_s.h"
	.include "ipg.h"

	.sect	code,text, absolute 0

 .ifdef	PERF
 .macro	PERF_TP,reg,n
  .if	((n) != 0)
	  const	  reg,(n)<<2
	  consth  reg,EPROM_BASE
	  load	  0,BYTE,reg,reg
  .endif
 .endm

 .macro	DEFPERF,nam,val,sw
  .if	((sw) != 0)
	  .equ	  nam,(val)
  .else
	  .equ	  nam,0
  .endif
 .endm
	DEFPERF	PERF_TXA_INT,	    0x01, 0
	DEFPERF	PERF_BUS_READ_INT,  0x02, 0
	DEFPERF	PERF_BUS_WRITE_INT, 0x03, 0
	DEFPERF	PERF_BUS_EXIT,	    0x04, 0
	DEFPERF	PERF_BUS_START,	    0x05, 0
	DEFPERF	PERF_RX_INT,	    0x06, 1
;XXX	DEFPERF	PERF_,		    0x07, 1
	DEFPERF	PERF_IDONE,	    0x08, 1
	DEFPERF	PERF_ODONE,	    0x09, 0
	DEFPERF	PERF_TX_DONE,	    0x0a, 1
	DEFPERF	PERF_SKIM,	    0x0b, 0
	DEFPERF	PERF_HLENS,	    0x0c, 0
	DEFPERF	PERF_TX_START,	    0x0d, 1
	DEFPERF	PERF_RX_OVF,	    0x0e, 1
	DEFPERF	PERF_BUS_DONE,	    0x0f, 1
	DEFPERF	PERF_FOR_CMT,	    0x10, 1
;XXX	DEFPERF	PERF_,		    0x11, 1
	DEFPERF	PERF_PINT,	    0x12, 1
	DEFPERF	PERF_TIMER,	    0x13, 1
	DEFPERF	PERF_BUS_READ,	    0x14, 1
	DEFPERF	PERF_BUS_WRITE,	    0x15, 1
	DEFPERF	PERF_RX_ON,	    0x16, 1

	DEFPERF	PERF_ACT,	    0x40, 1
 .else
 .macro	PERF_TP,reg,n
 .endm
 .endif


;macros to note instructions that must be adjusted for 64-bit hosts
 .macro	ASML, oldval
  .if ((oldval)&255)+MAX_DSML >= 256
	.print " ** ERROR: @oldval@+MAX_DSML might require a carry"
	.err
  .endif
  .if NEXT_ASML>9
	.err
  .endif
  .irep	n, 0,1,2,3,4,5,6,7,8,9
    .if NEXT_ASML==n
	.equ	ASML@n, .
    .endif
  .endr
	.set	NEXT_ASML, NEXT_ASML+1
 .endm
	.set	NEXT_ASML, 0

 .macro	APAD, oldval
  .if ((oldval)&255)+MAX_DPAD >= 256
	.print " ** ERROR: @oldval@+MAX_DPAD might require a carry"
	.err
  .endif
  .if NEXT_APAD>9
	.err
  .endif
  .irep	n, 0,1,2,3,4,5,6,7,8,9
    .if NEXT_APAD==n
	.equ	APAD@n, .
    .endif
  .endr
	.set	NEXT_APAD, NEXT_APAD+1
 .endm
	.set	NEXT_APAD, 0

 .macro	SPAD,oldval
  .if ((oldval)&255) < MAX_DPAD
	.print " ** ERROR: @oldval@-MAX_DPAD might require a carry"
	.err
  .endif
  .if NEXT_SPAD>9
	.err
  .endif
  .irep	n, 0,1,2,3,4,5,6,7,8,9
    .if NEXT_SPAD==n
	.equ	SPAD@n, .
    .endif
  .endr
	.set	NEXT_SPAD, NEXT_SPAD+1
 .endm
	.set	NEXT_SPAD, 0

 .macro	FIX_SML_NET
	ASML	SML_NET
	SPAD	SML_NET
 .endm

	.equ	DATA_START, 0x8000
	.sect	bss,bss,absolute DATA_START

;pointers to input buffers
;   The input buffers appear in a doublely linked circular list.
;   Since the list is circular, there is usually no need to change the links.
;   Advancing pointers and counters suffices to move buffers among
;   the catagories "free", "waiting to be DMAed," and "ready to be examined."
;   Before being DMAed, a frame in a buffer is checked to see if it is
;   bogus and can be discarded.  If so, it is unlinked and moved to the
;   start of the free part of the list.

;A single FDDI frame may span several buffers.  The front end puts the
;   the frame descriptor in yet another buffer.  When the frame is complete
;   in DRAM, the interrupt handler saves a pointer to the buffer descriptor
;   of the DRAM buffer that contains the frame descriptor.  This buffer
;   also contains the start of the next frame.

	.dsect
IBF_P_FOR:	.block	4		;next buffer in the list
IBF_P_BAK:	.block	4		;previous buffer
IBF_P_DSC:	.block	4		;ptr to buffer with frame descriptor
	.equ	IBF_P_DSC_SH,16		; and buffer count-2
	.equ	IBF_P_DSC_MASK,(-1)<<IBF_P_DSC_SH
IBF_P_ADR:	.block	4		;pad--not used
_LEN_IBF_P:	.block	1
	.use	bss

;retain one free buffer for bus packet pad.  count 2 in use by the front end.
;   1 to account for "test before decrement" behavior of jmpfdec.
	.equ	IBF_FREE_INIT, 0x80000000+IBF_NUM-1-2-1

;value to shut down receiving until the front end can be reset
	.equ	IBF_FREE_DEAD, 0x0000ffff


ibf_ptrs:.block IBF_NUM*_LEN_IBF_P

;Obtain the address of the buffer described by a descriptor
;   This macro is quicker than getting an address from SRAM
;   It masks the pointer number out of the address of the descriptor
;   so it can be used to untangle receive descriptors.
	CK	ibf_ptrs < 0xffff
	CK	IBF_LEN == 0x2010 && _LEN_IBF_P == 0x10
	CK	(_LEN_IBF_P & -_LEN_IBF_P) == _LEN_IBF_P
	CK	(ibf_ptrs & -ibf_ptrs) == ibf_ptrs
	.equ	IBF_PTR_MASK, ibf_ptrs-_LEN_IBF_P
;   tgt=target, dsc=buffer descriptor, tmp=temporary
 .macro	IBF_ADR, tgt,dsc,tmp
	  and	  tgt,dsc,ibf_ptr_mask
	  sll	  tmp,tgt,13-4
	  add	  tgt,tgt,tmp
	  add	  tgt,tgt,p_ibf_base
 .endm
	.equ	IBF_ADR_PAUSE,4

	.equ	MISFRM_LIM0, 4-1	;reset front end after 4 misses



;multicast and promiscuous DA hash tables
	.equ	DAHASH_SIZE, 256/8	;256 bits/table
dahash:	.block	DAHASH_SIZE


;shape of host-to-board control blocks
	.dsect
_H2B_OP:	.block	2
_H2B_LEN:	.block	2
_H2B_ADDR:	.block	4
_LEN_H2B_BLK:	.block	1
	.use	bss

;shape of board-to-host control blocks
	.dsect
_B2H_ENTRY:	.block	4
_LEN_B2H_BLK:	.block	1
	.use	bss

;address of host-to-board control ring in host VME space
h2b_buf:    .block  4			;start of H2B buffer in host

;limit interrupts to this
int_delay:  .block  4

;Do not let the work-inferred interrupt delay exceed this.
max_host_work:.block 4

;delay control board-to-host DMA by this
b2h_dma_delay:.block 4

;total maximum host buffers
max_bufs:   .block  4

;limit accumulated checksum credit
cksum_max:  .block  4


;host buffer pools
	DALIGN	MAX_BIG*4*2
bigs:	.block	MAX_BIG*4
	DALIGN	MAX_MED*4*2
meds:	.block	MAX_MED*4
	DALIGN	MAX_SML*4*2
smls:	.block	MAX_SML*4

	.equ	OBF_START, OBF_BASE | DMA2_ADR
	.equ	OBF_CNT_START, 0x80000000-2

	.equ	OBF_DESC, OBF_PAD_LEN<<(8+16)
	.equ	OBF_DESC_LEN, 4



;Advance to next DRAM output buffer
;   This macro used each time a transmission is started.  That is so
;   the following transmission can be started quickly.
 .macro	ADV_OBF,tmp,jloc
	  cpltu   tmp,obf_cur,obf_cur_lim
 .ifnes "@jloc@",""
	  jmpt	  tmp,jloc
	   add	  obf_next,obf_cur,obf_len
	  jmp	  jloc
	   sub	  obf_next,obf_next,obf_restart
 .else
	  add	  obf_next,obf_cur,obf_len
	  sra	  tmp,tmp,31
	  andn	  tmp,obf_restart,tmp
	  sub	  obf_next,obf_next,tmp
 .endif
 .endm


;DMA requests
	.dsect
DLIST_L:   .block	4		;requested length
DLIST_A:   .block	4		;VME address
_LEN_DLIST:.block	1
	DEFBT	DMA_WR,	    31		;write to VME bus
	DEFBT	DMA_C,	    30		;control instead of data
	DEFBT	DMA_FE,	    29		;end of frame, H2B, or B2H DMA
	DEFBT	DMA_FASINT, 28		;immediate host interrupt
	DEFBT	DMA_SLOINT, 27		;slow host interrupt
	DEFBT	DMA_FIN,    26		;VME read DMA finished at interrupt
	.use	bss
;The host is given a "PHY interrupt" using the DMA_FASINT bit associated
;    with the board-to-host DMA transfer which communicated the new
;    line state.

;The host is given a DMA interrupt only after the related input (to host)
;    transfer has been completed, the host has been notified by a
;    board-to-host control DMA transfer, and the host has had time
;    to notice the new data.  The original interrupt request moves from
;    ST2_SLOINT_REQ to DMA_SLOINT to ST2_SLOINT_RDY.

;buffer of DMA requests
	;one DMA entry for each buffer
	;	MAX_DMA_ELEM for each output buffer
	;	2 for each input buffer
	; + 2 for b2h
	; + 1 for h2b
	; + pad for errors in this analysis (none known, but...)
	.equ	DLIST_NUM, MAX_DMA_ELEM*OBF_NUM+2*IBF_NUM+PHY_INT_NUM+2+1+32
	.equ	DLIST_SIZE, DLIST_NUM*_LEN_DLIST
	.block	(0x80000000-.)&0xfff	;waste SRAM just for neatness
dlist:	.block DLIST_SIZE

;advance DMA list pointer.
;   ptr1 is the un-advanced value of either dlist_head or dlist_tail.
;   ptr2 is either dlist_head or dlist_tail.
;   tmp1 and tmp2 are scratch registers.
 .macro INC_DLIST0,ptr2,ptr1,tmp1,tmp2
	  cpgtu	  tmp1,dlist_lim,ptr1
	  sra	  tmp1,tmp1,31
	  add	  tmp2,ptr1,_LEN_DLIST
	  and	  tmp2,tmp2,tmp1
	  andn	  tmp1,dlist_start,tmp1
	  or	  ptr2,tmp1,tmp2
 .endm

;   ptr is either dlist_head or dlist_tail.
;   tmp1 and tmp2 are scratch registers.
 .macro INC_DLIST,ptr,tmp1,tmp2,jloc
	  INC_DLIST0 ptr,ptr,tmp1,tmp2,jloc,
 .endm


	.equ	esram, .
	CK	. < 64*1024
	.eject
;Trap and interrupt vectors
	.use	code

	.word	_main			;start execution here



;Support direct interrupt (VF=0) routines.
 .macro	NXTVEC,	num
 .if (. != num*64*4)
	  CK	  . <= num*64*4
	  .rep	  num*64 - ./4
	  nop
	  .endr
 .endif
 .endm



	.use	code

	CK	. < V_Timer*4*64
	.block	V_Timer*4*64 - .

	CK	. >= EPROM_CODE_SIZE

	NXTVEC	V_Timer
;Timer interrupt handler
;   This should happen a few times per second.

	.equ	TIME_NOW, TIME_INTVL*2
	CK	(TIME_NOW & 0xffff0000) > TIME_INTVL
	.equ	TIME_NEVER,0x80000000

timer_hand:
	PERF_TP it0,PERF_TIMER

	constx	it2,TIME_INTVL | TMR_IE
	mtsr	TMR,it2

	CK	((TIME_INTVL | TMR_IE) & 0xffff) == (TIME_INTVL & 0xffff)
	consth	it2,TIME_INTVL

;   Advance the timers.  Add to each of them the period of the clock,
;   unless they are already too large.
	consth	it3,0x7fffffff-TIME_INTVL-0x10000   ;limit to prevent overflow
						;fudged to allow consth
 .macro CNT_TIMER, reg
	  cplt	  it0,reg,it3
	  sra	  it0,it0,31
	  and	  it0,it0,it2
	  add	  reg,reg,it0
 .endm
	CNT_TIMER h2b_delay
	CNT_TIMER b2h_delay
	CNT_TIMER txs_time
	CNT_TIMER host_qt
	.purgem CNT_TIMER

	add	time_hz,time_hz,it2		;advance in the current second

	constn	bec_da,-1			;notice more beacons

    ;turn on beacon and claim interrupts so we can notice those states
	and	it0,formac_imsk,FORMAC_MORE_INTS
	cpeq	it0,it0,FORMAC_MORE_INTS
	jmpt	it0,timer_hand6
	 or	formac_imsk,formac_imsk,FORMAC_MORE_INTS
	xor	it0,p_formac_stmc,IO_FORMAC_IMSK ^ IO_FORMAC_STMC
	store	0,HWORD,formac_imsk,it0
	FORMAC_REC 5,
timer_hand6:

	and	it0,cur_st1,ST1_LED_ACT1	;turn off fiber-active LED
	sll	it0,it0,ST1_LED_ACT2_BT-ST1_LED_ACT1_BT
	CK	ST1_LED_ACT2_BT > ST1_LED_ACT1_BT
	andn	cur_st1,cur_st1,ST1_LED_ACT1 | ST1_LED_ACT2
	or	cur_st1,cur_st1,it0

	and	it1,cur_st1,ST1_RNGOP	;set ST1_LED_BAD=~ST1_RNGOP
	sll	it1,it1,ST1_LED_BAD_BT - ST1_RNGOP_BT
	CK	ST1_RNGOP_BT < ST1_LED_BAD_BT
	set_st	LED_BAD
	andn	cur_st1,cur_st1,it1

	iret

	.eject
	.equ	V_BUS_START, 15
	NXTVEC	V_BUS_START
;start the bus packet machinery.
;   This is done as an interrupt to simplify conflicts with the bus packet
;	interrupt handler.
;
;   here we know there is something in the DMA list.
bus_start:
	PERF_TP	it0,PERF_BUS_START

	load	0,WORD,bus_h_len,dlist_tail
	CK	DLIST_L==0
	add	it0,dlist_tail,DLIST_A
	load	0,WORD,bus_h_addr,it0
	INC_DLIST dlist_tail,it0,it1
	mov	next_bus_bits,bus_h_len	;save next operation bits
	jmpf	next_bus_bits,bus_r80	;go elsewhere to start a VME read
	 CK	DMA_WR==TRUE
	 consth	bus_h_len,0		;clean the length

;prepare the bus packet machinery for VME writes
	jmpt	cur_bus_bits,bus_w80	;branch if already doing VME writes
	 CK	DMA_WR==TRUE

	 constx	it0,IO_EXPORT_BWR
	const	prev_wfer,1		;must do initial transfer cycle

	or	export,export,it0	;set machinery for VME write
	jmp	bus_w80
	 store	0,WORD,export,p_export

	.eject
	NXTVEC	V_TX_S
;Sync transmit interrupt
;   The only synchronous frames we transmit are beacon and claim frames.

;   This function must be as fast as possible, to avoid violating
;   the MAC standard by being tardy, and not transmitting claim or
;   beacon frames "back-to-back."  It should also be fast to leave some
;   time to handle host commands or CMT on the other PHY.

txs_hand:
	load	0,HWORD,it3,p_txs_ack	;clear the TXS interrupt
	jmpfdec	txs_cnt,txs50
	 mfsr	txs_time,TMC		;time stamp finished transmission

	srl	it3,it3,STAT1_TX_UNDER_BT
	store	0,WORD,txs_dmacs,txs_dmacs  ;continue beaconing or claiming
	and	it3,it3,1
	add	cnt_tx_under,cnt_tx_under,it3
	load	0,WORD,it0,txs_addr
	iret


;see if we should still be beaconing or claiming
txs50:	FE_DELAY 3,
	srl	it3,it3,STAT1_TX_UNDER_BT   ;count transmit underrun
	and	it3,it3,1
	add	cnt_tx_under,cnt_tx_under,it3

;check which kind of frame to transmit only occassionally
	load	0,HWORD,it1,p_formac_stmc

	sll	txs_cnt,it1,31-FORMAC_XMIT5_BT
	sra	txs_cnt,txs_cnt,31	;txs_cnt=-1 if beaconing, else 0
	sll	it2,it1,31-FORMAC_XMIT4_BT
	or	it2,it2,txs_cnt		;it2=either beaconing or claiming

	jmpf	it2,txs99		;stop it
	 and	it2,txs_cnt,(BEACON_BASE^CLAIM_BASE)	;choose buffer

;we have decided to claim or beacon for a while
	store	0,WORD,txs_dmacs,txs_dmacs
	.equ	TXS_FORCE, 0x7fffffff+16    ;16 beacons or claims before check
	constx	txs_cnt,TXS_FORCE
	xor	txs_addr,txs_claim,it2
	load	0,WORD,it2,txs_addr	;start the front end
	iret

txs99:
	rs_st	TXA,,			;note end of latency measurement
 .if	REV29K<=3
	nop
	nop
	CK	.>=(txs99+3*4)
 .endif
	iret

	.eject
	NXTVEC	V_TX_A
;Async transmit interrupt handler
;    (loads and stores interleaved with other work)
txa_hand:
	load	0,HWORD,it3,p_txa_ack	;clear interrupt & get status

	PERF_TP	it0,PERF_TXA_INT

	jmpfdec	obf_cnt,txa_70		;any more output to do?
	 mov	obf_cur,obf_next

	sll	it0,formac_stat,31-FORMAC_SRNGOP_BT
	jmpf	it0,txa_40		;and is the ring up?
	 sub	out_done,out_done,1	;tell host previous frame is finished

	store	0,WORD,it0,txa_dmacs	;yes, start the following buffer
	srl	it3,it3,STAT1_TX_UNDER_BT
	and	it3,it3,1
	add	cnt_tx_under,cnt_tx_under,it3

	sll	it0,cmd1,31-CMD1_MEDREQA_BT
	xor	it0,it0,obf_cnt		;it0=TRUE if hog mode != more frames
	jmpf	it0,txa_20
	 load	0,WORD,it2,obf_cur	;finish starting the front end

	FE_DELAY 1,,
	xor	cmd1,cmd1,CMD1_MEDREQA
	store	0,HWORD,cmd1,p_cmd1	;hog token while we have > 1 frame
txa_20:

	ADV_OBF	it0,,			;advance pointer to next buffer
	CK	.>=(txa_20+3*4)||REV29K>3   ;IRET at end of BTC for erratum #7
	iret


;the ring is dead, but we have more to do
txa_40:	andn	cmd1,cmd1,CMD1_MEDREQA	;release the token
	store	0,HWORD,cmd1,p_cmd1

	rs_st	TXA

	srl	it3,it3,STAT1_TX_UNDER_BT
	and	it3,it3,1
	add	cnt_tx_under,cnt_tx_under,it3

	CK	.>=(txa_40+3*4)||REV29K>3
	iret


;no more output for now, so shut down
txa_70:	sub	out_done,out_done,1	;tell host previous frame is finished

	rs_st	TXA

	srl	it3,it3,STAT1_TX_UNDER_BT
	and	it3,it3,1
	add	cnt_tx_under,cnt_tx_under,it3

	CK	.>=(txa_70+3*4)||REV29K>3
	iret

	.eject
	NXTVEC	V_BUS
;Bus packet interrupt handler
bus_hand:
	jmpt	cur_bus_bits,bus_w	;go handle VME DMA write interrupt
	 CK	DMA_WR==TRUE
	 cpeq	it2,bus_tc,0		;it2=TRUE if end of write buffer

;output or board VME read bus packet interrupt handler
;   Board FDDI output buffers are assumed to be big enough to handle the
;   largest possible frame, so we do not need to worry about allocating more
;   than the initial board buffer.
;
;   Output buffers are integral multiples of "bus packet pages", so
;   "transfer cycles" are needed only at the end of a host buffer or a
;   DRAM page, whichever comes first.

	PERF_TP	it1,PERF_BUS_READ_INT

	jmpf	cur_rfer,bus_r12
	 CK	XFER_CYCLE >= TRUE
	 srl	it1,cur_bus_bits,DMA_C_BT - ST2_H2B_FIN_BT
	 CK	DMA_C_BT > ST2_H2B_FIN_BT

	store	0,WORD,cur_rfer,cur_rfer	;finish cycle if needed
	const	cur_rfer,0

bus_r12:
	and	it1,it1,ST2_H2B_FIN	;tell main line of finished control
	or	cur_st2,cur_st2,it1

;   here it2=TRUE if another bus packet has not been previously prepared

;   If we have finished a frame, then let the fiber start by increasing
;   the number of ready output frames.

;   Here is more code which assumes that the several DMA requests which
;   comprise a single output data request are contiguous and all present
;   before the DMA code starts.

	srl	it1,cur_bus_bits,DMA_FIN_BT
	and	it1,it1,1
	jmpt	it2,bus_90
	 add	obf_cnt,obf_cnt,it1

;begin the next VME "bus packet" host-to-board read
bus_r20:store	0,WORD,busrst_read,p_busrst	;clear the interrupt request

	mov	cur_rfer, next_rfer
	mov	cur_bus_bits, next_bus_bits

	constx	it0,IO_STRT01		;tell the VME DMA machine to start
	load	0,WORD,it1,it0

	consth	it0,IO_LDVADDR
	CK	(IO_LDVADDR & 0xffff)==(IO_STRT01 & 0xffff)
	store	0,WORD,bus_vaddr,it0

	consth	it0,IO_LDVME
	CK	(IO_LDVME & 0xffff)==(IO_STRT01 & 0xffff)
	store	0,WORD,bus_tc,it0
	const	bus_tc,0

	consth	it0,IO_REQBUS
	CK	(IO_REQBUS & 0xffff)==(IO_STRT01 & 0xffff)
	cpleu	it1,bus_h_len,0		;will that complete the host buffer?
	jmpt	it1,bus_r60		;yes, see if that ended the frame
	 load	0,WORD,it0,it0		;DMA should be running now


;get ready for the next VME read bus packet
;   limit the transfer count by the end of the page and host buffer limit
;   bus_h_addr=next address, bus_h_len=next length
bus_r30:
	srl	it1,bus_bd_addr,BUS_PAGE_BT
	add	it1,it1,1
	sll	it1,it1,BUS_PAGE_BT
	sub	it1,it1,bus_bd_addr	;it1=bytes to next page

	cpltu	it0,it1,bus_h_len
	sra	it0,it0,31
	and	it1,it1,it0
	andn	bus_tc,bus_h_len,it0
	or	bus_tc,bus_tc,it1	;bus_tc = min(it1, bus_h_len)

	mov	bus_vaddr,bus_h_addr

	sub	bus_h_len,bus_h_len,bus_tc

	add	it0,bus_bd_addr,bus_tc
	xor	it0,it0,bus_bd_addr	;see if transfer cycle will be needed
	sll	it0,it0,31-BUS_PAGE_BT	;--if at end of page
	sra	it0,it0,31
	sll	it0,it0,BUS_PAGE_BT
	or	next_rfer,xfer_cycle,bus_bd_addr
	and	next_rfer,next_rfer,it0

	add	bus_h_addr,bus_h_addr,bus_tc
	add	bus_bd_addr,bus_bd_addr,bus_tc

	add	bus_tc,bus_tc,3		;round byte count up to words
	srl	bus_tc,bus_tc,2		;so bus_tc=(bus_tc+3)/4

	t_st	it0,BUS_ACT
	jmpf	it0,bus_r20		;start machinery if not running
	 or	cur_st1,cur_st1,ST1_BUS_ACT | ST1_LED_ACT1 | ST1_LED_ACT2

	PERF_TP	it0,PERF_BUS_EXIT
	iret				;wait for DMA to finish

	.eject
	NXTVEC	V_MB
;"Mailbox" interrupt handler
;   the host wants something
mb_hand:
 .ifdef	PERF
	load	0,HWORD,it0,p_mailbox
	add	it0,it0,PERF_ACT
	sll	it0,it0,2
	consth	it0,EPROM_BASE
	load	0,BYTE,it0,it0
 .endif
	constx	cps_shadow,PS_INTSOFF
	mtsr	OPS,cps_shadow		;turn off interrupts so it gets done

	consth	cur_st1,ST1_MB		;tell the main loop to do the work
	CK	ST1_MB == TRUE

	CK	.>=mb_hand+3*4 || REV29K>3
	iret

	.eject
	NXTVEC	V_FOR_CMT
;FOR_CMT trap
;   Handle FORMAC-CMT interrupt by saving state.
;   Because this is a trap, it does terrible things to the latency
;   of the TX and RX interrupts.  So it should be fast when we might
;   be transmitting or receiving.  Line state changes usually do not happen
;   when transmitting or receiving, or at least not when latency matters.

;   keep it0=FORMAC status bits, it1,it2,it3=scratch
for_cmt_hand:
	PERF_TP it1,PERF_FOR_CMT
	load	0,HWORD,it0,p_cmd1	;get board status

;note noise events
	sll	it1,it0,31-STAT1_CNT1_BT
	jmpt	it1,for_ne1
	 sll	it1,it0,31-STAT1_CNT2_BT
	jmpt	it1,for_ne2
for_ne9: sll	it1,it0,31-STAT1_CMT1_BT


;save line state transition
	jmpt	it1,cmt_hand1
	 sll	it1,it0,31-STAT1_CMT2_BT
	jmpt	it1,cmt_hand2
	 sll	it1,it0,31-STAT1_FORMAC_BT
for_hand:
	jmpf	it1,for_hand9
	 xor	it0,p_formac_stmc, IO_FORMAC_STAT ^ IO_FORMAC_STMC

;For a FORMAC interrupt, get and save new recent FORMAC state (to turn off
;   the interrupt) and set a flag bit in a register to tell main loop to
;   consider interrupting the host.  FORMAC interrupts happen much more often
;   than line state changes (i.e. CMT interrupts).  So we assume we are
;   dealing with a FORMAC interrupt.

	load	0,HWORD,it0,it0
;   it0=FORMAC status

	set_st	FORINT

;Save FORMAC bits for the host.
;   Do not count TX abort for beacon or claim frames.  Send the FORMAC
;   RNGOP bit despite its different behavior.
	and	it2,cur_st1,ST1_TXA
	xor	it2,it2,ST1_TXA		;mark async output off
	sll	it2,it2,FORMAC_SXMTABT_BT - ST1_TXA_BT
	CK	FORMAC_SXMTABT_BT > ST1_TXA_BT

	FORMAC_REC 4,-PFETCH,		;finish delays for preceding fetch
	andn	it2,it0,it2

	sll	it3,it0,31-FORMAC_SXMTABT_BT
	jmpf	it3,for_hand4		;jump if TX abort did not happen
	 const0	it0,FORMAC_SRNGOP

;   The FORMAC has declared a TX abort.
	andn	cmd1,cmd1,CMD1_MEDREQA	;turn off async TX on abort
	or	it1,cmd1,CMD1_TXABT
	store	0,HWORD,it1,p_cmd1
	FE_DELAY 1,

	rs_st	TXA
	store	0,HWORD,cmd1,p_cmd1	;reset TX part of the front end
	FE_DELAY 2,

for_hand4:
;   it0=FORMAC_SRNGOP, it2=FORMAC status--abort filtered
	andn	formac_stat,formac_stat,it0
	or	formac_stat,formac_stat,it2

	load	0,HWORD,it1,p_formac_stmc

;turn off beacon and claim frame interrupts as soon as they are received
	and	it3,formac_imsk,FORMAC_MORE_INTS
	and	it3,it3,it2
	cpeq	tav,it3,0

	FORMAC_REC 3,,			;finish waiting for state machine bits

	jmpt	tav,for_hand6

	 xor	tav,p_formac_stmc,IO_FORMAC_IMSK ^ IO_FORMAC_STMC
	andn	formac_imsk,formac_imsk,it3
	store	0,HWORD,formac_imsk,tav

	FORMAC_REC 4,,
for_hand6:

	sll	txs_cnt,it1,31-FORMAC_XMIT5_BT
	sra	txs_cnt,txs_cnt,31	;txs_cnt=beaconing
	sll	it2,it1,31-FORMAC_XMIT4_BT
	or	it2,it2,txs_cnt		;it2=either beaconing or claiming

;   it0=FORMAC_SRNGOP, txs_cnt=beaconing, it2=either beaconing or claiming

;   By now, we have read from the FORMAC twice, from the front end status
;   register, and have paid the interrupt overhead.  A previously active
;   beacon or claim transmission will have finished.
	jmpf	it2,for_hand9		;now need to beacon or claim?
	 and	it2,txs_cnt,(BEACON_BASE^CLAIM_BASE)    ;choose buffer

	load	0,HWORD,it3,p_txs_ack	;yes, clear old TXS interrupt (if any)

;generate a true ring-operational bit by turning off RNGOP when we are
;   beaconing or claiming
	andn	formac_stat,formac_stat,it0
	set_st	LED_BAD

	store	0,WORD,txs_dmacs,txs_dmacs
	constx	txs_cnt,TXS_FORCE
	xor	txs_addr,txs_claim,it2
	load	0,WORD,it2,txs_addr	;start front end sending MAC frame

	iret

	.eject
	NXTVEC	V_RX
;Receive trap handler
;   Handle incoming frames.  The total number of cycles expended by this
;   function must be as small as possible.  That is why there are two
;   copies of the code.

;The descriptor for one frame is in the start of the first buffer of the
;   next frame.

;It is better to spend more time with frames that require more than one
;   buffer, since such frames arrive at a low rate, because they require
;   more time on the fiber.



 .macro	RX_HAND,n,m
 .if n==3
	  jmpt	  rx_toggle,rx4_hand
 .endif
rx@n@_hand1:
	   load	  0,HWORD,it0,p_rx@n@_ack   ;clear interrupt & get status
	  FE_DELAY 20,

	  jmpfdec ibf_free_cnt,rx_@n@ovf    ;stop if out of buffers
	   const0  misfrm_lim,MISFRM_LIM0   ;reset dead-front-end detector

	  IBF_ADR it1,ibf_free,tav,	;it1=address of next free buffer

	  store	  0,WORD,rx@n@_dmacs,rx@n@_dmacs
	  or	  tav,it1,rx@n@_dmach
 .ifdef RX_CONT
	  add	  ibf_dsc_cnt,ibf_dsc_cnt,1
 .endif
	  load	  0,WORD,tav,tav	;send address to the front end

 .ifdef RX_CONT
	  sll	  it2,it0,31-STAT1_RX@n@_CONT_BT
	  jmpt	  it2,rx@n@_hand8	;no frame descriptor for continuation
 .endif
	   sll	  it2,it0,31-STAT1_RX@m@_INT_BT

	  add	  ibf_raw_cnt,ibf_raw_cnt,1

;save buffer descriptor with frame dsc
 .ifdef RX_CONT
	  sll	  it3,ibf_dsc_cnt,IBF_P_DSC_SH
	  CK	  IBF_P_DSC_MASK == 0xffff0000
	  or	  it3,it3,ibf_next
	  store	  0,WORD,it3,ibf_p_dsc
 .else
	  store	  0,WORD,ibf_next,ibf_p_dsc
 .endif

	  add	  ibf_p_dsc,ibf_next,IBF_P_DSC
	  CK	  IBF_P_DSC!=0
 .ifdef RX_CONT
	  constn  ibf_dsc_cnt, -2	;start new descriptor

rx@n@_hand8:
 .endif
	  mov	  ibf_next,ibf_free
	  load	  0,WORD,ibf_free,ibf_free
	  CK	  IBF_P_FOR==0

	  jmpt	  it2,rx@m@_hand1
	   cpge	  rx_toggle,rx_toggle,0	;toggle interrupt handler
	  iret


;out of input buffers
;   Try to stop the FORMAC.
;   Do not try to keep the front end running.  Instead, make a note
;	to do the substance of the interrupt later.
rx_@n@ovf:constx  ibf_free_cnt,IBF_FREE_DEAD	;stay off until turned on
	  add	  cnt_buf_ovf,cnt_buf_ovf,1

	  PERF_TP it0,PERF_RX_OVF
 .if	REV29K<=3
	  nop
	  CK	  .>=rx_@n@ovf+3*4
 .endif
	  iret
 .endm


rx_hand:
	PERF_TP	it0,PERF_RX_INT
	RX_HAND	3,4


rx4_hand:
	RX_HAND 4,3

	.purgem RX_HAND

	.eject
	.global	_main
;assume all registers but lr[0-2] and the it*, t*, and v* are 0.
_main:	constx	cps_shadow,PS_INTSOFF
	mtsr	CPS,cps_shadow
	mtsrim	VAB,0
	mtsrim	CFG, CFG_DW


;set the global registers
	constx	p_mailbox, SIO_MAILBOX
	constx	p_cmd1, IO_CMD1
	constx	p_formac_stmc,IO_FORMAC_STMC

	constx	p_cmt1,IO_CMT1
	constx	p_cmt2,IO_CMT2

	constx	p_export, IO_EXPORT
	constx	export, IO_EXPORT_STD

	constx	p_busrst, IO_BUSRST
	constx	busrst_read, BUSRST_READ
	constx	xfer_cycle, XFER_CYCLE
	constx	p_ldvme, IO_LDVME


;reset the front end
	constx	cmd1, CMD1_RESET
	store	0,HWORD,cmd1,p_cmd1

;reset "bus packet" machinery
	store	0,WORD,busrst_read,p_busrst
	store	0,WORD,export,p_export



;refuse to run if the SRAM checksum is bad.
	const	t1,SRAM_BASE
	const	t2,(cksum-SRAM_BASE)/4
	const	t3,0
sck1:	load	0,WORD,t0,t1
	add	t1,t1,4
	add	t3,t3,t0
	srl	t0,t0,16
	jmpfdec	t2,sck1
	 add	t3,t3,t0

	consth	t3,0
	cpeq	t4,t3,0
	jmpf	t4,bad_cksum


;refuse to run if the board does not have the accept-all-multicast hardware
	 constx	t2,IO_CMD2
	load	0,HWORD,t0,t2
	sll	t0,t0,31-STAT2_MCAST_BT
	jmpf	t0,bad_mcast

;turn on multicast
;   it seems that you must reset it first, or some boards will not
;   pay attention to the multicast bit.
	FE_DELAY 0,,
	const	t0,0
	store	0,HWORD,t0,t2
	FE_DELAY 0,,
	constx	t0,CMD2_MCAST|CMD2_CAM_48ADR|CMD2_CAM_SM_ENABLE
	store	0,HWORD,t0,t2
	FE_DELAY 0,,

;initialize the 9513 counters
	constx	v1, IO_CNT1_CMD
	constx	v2, IO_CNT1_DATA
	constx	v3, IO_CNT2_CMD
	constx	v4, IO_CNT2_DATA
	const	v5,0

	CNTCMD2	IOCNT_RESET, v1,v3, 0
	CNTCMD2	IOCNT_DISABLE_FOUT, v1,v3, 0
	CNTCMD2	IOCNT_ENABLE_16BIT, v1,v3, 0


	;count symbols on with the built-in clock gated by gate #1, which
	;	is (LEM enabled & !MLS & !HLS)
	;active high level gating from gate N, count on rising edge,
	;	source F1, no special gating, reload from Load,
	;	count repetitively, count up, output off.
	CNTMODE2 IO_CNT_SYM, v1,v3, 0
	CNTWR2	(0x8000 + 0x0b00 + 0x0028 + IOCNT_OFF), v2,v4, 0
	CNTWR2	0, v2,v4, 0

	;count LEM "events" from source 2
	;active high level gating from gate N-1, count on rising edge,
	;	source 2, no special gating, reload from Load,
	;	count repetitively, count up, output off.
	CNTMODE2 IO_CNT_LEM, v1,v3, 0
	CNTWR2	(0x6000 + 0x0200 + 0x0028 + IOCNT_OFF), v2,v4, 0
	CNTWR2	0, v2,v4, 0

	;"noise events" from gate 3
	;active high level gating from gate N, count on rising edge,
	;	source F1, enable special gating, reload from Load,
	;	count repetitively ("mode Q"), count down, TC Toggled output.
	CNTMODE2 IO_CNT_NE, v1,v3, 0
	CNTWR2	(0x8000 + 0x0b00 + 0x00a0 + IOCNT_TOGGLE), v2,v4, 0
	CNTWR2	SMT_NS_MAX*IOCNT_F1_RES, v2,v4, 0

	;count frame pulses and frame errors on source 4s
	CNTSIMP2 IO_CNT1_FRAME, v1,v2,v3,v4, 0

	;count lost frames and tokens on source 5s
	CNTSIMP2 IO_CNT1_LOST, v1,v2,v3,v4, CNTCMD2_OVHD

	CNTCMD2	IOCNT_LOAD_ALL, v1,v3, CNTCMD2_OVHD
	CNTCMD2	IOCNT_ARM_ALL, v1,v3, 32


;start the 29K timer
	constx	v0,TIME_INTVL
	mtsr	TMC,v0
	CK	TMR_IE > 0xffff
	consth	v0,TIME_INTVL | TMR_IE
	mtsr	TMR,v0


;RX interrupt registers
	constx	rx3_dmach, DMA3_ADR
	constx	rx3_dmacs, DMACS_CH3
	constx	p_rx3_ack, IO_RX3_ACK
	constx	rx4_dmach, DMA4_ADR
	constx	rx4_dmacs, DMACS_CH4
	xor	p_rx4_ack, p_rx3_ack, IO_RX4_ACK ^ IO_RX3_ACK

;sync. TX interrupt registers
	constx	txs_dmacs,DMACS_CH1
	constx	txs_claim,DMA1_ADR | (CLAIM_BASE & 0xfffff)
	xor	p_txs_ack, p_rx3_ack, IO_TXS_ACK ^ IO_RX3_ACK

;async. TX interrupt registers
	constx	txa_dmacs,DMACS_CH2
	xor	p_txa_ack, p_txs_ack, IO_TXA_ACK ^ IO_TXS_ACK

	const	out_done,0

	constx	obf_len,    OBF_LEN
	constx	obf_restart,OBF_SIZE
	constx	obf_cnt,    OBF_CNT_START

	constx	obf_cur_lim, (OBF_BASE+OBF_SIZE-OBF_LEN) | DMA2_ADR
	constx	obf_cur,    OBF_START

	constx	obf_free_lim,(OBF_BASE+OBF_SIZE-OBF_LEN)
	constx	obf_free,   OBF_BASE


;reset the FORMAC to stop stray interrupts
	const	v0,0
	constx	v1,IO_FORMAC_RESET
	store	0,HWORD,v0,v1
	FORMAC_REC 20,

;reset the ENDECs
	const0	t1,ENDEC_CR2_RESET
	constx	v1,IO_ENDEC1_SEL
	constx	v2,IO_ENDEC2_SEL
	store	0,HWORD,t1,v1
	store	0,HWORD,t1,v2
	ENDEC_REC t1,ENDEC_WAIT+3
	sll	t1,t1,ENDEC_SLL
	xor	v1,v1,IO_ENDEC1_REG ^ IO_ENDEC1_SEL
	xor	v2,v2,IO_ENDEC2_REG ^ IO_ENDEC2_SEL
	store	0,HWORD,t1,v1
	store	0,HWORD,t1,v2
	ENDEC_REC t1,20



	constx	ibf_ptr_mask,IBF_PTR_MASK
	constx	p_ibf_base,IBF_BASE

;initialize the receive buffer list
;   v0=count, ibf_free=prev, v1=current, v4=DRAM buffer address
	constx	v0,IBF_NUM-2
	constx	ibf_free,ibf_ptrs+_LEN_IBF_P*(IBF_NUM-1)
	constx	v1,ibf_ptrs
	constx	v4,IBF_BASE

bfinit:	add	t0,v1,IBF_P_ADR
	store	0,WORD,v4,t0		;buffer address in current descriptor

	CK	IBF_P_FOR==0		;point previous to current descriptor
	store	0,WORD,v1,ibf_free

	add	t0,v1,IBF_P_BAK
	store	0,WORD,ibf_free,t0	;and current to previous descriptor

	mov	ibf_free,v1
	add	v1,v1,_LEN_IBF_P
	const0	t0,IBF_LEN
	jmpfdec	v0,bfinit
	 add	v4,v4,t0

;start the front end input channels
	constx	cmd1, CMD1_GO

;stop TX and reset RX
 .macro	FE_RESET, tmp
	  andn	  cmd1,cmd1,CMD1_MEDREQA
	  constx  tmp,CMD1_FE_ENABLE
	  andn	  tmp,cmd1,tmp
	  or	  tmp,tmp,CMD1_TXABT
	  store	  0,HWORD,tmp,p_cmd1	;release the token and stop output
	  NDELAY  notused, FE_RESET_REC, 1
	  rs_st	  TXA
 .endm

;restore the Front End
;   interrupts must be off
 .macro	FE_INIT,tmp1,tmp2
	  const	  txs_cnt,0

	  const	  rx_toggle,0

	  constx  ibf_free_cnt, IBF_FREE_INIT
	  const0  ibf_full,ibf_ptrs
	  const0  misfrm_lim,MISFRM_LIM0

	  constn  ibf_raw_cnt, 0-1
	  mov	  ibf_raw,ibf_full

	  add	  ibf_p_dsc,ibf_raw,IBF_P_DSC
 .ifdef RX_CONT
	  constn  ibf_dsc_cnt, -2
 .endif

	  store	  0,HWORD,cmd1,p_cmd1	;restore FE enable
	  FE_DELAY IBF_ADR_PAUSE,

	  IBF_ADR tmp1,ibf_raw,tmp2,	;get address of 1st free buffer

	  store	  0,WORD,rx3_dmacs,rx3_dmacs
	  add	  tmp1,tmp1,RX_DESC_LEN	;offset by missing descriptor
	  or	  tmp1,tmp1,rx3_dmach
	  load	  0,WORD,tmp1,tmp1	;send address to the front end

	  load	  0,WORD,ibf_next,ibf_full
	  CK	  IBF_P_FOR==0

	  IBF_ADR tmp1,ibf_next,tmp2,	;fetch address of next free buffer
	  store	  0,WORD,rx4_dmacs,rx4_dmacs
	  or	  tmp1,tmp1,rx4_dmach
	  load	  0,WORD,tmp1,tmp1	;send address to the front end

	  load	  0,WORD,ibf_free,ibf_next
	  CK	  IBF_P_FOR==0
 .endm

	FE_RESET t0
	FE_INIT	t0,t1,


;start host DMA list
	constx	dlist_start,dlist
	constx	dlist_lim,dlist + DLIST_SIZE - _LEN_DLIST
	mov	dlist_head,dlist_start
	mov	dlist_tail,dlist_start

	constx	nxt_big,bigs
	mov	lst_big,nxt_big
	constx	nxt_med,meds
	mov	lst_med,nxt_med
	constx	nxt_sml,smls
	mov	lst_sml,nxt_sml
	constx	sml_bufs,-MIN_SML
	constx	med_bufs,-MIN_MED
	constx	big_bufs,-MIN_BIG

	constx	b2h_lim, B2H_LEN*_LEN_B2H_BLK
	constx	t0,B2H_BASE
	add	b2h_lim, b2h_lim, t0
	mov	b2h_rpos,t0		;prevent H2B or B2H DMA until ready
	mov	b2h_wpos,t0
	constx	b2h_phy, PHY_INT_NUM-1

	jmp	cmd_phy_rep		;lurk in the ring
	 set_st	LED_BAD

	.set	CUR_NDELAY_LIM, NDELAY_LIM



bad_cksum:
	DIE	ACT_FLAG_CKSUM, t2

bad_mcast:
	DIE	ACT_FLAG_MCAST, t1

	.eject
;turn on traps and interrupts and start doing commands
cmd_done:
	constx	t1,ACT_DONE
	store	0,HWORD,t1,p_mailbox	;tell host we finished
	constx	cps_shadow,PS_INTSON

;main loop
;   stay in this loop forever
mlop0:	mtsr	CPS,cps_shadow
mlop:	jmpt	cur_st1,cmd		;host command
	 CK	ST1_MB == TRUE
	 mfsr	v1,TMC			;v1=current time while in this code

	jmpt	cur_st2,mnoise		;count noise event and fix counter
	 CK	ST2_NE == TRUE

	 t_st	t0,FORINT
	jmpt	t0,mformac		;listen to FORMAC complaints

;   Restart TX if there is at least one output buffer waiting for the
;   front end, and if the front end is not running.
	 t_st	t0,TXA
	sll	t1,formac_stat,31-FORMAC_SRNGOP_BT
	andn	t0,t1,t0		;t0=TRUE if TX idle and RNGOP true
	add	t1,obf_cnt,1		;t1=TRUE if work waiting
	and	t1,t1,t0
	jmpt	t1,m_tx

	 t_st	v2,BUS_ACT,		;v2=TRUE if VME DMA active

	cpneq	t0,dlist_head,dlist_tail
	andn	t0,t0,v2		;t0=TRUE if have work but not working
	asge	V_BUS_START,t0,0	;start bus packets--DMA with the host

;   Schedule B2H control DMA if it has been a while.
	 cple	t0,v1,b2h_delay		;t0=time for it
	cpneq	t1,b2h_rpos,b2h_wpos
	and	t1,t1,t0		;t1=traffic waiting
	jmpt	t1,reqb2h
	 const0	t4,mlop

;   process previously received H2B info
	t_st	t1,H2B_FIN
	jmpt	t1,m_finh2b

;   Schedule H2B control DMA if it has been a while.
	 cple	t1,v1,h2b_delay
	t_st	t2,H2B_ACT
	andn	t1,t1,t2
	jmpt	t1,m_reqh2b

;   Skim input and schedule DMA if we have enough host buffers, and if
;   we have not been hogging the DMA.
	 or	t1,sml_bufs,med_bufs
	or	t1,t1,big_bufs
	or	t1,t1,ibf_raw_cnt
	or	t1,t1,b2h_ok
	jmpf	t1,m_rx

;   start host PHY interrupt if one is pending and previous were ack'ed
	 t_st	t0,PINT_REQ
	andn	t0,t0,b2h_phy
	jmpt	t0,m_phy_int

;   schedule B2H DMA to ack output if it is time.
	add	t1,v1,h2b_dma_delay
	cplt	t1,t1,b2h_delay		;t1=time for it
	and	t1,t1,out_done
	jmpt	t1,reqb2h
	 const0	t4,mlop

;   poll the 9513s when we are in a different epoch
	srl	t0,v1,TIME_INTVL_BT+1-UP9513_BT
	CK	UP9513_BT<=TIME_INTVL_BT
	cpeq	t1,t0,ct_age
	jmpf	t1,p9513
	 const0	v7,mlop0

;   restore FDDI input after drowning
	cpeq	t1,ibf_full,ibf_raw	;T1=nothing waiting for DMA to host
	and	t1,t1,ibf_raw_cnt	;	& nothing to be skimmed
	andn	t2,ibf_free_cnt,misfrm_lim	;T2=not shut down or missing
	andn	t1,t1,t2		;T1=quiet and dead
	jmpt	t1,m_rx_on		;then restore input

;   increase the checksumming budget, clamping at the maximum.
	 constx	t0,cksum_max
	load	0,WORD,t0,t0
	add	cksum_ok,cksum_ok,1
	cpge	t1,t0,cksum_ok
	sra	t1,t1,31
	and	cksum_ok,cksum_ok,t1
	andn	t0,t0,t1
	or	cksum_ok,cksum_ok,t0

;set the LED(s) to
;   red		=ring not operational
;   green	=quiet and operational
;   red & green	=active
; This is accomplished by setting the LED bit=RNGOP & (~toggle | ~traffic)
;					    or RNGOP & ~(toggle & traffic)
; There is no LED indication that the board is alive, since the kernel
;   complains if it is dead.
; The "toggle" value is simply a bit in the timer, chosen to switch at
;   least 60 times/second.

	.set	TOG_BT, MHZ_BT+14
	CK	TOG_BT <= TIME_INTVL_BT
	srl	t0,v1,TOG_BT-IO_EXPORT_LED_BT
	CK	TOG_BT>IO_EXPORT_LED_BT
	sll	t1,cur_st1,IO_EXPORT_LED_BT - ST1_LED_ACT2_BT
	CK	IO_EXPORT_LED_BT > ST1_LED_ACT2_BT
	nand	t0,t0,t1
	sll	t2,cur_st1,IO_EXPORT_LED_BT - ST1_LED_BAD_BT
	CK	IO_EXPORT_LED_BT > ST1_LED_BAD_BT
	andn	t0,t0,t2
	constx	t1,IO_EXPORT_LED
	and	t0,t0,t1		;t0=new value of LED bit

	and	t2,export,t1		;t2=old value of LED bit
	cpeq	t2,t2,t0		;avoid unnecessary writes
	andn	export,export,t1	;but keep it atomic
	jmpf	t2,mlop7
	 or	export,export,t0


;Occassionally generate a self-addressed frame to measure ring latency
; Do it only occassionally and only when the ring is healthy and we are idle.
; Send it as a synchronous frame.
; If we have transmitted the latency void frame but not received it in
;   2*D_Max, then assume the ring is small.
	 t_st	t0,GET_LATENCY
	jmpf	t0,mlop

	t_st	t0,WAIT_LAT		;not another while waiting for old
	jmpt	t0,m_lat8

	 t_st	t0,LAT
	jmpf	t0,m_lat9

	 or	t1,sml_bufs,med_bufs
	or	t1,t1,big_bufs
	or	t1,t1,b2h_ok
	jmpt	t1, mlop		;wait if short of host buffers

	 constx	t1,(1000000*MHZ)/MAX_LAT_SEC,	;limit latency frames/second
	mtsrim  CPS,PS_INTSOFF
	sub	t1, txs_time,t1		;t1=timestamp when a new frame is ok
	cple	t0, v1,t1
	jmpf	t0,mlop0

	 and	t0,cur_st1, ST1_BUS_ACT | ST1_LED_BAD | ST1_TXA
	jmpf	ibf_raw_cnt,mlop0	;wait if other input ready
	 cpneq	t0,t0,0
	andn	t0,t0,h2b_active
	jmpt	t0, mlop0		;wait if normal I/O active
	 nop

	set_st	WAIT_LAT,t0,
	set_st	TXA,t0,

	mfsr	txs_time,TMC		;keep m_lat8 from being premature

	store	0,WORD,txs_dmacs,txs_dmacs
	xor	t1,txs_claim,(LAT_BASE^CLAIM_BASE)
	load	0,WORD,t2,t1		;start the front end

 .if JITTER_LATENCY > MIN_RNG_LATENCY
    ;Spin here for a few microseconds to get an early return sooner
    ;and so more accurately.
	.equ	FSCAN_LATENCY, 3*JITTER_LATENCY
	const	v5,FSCAN_LATENCY/4	;4 cycles/loop
	mtsr	CPS,cps_shadow
m_lat5:	jmpf	ibf_raw_cnt,m_rx
	 nop
	jmpfdec	v5,m_lat5
	 nop
	jmp	mlop			;wait in the normal loop
	 nop
 .else
	jmp	mlop0			;wait in the normal loop
	 nop
 .endif

;If we have recently transmitted the void frame, do not do another one yet.
;   We may have to wait TTRT until the token comes around and we can
;   transmit.
m_lat8:
	sub	t3,txs_time,v1
	constx	t0,(D_Max*2+T_Max)*MHZ   ; > 2*D_Max + TTRT
	cple	t0,t3,t0
	jmpt	t0,mlop
	 nop

	rs_st	WAIT_LAT,t0,		;forget it if it was lost
	jmp	mlop
	 const	cnt_rnglatency,0

;latency measurements are turned off
m_lat9:
	rs_st	GET_LATENCY,t0,
	jmp	mlop
	 nop


;actually change the LED(s)
mlop7:	jmp	mlop
	 store	0,WORD,export,p_export

	.eject
;interrupt the host for PHY events
m_phy_int:
	PERF_TP	t0,PERF_PINT

	rs_st	PINT_REQ

 .macro	LS_INT,	n
	  cpeq	  t0,cmt@n@,0		;tell host of new line state
	  jmpt	  t0,m_phy@n@_int4
	   const0 v1,PCM_ULS

	  constx  v2,IO_ENDEC@n@_SEL
	  mtsrim  CPS,PS_INTSOFF
	  clz	  t0,cmt@n@
	  sll	  cmt@n@,cmt@n@,t0
	  srl	  v1,cmt@n@,32-4	;safely get oldest line state
	  sll	  cmt@n@,cmt@n@,4
	  srl	  cmt@n@,cmt@n@,4
	  srl	  cmt@n@,cmt@n@,t0	;and remove it from the FIFO
	  mtsr	  CPS,cps_shadow

	  cplt	  t0,t0,4
	  jmpf	  t0,rep_ls		;convert and possibly repeat it
	   const  t4,m_phy@n@_int3

	  const	  cmt@n@,0
	  call	  t4,rep_ls
	   load	  0,BYTE,v1,p_cmt@n@	;line state change overrun
	  const	  v1,PCM_NLS

m_phy@n@_int3:
	  cpneq	  t0,cmt@n@,0		;interrupt again for accumulation
	  srl	  t0,t0,31-ST2_PINT_REQ_BT
	  or	  cur_st2,cur_st2,t0
m_phy@n@_int4:
 .endm


	LS_INT	2
	sll	v3,v1,8			;v3=growing value for the host
	LS_INT	1

	t_st	t1,PINT_ARM
	jmpf	t1,mlop			;done if no interrupt armed
	 or	t0,v1,v3		;finish v3=value for the host

	sub	b2h_phy,b2h_phy, 1
	set_st	FASINT_REQ,		;request host interrupt
	consth	t0,B2H_PHY
	CK	(B2H_PHY & 0xffff) == 0
	consth	b2h_delay,TIME_NOW	;force immediate B2H DMA
	jmp	addb2h
	 const0	t4,mlop

	.purgem	LS_INT

	.eject
;Add request to DMA queue to copy control info from the host to the board
;   Always copy a fixed number of words, relying on an empty sentinel
;   after the end of the real slots.
m_reqh2b:
	constx	t0,(DMA_C | DMA_FE) + H2B_BUF_LEN
	store	0,WORD,t0,dlist_head
	CK	DLIST_L == 0

	add	t2, dlist_head, DLIST_A
	store	0,WORD,h2b_pos,t2	;finish making DMA request
	INC_DLIST dlist_head,t0,t1

	mfsr	h2b_delay,TMC		;schedule next host-to-board DMA
	sub	h2b_delay,h2b_delay,h2b_dma_delay

	jmp	mlop
	 set_st	H2B_ACT,		;no more until this finishes



;finish processing previously received H2B info

finh2b_tbl:
	.word	finh2b_emp		;H2B_EMPTY
	.word	finh2b_sml		;H2B_SMLBUF add little mbuf to pool
	.word	finh2b_med		;H2B_MEDBUF add medium mbuf to pool
	.word	finh2b_big		;H2B_BIGBUF add cluster to pool
	.word	finh2b_out		;H2B_OUT    start output DMA chain
	.word	finh2b_out_fin		;H2B_OUT_FIN end output DMA chain
	.word	finh2b_phy		;H2B_PHY_ACK finished PHY interrupt
	.word	finh2b_wrap		;H2B_WRAP   back to start of buffer
	.word	finh2b_idelay		;H2B_IDELAY delay interrupts

m_finh2b:
	andn	cur_st2,cur_st2,ST2_H2B_FIN | ST2_H2B_ACT

	const	v2,0			;v2=accumulated new delay
	constn	v4,-MAX_TX_DMA_LEN	;v4=limit DMA queue length
	constx	v5,H2B_BASE		;v5=start of buffer
	add	v6,v5,0			;v6=pointer into H2B board buffer
	add	v7,v5,H2B_BUF_LEN	;v7=end of buffer

finh2b_lop:
	cpgeu	t3,v6,v7		;t3=TRUE if end of block read
	jmpt	t3,finh2b_act

	 add	t4,v6,_H2B_ADDR

	load	0,WORD,t1,v6		;t1=OP and length
	CK	_H2B_OP == 0
	load	0,WORD,t4,t4		;t4=ADDR

	add	v6,v6,_LEN_H2B_BLK
	constx	t3,finh2b_tbl

	srl	t2,t1,16		;t2=OP
	add	t3,t3,t2
	load	0,WORD,t3,t3
	jmpi	t3
	 add	h2b_pos,h2b_pos,_LEN_H2B_BLK


;output, host-to-FDDI DMA request
;   We cannot give any of the DMA chain to the DMA interrupt handler
;   until it is complete.  So ensure we have the entire thing in our
;   buffer before releasing it.
finh2b_out:
	mov	v0,h2b_pos
	mov	v1,dlist_head
finh2b_out1:
	cpgeu	t3,v6,v7		;stop at end of command buffer
	jmpt	t3,finh2b_out9		;abort the whole chain if incomplete

	 consth	t1,0
	store	0,WORD,t1,v1		;save this length and address
	CK	DLIST_L == 0
	add	t0, v1, DLIST_A
	store	0,WORD,t4,t0

	add	v4,v4,t1		;limit DMA queue length

	load	0,WORD,t1,v6		;t1=next OP and length
	CK	_H2B_OP == 0

	add	t4,v6,_H2B_ADDR		;advance to next slot
	add	v6,v6,_LEN_H2B_BLK

	load	0,WORD,t4,t4		;t4=next ADDR
	INC_DLIST v1,t0,t2		;advance DMA list past previous
	srl	t2,t1,16		;t2=next OP

	cpeq	t3,t2,H2B_OUT_FIN
	jmpf	t3,finh2b_out1		;continue until end of DMA chain
	 add	h2b_pos,h2b_pos,_LEN_H2B_BLK

	consth	t1,DMA_FE
	store	0,WORD,t1,v1
	CK	DLIST_L == 0
	add	t0, v1, DLIST_A
	store	0,WORD,t4,t0

	INC_DLIST0 dlist_head,v1,t2,t3

	consth	t1,0			;t1 cleaned to length
	jmp	finh2b_lop
	 add	v4,v4,t1		;limit DMA queue length


;one buffer of output, host-to-FDDI DMA
finh2b_out_fin:
	consth	t1,DMA_FE
	store	0,WORD,t1,dlist_head
	CK	DLIST_L == 0
	add	t0, dlist_head, DLIST_A
	store	0,WORD,t4,t0

	INC_DLIST dlist_head,t2,t3	;mark this DMA list entry used

	consth	t1,0			;t1 clenad to length
	jmp	finh2b_lop
	 add	v4,v4,t1		;limit DMA queue length


;save new small buffer
finh2b_sml:
	store	0,WORD,t4,lst_sml
	srl	lst_sml,lst_sml,2
	add	lst_sml,lst_sml,1
	andn	lst_sml,lst_sml,MAX_SML
	sll	lst_sml,lst_sml,2
	jmp	finh2b_lop
	 add	sml_bufs,sml_bufs,1

;save new medium buffer
finh2b_med:
	store	0,WORD,t4,lst_med
	add	lst_med,lst_med,4
	andn	lst_med,lst_med,MAX_MED*4
	jmp	finh2b_lop
	 add	med_bufs,med_bufs,1

;save new big buffer
finh2b_big:
	store	0,WORD,t4,lst_big
	add	lst_big,lst_big,4
	andn	lst_big,lst_big,MAX_BIG*4
	jmp	finh2b_lop
	 add	big_bufs,big_bufs,1

;finished PHY interrupt
finh2b_phy:
	jmp	finh2b_lop
	 add	b2h_phy,b2h_phy,1

;back to the start of the buffer
finh2b_wrap:
	constx	t1,h2b_buf
	jmp	finh2b_act
	 load	0,WORD,h2b_pos,t1	;wrap to start of host buffer

;delay host interrupt
finh2b_idelay:
	jmp	finh2b_lop
	 add	v2,v2,t4


;We have finished processing the requests read from the host.
;   If the host had nothing to say for a long time, stop doing host-to-board
;	DMA and tell the host.
;   If the host said more than fit in our buffer, then arrange to immediately
;	do another DMA.
;   If the host should have said something but did not, then interrupt it
;	and set the DMA delay to about the interrupt latency of the host.

finh2b_out9:				;output request was incomplete
	sub	h2b_pos,v0,_LEN_H2B_BLK	;will re-process it from the start

;The host more than filled our buffer
;   v2=accumulated new delay, v4=biased bytes added to DMA queue
finh2b_act:
	jmpf	v4,finh2b_act2		;unless we filled the queue,
	 nop
	consth	h2b_delay,TIME_NOW	;read again at once

;The host said something
;   v2=accumulated new delay
finh2b_act2:
	mfsr	v3,TMC
	sub	v3,v3,host_qt		;v3=expected host activity
	sra	t0,v3,31
	andn	v3,v3,t0
	add	v3,v3,v2

	constx	v4,max_host_work	;v4=limit on interrupt hold off
	load	0,WORD,v4,v4

	mov	h2b_active,h2b_sleep	;keep H2B DMA active
	rs_st	SLOINT_DONE,,		;re-enable host interrupt

	cple	t1,v4,v3
	sra	t1,t1,31
	and	t0,v4,t1
	andn	v3,v3,t1
	or	v3,v3,t0		;v3=min(v3, max activity)

	cpge	t1,h2b_dma_delay,v3
	sra	t1,t1,31
	and	t0,h2b_dma_delay,t1
	andn	v3,v3,t1
	or	v3,v3,t0		;v3=max(v3, H2B DMA delay)
	mfsr	host_qt,TMC
	jmp	mlop
	 sub	host_qt,host_qt,v3	;predict host activity



;The host did not fill the buffer
;   v2=accumulated new delay, v5=start of buffer, v6=last read
finh2b_emp:
	sub	v6,v6,v5
	cpgt	t0,v6,_LEN_H2B_BLK	;did the host say anything at all?
	jmpt	t0,finh2b_act2		;yes
	 sub	h2b_pos,h2b_pos,_LEN_H2B_BLK	;will re-read empty slot

;host was mute
;   If we do not have the maximum number of buffers, then the host may
;   not have noticed the most recent input frames.  If it seems the
;   host is inactive and has not noticed the recent input, interrupt it.
	sub	h2b_active,h2b_active,h2b_dma_delay

	constx	t0,max_bufs
	load	0,WORD,t0,t0
	add	t1,sml_bufs,med_bufs	;has host noticed all input?
	add	t1,t1,big_bufs
	cplt	t1,t1,t0
	jmpf	t1,finh2b_emp4		;yes, forget the interrupt

	 mfsr	v3,TMC
	cple	v3,v3,host_qt
	jmpf	v3,finh2b_emp5		;no, ready to interrupt the host?

	and	t1,cur_st2,ST2_SLOINT_DONE | ST2_SLOINT_RDY
	 cpeq	t1,t1,ST2_SLOINT_RDY
	jmpf	t1,finh2b_emp5		;yes, has host heard the input?--no

	 constx	t1,IO_SVINT		;yes, cause a host interrupt
	load	0,WORD,t1,t1
	set_st	SLOINT_DONE,		;no more until host speaks
	rs_st	SLOINT_RDY
	mov	h2b_active,h2b_sleep	;keep H2B DMA active

	constx	t0,int_delay
	mfsr	host_qt,TMC
	load	0,WORD,t0,t0		;t0=interrupt hold-off
	jmp	mlop
	 sub	host_qt,host_qt,t0	;mark host active


;The host has acknowledged all input.
finh2b_emp4:
	andn	cur_st2,cur_st2,ST2_SLOINT_RDY | ST2_SLOINT_DONE
finh2b_emp5:
	jmpf	h2b_active,mlop		;wait for next H2B DMA unless asleep

;   If we are about to go to sleep, tell the host.  There must be another
;   H2B DMA, which will cause this code to check a final time to see if
;   the host said anything.

;   Host interrupts can be merged together.  This is a consequence of
;   the mechanism which postpones the interrupts.  If one interrupt
;   is postponed until after the host has given us new buffers for
;   all of the input officially completed but not after buffers that
;   have not been announced, a second interrupt can occur before
;   the last work is announced, this code see the host said nothing, and
;   leave the interrupt marked done-but-not-acknowledged.
;   Meanwhile, the announcement B2H DMA can be made, the interrupt made
;   ready again, and this code will refuse to cause another host interrupt
;   on the grounds that the previous one has not been acknowledged.
;   Therefore, always do any possible interrupts before sleeping.

	 consth	t0,B2H_SLEEP
	CK	(B2H_SLEEP & 0xffff) == 0

;   Fix race with DMA interrupt which is also fiddling with H2B DMA delays.
	mtsrim  CPS,PS_INTSOFF

	t_st	t1,H2B_SLEEP
	jmpt	t1,finh2b_emp9
	 set_st	H2B_SLEEP,zzzz,

	rs_st	SLOINT_DONE,,		;re-enable host interrupt
	consth	b2h_delay,TIME_NOW	;force immediate B2H DMA
	jmp	addb2h
	 const0	t4,mlop0


;The host has been told of all input and that we are going to sleep.
;We have done a H2B DMA after announcing our sleepiness.
;The host appears to be asleep.
finh2b_emp9:
	constn	h2b_active,-1
	t_st	t1,OUTINT_REQ
	jmpf	t1,mlop0
	 consth	h2b_delay,TIME_NEVER
	constx	t1,IO_SVINT		;cause a host output interrupt
	load	0,WORD,t1,t1
	jmp	mlop0
	 rs_st	OUTINT_REQ

	.eject
;put an entry into the B2H ring
;   ST2_FASINT_REQ or ST2_SLOINT_REQ in cur_st2,
;   t0=value, t4=return, v4=unchanged
addb2h:
	srl	b2h_sn,b2h_sn,24
	add	b2h_sn,b2h_sn,1
	sll	b2h_sn,b2h_sn,24
	or	t0,t0,b2h_sn

	store	0,WORD,t0,b2h_wpos
	add	b2h_wpos,b2h_wpos,_LEN_B2H_BLK

	cpltu	t0,b2h_wpos,b2h_lim
	jmpti	t0,t4
	 nop
	jmp	reqb2h_1
	 nop



;schedule B2H transfer by adding a request to the VME DMA list
;   ST2_FASINT_REQ or ST2_SLOINT_REQ in cur_st2,
;   t4=return, v4=unchanged
reqb2h:
	jmpf	out_done,reqb2h_1

	 subr	t0,out_done,0		;tell host of completed output
	add	out_done,out_done,t0	;and avoid race with TX interrupt
	consth	t0,B2H_ODONE
	srl	b2h_sn,b2h_sn,24
	add	b2h_sn,b2h_sn,1
	sll	b2h_sn,b2h_sn,24
	or	t0,t0,b2h_sn
	store	0,WORD,t0,b2h_wpos
	add	b2h_wpos,b2h_wpos,_LEN_B2H_BLK
	set_st	OUTINT_REQ

reqb2h_1:
	and	t0,cur_st2,ST2_FASINT_REQ
	or	t0,t0,(DMA_C|DMA_WR|DMA_FE)>>(DMA_FASINT_BT-ST2_FASINT_REQ_BT)
	sll	t0,t0, (DMA_FASINT_BT-ST2_FASINT_REQ_BT)
	CK	(DMA_FASINT_BT-ST2_FASINT_REQ_BT) > 0
	CK	(DMA_FASINT_BT-ST2_FASINT_REQ_BT) < DMA_C_BT
	CK	(DMA_FASINT_BT-ST2_FASINT_REQ_BT) < DMA_WR_BT
	CK	(DMA_FASINT_BT-ST2_FASINT_REQ_BT) < DMA_FE_BT

	and	t1,cur_st2,ST2_SLOINT_REQ
	sll	t1,t1,DMA_SLOINT_BT - ST2_SLOINT_REQ_BT
	CK	(DMA_SLOINT_BT-ST2_SLOINT_REQ_BT) > 0
	or	t0,t0,t1

	andn	cur_st2,cur_st2,ST2_SLOINT_REQ | ST2_FASINT_REQ

	sub	t1,b2h_wpos,b2h_rpos
	or	t0,t0,t1
	store	0,WORD,t0,dlist_head	;put length and board address
	CK	DLIST_L == 0		;	into the DMA list
	add	t0, dlist_head, DLIST_A
	store	0,WORD,b2h_rpos,t0
	INC_DLIST dlist_head,t0,t1

	mfsr	b2h_delay,TMC		;no more B2H DMA for a while
	constx	t1,b2h_dma_delay
	load	0,WORD,t1,t1
	sub	b2h_delay,b2h_delay,t1

	cpltu	t0,b2h_wpos,b2h_lim	;wrap to start of the buffer
	jmpti	t0,t4			;if needed
	 mov	b2h_rpos,b2h_wpos

	constx	b2h_wpos,B2H_BASE
	jmpi	t4
	 mov	b2h_rpos,b2h_wpos

	.eject
;skim input frames, discarding wrong multi-cast or aborted frames
m_rx:
	PERF_TP	t0,PERF_SKIM

	add	t0,ibf_raw,IBF_P_DSC
	load	0,WORD,t0,t0
	IBF_ADR	v1,t0,t1,		;v1=address of frame descriptor

	load	0,WORD,v2,v1		;start fetching v2=frame descriptor

 .ifdef RX_CONT
	sra	v0,t0,IBF_P_DSC_SH	;v0=#-2 of buffers in frame
 .endif

	sub	ibf_raw_cnt,ibf_raw_cnt,1

	IBF_ADR	v3,ibf_raw,t0,		;v3=addr of FE descriptor before data

	srl	t3,v2,RX_DESC_OVF_BT
	CK	RX_DESC_OVF_BT < RX_DESC_MIS_BT
	CK	RX_DESC_OVF_BT < RX_DESC_ABT_BT
	CK	RX_DESC_OVF_BT < RX_DESC_FLSH_BT
	and	t3,t3,(RX_DESC_FLSH+RX_DESC_ABT+RX_DESC_MIS+RX_DESC_OVF) >> RX_DESC_OVF_BT
	cpeq	t3,t3,0
	jmpf	t3,m_rx_junk		;junk trash frames

	 add	v7,v3,RX_DESC_LEN+3
	load	0,BYTE,v7,v7		;start fetching v7=FC

	sub	v2,v2,4-IBF_PAD_LEN	;v2=descriptor bits + true length

	sll	t4,v2,32-RX_DESC_F_LEN_WIDTH
	sra	t4,t4,32-RX_DESC_F_LEN_WIDTH   ;t4=length from descriptor
	cplt	t3,t4,MIN_FRAME_LEN
	jmpt	t3,m_rx_short		;junk frame if too small

	 const0	t3,MAX_FRAME_LEN
	sub	t3,t3,t4		;truncate frame if too long
	jmpt	t3,m_rx_long

	 cpeq	t0,t4,LAT_LEN+IBF_PAD_LEN   ;empty frames measure latency
	jmpt	t0,m_rx_lat
	 mfsr	v1,TMC			;v1=current time

;   v2=descriptor bits + true length, v3=descriptor address, v7=FC
m_rx10:	 srl	t0,v2,RX_DESC_E_BT	;count E descriptor bit
	CK	RX_DESC_E_BT == 31	;(so we we can skip masking)
	add	cnt_e,cnt_e,t0

	add	v4,v3,RX_DESC_LEN+4	;v4=address of start of DA
	load	0,WORD,v4,v4		;start fetching v4=DA
					;also v4=TRUE if multicast frame

	srl	t0,v2,RX_DESC_A_BT	;count A descriptor bit
	and	t0,t0,1
	add	cnt_a,cnt_a,t0

	and	t3,v7,MAC_FC_VOID_MASK
	cpeq	t3,t3,MAC_FC_VOID
	jmpt	t3,m_rx_void		;junk void frames

	 t_st	t3,ALL_MULTI
	and	t3,t3,v4		;t3=TRUE if multicast & all-multi set
	jmpt	t3,m_rx70		;do not check if want all multicasts

	 add	v5,v3,RX_DESC_LEN+8	;v5=address of rest of DA
	load	0,HWORD,v5,v5		;get v5=rest of the DA

	srl	v6,v4,16
	xor	v6,v6,v4
	xor	v6,v6,v5		;XOR 6 bytes of DA into 8 bits in v6
	srl	t1,v6,8
	xor	v6,v6,t1		;leave top 24 bits garbage for beacon

	cpneq	t3,v7,MAC_FC_BEACON
	jmpf	t3,m_rx_bec
	 xor	bec_da,bec_da,t3	;notice changing beacons
m_rx15:

	srl	t0,v2,RX_DESC_C_BT	;finishing bit counting
	and	t0,t0,1
	add	cnt_c,cnt_c,t0

	srl	t0,v2,RX_DESC_ERR_BT
	srl	t2,v2,RX_DESC_E_BT	;XXX is this really necessary?
	andn	t0,t0,t2		;XXX from what UNH says, it is
	and	t0,t0,1

	and	t1,v6,0xe0
	srl	t1,t1,5-2
	constx	t2,dahash
	add	t1,t1,t2
	load	0,WORD,t3,t1		;t3=fetch hash table entry

	sll	t3,t3,v6
	jmpf	t3,m_rx_junk		;junk frame if hash table says no
	 add	cnt_error,cnt_error,t0

;We have decided the frame is for us.
;   v2=1st word of frame with corrected length, v3=descriptor address, v7=FC
m_rx18:
	load	0,WORD,ibf_raw,ibf_raw
	CK	IBF_P_FOR==0		;advance front end to the next buffer

	rs_st	OUTINT_REQ		;host will hear of old output

	add	v3,v3,RX_DESC_LEN	;v3=address of data

	srl	t0,v2,RX_DESC_LAST_BT	;combine descriptor error bits with FC
	const0	t1,RX_DESC_O_BAD/RX_DESC_LAST
	and	t1,t1,t0
	cpneq	t1,t1,0
	srl	t1,t1,31-RX_DESC_ERR_BT
	or	t0,v2,t1
	srl	t0,t0,RX_DESC_OFFICIAL_BT
	sll	t0,t0,8
	or	v7,v7,t0

	consth	v2,0			;v2=cleaned buffer length
	CK	RX_DESC_F_LEN_WIDTH == 16

; compute the IP checksum of the last 4KB of large packets containing an
;   even numbers of words.
;   Small or odd ones will be byte-copied by the host, which can checksum
;   faster as long as it does not waste the data cache misses.

	sub	b2h_ok,b2h_ok,v2	;limit size of DMA queue

	constx	v5,IN_PG_SIZE		;v5=page size
	sub	v4,v2,v5		;v4=# of bytes to not be checksummed
	cpge	t2, v4,SUM_OFF
	jmpf	t2,m_rx39		;quit if frame is small
	 add	t4,v3,v4		;t4=first byte to add to sum

	and	t2,v2,3
	cpeq	t2,t2,0			;require an even # of words
	jmpf	t2,m_rx39

	 cplt	t0,cksum_ok,CKSUM_MIN
	jmpt	t0,m_rx39
	 const	t0,IN_CKSUM_LEN/N_SUMREG/4-2
	sub	cksum_ok,cksum_ok,CKSUM_MIN

	const	t1,0			;t1=sum
	mtsrim	cr,N_SUMREG-1
m_rx32:	loadm	0,WORD,sumreg0,t4
	add	t1, t1,sumreg0
	.irep	nam, 1,2,3,4,5,6,7,8,9,a,b,c,d,e,f
	  addc	 t1,t1,sumreg@nam@
	.endr
	CK	N_SUMREG==16
	addc	t1,t1,0
	add	t4,t4,N_SUMREG*4	;advance the address
	jmpfdec	t0,m_rx32
	 mtsrim	cr,N_SUMREG-1		;save 1 cycle in loop & waste 1 at end

	 srl	t0,t1,16
	consth	t1,0xffff0000
	add	t1,t1,t0
	addc	t1,t1,0			;fold into 16 bits
	sll	t1,t1,16		;into top 16 bits

	cpeq	t0, t1,0
	sra	t0,t0,16
	xor	t1, t1,t0		;convert 0 to minus-0
	or	v7, v7,t1		;combine cksum with bits & FC
m_rx39:

	store	0,WORD,v7,v3		;put IP checksum, bits & FC in buffer


;   Pick the size of the 1st mbuf.  Either as big as necessary
;   or only the overflow after making the 2nd mbuf 4KB for page flipping.
;   Except that if it will fit in two small buffers, do it that way.

;   If the frame will not fit in two little mbufs, then use a little
;	mbuf for the first part if it will fit.
;   If the frame will fit in two little mbufs, than use a little mbuf
;	for the first part.
;   If we hope to page-flip, then put 4K in the 2nd buffer and whatever
;	is left over in the first buffer.
;   Otherwise, use as big a buffer as necessary for the first part,
;	and another, if necessary, for the remainder.

;   The result of this is that a single frame might use 1 or 2 small
;	mubfs, 1 medium, or 1 or 2 pages.  A single frame will never use
;	two medium sized mbufs.

;   here v2=buffer length, v5=4K,
;    t2=true if total frame length >= 4K+min header size & the size is
;	an even number of words.

	CK	IN_PG_SIZE == IN_CKSUM_LEN	;(check v4 & v5 are useful)
	sra	t2,t2,31
	and	v4,t2,v5		;v4=0 or 4KB if expect to page flip
	sub	v4,v2,v4		;v4=most we want in 1st mbuf

;   v2=buffer length, v4=most wanted for 1st mbuf, v5=4K
	;(do not adjust the following, since it does matter much and the
	;adjustment could overflow)
	cple	t0,v2,SML_NET+SML_SIZE0 ;t0=frame will fit in 2 (32-bit) mbufs
	FIX_SML_NET
	cple	t1,v4,SML_NET		;t1=1st part fits in small mbuf
	or	t2,t1,t0
	jmpf	t2, m_rx43		;jump if should not use a small mbuf
	 SPAD	MED_SIZE-PAD0_IN_DMA
	 const0	t0,MED_SIZE-PAD0_IN_DMA

	FIX_SML_NET
	cpgt	t0,v4,SML_NET
	sra	t0,t0,31
	andn	v4,v4,t0
	FIX_SML_NET
	and	t0,t0,SML_NET
	or	v4,v4,t0		;v4=min(SML_NET, size)

	load	0,WORD,t0,nxt_sml	;use small mbuf for 1st part
	srl	nxt_sml,nxt_sml,2
	add	nxt_sml,nxt_sml,1
	andn	nxt_sml,nxt_sml,MAX_SML
	sll	nxt_sml,nxt_sml,2
	jmp	m_rx45
	 sub	sml_bufs,sml_bufs,1

;   t0=MED_SIZE-PAD_IN_DMA, v2=buffer length, v4=most wanted for 1st, v5=4K
m_rx43:	cple	t0,v4,t0
	jmpf	t0, m_rx44
	 APAD	PAD0_IN_DMA
	 sub	t0,v5,PAD0_IN_DMA

	load	0,WORD,t0,nxt_med	;use a medium sized mbuf for 1st part
	add	nxt_med,nxt_med,4
	andn	nxt_med,nxt_med,MAX_MED*4
	jmp	m_rx45
	 sub	med_bufs,med_bufs,1

;   t0=4K-PAD_IN_DMA, v2=buffer length, v4=most wanted for 1st, v5=4K
m_rx44:	cpge	t1,t0,v4		;limit 1st chunk to space in a page
	sra	t1,t1,31
	andn	t0,t0,t1

	and	v4,v4,t1		;use a page for the 1st part
	or	v4,v4,t0
	load	0,WORD,t0,nxt_big
	add	nxt_big,nxt_big,4
	andn	nxt_big,nxt_big,MAX_BIG*4
	sub	big_bufs,big_bufs,1


m_rx45:	sub	v5,v2,v4
	cple	v6,v5,0
;   here v4=size of first chunk, v5=remaining length,
;   v6=true if 1 mbuf is enough

	;no DMA interrupts until we get entire job into the queue
	mtsrim	CPS,PS_INTSOFF

	consth	v4,DMA_WR
	CK	(DMA_WR & 0xffff) == 0
	srl	t2,v6,31-DMA_FE_BT	;mark this final if it is
	CK	DMA_FE_BT!=31
	or	v4,v4,t2
	store	0,WORD,v4,dlist_head
	CK	DLIST_L == 0
	APAD	PAD0_IN_DMA
	add	t0,t0,PAD0_IN_DMA	;offset 1st part by room for if_header
	add	t2, dlist_head, DLIST_A
	store	0,WORD,t0,t2		;request DMA for 1st part
	INC_DLIST dlist_head,t0,t2

;   Here the DMA request for the 1st part of the frame is in the DMA list.
;   v6=TRUE if the frame fits in 1 buffer,  v4=length of 1st part + DMA bits
;   v5=length of remainder.
	jmpt	v6,m_rx58

;   We need a 2nd buffer, pick a size
	 ASML	SML_SIZE0
	 cple	t0,v5,SML_SIZE0
	jmpf	t0, m_rx53

	 srl	t1,nxt_sml,2
	load	0,WORD,t0,nxt_sml
	add	t1,t1,1
	andn	t1,t1,MAX_SML
	sll	nxt_sml,t1,2
	jmp	m_rx55
	 sub	sml_bufs,sml_bufs,1

m_rx53:	constx	t0,MED_SIZE
	cple	t0,v5,t0
	jmpf	t0, m_rx54
	 nop

	sub	med_bufs,med_bufs,1
	load	0,WORD,t0,nxt_med	;pick correct buffer size for 2nd part
	add	nxt_med,nxt_med,4
	jmp	m_rx55
	 andn	nxt_med,nxt_med,MAX_MED*4

m_rx54:	sub	big_bufs,big_bufs,1
	load	0,WORD,t0,nxt_big
	add	nxt_big,nxt_big,4
	andn	nxt_big,nxt_big,MAX_BIG*4

m_rx55:	consth	v5,DMA_WR + DMA_FE
	CK	((DMA_WR + DMA_FE) & 0xffff) == 0
	store	0,WORD,v5,dlist_head
	CK	DLIST_L == 0
	add	t2, dlist_head, DLIST_A
	store	0,WORD,t0,t2
	INC_DLIST dlist_head,t0,t2	;request DMA for 1st part

	APAD	PAD0_IN_DMA
	add	t0,v4,PAD0_IN_DMA
	call	t4,addb2h		;tell the host about the 1st part
	 consth	t0,B2H_IN
	 CK	(B2H_IN & 0xffff) == 0

	APAD	PAD0_IN_DMA
	sub	v4,v5,PAD0_IN_DMA	;1st part has the if/snoop header

m_rx58:
	APAD	PAD0_IN_DMA
	add	t0,v4,PAD0_IN_DMA
	set_st	SLOINT_REQ
	constx	t4,mlop0
	jmp	addb2h			;tell the host about the last part
	 consth	t0,B2H_IN_DONE
	 CK	(B2H_IN_DONE & 0xffff) == 0



; This is a multicast frame, and we want all multicast frames.
;   Count the status bits that are normally counted while we check
;   the DA hash table.
m_rx70:	srl	t0,v2,RX_DESC_C_BT	;finish bit counting for non-multicast
	and	t0,t0,1
	add	cnt_c,cnt_c,t0

	srl	t0,v2,RX_DESC_ERR_BT
	srl	t2,v2,RX_DESC_E_BT	;XXX is this really necessary?
	andn	t0,t0,t2		;XXX from what UNH says, it is
	and	t0,t0,1
	jmp	m_rx18
	 add	cnt_error,cnt_error,t0



;do something about an excessively long frame
;   t3=-(excess length), v2=descriptor and length, v3=addr of FE, v7=FC
m_rx_long:
	const0	t0,MAX_MAX_FRAME
	add	t0,t0,t3
	jmpt	t0,m_rx_long5		;die if the FE did not stop at 6K

	 add	cnt_longerr,cnt_longerr,1
	add	v2,v2,t3		;fix the length
	constx	t3,RX_DESC_ERR
	jmp	m_rx10
	 or	v2,v2,t3		;set an error bit

m_rx_long5:
	mtsrim  CPS,PS_INTSOFF
	jmp	.			;die on ridiculously long packet
	 halt



;If SA=MA, then snag the timestamp and compute ring latency.
;   This code does not need to be fast, as long as it always requires
;   about the same number of cycles.
;   v1=current time
;   v2=descriptor bits + true length, v3=descriptor address, v7=FC
m_rx_lat:
	add	v4,v3,RX_DESC_LEN+8	;start fetching first 2 bytes of SA
	load	0,WORD,v4,v4		;(and 2 bytes of DA)

	t_st	t0,WAIT_LAT
	jmpf	t0,m_rx10		;do not worry if not waiting for frame

	 sub	v1,txs_time,v1		;v1=start new ring latency value
 .ifdef FSCAN_LATENCY
	sub	v1,v1,RCV_LATENCY
	cpgt	t1,v1,FSCAN_LATENCY
	sra	t1,t1,31
	and	t1,t1,JITTER_LATENCY/2
	sub	v1,v1,t1		;adjust by fast loop
 .else
	sub	v1,v1,RCV_LATENCY+JITTER_LATENCY/2
 .endif

	cpeq	t0,v4,ma_hi		;was it from us?
	jmpf	t0,m_rx10
	 add	v5,v3,RX_DESC_LEN+12
	load	0,WORD,v5,v5		;no, do not worry about it

 .if MIN_RNG_LATENCY > 0
	cpgt	t0,v1,MIN_RNG_LATENCY	;no answer if ring is too small
	sra	t0,t0,31
	and	v1,v1,t0
 .endif

	cpeq	t0,v5,ma_lo
	jmpf	t0,m_rx10		;do not worry about it if not from us

	 constx	t0,ST1_WAIT_LAT | ST1_GET_LATENCY
	 CK	(ST1_WAIT_LAT | ST1_GET_LATENCY)>=256
	andn	cur_st1,cur_st1,t0

	jmp	m_rx10
	 add	cnt_rnglatency,v1,0

;free buffer(s) containing junk frame
;   Useful buffers do not need to be re-linked in the queue of input
;	buffers.  Only junk buffer need to be moved out and put where
;	they will be used before the filled buffers are needed.
;   v0=#-2 of buffers, v2=frame descriptor, ibf_raw=next buffer to free
m_rx_short:
	constn	bec_da,-1
	jmp	m_rx_junk
	 add	cnt_shorterr,cnt_shorterr,1


;ignore void frames
;   v0=#-2 of buffers, v2=frame descriptor, ibf_raw=next buffer to free
m_rx_void:
	add	cnt_junk_void,cnt_junk_void,1
	jmp m_rx_junk
	 constn	bec_da,-1


;ignore a beacon with the same 8-bit hash and garbage other bits from its DA
;   as the previous frame, provided the previous frame was a beacon.
;   v0=#-2 of buffers, v2=frame descriptor, ibf_raw=next buffer to free
m_rx_bec:
	cpeq	t3,bec_da,v6
	jmpf	t3,m_rx15
	 mov	bec_da,v6
	add	cnt_junk_bec,cnt_junk_bec,1

;   v0=#-2 of buffers, v2=frame descriptor, ibf_raw=next buffer to free
m_rx_junk:
	add	cnt_tot_junk,cnt_tot_junk,1	;count junked frame

m_rx_junk0:
	srl	t0,v2,RX_DESC_FLSH_BT	;count error bits in frame
	and	t0,t0,1
	add	cnt_flsh,cnt_flsh,t0

	srl	t0,v2,RX_DESC_ABT_BT
	and	t0,t0,1
	add	cnt_abort,cnt_abort,t0

	srl	t0,v2,RX_DESC_MIS_BT
	and	t0,t0,1
	add	cnt_miss,cnt_miss,t0

	srl	t0,v2,RX_DESC_OVF_BT
	and	t0,t0,1
	add	cnt_rx_ovf,cnt_rx_ovf,t0


;   v0=#-2 of junk buffers,  ibf_raw=junk frame.
;   we will set t0=next raw frame, t1=address of back pointer in junk frame,
	mtsrim	CPS,PS_INTSOFF
 .ifdef RX_CONT
m_rx_junk5:
 .endif
	load	0,WORD,t0,ibf_raw	;t0=next raw frame
	CK	IBF_P_FOR==0
	cpeq	t4,ibf_raw,ibf_full
	jmpt	t4,m_rx_junk8		;avoid relinking if possible
	 add	t1,ibf_raw,IBF_P_BAK	;t1=back pointer in junk buffer

	load	0,WORD,t2,t1		;t2=previous raw frame

	store	0,WORD,t0,t2
	CK	IBF_P_FOR==0		;point neighbor previous to next
	add	t3,t0,IBF_P_BAK
	CK	IBF_P_BAK!=0
	store	0,WORD,t2,t3		;point neighbor next to previous

	add	t2,ibf_full,IBF_P_BAK	;put junk buffer on end of free list
	load	0,WORD,t3,t2		;t3=old next-to-end
	store	0,WORD,ibf_full,ibf_raw	;point junk at new neighbors
	CK	IBF_P_FOR==0
	store	0,WORD,t3,t1

	store	0,WORD,ibf_raw,t3	;point new neighbors at junk
	CK	IBF_P_FOR==0
	store	0,WORD,ibf_raw,t2


	add	ibf_free_cnt,ibf_free_cnt,1
 .ifdef RX_CONT
	jmpfdec	v0,m_rx_junk5		;do it to the entire junk frame
	 mov	ibf_raw,t0
	jmp	mlop0
	 nop
 .else
	jmp	mlop0
	 mov	ibf_raw,t0
 .endif


;if there are no waiting, full but skimmed frames, simply move
;   the full pointer and the raw pointer past the frame.
m_rx_junk8:
	mov	ibf_raw,t0
	mov	ibf_full,t0
	add	ibf_free_cnt,ibf_free_cnt,1
 .ifdef RX_CONT
	jmpfdec	v0,m_rx_junk5		;do it to the entire junk frame
	 nop
 .endif
	jmp	mlop0
	 nop

	.eject
;restore input when buffers are drained
;   reset the whole front end.
m_rx_on:
	mtsrim	CPS,PS_INTSOFF

	cpeq	t1,ibf_full,ibf_raw	;ensure that no interrupts happened
	and	t1,t1,ibf_raw_cnt	;	while we were checking
	andn	t2,ibf_free_cnt,misfrm_lim
	andn	t1,t1,t2
	jmpf	t1,m_rx_on9
	 nop

	PERF_TP	t0,PERF_RX_ON

	FE_RESET t0
	FE_INIT	t0,t1,

m_rx_on9:
	jmp	mlop0
	 nop

	.eject
;(re)start TX after a xmit abort or receiving new work
m_tx:
	mtsrim	CPS,PS_INTSOFF

	PERF_TP	t0,PERF_TX_START

	sll	t0,formac_stat,31-FORMAC_SRNGOP_BT
	add	t1,obf_cnt,1
	and	t1,t1,t0
	jmpf	t1,mlop0		;guard against extra TX interrupt
	 nop

	store	0,WORD,txa_dmacs,txa_dmacs	;start the front end
	or	cur_st1,cur_st1,ST1_LED_ACT1 | ST1_LED_ACT2 | ST1_TXA
	load	0,WORD,t0,obf_cur

	ADV_OBF	t0,mlop0,		;advance pointer to next buffer

	.eject
;poll the 9513s
;   The counters must be polled often to prevent overflow.
;   v0=unchanged, v7=return, t0,t1,t2,v1-v6
;   on exit interrupts are off
p9513:
	constx	v1, IO_CNT1_CMD
	constx	v3, IO_CNT2_CMD
	mtsrim	CPS,PS_INTSOFF		;interrupts off for valid #s and ticks

	CNTCMD2	IOCNT_SAVE_ALL, v1,v3, 2+CNTCMD2_OVHD
	xor	v2, v1, IO_CNT1_DATA ^ IO_CNT1_CMD
	xor	v4, v3, IO_CNT2_DATA ^ IO_CNT2_CMD

;start with counter after noise detector, so we do not have to read it
	CNTCMD2	IOCNT_HOLD+IO_CNT_NE+1, v1,v3, 3+CNTRD2_OVHD
	CK	IO_CNT_NE!=5
	mtsrim	BP,2			;set Byte Pointer for ACCUP macros

	CNTRD2	v5,v6, v2,v4, ACCUP_OVHD*2+CNTRD2_OVHD
	ACCUP	v5,cnt_frame_ct
	ACCUP	v6,cnt_err_ct

	CNTRD2	v5,v6, v2,v4, WAIT_9513+ACCUP_OVHD*2
	ACCUP	v5,cnt_lost_ct
	ACCUP	v6,cnt_tok_ct


	CNTRD2	v5,v6, v2,v4, ACCUP_OVHD*2+CNTRD2_OVHD
	ACCUP	v5,cnt_lem1_usec4
	ACCUP	v6,cnt_lem2_usec4

	CNTRD2	v5,v6, v2,v4, ACCUP_OVHD*2+CNTRD2_OVHD
	ACCUP	v5,cnt_lem1_ct
	ACCUP	v6,cnt_lem2_ct

	constx	t1,TIME_INTVL		;update free running usec clock
	mov	cnt_ticks,time_hz
	mfsr	t0,TMC
	sub	t1,t1,t0
	add	cnt_ticks,cnt_ticks,t1
	srl	cnt_ticks,cnt_ticks,MHZ_BT

	jmpi	v7			;note when counters last polled
	 srl	ct_age,t0,TIME_INTVL_BT+1-UP9513_BT
	CK	UP9513_BT<TIME_INTVL_BT


	.eject
;Handle a Noise Event.

;The TC output of the 9513s is only a pulse.  We cannot poll for it.
;   We do not want to touch a 9513 in the interrupt handler, because
;   that would force us to turn off interrupts when doing normal
;   9513 polling.

;Come here with interrupts off, to keep from sticking in the FORMAC/CMT
;   trap handler.
mnoise:
	const	t3, IOCNT_CLR_TC+IO_CNT_NE
	constx	v1, IO_CNT1_CMD
	constx	v3, IO_CNT2_CMD
	t_st	t1,NE1
	jmpf	t1,mnoise5
	 t_st	t0,NE2

	store	0,HWORD,t3, v1
	FE_DELAY 2,
	add	cnt_noise1,cnt_noise1,1
	set_st	PINT_REQ

mnoise5:
	jmpf	t0, mnoise9
	 nop

	store	0,HWORD,t3, v3
	FE_DELAY 2,
	add	cnt_noise2,cnt_noise2,1
	set_st	PINT_REQ

mnoise9:
	consth	cur_st2,0
	CK	((ST2_NE|ST2_NE1|ST2_NE2) & 0xffff) == 0
	constx	cps_shadow,PS_INTSON

;Things must be quiet before we turn interrupts on, or we suffer an
;   illegal instruction trap.
	NDELAY	t0, WR_REC_9513, WAIT_9513+4
	jmp	mlop0
	 nop

	.eject
;tell host about any FORMAC interrupts
mformac:
	rs_st	FORINT
	mov	v2,formac_stat		;snapshot FORMAC interrupt bits

	constx	v3,FORMAC_SRNGOP	;gobble all but RNGOP
	andn	v3, v2,v3
	andn	formac_stat, formac_stat,v3


;tell host about a change in RNGOP
	srl	v4,v2,FORMAC_SRNGOP_BT-ST1_RNGOP_BT
	CK	FORMAC_SRNGOP_BT>ST1_RNGOP_BT
	and	v4,v4,ST1_RNGOP
	xor	v5,cur_st1,v4
	sll	v6,v5,31-ST1_RNGOP_BT
	jmpf	v6,mformac3		;no change in RNGOP
	 srl	v4,v4,ST1_RNGOP_BT
	xor	cur_st1,cur_st1,ST1_RNGOP
	add	cnt_rngop,cnt_rngop,v4
	xor	t0,v4,1
	add	cnt_rngbroke,cnt_rngbroke,t0

	set_st	PINT_REQ,		;interrupt the host
	sll	t0,v4,ST1_GET_LATENCY_BT
	or	cur_st1,cur_st1,t0	;get new ring latency if ring ok
mformac3:



;handle the simple bits

 .macro	FCNT,	cnm,bnm,intr
 .if FORMAC_S@bnm@_BT != 0
	  srl	  v4,v2,FORMAC_S@bnm@_BT
	  and	  v4,v4,1
 .else
	  and	  v4,v2,1
 .endif
	  add	  cnt_@cnm@,cnt_@cnm@,v4
 .ifeqs "@intr@","intr"
	  sll	  v4,v4,ST2_PINT_REQ_BT
	  or	  cur_st2,cur_st2,v4	;interrupt the host
 .endif
 .endm
	CK	ST2_PINT_REQ_BT > 0

	FCNT	pos_dup,MULTDA,,
	FCNT	misfrm,	MISFRM,,
	sub	misfrm_lim,misfrm_lim,v4    ;reset front end after overflow
	FCNT	xmtabt,	XMTABT,,
	FCNT	tkerr,	TKERR,,
	FCNT	clm,	CLM,	intr,
	FCNT	bec,	BEC,	intr,
	FCNT	tvxexp,	TVXEXP,	intr,
	FCNT	trtexp,	TRTEXP,	intr,
	FCNT	tkiss,	TKISS,,
	FCNT	myclm,	MYCLM,,
	FCNT	loclm,	LOCLM,,
	FCNT	hiclm,	HICLM,,
	FCNT	mybec,	MYBEC,,
	FCNT	otrbec,	OTRBEC,,
	.purgem	FCNT

	tbit	t0,v2,FORMAC_SMYCLM
	tbit	t1,v2,FORMAC_SMYBEC
	or	t0,t0,t1		;t0=TRUE=received my claim or beacon
	mfsr	t1,TMC
	sub	t1,txs_time,t1		;t1=ticks since xmit
	constx	t2,(D_Max*2)*MHZ
	cpgt	t1,t1,t2		;t1=TRUE if no xmit in D_Max*2
	and	t0,t0,t1
	srl	t0,t0,31		;t0=1 if possible
	add	cnt_dup_mac,cnt_dup_mac,t0
	sll	t0,t0,31-ST2_PINT_REQ_BT
	or	cur_st2,cur_st2,t0	;interrupt if duplicate apparent


;get the MAC state
	constx	t1,FORMAC_MMODE_OFF
	and	t2,t1,formac_mode
	constx	t1,FORMAC_MMODE_ONL
	cpeq	t1,t1,t2
	jmpf	t1,mlop
	 const	cnt_mac_state,MAC_OFF

	jmpf	txs_cnt,mlop
	 const	cnt_mac_state,MAC_ACTIVE

	cpeq	t0,txs_addr,txs_claim
	jmpt	t0,mlop
	 const	cnt_mac_state,MAC_CLAIMING
	jmp	mlop
	 const	cnt_mac_state,MAC_BEACONING

	.eject
;host command table
cmd_tbl:.word	cmd_done		;00
	.word	cmd_done		;01 should never be used
	.word	cmd_vme			;02
	.word	cmd_flush		;03
	.word	cmd_phy1_cmd		;04
	.word	cmd_phy2_cmd		;05
	.word	cmd_phy_rep		;06
	.word	cmd_mux			;07
	.word	cmd_bypass_ins		;08
	.word	cmd_bypass_rem		;09
	.word	cmd_mac_aram		;0a
	.word	cmd_mac_mode		;0b
	.word	cmd_mac_parms		;0c
	.word	cmd_mac_state		;0d
	.word	cmd_claim		;0e
	.word	cmd_beacon		;0f
	.word	cmd_phy_arm		;10
	.word	cmd_wakeup		;11
	.word	cmd_fet_tneg		;12
	.word	cmd_fet_ls1		;13
	.word	cmd_fet_ls2		;14
	.word	cmd_fet_board		;15
	.word	cmd_fet_ct		;16
	.word	cmd_fet_nvram		;17
	.word	cmd_sto_nvram		;18
cmd_tbl_end:


;process a host command
;   This code, including many of the commands, expects to have interrupts
;	disabled.
;   Fetch the request and jump thru the table.
cmd:	load	0,HWORD,t1,p_mailbox	;fetch command

	constx	v0,SIO_ACT_DATA		;many commands can use v0
	constx	t2,cmd_tbl

	cpltu	t3,t1,(cmd_tbl_end-cmd_tbl)/4
	jmpf	t3,cmd_error
	 sll	t4,t1,2
	add	t2,t2,t4

	load	0,WORD,t3,t2		;fetch function address
	constx	t4,IO_CLRMBINT
	load	0,WORD,v5,t4		;clear the interrupt
	jmpi	t3			;go to the function
	 consth	cur_st1,0
	CK	ST1_MB == TRUE


;bad command from the host
cmd_error:
	load	0,HWORD,t2,p_mailbox
	jmp	.			;the mailbox already has a good flag
	 halt




;host commands
;   All of these commands are entered with v0=address of SIO_ACT_DATA


;set VME parameters
;   The Bus Request Level, interrupt vector, and whether to do VME Block-Mode
;	transfers must be set by the host.
;   This assume interrupts are off.
 .macro	ghword,	reg,addr,i1,i2
	  add	  v0,v0,SIO_INCR
	  load	  0,HWORD,reg,v0
  .ifnes "@addr@",""
	  constx  v3,addr
  .endif
  .ifnes "@i1@",""
	  i1	  reg,reg,i2
  .endif
  .ifnes "@addr@",""
	  store	  0,WORD,reg,v3
  .endif
 .endm

 .macro	gword,	reg,addr,i1,i2
	  ghword  reg
	  ghword  v2
	  sll	  reg,reg,16
  .ifnes "@addr@",""
	  constx  v3,addr
  .endif
	  or	  reg,reg,v2
  .ifnes "@i1@",""
	  i1	  reg,reg,i2
  .endif
  .ifnes "@addr@",""
	  store	  0,WORD,reg,v3
  .endif
 .endm

 .macro gtime,	reg,addr
	  gword  reg,addr,sll,MHZ_BT,
 .endm

cmd_vme:load	0,HWORD,v1,v0
	constx	t0,IO_EXPORT_IP
	andn	t0,export,t0
	or	t0,t0,IO_EXPORT_SEQ
	sll	v1,v1,IO_EXPORT_SEQ_BT	;set block mode switch
	CK	IO_EXPORT_SEQ_BT > 0
	xor	export,t0,v1		;hardware wants 0=on

	ghword	v1,,,
	or	export,export,v1	;set VME BRL
	store	0,WORD,export,p_export

	ghword	v1,,,			;VME interrupt vector
	constx	v2,VME_VEC
	const	t1,0xff00
	or	v1,v1,t1
	store	0,WORD,v1,v2

	ghword	v1,,,			;VME Address Modifier & burst count
	constx	v2,IO_LDAM
	sll	v1,v1,16
	store	0,WORD,v1,v2

	ghword	v1,,,			;configuration flags
	constx	t0, ST1_LAT
	andn	cur_st1,cur_st1,t0
	and	t0,v1,IPG_LAT
	sll	t0,t0,ST1_LAT_BT-IPG_LAT_BT
	CK	ST1_LAT_BT > IPG_LAT_BT
	or	cur_st1,cur_st1,t0

	gword	v1,cksum_max,,
	const	cksum_ok,0

	gword	b2h_buf,,,
	constx	v3,B2H_BASE
	sub	b2h_buf,b2h_buf,v3

	ghword	v1,,,			;B2H_LEN
	constx	v2,B2H_LEN
	cpeq	v2,v2,v1		;must match the host
	jmpf	v2,bad_b2h_len
	 nop

	ghword	v1,max_bufs,sub,MIN_BIG+MIN_MED+MIN_SML

	gword	h2b_pos,h2b_buf,,

	gtime	h2b_dma_delay,
	gtime	v1,b2h_dma_delay,

	gword	v1,int_delay,
	gtime	v1,max_host_work,

	gtime	h2b_sleep,,

    ;adjust all uses of SML_SIZE0 and PAD0_IN_DMA to match 64-bit IRIX.
 .macro	fixirix, op,tbl,len
	  constx  v2,tbl
	  const	  v3,len-2
$1:
	  load	  0,WORD,t3,v2
	  add	  v2,v2,4
	  load	  0,WORD,t4,t3
	  op	  t4,t4,v1
	  jmpfdec v3,$1
	   store  0,WORD,t4,t3
 .endm

	ghword	v1,,sub,SML_SIZE0
	fixirix	add,asml_tbl,NUM_ASML

	ghword	v1,,sub,PAD0_IN_DMA
	fixirix	add,apad_tbl,NUM_APAD

	fixirix	sub,spad_tbl,NUM_SPAD

	jmp	cmd_done
	 nop

	.purgem	ghword,gword,gtime,fixirix


bad_b2h_len:
	jmp	.
	 halt



;flush pending DMA requests
;   interrupts better be off here.
cmd_flush:
	FE_RESET t0			;release the token and stop output
	FE_INIT	t0,t1,

	jmp	cmd_done
	 nop



;send a command to an ENDEC
;   v0=address of command in DRAM, v1=ENDEC address
cmd_phy1_cmd:
	load	0,HWORD,v0,v0		;fetch host command
	const	v1,IO_ENDEC1_SEL
	jmp	phy_cmd
	 consth	v1,IO_ENDEC1_SEL

cmd_phy2_cmd:
	load	0,HWORD,v0,v0		;fetch host command
	constx	v1,IO_ENDEC2_SEL

phy_cmd:
	constx	t0,ST1_PHY_REP
	andn	cur_st1,cur_st1,t0	;stop repeating

	store	0,HWORD,v0,v1		;select the correct ENDEC register
	ENDEC_REC t0,3

	sll	v3,v0,ENDEC_SLL
	xor	v1,v1,IO_ENDEC1_REG ^ IO_ENDEC1_SEL

	jmp	cmd_done		;set the register
	 store	0,HWORD,v3,v1
	ENDEC_REC t0,20


;make the ENDEC(s) repeat
;   assume interrupts are off
cmd_phy_rep:
	set_st	PHY_REP,t0
	rs_st	PINT_ARM,t0

	constx	v2,IO_ENDEC1_SEL
	const0	v1,ENDEC_CR1_REPEAT	;get ENDECs ready to repeat
	store	0,HWORD,v1,v2
	ENDEC_REC t1,2
	sll	v1,v1,ENDEC_SLL
	xor	t1,v2,IO_ENDEC1_REG ^ IO_ENDEC1_SEL
	store	0,HWORD,v1,t1
	ENDEC_REC t1,1
	const	cmt1,0
	store	0,BYTE,cmt1,p_cmt1
	FE_DELAY 0,
	load	0,BYTE,v1,p_cmt1	;repeat the current line states
	FE_DELAY 2,
	call	t4,rep_ls
	 nop

	constx	v2,IO_ENDEC2_SEL
	const0	v1,ENDEC_CR1_REPEAT
	store	0,HWORD,v1,v2
	ENDEC_REC t1,2
	sll	v1,v1,ENDEC_SLL
	xor	t1,v2,IO_ENDEC1_REG ^ IO_ENDEC1_SEL
	store	0,HWORD,v1,t1
	ENDEC_REC t1,1
	const	cmt2,0
	store	0,BYTE,cmt2,p_cmt2
	FE_DELAY 0,
	load	0,BYTE,v1,p_cmt2
	FE_DELAY 2,
	call	t4,rep_ls
	 nop

	constx	t0,FORMAC_MMODE_OFF	;turn off the FORMAC
	andn	formac_mode,formac_mode,t0
	xor	t0,p_formac_stmc, IO_FORMAC_MODE ^ IO_FORMAC_STMC
	store	0,HWORD,formac_mode,t0
	FORMAC_REC 20,
	jmp	cmd_done
	 nop



;set the inter-board MUX
cmd_mux:
	load	0,HWORD,v0,v0
	const	v1,CMD1_FCMS_EXT|CMD1_FCMS_END|CMD1_FCMS_FOR|CMD1_DATTACH
	andn	cmd1,cmd1,v1
	or	cmd1,cmd1,v0
	jmp	cmd_done
	 store	0,HWORD,cmd1,p_cmd1


cmd_bypass_ins:
	const0	v1, CMD1_OBC1 | CMD1_OBC2
	or	cmd1, cmd1, v1
	jmp	cmd_done
	 store	0,HWORD,cmd1,p_cmd1


cmd_bypass_rem:
	const0	v1, CMD1_OBC1 | CMD1_OBC2
	andn	cmd1, cmd1, v1
	jmp	cmd_done
	 store	0,HWORD,cmd1,p_cmd1



;set the contents of the FORMAC "ARAM"
;   The FORMAC must have been reset.
cmd_mac_aram:
	constx	t0,FORMAC_MMODE_OFF
	andn	formac_mode,formac_mode,t0
	xor	t0,p_formac_stmc, IO_FORMAC_MODE ^ IO_FORMAC_STMC
	store	0,HWORD,formac_mode,t0
	FORMAC_REC 2,

	constx	t1,IO_FORMAC_ADRS
	const	v2,8-2

	xor	t0,p_formac_stmc, IO_FORMAC_CLAPTR ^ IO_FORMAC_STMC
	store	0,HWORD,v2,t0		;get the FORMAC ready
	FORMAC_REC 0,

aram5:	load	0,HWORD,t0,v0
	store	0,HWORD,t0,t1		;send the addresses to it
	FORMAC_REC 2,
	jmpfdec	v2,aram5
	 add	v0,v0,SIO_INCR

	jmp	cmd_done
	 nop



;set FORMAC mode register
cmd_mac_mode:
	load	0,HWORD,formac_mode,v0	;get FORMAC mode
	add	v0,v0,SIO_INCR
	constx	t2,IO_FORMAC_MODE
	store	0,HWORD,formac_mode,t2
	FORMAC_REC 2,

	load	0,HWORD,t0,v0		;1=receive all multicast frames
	add	v0,v0,SIO_INCR
	and	t0,t0,1
	sll	t0,t0,ST2_ALL_MULTI_BT
	CK	ST2_ALL_MULTI_BT > 0
	rs_st	ALL_MULTI,t1
	or	cur_st2,cur_st2,t0

	const	t0,DAHASH_SIZE/4-2	;set address filter
	constx	v1,dahash
set_filter1:
	load	0,HWORD,t1,v0
	add	v0,v0,SIO_INCR
	sll	t1,t1,16
	load	0,HWORD,t2,v0
	add	v0,v0,SIO_INCR
	or	t1,t1,t2
	store	0,WORD,t1,v1
	jmpfdec	t0,set_filter1
	 add	v1,v1,4

	jmp	cmd_done
	 nop

	.eject
;set FORMAC parameters
;   v0=source in DRAM, v1=values, v2=FORMAC address
cmd_mac_parms:
	CK	SIO_T_MAX == SIO_ACT_DATA
	load	0,HWORD,v1,v0		;set T_MAX
	xor	v2,p_formac_stmc,IO_FORMAC_TMAX ^ IO_FORMAC_STMC
	store	0,HWORD,v1,v2
	FORMAC_REC 0,

	xor	v0,v0, SIO_TVX ^ SIO_T_MAX
	load	0,HWORD,v1,v0		;set TVX
	xor	v2,v2,IO_FORMAC_TVX ^ IO_FORMAC_TMAX
	store	0,HWORD,v1,v2
	FORMAC_REC 0,

	const0	formac_imsk,FORMAC_INTS | FORMAC_MORE_INTS
	xor	v2,v2,IO_FORMAC_IMSK ^ IO_FORMAC_TVX
	store	0,HWORD,formac_imsk,v2	;choose the interrupts
	FORMAC_REC 0,

	jmp	cmd_done
	 const	txs_cnt,0		;stop beaconing or claiming



;set MAC state
formac_st:				;MAC states to formac commands
	.word	IO_FORMAC_RESET			;MAC_OFF
	.word	IO_FORMAC_IDLS			;MAC_IDLE
	.word	IO_FORMAC_CLLS			;MAC_CLAIMING
	.word	IO_FORMAC_BCLS			;MAC_BEACONING
	.word	IO_FORMAC_IDLS			;MAC_ACTIVE

cmd_mac_state:
	load	0,HWORD,v0,v0
	constx	v1,formac_st
	sll	t0,v0,2
	add	t0,t0,v1		;index into table
	load	0,WORD,t0,t0		;to get correct FORMAC "instruction"

	const	txs_cnt,0		;stop beaconing or claiming
	store	0,HWORD,txs_cnt,t0	;zap the FORMAC
	FORMAC_REC 0,

	const	t0,FORMAC_SRNGOP	;assume we have killed the ring
	andn	formac_stat,formac_stat,t0
	set_st	FORINT

	cpeq	t0,v0,MAC_OFF		;notice when we kill the FORMAC
	jmpf	t0,cmd_done
	 nop

	const	formac_mode,0
	jmp	cmd_done
	 add	cnt_rngbroke,cnt_rngop,0



;set claim frame
;   v0=source in DRAM
cmd_claim:
	const	v1,CLAIM_BASE
	CK	(BEACON_BASE & 0xffff0000) == (CLAIM_BASE & 0xffff0000)

;   v0=source in DRAM, v1=partial target address
set_frame:
	load	0,HWORD,v2,v0		;fetch frame length
	add	v0,v0,SIO_INCR

	consth	v1,CLAIM_BASE		;finish target address
	CK	(BEACON_BASE & 0xffff0000) == (CLAIM_BASE & 0xffff0000)

	subr	v3,v2,0
	consth	v3,OBF_DESC		;build front end TX descriptor
	store	0,WORD,v3,v1

	sub	v2,v2,8-(OBF_PAD_LEN+3)	;v2=transfer length, word length -2:
	sra	v2,v2,2			;   -1 = store above & -1 = jmpfdec

;   v0=source in DRAM, v1=first pad byte in target buffer, v2=word length-1
set_frame5:
	add	v1,v1,4
	load	0,HWORD,v3,v0		;copy the frame to its buffer
	add	v0,v0,SIO_INCR
	load	0,HWORD,v4,v0
	add	v0,v0,SIO_INCR		;combine 2 half-words
	sll	v3,v3,16
	or	v4,v4,v3
	jmpfdec	v2,set_frame5		;copy fixed amount
	 store	0,WORD,v4,v1

;   build the ring-latency frame
	constx	v0,CLAIM_BASE		;use MAC header of CLAIM frame
	mtsrim	cr,(LAT_SIZE+3)/4-1
	loadm	0,WORD,sumreg0,v0
 .if LAT_LEN < CLAIM_LEN
	add	sumreg0,sumreg0, CLAIM_LEN-LAT_LEN
 .else
   .if LAT_LEN > CLAIM_LEN
	sub	sumreg0,sumreg0, LAT_LEN-CLAIM_LEN
   .endif
 .endif
	const	sumreg1,MAC_FC_LLC_L	;convert FC to LLC

	add	ma_hi,sumreg3,0		;save my address
	add	ma_lo,sumreg4,0

	add	v0,v0,LAT_BASE-CLAIM_BASE
	mtsrim	cr,(LAT_SIZE+3)/4-1
	storem	0,WORD,sumreg0,v0

	jmp	cmd_done
	 nop


;set beacon frame
cmd_beacon:
	jmp	set_frame
	 const	v1,BEACON_BASE

	.eject
;fetch the current value of TNeg from the formac
cmd_fet_tneg:
	const	v1,IO_FORMAC_TNEG
	consth	v1,IO_FORMAC_TNEG
	load	0,HWORD,v2,v1
	FORMAC_REC 3,
	consth	v2,-1
	sll	v2,v2,5

	xor	v1,v1,IO_FORMAC_TLSB ^ IO_FORMAC_TNEG
	load	0,HWORD,v3,v1
	FORMAC_REC 8,
	srl	v3,v3,10
	and	v3,v3,0x1f
	or	v2,v2,v3

	sra	v3,v2,16
	store	0,HWORD,v3,v0
	add	v0,v0,SIO_INCR
	jmp	cmd_done
	 store	0,HWORD,v2,v0


;fetch the current line state
;   assume interrupts are disabled
 .macro	CMD_FET_LS, n
	  const	  cmt@n@,0
	  store	  0,BYTE,cmt@n@,p_cmt@n@
	  FE_DELAY 0,
	  load	  0,BYTE,v1,p_cmt@n@
	  FE_DELAY 3,
	  const	  v2,IO_ENDEC@n@_SEL
	  call	  t4,rep_ls
	   consth v2,IO_ENDEC@n@_SEL
	  jmp	  cmd_done
	   store  0,HWORD,v1,v0		;tell the host
 .endm
cmd_fet_ls1:
	CMD_FET_LS  1,

cmd_fet_ls2:
	CMD_FET_LS  2,
	nop				;things must be decodable after branch

;translate CMT/ENDEC line states for the PCM state machine
endec2ls:
	.byte	PCM_ILS			;ENDEC_STATUS_UNKNOWN
	.byte	PCM_ILS			;ENDEC_STATUS_NLS
	.byte	PCM_MLS			;ENDEC_STATUS_MLS
	.byte	PCM_ILS			;ENDEC_STATUS_ILS

	.byte	PCM_HLS			;ENDEC_STATUS_HLS
	.byte	PCM_QLS			;ENDEC_STATUS_QLS
	.byte	PCM_ILS			;ENDEC_STATUS_ALS
	.byte	PCM_ILS			;ENDEC_STATUS_ELASTICITY
	.align

;translate converted ENDEC line states to ENDEC line state commands
ls2ls:
	.hword	ENDEC_CR0_QLS		;PCM_QLS
	.hword	ENDEC_CR0_FILTER	;PCM_ILS
	.hword	ENDEC_CR0_MLS		;PCM_MLS
	.hword	ENDEC_CR0_HLS		;PCM_HLS

	.hword	ENDEC_CR0_FILTER	;PCM_ALS
	.hword	ENDEC_CR0_FILTER	;PCM_ULS
	.hword	ENDEC_CR0_FILTER	;PCM_NLS
	.hword	ENDEC_CR0_FILTER	;PCM_LSU
	.align


;convert the current line state, and possibly repeat it
;   on entry, t4=return, v1=raw line state,
;	v2=IO_ENDEC1_SEL or IO_ENDEC2_SEL,
;   on exit,  v1=converted state,  t0,t1,t2=scratch,  all else unchanged
rep_ls:
	and	t0,v1,CMT_MASK
	const0  t1,endec2ls
	add	t1,t1,t0		;convert to official value
	load	0,BYTE,v1,t1

	t_st	t0,PHY_REP
	jmpfi	t0,t4			;finished if not repeating
	 const0	t0,ls2ls

	add	t0,t0,v1		;repeat it if we are supposed to
	add	t0,t0,v1
	load	0,HWORD,t0,t0
	store	0,HWORD,t0,v2
	ENDEC_REC t2,3

	sll	t0,t0,ENDEC_SLL
	xor	t2,v2,IO_ENDEC1_REG ^ IO_ENDEC1_SEL
	 store	0,HWORD,t0,t2
	ENDEC_REC t2,2			;quiet before interrupts on

	jmpi	t4
	 nop




;fetch board type
;   return
;	|=BD_TYPE_MAC2 if 2nd board/MAC present
;	|=BD_TYPE_BYPASS if bypass is pressent.
cmd_fet_board:
	load	0,HWORD,t0,p_cmd1	;get status bits

	srl	t1,t0,STAT1_MAC2_BT-BD_TYPE_MAC2_BT
	CK	STAT1_MAC2_BT>BD_TYPE_MAC2_BT
	and	t2,t1,BD_TYPE_MAC2

	srl	t1,t0,STAT1_OBC_BT-BD_TYPE_BYPASS_BT
	CK	STAT1_OBC_BT>BD_TYPE_BYPASS_BT
	and	t1,t1,BD_TYPE_BYPASS
	or	t2,t2,t1

	jmp	cmd_done
	 store	0,HWORD,t2,v0

	.eject
;fetch counters
;   use v0=pointer to shared memory, t0=scratch
cmd_fet_ct:
	consth	h2b_delay,TIME_NOW	;see what the host has to say

	call	v7,p9513
	 nop

 .macro	CNT,	nam
	  srl	  t0,cnt_@nam@,16
	  store	  0,HWORD,t0,v0
	  add	  v0,v0,SIO_INCR
	  store	  0,HWORD,cnt_@nam@,v0
	  add	  v0,v0,SIO_INCR
 .endm
	CNTRS
	.purgem	CNT

;remember to get ring latency for next time.
	constx	cnt_rnglatency,MAX_RNG_LATENCY*2
	set_st	GET_LATENCY,t0
	jmp	cmd_done
	 nop

	.eject
;fetch MAC address from NVRAM
cmd_fet_nvram:
	call	v1,novram_rcl
	 nop

	const0	v1,SIZE_NOVRAM-2
fet_nvram1:
	sll	v4,v1,3
	call	t4,novram_cmd
	 subr	v4,v4,NOVRAM_READ+((SIZE_NOVRAM-2)<<3)

	const	v4,0
	const	v6,16-2			;here the clock is still high
	constx	v7,IO_IMPORT
fet_nvram2:
	const0	v5,IO_EXPORT_NVC
	andn	export,export,v5	;set clock low for half a period
	store	0,WORD,export,p_export	;SK=-  CE=+
	NDELAY	v5,375,0		;"data setup time"

	load	0,HWORD,v5,v7		;sample DO before rising edge of SK
	and	v5,v5,IO_IMPORT_NVD
	CK	IO_IMPORT_NVD==1
	sll	v4,v4,1
	or	v4,v4,v5

	NDELAY	v5,500-375,2		;rest of clock pulse

	const0	v5,IO_EXPORT_NVC
	or	export,export,v5
	store	0,WORD,export,p_export	;SK=+  CE=+
	NDELAY	v5,500,2+2		;clock pulse

	jmpfdec	v6,fet_nvram2		;get the next bit
	 nop

	store	0,HWORD,v4,v0		;save a completed half word

	jmpfdec	v1,fet_nvram1		;get the next half word
	 add	v0,v0,SIO_INCR

	const0	t4,cmd_done
	jmp	novram_cmd		;let the chip recover
	 const	v4,0



;store MAC address in NVRAM
cmd_sto_nvram:
	call	v1,novram_rcl
	 nop

	call	t4,novram_cmd
	 const	v4,NOVRAM_WREN

	const0	v1,SIZE_NOVRAM-2
sto_nvram1:
	sll	v4,v1,3
	call	t4,novram_cmd
	 subr	v4,v4,NOVRAM_WRITE+((SIZE_NOVRAM-2)<<3)

	load	0,HWORD,v4,v0
	add	v0,v0,SIO_INCR
	const	v6,16-2
sto_nvram2:
	const0	v5,IO_EXPORT_NVD | IO_EXPORT_NVC
	andn	export,export,v5
	CK	IO_EXPORT_NVD_BT==15
	const0	v5,IO_EXPORT_NVD
	and	v5,v5,v4
	or	export,export,v5
	store	0,WORD,export,p_export	;SK=-  CE=+ DI=b(n)
	NDELAY	v5,500,2		;"data setup time" and clock pulse

	const0	v5,IO_EXPORT_NVC
	or	export,export,v5
	store	0,WORD,export,p_export	;SK=+  CE=+ DI=b(n)
	NDELAY	v5,500,2+5		;"data hold" and half clock freq.

	jmpfdec	v6,sto_nvram2
	 sll	v4,v4,1

	jmpfdec	v1,sto_nvram1		;write all 16 half-words to RAM
	 nop

	call	t4,novram_cmd		;then tell chip to save them
	 const	v4,NOVRAM_STO

	const0	v5,IO_EXPORT_NVD | IO_EXPORT_NVC
	andn	export,export,v5
	store	0,WORD,export,p_export	;SK=- CE=+ DI=-
	NDELAY	v5,500,3		;"chip enable hold" for the store

	const0	v4,10000*2		;wait 10 msec
	const0	v6,IO_EXPORT_NVC
sto_nvram8:
	xor	export,export,v6
	store	0,WORD,export,p_export	;SK=toggle CE=+ DI=-
	NDELAY	v5,500,4
	jmpfdec	v4,sto_nvram8
	 nop

	const0	v5,IO_EXPORT_NVE | IO_EXPORT_NVD | IO_EXPORT_NVC
	andn	export,export,v5
	jmp	cmd_done
	 store	0,WORD,export,p_export	;SK=- CE=- DI=-



;recall NVRAM contents
;   v1=return,   t4,v4,v5,v6=scratch
novram_rcl:
	call	t4,novram_cmd
	 const	v4,NOVRAM_RCL

	const0	t0,IO_EXPORT_NVC	;let it recover
	andn	export,export,t0
	store	0,WORD,export,p_export	;SK=- CE=+ DI=+
	const0	t0,IO_EXPORT_NVD
	andn	export,export,t0
	store	0,WORD,export,p_export	;SK=- CE=+ DI=-
	NDELAY	v5,2500,2

	jmpi	v1
	 nop



;send a command to the NVRAM
;   t4=return, v4=command, v5=scratch, v6=scratch
novram_cmd:
    ;finish the previous command and otherwise reset the instruction register
	const0	v6,IO_EXPORT_NVC
	andn	export,export,v6
	store	0,WORD,export,p_export	;SK=-  CE=?  DI=?
	NDELAY	v5,500,2		;"chip enable hold" of previous cmd

	const0	v5,IO_EXPORT_NVD | IO_EXPORT_NVE
	andn	export,export,v5
	store	0,WORD,export,p_export	;SK=-  CE=-  DI=-
	or	export,export,v6
	store	0,WORD,export,p_export	;SK=+  CE=-  DI=-
	NDELAY	v5,500,2		;"chip deselect"
	andn	export,export,v6
	store	0,WORD,export,p_export	;SK=-  CE=-  DI=-
	NDELAY	v5,800-500,2		;rest of "chip deselect"

	const0	v5,IO_EXPORT_NVE
	or	export,export,v5
	store	0,WORD,export,p_export	;SK=-  CE=+  DI=?
	NDELAY	v5,800-500,7		;"chip enable setup"
					;reset done during neg. clock phase
	const	v6,8-2
novram_cmd1:
	const0	v5,IO_EXPORT_NVD | IO_EXPORT_NVC
	andn	export,export,v5
	and	v5,v4,0x80
	CK	IO_EXPORT_NVD_BT>7
	sll	v5,v5,IO_EXPORT_NVD_BT-7   ;set data-in at falling clock edge
	or	export,export,v5
	store	0,WORD,export,p_export	;SK=-  CE=+  DI=b(n)

	NDELAY	v5,500,2		;"data setup time"

	const0	v5,IO_EXPORT_NVC
	or	export,export,v5	;cause rising edge of the clock
	store	0,WORD,export,p_export	;SK=+  CE=+  DI=b(n)
	NDELAY	v5,500,2+5		;"data hold" and half clock freq.

	jmpfdec	v6,novram_cmd1
	 sll	v4,v4,1

	jmpi	t4			;quit with the clock, SK, high
	 nop

	.eject
;arm board-to-host PHY interrupts
cmd_phy_arm:
	set_st	PINT_ARM,t0
	jmp	cmd_done
	 nop


;restart H2B DMA
cmd_wakeup:
	rs_st	H2B_SLEEP
	add	h2b_active,h2b_sleep,0	;keep H2B DMA active
	jmp	cmd_done
	 consth	h2b_delay,TIME_NOW	;see what the host has to say

	.eject
;keep the 16K PC counter out of the code, since it forces a re-fetch
;many wait states.
	CK	. > 0x3e00 && . < 0x4000
	.rep	(0x4000 - .)/4
	nop
	.endr

;FDDI input, board VME write to host bus packet interrupt handler
;   This assumes all host buffers are quad-byte aligned, and that
;   the host always takes all of the frame.

;   it2==TRUE if bus_tc=0
bus_w:
	PERF_TP	it0,PERF_BUS_WRITE_INT

	jmpt	it2,bus_90		;just wait if last job finished

;do the next VME write "bus packet"
bus_w20:  cpeq	it1,next_wfer,prev_wfer
	jmpt	it1,bus_w22		;xfer cycle only if needed
	 add	it0,busrst_read,0	;just reset interrupt if not needed

	load	0,WORD,it1,next_wfer

	constx	it0,BUSRST_WRITE	;clock 1st word & clear interrupt
bus_w22:
	mov	cur_bus_bits, next_bus_bits

	sll	it2,bus_tc,2

	store	0,WORD,it0,p_busrst

	add	it2,it2,next_wfer	;get ((addr+cnt) & (BUS_PAGE-1))
	const0	it3,BUS_PAGE-1		;to get future xfer state, trimmed
	and	it2,it2,it3		;	to the current page

	consth	it0,IO_STRT01
	CK	(IO_STRT01 & 0xffff)==0
	CK	(BUSRST_READ & 0xffff)==0 && (BUSRST_WRITE & 0xffff)==0
	load	0,WORD,it1,it0

	andn	prev_wfer,next_wfer,it3
	or	prev_wfer,prev_wfer,it2	;future xfer state finished

	CK	(IO_LDVADDR & 0xffff)==0
	consth	it0,IO_LDVADDR
	store	0,WORD,bus_vaddr,it0

	CK	(IO_LDVME & 0xffff)==0
	consth	it0,IO_LDVME
	store	0,WORD,bus_tc,it0
	const	bus_tc,0

	CK	(IO_REQBUS & 0xffff)==0
	consth	it0,IO_REQBUS
	load	0,WORD,it1,it0		;VME should be running now


;get ready for the next VME write bus packet
;   At the end of a host buffer, see if we are at the end of the frame
	cple	it0,bus_h_len,0		;end of host buffer?
	jmpf	it0,bus_w25		;no, do another bus packet

	 sll	it0,cur_bus_bits,31-DMA_FE_BT
	jmpt	it0,bus_w70		;try next job at end of the frame
	 sll	it0,cur_bus_bits,31-DMA_C_BT

;   do next buffer in the frame
	load	0,WORD,bus_h_len,dlist_tail
	CK	DLIST_L==0
	add	it1,dlist_tail,DLIST_A
	load	0,WORD,bus_h_addr,it1
	INC_DLIST dlist_tail,it0,it1
	mov	next_bus_bits,bus_h_len	;save next operation bits
	consth	bus_h_len,0		;clean the length

;We come here with more work to do in the current host buffer.  This
;   means we are not at the end of an FDDI frame or a B2H transfer.
bus_w25:cple	it0,bus_bd_len,0	;current board buffer empty?
	jmpf	it0,bus_w40		;no, use current board or H2B buffer
	 nop

	load	0,WORD,ibf_full,ibf_full	;free previous board buffer
	CK	IBF_P_FOR==0
	add	ibf_free_cnt,ibf_free_cnt,1

;get ready to send the next board input buffer containing a frame to the host.
bus_w30:
	IBF_ADR	bus_bd_addr,ibf_full,it0,

	const0	bus_bd_len,IBF_LEN	;board FDDI buffers are full or end at
					;	the end of host buffers
	sub	bus_bd_len,bus_bd_len,bus_w_off
	add	bus_bd_addr,bus_bd_addr,bus_w_off

	const	bus_w_off,0		;no descriptor on continuation buffers


;continue the current host buffer.
;   limit the transfer count by the end of the page and both buffer limits
bus_w40:
 .ifdef CK_DMA_LEN
	constx	it1,BIG_SIZE
	cpgt	it1,bus_h_len,it1
	jmpt	it1,bus_werr
	 nop
 .endif
	srl	it1,bus_bd_addr,BUS_PAGE_BT
	add	it1,it1,1
	sll	it1,it1,BUS_PAGE_BT
	sub	it1,it1,bus_bd_addr	;it1=bytes to next page

	cpltu	it0,it1,bus_h_len	;bus_tc = min(it1, bus_h_len)
	sra	it0,it0,31
	and	it1,it1,it0
	andn	it0,bus_h_len,it0
	or	bus_tc,it0,it1

	cpltu	it0,bus_tc,bus_bd_len	;bus_tc = min(bus_tc, bus_bd_len)
	sra	it0,it0,31
	and	bus_tc,bus_tc,it0
	andn	it0,bus_bd_len,it0
	or	bus_tc,bus_tc,it0


;   finish computing transfer count etc
	mov	bus_vaddr,bus_h_addr
	or	next_wfer,xfer_cycle,bus_bd_addr

	sub	bus_h_len,bus_h_len, bus_tc
	add	bus_h_addr,bus_h_addr, bus_tc
	sub	bus_bd_len,bus_bd_len, bus_tc
	add	bus_bd_addr,bus_bd_addr, bus_tc

	add	bus_tc,bus_tc,3
	srl	bus_tc,bus_tc,2		;round byte count up to words

	t_st	it0,BUS_ACT
	jmpf	it0,bus_w20		;start machinery if not running
	 or	cur_st1,cur_st1,ST1_BUS_ACT | ST1_LED_ACT1 | ST1_LED_ACT2

	PERF_TP it0,PERF_BUS_EXIT
	iret				;wait for something to happen


;finish Board-to-Host control transfer
;   it0=TRUE if no more work to do
bus_w60:
	sll	it2,next_bus_bits,31-DMA_SLOINT_BT
	jmpf	it2,bus_w65
	 sll	it1,next_bus_bits,31-DMA_FASINT_BT

;   Arm the "slow" interrupt.  Generate the interrupt if the host is asleep.
;   It is not really necessary to generate the interrupt here, but it
;   improves latency slightly.

	rs_st	H2B_SLEEP,,		;host will think we are awake
	set_st	SLOINT_RDY
	mfsr	it2,TMC
	cplt	it1,it2,host_qt		;is the host quiet?
	or	it1,it1,h2b_active	;if so, H2B to cause interrupt
	jmpf	it1,bus_w78
	 add	h2b_active,h2b_sleep,0	;keep H2B DMA active
	jmp	bus_w78			;and restart it if it was asleep
	 consth	h2b_delay,TIME_NOW



;   cause "fast" host interrupt if asked
bus_w65:
	sll	it1,next_bus_bits,31-DMA_FASINT_BT
	jmpf	it1,bus_w78

	 constx	it2,IO_SVINT
	load	0,WORD,it1,it2		;cause host interrupt
	jmp	bus_w78
	 rs_st	OUTINT_REQ


;Advance to the next frame at the end of a VME write transaction
; This code assumes that VME write jobs are always complete.  There is
; no need to resume a partially completed host data movement.

;   it0=TRUE if end of B2H transaction
bus_w70:
	jmpt	it0,bus_w60
	 cpeq	it0,dlist_tail,dlist_head	;it0=TRUE if no more DMA to do

	load	0,WORD,ibf_full,ibf_full	;free final board buffer
	CK	IBF_P_FOR==0
	add	ibf_free_cnt,ibf_free_cnt,1

;   it0=TRUE if no more DMA to do
bus_w78:
	jmpt	it0,bus_99		;final job for now, so wait for quiet

;There is more work.  If it is another VME write, continue working.
;   If not, stop to switch the bus packet machinery.
	 add	it0,dlist_tail,DLIST_A
	load	0,WORD,bus_h_len,dlist_tail
	CK	DLIST_L==0
	jmpf	bus_h_len,bus_99	;next is a VME read, so stop working
	 CK	DMA_WR==TRUE
	 mov	next_bus_bits,bus_h_len	;save next operation bits

	load	0,WORD,bus_h_addr,it0
	consth	bus_h_len,0		;clean the length
	INC_DLIST dlist_tail,it0,it1

;start the next VME write to the host
;   bus_h_addr, bus_h_len and next_bus_bits have been fetched from the list
bus_w80:
	PERF_TP	it0,PERF_BUS_WRITE

	sll	it0,next_bus_bits,31-DMA_C_BT
	jmpf	it0,bus_w30
	 const0	bus_w_off,RX_DESC_LEN

;   Decode special B2H address encoding.  Instead of the usual length
;   and host address, we have the length and board address.  The host
;   address is computed from the board address, the start of the board
;   buffer, and the start of the host buffer.
	mov	bus_bd_addr,bus_h_addr
	mov	bus_bd_len,bus_h_len

	const	prev_wfer,1		;must re-load the shift register

	jmp	bus_w40
	 add	bus_h_addr,bus_h_addr,b2h_buf

	.eject
;the last interrupt for a frame has happened, and there was no
;   additional tranaction ready.
bus_90:	store	0,WORD,busrst_read,p_busrst  ;reset interrupt request
	rs_st	BUS_ACT

	cpneq	it0,dlist_head,dlist_tail
	sra	it0,it0,31
	and	b2h_ok,b2h_ok,it0
	constx	it1,MAX_RX_DMA_LEN	;reset DMA queue limit
	andn	it1,it1,it0
	or	b2h_ok,b2h_ok,it1

 .if	REV29K<=3
	CK	.>=bus_90+3*4
 .endif
	iret


;simply wait until the next interrupt
bus_99:
	PERF_TP it0,PERF_BUS_EXIT
 .if	REV29K<=3
	nop
	nop
	nop
 .endif
	iret

 .ifdef CK_DMA_LEN
;stop on impossible DMA length
bus_werr:
	mtsrim  CPS,PS_INTSOFF
	jmp	.
	 halt
 .endif


;At the end of a host buffer, see if we are at the end of the frame
;   We have started the last VME DMA read for the frame, and found
;   it is the end.  We want to prepare the next DMA transaction now
;   so we start it when the current one finishes, keeping things humming.
bus_r60:sll	it0,cur_bus_bits,31-DMA_FE_BT
	jmpf	it0,bus_r70		;branch if another buffer for frame
	 or	it0,xfer_cycle,bus_bd_addr

;at the end of a VME read for an FDDI output frame, remember to tell
;   the host.  Then advance to the next frame.  The DMA machinery should
;   be running now, finishing the last DMA of the frame.  We expect to get
;   an interrupt when it finishes, when we will have to do the final transfer
;   cycle for the FDDI frame.

	sll	it1,cur_bus_bits,31-DMA_C_BT
	cpge	it1,it1,0		;it1=TRUE if DMA_C false
	srl	it1,it1,31-DMA_FIN_BT
	or	cur_bus_bits,cur_bus_bits,it1	;remember to start the fiber

	sub	it0,it0,1		;remember to do final cycle
	srl	it0,it0,BUS_PAGE_BT

	cpeq	it1,dlist_tail,dlist_head
	jmpt	it1,bus_99		;final job for now, so wait for quiet
	 sll	cur_rfer,it0,BUS_PAGE_BT	;set column # for next read
	 CK	BUS_PAGE==OBF_ALIGN	;when we finish previous read


;There is another job in the VME DMA list.  See if it is a read or write.
	load	0,WORD,bus_h_len,dlist_tail
	CK	DLIST_L==0
	mov	next_bus_bits,bus_h_len	;save next operation bits
	jmpt	bus_h_len,bus_99	;next is a VME write, so stop work
	 CK	DMA_WR==TRUE
	 consth	bus_h_len,0		;clean the length

	add	it0,dlist_tail,DLIST_A
	PERF_TP it1,PERF_BUS_READ
	load	0,WORD,bus_h_addr,it0
	INC_DLIST dlist_tail,it0,it1

;   use the H2B buffer for DMA_C
	sll	it0,next_bus_bits,31-DMA_C_BT
	const	bus_bd_addr,H2B_BASE
	jmpt	it0, bus_r30
	 consth	bus_bd_addr,H2B_BASE

	mov	bus_bd_addr,obf_free	;take the next output board buffer

	cpltu	it0,obf_free,obf_free_lim
	jmpt	it0,bus_r30		;wrap to first buffer when we must
	 add	obf_free,obf_free,obf_len
	jmp	bus_r30
	 sub	obf_free,obf_free,obf_restart


;At the end of a host buffer, prepare to continue reading more of the frame
;   from the next host buffer.
bus_r70:load	0,WORD,bus_h_len,dlist_tail
	CK	DLIST_L==0
	add	it1,dlist_tail,DLIST_A
	INC_DLIST dlist_tail,it0,it2	;(too soon but interrupts are off)
	mov	next_bus_bits,bus_h_len	;save next operation bits
	consth	bus_h_len,0		;clean the length
	jmp	bus_r30			;the frame has another host buffer
	 load	0,WORD,bus_h_addr,it1	;so get host address



;start reading a frame from the host when the DMA machinery has been quiet.
;   here next_bus_bits, bus_h_len, and bus_h_addr are correct
bus_r80:
	PERF_TP	it0,PERF_BUS_READ

;   use the H2B buffer for DMA_C
	sll	it0,next_bus_bits,31-DMA_C_BT
	jmpt	it0, bus_r85

	mov	bus_bd_addr,obf_free	;take the next output board buffer

	or	it0,xfer_cycle,bus_bd_addr	;set the DRAM pointer
	store	0,WORD,it0,it0

	constx	it0,IO_EXPORT_BWR
	andn	export,export,it0	;set machinery for VME read
	store	0,WORD,export,p_export

	cpltu	it0,obf_free,obf_free_lim
	jmpt	it0,bus_r30		;wrap to first buffer when we must
	 add	obf_free,obf_free,obf_len
	jmp	bus_r30
	 sub	obf_free,obf_free,obf_restart

;   start H2B control read
bus_r85:constx	bus_bd_addr,H2B_BASE
	or	it0,xfer_cycle,bus_bd_addr	;set the DRAM pointer
	store	0,WORD,it0,it0

	constx	it0,IO_EXPORT_BWR
	andn	export,export,it0	;set machinery for VME read
	jmp	bus_r30
	 store	0,WORD,export,p_export

	.eject
;overflow code for FORMAC CMT trap

for_hand9:
 .if	REV29K<=3
	nop
	nop
	nop
	CK	.>=(for_hand9+3*4)
 .endif
	iret


;turn off interrupts to deal with a 9513 noise event timer
;  here it0=front end status, it1=TRUE if 2nd noise timer expired
for_ne1:
	constx	it3,ST2_NE1|ST2_NE
	or	cur_st2,cur_st2,it3
	constx	cps_shadow,PS_INTSOFF
	jmpf	it1,for_ne9
	 mtsr	OPS,cps_shadow		;turn off interrupts so it gets done

;  here it0=front end status
for_ne2:
	constx	it3,ST2_NE2|ST2_NE
	or	cur_st2,cur_st2,it3
	constx	cps_shadow,PS_INTSOFF
	jmp	for_ne9
	 mtsr	OPS,cps_shadow


;CMT interrupts are not common, so jump for them
 .macro	CMT_HAND, n,cycles
	  load	  0,BYTE,it2,p_cmt@n@
	  and	  it2,it2,CMT_MASK
	  FE_DELAY 3,,
	  or	  it2,it2,CMT_EMPTY
	  or	  cur_st2,cur_st2,ST2_PINT_REQ
	  sll	  cmt@n@,cmt@n@,4
	  store	  0,BYTE,it2,p_cmt@n@	;reset interrupt request
	  FE_DELAY cycles+1,,
	  or	  cmt@n@,cmt@n@,it2
 .endm


cmt_hand1:
	CMT_HAND 1,3

	sll	it1,it0,31-STAT1_CMT2_BT
	jmpf	it1,for_hand		;check FORMAC if 2nd CMT quiet
	 sll	it1,it0,31-STAT1_FORMAC_BT

cmt_hand2:
	CMT_HAND 2,4

	jmp	for_hand		;check FORMAC
	 sll	it1,it0,31-STAT1_FORMAC_BT


asml_tbl:
  .irep	n, 0,1,2,3,4,5,6,7,8,9,10
    .if n<NEXT_ASML
	.word	ASML@n
    .endif
  .endr
	.equ	NUM_ASML, NEXT_ASML

apad_tbl:
  .irep	n, 0,1,2,3,4,5,6,7,8,9,10
    .if n<NEXT_APAD
	.word	APAD@n
    .endif
  .endr
	.equ	NUM_APAD, NEXT_APAD

spad_tbl:
  .irep	n, 0,1,2,3,4,5,6,7,8,9,10
    .if n<NEXT_SPAD
	.word	SPAD@n
    .endif
  .endr
	.equ	NUM_SPAD, NEXT_SPAD


	nop
;assume the next word is a checksum, that it will be added to the C array
;generated by coof2firm.
cksum:
	.end
