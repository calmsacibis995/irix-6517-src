;Common definitions for the FDDIXPress, 6U FDDI board
;
; Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
;	"$Revision: 1.33 $"


;pad output with 3 bytes.
;   The host driver code knows about output padding as
	.equ	OBF_PAD_LEN,TX_DESC>>24

;pad input the same
	.equ	IBF_PAD_LEN, OBF_PAD_LEN

;maximum size of an incoming FDDI frame, including 3 bytes of padding
;   and 32 more just in case.
	.equ	MAX_FRAME_LEN, 4500+OBF_PAD_LEN+32

;the board is supposed to stop by 6K
	.equ	MAX_MAX_FRAME, MAX_FRAME_LEN+2048

;at least a complete short frame
	.equ	MIN_FRAME_LEN, 1+2+2+4


;map of buffer RAM
;   pad after VME interrupt vector to avoid corruption from babble frames
	.equ	CORRUPT_PAD, VME_VEC_END+4096-VME_VEC_SIZE

;   claim and beacon frames
	.equ	CLAIM_BASE, (CORRUPT_PAD + FE_AMSK) & ~FE_AMSK
	.equ	CLAIM_LEN,  1+6+6+4
	.equ	CLAIM_SIZE, (OBF_PAD_LEN+CLAIM_LEN + FE_AMSK+1) & ~FE_AMSK

	.equ	BEACON_BASE, (CLAIM_BASE+CLAIM_SIZE + FE_AMSK) & ~FE_AMSK
	.equ	BEACON_LEN,  1+6+6+4+6
	.equ	BEACON_SIZE, (OBF_PAD_LEN+BEACON_LEN+FE_AMSK+1) & ~FE_AMSK

;   frame for measuring ring latency
	.equ	LAT_BASE,   (BEACON_BASE+BEACON_SIZE + FE_AMSK) & ~FE_AMSK
	.equ	LAT_LEN,    1+6+6
	.equ	LAT_SIZE,   (OBF_PAD_LEN+LAT_LEN + FE_AMSK+1) & ~FE_AMSK

;   B2H control buffer
	.equ	B2H_ALIGN, 4
	.equ	B2H_AMSK, (B2H_ALIGN-1)
	.equ	B2H_BASE, (LAT_BASE+LAT_SIZE + B2H_AMSK) & ~B2H_AMSK
	.equ	B2H_SIZE, (B2H_LEN*4 + B2H_AMSK) & ~B2H_AMSK

;   H2B control buffer
;	aligned to page boundaries since VME DMA writes to it.
	.equ	H2B_ALIGN, BUS_PAGE
	.equ	H2B_AMSK, (H2B_ALIGN-1)
	.equ	H2B_BASE, (B2H_BASE+B2H_SIZE + H2B_AMSK) & ~H2B_AMSK
	.equ	H2B_NUM,  8
	CK	MAX_DMA_ELEM <= H2B_NUM
	.equ	H2B_BUF_LEN, H2B_NUM*8
	.equ	H2B_SIZE, (H2B_BUF_LEN + H2B_AMSK) & ~H2B_AMSK

;   output buffers
;	aligned to page boundaries since VME DMA writes to them.
	.equ	OBF_ALIGN, BUS_PAGE
	.equ	OBF_AMSK, (OBF_ALIGN-1)
	.equ	OBF_LEN, (MAX_FRAME_LEN+TX_DESC_LEN + OBF_AMSK) & ~OBF_AMSK

	.equ	OBF_BASE, (H2B_BASE+H2B_SIZE + OBF_AMSK) & ~OBF_AMSK
	.equ	OBF_NUM, IFQ_MAXLEN
	.equ	OBF_SIZE, (OBF_LEN*OBF_NUM + OBF_AMSK) & ~OBF_AMSK

;   input buffers
	.equ	IBF_ALIGN, FE_ALIGN	;align input buffers to this
	.equ	IBF_AMSK, (IBF_ALIGN-1)
;XXX use 8K since front-end scatter is broken
;XXX	.equ	IBF_LEN, (XXXX+RX_DESC_LEN + IBF_AMSK) & ~IBF_AMSK
	.equ	IBF_LEN, (8192+RX_DESC_LEN + IBF_AMSK) & ~IBF_AMSK

	.equ	IBF_BASE, (OBF_BASE+OBF_SIZE + IBF_AMSK) & ~IBF_AMSK
	.equ	IBF_NUM, (SIO_BASE - IBF_BASE) /IBF_LEN
	.equ	IBF_SIZE, IBF_LEN*IBF_NUM

	CK	(IBF_BASE+IBF_SIZE) <= SIO_BASE



 .if	MHZ==16
	.equ	MHZ_BT,4		;binary log of MHZ
 .else
	.print	"unanticipated clock rate"
	.err
 .endif
	.equ	USEC_SEC, 1000000

;interval for 29K timer
	DEFBT	TIME_TICK, 4		;wake up 16 times each second
	.equ	TIME_INTVL, (USEC_SEC/TIME_TICK)*MHZ
	CK	TIME_INTVL>0x10000
	CK	TIME_INTVL<(1<<24)
	.set	TIME_INTVL_BT, 16	;binary log of timer value
 .if	TIME_INTVL>0x20000
	.set	TIME_INTVL_BT, 17
 .endif
 .if	TIME_INTVL>0x40000
	.set	TIME_INTVL_BT, 18
 .endif
 .if	TIME_INTVL>0x80000
	.set	TIME_INTVL_BT, 19
 .endif
 .if	TIME_INTVL>0x100000
	.set	TIME_INTVL_BT, 20
 .endif
 .if	TIME_INTVL>0x200000
	.set	TIME_INTVL_BT, 21
 .endif
 .if	TIME_INTVL>0x400000
	.set	TIME_INTVL_BT, 22
 .endif
 .if	TIME_INTVL>0x800000
	.set	TIME_INTVL_BT, 23
 .endif
 .if	TIME_INTVL>0x1000000
	.err
 .endif


;parameters to compute ring latency
;   Time required to send and receive a frame.  This is the absolute
;   lower limit on the ring latency that could be measured.  This is
;   the time between starting to send the frame and taking the input
;   interrupt.  This period overlaps the passage of the packet around
;   the ring.
	.equ	IO_LATENCY,	3*MHZ

;   Jitter caused by 29K interrupts and other overhead.  It is the
;   length of mlop.
	.equ	JITTER_LATENCY,	5*MHZ

;   Time required after receiving a frame to process it.  Some of this
;   is the code at m_rx and some is the input DMA interrupt handler
	.equ	RCV_LATENCY,	7*MHZ

;   Ignore values smaller than this, because they are probably wrong.
	.equ	MIN_RNG_LATENCY, IO_LATENCY+JITTER_LATENCY

	.equ	MAX_LAT_SEC, 4		;maximum frames/second for latency


;approximate maximum DMA queue length
	.equ	MAX_TX_DMA_LEN, 4096*4
	.equ	MAX_RX_DMA_LEN, 4096*3

;static register allocations
;   Local registers are used as if they were globals.  They are used
;   in interrupt handlers.
;   The return registers are used as temporaries for interrupt handlers.

	.reg	it0, u0			;trap handler temps
	.reg	it1, u1
	.reg	it2, u2
	.reg	it3, u3

 .macro	greg, nam
	.reg	nam, %%NEXT_GREG
   .if NEXT_GREG==95
	.set	NEXT_GREG, 104		;use the extra V registers
    .else
	.set	NEXT_GREG, NEXT_GREG+1
   .endif
 .endm
	.set	NEXT_GREG, 64		;start at gr64

 .macro	lreg, nam
	.reg	nam, %%(NEXT_LREG+128)
	.set	NEXT_LREG, NEXT_LREG+1
 .endm
	.set	NEXT_LREG, 2		;start at lr2


	greg	p_mailbox,
	greg	p_cmd1,			;IO_CMD, as a half-word
	greg	cmd1,			;shadow of board command register
	greg	p_export,
	greg	export,			;VME machine "export" register

	greg	p_busrst,		;bus packet reset strobe
	greg	p_ldvme,
	greg	busrst_read,
	greg	xfer_cycle,		;"transfer cycle" address template
	greg	cur_rfer,		;current and next VME read xfer
	greg	next_rfer,		;	addresses
	greg	next_wfer,		;next and previous VME write xfer
	greg	prev_wfer,		;	addresses
	greg	bus_tc,			;next transfer count
	greg	bus_vaddr,
	greg	bus_bd_len,		;remainder of board buffer
	greg	bus_bd_addr,
	greg	bus_h_len,		;remainder of host buffer
	greg	bus_h_addr,
	greg	cur_bus_bits,		;control bits for the transfer
	greg	next_bus_bits,
	greg	bus_w_off,		;offset for non-continuations
	greg	bus_toggle,		;FALSE=do VME write next

	greg	b2h_ok,			;board-to-host data DMA govenor

	greg	p_formac_stmc,		;IO_FORMAC_STMC

	greg	p_cmt1,			;IO_CMT1
	.equ	CMT_EMPTY,CMT_MASK+1	;distinquish empty from 0
	greg	cmt1,			;recent line states
	greg	p_cmt2,			;IO_CMT2
	greg	cmt2,

	greg	ma_hi,			;my MAC address
	greg	ma_lo,

	greg	cps_shadow,		;shadow of CPS

	greg	time_hz,		;time in ticks in the current second

	greg	ct_age,			;when need to get 9513 values

	lreg	cur_st1,		;current firmware state
	.set	NEXT_BT,0
	NXTBT	ST1_FORINT,		;recent FORMAC interrupt
	NXTBT	ST1_RNGOP,		;ring operational as told to host
	NXTBT	ST1_TXA,		;async TX active
	NXTBT	ST1_OUTINT_REQ,		;tell host of finished output

	NXTBT	ST1_BUS_ACT,		;VME DMA active
	NXTBT	ST1_LED_ACT1,		;fiber is active
	NXTBT	ST1_LED_ACT2,
	NXTBT	ST1_LED_BAD,		;ring is or was recovering

	NXTBT	ST1_PHY_REP,		;repeat line states
	NXTBT	ST1_LAT,		;measure latency
	NXTBT	ST1_WAIT_LAT,		;waiting for ring-latency frame
	NXTBT	ST1_GET_LATENCY,	;need to recompute latency
	CK	NEXT_BT <=16

	.equ	ST1_MB, TRUE		;heard host request


	lreg	cur_st2,
	.set	NEXT_BT,0
	NXTBT	ST2_SLOINT_RDY,		;time to interrupt host
	NXTBT	ST2_SLOINT_REQ,		;interrupt host after next H2B
	NXTBT	ST2_SLOINT_DONE,	;host has been interrupted
	NXTBT	ST2_FASINT_REQ,		;interrupt host after next B2H

	NXTBT	ST2_PINT_REQ,		;need PHY interrupt
	NXTBT	ST2_H2B_SLEEP,		;Host to Board DMA is off
	NXTBT	ST2_H2B_ACT,		;Host to Board DMA is in progress
	NXTBT	ST2_H2B_FIN,		;need to process previous info

	NXTBT	ST2_PINT_ARM,		;PHY interrupts armed
	NXTBT	ST2_ALL_MULTI,		;receive all multicast frames

	DEFBT	ST2_NE1, 30
	DEFBT	ST2_NE2, 29
	CK	NEXT_BT <=16

	.equ	ST2_NE, TRUE		;noise events

;test state bit
 .macro	t_st,	reg,nm
   .ifdef ST1_@nm@_BT
	  sll	  reg,cur_st1,31-ST1_@nm@_BT
   .else
	  sll	  reg,cur_st2,31-ST2_@nm@_BT
   .endif
 .endm

;set state bte
 .macro	set_st,	nm,tmp
   .ifdef ST1_@nm@
      .if ST1_@nm@ >= 256 && ST1_@nm@ <= 0x8000
	  const	  tmp,ST1_@nm@
	  or	  cur_st1,cur_st1,tmp
      .else
	  or	  cur_st1,cur_st1,ST1_@nm@
      .endif
   .else
      .if ST2_@nm@ >= 256 && ST2_@nm@ <= 0x8000
	  const	  tmp,ST2_@nm@
	  or	  cur_st2,cur_st2,tmp
      .else
	  or	  cur_st2,cur_st2,ST2_@nm@
      .endif
   .endif
 .endm

;reset state bte
 .macro	rs_st,	nm,tmp
   .ifdef ST1_@nm@
      .if ST1_@nm@ >= 256 && ST1_@nm@ <= 0x8000
	  const	  tmp,ST1_@nm@
	  andn	  cur_st1,cur_st1,tmp
      .else
	  andn	  cur_st1,cur_st1,ST1_@nm@
      .endif
   .else
      .if ST2_@nm@ >= 256 && ST2_@nm@ <= 0x8000
	  const	  tmp,ST2_@nm@
	  andn	  cur_st2,cur_st2,tmp
      .else
	  andn	  cur_st2,cur_st2,ST2_@nm@
      .endif
   .endif
 .endm


	lreg	cksum_ok,		;checksum govenor
	.equ	CKSUM_MIN,1

	lreg	formac_mode,		;FORMAC mode for turning recv off
	lreg	formac_stat,		;bits causing recent FORMAC interrupts
	lreg	formac_imsk,		;shadow of IMSK register

	lreg	ibf_ptr_mask,
	lreg	p_ibf_base,
	lreg	ibf_free,		;first available buffer
	lreg	ibf_free_cnt,		;# of free input buffers
	lreg	ibf_raw,		;next input buffer to "skim"
	lreg	ibf_raw_cnt,		;#-1 completed frames in buffers
	lreg	ibf_full,		;oldest full input buffer
	lreg	ibf_p_dsc,		;descriptor in start of current frame
 .ifdef RX_CONT
	lreg	ibf_dsc_cnt,		;#-1 buffers in current frame
 .endif
	lreg	ibf_next,		;next descriptor hit by front end
	lreg	rx3_dmach,		;dma channel #3
	lreg	rx3_dmacs,		;dma channel #3  pointer
	lreg	p_rx3_ack,
	lreg	rx4_dmach,		;dma channel #4
	lreg	rx4_dmacs,		;dma channel #4  pointer
	lreg	p_rx4_ack,
	lreg	rx_toggle,		;toggle between interrupts
	lreg	misfrm_lim,		;missed frames -1 until we reset

	greg	h2b_dma_delay,		;delay control DMA by this
	greg	h2b_sleep,		;keep H2B DMA going this long

	greg	h2b_pos,		;position in H2B buffer in host
	lreg	host_qt,		;timer until host is quiet
	lreg	h2b_active,		;keep host to board DMA active
	lreg	h2b_delay,		;delay H2B DMA until then

	greg	b2h_delay,		;delay B2H DMA until then

	lreg	b2h_rpos,		;pointers into board B2H buffer
	lreg	b2h_wpos,
	lreg	b2h_lim,
	lreg	b2h_buf,		;host buffer in host VME space
	lreg	b2h_sn,			;serial number

	lreg	b2h_phy,		;limit outstanding CMT interrupts

	lreg	dlist_head,		;newest host DMA request
	lreg	dlist_tail,		;next DMA request to service
	lreg	dlist_lim,
	lreg	dlist_start,

	lreg	out_done,		;finished host-to-board data DMA

	lreg	sml_bufs,		;# of free host buffers
	lreg	med_bufs,
	lreg	big_bufs,
	lreg	nxt_big,		;free medium host buffers
	lreg	lst_big,
	lreg	nxt_med,		;free big host buffers
	lreg	lst_med,
	lreg	nxt_sml,		;free small host buffers
	lreg	lst_sml

	lreg	txs_dmacs,		;sync. TX DMA channel
	lreg	txs_claim,
	lreg	txs_addr,
	lreg	p_txs_ack,
	lreg	txs_cnt,		;continue beacon or claims while <0
	lreg	txs_time,		;when we last sent a beacon or claim

	lreg	obf_cur,		;current output buffer
	lreg	obf_next,
	lreg	obf_cur_lim,
	lreg	obf_free,		;next free output buffer
	lreg	obf_free_lim,
	lreg	obf_len,		;length of one output buffer
	lreg	obf_cnt,		;outstanding requests + 0x7fffffff
	lreg	obf_restart,

	lreg	txa_dmacs,
	lreg	p_txa_ack,

	lreg	bec_da,			;32 bits of recent beacon frame

;counters
 .macro	CNT,	snam
	lreg	cnt_@snam@,
 .endm
	CNTRS	cnt_
	.purgem	CNT


;block of registers for checksumming
	.set	N_SUMREG,0
	.irep	nam, sumreg0,sumreg1,sumreg2,sumreg3,sumreg4,sumreg5,sumreg6,sumreg7,sumreg8,sumreg9,sumrega,sumregb,sumregc,sumregd,sumrege,sumregf
	lreg	nam,
	.set	N_SUMREG,N_SUMREG+1
	.endr
	CK	N_SUMREG==16
	.set	N_SUMREG_BT,4

;ensure that we did not allocate too many registers
	CK	NEXT_GREG<=112
	CK	NEXT_LREG<=128
	.purgem	greg,lreg


;FORMAC interrupt bits
	.set	FORMAC_Ix, FORMAC_SMULTDA	;hate as29 short lines
	.set	FORMAC_Ix, FORMAC_Ix | FORMAC_SMISFRM
	.set	FORMAC_Ix, FORMAC_Ix | FORMAC_SXMTABT
	.set	FORMAC_Ix, FORMAC_Ix | FORMAC_STKERR
	.set	FORMAC_Ix, FORMAC_Ix | FORMAC_SCLM
	.set	FORMAC_Ix, FORMAC_Ix | FORMAC_SBEC
	.set	FORMAC_Ix, FORMAC_Ix | FORMAC_STVXEXP
	.set	FORMAC_Ix, FORMAC_Ix | FORMAC_STRTEXP
	.set	FORMAC_Ix, FORMAC_Ix | FORMAC_STKISS
	.equ	FORMAC_INTS, FORMAC_Ix

	.set	FORMAC_Ix, FORMAC_SMYCLM
	.set	FORMAC_Ix, FORMAC_Ix | FORMAC_SLOCLM
	.set	FORMAC_Ix, FORMAC_Ix | FORMAC_SHICLM
	.set	FORMAC_Ix, FORMAC_Ix | FORMAC_SMYBEC
	.set	FORMAC_Ix, FORMAC_Ix | FORMAC_SOTRBEC
	.equ	FORMAC_MORE_INTS, FORMAC_Ix


;9513 values
	.equ	IO_CNT_SYM, 1		;LEM symbol counter
	.equ	IO_CNT_LEM, 2		;LEM events
	.equ	IO_CNT_NE,  3		;times noise events
	.equ	IO_CNT1_FRAME,4		;FORMAC FRINC
	.equ	IO_CNT2_ERR,4		;FORMAC ERRINC
	.equ	IO_CNT1_LOST,5		;lost count
	.equ	IO_CNT2_TOK,5		;token count

	DEFBT	UP9513, 6-TIME_TICK_BT	;update 64/sec to prevent overflow


;Send a command to a 9513
 .macro	CNTCMD, val, cadr, cycles
	  const	  t0, val
	  store	  0,HWORD, t0, cadr
	  NDELAY  t0, WR_REC_9513, (cycles)
 .endm
	.equ	CNTCMD_OVHD, 1+WAIT_9513

;Send a word of data to a 9513
 .macro	CNTWR,	val, dadr, cycles
	  const	  t0, val
	  store	  0,HWORD,t0,dadr
	  NDELAY  t0, WR_REC_9513, (cycles)
 .endm
	.equ	CNTWR_OVHD, 1+WAIT_9513

;Send a command to both 9513s
;   The counters are symmetrically allocated, and this strategy saves time
;   and space.
 .macro	CNTCMD2, val, cadr1, cadr2, cycles
	  const	  t0, val
	  store	  0,HWORD, t0, cadr1
	  FE_DELAY 0,
	  store	  0,HWORD, t0, cadr2
	  NDELAY  t0, WR_REC_9513, (cycles)+WAIT_9513
 .endm
	.equ	CNTCMD2_OVHD, 1+FE_DELAY_CYCLE+WAIT_9513

;Send a word of data to both 9513s
 .macro	CNTWR2, val, dadr1, dadr2, cycles
	  const	  t0, val
	  store	  0,HWORD,t0,dadr1
	  FE_DELAY 0,
	  store	  0,HWORD,t0,dadr2
	  NDELAY  t0, WR_REC_9513, (cycles)+WAIT_9513
 .endm
	.equ	CNTWR2_OVHD, 1+FE_DELAY_CYCLE+WAIT_9513

;Fetch a word of data from both 9513s
 .macro	CNTRD2, cnt1,cnt2, cadr1,cadr2, cycles
	  load	  0,HWORD,cnt1,cadr1
	  FE_DELAY 0,
	  load	  0,HWORD,cnt2,cadr2
	  NDELAY  t0, RD_REC_9513, (cycles)+WAIT_9513
 .endm
	.equ	CNTRD2_OVHD, WAIT_9513+FE_DELAY_CYCLE

;Point to the mode register of a pair of counters
 .macro	CNTMODE2, cntr, cadr1, cadr2, cycles
	  CNTCMD2	  cntr+IOCNT_MODE, cadr1, cadr2, cycles
 .endm
	.equ	CNTMODE2_OVHD, CNTCMD2_OVHD

;no gating, count on rising edge, source (n), no special gating,
;	reload from Load, count repetitively, binary count, count up,
;	output off.
 .macro	CNTSIMP2, cntr, cadr1,dadr1, cadr2,dadr2, cycles
	  CNTMODE2 cntr, cadr1, cadr2, CNTWR2_OVHD
	  CNTWR2  (0x0000+((cntr)<<8)+0x0028+IOCNT_OFF),dadr1,dadr2,CNTWR2_OVHD
	  CNTWR2  0, dadr1,dadr2, cycles
 .endm
	.equ	CNTSIMP2_OVHD, CNTMODE2_OVHD



;cnt=value from 9513, tgt=counter register, t0=scratch,
;	Arrange to have 16 more ignificant bits be incremented if (tgt)
;	has wrapped since it was last checked.  The carry bit is valid.
;	The BP, Byte Pointer, regsister must be 2.
 .macro	ACCUP,	cnt,tgt
	  exhw	  t0,tgt,0
	  andn	  tgt,tgt,t0
	  cplt	  t0,cnt,t0
	  srl	  t0,t0,15
	  inhw	  t0,t0,cnt
	  add	  tgt,tgt,t0
 .endm
	.equ	ACCUP_OVHD, 6



;stop after discovering something bad
 .macro	DIE,	flag, reg
	  mtsrim  CPS,PS_INTSOFF | PS_TU
	  constx  reg,flag
	  jmp	  .
	   store  0,HWORD,reg,p_mailbox
 .endm
