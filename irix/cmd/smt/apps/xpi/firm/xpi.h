;GIO bus FDDI firmware Common definitions
;
; Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
;	"$Revision: 1.20 $"


;maximum size of an incoming FDDI frame, including 3 bytes of padding
;   and 32 more just in case.
	.equ	MAX_FRAME_LEN, (4500+FRAME_PAD_LEN+16+BLUR_ROUND-1) & ~BLUR_ROUND
	.equ	MIN_FRAME_LEN, 1+2+2	;at least a complete short frame


 .if	MHZ==16
	.equ	MHZ_BT,4		;binary log of MHZ
 .else
	.print	"unanticipated clock rate"
	.err
 .endif

;interval for 29K timer
	DEFBT	TIME_TICK, 4		;awaken 16 times each second
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


;static register allocations
;   These allocations preclude the use of C on the board.
;   Local registers are used as if they were globals.  They are used
;   in interrupt handlers.
;   The return registers are used as temporaries for interrupt handlers.

	.reg	it0, u0			;trap handler temps
	.reg	it1, u1
	.reg	it2, u2
	.reg	it3, u3


	greg	time_hz,		;time in ticks in the current second

	greg	p_crap,			;CRAP
	greg	p_blur,			;BLUR
	greg	blur_plen,		;DMA_ALIGN
	greg	blur_pmask,		;DMA_ALIGN-1
	greg	p_creg			;CREG
	greg	cregs,			;CREG shadow
	greg	p_vsar,			;VSAR
	greg	p_gsor,			;GSOR

	greg	buf_bit_mask,		;for trimming ring buffer pointers

	greg	host_cmd_buf,		;communication area in host memory
	greg	p_host_cmd,		;host_cmd

	greg	p_fsi,			;FSI registers
	greg	p_fsifcr,
	greg	p_mac,			;MAC registers
	greg	p_elm,			;ELM registers

	greg	fsi_imr,		;shadow FSI Interrupt Mask Register

	greg	t_rx_ring,		;tail of RX ring--last to be filled
	greg	h_rx_ring,		;head of RX ring--first to be filled

	greg	t_pm_ring,		;tail of promiscuous mac ring
	greg	h_pm_ring,		;head of promiscuous mac ring

	greg	h_tx_ring,		;first free TX ring descriptor
	greg	t_tx_ring,		;newest active TX ring descriptor
	greg	free_tx_ring,		;free TX descriptors

	greg	obf,			;put new output here
	greg	obf_avail,
	greg	p_obfend,		;end of output buffer+1

	greg	out_done,		;-# of finished output requests

	greg	ls0,			;current line state on ELM0
	greg	ls1,			;current line state on ELM1
	greg	ls0_stack,		;recent line states
	greg	ls1_stack,
	.equ	LS_SHIFT,4		;4 bits/state
	.equ	LS_DATA,1		;distinquish empty from 0

	lreg	cur_st1,		;current firmware state
	.set	NEXT_BT,0
	NXTBT	ST1_BEC,		;beaconing
	NXTBT	ST1_CLM,		;claiming
	NXTBT	ST1_LONGERR,		;previous rx frame was too long
	NXTBT	ST1_PROM,		;in promiscuous mode

	NXTBT	ST1_ELM0_INT,		;turn on more ELM interrupts
	NXTBT	ST1_ELM1_INT,
 .ifdef MEZ
	NXTBT	ST2_DANG_CACHE,		;some DMA may be stuck in DANG cache
 .else
	NXTBT	ST1_spare1,
 .endif
 .ifdef DANG_BUG
	NXTBT	ST1_DANG_DMA,		;work around DANG bug on next DMA
 .else
	NXTBT	ST1_spare2,
 .endif

	NXTBT	ST1_PHY_REP,		;repeat line states for DAS-DM
	NXTBT	ST1_DOWNING,		;downloading
	NXTBT	ST1_PINT_ARM,		;PHY interrupts armed
	NXTBT	ST1_AMULTI,		;receive all multicast frames

	CK	NEXT_BT <=16
	.equ	ST1_MB, TRUE		;heard host request


	lreg	cur_st2,
	.set	NEXT_BT,0
	NXTBT	ST2_HINT_DONE,		;host has been interrupted
	NXTBT	ST2_PINT_REQ,		;need PHY interrupt
	NXTBT	ST2_NEEDI,		;need to interrupt the host
	NXTBT	ST2_OUTINT_REQ,		;need to mention finished output

	NXTBT	ST2_C2B_SLEEP,		;Host to Board DMA is off
	NXTBT	ST2_GREEN,		;recently set green LED
	NXTBT	ST2_TXDMA,		;prefer output DMA
	NXTBT	ST2_spare1,

	NXTBT	ST2_DAS,		;have 2nd phy
	NXTBT	ST2_CDDI,		;SAS CDDI with rev D ELM
	NXTBT	ST2_LAT_VOID,		;measure latency with voids
	NXTBT	ST2_IN_CKSUM,		;allow checksumming

	NXTBT	ST2_ZAPRING,		;mess up the ring
	NXTBT	ST2_ZAPTIME,		;toggle for messing up the ring
	NXTBT	ST2_IO4IA_WAR,		;work around IO4 IA chip bug


	lreg	c2b_dma_delay,		;delay control DMA by this
	lreg	c2b_sleep,		;keep C2B DMA going this long

	lreg	c2b_pos,		;position in C2B buffer in host
	lreg	c2b_atime,		;timer to keep DMA active
	lreg	c2b_delay,		;delay C2B DMA until then
	lreg	host_qtime,		;timer until host is quiet

	lreg	rel_tokb,		;bytes queued since released token
	lreg	rel_tokr,		;release the token this often

	lreg	b2h_b_ptr,		;position in B2H buffer on board
	lreg	b2h_b_base,
	lreg	b2h_b_lim,
	lreg	b2h_h_ptr,		;position in B2H buffer in host
	lreg	b2h_h_lim,
	lreg	b2h_sn,			;serial number

	lreg	b2h_phy,		;limit outstanding CMT interrupts

	lreg	sml_bufs,		;# of free host buffers
	lreg	med_bufs,
	lreg	big_bufs,
	lreg	nxt_big,		;free medium host buffers
	lreg	lst_big,
	lreg	nxt_med,		;free big host buffers
	lreg	lst_med,
	lreg	nxt_sml,		;free small host buffers
	lreg	lst_sml,

	lreg	d2b_pos,		;position in D2B buffer in the host
	lreg	d2b_lim,		;end of buffer in the host
	lreg	d2b_len,		;length of next output frame or -1

	lreg	p_cnts,			;counters

	lreg	addr_hi0,		;EBus address bits
	lreg	addr_hi1,

;block of registers for checksumming
	lreg	sumreg,
	lreg	sumreg1,
	lreg	sumreg2,
	lreg	sumreg3,
	.equ	N_SUMREG, 96
	.set	NEXT_LREG, NEXT_LREG + N_SUMREG-4
	CK	NEXT_LREG<=128

	.purgem	greg,lreg



;test state bit
 .macro	t_st,	reg,nm
   .ifdef ST1_@nm@_BT
	  sll	  reg,cur_st1,31-ST1_@nm@_BT
   .else
	  sll	  reg,cur_st2,31-ST2_@nm@_BT
   .endif
 .endm

;set state bit
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


;reset state bit
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

;set state bit if result of a test is true.
;   This should only be used when the source register is clean.
 .macro	c2set_st, reg,nm
   .ifdef ST1_@nm@_BT
	  srl	  reg,reg,31-ST1_@nm@_BT
	  or	  cur_st1,cur_st1,reg
   .else
	  srl	  reg,reg,31-ST2_@nm@_BT
	  or	  cur_st2,cur_st2,reg
   .endif
 .endm

;conditionally set a state bit
;   This cleans the register before using it.
 .macro	cset_st, reg,nm
	  cplt	  t0,t0,0
	c2set_st  reg,nm
 .endm



;fetch the address of one of the tables in the EEPROM.
;   This is useful before the EEPROM has been copied to SRAM.
 .macro	CONTBL,	reg, tbl
	  const	  reg,(tbl)-XPI_SRAM+XPI_FIRM
 .endm


;get the address of a counter
 .macro	CNTADR,	reg, nam
	  add	  reg,p_cnts,cnt_@nam@-cnts
 .endm


 ;fetch the value of a counter
 .macro	LODCNT,	reg,nam
	  CNTADR  reg,nam
	  load	  0,WORD,reg,reg
 .endm


;store a new value into a counter
 .macro	STOCNT,	reg,nam,tmp
	 CNTADR	  tmp,nam
	 store	  0,WORD,reg,tmp
 .endm


;most of the work of increasing a counter
;   callers know the 1st instruction of this macro is ok for a delay slot
 .macro	UPCNT,	cnt,val,ireq,tmp0,tmp1
	  CNTADR  tmp1,cnt
	  load	  0,WORD,tmp0,tmp1
  .ifeqs "@ireq@","intr"
	  or	  cur_st2,cur_st2,ST2_PINT_REQ_BT
  .else
    .ifnes "@ireq@",""
	.print	"** ERROR: @ireq@ is not intr or null"
    .endif
  .endif
	  add	  tmp0,tmp0,val
	  store	  0,WORD,tmp0,tmp1
 .endm


;increase a counter by a value, usually a hardware counter
;   cnt=name of counter, val=register with delta, tmp0,tmp1=temporaries
 .macro	INCCNT,	cnt,val,ireq,tmp0,tmp1
  .if $ISREG(val)
	  cpeq	  tmp0,val,0
	  jmpt	  tmp0,$1
	  UPCNT	  cnt,val,ireq,tmp0,tmp1
$1:
  .else
	  UPCNT	  cnt,val,ireq,tmp0,tmp1
  .endif
 .endm


;increase a counter by a bit
;   cnt=name of counter, bit=bit name, val=register containing the bit,
;   tmp0,tmp1=temporaries
 .macro	BITCNT,	cnt,bit,reg,ireq,tmp0,tmp1
	  tbit	  tmp0,reg,bit
	  jmpf	  tmp0,$1
	  UPCNT	  cnt,1,ireq,tmp0,tmp1
$1:
 .endm



;fetch or store a word in the command buffer
 .macro	LOD_CMD, reg,tag,wid
	  OPST	  load,wid,host_cmd,p_host_cmd,host_cmd+tag,reg,reg,fast
 .endm

 .macro	STO_CMD, reg,tag,tmp
	  OPST	  store,,host_cmd,p_host_cmd,host_cmd+tag,reg,tmp,fast
 .endm
