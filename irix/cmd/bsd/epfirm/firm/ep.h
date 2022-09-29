;E-Plex 8-port Ethernet board common definitions
;
; Copyright 1994 Silicon Graphics, Inc.  All rights reserved.
;	$Revision: 1.11 $


;LED meanings:
;   while writing the EEPROM
;	YELLOW=during programming
;	counting GREEN=success
;	flashing YELLOW=failed to program

;   after reset
;	YELLOW=not yet finished nibble mode signaling
;	YELLOW,GREEN1,GREEN2=bad command
;	YELLOW,GREEN0GREEN1,GREEN2=bad interrupt



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


	.equ	NUM_RX_BUFS, 64
	.equ	ETHER_SIZE, 1518
	.equ	ETHER_CRC_SIZE, 4
	.equ	MIN_ETHER,  60		;not counting CRC


;register allocations
;   These allocations preclude the use of C on the board.
;   Local registers are used as if they were globals.  They are used
;   in interrupt handlers.
;   The return registers are used as temporaries for interrupt handlers.

	.reg	it0, u0			;trap handler temps
	.reg	it1, u1
	.reg	it2, u2
	.reg	it3, u3

	greg	g_st,			;global firmware state
	.set	NEXT_BT,0
	NXTBT	GST_HINT_DONE,		;host has been interrupted
	NXTBT	GST_NEEDI,		;need to interrupt the host
	NXTBT	GST_OUTINT_REQ,		;need to mention finished output
	NXTBT	GST_H2B_SLEEP,		;control host-to-board DMA is off

	NXTBT	GST_DOWNING,		;downloading
	NXTBT	GST_OUT_DONE,		;some output finished
	NXTBT	GST_DMA_CACHE,		;some DMA may be stuck in IO4 cache
 .ifdef DANG_BUG
	NXTBT	GST_DANG_DMA,		;work around DANG bug on next DMA
 .else
	NXTBT	GST_spare2,
 .endif

	NXTBT	GST_BSUM,		;bad firmware checksum

	CK	NEXT_BT <=16
	.equ	GST_MB, TRUE		;heard host request


    ;the following registers vary depending on which SONIC is currently
    ;being worked on
    ;.........
	greg	sn,			;ordinal of this SONIC

	.equ	SREG0, NEXT_GREG
	greg	isr,			;shadow of interrupt status register
	greg	rda,			;next RDA
	greg	rx_link			;_RXpkt_link in previous descriptor
	greg	rrp,			;next receive resource descriptor
	greg	rbuf,			;next receive buffer
	greg	d2b_len,		;length of next packet or D2B_END
	    DEFBT   D2B_START,  31
	    DEFBT   D2B_END,    30
	greg	d2b_head,		;find Ethernet TX work here
	greg	d2b_tail,		;add Ethernet TX work here
	greg	obf,			;put new output packets here
	greg	obf_avail,		;output space available
	greg	tx_head,		;oldest active TX descriptor
	greg	tx_link,		;_TXpkt_link in recent TX descriptor
	greg	out_done,		;-# of finished output requests
	greg	sn_st,			;SONIC state bits
	.set	NEXT_BT,0
	NXTBT	SNST_TX_ACT,		;output running
	NXTBT	SNST_TX_SICK,		;TX shut down by error
	NXTBT	SNST_AMULTI,		;get all multicast packets
	NXTBT	SNST_PINT0,		;count TX buffers without PINT bit
	.set	NEXT_BT,NEXT_BT+3	;interrupt every 16 packets
	NXTBT	SNST_PINT_OVF,
	NXTBT	SNST_LCD_WAIT,		;waiting for CAM to be loaded
	CK	NEXT_BT <=16
	.equ	N_VSREG, NEXT_GREG-SREG0

    ;registers that change above here, those that do not below

	greg	p_isr,			;SONIC interrupt status register
	greg	obf_end
	greg	d2b_lim
	.equ	N_SREG, NEXT_GREG-SREG0
	CK	NEXT_GREG <= 95 || SREG0 > 95	;(see definition of greg)
    ;.........

	greg	time_hz,		;time in ticks in the current second

	greg	p_gsor,			;GSOR
	greg	gsor
	greg	p_intc,			;INTC

	greg	p_cnts
	greg	p_spriv0
	greg	p_sram

	greg	buf_bit_mask

	greg	host_cmd_buf,		;communication area in host memory
	greg	p_host_cmd,		;host_cmd

	greg	snreset,		;shadow of SONIC reset register

	greg	snact,			;bit mask of active SONICs
	DEFBT	SNACT0, 0		;(conveniently same as GSOR_SIR0)

	greg	tx_lim,			;avoid starving input
	.equ	TX_LIM_RESET, 8

	lreg	p_rbounce,		;next receive bounce buffer to use

	lreg	max_obf_avail,		;max TX buffer space ever available

	lreg	h2b_dma_delay,		;delay control DMA by this
	lreg	h2b_sleep,		;keep H2B DMA going this long

	lreg	h2b_pos,		;position in H2B buffer in host
	lreg	h2b_atime,		;keep H2B DMA active until then
	lreg	h2b_delay,		;delay H2B DMA until then
	lreg	inth_time,		;delay board-to-host int. until then
	lreg	int_delay,		;delay board-to-host int.

	lreg	b2h_b_ptr,		;position in B2H buffer on board
	lreg	b2h_b_lim
	lreg	b2h_b_base
	lreg	b2h_h_ptr,		;position in B2H buffer in host
	lreg	b2h_h_lim
	lreg	b2h_sernum,		;B2H block serial number

	lreg	big_bufs,		;# of free host buffers
	lreg	lst_big
	lreg	nxt_big
	lreg	sml_bufs
	lreg	lst_sml
	lreg	nxt_sml
	lreg	sml_size,		;size of small buffers

;block of registers for checksumming
	lreg	sumreg
	lreg	sumreg1
	lreg	sumreg2
	lreg	sumreg3
	lreg	sumreg4
	lreg	sumreg5
	lreg	sumreg6
	.equ	N_SUMREG, 96
	.set	NEXT_LREG, NEXT_LREG + N_SUMREG-6
	CK	NEXT_LREG<=128

	.purgem	greg,lreg



;test state bit
 .macro	t_st,	reg,nm
   .ifdef GST_@nm@_BT
	  sll	  reg,g_st,31-GST_@nm@_BT
   .else
	  sll	  reg,sn_st,31-SNST_@nm@_BT
   .endif
 .endm

;set state bit
 .macro	set_st,	nm,tmp
   .ifdef GST_@nm@
      .if GST_@nm@ >= 256 && GST_@nm@ <= 0x8000
	  const	  tmp,GST_@nm@
	  or	  g_st,g_st,tmp
      .else
	  or	  g_st,g_st,GST_@nm@
      .endif
   .else
      .if SNST_@nm@ >= 256 && SNST_@nm@ <= 0x8000
	  const	  tmp,SNST_@nm@
	  or	  sn_st,sn_st,tmp
      .else
	  or	  sn_st,sn_st,SNST_@nm@
      .endif
   .endif
 .endm


;reset state bit
 .macro	rs_st,	nm,tmp
   .ifdef GST_@nm@
      .if GST_@nm@ >= 256 && GST_@nm@ <= 0x8000
	  const	  tmp,GST_@nm@
	  andn	  g_st,g_st,tmp
      .else
	  andn	  g_st,g_st,GST_@nm@
      .endif
   .else
      .if SNST_@nm@ >= 256 && SNST_@nm@ <= 0x8000
	  const	  tmp,SNST_@nm@
	  andn	  sn_st,sn_st,tmp
      .else
	  andn	  sn_st,sn_st,SNST_@nm@
      .endif
   .endif
 .endm

;set state bit if result of a test is true.
;   This should only be used when the source register is clean.
 .macro	c2set_st, reg,nm
   .ifdef GST_@nm@_BT
	  srl	  reg,reg,31-GST_@nm@_BT
	  or	  g_st,g_st,reg
   .else
	  srl	  reg,reg,31-SNST_@nm@_BT
	  or	  sn_st,sn_st,reg
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
	  const	  reg,(tbl)-GIORAM+EP_FIRM
 .endm


;Compute the address of a SONIC chip register.
;   This macro is safe "in" a delay slot.
 .macro	SNADR,	reg,sreg
   .if SN_@sreg@ >= SN_ISR
	  add	  reg,p_isr,SN_@sreg@-SN_ISR
   .else
	  sub	  reg,p_isr,SN_ISR-SN_@sreg
   .endif
 .endm

;fetch a SONIC register
 .macro	SNFET,	reg,sreg
	SNADR	reg,sreg
	load	0,HWORD,reg,reg
 .endm


;compute the address of one of a SONIC's SRAM structures
 .macro	SRAMADR,reg,tgt,tmp
	CK	tgt <= SONICRAM_SIZE
  .if tgt <= 255
	  sll	  reg,sn,SONICRAM_SIZE_BT
	  add	  reg,reg,tgt
  .else
	constx	reg,tgt
	  sll	  tmp,sn,SONICRAM_SIZE_BT
    .if reg==tmp
	.print "** Error: @reg@ and @tmp@ cannot be the same"
	.err
    .endif
	  add	  reg,reg,tmp
  .endif
	  add	  reg,reg,p_sram
 .endm


;compute the address of one of a SONIC's GIORAM private variables
 .macro	GRAMADR,reg,nam,tmp
	CK	@nam@0 >= spriv0 && @nam@0 < spriv0+_SIZE_SGRAM
	  sll	  reg,sn,_SIZE_SGRAM_BT
	  add	  reg,reg,p_spriv0
   .if @nam@0 != spriv0
    .if @nam@0-spriv0 <= 255
	  add	  reg,reg,@nam@0-spriv0
    .else
	  CK	  &reg != &tmp
	  constx  tmp,@nam@0-spriv0
	  add	  reg,reg,tmp
     .endif
   .endif
 .endm


;fetch a block of SONIC 29K registers
;   callers know the first instruction affects at most register tm1
 .macro LODREGS,tm1
	GRAMADR	tm1,sregs,,
	  mtsrim  CR,N_SREG-1
	  loadm	  0,WORD,%%SREG0,tm1
 .endm


;store a block of SONIC 29K registers
;   callers know the first instruction affects at most tm1
 .macro	STOREGS,tm1,all
	GRAMADR	tm1,sregs,,
  .ifeqs "@all@","all"
	  mtsrim  CR,N_SREG-1
  .else
    .ifnes "@all@",""
	.print "** Error: @all@ is not 'all' or null"
	.err
    .else
	  mtsrim  CR,N_VSREG-1
    .endif
  .endif
	  storem  0,WORD,%%SREG0,tm1
 .endm


;advance to next block of SONIC 29K registers
 .macro ADVREGS,op,tm1
	STOREGS	tm1,,
	  add	  sn,sn,1
	  and	  sn,sn,EP_PORTS-1
	LODREGS	tm1
 .endm


;select a block of SONIC 29K registers
;   assumed to do no more than mess up tm1 if used in a delay slot
 .macro	SELREGS,bn,tm1
	  cpeq	  tm1,sn,bn
	  jmpt	  tm1,$1
	STOREGS	tm1,,
   .if $ISREG(bn)
	  add	  sn,bn,0
   .else
	  const	  sn,bn
   .endif
	LODREGS	tm1
$1:
 .endm


;get the address of counter "nam" into register "reg"
 .macro	CNTADR,	reg,nam
	  sll	  reg,sn,SHIFT_CNTS0_BT
	  add	  reg,reg,p_cnts
  .ifnes "@nam@","all"
   .if cnt_@nam@ != 0
	  add	  reg,reg,cnt_@nam@
   .endif
  .endif
 .endm

 ;fetch the value of counter "nam" into register "reg"
 .macro	LODCNT,	reg,nam
	  CNTADR  reg,nam
	  load	  0,WORD,reg,reg
 .endm


;store a new value into counter "nam" from register "reg" using "tmp"
 .macro	STOCNT,	reg,nam,tmp
	 CNTADR	  tmp,nam
	 store	  0,WORD,reg,tmp
 .endm


;most of the work of increasing a counter
;   callers know the 1st instruction of this macro is ok for a delay slot
 .macro	UPCNT,	cnt,val,tmp0,tmp1
	  CNTADR  tmp1,cnt
	  load	  0,WORD,tmp0,tmp1
	  add	  tmp0,tmp0,val
	  store	  0,WORD,tmp0,tmp1
 .endm


;increase a counter by a value, usually a hardware counter
;   cnt=name of counter, val=register with delta, tmp0,tmp1=temporaries
;   callers know the first instruction of this macro changes only tmp0
 .macro	INCCNT,	cnt,val,tmp0,tmp1
  .if $ISREG(val)
	  cpeq	  tmp0,val,0
	  jmpt	  tmp0,$1
	  UPCNT	  cnt,val,tmp0,tmp1
$1:
  .else
	  UPCNT	  cnt,val,tmp0,tmp1
  .endif
 .endm


;increase a counter by a bit
;   cnt=name of counter,
;   bit=bit name,
;   val=register containing the bit,
;   lab=optional label for bit=0,
;   tmp0,tmp1=temporaries
;   callers know the 1st instruction of this macro is ok for a delay slot
 .macro	BITCNT,	cnt,bit,reg,lab,tmp0,tmp1
	  tbit	  tmp0,reg,bit
  .ifnes "@lab@",""
	  jmpf	  tmp0,lab
	  UPCNT	  cnt,1,tmp0,tmp1
  .else
	  jmpf	  tmp0,$1
	  UPCNT	  cnt,1,tmp0,tmp1
$1:
  .endif
 .endm



;fetch or store a word in the command buffer
 .macro	LOD_CMD, reg,tag,wid
	  OPST	  load,wid,host_cmd,p_host_cmd,host_cmd+tag,reg,reg,fast
 .endm

 .macro	STO_CMD, reg,tag,tmp
	  OPST	  store,,host_cmd,p_host_cmd,host_cmd+tag,reg,tmp,fast
 .endm
