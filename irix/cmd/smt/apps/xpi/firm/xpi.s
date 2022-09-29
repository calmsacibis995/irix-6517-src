;GIO bus FDDI firmware

; Copyright 1994 Silicon Graphics, Inc.  All rights reserved.
; "$Revision: 1.45 $"

;	.equ	DEBUG,1


	;XXX should be in gio.h.
	;XXX needed in std.h, but gio.h needs std.h
	.equ	AM29030,1

 .ifdef mez_d.o
	.file	"mez_d.s"
	.equ	BIGADDR, 1
	.equ	MEZ, 1
 .else
   .ifdef mez_s.o
	.file	"mez_s.s"
	.equ	BIGADDR, 1
	.equ	MEZ, 1
	.equ	DANG_BUG,1	;must read between all writes
   .else
     .ifdef lc.o
	.file	"lc.s"
	.equ	LC, 1
     .else
	.err
     .endif
   .endif
 .endif
	.include "std.h"
	.include "gio.h"
	.include "if_xpi_s.h"
	.include "xpi.h"

	.sect	code,text,absolute SRAM
	.use	code


	.equ	VERS_FAM, 2*0x01000000	;"family" of interface
	.equ	VERS_DAY, (((VERS_Y-1992)*13+VERS_M)*32+VERS_D)*24
	.equ	VERS,VERS_FAM + (VERS_DAY+VERS_H)*60+VERS_MM


	.sect	bss, bss, absolute SRAM+XPI_FIRM_SIZE
	.use	bss

;macros to note instructions that must be adjusted for 64-bit hosts
;add the difference between the default and actual values of MLEN
 .macro	ASML, oldval
  .if (((oldval)+MAX_DSML)&256) != ((oldval)&256)
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

;Add the difference between the default and actual 'if' header sizes.
 .macro	APAD, oldval
  .if (((oldval)+MAX_DPAD)&256) != ((oldval)&256)
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

;Subtract the difference between the default and actual 'if' header sizes.
;   This should be used on constant expresssions containin "-PAD0_IN_DMA".
 .macro	SPAD,oldval
  .if (((oldval)-MAX_DPAD)&256) != ((oldval)&256)
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

;Adjust the net size of a small mbuf, accounting for both the increased
;   MLEN and the increased padding for the 'fi' header.
 .macro	FIX_SML_NET
	ASML	SML_NET
	SPAD	SML_NET
 .endm

;host buffer pools
	DEFBT	MAX_POOL,   6
	.equ	POOL_LEN, MAX_POOL*4
	.equ	POOL_ADDR_HI_OFF, POOL_LEN
	.equ	BUF_BIT_MASK, POOL_LEN
	CKALIGN	BUF_BIT_MASK
	CK	MAX_POOL>=MAX_BIG && MAX_POOL>=MAX_MED && MAX_POOL>=MAX_SML
 .ifdef BIGADDR
smls:	.block	POOL_LEN*2
meds:	.block	POOL_LEN*2
bigs:	.block	POOL_LEN*2
 .else
smls:	.block	POOL_LEN
meds:	.block	POOL_LEN
bigs:	.block	POOL_LEN
 .endif

;Advance a pointer in the host buffer pools
 .macro	ADVPOOL, reg,sz
  .if (@sz@s & BUF_BIT_MASK) != 0
	  andn	  reg,reg,buf_bit_mask
	  add	  reg,reg,4
	  or	  reg,reg,buf_bit_mask
  .else
	  add	  reg,reg,4
	  andn	  reg,reg,buf_bit_mask
  .endif
 .endm

;save a buffer address from the host
 .ifdef BIGADDR
  .macro SAVBUF,typ,tmp
	  store	  0,WORD,t2,lst_@typ@
	  add	  tmp,lst_@typ@,buf_bit_mask
	  CK	  POOL_ADDR_HI_OFF == BUF_BIT_MASK
	  ADVPOOL lst_@typ@,typ
	  srl	  t1,t1,C2B_ADDR_HI_LSB_BT
	  sll	  t1,t1,CRAP_EB_LSB_BT
	  store	  0,WORD,t1,tmp
	  jmp	  m_c2b_lop
	   add	  @typ@_bufs,@typ@_bufs,1
  .endm
 .else
  .macro SAVBUF,typ
	  store	  0,WORD,t2,lst_@typ@
	  ADVPOOL lst_@typ@,typ
	  jmp	  m_c2b_lop
	   add	  @typ@_bufs,@typ@_bufs,1
  .endm
 .endif

;allocate a buffer
;   of type typ, putting its address into hiadr:adr
 .ifdef BIGADDR
  .macro USEBUF,typ,hiadr,adr,branch
	  load	  0,WORD,adr,nxt_@typ@
	  CK	  POOL_ADDR_HI_OFF == BUF_BIT_MASK
	  add	  hiadr,nxt_@typ@,buf_bit_mask
	  ADVPOOL nxt_@typ@,typ
	  load	  0,WORD,hiadr,hiadr
   .ifnes "@branch@",""
	  jmp	  branch
	   sub	  @typ@_bufs,@typ@_bufs,1
   .else
	  sub	  @typ@_bufs,@typ@_bufs,1
   .endif
  .endm
 .else
  .macro USEBUF,typ,hiadr,adr,branch
	  load	  0,WORD,adr,nxt_@typ@
	  ADVPOOL nxt_@typ@,typ
   .ifnes "@branch@",""
	  jmp	  branch
	   sub	  @typ@_bufs,@typ@_bufs,1
   .else
	  sub	  @typ@_bufs,@typ@_bufs,1
   .endif
  .endm
 .endif


;FSI CAM entries
	DMABLK	fsi_cam, FSI_CAM_LEN*8


;FSI rings
;   We use 4 rings,
;	normal output
;	beacon output
;	normal input
;	promiscuous mode MAC input

;input ring
	.equ	RX_RING_N,  4
	.equ	GSR_RX_ROV, GSR_ROV4
	.equ	GSR_RX_ROV_BT, GSR_ROV4_BT
	.equ	GSR_RX_RXC, GSR_RXC4
	.equ	GSR_RX_RXC_BT, GSR_RXC4_BT
	.equ	FSI_MRR_RX_RE, FSI_MRR_RE4
	.equ	FSI_MRR_RX_RE_BT, FSI_MRR_RE4_BT
	.equ	RX_BSIZE_BT,7+FSI_RBR_MIN_BT
	.equ	RX_BSIZE,(MAX_FRAME_LEN+DMA_ALIGN-1) & ~(DMA_ALIGN-1)
	CK	(1<<(RX_BSIZE_BT-1)) < RX_BSIZE
	CK	(1<<(RX_BSIZE_BT+1)) > RX_BSIZE
	DEFBT	RX_RING_LEN,5
	CKALIGN	RX_RING_LEN*FSI_RING_LEN
	.equ	rx_ring_bit_mask, RX_RING_LEN*FSI_RING_LEN
rx_ring:.block	RX_RING_LEN*FSI_RING_LEN

;output ring
	.equ	TX_RING_N,  1
	.equ	GSR_TX_RCC, GSR_RCC1
	.equ	GSR_TX_RCC_BT, GSR_RCC1_BT
	.equ	GSR_TX_RNR, GSR_RNR1
	.equ	GSR_TX_RNR_BT, GSR_RNR1_BT
	DEFBT	TX_RING_LEN,5		;32 transmit buffers
	.equ	TX_RING_NET, TX_RING_LEN-1
	CKALIGN	TX_RING_LEN*FSI_RING_LEN
	.equ	tx_ring_bit_mask, TX_RING_LEN*FSI_RING_LEN
tx_ring:.block	TX_RING_LEN*FSI_RING_LEN

;promiscuous mac frames
;   two rings to accept MAC frames without overflow
	.equ	PM_RING_N,  5
	.equ	GSR_PM_ROV, GSR_ROV5
	.equ	GSR_PM_ROV_BT, GSR_ROV5_BT
	.equ	GSR_PM_RXC, GSR_RXC5
	.equ	GSR_PM_RXC_BT, GSR_RXC5_BT
	.equ	FSI_MRR_PM_RE, FSI_MRR_RE5
	.equ	FSI_MRR_PM_RE_BT, FSI_MRR_RE5_BT
	DEFBT	PM_BSIZE,   5		;32 bytes/buffer
	DEFBT	PM_RING_LEN,2		;4 frames
	.equ	pm_ring_bit_mask, PM_RING_LEN*FSI_RING_LEN
	CKALIGN	PM_RING_LEN*FSI_RING_LEN
pm_ring:.block	PM_RING_LEN*FSI_RING_LEN
	CKALIGN	PM_RING_LEN*FSI_RING_LEN
pm_ring2:.block	PM_RING_LEN*FSI_RING_LEN

;the beacon ring
;   two rings to make the FSI transmit the same packets forever.
	.equ	B_RING_N,   0
	DEFBT	B_RING_LEN, 0
	CKALIGN	B_RING_LEN*FSI_RING_LEN
b_ring:	.block	B_RING_LEN*FSI_RING_LEN
	CKALIGN	B_RING_LEN*FSI_RING_LEN
b_ring2:.block	B_RING_LEN*FSI_RING_LEN		;fake to make it run forever


	.equ	FSIMSK, GSR_RX_RXC|GSR_PM_RXC|GSR_RX_ROV|GSR_PM_ROV|GSR_TX_RCC|GSR_TX_RNR



;advance a pointer in an ring.
;   tgt=put result here
;   src=starting with this pointer
;   delta=add this to the pointer
;   ring=target ring
;   jloc=optionally jump there
;   breg=mask for wrapping around
;   bval=set or clear this bit to wrap around
 .macro	ADVRP3,	tgt,src,delta, ring, jloc, breg,bval
  .if (@ring@_ring & bval) != 0
	  andn	  tgt,src,breg
	  add	  tgt,tgt,delta
    .ifnes "@jloc@",""
	  jmp	  jloc
	   or	  tgt,tgt,breg
     .else
	  or	  tgt,tgt,breg
    .endif
  .else
	  add	  tgt,src,FSI_RING_LEN
    .ifnes "@jloc@",""
	  jmp	  jloc
	   andn	  tgt,tgt,breg
    .else
	  andn	  tgt,tgt,breg
    .endif
  .endif
 .endm

;advance a pointer in a ring.
;   tgt=put result here
;   src=starting with this pointer
;   delta=add this to the pointer
;   ring=target ring
;   jloc=optionally jump there
 .macro	ADVRP2,	tgt,src,delta, ring, jloc
   .if @ring@_ring_bit_mask >= BUF_BIT_MASK
	CK	@ring@_ring_bit_mask == BUF_BIT_MASK
	ADVRP3	tgt,src,delta, ring, jloc, buf_bit_mask,BUF_BIT_MASK
   .else
	ADVRP3	tgt,src,delta, ring, jloc, @ring@_ring_bit_mask,@ring@_ring_bit_mask
   .endif
 .endm

 .macro	ADVRP,	reg, ring, jloc
	ADVRP2	reg,reg,FSI_RING_LEN, ring, jloc,
 .endm

	.use	bss

cmd_id:	.block	    4			;most recent host command ID

    ;command buffer or communications area
	DMABLK	host_cmd, HC_CMD_SIZE


;buffer for directed beacons
	.align
beacon_buf:.block   3+13+4+6+6		;6 extra just in case


;filter promiscous mac frames
	.equ	    PM_FILTER_LEN, 4
	.align
pm_filter:.block    PM_FILTER_LEN*4


;multicast and promiscuous DA hash table
	.equ	DAHASH_SIZE, 256/8	;256 bits in the table
	DMABLK	dahash, DAHASH_SIZE


;shape of host control-to-board blocks
	.dsect
_C2B_OP:    .block	4
_C2B_ADDR:  .block	4
_SIZE_C2B_BLK:.block	1
	.use	*
 .ifdef MEZ
	.equ	C2B_NUM,  16
 .else
	.equ	C2B_NUM,  10
 .endif
	.equ	C2B_SIZE, C2B_NUM * _SIZE_C2B_BLK
	.use	bss
	DMABLK	c2b, C2B_SIZE
	.block	_SIZE_C2B_BLK	;to ensure we do not run off the end

;address of host-to-board DMA control ring in host memory
c2b_base:   .block  4


;each host-to-board data DMA request is DMA'ed from the host to this buffer
;   of blocks.  The first one is a "struct hd", and subsequent ones are
;   "struct sg".
	.equ	D2B_SIZE, (MAX_DMA_ELEM+1) * _SIZE_D2B_BLK
	.use	bss
	DMABLK	d2b, D2B_SIZE

d2b_base:    .block	4		;start of buffer in the host

;board-to-host control queue
	.equ	BD_B2H_LEN, 32
	.equ	BD_B2H_SIZE, BD_B2H_LEN*_SIZE_B2H_BLK
	DMABLK	b2h, BD_B2H_SIZE,0

b2h_h_base: .block  4

;address of counters in host memory
ctptr:	    .block  4

;total maximum host buffers
max_bufs:   .block  4

;limit interrupts to this
int_delay:  .block  4

;Do not let the work-inferred interrupt delay exceed this.
max_host_work:.block 4

;when last latency measurement was made
void_time:  .block  4

 .ifdef MEZ
dma_dummy:  .block  4			;dummy DMA read
 .endif

;Counters sent to the host for XCMD_FET_CNT
	.set	CNT_SIZE,0
 .macro	CNT,	nm
	.set	CNT_SIZE, CNT_SIZE+4
 .endm
	CNTS
	.purgem	CNT
	.use	bss
	DMABLK	cnts,CNT_SIZE,128
	.set	CNT_CUR,0
 .macro	CNT,	nm
	.equ	cnt_@nm@,cnts+CNT_CUR
	.set	CNT_CUR, CNT_CUR+4
 .endm
	CNTS
	.purgem	CNT

 .ifdef MEZ
;The DANG chip prefers DMA that is aligned to 128-byte boundaries, so
;align the frame buffers.
	BALIGN	DANG_CACHE_SIZE,0
 .endif

;buffer for promiscuous MAC frame sampling
	.align
pm_bufs:.block	    PM_BSIZE*PM_RING_LEN


;The rest of SRAM is devoted to frame buffers.
	.use	bss
rx_bufs:.block	RX_BSIZE*(RX_RING_LEN-1)

	.block	8192-RX_BSIZE		;pad for frame overflow


	CK	. < SRAM+SRAM_SIZE-32*1024
	.equ    TX_BUFS_SIZE, SRAM+SRAM_SIZE - .
	CK	TX_BUFS_SIZE-MAX_FRAME_LEN < D2B_BAD
	CK	(TX_BUFS_SIZE / MAX_FRAME_LEN) <= TX_RING_LEN
tx_bufs:.block	TX_BUFS_SIZE

;Maximum output buffer space available.
;   It is reduced by one frame at the end of the buffer, because each frame
;   must be contiguous.
	.equ	MAX_OBF_AVAIL,	TX_BUFS_SIZE-MAX_FRAME_LEN

	.eject
;Execution starts here.
;   This is assembled as if at SRAM=0x400000, but executed at EPROM=0.
;   This works because some 29k jumps are PC-relative.  As soon as the
;   code has been copied to SRAM, we jump to it and run from there.

;To save the cost of aligning the 29K interrupt vector table, it is
;   located at the start of SRAM.  To simplify that, it is located
;   at the start of the EEPROM, with the initial vector replaced with
;   a jump around the rest of the table.

	.global	_main
	.use	code

	.equ	vectbl, .		;over write with interrupt vectors

_main:
    ;avoid cache-enable-jmp bug, Erratum #4 of Rev B1 and #3 of Rev D0
	CK	(. % (4*4)) == 0
cacheon:mtsrim	CFG, CFG_ILU
	mtsrim	CPS, PS_INTSOFF
	mtsrim	OPS, PS_INTSOFF
	xor	gr1,gr64,gr64

	inv
	nop
	const	gr64,255*4
	const	gr65,(255-65)-2
	CK	(.-cacheon) >= (2*4*4)	;avoid cache-enable-jmp bug
main1:	mtsr	IPA,gr64
	const	gr0,0			;clear all of the registers
	jmpfdec	gr65,main1
	 sub	gr64,gr64,4
	const	gr64,0
	const	gr65,0

    ;set global registers
	constx	buf_bit_mask,	BUF_BIT_MASK
	constx	blur_plen,	DMA_ALIGN
	sub	blur_pmask,blur_plen,1
	constx	p_crap,		CRAP
	add	p_blur, p_crap, BLUR-CRAP
	add	p_vsar, p_blur, VSAR-BLUR
	add	p_gsor, p_vsar, GSOR-VSAR
	constx	p_creg,		CREG
	constx	p_fsi,		FSIBASE
	add	p_fsifcr, p_fsi, FSI_FCR-FSIBASE
	constx	p_mac,		MACBASE
	constx	p_elm,		ELMBASE
	constx	p_host_cmd,	host_cmd
	constx	p_cnts,		cnts

    ;clear DMA hardware
	const	t0,0
	store	0,WORD,t0,p_blur

    ;Immediately try to ensure that the FSI is not doing anything.  This
    ;is in case the firmware has been restarted without a hardware reset.
	STO_IR	RESET,0,0,t0,fast
	STOFSI	0,IMR,t0,t1

    ;kill the MAC
	call	t4,zapmac
	 const0	t2,MAC_CA_OFF

	consth	cregs, CREG_RED | CREG_GREEN
	store	0,WORD,cregs,p_creg	;turn off LEDs

    ;decide what kind of hardware we have
	load	0,WORD,v0,p_blur
 .ifdef MEZ
	tbit	t0,v0,BLUR_PHY_PRES
	jmpf	t0,main4		;cannot ask if no hardware present
 .endif
	 srl	v0,v0,BLUR_CONF_LSB_BT
	and	v0,v0,BLUR_CONF_MASK
	cpeq	v1,v0,BLUR_DAS
	jmpf	v1,main2

    ;if DAS, then go into repeat mode.
	 srl	t0,v1,31-ST2_DAS_BT
	call	v4,set_rep
	 or	cur_st2,cur_st2,t0
	jmp	main4
	 nop

    ;if CDDI SAS, then assume rev D ELM
main2:	cpeq	v1,v0,BLUR_CDDI
	jmpf	v1,main4
	 srl	t0,v1,31-ST2_CDDI_BT
	or	cur_st2,cur_st2,t0

main4:	const	t2, 0
	STOELM0	t2, MASK, t1
	STOELM1	t2, MASK, t1


;Get the address of the start of host communication area
;   Treat v0:host_cmd_buf:v2:v3 as one big register.  Bits are shifted into
;   the most significant bit of v0 and out of the least significant of v3.
gethc1:	constx	v0,0x11111111
	add	host_cmd_buf,v0,v0	;start with something different from
	add	v2,host_cmd_buf,v0	;   the preamble
	add	v3,v2,v0

gethc2:	mfsr	t0,CPS
	tbit	t0,t0,PS_IP
	jmpt	t0,gethc4		;wait for an interrupt request

    ;repeat line states when we have nothing better to do.
	 t_st	t0,PHY_REP
	jmpf	t0,gethc2
	 LODELM0 t3,STATUS_A
	CONTBL	t1,rcv_ls_tbl
	srl	t3,t3,ELM_LS_LSB_BT-1	;get current and previous line states
	CK	ELM_LS_LSB_BT>1
	and	t3,t3,0x3e
	add	t3,t3,t1
	load	0,HWORD,t3,t3
	and	t3,t3,0xf		;t3=current LS translated
	cpeq	t0,t3,ls0
	mov	ls0,t3
	jmpf	t0,rep_ls
	 CONTBL	t0,gethc2
	jmp	gethc2

gethc4:	 mtsrim	FC,32-4
	load	0,WORD,t0,p_gsor
	extract	v3,v2,v3		;move 4 bits from word to word
	extract	v2,host_cmd_buf,v2
	extract	host_cmd_buf,v0,host_cmd_buf
	and	t0,t0,GSOR_MASK
	srl	t0,t0,GSOR_FLAG0_BT
	extract	v0,t0,v0		;put 4 bits from host into 1st word

	constx	t0,XPI_GIO_DL_PREAMBLE	;wait for the magic word
	cpneq	t0,t0,v3
	jmpt	t0,gethc2

	 xor	t0,v2,host_cmd_buf
	xor	t0,t0,v0
	add	t1,t0,1
	cpneq	t2,t1,0			;and the correct checksum
	jmpt	t2,gethc1		;re-sync. if wrong

	 and	t0,host_cmd_buf,3
	cpeq	t0,t0,0			;require word alignment
	jmpf	t0,gethc1
	 nop

    ;XXX do something with the opcode in v2


;   clear SRAM
    .if	N_SUMREG > 64
	.equ	CLR_LEN, 64
    .else
	.equ	CLR_LEN, N_SUMREG
	CK	CLR_LEN > 2
    .endif
	constx	t0,SRAM_SIZE/CLR_LEN/4-2
	constx	t1,SRAM-CLR_LEN*4
	const	t2,CLR_LEN*4
ramclr:	mtsrim	CR,CLR_LEN-1
	add	t1,t1,t2
	jmpfdec	t0,ramclr
	 storem	0,WORD,sumreg,t1

;   Checksum part of the EPROM as we copy it to SRAM
	const	v0,0			;v0=accumulating checksum
	constx	t0,EPROM
	constx	t1,SRAM-4
	constx	t2,XPI_FIRM_SIZE/4-2
ckprom2:load	0,WORD,t4,t0
	add	t0,t0,4
	add	t1,t1,4
	store	0,WORD,t4,t1
	jmpfdec	t2,ckprom2
	 add	v0,v0,t4
	inv

;   Checksum the rest of the EPROM
;	We could use loadm instructions, but the EPROM is so slow it is
;	as fast to do it this way, which uses less code.
	constx	t2,(EPROM_SIZE-XPI_FIRM_SIZE)/4/2-2
	const	t3,0
ckprom4:load	0,WORD,t4,t0
	add	t0,t0,4
	add	v0,v0,t3
	load	0,WORD,t3,t0
	add	t0,t0,4
	jmpfdec	t2,ckprom4
	 add	v0,v0,t4
	add	v0,v0,t3


;   If the checksum is ok, set the red LED.
;   If bad, continue in the hope that enough is ok to do a new download.
	cpeq	t0,v0,0
	srl	t0,t0,31-CREG_RED_BT
	andn	cregs,cregs,t0
	store	0,WORD,cregs,p_creg	;red LED on if the checksum was ok

	const	v1,badint
	consth	v1,badint
	const	v0,vectbl
	consth	v0,vectbl
	constx	t2,NUM_V-2
	mov	t3,v0
setiv:	store	0,WORD,v1,t3		;clean interrupt vector table
	jmpfdec	t2,setiv
	 add	t3,t3,4

	mtsrim	CR, LEN_VEC-1
	CONTBL	t3,intvecs
	loadm	0,WORD,sumreg,t3
	constx	t3,V_TIMER*4
	add	t3,t3,v0
	mtsrim	CR, LEN_VEC-1		;install the interesting vectors
	storem	0,WORD,sumreg,t3

	mtsr	VAB, v0			;start using the table


    ;set more global registers
	constx	t_tx_ring, tx_ring
	mov	h_tx_ring, t_tx_ring
	constx	free_tx_ring, TX_RING_NET

	constx	obf,tx_bufs
	constx	obf_avail,MAX_OBF_AVAIL
	add	p_obfend,obf,obf_avail

	set_st	C2B_SLEEP
	set_st	HINT_DONE
	consth	c2b_atime,TIME_NEVER
	consth	c2b_delay,TIME_NEVER


    ;Tell the host we are alive by writing our signature in the host
    ;communication area.  If the checksum was bad, say so in the version.
	add	v4,p_host_cmd,hc_sign
	constx	t1,XPI_SIGN
	store	0,WORD,t1,v4

	constx	t1,VERS

	tbit	t3,cregs,CREG_RED
	cplt	t3,t3,0			;red LED off means bad checksum
	or	t1,t1,t3		;put bad checksum bit in sign bit
	CK	CKSUM_VERS == TRUE

	load	0,WORD,t2,p_blur

 .ifdef MEZ
	tbit	t0,t2,BLUR_PHY_PRES	;if no daughter board,
	cpge	t0,t0,0			;then not DAS and no bypass
	srl	t3,t0,31-NO_PHY_VERS_BT
	jmpt	t0,ssign
	 or	t1,t1,t3
 .endif

	t_st	t0,DAS
	jmpf	t0,ssign
	 cplt	t0,t0,0
	srl	t3,t0,31-PHY2_VERS_BT	;say if we are DAS
	or	t1,t1,t3

	tbit	t0,t2,BLUR_BYPASS_PRES
	cplt	t0,t0,0
	srl	t0,t0,31-BYPASS_VERS_BT
	or	t1,t1,t0		;say if we have an optical bypass

ssign:	add	t0,p_host_cmd,hc_vers
	store	0,WORD,t1,t0

	DOLDMA	8,B2H,v4,,host_cmd_buf,hc_sign,t0



    ;clean the host buffer pools
	constx	nxt_big,bigs
	mov	lst_big,nxt_big
	constx	big_bufs,-MIN_BIG

	constx	nxt_med,meds
	mov	lst_med,nxt_med
	constx	med_bufs,-MIN_MED

	constx	nxt_sml,smls
	mov	lst_sml,nxt_sml
	constx	sml_bufs,-MIN_SML

	constx	b2h_b_base,b2h
	mov	b2h_b_ptr,b2h_b_base
	add	b2h_b_lim,b2h_b_ptr,BD_B2H_SIZE

	constx	b2h_phy, PHY_INT_NUM-1

	consth	d2b_len,D2B_BAD


    ;start the 29K timer
	constx	v0,TIME_INTVL
	mtsr	TMC,v0
	CK	TMR_IE > 0xffff
	consth	v0,TIME_INTVL | TMR_IE
	mtsr	TMR,v0

    ;use our MAC address
	constx	t0,XPI_MAC_ADDR
	mtsrim	CR,2-1
	loadm	0,WORD,v1,t0
	CK	1+&v1 == &v2
	srl	t1,v1,16
	STOMAC	t1,MLA_C,t0
	STOMAC	v1,MLA_B,t0
	srl	t1,v2,16
	STOMAC	t1,MLA_A,t0
	const	t1,0
	STOMAC	t1,MSA,t0

    ;Initialize the FSI.  It has already been reset.
	CONTBL	v0,ifsi_tbl
	const	v1,ifsi_tbl_len-2
ifsi1:	WFSI_CRF t0
	load	0,WORD,t1,v0
	store	0,WORD,t1,p_fsifcr
	jmpfdec	v1,ifsi1
	 add	v0,v0,4

    ;define main output ring
	RRESET	TX_RING_N,t0,t1
	mov	t0,t_tx_ring
	consth	t0,FSI_CMD_DEFR | (TX_RING_N*FSI_CMD_RING_N)
	CMD_FSI	t0,t_tx_ring,t2,t3

    ;start FSI interrupts,
    ; let the overflow recovery mechanism start input
	constx	fsi_imr,(FSIMSK & ~(GSR_RX_ROV|GSR_PM_ROV|GSR_TX_RNR)) | GSR_ERRORS
	STOFSI	(FSIMSK|GSR_ERRORS),GSR,t2,t3,	;clear pre-existing interrupts

    ;let the normal mechanisms repeat line states
	call	t4, set_elm_mask

;for debugging, turn off the caches
 .ifdef DEBUG
	constx	v0, CFG_PMB_16K | CFG_ILU | CFG_ID
	mtsr	CFG, v0
 .endif

;Go run the code in SRAM after we finally turn on interrupts
;   Interrupts must not be turned on before this point.
	const	v0,mlop0
	consth	v0,mlop0
	jmpi	v0
	 nop


;The preceding code is overwritten with the interrupt vectors after it
;has been copied from EEPROM to SRAM.
 .if . < NUM_V*4+SRAM
	.block	NUM_V*4+SRAM - .
 .endif
	.eject
;Trap and interrupt vectors
intvecs:
	.word	timerint		;V_TIMER
	.word	badint
	.word	hostint			;V_INTR0
	.word	fsiint			;V_INTR1
	.word	macint			;V_INTR2
	.word	elmint			;V_INTR3
	;.word	dmaint			;V_TRAP0
	;.word	xxx			;V_TRAP1

	.equ	LEN_VEC, (. - intvecs)/4




;translate previous and current line states into two, possibily identical,
;   line states.
	.use	code
	.align
 .macro	mk_tbl, p,c
	  .byte	  (ELM_CB_MATCH_LS_MASK-ELM_CB_MATCH_@c@LS)>>ELM_CB_MATCH_LS_LSB_BT
	  .byte	  PCM_@p@LS*0x10+PCM_@c@LS
 .endm
rcv_ls_tbl:			;prev	current
	mk_tbl	Q,Q,		;0=QLS,	0=NLS
	mk_tbl	Q,Q,		;0=QLS,	1=ALS
	mk_tbl	Q,Q,		;0=QLS,	2=???
	mk_tbl	Q,Q,		;0=QLS,	3=ILS4
	mk_tbl	Q,Q,		;0=QLS,	4=QLS
	mk_tbl	Q,M,		;0=QLS,	5=MLS
	mk_tbl	Q,H,		;0=QLS,	6=HLS
	mk_tbl	Q,I,		;0=QLS,	7=ILS16

	mk_tbl	M,M,		;1=MLS,	0=NLS
	mk_tbl	M,M,		;1=MLS,	1=ALS
	mk_tbl	M,M,		;1=MLS,	2=???
	mk_tbl	M,M,		;1=MLS,	3=ILS4
	mk_tbl	M,Q,		;1=MLS,	4=QLS
	mk_tbl	M,M,		;1=MLS,	5=MLS
	mk_tbl	M,H,		;1=MLS,	6=HLS
	mk_tbl	M,I,		;1=MLS,	7=ILS16

	mk_tbl	H,H,		;2=HLS,	0=NLS
	mk_tbl	H,H,		;2=HLS,	1=ALS
	mk_tbl	H,H,		;2=HLS,	2=???
	mk_tbl	H,H,		;2=HLS,	3=ILS4
	mk_tbl	H,Q,		;2=HLS,	4=QLS
	mk_tbl	H,M,		;2=HLS,	5=MLS
	mk_tbl	H,H,		;2=HLS,	6=HLS
	mk_tbl	H,I,		;2=HLS,	7=ILS16

	mk_tbl	I,I,		;3=ILS,	0=NLS
	mk_tbl	I,I,		;3=ILS,	1=ALS
	mk_tbl	I,I,		;3=ILS,	2=???
	mk_tbl	I,I,		;3=ILS,	3=ILS4
	mk_tbl	I,Q,		;3=ILS,	4=QLS
	mk_tbl	I,M,		;3=ILS,	5=MLS
	mk_tbl	I,H,		;3=ILS,	6=HLS
	mk_tbl	I,I,		;3=ILS,	7=ILS16

	.purgem	mk_tbl



;convert 4 official line states to ELM register settings
	.align
setls_tbl:
	.hword	ELM_CB_M_DEFAULT | ELM_CB_TX_QLS    ;PCM_QLS

	.hword	ELM_CB_M_DEFAULT | ELM_CB_TX_ILS    ;PCM_ILS

	.hword	ELM_CB_M_DEFAULT | ELM_CB_TX_MLS    ;PCM_MLS

	.hword	ELM_CB_M_DEFAULT | ELM_CB_TX_HLS    ;PCM_HLS

	.hword	ELM_CB_M_DEFAULT | ELM_CB_TX_ALS    ;PCM_ALS for host LCT


;convert 4 official line states to ELM register settings for repeating
;   line states for a DAS
	.align
rep_ls_tbl:
	.hword	ELM_CB_M_DEFAULT | ELM_CB_TX_QLS
	.hword	ELM_CA_DEFAULT		    ;PCM_QLS

	.hword	ELM_CB_M_DEFAULT | ELM_CB_TX_ALS
	.hword	ELM_CA_DEFAULT		    ;PCM_ILS

	.hword	ELM_CB_M_DEFAULT | ELM_CB_TX_MLS
	.hword	ELM_CA_DEFAULT		    ;PCM_MLS

	.hword	ELM_CB_M_DEFAULT | ELM_CB_TX_HLS
	.hword	ELM_CA_DEFAULT		    ;PCM_HLS


m_c2b_tbl:
	.word	m_c2b_emp		;0 C2B_EMPTY
	.word	m_c2b_sml		;1 C2B_SML    add little mbuf to pool
	.word	m_c2b_med		;2 C2B_MED    add medium mbuf to pool
	.word	m_c2b_big		;3 C2B_BIG    add cluster to pool
	.word	m_c2b_phy		;4 C2B_PHY_ACK finished PHY interrupt
	.word	m_c2b_wrap		;5 C2B_WRAP   back to start of buffer
	.word	m_c2b_idelay		;6 C2B_IDELAY delay interrupts
	.word	m_c2b_full		;7 C2B_LAST end of commands


;host command table
	.align
cmd_tbl:.word	cmd_done		;00	NOP
	.word	cmd_init		;01	set parameters
	.word	cmd_sto			;02	download to the board
	.word	cmd_exec		;03	execute downloaded code
	.word	cmd_fet			;04	upload from the board

	.word	cmd_rep			;05	repeat states & disarm PHY
	.word	cmd_setls0		;06	ELM0 transmitted line state
	.word	cmd_setls1		;07
	.word	cmd_pc_off0		;08	clear ELM PCM
	.word	cmd_pc_off1		;09
	.word	cmd_pc_start0		;0a	start ELM PCM
	.word	cmd_pc_start1		;0b
	.word	cmd_setvec0		;0c	signal some bits
	.word	cmd_setvec1		;0d
	.word	cmd_lct0		;0e	start LCT
	.word	cmd_lct1		;0f
	.word	cmd_pc_trace0		;10	start PC_TRACE
	.word	cmd_pc_trace1		;11
	.word	cmd_join0		;12	finish signalling
	.word	cmd_join1		;13
	.word	cmd_wrapa		;14
	.word	cmd_wrapb		;15
	.word	cmd_thru		;16

	.word	cmd_bypass_ins		;17	"insert" the bypass
	.word	cmd_bypass_rem		;18	turn off the bypass

	.word	cmd_mac_mode		;19	set MAC mode
	.word	cmd_cam			;1a	set FSI CAM
	.word	cmd_mac_parms		;1b	set MAC parameters
	.word	cmd_mac_off		;1c	turn off the MAC
	.word	cmd_d_bec		;1d	start directed beacon

	.word	cmd_pint_arm		;1e	arm PHY interrupts
	.word	cmd_wakeup		;1f	wake up C2B DMA

	.word	cmd_fet_ls0		;20	fetch line ELM0 state
	.word	cmd_fet_ls1		;21
	.word	cmd_fet_cnt		;22	fetch counters
cmd_tbl_end:



;initialization data for the FSI
 .macro	IFSIDAT, tgt,iri,dat
	.word	FSI_@tgt@ + ((iri)*FSI_IRI) + ((dat)*FSI_DATA)
 .endm
	.align
	.use	code
ifsi_tbl:
	IFSIDAT	PCRA,0,FSI_PCR_SO | FSI_PCR_PE
	IFSIDAT	PCRB,0,FSI_PCR_SO

	IFSIDAT	RPR, B_RING_N, B_RING_LEN_BT
	IFSIDAT	RPR,TX_RING_N,TX_RING_LEN_BT
	IFSIDAT	RPR,RX_RING_N,RX_RING_LEN_BT
	IFSIDAT	RPR,PM_RING_N,PM_RING_LEN_BT

    ;Watermarks:
    ;	For transmission, assume worst case bus latency of 10 usec:
    ;		64 reads by the 29K or 4 usec
    ;		64 reads or writes by the GIO DMA, 2.5 usec + 64*25/usec or
    ;		    5.0 usec
    ;	    and so a transmit watermark of >= 128*2 bytes
    ;	Section 3.3.6 says it does no good to make the receive watermark
    ;	    larger than the buffer size.  We want it large to minimize
    ;	    received fragments.  It must not be larger than legal.
    ;	    It need not be as large as a 4500 byte frame.

	IFSIDAT	FWR,RX_RING_N,1024/FSI_FWR_BLK
	IFSIDAT	FWR,TX_RING_N,1024/FSI_FWR_BLK
	IFSIDAT	FWR,PM_RING_N,FSI_FWR_MIN
	IFSIDAT	FWR, B_RING_N,FSI_FWR_MIN

	IFSIDAT	RMR,RX_RING_N,(MAX_FRAME_LEN+FSI_FWR_BLK-1)/FSI_FWR_BLK-1
	IFSIDAT	RMR,PM_RING_N,FSI_RMR_MIN

	IFSIDAT	RFR,RX_RING_N,FSI_RFR_DATA
	IFSIDAT	RFR,PM_RING_N,FSI_RFR_MAC

	IFSIDAT RBR,RX_RING_N,1<<(RX_BSIZE_BT-FSI_RBR_MIN_BT)
	IFSIDAT RBR,PM_RING_N,1<<(PM_BSIZE_BT-FSI_RBR_MIN_BT)

	IFSIDAT	MTR,0,1

	.equ	ifsi_tbl_len, (.-ifsi_tbl)/4
	.purgem	IFSIDAT
	.eject
;start running in SRAM here
	.use	code

;main loop
;   stay in this loop forever
	.use	code
	.align	16
mlop0:	STOFSI	fsi_imr,IMR,t1,t2	;turn on FSI interrupts
mlop01:	mtsrim	CPS,PS_INTSON
mlop:	jmpt	cur_st1,cmd		;host command
	 CK	ST1_MB == TRUE

    ;Do C2B control DMA if it has been a while.
	 mfsr	v1,TMC			;v1=current time
	cple	t1,v1,c2b_delay
	jmpt	t1,m_c2b

    ;start host PHY interrupt if one is pending and previous were ack'ed
	 t_st	t0,PINT_REQ
	andn	t0,t0,b2h_phy
	jmpt	t0,m_phy_int

    ;cause host interrupt
	 t_st	t0,NEEDI,
	jmpt	t0,m_inth


	 cpgt	v3,obf_avail,d2b_len	;v3=output ready & have enough space

    ;start the next output frame if we have been doing a lot of input.
	t_st	t0,TXDMA
	and	t0,t0,v3
	jmpt	t0,m_tx

    ;DMA a frame to the host if we have enough host buffers.
	 tbit	t0,fsi_imr,GSR_RX_RXC
	jmpt	t0,mlop2
	 or	t0,sml_bufs,med_bufs
	or	t0,t0,big_bufs
	jmpf	t0,m_rx
mlop2:

    ;notice output finished by the FSI and increase free buffer space
	 tbit	t0,fsi_imr,GSR_TX_RCC
	jmpf	t0,m_tx_fin

    ;start the next output frame
	 cpneq	t1,b2h_b_ptr,b2h_b_base
	jmpt	v3,m_tx

    ;Do B2H control DMA
	 const	t4,mlop
	jmpt	t1,dob2h
	 consth	t4,mlop

    ;if the FSI TX ring is idle, then look for something to do instead
    ;of waiting passively for a timer to expire.  The host might have
    ;new work but not interrupted us.  Maybe the FSI can be (re)started.

	 tbit	t0,fsi_imr,GSR_TX_RNR
	jmpt	t0,mlop7		;jump if output active
	 cplt	t1,free_tx_ring,TX_RING_NET	;tell the FSI to start output
	jmpt	t1,m_tx_st		;if anything waiting to start

	 srl	t1,c2b_dma_delay,3	;do extra C2B control DMA
	sub	t1,c2b_dma_delay,t1	;88% early
	sub	t1,v1,t1
	cple	t1,t1,c2b_delay
	jmpt	t1,m_c2b
    ;reset early token release since we know output is stopped
	 add	rel_tokb,rel_tokr,0
mlop7:

    ;restore FDDI input after drowning.
	tbit	t0,fsi_imr,GSR_RX_ROV
	jmpf	t0,m_rx_on

	 t_st	v2,PROM
	jmpf	v2,mlop8		;skip if not in promiscuous mode
	 lod	t0,max_bufs
	add	t1,sml_bufs,med_bufs
	add	t1,t1,big_bufs
	cpge	t2, t1,t0
	jmpf	t2,mlop8		;or if host has not noticed all input

	 tbit	t0,fsi_imr,GSR_PM_ROV
	jmpf	t0,m_pm_on		;restore MAC-promiscuous
	 tbit	t0,fsi_imr,GSR_PM_RXC
	jmpf	t0,m_pm			;process MAC-promiscuous input
mlop8:

    ;mess up the ring, if turned on
	 t_st	t0,ZAPRING
	jmpf	t0,mlop
	 t_st	t0,ZAPTIME
	sll	t1,v1,31-(20+MHZ_BT)+7	;about 128 times/sec
	xor	t2,t0,t1
	jmpf	t2,mlop

	 constx	t2,ST2_ZAPTIME
	mtsrim  CPS,PS_INTSOFF
	LODMAC	t0, CTRL_B
	constx	t1,MAC_CB_RESET_CLAIM
	or	t0,t0,t1
	STOMAC	t0, CTRL_B, t1
	jmp	mlop01
	 xor	cur_st2,cur_st2,t2



;interrupt the host for PHY events
m_phy_int:
	rs_st	PINT_REQ

 .macro	LS_INT,	n
	  cpeq	  t0,ls@n@_stack,0	;tell host of new line state
	  jmpt	  t0,$2
	   const0 v1,PCM_ULS

	  mtsrim  CPS,PS_INTSOFF
	  clz	  t0,ls@n@_stack
	  andn	  t0,t0,LS_SHIFT-1
	  CK	  LS_SHIFT == (LS_SHIFT & -LS_SHIFT)	;must be power of 2
	  sll	  ls@n@_stack,ls@n@_stack,t0
	  srl	  v1,ls@n@_stack,32-LS_SHIFT	;safely get oldest line state
	  sll	  ls@n@_stack,ls@n@_stack,LS_SHIFT
	  srl	  ls@n@_stack,ls@n@_stack,LS_SHIFT
	  srl	  ls@n@_stack,ls@n@_stack,t0	;and remove it from the FIFO
	  mtsrim  CPS,PS_INTSON

	  cplt	  t1,t0,LS_SHIFT
	  jmpf	  t1,$1
	   sub	  v1,v1,LS_DATA

	  add	  ls@n@_stack,ls@n@,LS_DATA	;line state change overrun
	  const	  v1,PCM_NLS

$1:	  cpneq	  t1,ls@n@_stack,0		;another int if FIFO not empty
	  c2set_st t1,PINT_REQ
$2:
 .endm


	LS_INT	0

	sll	v2,v1,8

	LS_INT	1

	t_st	t1,PINT_ARM
	jmpf	t1,mlop			;done if no interrupt armed

	 or	t0,v1,v2

	call	t4,addb2h		;send word to the host
	 consth	t0,B2H_PHY
	 CK	(B2H_PHY & 0xffff) == 0
	set_st	NEEDI
	jmp	mlop
	 sub	b2h_phy,b2h_phy, 1



;Cause a host interrupt
m_inth:
    ;first force pending b2h to ensure host has heard all we have to say
	cpneq	t1,b2h_b_ptr,b2h_b_base
	or	t1,t1,out_done
	callt	t1,t4,dob2h,,

    ;All of the GIO interrupts are shared in low-end systems, so the
    ;driver in the host must determine when its interrupt service routine
    ;is called whether the interrupt is from this board.  So the board DMAs
    ;an interrupt count to the host before the interrupt so the host can
    ;know the board caused the interrupt.

    ;Interrupts are not shared in Everest systems, but the DANG chip lets
    ;interrupts bypass DMA, which means the host might see the interrupt
    ;before the DMA has been completed.  To work around that problem,
    ;do an extra DMA before the interrupt.

    ;turn off interrupts to minimize race with graphics interrupts
    ;causing the host to prematurely field our interrupt
	mtsrim	CPS,PS_INTSOFF

 .ifdef MEZ
    ;do an extra DMA to flush the DANG cache
	t_st	t0,DANG_CACHE
	jmpf	t0,m_inth6
	 nop
	WAITDMA t0,fast
	DUMDMA	t0
m_inth6:
 .else
    ;tell the host to pay attention to our interrupt
	add	v0,p_host_cmd,hc_inum
	load	0,WORD,v1,v0
	add	v1,v1,1
	store	0,WORD,v1,v0
	STLDMA	4,B2H,v0,,host_cmd_buf,hc_inum,t0
 .endif

    ;zap the host, after the DMA is finished
	constx	v0,GSOR_HINT
	andn	cur_st2,cur_st2,ST2_NEEDI | ST2_OUTINT_REQ

	WAITDMA	t0

	jmp	mlop01
	 store	0,WORD,v0,p_gsor



;DMA control info from the host to the board
;   here v1=current time

;   Always copy a fixed number of words, relying on an empty sentinel
;   after the end of the real slots.
m_c2b:
	cpeq	c2b_delay,c2b_pos,0	;have things been initialized?
	jmpt	c2b_delay,mlop		;no, the host is not ready for us yet
	 CK	TIME_NEVER == TRUE

	 constx	v5,c2b			;v5=start of buffer

	constx	v1,C2B_SIZE		;compute size to fetch
 .ifdef MEZ
	CK	DANG_CACHE_SIZE<=C2B_SIZE   ;ending at end of cache line
	and	t0,c2b_pos,DANG_CACHE_SIZE-1
	sub	v1,v1,t0
 .endif

	STLDMA	v1,H2B,v5,,c2b_pos,,t0
	const	v2,0			;v2=accumulated new delay
	add	v6,v5,0			;v6=pointer into C2B board buffer
	add	v7,v5,v1		;v7=end of buffer

	consth	t0,C2B_LAST<<C2B_ADDR_HI_LSB_BT
	store	0,WORD,t0,v7		;install end-flag

	mfsr	c2b_delay,TMC
	sub	c2b_delay,c2b_delay,c2b_dma_delay  ;schedule next checkup

    ;acknowledge output or announce input instead of waiting for DMA
	cpneq	t1,b2h_b_ptr,b2h_b_base
	or	t1,t1,out_done
	const	t4,m_c2b_lop
	jmpt	t1,dob2h
	 consth	t4,m_c2b_lop

	WAITDMA	t0,fast

m_c2b_lop:
	 mtsrim	CR,2-1
	CK	_C2B_ADDR == 4
	CK	_C2B_OP == 0
	CK	&t1+1 == &t2
	loadm	0,WORD,t1,v6		;t1=ADDR_HI:OP, t2=ADDR

	add	v6,v6,_SIZE_C2B_BLK

	constx	t3,m_c2b_tbl
 .ifdef BIGADDR
	and	t4,t1,C2B_OP_MASK
	add	t3,t3,t4
 .else
	add	t3,t3,t1
 .endif
	load	0,WORD,t3,t3
    ;go to the function with t1==ADDR_HI:OP, t2=ADDR
	jmpi	t3
	 add	c2b_pos,c2b_pos,_SIZE_C2B_BLK


;save new small buffer
;   t1=ADDR_HI:OP, t2=ADDR
m_c2b_sml:
	SAVBUF	sml,t3

;save new medium buffer
;   t1=ADDR_HI:OP, t2=ADDR
m_c2b_med:
	SAVBUF	med,t3

;save new big buffer
;   t1=ADDR_HI:OP, t2=ADDR
m_c2b_big:
	SAVBUF	big,t3

;back to the start of the buffer
;   v2=accumulated new delay
m_c2b_wrap:
	constx	t1,c2b_base
	jmp	m_c2b_full1
	 load	0,WORD,c2b_pos,t1	;wrap to start of host buffer

;The host more than filled our buffer
;   v2=accumulated new delay
m_c2b_full:
	sub	c2b_pos,c2b_pos,_SIZE_C2B_BLK	;this opcode was not from host
m_c2b_full1:
	const	t3,0
	jmp	m_c2b_act
	 consth	c2b_delay,TIME_NOW	;read again at once

;finished PHY interrupt
m_c2b_phy:
	jmp	m_c2b_lop
	 add	b2h_phy,b2h_phy,1

;delay host interrupt
;   t2=delay in microseconds
m_c2b_idelay:
	sll	t2,t2,MHZ_BT
	jmp	m_c2b_lop
	 add	v2,v2,t2		;adjust v2 by host requested delay

;We have finished processing the requests read from the host.
;   If the host had nothing to say for a long time, stop doing host-to-board
;	DMA and tell the host.
;   If the host said more than fit in our buffer, then arrange to immediately
;	do another DMA.
;   If the host should have said something but did not, then interrupt it
;	and set the DMA delay to about the interrupt latency of the host.

;The host said something
;   v2=accumulated new delay, t3=TRUE to test D2B
m_c2b_act:
	lod	v4,max_host_work	;v4=limit on interrupt hold off

	rs_st	HINT_DONE,		;re-enable host interrupt
	mfsr	c2b_atime,TMC		;(to avoid needing to turn off ints)

	sub	v3,c2b_atime,host_qtime
	sra	t0,v3,31
	andn	v3,v3,t0
	add	v3,v3,v2		;v3=clamped ticks of host activity

	cple	t1,v4,v3
	sra	t1,t1,31
	and	v4,v4,t1
	andn	v3,v3,t1
	or	v3,v3,v4		;v3=min(v3, max interrupt hold of)

	sub	host_qtime,c2b_atime,v3    ;predict host activity
	jmpf	t3,mlop
	 sub	c2b_atime,c2b_atime,c2b_sleep	;keep C2B DMA active

	tbit	t0,d2b_len,D2B_BAD,	;if no waiting output,
	tbit	t1,fsi_imr,GSR_TX_RNR	;and if FSI is not running
	andn	t0,t0,t1
	callt	t0,t4,read_d2bs,mlop	;then see if the host has new output
	    ;as we go back to mlop


;The host did not fill the buffer
;   v2=accumulated new delay, v5=start of buffer, v6=last read
m_c2b_emp:
	sub	v6,v6,v5
	cpgt	t3,v6,_SIZE_C2B_BLK	;did the host say anything at all?
	jmpt	t3,m_c2b_act		;yes
	 sub	c2b_pos,c2b_pos,_SIZE_C2B_BLK	;will re-read empty slot

	tbit	t0,d2b_len,D2B_BAD,	;if no waiting output,
	tbit	t1,fsi_imr,GSR_TX_RNR	;and if FSI is not running
	andn	t0,t0,t1
	callt	t0,t4,read_d2bs,,	;then see if the host has new output


;host was mute
;   If we do not have the maximum number of buffers, then the host may
;   not have noticed the most recent input frames.  If it seems the
;   host is inactive and has not noticed the recent input, interrupt it.

	lod	t0,max_bufs
	add	t1,sml_bufs,med_bufs	;has host noticed all input?
	add	t1,t1,big_bufs
	cpge	t2,t1,t0
	jmpt	t2,m_c2b_emp4		;yes, forget the interrupt
	 mfsr	v2,TMC			;v2=current time

	cple	t0,v2,host_qtime
	jmpf	t0,m_c2b_emp5		;are we ready to interrupt the host?

	 t_st	t1,HINT_DONE		;yes, has the host heard the input?
	jmpt	t1,m_c2b_emp5

	 lod	t0,int_delay		;no, t0=interrupt hold-off
	sub	c2b_atime,v2,c2b_sleep	;keep C2B DMA active
	or	cur_st2,cur_st2,ST2_NEEDI | ST2_HINT_DONE   ;request interrupt
	jmp	mlop
	 sub	host_qtime,v2,t0	;no more interupts for a while


;The host has acknowledged all input.
;   v2=currrent time
m_c2b_emp4:
	rs_st	HINT_DONE,

;time to just wait
;   v2=currrent time
m_c2b_emp5:
	cplt	t0,c2b_atime,v2
	jmpt	t0,mlop			;wait for next C2B DMA if awake

;   If we are about to go to sleep, tell the host.  There must be another
;   C2B DMA, which will cause this code to check a final time to see if
;   the host said anything.

;   Host interrupts can be merged together.  This is a consequence of
;   the mechanism which postpones the interrupts.  If one interrupt
;   is postponed until after the host has given us new buffers for
;   all of the input officially completed but not after buffers that
;   have not been announced, a second interrupt can occur before
;   the last work is announced, this code has seen the host said nothing,
;   and left the interrupt marked done-but-not-acknowledged.
;   Meanwhile, the announcement B2H DMA can be made, the interrupt made
;   ready again, and this code will refuse to cause another host interrupt
;   on the grounds that the previous one has not been acknowledged.
;   Therefore, always do any possible interrupts before sleeping.

	 t_st	t1,C2B_SLEEP
	jmpt	t1,m_c2b_emp9
	 set_st	C2B_SLEEP

	call	t4,addb2h
	 consth	t0,B2H_SLEEP
	 CK	(B2H_SLEEP & 0xffff) == 0
	jmp	mlop
	 rs_st	HINT_DONE,		;re-enable host interrupt


;The host has been told of all input and that we are going to sleep.
;We have done a C2B DMA after announcing our sleepiness.
;The host appears to be asleep.
;Interrupt the host if it has not heard of its finished output.
m_c2b_emp9:
	mv_bt	t0,ST2_NEEDI, cur_st2,ST2_OUTINT_REQ
	and	t0,t0,ST2_NEEDI
	or	cur_st2,cur_st2,t0
	consth	c2b_atime,TIME_NEVER
	jmp	mlop
	 consth	c2b_delay,TIME_NEVER


;start reading the next output data request from the host
;   entry: t4=return address, t0,t1=scratch
;   exit:  t0,t1,t2=junk,  no other registers damaged
read_d2bs:
    ;wrap to start of host buffer at end of buffer
	cpltu	t0,d2b_pos,d2b_lim
	jmpf	t0,read_d2bs6

read_d2bs2:
	 STLDMA	D2B_SIZE,H2B,,d2b,d2b_pos,,t0
	jmpi	t4
	 const0	d2b_len,D2B_UNKNOWN

read_d2bs6:
	lod	d2b_pos,d2b_base
	jmp	read_d2bs2
	 nop



;put an entry into the B2H ring
;   entry: t0=word for the host,  t4=return
;   exit: t0-t3 changed
addb2h:
	add	b2h_sn,b2h_sn,1
	sll	t1,b2h_sn,24
	WAITDMA t3,fast,		;wait for previous B2H DMA to finish
	 or	t0,t0,t1
	store	0,WORD,t0,b2h_b_ptr
	add	b2h_b_ptr,b2h_b_ptr,_SIZE_B2H_BLK

	cpltu	t0,b2h_b_ptr,b2h_b_lim
	jmpti	t0,t4
	 nop
	jmp	dob2h_1			;do DMA now if wrapped the ring
	 nop


;do a B2H transfer
;   entry:  t4=return
;   exit: t0-t3 changed
dob2h:
	jmpf	out_done,dob2h_1

	 subr	t0,out_done,0		;tell host of completed output
	add	out_done,out_done,t0
	consth	t0,B2H_ODONE

	add	b2h_sn,b2h_sn,1
	sll	t1,b2h_sn,24
	WAITDMA t3,fast,		;wait for previous B2H DMA to finish
	 or	t0,t0,t1
	store	0,WORD,t0,b2h_b_ptr
	add	b2h_b_ptr,b2h_b_ptr,_SIZE_B2H_BLK
	set_st	OUTINT_REQ

dob2h_1:
	.ifdef MEZ
	t_st	t0,IO4IA_WAR		;work around IO4 IA chip bug if needed
	jmpf	t0,dob2h_3

	 const	t3,3
dob2h_2:sll	t1,t3,DANG_CACHE_SIZE_BT    ;write at b2h_h_lim+384
	add	t1,t1,b2h_h_lim		    ;	b2h_h_lim+256, b2h_h_lim+128,
	STLDMA	4,B2H,b2h_b_base,,t1,,t0,   ;	and b2h_h_lim
	cpgt	t1,t3,0
	jmpt	t1,dob2h_2
	 sub	t3,t3,1
dob2h_3:
 .endif
	sub	t3,b2h_b_ptr,b2h_b_base	;t3=DMA length
	STLDMA	t3,B2H,b2h_b_base,,b2h_h_ptr,,t0
 .ifdef MEZ
	set_st	DANG_CACHE		;remember to flush the DANG cache
 .endif
	add	b2h_h_ptr,b2h_h_ptr,t3	;advance pointer in host buffer

	add	b2h_b_ptr,b2h_b_base,0	;b2h_b_ptr=start of board buffer

	sub	t1,b2h_h_lim,b2h_h_ptr	;t1=bytes to end of host buffer
	cpge	t0,t1,BD_B2H_SIZE
	jmpti	t0,t4
	 add	b2h_b_lim,b2h_b_base,BD_B2H_SIZE

	cpneq	t0,t1,0
	jmpti	t0,t4
	 add	b2h_b_lim,b2h_b_base,t1

    ;wrap to start of host buffer at end
	lod	b2h_h_ptr,b2h_h_base
	jmpi	t4
	 add	b2h_b_lim,b2h_b_base,BD_B2H_SIZE


;Skim input frames, discarding wrong multi-cast or aborted frames
;   We know at least one complete frame is finished in the RX ring

;   XXX
;	This code assumes each frame occupies a single buffer.  This
;	tactic permanently wastes nearly 400 bytes in each input buffer
;	(4500 max FDDI frame - 4100 max typical), and it makes the board
;	less likely to keep up with high traffic in small frames.  However,
;	it is simpler this way.

m_rx:
	load	0,WORD,v0,h_rx_ring	;v0=1st word of 1st receive descriptor
	add	v3,h_rx_ring,FSI_RING_ADDR
	constx	v2,FSI_IND_RCV_LEN_MASK
	jmpt	v0,m_rx_fin		;quit if still owned by the FSI
	CK	FSI_CMD_OWN_BT==31

	 and	v2,v2,v0		;v2=frame length
	load	0,WORD,v3,v3		;v3=buffer address

	add	rel_tokb,rel_tokb,v2	;postpone early release if receiving

	set_st	TXDMA			;balance TX and RX DMA

    ;If previous frame len >4500, dump this frame.  We must also remember
    ;to zap the next if this is too long.
	cpeq	t0,v2,0
	srl	t0,t0,31-RX_BSIZE_BT
	or	v2,v2,t0		;assume 0 length is 8K

	add	t1,t_rx_ring,FSI_RING_ADDR
	store	0,WORD,v3,t1		;shift buffer to unused part of ring

	constx	t3,MAX_FRAME_LEN
	sub	t3,t3,v2		;truncate frame if too long

	t_st	t0,LONGERR		;was the previous frame too long?
	jmpt	t3,m_rx_long		;if this one is, then deal with it
	 rs_st	LONGERR
	jmpt	t0,m_rx_junk		;junk this if previous frame too long

m_rx10:	 tbit	t0,v0,FSI_IND_RCV_ERR
	jmpt	t0,m_rx_junk		;discard bad frames

	 srl	t0,v0,FSI_IND_FNUM_LSB_BT
	load	0,WORD,v7,v3		;v7=FC

	srl	v6,v0,FSI_IND_RCV_DA_LSB_BT - MAC_DA_BIT_BT
	CK	FSI_IND_RCV_DA_MSB_BT > MAC_DA_BIT_BT
	and	v6,v6,MAC_DA_BIT	;host DA bit=MLA or multicast

	and	t0,t0,FSI_IND_FNUM_MASK>>FSI_IND_FNUM_LSB_BT
	cplt	t0,t0,FSI_IND_FNUM_MIN>>FSI_IND_FNUM_LSB_BT
	srl	t0,t0,31-MAC_ERROR_BIT_BT
	or	v6,v6,t0		;fake error on missing A&C bits

    ;Skip bit counting if no interesting bits.  This assumes that most frames
    ;have no CRC errors and no E,A,or C flags set when received by our MAC.
    ;Do not count bits in frames received only because the MAC is in
    ;promiscuous mode.
	constx	t0,FSI_IND_RCV_CNT
	and	t0,t0,v0
	cpeq	t0,t0,0
	jmpt	t0,m_rx20

	 tbit	t0,v0,FSI_IND_RCV_E
	jmpf	t0,m_rx12
	 tbit	t0,v6,MAC_DA_BIT
	jmpf	t0,m_rx12
	 or	v6,v6,MAC_E_BIT
	CNTADR	t1,e
	load	0,WORD,t0,t1
	add	t0,t0,1			;count E flag bit
	store	0,WORD,t0,t1
m_rx12:

	tbit	t0,v0,FSI_IND_RCV_A
	jmpf	t0,m_rx14
	 tbit	t0,v6,MAC_DA_BIT
	jmpf	t0,m_rx14
	 or	v6,v6,MAC_A_BIT
	CNTADR	t1,a
	load	0,WORD,t0,t1
	add	t0,t0,1			;count A flag bit
	store	0,WORD,t0,t1
m_rx14:

	tbit	t0,v0,FSI_IND_RCV_C
	jmpf	t0,m_rx16
	 tbit	t0,v6,MAC_DA_BIT
	jmpf	t0,m_rx16
	 or	v6,v6,MAC_C_BIT
	CNTADR	t1,c
	load	0,WORD,t0,t1
	add	t0,t0,1			;count C flag bit
	store	0,WORD,t0,t1
m_rx16:

	tbit	t0,v0,FSI_IND_RCV_CRC
	jmpf	t0,m_rx20
	 tbit	t0,v6,MAC_DA_BIT
	jmpf	t0,m_rx14
	 or	v6,v6,MAC_ERROR_BIT
	CNTADR	t1,error
	load	0,WORD,t0,t1
	add	t0,t0,1			;count CRC errors
	store	0,WORD,t0,t1

m_rx20:
	sll	v6,v6,8
	or	v7,v7,v6		;combine flags with FC byte
	store	0,WORD,v7,v3		;assume no cheksum; put them in buffer

	cplt	t0,v2,MIN_FRAME_LEN
	jmpt	t0,m_rx_short		;junk frame if too small

	 add	v4,v3,4
	load	0,WORD,v4,v4		;v4=DA (v4=TRUE if multicast)

	t_st	t3,AMULTI
	and	t3,t3,v4		;t3=TRUE if multicast & all-multi set
	jmpt	t3,m_rx30		;do not check if want all multicasts

	 add	v5,v3,8
	load	0,HWORD,v5,v5		;v5=rest of the DA

	srl	v6,v4,16
	xor	v6,v6,v4
	xor	v6,v6,v5		;XOR 6 bytes of DA into 8 bits in v6
	srl	t1,v6,8
	xor	v6,v6,t1

	and	t1,v6,0xe0		;use 3 bits of hash to pick the word
	srl	t1,t1,5-2
	constx	t2,dahash
	add	t1,t1,t2
	load	0,WORD,t3,t1		;t3=word in the hash table

	sll	t3,t3,v6
	jmpf	t3,m_rx_junk		;junk frame if hash table says no


    ;We are committed to sending the frame to the host.
    ;here v2=total frame length, v3=buffer address, v7=FC and error bits,
m_rx30:
	 constx	v5,IN_PG_SIZE		;v5=page size

	set_st	GREEN			;turn on green LED
	rs_st	OUTINT_REQ		;host will hear of old output

;   Pick the size of the 1st mbuf.  Either as big as necessary
;   or only the overflow after making the 2nd mbuf 4KB for page flipping.
;   Except that if it will fit in two small buffers, do it that way.

;   Since the largest FDDI frame is < 2*4KB, the 2nd chunk will be 4KB only
;   if we intend to checksum on the board.

	sub	v4,v2,v5
	cpge	t0,v4,IN_SUM_OFF	;checksum only if big enough

	and	t1,v2,3			;checksum only an even # of words
	cpeq	t1,t1,0
	and	t0,t0,t1
	t_st	t1,IN_CKSUM
	and	t0,t0,t1		;checksum only if allowed

	sra	t0,t0,31
	and	v4,t0,v5		;v4=0 or 4KB if expect to page flip
	sub	v4,v2,v4		;v4=most we want in 1st mbuf

;   If the frame will not fit in two little mbufs, then try to use a little
;	mbuf for the first part.
;   If the frame will fit in two little mbufs, than use a little mbuf
;	for the first part.

	;   (Do not adjust this one, since it does matter much and the
	;   adjustment could overflow.)
	cple	t0,v2,SML_NET+SML_SIZE0 ;t0=frame will fit in 2 mbufs
	FIX_SML_NET
	cple	t1,v4,SML_NET		;t1=1st part fits in small mbuf
	or	t2,t1,t0
	jmpf	t2, m_rx43
	 SPAD	MED_SIZE-PAD0_IN_DMA
	 const0	t0,MED_SIZE-PAD0_IN_DMA

    ;use at least one small mbuf
	FIX_SML_NET
	cpgt	t0,v4,SML_NET
	sra	t0,t0,31
	andn	v4,v4,t0
	FIX_SML_NET
	and	t0,t0,SML_NET
	or	v4,v4,t0		;v4=min(SML_NET, data size)

	USEBUF	sml,addr_hi0,v0,m_rx45

    ;Need more than small mbuf first.  t0=space in medium, initial mbuf
m_rx43:	cple	t0,v4,t0		    ;t0=true if medium large enough
	jmpf	t0, m_rx44
	 SPAD	PAD0_IN_DMA
	 const0	t0,IN_PG_SIZE-PAD0_IN_DMA   ;t0=space in big, initial mbuf

	USEBUF	med,addr_hi0,v0,m_rx45,	;it fits in a medium mbuf

    ;Need at least one big mbuf.  t0=space in big, initial mbuf
m_rx44:	cpge	t1,t0,v4		;limit 1st chunk to space in a page
	sra	t1,t1,31
	andn	t0,t0,t1
	and	v4,v4,t1
	or	v4,v4,t0		;v4=min(big net size, data size)

	USEBUF	big,addr_hi0,v0,,

m_rx45:	sub	v5,v2,v4		;v5=remaining length
	cple	v6,v5,0

    ;v0=1st mbuf address, addr_hi0=EBus address bits
    ;v2=total frame length, v3=start of board buffer,
    ;v4=size of first chunk,  v5=remaining length,
    ;v6=true if 1 mbuf is enough,  v7=FC and error bits,

	jmpt	v6,m_rx70		;go DMA 1st mbuf if need only 1

    ;We need a 2nd buffer, pick a size
	 ASML	SML_SIZE0
	 cple	v1,v5,SML_SIZE0
	jmpf	v1, m_rx53
	 nop

	USEBUF	sml,addr_hi1,v1,m_rx60

m_rx53:	constx	v1,MED_SIZE		;(no padding in 2nd mbuf)
	cple	v1,v5,v1
	jmpf	v1,m_rx54
	 nop

	USEBUF	med,addr_hi1,v1,m_rx60,	;pick correct buffer size for 2nd part

m_rx54:	USEBUF	big,addr_hi1,v1,,


; compute the TCP or UDP checksum of the last 4KB of large packets containing
;   an even numbers of words.  Small frames will be byte-copied by the host,
;   which can checksum faster as long as it does not waste the data
;   cache misses.

m_rx60:
    ;v0=1st mbuf address, addr_hi0=EBus address bits
    ;v1=2nd mbuf address, addr_hi1=EBus address bits
    ;v2=total frame length, v3=start of board buffer,
    ;v4=1st mbuf size,  v5=2nd mbuf size,
    ;v6=true if 1 mbuf is enough,  v7=FC and error bits,

	add	v10,v3,v4		;v10=board DMA address for 2nd buffer
	constx	t0,IN_PG_SIZE
	cpeq	t0,t0,v5
	jmpf	t0,m_rx68		;no checksum if not a page

	 constx	v8,N_SUMREG*4		;v8=max DMA size
	mov	v9,v5			;v9=bytes remaining to process
	const	v12,0			;v12=checksum

m_rx62:
	cple	t0,v9,v8
	sra	t0,t0,31
	and	t1,v9,t0
	andn	t0,v8,t0
	or	t1,t1,t0		;t1=min(max DMA, remaining bytes)

	add	t2,v10,blur_plen
	andn	t2,t2,blur_pmask
	sub	t2,t2,v10		;t2=bytes to end of DMA board page

    ;Loop here to complete the 4KB checksummable mbuf.
    ;	The DMA machinery will usually be quiet by now.  Moving the test
    ;	here prevents pipeline stalls waiting for the BLUR.
m_rx63:
	load	0,WORD,t3,p_blur

	cple	t0,t1,t2		;start to compute min(t1,t2)
	sra	t0,t0,31
	and	t1,t1,t0
	andn	t0,t2,t0

	TDMA	t3
	jmpf	t3,m_rx63		;wait until previous DMA is finished
	 or	t1,t1,t0		;t1=min(t1, bytes to end of page)

 .ifdef DANG_BUG
    ;If the previous transfer was a write, then we must do a dummy read
    ;before the DMA write that happens after we checksum the data.
	t_st	t3,DANG_DMA
	jmpf	t3, m_rx64
	 DUMDMA t3
	WAITDMA t3,fast
m_rx64:  set_st	DANG_DMA		;worry about the DANG bug next time.
 .endif
	consth	t3,CREG_B2H
	or	cregs,cregs,t3
	store	0,WORD,cregs,p_creg

	srl	t2,t1,2
	sub	t2,t2,1

    ;We do not need to round the byte count up to an even number of words,
    ;becase we know the total mbuf size is 4096 and each chunk we DMA
    ;is an integral number of words.

	store	0,WORD,t1,p_blur	;send the byte count to the hardware

	constx	t0,(N_SUMREG+&sumreg)*4
	sub	t0,t0,t1
	mtsr	IPA,t0
	mtsr	CR,t2
	sub	v9,v9,t1		;reduce bytes remaining
	loadm	0,WORD,gr0,v10		;fetch bytes for checksum we will DMA

 .ifdef BIGADDR
	or	t0,addr_hi1,v10
	store	0,WORD,t0,p_crap
 .else
	store	0,WORD,v10,p_crap
 .endif
	add	v10,v10,t1

	const	t0,summer+N_SUMREG*4
	consth	t0,summer+N_SUMREG*4
	sub	t0,t0,t1

	store	0,WORD,v1,p_vsar	;start the DMA

	add	v1,v1,t1		;advance address and reset carry flag
	calli	t4,t0			;sum the bytes

	 cpgt	t0,v9,0
	jmpt	t0,m_rx62		;loop if not finished


	 srl	t0,v12,16
	consth	v12,0xffff0000
	add	v12,v12,t0
	addc	v12,v12,0		;fold checksum into 16 bits
	sll	v12,v12,16		;into top 16 bits

	cpeq	t0, v12,0
	sra	t0,t0,15
	xor	v12, v12,t0		;convert 0 to minus-0
	or	v7, v7,v12		;combine cksum with bits & FC
	jmp	m_rx70
	 store	0,WORD,v7,v3		;install checksum, bits & FC in buffer


;Like the previous code, DMA 2nd mbuf, but unlike the preceding do not
;	compute the checksum.
;Note that the 2nd mbuf is DMA'ed 1st.
    ;v0=1st mbuf address, addr_hi0=EBus address bits
    ;v1=2nd mbuf address, addr_hi1=EBus address bits
    ;v2=total frame length, v3=start of board buffer,
    ;v4=1st mbuf size,  v5=2nd mbuf size,
    ;v6=true if 1 mbuf is enough,
    ;v10=board DMA address for 2nd part

m_rx68:
	add	v9,v5,3
	andn	v9,v9,3			;v9=2nd mbuf size rounded up to words
	call	t4,dmasub
	 consth	v11,CREG_B2H
	jmpt	t0,dmasub		;loop until finished
	 nop


;DMA 1st mbuf.  Note that the 1st mbuf is DMA'd 2nd.
    ;v0=1st mbuf address, addr_hi0=EBus address bits
    ;v2=total frame length, v3=start of board buffer,
    ;v4=1st mbuf size,  v5=2nd mbuf size,
    ;v6=true if 1 mbuf is enough,
m_rx70:
	APAD	PAD0_IN_DMA
	add	v1,v0,PAD0_IN_DMA	;v1=host address after snoop header
 .ifdef BIGADDR
	mov	addr_hi1,addr_hi0	;addr_hi1=EBus address bits
 .endif
	add	v9,v4,3
	andn	v9,v9,3			;v9=1st mbuf size
	mov	v10,v3
	call	t4,dmasub
	 consth	v11,CREG_B2H
	jmpt	t0,dmasub		;loop until finished
	 nop

	jmpt	v6,m_rx75
	 APAD	PAD0_IN_DMA
	 add	t0,v4,PAD0_IN_DMA

	call	t4,addb2h
	 consth	t0,B2H_IN		;tell host about 1st of 2 mbufs
	 CK	(B2H_IN_DONE & 0xffff) == 0

	add	t0,v5,0
m_rx75:	call	t4,addb2h		;tell the host about the last mbuf
	 consth	t0,B2H_IN_DONE
	 CK	(B2H_IN & 0xffff) == 0

	rs_st	C2B_SLEEP,,		;host will think we are awake
	mfsr	c2b_atime,TMC
	cplt	t0,host_qtime,c2b_atime	;is the host awake?
	jmpt	t0,m_rx99
	 sub	c2b_atime,c2b_atime,c2b_sleep	;keep C2B DMA going
	consth	c2b_delay,TIME_NOW	;no, immediate C2B DMA

    ;fix FSI descriptor and quit
m_rx99:
	STOFSI	GSR_RX_RXC,GSR,t2,t3	;prevent unneeded interrupt

	constx	t2,FSI_CMD_OWN
	store	0,WORD,t2,t_rx_ring
	mov	t_rx_ring,h_rx_ring
	ADVRP	h_rx_ring, rx, mlop	;advance ring and back to main loop


;stop worrying about input when no more frames are ready
m_rx_fin:
	constx	t0,GSR_RX_RXC		;allow more FSI RX interrupts
	jmp	mlop0
	 or	fsi_imr,fsi_imr,t0


;count an impossibly short frame
;   v0=receive indication
m_rx_short:
	INCCNT	shorterr,1,,t2,t3
	jmp	m_rx_junk
	 nop

;do something about an excessively long frame
;   v0=receive indication
;   t0=TRUE if previous frame was too long, t3=-(excess length), v2=length
m_rx_long:
	add	v2,v2,t3		;fix the length
	INCCNT	longerr,1,,t2,t3
	set_st	LONGERR
	constx	t3,FSI_IND_RCV_CRC
	jmpf	t0,m_rx10
	 or	v0,v0,t3		;set an error bit for the host

;forget junk frames
;   v0=indication bits
m_rx_junk:
	INCCNT	tot_junk,1,,t0,t1	;count junked frame

	BITCNT	rx_ovf,FSI_IND_RCV_OE,v0,,t0,t1

	jmp	m_rx99
	 nop


;Start DMA'ing the next output frame from the host to the board.
;   Here we know there seems to be enough buffer space.
m_tx:
    ;Do we know what the next slot in the data-to-board queue looks like?
    ;If not, then wait for the recently started DMA of it to be finished.
	cpgt	t3,d2b_len,D2B_UNKNOWN
	const	v3,d2b+_D2B_ADDR	;v3=address of 2nd word of request
	jmpt	t3,m_tx10
	 consth	v3,d2b+_D2B_ADDR

	WAITDMA	t0,fast

	 sub	d2b_len, v3, _D2B_ADDR-_D2B_OP
	load	0,WORD,d2b_len,d2b_len	;fetch 1st word of request
	consth	t3,D2B_RDY
	CK	(D2B_RDY & 0xffff) == 0
	andn	d2b_len,d2b_len,t3	;leave d2b_len large if not ready
	cpgt	t0,obf_avail,d2b_len
	jmpf	t0,mlop			;give up if not enough room
	 nop

m_tx10:
	load	0,WORD,v12,v3		;v12=offsum:startsum:cksum

	rs_st	TXDMA			;balance TX and RX DMA
	sub	rel_tokb,rel_tokb,d2b_len   ;notice need for early release
	set_st	GREEN			;turn on green LED

	add	d2b_pos,d2b_pos,_SIZE_D2B_BLK

	add	v3,v3,_SIZE_D2B_BLK-_D2B_ADDR	;v3=host (address,len) pairs
	load	0,WORD,v9,v3		;v9=1st host buffer len & EBus addr
 .ifdef MEZ
	rs_st	DANG_CACHE		;reads force the DANG cache
 .endif
 .ifdef DANG_BUG
    ;do not need to worry about bad DANGs after DMA reads
	rs_st	DANG_DMA
 .endif
	mov	v10,obf			;v10=initial board address for DMA
	add	t0,t_tx_ring,FSI_RING_ADDR
	store	0,WORD,v10,t0		;begin TX command with start of buffer

	add	t1,d2b_len,BLUR_ROUND
	andn	t1,t1,BLUR_ROUND	;t1=total words needed for the frame

	add	obf,obf,t1		;allocate the buffer
	cpgt	t0,obf,p_obfend
	jmpf	t0,m_tx15
	 sub	obf_avail,obf_avail,t1
	constx	obf,tx_bufs,		;wrap to start after last buffer

m_tx15:
	cpeq	t0,v12,1		;offsum:startsum:cksum == 1?
	jmpf	t0,m_tx40		;no, checksum the frame
	 add	v1,v3,_D2B_ADDR

					;do not checksum the frame
;   d2b_len=byte count
;   v1=address of next host buffer address
;   v3=pointer into list of host (address,len) pairs
;   v9=host buffer length and EBus address bits
;   v10=board address
m_tx20:
	load	0,WORD,v1,v1		;v1=next host buffer address
 .ifdef BIGADDR
	srl	addr_hi1,v9,D2B_ADDR_HI_LSB_BT
	sll	addr_hi1,addr_hi1,CRAP_EB_LSB_BT
	consth	v9,0
	CK	D2B_LEN_MSB_BT==15
 .endif
	add	v3,v3,_SIZE_D2B_BLK	;advance for next time
	add	d2b_pos,d2b_pos,_SIZE_D2B_BLK

;   d2b_len=byte count
;   v1=pointer to current host buffer address,
;   v3=pointer into list of host (address,len) pairs,
;   v5=next word to checksum
;   v9=host buffer length, addr_hi1=EBus address bits
;   v10=board address
;   v11=0
	call	t4,dmasub
	 const	v11,0			;v11=DMA read for dmasub()
	jmpt	t0,dmasub		;loop until finished with buffer

	 add	v0,d2b_len,0
	load	0,WORD,v9,v3		;v9=next host buf length & EBus addr
	consth	v0,FSI_CMD_OWN | FSI_CMD_F | FSI_CMD_L
	CK	((FSI_CMD_OWN | FSI_CMD_F | FSI_CMD_L) & 0xffff) == 0
	jmpf	v9,m_tx20		;quit at the end of string of buffers
	 CK	D2B_RDY_BT==31
	 add	v1,v3,_D2B_ADDR

    ;After the last address and length pair,
    ;if there is another job waiting, start fetching it.
	 tbit	t0,v9,D2B_BAD
	srl	d2b_len,t0,31-D2B_BAD_BT    ;leave d2b_len large if no job
	const	t4,m_tx30
	jmpf	t0,read_d2bs
	 consth	t4,m_tx30
	WAITDMA	t0,,			;let DMA finish before the FSI starts


;Here the DMA has been completed, and the checksum computed, if requestd.

;   If there will be no available ring entries (i.e. only 1 not in use),
;   then there will be no free space after we install this entry.
;   v0=rest of descriptor for the FSI
m_tx30:	cple	t1,free_tx_ring,1
m_tx31:	or	obf_avail,obf_avail,t1
	sub	out_done,out_done,1	;as far as the host cares, we are done

;   Release the token if we have done a lot of output but the ring
;   seems to be keeping up.  We want to give the other machine a chance
;   to send us a TCP ACK.

;   If we have a lot of pending output, then do not release the token
;   in the hope that the ring is slow or big or we are doing UDP instead
;   of TCP.

;   One problem is that we must install the token-release bit long before
;   the FSI is ready to transmit the frame, because the FSI buffers
;   commands ahead.  It is a little early, but convenient and most clearly
;   safe to install the release bit as the frame is DMAed from the host.
;   At least we have delayed until after the host DMA is finished.


	jmpf	rel_tokb,m_tx35		;no release if few bytes transmitted

	 constx	t0,MAX_OBF_AVAIL	;no release if a lot buffered
	sub	t0,t0,obf_avail		;	t0=bytes waiting
	add	t1,rel_tokr,rel_tokr	;	t1=release rate*2
	cpge	t0,t0,t1
	jmpt	t0,m_tx35

	 add	t0,t_tx_ring,FSI_RING_ADDR
	load	0,WORD,t0,t0		;find address of start of buffer
	add	rel_tokb,rel_tokr,0
	load	0,WORD,t1,t0		;install the bit in the 1st word
	constx	t2,PRH_SEND_LAST
	or	t1,t1,t2
	store	0,WORD,t1,t0

	CNTADR	t1,early_rels
	load	0,WORD,t0,t1
	add	t0,t0,1
	store	0,WORD,t0,t1
m_tx35:

	store	0,WORD,v0,t_tx_ring	;install TX command
	ADVRP	t_tx_ring,tx,

;   start the FSI if it was not running
	tbit	v0,fsi_imr,GSR_TX_RNR
	jmpt	v0,mlop
	 sub	free_tx_ring,free_tx_ring,1
	mtsrim	CPS,PS_INTSOFF
	STO_IR	RDY,TX_RING_N,0,t0,
	jmp	mlop0
	 or	fsi_imr,fsi_imr,GSR_TX_RNR



;start the FSI
;   If the FSI appears to have forgotten to turn off an OWN bit, then
;   force it.
;   We come here only when the FSI was not successfully started
;   by the normal output code.
m_tx_st:
	mtsrim	CPS,PS_INTSOFF

	STO_IR	RDY,TX_RING_N,0,t0,

	cplt	t1,free_tx_ring, TX_RING_NET-1
	jmpf	t1,mlop0		;more than 1 frame unfinished?
	 or	fsi_imr,fsi_imr,GSR_TX_RNR

	ADVRP2	v1, h_tx_ring,FSI_RING_LEN,tx
	load	0,WORD,v1,v1		;and 2nd frame finished by FSI?
	jmpt	v1,mlop0
	 CK	FSI_CMD_OWN == 0x80000000
	 nop

	load	0,WORD,v0,h_tx_ring	;but next frame unfinished?
	jmpf	v0,mlop0

	 sll	v0,v0,1
	srl	v0,v0,1
	store	0,WORD,v0,h_tx_ring	;yes, reset the next OWN bit

	mtsrim	CPS,PS_INTSOFF
	CNTADR	t1,fsi_bug
	load	0,HWORD,t2,t1		;tell the host, using different
	add	t2,t2,1			;format for fsi_bug
	store	0,WORD,t2,t1

	jmp	m_tx_fin4		;go bang on the stuck FSI
	 mtsrim	CPS,PS_INTSON


;Compute the checksum on the rest of this buffer.
;   d2b_len=byte count
;   v1=address of next host buffer address
;   v3=pointer into list of host (address,len) pairs
;   v9=host buffer length and EBus address bits
;   v10=board address

m_tx40:
    ;disassemble v12=offsum:startsum:cksum
	mtsrim	BP,1
	exbyte	v5,v12,0
	add	v5,v5,v10		;v5=next word to checksum
	srl	v4,v12,24
	add	v4,v4,v10		;v4=address of the checksum
	constx	v8,N_SUMREG*4		;v8=max DMA size
	consth	v12,0			;v12=growing checksum

;   d2b_len=byte count
;   v1=next host buffer address
;   v2=checksum word count - 1
;   v3=pointer into list of host (address,len) pairs,
;   v4=address of the checksum
;   v5=next word to checksum,
;   v6=bytes in whole words to checksum this time,
;   v7=bytes to checksum this time,
;   v8=max DMA size
;   v9=host buffer length and EBus address bits
;   v10=board address
;   v12=growing checksum

m_tx41:
	load	0,WORD,v1,v1		;v1=next host buffer address
	add	v3,v3,_SIZE_D2B_BLK	;advance for next time
	add	d2b_pos,d2b_pos,_SIZE_D2B_BLK
 .ifdef BIGADDR
	srl	addr_hi1,v9,D2B_ADDR_HI_LSB_BT
	sll	addr_hi1,addr_hi1,CRAP_EB_LSB_BT
	consth	v9,0
	CK	D2B_LEN_MSB_BT==15
 .endif


;   The previous DMA will usually have finished while we were checksumming
;   the bytes from the one before that.
m_tx43:
	load	0,WORD,t3,p_blur

	sub	v7,v10,v5		;v7=bytes to checksum
	andn	v6,v7,3			;v6=bytes in whole words to checksum
	sra	v2,v7,2
	sub	v2,v2,1

	TDMA	t3
	jmpf	t3,m_tx43		;wait until previous DMA is finished

	 constx	t1,(N_SUMREG+&sumreg)*4

	jmpt	v2,m_tx44		;do not checksum MAC header (v6 <= 3)
	 sub	t1,t1,v6
	mtsr	IPA,t1
	mtsr	CR,v2
	loadm	0,WORD,gr0,v5		;fetch bytes to checksum
	add	v5,v5,v6		;v5=next block to checksum
m_tx44:

    ;start the next DMA
	consth	t0,CREG_B2H
	andn	cregs,cregs,t0
	store	0,WORD,cregs,p_creg

	cple	t0,v9,v8
	sra	t0,t0,31
	and	t1,v9,t0
	andn	t0,v8,t0
	or	t1,t1,t0		;t1=min(v9=remaining bytes,v8=max DMA)

	add	t2,v10,blur_plen
	andn	t2,t2,blur_pmask
	sub	t2,t2,v10		;t2=bytes to end of DMA board page
 .ifdef BIGADDR
	or	t3,addr_hi1,v10
	store	0,WORD,t3,p_crap
 .else
	store	0,WORD,v10,p_crap
 .endif

	cple	t0,t1,t2
	sra	t0,t0,31
	and	t1,t1,t0
	andn	t0,t2,t0
	or	t1,t1,t0		;t1=min(t1, t2)

	add	t0,t1,BLUR_ROUND	;round up to even number of words
	andn	t0,t0,BLUR_ROUND
	store	0,WORD,t0,p_blur	;send the byte count to the hardware

	add	v10,v10,t1		;v10=next board address to DMA
	sub	v9,v9,t1		;v9=reduce remaining length to DMA

	store	0,WORD,v1,p_vsar	;start the DMA

;   checksum previous block
	const	t2,summer+N_SUMREG*4
	consth	t2,summer+N_SUMREG*4
	sub	t2,t2,v6
	jmpt	v2,m_tx46		;do not checksum MAC header
	 add	v1,v1,t1		;clear carry & v1=next host address
	calli	t4,t2

m_tx46:
	 cpgt	v0,v9,0
	jmpt	v0,m_tx43		;do more of this host buffer
	 nop

	load	0,WORD,v9,v3		;v9=next host buffer len & EBus bits
	add	v1,v3,_D2B_ADDR
	jmpf	v9,m_tx41		;next host buffer if not finished

;   checksum last block DMA'ed
m_tx47:
	 constx	t1,(N_SUMREG+&sumreg)*4

	load	0,WORD,t3,p_blur

	sub	v7,v10,v5		;v7=bytes to checksum
	andn	v6,v7,3			;v6=bytes in whole words to checksum
	sra	v2,v7,2
	sub	v2,v2,1

	TDMA	t3
	jmpf	t3,m_tx47		;wait until previous DMA is finished

	 sub	t1,t1,v6
	jmpt	v2,m_tx48
	 mtsr	IPA,t1
	mtsr	CR,v2
	loadm	0,WORD,gr0,v5		;fetch bytes to checksum

	const	t2,summer+N_SUMREG*4
	consth	t2,summer+N_SUMREG*4
	sub	t2,t2,v6
	calli	t4,t2
m_tx48:	 add	v5,v5,v6		;v5=address of odd bytes & clear carry

;   add any trailing bytes to the checksum
	cpeq	t0,v6,v7
	jmpt	t0,m_tx49
	 and	t1,v7,3

	load	0,WORD,t0,v5
	subr	t1,t1,4
	sll	t1,t1,3
	srl	t0,t0,t1
	sll	t0,t0,t1		;remove stray bytes after odd ones
	add	v12,v12,t0
	addc	v12,v12,0

;   Fold and store the checksum.
m_tx49:
	srl	t0,v12,16
	consth	v12,0xffff0000
	add	v12,v12,t0
	addc	v12,v12,0		;fold into 16 bits
	nor	v12,v12,0		;1's complement
	consth	v12,0
	cpeq	t0,v12,0
	sra	t0,t0,31
	or	v12,v12,t0		;never +0 for the sake of UDP
	store	0,HWORD,v12,v4

	sll	v0,d2b_len,0		;make rest of FSI descriptor
	consth	v0,FSI_CMD_OWN | FSI_CMD_F | FSI_CMD_L
	CK	((FSI_CMD_OWN | FSI_CMD_F | FSI_CMD_L) & 0xffff) == 0

	sll	t0,v9,31-D2B_BAD_BT	;if there is another job waiting,
	srl	d2b_len,t0,31-D2B_BAD_BT
	const	t4,m_tx30
	jmpf	t0,read_d2bs		;start fetching it
	 consth	t4,m_tx30

;   start the FSI
	jmp	m_tx31
	 cple	t1,free_tx_ring,1



;finish output after the FSI is done
;   This function exits to mlop0 to set the FSI interrupt mask register.
m_tx_fin:
	load	0,WORD,v0,h_tx_ring
	cplt	t0,free_tx_ring,TX_RING_NET
	jmpf	t0,m_tx_fin9		;stop when all finished

;   jump here (ugh) to get the FSI running again after it messes up.
;   V0=next indication
m_tx_fin4:
	 consth	t0,FSI_IND_TX_BAD
	 CK	(FSI_IND_TX_BAD & 0xffff) == 0

	jmpt	v0,m_tx_fin9		;stop on unfinished frame
	 CK	FSI_CMD_OWN == TRUE

	 and	t0,t0,v0
	cpneq	v1,t0,0			;count TX problems
	jmpf	v1,m_tx_fin5

	 BITCNT	xmtabt,FSI_IND_TX_AB,v0,,t0,t1
	BITCNT	tx_under,FSI_IND_TX_UN,v0,,t0,t1

m_tx_fin5:
	add	free_tx_ring,free_tx_ring,1
	ADVRP	h_tx_ring,tx

	consth	v0,0
	CK	FSI_CMD_TX_LEN_MASK == 0xffff
	add	v0,v0,BLUR_ROUND
	andn	v0,v0,BLUR_ROUND	;v0=length of finished frame
	add	obf_avail,obf_avail,v0

	STOFSI	GSR_TX_RCC,GSR,t2,t3	;prevent unneeded interrupt

	sll	obf_avail,obf_avail,1	;there is at least 1 ring entry
	jmp	m_tx_fin
	 sra	obf_avail,obf_avail,1


m_tx_fin9:
    ;when <=1 frame remains for the FSI, check the host for new host output
	cpge	t0,free_tx_ring,TX_RING_NET-1
	tbit	t1,d2b_len,D2B_BAD
	and	t0,t0,t1
	callt	t0,t4,read_d2bs

	constx	t0,GSR_TX_RCC
	jmp	mlop0
	 or	fsi_imr,fsi_imr,t0


;sum some bytes, usually by calling into the middle of this code
;   entry: t4=return,  v12=previous checksum,  carry flag off
;   exit: v12 increased
summer:
	.set	sreg,&sumreg
	.rep	N_SUMREG
	  addc	  v12,v12,%%sreg
	  .set	  sreg,sreg+1
	.endr
	jmpi	t4
	 addc	v12,v12,0


;start DMA a chunk of a buffer
;   entry: t4=return,
;	v1=host address
;	v9=total byte count
;	v10=board address
;	addr_hi1=EBus address bits
;	v11=0 or CREG_B2H
;   exit: v1,v9,v10=advanced,  t0=TRUE if not finished,  t1=bytes moved,
;	t2,t3=garbage
dmasub:
	CK	BLUR_MAX_C >= DMA_ALIGN

dmasub3:
	load	0,WORD,t3,p_blur	;start to wait for previous DMA

	add	t2,v10,blur_plen
	andn	t2,t2,blur_pmask
	sub	t2,t2,v10		;t2=bytes to end of DMA board page

	TDMA	t3
	jmpf	t3,dmasub3		;wait until previous DMA is finished

 .ifdef DANG_BUG
    ;If the previous transfer was a write or if this one is,
    ;then work around the DANG bug.
	 t_st	t0,DANG_DMA		;t0=previous transfer was a write
	cpneq	t1,v11,0		;t1=this is a write
	jmpf	t1,dmasub5		;forget it all if this is a read
	 rs_st	DANG_DMA
	jmpf	t0,dmasub4		;no dummy if previous was a read
	 DUMDMA t0
	WAITDMA	t0,fast
dmasub4: set_st	DANG_DMA
dmasub5:
 .endif
	 consth	t1,CREG_B2H
	andn	cregs,cregs,t1
	or	cregs,cregs,v11
	store	0,WORD,cregs,p_creg

	cple	t0,v9,t2
	sra	t0,t0,31
	and	t1,v9,t0
	andn	t0,t2,t0

 .ifdef BIGADDR
	or	t3,addr_hi1,v10
	store	0,WORD,t3,p_crap
 .else
	store	0,WORD,v10,p_crap
 .endif

	or	t1,t1,t0		;t1=min(v9, bytes to end of page)
	add	v10,v10,t1

	add	t0,t1,BLUR_ROUND	;round up to even number of words
	andn	t0,t0,BLUR_ROUND
	store	0,WORD,t0,p_blur

	sub	v9,v9,t1
	cpgt	t0,v9,0			;see if this will finish it

	store	0,WORD,v1,p_vsar	;start the DMA
	jmpi	t4
	 add	v1,v1,t1


;handle promiscuous MAC input
m_pm:
    ;stop new input while we deal with this batch
	STO_IR	MRR,0,FSI_MRR_RPE|FSI_MRR_ROB | FSI_MRR_RX_RE,t0

m_pm10:
	mtsrim	CR,PM_FILTER_LEN-1
	constx	t4,pm_filter
	loadm	0,WORD,sumreg,t4	;sumreg0,1,2,3=previous FC,DA,&SA

m_pm15:
	load	0,WORD,v0,h_pm_ring	;v0=receive indication

	constx	t1,FSI_IND_RCV_LEN_MASK
	add	v3,h_pm_ring,FSI_RING_ADDR
	jmpt	v0,m_pm_fin		;back to work if false alarm

	 and	t1,t1,v0
	cpgt	t0,t1,PM_BSIZE
	load	0,WORD,v3,v3		;v3=buffer address

	sra	t0,t0,31
	and	v2,t0,PM_BSIZE
	andn	v2,t1,t0		;v2=frame length limited to buf size

	cplt	t0,v2,MIN_FRAME_LEN
	jmpt	t0,m_pm_junk		;junk frame if too small
	 tbit	t0,v0,FSI_IND_RCV_ERR
	jmpt	t0,m_pm_junk		;discard badly received frames

	 mtsrim	CR,PM_FILTER_LEN-1
	loadm	0,WORD,v7,v3		;v7=FC, v8,9,10=DA,SA
	CK	&v7+1==&v8 && &v8+1==&v9 && &v9+1==&v10

	cpgt	t0,v7,0xff		;discard junk FSI likes to deliver
	jmpt	t0,m_pm_junk

	cpeq	t0,sumreg,v7
	jmpf	t0,m_pm25
	 cpeq	t1,sumreg1,v8
	jmpf	t1,m_pm25
	 cpeq	t2,sumreg2,v9
	jmpf	t2,m_pm25
	 cpeq	t3,sumreg3,v10
	jmpt	t3,m_pm_junk		;junk if mac frame matches filter
	CK	PM_FILTER_LEN == 4
	CK	&t0+1==&t1 && &t1+1==&t2 && &t2+1==&t3

m_pm25:
	 constx	t4,pm_filter
	mtsrim	CR,PM_FILTER_LEN-1
	storem	0,WORD,v7,t4		;save new frame header in the filter

    ;Skip noting errors if no error bits.  This assumes that most frames
    ;have no CRC errors and no E,A,or C flags set when received by our MAC.
    ;Do not count the error bits, because we would not see them if not
    ;in promiscuous mode.
	constx	t0,FSI_IND_RCV_CNT
	and	t0,t0,v0
	cpeq	v6,t0,0
	jmpt	v6,m_pm40		;v6=0 if need to set bits

	 mv_bt	t0,MAC_E_BIT, v0,FSI_IND_RCV_E
	and	t0,t0,MAC_E_BIT
	or	v6,v6,t0

	mv_bt	t0,MAC_A_BIT, v0,FSI_IND_RCV_A
	and	t0,t0,MAC_A_BIT
	or	v6,v6,t0

	mv_bt	t0,MAC_C_BIT, v0,FSI_IND_RCV_C
	and	t0,t0,MAC_C_BIT
	or	v6,v6,t0

	mv_bt	t0,MAC_ERROR_BIT, v0,FSI_IND_RCV_CRC
	and	t0,t0,MAC_ERROR_BIT
	or	v6,v6,t0

	sll	v6,v6,8
	or	v7,v7,v6		;combine flags with FC byte
	store	0,WORD,v7,v3
m_pm40:

	USEBUF	sml,addr_hi1,v0,,

    ;v0=mbuf address,, addr_hi1=EBus address bits
    ;v2=total frame length, v3=start of board buffer,

	APAD	PAD0_IN_DMA
	add	v1,v0,PAD0_IN_DMA	;v1=host address after snoop header
	add	v9,v2,3
	andn	v9,v9,3			;v9=mbuf size
	mov	v10,v3
	call	t4,dmasub
	 consth	v11,CREG_B2H
	jmpt	t0,dmasub		;loop until finished

	 APAD	PAD0_IN_DMA
	 add	t0,v2,PAD0_IN_DMA	;counting snoop header as data
	call	t4,addb2h		;   tell the host about the mbuf
	 consth	t0,B2H_IN_DONE
	 CK	(B2H_IN & 0xffff) == 0

	rs_st	C2B_SLEEP,,		;host will think we are awake
	mfsr	c2b_atime,TMC
	cplt	t0,host_qtime,c2b_atime	;is the host awake?
	jmpt	t0,m_pm99
	 sub	c2b_atime,c2b_atime,c2b_sleep	;keep C2B DMA going
	consth	c2b_delay,TIME_NOW	;no, immediate C2B DMA

m_pm99:
    ;fix this RX descriptor and check the next one
	constx	t2, FSI_CMD_OWN
	store	0,WORD,t2,t_pm_ring
	mov	t_pm_ring,h_pm_ring
	ADVRP	h_pm_ring, pm, m_pm10


;forget junk frames
m_pm_junk:
	INCCNT	tot_junk,1,,t0,t1	;count junked frame

    ;fix this RX descriptor and check the next one
	constx	t2, FSI_CMD_OWN
	store	0,WORD,t2,t_pm_ring
	mov	t_pm_ring,h_pm_ring
	ADVRP	h_pm_ring, pm, m_pm15


;turn promiscuous MAC ring back on when we have caught up
;   v0=recent receive indication
m_pm_fin:
	mtsrim	CPS,PS_INTSOFF
	STO_IR	MRR,0,FSI_MRR_RPE|FSI_MRR_ROB | FSI_MRR_RX_RE|FSI_MRR_PM_RE,t0
	constx	t0,GSR_PM_RXC		;allow more interrupts
	jmp	mlop0
	 or	fsi_imr,fsi_imr,t0


;Restore input when buffers are drained
;   Assume all previously received frames in both rings have been processed.

m_rx_on:
	lod	t0,max_bufs
	add	t1,sml_bufs,med_bufs	;has host noticed all input?
	add	t1,t1,big_bufs
	cpge	t0,t1,t0
	jmpf	t0,mlop0		;no, do not reset overflow yet

	 mtsrim	CPS,PS_INTSOFF

    ;Prepare the input ring, including following erratum for revised FSI.
	STO_IR	MRR,0,FSI_MRR_RPE|FSI_MRR_ROB | 0,t0
	RRESET	RX_RING_N,t2,t3

	constx	h_rx_ring, rx_ring
	mov	t_rx_ring, h_rx_ring

    ;put the pointers into the input ring.
	constx	t0,rx_bufs
	constx	t1,RX_RING_LEN-1-2
	constx	t2,FSI_CMD_OWN
m_rx_on3:
	store	0,WORD,t2,t_rx_ring
	add	t_rx_ring,t_rx_ring,FSI_RING_ADDR
	constx	t3,RX_BSIZE
	store	0,WORD,t0,t_rx_ring
	add	t_rx_ring,t_rx_ring,FSI_RING_LEN-FSI_RING_ADDR
	jmpfdec	t1,m_rx_on3
	 add	t0,t0,t3

	constx	t0,GSR_RX_ROV
	or	fsi_imr,fsi_imr,t0
	STOFSI	t0,GSR,t1,t2,

    ;define and ready the input ring
	mov	t0,h_rx_ring
	consth	t0,FSI_CMD_DEFRDY | (RX_RING_N*FSI_CMD_RING_N)
	CMD_FSI	t0,h_rx_ring,t2,t3
	WFSI_CDN t1
	 nop

	WFSI_CRF t1
	tbit	t1,fsi_imr,GSR_PM_RXC
	jmpt	t1,m_rx_on4
	 C_IRI	t0,MRR,0,FSI_MRR_RPE|FSI_MRR_ROB | FSI_MRR_RX_RE|FSI_MRR_PM_RE
	C_IRI	t0,MRR,0,FSI_MRR_RPE|FSI_MRR_ROB | FSI_MRR_RX_RE
m_rx_on4:
	jmp	mlop0
	 store	0,WORD,t0,p_fsifcr


;Restore MAC promiscuous input
m_pm_on:
	mtsrim	CPS,PS_INTSOFF

	STO_IR	MRR,0,FSI_MRR_RPE|FSI_MRR_ROB | FSI_MRR_RX_RE,t0
	RRESET	PM_RING_N,t2,t3

	constx	h_pm_ring, pm_ring
	mov	t_pm_ring, h_pm_ring

	constx	t0,pm_bufs		;fix descriptors in both rings
	constx	t1,PM_RING_LEN-2	;   t0=buffer, t1=loop count,
	constx	t2,FSI_CMD_OWN		;   t2=RX indication
m_pm_on5:
	add	t3,t_pm_ring,PM_RING_LEN*FSI_RING_LEN
	store	0,WORD,t2,t_pm_ring
	store	0,WORD,t2,t3
	add	t_pm_ring,t_pm_ring,FSI_RING_ADDR
	add	t3,t3,FSI_RING_ADDR
	store	0,WORD,t0,t_pm_ring
	store	0,WORD,t0,t3
	add	t_pm_ring,t_pm_ring,FSI_RING_LEN-FSI_RING_ADDR
	jmpfdec	t1,m_pm_on5
	 add	t0,t0,PM_BSIZE

	constx	t0,GSR_PM_ROV
	or	fsi_imr,fsi_imr,t0	;turn the interrupt back on
	STOFSI	t0,GSR,t1,t2,

	constx	t0,FSI_CMD_DEFRDY|(PM_RING_N*FSI_CMD_RING_N)|(pm_ring2&0xffff)
	constx	t1,pm_ring
	CMD_FSI	t0,h_pm_ring,t2,t3
	WFSI_CDN t1
	 nop

	jmp	mlop0
	 nop



;process a host command
	.align	16
cmd:
    ;see what the host wants
	STLDMA	HC_CMD_RSIZE,H2B,p_host_cmd,hc_cmd,host_cmd_buf,hc_cmd,t0

	consth	cur_st1,0
	CK	ST1_MB == TRUE

    ;get ready to acknowledge it
	constx	t1,cmd_id		;t1=address of cmd_id
	load	0,WORD,t2,t1

	WAITDMA	t0,fast

	 mtsrim	CR,3-1
	loadm	0,WORD,v0,p_host_cmd	;v0=hc_cmd, v1=hc_cmd_id, v2=data

	cpeq	t0,v1,t2
	jmpt	t0,mlop			;quit on spurious interrupt
	 CK	hc_cmd_id == hc_cmd+4

	cpltu	t3,v0,(cmd_tbl_end-cmd_tbl)/4
	jmpf	t3,cmd_error		;die on bad command

	 constx	t2,cmd_tbl
	sll	t4,v0,2
	add	t2,t2,t4
	load	0,WORD,t2,t2		;fetch function address

	STO_CMD v1,hc_cmd_ack,t0	;get ready to acknowledge it
	const	v0,HC_CMD_WRES_SIZE
	jmpi	t2			;go to the function
	 store	0,WORD,v1,t1		;save new cmd_id against spurious int

;Tell host the result of the command
;   Some commands assume this path turns on the FSI interrupts.
;   v0=bytes to write to the host
cmd_done:
	add	v0,v0,hc_cmd_ack-hc_sign    ;also send the signature
	STLDMA	v0,B2H, p_host_cmd,hc_sign, host_cmd_buf,hc_sign, t0
	jmp	mlop0
	 nop


;bad command from the host
cmd_error:
	jmp	.
	 halt




;host commands
;   All of these commands are entered with v2=first data word.



;download
sto_cksum:.word	0

cmd_sto:
	lod	v4,sto_cksum		;v4=data checksum

	t_st	t0,DOWNING
	jmpt	t0,cmd_sto7
	 nop

    ;if not already set for it, then kill FSI, and copy EPROM to SRAM
    ;offset by same value used by host, and start checksum

	mtsrim	CPS,PS_INTSOFF

	STO_IR	RESET,0,0,t0		;zap the FSI
	constx	fsi_imr,FSIMSK | GSR_ERRORS ;fake out the main loop
	STOFSI	0,IMR,t0,t1

	set_st	DOWNING,t0
	call	v4,set_rep
	 nop

	mtsrim	CPS,PS_INTSON
	asneq	V_ELM,t0,t0		;ELM interrupt to fix line states

	.set	sto_len, 16
	constx	t0,EPROM
	constx	t1,XPI_DOWN
	constx	t3,EPROM_SIZE/4/sto_len-2
	CK	(EPROM_SIZE % sto_len) == 0
cmd_sto2:
	mtsrim	CR,sto_len-1
	loadm	0,WORD,sumreg,t0
	add	t0,t0,4*sto_len
	mtsrim	CR,sto_len-1
	storem	0,WORD,sumreg,t1	;copy EPROM to SRAM
	jmpfdec	t3,cmd_sto2
	 add	t1,t1,4*sto_len

	const	v4,0			;v4=initialized checksum

cmd_sto7:
	mtsrim	CR,XPI_DWN_LEN+2-1
	add	t0,p_host_cmd,hc_cmd_data
	loadm	0,WORD,sumreg,t0	;fetch address, count, & data
	const	v3,&sumreg2*4
	sub	sumreg1,sumreg1,2

cmd_sto8:
	mtsr	IPA,v3
	mtsr	IPB,v3
	add	v3,v3,4
	store	0,WORD,gr0,sumreg	;store and checksum the data
	add	v4,v4,gr0
	jmpfdec	sumreg1,cmd_sto8
	 add	sumreg,sumreg,4

	inv				;things may have changed
	constx	t0,sto_cksum		;save the checksum
	store	0,WORD,v4,t0
	jmp	cmd_done
	 const	v0,HC_CMD_WRES_SIZE


;execute downloaded code
;   v2=target
cmd_exec:
	t_st	t0,DOWNING
	jmpf	t0,cmd_error

	 lod	v4,sto_cksum		;if the checksum is bad, forget it
	cpeq	t0,v4,0
	jmpf	t0,cmd_error

	 mov	t4,v2			;save start address for loads below

    ;say we are starting
	STLDMA	HC_CMD_WRES_SIZE,B2H,p_host_cmd,hc_cmd_ack,host_cmd_buf,hc_cmd_ack,t0

	add	v7,p_host_cmd,hc_cmd_data+8
	mtsrim	CR,8-1
	loadm	0,WORD,v0,v7		;go to code with 8 args
	CK	XPI_DWN_LEN >= 8

	WAITDMA	t0

	jmpi	t4
	 mtsrim	CPS,PS_INTSOFF



;upload
;   v2=source address
cmd_fet:
	add	v4,p_host_cmd,hc_cmd_data+4
	load	0,WORD,v3,v4		;fetch the count

	sub	v3,v3,1
	mtsr	CR,v3			;fetch the data
	loadm	0,WORD,sumreg,v2

	mtsr	CR,v3
	add	v4,p_host_cmd,hc_cmd_res
	storem	0,WORD,sumreg,v4

	sll	v0,v3,2
	jmp	cmd_done
	 add	v0,v0,HC_CMD_WRES_SIZE+4



;set parameters
;   v2=initial word of request
cmd_init:
    ;many basic configuration values are changed
	mtsrim	CPS,PS_INTSOFF

    ;configuration flags in the first word
	constx	t0,ST2_IN_CKSUM | ST2_LAT_VOID | ST2_ZAPRING
	andn	cur_st2,cur_st2,t0

	tbit	t0,v2,XPI_IN_CKSUM
	cset_st	t0,IN_CKSUM

	tbit	t0,v2,XPI_LAT_VOID
	cset_st	t0,LAT_VOID

	tbit	t0,v2,XPI_ZAPRING
	cset_st	t0,ZAPRING

 .ifdef MEZ
	tbit	t0,v2,XPI_IO4IA_WAR
	cset_st	t0,IO4IA_WAR
 .endif

	.set	nxt,hc_cmd_data+4
 .macro	ghword, reg
	  LOD_CMD reg,nxt,H,
	  .set	  nxt,nxt+2
 .endm

 .macro	gword,	reg,addr,tmp,i1,i2
	  LOD_CMD reg,nxt,,
	  .set	  nxt,nxt+4
  .ifnes "@addr@",""
   .if $ISREG(addr)
	.err
   .endif
	  constx  tmp,addr
  .endif
  .ifnes "@i1@",""
	  i1	  reg,reg,i2
  .endif
  .ifnes "@addr@",""
	  store	  0,WORD,reg,tmp
  .endif
 .endm

;XXX this should do more than a single 'sll'
 .macro gtime,	reg,addr,tmp
	  gword  reg,addr,tmp,sll,MHZ_BT
 .endm

	gword	b2h_h_ptr,b2h_h_base,v3,,,	;position in host buffer

    ;The host-to-board buffer length used by the board must be no larger
    ;than that of the actual buffer provided by the host, or the board will
    ;trash host memory.  If the board uses a shorter length, data transfers
    ;and CMT state changes will be lost and the board will appear to the
    ;host to be quiet.  The host buffer must be big enough, or the board will
    ;write over previous notes before the host has seen them.
    ;We cannot refuse to procede if the lengths differ, since that would
    ;prevent downloading new firmware.  To let the host know, we just refuse
    ;to acknowledge the command

	gword	v1
	constx	v2,BD_B2H_LEN
	cple	t0,v1,v2
	jmpf	t0,cmd_init4
	 sra	t0,t0,31
	STO_CMD	t0,hc_cmd_ack,t1
cmd_init4:
	sll	v1,v1,2
	CK	_SIZE_B2H_BLK == 4
	add	b2h_h_lim,b2h_h_ptr,v1


	gword	v1,max_bufs,v3,sub,MIN_BIG+MIN_MED+MIN_SML

	gword	d2b_pos,d2b_base,v3
	gword	d2b_lim

	gword	c2b_pos,c2b_base,v3

	gtime	c2b_dma_delay

	gtime	v1,int_delay,v3

	gtime	v1,max_host_work,v3

	gtime	c2b_sleep

	gword	v1,ctptr,v3

	ghword	rel_tokr

    ;adjust all uses of SML_SIZE0 and PAD0_IN_DMA to match 64-bit IRIX.
 .macro	fixirix, op,tbl,len
	  CONTBL  v2,tbl
	  const	  v3,len-2
$1:
	  load	  0,WORD,t3,v2
	  add	  v2,v2,4
	  load	  0,WORD,t4,t3
	  op	  t4,t4,v1
	  jmpfdec v3,$1
	   store  0,WORD,t4,t3
 .endm

	ghword	v1
	sub	v1,v1,SML_SIZE0
	fixirix	add,asml_tbl,NUM_ASML

	ghword	v1
	sub	v1,v1,PAD0_IN_DMA
	fixirix	add,apad_tbl,NUM_APAD

	fixirix	sub,spad_tbl,NUM_SPAD


    ;set the ELM timers
	constx	v0, SMT_C_MIN
	STOELM0	v0, A_MAX,t0		;Motorola "A_MAX", but really C_Min
	STOELM1	v0, A_MAX,t1

	constx	v0, SMT_TL_MIN		;Motorola "LS_Max", but it is TL_Min
	STOELM0	v0, LS_MAX,t0
	STOELM1	v0, LS_MAX,t1

	constx	v0, SMT_TB_MIN
	STOELM0	v0, TB_MIN,t0
	STOELM1	v0, TB_MIN,t1

	constx	v0, SMT_T_OUT
	STOELM0	v0, T_OUT,t0
	STOELM1	v0, T_OUT,t1

	constx	v0, SMT_T_SCRUB
	STOELM0	v0, T_SCRUB,t0
	STOELM1	v0, T_SCRUB,t1

	constx	v0, SMT_NS_MAX
	STOELM0	v0, NS_MAX,t0
	STOELM1	v0, NS_MAX,t1

	constx	v0,200
	STOELM0	v0, LE_THRESH,t0
	STOELM1	v0, LE_THRESH,t1

	constx	v0,-16
	STOELM0	v0, LC_SHORT,t0
	STOELM1	v0, LC_SHORT,t1

	t_st	t0,CDDI
	jmpf	t0,cmd_init6
	 constx	v0,0xfd76
	STOELM0	v0,FOTOFF_ON,t0,	;set CDDI parameters
	STOELM1	v0,FOTOFF_ON,t1

	constx	v0,0
	STOELM0	v0,FOTOFF_OFF,t0
	STOELM1	v0,FOTOFF_OFF,t1

	constx	v0,ELM_CPI_SDONEN+ELM_CPI_SDOFFEN+ELM_CPI_CIPHER_ENABLE
	STOELM0	v0,CIPHER,t0
	STOELM1	v0,CIPHER,t1
cmd_init6:

	mtsrim	CPS,PS_INTSON
	jmp	cmd_done
	 const	v0,HC_CMD_WRES_SIZE

	.purgem	gword,gtime



;make the ELM(s) repeat
cmd_rep:
	mtsrim	CPS,PS_INTSOFF
	call	v4, set_rep
	 nop
	mtsrim	CPS,PS_INTSON
	const	v0,HC_CMD_WRES_SIZE
	jmp	cmd_done
	 asneq	V_ELM,t0,t0		;ELM interrupt to fix line states



;set PHY line state on ELM0
;   v2=new line state
cmd_setls0:
	constx	t0, setls_tbl
	sll	v0,v2,1
	add	v0,v0,t0
	load	0,HWORD,v0,v0

	mtsrim	CPS,PS_INTSOFF

	STOELM0	v0, CTRL_B,t0,fast
	constx	v3, ELM_CA_DEFAULT | ELM_CA_SC_BYPASS | ELM_CA_REM_LOOP
	STOELM0	v3, CTRL_A,t0

    ;fancy ELM interrupts only if "active"
	rs_st	ELM0_INT
	cpeq	t0,v2,PCM_ALS
	c2set_st t0,ELM0_INT

    ;if we were repeating, then set the other PHY to QLS
	t_st	t0,PHY_REP
	jmpf	t0,pcm_fin

	 rs_st	PHY_REP,t0
	STOELM1	v3, CTRL_A,t0
	constx	t1, ELM_CB_PCM_OFF
	STOELM1	t1, CTRL_B,t0
	jmp	pcm_fin
	 nop


;set PHY line state on ELM1
;   v2=new line state
cmd_setls1:
	constx	t0, setls_tbl
	sll	v0,v2,1
	add	v0,v0,t0
	load	0,HWORD,v0,v0

	mtsrim	CPS,PS_INTSOFF

	STOELM1	v0, CTRL_B,t0,fast
	constx	v3, ELM_CA_DEFAULT | ELM_CA_SC_BYPASS | ELM_CA_REM_LOOP
	STOELM1	v3, CTRL_A,t0

    ;fancy ELM interrupts only if "active"
	rs_st	ELM1_INT
	cpeq	t0,v2,PCM_ALS
	c2set_st t0,ELM1_INT

    ;if we were repeating, then set the other PHY to QLS
	t_st	t0,PHY_REP
	jmpf	t0,pcm_fin

	 rs_st	PHY_REP,t0
	STOELM0	v3, CTRL_A,t0
	constx	t1, ELM_CB_PCM_OFF
	STOELM0	t1, CTRL_B,t0
	jmp	pcm_fin
	 nop



;turn off ELM PCM
cmd_pc_off0:
	CMDELM0	PCM_STOP, v0,t1,t2

	constx	v3, ELM_CA_DEFAULT | ELM_CA_SC_BYPASS | ELM_CA_REM_LOOP
	STOELM0	v3, CTRL_A,t0

	rs_st	ELM0_INT

    ;if we were repeating, then set the other PHY to QLS
	t_st	t0,PHY_REP
	jmpf	t0,pcm_fin
	 rs_st	PHY_REP,t0
	STOELM1	v3, CTRL_A,t0
	constx	t1, ELM_CB_PCM_OFF
	STOELM1	t1, CTRL_B,t0
	rs_st	ELM1_INT
	jmp	pcm_fin
	 nop


cmd_pc_off1:
	CMDELM1	PCM_STOP, v0,t1,t2

	constx	v3, ELM_CA_DEFAULT | ELM_CA_SC_BYPASS | ELM_CA_REM_LOOP
	STOELM1	v3, CTRL_A,t0

	rs_st	ELM1_INT

    ;if we were repeating, then set the other PHY to QLS
	t_st	t0,PHY_REP
	jmpf	t0,pcm_fin
	 rs_st	PHY_REP,t0
	STOELM0	v3, CTRL_A,t0
	constx	t1, ELM_CB_PCM_OFF
	STOELM0	t1, CTRL_B,t0
	rs_st	ELM0_INT
	jmp	pcm_fin
	 nop



;common PCM command code
;   v1=command bits
elm0_cmd:
	sub	t1,p_elm,ELMBASE - ELM0_CTRL_B
elm0_cmda:
	mtsrim  CPS,PS_INTSOFF
	load	0,HWORD,v0,t1
	const	t0,ELM_CB_MATCH_LS_MASK
	and	v0,v0,t0
	store	0,HWORD,v0,t1
	or	v0,v0,v1
	mtsrim  MMU,0xff
	store	0,HWORD,v0,t1

;common code to finish a PCM command
pcm_fin:
	call	t4, set_elm_mask
	 nop
	mtsrim	CPS,PS_INTSON
	const	v0,HC_CMD_WRES_SIZE
	jmp	cmd_done
	 asneq	V_ELM,t0,t0

elm1_cmd:
	jmp	elm0_cmda
	 add	t1,p_elm,ELM1_CTRL_B - ELMBASE


;turn on ELM PCM
cmd_pc_start0:
	constx	v1,ELM_CB_PCM_START
	jmp	elm0_cmd
	 rs_st	ELM0_INT

cmd_pc_start1:
	constx	v1,ELM_CB_PCM_START
	jmp	elm1_cmd
	 rs_st	ELM1_INT


;set transmit vector and length for sending PCM bits
;   v2=combined vector and length
cmd_setvec0:
	mtsrim  CPS,PS_INTSOFF
	STOELM0	v2, XMIT_LEN,t0
	srl	v2,v2,16
	STOELM0	v2, XMIT_VEC,t0
	jmp	pcm_fin
	 rs_st	ELM0_INT

cmd_setvec1:
	mtsrim  CPS,PS_INTSOFF
	STOELM1	v2, XMIT_LEN,t0
	srl	v2,v2,16
	STOELM1	v2, XMIT_VEC,t0
	jmp	pcm_fin
	 rs_st	ELM1_INT



;turn on ELM PCM
cmd_pc_trace0:
	jmp	elm0_cmd
	 const0	v1,ELM_CB_PCM_TRACE

cmd_pc_trace1:
	jmp	elm1_cmd
	 const0	v1,ELM_CB_PCM_TRACE


;start ELM link Confidence Test
cmd_lct0:
	constx	v1,ELM_CB_LCT
	jmp	elm0_cmd
	 set_st	ELM0_INT

cmd_lct1:
	constx	v1,ELM_CB_LCT
	jmp	elm1_cmd
	 set_st	ELM1_INT


;finish signaling
cmd_join0:
	constx	v1,ELM_CB_PC_JOIN
	jmp	elm0_cmd
	 set_st	ELM0_INT

cmd_join1:
	constx	v1,ELM_CB_PC_JOIN
	jmp	elm1_cmd
	 set_st	ELM1_INT



cmd_wrapa:
cmd_wrapb:
cmd_thru:
	jmp	.
	 halt


;control the optical bypass
cmd_bypass_ins:
	consth	t0,CREG_BYPASS
	or	cregs,cregs,t0		;write bit in CREG during cmd ack DMA
	jmp	cmd_done
	 const	v0,HC_CMD_WRES_SIZE

cmd_bypass_rem:
	consth	t0,CREG_BYPASS
	andn	cregs,cregs,t0		;write bit in CREG during cmd ack DMA
	jmp	cmd_done
	 const	v0,HC_CMD_WRES_SIZE


;set the MAC multicast mode, address filters, and so on
;   v2=initial word of request
cmd_mac_mode:
	add	v0,p_host_cmd,hc_cmd_data+4
	mtsrim	CR,MMODE_HLEN/4-1
	loadm	0,WORD,sumreg,v0
	constx	v0,dahash
	mtsrim	CR,MMODE_HLEN/4-1	;set DA hash table
	storem	0,WORD,sumreg,v0

	constx	v3,MAC_CA_DEFAULT
	mtsrim	CPS,PS_INTSOFF

	tbit	t0,v2,MMODE_PROM
	jmpf	t0,cmd_mac_mode4
	 t_st	t1,PROM,		;mac-promiscuous ring already alive?

	jmpt	t1,cmd_mac_mode6
	 const0	v3,MAC_CA_PROM

	constn	t0,-1
	constx	t1,pm_filter		;no, reset MAC frame filter
	store	0,WORD,t0,t1

	constx	t0,GSR_PM_RXC | GSR_PM_ROV
	andn	fsi_imr,fsi_imr,t0
	jmp	cmd_mac_mode6
	 set_st	PROM			;make a note to turn it on

;   v2=1st word of request,  v3=growing MAC control register A bits
cmd_mac_mode4:				;turn off promiscuous mac ring
	rs_st	PROM
	STO_IR	MRR,0,FSI_MRR_RPE|FSI_MRR_ROB | FSI_MRR_RX_RE,t0


;   v2=1st word of request,  v3=growing MAC control register A bits
cmd_mac_mode6:
	and	t0,v2,MMODE_HMULT
	mv_bt	t0,MAC_CA_COPY_GRP, t0,MMODE_HMULT
	or	v3,v3,t0		;receive all multicasts if asked

	STOMAC	v3,CTRL_A,t0,fast,	;install new MAC settings

	and	t0,v2,MMODE_AMULTI	;remember if receiving all multicasts
	mv_bt	t0,ST1_AMULTI, t0,MMODE_AMULTI
	or	cur_st1,cur_st1,t0

	mtsrim	CPS,PS_INTSON
	jmp	cmd_done
	 const	v0,HC_CMD_WRES_SIZE


;set FSI CAM
;   v2=address of CAM entries on the host
cmd_cam:
	mtsrim	CPS,PS_INTSOFF

	consth	t0,CREG_RED		;red LED to say ring-op will be false
	andn	cregs,cregs,t0		;write bit in CREG during DMA
	constx	v5,fsi_cam		;v5=CAM entries
	STLDMA	FSI_CAM_LEN*8,H2B,v5,,v2,0,t0

    ;turn off the MAC to change the CAM because of FSI bug
	LODMAC	v3,CTRL_A,fast,		;v3=MAC control register A
	LODMAC	v4,CTRL_B		;v4=MAC control register B
	constx	t2,MAC_CA_ON
	andn	t2,v3,t2
	STOMAC	t2,CTRL_A,t0

	WAITDMA	t0,fast

	 const	v0,FSI_CAM_LEN-2
cmd_cam8:
	mtsrim	CR,2-1
	loadm	0,WORD,t2,v5
	CK	1+&t2 == &t3
	CMD_FSI	t2,t3,t0,t1,fast,	;send the CAM entries to the FSI
	jmpfdec	v0,cmd_cam8
	 add	v5,v5,8

	STOMAC	v3,CTRL_A,t0,fast,	;restore MAC
	constx	t0,MAC_CB_RESET
	or	v4,v4,t0
	STOMAC	v4,CTRL_B,t1,,		;FDDI MAC reset

	mtsrim	CPS,PS_INTSON
	jmp	cmd_done
	 const	v0,HC_CMD_WRES_SIZE



;set MAC parameters and turn on the MAC
;   v2=initial word of MAC long address
cmd_mac_parms:
	mtsrim	CPS,PS_INTSOFF

	LODMAC	v3,CTRL_A,fast,		;v3=MAC control register A
	LODMAC	v4,CTRL_B		;v4=MAC control register B

    ;turn off the MAC to change the control registers
	constx	t2,MAC_CA_ON
	andn	t2,v3,t2
	STOMAC	t2,CTRL_A,t0

	.set	nxt,hc_cmd_data
 .macro	gnext,	reg
	  .set	  nxt,nxt+4
	  LOD_CMD  reg,nxt,,
 .endm


    ;set the MAC addresses
	gnext	v0
	srl	t1,v2,16
	STOMAC	t1,MLA_C,t0
	STOMAC	v2,MLA_B,t0
	srl	t1,v0,16
	STOMAC	t1,MLA_A,t0
	const	t1,0
	STOMAC	t1,MSA,t0

    ;set T_REQ, TVX, and T_Max
	gnext	v0
	STOMAC	v0,T_REQ,t0

	gnext	v0
	STOMAC	v0,TVX_T_MAX,t0

    ;set the MAC interrupt masks, if it was off
	tbit	t0,v3,MAC_CA_ON
	jmpf	t0,cmd_mac_parms2
	 tbit	t0,v4,MAC_CB_OFF
	jmpf	t0,cmd_mac_parms4

cmd_mac_parms2:
	 constx	t0,MAC_MASK_A_DEFAULT
	STOMAC	t0,MASK_A,t1
	constx	t0,MAC_MASK_B_DEFAULT | MAC_MASK_B_MORE
	STOMAC	t0,MASK_B,t1
	constx	t0,MAC_MASK_C_DEFAULT
	STOMAC	t0,MASK_C,t1

	LODMAC	t0,EVNT_A,,		;clear any old MAC interrupts
	LODMAC	t0,EVNT_B
	LODMAC	t0,EVNT_C

cmd_mac_parms4:

    ;turn on the MAC
	constx	t0,MAC_CB_DEFAULT
	STOMAC	t0,CTRL_B,t1
	constx	t0,MAC_CA_ON
	or	v3,v3,t0
	STOMAC	v3,CTRL_A,t0

	constx	t0,MAC_CB_DEFAULT | MAC_CB_RESET_CLAIM
	STOMAC	t0,CTRL_B,t1,,		;FDDI MAC reset

	consth	t0,CREG_RED		;set red LED to say ring-op is false
	andn	cregs,cregs,t0		;write bit in CREG during cmd-ACK DMA

	.purgem	gnext

	mtsrim	CPS,PS_INTSON
	jmp	cmd_done
	 const	v0,HC_CMD_WRES_SIZE



;turn off the MAC
cmd_mac_off:
	mtsrim	CPS,PS_INTSOFF
	call	t4,zapmac
	 const	t2,MAC_CA_OFF
	mtsrim	CPS,PS_INTSON
	jmp	cmd_done
	 const	v0,HC_CMD_WRES_SIZE



;start directed beacon
;   v2=1st word, length, from host
cmd_d_bec:
    ;fetch the body of the beacon frame
	add	v4,p_host_cmd,hc_cmd_data+4
	sub	t0,v2,1			;add +3 to round up, -4 to get count-1
	jmpt	t0,cmd_d_bec6		;length=0 says turn it off
	 sra	t0,t0,2

	mtsr	CR, t0
	loadm	0,WORD,sumreg,v4
	constx	v4,beacon_buf		;v4=beacon buffer
	mtsr	CR, t0
	storem	0,WORD,sumreg,v4	;install new directed beacon

    ;put the length and pointer into the beacon ring
	 constx	v1,b_ring
	consth	v2,FSI_CMD_OWN | FSI_CMD_F | FSI_CMD_L
	CK	((FSI_CMD_OWN | FSI_CMD_F | FSI_CMD_L) & 0xffff) == 0
	mtsrim	CPS,PS_INTSOFF
	store	0,WORD,v2,v1
	add	t0,v1,FSI_RING_ADDR
	store	0,WORD,v4,t0

    ;start high priority FSI transmit ring
	constx	t0,FSI_CMD_DEFRDY | (B_RING_N*FSI_CMD_RING_N)|(b_ring&0xffff)
	constx	t1,b_ring2
	CMD_FSI	t0,t1,t2,t3

    ;start beaconing
	LODMAC	v4,CTRL_B
	constx	t0,MAC_CB_RESET_BEACON | MAC_CB_FSI_BEACON
	or	v4,v4,t0
	STOMAC	v4,CTRL_B,t1,,
	mtsrim	CPS,PS_INTSON
	jmp	cmd_done
	 const	v0,HC_CMD_WRES_SIZE


cmd_d_bec6:
    ;turn off directed beacon
	constx	t0, FSI_CMD_RSTOP | (B_RING_N*FSI_CMD_RING_N)
	constx	t1,b_ring
	store	0,WORD,t0,t1
	jmp	cmd_done
	 const	v0,HC_CMD_WRES_SIZE


;fetch the current line state
cmd_fet_ls0:
	setip   ls0,ls0,ls0
cmd_fet_ls0a:
	asneq	V_ELM,t0,t0		;get the most current state
	STO_CMD	gr0,hc_cmd_res,t0
	jmp	cmd_done
	 const	v0,HC_CMD_WRES_SIZE+4

cmd_fet_ls1:
	jmp	cmd_fet_ls0a
	 setip	ls1,ls1,ls1


;fetch counters
cmd_fet_cnt:
	consth	c2b_delay,TIME_NOW	;see what host has to say after this

    ;update the PHY counters by causing an interrupt
	asneq	V_ELM,t0,t0

	mtsrim	CPS,PS_INTSOFF
	LODELM0	t1,STATUS_B
	LODELM0	t2,RCV_VEC
	sll	t2,t2,16
	or	t1,t1,t2
	STOCNT	t1,elm0st,t0

	LODELM1	t1,STATUS_B,fast
	LODELM1	t2,RCV_VEC
	sll	t2,t2,16
	or	t1,t1,t2
	STOCNT	t1,elm1st,t0


	LODMAC	t1,CTRL_B,fast
	tbit	t1,t1,MAC_CB_OFF
	jmpt	t1,cmd_fet_cnt6
	 const0	v0,MAC_OFF

    ;poke the MAC only if it is alive
	mtsrim	CPS,PS_INTSON
	asneq	V_MAC,t0,t0
	mtsrim	CPS,PS_INTSOFF

	t_st	t0,LAT_VOID		;if VOIDs turned on
	jmpf	t0,cmd_fet_cnt1
	 constx	t1,void_time
	load	0,WORD,t0,t1
	sub	t0,time_hz,t0
	srl	t0,t0,MHZ_BT+20		;t0=~seconds since last measurement
	cpgeu	t0,t0,2			;and > ~2 seconds since last
	jmpf	t0,cmd_fet_cnt1
	 nop

    ;trigger a future ring latency measurement
	LODMAC	t0,CTRL_B,fast
	store	0,WORD,time_hz,t1
	constx	t1,MAC_CB_RING_PURGE
	or	t0,t0,t1
	STOMAC	t0,CTRL_B,t1,fast
cmd_fet_cnt1:

    ;fetch a consistent T_Neg from the MAC
	const	v3,0
cmd_fet_cnt2:
	LODMAC	t0,T_NEG_B,fast
	sll	t0,t0,24
	sra	t0,t0,8
	LODMAC	v0,T_NEG_A,fast
	or	v0,v0,t0
	cpeq	t0,v0,v3
	jmpf	t0,cmd_fet_cnt2
	 add	v3,v0,0
	STOCNT	v0,t_neg,t0

	t_st	t0,CLM
	jmpt	t0,cmd_fet_cnt6
	 const0	v0,MAC_CLAIMING

	t_st	t0,BEC
	jmpt	t0,cmd_fet_cnt6
	 const0	v0,MAC_BEACONING

	constx	v0,MAC_ACTIVE
cmd_fet_cnt6:
	STOCNT	v0,mac_state,t0		;save current MAC state


cmd_fet_cnt7:
	mov	t2,time_hz
	mfsr	t0,TMC
	cpeq	t3,t2,time_hz		;get (consistent) current time
	jmpf	t3,cmd_fet_cnt7
	 constx	t1,TIME_INTVL
	sub	t1,t1,t0
	add	t1,t1,t2
	srl	t1,t1,MHZ_BT
	CK	cnt_ticks == cnts
	store	0,WORD,t1,p_cnts

    ;DMA the counters with interrupts off to ensure they are consistent.
    ;This is costly, but should happen rarely.
	lod	v2,ctptr
	STLDMA	CNT_SIZE,B2H,p_cnts,,v2,,t0
	WAITDMA	t0
	mtsrim	CPS,PS_INTSON
	jmp	cmd_done
	 const	v0,HC_CMD_WRES_SIZE



;arm board-to-host PHY interrupts
cmd_pint_arm:
	set_st	PINT_ARM,t0
	jmp	cmd_done
	 const	v0,HC_CMD_WRES_SIZE


;restart C2B DMA
	.align	16
cmd_wakeup:
	rs_st	C2B_SLEEP
	mfsr	c2b_atime,TMC			;keep C2B DMA active
	sub	c2b_atime,c2b_atime,c2b_sleep
	consth	c2b_delay,TIME_NOW	;see what the host has to say
	jmp	cmd_done
	 const	v0,HC_CMD_WRES_SIZE


;turn on repeat mode
;   entry: interrupts off,  v4=return,  t0,t1,t2=scratch
;   This is called from the EEPROM, and so cannot assume SRAM is correct.
set_rep:
	rs_st	PINT_ARM,t0,		;no more PHY interrupts

	t_st	t0,DAS			;is this a SAS?
	jmpt	t0,set_rep3
	 andn	cur_st1,cur_st1,ST1_ELM0_INT | ST1_ELM1_INT

    ;hardware can handle SAS repeating
	const	t1, 0
	STOELM0	t1, MASK, t0
	constx	t1, ELM_CB_M_DEFAULT | ELM_CB_TX_ALS
	STOELM0	t1, CTRL_B,t0
	constx	t1, ELM_CA_DEFAULT | ELM_CA_SC_BYPASS | ELM_CA_REM_LOOP | ELM_CA_RF_DISABLE
	STOELM0	t1, CTRL_A,t0
	jmpi	v4
	 nop


set_rep3:
    ;if DAS, turn off the repeat filters, put both in THRU, zap MAC
    ;and finally fake an interrupt to start repeating line states

	call	t4,zapmac		;make MAC repeat
	 const0	t2,MAC_CA_OFF

	set_st	PHY_REP,t0,		;tell the interrupt handler

	constx	t1, ELM_CA_DEFAULT | ELM_CA_RF_DISABLE
	STOELM0	t1, CTRL_A,t0
	STOELM1	t1, CTRL_A,t0

	constx	t1, ELM_CB_M_DEFAULT | ELM_CB_TX_ALS
	STOELM0	t1, CTRL_B,t0
	STOELM1	t1, CTRL_B,t0

	const	t1, 0
	STOELM1	t1, MASK, t0

	jmp	rep_ls			;start repeating
	 mov	 it0,v4



;turn off the mac
;   entry: interrupts off,  t2=new MAC CTRL_A,  t4=return,  t0,t1=scratch
;   This is called from the EEPROM, and so cannot assume SRAM is correct.
zapmac:
    ;turn off directed beacon
	constx	t0, FSI_CMD_RRESET | (B_RING_N*FSI_CMD_RING_N)
	constx	t1,b_ring
	store	0,WORD,t0,t1

    ;zap the MAC
	STOMAC	t2,CTRL_A,t1

	constx	t0,MAC_CB_DEFAULT | MAC_CB_OFF
	STOMAC	t0,CTRL_B,t1

	const	t0,0
	STOMAC	t0,MASK_A,t1
	STOMAC	t0,MASK_B,t1
	STOMAC	t0,MASK_C,t1

    ;clear event registers
	LODMAC	t0,EVNT_A
	LODMAC	t0,EVNT_B
	LODMAC	t0,EVNT_C

    ;turn off RNGOP
	LODCNT	t0,rngop
	STOCNT	t0,rngbroke,t1

	consth	t0,CREG_RED		;set red LED to say ring-op is false
	jmpi	t4
	 andn	cregs,cregs,t0		;write bit in CREG during cmd ack DMA




;set the ELM interrupt masks
;   entry: interrupts off,  t4=return,  t1,t2,t3,t0=scratch
set_elm_mask:
    ;Use either the "active" or the "signalling" interrupt masks.
	t_st	t1,ELM0_INT
	sra	t1,t1,31
	constx	t2,ELM_EV_SIG
	constx	t3,ELM_EV_ACT
	and	t0,t3,t1
	andn	t1,t2,t1
	or	t1,t1,t0
	STOELM0	t1,MASK,t0

	t_st	t1,DAS			;finished if no only 1 ELM
	jmpfi	t1,t4

    ;no LS interrupts on ELM1 if just repeating
	 mv_bt	t1,ELM_EV_LS, cur_st1,ST1_PHY_REP
	and	t1,t1,ELM_EV_LS
	andn	t2,t2,t1

	t_st	t1,ELM1_INT
	sra	t1,t1,31
	and	t0,t3,t1
	andn	t1,t2,t1
	or	t1,t1,t0
	STOELM1	t1,MASK,t0

	jmpi	t4
	 nop


;repeat the current line state for a DAS_SM
;   entry: interrupts off, ls0=new line state, it0=return,  it1,it2=scratch
;   This is called by interrupt handlers as well as by non-interrupt code.
rep_ls:
	sll	it2,ls0,2
	CONTBL	it1,rep_ls_tbl
	add	it1,it1,it2
	load	0,WORD,it1,it1
	STOELM1	it1, CTRL_A,it2,fast
	srl	it1,it1,16
	STOELM1	it1, CTRL_B,it2

	jmpi	it0
	 nop


;Timer interrupt handler
;   This happens a few times per second.

	.equ	TIME_NOW, TIME_INTVL*2
	CK	(TIME_NOW & 0xffff0000) > TIME_INTVL
	.equ	TIME_NEVER,0x80000000

	.align	16
timerint:
	constx	it2,TIME_INTVL | TMR_IE
	mtsr	TMR,it2

	CK	((TIME_INTVL | TMR_IE) & 0xffff) == (TIME_INTVL & 0xffff)
	consth	it2,TIME_INTVL		;it2=time change since previous tick

    ;advance the timers
	consth	it3,0x7fffffff-TIME_INTVL-0x10000   ;limit to prevent overflow
						;fudged to allow consth
 .macro CNT_TIMER, tmr
   .if $ISREG(tmr)
	  cplt	  it0,tmr,it3
	  sra	  it0,it0,31
	  and	  it0,it0,it2
	  add	  tmr,tmr,it0
   .else
	  constx  tpc,tmr
	  load	  0,WORD,tav,tpc
	  CNT_TIMER tav
	  store	  0,WORD,tav,tpc
   .endif
 .endm
	CNT_TIMER c2b_delay
	CNT_TIMER c2b_atime
	CNT_TIMER host_qtime
	.purgem CNT_TIMER

	add	time_hz,time_hz,it2	;advance the clock

    ;turn on more MAC interrupts, if any are already on
	LODMAC	it0,MASK_B,fast
	constx	it2,MAC_MASK_B_MORE
	cpeq	it3,it0,0
	jmpt	it3,timerint4
	 or	it0,it0,it2
	STOMAC	it0,MASK_B,it1
timerint4:

    ;control the LEDs.
    ;The green LED is turned on and off here to make consistent flashes.
    ;The red LED may be turned off here as a side effect, after being
    ;explicitly turned on by ring problems.
	constx	it0,CREG_GREEN
	or	cregs,cregs,it0
	mv_bt	it2,CREG_GREEN, cur_st2,ST2_GREEN
	and	it0,it0,it2
	andn	cregs,cregs,it0		;green on if recent traffic
	rs_st	GREEN			;but on for only 1 tick

	jmp	t_iret
	 store	0,WORD,cregs,p_creg

;FSI interrupt handler
fsiint:
	LODFSI	tav,GSR,fast		;tav=FSI status

	constx	it0,GSR_ERRORS
	constx	it1,FSIMSK | GSR_ERRORS

	and	it1,it1,tav
	STOFSI	it1,GSR,it2,it3,,	;turn off the interrupt bits

    ;Turn off interrupt mask bits to stop the interrupt until we have
    ;handled all of the interrupt in the main loop.
	andn	it1,it1,it0
	andn	fsi_imr,fsi_imr,it1
	STOFSI	fsi_imr,IMR,it2,it3,,

    ;Ensure the FSI has removed its interrupt request.
	mtsrim	MMU,0xff

    ;try to recover from errors
	and	it0,it0,tav
	cpneq	it0,it0,0
	jmpf	it0,t_iret

fsibad:	 srl	it0,tav,GSR_RER0_BT
	and	it0,it0,(GSR_RER5|GSR_RER4|GSR_RER3|GSR_RER2|GSR_RER1|GSR_RER0)/GSR_RER0
	cpeq	it1,it0,0
	jmpt	it1,fsiier
	 clz	it1,it1
	subr	it1,it1,31+(0x100-FSI_RSR/0x1000000)
	sll	it1,it1,31-FSI_R_BT
	LOD_IR	it0,it1,xx,,		;it0=appropriate Ring State Register
	jmp	fsibad9

fsiier:
	 tbit	it0,tav,GSR_HER
	jmpt	it0,badint		;die on a "host error"
	 nop

	LOD_IR	it0,IER,0,,		;it0=internal error status register

fsibad9:
	CNTADR	it1,fsi_bug
	load	0,HWORD,it2,it1
	add	it2,it2,1
	sll	it2,it2,16
	srl	it0,it0,16
	or	it2,it2,it0
	store	0,WORD,it2,it1

    ;restoring the rings seems to help
	constx	it1,GSR_RX_ROV | GSR_PM_ROV
	andn	fsi_imr,fsi_imr,it1

	STO_IR	IER,0,0,it1

	jmp	t_iret
	 set_st	PINT_REQ


;host to board interrupt
hostint:
	load	0,WORD,it0,p_gsor	;clear the interrupt

	consth	cur_st1,ST1_MB		;tell the main loop to do the work
	CK	ST1_MB == TRUE

;Work around Erratum #6 and the "Unresolved #1" that suggests aligning
;   iret instructions on quadword boundaries.
	.equ	UR_K, 1
	.rep	(8-UR_K-((./4)%4)) % 4	;align the iret
	 nop
	.endr
;Work around 29K cache bug
t_iret:
	mtsrim	CDR,0			;invalid tag
	mfsr	it0, PC0
	const	it1, 0xff0
	and	it0,it0,it1		;extra line number
	consth	it0, CIR_FSEL_TAG + CIR_RW
	CK	(. % (4*4)) == 0
	mtsr	CIR, it0		;clear column 0
	const	it1, 0x1000
	mtsrim	CDR,0
	add	it0,it0,it1
	CK	(. % (4*4)) == 0
	mtsr	CIR, it0		;clear column 1
	nop
	nop
	nop

t_iret9:
	CK	((t_iret9 - t_iret) % (4*4)) == (UR_K*4)
	CK	(. % (4*4)) == 0
	iret
	nop


;The following interrupts are infrequent

;bad interrupt
	.align	16
badint:	jmp	.
	 halt


;MAC interrupt handler
	.align	16
macint:
	LODMAC	it0,EVNT_C,fast,	;it0=EVNT_C

	and	it2,it0,MAC_EVC_TKN_CNT_OVF
	LODMAC	it1,TOKEN_CT,fast
	sll	it2,it2,16 - MAC_EVC_TKN_CNT_OVF_BT
	CK	16 > MAC_EVC_TKN_CNT_OVF_BT
	add	it1,it1,it2
	INCCNT	tok_ct,it1,,it2,it3	;count tokens

	tbit	it1,it0,MAC_EVC_VOID_OVF
	jmpt	it1, macint15
	 tbit	it1,it0,MAC_EVC_VOID_RDY
	jmpf	it1,macint20
	 LODMAC	it1,VOID_TIME,fast
	STOCNT	it1,rnglatency,it2
macint15:
	LODMAC	it1,CTRL_B,fast,	;turn off purger after a measurement
	constx	it2,MAC_CB_RING_PURGE	;or a failure
	andn	it1,it1,it2
	STOMAC	it1,CTRL_B,it2

macint20:
	LODMAC	it0,EVNT_A		;it0=EVNT_A
	LODMAC	tpc,TX_STATUS		;tpc=TX status

	constx	it2,MAC_EVA_FRAME_RCVD
	and	it2,it2,it0
	sll	it2,it2,16 - MAC_EVA_FRAME_RCVD_BT
	CK	16 > MAC_EVA_FRAME_RCVD_BT
	LODMAC	it1,FRAME_CT,fast,	;count frames
	add	it1,it1,it2		;and notice counter overflow
	INCCNT	frame_ct,it1,,it2,it3

	LODMAC	it1,LOST_ERR,fast
	srl	tav,it1,8
	INCCNT	lost_ct,tav,,it2,it3
	and	it1,it1,0xff
	INCCNT	err_ct,it1,,it2,it3

	BITCNT	mac_cnt_ovf,MAC_EVA_DBL_OVFL,it0,intr,it1,it2


    ;Notice when RNGOP changes.  If our records suggest it has not
    ;change, then count an additional recovery and failure.
	tbit	it1,it0,MAC_EVA_RNGOP_CHG
	jmpf	it1, macint40		;jump if RINGOP did not change

	 tbit	it2,tpc,MAC_TX_ST_RNGOP
	set_st	PINT_REQ		;it changed, so wake up the host
	consth	it1,CREG_RED
	jmpf	it2,macint35
	 or	cregs,cregs,it1		;turn off the red LED eventually

    ;the ring is ok
	LODCNT	it2,rngbroke
	t_st	tav,LAT_VOID		;tav=latency measuring turned on
	add	it2,it2,1		;up count=broke+1 to signal good
	STOCNT	it2,rngop,it3

	jmpf	tav,macint40
	 constx	it2,MAC_CB_RING_PURGE
	LODMAC	it1,CTRL_B,fast,	;measure after a latency change
	or	it1,it1,it2
	STOMAC	it1,CTRL_B,it2
	jmp	macint40
	 nop

    ;the ring died.  Turn on the red LED immediately.
macint35:
	store	0,WORD,cregs,p_creg
	andn	cregs,cregs,it1
	LODCNT	it2,rngop
	STOCNT	it2,rngbroke,it3	;broke count=up count to signal bad

macint40:
    ;Generate counts of entering claim and beacon states.  These counts
    ;are not accurate, because claim or beacon states do not generate
    ;any interrupts.  If a MAC interrupt happens to occur while the
    ;MAC is (still) in one of those states, we can count it.  This
    ;is particularly unfortunate because it prevents detecting some
    ;kinds of non-operational rings caused by duplicate MAC addresses.
    ;We cannot tell if a stray beacon or claim frame was transmitted
    ;during an undetected beacon or claim state.

	srl	tpc,tpc,MAC_TX_ST_FSM_LSB_BT	;tpc=TX state
	CK	MAC_TX_ST_FSM_MSB_BT == 15	;no masking needed
	cpneq	it1,tpc,MAC_TX_ST_BEACON>>MAC_TX_ST_FSM_LSB_BT
	jmpt	it1,macint45

	 t_st	it2,BEC
	set_st	BEC
	jmpt	it2,macint50
	 rs_st	CLM

	INCCNT	bec,1,intr,it2,it3
	jmp	macint50
	 nop

macint45:
    ;tpc=TX state
	t_st	it3,CLM
	cpneq	it1,tpc,MAC_TX_ST_CLAIM>>MAC_TX_ST_FSM_LSB_BT
	jmpt	it1,macint50
	 andn	cur_st1,cur_st1,ST1_BEC | ST1_CLM
	jmpt	it3,macint50
	 set_st	CLM
	INCCNT	clm,1,intr,it2,it3

macint50:
	BITCNT	tvxexp,MAC_EVA_TVX_EXPIR,it0,intr,it1,it2
	BITCNT	trtexp,MAC_EVA_LATE_TKN,it0,intr,it1,it2
	BITCNT	trtexp,MAC_EVA_RCVRY_FAIL,it0,intr,it1,it2
	BITCNT	tkerr,MAC_EVA_DUPL_TKN,it0,,it1,it2
	BITCNT	pos_dup,MAC_EVA_DUPL_ADDR,it0,intr,it1,it2

	LODMAC	it0,EVNT_B,fast,	;it0=EVNT_B

	constx	it1,MAC_MASK_B_MORE	;stop frequents interrupts for now
	and	it1,it1,it0
	cpeq	it2,it1,0
	jmpt	it2,macint60
	 LODMAC	it2,MASK_B,fast		;do this now, so MAC will remove
	andn	it2,it2,it1		;interrupt request before we exit,
	STOMAC	it2,MASK_B,it1		;to avoid bad-interrupt trap
macint60:

	BITCNT	mybec,MAC_EVB_MY_BEACON,it0,,it1,it2
	BITCNT	otrbec,MAC_EVB_OTR_BEACON,it0,,it1,it2
	BITCNT	hiclm,MAC_EVB_HI_CLAIM,it0,,it1,it2
	BITCNT	loclm,MAC_EVB_LO_CLAIM,it0,,it1,it2
	BITCNT	myclm,MAC_EVB_MY_CLAIM,it0,,it1,it2
	BITCNT	dup_mac,MAC_EVB_BAD_BID,it0,,it1,it2

    ;something is gobbling our voids
	tbit	it1,it0,MAC_EVB_PURGE_ERR
	jmpf	it1,macint65
	 UPCNT	tkerr,1,,it1,it2
	UPCNT	pos_dup,1,,it1,it2	;it could be a duplicate MAC address
macint65:

	BITCNT	tkerr,MAC_EVB_BS_ERR,it0,,it1,it2
	BITCNT	tkiss,MAC_EVB_WON_CLAIM,it0,,it1,it2
	BITCNT	misfrm,MAC_EVB_NOT_COPIED,it0,,it1,it2

	and	it1,it0,MAC_MASK_B_BUG
	cpneq	it1,it1,0
	jmpf	it1,macint90

	 CNTADR	it1,mac_bug
 .ifdef DEBUG
	jmp	badint
	 nop
 .endif
	load	0,HWORD,it2,it1
	add	it2,it2,1
	sll	it2,it2,16
	or	it2,it2,it0
	store	0,WORD,it2,it1
	set_st	PINT_REQ

macint90:
	jmp	t_iret
	 nop



;ELM interrupt handler
	.align	16
elmint:

    ;update ELM counters
	LODELM0	tpc,EVENT,fast,		;tpc=event register
	srl	it0,tpc,ELM_EV_VSYM_CTR_BT
	LODELM0	it2,VIOL_SYM,fast
	and	it0,it0,1
	sll	it0,it0,8
	add	it2,it2,it0
	LODELM0	it3,LINK_ERR,fast
	add	it2,it2,it0

	t_st	it0,ELM0_INT
	jmpf	it0,elmint1		;ignore things when line state is bad

	INCCNT	vio0,it2,,it0,it1
	INCCNT	lem0_ct,it3,,it0,it1
	BITCNT	eovf0,ELM_EV_EBUF,tpc,,it0,it1
	BITCNT	noise0,ELM_EV_TNE,tpc,intr,it0,it1

elmint1:
 .ifdef DEBUG
	tbit	it0,tpc,ELM_EV_NP_ERR
	jmpt	it0, badint
 .endif
	BITCNT	elm_bug,ELM_EV_NP_ERR,tpc,intr,it0,it1

    ;Get the current and previous line states, look them up in a table which
    ;infers the current line state from the previous if the current is not
    ;well defined, and then generate line state changes for the host.

    ;Generate an additional line state transition if the ELM says the
    ;line state has changed and the computed previous line state differs
    ;the current line state.

	LODELM0	it3,STATUS_A,fast
	srl	it3,it3,ELM_LS_LSB_BT-1	;get current and previous line states
	CK	ELM_LS_LSB_BT>1
	and	it3,it3,0x3e
	constx	it1,rcv_ls_tbl
	add	it3,it3,it1
	load	0,HWORD,it3,it3		;it3=LS translated

	 tbit	it0,tpc,ELM_EV_LS
	jmpf	it0,elmint3
	 srl	it0,it3,4
	and	it0,it0,0xf		;it0=previous line state
	cpeq	tav,it0,ls0
	jmpt	tav,elmint3

	 mov	ls0,it0
	sll	ls0_stack,ls0_stack,LS_SHIFT	;host event on previous state
	or	ls0_stack,ls0_stack,ls0
	add	ls0_stack,ls0_stack,LS_DATA
	set_st	PINT_REQ

elmint3:
	and	it0,it3,0xf		;generate host event on current state
	cpeq	tav,it0,ls0
	jmpt	tav,elmint4

	 mov	ls0,it0
	sll	ls0_stack,ls0_stack,LS_SHIFT
	or	ls0_stack,ls0_stack,ls0
	add	ls0_stack,ls0_stack,LS_DATA
	set_st	PINT_REQ

    ;Whether or not it seems the line state has changed, set the match
    ;field to cause an interrupt if it does change.
elmint4:
	LODELM0	it0,CTRL_B,fast
	constx	tav,ELM_CB_MATCH_LS_MASK
	sll	it2,it3,ELM_CB_MATCH_LS_LSB_BT-8
	CK	ELM_CB_MATCH_LS_LSB>8
	and	it2,it2,tav
	andn	it0,it0,tav		;remove old match field
	or	it0,it0,it2
	STOELM0	it0,CTRL_B,tav,fast,	;install new from translated state

    ;repeat line state
	t_st	it1,PHY_REP
	callt	it1,it0,rep_ls

    ;mention important PCM changes
	and	it1,tpc,ELM_EV_PCMS
	cpneq	it1,it1,0
	jmpf	it1,elmint5

	 tbit	it1,tpc,ELM_EV_PCM_ST
	jmpf	it1,elmint4a
	 sll	it1,ls0_stack,LS_SHIFT
	or	ls0_stack,it1,RCV_SELF_TEST + LS_DATA
elmint4a:
	tbit	it1,tpc,ELM_EV_PCM_TRACE
	jmpf	it1,elmint4b
	 sll	it1,ls0_stack,LS_SHIFT
	or	ls0_stack,it1,RCV_TRACE_PROP + LS_DATA
elmint4b:
	tbit	it1,tpc,ELM_EV_PCM_BREAK
	jmpf	it1,elmint4c
	 LODELM0 it1,STATUS_B
	and	it1,it1,ELM_SB_BREAK_MASK
	cpgt	it1,it1,ELM_SB_BREAK_PC_START
	jmpf	it1,elmint4c
	 sll	it1,ls0_stack,LS_SHIFT
	or	ls0_stack,it1,RCV_BREAK + LS_DATA
elmint4c:
	tbit	it1,tpc,ELM_EV_PCM_CODE
	jmpf	it1,elmint4d
	 sll	it1,ls0_stack,LS_SHIFT
	or	ls0_stack,it1,RCV_PCM_CODE + LS_DATA
elmint4d:

    ;Generate fake line state interrupt if an ELM PCM event happened
    ;without a line state interrupt.
	cpeq	it0,ls0_stack,0
	jmpf	it0,elmint5
	 set_st	PINT_REQ
	sll	it0,ls0_stack,LS_SHIFT
	or	ls0_stack,it0,PCM_LSU + LS_DATA



;do the same for the 2nd ELM if present
elmint5:
	t_st	tav,DAS
	jmpf	tav,elmint9

	 LODELM1 tpc,EVENT,,		;tpc=event register
	srl	it0,tpc,ELM_EV_VSYM_CTR_BT
	LODELM1 it2,VIOL_SYM,fast
	and	it0,it0,1
	sll	it0,it0,8
	add	it2,it2,it0
	LODELM1	it3,LINK_ERR,fast
	add	it2,it2,it0

	t_st	it0,ELM1_INT
	jmpf	it0,elmint6		;ignore things when line state is bad

	 INCCNT	vio1,it2,,it0,it1
	INCCNT	lem1_ct,it3,,it0,it1
	BITCNT	eovf1,ELM_EV_EBUF,tpc,,it0,it1
	BITCNT	noise1,ELM_EV_TNE,tpc,intr,it0,it1

elmint6:
 .ifdef DEBUG
	tbit	it0,tpc,ELM_EV_NP_ERR
	jmpt	it0, badint
 .endif
	BITCNT	elm_bug,ELM_EV_NP_ERR,tpc,intr,it0,it1

	LODELM1	it3,STATUS_A,fast
	srl	it3,it3,ELM_LS_LSB_BT-1	;get current and previous line states
	CK	ELM_LS_LSB_BT>1
	and	it3,it3,0x3e
	constx	it1,rcv_ls_tbl
	add	it3,it3,it1
	load	0,HWORD,it3,it3		;it3=LS translated

	 tbit	it0,tpc,ELM_EV_LS
	jmpf	it0,elmint7
	 srl	it0,it3,4
	and	it0,it0,0xf		;it0=previous line state
	cpeq	tav,it0,ls1
	jmpt	tav,elmint7

	 mov	ls1,it0
	sll	ls1_stack,ls1_stack,LS_SHIFT	;host event on previous state
	or	ls1_stack,ls1_stack,ls1
	add	ls1_stack,ls1_stack,LS_DATA
	set_st	PINT_REQ

elmint7:
	and	it0,it3,0xf		;generate host event on current state
	cpeq	tav,it0,ls1
	jmpt	tav,elmint8

	 mov	ls1,it0
	sll	ls1_stack,ls1_stack,LS_SHIFT
	or	ls1_stack,ls1_stack,ls1
	add	ls1_stack,ls1_stack,LS_DATA
	set_st	PINT_REQ

elmint8:
	LODELM1	it0,CTRL_B,fast
	constx	tav,ELM_CB_MATCH_LS_MASK
	sll	it2,it3,ELM_CB_MATCH_LS_LSB_BT-8
	CK	ELM_CB_MATCH_LS_LSB>8
	and	it2,it2,tav
	andn	it0,it0,tav		;remove old match field
	or	it0,it0,it2
	STOELM1	it0,CTRL_B,tav,fast,	;install new from translated state

    ;mention important PCM changes
	and	it1,tpc,ELM_EV_PCMS
	cpneq	it1,it1,0
	jmpf	it1,elmint9

	 tbit	it1,tpc,ELM_EV_PCM_ST
	jmpf	it1,elmint8a
	 sll	it1,ls1_stack,LS_SHIFT
	or	ls1_stack,it1,RCV_SELF_TEST + LS_DATA
elmint8a:
	tbit	it1,tpc,ELM_EV_PCM_TRACE
	jmpf	it1,elmint8b
	 sll	it1,ls1_stack,LS_SHIFT
	or	ls1_stack,it1,RCV_TRACE_PROP + LS_DATA
elmint8b:
	tbit	it1,tpc,ELM_EV_PCM_BREAK
	jmpf	it1,elmint8c
	 LODELM1 it1,STATUS_B
	and	it1,it1,ELM_SB_BREAK_MASK
	cpgt	it1,it1,ELM_SB_BREAK_PC_START
	jmpf	it1,elmint8c
	 sll	it1,ls1_stack,LS_SHIFT
	or	ls1_stack,it1,RCV_BREAK + LS_DATA
elmint8c:
	tbit	it1,tpc,ELM_EV_PCM_CODE
	jmpf	it1,elmint8d
	 sll	it1,ls1_stack,LS_SHIFT
	or	ls1_stack,it1,RCV_PCM_CODE + LS_DATA
elmint8d:

    ;Generate fake line state interrupt if an ELM PCM event happened
    ;without a line state interrupt.
	cpeq	it0,ls1_stack,0
	jmpf	it0,elmint9
	 set_st	PINT_REQ
	sll	it0,ls1_stack,LS_SHIFT
	or	ls1_stack,it0,PCM_LSU + LS_DATA

elmint9:
	iret
	nop



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

    ;coff2firm assumes the last word is the version number
	.word	VERS

    ;there better be room for the checksum
	CK	. < SRAM + XPI_FIRM_SIZE - 4
	.end
